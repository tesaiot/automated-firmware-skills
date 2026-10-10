#!/usr/bin/env python3
"""Browser forms for questions that would otherwise be asked one by one in the console.

Opens a local page, waits until the user submits (or cancels / times out), prints the answers as
one JSON line and exits. Nothing is built, flashed or changed here - the agent acts on the answers.

Modes:
  wizard.py project [--board <id>] [--app <name>]     new-app wizard for the pse84-firmware engine:
        board, template + core scope + execution model per core (from the template's scopes.txt),
        IPC choice, on-board hardware / services components (from the hardware packs), component
        options (e.g. flash_area), action (generate + VS Code | + build | + build + flash + test).
        "Check" runs the real gates (components.py check) before submit.
  wizard.py ask <questions.json>                      generic form, AskUserQuestion-like spec:
        {"title": "...", "questions": [{"id": "q1", "header": "Scope", "question": "...",
          "multi": false, "other": true, "options": [{"label": "...", "description": "..."}]}]}

Output (stdout, last line): WIZARD: ANSWERS {json} | WIZARD: CANCELLED | WIZARD: TIMEOUT
Options: --http-port 8766  --timeout 900 (s)  --no-open  --out <file.json> (also save answers)
Binds 127.0.0.1 only. One page; the server exits after the answer.
"""
import argparse
import json
import subprocess
import sys
import threading
import time
import webbrowser
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

SKILL = Path(__file__).resolve().parent.parent
ENGINE = SKILL.parent / "pse84-firmware"
sys.path.insert(0, str(ENGINE / "scripts"))
import board_gate  # noqa: E402
import components  # noqa: E402
import packs  # noqa: E402

BASE_IDS = {"ai-kit-board", "cm55-board", "qwa309-board"}     # always pulled in by the template/board


def read_lines(p):
    return [l.strip() for l in p.read_text(encoding="utf-8").splitlines()
            if l.strip() and not l.lstrip().startswith("#")] if p.is_file() else []


def catalog(default_board, default_app):
    """everything the project page needs, from the live engine/pack data"""
    comps = components.load()
    pub = lambda c: {k: v for k, v in c.items() if not k.startswith("_")} | {"pack": c["_pack"],
                                                                          "group": c["_group"]}
    boards = {}
    for b in packs.board_ids():
        env = board_gate.board_env(b)
        groups = sorted(board_gate.board_groups(b))
        boards[b] = {"groups": groups, "board_components": env.get("BOARD_COMPONENTS", "").split(),
                     "title": " + ".join(board_gate.GROUPS[g]["name"] for g in groups if g in board_gate.GROUPS)}
    templates = {}
    for t in sorted((ENGINE / "templates").iterdir()):
        if not t.is_dir() or t.name.startswith("_"):
            continue
        desc = (t / "description.txt").read_text(encoding="utf-8").strip() if (t / "description.txt").is_file() else ""
        variants = []
        for line in read_lines(t / "scopes.txt"):
            parts = line.split()
            variants.append({"scope": parts[0], "models": dict(p.split("=", 1) for p in parts[1:])})
        templates[t.name] = {"description": desc, "variants": variants,
                             "components": read_lines(t / "components.txt"),
                             "requires": read_lines(t / "requires.txt"),
                             "has_cm55": (t / "proj_cm55").is_dir()}
    return {"boards": boards, "default_board": default_board or next(iter(boards)),
            "default_app": default_app or "", "templates": templates,
            "components": {cid: pub(c) for cid, c in comps.items()},
            "scopes": {s: {"id": cs[0]["id"], "title": cs[0].get("scope_title", ""),
                           "jobs": cs[0].get("scope_jobs", {}), "app_cores": cs[0].get("app_cores", []),
                           "status": cs[0].get("status")} for s, cs in components.scopes(comps).items()},
            "base_ids": sorted(BASE_IDS)}


def run_check(board, ids, opts):
    cmd = [sys.executable, str(ENGINE / "scripts" / "components.py"), "check", board, *ids]
    for k, v in (opts or {}).items():
        cmd += ["--opt", f"{k}={v}"]
    r = subprocess.run(cmd, capture_output=True, text=True, encoding="utf-8", errors="replace")
    return {"rc": r.returncode, "lines": [l for l in (r.stdout + r.stderr).splitlines() if l.strip()]}


class Handler(BaseHTTPRequestHandler):
    state = None

    def log_message(self, fmt, *args):
        pass

    def _send(self, code, body, ctype="application/json"):
        data = body if isinstance(body, bytes) else json.dumps(body).encode()
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
        st = self.state
        if self.path in ("/", "/index.html"):
            return self._send(200, (SKILL / "web" / f"{st['mode']}.html").read_bytes(), "text/html; charset=utf-8")
        if self.path == "/data.json":
            return self._send(200, st["data"])
        return self._send(404, {"error": "not found"})

    def do_POST(self):
        st = self.state
        try:
            b = self._body()
        except json.JSONDecodeError as e:
            return self._send(400, {"error": str(e)})
        if self.path == "/check" and st["mode"] == "project":
            return self._send(200, run_check(str(b.get("board", "")), [str(i) for i in b.get("ids", [])],
                                             b.get("opts", {})))
        if self.path in ("/submit", "/cancel"):
            st["result"] = ("ANSWERS", b) if self.path == "/submit" else ("CANCELLED", None)
            self._send(200, {"ok": True})
            st["done"].set()
            return None
        return self._send(404, {"error": "not found"})


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("mode", choices=["project", "ask"])
    ap.add_argument("spec", nargs="?", help="questions.json (mode ask)")
    ap.add_argument("--board")
    ap.add_argument("--app")
    ap.add_argument("--http-port", type=int, default=8766)
    ap.add_argument("--timeout", type=float, default=900)
    ap.add_argument("--no-open", action="store_true")
    ap.add_argument("--out", help="also write the answers JSON to this file")
    a = ap.parse_args()
    try:
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    except AttributeError:
        pass
    if a.mode == "ask":
        if not a.spec:
            sys.exit("WIZARD: ERROR mode ask needs a questions.json")
        data = json.loads(Path(a.spec).read_text(encoding="utf-8"))
    else:
        data = catalog(a.board, a.app)
    state = {"mode": a.mode, "data": data, "done": threading.Event(), "result": ("TIMEOUT", None)}
    Handler.state = state
    srv = ThreadingHTTPServer(("127.0.0.1", a.http_port), Handler)
    srv.daemon_threads = True
    threading.Thread(target=srv.serve_forever, daemon=True).start()
    url = f"http://127.0.0.1:{a.http_port}/"
    print(f"WIZARD: waiting for the answer at {url} (timeout {a.timeout:.0f} s)", flush=True)
    if not a.no_open:
        webbrowser.open(url)
    state["done"].wait(a.timeout)
    time.sleep(0.3)                       # let the page receive its reply
    srv.shutdown()
    kind, ans = state["result"]
    if kind == "ANSWERS":
        if a.out:
            Path(a.out).write_text(json.dumps(ans, indent=2, ensure_ascii=False), encoding="utf-8")
        print("WIZARD: ANSWERS " + json.dumps(ans, ensure_ascii=False), flush=True)
        return 0
    print(f"WIZARD: {kind}", flush=True)
    return 1


if __name__ == "__main__":
    sys.exit(main())
