#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
crcmap_cmp.py —— 把板端 `fatfs_test crcmap` 的输出与 PC 源文件逐块 CRC 对账

用法：
    python crcmap_cmp.py <板端输出文件> <PC 源文件> [块大小]

输出：每块一行，标注 pass1/pass2 自比结果（读是否稳定）与和 PC 的差异（内容是否真坏），
最后给出首个差异块与差异块总数。
"""
import binascii
import re
import sys

LINE = re.compile(r"^\s*(\d+)\s+([0-9A-Fa-f]{8})\s+([0-9A-Fa-f]{8})\s+([0-9A-Fa-f]{8})\s+(.)\s*$")


def main():
    if len(sys.argv) < 3:
        print(__doc__)
        return 2
    log_path, wav_path = sys.argv[1], sys.argv[2]
    block = int(sys.argv[3]) if len(sys.argv) > 3 else 4096

    data = open(wav_path, "rb").read()
    rows = []
    for line in open(log_path, encoding="utf-8", errors="replace"):
        m = LINE.match(line.rstrip())
        if m:
            rows.append((int(m.group(1)), int(m.group(2), 16), int(m.group(3), 16), int(m.group(4), 16),
                         m.group(5)))

    if not rows:
        print("[ERROR] 未从输出文件中解析到任何 crcmap 行，检查命令是否执行、日志是否完整")
        return 1

    unstable = []
    diverged = []
    for blk, off, p1, p2, same in rows:
        pc = binascii.crc32(data[off:off + block]) & 0xFFFFFFFF
        u = "rdOK " if p1 == p2 else "RD-BUSY"
        d = "pcOK " if p1 == pc else f"PC-DIFF(pc={pc:08X})"
        if p1 != p2:
            unstable.append(blk)
        if p1 != pc:
            diverged.append(blk)
        if p1 != p2 or p1 != pc:
            print(f"  blk {blk:<3} off=0x{off:08X} p1={p1:08X} p2={p2:08X}  {u}  {d}")

    print(f"--- blocks parsed={len(rows)}  read-unstable={len(unstable)}  pc-diverged={len(diverged)} ---")
    if diverged:
        print(f"    first diverged block = {diverged[0]} (offset 0x{diverged[0] * block:08X})")
    else:
        print("    内容与 PC 完全一致")
    if unstable:
        print(f"    [!] 读不稳定块 = {unstable}  -> 读路径本身不可靠")
    return 0


if __name__ == "__main__":
    sys.exit(main())
