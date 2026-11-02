#include "clipping.h"

#include <algorithm>
#include <cmath>

namespace cc::core {

namespace {
double coord(const Vec3& p, int axis) { return axis == 0 ? p.x : (axis == 1 ? p.y : p.z); }
void setCoord(Vec3& p, int axis, double v) { (axis == 0 ? p.x : (axis == 1 ? p.y : p.z)) = v; }

/// Signed distance to the plane, positive on the kept side.
double inside(const ClipHalfSpace& h, const Vec3& p) {
    const double d = coord(p, h.axis) - h.value;
    return h.keepBelow ? -d : d;
}
}  // namespace

bool ClipHalfSpace::contains(const Vec3& p, double tolerance) const { return inside(*this, p) >= -tolerance; }

bool ClipSettings::anyActive() const {
    return volume.active || std::any_of(sections.begin(), sections.end(), [](const SectionPlane& s) { return s.active; });
}

std::vector<ClipHalfSpace> ClipSettings::halfSpaces() const {
    std::vector<ClipHalfSpace> out;
    for (int axis = 0; axis < 3; ++axis) {
        const SectionPlane& s = sections[static_cast<std::size_t>(axis)];
        // The arrow points to the removed side: arrow +1 removes coordinate > position, so keep the lower side.
        if (s.active) out.push_back({axis, s.position, s.arrowSign() > 0});
    }
    if (volume.active) {
        for (int axis = 0; axis < 3; ++axis) {
            out.push_back({axis, coord(volume.min, axis), false});
            out.push_back({axis, coord(volume.max, axis), true});
        }
    }
    return out;
}

void ClipSettings::setSectionActive(int axis, bool on, double defaultPosition) {
    if (axis < 0 || axis > 2) return;
    SectionPlane& s = sections[static_cast<std::size_t>(axis)];
    if (on && !s.active) s.position = defaultPosition;  // a plane shown again starts at the middle, not where it was left
    s.active = on;
    if (!on) s.flipped = false;
}

bool ClipSettings::flipSection(int axis) {
    if (axis < 0 || axis > 2 || !sections[static_cast<std::size_t>(axis)].active) return false;
    SectionPlane& s = sections[static_cast<std::size_t>(axis)];
    s.flipped = !s.flipped;
    return true;
}

void ClipSettings::setVolumeActive(bool on, const Vec3& min, const Vec3& max) {
    if (on && !volume.active) {
        volume.min = min;
        volume.max = max;
    }
    volume.active = on;
}

void ClipSettings::moveVolumeFace(int axis, bool faceMax, double value) {
    if (axis < 0 || axis > 2 || !volume.active || !std::isfinite(value)) return;
    const double lo = coord(volume.min, axis), hi = coord(volume.max, axis);
    if (faceMax)
        setCoord(volume.max, axis, std::max(value, lo));
    else
        setCoord(volume.min, axis, std::min(value, hi));
}

std::optional<std::pair<Vec3, Vec3>> clipSegment(const Vec3& a, const Vec3& b, const std::vector<ClipHalfSpace>& spaces) {
    double t0 = 0.0, t1 = 1.0;
    for (const ClipHalfSpace& h : spaces) {
        const double da = inside(h, a), db = inside(h, b);
        if (da < 0 && db < 0) return std::nullopt;
        if (da >= 0 && db >= 0) continue;
        const double t = da / (da - db);  // crossing parameter
        if (da < 0)
            t0 = std::max(t0, t);
        else
            t1 = std::min(t1, t);
        if (t0 > t1) return std::nullopt;
    }
    const Vec3 d = b - a;
    return std::make_pair(a + d * t0, a + d * t1);
}

std::vector<Vec3> clipPolygon(const std::vector<Vec3>& polygon, const std::vector<ClipHalfSpace>& spaces) {
    std::vector<Vec3> current = polygon;
    for (const ClipHalfSpace& h : spaces) {
        if (current.empty()) break;
        std::vector<Vec3> next;
        for (std::size_t i = 0; i < current.size(); ++i) {
            const Vec3& p = current[i];
            const Vec3& q = current[(i + 1) % current.size()];
            const double dp = inside(h, p), dq = inside(h, q);
            if (dp >= 0) next.push_back(p);
            if ((dp >= 0) != (dq >= 0)) {
                const double t = dp / (dp - dq);
                next.push_back(p + (q - p) * t);
            }
        }
        current = std::move(next);
    }
    if (current.size() < 3) current.clear();
    return current;
}

std::pair<Vec3, Vec3> envelope(const std::vector<Vec3>& points, double margin) {
    if (points.empty()) return {Vec3{-5, -5, -5}, Vec3{5, 5, 5}};
    Vec3 lo = points.front(), hi = points.front();
    for (const Vec3& p : points) {
        lo = {std::min(lo.x, p.x), std::min(lo.y, p.y), std::min(lo.z, p.z)};
        hi = {std::max(hi.x, p.x), std::max(hi.y, p.y), std::max(hi.z, p.z)};
    }
    return {Vec3{lo.x - margin, lo.y - margin, lo.z - margin}, Vec3{hi.x + margin, hi.y + margin, hi.z + margin}};
}

}  // namespace cc::core
