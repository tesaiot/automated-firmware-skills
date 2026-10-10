#!/usr/bin/env python3
"""Sensor debugging from the PC: drive the firmware console over the KitProg3 UART and judge the
numbers it prints. Works with any app that speaks the console protocol of pse84-firmware:
  "<TAG> key=<number><unit> ..."  data lines        (ENV t=24.81C rh=55.20% p=100812Pa)
  "<TAG> ERR <status> [rslt=0x..]"  failed reads    (MAG ERR ERR_BUS rslt=0xFFFFFFFE)
  "SENSORS a=OK b=ERR_NODEV"        init status     "SCAN bus=i2c0 n=4 0x18 0x44"
  "SELFTEST a=OK rslt=0x0 ..."      self-tests      "OK ..." / "ERR ..." command replies

Usage (port: --port COMx, or --board <id> = apps/.bench/<id>.env written by discover.sh):
  sensor_debug.py ports
  sensor_debug.py send <cmd> [<cmd> ...] [--wait S]       raw console session (S s per command)
  sensor_debug.py diag                                     info + scan + selftest, decode codes
  sensor_debug.py watch [--seconds N] [--rate MS] [--tags ENV,IMU] [--csv F] [--limits F]
                                                           capture, stats per TAG.field, limits
  sensor_debug.py snapshot <file.json> [--seconds N]       save field means (before a stimulus)
  sensor_debug.py diff <file.json> [--seconds N] [--expect EXPR ...]
                                                           compare now vs snapshot; EXPR:
        TAG.field>+N  rose by >= N     TAG.field<-N  fell by >= N
        TAG.field~N   changed <= N     TAG.field!N   changed >= N (either way)
  sensor_debug.py sweep --cmd "magrate {v} 1" --values 12,25,50 [--seconds N] [--tags MAG]
                                                           run a command per value, count errors
Gate lines: "DEBUG: PASS ..." (exit 0) / "DEBUG: FAIL ..." (exit 1); also DIAG:, CHECK:, EXPECT:.
Never flashes or resets anything - it only talks to the running firmware.
"""
import argparse
import csv
import json
import math
import re
import sys
import time
from pathlib import Path

try:
    import serial
    import serial.tools.list_ports
except ImportError:                                   # pragma: no cover
    sys.exit("DEBUG: FAIL pyserial missing - pip install --user pyserial")

SKILL = Path(__file__).resolve().parent.parent
CODES = json.loads((SKILL / "reference" / "result_codes.json").read_text(encoding="utf-8"))
DATA_RE = re.compile(r"^([A-Z][A-Z0-9_]*) (.*)$")
KV_RE = re.compile(r"(\w+)=(-?\d+(?:\.\d+)?)([A-Za-z%]*)(?=\s|$)")
ERR_RE = re.compile(r"^([A-Z][A-Z0-9_]*) ERR (\S+)(?:.*?rslt=(0x[0-9A-Fa-f]+))?")
SKIP_TAGS = {"OK", "ERR", "INFO", "READY", "SENSORS", "SCAN", "SELFTEST", "EVT"}


# ---------------------------------------------------------------- serial plumbing
def find_bench(board):
    for base in [Path.cwd(), *Path.cwd().parents, SKILL.parent.parent.parent]:
        f = base / "apps" / ".bench" / f"{board}.env"
        if f.is_file():
            for line in f.read_text(encoding="utf-8").splitlines():
                if line.startswith("CONSOLE_PORT="):
                    return line.split("=", 1)[1].strip().strip("'\"")
    sys.exit(f"DEBUG: FAIL no apps/.bench/{board}.env - run pse84-firmware discover.sh {board} first")


def resolve_port(args):
    """--port, else --board -> apps/.bench/<board>.env, else the only KitProg3 port"""
    port = args.port or (find_bench(args.board) if args.board else None)
    if not port:
        ports = [p.device for p in serial.tools.list_ports.comports() if "KitProg" in (p.description or "")]
        if len(ports) != 1:
            sys.exit(f"DEBUG: FAIL give --port or --board (KitProg3 ports found: {ports or 'none'})")
        port = ports[0]
    return port


def expect_eval(expr, before, now):
    """(ok, text) for one EXPR (TAG.field>+N | <-N | ~N | !N); before/now: {key: mean}"""
    m = EXPECT_RE.match(expr)
    if not m:
        return False, f"bad expression '{expr}' (TAG.field>+N | <-N | ~N | !N)"
    k, op, n = m.group(1), m.group(2), abs(float(m.group(3)))
    if k not in now or k not in before:
        return False, f"{k} not in both captures"
    d = now[k] - before[k]
    ok = {">": d >= n, "<": d <= -n, "~": abs(d) <= n, "!": abs(d) >= n}[op]
    return ok, f"delta {d:+.3f}"


def open_port(args):
    port = resolve_port(args)
    try:
        s = serial.Serial(port, args.baud, timeout=0.05)
    except serial.SerialException as e:
        sys.exit(f"DEBUG: FAIL cannot open {port}: {e} (closed in other tools? serial monitor, IDE)")
    s.reset_input_buffer()
    return s


class Console:
    def __init__(self, ser, log=None):
        self.ser, self.buf, self.log = ser, b"", log

    def lines(self, seconds):
        end = time.time() + seconds
        while time.time() < end:
            self.buf += self.ser.read(256)
            while b"\n" in self.buf:
                raw, self.buf = self.buf.split(b"\n", 1)
                line = raw.decode(errors="replace").strip()
                if line:
                    if self.log:
                        self.log.write(f"{time.time():.3f} {line}\n")
                    yield line

    def cmd(self, text, seconds=1.5):
        self.ser.write((text + "\r").encode())
        return [l for l in self.lines(seconds) if l != text]


# ---------------------------------------------------------------- parsing + stats
# JSON-lines telemetry as printed by the E84_TouchUI / WheelSense firmware
# (https://github.com/NnopponS/WheelSense_Project/tree/prototype/tesaIoT-hackathon/firmware/E84_TouchUI):
#   {"sensor":"sht4x","t_cx100":2512,"h_rhx100":4630,"seq":7}   {"sensor":"dps368","error":"init failed"}
# key = <name>_<unit>[x<scale>]  ->  field <name>, value / scale, unit from JSON_UNITS
JSON_UNITS = {"c": "C", "rh": "%", "pct": "%", "hpa": "hPa", "pa": "Pa", "g": "g", "mg": "mg",
              "dps": "dps", "mdps": "mdps", "ut": "uT", "nt": "nT", "lux": "lux", "ms": "ms", "v": "V", "mv": "mV"}
JSON_KEY_RE = re.compile(r"^(?P<name>[a-z0-9_]+?)_(?P<unit>[a-z]+?)(?:x(?P<scale>\d+))?$")
JSON_META = {"sensor", "tag", "seq", "t", "ts", "time"}


def parse_json(line):
    try:
        obj = json.loads(line)
    except ValueError:
        return None
    tag = str(obj.get("sensor") or obj.get("tag") or "").upper() if isinstance(obj, dict) else ""
    if not tag:
        return None
    if "error" in obj:
        return ("err", tag, str(obj["error"]).replace(" ", "_"), None)
    kv = {}
    for k, v in obj.items():
        if k in JSON_META or isinstance(v, bool) or not isinstance(v, (int, float)):
            continue
        m = JSON_KEY_RE.match(k)
        if m and m.group("unit") in JSON_UNITS:
            kv[m.group("name")] = (v / int(m.group("scale") or 1), JSON_UNITS[m.group("unit")])
        else:
            kv[k] = (float(v), "")
    return ("data", tag, kv) if kv else None


def parse(line):
    """('data', tag, {field: (value, unit)}) | ('err', tag, status, rslt) | None
    Understands "<TAG> key=<num><unit>" lines and JSON-lines telemetry (parse_json)."""
    if line.startswith("{"):
        return parse_json(line)
    m = ERR_RE.match(line)
    if m and m.group(1) not in SKIP_TAGS:
        return ("err", m.group(1), m.group(2), m.group(3))
    m = DATA_RE.match(line)
    if not m or m.group(1) in SKIP_TAGS:
        return None
    kv = {k: (float(v), u) for k, v, u in KV_RE.findall(m.group(2))}
    return ("data", m.group(1), kv) if kv else None


def capture(con, seconds, tags=None, csv_path=None):
    series, units, errors = {}, {}, {}
    writer = None
    fh = open(csv_path, "w", newline="", encoding="utf-8") if csv_path else None
    if fh:
        writer = csv.writer(fh)
        writer.writerow(["t", "tag", "field", "value", "unit"])
    t0 = time.time()
    for line in con.lines(seconds):
        p = parse(line)
        if not p or (tags and p[1] not in tags):
            continue
        if p[0] == "err":
            key = (p[1], p[2], p[3])
            errors[key] = errors.get(key, 0) + 1
            continue
        for f, (v, u) in p[2].items():
            k = f"{p[1]}.{f}"
            series.setdefault(k, []).append(v)
            units[k] = u
            if writer:
                writer.writerow([f"{time.time() - t0:.3f}", p[1], f, v, u])
    if fh:
        fh.close()
    return series, units, errors


def stats(vals):
    n = len(vals)
    mean = sum(vals) / n
    sd = math.sqrt(sum((v - mean) ** 2 for v in vals) / (n - 1)) if n > 1 else 0.0
    return {"n": n, "mean": mean, "sd": sd, "min": min(vals), "max": max(vals)}


def decode(rslt):
    if not rslt:
        return ""
    v = int(rslt, 16)
    hint = CODES.get(f"0x{v:08X}")
    if hint is None and v >= 0x80000000:               # negative int8/int32 vendor code
        hint = CODES.get(str(v - (1 << 32)))
    return f" ({hint})" if hint else ""


def print_stats(series, units, errors):
    for k in sorted(series):
        s = stats(series[k])
        print(f"STAT {k:14s} n={s['n']:<4d} mean={s['mean']:.3f}{units[k]} sd={s['sd']:.3f} "
              f"min={s['min']:g} max={s['max']:g}")
    for (tag, st, rslt), n in sorted(errors.items()):
        print(f"ERRS {tag} {st} rslt={rslt or '-'} x{n}{decode(rslt)}")


# ---------------------------------------------------------------- commands
def do_send(con, a):
    for c in a.cmds:
        print(f"> {c}")
        for l in con.cmd(c, a.wait):
            print(f"  {l}")
    return 0


def do_diag(con, a):
    ok = True
    info = con.cmd("info", 1.5)
    for l in info:
        if l.startswith(("INFO", "SENSORS")):
            print(l)
    sens = next((l for l in info if l.startswith("SENSORS ")), None)
    if not sens:
        print("DIAG: console no SENSORS line - is the sensors firmware running? (send info)")
        ok = False
    else:
        for name, st in re.findall(r"(\w+)=(\S+)", sens):
            good = st == "OK"
            ok &= good
            print(f"DIAG: {name:8s} init {'OK' if good else 'FAIL ' + st}"
                  + ("" if good else f" - {CODES.get(st, '')}"))
    scan = next((l for l in con.cmd("scan", 1.5) if l.startswith("SCAN")), None)
    if scan:
        print(f"DIAG: i2c scan {scan[5:]}")
    for l in con.cmd("selftest", 3.0):
        if l.startswith("SELFTEST"):
            for name, st, rslt in re.findall(r"(\w+)=(\S+) rslt=(0x[0-9A-Fa-f]+)", l):
                good = st == "OK"
                ok &= good
                print(f"DIAG: {name:8s} self-test {st}{'' if good else decode(rslt)}")
    series, units, errors = capture(con, a.seconds)
    print_stats(series, units, errors)
    if errors:
        ok = False
    print(f"DEBUG: {'PASS' if ok else 'FAIL'} diag")
    return 0 if ok else 1


def do_watch(con, a):
    if a.rate:
        print(con.cmd(f"rate {a.rate}", 0.5))
    tags = set(a.tags.split(",")) if a.tags else None
    series, units, errors = capture(con, a.seconds, tags, a.csv)
    print_stats(series, units, errors)
    ok = not errors and bool(series)
    if a.limits:
        lim = json.loads(Path(a.limits).read_text(encoding="utf-8"))
        for k, rule in lim.get("fields", {}).items():
            if k not in series:
                if not tags or k.split(".")[0] in tags:
                    print(f"CHECK: FAIL {k} no samples")
                    ok = False
                continue
            s = stats(series[k])
            bad = []
            if "min" in rule and s["min"] < rule["min"]:
                bad.append(f"min {s['min']:g} < {rule['min']}")
            if "max" in rule and s["max"] > rule["max"]:
                bad.append(f"max {s['max']:g} > {rule['max']}")
            if "sd_max" in rule and s["sd"] > rule["sd_max"]:
                bad.append(f"noise sd {s['sd']:.3f} > {rule['sd_max']}")
            ok &= not bad
            print(f"CHECK: {'FAIL' if bad else 'PASS'} {k} mean={s['mean']:.3f}{units[k]}"
                  + (f" ({'; '.join(bad)}) - {rule.get('why', '')}" if bad else ""))
    if a.rate:
        con.cmd("rate 1000", 0.5)
    print(f"DEBUG: {'PASS' if ok else 'FAIL'} watch {a.seconds}s")
    return 0 if ok else 1


def do_snapshot(con, a):
    series, units, errors = capture(con, a.seconds)
    snap = {k: {"mean": stats(v)["mean"], "sd": stats(v)["sd"], "unit": units[k]} for k, v in series.items()}
    Path(a.file).write_text(json.dumps(snap, indent=1), encoding="utf-8")
    print_stats(series, units, errors)
    print(f"DEBUG: {'PASS' if series else 'FAIL'} snapshot {a.file} ({len(snap)} fields)")
    return 0 if series else 1


EXPECT_RE = re.compile(r"^(\w+\.\w+)([><~!])([+-]?\d+(?:\.\d+)?)$")


def do_diff(con, a):
    before = json.loads(Path(a.file).read_text(encoding="utf-8"))
    series, units, errors = capture(con, a.seconds)
    now = {k: stats(v)["mean"] for k, v in series.items()}
    for k in sorted(set(before) & set(now)):
        d = now[k] - before[k]["mean"]
        print(f"DIFF {k:14s} before={before[k]['mean']:.3f} now={now[k]:.3f} delta={d:+.3f}{units[k]}")
    ok = not errors
    for e in a.expect or []:
        m = EXPECT_RE.match(e)
        if not m:
            print(f"EXPECT: FAIL bad expression '{e}' (TAG.field>+N | <-N | ~N | !N)")
            ok = False
            continue
        good, txt = expect_eval(e, {k: v["mean"] for k, v in before.items()}, now)
        ok &= good
        print(f"EXPECT: {'PASS' if good else 'FAIL'} {e} ({txt}{units.get(m.group(1), '')})")
    print_stats({}, {}, errors)
    print(f"DEBUG: {'PASS' if ok else 'FAIL'} diff")
    return 0 if ok else 1


def do_sweep(con, a):
    ok = True
    tags = set(a.tags.split(",")) if a.tags else None
    for v in a.values.split(","):
        c = a.cmd.format(v=v)
        reply = [l for l in con.cmd(c, 0.5) if l.startswith(("OK", "ERR"))]
        series, units, errors = capture(con, a.seconds, tags)
        n_err = sum(errors.values())
        n_ok = max((len(x) for x in series.values()), default=0)
        ok &= n_err == 0 and n_ok > 0
        print(f"SWEEP {c!r:24s} reply={reply[:1]} samples={n_ok} errors={n_err}"
              + "".join(f" [{t} {s}{decode(r)}]" for (t, s, r) in errors))
    print(f"DEBUG: {'PASS' if ok else 'FAIL'} sweep")
    return 0 if ok else 1


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--port")
    ap.add_argument("--board")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--log", help="append every received line to this file")
    sub = ap.add_subparsers(dest="action", required=True)
    sub.add_parser("ports")
    p = sub.add_parser("send")
    p.add_argument("cmds", nargs="+")
    p.add_argument("--wait", type=float, default=1.5)
    p = sub.add_parser("diag")
    p.add_argument("--seconds", type=float, default=3)
    p = sub.add_parser("watch")
    p.add_argument("--seconds", type=float, default=10)
    p.add_argument("--rate", type=int)
    p.add_argument("--tags")
    p.add_argument("--csv")
    p.add_argument("--limits")
    p = sub.add_parser("snapshot")
    p.add_argument("file")
    p.add_argument("--seconds", type=float, default=5)
    p = sub.add_parser("diff")
    p.add_argument("file")
    p.add_argument("--seconds", type=float, default=5)
    p.add_argument("--expect", nargs="*")
    p = sub.add_parser("sweep")
    p.add_argument("--cmd", required=True)
    p.add_argument("--values", required=True)
    p.add_argument("--seconds", type=float, default=3)
    p.add_argument("--tags")
    a = ap.parse_args()
    try:
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    except AttributeError:
        pass
    if a.action == "ports":
        for pt in serial.tools.list_ports.comports():
            print(f"{pt.device:8s} {pt.description} {pt.serial_number or ''}")
        return 0
    with open_port(a) as ser:
        log = open(a.log, "a", encoding="utf-8") if a.log else None
        try:
            con = Console(ser, log)
            return {"send": do_send, "diag": do_diag, "watch": do_watch, "snapshot": do_snapshot,
                    "diff": do_diff, "sweep": do_sweep}[a.action](con, a)
        finally:
            if log:
                log.close()


if __name__ == "__main__":
    sys.exit(main())
