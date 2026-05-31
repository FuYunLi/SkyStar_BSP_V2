#!/usr/bin/env python
"""Keil MDK 命令行构建工具。

这个脚本为 `build-keil` skill 提供可重复调用的执行入口，支持：

- 探测 Keil MDK 安装路径和 UV4.exe
- 扫描工作区中的 .uvprojx / .uvproj 工程文件
- 解析工程文件中的目标列表、输出目录和芯片信息
- 通过 UV4.exe 命令行执行 build / rebuild
- 在输出目录中搜索 AXF、HEX、BIN 产物
- 解析编译日志，提取错误和警告统计
"""

from __future__ import annotations

import argparse
import io
import os
import platform
import re
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Any

if sys.stdout and hasattr(sys.stdout, "reconfigure"):
    try:
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass
elif sys.stdout:
    sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding="utf-8", errors="replace")
if sys.stderr and hasattr(sys.stderr, "reconfigure"):
    try:
        sys.stderr.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass
elif sys.stderr:
    sys.stderr = io.TextIOWrapper(sys.stderr.buffer, encoding="utf-8", errors="replace")

_SCRIPT_DIR = Path(__file__).resolve().parent
_SKILLS_DIR = _SCRIPT_DIR.parent.parent
for _candidate in [_SKILLS_DIR / "shared", _SKILLS_DIR.parent / "shared"]:
    if (_candidate / "tool_config.py").exists():
        sys.path.insert(0, str(_candidate))
        break
from tool_config import get_tool_path, set_tool_path

try:
    import xml.etree.ElementTree as ET
except ImportError:
    ET = None  # type: ignore[assignment,misc]

ARTIFACT_EXTENSIONS = {".axf": "elf", ".elf": "elf", ".hex": "hex", ".bin": "bin"}
ARTIFACT_PRIORITY = {"elf": 1, "hex": 2, "bin": 3}
PROJECT_EXTENSIONS = {".uvprojx", ".uvproj"}

# Keil MDK 常见安装路径
KEIL_SEARCH_PATHS = [
    r"C:\Keil_v5\UV4\UV4.exe",
    r"C:\Keil\UV4\UV4.exe",
    r"D:\Keil_v5\UV4\UV4.exe",
    r"D:\Keil\UV4\UV4.exe",
]


@dataclass
class KeilTarget:
    name: str
    device: str
    output_dir: str
    output_name: str
    toolchain: str  # ARMCC or ARMCLANG
    create_hex: bool
    create_bin: bool


@dataclass
class Artifact:
    path: Path
    kind: str
    size: int


@dataclass
class BuildResult:
    status: str  # success, failure, blocked
    summary: str
    build_cmd: str | None = None
    project_file: str | None = None
    target_name: str | None = None
    device: str | None = None
    toolchain: str | None = None
    errors: int = 0
    warnings: int = 0
    evidence: list[str] | None = None
    artifacts: list[Artifact] | None = None
    primary_artifact: Artifact | None = None
    program_size: dict[str, int] | None = None
    build_time: str | None = None


# ---------------------------------------------------------------------------
# 环境探测
# ---------------------------------------------------------------------------

def is_windows() -> bool:
    return platform.system() == "Windows"


def find_uv4(explicit_path: str | None = None) -> Path | None:
    """查找 UV4.exe 路径。"""
    if explicit_path:
        p = Path(explicit_path)
        if p.exists():
            return p

    # 从配置文件读取
    cfg_path = get_tool_path("uv4")
    if cfg_path and Path(cfg_path).exists():
        return Path(cfg_path)

    # 搜索常见路径
    for candidate in KEIL_SEARCH_PATHS:
        p = Path(candidate)
        if p.exists():
            return p

    # 环境变量
    for env_var in ["KEIL_ROOT", "MDK_ROOT", "UV4_ROOT"]:
        root = os.environ.get(env_var)
        if root:
            for sub in ["UV4", "BIN", ""]:
                uv4 = Path(root) / sub / "UV4.exe"
                if uv4.exists():
                    return uv4

    return None


def detect_environment(explicit_uv4: str | None = None) -> dict[str, Any]:
    """探测 Keil MDK 环境。"""
    uv4_path = find_uv4(explicit_uv4)
    return {
        "platform": platform.system(),
        "uv4": {
            "available": uv4_path is not None,
            "path": str(uv4_path) if uv4_path else None,
        },
    }


def print_detect_report(env: dict[str, Any]) -> None:
    """打印环境探测报告。"""
    print("🔍 Keil MDK 环境探测")
    print(f"  平台: {env['platform']}")
    uv4 = env["uv4"]
    if uv4["available"]:
        print(f"  ✅ UV4.exe: {uv4['path']}")
    else:
        print("  ❌ UV4.exe: 未找到")


# ---------------------------------------------------------------------------
# 工程文件解析
# ---------------------------------------------------------------------------

def scan_project_files(search_dir: Path) -> list[Path]:
    """扫描目录中的 Keil 工程文件。"""
    projects: list[Path] = []
    for ext in PROJECT_EXTENSIONS:
        projects.extend(search_dir.rglob(f"*{ext}"))
    return sorted(set(projects))


def parse_project(project_path: Path) -> list[KeilTarget]:
    """解析 Keil 工程文件，提取目标配置。"""
    if ET is None:
        print("❌ xml.etree.ElementTree 不可用")
        return []

    try:
        tree = ET.parse(project_path)
    except ET.ParseError as exc:
        print(f"❌ 工程文件解析失败: {exc}")
        return []

    root = tree.getroot()
    targets: list[KeilTarget] = []

    for target_elem in root.iter("Target"):
        name_elem = target_elem.find("TargetName")
        if name_elem is None or not name_elem.text:
            continue
        name = name_elem.text.strip()

        device = ""
        device_elem = target_elem.find(".//Device")
        if device_elem is not None and device_elem.text:
            device = device_elem.text.strip()

        output_dir = ""
        output_name = ""
        output_elem = target_elem.find(".//OutputDirectory")
        if output_elem is not None and output_elem.text:
            output_dir = output_elem.text.strip().rstrip("\\/")
        oname_elem = target_elem.find(".//OutputName")
        if oname_elem is not None and oname_elem.text:
            output_name = oname_elem.text.strip()

        toolchain = "ARMCC"
        armclang_elem = target_elem.find(".//uAC6")
        if armclang_elem is not None and armclang_elem.text == "1":
            toolchain = "ARMCLANG"

        create_hex = False
        create_bin = False
        hex_elem = target_elem.find(".//CreateHexFile")
        if hex_elem is not None and hex_elem.text == "1":
            create_hex = True

        targets.append(KeilTarget(
            name=name,
            device=device,
            output_dir=output_dir,
            output_name=output_name,
            toolchain=toolchain,
            create_hex=create_hex,
            create_bin=create_bin,
        ))

    return targets


# ---------------------------------------------------------------------------
# 产物扫描
# ---------------------------------------------------------------------------

def scan_artifacts(search_dir: Path) -> list[Artifact]:
    if not search_dir.exists():
        return []

    artifacts: list[Artifact] = []
    seen: set[str] = set()
    for root, _dirs, files in os.walk(search_dir):
        for fname in files:
            ext = Path(fname).suffix.lower()
            kind = ARTIFACT_EXTENSIONS.get(ext)
            if not kind:
                continue
            fpath = Path(root) / fname
            real = str(fpath.resolve())
            if real in seen:
                continue
            seen.add(real)
            try:
                size = fpath.stat().st_size
            except OSError:
                size = 0
            if size < 256:
                continue
            artifacts.append(Artifact(path=fpath, kind=kind, size=size))

    artifacts.sort(key=lambda a: (ARTIFACT_PRIORITY.get(a.kind, 9), -a.size))
    return artifacts


def resolve_output_dir(project_path: Path, target: KeilTarget) -> Path:
    if target.output_dir:
        candidate = project_path.parent / target.output_dir
        return candidate.resolve()
    return project_path.parent.resolve()


# ---------------------------------------------------------------------------
# 编译执行
# ---------------------------------------------------------------------------

def parse_build_log(log_path: Path) -> tuple[int, int, list[str], dict[str, int] | None, str | None]:
    """解析编译日志，返回 (errors, warnings, evidence_lines, program_size, build_time)"""
    errors = 0
    warnings = 0
    evidence: list[str] = []
    program_size: dict[str, int] | None = None
    build_time: str | None = None

    if not log_path.exists():
        return 0, 0, [], None, None

    try:
        for encoding in ["utf-8", "gbk", "latin-1"]:
            try:
                content = log_path.read_text(encoding=encoding)
                break
            except UnicodeDecodeError:
                continue
        else:
            return 0, 0, ["(日志编码无法识别)"], None, None
    except OSError:
        return 0, 0, [], None, None

    for line in content.split("\n"):
        stripped = line.strip()
        if not stripped:
            continue

        if ": error" in stripped.lower() or "error:" in stripped.lower():
            errors += 1
            if len(evidence) < 20:
                evidence.append(stripped)
        elif ": warning" in stripped.lower() or "warning:" in stripped.lower():
            warnings += 1

        summary_match = re.search(r"(\d+)\s+Error\(s\),\s+(\d+)\s+Warning\(s\)", stripped)
        if summary_match:
            errors = max(errors, int(summary_match.group(1)))
            warnings = max(warnings, int(summary_match.group(2)))

        if "build target" in stripped.lower() or "rebuild target" in stripped.lower():
            evidence.append(stripped)
        if "compiling" in stripped.lower() and errors == 0 and len(evidence) < 5:
            evidence.append(stripped)

        size_match = re.search(
            r"Program Size:\s*Code=(\d+)\s+RO-data=(\d+)\s+RW-data=(\d+)\s+ZI-data=(\d+)",
            stripped,
        )
        if size_match:
            program_size = {
                "Code": int(size_match.group(1)),
                "RO-data": int(size_match.group(2)),
                "RW-data": int(size_match.group(3)),
                "ZI-data": int(size_match.group(4)),
            }

        time_match = re.search(r"Build Time\s*[:：]\s*(\d{2}:\d{2}:\d{2})", stripped)
        if time_match:
            build_time = time_match.group(1)

    return errors, warnings, evidence, program_size, build_time


def run_keil_build(
    uv4_path: Path,
    project_path: Path,
    target_name: str,
    rebuild: bool = False,
    log_path: Path | None = None,
) -> BuildResult:
    """执行 Keil 编译。"""
    if not is_windows():
        return BuildResult(
            status="blocked",
            summary="Keil MDK 仅在 Windows 上支持编译",
            project_file=str(project_path),
            target_name=target_name,
        )

    cmd = [
        str(uv4_path),
        "-b" if not rebuild else "-r",
        str(project_path),
        "-j0",
        f"-t{target_name}",
        "-o",
        str(log_path) if log_path else "build_log.txt",
    ]

    cmd_str = " ".join(cmd)

    try:
        result = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            timeout=600,
        )
        exit_code = result.returncode
    except subprocess.TimeoutExpired:
        return BuildResult(
            status="failure",
            summary="编译超时（>10分钟）",
            build_cmd=cmd_str,
            project_file=str(project_path),
            target_name=target_name,
        )
    except Exception as exc:
        return BuildResult(
            status="failure",
            summary=f"编译执行失败: {exc}",
            build_cmd=cmd_str,
            project_file=str(project_path),
            target_name=target_name,
        )

    errors, warnings, evidence, program_size, build_time = parse_build_log(
        log_path if log_path else project_path.parent / "build_log.txt"
    )

    if exit_code != 0 or errors > 0:
        return BuildResult(
            status="failure",
            summary=f"编译失败: {errors} 错误, {warnings} 警告",
            build_cmd=cmd_str,
            project_file=str(project_path),
            target_name=target_name,
            errors=errors,
            warnings=warnings,
            evidence=evidence,
            program_size=program_size,
            build_time=build_time,
        )

    return BuildResult(
        status="success",
        summary=f"编译成功: {warnings} 警告",
        build_cmd=cmd_str,
        project_file=str(project_path),
        target_name=target_name,
        errors=0,
        warnings=warnings,
        evidence=evidence,
        program_size=program_size,
        build_time=build_time,
    )


# ---------------------------------------------------------------------------
# 报告输出
# ---------------------------------------------------------------------------

def format_size(size_bytes: int) -> str:
    if size_bytes < 1024:
        return f"{size_bytes} B"
    elif size_bytes < 1024 * 1024:
        return f"{size_bytes / 1024:.1f} KB"
    else:
        return f"{size_bytes / (1024 * 1024):.1f} MB"


def print_build_report(result: BuildResult) -> None:
    """打印编译报告。"""
    if result.status == "success":
        print(f"\n✅ 编译成功")
    elif result.status == "blocked":
        print(f"\n⚠️ 编译阻塞")
    else:
        print(f"\n❌ 编译失败")

    if result.project_file and result.target_name:
        print(f"  工程: {Path(result.project_file).name} → 目标: {result.target_name}")

    if result.device and result.toolchain:
        print(f"  芯片: {result.device} | 工具链: {result.toolchain}")

    if result.program_size:
        code = result.program_size.get("Code", 0)
        ro = result.program_size.get("RO-data", 0)
        rw = result.program_size.get("RW-data", 0)
        zi = result.program_size.get("ZI-data", 0)
        flash = code + ro + rw
        ram = rw + zi
        print(f"  固件大小: Flash ≈ {flash / 1024:.1f} KB  RAM ≈ {ram / 1024:.1f} KB")

    if result.build_time:
        print(f"  编译耗时: {result.build_time}")

    if result.artifacts:
        artifact_strs = []
        for a in result.artifacts[:3]:
            artifact_strs.append(f"{a.path.name} ({format_size(a.size)})")
        print(f"  产物: {', '.join(artifact_strs)}")

    if result.errors > 0 or result.warnings > 0:
        print(f"  错误: {result.errors} | 警告: {result.warnings}")

    if result.evidence and result.status == "failure":
        print("  错误详情:")
        for line in result.evidence[:5]:
            print(f"    {line}")


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------

def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Keil MDK 命令行构建工具",
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )

    parser.add_argument("--detect", action="store_true", help="探测 Keil MDK 环境")
    parser.add_argument("--project", help=".uvprojx 或 .uvproj 工程文件路径")
    parser.add_argument("--target", help="构建目标名称")
    parser.add_argument("--list-targets", action="store_true", help="列出工程中的所有目标")
    parser.add_argument("--rebuild", action="store_true", help="重新编译（clean + build）")
    parser.add_argument("--scan", help="扫描指定目录中的 Keil 工程文件")
    parser.add_argument("--scan-artifacts", help="仅扫描指定目录中的产物")
    parser.add_argument("--uv4", help="显式指定 UV4.exe 路径")
    parser.add_argument("--save-config", action="store_true", help="探测成功后保存工具路径到配置")
    parser.add_argument("--log", help="编译日志输出路径")
    parser.add_argument("-v", "--verbose", action="store_true", help="详细输出")

    return parser


def main() -> int:
    parser = build_parser()
    args = parser.parse_args()

    if args.detect:
        env = detect_environment(args.uv4)
        print_detect_report(env)
        if args.save_config and env["uv4"]["available"]:
            cfg_path = set_tool_path("uv4", env["uv4"]["path"])
            print(f"  💾 已保存到 {cfg_path}")
        if not env["uv4"]["available"]:
            return 1
        if not args.project:
            return 0

    if args.scan:
        scan_dir = Path(args.scan).resolve()
        projects = scan_project_files(scan_dir)
        if not projects:
            print(f"❌ 在 {scan_dir} 中未找到 Keil 工程文件")
            return 1
        print(f"📋 找到 {len(projects)} 个 Keil 工程文件：")
        for i, p in enumerate(projects, 1):
            print(f"  {i}. {p}")
        return 0

    if args.scan_artifacts:
        scan_dir = Path(args.scan_artifacts).resolve()
        artifacts = scan_artifacts(scan_dir)
        if not artifacts:
            print(f"❌ 在 {scan_dir} 中未找到固件产物")
            return 1
        primary = artifacts[0] if artifacts else None
        result = BuildResult(
            status="success",
            summary=f"找到 {len(artifacts)} 个产物",
            artifacts=artifacts,
            primary_artifact=primary,
        )
        print_build_report(result)
        return 0

    if not args.project:
        print("❌ 请提供 --project（Keil 工程文件路径）。")
        return 1

    project_path = Path(args.project).resolve()
    if not project_path.exists():
        print(f"❌ 工程文件不存在: {project_path}")
        return 1

    targets = parse_project(project_path)
    if not targets:
        print(f"❌ 未能从工程文件中解析出目标: {project_path}")
        return 1

    if args.list_targets:
        print(f"📋 工程 {project_path.name} 中的目标：")
        for i, t in enumerate(targets, 1):
            tc_info = f" [{t.toolchain}]" if t.toolchain else ""
            dev_info = f" ({t.device})" if t.device else ""
            out_info = f" → {t.output_dir}/{t.output_name}" if t.output_name else ""
            print(f"  {i}. {t.name}{tc_info}{dev_info}{out_info}")
        return 0

    selected: KeilTarget | None = None
    if args.target:
        for t in targets:
            if t.name == args.target:
                selected = t
                break
        if not selected:
            print(f"❌ 未找到目标 '{args.target}'，可用目标：")
            for t in targets:
                print(f"  - {t.name}")
            return 1
    else:
        selected = targets[0]
        if len(targets) > 1:
            print(f"ℹ️ 未指定目标，默认使用: {selected.name}")

    print(f"📦 目标: {selected.name} [{selected.toolchain}] {selected.device}")

    uv4_path = find_uv4(args.uv4)
    if not uv4_path:
        if not is_windows():
            print("❌ Keil MDK 仅在 Windows 上支持编译。")
            print("   当前平台可使用 --list-targets 和 --scan-artifacts。")
        else:
            print("❌ 未找到 UV4.exe，请安装 Keil MDK 或通过 --uv4 指定路径。")
        return 1

    log_path = Path(args.log) if args.log else project_path.parent / f"{selected.name}_build.log"

    build_out = run_keil_build(
        uv4_path=uv4_path,
        project_path=project_path,
        target_name=selected.name,
        rebuild=args.rebuild,
        log_path=log_path,
    )

    output_dir = resolve_output_dir(project_path, selected)
    artifacts = scan_artifacts(output_dir)
    if not artifacts:
        project_dir = project_path.parent.resolve()
        if output_dir != project_dir and not str(output_dir).startswith(str(project_dir) + os.sep):
            artifacts = scan_artifacts(project_dir)
    primary = artifacts[0] if artifacts else None

    build_out.artifacts = artifacts
    build_out.primary_artifact = primary
    build_out.device = selected.device
    build_out.toolchain = selected.toolchain

    print_build_report(build_out)

    if build_out.status == "success" and primary:
        print(f"\n📦 首选产物: {primary.path}")
        print(f"   类型: {primary.kind.upper()}")
        print(f"   大小: {format_size(primary.size)}")

    return 0 if build_out.status == "success" else 1


if __name__ == "__main__":
    sys.exit(main())
