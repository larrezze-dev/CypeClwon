#include <algorithm>
#include <cmath>
#include <unordered_map>

#include <Eigen/Dense>

#include "core/fem/linear_solver.h"
#include "core/fem/shell_analysis.h"
#include "core/fem/shell_element.h"

namespace cc::fem {

FaceStress topFaceStress(const ShellForces& f, double h) {
    return {f[0] / h + 6.0 * f[3] / (h * h), f[1] / h + 6.0 * f[4] / (h * h), f[2] / h + 6.0 * f[5] / (h * h), f[6] / h, f[7] / h};
}
FaceStress bottomFaceStress(const ShellForces& f, double h) {
    return {f[0] / h - 6.0 * f[3] / (h * h), f[1] / h - 6.0 * f[4] / (h * h), f[2] / h - 6.0 * f[5] / (h * h), f[6] / h, f[7] / h};
}
double vonMises(const FaceStress& s) {
    return std::sqrt(s.sx * s.sx - s.sx * s.sy + s.sy * s.sy + 3.0 * (s.txy * s.txy + s.txz * s.txz + s.tyz * s.tyz));
}
std::array<double, 3> principalStresses(const FaceStress& s) {
    const double c = 0.5 * (s.sx + s.sy), r = std::hypot(0.5 * (s.sx - s.sy), s.txy);
    return {c + r, c - r, 0.5 * std::atan2(2.0 * s.txy, s.sx - s.sy)};
}
ShellForces rotateShellForces(const ShellForces& f, double angle) {
    const double c = std::cos(angle), s = std::sin(angle);
    const auto tensor = [&](double xx, double yy, double xy) {
        return std::array<double, 3>{xx * c * c + yy * s * s + 2 * xy * c * s, xx * s * s + yy * c * c - 2 * xy * c * s, (yy - xx) * c * s + xy * (c * c - s * s)};
    };
    const auto n = tensor(f[0], f[1], f[2]), m = tensor(f[3], f[4], f[5]);
    return {n[0], n[1], n[2], m[0], m[1], m[2], f[6] * c + f[7] * s, -f[6] * s + f[7] * c};
}

namespace {

using P2 = std::array<double, 2>;

/// Everything about one shell of a solved case that the band integration needs.
struct ShellView {
    const FemShell* shell = nullptr;
    int index = -1;
    Vec3S origin{};
    std::vector<P2> contour;
    std::vector<int> triangles;                      // indices into mesh.triangles
    std::unordered_map<int, P2> xy;                  // mesh node -> local 2D
    std::unordered_map<int, ShellForces> smoothed;   // mesh node -> averaged forces
    const LoadCaseResult* lc = nullptr;
    const ShellMesh* mesh = nullptr;
};

P2 toLocal(const ShellView& v, const Vec3S& p) {
    const double d[3] = {p[0] - v.origin[0], p[1] - v.origin[1], p[2] - v.origin[2]};
    const FemShell& s = *v.shell;
    return {d[0] * s.ex[0] + d[1] * s.ex[1] + d[2] * s.ex[2], d[0] * s.ey[0] + d[1] * s.ey[1] + d[2] * s.ey[2]};
}

std::optional<ShellView> viewOf(const FrameModel& model, const LinearResult& result, std::size_t caseIndex, int shellId) {
    if (caseIndex >= result.cases.size()) return std::nullopt;
    const LoadCaseResult& lc = result.cases[caseIndex];
    if (lc.triangleForces.size() != result.mesh.triangles.size() || lc.triangleForces.empty()) return std::nullopt;
    ShellView v;
    for (std::size_t s = 0; s < model.shells.size(); ++s)
        if (model.shells[s].id == shellId) { v.shell = &model.shells[s]; v.index = static_cast<int>(s); }
    if (!v.shell) return std::nullopt;
    std::unordered_map<int, int> nodeIndex;
    for (std::size_t i = 0; i < model.nodes.size(); ++i) nodeIndex.emplace(model.nodes[i].id, static_cast<int>(i));
    for (int id : v.shell->nodes)
        if (!nodeIndex.count(id)) return std::nullopt;
    v.origin = result.mesh.positions[nodeIndex.at(v.shell->nodes.front())];
    for (int id : v.shell->nodes) v.contour.push_back(toLocal(v, result.mesh.positions[nodeIndex.at(id)]));
    v.lc = &lc;
    v.mesh = &result.mesh;
    // Smoothing [V, cyopensees.dll FUN_180042560, section 16.2]: at every CORNER node, the plain arithmetic mean (no weights) of the
    // unsmoothed values of the triangles of the same shell that share it; a node used by one triangle keeps its value. Mid-side nodes are not
    // smoothed. The unsmoothed value of a triangle at a corner is the value at the Gauss point nearest to it ((1/6, 1/6) -> vertex 0,
    // (2/3, 1/6) -> vertex 1, (1/6, 2/3) -> vertex 2) [N: the original takes the three records from the engine, `stressesNodes`, whose
    // evaluation is encrypted (section 16.1); of the candidates tried (field at the node, centroid, nearest Gauss point) this is the one that
    // fits every measured value of the plate and the band at once, section 18.2].
    std::unordered_map<int, int> uses;
    for (std::size_t e = 0; e < result.mesh.triangles.size(); ++e) {
        if (result.mesh.triangles[e].shell != v.index) continue;
        v.triangles.push_back(static_cast<int>(e));
        for (int k = 0; k < 6; ++k) {
            const int n = result.mesh.triangles[e].nodes[k];
            if (!v.xy.count(n)) v.xy[n] = toLocal(v, result.mesh.positions[n]);
        }
        for (int k = 0; k < 3; ++k) {
            const int n = result.mesh.triangles[e].nodes[k];
            ShellForces& acc = v.smoothed[n];  // value-initialised to zero on first use
            for (int q = 0; q < 8; ++q) acc[q] += lc.triangleForces[e].atGauss[k][q];
            ++uses[n];
        }
    }
    for (auto& [n, f] : v.smoothed)
        if (uses.at(n) > 1)
            for (double& x : f) x /= uses.at(n);
    return v;
}

/// Triangle of the shell that contains the point, with its natural coordinates.
bool locate(const ShellView& v, const P2& p, int& tri, double& r, double& s) {
    constexpr double tol = 1e-9;
    for (int e : v.triangles) {
        const auto& n = v.mesh->triangles[e].nodes;
        const P2 &a = v.xy.at(n[0]), &b = v.xy.at(n[1]), &c = v.xy.at(n[2]);
        const double j00 = b[0] - a[0], j01 = c[0] - a[0], j10 = b[1] - a[1], j11 = c[1] - a[1];
        const double det = j00 * j11 - j01 * j10;
        if (std::fabs(det) < 1e-300) continue;
        const double dx = p[0] - a[0], dy = p[1] - a[1];
        r = (j11 * dx - j01 * dy) / det;
        s = (-j10 * dx + j00 * dy) / det;
        if (r >= -tol && s >= -tol && r + s <= 1.0 + tol) { tri = e; return true; }
    }
    return false;
}

std::optional<ShellForces> fieldAt(const ShellView& v, const P2& p, bool smoothed) {
    int e = -1;
    double r = 0, s = 0;
    if (!locate(v, p, e, r, s)) return std::nullopt;
    // [N, section 16.3] The original keeps values only at the corner nodes (smoothed: the nodal mean; unsmoothed: the value of the triangle at
    // its three vertices) and does not interpolate in cyopensees; the clone interpolates linearly between the three corners of the triangle that
    // contains the point, so that a point on a mesh node returns exactly the value of that node. On an edge shared by two triangles the
    // unsmoothed value is that of the first triangle found (the choice of the original, calcmef, was not read).
    const double n[3] = {1.0 - r - s, r, s};
    ShellForces f{};
    for (int k = 0; k < 3; ++k) {
        const ShellForces& nodal = smoothed ? v.smoothed.at(v.mesh->triangles[e].nodes[k]) : v.lc->triangleForces[e].atGauss[k];
        for (int q = 0; q < 8; ++q) f[q] += n[k] * nodal[q];
    }
    return f;
}

bool insidePolygon(const std::vector<P2>& poly, const P2& p) {
    bool inside = false;
    for (std::size_t i = 0, j = poly.size() - 1; i < poly.size(); j = i++) {
        const P2 &a = poly[j], &b = poly[i];
        const double ex = b[0] - a[0], ey = b[1] - a[1], len2 = ex * ex + ey * ey;
        const double t = len2 > 0 ? std::clamp(((p[0] - a[0]) * ex + (p[1] - a[1]) * ey) / len2, 0.0, 1.0) : 0.0;
        if (std::hypot(a[0] + t * ex - p[0], a[1] + t * ey - p[1]) < 1e-9) return true;
        if ((a[1] > p[1]) != (b[1] > p[1]) && p[0] < a[0] + (p[1] - a[1]) * ex / ey) inside = !inside;
    }
    return inside;
}

/// Distance from p (inside) to the contour along the unit direction d, capped at `cap`.
double reach(const std::vector<P2>& poly, const P2& p, const P2& d, double cap) {
    if (!insidePolygon(poly, p)) return 0.0;
    double best = cap;
    for (std::size_t i = 0, j = poly.size() - 1; i < poly.size(); j = i++) {
        const P2 &a = poly[j], &b = poly[i];
        const double ex = b[0] - a[0], ey = b[1] - a[1];
        const double den = d[0] * ey - d[1] * ex;
        if (std::fabs(den) < 1e-14) continue;
        const double wx = a[0] - p[0], wy = a[1] - p[1];
        const double lambda = (wx * ey - wy * ex) / den, mu = (wx * d[1] - wy * d[0]) / den;
        if (lambda >= 0.0 && mu >= -1e-12 && mu <= 1.0 + 1e-12) best = std::min(best, lambda);
    }
    return std::max(best, 0.0);
}

BandPoint integrateAt(const ShellView& v, const BandSpec& band, double s, BandMethod method, bool& ok) {
    ok = false;
    BandPoint out;
    const double lx = band.x2 - band.x1, ly = band.y2 - band.y1, length = std::hypot(lx, ly);
    if (length <= 0.0 || band.width <= 0.0) return out;
    const P2 t{lx / length, ly / length}, n{-t[1], t[0]};
    const double half = 0.5 * band.width;
    s = std::clamp(s, 0.0, length);
    const auto widths = [&](double at, double& left, double& right) {
        const P2 p{band.x1 + at * t[0], band.y1 + at * t[1]};
        right = reach(v.contour, p, n, half);
        left = reach(v.contour, p, P2{-n[0], -n[1]}, half);
    };
    double left = 0.0, right = 0.0;
    widths(s, left, right);
    if (left < 1e-3 && right < 1e-3) {
        // A point on the contour is moved inside by min(1 % of the length, 0.01 m) [V, cefbandint_anchos_banda_integracion_en_punto].
        const double shift = std::min(0.01 * length, 0.01);
        s = s < 0.5 * length ? s + shift : s - shift;
        widths(s, left, right);
        if (left < 1e-6 && right < 1e-6) return out;
    }
    out.s = s;
    out.leftWidth = left;
    out.rightWidth = right;
    const P2 p{band.x1 + s * t[0], band.y1 + s * t[1]};
    const double angle = std::atan2(t[1], t[0]);
    std::array<double, 6> canonical{};  // N, Vy, Vz, Mt, My, Mz (resultant of the part beyond s on the part before it)
    if (method == BandMethod::NodalForces) {
        // [V algorithm, calcmef cefbandint FUN_18001abf0 / FUN_18001f290 / FUN_180022e70 / FUN_18001cd40, section 15.2] The cut is the
        // segment across the band at s (from -left to +right); the triangles it crosses contribute the element nodal forces (K_e u_e - f_e)
        // of their nodes on one side of the cut (a node on the cut line counts on both sides), weighted by
        // w = length of the cut inside the triangle / length of the infinite cut line inside the triangle (clamped to [0, 1]); a triangle
        // that the cut touches only at a point takes the mean factor of the others. Moments: M + r x F about the point of the band.
        // [N] which side is listed and its sign: the nodes beyond the cut with sign +1. On a fine mesh with a wide band both sides agree with
        // the stress methods (+0.3 % beyond, +0.7 % before, with sign -1); with the coarse mesh "beyond" is the closer of the two to the
        // original (Vz at the centre -36 against -71; "before": +5314).
        const auto along = [&](const P2& q) { return (q[0] - band.x1) * t[0] + (q[1] - band.y1) * t[1] - s; };
        struct Hit { int e; double w; bool atPoint; };
        std::vector<Hit> hits;
        for (int e : v.triangles) {
            const auto& nodes = v.mesh->triangles[e].nodes;
            const P2 c[3] = {v.xy.at(nodes[0]), v.xy.at(nodes[1]), v.xy.at(nodes[2])};
            // Interval [u0, u1] of the cut line p + u n inside the triangle (clipping against the three edges).
            double u0 = -1e300, u1 = 1e300;
            bool empty = false;
            for (int i = 0; i < 3 && !empty; ++i) {
                const P2 &a = c[i], &b = c[(i + 1) % 3];
                // inside: orient(a, b, x) >= 0 with x = p + u n -> g0 + g1 u >= 0
                const double g0 = (b[0] - a[0]) * (p[1] - a[1]) - (b[1] - a[1]) * (p[0] - a[0]);
                const double g1 = (b[0] - a[0]) * n[1] - (b[1] - a[1]) * n[0];
                const double tol = 1e-9 * std::hypot(b[0] - a[0], b[1] - a[1]);
                if (std::fabs(g1) < 1e-14) {
                    if (g0 < -tol) empty = true;
                } else if (g1 > 0) {
                    u0 = std::max(u0, -(g0 + tol) / g1);
                } else {
                    u1 = std::min(u1, -(g0 + tol) / g1);
                }
            }
            if (empty || u1 < u0) continue;
            const double lo = std::max(u0, -left), hi = std::min(u1, right);
            if (hi < lo - 1e-9) continue;
            const double lineLength = u1 - u0, cutLength = std::max(hi - lo, 0.0);
            if (cutLength <= 1e-6 || lineLength <= 1e-6) hits.push_back({e, 1.0, true});
            else hits.push_back({e, std::clamp(cutLength / lineLength, 0.0, 1.0), false});
        }
        double sumW = 0.0;
        int counted = 0;
        for (const Hit& h : hits)
            if (!h.atPoint) { sumW += h.w; ++counted; }
        for (Hit& h : hits)
            if (h.atPoint && counted > 0) h.w = sumW / counted;
        const FemShell& sh = *v.shell;
        const Vec3S p3{v.origin[0] + p[0] * sh.ex[0] + p[1] * sh.ey[0], v.origin[1] + p[0] * sh.ex[1] + p[1] * sh.ey[1],
                       v.origin[2] + p[0] * sh.ex[2] + p[1] * sh.ey[2]};
        double f[3] = {0, 0, 0}, m[3] = {0, 0, 0};
        for (const Hit& h : hits) {
            const auto& nodes = v.mesh->triangles[h.e].nodes;
            const auto& nf = v.lc->triangleForces[h.e].nodalForces;
            for (int k = 0; k < 6; ++k) {
                if (along(v.xy.at(nodes[k])) < -1e-6) continue;  // positive side (beyond the cut) and on the cut line
                const Vec3S& q = v.mesh->positions[nodes[k]];
                const double r[3] = {q[0] - p3[0], q[1] - p3[1], q[2] - p3[2]};
                double F[6];
                for (int c = 0; c < 6; ++c) F[c] = h.w * nf[6 * k + c];
                for (int c = 0; c < 3; ++c) { f[c] += F[c]; m[c] += F[3 + c]; }
                m[0] += r[1] * F[2] - r[2] * F[1];
                m[1] += r[2] * F[0] - r[0] * F[2];
                m[2] += r[0] * F[1] - r[1] * F[0];
            }
        }
        double axes[3][3];
        for (int c = 0; c < 3; ++c) {
            axes[0][c] = t[0] * sh.ex[c] + t[1] * sh.ey[c];
            axes[1][c] = n[0] * sh.ex[c] + n[1] * sh.ey[c];
            axes[2][c] = sh.ez[c];
        }
        for (int a = 0; a < 3; ++a) {
            canonical[a] = f[0] * axes[a][0] + f[1] * axes[a][1] + f[2] * axes[a][2];
            canonical[3 + a] = m[0] * axes[a][0] + m[1] * axes[a][1] + m[2] * axes[a][2];
        }
    } else {
        const bool smoothed = method == BandMethod::SmoothedStresses;
        // Composite 3-point Gauss across the width (pieces of at most 0.05 m).
        static constexpr double gx[3] = {-0.7745966692414834, 0.0, 0.7745966692414834}, gw[3] = {5.0 / 9, 8.0 / 9, 5.0 / 9};
        const double a = -left, b = right;
        const int pieces = std::max(1, static_cast<int>(std::ceil((b - a) / 0.05)));
        const double hpiece = (b - a) / pieces;
        for (int k = 0; k < pieces; ++k)
            for (int g = 0; g < 3; ++g) {
                const double eta = a + hpiece * (k + 0.5 + 0.5 * gx[g]), w = 0.5 * hpiece * gw[g];
                const auto field = fieldAt(v, P2{p[0] + eta * n[0], p[1] + eta * n[1]}, smoothed);
                if (!field) continue;
                const ShellForces r = rotateShellForces(*field, angle);  // Ntt Nnn Ntn Mtt Mnn Mtn Qt Qn
                canonical[0] += r[0] * w;
                canonical[1] += r[2] * w;
                canonical[2] += r[6] * w;
                canonical[3] += (eta * r[6] - r[5]) * w;
                canonical[4] += r[3] * w;
                canonical[5] += -eta * r[0] * w;
            }
    }
    out.forces = applySignConvention(canonical);
    ok = true;
    return out;
}

}  // namespace

std::map<int, ShellForces> smoothedShellForces(const FrameModel& model, const LinearResult& result, std::size_t caseIndex, int shellId) {
    std::map<int, ShellForces> out;
    const std::optional<ShellView> v = viewOf(model, result, caseIndex, shellId);
    if (v) out.insert(v->smoothed.begin(), v->smoothed.end());
    return out;
}

std::optional<ShellForces> shellForcesAt(const FrameModel& model, const LinearResult& result, std::size_t caseIndex, int shellId, double x,
                                         double y, bool smoothed) {
    const std::optional<ShellView> v = viewOf(model, result, caseIndex, shellId);
    if (!v) return std::nullopt;
    return fieldAt(*v, P2{x, y}, smoothed);
}

std::optional<Dof6> shellDisplacementAt(const FrameModel& model, const LinearResult& result, std::size_t caseIndex, int shellId, double x,
                                        double y) {
    const std::optional<ShellView> v = viewOf(model, result, caseIndex, shellId);
    if (!v || v->lc->meshDisplacements.size() != result.mesh.positions.size()) return std::nullopt;
    int e = -1;
    double r = 0, s = 0;
    if (!locate(*v, P2{x, y}, e, r, s)) return std::nullopt;
    // Quadratic interpolation of the six nodes of the triangle (corners, then mid-sides 01, 12, 20) [N: display interpolation of the clone].
    const double l0 = 1.0 - r - s, l1 = r, l2 = s;
    const double n[6] = {l0 * (2 * l0 - 1), l1 * (2 * l1 - 1), l2 * (2 * l2 - 1), 4 * l0 * l1, 4 * l1 * l2, 4 * l2 * l0};
    Dof6 out{};
    for (int k = 0; k < 6; ++k) {
        const Dof6& d = v->lc->meshDisplacements[result.mesh.triangles[e].nodes[k]];
        for (int q = 0; q < 6; ++q) out[q] += n[k] * d[q];
    }
    return out;
}

std::optional<BandPoint> bandForcesAt(const FrameModel& model, const LinearResult& result, std::size_t caseIndex, const BandSpec& band, double s,
                                      BandMethod method) {
    const std::optional<ShellView> v = viewOf(model, result, caseIndex, band.shell);
    if (!v) return std::nullopt;
    bool ok = false;
    const BandPoint p = integrateAt(*v, band, s, method, ok);
    if (!ok) return std::nullopt;
    return p;
}

std::vector<BandPoint> integrateBand(const FrameModel& model, const LinearResult& result, std::size_t caseIndex, const BandSpec& band, int points,
                                     BandMethod method) {
    std::vector<BandPoint> out;
    const std::optional<ShellView> v = viewOf(model, result, caseIndex, band.shell);
    if (!v || points < 2) return out;
    const double length = std::hypot(band.x2 - band.x1, band.y2 - band.y1);
    for (int i = 0; i < points; ++i) {
        bool ok = false;
        const BandPoint p = integrateAt(*v, band, length * i / (points - 1), method, ok);
        if (ok) out.push_back(p);
    }
    return out;
}

}  // namespace cc::fem
