#include "core/shell_properties.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <utility>

#include "core/geom3.h"
#include "core/shell_edit.h"

namespace cc::core {

namespace {

using geom::add;
using geom::cross;
using geom::dot;
using geom::scale;
using geom::sub;

std::string fmt(const char* pattern, double v) {
    char buf[64];
    std::snprintf(buf, sizeof buf, pattern, v);
    return buf;
}

bool finiteIn(double v, double lo, double hi) { return std::isfinite(v) && v >= lo && v <= hi; }

Point2 operator-(const Point2& a, const Point2& b) { return {a.x - b.x, a.y - b.y}; }
Point2 operator+(const Point2& a, const Point2& b) { return {a.x + b.x, a.y + b.y}; }
Point2 operator*(const Point2& a, double s) { return {a.x * s, a.y * s}; }
double dot2(const Point2& a, const Point2& b) { return a.x * b.x + a.y * b.y; }
double cross2(const Point2& a, const Point2& b) { return a.x * b.y - a.y * b.x; }
double len2(const Point2& a) { return std::sqrt(dot2(a, a)); }
Point2 unit2(const Point2& a) {
    const double l = len2(a);
    return l < 1e-12 ? Point2{1.0, 0.0} : Point2{a.x / l, a.y / l};
}

double distToSegment(const Point2& p, const Point2& a, const Point2& b) {
    const Point2 ab = b - a;
    const double l2 = dot2(ab, ab);
    double t = l2 > 0.0 ? dot2(p - a, ab) / l2 : 0.0;
    t = std::clamp(t, 0.0, 1.0);
    return len2(p - (a + ab * t));
}

/// Proper crossing: the segments cross at a point interior to both (touching or collinear overlap does not count).
bool properCross(const Point2& a, const Point2& b, const Point2& c, const Point2& d, double eps) {
    const double d1 = cross2(b - a, c - a), d2 = cross2(b - a, d - a);
    const double d3 = cross2(d - c, a - c), d4 = cross2(d - c, b - c);
    const double ab = len2(b - a), cd = len2(d - c);
    const double e1 = eps * std::max(ab, 1e-12), e2 = eps * std::max(cd, 1e-12);
    return ((d1 > e1 && d2 < -e1) || (d1 < -e1 && d2 > e1)) && ((d3 > e2 && d4 < -e2) || (d3 < -e2 && d4 > e2));
}

/// Parameter intervals [t0, t1] of the line o + t d (d unit) that lie inside the polygon, ordered, touching ones merged.
std::vector<std::pair<double, double>> lineIntervals(const std::vector<Point2>& poly, const Point2& o, const Point2& d) {
    std::vector<double> ts;
    const std::size_t n = poly.size();
    for (std::size_t i = 0; i < n; ++i) {
        const Point2 a = poly[i], e = poly[(i + 1) % n] - a;
        const double den = cross2(d, e);
        if (std::fabs(den) < 1e-12 * std::max(len2(e), 1e-12)) continue;
        const double t = cross2(a - o, e) / den;
        const double s = cross2(a - o, d) / den;
        if (s >= -1e-12 && s <= 1.0 + 1e-12) ts.push_back(t);
    }
    std::sort(ts.begin(), ts.end());
    std::vector<std::pair<double, double>> out;
    for (std::size_t i = 0; i + 1 < ts.size(); ++i) {
        if (ts[i + 1] - ts[i] < 1e-9) continue;
        const double mid = 0.5 * (ts[i] + ts[i + 1]);
        if (!pointInPolygon2D(o + d * mid, poly, 1e-9)) continue;
        if (!out.empty() && ts[i] - out.back().second < 1e-9) out.back().second = ts[i + 1];
        else out.emplace_back(ts[i], ts[i + 1]);
    }
    return out;
}

std::vector<std::pair<double, double>> intersectIntervals(const std::vector<std::pair<double, double>>& a,
                                                          const std::vector<std::pair<double, double>>& b) {
    std::vector<std::pair<double, double>> out;
    for (const auto& x : a)
        for (const auto& y : b) {
            const double lo = std::max(x.first, y.first), hi = std::min(x.second, y.second);
            if (hi - lo > 1e-9) out.emplace_back(lo, hi);
        }
    std::sort(out.begin(), out.end());
    return out;
}

bool lexLessOrEqual(const Point2& a, const Point2& b) { return a.x < b.x || (a.x == b.x && a.y <= b.y); }

ShellPropIssue issue(int id, std::string text) { return ShellPropIssue{id, std::move(text)}; }

std::vector<int> validIndices(const std::vector<int>& indices, std::size_t count) {
    std::vector<int> out;
    for (int i : indices)
        if (i >= 0 && static_cast<std::size_t>(i) < count && std::find(out.begin(), out.end(), i) == out.end()) out.push_back(i);
    std::sort(out.begin(), out.end());
    return out;
}

const char* const kStiffnessGroups[kStiffnessFactorCount] = {"Membrana", "Membrana", "Membrana", "Flexión", "Flexión", "Flexión", "Cortante", "Cortante"};
const char* const kStiffnessCoefs[kStiffnessFactorCount] = {"Fxx", "Fxy", "Fyy", "Mxx", "Mxy", "Myy", "Vxz", "Vyz"};

}  // namespace

// --- value types ---

double ShellStiffnessFactors::value(int i) const {
    switch (i) {
        case 0: return fxx;
        case 1: return fxy;
        case 2: return fyy;
        case 3: return mxx;
        case 4: return mxy;
        case 5: return myy;
        case 6: return vxz;
        case 7: return vyz;
    }
    return 1.0;
}

void ShellStiffnessFactors::set(int i, double v) {
    switch (i) {
        case 0: fxx = v; break;
        case 1: fxy = v; break;
        case 2: fyy = v; break;
        case 3: mxx = v; break;
        case 4: mxy = v; break;
        case 5: myy = v; break;
        case 6: vxz = v; break;
        case 7: vyz = v; break;
    }
}

std::string stiffnessFieldLabel(int i) {
    if (i < 0 || i >= kStiffnessFactorCount) return {};
    return std::string(kStiffnessGroups[i]) + " " + kStiffnessCoefs[i];
}

double ShellIntegrationBand::length() const { return std::hypot(x2 - x1, y2 - y1); }

ShellIntegrationBand normaliseBand(ShellIntegrationBand b) {
    if (!lexLessOrEqual({b.x1, b.y1}, {b.x2, b.y2})) {
        std::swap(b.x1, b.x2);
        std::swap(b.y1, b.y2);
    }
    return b;
}

EdgeInteriorBond normaliseEdgeInterior(EdgeInteriorBond bond) {
    const EdgeInteriorBond def;
    if (bond.kind != EdgeInteriorKind::Generic) {
        bond.dof = def.dof;
        bond.k = def.k;
        return bond;
    }
    for (std::size_t i = 0; i < 6; ++i)
        if (bond.dof[i] != DofMode::Elastic) bond.k[i] = def.k[i];
    return bond;
}

bool isDefaultEdgeInterior(const EdgeInteriorBond& bond) { return normaliseEdgeInterior(bond) == EdgeInteriorBond{}; }

ShellEdgeKey makeEdgeKey(int shellId, int a, int b) { return ShellEdgeKey{shellId, std::min(a, b), std::max(a, b)}; }

std::vector<ShellEdgeKey> shellEdges(const StructuralModel& model, int shellId) {
    std::vector<ShellEdgeKey> out;
    const Shell* s = model.findShell(shellId);
    if (!s) return out;
    const std::size_t n = s->nodeIds.size();
    for (std::size_t i = 0; i < n; ++i) out.push_back(makeEdgeKey(shellId, s->nodeIds[i], s->nodeIds[(i + 1) % n]));
    return out;
}

// --- storage helpers ---

bool isDefaultShellRecord(const ShellRecord& r) { return r == ShellRecord{}; }

ShellRecord shellRecordOf(const ShellProperties& data, int shellId) {
    const auto it = data.shells.find(shellId);
    return it == data.shells.end() ? ShellRecord{} : it->second;
}

void setShellRecord(ShellProperties& data, int shellId, const ShellRecord& record) {
    if (isDefaultShellRecord(record)) data.shells.erase(shellId);
    else data.shells[shellId] = record;
}

EdgeInteriorBond edgeInteriorOf(const ShellProperties& data, const ShellEdgeKey& key) {
    const auto it = data.interior.find(key);
    return it == data.interior.end() ? EdgeInteriorBond{} : it->second;
}

ExternalSupport edgeExteriorOf(const ShellProperties& data, const ShellEdgeKey& key) {
    const auto it = data.exterior.find(key);
    return it == data.exterior.end() ? ExternalSupport{} : it->second;
}

void setEdgeInterior(ShellProperties& data, const ShellEdgeKey& key, const EdgeInteriorBond& bond) {
    const EdgeInteriorBond n = normaliseEdgeInterior(bond);
    if (n == EdgeInteriorBond{}) data.interior.erase(key);
    else data.interior[key] = n;
}

void setEdgeExterior(ShellProperties& data, const ShellEdgeKey& key, const ExternalSupport& support) {
    if (!restraintOf(support).any()) data.exterior.erase(key);
    else data.exterior[key] = support;
}

// --- validation ---

std::string shellRangeMessage(const std::string& field, const std::string& lo, const std::string& hi) {
    return "El valor de '" + field + "' debe estar entre " + lo + " y " + hi + ".";
}

std::string triangleSizeRangeMessage() {
    return "El valor de 'Tamaño máximo de los lados de un triángulo' debe estar entre 0.01 m y 100.00 m.";
}

std::string stiffnessRangeMessage(const std::string& field) {
    // The template is "entre %s %s y %s %s." with the (empty) units inserted: hence the double space and the space before the full stop.
    return "El valor de '" + field + "' debe estar entre 0.000  y 100.000 .";
}

std::string bandWidthRangeMessage() { return "El valor de 'Anchura' debe estar entre 1.0 y 10000.0."; }

std::optional<ShellPropIssue> checkTriangleSize(double sideM) {
    // 0.005 mm of slack so that a value typed at the two decimals of the dialog never fails by rounding.
    if (!std::isfinite(sideM) || sideM < kShellTriangleSideMinM - 5e-6 || sideM > kShellTriangleSideMaxM + 5e-6) return issue(0, triangleSizeRangeMessage());
    return std::nullopt;
}

std::optional<ShellPropIssue> checkStiffnessFactors(const ShellStiffnessFactors& f) {
    for (int i = 0; i < kStiffnessFactorCount; ++i)
        if (!finiteIn(f.value(i), kStiffnessFactorMin, kStiffnessFactorMax)) return issue(0, stiffnessRangeMessage(stiffnessFieldLabel(i)));
    return std::nullopt;
}

std::optional<ShellPropIssue> checkBandWidth(double widthM) {
    const double cm = widthM * 100.0;
    if (!std::isfinite(cm) || cm < 1.0 - 1e-6 || cm > 10000.0 + 1e-6) return issue(0, bandWidthRangeMessage());
    return std::nullopt;
}

std::optional<ShellPropIssue> checkEdgeInterior(const EdgeInteriorBond& bond) {
    if (static_cast<int>(bond.kind) < 0 || static_cast<int>(bond.kind) > static_cast<int>(EdgeInteriorKind::Generic)) return issue(0, "Tipo de vinculación interior no válido.");
    if (bond.kind != EdgeInteriorKind::Generic) return std::nullopt;
    for (std::size_t i = 0; i < 6; ++i)
        if (bond.dof[i] == DofMode::Elastic && !finiteIn(bond.k[i], kMinElasticConstant, kMaxElasticConstant))
            return issue(0, "El valor de 'K' debe estar entre 1 y 10000000.");  // TODO(medir): wording / range on edges
    return std::nullopt;
}

// --- local 2D system ---

Point2 ShellFrame::toLocal(const Point3& w) const {
    const Point3 d = sub(w, origin);
    return {dot(d, axisX), dot(d, axisY)};
}

Point3 ShellFrame::toWorld(const Point2& p) const { return add(origin, add(scale(axisX, p.x), scale(axisY, p.y))); }

double ShellFrame::heightOf(const Point3& w) const { return dot(sub(w, origin), axisZ); }

std::optional<ShellFrame> shellFrame(const StructuralModel& model, const Shell& shell) {
    if (shell.nodeIds.empty()) return std::nullopt;
    const Node* first = model.findNode(shell.nodeIds.front());
    const auto axes = shellLocalAxes(model, shell);
    if (!first || !axes) return std::nullopt;
    return ShellFrame{first->position, axes->x, axes->y, axes->z};
}

std::vector<Point2> shellContour2D(const StructuralModel& model, const Shell& shell) {
    const auto frame = shellFrame(model, shell);
    if (!frame) return {};
    std::vector<Point2> out;
    for (int id : shell.nodeIds) {
        const Node* n = model.findNode(id);
        if (!n) return {};
        out.push_back(frame->toLocal(n->position));
    }
    return out;
}

bool pointInPolygon2D(const Point2& p, const std::vector<Point2>& poly, double tol) {
    const std::size_t n = poly.size();
    if (n < 3) return false;
    for (std::size_t i = 0; i < n; ++i)
        if (distToSegment(p, poly[i], poly[(i + 1) % n]) <= tol) return true;
    bool inside = false;
    for (std::size_t i = 0, j = n - 1; i < n; j = i++) {
        const Point2 &a = poly[i], &b = poly[j];
        if ((a.y > p.y) != (b.y > p.y) && p.x < (b.x - a.x) * (p.y - a.y) / (b.y - a.y) + a.x) inside = !inside;
    }
    return inside;
}

std::array<Point2, 4> bandCorners(const ShellIntegrationBand& b) {
    const Point2 p1{b.x1, b.y1}, p2{b.x2, b.y2};
    const Point2 d = unit2(p2 - p1);
    const Point2 h = Point2{-d.y, d.x} * (b.widthM * 0.5);
    return {p1 - h, p2 - h, p2 + h, p1 + h};
}

// --- bands ---

namespace {

bool rectInside(const std::vector<Point2>& contour, const std::array<Point2, 4>& r) {
    constexpr double tol = kShellGeomToleranceM;
    for (const Point2& c : r)
        if (!pointInPolygon2D(c, contour, tol)) return false;
    const std::size_t n = contour.size();
    for (std::size_t i = 0; i < 4; ++i)
        for (std::size_t j = 0; j < n; ++j)
            if (properCross(r[i], r[(i + 1) % 4], contour[j], contour[(j + 1) % n], 1e-9)) return false;
    // A vertex of the contour strictly inside the rectangle: the boundary enters it (concave contour).
    for (const Point2& v : contour) {
        bool strictlyInside = true;
        for (std::size_t i = 0; i < 4 && strictlyInside; ++i) {
            const Point2 a = r[i], b = r[(i + 1) % 4];
            if (cross2(b - a, v - a) / std::max(len2(b - a), 1e-12) <= tol) strictlyInside = false;  // rectangle corners are in the same turn order
        }
        if (strictlyInside) return false;
        strictlyInside = true;
        for (std::size_t i = 0; i < 4 && strictlyInside; ++i) {
            const Point2 a = r[i], b = r[(i + 1) % 4];
            if (cross2(b - a, v - a) / std::max(len2(b - a), 1e-12) >= -tol) strictlyInside = false;  // opposite turn order
        }
        if (strictlyInside) return false;
    }
    return true;
}

BandResult fail(std::optional<ShellPropIssue> why) {
    BandResult r;
    r.issue = std::move(why);
    return r;
}

BandResult done(std::vector<ShellIntegrationBand> bands) {
    BandResult r;
    r.bands = std::move(bands);
    return r;
}

}  // namespace

std::optional<ShellPropIssue> checkBand(const std::vector<Point2>& contour, const ShellIntegrationBand& b) {
    if (auto w = checkBandWidth(b.widthM)) return w;
    if (!std::isfinite(b.x1) || !std::isfinite(b.y1) || !std::isfinite(b.x2) || !std::isfinite(b.y2))
        return issue(64190, "La banda de integración no está completamente contenida en la lámina.");
    if (b.length() < kBandMinLengthM - 1e-9)
        return issue(64191, "La longitud de la banda de integración es inferior a la mínima permitida (" + fmt("%.2f", kBandMinLengthM) + " m).");
    if (!rectInside(contour, bandCorners(b))) return issue(64190, "La banda de integración no está completamente contenida en la lámina.");
    return std::nullopt;
}

BandResult newBand(const std::vector<Point2>& contour, const std::vector<ShellIntegrationBand>& current, const Point2& p1, const Point2& p2,
                   double widthM) {
    const ShellIntegrationBand b = normaliseBand({p1.x, p1.y, p2.x, p2.y, widthM});
    if (auto why = checkBand(contour, b)) return fail(why);
    std::vector<ShellIntegrationBand> out = current;
    out.push_back(b);
    return done(std::move(out));
}

BandResult editBands(const std::vector<Point2>& contour, const std::vector<ShellIntegrationBand>& current, const std::vector<int>& indices,
                     double widthM) {
    const std::vector<int> ids = validIndices(indices, current.size());
    if (ids.empty()) return fail(issue(0, "No hay bandas de integración seleccionadas."));
    if (auto w = checkBandWidth(widthM)) return fail(w);
    std::vector<ShellIntegrationBand> out = current;
    for (int i : ids) {
        out[static_cast<std::size_t>(i)].widthM = widthM;
        if (auto why = checkBand(contour, out[static_cast<std::size_t>(i)])) return fail(why);
    }
    return done(std::move(out));
}

BandResult deleteBands(const std::vector<ShellIntegrationBand>& current, const std::vector<int>& indices) {
    const std::vector<int> ids = validIndices(indices, current.size());
    if (ids.empty()) return fail(issue(0, "No hay bandas de integración seleccionadas."));
    std::vector<ShellIntegrationBand> out;
    for (std::size_t i = 0; i < current.size(); ++i)
        if (!std::binary_search(ids.begin(), ids.end(), static_cast<int>(i))) out.push_back(current[i]);
    return done(std::move(out));
}

BandResult moveBandEnd(const std::vector<Point2>& contour, const std::vector<ShellIntegrationBand>& current, int index, int end, const Point2& to) {
    if (index < 0 || static_cast<std::size_t>(index) >= current.size() || (end != 0 && end != 1)) return fail(issue(0, "No hay bandas de integración seleccionadas."));
    ShellIntegrationBand b = normaliseBand(current[static_cast<std::size_t>(index)]);
    if (end == 0) {
        b.x1 = to.x;
        b.y1 = to.y;
    } else {
        b.x2 = to.x;
        b.y2 = to.y;
    }
    b = normaliseBand(b);
    if (auto why = checkBand(contour, b)) return fail(why);
    std::vector<ShellIntegrationBand> out = current;
    out[static_cast<std::size_t>(index)] = b;
    return done(std::move(out));
}

BandResult joinBands(const std::vector<Point2>& contour, const std::vector<ShellIntegrationBand>& current, const std::vector<int>& indices) {
    const std::vector<int> ids = validIndices(indices, current.size());
    if (ids.size() < 2) return fail(issue(0, "Debe seleccionar, al menos, dos bandas de integración."));
    const ShellIntegrationBand first = normaliseBand(current[static_cast<std::size_t>(ids.front())]);
    const Point2 o{first.x1, first.y1};
    const Point2 d = unit2(Point2{first.x2, first.y2} - o);
    double tMin = 0.0, tMax = 0.0, width = 0.0;
    for (int i : ids) {
        const ShellIntegrationBand b = normaliseBand(current[static_cast<std::size_t>(i)]);
        const Point2 p1{b.x1, b.y1}, p2{b.x2, b.y2};
        if (std::fabs(cross2(d, unit2(p2 - p1))) > 1e-6) return fail(issue(63999, "Las bandas de integración seleccionadas deben ser paralelas."));
        for (const Point2& p : {p1, p2}) {
            const double t = dot2(p - o, d);
            tMin = std::min(tMin, t);
            tMax = std::max(tMax, t);
        }
        width = std::max(width, b.widthM);
    }
    const Point2 a = o + d * tMin, c = o + d * tMax;
    const ShellIntegrationBand joined = normaliseBand({a.x, a.y, c.x, c.y, width});
    if (auto why = checkBand(contour, joined)) return fail(why);
    std::vector<ShellIntegrationBand> out;
    for (std::size_t i = 0; i < current.size(); ++i) {
        if (static_cast<int>(i) == ids.front()) out.push_back(joined);
        else if (!std::binary_search(ids.begin(), ids.end(), static_cast<int>(i))) out.push_back(current[i]);
    }
    return done(std::move(out));
}

BandResult divideBands(const std::vector<Point2>& contour, const std::vector<ShellIntegrationBand>& current, const std::vector<int>& indices,
                       int parts) {
    const std::vector<int> ids = validIndices(indices, current.size());
    if (ids.empty()) return fail(issue(0, "No hay bandas de integración seleccionadas."));
    if (parts < kBandMinDivisions || parts > kBandMaxDivisions)
        return fail(issue(0, shellRangeMessage("Número de divisiones", std::to_string(kBandMinDivisions), std::to_string(kBandMaxDivisions))));
    std::vector<ShellIntegrationBand> out;
    for (std::size_t i = 0; i < current.size(); ++i) {
        if (!std::binary_search(ids.begin(), ids.end(), static_cast<int>(i))) {
            out.push_back(current[i]);
            continue;
        }
        const ShellIntegrationBand b = normaliseBand(current[i]);
        const Point2 p1{b.x1, b.y1}, step = (Point2{b.x2, b.y2} - p1) * (1.0 / parts);
        for (int k = 0; k < parts; ++k) {
            const Point2 a = p1 + step * k, c = (k + 1 == parts) ? Point2{b.x2, b.y2} : p1 + step * (k + 1);
            const ShellIntegrationBand piece{a.x, a.y, c.x, c.y, b.widthM};
            if (auto why = checkBand(contour, piece)) return fail(why);
            out.push_back(piece);
        }
    }
    return done(std::move(out));
}

BandResult generateBands(const std::vector<Point2>& contour, const std::vector<ShellIntegrationBand>& current, const std::array<Point2, 4>& z,
                         double widthM) {
    if (auto w = checkBandWidth(widthM)) return fail(w);
    const std::string notQuad = "Los puntos seleccionados no definen un cuadrilátero";
    const std::vector<Point2> zone(z.begin(), z.end());
    double area2 = 0.0;
    for (std::size_t i = 0; i < 4; ++i) {
        if (len2(z[(i + 1) % 4] - z[i]) < 1e-9) return fail(issue(65158, notQuad));
        area2 += cross2(z[i], z[(i + 1) % 4]);
    }
    if (std::fabs(area2) < 1e-9 || properCross(z[0], z[1], z[2], z[3], 1e-9) || properCross(z[1], z[2], z[3], z[0], 1e-9))
        return fail(issue(65158, notQuad));

    const Point2 d = unit2(z[1] - z[0]);
    Point2 n{-d.y, d.x};
    if (dot2(z[2] - z[1], n) < 0.0) n = n * -1.0;
    double extent = 0.0;
    for (const Point2& p : z) extent = std::max(extent, dot2(p - z[0], n));
    if (extent <= widthM + 1e-9) return fail(issue(65159, "El ancho de la zona de generación debe ser mayor que el ancho de la banda de integración."));
    const int count = static_cast<int>(std::floor(extent / widthM + 1e-9));

    std::vector<ShellIntegrationBand> out = current;
    const std::size_t before = out.size();
    const double h = widthM * 0.5;
    for (int i = 0; i < count; ++i) {
        const double centre = (i + 0.5) * widthM;
        std::vector<std::pair<double, double>> ivs;
        bool first = true;
        for (double off : {centre - h, centre, centre + h}) {
            const Point2 base = z[0] + n * off;
            auto part = intersectIntervals(lineIntervals(contour, base, d), lineIntervals(zone, base, d));
            ivs = first ? part : intersectIntervals(ivs, part);
            first = false;
        }
        const Point2 base = z[0] + n * centre;
        for (const auto& [t0, t1] : ivs) {
            if (t1 - t0 < kBandMinLengthM) continue;
            const Point2 a = base + d * t0, c = base + d * t1;
            const ShellIntegrationBand b = normaliseBand({a.x, a.y, c.x, c.y, widthM});
            if (!checkBand(contour, b)) out.push_back(b);
        }
    }
    if (out.size() == before) return fail(issue(64476, "No es posible generar bandas de integración en la zona seleccionada."));
    return done(std::move(out));
}

// --- info panel ---

std::string edgeInteriorKindName(EdgeInteriorKind kind) {
    switch (kind) {
        case EdgeInteriorKind::Free: return "Libre";
        case EdgeInteriorKind::Fixed: return "Empotrada";
        case EdgeInteriorKind::Pinned: return "Articulada";
        case EdgeInteriorKind::Generic: return "Genérica";
    }
    return {};
}

std::string edgeInteriorLine(const EdgeInteriorBond& bond) { return "Vinc. interior : " + edgeInteriorKindName(bond.kind); }

std::vector<std::string> shellInfoLines(const ShellRecord& r, ShellInfoPart part) {
    std::vector<std::string> out;
    switch (part) {
        case ShellInfoPart::Stiffness:
            for (int i : {0, 2, 1, 3, 5, 4, 6, 7})  // ficha order: Fxx, Fyy, Fxy, Mxx, Myy, Mxy, Vxz, Vyz
                out.push_back(stiffnessFieldLabel(i) + " : " + fmt("%g", r.stiffness.value(i)));
            break;
        case ShellInfoPart::Discretisation:
            out.push_back("Tamaño máximo de los lados de un triángulo : " + fmt("%.2f", r.maxTriangleSideM) + " m");
            break;
        case ShellInfoPart::Buckling:
            out.push_back("Análisis de pandeo");
            out.push_back(r.bucklingConsidered ? "Se considera en el análisis" : "No se considera en el análisis");
            break;
        case ShellInfoPart::Modal:
            out.push_back("Análisis modal");
            out.push_back(r.modalConsidered ? "Se considera en el análisis" : "No se considera en el análisis");  // TODO(medir): negative text
            break;
    }
    return out;
}

// --- consistency ---

std::string validateShellProperties(const ShellProperties& data) {
    for (const auto& [id, r] : data.shells) {
        (void)id;
        if (isDefaultShellRecord(r)) return "láminas: registro por defecto almacenado";
        if (checkTriangleSize(r.maxTriangleSideM)) return "láminas: tamaño de triángulo fuera de rango";
        if (checkStiffnessFactors(r.stiffness)) return "láminas: factor de rigidez fuera de rango";
        for (const ShellIntegrationBand& b : r.bands) {
            if (checkBandWidth(b.widthM)) return "láminas: ancho de banda fuera de rango";
            if (!(b.length() > 0.0)) return "láminas: banda de longitud nula";
            if (!lexLessOrEqual({b.x1, b.y1}, {b.x2, b.y2})) return "láminas: extremos de banda sin ordenar";
        }
    }
    for (const auto& [key, bond] : data.interior) {
        if (key.nodeA >= key.nodeB) return "láminas: arista sin normalizar";
        if (isDefaultEdgeInterior(bond)) return "láminas: vinculación interior por defecto almacenada";
        if (bond != normaliseEdgeInterior(bond)) return "láminas: vinculación interior sin normalizar";
        if (checkEdgeInterior(bond)) return "láminas: vinculación interior no válida";
    }
    for (const auto& [key, support] : data.exterior) {
        if (key.nodeA >= key.nodeB) return "láminas: arista sin normalizar";
        if (!restraintOf(support).any()) return "láminas: vinculación exterior libre almacenada";
    }
    return {};
}

// --- reconcile ---

namespace {

bool sameFrame(const ShellFrame& a, const ShellFrame& b) {
    auto close = [](const Point3& p, const Point3& q) { return geom::length(sub(p, q)) < 1e-9; };
    return close(a.origin, b.origin) && close(a.axisX, b.axisX) && close(a.axisY, b.axisY) && close(a.axisZ, b.axisZ);
}

Point3 centroid(const StructuralModel& m, const Shell& s) {
    Point3 c;
    int n = 0;
    for (int id : s.nodeIds)
        if (const Node* node = m.findNode(id)) {
            c = add(c, node->position);
            ++n;
        }
    return n ? scale(c, 1.0 / n) : c;
}

/// True when the point lies in the shell (within 1 mm of its plane and inside its contour).
bool shellContains(const StructuralModel& m, const Shell& s, const Point3& p) {
    const auto frame = shellFrame(m, s);
    if (!frame || std::fabs(frame->heightOf(p)) > 1e-3) return false;
    return pointInPolygon2D(frame->toLocal(p), shellContour2D(m, s), 1e-6);
}

bool onSegment(const Point3& p, const Point3& a, const Point3& b) {
    const Point3 ab = sub(b, a);
    const double l2 = dot(ab, ab);
    if (l2 <= 0.0) return false;
    const double t = dot(sub(p, a), ab) / l2;
    if (t < -1e-9 || t > 1.0 + 1e-9) return false;
    return geom::length(sub(p, add(a, scale(ab, t)))) <= 1e-6;
}

}  // namespace

ShellProperties reconcileShellProperties(const ShellProperties& data, const StructuralModel& before, const StructuralModel& after) {
    if (data.empty()) return data;
    ShellProperties out;
    out.bandForce = data.bandForce;
    for (const Shell& s : after.shells) {
        // Sources: the shell itself (same id) + the shells that vanished inside it, or the parent it was cut from.
        std::vector<const Shell*> sources;
        if (const Shell* same = before.findShell(s.id)) {
            sources.push_back(same);
            for (const Shell& gone : before.shells)
                if (gone.id != s.id && !after.findShell(gone.id) && shellContains(after, s, centroid(before, gone))) sources.push_back(&gone);
        } else {
            const Point3 c = centroid(after, s);
            for (const Shell& cand : before.shells)
                if (shellContains(before, cand, c)) {
                    sources.push_back(&cand);
                    break;
                }
        }
        if (sources.empty()) continue;

        const auto dstFrame = shellFrame(after, s);
        const std::vector<Point2> dstContour = shellContour2D(after, s);
        ShellRecord rec = shellRecordOf(data, sources.front()->id);
        std::vector<ShellIntegrationBand> bands;
        for (std::size_t si = 0; si < sources.size(); ++si) {
            const Shell& src = *sources[si];
            const auto rit = data.shells.find(src.id);
            if (rit == data.shells.end()) continue;
            const auto srcFrame = shellFrame(before, src);
            if (!srcFrame || !dstFrame || dstContour.empty()) continue;
            const bool identity = sameFrame(*srcFrame, *dstFrame);
            for (const ShellIntegrationBand& b : rit->second.bands) {
                ShellIntegrationBand nb = b;
                if (!identity) {
                    const Point2 p1 = dstFrame->toLocal(srcFrame->toWorld({b.x1, b.y1}));
                    const Point2 p2 = dstFrame->toLocal(srcFrame->toWorld({b.x2, b.y2}));
                    nb = normaliseBand({p1.x, p1.y, p2.x, p2.y, b.widthM});
                }
                if (checkBand(dstContour, nb)) continue;  // no longer fits
                if (std::find(bands.begin(), bands.end(), nb) == bands.end()) bands.push_back(nb);
            }
        }
        rec.bands = std::move(bands);
        setShellRecord(out, s.id, rec);

        // Edge bonds: copy by node pair, otherwise inherit from a source edge that contains the new edge.
        const std::size_t n = s.nodeIds.size();
        for (std::size_t e = 0; e < n; ++e) {
            const int u = s.nodeIds[e], v = s.nodeIds[(e + 1) % n];
            const ShellEdgeKey dstKey = makeEdgeKey(s.id, u, v);
            const Node *nu = after.findNode(u), *nv = after.findNode(v);
            if (!nu || !nv) continue;
            bool haveInterior = false, haveExterior = false;
            for (const Shell* src : sources) {
                auto matches = [&](const ShellEdgeKey& k) {
                    if (k.shellId != src->id) return false;
                    if (k.nodeA == dstKey.nodeA && k.nodeB == dstKey.nodeB) return true;
                    const Node *a = before.findNode(k.nodeA), *b = before.findNode(k.nodeB);
                    return a && b && onSegment(nu->position, a->position, b->position) && onSegment(nv->position, a->position, b->position);
                };
                if (!haveInterior)
                    for (const auto& [k, bond] : data.interior)
                        if (matches(k)) {
                            out.interior[dstKey] = bond;
                            haveInterior = true;
                            break;
                        }
                if (!haveExterior)
                    for (const auto& [k, support] : data.exterior)
                        if (matches(k)) {
                            out.exterior[dstKey] = support;
                            haveExterior = true;
                            break;
                        }
            }
        }
    }
    return out;
}

}  // namespace cc::core
