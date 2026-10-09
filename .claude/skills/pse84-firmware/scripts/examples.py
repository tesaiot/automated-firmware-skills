"""List / describe ModusToolbox code examples that support a PSOC Edge E84 BSP.

Source: Infineon's code-example manifest (the same one Project Creator reads),
cached for a day. Called by examples.sh, which supplies the BSP of a board profile.

Usage:
  python examples.py --bsp KIT_PSE84_AI [--tools 3.9] [--cache DIR] [--refresh]
                     [--category TEXT] [words ...]          # list (filter by words)
  python examples.py --bsp KIT_PSE84_AI --detail <id>        # one example in detail
  python examples.py --bsp KIT_PSE84_AI --resolve <id>       # "uri commit" for new_app.sh

An example is listed when one of its releases names the BSP's kit capability
(e.g. kit_pse84_ai) and needs ModusToolbox tools <= --tools.
"""
import argparse
import html
import os
import re
import sys
import time
import urllib.request
import xml.etree.ElementTree as ET
import signal

if hasattr(signal, "SIGPIPE"):
    signal.signal(signal.SIGPIPE, signal.SIG_DFL)

MANIFEST_URL = "https://github.com/Infineon/mtb-ce-manifest/raw/v2.X/mtb-ce-manifest-fv2.xml"

ap = argparse.ArgumentParser()
ap.add_argument("--bsp", required=True)
ap.add_argument("--tools", default="3.9")
ap.add_argument("--cache", default=os.path.join(os.path.expanduser("~"), ".cache", "pse84-firmware"))
ap.add_argument("--refresh", action="store_true")
ap.add_argument("--category")
ap.add_argument("--detail")
ap.add_argument("--resolve")
ap.add_argument("words", nargs="*")
args = ap.parse_args()
try:
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
except Exception:
    pass


def vtuple(v):
    return tuple(int(x) for x in re.findall(r"\d+", v or "0")[:3])


def manifest_path():
    os.makedirs(args.cache, exist_ok=True)
    path = os.path.join(args.cache, "mtb-ce-manifest-fv2.xml")
    fresh = os.path.exists(path) and time.time() - os.path.getmtime(path) < 86400
    if args.refresh or not fresh:
        try:
            with urllib.request.urlopen(MANIFEST_URL, timeout=60) as r, open(path + ".tmp", "wb") as f:
                f.write(r.read())
            os.replace(path + ".tmp", path)
        except Exception as e:  # offline: fall back to an old cache if there is one
            if not os.path.exists(path):
                sys.exit(f"cannot download the example manifest ({e}); GitHub must be reachable")
            print(f"WARN: using cached manifest ({e})", file=sys.stderr)
    return path


def text(el, tag):
    t = el.find(tag)
    return (t.text or "").strip() if t is not None else ""


def plain(s):
    s = re.sub(r"<br\s*/?>", " ", s)
    s = re.sub(r"<[^>]+>", "", s)
    return re.sub(r"\s+", " ", html.unescape(s)).strip()


cap = args.bsp.lower()                      # KIT_PSE84_AI -> kit_pse84_ai
tools = vtuple(args.tools)
apps = []
for app in ET.parse(manifest_path()).getroot().iter("app"):
    vers = []
    for v in app.iter("version"):
        caps = v.get("req_capabilities_per_version_v2", "") + " " + v.get("req_capabilities_per_version", "")
        if re.search(rf"\b{re.escape(cap)}\b", caps) and vtuple(v.get("tools_min_version")) <= tools:
            vers.append((text(v, "num"), text(v, "commit"), v.get("tools_min_version", "")))
    if not vers:
        continue
    releases = [x for x in vers if x[1].startswith("release-")]
    apps.append({
        "id": text(app, "id"), "name": plain(text(app, "name")), "category": plain(text(app, "category")),
        "uri": text(app, "uri"), "keywords": app.get("keywords", ""),
        "description": plain(text(app, "description")).replace("For more details, see the README on GitHub.", "").strip(),
        "pinned": releases[0][1] if releases else vers[0][1], "versions": vers,
    })
apps.sort(key=lambda a: (a["category"], a["id"]))

if args.resolve or args.detail:
    want = args.resolve or args.detail
    hit = [a for a in apps if a["id"] == want]
    if not hit:
        sys.exit(f"example '{want}' does not support {args.bsp} with tools {args.tools} (try the list)")
    a = hit[0]
    if args.resolve:
        print(a["uri"], a["pinned"])
        sys.exit(0)
    print(f"{a['name']}\n  id:        {a['id']}\n  category:  {a['category']}\n  keywords:  {a['keywords']}")
    print(f"  source:    {a['uri']}\n  pinned to: {a['pinned']}  (newest release for {args.bsp})")
    print("  releases:  " + ", ".join(v[1] for v in a["versions"][:6]))
    print(f"  README:    {a['uri']}/blob/master/README.md\n\n{a['description']}")
    sys.exit(0)

words = [w.lower() for w in args.words]
def hay(a):
    return " ".join([a["id"].replace("-", " "), a["name"], a["keywords"], a["category"]]).lower()


# each word must match the START of a word (so "ble" finds "BLE", not "double")
rows = [a for a in apps
        if all(re.search(r"(?<![a-z0-9])" + re.escape(w), hay(a)) for w in words)
        and (not args.category or args.category.lower() in a["category"].lower())]
print(f"{len(rows)} of {len(apps)} code examples for {args.bsp} (tools {args.tools})"
      + (f" matching {' '.join(args.words)}" if words else "") + ":")
cat = None
for a in rows:
    if a["category"] != cat:
        cat = a["category"]
        print(f"\n[{cat}]")
    short = a["id"].replace("mtb-example-psoc-edge-", "")
    print(f"  {short:<42} {a['name']}")
print("\nDetails: examples.sh <board> --detail <id>   Create: new_app.sh <board> <app> --example <id>")
print("(ids may be given without the 'mtb-example-psoc-edge-' prefix)")

sys.stdout.flush()
