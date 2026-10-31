#include <gtest/gtest.h>

#include <cmath>

#include "core/snap.h"

using namespace cc::core;

namespace {
// Portal frame in the XZ plane seen from the front (orthographic): nodes 1..4 = (0,0,0) (4,0,0) (0,0,3) (4,0,3);
// bars 1 = left column, 2 = right column, 3 = lintel.
struct Px {
    double x = 0, y = 0;
};
struct Fixture {
    StructuralModel model;
    Camera camera;
    ScreenMapper map;
    SnapOptions options;
    Fixture() {
        const int n1 = model.addNode({0, 0, 0}), n2 = model.addNode({4, 0, 0});
        const int n3 = model.addNode({0, 0, 3}), n4 = model.addNode({4, 0, 3});
        model.addBar(n1, n3);
        model.addBar(n2, n4);
        model.addBar(n3, n4);
        camera.setStandardView(StandardView::Front);
        camera.setProjection(Camera::Projection::Orthographic);
        camera.target = {2, 0, 1.5};
        camera.distance = 10.0;
        map = {&camera, 800.0, 600.0};
    }
    Px at(double x, double z) const {
        Px p;
        map.toScreen(Vec3{x, 0, z}, p.x, p.y);
        return p;
    }
    SnapResult snap(Px p, double dx = 0, double dy = 0) const { return snapCursor(model, map, p.x + dx, p.y + dy, options); }
};
}  // namespace

TEST(Snap, DefaultsMatchTheOriginalDialog) {
    SnapOptions o;
    EXPECT_FALSE(o.enabled);
    EXPECT_FALSE(o.tracking);
    EXPECT_TRUE(o.endpoint && o.midpoint && o.intersection && o.nearest && o.extension);
    EXPECT_FALSE(o.point || o.insertion || o.centre || o.perpendicular || o.parallel || o.quadrant);
    o.clearAll();
    EXPECT_FALSE(o.endpoint || o.midpoint || o.nearest || o.extension || o.intersection);
}

TEST(Snap, BaselineKindsWorkWithTheMasterSwitchOff) {
    SnapOptions o;
    EXPECT_TRUE(o.active(SnapKind::Endpoint));
    EXPECT_TRUE(o.active(SnapKind::Nearest));
    EXPECT_TRUE(o.active(SnapKind::Extension));
    EXPECT_FALSE(o.active(SnapKind::Midpoint));
    o.enabled = true;
    EXPECT_TRUE(o.active(SnapKind::Midpoint));
    o.nearest = false;
    EXPECT_FALSE(o.active(SnapKind::Nearest));
}

TEST(Snap, SnapsToANodeWithinTheNodeRadius) {
    Fixture f;
    const SnapResult r = f.snap(f.at(4, 3), 8, -6);  // 10 px away
    ASSERT_EQ(r.kind, SnapKind::Endpoint);
    EXPECT_EQ(r.element->id, 4);
    EXPECT_EQ(r.point, (Vec3{4, 0, 3}));
}

TEST(Snap, BeyondTheNodeRadiusItFallsBackToTheGuideLine) {
    Fixture f;
    const SnapResult r = f.snap(f.at(4, 0), 18, 0);  // 18 px right of node 2, on the X guide line z = 0
    ASSERT_EQ(r.kind, SnapKind::Extension);
    EXPECT_EQ(r.axis, 0);
    EXPECT_NEAR(r.point.y, 0.0, 1e-9);
    EXPECT_NEAR(r.point.z, 0.0, 1e-9);
    EXPECT_GT(r.point.x, 4.0);
    EXPECT_NEAR(r.referenceDistance, r.point.x - 4.0, 1e-6);  // measured from the nearest node of the line
}

TEST(Snap, SnapsToTheClosestPointOfABarNotItsMidpoint) {
    Fixture f;
    const SnapResult r = f.snap(f.at(1, 3), 0, 5);  // a quarter of the lintel, 5 px below it
    ASSERT_EQ(r.kind, SnapKind::Nearest);
    EXPECT_EQ(r.element->kind, ElementKind::Bar);
    EXPECT_EQ(r.element->id, 3);
    EXPECT_NEAR(r.point.x, 1.0, 0.05);
    EXPECT_NEAR(r.point.z, 3.0, 1e-9);
    EXPECT_NEAR(r.referenceDistance, 1.0, 0.05);
    ASSERT_TRUE(r.referenceNode);
    EXPECT_EQ(*r.referenceNode, 3);
}

TEST(Snap, MidpointNeedsTheMasterSwitch) {
    Fixture f;
    EXPECT_EQ(f.snap(f.at(2, 3), 0, 3).kind, SnapKind::Nearest);
    f.options.enabled = true;
    const SnapResult r = f.snap(f.at(2, 3), 0, 3);
    ASSERT_EQ(r.kind, SnapKind::Midpoint);
    EXPECT_EQ(r.point, (Vec3{2, 0, 3}));
    EXPECT_EQ(r.element->id, 3);
}

TEST(Snap, DisabledKindsAreSkipped) {
    Fixture f;
    f.options.enabled = true;
    f.options.endpoint = false;
    EXPECT_NE(f.snap(f.at(0, 0), 2, -2).kind, SnapKind::Endpoint);
    f.options.clearAll();
    EXPECT_FALSE(f.snap(f.at(0, 0)).snapped());
}

TEST(Snap, FarFromEverythingIsFree) {
    Fixture f;
    const SnapResult r = f.snap(f.at(2, 1.5));
    EXPECT_FALSE(r.snapped());
    EXPECT_EQ(r.kind, SnapKind::None);
}

TEST(Snap, WorksInPerspectiveToo) {
    Fixture f;
    f.camera.setProjection(Camera::Projection::Perspective);
    const SnapResult r = f.snap(f.at(0, 3), 5, 5);
    ASSERT_EQ(r.kind, SnapKind::Endpoint);
    EXPECT_EQ(r.element->id, 3);
    const SnapResult bar = f.snap(f.at(2, 3), 0, 4);
    ASSERT_EQ(bar.kind, SnapKind::Nearest);
    EXPECT_NEAR(bar.point.z, 3.0, 1e-6);
}

TEST(Snap, EmptyViewportOrModelNeverSnaps) {
    Fixture f;
    f.map.width = 0;
    EXPECT_FALSE(f.snap({10, 10}).snapped());
    Fixture g;
    g.model = StructuralModel{};
    EXPECT_FALSE(g.snap(g.at(0, 0)).snapped());
}
