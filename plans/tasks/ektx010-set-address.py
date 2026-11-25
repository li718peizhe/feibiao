#!/usr/bin/env python3
"""EKT010 + 24V(485) 绿板称重变送器：改站号 / 改波特率 / 核对寄存器。

为什么要有这个脚本：寄存器 09（站号）、10（波特率）只读，Modbus 写无效，
只能发专用 5 字节 HEX 指令，而且**只在上电后 30 秒内响应**。手点串口助手
去抢这 30 秒不可靠，所以先让脚本跑起来循环重发，再给变送器上电。

必须先做的事情：总线上只接 1 台变送器。这条指令不带地址，两台同时在线会
被一起改成同一个站号。

依赖：pyserial（系统 python 已有 3.5；xrobot/.venv 里没装）。

用法（在 xrobot 根目录）：
  # 改站号为 2：先运行，看到“等待上电”再给变送器上电
  python plans\\tasks\\ektx010-set-address.py --addr 2

  # 顺带改成 115200（改完自动用新波特率复核）
  python plans\\tasks\\ektx010-set-address.py --addr 2 --baud-code 5

  # 只读校验：站号/波特率/读数，不改任何东西
  python plans\\tasks\\ektx010-set-address.py --verify --addr 2
  python plans\\tasks\\ektx010-set-address.py --scan

  # 改完站号顺手把拉皮筋用的参数写进去（07 转换速度 80Hz、08 零点跟踪关）
  python plans\\tasks\\ektx010-set-address.py --addr 2 --config 07=2,08=0

  # 只看要发什么，不开串口（无硬件时先核指令）
  python plans\\tasks\\ektx010-set-address.py --addr 2 --dry-run
"""

import argparse
import sys
import time

try:
    import serial
    from serial.tools import list_ports
except ImportError:
    sys.exit("缺少 pyserial：pip install pyserial")

# 专用指令：C4 3B <命令字> <校验字> <参数>
CMD_SET_ADDR = bytes([0xC4, 0x3B, 0xAA, 0x55])   # + 站号 1 字节
CMD_SET_BAUD = bytes([0xC4, 0x3B, 0xA9, 0x56])   # + 波特率码 1 字节
BAUD_TABLE = {0: 4800, 1: 9600, 2: 19200, 3: 38400, 4: 57600, 5: 115200}

REG_DECIMAL_POINT = 1
REG_VALUE_HI = 2
REG_VALUE_LO = 3
REG_FILTER = 6
REG_AD_SPEED = 7
REG_ZERO_TRACK = 8
REG_ADDR = 9
REG_BAUD = 10
REG_VALUE_TYPE = 11


def crc16(data):
    """Modbus RTU CRC，低字节在前。"""
    crc = 0xFFFF
    for byte in data:
        crc ^= byte
        for _ in range(8):
            crc = (crc >> 1) ^ 0xA001 if crc & 1 else crc >> 1
    return bytes([crc & 0xFF, crc >> 8])


def frame(*parts):
    body = bytes(parts)
    return body + crc16(body)


def read_regs_frame(addr, start, count):
    return frame(addr, 0x03, 0x00, start, 0x00, count)


def write_reg_frame(addr, reg, value):
    return frame(addr, 0x06, 0x00, reg, (value >> 8) & 0xFF, value & 0xFF)


def hexs(data):
    return " ".join(f"{b:02X}" for b in data)


def auto_port():
    """自动挑一个不像蓝牙的串口。"""
    candidates = []
    for port in list_ports.comports():
        hwid = (port.hwid or "").upper()
        desc = port.description or ""
        if "BTHENUM" in hwid or "蓝牙" in desc or "Bluetooth" in desc:
            continue
        candidates.append(port.device)
    if not candidates:
        sys.exit("没找到可用串口（USB-TTL 插上了吗？用 --port COMx 指定）")
    return candidates[0]


def open_port(port, baud):
    ser = serial.Serial(port, baud, bytesize=8, parity="N", stopbits=1, timeout=0.02)
    ser.reset_input_buffer()
    return ser


def drain(ser, wait=0.25, quiet=0.06):
    """收一帧：最多等 wait 秒，收到数据后再静默 quiet 秒就返回。"""
    deadline = time.monotonic() + wait
    quiet_deadline = None
    data = b""
    while time.monotonic() < deadline:
        chunk = ser.read(ser.in_waiting or 1)
        now = time.monotonic()
        if chunk:
            data += chunk
            quiet_deadline = now + quiet
        elif quiet_deadline is not None and now > quiet_deadline:
            break
    return data


def read_regs(ser, addr, start, count, expect_reply=True):
    """发 0x03 读寄存器，返回整数列表；没回应返回 None。"""
    ser.reset_input_buffer()
    ser.write(read_regs_frame(addr, start, count))
    ser.flush()
    reply = drain(ser)
    if not reply:
        return None
    if len(reply) < 5:
        print(f"  回应过短：{hexs(reply)}")
        return None
    if reply[0] != addr or reply[1] != 0x03:
        print(f"  回应异常（期望站号 {addr} 功能码 03）：{hexs(reply)}")
        return None
    byte_count = reply[2]
    if len(reply) < 3 + byte_count + 2:
        print(f"  回应长度不够：{hexs(reply)}")
        return None
    values = [reply[3 + i * 2] << 8 | reply[4 + i * 2] for i in range(byte_count // 2)]
    return values


def show_status(ser, addr):
    regs = read_regs(ser, addr, REG_DECIMAL_POINT, 3)
    if regs is None:
        print(f"  站号 {addr} 无应答（波特率不对，或这个站号上没设备）")
    else:
        point, hi, lo = regs
        print(f"  站号 {addr}：小数点={point}  02/03={hi}/{lo}  （11=0 时 03 就是显示值，02 是符号）")
    cfg = read_regs(ser, addr, REG_FILTER, 6)   # 06..11
    if cfg is not None and len(cfg) == 6:
        filt, speed, track, r_addr, r_baud, vtype = cfg
        print(f"  06 滤波={filt}  07 转换速度={speed}（0=10Hz 1=20Hz 2=80Hz）  "
              f"08 零点跟踪={track}  09 站号={r_addr}  10 波特率码={r_baud}"
              f"（{BAUD_TABLE.get(r_baud, '?')}）  11 显示类型={vtype}")
    return cfg


def run_special(ser, cmd, label, window, interval):
    """循环发专用指令，靠循环覆盖上电 30 秒窗口。"""
    print(f"{label}：{hexs(cmd)}")
    print(f"  现在给变送器上电（窗口 {window:.0f} s，每 {interval * 1000:.0f} ms 重发一次，Ctrl+C 退出）")
    start = time.monotonic()
    next_report = start + 5.0
    try:
        while time.monotonic() - start < window:
            ser.reset_input_buffer()
            ser.write(cmd)
            ser.flush()
            reply = drain(ser, 0.2)
            if cmd in reply:
                elapsed = time.monotonic() - start
                print(f"  第 {elapsed:.1f} s 收到原样返回：{hexs(reply)}  → 成功")
                return True
            if reply:
                print(f"  收到非预期回应：{hexs(reply)}")
            now = time.monotonic()
            if now >= next_report:
                print(f"  ...等待中 {now - start:.0f} s（还没上电或已过 30 s 窗口，可断电重来）")
                next_report = now + 5.0
            time.sleep(max(0.0, interval - 0.2))
    except KeyboardInterrupt:
        print("\n  用户中断")
        return False
    print(f"  {window:.0f} s 内没收到回应")
    return False


def parse_config(text):
    pairs = []
    for item in text.split(","):
        item = item.strip()
        if not item:
            continue
        reg, value = item.split("=")
        pairs.append((int(reg), int(value)))
    return pairs


def main():
    parser = argparse.ArgumentParser(description="EKT010 485 变送器改站号/波特率/核对寄存器")
    parser.add_argument("--port", help="串口号，例如 COM47；省略则自动选")
    parser.add_argument("--baud", type=int, default=9600, help="当前波特率（默认 9600）")
    parser.add_argument("--addr", type=int, default=2, help="目标站号（默认 2，范围 1~99）")
    parser.add_argument("--baud-code", type=int, choices=sorted(BAUD_TABLE), help="同时改波特率，0/1/2/3/4/5")
    parser.add_argument("--config", help='改完站号后写寄存器，例如 "07=2,08=0"')
    parser.add_argument("--window", type=float, default=45.0, help="等待上电的最长秒数（默认 45）")
    parser.add_argument("--interval", type=float, default=0.2, help="重发间隔秒（默认 0.2）")
    parser.add_argument("--verify", action="store_true", help="只读校验，不发改名指令")
    parser.add_argument("--scan", action="store_true", help="在 1~8 号里找谁应答")
    parser.add_argument("--dry-run", action="store_true", help="只打印要发的帧")
    args = parser.parse_args()

    set_addr_cmd = CMD_SET_ADDR + bytes([args.addr])
    set_baud_cmd = CMD_SET_BAUD + bytes([args.baud_code]) if args.baud_code is not None else None

    if args.dry_run:
        print(f"改站号 → {hexs(set_addr_cmd)}")
        if set_baud_cmd:
            print(f"改波特率 → {hexs(set_baud_cmd)}（{BAUD_TABLE[args.baud_code]}）")
        print(f"复核站号/波特率（站号 {args.addr}）→ {hexs(read_regs_frame(args.addr, REG_ADDR, 2))}")
        print(f"读显示值（站号 {args.addr}）→ {hexs(read_regs_frame(args.addr, REG_VALUE_HI, 2))}")
        if args.config:
            for reg, value in parse_config(args.config):
                print(f"写寄存器 {reg:02d}={value} → {hexs(write_reg_frame(args.addr, reg, value))}")
        return 0

    port = args.port or auto_port()
    work_baud = args.baud
    print(f"串口 {port} @ {work_baud}-8-N-1")

    if args.scan:
        ser = open_port(port, work_baud)
        try:
            found = [a for a in range(1, 9) if read_regs(ser, a, REG_ADDR, 2) is not None]
            print(f"应答的站号：{found if found else '无'}")
            for addr in found:
                show_status(ser, addr)
        finally:
            ser.close()
        return 0 if found else 1

    if args.verify:
        ser = open_port(port, work_baud)
        try:
            show_status(ser, args.addr)
        finally:
            ser.close()
        return 0

    ser = open_port(port, work_baud)
    try:
        if not run_special(ser, set_addr_cmd, f"改站号为 {args.addr}", args.window, args.interval):
            print("站号没改成功。检查：只接了 1 台变送器、A/B 没接反、供电正常、上电 30 s 内、当前波特率正确。")
            return 1
        time.sleep(0.8)
        print(f"\n复核（站号 {args.addr}）：")
        if show_status(ser, args.addr) is None:
            print("  注意：改完立即读不到，先断电重新上电再看，或用 --scan 找一下实际站号")

        if set_baud_cmd:
            time.sleep(0.3)
            if not run_special(ser, set_baud_cmd, f"改波特率为 {BAUD_TABLE[args.baud_code]}",
                               args.window, args.interval):
                print("波特率没改成功（站号已改好，可以单独再来一次）。")
                return 1
            time.sleep(0.8)
            ser.close()
            work_baud = BAUD_TABLE[args.baud_code]
            print(f"\n按新波特率 {work_baud} 重开串口复核：")
            ser = open_port(port, work_baud)
            show_status(ser, args.addr)

        if args.config:
            print("\n写配置寄存器：")
            for reg, value in parse_config(args.config):
                cmd = write_reg_frame(args.addr, reg, value)
                ser.reset_input_buffer()
                ser.write(cmd)
                ser.flush()
                reply = drain(ser, 0.25)
                ok = cmd[:6] == reply[:6] if len(reply) >= 6 else False
                print(f"  {reg:02d}={value}  {hexs(cmd)}  {'回显一致' if ok else '回应：' + hexs(reply)}")
                time.sleep(0.15)
            print("\n写后复核：")
            show_status(ser, args.addr)
    finally:
        ser.close()

    print("\n完成。两台都改的话：断电，只接另一台，再跑一次；改完再并联。")
    return 0


if __name__ == "__main__":
    sys.exit(main())
