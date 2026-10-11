#!/usr/bin/env python3
"""Which values move when the user does X?  Compare marked phases of a live-view raw log.

  phase_diff.py <raw.log> [--tag EZW --tag FRAME] [--ignore w1,b2] [--rest REGEX] [--margin 4]

Input: the raw log of sensor_dash.py (<app>/logs/liveview-*.log): board lines '... < TAG k=v k=v',
phase markers '... # mark <name>' (dash_ctl.py mark <name> while the live view runs).
For every TAG line, numeric key=value fields are collected per phase. Phases whose name matches
--rest (default: starts with "rest") are pooled into the reference range per field; every other
phase reports the fields that leave that range by more than max(range, --margin).
Typical use: marks SW1 / rest_after_SW1 / DIP / BTN0 ... -> "SW1: none" means the action did
not reach this data at all; a positive-control phase (e.g. SLIDER) must show changes, otherwise
the capture itself is broken. Counters that always move (scan counters) go into --ignore.
"""
import argparse
import collections
import re
import sys

LINE = re.compile(r"^\S+\s+[\d.]+\s+(?P<kind>[<>#])\s(?P<text>.*)$")
KV = re.compile(r"(\w+)=(-?\d+)(?=\s|$)")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("log")
    ap.add_argument("--tag", action="append", help="line tags to analyse (default: all tags with numbers)")
    ap.add_argument("--ignore", default="", help="comma list of field names to skip (e.g. counters)")
    ap.add_argument("--rest", default=r"^rest", help="regex of reference phase names")
    ap.add_argument("--margin", type=int, default=4)
    a = ap.parse_args()
    ignore = {x.strip() for x in a.ignore.split(",") if x.strip()}
    rest_re = re.compile(a.rest)

    phase = "rest"
    order = [phase]
    data = collections.defaultdict(lambda: collections.defaultdict(list))   # phase -> field -> values
    for raw in open(a.log, encoding="utf-8", errors="replace"):
        m = LINE.match(raw.rstrip("\n"))
        if not m:
            continue
        if m["kind"] == "#" and m["text"].startswith("mark "):
            phase = m["text"][5:].strip()
            if phase not in order:
                order.append(phase)
            continue
        if m["kind"] != "<":
            continue
        tag, _, rest = m["text"].partition(" ")
        if a.tag and tag not in a.tag:
            continue
        for k, v in KV.findall(rest):
            f = f"{tag}.{k}"
            if k not in ignore and f not in ignore:
                data[phase][f].append(int(v))

    ref = collections.defaultdict(list)
    for ph in order:
        if rest_re.search(ph):
            for f, vs in data[ph].items():
                ref[f] += vs
    if not ref:
        sys.exit("PHASE: no reference (rest) samples - mark rest phases")
    rng = {f: (min(v), max(v)) for f, v in ref.items()}
    print(f"reference phases: {[p for p in order if rest_re.search(p)]}  fields: {len(rng)}")
    for ph in order:
        if rest_re.search(ph) or ph == "end":
            continue
        n = max((len(v) for v in data[ph].values()), default=0)
        out = []
        for f, vs in data[ph].items():
            if f not in rng:
                continue
            lo, hi = rng[f]
            span = max(hi - lo, a.margin)
            if min(vs) < lo - span or max(vs) > hi + span:
                out.append(f"{f} {min(vs)}..{max(vs)} (rest {lo}..{hi})")
        print(f"== {ph}: {n} samples -> " + ("; ".join(out) if out else "none"))
    return 0


if __name__ == "__main__":
    sys.exit(main())
