#!/usr/bin/env python3
"""PreToolUse guard: 拦截烧录/擦除/危险命令,返回 exit 2 阻止执行,交由用户确认。

输入: stdin JSON ({"tool_name": ..., "tool_input": {"command": ...}})
输出: 阻止时向 stdout 打印原因;放行时静默 exit 0。
"""
import json
import re
import sys

# 烧录/擦除/复位类命令特征(大小写不敏感)
FLASH_PATTERNS = re.compile(
    r"(" 
    r"\bUV4(?:\.exe)?\b[^|;&]*\s-f\b"      # Keil 烧录 (-f), 不误伤 -b 构建
    r"|\bopenocd\b"
    r"|\bpyocd\b"
    r"|\bst-flash\b"
    r"|\besptool\b"
    r"|\bJLink(?:Exe|RTTClient|\.exe)\b"
    r"|\berase_flash\b|\bmass_erase\b|\bchip_erase\b|\bfull_erase\b"
    r"|\bdirtyJtag\b|\bstm32-programmer\b.*\b-d\b"
    r")",
    re.IGNORECASE,
)

def main() -> int:
    try:
        raw = sys.stdin.read()
        data = json.loads(raw) if raw.strip() else {}
    except Exception:
        return 0  # 解析失败不拦,避免误伤

    tool = data.get("tool_name") or data.get("tool") or ""
    ti = data.get("tool_input") or data.get("input") or {}
    cmd = ti.get("command", "") if isinstance(ti, dict) else ""

    if tool != "Bash" or not cmd:
        return 0

    m = FLASH_PATTERNS.search(cmd)
    if m:
        print(f"[guard] 检测到烧录/擦除类命令(匹配: {m.group(1)})。", file=sys.stderr)
        print("[guard] 按项目红线,烧录/擦除/复位硬件必须由用户确认后手动执行;请向用户说明意图并等待确认,禁止重试该命令。", file=sys.stderr)
        return 2
    return 0

if __name__ == "__main__":
    try:
        sys.exit(main())
    except Exception:
        # 任何意外异常一律放行: hook 自身故障绝不能阻断工作流。
        # 教训: 退出码 2 = 阻止, 而 python 找不到脚本文件时退出码也是 2,
        # 曾导致全部 Bash 调用被误拦。路径已在 zcode.json 写绝对路径, 此处再兜底。
        sys.exit(0)
