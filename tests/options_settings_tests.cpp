#include <gtest/gtest.h>

#include "core/options_settings.h"

using namespace cc::core;

TEST(OptionsSettings, DefaultsMatchOriginal) {
    OptionsSettings o;
    EXPECT_TRUE(o.isValid());
    EXPECT_EQ(o.beams.numbering, BeamNumbering::None);
    EXPECT_EQ(o.beams.continuousNumbering.baseReference, "Pórtico 1");
    EXPECT_EQ(o.beams.errors.minStirrupDiameterMm, 16);
    EXPECT_EQ(o.joints.materials.rolledSteel, "S275 (UNE-EN 10025-2)");
    EXPECT_EQ(o.joints.bolts.steelClass, "8.8");
}

TEST(OptionsSettings, DeflectionLimitsTable) {
    const auto& groups = defaultDeflectionLimits();
    ASSERT_EQ(groups.size(), 4u);
    EXPECT_EQ(groups[0].limits.size(), 2u);
    EXPECT_EQ(groups[0].limits[0].text, "L/ 250");
    EXPECT_TRUE(groups[1].limits.empty());  // acero laminado: no limit defined
    EXPECT_EQ(groups[2].limits[1].text, "L/ 300");
    EXPECT_EQ(groups[3].limits[2].text, "L/ 400");
}

TEST(OptionsSettings, RejectsInvalidValues) {
    BoltSettings bolts;
    bolts.steelClass = "12.9";
    EXPECT_FALSE(bolts.isValid());
    JointMaterials materials;
    materials.concrete = "C99/99";
    EXPECT_FALSE(materials.isValid());
    ContinuousBeamNumbering numbering;
    numbering.baseReference.clear();
    EXPECT_FALSE(numbering.isValid());
    EXPECT_EQ(boltDiameterNames().size(), BoltSettings::kDiameterCount);
}
