#!/usr/bin/env python3
"""假裁判系统：电脑通过 USB 转 TTL 往 C 板的裁判串口（UART6）发 2027 通信协议的帧，在家演练整场比赛流程。

接线：C 板外壳丝印 UART1 的那个口是 3 针（C 板手册 L283-309），就是 STM32 的 UART6，固件把它配给了裁判系统。
线序 1 GND、2 TXD（板子发）、3 RXD（板子收）。USB 转 TTL 的 GND 接第 1 针，TX 接第 3 针（RXD）；
这个工具只发不收，USB 转 TTL 的 RX 可以不接（要接就接第 2 针）。不接 5 V。波特率 115200，8N1。
（丝印 UART2 的 4 针口是 STM32 的 UART1，别接错：它留给以后的调试日志。）

只用标准库；装了 pyserial 就用它，没装就走 Windows 自带的串口接口。

用法（在 C:\\Users\\Lenovo\\Desktop\\xrobot 下）：
    .venv\\Scripts\\python.exe xrobot-local\\DartLauncher\\tools\\fake_referee.py --port COM5
    ... --port COM5 --speed 5 --auto        # 时钟 5 倍速，闸门按剧本自动开
    ... --list                              # 列出电脑上的串口，找出 USB 转 TTL 是哪个口
    ... --selftest                          # 不接板子，自检帧格式和比赛剧本
    ... --dry-run                           # 不开串口，只打印帧

发的包和真实频率一致：0x0001 比赛状态 1 Hz、0x0105 飞镖发射相关 1 Hz、0x020A 飞镖选手端指令 3 Hz，
另外发 0x0201（17 字节，2027 新长度）10 Hz、0x0003（24 字节）3 Hz、0x0101（8 字节）1 Hz，用来确认模块认得新长度、不被不相关的包带歪。
"""
import argparse
import math
import struct
import sys
import time

# ---- 帧格式（协议第 1.1 节）：0xA5 + 数据长度(2) + 包序号 + CRC8，再接命令码(2) + 数据 + 整包 CRC16，全部小端 ----


def _make_crc8_table():
    table = []
    for i in range(256):
        crc = i
        for _ in range(8):
            crc = (crc >> 1) ^ 0x8C if crc & 1 else crc >> 1
        table.append(crc)
    return table


def _make_crc16_table():
    table = []
    for i in range(256):
        crc = i
        for _ in range(8):
            crc = (crc >> 1) ^ 0x8408 if crc & 1 else crc >> 1
        table.append(crc)
    return table


CRC8_TAB = _make_crc8_table()
CRC16_TAB = _make_crc16_table()


def crc8(data):
    crc = 0xFF
    for byte in data:
        crc = CRC8_TAB[(crc ^ byte) & 0xFF]
    return crc


def crc16(data):
    crc = 0xFFFF
    for byte in data:
        crc = CRC16_TAB[(crc ^ byte) & 0xFF] ^ (crc >> 8)
    return crc


def build_frame(cmd_id, payload, seq):
    header = struct.pack("<BHB", 0xA5, len(payload), seq & 0xFF)
    header += bytes([crc8(header)])
    body = header + struct.pack("<H", cmd_id) + bytes(payload)
    return body + struct.pack("<H", crc16(body))


def parse_frame(frame):
    """按模块的收帧顺序校验一帧，返回 (命令码, 包序号, 数据)；校验不过抛 ValueError。"""
    if len(frame) < 9 or frame[0] != 0xA5:
        raise ValueError("帧头不对")
    length, seq = struct.unpack_from("<HB", frame, 1)
    if crc8(frame[:4]) != frame[4]:
        raise ValueError("CRC8 不对")
    if len(frame) != 5 + 2 + length + 2:
        raise ValueError("长度不对")
    if crc16(frame[:-2]) != struct.unpack_from("<H", frame, len(frame) - 2)[0]:
        raise ValueError("CRC16 不对")
    cmd_id = struct.unpack_from("<H", frame, 5)[0]
    return cmd_id, seq, frame[7:-2]


# ---- 各个包的数据段（协议第 1.2 节） ----
CMD_GAME_STATUS = 0x0001
CMD_ROBOT_HP = 0x0003
CMD_FIELD_EVENT = 0x0101
CMD_DART_INFO = 0x0105
CMD_ROBOT_STATUS = 0x0201
CMD_DART_CLIENT = 0x020A


def game_status(stage, remaining, unix_time):
    # bit 0-3 比赛类型（1 超级对抗赛），bit 4-7 比赛阶段；之后 2 字节阶段剩余秒数、8 字节 UNIX 时间。
    return struct.pack("<BHQ", (stage << 4) | 1, remaining, unix_time)


def dart_info(remaining, last_hit, hits, target):
    # 第 0 字节发射剩余秒数；之后 2 字节：bit 0-2 最近命中的目标，bit 3-5 被命中次数，bit 6-8 选中的目标。
    return struct.pack("<BH", remaining, (last_hit & 7) | ((hits & 7) << 3) | ((target & 7) << 6))


def dart_client(gate_status, target_change_time, launch_time):
    # 发射站状态：0 已开启，1 关闭，2 正在开启或关闭；再保留 1 字节，切换目标时和最后一次确认发射时的比赛剩余秒数。
    return struct.pack("<BBHH", gate_status, 0, target_change_time, launch_time)


def robot_status(robot_id=8):
    # 2027 新长度 17 字节：多了偏移 12 的 4 字节射击初速度上限，电源管理模块输出位挪到偏移 16。
    return struct.pack("<BBHHHHHIB", robot_id, 1, 200, 200, 0, 0, 0, 0, 0x04)


def robot_hp():
    return struct.pack("<12H", *([200] * 12))


def field_event():
    return struct.pack("<II", 0, 0)


# ---- 比赛剧本 ----
GATE_OPEN, GATE_CLOSED, GATE_MOVING = 0, 1, 2
GATE_NAMES = {GATE_OPEN: "已开启", GATE_CLOSED: "关闭", GATE_MOVING: "开关中"}
GATE_MOVE_TIME = 7.0   # 闸门完全打开或关上约 7 s（规则 L2298-2307）
GATE_WINDOW = 30.0     # 全开后 30 s 内可以发射，过了自动关
# (名称, 比赛阶段编号, 时长 s)；最后一个阶段不会结束。
STAGES = [("未开始", 0, 3.0), ("准备阶段", 1, 180.0), ("15 s 自检", 2, 15.0),
          ("5 s 倒计时", 3, 5.0), ("比赛中", 4, 420.0), ("结算中", 5, None)]
TARGET_NAMES = ["前哨站", "基地固定", "随机固定", "随机移动", "末端移动"]


class Match:
    """比赛时钟、闸门、目标选择和发射指令。时间都是比赛里的秒，dt 由调用方按倍速换算好。"""

    def __init__(self, auto_gate=False, start_stage=0):
        self.index = start_stage
        self.elapsed = 0.0
        self.paused = False
        self.gate = GATE_CLOSED
        self.gate_time = 0.0
        self.target = 0
        self.target_change_time = 0
        self.launch_time = 0
        self.hits = 0
        self.last_hit = 0
        self.auto_gate = auto_gate
        self.auto_fired = set()

    @property
    def stage(self):
        return STAGES[self.index][1]

    @property
    def stage_name(self):
        return STAGES[self.index][0]

    def remaining(self):
        duration = STAGES[self.index][2]
        return 0 if duration is None else max(0, int(math.ceil(duration - self.elapsed)))

    def match_remaining(self):
        """给 0x020A 用的“比赛剩余时间”：比赛中是本阶段剩余，开赛前是整场 420 s，结算后是 0。"""
        if self.stage == 4:
            return self.remaining()
        return 420 if self.stage < 4 else 0

    def next_stage(self):
        if self.index < len(STAGES) - 1:
            self.index += 1
            self.elapsed = 0.0
            if self.stage != 4:
                self.auto_fired.clear()

    def open_gate(self):
        if self.gate == GATE_CLOSED:
            self.gate = GATE_MOVING
            self.gate_time = 0.0
            self._opening = True

    def close_gate(self):
        if self.gate == GATE_OPEN:
            self.gate = GATE_MOVING
            self.gate_time = 0.0
            self._opening = False

    def cycle_target(self):
        self.target = (self.target + 1) % len(TARGET_NAMES)
        self.target_change_time = self.match_remaining()

    def confirm_launch(self):
        self.launch_time = self.match_remaining()

    def hit(self):
        self.last_hit = self.target + 1 if self.target else 1
        self.hits = min(4, self.hits + 1)

    def step(self, dt):
        if self.paused or dt <= 0:
            return
        self.elapsed += dt
        duration = STAGES[self.index][2]
        while duration is not None and self.elapsed >= duration:
            self.elapsed -= duration
            self.next_stage()
            duration = STAGES[self.index][2]
        if self.stage == 2:
            # 15 s 自检里闸门会开约 5 s，剧本里是第 1~6 秒；这时发射架绝不能发射。
            self.gate = GATE_OPEN if 1.0 <= self.elapsed < 6.0 else GATE_CLOSED
            return
        if self.auto_gate and self.stage == 4:
            for start in (30.0, 240.0):
                if self.elapsed >= start and start not in self.auto_fired:
                    self.auto_fired.add(start)
                    self.open_gate()
        if self.gate == GATE_MOVING:
            self.gate_time += dt
            if self.gate_time >= GATE_MOVE_TIME:
                self.gate = GATE_OPEN if self._opening else GATE_CLOSED
                self.gate_time = 0.0
        elif self.gate == GATE_OPEN:
            self.gate_time += dt
            if self.gate_time >= GATE_WINDOW:
                self.close_gate()

    def dart_remaining(self):
        """0x0105 第 0 字节：协议只写“己方飞镖发射剩余时间”。剧本里在全开后从 30 s 往下数，其余时间为 0。"""
        if self.gate == GATE_OPEN:
            return max(0, int(math.ceil(GATE_WINDOW - self.gate_time)))
        return 0

    _opening = True


# ---- 串口 ----
class DryPort:
    def write(self, data):
        print(data.hex(" "))

    def close(self):
        pass


def open_port(name, baud):
    try:
        import serial  # pyserial

        class SerialPort:
            def __init__(self):
                self.port = serial.Serial(name, baud, bytesize=8, parity="N", stopbits=1, write_timeout=1)

            def write(self, data):
                self.port.write(data)

            def close(self):
                self.port.close()

        return SerialPort()
    except ImportError:
        return Win32Port(name, baud)


def list_ports():
    """列出 Windows 上现有的串口，返回 [(设备名, 口名)]；查不到就返回空。"""
    try:
        import winreg

        key = winreg.OpenKey(winreg.HKEY_LOCAL_MACHINE, r"HARDWARE\DEVICEMAP\SERIALCOMM")
    except (ImportError, OSError):
        return []
    ports = []
    index = 0
    while True:
        try:
            device, port, _ = winreg.EnumValue(key, index)
        except OSError:
            break
        ports.append((device, port))
        index += 1
    return ports


class Win32Port:
    """没装 pyserial 时的备用：直接调 Windows 的串口接口，只写不读。"""

    def __init__(self, name, baud):
        import ctypes
        from ctypes import wintypes

        class DCB(ctypes.Structure):
            _fields_ = [("DCBlength", wintypes.DWORD), ("BaudRate", wintypes.DWORD),
                        ("flags", wintypes.DWORD), ("wReserved", wintypes.WORD),
                        ("XonLim", wintypes.WORD), ("XoffLim", wintypes.WORD),
                        ("ByteSize", wintypes.BYTE), ("Parity", wintypes.BYTE),
                        ("StopBits", wintypes.BYTE), ("XonChar", ctypes.c_char),
                        ("XoffChar", ctypes.c_char), ("ErrorChar", ctypes.c_char),
                        ("EofChar", ctypes.c_char), ("EvtChar", ctypes.c_char),
                        ("wReserved1", wintypes.WORD)]

        class Timeouts(ctypes.Structure):
            _fields_ = [("ReadIntervalTimeout", wintypes.DWORD), ("ReadTotalTimeoutMultiplier", wintypes.DWORD),
                        ("ReadTotalTimeoutConstant", wintypes.DWORD), ("WriteTotalTimeoutMultiplier", wintypes.DWORD),
                        ("WriteTotalTimeoutConstant", wintypes.DWORD)]

        self._ctypes = ctypes
        self._wintypes = wintypes
        kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
        kernel32.CreateFileW.restype = wintypes.HANDLE
        kernel32.CreateFileW.argtypes = [wintypes.LPCWSTR, wintypes.DWORD, wintypes.DWORD, ctypes.c_void_p,
                                         wintypes.DWORD, wintypes.DWORD, wintypes.HANDLE]
        kernel32.WriteFile.argtypes = [wintypes.HANDLE, ctypes.c_void_p, wintypes.DWORD,
                                       ctypes.POINTER(wintypes.DWORD), ctypes.c_void_p]
        kernel32.GetCommState.argtypes = [wintypes.HANDLE, ctypes.POINTER(DCB)]
        kernel32.SetCommState.argtypes = [wintypes.HANDLE, ctypes.POINTER(DCB)]
        kernel32.SetCommTimeouts.argtypes = [wintypes.HANDLE, ctypes.POINTER(Timeouts)]
        self._kernel32 = kernel32
        handle = kernel32.CreateFileW("\\\\.\\" + name, 0xC0000000, 0, None, 3, 0, None)
        if handle in (None, wintypes.HANDLE(-1).value):
            raise OSError("打不开串口 %s（错误码 %d）：口名对吗？别的软件占着它吗？" % (name, ctypes.get_last_error()))
        self._handle = handle
        dcb = DCB()
        dcb.DCBlength = ctypes.sizeof(DCB)
        if not kernel32.GetCommState(handle, ctypes.byref(dcb)):
            raise OSError("读串口参数失败（错误码 %d）" % ctypes.get_last_error())
        dcb.BaudRate = baud
        dcb.ByteSize = 8
        dcb.Parity = 0
        dcb.StopBits = 0
        dcb.flags = 0x0001 | 0x0010  # fBinary，DTR 使能；不开任何流控
        if not kernel32.SetCommState(handle, ctypes.byref(dcb)):
            raise OSError("设串口参数失败（错误码 %d）" % ctypes.get_last_error())
        timeouts = Timeouts(0, 0, 0, 0, 1000)
        kernel32.SetCommTimeouts(handle, ctypes.byref(timeouts))

    def write(self, data):
        written = self._wintypes.DWORD(0)
        buffer = self._ctypes.create_string_buffer(bytes(data))
        if not self._kernel32.WriteFile(self._handle, buffer, len(data), self._ctypes.byref(written), None):
            raise OSError("写串口失败（错误码 %d）" % self._ctypes.get_last_error())

    def close(self):
        self._kernel32.CloseHandle(self._handle)


# ---- 发送调度 ----
class Sender:
    """按真实频率发包。period 是真实秒，和倍速无关。"""

    def __init__(self, port, match):
        self.port = port
        self.match = match
        self.seq = 0
        self.sent = 0
        self.silent = False        # 全部不发：演练裁判系统掉线
        self.silent_client = False  # 只不发 0x020A
        self.start = time.monotonic()
        self.last = {}
        self.tasks = [
            (CMD_GAME_STATUS, 1.0, self._game_status),
            (CMD_DART_INFO, 1.0, self._dart_info),
            (CMD_DART_CLIENT, 1.0 / 3, self._dart_client),
            (CMD_ROBOT_STATUS, 0.1, lambda: robot_status()),
            (CMD_ROBOT_HP, 1.0 / 3, lambda: robot_hp()),
            (CMD_FIELD_EVENT, 1.0, lambda: field_event()),
        ]

    def _game_status(self):
        return game_status(self.match.stage, self.match.remaining(), int(time.time()))

    def _dart_info(self):
        m = self.match
        return dart_info(m.dart_remaining(), m.last_hit, m.hits, m.target)

    def _dart_client(self):
        m = self.match
        return dart_client(m.gate, m.target_change_time, m.launch_time)

    def poll(self, now):
        if self.silent:
            return
        for cmd_id, period, make in self.tasks:
            if cmd_id == CMD_DART_CLIENT and self.silent_client:
                continue
            if now - self.last.get(cmd_id, -1e9) >= period:
                self.last[cmd_id] = now
                self.port.write(build_frame(cmd_id, make(), self.seq))
                self.seq = (self.seq + 1) & 0xFF
                self.sent += 1


HELP = ("按键：o 开闸门  c 关闸门  t 换目标  l 确认发射  h 记一次命中  n 下一阶段  空格 暂停  "
        "x 全部不发（演练裁判掉线）  z 只不发 0x020A  ? 帮助  q 退出")


def status_line(match, sender, speed):
    return ("阶段=%s(%d) 剩余=%ds 闸门=%s 目标=%s 发射指令时刻=%d 倍速x%g %s已发%d帧%s%s" % (
        match.stage_name, match.stage, match.remaining(), GATE_NAMES[match.gate], TARGET_NAMES[match.target],
        match.launch_time, speed, "【暂停】" if match.paused else "", sender.sent,
        "【全部不发】" if sender.silent else "", "【0x020A 不发】" if sender.silent_client else ""))


def run(args):
    match = Match(auto_gate=args.auto, start_stage=args.stage)
    port = DryPort() if args.dry_run else open_port(args.port, args.baud)
    sender = Sender(port, match)
    print("假裁判系统已启动，%s。" % ("只打印帧" if args.dry_run else "串口 %s %d 8N1" % (args.port, args.baud)))
    print(HELP)
    try:
        import msvcrt
    except ImportError:
        msvcrt = None
    previous = time.monotonic()
    next_print = previous
    try:
        while True:
            now = time.monotonic()
            match.step((now - previous) * args.speed)
            previous = now
            sender.poll(now)
            if msvcrt is not None:
                while msvcrt.kbhit():
                    key = msvcrt.getwch().lower()
                    if key in ("q", "\x1b"):
                        return
                    if key == "o":
                        match.open_gate()
                    elif key == "c":
                        match.close_gate()
                    elif key == "t":
                        match.cycle_target()
                    elif key == "l":
                        match.confirm_launch()
                    elif key == "h":
                        match.hit()
                    elif key == "n":
                        match.next_stage()
                    elif key == " ":
                        match.paused = not match.paused
                    elif key == "x":
                        sender.silent = not sender.silent
                    elif key == "z":
                        sender.silent_client = not sender.silent_client
                    elif key == "?":
                        print("\n" + HELP)
            if now >= next_print and not args.dry_run:
                next_print = now + 1.0
                print("\r" + status_line(match, sender, args.speed) + "    ", end="", flush=True)
            time.sleep(0.005)
    except KeyboardInterrupt:
        pass
    finally:
        port.close()
        print()


# ---- 自检 ----
def selftest():
    failures = []

    def check(condition, label):
        if not condition:
            failures.append(label)

    # CRC 表的前 16 项和协议附录一（以及 LibXR）一致。
    check(CRC8_TAB[:16] == [0x00, 0x5E, 0xBC, 0xE2, 0x61, 0x3F, 0xDD, 0x83, 0xC2, 0x9C, 0x7E, 0x20, 0xA3, 0xFD, 0x1F, 0x41],
          "CRC8 表前 16 项")
    check(CRC16_TAB[:16] == [0x0000, 0x1189, 0x2312, 0x329B, 0x4624, 0x57AD, 0x6536, 0x74BF,
                             0x8C48, 0x9DC1, 0xAF5A, 0xBED3, 0xCA6C, 0xDBE5, 0xE97E, 0xF8F7], "CRC16 表前 16 项")
    # 包的长度和协议一致。
    check(len(game_status(4, 420, 0)) == 11, "0x0001 长 11")
    check(len(dart_info(0, 0, 0, 0)) == 3, "0x0105 长 3")
    check(len(dart_client(1, 0, 0)) == 6, "0x020A 长 6")
    check(len(robot_status()) == 17, "0x0201 长 17")
    check(len(robot_hp()) == 24, "0x0003 长 24")
    check(len(field_event()) == 8, "0x0101 长 8")
    # 数据段的位布局：末端移动（4）占 bit 6-8，不能被截成 0。
    info = dart_info(30, 6, 4, 4)
    check(info == bytes([30, 0x26, 0x01]), "0x0105 位布局")
    check(game_status(4, 388, 0)[:3] == bytes([0x41, 388 & 0xFF, 388 >> 8]), "0x0001 位布局")
    # 整帧能按模块的顺序校验通过，坏一位就不过。
    for cmd_id, payload in ((CMD_GAME_STATUS, game_status(4, 420, 123)), (CMD_DART_CLIENT, dart_client(0, 5, 7)),
                            (CMD_ROBOT_STATUS, robot_status())):
        frame = build_frame(cmd_id, payload, 200)
        parsed = parse_frame(frame)
        check(parsed == (cmd_id, 200, payload), "帧往返 0x%04X" % cmd_id)
        for index in range(len(frame)):
            damaged = bytearray(frame)
            damaged[index] ^= 0x01
            try:
                parse_frame(bytes(damaged))
                check(False, "坏一位应该校验不过 0x%04X 第 %d 字节" % (cmd_id, index))
            except ValueError:
                pass
    # Windows 串口结构体大小。
    if sys.platform == "win32":
        import ctypes
        from ctypes import wintypes

        class DCB(ctypes.Structure):
            _fields_ = [("DCBlength", wintypes.DWORD), ("BaudRate", wintypes.DWORD),
                        ("flags", wintypes.DWORD), ("wReserved", wintypes.WORD),
                        ("XonLim", wintypes.WORD), ("XoffLim", wintypes.WORD),
                        ("ByteSize", wintypes.BYTE), ("Parity", wintypes.BYTE),
                        ("StopBits", wintypes.BYTE), ("XonChar", ctypes.c_char),
                        ("XoffChar", ctypes.c_char), ("ErrorChar", ctypes.c_char),
                        ("EofChar", ctypes.c_char), ("EvtChar", ctypes.c_char),
                        ("wReserved1", wintypes.WORD)]

        check(ctypes.sizeof(DCB) == 28, "DCB 结构体 28 字节")
    # 剧本：阶段依次走完，自检里闸门开 5 s，比赛中按剧本开两次闸门，窗口 30 s 后自动关。
    match = Match(auto_gate=True)
    seen_stages = []
    gate_in_self_check = 0.0
    open_in_match = []
    previous_gate = match.gate
    dt = 0.05
    for _ in range(int(700 / dt)):
        match.step(dt)
        if not seen_stages or seen_stages[-1] != match.stage:
            seen_stages.append(match.stage)
        if match.stage == 2 and match.gate == GATE_OPEN:
            gate_in_self_check += dt
        if match.stage == 4 and match.gate == GATE_OPEN and previous_gate != GATE_OPEN:
            open_in_match.append(420 - match.remaining())
        previous_gate = match.gate
    check(seen_stages == [0, 1, 2, 3, 4, 5], "阶段顺序 %s" % seen_stages)
    check(abs(gate_in_self_check - 5.0) < 0.2, "自检里闸门开约 5 s：%.2f" % gate_in_self_check)
    check(len(open_in_match) == 2 and abs(open_in_match[0] - 37) <= 1 and abs(open_in_match[1] - 247) <= 1,
          "比赛中两次全开约在 37 s 和 247 s：%s" % open_in_match)
    check(match.stage == 5 and match.match_remaining() == 0, "结算后比赛剩余为 0")
    # 手动开闸、窗口倒计时和换目标、确认发射。
    manual = Match(start_stage=4)
    manual.open_gate()
    manual.step(6.9)
    check(manual.gate == GATE_MOVING and manual.dart_remaining() == 0, "开闸中还不是全开")
    manual.step(0.2)
    check(manual.gate == GATE_OPEN and manual.dart_remaining() == 30, "7 s 后全开")
    manual.step(10)
    check(manual.dart_remaining() == 20, "全开后倒计时")
    manual.cycle_target()
    manual.confirm_launch()
    check(manual.target == 1 and manual.target_change_time == manual.remaining() == manual.launch_time and manual.launch_time > 0,
          "换目标和确认发射记下比赛剩余时间")
    manual.step(30)
    check(manual.gate in (GATE_MOVING, GATE_CLOSED), "窗口用完自动关")
    if failures:
        print("自检失败：")
        for label in failures:
            print("  -", label)
        return 1
    print("自检通过：帧格式、CRC、包长、位布局、比赛剧本都对。")
    return 0


def main():
    parser = argparse.ArgumentParser(description="假裁判系统：往 C 板的裁判串口发 2027 通信协议的帧")
    parser.add_argument("--port", help="串口名，比如 COM5（USB 转 TTL 的口）")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--speed", type=float, default=1.0, help="比赛时钟倍速（帧频率不变），默认 1")
    parser.add_argument("--stage", type=int, default=0, help="从第几个阶段开始，0 未开始 1 准备 2 自检 3 倒计时 4 比赛中 5 结算")
    parser.add_argument("--auto", action="store_true", help="比赛中按剧本自动开闸门（开赛后 30 s 和 240 s）")
    parser.add_argument("--dry-run", action="store_true", help="不开串口，只打印帧")
    parser.add_argument("--selftest", action="store_true", help="不接板子，自检帧格式和比赛剧本")
    parser.add_argument("--list", action="store_true", help="列出电脑上现有的串口，找出 USB 转 TTL 是哪个口")
    args = parser.parse_args()
    if args.selftest:
        return selftest()
    if args.list:
        ports = list_ports()
        if not ports:
            print("没查到串口。USB 转 TTL 插好了吗？")
        for device, port in ports:
            print("%s  (%s)" % (port, device))
        return 0
    if not args.dry_run and not args.port:
        parser.error("要给 --port（比如 COM5），或者用 --dry-run / --selftest")
    if not 0 <= args.stage < len(STAGES):
        parser.error("--stage 要在 0~5 之间")
    try:
        run(args)
    except OSError as error:
        print("串口出错：%s\n可以先运行一遍 --list 看现有的串口口名。" % error)
        return 2
    return 0


if __name__ == "__main__":
    sys.exit(main())
