#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
serial_cmd.py —— 向 SkyStar BSP 的 Letter Shell 发送指令并抓取回显

与 .agents/skills/serial-monitor（只读抓取日志）互补：本工具负责"写"，
用于把 shell 验收命令送进板子并取回输出，适合 fatfs_test align / dump 这类
需要主动触发自检并读取结果的场景。

用法：
    python serial_cmd.py -p COM3 --cmd "fatfs_test mount" --cmd "fatfs_test align"
    python serial_cmd.py --auto --cmd "play_wav 0:/tour.wav" --wait 4 --tail 80

参数：
    -p/--port   串口号；--auto 自动挑第一个可用串口
    -b/--baud   波特率，默认 115200
    --cmd       要发送的指令，可重复多次，按顺序执行
    --wait      每条指令后等待秒数，默认 2.0
    --tail      只回显最后 N 行，默认全部
    --no-wake   不先发空行唤醒提示符
"""
import argparse
import sys
import time

try:
    import serial
    from serial.tools import list_ports
except ImportError:
    print("[ERROR] 需要 pyserial：pip install pyserial", file=sys.stderr)
    sys.exit(2)


def pick_port():
    ports = list(list_ports.comports())
    if not ports:
        print("[ERROR] 未发现任何串口", file=sys.stderr)
        sys.exit(2)
    for p in ports:
        if "SERIAL" in (p.description or "").upper():
            return p.device
    return ports[0].device


def drain(ser, seconds):
    """读取 seconds 秒内到达的全部字节"""
    buf = bytearray()
    deadline = time.time() + seconds
    while time.time() < deadline:
        n = ser.in_waiting
        if n:
            buf.extend(ser.read(n))
            time.sleep(0.02)
        else:
            time.sleep(0.05)
    return buf.decode("utf-8", errors="replace")


def main():
    ap = argparse.ArgumentParser(description="Send shell commands and capture echo")
    ap.add_argument("-p", "--port", help="串口号，例如 COM3")
    ap.add_argument("--auto", action="store_true", help="自动选择串口")
    ap.add_argument("-b", "--baud", type=int, default=115200)
    ap.add_argument("--cmd", action="append", default=[], help="要发送的指令，可重复")
    ap.add_argument("--wait", type=float, default=2.0, help="每条指令后等待秒数")
    ap.add_argument("--tail", type=int, default=0, help="仅回显最后 N 行")
    ap.add_argument("--no-wake", action="store_true", help="不先发空行唤醒提示符")
    a = ap.parse_args()

    port = a.port or (pick_port() if a.auto else None)
    if not port:
        print("[ERROR] 必须指定 -p COMx 或 --auto", file=sys.stderr)
        sys.exit(2)

    try:
        ser = serial.Serial(port, a.baud, timeout=0.2)
    except Exception as e:
        print(f"[ERROR] 打开 {port} 失败：{e}", file=sys.stderr)
        print("        常见原因：串口终端（含 serial-monitor）占用该端口，请先关闭。", file=sys.stderr)
        sys.exit(3)

    out = []
    try:
        if not a.no_wake:
            ser.write(b"\r\n")
            time.sleep(0.2)
            drain(ser, 0.6)

        for c in a.cmd:
            ser.write(c.encode("utf-8") + b"\r\n")
            got = drain(ser, a.wait)
            out.append(f"### >>> {c}\n{got}")
            if "ymodem" in c.lower():
                print("[WARN] ymodem_recv 需要与发送端握手，本工具不等待传输完成", file=sys.stderr)

        if not a.cmd:
            out.append(drain(ser, a.wait))
    finally:
        ser.close()

    text = "".join(out)
    if a.tail:
        text = "\n".join(text.splitlines()[-a.tail:])
    sys.stdout.write(text + "\n")


if __name__ == "__main__":
    main()
