#include <gtest/gtest.h>

#include <cmath>

#include "core/camera.h"

using namespace cc::core;

namespace {
constexpr double kPi = 3.14159265358979323846;
void expectNear(const Vec3& a, const Vec3& b, double tol = 1e-9) {
    EXPECT_NEAR(a.x, b.x, tol);
    EXPECT_NEAR(a.y, b.y, tol);
    EXPECT_NEAR(a.z, b.z, tol);
}
}  // namespace

TEST(Camera, FrontViewLooksAlongPlusYWithXRightAndZUp) {
    Camera c;
    c.setStandardView(StandardView::Front);
    expectNear(c.forward(), {0, 1, 0});
    expectNear(c.right(), {1, 0, 0});
    expectNear(c.up(), {0, 0, 1});
    expectNear(c.position(), {0, -c.distance, 0});
}

TEST(Camera, StandardViewsHaveOrthonormalAxes) {
    for (StandardView v : {StandardView::Top, StandardView::Bottom, StandardView::Front, StandardView::Back, StandardView::Left,
                           StandardView::Right, StandardView::Isometric}) {
        Camera c;
        c.setStandardView(v);
        EXPECT_NEAR(length(c.forward()), 1.0, 1e-12);
        EXPECT_NEAR(length(c.right()), 1.0, 1e-12);
        EXPECT_NEAR(length(c.up()), 1.0, 1e-12);
        EXPECT_NEAR(dot(c.forward(), c.right()), 0.0, 1e-12);
        EXPECT_NEAR(dot(c.forward(), c.up()), 0.0, 1e-12);
        EXPECT_NEAR(dot(c.right(), c.up()), 0.0, 1e-12);
    }
}

TEST(Camera, TopViewHasXRightAndYUp) {
    Camera c;
    c.setStandardView(StandardView::Top);
    expectNear(c.forward(), {0, 0, -1});
    expectNear(c.right(), {1, 0, 0});
    expectNear(c.up(), {0, 1, 0});
    c.setStandardView(StandardView::Bottom);
    expectNear(c.forward(), {0, 0, 1});
    expectNear(c.up(), {0, -1, 0});
}

TEST(Camera, SideViews) {
    Camera c;
    c.setStandardView(StandardView::Right);  // camera on +X looking at -X
    expectNear(c.forward(), {-1, 0, 0});
    expectNear(c.right(), {0, 1, 0});
    c.setStandardView(StandardView::Left);
    expectNear(c.forward(), {1, 0, 0});
    c.setStandardView(StandardView::Back);
    expectNear(c.forward(), {0, -1, 0});
    expectNear(c.right(), {-1, 0, 0});
}

TEST(Camera, DefaultViewMatchesTheAxesTriadOfTheOriginal) {
    // Original: X points down-right, Y up-right, Z up.
    Camera c;
    const Vec3 r = c.right(), u = c.up();
    EXPECT_GT(dot(Vec3{1, 0, 0}, r), 0);  // X goes right
    EXPECT_LT(dot(Vec3{1, 0, 0}, u), 0);  // and down
    EXPECT_GT(dot(Vec3{0, 1, 0}, r), 0);  // Y goes right
    EXPECT_GT(dot(Vec3{0, 1, 0}, u), 0);  // and up
    EXPECT_GT(dot(Vec3{0, 0, 1}, u), 0);  // Z up
}

TEST(Camera, OrbitClampsPitchAndNeverFlips) {
    Camera c;
    c.orbit(0, 500);
    EXPECT_DOUBLE_EQ(c.pitchDeg, 90.0);
    expectNear(c.up(), {-std::sin(c.yawDeg * kPi / 180), std::cos(c.yawDeg * kPi / 180), 0});
    c.orbit(0, -1000);
    EXPECT_DOUBLE_EQ(c.pitchDeg, -90.0);
    Camera d;
    d.orbit(30, 0);
    EXPECT_DOUBLE_EQ(d.yawDeg, 15.0) << "dragging right turns the camera the opposite way";
    d.orbit(1000, 0);
    EXPECT_LT(std::fabs(d.yawDeg), 360.0);
}

TEST(Camera, OrbitKeepsTheTargetAndDistance) {
    Camera c;
    c.target = {1, 2, 3};
    c.distance = 7;
    c.orbit(33, -12);
    EXPECT_NEAR(length(c.position() - c.target), 7.0, 1e-12);
    expectNear(c.target, {1, 2, 3});
}

TEST(Camera, PanMovesTheSceneWithTheMouse) {
    Camera c;
    c.setStandardView(StandardView::Front);
    c.distance = 10;
    const double perPixel = c.worldPerPixel(800);
    EXPECT_NEAR(perPixel, 2 * 10 * std::tan(22.5 * kPi / 180) / 800, 1e-12);
    // A world point on the target plane follows the cursor: drag 100 px right, 50 px down.
    Vec3 before;
    ASSERT_TRUE(c.project({0, 0, 0}, 1.0, before));
    c.pan(100, 50, 800);
    Vec3 after;
    ASSERT_TRUE(c.project({0, 0, 0}, 1.0, after));
    EXPECT_NEAR((after.x - before.x) * 400.0, 100.0, 1e-6);   // ndc spans 800 px over [-1, 1]
    EXPECT_NEAR((before.y - after.y) * 400.0, 50.0, 1e-6);    // screen y grows downwards
}

TEST(Camera, ZoomChangesDistanceWithinLimits) {
    Camera c;
    c.distance = 20;
    c.zoom(1);
    EXPECT_NEAR(c.distance, 17.0, 1e-12);
    c.zoom(-1);
    EXPECT_NEAR(c.distance, 20.0, 1e-9);
    c.zoom(1e6);
    EXPECT_DOUBLE_EQ(c.distance, Camera::kMinDistance);
    c.zoom(-1e6);
    EXPECT_DOUBLE_EQ(c.distance, Camera::kMaxDistance);
}

TEST(Camera, ZoomAtKeepsThePointUnderTheCursor) {
    for (Camera::Projection proj : {Camera::Projection::Perspective, Camera::Projection::Orthographic}) {
        Camera c;
        c.projection = proj;
        c.target = {2, 1, 0};
        const double aspect = 1.6, nx = 0.6, ny = -0.3;
        const Vec3 world = c.pointOnTargetPlane(nx, ny, aspect);
        c.zoomAt(3, nx, ny, aspect);
        Vec3 ndc;
        ASSERT_TRUE(c.project(world, aspect, ndc));
        EXPECT_NEAR(ndc.x, nx, 1e-9);
        EXPECT_NEAR(ndc.y, ny, 1e-9);
        EXPECT_LT(c.distance, 20.0);
    }
}

TEST(Camera, ProjectAndRayAreInverse) {
    for (Camera::Projection proj : {Camera::Projection::Perspective, Camera::Projection::Orthographic}) {
        Camera c;
        c.projection = proj;
        c.orbit(20, 10);
        c.target = {1, -2, 0.5};
        const double aspect = 1.33;
        const Vec3 world{3, 4, 2};
        Vec3 ndc;
        ASSERT_TRUE(c.project(world, aspect, ndc));
        const Ray r = c.ray(ndc.x, ndc.y, aspect);
        // The ray through the projected point passes through the world point.
        const Vec3 toPoint = world - r.origin;
        const Vec3 along = r.direction * dot(toPoint, r.direction);
        expectNear(toPoint, along, 1e-9);
    }
}

TEST(Camera, TargetProjectsToTheCentreAndBehindTheEyeIsRejected) {
    Camera c;
    Vec3 ndc;
    ASSERT_TRUE(c.project(c.target, 1.0, ndc));
    EXPECT_NEAR(ndc.x, 0.0, 1e-12);
    EXPECT_NEAR(ndc.y, 0.0, 1e-12);
    EXPECT_GT(ndc.z, -1.0);
    EXPECT_LT(ndc.z, 1.0);
    EXPECT_FALSE(c.project(c.position() + (c.position() - c.target), 1.0, ndc)) << "behind the camera";
}

TEST(Camera, ProjectionsShowTheSameSizeAtTheTargetDistance) {
    Camera p, o;
    o.projection = Camera::Projection::Orthographic;
    const Vec3 point = p.target + p.right() * 2.0;
    Vec3 a, b;
    ASSERT_TRUE(p.project(point, 1.0, a));
    ASSERT_TRUE(o.project(point, 1.0, b));
    EXPECT_NEAR(a.x, b.x, 1e-9);
    EXPECT_NEAR(a.y, b.y, 1e-9);
}

TEST(Camera, FrameBoxCentresAndFitsTheBox) {
    Camera c;
    c.frameBox({-5, -5, 0}, {5, 5, 10}, 1.5);
    expectNear(c.target, {0, 0, 5});
    for (const Vec3& corner : {Vec3{-5, -5, 0}, Vec3{5, 5, 10}, Vec3{-5, 5, 0}, Vec3{5, -5, 10}}) {
        Vec3 ndc;
        ASSERT_TRUE(c.project(corner, 1.5, ndc));
        EXPECT_LE(std::fabs(ndc.x), 1.0);
        EXPECT_LE(std::fabs(ndc.y), 1.0);
    }
    Camera d;
    d.frameBox({1, 1, 1}, {1, 1, 1}, 1.0);
    EXPECT_DOUBLE_EQ(d.distance, 1.0);
}

TEST(Camera, MatrixMultiplicationAndIdentity) {
    Camera c;
    const Mat4 vp = c.viewProjection(1.2);
    const Mat4 same = vp * Mat4{};
    for (std::size_t i = 0; i < 16; ++i) EXPECT_DOUBLE_EQ(vp.m[i], same.m[i]);
}
