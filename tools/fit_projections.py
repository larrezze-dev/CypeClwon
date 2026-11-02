"""Fits camera (yaw, pitch) of the original's 22 predefined projections from the axes triad of captures pj_NN.png.
Usage: python tools/fit_projections.py   (reads docs/screenshots/pj_01..22.png, prints a table)."""
import math
from PIL import Image

def dirs(path):
    im = Image.open(path).convert("RGB").crop((420, 860, 500, 980))
    px = im.load(); W, H = im.size
    pts = {"R": [], "G": [], "B": []}
    for y in range(H):
        for x in range(W):
            r, g, b = px[x, y]
            if r > 170 and g < 70 and b < 70: pts["R"].append((x, y))
            elif g > 170 and r < 90 and b < 90: pts["G"].append((x, y))
            elif b > 170 and r < 70 and g < 70: pts["B"].append((x, y))
    if not pts["B"]: return None
    ox = sum(p[0] for p in pts["B"]) / len(pts["B"]); oy = max(p[1] for p in pts["B"])
    out = {}
    for k in "RGB":
        if not pts[k]: out[k] = None; continue
        far = max(pts[k], key=lambda p: (p[0] - ox) ** 2 + (p[1] - oy) ** 2)
        out[k] = (far[0] - ox, -(far[1] - oy))
    return out

def proj(yaw, pitch):
    y, p = math.radians(yaw), math.radians(pitch)
    d = (math.sin(y) * math.cos(p), -math.cos(y) * math.cos(p), math.sin(p))
    r = (math.cos(y), math.sin(y), 0.0)
    u = (r[1] * d[2] - r[2] * d[1], r[2] * d[0] - r[0] * d[2], r[0] * d[1] - r[1] * d[0])  # r x d ... sign fixed below
    if u[2] < 0: u = tuple(-c for c in u)
    ax = [(1, 0, 0), (0, 1, 0), (0, 0, 1)]
    return [(sum(a[i] * r[i] for i in range(3)), sum(a[i] * u[i] for i in range(3))) for a in ax]

def ang(v): return math.degrees(math.atan2(v[1], v[0]))
def diff(a, b): return abs((a - b + 180) % 360 - 180)

for n in range(1, 23):
    d = dirs(f"docs/screenshots/pj_{n:02d}.png")
    if not d or not d["R"] or not d["G"]: print(n, "no data", d); continue
    ax, ay = ang(d["R"]), ang(d["G"])
    best = None
    for yaw in range(0, 360, 5):
        for pitch in range(-85, 90, 5):
            P = proj(yaw, pitch)
            if min(math.hypot(*P[0]), math.hypot(*P[1])) < 0.25: continue
            e = diff(ang(P[0]), ax) + diff(ang(P[1]), ay)
            if best is None or e < best[0]: best = (e, yaw, pitch)
    print(n, "X %4.0f Y %4.0f" % (ax, ay), "len", [round(math.hypot(*d[k])) for k in "RGB"], "-> yaw %s pitch %s err %.0f" % (best[1], best[2], best[0]))
