"""Read-only SWD trace for the verified 3m firmware; no reset or motor commands."""
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
EXPECTED_SHA256 = "88A0E5A92E23C8236A88165602A2B85C97C0381E3F7AE0558065380C170C1046"
OPENOCD = Path("C:/Users/Lenovo/.eide/tools/openocd_7a1adfbec_mingw32/bin/openocd.exe")

# Offsets are from the matching ELF's DWARF layout. Reject another ELF before reading.
CONFIG_ADDRESS = 0x20003638
ARM_ADDRESS = 0x20003750
REMOTE_ADDRESS = 0x200045B8
TICKS_ADDRESS = 0x20004390


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


def connect(process, port):
    deadline = time.monotonic() + 6
    while time.monotonic() < deadline:
        if process.poll() is not None:
            raise RuntimeError("OpenOCD exited before its read-only interface became ready")
        try:
            sock = socket.create_connection(("127.0.0.1", port), timeout=0.2)
            sock.settimeout(2)
            return sock
        except OSError:
            time.sleep(0.1)
    raise RuntimeError("OpenOCD read-only interface did not start")


def validate_layout(sock):
    config = memory(sock, CONFIG_ADDRESS, 32)
    arm = memory(sock, ARM_ADDRESS, 64)
    actual = (
        number(config, 0, "d"), number(config, 8, "d"), number(config, 16, "d"),
        number(config, 60, "i"), number(config, 112, "d"), number(arm, 0xC8, "i"),
    )
    expected = (8192 / (2 * math.pi), math.radians(-45), math.radians(50), 10922, 150, 4642)
    if not all(math.isclose(a, b, rel_tol=1e-8, abs_tol=1e-8) for a, b in zip(actual, expected)):
        raise RuntimeError(f"Target state does not match the expected 3m firmware: {actual}")


def sample(sock, start):
    result = request(sock, f"concat [read_memory 0x{ARM_ADDRESS:x} 32 64] "
                           f"[read_memory 0x{REMOTE_ADDRESS:x} 32 13] "
                           f"[read_memory 0x{TICKS_ADDRESS:x} 32 1]")
    words = result.split()
    if len(words) != 78:
        raise RuntimeError(f"Unexpected SWD sample: {result[:120]}")
    data = struct.pack("<" + "I" * len(words), *(int(word, 0) for word in words))
    arm, remote = data[:256], data[256:308]
    counts = number(arm, 0x18, "d")
    offset = number(arm, 0x28, "d")
    angle = math.degrees(counts / (8192 / (2 * math.pi)) + offset)
    rpm = number(arm, 0x20, "d")
    integral = number(arm, 0, "d")
    stick = number(remote, 8, "d")
    raw = number(arm, 0x98, "H")
    command = number(arm, 0x88, "i")
    temperature = number(arm, 0xB0, "d")
    stall = number(arm, 0xE8, "B")
    valid = all(math.isfinite(v) for v in (angle, rpm, integral, stick, temperature)) and (
        abs(angle) <= 180 and abs(rpm) <= 500 and abs(integral) <= 10922 and abs(stick) <= 1.1 and
        raw < 8192 and abs(command) <= 10922 and 0 <= temperature <= 125 and stall in (0, 1))
    return (
        round((time.monotonic() - start) * 1000, 1), number(data, 308, "I"),
        number(remote, 0, "i"), number(remote, 4, "i"), number(remote, 48, "B"),
        stick, angle, raw, rpm, command,
        number(arm, 0xA0, "d"), integral, number(arm, 0xA8, "d"),
        temperature, number(arm, 0xE0, "d"), stall, int(valid),
    )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--duration", type=float, default=5)
    parser.add_argument("--interval", type=float, default=0.02)
    args = parser.parse_args()
    if not 0 < args.duration <= 60 or not 0.02 <= args.interval <= 1:
        parser.error("duration must be 0–60 s and interval 0.02–1 s")
    if hashlib.sha256(ELF.read_bytes()).hexdigest().upper() != EXPECTED_SHA256:
        raise RuntimeError("ELF differs from the verified 0.85 A firmware; refusing the stored addresses")
    with socket.socket() as reserve:
        reserve.bind(("127.0.0.1", 0))
        port = reserve.getsockname()[1]
    process = subprocess.Popen(
        [str(OPENOCD), "-f", "interface/cmsis-dap.cfg", "-f", "target/stm32f4x.cfg",
         "-c", "bindto 127.0.0.1", "-c", "gdb_port disabled", "-c", "telnet_port disabled",
         "-c", f"tcl_port {port}", "-c", "init"],
        stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
        creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0),
    )
    sock = None
    try:
        sock = connect(process, port)
        validate_layout(sock)
        output_dir = ROOT / "plans" / "tasks" / "diagnostics"
        output_dir.mkdir(exist_ok=True)
        output = output_dir / datetime.now().strftime("arm-%Y%m%d-%H%M%S-%f.csv")
        fields = ("elapsed_ms", "tick", "right_switch", "left_switch", "remote_online", "left_x",
                  "angle_deg", "raw", "rpm", "cmd", "feedback_torque_nm", "speed_integral",
                  "feedback_rpm", "temp_c", "stall_elapsed_s", "stall", "plausible")
        with output.open("x", encoding="utf-8", newline="") as stream:
            writer = csv.writer(stream)
            writer.writerow(fields)
            start = time.monotonic()
            print(f"READY: read-only SWD trace for {args.duration:g}s -> {output}", flush=True)
            count = 0
            while time.monotonic() - start < args.duration:
                deadline = time.monotonic() + args.interval
                writer.writerow(sample(sock, start))
                count += 1
                if count % 20 == 0:
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


if __name__ == "__main__":
    try:
        main()
    except (OSError, RuntimeError, ValueError) as error:
        print(f"TRACE FAILED: {error}", file=sys.stderr)
        sys.exit(1)
