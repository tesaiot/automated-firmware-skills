#!/usr/bin/env python3
"""Board-scope gate: a board can only use the parts of the hardware packs it is made of. The bare
PSOC Edge E84 AI Kit (board id kit-pse84-ai, pack psoc-e84-ai-kit) cannot reach the parts of the
QWA309 base board (pack qwa309: pots, SW4/SW5, LED matrix, CapSense, headers, CAN, OPTIGA ...);
requests and apps that need them must target a board that has the base board (tesaiot).

Data (scripts/packs.py): each pack's pack.json (kind, group, board names) + parts.json (parts,
prompt keywords, source symbols); BOARD_PARTS in boards/<id>/board.env (groups a board has,
BOARD_EXTENDS followed); scripts/prompt_gate.json (override token, size limit).

Usage:
  board_gate.py hook                      UserPromptSubmit hook: JSON on stdin; blocks a prompt that
                                          asks for a base-board part on its bare SoM. "#board-ok"
                                          in the prompt skips it.
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

sys.path.insert(0, str(Path(__file__).resolve().parent))
import packs  # noqa: E402

PACKS = packs.packs()
GROUPS = packs.groups()
PROMPT = json.loads((Path(__file__).resolve().parent / "prompt_gate.json").read_text(encoding="utf-8"))
SRC_SKIP = {"board", "libs", "build", "GeneratedSource", "bsps", ".git"}


def board_env(board_id):
    env, seen, bid, chain = {}, set(), board_id, []
    while bid and bid not in seen:
        seen.add(bid)
        d = packs.board_dir(bid)
        if d is None:
            if bid == board_id:
                raise SystemExit(f"GATE: BLOCKED unknown board id '{board_id}' (known: {' '.join(packs.board_ids())})")
            break
        vals = {}
        for line in (d / "board.env").read_text(encoding="utf-8").splitlines():
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
    for gid, g in GROUPS.items():
        for pid, p in g["parts"].items():
            yield gid, pid, p


def boards_with(group):
    return [b for b in packs.board_ids() if group in board_groups(b)]


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
              f"{GROUPS[g]['name']}")
    alt = sorted({b for g, _, _ in missing for b in boards_with(g)})
    print(f"ACTION: CHOOSE use a board that has it ({', '.join(alt) or 'none known'}): "
          "new_app.sh <board> <app> ... - or drop the part")
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


def blocked_by(prompt, base):
    """parts of base-board pack `base` that the prompt asks for on its bare SoM (else [])"""
    som = PACKS.get(base.get("som", ""))
    hits = [(g, pid, p) for g, pid, p in find_parts(prompt) if g == base["group"]]
    if not hits or som is None:
        return []
    names = lambda pack: any(re.search(k, prompt, re.I) for k in pack.get("names", []))
    explicit = any(re.search(k, prompt) for k in som.get("explicit_ids", []))   # case-sensitive ids
    if not explicit and (names(base) or not names(som)):
        return []                 # base board named, or no board named at all: allowed
    return hits


def hook():
    try:                                   # Claude Code sends UTF-8; don't trust the console code page
        prompt = json.loads(sys.stdin.buffer.read().decode("utf-8", "replace")).get("prompt", "") or ""
    except (ValueError, AttributeError):
        return 0
    if PROMPT["override"] in prompt or len(prompt) > PROMPT.get("max_prompt_chars", 1500):
        return 0                  # long text = agent report / pasted doc, not a request
    for base in (p for p in PACKS.values() if p.get("kind") == "baseboard"):
        hits = blocked_by(prompt, base)
        if not hits:
            continue
        som = PACKS[base["som"]]
        parts = "; ".join(f"{pid} ({p['label']})" for _, pid, p in hits)
        boards = ", ".join(base.get("boards", []))
        reason = (
            f"BOARD GATE: this request targets the bare {som.get('title', som['id'])} "
            f"(board '{', '.join(som.get('boards', []))}'), which cannot reach the {base.get('title', base['id'])} "
            f"parts it asks for: {parts}. Re-send it for a board with that base board ({boards}), or drop "
            f"those parts. If you only want to discuss it, add {PROMPT['override']} to the prompt.\n"
            "บอร์ด SoM เปล่า ๆ ใช้อุปกรณ์บนบอร์ดฐานไม่ได้ - ให้ระบุบอร์ดที่มีบอร์ดฐาน "
            f"({boards}) แล้วส่งใหม่ (หรือใส่ {PROMPT['override']} ถ้าแค่ต้องการถาม)")
        print(json.dumps({"decision": "block", "reason": reason}))   # ASCII-escaped: safe on any code page
        return 0
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
        for gid, g in GROUPS.items():
            print(f"{'HAS ' if gid in have else 'NO  '} {gid} (pack {g['pack']}): {g['name']}")
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
