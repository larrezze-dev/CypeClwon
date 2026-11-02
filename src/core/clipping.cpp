#include "clipping.h"

#include <algorithm>
#include <cmath>

namespace cc::core {

namespace {
constexpr double kPi = 3.14159265358979323846;
double coord(const Vec3& p, int axis) { return axis == 0 ? p.x : (axis == 1 ? p.y : p.z); }
void setCoord(Vec3& p, int axis, double v) { (axis == 0 ? p.x : (axis == 1 ? p.y : p.z)) = v; }

/// Signed distance to the plane, positive on the kept side.
double inside(const ClipHalfSpace& h, const Vec3& p) { return h.offset - dot(h.normal, p); }

Vec3 turnAboutZ(const Vec3& v, double angle) {
    const double c = std::cos(angle), s = std::sin(angle);
    return {c * v.x - s * v.y, s * v.x + c * v.y, v.z};
}
}  // namespace

bool ClipHalfSpace::contains(const Vec3& p, double tolerance) const { return inside(*this, p) >= -tolerance; }

ClipHalfSpace ClipHalfSpace::local(int axis, double value, bool keepBelow, const Vec3& centre, double angleRad) {
    Vec3 e{};
    setCoord(e, axis, 1.0);
    const Vec3 n = turnAboutZ(e, angleRad);  // local axis in world coordinates
    // local coordinate u_axis = n . (p - centre) + centre_axis
    const double offsetBelow = value - coord(centre, axis) + dot(n, centre);
    return keepBelow ? ClipHalfSpace{n, offsetBelow} : ClipHalfSpace{n * -1.0, -offsetBelow};
}

bool ClipSettings::anyActive() const {
    return volume.active || std::any_of(sections.begin(), sections.end(), [](const SectionPlane& s) { return s.active; });
}

Vec3 ClipSettings::toWorld(const Vec3& local) const { return centre + turnAboutZ(local - centre, angleRad); }
Vec3 ClipSettings::toLocal(const Vec3& world) const { return centre + turnAboutZ(world - centre, -angleRad); }
Vec3 ClipSettings::axisDirection(int axis) const {
    Vec3 e{};
    setCoord(e, axis, 1.0);
    return turnAboutZ(e, angleRad);
}

void ClipSettings::rotate(double deltaRad) {
    if (!std::isfinite(deltaRad)) return;
    angleRad = std::remainder(angleRad + deltaRad, 2.0 * kPi);
}

std::vector<ClipHalfSpace> ClipSettings::halfSpaces() const {
    std::vector<ClipHalfSpace> out;
    for (int axis = 0; axis < 3; ++axis) {
        const SectionPlane& s = sections[static_cast<std::size_t>(axis)];
        // The arrow points to the removed side: arrow +1 removes coordinate > position, so keep the lower side.
        if (s.active) out.push_back(ClipHalfSpace::local(axis, s.position, s.arrowSign() > 0, centre, angleRad));
    }
    if (volume.active) {
        for (int axis = 0; axis < 3; ++axis) {
            out.push_back(ClipHalfSpace::local(axis, coord(volume.min, axis), false, centre, angleRad));
            out.push_back(ClipHalfSpace::local(axis, coord(volume.max, axis), true, centre, angleRad));
        }
    }
    return out;
}

void ClipSettings::setSectionActive(int axis, bool on, double defaultPosition, const Vec3& centreIfFirst) {
    if (axis < 0 || axis > 2) return;
    SectionPlane& s = sections[static_cast<std::size_t>(axis)];
    if (on && !s.active) {
        if (!anyActive()) {  // the turn starts from nothing
            centre = centreIfFirst;
            angleRad = 0.0;
        }
        s.position = defaultPosition;  // a plane shown again starts at the middle, not where it was left
    }
    s.active = on;
    if (!on) s.flipped = false;
    if (!anyActive()) angleRad = 0.0;
}

bool ClipSettings::flipSection(int axis) {
    if (axis < 0 || axis > 2 || !sections[static_cast<std::size_t>(axis)].active) return false;
    SectionPlane& s = sections[static_cast<std::size_t>(axis)];
    s.flipped = !s.flipped;
    return true;
}

void ClipSettings::setVolumeActive(bool on, const Vec3& min, const Vec3& max) {
    if (on && !volume.active) {
        if (!anyActive()) {
            centre = (min + max) * 0.5;
            angleRad = 0.0;
        }
        volume.min = min;
        volume.max = max;
    }
    volume.active = on;
    if (!anyActive()) angleRad = 0.0;
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
