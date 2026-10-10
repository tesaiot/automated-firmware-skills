#!/usr/bin/env python3
"""Live sensor view: forward every line the board prints on the KitProg3 UART to one browser page.

Run it only when asked. It opens the COM port, starts a local server, opens the browser and pushes
each received line to the page (Server-Sent Events); the page parses the console protocol and draws
live charts. One browser page at a time (a new page takes over the stream). Ctrl+C stops it and
frees the COM port for sensor_debug.py / serial_test.py.

  GET  /        the page            GET /stream   the lines (SSE)
  GET  /config.json  page settings (web/config.json, or --config <file> for an app of its own)
  POST /cmd     {"cmd": "rate 200"} write one console command to the board (used by the page)

Usage: sensor_dash.py [--app <app-dir> | --board <id> | --port COMx] [--http-port 8765] [--config F] [--no-open]
  --app: board from <app>/pse84-app.env (APP_BOARD), page settings from <app>/tests/liveview.json if present
Binds 127.0.0.1 only. Never flashes or resets the board.
"""
import argparse
import json
import queue
import sys
import threading
import time
import webbrowser
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import sensor_debug as sd  # noqa: E402  (same folder; port lookup, pyserial check)
import serial  # noqa: E402

PAGE = sd.SKILL / "web" / "index.html"
CONFIG = sd.SKILL / "web" / "config.json"      # template settings (title, quick buttons, units)


class Bridge:
    """serial -> the current browser page (one queue, replaced when a new page connects)"""

    def __init__(self, port, baud):
        self.port, self.baud = port, baud
        self.ser, self.q, self.lock = None, None, threading.Lock()

    def push(self, obj):
        q = self.q
        if q is not None:
            try:
                q.put_nowait(json.dumps(obj))
            except queue.Full:
                pass

    def run(self):
        buf = b""
        while True:
            if self.ser is None:
                try:
                    self.ser = serial.Serial(self.port, self.baud, timeout=0.1)
                    self.push({"status": f"connected {self.port}"})
                except serial.SerialException as e:
                    self.push({"status": f"waiting for {self.port}: {e}"})
                    time.sleep(1.0)
                    continue
            try:
                buf += self.ser.read(512)
            except (serial.SerialException, OSError) as e:       # USB replug / flashing
                self.ser.close()
                self.ser = None
                self.push({"status": f"lost {self.port}: {e}"})
                continue
            while b"\n" in buf:
                raw, buf = buf.split(b"\n", 1)
                line = raw.decode(errors="replace").strip()
                if line:
                    self.push({"t": time.time(), "line": line})

    def write(self, cmd):
        with self.lock:
            if self.ser is None:
                raise RuntimeError(f"{self.port} not open")
            self.ser.write((cmd + "\r").encode())


class Handler(BaseHTTPRequestHandler):
    bridge = None
    config = CONFIG

    def log_message(self, fmt, *args):
        pass

    def _send(self, code, data, ctype):
        self.send_response(code)
        self.send_header("Content-Type", ctype)
        self.send_header("Content-Length", str(len(data)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(data)

    def do_GET(self):
        if self.path in ("/", "/index.html"):
            return self._send(200, PAGE.read_bytes(), "text/html; charset=utf-8")
        if self.path == "/config.json":
            return self._send(200, Path(self.config).read_bytes(), "application/json")
        if self.path != "/stream":
            return self._send(404, b"not found", "text/plain")
        q = queue.Queue(maxsize=5000)
        self.bridge.q = q                       # this page now gets the stream
        self.send_response(200)
        self.send_header("Content-Type", "text/event-stream")
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        port_state = "connected" if self.bridge.ser else "waiting for"
        try:
            self.wfile.write(f"data: {json.dumps({'status': f'{port_state} {self.bridge.port}'})}\n\n".encode())
            self.wfile.flush()
            while self.bridge.q is q:
                try:
                    self.wfile.write(f"data: {q.get(timeout=5)}\n\n".encode())
                except queue.Empty:
                    self.wfile.write(b": keepalive\n\n")
                self.wfile.flush()
        except OSError:
            pass
        finally:
            if self.bridge.q is q:
                self.bridge.q = None

    def do_POST(self):
        if self.path != "/cmd":
            return self._send(404, b"not found", "text/plain")
        try:
            n = int(self.headers.get("Content-Length") or 0)
            cmd = str(json.loads(self.rfile.read(n) or b"{}").get("cmd", "")).strip()
            if not 0 < len(cmd) <= 64:
                raise ValueError("cmd must be 1..64 characters")
            self.bridge.write(cmd)
            return self._send(200, b'{"ok": true}', "application/json")
        except (ValueError, RuntimeError, json.JSONDecodeError) as e:
            return self._send(409, json.dumps({"ok": False, "error": str(e)}).encode(), "application/json")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--port")
    ap.add_argument("--board")
    ap.add_argument("--app", help="app folder made by new_app.sh (board + tests/liveview.json)")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--http-port", type=int, default=8765)
    ap.add_argument("--config", help="page settings JSON (default: <app>/tests/liveview.json, else web/config.json)")
    ap.add_argument("--no-open", action="store_true", help="do not open the default browser")
    a = ap.parse_args()
    if a.app:
        env = Path(a.app) / "pse84-app.env"
        if not env.is_file():
            sys.exit(f"DASH: no {env} - not an app made by new_app.sh")
        board = next((l.split("=", 1)[1].strip() for l in env.read_text(encoding="utf-8").splitlines()
                      if l.startswith("APP_BOARD=")), None)
        a.board = a.board or board
        app_cfg = Path(a.app) / "tests" / "liveview.json"
        a.config = a.config or (str(app_cfg) if app_cfg.is_file() else None)
    a.config = a.config or str(CONFIG)
    json.loads(Path(a.config).read_text(encoding="utf-8"))      # fail early on a broken file
    Handler.config = a.config
    bridge = Bridge(sd.resolve_port(a), a.baud)
    threading.Thread(target=bridge.run, daemon=True).start()
    Handler.bridge = bridge
    srv = ThreadingHTTPServer(("127.0.0.1", a.http_port), Handler)
    srv.daemon_threads = True
    url = f"http://127.0.0.1:{a.http_port}/"
    print(f"DASH: {url}  <- {bridge.port} @ {a.baud}   (Ctrl+C stops and frees the port)", flush=True)
    if not a.no_open:
        webbrowser.open(url)
    try:
        srv.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        if bridge.ser:
            bridge.ser.close()


if __name__ == "__main__":
    main()
