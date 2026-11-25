"""Read-only SWD trace of the arm for the 3u 5be996c firmware; no reset, halt or motor commands."""
import argparse
import csv
import hashlib
import math
import socket
import struct
import subprocess
import sys
import time
from datetime import datetime
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
ELF = ROOT / "dart-fw" / "build" / "debug" / "DevC.elf"
EXPECTED_SHA256 = "765DA104360E0CCC59D25C005161715AE1EB56F4733392F7F20BA61AF9B8FA68"
OPENOCD = Path("C:/Users/Lenovo/.eide/tools/openocd_7a1adfbec_mingw32/bin/openocd.exe")

# Addresses are from the matching ELF (arm-none-eabi-gdb on DartLauncher 5be996c / dart-fw d38c85b).
CONFIG_ADDRESS = 0x200039E0  # arm AxisConfig
ARM_ADDRESS = 0x20003B98  # arm speed-loop integral, followed by MotorAxis/MotorPort/StallGuard fields
REMOTE_ADDRESS = 0x20004C38
MODE_ADDRESS = 0x200049C0
TICKS_ADDRESS = 0x20004A10
COUNTS_PER_RAD = 8192 / (2 * math.pi)


def request(sock, command):
    sock.sendall(command.encode("ascii") + b"\x1a")
    data = bytearray()
    while not data.endswith(b"\x1a"):
        chunk = sock.recv(8192)
        if not chunk:
            raise RuntimeError("OpenOCD disconnected")
        data.extend(chunk)
    return data[:-1].decode("ascii").strip()


def memory(sock, address, count):
    result = request(sock, f"read_memory 0x{address:x} 32 {count}")
    words = result.split()
    if len(words) != count:
        raise RuntimeError(f"Unexpected SWD read response: {result[:120]}")
    return struct.pack("<" + "I" * count, *(int(word, 0) for word in words))


def number(data, offset, code):
    return struct.unpack_from("<" + code, data, offset)[0]


def openocd_tail(log):
    try:
        lines = log.read_text(encoding="utf-8", errors="replace").strip().splitlines()
    except OSError:
        return "(no OpenOCD output)"
    return "\n".join(lines[-12:]) or "(no OpenOCD output)"


def connect(process, port, log):
    deadline = time.monotonic() + 20
    while time.monotonic() < deadline:
        if process.poll() is not None:
            raise RuntimeError("OpenOCD exited before its read-only interface became ready:\n" + openocd_tail(log))
        try:
            sock = socket.create_connection(("127.0.0.1", port), timeout=0.2)
            sock.settimeout(2)
            return sock
        except OSError:
            time.sleep(0.1)
    raise RuntimeError("OpenOCD read-only interface did not start within 20 s:\n" + openocd_tail(log))


def validate_layout(sock):
    config = memory(sock, CONFIG_ADDRESS, 70)
    actual = (
        number(config, 0x00, "d"), number(config, 0x08, "d"), number(config, 0x10, "d"),
        number(config, 0x3C, "i"), number(config, 0x70, "d"), number(config, 0xB4, "i"),
        number(config, 0xF8, "i"), number(config, 0x110, "i"), number(config, 0xF0, "d"),
    )
    expected = (COUNTS_PER_RAD, math.radians(-45), math.radians(50), 10922, 150, 6553, 5898, 2211, 0.05)
    if not all(math.isclose(a, b, rel_tol=1e-8, abs_tol=1e-8) for a, b in zip(actual, expected)):
        raise RuntimeError(f"Board arm config does not match the 5be996c build ({actual})")


def sample(sock, start):
    result = request(sock, f"concat [read_memory 0x{ARM_ADDRESS:x} 32 66] "
                           f"[read_memory 0x{REMOTE_ADDRESS:x} 32 14] "
                           f"[read_memory 0x{TICKS_ADDRESS:x} 32 1] "
                           f"[read_memory 0x{MODE_ADDRESS:x} 32 1]")
    words = result.split()
    if len(words) != 82:
        raise RuntimeError(f"Unexpected SWD sample: {result[:120]}")
    data = struct.pack("<" + "I" * len(words), *(int(word, 0) for word in words))
    arm, remote = data[:264], data[264:320]
    offset = number(arm, 0x28, "d")
    angle = math.degrees(number(arm, 0x18, "d") / COUNTS_PER_RAD + offset)
    target = math.degrees(number(arm, 0x30, "d"))
    torque = number(arm, 0xB8, "d")
    return (
        round((time.monotonic() - start) * 1000, 1), number(data, 320, "I"), number(data, 324, "i"),
        number(remote, 0, "i"), number(remote, 4, "i"), number(remote, 48, "B"),
        round(number(remote, 8, "d"), 3), round(number(remote, 16, "d"), 3),
        round(angle, 3), round(target, 3), round(number(arm, 0x48, "d"), 4), number(arm, 0x69, "B"),
        number(arm, 0x20, "d"), number(arm, 0xA0, "i"), round(torque / 0.741 / 3.0 * 16384),
        round(number(arm, 0x00, "d")), number(arm, 0xC8, "d"),
        number(arm, 0xE0, "i"), round(number(arm, 0xF8, "d"), 3), number(arm, 0x100, "B"),
        number(arm, 0xB0, "H"),
    )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--duration", type=float, default=60)
    parser.add_argument("--interval", type=float, default=0.02)
    args = parser.parse_args()
    if not 0 < args.duration <= 180 or not 0.01 <= args.interval <= 1:
        parser.error("duration must be 0-180 s and interval 0.01-1 s")
    if hashlib.sha256(ELF.read_bytes()).hexdigest().upper() != EXPECTED_SHA256:
        raise RuntimeError("DevC.elf is not the 5be996c build; refusing the stored addresses")
    with socket.socket() as reserve:
        reserve.bind(("127.0.0.1", 0))
        port = reserve.getsockname()[1]
    output_dir = ROOT / "plans" / "tasks" / "diagnostics"
    output_dir.mkdir(exist_ok=True)
    log = output_dir / "arm3p-openocd.log"
    log_stream = log.open("w", encoding="utf-8")
    process = subprocess.Popen(
        [str(OPENOCD), "-f", "interface/cmsis-dap.cfg", "-f", "target/stm32f4x.cfg",
         "-c", "bindto 127.0.0.1", "-c", "gdb_port disabled", "-c", "telnet_port disabled",
         "-c", f"tcl_port {port}", "-c", "init"],
        stdout=log_stream, stderr=subprocess.STDOUT,
        creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0),
    )
    sock = None
    try:
        sock = connect(process, port, log)
        validate_layout(sock)
        output = output_dir / datetime.now().strftime("arm3p-%Y%m%d-%H%M%S.csv")
        fields = ("elapsed_ms", "tick", "mode", "right_switch", "left_switch", "remote_online",
                  "left_x", "right_y", "angle_deg", "target_deg", "jog_rad_s", "active", "rpm",
                  "cmd", "act", "speed_integral", "temp_c", "stall_limit", "stall_elapsed_s",
                  "stall", "raw")
        with output.open("x", encoding="utf-8", newline="") as stream:
            writer = csv.writer(stream)
            writer.writerow(fields)
            start = time.monotonic()
            print(f"READY: tracing the arm for {args.duration:g} s -> {output}", flush=True)
            count = 0
            while time.monotonic() - start < args.duration:
                deadline = time.monotonic() + args.interval
                writer.writerow(sample(sock, start))
                count += 1
                if count % 50 == 0:
                    stream.flush()
                time.sleep(max(0, deadline - time.monotonic()))
        print(f"DONE: {count} samples -> {output}")
    finally:
        if sock is not None:
            try:
                sock.sendall(b"shutdown\x1a")
            except OSError:
                pass
            sock.close()
        try:
            process.wait(timeout=3)
        except subprocess.TimeoutExpired:
            process.terminate()
            process.wait(timeout=3)
        log_stream.close()


if __name__ == "__main__":
    try:
        main()
    except (OSError, RuntimeError, ValueError) as error:
        print(f"TRACE FAILED: {error}", file=sys.stderr)
        sys.exit(1)
