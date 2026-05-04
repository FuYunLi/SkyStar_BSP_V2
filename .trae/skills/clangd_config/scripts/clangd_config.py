#!/usr/bin/env python3
"""
clangd 配置生成工具

从 Keil MDK 工程文件生成 compile_commands.json 和 .clangd 配置文件，
用于支持 clangd LSP 的代码跳转和智能提示。

用法:
    python clangd_config.py --project app.uvprojx --target Debug --export-all
    python clangd_config.py --project app.uvprojx --target Debug --export-compile-commands
    python clangd_config.py --project app.uvprojx --target Debug --export-clangd
"""

from __future__ import annotations

import argparse
import json
import os
import re
import sys
from dataclasses import dataclass, field
from pathlib import Path

try:
    import xml.etree.ElementTree as ET
except ImportError:
    ET = None

SCRIPT_VERSION = "1.0.0"


@dataclass
class KeilTarget:
    """Keil 工程目标配置。"""
    name: str
    device: str
    toolchain: str
    include_paths: list[str] = field(default_factory=list)
    defines: list[str] = field(default_factory=list)
    source_files: list[str] = field(default_factory=list)
    cflags: str = ""


def parse_keil_project(project_path: Path) -> list[KeilTarget]:
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

        target_name = name_elem.text.strip()

        device_elem = target_elem.find(".//TargetOption/TargetCommonProp/Device")
        device = device_elem.text.strip() if device_elem is not None and device_elem.text else "Unknown"

        toolchain_elem = target_elem.find(".//TargetOption/TargetCommonProp/Toolchain")
        toolchain = toolchain_elem.text.strip() if toolchain_elem is not None and toolchain_elem.text else "ARMCLANG"

        include_paths: list[str] = []
        include_elem = target_elem.find(".//Cads/VariousControls/IncludePath")
        if include_elem is not None and include_elem.text:
            include_paths = [p.strip() for p in include_elem.text.split(";") if p.strip()]

        defines: list[str] = []
        define_elem = target_elem.find(".//Cads/VariousControls/Define")
        if define_elem is not None and define_elem.text:
            defines = [d.strip() for d in define_elem.text.split(",") if d.strip()]

        source_files: list[str] = []
        for file_elem in target_elem.iter("FilePath"):
            if file_elem.text:
                ext = Path(file_elem.text).suffix.lower()
                if ext in (".c", ".cpp", ".cc", ".cxx"):
                    source_files.append(file_elem.text.strip())

        targets.append(KeilTarget(
            name=target_name,
            device=device,
            toolchain=toolchain,
            include_paths=include_paths,
            defines=defines,
            source_files=source_files,
        ))

    return targets


def resolve_include_paths(includes: list[str], project_dir: Path) -> list[str]:
    """将相对路径转换为绝对路径。"""
    resolved: list[str] = []
    for inc in includes:
        if not os.path.isabs(inc):
            abs_path = (project_dir / inc).resolve()
        else:
            abs_path = Path(inc).resolve()
        resolved.append(str(abs_path).replace("\\", "/"))
    return resolved


def export_compile_commands(
    project_path: Path,
    target: KeilTarget,
    output_path: Path | None = None,
    compiler_path: str | None = None,
) -> Path:
    """生成 compile_commands.json 文件。"""
    project_dir = project_path.parent.resolve()
    project_root = project_path.parent.parent.resolve()

    if output_path is None:
        output_path = project_root / "compile_commands.json"

    include_paths = resolve_include_paths(target.include_paths, project_dir)

    if compiler_path is None:
        if target.toolchain == "ARMCC":
            compiler_path = "armcc"
        else:
            compiler_path = "armclang"

    compile_commands: list[dict] = []

    for src in target.source_files:
        if not os.path.isabs(src):
            src_abs = (project_dir / src).resolve()
        else:
            src_abs = Path(src).resolve()

        command_parts = [compiler_path]
        for inc in include_paths:
            command_parts.append(f'-I"{inc}"')
        for define in target.defines:
            command_parts.append(f'-D{define}')
        if target.cflags:
            command_parts.append(target.cflags)
        command_parts.append('-c')
        command_parts.append(f'"{str(src_abs).replace(chr(92), "/")}"')

        entry = {
            "directory": str(project_root).replace("\\", "/"),
            "command": " ".join(command_parts),
            "file": str(src_abs).replace("\\", "/"),
        }
        compile_commands.append(entry)

    with open(output_path, "w", encoding="utf-8") as f:
        json.dump(compile_commands, f, indent=2, ensure_ascii=False)

    return output_path


def export_clangd_config(
    project_path: Path,
    target: KeilTarget,
    output_path: Path | None = None,
) -> Path:
    """生成 .clangd 配置文件。"""
    project_dir = project_path.parent.resolve()
    project_root = project_path.parent.parent.resolve()

    if output_path is None:
        output_path = project_root / ".clangd"

    include_paths = resolve_include_paths(target.include_paths, project_dir)

    # 使用纯文本方式生成 YAML 格式
    lines = ["CompileFlags:", "  Add:"]

    for p in include_paths:
        lines.append(f'    - "-I{p}"')

    for d in target.defines:
        lines.append(f'    - "-D{d}"')

    lines.extend([
        '    - "--target=arm-none-eabi"',
        '    - "-mcpu=cortex-m4"',
        "  Remove:",
        '    - "-mcpu=*"',
        '    - "-mfpu=*"',
        '    - "-mfloat-abi=*"',
        "",
        "Diagnostics:",
        "  UnusedIncludes: None",
        "  MissingIncludes: None",
    ])

    with open(output_path, "w", encoding="utf-8") as f:
        f.write("\n".join(lines) + "\n")

    return output_path


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="clangd 配置生成工具 - 从 Keil 工程生成 LSP 配置文件",
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )

    parser.add_argument("--project", "-p", help="Keil 工程文件路径 (.uvprojx)")
    parser.add_argument("--target", "-t", help="目标名称（默认使用第一个目标）")
    parser.add_argument("--list-targets", action="store_true", help="列出工程中的所有目标")

    parser.add_argument("--export-all", action="store_true", help="生成所有配置文件")
    parser.add_argument("--export-compile-commands", action="store_true", help="生成 compile_commands.json")
    parser.add_argument("--export-clangd", action="store_true", help="生成 .clangd 配置文件")
    parser.add_argument("--output", "-o", help="输出文件路径")
    parser.add_argument("--compiler", help="指定编译器路径")

    parser.add_argument("--version", "-v", action="version", version=f"%(prog)s {SCRIPT_VERSION}")

    return parser


def main() -> int:
    parser = build_parser()
    args = parser.parse_args()

    if not args.project:
        print("❌ 请指定工程文件路径: --project <path>")
        return 1

    project_path = Path(args.project).resolve()
    if not project_path.exists():
        print(f"❌ 工程文件不存在: {project_path}")
        return 1

    targets = parse_keil_project(project_path)
    if not targets:
        print("❌ 未找到有效的编译目标")
        return 1

    if args.list_targets:
        print(f"📋 工程目标列表 ({project_path.name}):")
        for i, t in enumerate(targets, 1):
            print(f"  {i}. {t.name} [{t.toolchain}] {t.device}")
        return 0

    selected = targets[0]
    if args.target:
        for t in targets:
            if t.name == args.target:
                selected = t
                break
        else:
            print(f"❌ 未找到目标: {args.target}")
            return 1

    project_root = project_path.parent.parent.resolve()

    if args.export_all or args.export_compile_commands:
        output = Path(args.output) if args.output else None
        cc_path = export_compile_commands(project_path, selected, output, args.compiler)
        print(f"✅ 生成 compile_commands.json: {cc_path}")

    if args.export_all or args.export_clangd:
        output = Path(args.output) if args.output else None
        if args.export_all and args.export_compile_commands:
            output = None
        clangd_path = export_clangd_config(project_path, selected, output)
        print(f"✅ 生成 .clangd: {clangd_path}")

    if not (args.export_all or args.export_compile_commands or args.export_clangd):
        print("❌ 请指定要生成的配置文件: --export-all, --export-compile-commands, 或 --export-clangd")
        return 1

    print(f"\n📁 输出目录: {project_root}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
