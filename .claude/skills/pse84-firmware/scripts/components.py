#!/usr/bin/env python3
"""Software components: compose an app from bsp_templates/, func_templates/ and exec_templates/
found in the engine (pse84-firmware) and in every hardware pack (scripts/packs.py).

A component is a folder <layer>_templates/<id>/ with
  component.json   manifest (schema: reference/components.md)
  files/           tree copied into the app as-is (proj_cm33_ns/..., proj_cm55/..., shared/...)
  README.md        optional notes (integration, sources, licence)

Usage:
  components.py list [--layer bsp|func|exec] [--all]     catalog (--all includes planned)
  components.py show <id>                                 manifest + files
  components.py scopes                                    core scopes (exec components with "scope")
  components.py variant <template-dir> <scope|-> <models|-> [<given ids...>]
                                  pick the template's scope + per-core models from its scopes.txt
                                  (lines "dual cm33_ns=freertos cm55=baremetal"); models "cm33=freertos,cm55=..."
                                  Prints "VARIANT: <line>" + "IDS: <scope> <models> [<IPC>]" or ACTION: CHOOSE
  components.py patch <app-dir>                            apply the "patches" of the app's components to
                                  their libraries in <workspace>/mtb_shared (idempotent; build.sh runs it
                                  after getlibs). Lines: PATCH: applied|already|BLOCKED ...
  components.py check <board-id> <id...> [--opt key=value ...] [--allow-planned]
                                  resolve requires (transitively, plus BOARD_COMPONENTS of the
                                  board) and gate: pack + board parts (board_gate.py), exactly one
                                  core scope (cm33 | dual | cm55) and its rules, one exec model per
                                  core, conflicts, pin/peripheral claims, required options (e.g.
                                  flash_area of qspi-flash), status (planned = BLOCKED); hardware
                                  settings print as ACTION: JUMPER, missing choices as ACTION: CHOOSE
  components.py apply <app-dir> --board <board-id> <id...> [--opt key=value ...] [--allow-planned]
                                  check, then copy files/, write deps/*.mtb, add Makefile
                                  lines (COMPONENTS/DEFINES/INCLUDES/PREBUILD + option defines);
                                  prints APP_COMPONENTS=, APP_SCOPE=, APP_OPTIONS=, APP_HW_SETTINGS=
Gate lines: "COMPONENTS: PASS ..." (exit 0) / "COMPONENTS: BLOCKED ..." (exit 1).
"""
import json
import re
import shutil
import sys
from pathlib import Path

ORDER = {"bsp": 0, "exec": 1, "func": 2}
sys.path.insert(0, str(Path(__file__).resolve().parent))
import board_gate  # noqa: E402  (same folder)
import packs  # noqa: E402


def load():
    comps, owner = {}, {p["_dir"]: p for p in packs.packs().values()}
    for layer in packs.LAYERS:
        for root in packs.component_roots(layer):
            pack = owner.get(root.parent)
            for mf in sorted(root.glob("*/component.json")):
                c = json.loads(mf.read_text(encoding="utf-8"))
                c["_dir"] = mf.parent
                c["_pack"] = pack["id"] if pack else "pse84-firmware"
                c["_group"] = pack["group"] if pack else None
                if c.get("layer") != layer or c.get("id") != mf.parent.name:
                    raise SystemExit(f"COMPONENTS: BLOCKED bad manifest {mf} (id/layer must match folder)")
                if c["id"] in comps:
                    raise SystemExit(f"COMPONENTS: BLOCKED duplicate component id '{c['id']}' "
                                     f"({comps[c['id']]['_dir']} and {mf.parent})")
                comps[c["id"]] = c
    return comps


def board_components(board):
    """components a board always brings (BOARD_COMPONENTS in board.env, e.g. qwa309-board)"""
    return board_gate.board_env(board).get("BOARD_COMPONENTS", "").split()


def scopes(comps):
    """exec components that define a core scope: {scope: [components]}"""
    out = {}
    for c in sorted(comps.values(), key=lambda c: c["id"]):
        if c.get("scope"):
            out.setdefault(c["scope"], []).append(c)
    return out


def scope_choices(comps):
    parts = []
    for sc, cs in scopes(comps).items():
        for c in cs:
            ipc = " + an IPC template (ipc-mailbox | ipc-none)" if c.get("needs_kind") == "ipc" else ""
            parts.append(f"{sc} = {c.get('scope_title', sc)} [{c['id']}{ipc}, {c.get('status', '?')}]")
    return " | ".join(parts)


def _int(v):
    return int(str(v), 0)


def check_option(c, key, spec, value):
    """returns (defines per project, error or None) for one option value"""
    ch = spec.get("choices", {})
    if value in ch:
        return ch[value].get("defines", {}), None
    if value.startswith("custom:") and "custom" in spec:
        rule = spec["custom"]
        try:
            off, size = (_int(x) for x in value[len("custom:"):].split(","))
        except ValueError:
            return {}, f"option {key}={value}: use custom:<offset>,<size> (hex ok)"
        al, lo, hi = _int(rule["align"]), _int(rule["min_offset"]), _int(rule["max_end"])
        if off % al or size % al or size <= 0:
            return {}, f"option {key}={value}: offset and size must be multiples of {rule['align']}"
        if off < lo or off + size > hi:
            return {}, (f"option {key}={value}: must lie in [{rule['min_offset']}, {rule['max_end']}) "
                        f"({rule.get('why', '')})")
        defs = {p: [d.format(offset=f"0x{off:08X}", size=f"0x{size:08X}") for d in ds]
                for p, ds in rule.get("defines", {}).items()}
        return defs, None
    more = ", custom:<offset>,<size>" if "custom" in spec else ""
    return {}, f"option {key}={value}: unknown choice for '{c['id']}' (have: {', '.join(ch)}{more})"


def options(sel, opts):
    """(defines per project, errors, missing [(component, key, spec)]) for the selected components"""
    defs, errs, missing = {}, [], []
    for c in sel:
        for key, spec in c.get("options", {}).items():
            if key not in opts:
                missing.append((c, key, spec))
                errs.append(f"'{c['id']}' needs option {key} - nothing chosen (--opt {key}=<choice>)")
                continue
            d, e = check_option(c, key, spec, opts[key])
            if e:
                errs.append(e)
            for proj, vals in d.items():
                defs.setdefault(proj, []).extend(vals)
    return defs, errs, missing


def resolve(comps, ids, board, allow_planned=False, opts=None):
    """returns (ordered list, errors)"""
    errs, seen, out = [], set(), []

    def visit(cid, chain):
        if cid in seen:
            return
        if cid not in comps:
            errs.append(f"unknown component '{cid}'" + (f" (needed by {chain[-1]})" if chain else ""))
            return
        seen.add(cid)
        for dep in comps[cid].get("requires", {}).get("components", []):
            visit(dep, chain + [cid])
        out.append(cid)

    for cid in list(ids) + [b for b in board_components(board) if b not in ids]:
        visit(cid, [])
    found = [c for c in out if c in comps]          # dependency order, then layer order
    sel = [comps[c] for c in sorted(found, key=lambda c: (ORDER[comps[c]["layer"]], found.index(c)))]
    # status
    for c in sel:
        if c.get("status") == "planned" and not allow_planned:
            errs.append(f"'{c['id']}' is planned, not implemented yet ({c.get('summary', '')})")
    # one execution model per core
    models = {}
    for c in sel:
        for core, model in c.get("exec_model", {}).items():
            if models.setdefault(core, (model, c["id"]))[0] != model:
                errs.append(f"core {core}: exec model '{model}' ({c['id']}) conflicts with "
                            f"'{models[core][0]}' ({models[core][1]}) - pick one")
    for c in sel:
        allowed = c.get("exec_models")
        if allowed:
            for core in c.get("cores", []):
                if core in models and models[core][0] not in allowed:
                    errs.append(f"'{c['id']}' runs under {allowed} but core {core} uses '{models[core][0]}'")
    # core scope: which cores run application code (exactly one scope component per app)
    sc = {c["scope"]: c for c in sel if c.get("scope")}
    if sel and not sc:
        errs.append("no core scope chosen (cm33 | dual | cm55) - new_app.sh --scope <scope>")
    elif len(sc) > 1:
        errs.append("several core scopes: " + ", ".join(f"{k} ({v['id']})" for k, v in sc.items())
                    + " - pick one")
    elif sc:
        scope, owner = next(iter(sc.items()))
        parked, app_cores = owner.get("parked", {}), owner.get("app_cores", [])
        # one execution model per application core, none for a parked core
        for core in app_cores:
            if core not in models:
                opts = sorted(x["id"] for x in comps.values() if core in x.get("exec_model", {}))
                errs.append(f"core {core} runs application code in scope {scope} - choose its "
                            f"execution model ({' | '.join(opts)}; new_app.sh --model)")
        for core, (model, cid) in models.items():
            if core in parked:
                errs.append(f"'{cid}' sets model {model} for {core}, but scope {scope} keeps {core} "
                            f"'{parked[core]}' - drop it or choose another scope")
        # no application/driver code on a parked core (except what the scope allows, e.g. boot shim)
        allow = owner.get("scope_allows", {})
        for c in sel:
            if c["layer"] == "exec":
                continue
            for core in c.get("cores", []):
                if core in parked and c["id"] not in allow.get(core, []):
                    errs.append(f"'{c['id']}' runs on {core}, but scope {scope} keeps {core} "
                                f"'{parked[core]}' - choose scope dual")
        if owner.get("needs_kind") and not any(c.get("kind") == owner["needs_kind"] for c in sel):
            alts = sorted(x["id"] for x in comps.values() if x.get("kind") == owner["needs_kind"])
            errs.append(f"scope {scope} needs an {owner['needs_kind'].upper()} template - add one of: "
                        + ", ".join(f"{a} ({comps[a].get('status')})" for a in alts))
    # conflicts
    ids_sel = {c["id"] for c in sel}
    for c in sel:
        for x in c.get("conflicts", []):
            if x in ids_sel:
                errs.append(f"'{c['id']}' conflicts with '{x}'")
    # hardware packs: a component shipped by a pack needs a board made with that pack
    have = board_gate.board_groups(board)
    for c in sel:
        if c["_group"] and c["_group"] not in have:
            errs.append(f"'{c['id']}' is part of pack '{c['_pack']}' - board '{board}' has no "
                        f"{board_gate.GROUPS[c['_group']]['name']}")
    # pins / peripherals: one claimer per resource, unless a selected component arbitrates it
    arbit = {r for c in sel for r in c.get("arbitrates", [])}
    claimed = {}
    for c in sel:
        for r in c.get("claims", []):
            if r in claimed and r not in arbit:
                fix = [x["id"] for x in comps.values() if r in x.get("arbitrates", [])]
                errs.append(f"resource {r}: claimed by '{claimed[r]}' and '{c['id']}' - "
                            + (f"add '{fix[0]}' (arbitrates it) or drop one" if fix else "drop one"))
            claimed.setdefault(r, c["id"])
    # required options (e.g. qspi-flash flash_area)
    errs += options(sel, opts or {})[1]
    # board parts (QWA309 base board etc.)
    for c in sel:
        for part in c.get("requires", {}).get("parts", []):
            g = next((g for g, pid, _ in board_gate.all_parts() if pid == part), None)
            if g is None:
                errs.append(f"'{c['id']}' needs unknown part '{part}' (no pack parts.json lists it)")
            elif g not in have:
                errs.append(f"'{c['id']}' needs part '{part}' - board '{board}' has no "
                            f"{board_gate.GROUPS[g]['name']}")
    return sel, errs


def hw_settings(sel):
    """switch / jumper / accessory settings the selected components need (warnings, not blocks)"""
    out = []
    for c in sel:
        for h in c.get("hw_settings", []):
            if h not in out:
                out.append(h)
    return out


def gate(sel, errs, board, comps=None, opts=None):
    if errs:
        for e in errs:
            print(f"COMPONENTS: BLOCKED {e}")
        if any("board '" in e for e in errs):
            print("ACTION: CHOOSE use a board made with the needed pack (board_gate.py list <board>), "
                  "or drop that component")
        if comps and any(e.startswith("no core scope") for e in errs):
            print(f"ACTION: CHOOSE core scope (ask the user): {scope_choices(comps)}")
        if any("choose its execution model" in e for e in errs):
            print("ACTION: CHOOSE execution model per core (ask the user): baremetal = super-loop, "
                  "freertos = tasks + kernel tick; the app template must implement the combination")
        for c, key, spec in options(sel, opts or {})[2]:
            rec = spec.get("recommended")
            ch = " | ".join(f"{k} = {v.get('label', '')}" + (" (recommended)" if k == rec else "")
                            for k, v in spec.get("choices", {}).items())
            if "custom" in spec:
                ch += f" | custom:<offset>,<size> ({spec['custom'].get('why', '')})"
            print(f"ACTION: CHOOSE {key} for '{c['id']}' (ask the user: {spec.get('question', key)}): {ch}")
        return 1
    print(f"COMPONENTS: PASS board={board} " + " ".join(f"{c['layer']}:{c['id']}" for c in sel))
    for h in hw_settings(sel):
        print(f"ACTION: JUMPER (before flash/test) {h}")
    return 0


def add_make_lines(mk, key, values):
    """add values to the project's first KEY+= line (as ModusToolbox templates have one), else
    insert a KEY+= line before `include .../start.mk` - lines after start.mk have no effect"""
    text = mk.read_text(encoding="utf-8")
    for v in values:
        if re.search(rf"^{key}\+=.*(?<![\w./-]){re.escape(v)}(?![\w./-])", text, re.M):
            continue
        m = re.search(rf"^{key}\+=", text, re.M)
        if m:
            text = text[:m.end()] + v + " " + text[m.end():]
            continue
        m = re.search(r"^include .*start\.mk", text, re.M)
        if not m:
            raise SystemExit(f"COMPONENTS: BLOCKED {mk}: no {key}+= line and no start.mk include")
        text = text[:m.start()] + f"{key}+={v}\n" + text[m.start():]
    mk.write_text(text, encoding="utf-8", newline="\n")


def apply(appdir, sel, opts=None):
    appdir = Path(appdir)
    opt_defs = options(sel, opts or {})[0]
    for c in sel:
        src = c["_dir"] / "files"
        if src.is_dir():
            for f in src.rglob("*"):
                if f.is_file():
                    rel = f.relative_to(src)
                    if rel.parts[0] != "shared" and not (appdir / rel.parts[0]).is_dir():
                        raise SystemExit(f"COMPONENTS: BLOCKED '{c['id']}' needs project "
                                         f"{rel.parts[0]} - not in this app")
                    dst = appdir / rel
                    dst.parent.mkdir(parents=True, exist_ok=True)
                    shutil.copy2(f, dst)
        for proj, deps in c.get("mtb_deps", {}).items():
            ddir = appdir / proj / "deps"
            ddir.mkdir(parents=True, exist_ok=True)
            for name, line in deps.items():
                (ddir / f"{name}.mtb").write_text(line.strip() + "\n", encoding="utf-8", newline="\n")
        for proj, keys in c.get("makefile", {}).items():
            mk = appdir / proj / "Makefile"
            if not mk.is_file():
                raise SystemExit(f"COMPONENTS: BLOCKED '{c['id']}' edits {proj}/Makefile - not in this app")
            for key, values in keys.items():
                add_make_lines(mk, key, values)
    for proj, values in opt_defs.items():
        mk = appdir / proj / "Makefile"
        if not mk.is_file():
            raise SystemExit(f"COMPONENTS: BLOCKED option defines for {proj} - not in this app")
        add_make_lines(mk, "DEFINES", values)
    print("APP_COMPONENTS=" + " ".join(c["id"] for c in sel))
    print("APP_SCOPE=" + next((c["scope"] for c in sel if c.get("scope")), "none"))
    models = {core: m for c in sel for core, m in c.get("exec_model", {}).items()}
    print("APP_MODELS=" + " ".join(f"{k}={v}" for k, v in sorted(models.items())))
    print("APP_OPTIONS=" + " ".join(f"{k}={v}" for k, v in sorted((opts or {}).items())))
    print("APP_HW_SETTINGS=" + " | ".join(hw_settings(sel)))


def patch_libs(appdir, comps):
    """apply component "patches" (regex text edits of vendor library files) to mtb_shared.
    A patch: {"lib": "<mtb_shared folder>", "file": "<path in the lib version folder>",
              "find_re": "...", "replace": "...", "applied_re": "...", "why": "...", "source": "..."}
    applied_re present -> already patched; find_re present -> patch; neither -> BLOCKED (library changed)."""
    appdir = Path(appdir)
    env = appdir / "pse84-app.env"
    if not env.is_file():
        print(f"PATCH: BLOCKED {env} missing (not an app made by new_app.sh)")
        return 1
    ids = next((l.split("=", 1)[1].split() for l in env.read_text(encoding="utf-8").splitlines()
                if l.startswith("APP_COMPONENTS=")), [])
    shared, rc = appdir.parent / "mtb_shared", 0
    for cid in ids:
        for p in comps.get(cid, {}).get("patches", []):
            files = sorted((shared / p["lib"]).glob(f"*/{p['file']}"))
            if not files:
                print(f"PATCH: BLOCKED {cid}: {p['lib']}/*/{p['file']} not in {shared} - run build.sh --getlibs")
                rc = 1
                continue
            for f in files:
                with open(f, encoding="utf-8", errors="surrogateescape", newline="") as fh:
                    text = fh.read()                     # keep the file's own line endings
                rel = f.relative_to(shared)
                if re.search(p["applied_re"], text, re.M):
                    print(f"PATCH: already {cid}: {rel}")
                    continue
                new, n = re.subn(p["find_re"], p["replace"], text, count=1, flags=re.M)
                if n != 1:
                    print(f"PATCH: BLOCKED {cid}: {rel} - pattern not found (library changed? re-check: {p.get('why', '')})")
                    rc = 1
                    continue
                f.write_text(new, encoding="utf-8", errors="surrogateescape", newline="")
                print(f"PATCH: applied {cid}: {rel} ({p.get('why', '')})")
    return rc


CORE_ALIAS = {"cm33": "cm33_ns", "cm33_ns": "cm33_ns", "cm55": "cm55"}


def variant(comps, tdir, scope, models, given):
    """choose one line of <template>/scopes.txt; returns exit code, prints VARIANT/IDS or ACTION"""
    f = Path(tdir) / "scopes.txt"
    lines = [l.split() for l in f.read_text(encoding="utf-8").splitlines()
             if l.strip() and not l.lstrip().startswith("#")] if f.is_file() else []
    if not lines:
        print(f"COMPONENTS: BLOCKED {f} missing - a template must list its scope/model variants")
        return 1
    want = {}
    if models not in ("", "-"):
        for kv in models.split(","):
            core, _, model = kv.partition("=")
            if CORE_ALIAS.get(core.strip()) is None or not model:
                print(f"COMPONENTS: BLOCKED bad --model '{kv}' (use cm33=<model>,cm55=<model>)")
                return 1
            want[CORE_ALIAS[core.strip()]] = model.strip()
    variants = []
    for parts in lines:
        v = {"scope": parts[0], "models": dict(p.split("=", 1) for p in parts[1:])}
        if scope not in ("", "-") and v["scope"] != scope:
            continue
        if any(v["models"].get(c) != m for c, m in want.items()):
            continue
        variants.append(v)
    show = lambda v: v["scope"] + "".join(f" {c}={m}" for c, m in v["models"].items())
    if not variants:
        print(f"COMPONENTS: BLOCKED template {Path(tdir).name} implements only: "
              + " | ".join(" ".join(p) for p in lines) + " - its main.c files are written for those")
        return 1
    if len(variants) > 1:
        print("ACTION: CHOOSE variant (ask the user: which cores run the app, bare metal or FreeRTOS on "
              "each): " + " | ".join(show(v) for v in variants) + "  -> new_app.sh --scope S --model cm33=..,cm55=..")
        return 2
    v = variants[0]
    sc = scopes(comps).get(v["scope"])
    if not sc:
        print(f"COMPONENTS: BLOCKED unknown core scope '{v['scope']}' (have: {', '.join(scopes(comps))})")
        return 1
    owner = sc[0]
    ids = [owner["id"]]
    for core in owner.get("app_cores", []):
        model = v["models"].get(core, "baremetal")
        cid = f"{model}-{core.split('_')[0]}"
        if cid not in comps:
            print(f"COMPONENTS: BLOCKED no execution model component '{cid}' for {core}")
            return 1
        ids.append(cid)
    kind = owner.get("needs_kind")
    if not (kind and any(comps.get(g, {}).get("kind") == kind for g in given)):
        ids += owner.get("default_with", [])
    print(f"VARIANT: {show(v)}")
    print("IDS: " + " ".join(ids))
    return 0


def main(argv):
    if not argv or argv[0] in ("-h", "--help"):
        print(__doc__)
        return 0
    allow = "--allow-planned" in argv
    argv = [a for a in argv if a != "--allow-planned"]
    opts, rest_args, i = {}, [], 0
    while i < len(argv):
        if argv[i] == "--opt" and i + 1 < len(argv) and "=" in argv[i + 1]:
            k, v = argv[i + 1].split("=", 1)
            opts[k.strip()] = v.strip()
            i += 2
            continue
        rest_args.append(argv[i])
        i += 1
    argv = rest_args
    comps = load()
    cmd, rest = argv[0], argv[1:]
    if cmd == "list":
        layer = rest[rest.index("--layer") + 1] if "--layer" in rest else None
        for c in sorted(comps.values(), key=lambda c: (ORDER[c["layer"]], c["id"])):
            if (layer and c["layer"] != layer) or (c.get("status") == "planned" and "--all" not in rest):
                continue
            print(f"{c['layer']:5s} {c['id']:18s} {c.get('status', '?'):9s} {c['_pack']:16s} {','.join(c.get('cores', [])):14s} {c.get('summary', '')}")
        return 0
    if cmd == "scopes":
        for sc, cs in scopes(comps).items():
            for c in cs:
                print(f"{sc:5s} {c['id']:16s} {c.get('status', '?'):9s} {c.get('scope_title', '')}")
                for core, job in c.get("scope_jobs", {}).items():
                    print(f"      {core:8s} {job}")
        return 0
    if cmd == "variant" and len(rest) >= 3:
        return variant(comps, rest[0], rest[1], rest[2], rest[3:])
    if cmd == "patch" and rest:
        return patch_libs(rest[0], comps)
    if cmd == "show" and rest:
        c = comps.get(rest[0]) or sys.exit(f"unknown component '{rest[0]}'")
        print(json.dumps({k: v for k, v in c.items() if not k.startswith("_")} | {"pack": c["_pack"]},
                         indent=2, ensure_ascii=False))
        for f in sorted((c["_dir"] / "files").rglob("*")) if (c["_dir"] / "files").is_dir() else []:
            if f.is_file():
                print("  file:", f.relative_to(c["_dir"] / "files"))
        return 0
    if cmd == "check" and len(rest) >= 2:
        sel, errs = resolve(comps, rest[1:], rest[0], allow, opts)
        return gate(sel, errs, rest[0], comps, opts)
    if cmd == "apply" and "--board" in rest:
        i = rest.index("--board")
        board = rest[i + 1]
        ids = [a for j, a in enumerate(rest) if j not in (i, i + 1)]
        appdir, ids = ids[0], ids[1:]
        sel, errs = resolve(comps, ids, board, allow, opts)
        if gate(sel, errs, board, comps, opts):
            return 1
        apply(appdir, sel, opts)
        return 0
    print(__doc__)
    return 2


if __name__ == "__main__":
    try:
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    except AttributeError:
        pass
    sys.exit(main(sys.argv[1:]))
