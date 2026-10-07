"""Koppel elk API-veld aan een plek in de busstroom.

    python api_bus_match.py <snifflog> <sweep_api.json>

Werkwijze: alle request/reply-paren uit de log halen (STRIKT op frame-id, want
op volgorde koppelen levert plausibel ogende onzin op), van elk antwoord alle
denkbare (offset, breedte, schaal)-lezingen maken, en daar de API-waarden in
opzoeken. Velden met precies een treffer zijn hard; velden met veel treffers
zijn te generiek (0, 1, 100) om iets te bewijzen.
"""
import collections
import json
import re
import sys

sys.path.insert(0, __import__("os").path.dirname(__file__))
from ident_trace import frames, streams

# Vraag en antwoord delen het frame-id bij svc3/4/6/12 -- maar NIET bij svc9:
# daar loopt de 0x26-teller apart (verzoek id=F1, antwoord id=16). Dat hoeft ook
# niet, want het 0x26-antwoord echoot het parameternummer zelf terug.
PAIRS = {0x0C: 0x0E, 0x10: 0x12, 0x18: 0x1A, 0x30: 0x32}


def bus_pairs(path):
    buf, stamp = streams(path)
    rep = collections.defaultdict(list)
    for ts, fn, fid, d in frames(buf["BOX>PRT"], stamp["BOX>PRT"]):
        if fn in PAIRS.values():
            rep[(fn, fid)].append(bytes(d))
    out = collections.OrderedDict()
    # svc9 los: sleutel uit het antwoord zelf (01 <lo> <hi> <4 databytes>)
    for ts, fn, fid, d in frames(buf["BOX>PRT"], stamp["BOX>PRT"]):
        if fn == 0x26 and len(d) >= 7 and d[0] == 0x01:
            out.setdefault((0x24, bytes([0x00, d[1], d[2]])), bytes(d))
    for ts, fn, fid, d in frames(buf["PRT>BOX"], stamp["PRT>BOX"]):
        if fn not in PAIRS:
            continue
        q = rep.get((PAIRS[fn], fid))
        if q:
            out.setdefault((fn, bytes(d)), q[0])
    return out


def readings(r):
    """alle plausibele getalslezingen van een antwoord"""
    for off in range(0, len(r)):
        for w in (1, 2, 4):
            if off + w > len(r):
                continue
            u = int.from_bytes(r[off:off + w], "little")
            s = u - (1 << (8 * w)) if u >= 1 << (8 * w - 1) else u
            yield off, w, u, s


def flatten(obj, prefix=""):
    if isinstance(obj, dict):
        if "Val" in obj and not isinstance(obj["Val"], (dict, list)):
            yield prefix, obj["Val"]
            return
        for k, v in obj.items():
            yield from flatten(v, f"{prefix}.{k}" if prefix else k)
    elif isinstance(obj, list):
        for i, v in enumerate(obj):
            yield from flatten(v, f"{prefix}[{i}]")


def scope(path, name):
    """welke node hoort bij dit API-veld? (None = niet node-gebonden)"""
    m = re.search(r"/nodes/(\d+)", path)
    if m:
        return int(m.group(1))
    m = re.search(r"Nodes\[(\d+)\]", name)
    if m:
        return None      # index != nodenummer; alleen via het Node-veld te bepalen
    return None


def req_node(fn, q):
    """welke node adresseert dit busverzoek? (None = onbekend/globaal)"""
    if fn in (0x10, 0x30) and len(q) >= 2:
        return q[1]
    if fn == 0x18 and len(q) >= 2:
        return q[1]
    return None


def main(logpaths, apipath):
    pairs = collections.OrderedDict()
    for lp in logpaths:
        n0 = len(pairs)
        for k, v in bus_pairs(lp).items():
            pairs.setdefault(k, v)
        print(f"  {lp.split('/')[-1]:<18} +{len(pairs)-n0} nieuwe paren")
    print(f"unieke request/reply-paren op de bus: {len(pairs)}")

    index = collections.defaultdict(list)          # waarde -> [(func, req, off, w, schaal)]
    strings = []
    for (fn, q), r in pairs.items():
        for off, w, u, s in readings(r):
            index[u].append((fn, q, off, w, 1))
            if s != u:
                index[s].append((fn, q, off, w, 1))
            if u % 1 == 0:
                index[u / 10].append((fn, q, off, w, 0.1))
        for m in re.finditer(rb"[ -~]{4,}", r):
            strings.append((fn, q, m.start(), m.group().decode("latin1")))

    raw = json.loads(open(apipath, "rb").read().decode("utf-8-sig"))
    fields = collections.OrderedDict()
    for path, body in raw.items():
        try:
            doc = json.loads(body)
        except ValueError:
            continue
        for name, val in flatten(doc):
            fields.setdefault((path, name), val)

    uniek = ambigu = niet = tekst = 0
    hard = []
    ontbreekt = []
    for (path, name), val in fields.items():
        if isinstance(val, bool) or val is None:
            continue
        if isinstance(val, str):
            hit = [x for x in strings if val and val in x[3]]
            if hit:
                tekst += 1
            continue
        hits = index.get(val, [])
        nd = scope(path, name)
        if nd is not None:
            # alleen busverzoeken die deze node adresseren tellen mee
            f = [h for h in hits if req_node(h[0], h[1]) == nd]
            if f:
                hits = f
        if not hits:
            niet += 1
            ontbreekt.append((path, name, val))
        elif len({(h[0], h[1], h[2], h[3]) for h in hits}) == 1:
            uniek += 1
            hard.append((path, name, val, hits[0]))
        else:
            ambigu += 1

    print(f"\nAPI-velden bekeken: {len(fields)}")
    print(f"  eenduidig terug te vinden op de bus : {uniek}")
    print(f"  meerdere kandidaten (te generieke waarde): {ambigu}")
    print(f"  strings teruggevonden              : {tekst}")
    print(f"  NIET op de bus gevonden            : {niet}")

    print("")
    print("=== NIET op de bus teruggevonden ===")
    for path, name, val in sorted(ontbreekt):
        print(f"  {path:<26} {name:<44} = {val}")

    print("\n=== eenduidige koppelingen ===")
    for path, name, val, (fn, q, off, w, sc) in sorted(hard)[:400]:
        qh = " ".join(f"{b:02X}" for b in q)
        print(f"  {path:<28} {name:<42} = {str(val):>10}   "
              f"func 0x{fn:02X} [{qh}] off {off} w {w}" + (" x0.1" if sc != 1 else ""))


if __name__ == "__main__":
    main(sys.argv[1:-1], sys.argv[-1])
