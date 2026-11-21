"""Run in the nest on two facing StickS3 boards: python tests/ir_pair.py COM6 COM8."""
import argparse
import time
from contextlib import ExitStack

import serial


def command(port, message):
    port.write(f"RIFT {message}\n".encode("ascii"))
    port.flush()


def until(port, prefix, seconds, captures=None):
    deadline = time.monotonic() + seconds
    while time.monotonic() < deadline:
        line = port.readline().decode("ascii", errors="replace").strip()
        if captures is not None and line.startswith("RIFT "):
            captures.append(line)
        if line.startswith(prefix):
            return line
    return ""


def check_direction(sender, receiver, count, first_index, attempts):
    hits = 0
    for i in range(count):
        receiver.reset_input_buffer()
        command(receiver, "RX")
        if not until(receiver, "RIFT READY", 2):
            raise RuntimeError(f"{receiver.port} did not arm its receiver")
        time.sleep(0.10)
        capture = []
        window_end = time.monotonic() + 1.8
        result = ""
        for attempt in range(attempts):
            shot_start = time.monotonic()
            if shot_start >= window_end:
                break
            command(sender, f"TX {first_index+i}")
            sent = until(sender, "RIFT SENT ", 1)
            if not sent:
                raise RuntimeError(f"{sender.port} did not transmit")
            expected = sent.split()[-1]
            until_next = min(shot_start + 0.16, window_end) if attempt+1 < attempts else window_end
            result = until(receiver, f"RIFT WORD {expected}", until_next-time.monotonic(), capture)
            if result:
                break
        hits += bool(result)
        print(f"{sender.port} -> {receiver.port} {expected} {'OK' if result else 'MISS'} attempt={attempt+1}", flush=True)
        if i == 0 or not result:
            for line in capture:
                print(" ", line, flush=True)
        command(receiver, "STOP")
        if not until(receiver, "RIFT STOPPED", 1):
            raise RuntimeError(f"{receiver.port} did not stop its receiver")
    print(f"{sender.port} -> {receiver.port}: {hits}/{count}", flush=True)
    return hits


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("first")
    parser.add_argument("second")
    parser.add_argument("--count", type=int, default=10, choices=range(1, 101), metavar="1..100")
    parser.add_argument("--attempts", type=int, default=1, choices=range(1, 12), metavar="1..11")
    args = parser.parse_args()
    if args.first.upper() == args.second.upper():
        parser.error("choose two different devices")
    with ExitStack() as stack:
        ports = []
        for name in (args.first, args.second):
            port = serial.Serial(port=None, baudrate=115200, timeout=0.08, write_timeout=2)
            port.rts = False
            port.dtr = True
            port.port = name
            port.open()
            stack.enter_context(port)
            ports.append(port)
        for port in ports:
            for _ in range(10):
                command(port, "PING")
                if until(port, "RIFT NEST", 0.8):
                    break
            else:
                raise RuntimeError(f"{port.port} did not answer in the nest")
        a, b = ports
        hits = check_direction(a, b, args.count, 0, args.attempts)
        hits += check_direction(b, a, args.count, args.count, args.attempts)
        return int(hits != 2 * args.count)


if __name__ == "__main__":
    raise SystemExit(main())
