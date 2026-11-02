#include <gtest/gtest.h>

#include <cmath>
#include <limits>

#include "core/clipping.h"

using namespace cc::core;

TEST(Clipping, SectionKeepsTheSideOppositeTheArrow) {
    ClipSettings c;
    c.setSectionActive(0, true, 2.0);
    const auto spaces = c.halfSpaces();
    ASSERT_EQ(spaces.size(), 1u);
    EXPECT_TRUE(spaces[0].contains({1, 0, 0}));
    EXPECT_FALSE(spaces[0].contains({3, 0, 0}));  // the arrow points to +X: that side disappears
    EXPECT_TRUE(c.flipSection(0));
    const auto flipped = c.halfSpaces();
    EXPECT_FALSE(flipped[0].contains({1, 0, 0}));
    EXPECT_TRUE(flipped[0].contains({3, 0, 0}));
}

TEST(Clipping, FlipOfAnInactiveSectionDoesNothing) {
    ClipSettings c;
    EXPECT_FALSE(c.flipSection(1));
    EXPECT_FALSE(c.flipSection(7));
    EXPECT_FALSE(c.anyActive());
}

TEST(Clipping, ReactivatedSectionRestartsAtTheDefaultPosition) {
    ClipSettings c;
    c.setSectionActive(2, true, 1.0);
    c.sections[2].position = 9.0;
    c.flipSection(2);
    c.setSectionActive(2, false, 1.0);
    c.setSectionActive(2, true, 4.0);
    EXPECT_DOUBLE_EQ(c.sections[2].position, 4.0);
    EXPECT_FALSE(c.sections[2].flipped);
}

TEST(Clipping, SegmentIsCutAtThePlane) {
    const std::vector<ClipHalfSpace> s{ClipHalfSpace::axisAligned(0, 2.0, true)};
    const auto r = clipSegment({0, 0, 0}, {4, 0, 0}, s);
    ASSERT_TRUE(r);
    EXPECT_DOUBLE_EQ(r->first.x, 0.0);
    EXPECT_DOUBLE_EQ(r->second.x, 2.0);
    EXPECT_FALSE(clipSegment({3, 0, 0}, {4, 1, 0}, s));
    const auto whole = clipSegment({0, 0, 0}, {1, 1, 1}, s);
    ASSERT_TRUE(whole);
    EXPECT_EQ(whole->second, (Vec3{1, 1, 1}));
}

TEST(Clipping, SegmentCrossingTheVolumeKeepsTheInsidePart) {
    ClipSettings c;
    c.setVolumeActive(true, {0, 0, 0}, {2, 2, 2});
    const auto r = clipSegment({-1, 1, 1}, {3, 1, 1}, c.halfSpaces());
    ASSERT_TRUE(r);
    EXPECT_DOUBLE_EQ(r->first.x, 0.0);
    EXPECT_DOUBLE_EQ(r->second.x, 2.0);
    EXPECT_FALSE(clipSegment({5, 5, 5}, {6, 6, 6}, c.halfSpaces()));
}

TEST(Clipping, PolygonIsCutAndCanVanish) {
    const std::vector<Vec3> square{{0, 0, 0}, {4, 0, 0}, {4, 4, 0}, {0, 4, 0}};
    const auto half = clipPolygon(square, {ClipHalfSpace::axisAligned(0, 2.0, true)});
    ASSERT_EQ(half.size(), 4u);
    for (const Vec3& p : half) EXPECT_LE(p.x, 2.0 + 1e-12);
    EXPECT_TRUE(clipPolygon(square, {ClipHalfSpace::axisAligned(0, -1.0, true)}).empty());
    EXPECT_EQ(clipPolygon(square, {ClipHalfSpace::axisAligned(2, 1.0, true)}).size(), 4u);  // coplanar with Z=0, kept
}

TEST(Clipping, VolumeFacesNeverCross) {
    ClipSettings c;
    c.setVolumeActive(true, {0, 0, 0}, {2, 2, 2});
    c.moveVolumeFace(0, true, -5.0);
    EXPECT_DOUBLE_EQ(c.volume.max.x, 0.0);
    c.moveVolumeFace(1, false, 9.0);
    EXPECT_DOUBLE_EQ(c.volume.min.y, 2.0);
    c.moveVolumeFace(2, true, 1.5);
    EXPECT_DOUBLE_EQ(c.volume.max.z, 1.5);
    c.moveVolumeFace(0, true, std::numeric_limits<double>::quiet_NaN());
    EXPECT_DOUBLE_EQ(c.volume.max.x, 0.0);
    EXPECT_EQ(c.halfSpaces().size(), 6u);
}

TEST(Clipping, EnvelopeOfPoints) {
    const auto [lo, hi] = envelope({{1, 2, 3}, {-1, 5, 0}}, 0.5);
    EXPECT_EQ(lo, (Vec3{-1.5, 1.5, -0.5}));
    EXPECT_EQ(hi, (Vec3{1.5, 5.5, 3.5}));
    const auto empty = envelope({});
    EXPECT_LT(empty.first.x, empty.second.x);
}

TEST(Clipping, TurnedSectionKeepsTheSideOppositeTheRotatedArrow) {
    ClipSettings c;
    c.setSectionActive(0, true, 2.0, {2, 0, 0});  // centre of the turn on the plane
    c.rotate(3.14159265358979323846 / 2.0);        // 90 deg ccw: the X plane becomes a Y plane (normal +Y)
    const auto h = c.halfSpaces();
    ASSERT_EQ(h.size(), 1u);
    EXPECT_TRUE(h[0].contains({2, -1, 5}));
    EXPECT_FALSE(h[0].contains({2, 1, 5}));
    EXPECT_NEAR(c.axisDirection(0).y, 1.0, 1e-12);
    EXPECT_NEAR(c.axisDirection(0).x, 0.0, 1e-12);
}

TEST(Clipping, ToWorldAndToLocalAreInverse) {
    ClipSettings c;
    c.setSectionActive(1, true, 0.0, {3, 4, 1});
    c.rotate(0.7);
    const Vec3 p{5, -2, 8};
    const Vec3 q = c.toLocal(c.toWorld(p));
    EXPECT_NEAR(q.x, p.x, 1e-12);
    EXPECT_NEAR(q.y, p.y, 1e-12);
    EXPECT_NEAR(q.z, p.z, 1e-12);
    EXPECT_NEAR(c.toWorld(c.centre).x, 3.0, 1e-12);  // the centre does not move
}

TEST(Clipping, TurnResetsWhenNothingRemainsActiveAndRotateWraps) {
    ClipSettings c;
    c.setSectionActive(0, true, 0.0, {});
    c.rotate(7.0);
    EXPECT_LE(std::abs(c.angleRad), 3.1416);
    c.rotate(std::numeric_limits<double>::infinity());  // ignored
    EXPECT_TRUE(std::isfinite(c.angleRad));
    c.setSectionActive(0, false, 0.0, {});
    EXPECT_DOUBLE_EQ(c.angleRad, 0.0);
}
