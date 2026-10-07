# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 esphome-ventilation-connect contributors
#
# Deze component is een afgeleid werk van ESPHome, waarvan de C++/runtime onder
# GPLv3 staat. Zie LICENSE.
"""Volg de identity-velden over de tijd, uit de HERBOUWDE bytestroom.

De vorige poging zocht met een regex naar 0x22-frames binnen losse logregels.
Frames breken regelmatig over een regelgrens, en dan matcht die regex op een
halve staart plus het begin van het volgende frame -- vandaar waarden als
1456358146. Hier wordt eerst de bytestroom per richting samengesteld, met een
tijdstempel per byte, en daarna pas geframed (inclusief AA 01 -> AA unstuffing).
"""
import collections
import re
import sys

RX = re.compile(r"^\[(\d\d:\d\d:\d\d)\.\d+\].*?RX \[([A-Z>]+)\] \[\s*\d+\] ([0-9A-F ]+?)\s*\|")


def streams(path):
    raw = open(path, "rb").read()
    enc = "utf-16" if raw[:2] in (b"\xff\xfe", b"\xfe\xff") else "utf-8"
    buf = collections.defaultdict(bytearray)
    stamp = collections.defaultdict(list)
    for line in raw.decode(enc, errors="replace").splitlines():
        m = RX.match(line)
        if not m:
            continue
        for tok in m.group(3).split():
            buf[m.group(2)].append(int(tok, 16))
            stamp[m.group(2)].append(m.group(1))
    return buf, stamp


def frames(buf, stamp):
    """geef (tijd, func, id, data) -- met unstuffing, zoals de DLL-laag doet"""
    i, n = 0, len(buf)
    while i < n - 4:
        if buf[i] == 0xAA and buf[i + 1] == 0x55:
            j, raw = i + 2, bytearray()
            while j < n:
                if buf[j] == 0xAA:
                    if j + 1 < n and buf[j + 1] == 0x01:
                        raw.append(0xAA)
                        j += 2
                        continue
                    break
                raw.append(buf[j])
                j += 1
            if len(raw) >= 4:
                ln = raw[0]
                if 2 <= ln <= 40 and len(raw) >= ln + 3:
                    yield stamp[i], raw[1], raw[2], bytes(raw[3:ln + 1])
            i = j
        else:
            i += 1


if __name__ == "__main__":
    path = sys.argv[1]
    buf, stamp = streams(path)
    last, hist = {}, []
    for ts, func, fid, data in frames(buf["PRT>BOX"], stamp["PRT>BOX"]):
        if func != 0x22 or len(data) < 7 or data[0] != 0x01:
            continue
        f, g = data[1], data[2]
        v = data[3] | data[4] << 8 | data[5] << 16 | data[6] << 24
        k = (g, f)
        if k in last and last[k] != v:
            hist.append((ts, g, f, last[k], v))
        last[k] = v

    print(f"identity-velden gevolgd: {len(last)}")
    print(f"echte wijzigingen: {len(hist)}\n")
    for ts, g, f, a, b in hist:
        note = "   <-- UpTime, loopt altijd op" if (g, f) == (3, 4) else ""
        print(f"  {ts}  g{g}/f{f:02X}: {a} -> {b}{note}")

    print("\nkandidaatvelden voor CLOUD (eindwaarde):")
    for k in ((0, 0), (0, 1), (2, 6), (3, 8), (4, 0), (5, 0), (6, 0)):
        if k in last:
            print(f"  g{k[0]}/f{k[1]:02X} = {last[k]}")
