#include <gtest/gtest.h>

#include "core/picking.h"
#include "core/selection.h"
#include "core/structure_model.h"

using namespace cc::core;

namespace {
// Portal frame in the XZ plane seen from the front: nodes 1..4 = (0,0,0) (4,0,0) (0,0,3) (4,0,3);
// bars 1 = left column, 2 = right column, 3 = lintel; shell 1 = a plate on the right half of the frame.
struct Px {
    double x = 0, y = 0;
};
struct Fixture {
    StructuralModel model;
    Camera camera;
    ScreenMapper map;
    Fixture() {
        const int n1 = model.addNode({0, 0, 0}), n2 = model.addNode({4, 0, 0});
        const int n3 = model.addNode({0, 0, 3}), n4 = model.addNode({4, 0, 3});
        model.addBar(n1, n3);
        model.addBar(n2, n4);
        model.addBar(n3, n4);
        const int n5 = model.addNode({6, 0, 0}), n6 = model.addNode({8, 0, 0});
        const int n7 = model.addNode({8, 0, 3}), n8 = model.addNode({6, 0, 3});
        model.addShell({n5, n6, n7, n8}, 0.1);
        camera.setStandardView(StandardView::Front);
        camera.setProjection(Camera::Projection::Orthographic);
        camera.target = {4, 0, 1.5};
        camera.distance = 10.0;
        map = {&camera, 800.0, 600.0};
    }
    Px at(double x, double z) const {
        Px p;
        map.toScreen(Vec3{x, 0, z}, p.x, p.y);
        return p;
    }
};

bool has(const std::vector<ElementRef>& v, ElementKind k, int id) {
    for (const ElementRef& e : v)
        if (e.kind == k && e.id == id) return true;
    return false;
}
}  // namespace

TEST(Selection, ReplaceAddRemove) {
    Selection s;
    EXPECT_TRUE(s.apply({{ElementKind::Node, 1}, {ElementKind::Node, 2}}, SelectMode::Replace));
    EXPECT_TRUE(s.apply({{ElementKind::Bar, 1}}, SelectMode::Add));
    EXPECT_EQ(s.size(), 3u);
    EXPECT_FALSE(s.apply({{ElementKind::Bar, 1}}, SelectMode::Add));  // nothing new
    EXPECT_TRUE(s.apply({{ElementKind::Node, 1}}, SelectMode::Remove));
    EXPECT_FALSE(s.contains({ElementKind::Node, 1}));
    EXPECT_TRUE(s.apply({{ElementKind::Shell, 9}}, SelectMode::Replace));
    EXPECT_EQ(s.size(), 1u);
    EXPECT_FALSE(s.apply({}, SelectMode::Remove));
}

TEST(Selection, FilterRestrictsTheSelection) {
    Selection s;
    s.apply({{ElementKind::Node, 1}, {ElementKind::Bar, 1}, {ElementKind::Shell, 1}}, SelectMode::Replace);
    EXPECT_TRUE(s.restrictTo(SelectionFilter::only(ElementKind::Bar)));
    EXPECT_EQ(s.size(), 1u);
    EXPECT_EQ(s.count(ElementKind::Bar), 1u);
    EXPECT_FALSE(s.restrictTo(SelectionFilter::only(ElementKind::Bar)));
    EXPECT_EQ(s.ids(ElementKind::Bar), std::vector<int>{1});
}

TEST(Picking, ClickOnANodeBeatsTheBarThroughIt) {
    Fixture f;
    const auto p = f.at(0, 0);  // node 1 is also the end of bar 1
    const auto hit = pickAt(f.model, f.map, {}, p.x + 3, p.y - 2);
    ASSERT_TRUE(hit);
    EXPECT_EQ(hit->kind, ElementKind::Node);
    EXPECT_EQ(hit->id, 1);
}

TEST(Picking, ClickOnABarAwayFromNodes) {
    Fixture f;
    const auto p = f.at(2, 3);  // middle of the lintel
    const auto hit = pickAt(f.model, f.map, {}, p.x, p.y + 3);
    ASSERT_TRUE(hit);
    EXPECT_EQ(hit->kind, ElementKind::Bar);
    EXPECT_EQ(hit->id, 3);
}

TEST(Picking, ClickInsideAShell) {
    Fixture f;
    const auto p = f.at(7, 1.5);
    const auto hit = pickAt(f.model, f.map, {}, p.x, p.y);
    ASSERT_TRUE(hit);
    EXPECT_EQ(hit->kind, ElementKind::Shell);
}

TEST(Picking, ClickOnNothingAndFiltersAndTolerance) {
    Fixture f;
    const auto empty = f.at(2, 1.5);
    EXPECT_FALSE(pickAt(f.model, f.map, {}, empty.x, empty.y));
    const auto lintel = f.at(2, 3);
    EXPECT_FALSE(pickAt(f.model, f.map, {}, lintel.x, lintel.y + 20));  // too far from the bar
    EXPECT_FALSE(pickAt(f.model, f.map, SelectionFilter::only(ElementKind::Node), lintel.x, lintel.y));
    const auto node = f.at(0, 0);
    EXPECT_FALSE(pickAt(f.model, f.map, SelectionFilter::only(ElementKind::Shell), node.x, node.y));
}

TEST(Picking, WindowTakesOnlyWhatIsFullyInside) {
    Fixture f;
    const auto a = f.at(-1, 4), b = f.at(5, -1);  // left to right: covers the frame, not the plate
    const auto hits = pickRect(f.model, f.map, {}, a.x, a.y, b.x, b.y);
    EXPECT_EQ(rectModeFor(a.x, b.x), RectMode::Window);
    for (int id = 1; id <= 4; ++id) EXPECT_TRUE(has(hits, ElementKind::Node, id));
    for (int id = 1; id <= 3; ++id) EXPECT_TRUE(has(hits, ElementKind::Bar, id));
    EXPECT_EQ(hits.size(), 7u);
}

TEST(Picking, WindowLeavesOutPartiallyCoveredBars) {
    Fixture f;
    const auto a = f.at(-1, 4), b = f.at(2, -1);  // ends mid-lintel
    const auto hits = pickRect(f.model, f.map, {}, a.x, a.y, b.x, b.y);
    EXPECT_TRUE(has(hits, ElementKind::Bar, 1));
    EXPECT_FALSE(has(hits, ElementKind::Bar, 3));
    EXPECT_FALSE(has(hits, ElementKind::Node, 4));
}

TEST(Picking, CrossingTakesWhatTheRectangleTouches) {
    Fixture f;
    const auto a = f.at(2, 4), b = f.at(-1, 2);  // right to left: only the left column and lintel are touched
    const auto hits = pickRect(f.model, f.map, {}, a.x, a.y, b.x, b.y);
    EXPECT_EQ(rectModeFor(a.x, b.x), RectMode::Crossing);
    EXPECT_TRUE(has(hits, ElementKind::Bar, 1));
    EXPECT_TRUE(has(hits, ElementKind::Bar, 3));
    EXPECT_FALSE(has(hits, ElementKind::Bar, 2));
    EXPECT_TRUE(has(hits, ElementKind::Node, 3));
    EXPECT_FALSE(has(hits, ElementKind::Node, 1));
}

TEST(Picking, CrossingSelectsAShellFromInsideItOrByAnEdge) {
    Fixture f;
    auto a = f.at(7.1, 1.6), b = f.at(6.9, 1.4);  // small rectangle wholly inside the plate
    EXPECT_TRUE(has(pickRect(f.model, f.map, {}, a.x, a.y, b.x, b.y), ElementKind::Shell, 1));
    a = f.at(6.5, 1.6);
    b = f.at(5.5, 1.4);  // right to left, straddles the left edge
    EXPECT_TRUE(has(pickRect(f.model, f.map, {}, a.x, a.y, b.x, b.y), ElementKind::Shell, 1));
    // The same rectangle drawn left to right is a window, which does not contain the plate.
    EXPECT_FALSE(has(pickRect(f.model, f.map, {}, b.x, a.y, a.x, b.y), ElementKind::Shell, 1));
}

TEST(Picking, FiltersApplyToRectangles) {
    Fixture f;
    const auto a = f.at(-1, 4), b = f.at(9, -1);
    const auto hits = pickRect(f.model, f.map, SelectionFilter::only(ElementKind::Shell), a.x, a.y, b.x, b.y);
    ASSERT_EQ(hits.size(), 1u);
    EXPECT_EQ(hits[0].kind, ElementKind::Shell);
}

TEST(Picking, EmptyViewportPicksNothing) {
    Fixture f;
    f.map.width = 0;
    EXPECT_FALSE(pickAt(f.model, f.map, {}, 0, 0));
    EXPECT_TRUE(pickRect(f.model, f.map, {}, -10, -10, 10, 10).empty());
}
