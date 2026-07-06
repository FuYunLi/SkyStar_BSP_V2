#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
Ymodem 串口文件传输脚本
适用于 SkyStar BSP V2 的 Letter Shell 终端自动交互与固件/数据发送。

使用方法:
    1. 双击运行 ymodem_sender.bat 开启图形/引导向导（面向不熟悉 Python 的开发者）
    2. 命令行运行: python ymodem_sender.py -p COM3 -f firmware.bin [--flash] [-d 0:/data/]
"""

import os
import sys
import time
import argparse

try:
    import serial
    import serial.tools.list_ports
except ImportError:
    print("[ERROR] 缺失 pyserial 库。")
    print("正在尝试自动为您安装 pyserial，请稍候...")
    try:
        import subprocess
        subprocess.check_call([sys.executable, "-m", "pip", "install", "pyserial"])
        import serial
        import serial.tools.list_ports
        print("[SUCCESS] pyserial 库自动安装成功！\n")
    except Exception as e:
        print(f"[ERROR] 自动安装失败: {e}")
        print("请在命令行中手动输入以下命令安装依赖：")
        print("    pip install pyserial")
        input("\n按回车键退出程序...")
        sys.exit(1)

# Ymodem 协议控制字符定义
SOH = b'\x01'  # 128字节数据块起始
STX = b'\x02'  # 1024字节数据块起始
EOT = b'\x04'  # 结束传输
ACK = b'\x06'  # 应答
NAK = b'\x15'  # 否认
CAN = b'\x18'  # 取消传输
CHAR_C = b'\x43'  # 字符 'C'，请求 CRC16 校验

def calc_crc16(data: bytes) -> int:
    """计算 CCITT CRC16 校验码"""
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

def safe_print_mcu_debug(mcu_output: bytes):
    if not mcu_output:
        return
    msg = mcu_output.decode('utf-8', errors='ignore').strip()
    try:
        print(f"[MCU Debug]: {msg}")
    except UnicodeEncodeError:
        encoding = sys.stdout.encoding or 'utf-8'
        safe_msg = msg.encode(encoding, errors='replace').decode(encoding)
        print(f"[MCU Debug]: {safe_msg}")

class YmodemSender:
    def __init__(self, port: str, baudrate: int = 115200):
        self.ser = serial.Serial(
            port=port,
            baudrate=baudrate,
            bytesize=serial.EIGHTBITS,
            parity=serial.PARITY_NONE,
            stopbits=serial.STOPBITS_ONE,
            timeout=2.0
        )
        if not self.ser.is_open:
            self.ser.open()

    def close(self):
        if self.ser and self.ser.is_open:
            self.ser.close()

    def send_shell_cmd(self, command: str):
        """向 Shell 发送指令"""
        print(f"\n[INFO] 发送 Shell 唤醒与启动指令...")
        self.ser.write(b'\r\n')
        time.sleep(0.1)
        self.ser.write(b'\r\n')
        time.sleep(0.1)
        # 清除接收缓冲区
        self.ser.reset_input_buffer()
        # 发送真正的 ymodem 启动命令
        full_cmd = f"{command}\r\n".encode('utf-8')
        self.ser.write(full_cmd)
        print(f"[INFO] 已向终端写入指令: {command}")

    def wait_for_char(self, target_char: bytes, timeout: float = 10.0) -> bool:
        """等待目标字符（如 'C'）"""
        start_time = time.time()
        while (time.time() - start_time) < timeout:
            if self.ser.in_waiting > 0:
                char = self.ser.read(1)
                if char == target_char:
                    return True
        return False

    def send_packet(self, seq: int, data: bytes) -> bool:
        """发送单个 Ymodem 数据包"""
        # 判断是 128 还是 1024 字节包
        if len(data) == 128:
            header = SOH
        elif len(data) == 1024:
            header = STX
        else:
            raise ValueError("数据块长度必须为 128 或 1024 字节")

        # 数据包 structure: [Header] [Seq] [~Seq] [Data] [CRC_H] [CRC_L]
        seq_num = seq & 0xFF
        seq_inv = (~seq_num) & 0xFF
        packet = header + bytes([seq_num, seq_inv]) + data
        crc = calc_crc16(data)
        packet += bytes([(crc >> 8) & 0xFF, crc & 0xFF])

        # 尝试发送并等待 ACK
        for retry in range(5):
            # 发送前先清理接收缓冲区，确保读到的响应是针对当前包的应答，而不是历史堆积的日志或回显
            self.ser.reset_input_buffer()
            
            self.ser.write(packet)
            self.ser.flush()
            
            # 等待接收端的 ACK/NAK/CAN，记录其他干扰字符（如日志、回显等）
            response = b''
            start_wait = time.time()
            mcu_output = bytearray()
            while (time.time() - start_wait) < 5.0:
                if self.ser.in_waiting > 0:
                    ch = self.ser.read(1)
                    if ch in (ACK, NAK, CAN):
                        response = ch
                        break
                    else:
                        mcu_output.extend(ch)
            
            if response == ACK:
                if mcu_output:
                    # 如果有调试输出，可以在这里做额外记录或静默
                    pass
                return True
            elif response == CAN:
                print(f"\n[ERROR] 接收端主动取消了传输 (CAN)")
                safe_print_mcu_debug(mcu_output)
                return False
            else:
                print(f"\n[WARN] 数据包 seq {seq} 发送未收到预期 ACK (期望 0x06，实际收到 {response or '超时'})，正在进行第 {retry+1} 次重试...")
                safe_print_mcu_debug(mcu_output)
                time.sleep(0.5)
        
        return False

    def send_files(self, file_paths: list) -> bool:
        """批量文件传输逻辑，支持随时按 Ctrl+C 中断并向 MCU 发送 CAN 信号退出"""
        if not file_paths:
            return False

        # 1. 等待接收端发起第一个传输请求符 'C'
        print("[INFO] 等待 MCU 握手信号 'C'...")
        if not self.wait_for_char(CHAR_C, timeout=10.0):
            print("[ERROR] 等待 MCU 发起 'C' 信号超时，传输失败。")
            return False
        print("[INFO] 握手成功，开始批量传输。")

        try:
            for idx, file_path in enumerate(file_paths):
                if not os.path.exists(file_path):
                    print(f"[ERROR] 找不到要发送的文件: {file_path}")
                    return False

                file_name = os.path.basename(file_path)
                file_size = os.stat(file_path).st_size
                print(f"\n[INFO] 正在传输第 {idx+1}/{len(file_paths)} 个文件: {file_name} ({file_size} 字节)")

                # 发送 Packet 0 (文件元数据)
                header_str = f"{file_name}\0{file_size}\0".encode('utf-8')
                header_data = header_str + b'\x00' * (128 - len(header_str))
                
                if not self.send_packet(0, header_data):
                    print(f"[ERROR] 发送文件信息包 (Packet 0) 失败")
                    return False

                # 等待接收端 'C' 启动数据传输
                if not self.wait_for_char(CHAR_C, timeout=5.0):
                    print(f"[ERROR] 文件元数据包发送后，未收到接收端的 'C' 传输启动信号")
                    return False

                # 发送文件内容
                with open(file_path, 'rb') as f:
                    seq = 1
                    while True:
                        chunk = f.read(1024)
                        if not chunk:
                            break
                        
                        if len(chunk) < 1024:
                            if len(chunk) <= 128:
                                chunk = chunk + b'\x00' * (128 - len(chunk))
                            else:
                                chunk = chunk + b'\x00' * (1024 - len(chunk))

                        if not self.send_packet(seq, chunk):
                            print(f"\n[ERROR] 发送数据包 {seq} 失败")
                            return False
                        
                        sent_bytes = min(seq * 1024, file_size)
                        progress = (sent_bytes / file_size) * 100
                        print(f"[INFO] 进度: {progress:.1f}% ({sent_bytes}/{file_size} 字节)", end='\r')
                        seq += 1
                
                print(f"\n[INFO] {file_name} 内容发送完毕，开始结束握手...")

                # 发送第一个 EOT
                for retry in range(3):
                    self.ser.write(EOT)
                    self.ser.flush()
                    response = self.ser.read(1)
                    if response == NAK:
                        break
                    print(f"[WARN] 发送 EOT 未收到 NAK 应答 (收到 {response})，正在重试...")
                else:
                    print("[ERROR] 结束标志 EOT 交互失败")
                    return False

                # 再次发送 EOT
                self.ser.write(EOT)
                self.ser.flush()
                response = self.ser.read(1)
                if response != ACK:
                    print(f"[WARN] 第二次发送 EOT 未收到 ACK (收到 {response})")
                
                # 等待接收端发送 'C' 启动下一个文件的传输（或者用于最后的空包握手）
                if not self.wait_for_char(CHAR_C, timeout=5.0):
                    print("[ERROR] 传输结束未收到下一个文件的 'C' 启动信号")
                    return False

            # 全部文件发送完毕，发送空 Packet 0 代表全传输流结束
            print("\n[INFO] 所有文件发送完毕，发送空终止包关闭会话...")
            empty_packet = b'\x00' * 128
            self.send_packet(0, empty_packet)

            print("[SUCCESS] 批量文件传输已全部成功完成！")
            return True

        except KeyboardInterrupt:
            print("\n[WARN] 检测到用户按下 Ctrl+C，正在终止传输...")
            # 发送 5 个 CAN 字节，通知 MCU 取消接收
            self.ser.write(CAN * 5)
            self.ser.flush()
            time.sleep(0.5)
            print("[INFO] 已发送 CAN 中断指示，传输已强行取消。")
            return False

def scan_ports():
    """扫描系统串口，检测设备描述与占用状态"""
    print("[INFO] 正在检索系统串口端口...")
    ports = list(serial.tools.list_ports.comports())
    if not ports:
        print("[ERROR] 未检测到任何串口设备，请检查硬件连接或驱动！")
        return []
    
    port_list = []
    for p in ports:
        status = "可用"
        try:
            test_ser = serial.Serial(p.device)
            test_ser.close()
        except Exception:
            status = "已被占用"
        port_list.append((p.device, p.description, status))
    return port_list

def collect_files(path_input: str) -> list:
    """收集路径下的所有待发送文件（仅扫描一层，防止目录过深）"""
    file_paths = []
    if os.path.isfile(path_input):
        file_paths.append(path_input)
    elif os.path.isdir(path_input):
        for item in sorted(os.listdir(path_input)):
            full_path = os.path.join(path_input, item)
            if os.path.isfile(full_path):
                file_paths.append(full_path)
    return file_paths

def get_user_input(prompt_text, allow_back=True):
    """获取用户输入，统一处理返回(b)和退出(q)"""
    while True:
        try:
            val = input(prompt_text).strip()
        except KeyboardInterrupt:
            print("\n[INFO] 检测到 Ctrl+C 信号，退出程序。")
            sys.exit(0)
            
        if val.lower() == 'q':
            print("\n[INFO] 用户主动退出。")
            sys.exit(0)
        if allow_back and val.lower() == 'b':
            return 'BACK'
        return val

def interactive_wizard():
    """面向非技术人员的交互式向导模式"""
    selected_port = None
    file_paths = []
    target_dir = "0:/"
    
    step = 1
    while True:
        if step == 1:
            print("\n" + "=" * 65)
            print("          SkyStar BSP Ymodem 自动串口文件传输助手")
            print("    提示: 在任何步骤输入 'q' 退出程序，输入 'b' 返回上一步")
            print("=" * 65)
            
            # 1. 扫描串口并展示
            ports = scan_ports()
            if not ports:
                print("[ERROR] 未发现可用串口端口！")
                get_user_input("\n按【回车键】重新扫描，或输入 'q' 退出: ", allow_back=False)
                continue
                
            print("\n检测到以下串口设备:")
            for idx, (device, desc, status) in enumerate(ports, 1):
                print(f"  [{idx}] {device} - {desc} ({status})")
                
            choice = get_user_input(f"\n请选择串口编号 (输入 1-{len(ports)}, 或输入 'q' 退出): ", allow_back=False)
            try:
                idx = int(choice)
                if 1 <= idx <= len(ports):
                    selected_port = ports[idx-1][0]
                    if ports[idx-1][2] == "已被占用":
                        print("[WARNING] 该串口已被占用！运行前请务必先关闭串口调试助手。")
                    step = 2
                else:
                    print(f"[WARN] 请输入 1 到 {len(ports)} 之间的数字。")
            except ValueError:
                print("[WARN] 输入无效，请输入数字。")
                
        elif step == 2:
            # 步骤 2: 输入文件或文件夹路径
            path_input = get_user_input("\n请拖入要传输的文件/文件夹（或直接输入路径）: ")
            if path_input == 'BACK':
                step = 1
                continue
                
            path_input = path_input.strip('"').strip("'")
            if not path_input:
                continue
                
            if not os.path.exists(path_input):
                print(f"[ERROR] 路径不存在: {path_input}")
                continue
                
            file_paths = collect_files(path_input)
            if not file_paths:
                print(f"[ERROR] 该路径下未发现任何有效文件！")
                continue
                
            print(f"\n[INFO] 已准备好以下 {len(file_paths)} 个待传输文件:")
            for f in file_paths:
                print(f"  - {os.path.basename(f)} ({os.path.getsize(f)} 字节)")
            
            step = 3
            
        elif step == 3:
            # 步骤 3: 选择并输入接收端存储路径
            print("\n请输入接收端存储路径前缀。支持路径形式包括:")
            print("  - '0:/' (SD 卡根目录，默认)")
            print("  - 'flash/' (SPI Flash 根目录)")
            print("  - 也可以直接指定子目录，如 '0:/data/' 或 'flash/config/'")
            
            dir_input = get_user_input("请输入目标路径 [回车使用默认 '0:/']: ")
            if dir_input == 'BACK':
                step = 2
                continue
                
            if not dir_input:
                target_dir = "0:/"
            else:
                target_dir = dir_input.strip('"').strip("'").strip().replace('\\', '/')
                if not target_dir.endswith('/'):
                    target_dir += '/'
            
            print(f"\n[确认] 目标存储路径已设定为: {target_dir}")
            step = 4
            
        elif step == 4:
            # 步骤 4: 确认并开始传输
            print("\n" + "=" * 50)
            print(f"  串口端口 : {selected_port}")
            print(f"  文件总数 : {len(file_paths)} 个")
            print(f"  目标路径 : {target_dir}")
            print("=" * 50)
            
            flash_flag = " --flash" if target_dir == "flash/" else ""
            dir_flag = f" -d {target_dir}" if target_dir not in ("0:/", "flash/") else ""
            print(f"[提示] 专业命令行用法示范 (命令行亦支持 -d 参数指定绝对路径):")
            if len(file_paths) == 1:
                print(f"  python ymodem_sender.py -p {selected_port} -f \"{file_paths[0]}\"{dir_flag or flash_flag}")
            else:
                print(f"  python ymodem_sender.py -p {selected_port} -f \"文件夹路径\"{dir_flag or flash_flag}")
            print("-" * 50)
            
            confirm = get_user_input("\n请确认已关闭外部串口助手！是否开始传输？[y: 开始 | b: 返回上一步 | q: 退出]: ")
            if confirm == 'BACK':
                step = 3
                continue
            if confirm.lower() != 'y':
                print("[WARN] 输入无效，请输入 'y' 开始，或 'b' 返回，'q' 退出。")
                continue
                
            sender = None
            success = False
            try:
                sender = YmodemSender(selected_port, 115200)
                
                if target_dir == "flash/":
                    cmd = "ymodem_recv -flash"
                elif target_dir == "0:/":
                    cmd = "ymodem_recv"
                else:
                    cmd = f"ymodem_recv -d {target_dir}"
                    
                sender.send_shell_cmd(cmd)
                success = sender.send_files(file_paths)
                
                if success:
                    print("\n[SUCCESS] 批量文件传输已全部成功完成！")
                else:
                    print("\n[ERROR] 文件传输失败。")
            except Exception as e:
                print(f"\n[ERROR] 传输发生异常: {e}")
            finally:
                if sender:
                    sender.close()
                    print("[INFO] 串口连接已关闭。")
            
            # 传输完成后的规范菜单，不直接退出
            print("\n" + "=" * 50)
            print("  接下来您想做什么？")
            print("  [1] 重新传输当前文件列表")
            print("  [2] 传输其他新文件 (返回向导首步)")
            print("  [3] 退出程序")
            print("=" * 50)
            
            while True:
                next_action = get_user_input("请选择操作 [1-3]: ", allow_back=False)
                if next_action == '1':
                    step = 4
                    break
                elif next_action == '2':
                    step = 1
                    break
                elif next_action == '3' or next_action.lower() == 'q':
                    print("\n再见！")
                    sys.exit(0)
                else:
                    print("[WARN] 输入无效，请输入 1、2 或 3。")

def main():
    if len(sys.argv) == 1:
        interactive_wizard()
        return

    parser = argparse.ArgumentParser(description="Ymodem 串口自动传输工具 (STM32/Letter Shell 适配)")
    parser.add_argument("-p", "--port", help="串口设备名称 (例如: COM3 或 /dev/ttyUSB0)")
    parser.add_argument("-b", "--baud", type=int, default=115200, help="波特率 (默认: 115200)")
    parser.add_argument("-f", "--file", help="要传输的本地文件或文件夹路径")
    parser.add_argument("-d", "--dir", help="指定接收端目标存储路径 (例如: 0:/data/ 或 flash/config/)")
    parser.add_argument("--flash", action="store_true", help="指定写入目标为 SPI Flash 根目录 (即 -d flash/)")
    
    args = parser.parse_args()

    if not args.port or not args.file:
        print("[ERROR] 命令行模式下必须指定 -p/--port 与 -f/--file 参数。")
        print("例如: python ymodem_sender.py -p COM3 -f firmware.bin")
        print("\n如果不带任何参数直接双击运行，将自动进入友好交互向导模式。")
        sys.exit(1)

    file_paths = collect_files(args.file)
    if not file_paths:
        print(f"[ERROR] 找不到或解析不到任何有效文件: {args.file}")
        sys.exit(1)

    if args.flash:
        target_dir = "flash/"
    elif args.dir:
        target_dir = args.dir.strip('"').strip("'").strip().replace('\\', '/')
        if not target_dir.endswith('/'):
            target_dir += '/'
    else:
        target_dir = "0:/"

    sender = None
    try:
        sender = YmodemSender(args.port, args.baud)
        
        if target_dir == "flash/":
            cmd = "ymodem_recv -flash"
        elif target_dir == "0:/":
            cmd = "ymodem_recv"
        else:
            cmd = f"ymodem_recv -d {target_dir}"
            
        sender.send_shell_cmd(cmd)
        sender.send_files(file_paths)
        
    except Exception as e:
        print(f"[ERROR] 发生异常: {e}")
    finally:
        if sender:
            sender.close()
            print("[INFO] 串口连接已断开。")

if __name__ == "__main__":
    main()
