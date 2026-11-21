"""Serve the RIFT workbench and USB spectator feed on this computer only."""
import argparse
import json
import time
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from threading import Lock
from pathlib import Path
from urllib.parse import urlsplit

import serial
from serial.tools.list_ports import comports

WEB = Path(__file__).resolve().parents[1] / "web"


class Board:
    def __init__(self, name):
        self.port = serial.Serial(port=None, baudrate=115200, timeout=0, write_timeout=0.1)
        self.port.dtr, self.port.rts = True, False
        self.port.port = name
        self.port.open()
        self.buffer = bytearray()
        self.requested = self.updated = 0
        self.frame = None

    def update(self, now):
        if now - self.requested >= 1:
            self.port.write(b"RIFT WATCH\n")
            self.requested = now
        self.buffer.extend(self.port.read(min(self.port.in_waiting, 8192)))
        while b"\n" in self.buffer:
            line, _, self.buffer = self.buffer.partition(b"\n")
            if not line.startswith(b"RIFT LIVE "):
                continue
            try:
                frame = json.loads(line[10:])
            except (ValueError, UnicodeError):
                continue
            if isinstance(frame, list) and len(frame) == 8 and frame[0] == 1 and all(
                type(field) is int for field in frame[:5]
            ) and all(isinstance(row, list) and len(row) == size and all(
                type(value) is int for value in row
            ) for row, size in zip(frame[5:], (9, 9, 4))):
                self.frame, self.updated = frame, now
        if len(self.buffer) > 8192:
            self.buffer.clear()


class Workbench(ThreadingHTTPServer):
    request_queue_size = 64

    def __init__(self, address):
        super().__init__(address, Handler)
        self.lock = Lock()
        self.boards = {}
        self.viewed = self.scanned = 0

    def service_actions(self):
        with self.lock:
            self.update_boards()

    def update_boards(self):
        now = time.monotonic()
        if now - self.viewed > 2:
            for board in self.boards.values():
                board.port.close()
            self.boards.clear()
            return
        if now - self.scanned > 1:
            self.scanned = now
            for port in comports():
                if (port.vid, port.pid) == (0x303A, 0x1001) and port.device not in self.boards:
                    try:
                        self.boards[port.device] = Board(port.device)
                    except (OSError, serial.SerialException):
                        pass
        for name, board in list(self.boards.items()):
            try:
                board.update(now)
            except (OSError, serial.SerialException):
                board.port.close()
                del self.boards[name]

    def snapshot(self):
        with self.lock:
            self.viewed = time.monotonic()
            live = [b for b in self.boards.values() if b.frame and self.viewed - b.updated < 1.4]
            if not live:
                return {"port": None, "state": None}
            # Either player has both fighters' data. Prefer the active duel, then host.
            board = max(live, key=lambda b: (b.frame[1:3] == [5, 1], b.frame[1] == 3, b.frame[4] == 0))
            return {"port": board.port.port, "state": board.frame}


class Handler(SimpleHTTPRequestHandler):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=str(WEB), **kwargs)

    def do_GET(self):
        if urlsplit(self.path).path != "/api/live":
            return super().do_GET()
        # Only pages served from this loopback origin can access local USB data.
        if self.headers.get("Host") not in (f"127.0.0.1:{self.server.server_port}", f"localhost:{self.server.server_port}"):
            self.send_error(403)
            return
        body = json.dumps(self.server.snapshot(), separators=(",", ":")).encode()
        self.send_response(200)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        try:
            self.wfile.write(body)
        except (ConnectionResetError, BrokenPipeError):
            pass

    def log_message(self, *args):
        pass


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", type=int, default=8765)
    with Workbench(("127.0.0.1", parser.parse_args().port)) as server:
        print(f"http://127.0.0.1:{server.server_port}/#/watch", flush=True)
        try:
            server.serve_forever(poll_interval=0.04)
        except KeyboardInterrupt:
            pass
