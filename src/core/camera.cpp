#include "core/camera.h"

#include <algorithm>
#include <cmath>

namespace cc::core {

namespace {
constexpr double kPi = 3.14159265358979323846;
double rad(double deg) { return deg * kPi / 180.0; }
constexpr double kZoomPerNotch = 0.85;  // each notch brings the camera 15 % closer
}  // namespace

Vec3 operator+(const Vec3& a, const Vec3& b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
Vec3 operator-(const Vec3& a, const Vec3& b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
Vec3 operator*(const Vec3& a, double s) { return {a.x * s, a.y * s, a.z * s}; }
double dot(const Vec3& a, const Vec3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
Vec3 cross(const Vec3& a, const Vec3& b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
double length(const Vec3& a) { return std::sqrt(dot(a, a)); }
Vec3 normalised(const Vec3& a) {
    const double l = length(a);
    return l > 0.0 ? a * (1.0 / l) : Vec3{};
}

Mat4 operator*(const Mat4& a, const Mat4& b) {
    Mat4 r;
    for (int row = 0; row < 4; ++row)
        for (int col = 0; col < 4; ++col) {
            double sum = 0.0;
            for (int k = 0; k < 4; ++k) sum += a.at(row, k) * b.at(k, col);
            r.at(row, col) = sum;
        }
    return r;
}

Vec3 Camera::position() const {
    const double y = rad(yawDeg), p = rad(pitchDeg);
    return target + Vec3{std::sin(y) * std::cos(p), -std::cos(y) * std::cos(p), std::sin(p)} * distance;
}

Vec3 Camera::forward() const { return normalised(target - position()); }
Vec3 Camera::right() const { return {std::cos(rad(yawDeg)), std::sin(rad(yawDeg)), 0.0}; }
Vec3 Camera::up() const { return normalised(cross(right(), forward())); }

void Camera::orbit(double dxDeg, double dyDeg) {
    yawDeg = std::fmod(yawDeg - dxDeg, 360.0);
    pitchDeg = std::clamp(pitchDeg + dyDeg, -90.0, 90.0);
}

double Camera::halfHeight() const { return distance * std::tan(rad(fovYDeg) / 2.0); }
double Camera::worldPerPixel(double viewportHeightPx) const { return viewportHeightPx > 0 ? 2.0 * halfHeight() / viewportHeightPx : 0.0; }

void Camera::pan(double dxPx, double dyPx, double viewportHeightPx) {
    const double s = worldPerPixel(viewportHeightPx);
    target = target - right() * (dxPx * s) + up() * (dyPx * s);
}

void Camera::zoom(double notches) { distance = std::clamp(distance * std::pow(kZoomPerNotch, notches), kMinDistance, kMaxDistance); }

void Camera::zoomAt(double notches, double ndcX, double ndcY, double aspect) {
    const Vec3 before = pointOnTargetPlane(ndcX, ndcY, aspect);
    zoom(notches);
    const Vec3 after = pointOnTargetPlane(ndcX, ndcY, aspect);
    target = target + (before - after);
}

void Camera::setStandardView(StandardView view) {
    switch (view) {
        case StandardView::Top: yawDeg = 0; pitchDeg = 90; break;
        case StandardView::Bottom: yawDeg = 0; pitchDeg = -90; break;
        case StandardView::Front: yawDeg = 0; pitchDeg = 0; break;
        case StandardView::Back: yawDeg = 180; pitchDeg = 0; break;
        case StandardView::Right: yawDeg = 90; pitchDeg = 0; break;
        case StandardView::Left: yawDeg = -90; pitchDeg = 0; break;
        case StandardView::Isometric: yawDeg = 45; pitchDeg = 30; break;
    }
}

void Camera::frameBox(const Vec3& min, const Vec3& max, double aspect) {
    target = (min + max) * 0.5;
    const double radius = length(max - min) * 0.5;
    if (radius <= 0.0) {
        distance = 1.0;
        return;
    }
    const double t = std::tan(rad(fovYDeg) / 2.0);
    // Fit the bounding sphere in the narrower dimension of the viewport.
    const double limiting = aspect >= 1.0 ? t : t * std::max(aspect, 1e-6);
    distance = std::clamp(radius / limiting * 1.05, kMinDistance, kMaxDistance);
}

double Camera::nearPlane() const { return std::max(distance * 0.01, 1e-4); }
double Camera::farPlane() const { return distance * 1000.0; }

Mat4 Camera::viewMatrix() const {
    const Vec3 f = forward(), r = right(), u = up(), eye = position();
    Mat4 v;
    v.at(0, 0) = r.x; v.at(0, 1) = r.y; v.at(0, 2) = r.z; v.at(0, 3) = -dot(r, eye);
    v.at(1, 0) = u.x; v.at(1, 1) = u.y; v.at(1, 2) = u.z; v.at(1, 3) = -dot(u, eye);
    v.at(2, 0) = -f.x; v.at(2, 1) = -f.y; v.at(2, 2) = -f.z; v.at(2, 3) = dot(f, eye);
    return v;
}

Mat4 Camera::projectionMatrix(double aspect) const {
    const double n = nearPlane(), f = farPlane();
    Mat4 p;
    p.m.fill(0.0);
    const double hh = halfHeight(), hw = hh * aspect;
    if (projection == Projection::Perspective) {
        const double th = std::tan(rad(fovYDeg) / 2.0);
        p.at(0, 0) = 1.0 / (th * aspect);
        p.at(1, 1) = 1.0 / th;
        p.at(2, 2) = -(f + n) / (f - n);
        p.at(2, 3) = -2.0 * f * n / (f - n);
        p.at(3, 2) = -1.0;
    } else {
        p.at(0, 0) = 1.0 / hw;
        p.at(1, 1) = 1.0 / hh;
        p.at(2, 2) = -2.0 / (f - n);
        p.at(2, 3) = -(f + n) / (f - n);
        p.at(3, 3) = 1.0;
    }
    return p;
}

bool Camera::project(const Vec3& world, double aspect, Vec3& ndc) const {
    const Mat4 vp = viewProjection(aspect);
    const double in[4] = {world.x, world.y, world.z, 1.0};
    double out[4];
    for (int r = 0; r < 4; ++r) out[r] = vp.at(r, 0) * in[0] + vp.at(r, 1) * in[1] + vp.at(r, 2) * in[2] + vp.at(r, 3) * in[3];
    if (out[3] <= 0.0) return false;  // behind the eye (orthographic w is always 1)
    ndc = {out[0] / out[3], out[1] / out[3], out[2] / out[3]};
    return true;
}

Ray Camera::ray(double ndcX, double ndcY, double aspect) const {
    const Vec3 f = forward(), r = right(), u = up();
    const double hh = halfHeight(), hw = hh * aspect;
    if (projection == Projection::Orthographic) return {position() + r * (ndcX * hw) + u * (ndcY * hh), f};
    const double th = std::tan(rad(fovYDeg) / 2.0);
    return {position(), normalised(f + r * (ndcX * th * aspect) + u * (ndcY * th))};
}

Vec3 Camera::pointOnTargetPlane(double ndcX, double ndcY, double aspect) const {
    const Ray rr = ray(ndcX, ndcY, aspect);
    const Vec3 f = forward();
    const double denom = dot(rr.direction, f);
    if (std::fabs(denom) < 1e-12) return target;
    return rr.origin + rr.direction * (dot(target - rr.origin, f) / denom);
}

}  // namespace cc::core
