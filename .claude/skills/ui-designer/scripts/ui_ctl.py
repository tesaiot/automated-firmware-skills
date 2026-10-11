#!/usr/bin/env python3
"""Claude Code side of the UI designer (talks to ui_server.py on 127.0.0.1).

  ui_ctl.py wait [--timeout 3000]     block until the page sends a command or Submit; prints it
                                      and exits (run it in the background: the session is woken
                                      when it exits). Exit 0 = item, 3 = idle timeout, 4 = no server
  ui_ctl.py show                      outline (+ editor boxes), errors/warnings, open requests
  ui_ctl.py patch <file|-> --cmd N --say "..." [--keep-open]
                                      apply an RFC 6902 patch ('#id' path segments allowed);
                                      the page updates at once; rejected if the result is invalid
  ui_ctl.py replace <ui.json> --cmd N --say "..."   replace the whole spec
  ui_ctl.py say --cmd N "text" [--keep-open]         answer / ask back without changing the spec
  ui_ctl.py device <serial log> [--note "..."]       parse 'UIW ...' lines (uitree) -> dashed
                                      device boxes on the canvas + editor-vs-device report
  ui_ctl.py quit                      stop the server (frees the port)
Options: --port 8767
"""
import argparse
import json
import re
import sys
import urllib.error
import urllib.request
from pathlib import Path

UIW_RE = re.compile(r"UIW i=(\d+) id=(\w+) type=(\w+) scr=(\w+) x=(-?\d+) y=(-?\d+) w=(\d+) h=(\d+) "
                    r"vis=(\d) act=(\d) in=(\d) val=(-?\d+)")


def call(port, path, body=None, timeout=60):
    url = f"http://127.0.0.1:{port}{path}"
    data = None if body is None else json.dumps(body).encode("utf-8")
    req = urllib.request.Request(url, data=data, headers={"Content-Type": "application/json"},
                                 method="GET" if body is None else "POST")
    try:
        with urllib.request.urlopen(req, timeout=timeout) as r:
            return r.status, json.loads(r.read() or b"{}")
    except urllib.error.HTTPError as e:
        try:
            return e.code, json.loads(e.read() or b"{}")
        except json.JSONDecodeError:
            return e.code, {"error": str(e)}


def gone(e):
    print(f"UI: SERVER GONE ({e}) - start it: python .claude/skills/ui-designer/scripts/ui_server.py <ui.json>")
    return 4


def print_items(r):
    for it in r.get("items", []):
        if it["kind"] == "submit":
            print(f"UI: SUBMIT id={it['id']} rev={it['rev']}" + (f" note={it['text']!r}" if it["text"] else ""))
        else:
            print(f"UI: COMMAND id={it['id']} rev={it['rev']} selected={it.get('selected') or '-'}")
            print(f"UI: TEXT {it['text']}")
    print(f"UI: SPEC {r.get('path')}")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("action", choices=["wait", "show", "patch", "replace", "say", "device", "quit"])
    ap.add_argument("arg", nargs="?")
    ap.add_argument("--cmd", type=int)
    ap.add_argument("--say")
    ap.add_argument("--note")
    ap.add_argument("--keep-open", action="store_true", help="reply but leave the request open")
    ap.add_argument("--timeout", type=float, default=3000)
    ap.add_argument("--port", type=int, default=8767)
    a = ap.parse_args()
    try:
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    except AttributeError:
        pass
    try:
        if a.action == "wait":
            left = a.timeout
            while left > 0:
                chunk = min(25.0, left)
                _, r = call(a.port, f"/agent/wait?timeout={chunk}", timeout=chunk + 10)
                if r.get("quit"):
                    print("UI: SERVER QUIT")
                    return 4
                if r.get("items"):
                    print_items(r)
                    return 0
                left -= chunk
            print("UI: IDLE (no request) - run wait again to keep listening")
            return 3
        if a.action == "show":
            _, r = call(a.port, "/agent/state")
            print(r["outline"])
            for e in r.get("errors", []):
                print("ERROR:", e)
            for w in r.get("warnings", []):
                print("WARN:", w)
            for q in r.get("queue", []):
                print(f"OPEN: #{q['id']} {q['kind']} {q['status']}: {q['text']}")
            if r.get("device"):
                print(f"DEVICE: layout of {len(r['device']['layout'])} widgets ({r['device'].get('note', '')})")
            print(f"UI: rev={r['rev']} path={r['path']}")
            return 0
        if a.action in ("patch", "replace"):
            if not a.arg:
                sys.exit("UI: ERROR give a patch/spec file or '-' (stdin)")
            text = sys.stdin.read() if a.arg == "-" else Path(a.arg).read_text(encoding="utf-8")
            body = {"cmd": a.cmd, "say": a.say, "done": not a.keep_open}
            if a.action == "patch":
                body["ops"] = json.loads(text)
                code, r = call(a.port, "/agent/patch", body)
            else:
                body["spec"] = json.loads(text)
                code, r = call(a.port, "/agent/spec", body)
            if code != 200:
                for e in r.get("errors", [r.get("error")]):
                    print("ERROR:", e)
                print("UI: REJECTED (nothing changed) - fix and retry")
                return 1
            for w in r.get("warnings", []):
                print("WARN:", w)
            print(f"UI: APPLIED rev={r['rev']}")
            return 0
        if a.action == "say":
            if not a.arg:
                sys.exit("UI: ERROR say needs the text")
            call(a.port, "/agent/say", {"cmd": a.cmd, "text": a.arg, "done": not a.keep_open})
            print("UI: SAID")
            return 0
        if a.action == "device":
            layout = {}
            for line in Path(a.arg).read_text(encoding="utf-8", errors="replace").splitlines():
                m = UIW_RE.search(line)
                if m:
                    i, wid, t, scr, x, y, w, h, vis, act, ins, val = m.groups()
                    layout[wid] = {"type": t, "scr": scr, "x": int(x), "y": int(y), "w": int(w), "h": int(h),
                                   "vis": vis == "1", "in": ins == "1", "val": int(val)}
            if not layout:
                print("UI: ERROR no 'UIW ...' lines in that log (run uitree on the board)")
                return 1
            call(a.port, "/agent/device", {"layout": layout, "note": a.note or f"{len(layout)} widgets from uitree"})
            _, r = call(a.port, "/agent/state")
            boxes = r.get("boxes", {})
            diffs = []
            for wid, d in layout.items():
                e = boxes.get(wid)
                if e:
                    off = max(abs(e[0] - d["x"]), abs(e[1] - d["y"]), abs(e[2] - d["w"]), abs(e[3] - d["h"]))
                    if off > 3:
                        diffs.append((off, wid, e, d))
            for off, wid, e, d in sorted(diffs, reverse=True):
                print(f"DIFF {wid}: editor {e[0]},{e[1]} {e[2]}x{e[3]} vs device {d['x']},{d['y']} {d['w']}x{d['h']} ({off} px)")
            print(f"UI: DEVICE {len(layout)} widgets, {len(diffs)} differ > 3 px "
                  f"(editor boxes known for {sum(1 for w in layout if w in boxes)}; screens not yet shown in the editor have none)")
            return 0
        if a.action == "quit":
            call(a.port, "/agent/quit", {})
            print("UI: QUIT sent")
            return 0
    except (urllib.error.URLError, ConnectionError, TimeoutError) as e:
        return gone(e)
    return 2


if __name__ == "__main__":
    sys.exit(main())
