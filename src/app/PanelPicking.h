#pragma once
#include <QPointF>
#include <QPolygonF>
#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <optional>
#include <vector>

#include "core/panel.h"

namespace cc::app::picking {

/// Hit-testing of panels, surface loads, sides and vertices on the screen (T-P7-05). Pure functions: the projection of a world point to the widget is
/// given by the caller (Viewport3D::worldToScreen in the program, a plain matrix in the tests). The selection of panels and surface loads lives in the
/// tools (it is not an ElementKind of the structure): measured, every LMB over the element toggles its selection.
using Projector = std::function<bool(const cc::core::Point3&, QPointF&)>;

inline constexpr double kPickTolerancePx = 6.0;

inline double distanceToSegment(const QPointF& p, const QPointF& a, const QPointF& b) {
    const QPointF ab = b - a;
    const double len2 = ab.x() * ab.x() + ab.y() * ab.y();
    if (len2 < 1e-12) return std::hypot(p.x() - a.x(), p.y() - a.y());
    const double t = std::clamp(((p.x() - a.x()) * ab.x() + (p.y() - a.y()) * ab.y()) / len2, 0.0, 1.0);
    return std::hypot(p.x() - (a.x() + t * ab.x()), p.y() - (a.y() + t * ab.y()));
}

/// Distance to the infinite line through a and b (the auxiliary lines of the direction step are prolonged to the border of the window).
inline double distanceToLine(const QPointF& p, const QPointF& a, const QPointF& b) {
    const QPointF ab = b - a;
    const double len = std::hypot(ab.x(), ab.y());
    if (len < 1e-9) return std::hypot(p.x() - a.x(), p.y() - a.y());
    return std::abs((p.x() - a.x()) * ab.y() - (p.y() - a.y()) * ab.x()) / len;
}

/// Projected polygon; empty when some vertex cannot be seen.
inline QPolygonF projectPolygon(const Projector& project, const std::vector<cc::core::Point3>& polygon) {
    QPolygonF out;
    for (const cc::core::Point3& p : polygon) {
        QPointF s;
        if (!project(p, s)) return {};
        out << s;
    }
    return out;
}

inline double screenArea(const QPolygonF& poly) {
    double a = 0.0;
    for (int i = 0; i < poly.size(); ++i) {
        const QPointF& p = poly[i];
        const QPointF& q = poly[(i + 1) % poly.size()];
        a += p.x() * q.y() - q.x() * p.y();
    }
    return std::abs(a) / 2.0;
}

/// Index of the nearest edge of the polygon within the tolerance (screen pixels), or nullopt.
inline std::optional<int> edgeAt(const QPolygonF& poly, const QPointF& pos, double tolerance = kPickTolerancePx) {
    std::optional<int> best;
    double bestDistance = tolerance;
    for (int i = 0; i < poly.size(); ++i) {
        const double d = distanceToSegment(pos, poly[i], poly[(i + 1) % poly.size()]);
        if (d <= bestDistance) {
            bestDistance = d;
            best = i;
        }
    }
    return best;
}

/// Ids of the polygons hit by a click: when the click is on the border of some of them, ALL those (measured: a click on the common border selected both
/// panels); otherwise the smallest one that contains the point (the topmost of nested polygons). Empty when nothing is hit.
inline std::vector<int> polygonsAt(const std::vector<std::pair<int, QPolygonF>>& polygons, const QPointF& pos, double tolerance = kPickTolerancePx) {
    std::vector<int> onBorder;
    for (const auto& [id, poly] : polygons)
        if (poly.size() >= 3 && edgeAt(poly, pos, tolerance)) onBorder.push_back(id);
    if (!onBorder.empty()) return onBorder;
    std::optional<int> best;
    double bestArea = std::numeric_limits<double>::max();
    for (const auto& [id, poly] : polygons) {
        if (poly.size() < 3 || !poly.containsPoint(pos, Qt::OddEvenFill)) continue;
        const double area = screenArea(poly);
        if (area <= bestArea) {  // equal areas: the later one (created last, drawn on top)
            bestArea = area;
            best = id;
        }
    }
    return best ? std::vector<int>{*best} : std::vector<int>{};
}

/// Index of the vertex of the polygon nearest to the click within the tolerance.
inline std::optional<int> vertexAt(const QPolygonF& poly, const QPointF& pos, double tolerance = 8.0) {
    std::optional<int> best;
    double bestDistance = tolerance;
    for (int i = 0; i < poly.size(); ++i) {
        const double d = std::hypot(pos.x() - poly[i].x(), pos.y() - poly[i].y());
        if (d <= bestDistance) {
            bestDistance = d;
            best = i;
        }
    }
    return best;
}

/// Index of the side whose prolonged line (auxiliary line) is nearest to the click within the tolerance.
inline std::optional<int> sideLineAt(const QPolygonF& poly, const QPointF& pos, double tolerance = kPickTolerancePx) {
    std::optional<int> best;
    double bestDistance = tolerance;
    for (int i = 0; i < poly.size(); ++i) {
        const double d = distanceToLine(pos, poly[i], poly[(i + 1) % poly.size()]);
        if (d <= bestDistance) {
            bestDistance = d;
            best = i;
        }
    }
    return best;
}

/// Endpoints of the auxiliary line through a and b prolonged to `reach` pixels on both sides.
inline std::pair<QPointF, QPointF> prolongedLine(const QPointF& a, const QPointF& b, double reach) {
    const QPointF ab = b - a;
    const double len = std::hypot(ab.x(), ab.y());
    if (len < 1e-9) return {a, b};
    const QPointF u(ab.x() / len, ab.y() / len);
    return {a - u * reach, a + u * reach};
}

}  // namespace cc::app::picking
