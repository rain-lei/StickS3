"""Run in the nest on two facing StickS3 boards: python tests/ir_pair.py COM6 COM8."""
import argparse
import time
from collections import defaultdict
from contextlib import ExitStack

import serial

pending = defaultdict(bytearray)


def game_token(index, reverse=False):
    mixed = ((0x52494654 ^ (index * 0x9e3779b9)) * 0x85ebca6b) & 0xffffffff
    mixed ^= mixed >> 16
    word = (mixed & 0xffffff) | ((mixed & 0xff0000) << 8)
    return word ^ (0x96966db3 if reverse else 0)


def command(port, message):
    port.write(f"RIFT {message}\n".encode("ascii"))


def until(port, prefix, seconds, captures=None):
    deadline = time.monotonic() + seconds
    while time.monotonic() < deadline:
        buf = pending[port]
        if b"\n" not in buf:
            buf.extend(port.read_all())
        if b"\n" not in buf:
            time.sleep(0.005)
            continue
        end = buf.index(b"\n") + 1
        line = bytes(buf[:end]).decode("ascii", errors="replace").strip()
        del buf[:end]
        if "RIFT " in line:
            line = line[line.index("RIFT "):]
        if captures is not None and line.startswith("RIFT "):
            captures.append(line)
        if line.startswith(prefix):
            return line
    return ""


def optical_window(sender, receiver, index, attempts, seconds, nec=False, reverse=False):
    receiver.reset_input_buffer()
    pending[receiver].clear()
    arg = f"{index} R" if reverse else str(index)
    command(receiver, "RX" if nec else f"ARM {arg}")
    if not until(receiver, "RIFT READY", 2):
        raise RuntimeError(f"{receiver.port} did not arm its receiver")
    expected = f"RIFT WORD {game_token(index, reverse):08X}"
    sent = expected.replace("WORD", "SENT")
    end = time.monotonic() + seconds
    capture = []
    try:
        for attempt in range(attempts):
            start = time.monotonic()
            if start >= end:
                break
            command(sender, f"TX {index}" if nec else f"SH {arg}")
            until(sender, sent, min(0.4, end - start))
            next_shot = min(start + (0.16 if nec else 0.285), end)
            deadline = end if attempt + 1 == attempts else next_shot
            if until(receiver, expected, max(0.01, deadline - time.monotonic()), capture):
                return True
        for line in capture[-2:]:
            print(f"  {receiver.port} {line}", flush=True)
        return False
    finally:
        command(receiver, "STOP")
        if not until(receiver, "RIFT STOPPED", 1):
            raise RuntimeError(f"{receiver.port} did not stop")


def check_direction(sender, receiver, count, first_index, attempts, nec, duplex):
    hits = 0
    echoes = 0
    for index in range(first_index, first_index + count):
        hit = optical_window(sender, receiver, index, attempts, 1.12 if duplex else 1.8, nec)
        path = "direct"
        if duplex and not hit:
            time.sleep(0.285)  # Finish the last in-flight burst before swapping roles.
            hit = optical_window(receiver, sender, index, attempts, 1.65, reverse=True)
            echoes += hit
            path = "echo"
        hits += hit
        print(f"{sender.port} -> {receiver.port} shot {index} {path} {'OK' if hit else 'MISS'}", flush=True)
    print(f"{sender.port} -> {receiver.port}: {hits}/{count} (echo {echoes})", flush=True)
    return hits


def check_misses(sender, receiver):
    for wrong_shot in (False, True):
        command(receiver, "ARM 4000")
        if not until(receiver, "RIFT READY", 2):
            raise RuntimeError(f"{receiver.port} did not arm")
        label = "wrong token" if wrong_shot else "silent window"
        confirmed = 0
        end = time.monotonic() + 1.8
        while time.monotonic() < end:
            if wrong_shot:
                command(sender, "SH 4001")
                confirmed += bool(until(sender, f"RIFT SENT {game_token(4001):08X}", 0.35))
            if until(receiver, "RIFT WORD ", 0.30):
                raise RuntimeError(f"{receiver.port} accepted {label}")
        if wrong_shot and confirmed < 2:
            raise RuntimeError(f"{sender.port} did not confirm two negative-control frames")
        command(receiver, "STOP")
        if not until(receiver, "RIFT STOPPED", 1):
            raise RuntimeError(f"{receiver.port} did not stop")
        print(f"{receiver.port}: {label} rejected", flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check-misses", action="store_true", help="also test silence and an unrelated game token")
    protocol = parser.add_mutually_exclusive_group()
    protocol.add_argument("--duplex", action="store_true", help="try the independent return probe after a forward miss")
    protocol.add_argument("--nec", action="store_true", help="exercise the 32-bit remote-control path")
    parser.add_argument("first")
    parser.add_argument("second")
    parser.add_argument("--count", type=int, default=10, choices=range(1, 101), metavar="1..100")
    parser.add_argument("--attempts", type=int, default=10, choices=range(1, 12), metavar="1..11")
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
        hits = check_direction(a, b, args.count, 0, args.attempts, args.nec, args.duplex)
        hits += check_direction(b, a, args.count, args.count, args.attempts, args.nec, args.duplex)
        if args.check_misses:
            check_misses(a, b)
            check_misses(b, a)
        return int(hits != 2 * args.count)


if __name__ == "__main__":
    raise SystemExit(main())
