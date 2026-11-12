"""Count the triangles of the original CYPE 3D shell mesh from mesh_v.png using hand-read node positions
(screen pixels, 1920x1080 capture of the isovalues viewer, Deformada, scale 0, Discretizacion on).
Edges are validated against the image: a node pair is an edge when >= 93 % of the points along the segment are coloured
(saturated) pixels.  Triangles = empty 3-cliques of that edge graph.
Plate 4 x 4 m, 1.0 m maximum side (default).  Corner pixels: L (0,0) (110,541), B (4,0) (960,966), T (0,4) (960,115), R (4,4) (1812,541).
"""
import itertools
import math
import sys

import numpy as np
from PIL import Image

IMG = sys.argv[1] if len(sys.argv) > 1 else "D:/Coding/CypeClwon/docs/screenshots/mesh_v.png"
img = np.asarray(Image.open(IMG).convert("RGB")).astype(int)
sat = img.max(axis=2) - img.min(axis=2)
mask = sat > 25

corners = {"L": (110, 541), "B": (960, 966), "T": (960, 115), "R": (1812, 541)}
boundary = [
    (110, 541), (960, 966), (960, 115), (1812, 541),
    (279, 457), (412, 390), (576, 308), (703, 244), (836, 178),
    (1061, 166), (1208, 239), (1348, 309), (1516, 393), (1651, 461),
    (325, 648), (438, 704), (527, 749), (677, 824), (801, 886),
    (1116, 888), (1245, 824), (1364, 764), (1471, 710), (1541, 675), (1643, 625),
]
interior = [
    (1014, 265), (820, 298), (1168, 335), (766, 399), (975, 371), (580, 416), (1311, 391), (463, 481), (940, 490),
    (1152, 436), (1371, 483), (678, 507), (335, 541), (1162, 541), (1508, 554), (504, 576), (830, 577), (1000, 604),
    (1334, 590), (679, 618), (1176, 642), (535, 664), (849, 665), (1465, 637), (689, 715), (1028, 704), (1334, 680),
    (1203, 736), (868, 780), (1071, 799), (962, 863), (1654, 545),
]
nodes = boundary + interior
Lx, Ly = corners["L"]
a = np.array([corners["B"][0] - Lx, corners["B"][1] - Ly], float)
b = np.array([corners["T"][0] - Lx, corners["T"][1] - Ly], float)
M = np.array([a, b]).T


def to_m(p):
    s = np.linalg.solve(M, np.array([p[0] - Lx, p[1] - Ly], float))
    return s * 4.0


def snap(p, r=6):
    """snap a hand-read point to the centroid of coloured pixels around it"""
    x, y = p
    ys, xs = np.nonzero(mask[y - r:y + r + 1, x - r:x + r + 1])
    if len(xs) == 0:
        return p
    return (x - r + xs.mean(), y - r + ys.mean())


nodes = [snap(p) for p in nodes]
P = [to_m(p) for p in nodes]
n = len(nodes)


def frac(i, j):
    p, q = np.array(nodes[i]), np.array(nodes[j])
    L = np.hypot(*(q - p))
    k = max(int(L / 2), 4)
    hit = 0
    for t in np.linspace(0.08, 0.92, k):
        x, y = p + (q - p) * t
        xi, yi = int(round(x)), int(round(y))
        if mask[yi - 1:yi + 2, xi - 1:xi + 2].any():
            hit += 1
    return hit / k


edges = set()
for i, j in itertools.combinations(range(n), 2):
    d = math.dist(P[i], P[j])
    if d > 1.8:
        continue
    if frac(i, j) >= 0.97:
        edges.add((i, j))
adj = {i: set() for i in range(n)}
for i, j in edges:
    adj[i].add(j); adj[j].add(i)
tris = []
for i, j in edges:
    for k in adj[i] & adj[j]:
        if k > j:
            tris.append((i, j, k))


def inside(p, t):
    (x1, y1), (x2, y2), (x3, y3) = [P[v] for v in t]
    d = (y2 - y3) * (x1 - x3) + (x3 - x2) * (y1 - y3)
    l1 = ((y2 - y3) * (p[0] - x3) + (x3 - x2) * (p[1] - y3)) / d
    l2 = ((y3 - y1) * (p[0] - x3) + (x1 - x3) * (p[1] - y3)) / d
    l3 = 1 - l1 - l2
    return min(l1, l2, l3) > 1e-6


empty = [t for t in tris if not any(inside(P[v], t) for v in range(n) if v not in t)]
area = sum(abs((P[t[1]][0] - P[t[0]][0]) * (P[t[2]][1] - P[t[0]][1]) - (P[t[2]][0] - P[t[0]][0]) * (P[t[1]][1] - P[t[0]][1])) / 2 for t in empty)
print("nodes", n, "boundary", len(boundary), "edges", len(edges), "empty triangles", len(empty), "area", round(area, 3))
lens = sorted(math.dist(P[i], P[j]) for i, j in edges)
print("edge length min/median/max", round(lens[0], 3), round(lens[len(lens) // 2], 3), round(lens[-1], 3))
for i, p in enumerate(P):
    print(i, "%.2f %.2f" % (p[0], p[1]), "B" if i < len(boundary) else "I")

# --- validation against a Delaunay triangulation of the hand-read node set ---
sys.path.insert(0, "D:/Coding/CypeClwon/docs/research/data")
from mesh_sim import delaunay  # noqa: E402
rnd = np.random.default_rng(1)
jit = [(p[0] + rnd.uniform(-1e-7, 1e-7), p[1] + rnd.uniform(-1e-7, 1e-7)) for p in P]
dt = delaunay(jit)
de = set()
for t in dt:
    for u, v in ((t[0], t[1]), (t[1], t[2]), (t[2], t[0])):
        de.add((min(u, v), max(u, v)))
bad = [(e, round(frac(*e), 2)) for e in sorted(de) if frac(*e) < 0.9]
print("Delaunay triangles", len(dt), "edges", len(de), "edges missing in image", len(bad))
print(bad[:20])
