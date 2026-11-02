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

namespace {
constexpr double kPi = 3.14159265358979323846;
}

TEST(Clipping, SectionXTurnsAboutZThroughItsPivot) {
    ClipSettings c;
    c.setSectionActive(0, true, 2.0, {0, 0, 0});  // pivot (2, 0, 0)
    ASSERT_TRUE(c.rotateSection(0, kPi / 4.0));
    const Vec3 n = c.sectionNormal(0);
    EXPECT_NEAR(n.x, std::cos(kPi / 4.0), 1e-12);
    EXPECT_NEAR(n.y, std::sin(kPi / 4.0), 1e-12);
    EXPECT_NEAR(n.z, 0.0, 1e-12);
    const auto h = c.halfSpaces();
    ASSERT_EQ(h.size(), 1u);
    EXPECT_TRUE(h[0].contains({2, 0, 7}));   // the pivot stays on the plane
    EXPECT_TRUE(h[0].contains({2, -1, 0}));  // the lower side is kept
    EXPECT_FALSE(h[0].contains({2, 1, 0}));  // the arrow side goes
}

TEST(Clipping, SectionYTurnsAboutZWithOppositeSense) {
    ClipSettings c;
    c.setSectionActive(1, true, 1.0, {0, 0, 0});
    ASSERT_TRUE(c.rotateSection(1, kPi / 6.0));
    const Vec3 n = c.sectionNormal(1);
    EXPECT_NEAR(n.x, -std::sin(kPi / 6.0), 1e-12);
    EXPECT_NEAR(n.y, std::cos(kPi / 6.0), 1e-12);
}

TEST(Clipping, SectionZTiltsAboutY) {
    ClipSettings c;
    c.setSectionActive(2, true, 3.0, {1, 2, 0});
    ASSERT_TRUE(c.rotateSection(2, kPi / 3.0));
    const Vec3 n = c.sectionNormal(2);
    EXPECT_NEAR(n.x, std::sin(kPi / 3.0), 1e-12);
    EXPECT_NEAR(n.y, 0.0, 1e-12);
    EXPECT_NEAR(n.z, std::cos(kPi / 3.0), 1e-12);
    const Vec3 pivot = c.sectionPoint(2);
    EXPECT_EQ(pivot, (Vec3{1, 2, 3}));
    // A point of the unturned plane, turned about the pivot, lies on the tilted plane.
    const Vec3 q = c.sectionToWorld(2, {5, 2, 3});
    EXPECT_NEAR(dot(n, q - pivot), 0.0, 1e-12);
    const auto h = c.halfSpaces();
    ASSERT_EQ(h.size(), 1u);
    EXPECT_TRUE(h[0].contains(pivot));
    EXPECT_TRUE(h[0].contains({1, 2, 2}));
    EXPECT_FALSE(h[0].contains({1, 2, 4}));
}

TEST(Clipping, SectionTurnIsRefusedBeyondEightyDegreesNotClamped) {
    ClipSettings c;
    c.setSectionActive(0, true, 0.0);
    const double deg = kPi / 180.0;
    EXPECT_TRUE(c.rotateSection(0, 70 * deg));
    EXPECT_FALSE(c.rotateSection(0, 15 * deg));  // 85 degrees: the whole step is rejected
    EXPECT_NEAR(c.sections[0].angleRad, 70 * deg, 1e-12);
    EXPECT_TRUE(c.rotateSection(0, 10 * deg));   // exactly 80 degrees is allowed
    EXPECT_FALSE(c.rotateSection(0, 1 * deg));
    EXPECT_TRUE(c.rotateSection(0, -160 * deg));  // -80 degrees
    EXPECT_FALSE(c.rotateSection(0, -1 * deg));
    EXPECT_FALSE(c.rotateSection(0, std::numeric_limits<double>::quiet_NaN()));
    EXPECT_FALSE(c.rotateSection(1, 0.1));  // inactive
    EXPECT_FALSE(c.rotateSection(5, 0.1));
}

TEST(Clipping, SectionsTurnIndependentlyAndResetWhenShownAgain) {
    ClipSettings c;
    c.setSectionActive(0, true, 1.0);
    c.setSectionActive(1, true, 1.0);
    ASSERT_TRUE(c.rotateSection(0, 0.5));
    EXPECT_DOUBLE_EQ(c.sections[1].angleRad, 0.0);
    EXPECT_DOUBLE_EQ(c.angleRad, 0.0);  // the global turn belongs to the volume
    c.setSectionActive(0, false, 1.0);
    c.setSectionActive(0, true, 1.0);
    EXPECT_DOUBLE_EQ(c.sections[0].angleRad, 0.0);
}

TEST(Clipping, VolumeKeepsTheGlobalTurnWhileSectionsDoNotFollowIt) {
    ClipSettings c;
    c.setSectionActive(0, true, 2.0, {2, 0, 0});
    c.rotate(1.0);  // no volume yet: ignored
    EXPECT_DOUBLE_EQ(c.angleRad, 0.0);
    c.setVolumeActive(true, {0, 0, 0}, {4, 4, 4});
    EXPECT_EQ(c.centre, (Vec3{2, 2, 2}));
    c.rotate(kPi / 2.0);
    EXPECT_NEAR(c.axisDirection(0).y, 1.0, 1e-12);
    EXPECT_NEAR(c.sectionNormal(0).x, 1.0, 1e-12);  // the section stays at angle 0
    EXPECT_NEAR(c.sectionNormal(0).y, 0.0, 1e-12);
    const Vec3 p{1, 3, 2};
    const Vec3 q = c.toLocal(c.toWorld(p));
    EXPECT_NEAR(q.x, p.x, 1e-12);
    EXPECT_NEAR(q.y, p.y, 1e-12);
    EXPECT_NEAR(c.toWorld(c.centre).x, 2.0, 1e-12);
    c.rotate(7.0);  // wraps, no limit
    EXPECT_LE(std::abs(c.angleRad), 3.1416);
    c.rotate(std::numeric_limits<double>::infinity());  // ignored
    EXPECT_TRUE(std::isfinite(c.angleRad));
    c.setVolumeActive(false, {}, {});
    EXPECT_DOUBLE_EQ(c.angleRad, 0.0);
}
