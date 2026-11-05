#pragma once
#include <cmath>
#include <vector>

#include "core/structure_model.h"

namespace cc::core::geom {

/// Small 3D vector helpers on Point3 (metres). Header-only; used by the shell, tools and import modules.
inline Point3 sub(const Point3& a, const Point3& b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
inline Point3 add(const Point3& a, const Point3& b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
inline Point3 scale(const Point3& a, double s) { return {a.x * s, a.y * s, a.z * s}; }
inline double dot(const Point3& a, const Point3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline Point3 cross(const Point3& a, const Point3& b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
inline double length(const Point3& a) { return std::sqrt(dot(a, a)); }
/// Unit vector; the zero vector when `a` is shorter than 1e-12.
inline Point3 unit(const Point3& a) {
    const double l = length(a);
    return l < 1e-12 ? Point3{} : scale(a, 1.0 / l);
}
/// Vector area of a closed polygon (Newell): direction = right-hand normal of the vertex order, length = 2 x area.
inline Point3 newell(const std::vector<Point3>& poly) {
    Point3 n;
    for (std::size_t i = 0; i < poly.size(); ++i) {
        const Point3& a = poly[i];
        const Point3& b = poly[(i + 1) % poly.size()];
        n.x += (a.y - b.y) * (a.z + b.z);
        n.y += (a.z - b.z) * (a.x + b.x);
        n.z += (a.x - b.x) * (a.y + b.y);
    }
    return n;
}

}  // namespace cc::core::geom
