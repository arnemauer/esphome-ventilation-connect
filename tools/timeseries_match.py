"""Koppel LIVE API-waarden aan de bus door TIJDREEKSEN te vergelijken.

    python timeseries_match.py <snifflog> <series_api.json>

Een losse momentopname bewijst niets voor waarden die voortdurend veranderen
(druk, toerental, CO2): tegen de tijd dat je de API leest is de bus alweer
verder. Door tijdens een opname herhaald te bemonsteren en de REEKSEN te
vergelijken, valt een veld alleen samen met de juiste plek in de bus.
"""
import collections
import json
import os
import sys

sys.path.insert(0, os.path.dirname(__file__))
from ident_trace import frames, streams

PAIRS = {0x0C: 0x0E, 0x10: 0x12, 0x18: 0x1A, 0x30: 0x32}


def secs(t):
    h, m, s = map(int, t.split(":"))
    return h * 3600 + m * 60 + s


def bus_series(path):
    """(func, request) -> [(tijd, antwoordbytes)]"""
    buf, stamp = streams(path)
    rep = collections.defaultdict(list)
    for ts, fn, fid, d in frames(buf["BOX>PRT"], stamp["BOX>PRT"]):
        if fn in PAIRS.values():
            rep[(fn, fid)].append((secs(ts), bytes(d)))
    out = collections.defaultdict(list)
    for ts, fn, fid, d in frames(buf["PRT>BOX"], stamp["PRT>BOX"]):
        if fn in PAIRS and rep.get((PAIRS[fn], fid)):
            t, r = rep[(PAIRS[fn], fid)].pop(0)
            out[(fn, bytes(d))].append((t, r))
    return out


def api_series(path):
    snaps = json.loads(open(path, "rb").read().decode("utf-8-sig"))
    out = collections.defaultdict(list)
    for s in snaps:
        t = secs(s["t"])
        for doc_key in ("info", "nodes"):
            try:
                doc = json.loads(s[doc_key])
            except Exception:
                continue
            stack = [("", doc)]
            while stack:
                pre, o = stack.pop()
                if isinstance(o, dict):
                    if "Val" in o and isinstance(o["Val"], (int, float)) and not isinstance(o["Val"], bool):
                        out[pre].append((t, o["Val"]))
                    else:
                        for k, v in o.items():
                            stack.append((f"{pre}.{k}" if pre else k, v))
                elif isinstance(o, list):
                    for i, v in enumerate(o):
                        stack.append((f"{pre}[{i}]", v))
    return {k: v for k, v in out.items() if len({x[1] for x in v}) > 1}


def main(logpath, apipath):
    bus = bus_series(logpath)
    api = api_series(apipath)
    print(f"bus-reeksen: {len(bus)}   API-velden die BEWEGEN: {len(api)}\n")

    geen = []
    for field, pts in sorted(api.items()):
        best = None
        for (fn, q), samples in bus.items():
            if len(samples) < 3:
                continue
            for off in range(0, len(samples[0][1]) - 1):
                for w in (1, 2, 4):
                    if off + w > len(samples[0][1]):
                        continue
                    for sc in (1.0, 0.1, 10.0):
                        err = n = 0
                        for t, want in pts:
                            near = [s for s in samples if abs(s[0] - t) <= 45]
                            if not near:
                                continue
                            got = min(
                                (abs(int.from_bytes(r[off:off + w], "little") * sc - want)
                                 for _, r in near))
                            err += got
                            n += 1
                        if n >= max(3, len(pts) // 2):
                            avg = err / n
                            if best is None or avg < best[0]:
                                best = (avg, fn, q, off, w, sc)
        if best is None or best[0] > 1.5:
            geen.append((field, pts, best))
            continue
        if True:
            avg, fn, q, off, w, sc = best
            qh = " ".join(f"{b:02X}" for b in q)
            waarden = ", ".join(str(v) for _, v in pts[:5])
            print(f"  {field:<44} [{waarden} ...]")
            print(f"      -> func 0x{fn:02X} [{qh}] offset {off} breedte {w}"
                  + (f" x{sc}" if sc != 1.0 else "") + f"   gem. afwijking {avg:.2f}")
    _tail(geen)


def _tail(geen):
    print("")
    print("=== bewegende API-velden ZONDER match op de bus ===")
    for field, pts, best in sorted(geen):
        w = ", ".join(str(v) for _, v in pts[:6])
        b = f"beste afwijking {best[0]:.1f}" if best else "geen enkele kandidaat"
        print(f"  {field:<44} [{w} ...]   {b}")


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2])
