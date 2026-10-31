#include <gtest/gtest.h>

#include <cmath>

#include "core/project_settings.h"

using namespace cc::core;

TEST(ProjectSettings, DefaultsMatchOriginal) {
    ProfileSettings profiles;
    EXPECT_EQ(profiles.hotRolledSteel, "S275 (UNE-EN 10025-2)");
    EXPECT_EQ(profiles.coldFormedSteel, "S235");
    EXPECT_EQ(profiles.concrete, "C25/30");
    EXPECT_EQ(profiles.timber.strengthClass, "C24");
    EXPECT_EQ(profiles.timber.serviceClass, 1);
    EXPECT_EQ(profiles.aluminium.summary(), "EN AW-5083 - F");
    EXPECT_TRUE(profiles.isValid());
}

TEST(ProjectSettings, ReinforcedConcreteDefaults) {
    ReinforcedConcreteSettings rc;
    EXPECT_TRUE(rc.isValid());
    EXPECT_EQ(rc.rebarSteel.summary(), "S-400");
    EXPECT_EQ(rc.aggregate.summary(), "Cuarcita (15 mm), 30 mm");
    EXPECT_DOUBLE_EQ(rc.cover.footingSide, 7.5);
    EXPECT_DOUBLE_EQ(rc.cover.tieBeamTop, 5.0);
    EXPECT_DOUBLE_EQ(rc.waste.tieBeams, 10.0);
}

TEST(ProjectSettings, ListsAndServiceClassBounds) {
    EXPECT_EQ(hotRolledSteelGrades().size(), 18u);
    EXPECT_EQ(aluminiumAlloys().size(), 8u);
    ProfileSettings profiles;
    profiles.timber.serviceClass = 4;
    EXPECT_FALSE(profiles.isValid());
}

TEST(ProjectSettings, RejectsOutOfRangeValues) {
    CoverSettings cover;
    cover.columns = -1.0;
    EXPECT_FALSE(cover.isValid());
    WasteSettings waste;
    waste.beams = 120.0;
    EXPECT_FALSE(waste.isValid());
    waste.beams = std::nan("");
    EXPECT_FALSE(waste.isValid());
}
