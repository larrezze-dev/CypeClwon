#include "snap.h"

#include <algorithm>
#include <cmath>

namespace cc::core {

bool SnapOptions::active(SnapKind kind) const {
    switch (kind) {
        case SnapKind::None: return false;
        case SnapKind::Endpoint: return enabled ? endpoint : true;
        case SnapKind::Nearest: return enabled ? nearest : true;
        case SnapKind::Extension: return enabled ? extension : true;
        case SnapKind::Midpoint: return enabled && midpoint;
    }
    return false;
}

void SnapOptions::clearAll() {
    point = endpoint = insertion = centre = perpendicular = parallel = false;
    midpoint = intersection = quadrant = nearest = extension = false;
}

namespace {
Vec3 toVec(const Point3& p) { return {p.x, p.y, p.z}; }

// Parameter t of the point O + t*d of the line P + t*d1 closest to the ray (P + t d1 vs O + s d2); false when parallel.
bool closestOnLine(const Vec3& p, const Vec3& d1, const Ray& ray, double& t) {
    const Vec3 w0 = p - ray.origin;
    const double a = dot(d1, d1), b = dot(d1, ray.direction), c = dot(ray.direction, ray.direction);
    const double d = dot(d1, w0), e = dot(ray.direction, w0);
    const double den = a * c - b * b;
    if (std::abs(den) < 1e-12 * std::max(1.0, a * c)) return false;
    t = (b * e - c * d) / den;
    return true;
}

struct Candidate {
    SnapKind kind = SnapKind::None;
    Vec3 point;
    std::optional<ElementRef> element;
    std::optional<int> referenceNode;
    int axis = -1;
    double px = 1e300;
    double tie = 0.0;  // world distance to the reference node: breaks ties between candidates at the same pixel distance
};

void consider(Candidate& best, const Candidate& c, double limit) {
    if (c.px > limit) return;
    if (c.px < best.px - 1e-9 || (std::abs(c.px - best.px) <= 1e-9 && c.tie < best.tie)) best = c;
}
}  // namespace

SnapResult snapCursor(const StructuralModel& model, const ScreenMapper& map, double x, double y,
                      const SnapOptions& options, const SnapParams& params) {
    SnapResult out;
    if (!map.camera || map.width <= 0 || map.height <= 0) return out;
    const auto finish = [&](const Candidate& c) {
        out.kind = c.kind;
        out.point = c.point;
        out.element = c.element;
        out.referenceNode = c.referenceNode;
        out.axis = c.axis;
        out.pixelDistance = c.px;
        if (c.referenceNode) {
            if (const Node* n = model.findNode(*c.referenceNode)) out.referenceDistance = length(c.point - toVec(n->position));
        }
        return out;
    };
    const auto pixels = [&](const Vec3& world, double& d) {
        double sx, sy;
        if (!map.toScreen(world, sx, sy)) return false;
        d = std::hypot(sx - x, sy - y);
        return true;
    };

    // 1. Endpoints: the node closest to the cursor within the node radius.
    if (options.active(SnapKind::Endpoint)) {
        Candidate best;
        for (const Node& n : model.nodes) {
            double d;
            if (pixels(toVec(n.position), d)) consider(best, {SnapKind::Endpoint, toVec(n.position), ElementRef{ElementKind::Node, n.id}, std::nullopt, -1, d}, params.nodeRadiusPx);
        }
        if (best.kind != SnapKind::None) return finish(best);
    }

    // 2. Midpoints (object snap only).
    if (options.active(SnapKind::Midpoint)) {
        Candidate best;
        for (const Bar& b : model.bars) {
            const Node* na = model.findNode(b.startNode);
            const Node* nb = model.findNode(b.endNode);
            if (!na || !nb) continue;
            const Vec3 mid = (toVec(na->position) + toVec(nb->position)) * 0.5;
            double d;
            if (pixels(mid, d)) consider(best, {SnapKind::Midpoint, mid, ElementRef{ElementKind::Bar, b.id}, std::nullopt, -1, d}, params.nodeRadiusPx);
        }
        if (best.kind != SnapKind::None) return finish(best);
    }

    // 3. Nearest point of a bar, or of a guide line through a node: the closest to the cursor wins.
    const double aspect = map.width / map.height;
    const Ray ray = map.camera->ray(2.0 * x / map.width - 1.0, 1.0 - 2.0 * y / map.height, aspect);
    Candidate best;
    if (options.active(SnapKind::Nearest)) {
        for (const Bar& b : model.bars) {
            const Node* na = model.findNode(b.startNode);
            const Node* nb = model.findNode(b.endNode);
            if (!na || !nb) continue;
            const Vec3 a = toVec(na->position), dir = toVec(nb->position) - a;
            double t = 0.0;
            if (!closestOnLine(a, dir, ray, t)) continue;
            t = std::clamp(t, 0.0, 1.0);
            const Vec3 p = a + dir * t;
            double d;
            if (!pixels(p, d)) continue;
            consider(best, {SnapKind::Nearest, p, ElementRef{ElementKind::Bar, b.id}, t < 0.5 ? b.startNode : b.endNode, -1, d}, params.lineRadiusPx);
        }
    }
    if (options.active(SnapKind::Extension)) {
        static const Vec3 axes[3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
        for (const Node& n : model.nodes) {
            const Vec3 origin = toVec(n.position);
            for (int i = 0; i < 3; ++i) {
                double t = 0.0;
                if (!closestOnLine(origin, axes[i], ray, t)) continue;
                const Vec3 p = origin + axes[i] * t;
                double d;
                if (!pixels(p, d)) continue;
                // Ties with a bar lying on the line go to the bar (tie 0); between nodes of the same line, to the nearest one.
                consider(best, {SnapKind::Extension, p, ElementRef{ElementKind::Node, n.id}, n.id, i, d, length(p - origin)}, params.lineRadiusPx);
            }
        }
    }
    if (best.kind != SnapKind::None) return finish(best);
    return out;
}

}  // namespace cc::core
