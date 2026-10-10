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
