#include <gtest/gtest.h>

#include <limits>

#include "core/display_options.h"

using namespace cc::core;

TEST(DisplayOptions, DefaultsAreThoseOfTheOriginal) {
    const DisplayOptions o;
    EXPECT_TRUE(o.showElementInfo);
    EXPECT_TRUE(o.showTexts);
    EXPECT_TRUE(o.showLocalAxes);
    EXPECT_FALSE(o.fillShells);
    EXPECT_TRUE(o.fillLoads);
    EXPECT_DOUBLE_EQ(o.nodeSizeCm, 15.0);
    EXPECT_DOUBLE_EQ(o.textSizePercent, 100.0);
    EXPECT_EQ(o.tooltipPlacement, TooltipPlacement::TopRightCorner);
    EXPECT_TRUE(isValid(o));
}

TEST(DisplayOptions, SizesMustBePositiveAndFinite) {
    DisplayOptions o;
    o.nodeSizeCm = 0;
    EXPECT_FALSE(isValid(o));
    o.nodeSizeCm = -1;
    EXPECT_FALSE(isValid(o));
    o.nodeSizeCm = 0.5;  // below the original's minimum of 1.0 cm
    EXPECT_FALSE(isValid(o));
    o.nodeSizeCm = 1000.1;
    EXPECT_FALSE(isValid(o));
    o.nodeSizeCm = 1000.0;
    EXPECT_TRUE(isValid(o));
    o.nodeSizeCm = 1.0;
    EXPECT_TRUE(isValid(o));
    o.textSizePercent = 9.9;
    EXPECT_FALSE(isValid(o));
    o.textSizePercent = 1000.0;
    EXPECT_TRUE(isValid(o));
    o.textSizePercent = 100.0;
    o.textSizePercent = std::numeric_limits<double>::quiet_NaN();
    EXPECT_FALSE(isValid(o));
}

TEST(DisplayOptions, JsonRoundTrip) {
    DisplayOptions o;
    o.showElementInfo = false;
    o.fillShells = true;
    o.nodeSizeCm = 22.5;
    o.textSizePercent = 150;
    o.tooltipPlacement = TooltipPlacement::OnElement;
    const auto back = displayOptionsFromJson(toJson(o));
    ASSERT_TRUE(back.has_value());
    EXPECT_EQ(*back, o);
}

TEST(DisplayOptions, JsonIsTolerantButStrict) {
    const auto partial = displayOptionsFromJson(R"({"fillShells":true})");
    ASSERT_TRUE(partial.has_value());
    EXPECT_TRUE(partial->fillShells);
    EXPECT_DOUBLE_EQ(partial->nodeSizeCm, 15.0);
    EXPECT_FALSE(displayOptionsFromJson("not json"));
    EXPECT_FALSE(displayOptionsFromJson(R"({"fillShells":"yes"})"));
    EXPECT_FALSE(displayOptionsFromJson(R"({"tooltipPlacement":"Nowhere"})"));
    EXPECT_FALSE(displayOptionsFromJson(R"({"nodeSizeCm":0})"));
}
