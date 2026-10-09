"""Semantic diff of two ModusToolbox design.modus files (namespace-agnostic).

Personality elements wrap <Block location=...> + <Parameters>; plain blocks carry only
aliases. <Netlist><Net><Port name=.../></Net> holds signal routing.
"""
import sys
import xml.etree.ElementTree as ET

KEY_PARAMS = ('BaudRate', 'DataRate', 'Frequency', 'frequency', 'targetFreq', 'DriveModes', 'initialState',
              'Mode', 'mode', 'ClockSource', 'sourceClock', 'intDivider', 'divider', 'Divider', 'vddaVoltage',
              'SpeedMode', 'bitRate', 'BusWidth', 'busWidth', 'cardType', 'inFlash', 'Resolution')


def local(tag):
    return tag.split('}', 1)[-1]


def load(path):
    root = ET.parse(path).getroot()
    blocks, nets = {}, set()
    for el in root.iter():
        t = local(el.tag)
        if t == 'Block':
            loc = el.get('location')
            al = [a.get('value') for c in el if local(c.tag) == 'Aliases' for a in c]
            blocks.setdefault(loc, {'aliases': al, 'pers': None, 'params': {}})['aliases'] = al
        elif t == 'Personality':
            pers = f"{el.get('template')}-{el.get('version')}"
            params = {p.get('id'): p.get('value') for c in el if local(c.tag) == 'Parameters' for p in c}
            for c in el:
                if local(c.tag) == 'Block':
                    d = blocks.setdefault(c.get('location'), {'aliases': [], 'pers': None, 'params': {}})
                    d['pers'], d['params'] = pers, params
        elif t == 'Net':
            nets.add(tuple(sorted(p.get('name') for p in el if local(p.tag) == 'Port')))
    return blocks, nets


def key(params):
    return {k: v for k, v in params.items() if k in KEY_PARAMS}


(a, an), (b, bn) = load(sys.argv[1]), load(sys.argv[2])
print(f"# blocks base={len(a)} board={len(b)}; nets base={len(an)} board={len(bn)}\n")
print("## REMOVED blocks")
for k in sorted(set(a) - set(b)):
    print(f"- {k}: {a[k]['aliases']} {a[k]['pers']}")
print("\n## ADDED blocks")
for k in sorted(set(b) - set(a)):
    print(f"- {k}: {b[k]['aliases']} {b[k]['pers']} {key(b[k]['params'])}")
print("\n## CHANGED blocks")
for k in sorted(set(a) & set(b)):
    x, y = a[k], b[k]
    if x == y:
        continue
    out = []
    if x['aliases'] != y['aliases']:
        out.append(f"aliases {x['aliases']} -> {y['aliases']}")
    if x['pers'] != y['pers']:
        out.append(f"pers {x['pers']} -> {y['pers']}")
    d = [f"{p}: {x['params'].get(p)} -> {y['params'].get(p)}"
         for p in sorted(set(x['params']) | set(y['params'])) if x['params'].get(p) != y['params'].get(p)]
    if d and x['pers'] == y['pers']:
        out.append("params {" + "; ".join(d[:15]) + (f" ...(+{len(d)-15})" if len(d) > 15 else "") + "}")
    elif y['pers']:
        out.append(f"key {key(y['params'])}")
    print(f"- {k}: " + " | ".join(out))
print("\n## NETS removed")
for n in sorted(an - bn):
    print("-", " <-> ".join(n))
print("\n## NETS added")
for n in sorted(bn - an):
    print("+", " <-> ".join(n))
