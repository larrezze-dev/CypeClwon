#include <algorithm>
#include <cmath>
#include <optional>
#include <unordered_map>

#include "core/fem/shell_analysis.h"

// Mesh of the shells (T-P8-02). Behaviour source: docs/research/elemento_lamina_bandas.md section 15.1 (read with REA in calcmallad3d.dll,
// modelo3d.dll and geom2d.dll):
//  - [V] Size s of a plane of shells = the maximum "Tamaño máximo de los lados de un triángulo" of its shells; an edge shared by shells of
//    different planes takes the minimum of the sizes of those planes.
//  - [V] Intermediate points of a contour edge of length L: 0 if L <= 1.01 s, else floor((L - 0.001 s) / s), equally spaced.
//  - [V] Delaunay triangulation of the points of the plane, then passes of bisection at the midpoint of every interior edge marked by the
//    rule of FUN_18005c3e0 (length > 1.01 s, or the shape test with k = 2.0, m = 0.35 s), until no edge is marked. A midpoint on a contour
//    line is never inserted, so the contour chains (and the meshes of neighbouring shells and the bars along the edges) stay conforming.
//  - [V] Section 18.1 (4th REA pass): the edge flip test (shape factor, FUN_18008bb20) used in every sweep, and the splits of the contour
//    edges of the second improvement pass (FUN_1800599d0 / FUN_18005a430). [N] the local flips after every insertion (FUN_180063ec0 not read).
//  - [N] Not read / not reproduced: the insertion routine FUN_180063ec0, the validity filter FUN_18009e0e0, the relocation of the free
//    interior points (troptimo_coordenadas_optimas), the rounding to the discrete grid of the plane and the stop by the ratio of new points.
//    The result is still 49 nodes / 80 triangles in the 4 x 4 m plate with s = 1 m; the original has 57 / 87 (25 contour nodes, irregularly
//    spaced, against 16: the contour rules read add nothing to that plate when applied once, section 18.1).

namespace cc::fem {

namespace {

using V = Vec3S;
using P2 = std::array<double, 2>;
V sub(const V& a, const V& b) { return {a[0] - b[0], a[1] - b[1], a[2] - b[2]}; }
V add(const V& a, const V& b) { return {a[0] + b[0], a[1] + b[1], a[2] + b[2]}; }
V scale(const V& a, double s) { return {a[0] * s, a[1] * s, a[2] * s}; }
double dot(const V& a, const V& b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
double norm(const V& a) { return std::sqrt(dot(a, a)); }
V lerp(const V& a, const V& b, double t) { return add(a, scale(sub(b, a), t)); }
double dist(const P2& a, const P2& b) { return std::hypot(a[0] - b[0], a[1] - b[1]); }

using EdgeKey = std::pair<int, int>;  // mesh node indices, first < second
EdgeKey key(int a, int b) { return a < b ? EdgeKey{a, b} : EdgeKey{b, a}; }

constexpr double kTol = 1e-3;  // cm3dmalla_precision_nudos_iguales = 0.001 m [V]

/// Intermediate points of a contour edge [V, FUN_180067290].
int intermediatePoints(double length, double s) { return length <= 1.01 * s ? 0 : static_cast<int>(std::floor((length - 0.001 * s) / s)); }

/// Bisection rule of an interior edge p1p2 with opposite vertices p3, p4 [V, FUN_18005c3e0].
bool shouldSplit(const P2& p1, const P2& p2, const P2& p3, const P2& p4, double s) {
    const double dx = std::fabs(p2[0] - p1[0]), dy = std::fabs(p2[1] - p1[1]), l = dist(p1, p2);
    if (dx > 1.01 * s || dy > 1.01 * s || l > 1.01 * s) return true;
    if (intermediatePoints(l, s) != 0) return true;
    constexpr double k = 2.0;
    const double m = 0.35 * s;
    if (!(k * 1.01 * m < l)) return false;
    const double d6 = dist(p1, p3), d7 = dist(p1, p4), d8 = dist(p2, p3), d9 = dist(p2, p4), f = k * 1.01;
    return (f * d6 < l && f * d7 < l) || (f * d8 < l && f * d9 < l) || (f * d8 < l && f * d8 < d6 && f * m < d6) ||
           (f * d6 < l && f * d6 < d8 && f * m < d8) || (f * d9 < l && f * d9 < d7 && f * m < d7) || (f * d7 < l && f * d7 < d9 && f * m < d9);
}

/// Incremental Delaunay triangulation (Bowyer-Watson with adjacency) of the points of one shell in its local 2D system. Points exactly on a
/// circumcircle are left outside (deterministic tie break by insertion order).
class Delaunay {
public:
    struct Tri {
        std::array<int, 3> v{};   // counter-clockwise
        std::array<int, 3> nb{};  // neighbour across the edge opposite v[i] (-1: none)
        bool alive = true;
    };
    std::vector<P2> pts;  // 0..2: super triangle
    std::vector<Tri> tris;

    explicit Delaunay(double size) {
        const double b = 1e3 * std::max(size, 1.0);
        pts = {P2{-b, -b}, P2{b, -b}, P2{0.0, b}};
        tris.push_back(Tri{{0, 1, 2}, {-1, -1, -1}, true});
    }
    static double orient(const P2& a, const P2& b, const P2& c) { return (b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0]); }
    bool inCircle(const Tri& t, const P2& p) const {
        const P2 &a = pts[t.v[0]], &b = pts[t.v[1]], &c = pts[t.v[2]];
        const double adx = a[0] - p[0], ady = a[1] - p[1], bdx = b[0] - p[0], bdy = b[1] - p[1], cdx = c[0] - p[0], cdy = c[1] - p[1];
        const double ad = adx * adx + ady * ady, bd = bdx * bdx + bdy * bdy, cd = cdx * cdx + cdy * cdy;
        const double t1 = ad * (bdx * cdy - cdx * bdy), t2 = bd * (cdx * ady - adx * cdy), t3 = cd * (adx * bdy - bdx * ady);
        return t1 + t2 + t3 > 1e-11 * (std::fabs(t1) + std::fabs(t2) + std::fabs(t3));
    }
    bool contains(const Tri& t, const P2& p) const {
        const double eps = -1e-12;
        return orient(pts[t.v[0]], pts[t.v[1]], p) >= eps && orient(pts[t.v[1]], pts[t.v[2]], p) >= eps && orient(pts[t.v[2]], pts[t.v[0]], p) >= eps;
    }
    int locate(const P2& p) {
        int t = last_ >= 0 && tris[last_].alive ? last_ : -1;
        if (t < 0)
            for (std::size_t i = tris.size(); i-- > 0;)
                if (tris[i].alive) { t = static_cast<int>(i); break; }
        for (std::size_t steps = 0; t >= 0 && steps < tris.size() + 8; ++steps) {
            const Tri& tr = tris[t];
            int next = -1;
            for (int i = 0; i < 3 && next < 0; ++i)
                if (orient(pts[tr.v[(i + 1) % 3]], pts[tr.v[(i + 2) % 3]], p) < -1e-12) next = tr.nb[i];
            if (next < 0) {
                if (contains(tr, p)) return t;
                break;
            }
            t = next;
        }
        for (std::size_t i = 0; i < tris.size(); ++i)
            if (tris[i].alive && contains(tris[i], p)) return static_cast<int>(i);
        return -1;
    }
    /// Inserts a point; false when it falls outside the super triangle or on an existing point.
    bool insert(const P2& p) {
        const int start = locate(p);
        if (start < 0) return false;
        for (int k : tris[start].v)
            if (dist(pts[k], p) < 1e-9) return false;
        const int pi = static_cast<int>(pts.size());
        pts.push_back(p);
        std::vector<int> bad{start}, stack{start};
        std::vector<char> isBad(tris.size(), 0);
        isBad[start] = 1;
        while (!stack.empty()) {
            const int t = stack.back();
            stack.pop_back();
            for (int nb : tris[t].nb)
                if (nb >= 0 && !isBad[nb] && inCircle(tris[nb], p)) {
                    isBad[nb] = 1;
                    bad.push_back(nb);
                    stack.push_back(nb);
                }
        }
        struct Edge { int a, b, outer; };
        std::vector<Edge> boundary;
        for (int t : bad)
            for (int i = 0; i < 3; ++i) {
                const int nb = tris[t].nb[i];
                if (nb < 0 || !isBad[nb]) boundary.push_back({tris[t].v[(i + 1) % 3], tris[t].v[(i + 2) % 3], nb});
            }
        for (int t : bad) tris[t].alive = false;
        std::unordered_map<int, int> startsAt, endsAt;
        std::vector<int> created;
        for (const Edge& e : boundary) {
            const int id = static_cast<int>(tris.size());
            tris.push_back(Tri{{e.a, e.b, pi}, {-1, -1, e.outer}, true});
            if (e.outer >= 0)
                for (int i = 0; i < 3; ++i) {
                    const Tri& o = tris[e.outer];
                    if (o.v[(i + 1) % 3] == e.b && o.v[(i + 2) % 3] == e.a) tris[e.outer].nb[i] = id;
                }
            startsAt[e.a] = id;
            endsAt[e.b] = id;
            created.push_back(id);
        }
        for (int id : created) {
            Tri& t = tris[id];
            t.nb[0] = startsAt.at(t.v[1]);  // edge (b, p)
            t.nb[1] = endsAt.at(t.v[0]);    // edge (p, a)
        }
        last_ = created.front();
        return true;
    }

private:
    int last_ = -1;
};

/// Shape factor of a triangle (a, b, c), counter-clockwise [V, geom2d FUN_180096be0 / FUN_1800949c0]: for every vertex, the squared distance to
/// the apex of the equilateral triangle built on the opposite side, divided by the area; summed over the three vertices (0 = equilateral).
double shapeFactor(const P2& a, const P2& b, const P2& c) {
    const double area = 0.5 * Delaunay::orient(a, b, c);
    if (area <= 0.0) return 1e300;
    const auto term = [&](const P2& p1, const P2& p2, const P2& p3) {
        const double ax = 0.5 * (p2[0] + p3[0]) - (p3[1] - p2[1]) * 0.866025404, ay = 0.5 * (p2[1] + p3[1]) + (p3[0] - p2[0]) * 0.866025404;
        return ((p1[0] - ax) * (p1[0] - ax) + (p1[1] - ay) * (p1[1] - ay)) / area;
    };
    return term(a, b, c) + term(b, c, a) + term(c, a, b);
}

/// Mutable triangulation of the inside of one contour (geom2d triang3d_*): the contour segments are the edges without a neighbour (restricted:
/// never flipped). Point insertion splits the triangle or the edge and restores the shape criterion with local flips [N: the insertion
/// routine FUN_180063ec0 was not read; the original runs the same flip test in its sweeps].
class Mesh2 {
public:
    struct Tri {
        std::array<int, 3> v{};   // counter-clockwise
        std::array<int, 3> nb{};  // neighbour across the edge opposite v[i] (-1: contour)
    };
    std::vector<P2> pts;
    std::vector<char> contour;  // the point lies on the contour
    std::vector<Tri> tris;

    /// Edge j of a triangle = (v[j], v[j+1]); its neighbour is nb[(j+2)%3].
    static int edgeNb(const Tri& t, int j) { return t.nb[(j + 2) % 3]; }

    /// Flip test of the edge opposite v[i] [V, FUN_18008bb20 with the shape option]: the quadrilateral must stay convex (both new areas
    /// >= 1e-20) and the flip is done when max(old shape factors) >= 1.00001 max(new shape factors).
    bool tryFlip(int t, int i) {
        const int u = tris[t].nb[i];
        if (u < 0) return false;
        const int a = tris[t].v[i], b = tris[t].v[(i + 1) % 3], c = tris[t].v[(i + 2) % 3];
        int j = 0;
        while (j < 3 && (tris[u].v[j] == b || tris[u].v[j] == c)) ++j;
        const int d = tris[u].v[j];
        const P2 &pa = pts[a], &pb = pts[b], &pc = pts[c], &pd = pts[d];
        if (0.5 * Delaunay::orient(pa, pb, pd) < 1e-20 || 0.5 * Delaunay::orient(pa, pd, pc) < 1e-20) return false;
        const double before = std::max(shapeFactor(pa, pb, pc), shapeFactor(pd, pc, pb));
        const double after = std::max(shapeFactor(pa, pb, pd), shapeFactor(pa, pd, pc));
        if (before < after * 1.00001) return false;
        const int nAB = across(t, a, b), nCA = across(t, c, a), nBD = across(u, b, d), nDC = across(u, d, c);
        tris[t] = Tri{{a, b, d}, {nBD, u, nAB}};
        tris[u] = Tri{{a, d, c}, {nDC, nCA, t}};
        repoint(nBD, u, t);
        repoint(nCA, t, u);
        return true;
    }
    /// One sweep of flips over every edge, repeated until nothing changes.
    void flipSweeps() {
        for (int sweep = 0; sweep < 1000; ++sweep) {
            bool changed = false;
            for (std::size_t t = 0; t < tris.size(); ++t)
                for (int i = 0; i < 3; ++i)
                    if (tryFlip(static_cast<int>(t), i)) changed = true;
            if (!changed) break;
        }
    }
    /// Inserts a point (on an edge when it lies on one) and restores the shape criterion around it. -1 when outside.
    int insert(const P2& p, bool onContour) {
        for (std::size_t t = 0; t < tris.size(); ++t) {
            const Tri& tr = tris[t];
            double o[3];
            int zero = -1;
            bool inside = true;
            for (int j = 0; j < 3; ++j) {
                const P2 &a = pts[tr.v[j]], &b = pts[tr.v[(j + 1) % 3]];
                const double len = std::max(dist(a, b), 1e-300);
                o[j] = Delaunay::orient(a, b, p) / len;
                if (std::fabs(o[j]) < 1e-9) zero = j;
                else if (o[j] < 0) inside = false;
            }
            if (!inside) continue;
            for (int k : tr.v)
                if (dist(pts[k], p) < 1e-9) return -1;
            const int pi = static_cast<int>(pts.size());
            pts.push_back(p);
            contour.push_back(onContour ? 1 : 0);
            std::vector<int> around;
            if (zero >= 0) splitEdge(static_cast<int>(t), zero, pi, around);
            else splitTriangle(static_cast<int>(t), pi, around);
            for (std::size_t k = 0; k < around.size(); ++k) {
                const int x = around[k];
                int i = 0;
                while (tris[x].v[i] != pi) ++i;
                const int u = tris[x].nb[i];
                if (tryFlip(x, i)) {
                    around.push_back(x);
                    around.push_back(u);
                }
                if (around.size() > 100000) break;
            }
            return pi;
        }
        return -1;
    }

private:
    int across(int t, int p, int q) const {
        for (int i = 0; i < 3; ++i)
            if (tris[t].v[i] != p && tris[t].v[i] != q) return tris[t].nb[i];
        return -1;
    }
    void repoint(int x, int from, int to) {
        if (x < 0) return;
        for (int& n : tris[x].nb)
            if (n == from) n = to;
    }
    void splitTriangle(int t, int p, std::vector<int>& around) {
        const Tri old = tris[t];
        const int a = old.v[0], b = old.v[1], c = old.v[2];
        const int t1 = static_cast<int>(tris.size()), t2 = t1 + 1;
        tris[t] = Tri{{a, b, p}, {t1, t2, old.nb[2]}};
        tris.push_back(Tri{{b, c, p}, {t2, t, old.nb[0]}});
        tris.push_back(Tri{{c, a, p}, {t, t1, old.nb[1]}});
        repoint(old.nb[0], t, t1);
        repoint(old.nb[1], t, t2);
        around = {t, t1, t2};
    }
    void splitEdge(int t, int j, int p, std::vector<int>& around) {
        const int i = (j + 2) % 3;  // edge j is opposite v[i]
        const int a = tris[t].v[i], b = tris[t].v[(i + 1) % 3], c = tris[t].v[(i + 2) % 3];
        const int u = tris[t].nb[i];
        const int nAB = across(t, a, b), nCA = across(t, c, a);
        const int tN = static_cast<int>(tris.size());
        if (u < 0) {
            tris[t] = Tri{{a, b, p}, {-1, tN, nAB}};
            tris.push_back(Tri{{a, p, c}, {-1, nCA, t}});
            repoint(nCA, t, tN);
            around = {t, tN};
            return;
        }
        int k = 0;
        while (tris[u].v[k] == b || tris[u].v[k] == c) ++k;
        const int d = tris[u].v[k];
        const int nDC = across(u, d, c), nBD = across(u, b, d);
        const int uN = tN + 1;
        tris[t] = Tri{{a, b, p}, {uN, tN, nAB}};
        tris.push_back(Tri{{a, p, c}, {u, nCA, t}});
        tris[u] = Tri{{d, c, p}, {tN, uN, nDC}};
        tris.push_back(Tri{{d, p, b}, {t, nBD, u}});
        repoint(nCA, t, tN);
        repoint(nBD, u, uN);
        around = {t, tN, u, uN};
    }
};

/// Splits of a contour edge p1p2 of a triangle with opposite vertex p3 [V, geom2d FUN_1800599d0, second improvement pass FUN_18005b850 /
/// FUN_18005aff0]. `n2`, `n3`: the neighbours across (p2, p3) and (p3, p1) (-1: contour). Returns the point to insert, if any.
std::optional<P2> contourEdgeSplit(const Mesh2& m, int p1, int p2, int p3, int n2, int n3, double s) {
    if (!m.contour[p3]) return std::nullopt;  // FUN_18005f900: the opposite vertex must be a boundary point
    const P2 &a = m.pts[p1], &b = m.pts[p2], &c = m.pts[p3];
    const double L = dist(a, b);
    if (L <= 0.0) return std::nullopt;
    const auto project = [&](const P2& q, double& t, double& d) {
        t = ((q[0] - a[0]) * (b[0] - a[0]) + (q[1] - a[1]) * (b[1] - a[1])) / L;
        d = std::fabs((b[0] - a[0]) * (q[1] - a[1]) - (b[1] - a[1]) * (q[0] - a[0])) / L;
    };
    double t = 0, d = 0;
    project(c, t, d);
    const auto at = [&](double u) { return P2{a[0] + u * (b[0] - a[0]), a[1] + u * (b[1] - a[1])}; };
    constexpr double k = 2.0;                  // FUN_18005e5a0
    const double h = std::max(0.35 * s, d * k);  // FUN_1800703d0(s) = 0.35 s
    const bool atStart = std::fabs(t) < 0.001 * d, atEnd = !atStart && std::fabs(L - t) < 0.001 * d;
    if (atStart || atEnd) {
        const double l = std::min(s, h);
        if (l * 1.01 < L) {
            const double u = l * 2.0 <= L ? (atStart ? l / L : 1.0 - l / L) : 0.5;
            return at(u);
        }
    }
    if (L < h || t < 0.75 * h || L - t < 0.75 * h) {
        const bool central = t > 0.3 * L && t < 0.7 * L;
        if (central && L > 1.01 * s) return at(t / L);
        if (central && n2 >= 0 && n3 >= 0) {
            for (int n : {n2, n3}) {
                int q = -1;
                for (int v : m.tris[n].v)
                    if (v != p1 && v != p2 && v != p3) q = v;
                if (q < 0 || !m.contour[q]) return std::nullopt;
                double tq = 0, dq = 0;
                project(m.pts[q], tq, dq);
                if (!(dq <= d * 1.01 && tq > 0.15 * L && tq < 0.85 * L)) return std::nullopt;
            }
            return at(t / L);
        }
        return std::nullopt;
    }
    return at(t / L);
}

/// Corner triangle with two contour edges p1p2, p2p3 [V, geom2d FUN_18005a430]: the longer one is halved when it exceeds max(0.35 s, 2 x the
/// other).
std::optional<P2> contourCornerSplit(const Mesh2& m, int p1, int p2, int p3, double s) {
    const P2 &a = m.pts[p1], &b = m.pts[p2], &c = m.pts[p3];
    const double m35 = 0.35 * s, d4 = dist(a, b), d5 = dist(b, c);
    if (d4 > std::max(m35, d5 * 2.0)) return P2{0.5 * (a[0] + b[0]), 0.5 * (a[1] + b[1])};
    if (d5 > std::max(m35, d4 * 2.0)) return P2{0.5 * (b[0] + c[0]), 0.5 * (b[1] + c[1])};
    return std::nullopt;
}

/// Second improvement pass [V, FUN_18005b850 / FUN_18005aff0]: flips of the interior edges and splits of the contour edges, restarted after
/// every split until nothing changes. Returns the inserted contour points. [N] traversal order: triangle index order (the original walks the
/// neighbours recursively from a root triangle).
std::vector<P2> contourPass(Mesh2& m, double s) {
    std::vector<P2> added;
    for (int guard = 0; guard < 10000; ++guard) {
        bool changed = false, split = false;
        for (std::size_t t = 0; t < m.tris.size() && !split; ++t)
            for (int j = 0; j < 3 && !split; ++j) {
                const Mesh2::Tri tr = m.tris[t];
                const int p1 = tr.v[j], p2 = tr.v[(j + 1) % 3], p3 = tr.v[(j + 2) % 3];
                if (Mesh2::edgeNb(tr, j) >= 0) {
                    if (m.tryFlip(static_cast<int>(t), (j + 2) % 3)) changed = true;
                    continue;
                }
                std::optional<P2> p = contourEdgeSplit(m, p1, p2, p3, Mesh2::edgeNb(tr, (j + 1) % 3), Mesh2::edgeNb(tr, (j + 2) % 3), s);
                if (!p && Mesh2::edgeNb(tr, (j + 1) % 3) < 0) p = contourCornerSplit(m, p1, p2, p3, s);
                if (p && m.insert(*p, true) >= 0) {
                    added.push_back(*p);
                    split = true;
                }
            }
        if (!changed && !split) break;
    }
    return added;
}

struct Plane {
    V normal{};
    double offset = 0.0;
    double size = 0.0;
};

/// Is the 2D point inside the polygon (or on its contour within tol)?
bool insidePolygon(const std::vector<P2>& poly, const P2& p, double tol, bool* onContour = nullptr) {
    bool inside = false;
    if (onContour) *onContour = false;
    for (std::size_t i = 0, j = poly.size() - 1; i < poly.size(); j = i++) {
        const P2 &a = poly[j], &b = poly[i];
        const double ex = b[0] - a[0], ey = b[1] - a[1], len2 = ex * ex + ey * ey;
        const double t = len2 > 0 ? std::clamp(((p[0] - a[0]) * ex + (p[1] - a[1]) * ey) / len2, 0.0, 1.0) : 0.0;
        if (std::hypot(a[0] + t * ex - p[0], a[1] + t * ey - p[1]) < tol) {
            if (onContour) *onContour = true;
            return true;
        }
        if ((a[1] > p[1]) != (b[1] > p[1]) && p[0] < a[0] + (p[1] - a[1]) * ex / ey) inside = !inside;
    }
    return inside;
}

}  // namespace

ShellMesh meshShells(const FrameModel& model) {
    ShellMesh mesh;
    const int nNodes = static_cast<int>(model.nodes.size());
    std::unordered_map<int, int> index;
    for (int i = 0; i < nNodes; ++i) {
        index.emplace(model.nodes[i].id, i);
        mesh.positions.push_back(model.nodes[i].position);
    }
    mesh.modelNodes = nNodes;
    mesh.edgeFixed.assign(nNodes, {});
    if (model.shells.empty()) return mesh;
    const auto newNode = [&](const V& p) {
        mesh.positions.push_back(p);
        mesh.edgeFixed.push_back({});
        return static_cast<int>(mesh.positions.size()) - 1;
    };

    // --- contours and planes ---
    const int nShells = static_cast<int>(model.shells.size());
    std::vector<std::vector<int>> corners(nShells);
    std::vector<int> planeOf(nShells, -1);
    std::vector<Plane> planes;
    for (int s = 0; s < nShells; ++s) {
        const FemShell& sh = model.shells[s];
        std::vector<int> c;
        for (int id : sh.nodes) {
            const auto it = index.find(id);
            if (it == index.end()) { c.clear(); break; }
            c.push_back(it->second);
        }
        if (c.size() < 3) {
            mesh.issues.push_back("Lámina " + std::to_string(sh.id) + ": contorno no válido.");
            continue;
        }
        corners[s] = c;
        const double side = sh.maxSide > 0.0 ? sh.maxSide : 1.0;
        const double offset = dot(sh.ez, mesh.positions[c[0]]);
        for (std::size_t p = 0; p < planes.size() && planeOf[s] < 0; ++p) {
            const double d = dot(planes[p].normal, sh.ez);
            if (std::fabs(std::fabs(d) - 1.0) < 1e-9 && std::fabs((d > 0 ? offset : -offset) - planes[p].offset) < kTol) planeOf[s] = static_cast<int>(p);
        }
        if (planeOf[s] < 0) {
            planeOf[s] = static_cast<int>(planes.size());
            planes.push_back(Plane{sh.ez, offset, 0.0});
        }
        planes[planeOf[s]].size = std::max(planes[planeOf[s]].size, side);  // the plane takes the maximum of its shells [V]
    }

    // --- contour points of every edge [V]: parameters from key.first to key.second, shared by every shell and bar on the edge ---
    std::map<EdgeKey, double> edgeSize;  // the minimum size of the planes that meet at the edge [V]
    for (int s = 0; s < nShells; ++s) {
        const std::vector<int>& c = corners[s];
        for (std::size_t i = 0; i < c.size(); ++i) {
            const EdgeKey k = key(c[i], c[(i + 1) % c.size()]);
            const double size = planes[planeOf[s]].size;
            const auto it = edgeSize.find(k);
            edgeSize[k] = it == edgeSize.end() ? size : std::min(it->second, size);
        }
    }
    std::map<EdgeKey, std::vector<double>> edgeParams;
    for (const auto& [k, size] : edgeSize) {
        const int n = intermediatePoints(norm(sub(mesh.positions[k.second], mesh.positions[k.first])), size) + 1;  // segments
        std::vector<double>& ts = edgeParams[k];
        for (int j = 1; j < n; ++j) ts.push_back(static_cast<double>(j) / n);
    }

    // Local 2D system of every shell.
    std::vector<std::vector<P2>> polys(nShells);
    std::vector<double> extents(nShells, 0.0);
    const auto toLocal = [&](int s, const V& p) {
        const FemShell& sh = model.shells[s];
        const V d = sub(p, mesh.positions[corners[s][0]]);
        return P2{dot(d, sh.ex), dot(d, sh.ey)};
    };
    for (int s = 0; s < nShells; ++s) {
        if (corners[s].empty()) continue;
        for (int k : corners[s]) polys[s].push_back(toLocal(s, mesh.positions[k]));
        for (const P2& p : polys[s]) extents[s] = std::max({extents[s], std::fabs(p[0]), std::fabs(p[1])});
    }
    const auto edgePoint = [&](const EdgeKey& k, double t) { return lerp(mesh.positions[k.first], mesh.positions[k.second], t); };
    // Contour points of a shell in contour order (corners and intermediate points).
    const auto contourPoints = [&](int s) {
        std::vector<P2> out;
        const std::vector<int>& c = corners[s];
        for (std::size_t i = 0; i < c.size(); ++i) {
            const int p = c[i], q = c[(i + 1) % c.size()];
            out.push_back(toLocal(s, mesh.positions[p]));
            const EdgeKey k = key(p, q);
            std::vector<double> ts = edgeParams.at(k);
            if (p > q) std::reverse(ts.begin(), ts.end());
            for (double t : ts) out.push_back(toLocal(s, edgePoint(k, t)));
        }
        return out;
    };
    // Triangulation of the contour points only (Delaunay, then the triangles inside the contour).
    const auto initial = [&](int s, Mesh2& m) {
        const std::vector<P2> cp = contourPoints(s);
        Delaunay dt(extents[s]);
        for (const P2& p : cp)
            if (!dt.insert(p)) return false;
        m = Mesh2{};
        m.pts = cp;
        m.contour.assign(cp.size(), 1);
        std::vector<int> renumber(dt.tris.size(), -1);
        for (std::size_t t = 0; t < dt.tris.size(); ++t) {
            const auto& v = dt.tris[t].v;
            if (!dt.tris[t].alive || v[0] < 3 || v[1] < 3 || v[2] < 3) continue;
            const P2 g{(dt.pts[v[0]][0] + dt.pts[v[1]][0] + dt.pts[v[2]][0]) / 3.0, (dt.pts[v[0]][1] + dt.pts[v[1]][1] + dt.pts[v[2]][1]) / 3.0};
            if (!insidePolygon(polys[s], g, 0.0)) continue;
            renumber[t] = static_cast<int>(m.tris.size());
            m.tris.push_back(Mesh2::Tri{{v[0] - 3, v[1] - 3, v[2] - 3}, {-1, -1, -1}});
        }
        for (std::size_t t = 0; t < dt.tris.size(); ++t)
            if (renumber[t] >= 0)
                for (int i = 0; i < 3; ++i) {
                    const int nb = dt.tris[t].nb[i];
                    m.tris[renumber[t]].nb[i] = nb >= 0 ? renumber[nb] : -1;
                }
        // Every contour segment must be an edge without a neighbour (a reflex corner can be lost: reported by the caller).
        std::size_t free = 0;
        for (const Mesh2::Tri& t : m.tris)
            for (int n : t.nb) free += n < 0 ? 1 : 0;
        return !m.tris.empty() && free == cp.size();
    };

    // --- improvement of the contour [V, triang3d_mejora_aspecto_mallado = FUN_18005b960 + FUN_18005b850 + FUN_18004b500] ---
    // Pass 1: flip sweeps (the contour floor rule of FUN_1800598b0 adds nothing to the points already placed); pass 2: flips and splits of
    // the contour edges (FUN_1800599d0 / FUN_18005a430). A point added on an edge shared with other shells is added to them too and those
    // shells are triangulated again, until no shell adds points ([N] the original compatibilises the planes in modelo3d with its own loop).
    std::vector<Mesh2> m2(nShells);
    std::vector<char> dirty(nShells, 1), failed(nShells, 0);
    for (int round = 0; round < 50; ++round) {
        bool any = false;
        for (int s = 0; s < nShells; ++s) {
            if (!dirty[s] || corners[s].empty()) continue;
            dirty[s] = 0;
            failed[s] = 0;
            Mesh2 m;
            if (!initial(s, m)) {
                failed[s] = 1;
                continue;
            }
            m.flipSweeps();
            for (const P2& p : contourPass(m, planes[planeOf[s]].size)) {
                // the contour edge of the shell that holds the point
                const std::vector<int>& c = corners[s];
                std::size_t best = 0;
                double bestD = 1e300;
                for (std::size_t i = 0; i < c.size(); ++i) {
                    const P2 &a = polys[s][i], &b = polys[s][(i + 1) % c.size()];
                    const double ex = b[0] - a[0], ey = b[1] - a[1], l2 = ex * ex + ey * ey;
                    const double t = std::clamp(((p[0] - a[0]) * ex + (p[1] - a[1]) * ey) / l2, 0.0, 1.0);
                    const double d = std::hypot(a[0] + t * ex - p[0], a[1] + t * ey - p[1]);
                    if (d < bestD) { bestD = d; best = i; }
                }
                const int pa = c[best], pb = c[(best + 1) % c.size()];
                const P2 &a = polys[s][best], &b = polys[s][(best + 1) % c.size()];
                double t = ((p[0] - a[0]) * (b[0] - a[0]) + (p[1] - a[1]) * (b[1] - a[1])) / ((b[0] - a[0]) * (b[0] - a[0]) + (b[1] - a[1]) * (b[1] - a[1]));
                if (pa > pb) t = 1.0 - t;
                const EdgeKey k = key(pa, pb);
                std::vector<double>& ts = edgeParams[k];
                if (std::any_of(ts.begin(), ts.end(), [&](double x) { return std::fabs(x - t) < 1e-9; })) continue;
                ts.insert(std::upper_bound(ts.begin(), ts.end(), t), t);
                for (int o = 0; o < nShells; ++o) {
                    if (o == s) continue;
                    const std::vector<int>& co = corners[o];
                    for (std::size_t i = 0; i < co.size(); ++i)
                        if (key(co[i], co[(i + 1) % co.size()]) == k) dirty[o] = 1;
                }
                any = true;
            }
            m2[s] = std::move(m);
        }
        if (!any) break;
    }

    // --- contour chains: corners, intermediate points and mid-side nodes ---
    std::map<EdgeKey, std::vector<int>> edgeNodes;  // from key.first to key.second, ends included, mid-side nodes interleaved
    std::map<EdgeKey, int> midNode;                 // vertex pair -> mid-side node
    for (const auto& [k, ts] : edgeParams) {
        std::vector<int> vertices{k.first};
        for (double t : ts) vertices.push_back(newNode(edgePoint(k, t)));
        vertices.push_back(k.second);
        std::vector<int> chain{vertices.front()};
        for (std::size_t j = 0; j + 1 < vertices.size(); ++j) {
            const int mid = newNode(lerp(mesh.positions[vertices[j]], mesh.positions[vertices[j + 1]], 0.5));
            midNode[key(vertices[j], vertices[j + 1])] = mid;
            chain.push_back(mid);
            chain.push_back(vertices[j + 1]);
        }
        edgeNodes[k] = std::move(chain);
    }
    const auto chain = [&](int p, int q) {
        std::vector<int> c = edgeNodes.at(key(p, q));
        if (p > q) std::reverse(c.begin(), c.end());
        return c;
    };

    // --- refinement of the inside of every shell ---
    for (int s = 0; s < nShells; ++s) {
        const std::vector<int>& c = corners[s];
        if (c.empty()) continue;
        const FemShell& sh = model.shells[s];
        if (failed[s]) {
            mesh.issues.push_back("Lámina " + std::to_string(sh.id) + ": no se pudo mallar el contorno (contorno no convexo, pendiente).");
            continue;
        }
        Mesh2& m = m2[s];
        const double size = planes[planeOf[s]].size;
        const double extent = extents[s];
        // FUN_18004b500: flip sweep (FUN_180062fd0), then passes of bisection of the interior edges marked by FUN_18005c3e0 until no edge is
        // marked [V rule]. [N] not reproduced: the relocation of the free interior points to their optimal position (FUN_180068b70 ->
        // troptimo_coordenadas_optimas), the validity filter FUN_18009e0e0, the rounding to the discrete grid of the plane and the stop
        // when the ratio new points / vertices stops improving.
        m.flipSweeps();
        for (int pass = 0; pass < 200; ++pass) {
            std::vector<P2> fresh;
            for (std::size_t t = 0; t < m.tris.size(); ++t) {
                const Mesh2::Tri& tr = m.tris[t];
                for (int i = 0; i < 3; ++i) {
                    const int nb = tr.nb[i];
                    if (nb < static_cast<int>(t)) continue;  // every interior edge once (from the triangle with the smaller index)
                    const int a = tr.v[(i + 1) % 3], b = tr.v[(i + 2) % 3];
                    int opposite = -1;
                    for (int k : m.tris[nb].v)
                        if (k != a && k != b) opposite = k;
                    const P2 mid{0.5 * (m.pts[a][0] + m.pts[b][0]), 0.5 * (m.pts[a][1] + m.pts[b][1])};
                    bool onContour = false;
                    insidePolygon(polys[s], mid, 1e-9 * std::max(extent, 1.0), &onContour);
                    if (onContour) continue;  // a midpoint on a contour line is never inserted [V]
                    if (shouldSplit(m.pts[a], m.pts[b], m.pts[tr.v[i]], m.pts[opposite], size)) fresh.push_back(mid);
                }
            }
            std::sort(fresh.begin(), fresh.end());
            fresh.erase(std::unique(fresh.begin(), fresh.end(), [](const P2& p, const P2& q) { return dist(p, q) < 1e-9; }), fresh.end());
            int added = 0;
            for (const P2& p : fresh)
                if (m.insert(p, false) >= 0) ++added;
            if (added == 0) break;
            if (m.pts.size() > 400000) {
                mesh.issues.push_back("Lámina " + std::to_string(sh.id) + ": malla demasiado fina.");
                break;
            }
        }

        // Mesh nodes: contour points are the chain vertices (matched by position), the others are new nodes.
        std::vector<std::pair<P2, int>> contourNodes;
        for (std::size_t i = 0; i < c.size(); ++i) {
            const std::vector<int> e = chain(c[i], c[(i + 1) % c.size()]);
            for (std::size_t j = 0; j + 1 < e.size(); j += 2) contourNodes.emplace_back(toLocal(s, mesh.positions[e[j]]), e[j]);
        }
        const V o = mesh.positions[c[0]];
        std::vector<int> node(m.pts.size(), -1);
        for (std::size_t i = 0; i < m.pts.size(); ++i) {
            if (m.contour[i])
                for (const auto& [p, n] : contourNodes)
                    if (dist(p, m.pts[i]) < 1e-6 * std::max(extent, 1.0)) { node[i] = n; break; }
            if (node[i] < 0) node[i] = newNode(add(o, add(scale(sh.ex, m.pts[i][0]), scale(sh.ey, m.pts[i][1]))));
        }
        for (const Mesh2::Tri& t : m.tris) {
            const std::array<int, 3> g{node[t.v[0]], node[t.v[1]], node[t.v[2]]};
            ShellMesh::Triangle tri;
            tri.shell = s;
            int mids[3];
            for (int i = 0; i < 3; ++i) {
                const int a = g[i], b = g[(i + 1) % 3];
                auto it = midNode.find(key(a, b));
                if (it == midNode.end()) it = midNode.emplace(key(a, b), newNode(lerp(mesh.positions[a], mesh.positions[b], 0.5))).first;
                mids[i] = it->second;
            }
            tri.nodes = {g[0], g[1], g[2], mids[0], mids[1], mids[2]};
            mesh.triangles.push_back(tri);
        }
    }

    // --- exterior bonds of the edges ---
    // Inner nodes of an edge take its restraint. A corner takes it only when every bonded edge that meets there has the same restraint
    // (#64886: edges with different exterior bonds leave the node without restraint [V text; free edges are ignored here, N]).
    std::map<int, std::vector<std::array<bool, 6>>> atCorner;
    for (int s = 0; s < nShells; ++s) {
        const std::vector<int>& c = corners[s];
        const FemShell& sh = model.shells[s];
        for (std::size_t i = 0; i < c.size() && i < sh.edgeFixed.size(); ++i) {
            const std::array<bool, 6>& mask = sh.edgeFixed[i];
            if (std::none_of(mask.begin(), mask.end(), [](bool b) { return b; })) continue;
            const std::vector<int> nodes = chain(c[i], c[(i + 1) % c.size()]);
            for (std::size_t k = 1; k + 1 < nodes.size(); ++k)
                for (int d = 0; d < 6; ++d) mesh.edgeFixed[nodes[k]][d] = mesh.edgeFixed[nodes[k]][d] || mask[d];
            atCorner[nodes.front()].push_back(mask);
            atCorner[nodes.back()].push_back(mask);
        }
    }
    for (const auto& [node, masks] : atCorner)
        if (std::all_of(masks.begin(), masks.end(), [&](const std::array<bool, 6>& m) { return m == masks.front(); })) mesh.edgeFixed[node] = masks.front();

    // --- bars along shell edges ---
    for (std::size_t b = 0; b < model.bars.size(); ++b) {
        const auto ni = index.find(model.bars[b].nodeI), nj = index.find(model.bars[b].nodeJ);
        if (ni == index.end() || nj == index.end()) continue;
        if (!edgeNodes.count(key(ni->second, nj->second))) continue;
        std::vector<std::pair<int, double>> split;
        for (int node : chain(ni->second, nj->second)) split.emplace_back(node, norm(sub(mesh.positions[node], mesh.positions[ni->second])));
        mesh.barSplits[static_cast<int>(b)] = std::move(split);
    }

    // --- model nodes inside a shell or on one of its edges that are not its corners: not connected to the mesh ---
    for (int s = 0; s < nShells; ++s) {
        const std::vector<int>& c = corners[s];
        if (c.empty()) continue;
        const FemShell& sh = model.shells[s];
        const V& o = mesh.positions[c[0]];
        std::vector<P2> poly;
        for (int k : c) poly.push_back({dot(sub(mesh.positions[k], o), sh.ex), dot(sub(mesh.positions[k], o), sh.ey)});
        for (int k = 0; k < nNodes; ++k) {
            if (std::find(c.begin(), c.end(), k) != c.end()) continue;
            const V d = sub(mesh.positions[k], o);
            if (std::fabs(dot(d, sh.ez)) > kTol) continue;
            if (insidePolygon(poly, P2{dot(d, sh.ex), dot(d, sh.ey)}, kTol))
                mesh.issues.push_back("Nudo " + std::to_string(model.nodes[k].id) + ": situado en la lámina " + std::to_string(sh.id) +
                                      " sin ser vértice de su contorno; no se conecta a la malla (pendiente).");
        }
    }
    return mesh;
}

}  // namespace cc::fem
