#include <gtest/gtest.h>

#include "core/categories.h"

using namespace cc::core;

TEST(Categories, DefaultsAreThoseOfTheOriginal) {
    const CategorySettings s;
    for (int i = 0; i < kCategoryCount; ++i) {
        EXPECT_TRUE(s.visible[i]);
        EXPECT_FALSE(s.locked[i]);
    }
    EXPECT_TRUE(s.useLayerColours);
    EXPECT_EQ(s.analysis, AnalysisElements::StressDeformation);
    EXPECT_TRUE(s.nodesDrawn());
    EXPECT_TRUE(s.supportsDrawn());
}

TEST(Categories, HidingRulesFollowTheOriginal) {
    CategorySettings s;
    s.visible[static_cast<int>(Category::Nodes)] = false;
    EXPECT_FALSE(s.nodesDrawn());
    EXPECT_FALSE(s.supportsDrawn());
    EXPECT_TRUE(s.barsDrawn());
    s = {};
    s.visible[static_cast<int>(Category::Bars)] = false;
    EXPECT_FALSE(s.barsDrawn());
    EXPECT_FALSE(s.nodesDrawn());
    EXPECT_FALSE(s.supportsDrawn());
    EXPECT_TRUE(s.shellsDrawn());
    s = {};
    s.visible[static_cast<int>(Category::Shells)] = false;
    EXPECT_FALSE(s.shellsDrawn());
    EXPECT_TRUE(s.barsDrawn());
    s = {};
    s.visible[static_cast<int>(Category::Columns)] = false;  // no element type yet: no effect
    EXPECT_TRUE(s.barsDrawn() && s.nodesDrawn() && s.shellsDrawn());
}

TEST(Categories, JsonRoundTrip) {
    CategorySettings s;
    s.visible[1] = false;
    s.locked[3] = true;
    s.useLayerColours = false;
    s.analysis = AnalysisElements::Modal;
    const auto back = categorySettingsFromJson(toJson(s));
    ASSERT_TRUE(back);
    EXPECT_EQ(*back, s);
    EXPECT_EQ(*categorySettingsFromJson(toJson(CategorySettings{})), CategorySettings{});
}

TEST(Categories, JsonMissingFieldsKeepDefaultsAndBadInputIsRejected) {
    EXPECT_EQ(*categorySettingsFromJson("{}"), CategorySettings{});
    EXPECT_FALSE(categorySettingsFromJson("not json"));
    EXPECT_FALSE(categorySettingsFromJson("[]"));
    EXPECT_FALSE(categorySettingsFromJson("{\"analysis\":\"Nope\"}"));
    EXPECT_FALSE(categorySettingsFromJson("{\"visible\":[true]}"));
    EXPECT_FALSE(categorySettingsFromJson("{\"useLayerColours\":3}"));
}
