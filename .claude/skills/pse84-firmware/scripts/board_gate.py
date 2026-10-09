#!/usr/bin/env python3
"""Board-scope gate: the bare PSOC Edge E84 AI Kit (board id kit-pse84-ai) cannot reach the parts of
the TESAIoT QWA309 base board (pots, SW4/SW5, LED matrix, CapSense, Arduino/mikroBUS headers, CAN,
OPTIGA ...). Requests and apps that need them must target board id `tesaiot`.

Data: boards/components.json (parts per hardware group, prompt keywords, source symbols) and
BOARD_PARTS in boards/<id>/board.env (groups a board has; BOARD_EXTENDS is followed).

Usage:
  board_gate.py hook                      UserPromptSubmit hook: JSON on stdin; prints a block
                                          decision when a prompt asks for a base-board part on the
                                          bare E84 / AI Kit. "#board-ok" in the prompt skips it.
  board_gate.py check <board-id> <part|words...>
                                          part ids (pots, led-matrix ...) or free text
  board_gate.py app <app-dir> [<part|words...>]
                                          board from <app>/pse84-app.env; also scans the app's
                                          sources (outside board/) for base-board symbols
  board_gate.py list <board-id>           parts this board has / lacks
Gate lines: "GATE: PASS ..." (exit 0) or "GATE: BLOCKED ..." (exit 1). Hook mode always exits 0.
"""
import json
import re
import sys
from pathlib import Path

SKILL = Path(__file__).resolve().parent.parent
BOARDS = SKILL / "boards"
DATA = json.loads((BOARDS / "components.json").read_text(encoding="utf-8"))
SRC_SKIP = {"board", "libs", "build", "GeneratedSource", "bsps", ".git"}


def board_env(board_id):
    env, seen, bid = {}, set(), board_id
    chain = []
    while bid and bid not in seen:
        seen.add(bid)
        f = BOARDS / bid / "board.env"
        if not f.is_file():
            if bid == board_id:
                raise SystemExit(f"GATE: BLOCKED unknown board id '{board_id}' (boards/<id>/board.env)")
            break
        vals = {}
        for line in f.read_text(encoding="utf-8").splitlines():
            m = re.match(r'\s*([A-Z_]+)=("([^"]*)"|[^#\s]*)', line)
            if m:
                vals[m.group(1)] = m.group(3) if m.group(3) is not None else m.group(2)
        chain.append(vals)
        bid = vals.get("BOARD_EXTENDS")
    for vals in reversed(chain):          # parent first, child overrides
        env.update(vals)
    return env


def board_groups(board_id):
    return set(board_env(board_id).get("BOARD_PARTS", "ai-kit").split())


def all_parts():
    for gid, g in DATA["groups"].items():
        for pid, p in g["parts"].items():
            yield gid, pid, p


def find_parts(text):
    """part ids named directly, or matched by keyword regexes"""
    hits, words = [], set(text.lower().split())
    for gid, pid, p in all_parts():
        if pid in words or any(re.search(k, text, re.I) for k in p.get("keywords", [])):
            hits.append((gid, pid, p))
    return hits


def verdict(board_id, hits, source="request"):
    have = board_groups(board_id)
    missing = [(g, pid, p) for g, pid, p in hits if g not in have]
    if not missing:
        names = ", ".join(pid for _, pid, _ in hits) or "no board-specific part"
        print(f"GATE: PASS board={board_id} ({names})")
        return 0
    for g, pid, p in missing:
        print(f"GATE: BLOCKED {source} needs '{pid}' - {p['label']}; board '{board_id}' has no "
              f"{DATA['groups'][g]['name']}")
    print("ACTION: CHOOSE use board id 'tesaiot' (AI Kit on the QWA309 base board): "
          "new_app.sh tesaiot <app> ... - or drop the base-board part")
    return 1


def scan_app(appdir):
    hits = []
    pats = [(g, pid, p, re.compile(s)) for g, pid, p in all_parts() for s in p.get("symbols", [])]
    for f in Path(appdir).rglob("*"):
        if f.suffix not in (".c", ".h") or SRC_SKIP & set(f.relative_to(appdir).parts):
            continue
        text = f.read_text(encoding="utf-8", errors="ignore")
        for g, pid, p, rx in pats:
            if rx.search(text) and (g, pid, p) not in hits:
                hits.append((g, pid, p))
                print(f"  found {pid} in {f.relative_to(appdir)} ({rx.pattern})")
    return hits


def hook():
    try:                                   # Claude Code sends UTF-8; don't trust the console code page
        prompt = json.loads(sys.stdin.buffer.read().decode("utf-8", "replace")).get("prompt", "") or ""
    except (ValueError, AttributeError):
        return 0
    cfg = DATA["prompt"]
    if cfg["override"] in prompt:
        return 0
    hits = [(g, pid, p) for g, pid, p in find_parts(prompt) if g == "qwa309"]
    if not hits:
        return 0
    has = lambda key: any(re.search(k, prompt, re.I) for k in cfg[key])
    explicit_e84 = has("explicit_e84_id")
    if not explicit_e84 and (has("tesaiot") or not has("e84_only")):
        return 0                  # TESAIoT named, or no board named at all: allowed
    parts = "; ".join(f"{pid} ({p['label']})" for _, pid, p in hits)
    reason = (
        "BOARD GATE: this request targets the bare PSOC Edge E84 AI Kit (board 'kit-pse84-ai'), "
        f"which cannot reach the TESAIoT base-board (QWA309) parts it asks for: {parts}. "
        "Re-send it for the TESAIoT Dev Kit (say 'tesaiot', board id tesaiot), or drop those parts. "
        f"If you only want to discuss it, add {cfg['override']} to the prompt.\n"
        "บอร์ด PSOC Edge E84 AI Kit เปล่า ๆ ใช้อุปกรณ์บนบอร์ดฐาน QWA309 ของ TESAIoT ไม่ได้ - "
        "ให้ระบุบอร์ด tesaiot แล้วส่งใหม่ (หรือใส่ #board-ok ถ้าแค่ต้องการถาม)")
    print(json.dumps({"decision": "block", "reason": reason}))   # ASCII-escaped: safe on any code page
    return 0


def main(argv):
    if not argv or argv[0] in ("-h", "--help"):
        print(__doc__)
        return 0
    cmd, rest = argv[0], argv[1:]
    if cmd == "hook":
        return hook()
    if cmd == "list" and rest:
        have = board_groups(rest[0])
        for gid, g in DATA["groups"].items():
            mark = "HAS " if gid in have else "NO  "
            print(f"{mark} {gid}: {g['name']}")
            for pid, p in g["parts"].items():
                print(f"       {pid:15s} {p['label']}")
        return 0
    if cmd == "check" and len(rest) >= 2:
        return verdict(rest[0], find_parts(" ".join(rest[1:])))
    if cmd == "app" and rest:
        appdir = Path(rest[0])
        env = appdir / "pse84-app.env"
        if not env.is_file():
            print(f"GATE: BLOCKED {appdir} has no pse84-app.env (not made by new_app.sh)")
            return 1
        m = re.search(r"^APP_BOARD=(\S+)", env.read_text(encoding="utf-8"), re.M)
        board = m.group(1) if m else "?"
        hits = scan_app(appdir)
        if len(rest) > 1:
            hits += [h for h in find_parts(" ".join(rest[1:])) if h not in hits]
        return verdict(board, hits, source=f"app {appdir.name}")
    print(__doc__)
    return 2


if __name__ == "__main__":
    try:
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    except AttributeError:
        pass
    sys.exit(main(sys.argv[1:]))
