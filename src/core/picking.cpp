#include "picking.h"

#include <algorithm>
#include <cmath>

namespace cc::core {

namespace {
struct P2 {
    double x = 0, y = 0;
};
struct Rect {
    double l, t, r, b;
};

bool inside(const Rect& r, const P2& p) { return p.x >= r.l && p.x <= r.r && p.y >= r.t && p.y <= r.b; }

double distanceToSegment(const P2& p, const P2& a, const P2& b) {
    const double dx = b.x - a.x, dy = b.y - a.y;
    const double len2 = dx * dx + dy * dy;
    double t = len2 > 0 ? ((p.x - a.x) * dx + (p.y - a.y) * dy) / len2 : 0.0;
    t = std::clamp(t, 0.0, 1.0);
    return std::hypot(p.x - (a.x + t * dx), p.y - (a.y + t * dy));
}

// Liang-Barsky clipping: does the segment touch the rectangle?
bool segmentTouches(const Rect& r, const P2& a, const P2& b) {
    double t0 = 0.0, t1 = 1.0;
    const double dx = b.x - a.x, dy = b.y - a.y;
    const double p[4] = {-dx, dx, -dy, dy};
    const double q[4] = {a.x - r.l, r.r - a.x, a.y - r.t, r.b - a.y};
    for (int i = 0; i < 4; ++i) {
        if (p[i] == 0.0) {
            if (q[i] < 0.0) return false;
        } else {
            const double t = q[i] / p[i];
            if (p[i] < 0.0) t0 = std::max(t0, t);
            else t1 = std::min(t1, t);
            if (t0 > t1) return false;
        }
    }
    return true;
}

bool pointInPolygon(const P2& p, const std::vector<P2>& poly) {
    bool in = false;
    for (std::size_t i = 0, j = poly.size() - 1; i < poly.size(); j = i++) {
        if ((poly[i].y > p.y) != (poly[j].y > p.y) &&
            p.x < (poly[j].x - poly[i].x) * (p.y - poly[i].y) / (poly[j].y - poly[i].y) + poly[i].x)
            in = !in;
    }
    return in;
}

bool project(const ScreenMapper& map, const Point3& w, P2& out) { return map.toScreen(w, out.x, out.y); }

bool shellPolygon(const StructuralModel& model, const ScreenMapper& map, const Shell& shell, std::vector<P2>& poly) {
    poly.clear();
    for (int id : shell.nodeIds) {
        const Node* n = model.findNode(id);
        P2 p;
        if (!n || !project(map, n->position, p)) return false;
        poly.push_back(p);
    }
    return poly.size() >= 3;
}

bool barEnds(const StructuralModel& model, const ScreenMapper& map, const Bar& bar, P2& a, P2& b) {
    const Node* na = model.findNode(bar.startNode);
    const Node* nb = model.findNode(bar.endNode);
    return na && nb && project(map, na->position, a) && project(map, nb->position, b);
}
}  // namespace

bool ScreenMapper::toScreen(const Vec3& world, double& x, double& y) const {
    if (!camera || width <= 0.0 || height <= 0.0) return false;
    Vec3 ndc;
    if (!camera->project(world, width / height, ndc)) return false;
    x = (ndc.x + 1.0) * 0.5 * width;
    y = (1.0 - ndc.y) * 0.5 * height;
    return true;
}

RectMode rectModeFor(double x1, double x2) { return x2 >= x1 ? RectMode::Window : RectMode::Crossing; }

std::optional<ElementRef> pickAt(const StructuralModel& model, const ScreenMapper& map, const SelectionFilter& filter,
                                 double x, double y, const PickOptions& options) {
    const P2 cursor{x, y};
    if (filter.nodes) {
        double best = options.nodeRadiusPx;
        std::optional<ElementRef> hit;
        for (const Node& n : model.nodes) {
            P2 p;
            if (!project(map, n.position, p)) continue;
            const double d = std::max(std::abs(p.x - x), std::abs(p.y - y));  // the node is a square: Chebyshev distance
            if (d <= best) {
                best = d;
                hit = ElementRef{ElementKind::Node, n.id};
            }
        }
        if (hit) return hit;
    }
    if (filter.bars) {
        double best = options.barTolerancePx;
        std::optional<ElementRef> hit;
        for (const Bar& b : model.bars) {
            P2 pa, pb;
            if (!barEnds(model, map, b, pa, pb)) continue;
            const double d = distanceToSegment(cursor, pa, pb);
            if (d <= best) {
                best = d;
                hit = ElementRef{ElementKind::Bar, b.id};
            }
        }
        if (hit) return hit;
    }
    if (filter.shells) {
        std::vector<P2> poly;
        for (const Shell& s : model.shells)
            if (shellPolygon(model, map, s, poly) && pointInPolygon(cursor, poly)) return ElementRef{ElementKind::Shell, s.id};
    }
    return std::nullopt;
}

std::vector<ElementRef> pickRect(const StructuralModel& model, const ScreenMapper& map, const SelectionFilter& filter,
                                 double x1, double y1, double x2, double y2) {
    const bool window = rectModeFor(x1, x2) == RectMode::Window;
    const Rect r{std::min(x1, x2), std::min(y1, y2), std::max(x1, x2), std::max(y1, y2)};
    std::vector<ElementRef> hits;
    if (filter.nodes) {
        for (const Node& n : model.nodes) {
            P2 p;
            if (project(map, n.position, p) && inside(r, p)) hits.push_back({ElementKind::Node, n.id});
        }
    }
    if (filter.bars) {
        for (const Bar& b : model.bars) {
            P2 pa, pb;
            if (!barEnds(model, map, b, pa, pb)) continue;
            const bool hit = window ? (inside(r, pa) && inside(r, pb)) : segmentTouches(r, pa, pb);
            if (hit) hits.push_back({ElementKind::Bar, b.id});
        }
    }
    if (filter.shells) {
        std::vector<P2> poly;
        for (const Shell& s : model.shells) {
            if (!shellPolygon(model, map, s, poly)) continue;
            bool hit;
            if (window) {
                hit = std::all_of(poly.begin(), poly.end(), [&](const P2& p) { return inside(r, p); });
            } else {
                hit = std::any_of(poly.begin(), poly.end(), [&](const P2& p) { return inside(r, p); });
                for (std::size_t i = 0; !hit && i < poly.size(); ++i) hit = segmentTouches(r, poly[i], poly[(i + 1) % poly.size()]);
                if (!hit) hit = pointInPolygon({r.l, r.t}, poly);  // the rectangle lies inside the shell
            }
            if (hit) hits.push_back({ElementKind::Shell, s.id});
        }
    }
    return hits;
}

}  // namespace cc::core
