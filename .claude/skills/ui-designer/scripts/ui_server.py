#!/usr/bin/env python3
"""UI designer server: canvas editor page <-> ui.json on disk <-> the Claude Code session.

  ui_server.py <ui.json> [--new NAME | --reset NAME] [--http-port 8767] [--no-open]

The page (web/designer.html) edits the spec directly (drag, resize, properties, tree) and
sends free-text commands ("add a slider bound to pot1") and Submit. Commands and Submit are
queued here; the Claude Code session picks them up with ui_ctl.py (wait -> patch/say), so the
"AI" is the session itself - no API key. Every accepted change: validated (ui_spec.check),
saved to <ui.json>, pushed on the undo history, shown on the page by polling.

Endpoints - page: GET /, GET /api/state?since=REV, POST /api/spec {base_rev, spec, note},
  /api/boxes {rev, boxes, warnings}, /api/ask {text, selected}, /api/submit {note},
  /api/undo, /api/redo, /api/cancel {id}
agent (ui_ctl.py): GET /agent/wait?timeout=S, GET /agent/state, POST /agent/patch
  {cmd, ops, say}, /agent/spec {cmd, spec, say}, /agent/say {cmd, text, done},
  /agent/device {layout, note}, /agent/quit
Binds 127.0.0.1 only. Chat + decisions are appended to <ui.json dir>/designer-chat.jsonl.
"""
import argparse
import json
import sys
import threading
import time
import webbrowser
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import parse_qs, urlparse

sys.path.insert(0, str(Path(__file__).resolve().parent))
import ui_spec  # noqa: E402

SKILL = Path(__file__).resolve().parent.parent
HISTORY_MAX = 200


class State:
    def __init__(self, path):
        self.path = Path(path)
        self.lock = threading.Condition()
        self.spec = ui_spec.load(self.path)
        self.rev = 1
        self.tick = 1                           # any change the page should see
        self.undo, self.redo = [], []
        self.boxes, self.page_warnings = {}, []
        self.device = None                      # {layout: {id: {...}}, note, time}
        self.messages = []                      # {id, who, text, cmd, time}
        self.queue = []                         # {id, kind, text, selected, rev, status, time}
        self.next_id = 1
        self.quit = False
        self.log = self.path.parent / "designer-chat.jsonl"

    def _log(self, rec):
        try:
            with self.log.open("a", encoding="utf-8") as f:
                f.write(json.dumps(rec, ensure_ascii=False) + "\n")
        except OSError:
            pass

    def bump(self):
        self.tick += 1
        self.lock.notify_all()

    def say(self, who, text, cmd=None):
        m = {"id": self.next_id, "who": who, "text": text, "cmd": cmd, "time": time.time()}
        self.next_id += 1
        self.messages.append(m)
        self.bump()
        self._log({"t": time.strftime("%Y-%m-%d %H:%M:%S"), "who": who, "text": text, "cmd": cmd, "rev": self.rev})
        return m

    def set_spec(self, spec, who, note):
        """validated replace; returns (ok, errors, warnings)"""
        errors, warnings = ui_spec.check(spec)
        if errors:
            return False, errors, warnings
        self.undo.append(self.spec)
        del self.undo[:-HISTORY_MAX]
        self.redo.clear()
        self.spec = spec
        self.rev += 1
        ui_spec.save(self.path, spec)
        self._log({"t": time.strftime("%Y-%m-%d %H:%M:%S"), "who": who, "change": note, "rev": self.rev})
        self.bump()
        return True, [], warnings

    def view(self):
        errors, warnings = ui_spec.check(self.spec, self.boxes)
        return {"rev": self.rev, "tick": self.tick, "spec": self.spec, "path": str(self.path),
                "undo": len(self.undo), "redo": len(self.redo),
                "errors": errors, "warnings": warnings, "messages": self.messages[-200:],
                "queue": [q for q in self.queue if q["status"] != "done"],
                "device": self.device, "fonts": ui_spec.FONTS, "types": list(ui_spec.TYPES),
                "events": ui_spec.EVENTS, "aligns": ui_spec.ALIGNS}


class Handler(BaseHTTPRequestHandler):
    st = None

    def log_message(self, fmt, *args):
        pass

    def _send(self, code, body, ctype="application/json"):
        data = body if isinstance(body, bytes) else json.dumps(body, ensure_ascii=False).encode("utf-8")
        self.send_response(code)
        self.send_header("Content-Type", ctype)
        self.send_header("Content-Length", str(len(data)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(data)

    def _body(self):
        n = int(self.headers.get("Content-Length") or 0)
        return json.loads(self.rfile.read(n) or b"{}")

    def do_GET(self):
        st, u = self.st, urlparse(self.path)
        q = parse_qs(u.query)
        if u.path in ("/", "/index.html"):
            return self._send(200, (SKILL / "web" / "designer.html").read_bytes(), "text/html; charset=utf-8")
        if u.path == "/api/state":
            since = int((q.get("since") or ["0"])[0])
            with st.lock:
                if since == st.tick:             # short long-poll: page sees changes quickly
                    st.lock.wait(timeout=8)
                return self._send(200, st.view())
        if u.path == "/agent/state":
            with st.lock:
                v = st.view()
                v["boxes"] = st.boxes
                v["outline"] = ui_spec.outline(st.spec, st.boxes)
                return self._send(200, v)
        if u.path == "/agent/wait":
            timeout = float((q.get("timeout") or ["25"])[0])
            end = time.time() + timeout
            with st.lock:
                while True:
                    items = [x for x in st.queue if x["status"] == "pending"]
                    if items or st.quit:
                        for x in items:
                            x["status"] = "working"
                        st.bump()
                        return self._send(200, {"items": items, "rev": st.rev, "path": str(st.path),
                                                "quit": st.quit})
                    left = end - time.time()
                    if left <= 0:
                        return self._send(200, {"items": [], "rev": st.rev, "path": str(st.path)})
                    st.lock.wait(timeout=left)
        return self._send(404, {"error": "not found"})

    def do_POST(self):
        st, path = self.st, urlparse(self.path).path
        try:
            b = self._body()
        except json.JSONDecodeError as e:
            return self._send(400, {"error": f"bad JSON: {e}"})
        with st.lock:
            if path == "/api/spec":
                if int(b.get("base_rev", -1)) != st.rev:
                    return self._send(409, {"error": "the spec changed meanwhile - reloaded", "rev": st.rev})
                ok, errors, warnings = st.set_spec(b.get("spec"), "user", b.get("note") or "edit")
                return self._send(200 if ok else 400, {"ok": ok, "errors": errors, "rev": st.rev})
            if path == "/api/boxes":
                if int(b.get("rev", -1)) == st.rev:
                    st.boxes = b.get("boxes") or {}
                    st.page_warnings = b.get("warnings") or []
                return self._send(200, {"ok": True})
            if path in ("/api/undo", "/api/redo"):
                src, dst = (st.undo, st.redo) if path == "/api/undo" else (st.redo, st.undo)
                if not src:
                    return self._send(400, {"error": "nothing to " + path[5:]})
                dst.append(st.spec)
                st.spec = src.pop()
                st.rev += 1
                ui_spec.save(st.path, st.spec)
                st._log({"t": time.strftime("%Y-%m-%d %H:%M:%S"), "who": "user", "change": path[5:], "rev": st.rev})
                st.bump()
                return self._send(200, {"ok": True, "rev": st.rev})
            if path in ("/api/ask", "/api/submit"):
                kind = "command" if path == "/api/ask" else "submit"
                text = str(b.get("text") or b.get("note") or "").strip()
                if kind == "command" and not text:
                    return self._send(400, {"error": "empty command"})
                item = {"id": st.next_id, "kind": kind, "text": text, "selected": b.get("selected"),
                        "rev": st.rev, "status": "pending", "time": time.time()}
                st.say("user", text if kind == "command" else ("Submit" + (f": {text}" if text else "")),
                       cmd=item["id"])
                item["id"] = st.messages[-1]["id"]
                st.messages[-1]["cmd"] = item["id"]
                st.queue.append(item)
                st.bump()
                return self._send(200, {"ok": True, "id": item["id"]})
            if path == "/api/cancel":
                for x in st.queue:
                    if x["id"] == b.get("id") and x["status"] == "pending":
                        x["status"] = "done"
                        st.say("system", f"cancelled #{x['id']}", cmd=x["id"])
                st.bump()
                return self._send(200, {"ok": True})
            # ---------------------------------------------------------------- agent side
            cmd = b.get("cmd")
            if path in ("/agent/patch", "/agent/spec"):
                try:
                    new = ui_spec.apply_patch(st.spec, b.get("ops")) if path == "/agent/patch" else b.get("spec")
                except ui_spec.PatchError as e:
                    return self._send(400, {"ok": False, "errors": [f"patch: {e}"]})
                ok, errors, warnings = st.set_spec(new, "claude", b.get("say") or "agent change")
                if not ok:
                    return self._send(400, {"ok": False, "errors": errors})
                if b.get("say"):
                    st.say("claude", b["say"], cmd=cmd)
                self._finish(cmd, b.get("done", True))
                return self._send(200, {"ok": True, "rev": st.rev, "warnings": warnings})
            if path == "/agent/say":
                st.say("claude", str(b.get("text") or ""), cmd=cmd)
                self._finish(cmd, b.get("done", True))
                st.bump()
                return self._send(200, {"ok": True})
            if path == "/agent/device":
                st.device = {"layout": b.get("layout") or {}, "note": b.get("note") or "", "time": time.time()}
                st.say("claude", b.get("note") or "device layout updated (dashed boxes)")
                st.rev += 1                      # page redraws with the overlay
                st.bump()
                return self._send(200, {"ok": True})
            if path == "/agent/quit":
                st.quit = True
                st.bump()
                self._send(200, {"ok": True})
                threading.Thread(target=lambda: (time.sleep(0.5), self.server.shutdown()), daemon=True).start()
                return None
        return self._send(404, {"error": "not found"})

    def _finish(self, cmd, done):
        if cmd is None or not done:
            return
        for x in self.st.queue:
            if x["id"] == cmd:
                x["status"] = "done"
        self.st.bump()


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("spec", help="ui.json to edit (created with --new)")
    ap.add_argument("--new", metavar="NAME", help="create an empty spec if the file does not exist")
    ap.add_argument("--reset", metavar="NAME", help="start an empty spec even if the file exists (old one -> .bak)")
    ap.add_argument("--http-port", type=int, default=8767)
    ap.add_argument("--no-open", action="store_true")
    a = ap.parse_args()
    try:
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    except AttributeError:
        pass
    p = Path(a.spec)
    if a.reset:
        if p.is_file():
            p.with_name(p.name + ".bak").write_bytes(p.read_bytes())
            print(f"UI: old spec kept as {p.name}.bak")
        p.parent.mkdir(parents=True, exist_ok=True)
        ui_spec.save(p, ui_spec.new_spec(a.reset))
    if not p.is_file():
        if not a.new:
            sys.exit(f"UI: ERROR {p} not found (use --new <name> to start an empty design)")
        p.parent.mkdir(parents=True, exist_ok=True)
        ui_spec.save(p, ui_spec.new_spec(a.new))
    errors, _ = ui_spec.check(ui_spec.load(p))
    if errors:
        for e in errors:
            print("ERROR:", e)
        sys.exit("UI: ERROR the spec is invalid - fix it first (ui_spec.py check)")
    Handler.st = State(p)
    srv = ThreadingHTTPServer(("127.0.0.1", a.http_port), Handler)
    srv.daemon_threads = True
    url = f"http://127.0.0.1:{a.http_port}/"
    print(f"UI: designer at {url} editing {p.resolve()}", flush=True)
    if not a.no_open:
        webbrowser.open(url)
    try:
        srv.serve_forever()
    except KeyboardInterrupt:
        pass
    print("UI: server stopped", flush=True)


if __name__ == "__main__":
    main()
