#!/usr/bin/env python3
"""Hardware packs: sibling skill folders (next to pse84-firmware) that carry a pack.json.

  <skills>/pse84-firmware/      engine: scripts, exec_templates/, func_templates/, templates/
  <skills>/<pack>/pack.json     {"id", "kind": "som" | "baseboard", "group", "boards", "names", ...}
  <skills>/<pack>/parts.json    {"name", "parts": {<part-id>: {label, keywords, symbols}}}
  <skills>/<pack>/boards/<id>/  board.env + README.md
  <skills>/<pack>/bsp_templates/<component>/   (packs may also carry func_/exec_templates)

A new SoM or base board = a new pack folder; the engine finds it here, nothing else to register.
Used by board_gate.py and components.py; `packs.py` alone prints what was found, `packs.py board-dir <id>`
prints a board's profile folder (env.sh load_board).
"""
import json
import sys
from pathlib import Path

ENGINE = Path(__file__).resolve().parent.parent
SKILLS = ENGINE.parent
LAYERS = ("bsp", "func", "exec")


def packs():
    out = {}
    for f in sorted(SKILLS.glob("*/pack.json")):
        p = json.loads(f.read_text(encoding="utf-8"))
        p["_dir"] = f.parent
        out[p["id"]] = p
    return out


def board_dir(board_id):
    for d in [ENGINE / "boards" / board_id] + [p["_dir"] / "boards" / board_id for p in packs().values()]:
        if (d / "board.env").is_file():
            return d
    return None


def board_ids():
    ids = []
    for d in [ENGINE] + [p["_dir"] for p in packs().values()]:
        ids += [b.parent.name for b in sorted((d / "boards").glob("*/board.env")) if not b.parent.name.startswith("_")]
    return ids


def groups():
    """hardware groups {group: {name, parts, pack}} from every pack's parts.json"""
    out = {}
    for pid, p in packs().items():
        f = p["_dir"] / "parts.json"
        if f.is_file():
            data = json.loads(f.read_text(encoding="utf-8"))
            out[p["group"]] = {"name": data.get("name", p.get("title", pid)), "parts": data.get("parts", {}), "pack": pid}
    return out


def read_env(path):
    """KEY=value lines (quotes and trailing # comments stripped) -> dict; {} if missing"""
    import re
    vals = {}
    p = Path(path) if path else None
    if p and p.is_file():
        for line in p.read_text(encoding="utf-8").splitlines():
            m = re.match(r'\s*([A-Z_][A-Z0-9_]*)=("([^"]*)"|[^#\s]*)', line)
            if m:
                vals[m.group(1)] = m.group(3) if m.group(3) is not None else m.group(2)
    return vals


def _native(p):
    """MSYS /d/x -> D:/x so Windows Python can open paths written for Git Bash"""
    import re
    m = re.match(r"^/([a-zA-Z])/(.*)$", p or "")
    return f"{m.group(1).upper()}:/{m.group(2)}" if m and sys.platform == "win32" else p


def local_settings():
    """<project>/pse84.local.env (same per-PC file env.sh reads), environment variables win"""
    import os
    project = SKILLS.parent.parent if SKILLS.parent.name == ".claude" else None
    vals = read_env(project / "pse84.local.env") if project else {}
    for k in list(vals) + ["PSE84_WS", "PSE84_BENCH_DIR", "PSE84_BACKUP_DIR"]:
        if os.environ.get(k):
            vals[k] = os.environ[k]
    return {k: _native(v) for k, v in vals.items()}, project


def workspace():
    """apps workspace, same rule as env.sh: PSE84_WS (env or pse84.local.env), else <project>/apps"""
    s, project = local_settings()
    return Path(s["PSE84_WS"]) if s.get("PSE84_WS") else (project or Path.cwd()) / "apps"


def bench_file(board_id):
    s, _ = local_settings()
    return Path(s.get("PSE84_BENCH_DIR") or workspace() / ".bench") / f"{board_id}.env"


def board_env(board_id):
    """board profile with BOARD_EXTENDS parents applied (parent first, child overrides)"""
    chain, bid, seen = [], board_id, set()
    while bid and bid not in seen:
        seen.add(bid)
        d = board_dir(bid)
        if d is None:
            break
        vals = read_env(d / "board.env")
        chain.append(vals)
        bid = vals.get("BOARD_EXTENDS")
    env = {}
    for vals in reversed(chain):
        env.update(vals)
    return env


def component_roots(layer):
    """<layer>_templates folders: engine first, then packs"""
    roots = [ENGINE / f"{layer}_templates"] + [p["_dir"] / f"{layer}_templates" for p in packs().values()]
    return [r for r in roots if r.is_dir()]


if __name__ == "__main__":
    if len(sys.argv) == 3 and sys.argv[1] == "board-dir":
        d = board_dir(sys.argv[2])
        print(d.as_posix() if d else "")
        sys.exit(0 if d else 1)
    for pid, p in packs().items():
        print(f"{pid:18s} {p['kind']:9s} group={p['group']:8s} boards={','.join(p.get('boards', []))}  {p.get('title', '')}")
