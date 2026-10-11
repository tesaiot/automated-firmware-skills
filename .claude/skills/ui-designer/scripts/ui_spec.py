#!/usr/bin/env python3
"""UI spec model (format pse84-ui/1): load, validate + lint, JSON Patch with #id paths, outline.

Shared by ui_server.py (editor), ui_ctl.py (agent side) and ui_gen.py (C generator).
The format is documented in ../reference/ui-spec.md. Standard library only.

CLI:
  ui_spec.py check <ui.json>            errors + warnings, exit 1 on errors
  ui_spec.py tree  <ui.json>            outline with JSON pointers (for writing patches)
"""
import copy
import json
import re
import sys
from pathlib import Path

SPEC_ID = "pse84-ui/1"
DISPLAY = {"width": 800, "height": 480}

TYPES = {
    # type: (may have children, own props)
    "container": (True,  {"layout"}),
    "label":     (False, {"text", "bind", "fmt", "long_mode"}),
    "button":    (True,  {"text", "layout", "checkable", "bind"}),
    "slider":    (False, {"min", "max", "value", "bind"}),
    "bar":       (False, {"min", "max", "value", "bind"}),
    "arc":       (False, {"min", "max", "value", "bind"}),
    "switch":    (False, {"checked", "bind"}),
    "checkbox":  (False, {"text", "checked", "bind"}),
    "dropdown":  (False, {"options", "selected", "bind"}),
    "spinner":   (False, set()),
}
COMMON = {"id", "type", "align", "x", "y", "w", "h", "hidden", "style", "grow", "children", "on", "note"}
SCREEN_KEYS = {"id", "style", "layout", "children", "note"}
ALIGNS = ["top_left", "top_mid", "top_right", "left_mid", "center", "right_mid",
          "bottom_left", "bottom_mid", "bottom_right"]
EVENTS = ["clicked", "pressed", "released", "long_pressed", "value_changed"]
FLEX_FLOWS = ["row", "column", "row_wrap", "column_wrap"]
FLEX_PLACES = ["start", "center", "end", "space_between", "space_around", "space_evenly"]
LONG_MODES = ["wrap", "dot", "scroll", "clip"]
STYLE_KEYS = {"bg", "bg_opa", "text_color", "font", "radius", "border_width", "border_color",
              "pad", "gap", "text_align", "accent", "shadow"}
FONT_SIZES = list(range(8, 50, 2))                       # LVGL's built-in Montserrat set
FONTS = [f"montserrat_{s}" for s in FONT_SIZES]
COLOR_RE = re.compile(r"^#[0-9A-Fa-f]{6}$")
ID_RE = re.compile(r"^[a-z][a-z0-9_]{0,31}$")
C_RESERVED = {"auto", "break", "case", "char", "const", "continue", "default", "do", "double", "else",
              "enum", "extern", "float", "for", "goto", "if", "inline", "int", "long", "register",
              "restrict", "return", "short", "signed", "sizeof", "static", "struct", "switch",
              "typedef", "union", "unsigned", "void", "volatile", "while", "ui", "data"}
MAX_WIDGETS, MAX_DATA, MAX_SCREENS = 255, 255, 16
MIN_TOUCH_PX = 40          # 4.3" at 130 DPI: ~8 mm
INTERACTIVE = {"button", "slider", "arc", "switch", "checkbox", "dropdown"}


def load(path):
    return json.loads(Path(path).read_text(encoding="utf-8"))


def save(path, spec):
    Path(path).write_text(json.dumps(spec, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")


def new_spec(name="ui"):
    return {"spec": SPEC_ID, "name": name, "display": dict(DISPLAY),
            "theme": {"dark": False, "primary": "#2196F3", "secondary": "#F44336", "font": "montserrat_14"},
            "data": [], "screens": [{"id": "main", "children": []}]}


# --------------------------------------------------------------------------- walking

def walk(spec):
    """yield (widget, parent, screen, pointer) in creation order (depth first)"""
    def rec(node, parent, scr, ptr):
        for i, ch in enumerate(node.get("children") or []):
            p = f"{ptr}/children/{i}"
            yield ch, node, scr, p
            yield from rec(ch, ch, scr, p)
    for si, scr in enumerate(spec.get("screens") or []):
        yield from rec(scr, scr, scr, f"/screens/{si}")


def widgets(spec):
    return [w for w, _, _, _ in walk(spec)]


def index(spec):
    """id -> pointer for screens, widgets and data"""
    out = {}
    for si, scr in enumerate(spec.get("screens") or []):
        out.setdefault(scr.get("id"), f"/screens/{si}")
    for w, _, _, p in walk(spec):
        out.setdefault(w.get("id"), p)
    for di, d in enumerate(spec.get("data") or []):
        out.setdefault("data:" + str(d.get("id")), f"/data/{di}")
    return out


def font_px(name):
    m = re.match(r"montserrat_(\d+)$", str(name or ""))
    return int(m.group(1)) if m else None


def fonts_used(spec):
    used = {spec.get("theme", {}).get("font") or "montserrat_14"}
    for scr in spec.get("screens") or []:
        if (scr.get("style") or {}).get("font"):
            used.add(scr["style"]["font"])
    for w in widgets(spec):
        if (w.get("style") or {}).get("font"):
            used.add(w["style"]["font"])
    return sorted(used, key=lambda f: font_px(f) or 0)


def actions_of(handler):
    if handler is None:
        return []
    return handler if isinstance(handler, list) else [handler]


# --------------------------------------------------------------------------- validation

def _size_ok(v):
    return (isinstance(v, int) and 0 <= v <= 4096) or v == "content" or \
        (isinstance(v, str) and re.fullmatch(r"(100|[1-9]?\d)%", v) is not None)


def _check_style(st, where, err):
    if not isinstance(st, dict):
        return err(f"{where}: style must be an object")
    for k, v in st.items():
        if k not in STYLE_KEYS:
            err(f"{where}: unknown style key '{k}' (allowed: {', '.join(sorted(STYLE_KEYS))})")
        elif k in ("bg", "text_color", "border_color", "accent") and not (isinstance(v, str) and COLOR_RE.match(v)):
            err(f"{where}: style.{k} must be '#RRGGBB'")
        elif k == "bg_opa" and not (isinstance(v, int) and 0 <= v <= 100):
            err(f"{where}: style.bg_opa is a percentage 0..100")
        elif k == "font" and v not in FONTS:
            err(f"{where}: font '{v}' - use montserrat_8 .. montserrat_48 (even sizes)")
        elif k in ("radius", "border_width", "pad", "gap", "shadow") and not (isinstance(v, int) and 0 <= v <= 400):
            err(f"{where}: style.{k} must be an integer 0..400 (px)")
        elif k == "text_align" and v not in ("left", "center", "right"):
            err(f"{where}: style.text_align is left|center|right")


def _check_layout(lay, where, err):
    if not isinstance(lay, dict):
        return err(f"{where}: layout must be an object")
    for k in lay:
        if k not in ("flex", "gap", "main", "cross"):
            err(f"{where}: unknown layout key '{k}' (flex, gap, main, cross)")
    if lay.get("flex") not in FLEX_FLOWS:
        err(f"{where}: layout.flex must be one of {FLEX_FLOWS}")
    for k in ("main", "cross"):
        if k in lay and lay[k] not in FLEX_PLACES:
            err(f"{where}: layout.{k} must be one of {FLEX_PLACES}")
    if "gap" in lay and not (isinstance(lay["gap"], int) and 0 <= lay["gap"] <= 400):
        err(f"{where}: layout.gap must be an integer px")


def check(spec, boxes=None):
    """-> (errors, warnings). boxes = {id: [x, y, w, h]} from the editor's layout (optional)."""
    errors, warnings = [], []
    err, warn = errors.append, warnings.append
    if not isinstance(spec, dict):
        return ["spec must be a JSON object"], []
    if spec.get("spec") != SPEC_ID:
        err(f"'spec' must be '{SPEC_ID}'")
    if not isinstance(spec.get("name"), str) or not spec["name"]:
        err("'name' (text) is required")
    disp = spec.get("display") or {}
    if disp and (disp.get("width"), disp.get("height")) != (DISPLAY["width"], DISPLAY["height"]):
        err(f"display must be {DISPLAY['width']}x{DISPLAY['height']} (Waveshare 4.3\" DSI)")
    th = spec.get("theme") or {}
    for k, v in th.items():
        if k == "dark" and not isinstance(v, bool):
            err("theme.dark must be true/false")
        elif k in ("primary", "secondary") and not (isinstance(v, str) and COLOR_RE.match(v)):
            err(f"theme.{k} must be '#RRGGBB'")
        elif k == "font" and v not in FONTS:
            err(f"theme.font '{v}' - use montserrat_8 .. montserrat_48")
        elif k not in ("dark", "primary", "secondary", "font"):
            err(f"unknown theme key '{k}'")

    seen = {}

    def claim(i, what):
        if not isinstance(i, str) or not ID_RE.match(i):
            err(f"{what} id {i!r}: lower_snake_case C identifier, max 32 chars, starts with a letter")
            return
        if i in C_RESERVED:
            err(f"{what} id '{i}' is reserved")
        if i in seen:
            err(f"duplicate id '{i}' ({seen[i]} and {what})")
        seen[i] = what

    data = spec.get("data") or []
    if not isinstance(data, list):
        err("'data' must be a list")
        data = []
    data_ids = set()
    for d in data:
        if not isinstance(d, dict):
            err("data entries are objects")
            continue
        claim(d.get("id"), "data")
        data_ids.add(d.get("id"))
        for k in d:
            if k not in ("id", "init", "min", "max", "source", "note"):
                err(f"data '{d.get('id')}': unknown key '{k}'")
        for k in ("init", "min", "max"):
            if k in d and not (isinstance(d[k], int) and -2**31 <= d[k] < 2**31):
                err(f"data '{d.get('id')}': {k} must be a 32-bit integer")
        if isinstance(d.get("min"), int) and isinstance(d.get("max"), int) and d["min"] > d["max"]:
            err(f"data '{d.get('id')}': min > max")
    if len(data) > MAX_DATA:
        err(f"too many data entries ({len(data)} > {MAX_DATA})")

    screens = spec.get("screens")
    if not isinstance(screens, list) or not screens:
        err("'screens' must be a non-empty list")
        screens = []
    if len(screens) > MAX_SCREENS:
        err(f"too many screens ({len(screens)} > {MAX_SCREENS})")
    screen_ids = set()
    for s in screens:
        if not isinstance(s, dict):
            err("screens are objects")
            continue
        claim(s.get("id"), "screen")
        screen_ids.add(s.get("id"))
        for k in s:
            if k not in SCREEN_KEYS:
                err(f"screen '{s.get('id')}': unknown key '{k}'")
        if "style" in s:
            _check_style(s["style"], f"screen '{s.get('id')}'", err)
        if "layout" in s:
            _check_layout(s["layout"], f"screen '{s.get('id')}'", err)

    count = 0
    for w, parent, scr, ptr in walk(spec):
        count += 1
        if not isinstance(w, dict):
            err(f"{ptr}: widget must be an object")
            continue
        wid, t = w.get("id"), w.get("type")
        where = f"'{wid}' ({ptr})"
        claim(wid, t or "widget")
        if t not in TYPES:
            err(f"{where}: type {t!r} - one of {', '.join(TYPES)}")
            continue
        kids_ok, own = TYPES[t]
        for k in w:
            if k not in COMMON and k not in own:
                err(f"{where}: key '{k}' not valid for {t} (own: {', '.join(sorted(own)) or '-'})")
        if w.get("children") and not kids_ok:
            err(f"{where}: a {t} has no children")
        if "align" in w and w["align"] not in ALIGNS:
            err(f"{where}: align must be one of {ALIGNS}")
        for k in ("x", "y"):
            if k in w and not (isinstance(w[k], int) and -2000 <= w[k] <= 2000):
                err(f"{where}: {k} must be an integer px")
        for k in ("w", "h"):
            if k in w and not _size_ok(w[k]):
                err(f"{where}: {k} must be px (int), 'content' or 'N%'")
        if "grow" in w and not (isinstance(w["grow"], int) and 0 <= w["grow"] <= 10):
            err(f"{where}: grow is 0..10")
        if "style" in w:
            _check_style(w["style"], where, err)
        if "layout" in w:
            _check_layout(w["layout"], where, err)
        if "text" in w and not isinstance(w["text"], str):
            err(f"{where}: text must be a string")
        if "long_mode" in w and w["long_mode"] not in LONG_MODES:
            err(f"{where}: long_mode is one of {LONG_MODES}")
        for k in ("min", "max", "value", "selected"):
            if k in w and not isinstance(w[k], int):
                err(f"{where}: {k} must be an integer")
        for k in ("checked", "checkable", "hidden"):
            if k in w and not isinstance(w[k], bool):
                err(f"{where}: {k} must be true/false")
        if t == "dropdown":
            opts = w.get("options")
            if not (isinstance(opts, list) and opts and all(isinstance(o, str) and "\n" not in o for o in opts)):
                err(f"{where}: options must be a non-empty list of strings")
        if "bind" in w:
            if w["bind"] not in data_ids:
                err(f"{where}: bind '{w['bind']}' is not a data id (add it to 'data')")
            if t == "label" and "fmt" not in w:
                warn(f"{where}: label bound to '{w['bind']}' without fmt - shows the bare number (fmt '%d')")
            if t == "button" and not w.get("checkable"):
                err(f"{where}: a bound button must be checkable (bind = checked state)")
        if "fmt" in w:
            f = w["fmt"]
            bare = f.replace("%%", "") if isinstance(f, str) else ""
            if not isinstance(f, str) or bare.count("%") != 1 or \
                    not re.search(r"%[-+ 0#]*\d*(?:d|i)", bare):
                err(f"{where}: fmt needs exactly one %d (use %% for a percent sign)")
            if "bind" not in w:
                err(f"{where}: fmt without bind")
        if t in ("slider", "bar", "arc"):
            lo, hi = w.get("min", 0), w.get("max", 100)
            if isinstance(lo, int) and isinstance(hi, int) and lo >= hi:
                err(f"{where}: min must be < max")
        on = w.get("on")
        if on is not None:
            if not isinstance(on, dict):
                err(f"{where}: on must be an object {{event: action}}")
                on = {}
            for ev, h in on.items():
                if ev not in EVENTS:
                    err(f"{where}: event '{ev}' - one of {EVENTS}")
                for a in actions_of(h):
                    if a == "user":
                        continue
                    if not isinstance(a, dict) or len([k for k in a if k in ("goto", "set", "inc", "toggle")]) != 1:
                        err(f"{where}: action {a!r} - \"user\" | {{goto}} | {{set, value}} | {{inc, by}} | {{toggle}}")
                        continue
                    if "goto" in a and a["goto"] not in screen_ids:
                        err(f"{where}: goto '{a['goto']}' is not a screen id")
                    for k in ("set", "inc", "toggle"):
                        if k in a and a[k] not in data_ids:
                            err(f"{where}: {k} '{a[k]}' is not a data id")
                    if "set" in a and not isinstance(a.get("value"), int):
                        err(f"{where}: set needs an integer value")
                    if "inc" in a and not isinstance(a.get("by", 1), int):
                        err(f"{where}: inc 'by' must be an integer")
            if t not in INTERACTIVE and on and "clicked" in on:
                warn(f"{where}: a {t} is not clickable by default - use a button")
        if parent.get("layout") and ("align" in w or "x" in w or "y" in w):
            warn(f"{where}: parent has a flex layout - align/x/y are ignored")
    if count > MAX_WIDGETS:
        err(f"too many widgets ({count} > {MAX_WIDGETS})")
    if len(fonts_used(spec)) > 4:
        warn(f"{len(fonts_used(spec))} font sizes compiled in ({', '.join(fonts_used(spec))}) - each costs flash")

    if boxes:
        wmap = {w.get("id"): (w, parent) for w, parent, _, _ in walk(spec)}
        W, H = DISPLAY["width"], DISPLAY["height"]
        for wid, b in boxes.items():
            if wid not in wmap or not isinstance(b, list) or len(b) != 4:
                continue
            w, parent = wmap[wid]
            x, y, bw, bh = b
            if w.get("hidden"):
                continue
            if x < 0 or y < 0 or x + bw > W or y + bh > H:
                warn(f"'{wid}': outside the display (editor layout {x},{y} {bw}x{bh})")
            if w.get("type") in INTERACTIVE and min(bw, bh) < MIN_TOUCH_PX and w.get("type") not in ("slider",):
                warn(f"'{wid}': touch target {bw}x{bh} px < {MIN_TOUCH_PX} px")
            if w.get("type") == "slider" and bh < 10:
                warn(f"'{wid}': slider only {bh} px high")
        # overlaps between siblings without a flex layout
        sib = {}
        for w, parent, _, _ in walk(spec):
            if not parent.get("layout") and not w.get("hidden") and w.get("id") in boxes:
                sib.setdefault(id(parent), []).append(w.get("id"))
        for ids in sib.values():
            for i in range(len(ids)):
                for j in range(i + 1, len(ids)):
                    a, b = boxes[ids[i]], boxes[ids[j]]
                    ox = min(a[0] + a[2], b[0] + b[2]) - max(a[0], b[0])
                    oy = min(a[1] + a[3], b[1] + b[3]) - max(a[1], b[1])
                    if ox > 2 and oy > 2:
                        warn(f"'{ids[i]}' and '{ids[j]}' overlap ({ox}x{oy} px)")
    return errors, warnings


# --------------------------------------------------------------------------- JSON Patch

class PatchError(Exception):
    pass


def _resolve_ptr(spec, path):
    """RFC 6901 pointer; a segment '#<id>' jumps to that screen/widget/data item."""
    if path == "":
        return []
    if not path.startswith("/") and not path.startswith("#"):
        raise PatchError(f"path '{path}' must start with '/' or '#id'")
    segs = path.lstrip("/").split("/")
    out = []
    for s in segs:
        s = s.replace("~1", "/").replace("~0", "~")
        if s.startswith("#"):
            key = s[1:]
            idx = index(spec)
            ptr = idx.get(key) or idx.get("data:" + key)
            if not ptr:
                raise PatchError(f"no screen/widget/data with id '{key}'")
            out = [p for p in ptr.lstrip("/").split("/")]
        else:
            out.append(s)
    return out


def _get(doc, segs):
    cur = doc
    for s in segs:
        if isinstance(cur, list):
            if not s.isdigit() or int(s) >= len(cur):
                raise PatchError(f"index '{s}' out of range")
            cur = cur[int(s)]
        elif isinstance(cur, dict):
            if s not in cur:
                raise PatchError(f"no key '{s}'")
            cur = cur[s]
        else:
            raise PatchError(f"cannot descend into {type(cur).__name__} at '{s}'")
    return cur


def _parent(doc, segs):
    if not segs:
        raise PatchError("cannot modify the root")
    return _get(doc, segs[:-1]), segs[-1]


def _add(doc, segs, value):
    par, last = _parent(doc, segs)
    if isinstance(par, list):
        if last == "-":
            par.append(value)
        elif last.isdigit() and int(last) <= len(par):
            par.insert(int(last), value)
        else:
            raise PatchError(f"bad list index '{last}'")
    elif isinstance(par, dict):
        par[last] = value
    else:
        raise PatchError("add target is not a container")


def _remove(doc, segs):
    par, last = _parent(doc, segs)
    if isinstance(par, list):
        if not last.isdigit() or int(last) >= len(par):
            raise PatchError(f"bad list index '{last}'")
        return par.pop(int(last))
    if last not in par:
        raise PatchError(f"no key '{last}' to remove")
    return par.pop(last)


def apply_patch(spec, ops):
    """RFC 6902 (add, remove, replace, move, copy, test) on a copy; '#id' path segments allowed.
    Pointers are resolved against the document as it is before each op."""
    doc = copy.deepcopy(spec)
    if isinstance(ops, dict):
        ops = [ops]
    if not isinstance(ops, list):
        raise PatchError("patch must be a list of operations")
    for n, op in enumerate(ops):
        try:
            kind, path = op["op"], op["path"]
        except (KeyError, TypeError):
            raise PatchError(f"op {n}: needs 'op' and 'path'")
        try:
            segs = _resolve_ptr(doc, path)
            if kind == "add":
                _add(doc, segs, copy.deepcopy(op["value"]))
            elif kind == "remove":
                _remove(doc, segs)
            elif kind == "replace":
                if not segs:
                    doc = copy.deepcopy(op["value"])
                else:
                    _get(doc, segs)
                    par, last = _parent(doc, segs)
                    par[int(last) if isinstance(par, list) else last] = copy.deepcopy(op["value"])
            elif kind in ("move", "copy"):
                src = _resolve_ptr(doc, op["from"])
                val = copy.deepcopy(_get(doc, src))
                if kind == "move":
                    _remove(doc, src)
                    segs = _resolve_ptr(doc, path)      # indexes may have shifted
                _add(doc, segs, val)
            elif kind == "test":
                if _get(doc, segs) != op["value"]:
                    raise PatchError(f"test failed at {path}")
            else:
                raise PatchError(f"unknown op '{kind}'")
        except KeyError as e:
            raise PatchError(f"op {n} ({kind}): missing {e}")
        except PatchError as e:
            raise PatchError(f"op {n} ({kind} {path}): {e}")
    return doc


# --------------------------------------------------------------------------- outline

def outline(spec, boxes=None):
    lines = [f"{spec.get('name')}  theme dark={spec.get('theme', {}).get('dark', False)} "
             f"primary={spec.get('theme', {}).get('primary', '#2196F3')} font={spec.get('theme', {}).get('font', 'montserrat_14')}"]
    for di, d in enumerate(spec.get("data") or []):
        rng = f" {d['min']}..{d['max']}" if "min" in d and "max" in d else ""
        lines.append(f"  data {d.get('id')} init={d.get('init', 0)}{rng}  /data/{di}"
                     + (f"  source: {d['source']}" if d.get("source") else ""))

    def desc(w):
        bits = [w.get("type", "?")]
        for k in ("text", "bind", "fmt"):
            if k in w:
                bits.append(f"{k}={w[k]!r}")
        if "align" in w or "x" in w or "y" in w:
            bits.append(f"@{w.get('align', 'top_left')}{w.get('x', 0):+d}{w.get('y', 0):+d}")
        if "w" in w or "h" in w:
            bits.append(f"{w.get('w', '-')}x{w.get('h', '-')}")
        if w.get("layout"):
            bits.append(f"flex={w['layout'].get('flex')}")
        if w.get("on"):
            bits.append("on=" + ",".join(w["on"]))
        if boxes and w.get("id") in boxes:
            bits.append("box=" + ",".join(str(v) for v in boxes[w["id"]]))
        return " ".join(bits)

    def rec(node, ptr, depth):
        for i, ch in enumerate(node.get("children") or []):
            p = f"{ptr}/children/{i}"
            lines.append("  " * depth + f"{ch.get('id')}: {desc(ch)}  {p}")
            rec(ch, p, depth + 1)
    for si, s in enumerate(spec.get("screens") or []):
        lines.append(f"  screen {s.get('id')}" + (f" flex={s['layout'].get('flex')}" if s.get("layout") else "")
                     + f"  /screens/{si}")
        rec(s, f"/screens/{si}", 2)
    return "\n".join(lines)


def main(argv):
    if len(argv) != 3 or argv[1] not in ("check", "tree"):
        print(__doc__)
        return 2
    spec = load(argv[2])
    if argv[1] == "tree":
        print(outline(spec))
        return 0
    errors, warnings = check(spec)
    for e in errors:
        print("ERROR:", e)
    for w in warnings:
        print("WARN:", w)
    print(f"SPEC: {'OK' if not errors else 'INVALID'} errors={len(errors)} warnings={len(warnings)}")
    return 1 if errors else 0


if __name__ == "__main__":
    try:
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    except AttributeError:
        pass
    sys.exit(main(sys.argv))
