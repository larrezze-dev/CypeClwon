#include "core/panel.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "core/bar_load.h"
#include "core/geom3.h"

namespace cc::core {

namespace {

using geom::add;
using geom::cross;
using geom::dot;
using geom::length;
using geom::scale;
using geom::sub;
using geom::unit;

std::string fixedText(double v, int decimals) {
    char buf[64];
    std::snprintf(buf, sizeof buf, "%.*f", decimals, v);
    return buf;
}

/// "20", "-0.447", "9.81": up to `decimals` decimals, trailing zeros removed, no negative zero.
std::string trimmedText(double v, int decimals) {
    const double r = roundToDecimals(v, decimals);
    std::string s = fixedText(r == 0.0 ? 0.0 : r, decimals);
    if (s.find('.') != std::string::npos) {
        while (!s.empty() && s.back() == '0') s.pop_back();
        if (!s.empty() && s.back() == '.') s.pop_back();
    }
    return s.empty() ? "0" : s;
}

bool inRange(const SurfaceLoadFieldSpec& s, double v) {
    if (!std::isfinite(v)) return false;
    const double r = roundToDecimals(v, s.decimals);
    return r >= s.min - 1e-12 && r <= s.max + 1e-12;
}

SurfaceLoadIssue makeIssue(SurfaceLoadIssueKind kind, std::string message, std::optional<SurfaceLoadField> field = std::nullopt) {
    return SurfaceLoadIssue{kind, std::move(message), field};
}

bool finite(const Point3& p) { return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z); }
bool nullVector(const Point3& v) { return !finite(v) || length(v) < 1e-12; }

// --- 2D helpers ---

Point2 sub2(const Point2& a, const Point2& b) { return {a.x - b.x, a.y - b.y}; }
double cross2(const Point2& a, const Point2& b) { return a.x * b.y - a.y * b.x; }
double dot2(const Point2& a, const Point2& b) { return a.x * b.x + a.y * b.y; }

double distPointSegment2(const Point2& p, const Point2& a, const Point2& b) {
    const Point2 ab = sub2(b, a);
    const double l2 = dot2(ab, ab);
    if (l2 < 1e-24) return std::hypot(p.x - a.x, p.y - a.y);
    const double t = std::clamp(dot2(sub2(p, a), ab) / l2, 0.0, 1.0);
    return std::hypot(p.x - (a.x + ab.x * t), p.y - (a.y + ab.y * t));
}

bool segmentsCross2(const Point2& a, const Point2& b, const Point2& c, const Point2& d) {
    const double d1 = cross2(sub2(b, a), sub2(c, a)), d2 = cross2(sub2(b, a), sub2(d, a));
    const double d3 = cross2(sub2(d, c), sub2(a, c)), d4 = cross2(sub2(d, c), sub2(b, c));
    return ((d1 > 0) != (d2 > 0)) && ((d3 > 0) != (d4 > 0)) && d1 != 0 && d2 != 0 && d3 != 0 && d4 != 0;
}

double distSegmentSegment2(const Point2& a, const Point2& b, const Point2& c, const Point2& d) {
    if (segmentsCross2(a, b, c, d)) return 0.0;
    return std::min({distPointSegment2(a, c, d), distPointSegment2(b, c, d), distPointSegment2(c, a, b), distPointSegment2(d, a, b)});
}

double distPointToPlanePoint(const Point3& a, const Point3& b) { return length(sub(a, b)); }

}  // namespace

// --- fields ---

SurfaceLoadData surfaceLoadDefaults(int loadCaseId, SurfaceLoadType type) {
    SurfaceLoadData d;
    d.loadCaseId = loadCaseId;
    d.type = type;
    return d;
}

SurfaceLoadFieldSpec surfaceLoadFieldSpec(SurfaceLoadField field) {
    SurfaceLoadFieldSpec s;
    switch (field) {
        case SurfaceLoadField::Value: s.label = "Valor"; break;
        case SurfaceLoadField::Lower: s.label = "Valor en la cota inferior"; break;
        case SurfaceLoadField::Upper: s.label = "Valor en la cota superior"; break;
    }
    return s;
}

std::string surfaceLoadRangeMessage(const SurfaceLoadFieldSpec& spec) {
    const std::string lo = fixedText(spec.min, spec.decimals), hi = fixedText(spec.max, spec.decimals);
    return "El valor de '" + spec.label + "' debe estar entre " + lo + " " + spec.unit + " y " + hi + " " + spec.unit + ".";
}
std::string surfaceLoadZeroMessage() { return "No puede introducir cargas de valor nulo."; }
std::string surfaceLoadNotNumberMessage() { return nodalLoadNotNumberMessage(); }
std::string surfaceLoadVariableOnHorizontalMessage() { return "Las cargas variables con la altura sólo pueden aplicarse a superficies no horizontales."; }

SurfaceLoadFieldParse parseSurfaceLoadField(SurfaceLoadField field, const std::string& text) {
    SurfaceLoadFieldParse out;
    const SurfaceLoadFieldSpec spec = surfaceLoadFieldSpec(field);
    double v = 0.0;
    if (!parsePlainDecimal(text, v)) {
        out.message = surfaceLoadNotNumberMessage();
        return out;
    }
    if (!inRange(spec, v)) {
        out.message = surfaceLoadRangeMessage(spec);
        return out;
    }
    out.ok = true;
    out.value = roundToDecimals(v, spec.decimals);
    return out;
}

SurfaceLoadIssue checkSurfaceLoadData(const SurfaceLoadData& d) {
    const int type = static_cast<int>(d.type);
    if (type < 0 || type > 1) return makeIssue(SurfaceLoadIssueKind::Error, "El tipo de carga no es válido.");
    const auto warn = [&](SurfaceLoadField f) { return makeIssue(SurfaceLoadIssueKind::Warning, surfaceLoadRangeMessage(surfaceLoadFieldSpec(f)), f); };
    if (d.type == SurfaceLoadType::Uniform) {
        if (!inRange(surfaceLoadFieldSpec(SurfaceLoadField::Value), d.value)) return warn(SurfaceLoadField::Value);
        if (roundToDecimals(d.value, kSurfaceLoadDecimals) == 0.0) return makeIssue(SurfaceLoadIssueKind::Error, surfaceLoadZeroMessage(), SurfaceLoadField::Value);
        return {};
    }
    // Dialog order of the variable type: the surface-load dialog lists the upper value first, the panel table the lower one.
    if (!inRange(surfaceLoadFieldSpec(SurfaceLoadField::Upper), d.upper)) return warn(SurfaceLoadField::Upper);
    if (!inRange(surfaceLoadFieldSpec(SurfaceLoadField::Lower), d.lower)) return warn(SurfaceLoadField::Lower);
    if (roundToDecimals(d.upper, kSurfaceLoadDecimals) == 0.0 && roundToDecimals(d.lower, kSurfaceLoadDecimals) == 0.0)
        return makeIssue(SurfaceLoadIssueKind::Error, surfaceLoadZeroMessage(), SurfaceLoadField::Upper);
    return {};
}

SurfaceLoadData normaliseSurfaceLoadData(SurfaceLoadData d) {
    if (d.type == SurfaceLoadType::Uniform) {
        d.value = roundToDecimals(d.value, kSurfaceLoadDecimals);
        d.lower = d.upper = 0.0;
    } else {
        d.lower = roundToDecimals(d.lower, kSurfaceLoadDecimals);
        d.upper = roundToDecimals(d.upper, kSurfaceLoadDecimals);
        d.value = 0.0;
    }
    return d;
}

// --- plane of a polygon ---

Point2 PanelFrame::toLocal(const Point3& p) const {
    const Point3 d = sub(p, origin);
    return {dot(d, u), dot(d, v)};
}
Point3 PanelFrame::toWorld(const Point2& l) const { return add(origin, add(scale(u, l.x), scale(v, l.y))); }
double PanelFrame::heightOf(const Point3& p) const { return dot(sub(p, origin), w); }

std::optional<PanelFrame> panelFrame(const std::vector<Point3>& polygon) {
    if (polygon.size() < 3) return std::nullopt;
    for (const Point3& p : polygon)
        if (!finite(p)) return std::nullopt;
    const Point3 n = geom::newell(polygon);
    if (length(n) * 0.5 < kPanelMinAreaM2) return std::nullopt;
    PanelFrame f;
    f.origin = polygon.front();
    f.w = unit(n);
    Point3 e;
    for (std::size_t i = 1; i < polygon.size(); ++i) {
        e = sub(polygon[i], polygon[0]);
        if (length(e) > 1e-9) break;
    }
    e = sub(e, scale(f.w, dot(e, f.w)));
    if (length(e) < 1e-9) return std::nullopt;
    f.u = unit(e);
    f.v = cross(f.w, f.u);
    return f;
}

bool isHorizontalPlane(const PanelFrame& f) { return std::fabs(f.w.x) < 1e-4 && std::fabs(f.w.y) < 1e-4; }
bool isHorizontalPolygon(const std::vector<Point3>& polygon) {
    const auto f = panelFrame(polygon);
    return f && isHorizontalPlane(*f);
}

std::vector<Point2> polygonLocal(const PanelFrame& f, const std::vector<Point3>& polygon) {
    std::vector<Point2> out;
    out.reserve(polygon.size());
    for (const Point3& p : polygon) out.push_back(f.toLocal(p));
    return out;
}

double polygonArea(const std::vector<Point3>& polygon) { return polygon.size() < 3 ? 0.0 : length(geom::newell(polygon)) * 0.5; }

Point3 polygonCentroid(const std::vector<Point3>& polygon) {
    if (polygon.empty()) return {};
    const auto f = panelFrame(polygon);
    if (f) {
        const std::vector<Point2> l = polygonLocal(*f, polygon);
        double a = 0.0, cx = 0.0, cy = 0.0;
        for (std::size_t i = 0; i < l.size(); ++i) {
            const Point2 &p = l[i], &q = l[(i + 1) % l.size()];
            const double c = cross2(p, q);
            a += c;
            cx += (p.x + q.x) * c;
            cy += (p.y + q.y) * c;
        }
        if (std::fabs(a) > 1e-12) return f->toWorld({cx / (3.0 * a), cy / (3.0 * a)});
    }
    Point3 m;
    for (const Point3& p : polygon) m = add(m, p);
    return scale(m, 1.0 / static_cast<double>(polygon.size()));
}

bool pointInsidePolygon(const std::vector<Point3>& polygon, const Point3& p, double toleranceM) {
    const auto f = panelFrame(polygon);
    if (!f || std::fabs(f->heightOf(p)) > kPanelPlaneToleranceM) return false;
    return pointInPolygon2D(f->toLocal(p), polygonLocal(*f, polygon), toleranceM);
}

ContourError validatePanelContour(const std::vector<Point3>& points) {
    const std::size_t n = points.size();
    if (n < 3) return ContourError::TooFewPoints;
    for (const Point3& p : points)
        if (!finite(p)) return ContourError::Degenerate;
    for (std::size_t i = 0; i < n; ++i)
        for (std::size_t j = i + 1; j < n; ++j)
            if (distPointToPlanePoint(points[i], points[j]) < kPanelPointToleranceM) return ContourError::CoincidentPoints;
    const Point3 normal = geom::newell(points);
    if (length(normal) * 0.5 < kPanelMinAreaM2) return ContourError::Degenerate;
    // Planarity: every point within the tolerance of the plane through the mean point with the normal of the polygon.
    Point3 mean;
    for (const Point3& p : points) mean = add(mean, p);
    mean = scale(mean, 1.0 / static_cast<double>(n));
    const Point3 w = unit(normal);
    for (const Point3& p : points)
        if (std::fabs(dot(sub(p, mean), w)) > kPanelPlaneToleranceM) return ContourError::NotPlanar;
    const auto f = panelFrame(points);
    if (!f) return ContourError::Degenerate;
    const std::vector<Point2> l = polygonLocal(*f, points);
    for (std::size_t i = 0; i < n; ++i) {
        const Point2 &a = l[i], &b = l[(i + 1) % n];
        for (std::size_t j = i + 1; j < n; ++j) {
            if (j == i) continue;
            const bool adjacent = j == i + 1 || (i == 0 && j == n - 1);
            const Point2 &c = l[j], &d = l[(j + 1) % n];
            if (adjacent) {
                // Two consecutive edges folded back on each other (a spike) overlap along a line.
                const Point2 shared = (j == i + 1) ? b : a;
                const Point2 e1 = (j == i + 1) ? sub2(a, shared) : sub2(b, shared);
                const Point2 e2 = (j == i + 1) ? sub2(d, shared) : sub2(c, shared);
                const double l1 = std::hypot(e1.x, e1.y), l2 = std::hypot(e2.x, e2.y);
                if (l1 > 0 && l2 > 0 && std::fabs(cross2(e1, e2)) / (l1 * l2) < 1e-9 && dot2(e1, e2) > 0) return ContourError::SelfIntersecting;
                continue;
            }
            if (distSegmentSegment2(a, b, c, d) <= kPanelPointToleranceM) return ContourError::SelfIntersecting;
        }
    }
    return ContourError::None;
}

std::string panelContourMessage(bool forSurfaceLoad, bool indicated) {
    const std::string head = indicated ? "Los puntos indicados no permiten definir correctamente " : "Los puntos introducidos no permiten definir correctamente ";
    return head + (forSurfaceLoad ? "el contorno de una carga superficial." : "un paño.");
}

// --- queries ---

const Panel* findPanel(const Panels& panels, int id) {
    for (const Panel& p : panels.items)
        if (p.id == id) return &p;
    return nullptr;
}
Panel* findPanel(Panels& panels, int id) {
    for (Panel& p : panels.items)
        if (p.id == id) return &p;
    return nullptr;
}
const SurfaceLoad* findSurfaceLoad(const SurfaceLoads& loads, int id) {
    for (const SurfaceLoad& l : loads.items)
        if (l.id == id) return &l;
    return nullptr;
}
std::vector<int> surfaceLoadsOfCase(const SurfaceLoads& loads, int loadCaseId) {
    std::vector<int> out;
    for (const SurfaceLoad& l : loads.items)
        if (l.data.loadCaseId == loadCaseId) out.push_back(l.id);
    return out;
}
std::vector<int> panelsWithCase(const Panels& panels, int loadCaseId) {
    std::vector<int> out;
    for (const Panel& p : panels.items)
        if (std::any_of(p.loads.begin(), p.loads.end(), [&](const SurfaceLoadData& d) { return d.loadCaseId == loadCaseId; })) out.push_back(p.id);
    return out;
}

// --- direction ---

Point3 panelDirectionFromSide(const std::vector<Point3>& polygon, int side, PanelDirectionMode mode) {
    const auto f = panelFrame(polygon);
    if (!f || side < 0 || static_cast<std::size_t>(side) >= polygon.size()) return {};
    Point3 e = sub(polygon[(static_cast<std::size_t>(side) + 1) % polygon.size()], polygon[static_cast<std::size_t>(side)]);
    e = unit(sub(e, scale(f->w, dot(e, f->w))));
    if (nullVector(e)) return {};
    return mode == PanelDirectionMode::Parallel ? e : unit(cross(f->w, e));
}

std::vector<std::pair<Point3, Point3>> panelSideLines(const std::vector<Point3>& polygon) {
    std::vector<std::pair<Point3, Point3>> out;
    for (std::size_t i = 0; i < polygon.size(); ++i) out.emplace_back(polygon[i], polygon[(i + 1) % polygon.size()]);
    return out;
}

Point3 panelDirectionInPlane(const std::vector<Point3>& polygon, const Point3& direction) {
    const auto f = panelFrame(polygon);
    if (!f) return {};
    Point3 d = finite(direction) ? sub(direction, scale(f->w, dot(direction, f->w))) : Point3{};
    if (length(d) < 1e-9) return f->u;
    return unit(d);
}

PanelArrow panelArrow(const Panel& panel) {
    PanelArrow a;
    if (nullVector(panel.direction) || !panelFrame(panel.vertices)) return a;
    const Point3 c = polygonCentroid(panel.vertices), d = unit(panel.direction);
    double lo = 0.0, hi = 0.0;
    for (const Point3& p : panel.vertices) {
        const double t = dot(sub(p, c), d);
        lo = std::min(lo, t);
        hi = std::max(hi, t);
    }
    const double len = std::max(0.2, 0.5 * (hi - lo));
    a.from = sub(c, scale(d, 0.5 * len));
    a.to = add(c, scale(d, 0.5 * len));
    a.valid = true;
    return a;
}

// --- sense and colours ---

bool isWindCase(const LoadCaseSet& cases, int loadCaseId) {
    const LoadCase* c = findLoadCase(cases, loadCaseId);
    return c && c->nature == LoadNature::Wind;
}

Point3 panelPositiveDirection(const std::vector<Point3>& polygon, bool wind) {
    if (!wind) return {0.0, 0.0, -1.0};
    const auto f = panelFrame(polygon);
    if (!f) return {0.0, 0.0, 1.0};
    Point3 w = f->w;
    if (w.z < -1e-9) w = scale(w, -1.0);
    return w;
}

std::string panelPositiveSenseText(const std::vector<Point3>& polygon, bool wind) {
    const Point3 d = panelPositiveDirection(polygon, wind);
    if (std::fabs(d.x) < 1e-6 && std::fabs(d.y) < 1e-6) return d.z < 0 ? "Vertical hacia abajo" : "Vertical hacia arriba";
    return "(" + trimmedText(d.x, 3) + ", " + trimmedText(d.y, 3) + ", " + trimmedText(d.z, 3) + ")";
}

Point3 panelLoadActionDirection(const std::vector<Point3>& polygon, bool wind, double value) {
    if (value == 0.0 || !std::isfinite(value)) return {};
    return scale(panelPositiveDirection(polygon, wind), value < 0 ? -1.0 : 1.0);
}

int panelLoadColourSlot(const LoadCaseSet& cases, int loadCaseId) {
    if (loadCaseId == 0) return 0;
    int position = 0;
    for (const LoadCase& c : cases.cases) {
        if (c.id == 0) continue;
        ++position;
        if (c.id == loadCaseId) return ((position - 1) % 2) + 1;
    }
    return 0;
}

unsigned panelLoadSlotRgb(int slot) {
    switch (slot) {
        case 1: return 0x00BFFF;
        case 2: return 0x00FF00;
        default: return 0xFFFF00;
    }
}

// --- drawing ---

double panelLoadDrawSizeM(const LoadScales& scales, int loadCaseId, double value, double maxValueAbs) {
    const double v = std::fabs(value);
    if (scales.mode == LoadScaleMode::Fixed) return maxValueAbs > 0.0 ? scales.fixedSizeM * v / maxValueAbs : scales.fixedSizeM;
    return kLoadMetresPerKn * v * loadScaleOfCase(scales, loadCaseId) * scales.perType[static_cast<std::size_t>(LoadScaleType::Surface)];
}

namespace {
double dataMaxAbs(const SurfaceLoadData& d) { return d.type == SurfaceLoadType::Uniform ? std::fabs(d.value) : std::max(std::fabs(d.lower), std::fabs(d.upper)); }
double dominantValue(const SurfaceLoadData& d) {
    if (d.type == SurfaceLoadType::Uniform) return d.value;
    return std::fabs(d.upper) >= std::fabs(d.lower) ? d.upper : d.lower;
}
}  // namespace

double panelLoadMaxSizeM(const LoadScales& scales, const SurfaceLoadData& data) {
    const double m = dataMaxAbs(data);
    return panelLoadDrawSizeM(scales, data.loadCaseId, m, m);
}

std::vector<double> panelLoadStackOffsets(const std::vector<PanelLoadView>& loads, const LoadScales& scales) {
    std::vector<double> out(loads.size(), 0.0);
    const auto side = [](const PanelLoadView& l) { return scale(l.positiveDirection, dominantValue(l.data) < 0 ? 1.0 : -1.0); };
    for (std::size_t i = 0; i < loads.size(); ++i)
        for (std::size_t j = 0; j < i; ++j)
            if (length(sub(side(loads[j]), side(loads[i]))) < 1e-6) out[i] += panelLoadMaxSizeM(scales, loads[j].data);
    return out;
}

std::string panelLoadLabelText(double value) { return trimmedText(value, kSurfaceLoadDecimals) + " kN/m²"; }

double surfaceLoadValueAt(const SurfaceLoadData& d, const std::vector<Point3>& polygon, const Point3& p) {
    if (d.type == SurfaceLoadType::Uniform) return d.value;
    if (polygon.empty()) return d.lower;
    double zMin = polygon.front().z, zMax = polygon.front().z;
    for (const Point3& q : polygon) {
        zMin = std::min(zMin, q.z);
        zMax = std::max(zMax, q.z);
    }
    const double span = zMax - zMin;
    if (span < 1e-9) return d.lower;
    const double t = std::clamp((p.z - zMin) / span, 0.0, 1.0);
    return d.lower + (d.upper - d.lower) * t;
}

PanelLoadDrawing panelLoadDrawing(const PanelLoadView& load, const std::vector<Point3>& polygon, const LoadScales& scales, double stackOffsetM) {
    PanelLoadDrawing out;
    const std::size_t n = polygon.size();
    if (n < 3 || nullVector(load.positiveDirection)) return out;
    const Point3 pos = unit(load.positiveDirection);
    std::vector<double> values(n);
    double maxAbs = 0.0;
    for (std::size_t i = 0; i < n; ++i) {
        values[i] = surfaceLoadValueAt(load.data, polygon, polygon[i]);
        maxAbs = std::max(maxAbs, std::fabs(values[i]));
    }
    if (maxAbs <= 0.0) return out;
    // The prism lies on the side from which the load comes: opposite to the action direction (positive vector times the sign of the dominant value).
    const Point3 side = scale(pos, dominantValue(load.data) < 0 ? 1.0 : -1.0);
    for (std::size_t i = 0; i < n; ++i) {
        const double signedHeight = (values[i] < 0 ? -1.0 : 1.0) * panelLoadDrawSizeM(scales, load.data.loadCaseId, values[i], maxAbs);
        out.sizeM = std::max(out.sizeM, std::fabs(signedHeight));
        const Point3 base = add(polygon[i], scale(side, stackOffsetM));
        out.base.push_back(base);
        out.top.push_back(add(base, scale(scale(pos, -1.0), signedHeight)));
    }
    for (std::size_t i = 0; i < n; ++i) {
        const std::size_t j = (i + 1) % n;
        const double len = length(sub(out.base[j], out.base[i]));
        const int steps = std::max(1, static_cast<int>(std::ceil(len / kPanelTickSpacingM - 1e-9)));
        for (int k = 0; k < steps; ++k) {
            const double t = static_cast<double>(k) / steps;
            out.ticks.push_back({add(out.base[i], scale(sub(out.base[j], out.base[i]), t)), add(out.top[i], scale(sub(out.top[j], out.top[i]), t))});
        }
    }
    const auto mid = [&](std::size_t i) { return scale(add(out.base[i], out.top[i]), 0.5); };
    if (load.data.type == SurfaceLoadType::Variable)
        for (std::size_t i = 0; i < n; ++i) out.labels.push_back({mid(i), panelLoadLabelText(values[i])});
    else
        out.labels.push_back({mid(0), panelLoadLabelText(values[0])});
    out.valid = true;
    return out;
}

// --- containment ---

bool panelContainsPolygon(const std::vector<Point3>& outer, const std::vector<Point3>& inner) {
    const auto f = panelFrame(outer);
    if (!f || inner.size() < 3) return false;
    const std::vector<Point2> poly = polygonLocal(*f, outer);
    const auto inside = [&](const Point3& p) { return std::fabs(f->heightOf(p)) <= kPanelPlaneToleranceM && pointInPolygon2D(f->toLocal(p), poly, kPanelGeomToleranceM); };
    for (std::size_t i = 0; i < inner.size(); ++i) {
        if (!inside(inner[i])) return false;
        if (!inside(scale(add(inner[i], inner[(i + 1) % inner.size()]), 0.5))) return false;
    }
    return true;
}

std::vector<int> panelsContaining(const Panels& panels, const std::vector<Point3>& polygon) {
    std::vector<int> out;
    for (const Panel& p : panels.items)
        if (panelContainsPolygon(p.vertices, polygon)) out.push_back(p.id);
    return out;
}

std::string surfaceLoadSeveralPanelsMessage() { return "La carga superficial se solapa con varios paños."; }
std::string surfaceLoadNotContainedMessage() { return "La superficie de la carga debe estar contenida completamente en un paño."; }
std::string panelWarningNoBarsMessage(bool withShells) {
    return withShells ? "Se han encontrado paños sin barras ni láminas asociadas." : "Se han encontrado paños sin ninguna barra asociada.";
}
std::string panelWarningOverlapMessage() { return "Existen paños solapados."; }
std::string panelWarningNotCoveredByShellsMessage() { return "El paño no está cubierto completamente por láminas."; }
std::string panelWarningNotAssignedMessage() { return "No se han podido asignar totalmente las cargas superficiales del paño."; }

// --- edits on the data ---

std::vector<int> applyPanelsDeleted(Panels& panels, SurfaceLoads& loads, const std::vector<int>& panelIds) {
    const std::set<int> gone(panelIds.begin(), panelIds.end());
    Panels remaining;
    std::vector<std::vector<Point3>> deletedShapes;
    for (const Panel& p : panels.items) {
        if (gone.count(p.id)) deletedShapes.push_back(p.vertices);
        else remaining.items.push_back(p);
    }
    std::vector<int> removed;
    std::vector<SurfaceLoad> kept;
    for (const SurfaceLoad& l : loads.items) {
        const bool inDeleted = std::any_of(deletedShapes.begin(), deletedShapes.end(), [&](const auto& shape) { return panelContainsPolygon(shape, l.contour); });
        const bool inRemaining = !panelsContaining(remaining, l.contour).empty();
        if (inDeleted && !inRemaining) removed.push_back(l.id);
        else kept.push_back(l);
    }
    loads.items = std::move(kept);
    panels.items = std::move(remaining.items);
    return removed;
}

void applyPanelsCaseDeleted(Panels& panels, SurfaceLoads& loads, int loadCaseId) {
    loads.items.erase(std::remove_if(loads.items.begin(), loads.items.end(), [&](const SurfaceLoad& l) { return l.data.loadCaseId == loadCaseId; }), loads.items.end());
    for (Panel& p : panels.items)
        p.loads.erase(std::remove_if(p.loads.begin(), p.loads.end(), [&](const SurfaceLoadData& d) { return d.loadCaseId == loadCaseId; }), p.loads.end());
}

namespace {

struct NodeFollow {
    Point3 from;
    std::optional<Point3> to;  ///< nullopt = the node vanished
};

enum class FollowResult { Unchanged, Changed, Gone };

FollowResult followNodes(std::vector<Point3>& points, const std::vector<NodeFollow>& follow) {
    bool changed = false, gone = false;
    for (Point3& p : points)
        for (const NodeFollow& f : follow)
            if (distPointToPlanePoint(f.from, p) <= kPanelPointToleranceM) {
                if (f.to) {
                    p = *f.to;
                    changed = true;
                } else {
                    gone = true;
                }
                break;
            }
    if (gone) return FollowResult::Gone;
    if (!changed) return FollowResult::Unchanged;
    std::vector<Point3> fused;
    for (const Point3& p : points)
        if (fused.empty() || distPointToPlanePoint(fused.back(), p) > kPanelPointToleranceM) fused.push_back(p);
    while (fused.size() > 1 && distPointToPlanePoint(fused.front(), fused.back()) <= kPanelPointToleranceM) fused.pop_back();
    points = std::move(fused);
    return points.size() < 3 ? FollowResult::Gone : FollowResult::Changed;
}

bool onSegment(const Point3& p, const Point3& a, const Point3& b) {
    const Point3 ab = sub(b, a);
    const double l2 = dot(ab, ab);
    if (l2 < 1e-24) return false;
    const double t = std::clamp(dot(sub(p, a), ab) / l2, 0.0, 1.0);
    return length(sub(p, add(a, scale(ab, t)))) <= kPanelPointToleranceM;
}

}  // namespace

void reconcilePanels(Panels& panels, SurfaceLoads& loads, const StructuralModel& before, const StructuralModel& after, const std::map<int, int>& nodeRemap,
                     PanelReconcileInfo* info) {
    if (panels.items.empty() && loads.items.empty() && loads.unassignedBars.empty()) return;
    PanelReconcileInfo local;
    PanelReconcileInfo& stats = info ? *info : local;

    // Assignment of bars: a new bar lying on a bar that was switched off inherits it; bars that vanished leave the list.
    if (!loads.unassignedBars.empty()) {
        std::set<int> next = loads.unassignedBars;
        for (const Bar& nb : after.bars) {
            if (before.findBar(nb.id)) continue;
            const Node *na = after.findNode(nb.startNode), *nb2 = after.findNode(nb.endNode);
            if (!na || !nb2) continue;
            for (int id : loads.unassignedBars) {
                const Bar* old = before.findBar(id);
                if (!old) continue;
                const Node *oa = before.findNode(old->startNode), *ob = before.findNode(old->endNode);
                if (oa && ob && onSegment(na->position, oa->position, ob->position) && onSegment(nb2->position, oa->position, ob->position)) {
                    next.insert(nb.id);
                    break;
                }
            }
        }
        for (auto it = next.begin(); it != next.end();) it = after.findBar(*it) ? std::next(it) : next.erase(it);
        loads.unassignedBars = std::move(next);
    }

    std::vector<NodeFollow> follow;
    for (const Node& n : before.nodes) {
        if (const Node* a = after.findNode(n.id)) {
            if (distance(a->position, n.position) > 1e-9) follow.push_back({n.position, a->position});
            continue;
        }
        const auto it = nodeRemap.find(n.id);
        const Node* survivor = it == nodeRemap.end() ? nullptr : after.findNode(it->second);
        follow.push_back({n.position, survivor ? std::optional<Point3>(survivor->position) : std::nullopt});
    }
    if (follow.empty()) return;

    // 1. Panels that vanish take their surface loads (rule of applyPanelsDeleted, on the old geometry).
    std::vector<int> goneIds;
    for (const Panel& p : panels.items) {
        std::vector<Point3> copy = p.vertices;
        if (followNodes(copy, follow) == FollowResult::Gone) goneIds.push_back(p.id);
    }
    if (!goneIds.empty()) {
        stats.panelsRemoved += static_cast<int>(goneIds.size());
        stats.surfaceLoadsRemoved += static_cast<int>(applyPanelsDeleted(panels, loads, goneIds).size());
    }
    // 2. The rest follow their nodes.
    for (Panel& p : panels.items)
        if (followNodes(p.vertices, follow) == FollowResult::Changed) p.direction = panelDirectionInPlane(p.vertices, p.direction);
    std::vector<SurfaceLoad> kept;
    for (SurfaceLoad& l : loads.items) {
        if (followNodes(l.contour, follow) == FollowResult::Gone) {
            ++stats.surfaceLoadsRemoved;
            continue;
        }
        kept.push_back(std::move(l));
    }
    loads.items = std::move(kept);
}

// --- validation ---

namespace {

std::string validateRow(const SurfaceLoadData& d, const LoadCaseSet& cases, bool horizontal, const std::string& name) {
    if (!findLoadCase(cases, d.loadCaseId)) return name + " en una hipótesis inexistente.";
    if (normaliseSurfaceLoadData(d) != d) return name + " con datos no canónicos.";
    if (!checkSurfaceLoadData(d).ok()) return name + " con datos no válidos.";
    if (horizontal && d.type == SurfaceLoadType::Variable) return name + " variable sobre una superficie horizontal.";
    return {};
}

}  // namespace

std::string validatePanels(const Panels& panels, const LoadCaseSet& cases) {
    std::set<int> ids;
    for (const Panel& p : panels.items) {
        const std::string name = "Paño " + std::to_string(p.id);
        if (p.id < 1 || p.id >= panels.nextId) return "Paño con identificador no válido.";
        if (!ids.insert(p.id).second) return "Paño con identificador repetido.";
        if (validatePanelContour(p.vertices) != ContourError::None) return name + " con contorno no válido.";
        const auto f = panelFrame(p.vertices);
        if (!f) return name + " sin plano.";
        if (nullVector(p.direction) || std::fabs(length(p.direction) - 1.0) > 1e-6 || std::fabs(dot(p.direction, f->w)) > 1e-6) return name + " con dirección de reparto no válida.";
        const bool horizontal = isHorizontalPlane(*f);
        for (const SurfaceLoadData& d : p.loads)
            if (std::string e = validateRow(d, cases, horizontal, "Carga del " + name); !e.empty()) return e;
    }
    return {};
}

std::string validateSurfaceLoads(const SurfaceLoads& loads, const LoadCaseSet& cases) {
    std::set<int> ids;
    for (const SurfaceLoad& l : loads.items) {
        const std::string name = "Carga superficial " + std::to_string(l.id);
        if (l.id < 1 || l.id >= loads.nextId) return "Carga superficial con identificador no válido.";
        if (!ids.insert(l.id).second) return "Carga superficial con identificador repetido.";
        if (validatePanelContour(l.contour) != ContourError::None) return name + " con contorno no válido.";
        if (std::string e = validateRow(l.data, cases, isHorizontalPolygon(l.contour), name); !e.empty()) return e;
    }
    for (int id : loads.unassignedBars)
        if (id < 1) return "Asignación de cargas superficiales en una barra no válida.";
    return {};
}

}  // namespace cc::core
