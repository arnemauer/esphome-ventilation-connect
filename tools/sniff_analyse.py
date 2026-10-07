# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 esphome-ventilation-connect contributors
#
# Deze component is een afgeleid werk van ESPHome, waarvan de C++/runtime onder
# GPLv3 staat. Zie LICENSE.
"""Standaardanalyse van een tweerichtings-snifflog.

    python tools/sniff_analyse.py <logbestand> [hh:mm:ss]

Doet in één keer wat bij elke sessie opnieuw nodig blijkt:
  - bytestroom per richting herbouwen, unstuffen, framen (via ident_trace)
  - functiecodes tellen voor/na een keerpunt, met de dienst erbij
  - stiltes van >=3 s (herstarts)
  - alle 0x26-antwoorden als box-parametertabel, inclusief strings
  - service 6 als (node, param) -> val/inc/min/max
  - waarschuwen bij functiecodes die de decoder in duco.cpp niet kent

Twee valkuilen die twee keer geld hebben gekost, vandaar hier vastgelegd:
  1. Zoek NOOIT met een regex naar frames binnen losse logregels -- frames breken
     over regelgrenzen en dan match je op een halve staart. Altijd eerst de
     bytestroom herbouwen.
  2. In service 6 en in 0x24 sub 00 zijn de bytes na het subcommando GEEN 16-bits
     woord. Bij svc6 is het <node> <param> apart. Bij 0x26 echoot het antwoord het
     parameternummer terug, dus koppelen aan de vraag hoeft niet.
"""
import collections
import re
import sys

from ident_trace import frames, streams

DECODER = "components/duco/duco.cpp"


def secs(t):
    h, m, s = map(int, t.split(":"))
    return h * 3600 + m * 60 + s


def known_funcs(path=DECODER):
    try:
        src = open(path, encoding="utf-8", errors="replace").read()
    except OSError:
        return None
    return {int(x, 16) for x in re.findall(r"case 0x([0-9A-Fa-f]{2})", src)}


def main(path, cut=None):
    buf, stamp = streams(path)
    F = {d: list(frames(buf[d], stamp[d])) for d in buf}
    cut = secs(cut) if cut else None

    seen = set()
    for d in F:
        print(f"\n=== {d}: {len(F[d])} frames ===")
        pre, post = collections.Counter(), collections.Counter()
        for ts, fn, _, _ in F[d]:
            (post if cut and secs(ts) >= cut else pre)[fn] += 1
            seen.add(fn)
        for fn in sorted(set(pre) | set(post)):
            col = f"{pre.get(fn, 0):6d}" + (f" / {post.get(fn, 0):6d}" if cut else "")
            print(f"   0x{fn:02X} (svc {fn >> 2:2d}): {col}")

        ts = [secs(f[0]) for f in F[d]]
        for i in range(1, len(ts)):
            if ts[i] - ts[i - 1] >= 3:
                print(f"   ! {ts[i]-ts[i-1]} s stil: {F[d][i-1][0]} -> {F[d][i][0]}")

    kn = known_funcs()
    if kn is not None:
        onbekend = sorted(seen - kn)
        print("\n=== onbekende functiecodes ===")
        print("   " + (", ".join(f"0x{f:02X}" for f in onbekend) if onbekend
                       else "geen -- alles zit al in de decoder"))

    if "BOX>PRT" not in F:
        return
    print("\n=== box-parameters uit de 0x26-antwoorden ===")
    val = collections.OrderedDict()
    for _, fn, _, d in F["BOX>PRT"]:
        if fn == 0x26 and len(d) >= 7 and d[0] == 0x01:
            val.setdefault(d[1] | d[2] << 8, bytes(d[3:7]))
    print(f"   {len(val)} parameters")
    run, start = b"", None
    for p in sorted(val):
        if start is not None and p == start + len(run) // 4 and val[p].strip(b"\x00"):
            run += val[p]
        else:
            if start is not None and len(run) > 4 and run[:1].isalnum():
                print(f"   string 0x{start:04X}: "
                      f"{run.split(b'\x00')[0].decode('latin1', 'replace')!r}")
            start, run = p, val[p]

    if "PRT>BOX" not in F:
        return
    print("\n=== service 6: (node, param) -> waarde / stap / min / max ===")
    req = [(fid, d[1], d[2]) for _, fn, fid, d in F["PRT>BOX"] if fn == 0x18 and len(d) >= 3]
    rep = [(fid, bytes(d)) for _, fn, fid, d in F["BOX>PRT"] if fn == 0x1A and len(d) >= 9]
    rows = {}
    for (fid, node, par), (fid2, r) in zip(req, rep):
        if fid == fid2:
            rows.setdefault((node, par), (r[1] | r[2] << 8, r[3] | r[4] << 8,
                                          r[5] | r[6] << 8, r[7] | r[8] << 8))
    for (node, par), (v, st, lo, hi) in sorted(rows.items()):
        print(f"   node {node:3d} param 0x{par:02X}: {v:6d}  stap {st:3d}  {lo}..{hi}")


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2] if len(sys.argv) > 2 else None)
