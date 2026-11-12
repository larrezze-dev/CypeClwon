"""Simulation of the original CYPE 3D shell mesher (geom2d.dll triang3d_mejora_aspecto_mallado).

Rule read with REA (see docs/research/elemento_lamina_bandas.md section 15):
  * boundary edge of length L, size s: intermediate points = 0 if L <= 1.01 s else floor((L - 0.001 s) / s)
  * Delaunay triangulation of the boundary points
  * repeat: every interior edge p1p2 (opposite vertices p3, p4) is bisected at its midpoint when
        L > 1.01 s
     or (L <= 1.01 s and 2.02 * 0.35 s < L and one of the "short neighbours" tests of FUN_18005c3e0 holds)
    until no edge is marked.
Pure Python, no dependencies. Cocircular ties are broken by a deterministic jitter (seeded).
"""
import math
import random
import sys


def circum(a, b, c):
    ax, ay = a; bx, by = b; cx, cy = c
    d = 2 * (ax * (by - cy) + bx * (cy - ay) + cx * (ay - by))
    if abs(d) < 1e-18:
        return None
    ux = ((ax * ax + ay * ay) * (by - cy) + (bx * bx + by * by) * (cy - ay) + (cx * cx + cy * cy) * (ay - by)) / d
    uy = ((ax * ax + ay * ay) * (cx - bx) + (bx * bx + by * by) * (ax - cx) + (cx * cx + cy * cy) * (bx - ax)) / d
    return ux, uy, (ux - ax) ** 2 + (uy - ay) ** 2


def delaunay(pts):
    big = 1e4
    st = [(-big, -big), (big, -big), (0, big)]
    P = list(pts) + st
    n = len(pts)
    tris = {(n, n + 1, n + 2)}
    cc = {(n, n + 1, n + 2): circum(P[n], P[n + 1], P[n + 2])}
    for i in range(n):
        px, py = P[i]
        bad = [t for t in tris if cc[t] and (px - cc[t][0]) ** 2 + (py - cc[t][1]) ** 2 < cc[t][2]]
        edges = {}
        for t in bad:
            for e in ((t[0], t[1]), (t[1], t[2]), (t[2], t[0])):
                k = tuple(sorted(e))
                edges[k] = edges.get(k, 0) + 1
        for t in bad:
            tris.discard(t); cc.pop(t, None)
        for e, c in edges.items():
            if c == 1:
                t = (e[0], e[1], i)
                tris.add(t); cc[t] = circum(P[t[0]], P[t[1]], P[t[2]])
    return [t for t in tris if max(t) < n]


def should_split(p1, p2, p3, p4, s):
    L = math.dist(p1, p2)
    if L > 1.01 * s:
        return True
    k = 2.0
    m = 0.35 * s
    if not (k * 1.01 * m < L):
        return False
    d6 = math.dist(p1, p3); d7 = math.dist(p1, p4)
    d8 = math.dist(p2, p3); d9 = math.dist(p2, p4)
    f = k * 1.01
    if f * d6 < L and f * d7 < L: return True
    if f * d8 < L and f * d9 < L: return True
    if f * d8 < L and f * d8 < d6 and f * m < d6: return True
    if f * d6 < L and f * d6 < d8 and f * m < d8: return True
    if f * d9 < L and f * d9 < d7 and f * m < d7: return True
    if f * d7 < L and f * d7 < d9 and f * m < d9: return True
    return False


def mesh_rect(a, b, s, seed=0, boundary_split=None):
    rnd = random.Random(seed)
    pts = []
    corners = [(0, 0), (a, 0), (a, b), (0, b)]
    for i in range(4):
        p, q = corners[i], corners[(i + 1) % 4]
        L = math.dist(p, q)
        n_int = 0 if L <= 1.01 * s else int((L - 0.001 * s) / s)
        for j in range(n_int + 1):
            t = j / (n_int + 1)
            pts.append((p[0] + (q[0] - p[0]) * t, p[1] + (q[1] - p[1]) * t))
    def onb(p):
        return p[0] < 1e-9 or p[1] < 1e-9 or abs(p[0] - a) < 1e-9 or abs(p[1] - b) < 1e-9
    for it in range(60):
        jit = [(x + rnd.uniform(-1e-9, 1e-9), y + rnd.uniform(-1e-9, 1e-9)) for x, y in pts]
        tris = delaunay(jit)
        em = {}
        for t in tris:
            for i in range(3):
                u, v, w = t[i], t[(i + 1) % 3], t[(i + 2) % 3]
                em.setdefault(tuple(sorted((u, v))), []).append(w)
        new = []
        for (u, v), ws in em.items():
            if len(ws) != 2:
                continue
            p1, p2 = pts[u], pts[v]
            if onb(p1) and onb(p2) and ((abs(p1[0] - p2[0]) < 1e-9 and (p1[0] < 1e-9 or abs(p1[0] - a) < 1e-9)) or
                                        (abs(p1[1] - p2[1]) < 1e-9 and (p1[1] < 1e-9 or abs(p1[1] - b) < 1e-9))):
                continue
            if should_split(p1, p2, pts[ws[0]], pts[ws[1]], s):
                new.append(((p1[0] + p2[0]) / 2, (p1[1] + p2[1]) / 2))
        new = [p for p in set((round(x, 9), round(y, 9)) for x, y in new) if all(math.dist(p, q) > 1e-6 for q in pts)]
        if not new:
            break
        pts += new
    jit = [(x + rnd.uniform(-1e-9, 1e-9), y + rnd.uniform(-1e-9, 1e-9)) for x, y in pts]
    return pts, delaunay(jit)


if __name__ == "__main__":
    a = float(sys.argv[1]) if len(sys.argv) > 1 else 4.0
    b = float(sys.argv[2]) if len(sys.argv) > 2 else 4.0
    s = float(sys.argv[3]) if len(sys.argv) > 3 else 1.0
    res = []
    for seed in range(12):
        pts, tr = mesh_rect(a, b, s, seed)
        res.append((len(tr), len(pts)))
    print("rect", a, b, "s", s, "(triangles, nodes) per seed:", res)
