#!/usr/bin/env python3
"""PostToolUse formatter: agent 编辑 .c/.h 后自动 clang-format(用 Keil 自带工具)。

输入: stdin JSON ({"tool_name": ..., "tool_input": {"file_path": ...}})
静默成功 exit 0;文件无关或工具缺失时不报错。
"""
import json
import os
import subprocess
import sys

CLANG_FORMAT = r"D:\Keil5\ARM\ARMCLANG\bin\clang-format.exe"
CODE_EXTS = {".c", ".h", ".cpp", ".hpp"}

def main() -> int:
    try:
        raw = sys.stdin.read()
        data = json.loads(raw) if raw.strip() else {}
    except Exception:
        return 0

    ti = data.get("tool_input") or data.get("input") or {}
    path = ""
    if isinstance(ti, dict):
        path = ti.get("file_path") or ti.get("path") or ti.get("notebook_path") or ""
    if not path or os.path.splitext(path)[1].lower() not in CODE_EXTS:
        return 0
    if not os.path.isfile(path) or not os.path.isfile(CLANG_FORMAT):
        return 0

    try:
        subprocess.run(
            [CLANG_FORMAT, "-i", path],
            capture_output=True, timeout=15, check=False,
        )
    except Exception:
        pass  # 格式化失败不阻塞工作流
    return 0

if __name__ == "__main__":
    sys.exit(main())
