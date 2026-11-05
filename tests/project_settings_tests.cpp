#include <gtest/gtest.h>

#include <algorithm>
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

TEST(ProjectSettings, VerifiedListsFromManualCaptures) {
    EXPECT_EQ(concreteGrades().size(), 17u);
    EXPECT_EQ(rebarSteels().size(), 8u);
    EXPECT_EQ(aggregateKinds().size(), 4u);
    EXPECT_EQ(timberStrengthClasses().size(), 12u);
    ASSERT_EQ(timberMaterials().size(), 4u);
    EXPECT_EQ(timberMaterials()[3], "Laminada encolada, combinada");
    EXPECT_EQ(std::count(concreteGrades().begin(), concreteGrades().end(), std::string("Bc 45")), 0);
}

TEST(ProjectSettings, ServiceClassTextsAndAggregateRange) {
    EXPECT_NE(timberServiceClassText(2).find("85%"), std::string::npos);
    EXPECT_NE(timberServiceClassText(3).find("superiores a la clase de servicio 2."), std::string::npos);
    EXPECT_TRUE(timberServiceClassText(4).empty());
    double mm = 0.0;
    EXPECT_TRUE(parseAggregateSizeMm("15 mm", &mm));
    EXPECT_DOUBLE_EQ(mm, 15.0);
    EXPECT_TRUE(parseAggregateSizeMm("5", nullptr));
    EXPECT_TRUE(parseAggregateSizeMm("100 mm", nullptr));
    EXPECT_FALSE(parseAggregateSizeMm("4 mm", nullptr));
    EXPECT_FALSE(parseAggregateSizeMm("101", nullptr));
    EXPECT_FALSE(parseAggregateSizeMm("abc", nullptr));
    AggregateSettings a;
    a.foundationMaxSizeMm = 4.0;
    EXPECT_FALSE(a.isValid());
}
