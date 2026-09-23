#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
ymodem_short_sender.py —— 故意"少发数据"的 Ymodem 发送端，用于负向回归测试

用途：验证接收端在"实际收到字节数 < 文件头声明大小"（传输被中途打断、发送端掉线、
源文件被截断等）时，必须**丢弃临时文件并上报失败**，且**不得破坏目标位置上已存在的
同名正确文件**。协议主干与 ymodem_sender.py 一致，唯一区别是数据包只发 --packets 个
就发 EOT 收尾，从而确定性地制造"字节数与声明不符"。

用法：
    python ymodem_short_sender.py -p COM3 -f tour.wav --packets 10
    python ymodem_short_sender.py -p COM3 -f tour.wav --packets 10 --flash
"""
import argparse
import os
import sys
import time

import serial

SOH = b'\x01'
STX = b'\x02'
EOT = b'\x04'
ACK = b'\x06'
NAK = b'\x15'
CAN = b'\x18'
CHAR_C = b'\x43'


def calc_crc16(data: bytes) -> int:
    """计算 CCITT CRC16 校验码（与接收端 ymodem.c 一致：多项式 0x1021，初值 0x0000）"""
    crc = 0
    for b in data:
        crc ^= (b << 8)
        for _ in range(8):
            if crc & 0x8000:
                crc = (crc << 1) ^ 0x1021
            else:
                crc = crc << 1
            crc &= 0xFFFF
    return crc


class ShortSender:
    def __init__(self, port: str, baudrate: int = 115200):
        self.ser = serial.Serial(port=port, baudrate=baudrate, bytesize=serial.EIGHTBITS,
                                 parity=serial.PARITY_NONE, stopbits=serial.STOPBITS_ONE, timeout=2.0)

    def close(self):
        if self.ser and self.ser.is_open:
            self.ser.close()

    def start_session(self, command: str):
        """唤醒 Shell 并启动接收命令，随后等待首个 'C'"""
        self.ser.write(b'\r\n')
        time.sleep(0.1)
        self.ser.reset_input_buffer()
        self.ser.write(f"{command}\r\n".encode('utf-8'))
        print(f"[INFO] 已向终端写入指令: {command}")

    def wait_char(self, target: bytes, timeout: float) -> bool:
        start = time.time()
        while (time.time() - start) < timeout:
            if self.ser.in_waiting > 0:
                if self.ser.read(1) == target:
                    return True
        return False

    def send_packet(self, seq: int, data: bytes) -> str:
        """发送一个包，返回 'ACK'/'CAN'/'TIMEOUT'"""
        header = STX if len(data) == 1024 else SOH
        seq_num = seq & 0xFF
        packet = header + bytes([seq_num, (~seq_num) & 0xFF]) + data
        crc = calc_crc16(data)
        packet += bytes([(crc >> 8) & 0xFF, crc & 0xFF])

        self.ser.reset_input_buffer()
        self.ser.write(packet)
        self.ser.flush()

        start = time.time()
        echo = bytearray()
        while (time.time() - start) < 5.0:
            if self.ser.in_waiting > 0:
                ch = self.ser.read(1)
                if ch == ACK:
                    self.dump_mcu_echo(echo)
                    return 'ACK'
                if ch == CAN:
                    self.dump_mcu_echo(echo)
                    return 'CAN'
                echo.extend(ch)
        self.dump_mcu_echo(echo)
        return 'TIMEOUT'

    def dump_mcu_echo(self, echo: bytearray):
        """打印等待应答期间夹到的板端日志（提交/丢弃结论就在其中，不要丢）"""
        if echo:
            txt = bytes(echo).decode('utf-8', errors='replace').strip()
            if txt:
                print(f"[MCU] {txt}")

    def run(self, file_path: str, packets: int) -> int:
        size = os.stat(file_path).st_size
        if packets * 1024 >= size:
            print(f"[ERROR] --packets 必须使 {packets}*1024 < 文件长度 {size}，否则不构成少发")
            return 2

        print("[INFO] 等待 MCU 握手信号 'C'...")
        if not self.wait_char(CHAR_C, 10.0):
            print("[ERROR] 等待 'C' 超时，接收端未进入会话")
            return 1
        print("[INFO] 握手成功。")

        # Packet 0：声明真实长度，但后面只发 packets 个数据包
        name = os.path.basename(file_path)
        head_str = f"{name}\0{size}\0".encode('utf-8')
        head_data = head_str + b'\x00' * (128 - len(head_str))
        ret = self.send_packet(0, head_data)
        print(f"[INFO] 头包（声明 {size} 字节）应答: {ret}")
        if ret != 'ACK' or not self.wait_char(CHAR_C, 5.0):
            print("[ERROR] 头包阶段失败，接收端可能已中止")
            return 1

        with open(file_path, 'rb') as fp:
            for seq in range(1, packets + 1):
                chunk = fp.read(1024)
                chunk += b'\x1a' * (1024 - len(chunk))
                ret = self.send_packet(seq, chunk)
                print(f"[INFO] 数据包 seq={seq} 应答: {ret}")
                if ret != 'ACK':
                    print(f"[WARN] seq={seq} 未获 ACK（{ret}），停止发包并收尾")
                    break

        # EOT 双次确认 + 结束会话（空头包）
        for attempt in range(2):
            self.ser.write(EOT)
            print(f"[INFO] 发送 EOT (第 {attempt + 1} 次)，等待应答...")
            start = time.time()
            while (time.time() - start) < 5.0:
                if self.ser.in_waiting > 0:
                    ch = self.ser.read(1)
                    if ch in (NAK, ACK):
                        print(f"[INFO] 收到 {ch.hex()} ({'NAK' if ch == NAK else 'ACK'})")
                        break
        if self.wait_char(CHAR_C, 5.0):
            end_data = b'\x00' * 128
            print(f"[INFO] 结束头包应答: {self.send_packet(0, end_data)}")
        self.drain(3.0)      # 会话收尾后板端才打印提交/丢弃结论，必须取回
        print(f"[INFO] 本次共发出 {packets} 个数据包（{packets * 1024} 字节），声明 {size} 字节 —— "
              f"接收端应判为不完整并丢弃临时文件")
        return 0

    def drain(self, seconds: float):
        """在指定时长内把串口剩余输出全部打印出来（板端会话结束报告）"""
        buf = bytearray()
        start = time.time()
        while (time.time() - start) < seconds:
            if self.ser.in_waiting > 0:
                buf.extend(self.ser.read(self.ser.in_waiting))
            else:
                time.sleep(0.05)
        if buf:
            print("[MCU-TAIL] " + bytes(buf).decode('utf-8', errors='replace').strip())


def main():
    ap = argparse.ArgumentParser(description="故意少发数据的 Ymodem 发送端（负向测试）")
    ap.add_argument('-p', '--port', required=True)
    ap.add_argument('-b', '--baud', type=int, default=115200)
    ap.add_argument('-f', '--file', required=True)
    ap.add_argument('--packets', type=int, default=10, help="实际发送的数据包个数（每包 1024 字节）")
    ap.add_argument('--flash', action='store_true', help="接收端写 flash/ 前缀（默认 SD 0:/）")
    args = ap.parse_args()

    if not os.path.exists(args.file):
        print(f"[ERROR] 文件不存在: {args.file}")
        return 2

    s = ShortSender(args.port, args.baud)
    try:
        s.start_session("ymodem_recv -flash" if args.flash else "ymodem_recv")
        return s.run(args.file, args.packets)
    finally:
        s.close()
        print("[INFO] 串口已关闭。")


if __name__ == '__main__':
    sys.exit(main())
