#include <gtest/gtest.h>

#include "core/units.h"

using namespace cc::core;

TEST(Units, DefaultsMatchTheOriginal) {
    const DimensioningUnits u;
    EXPECT_EQ(u.dimensions, LengthUnit::Centimetres);
    EXPECT_EQ(u.reinforcement, LengthUnit::Centimetres);
    EXPECT_EQ(u.lengths, LengthUnit::Metres);
}

TEST(Units, ConversionsAreExactAndRoundTrip) {
    EXPECT_DOUBLE_EQ(fromMetres(1.5, LengthUnit::Centimetres), 150.0);
    EXPECT_DOUBLE_EQ(fromMetres(0.025, LengthUnit::Millimetres), 25.0);
    EXPECT_DOUBLE_EQ(toMetres(15.0, LengthUnit::Centimetres), 0.15);
    EXPECT_DOUBLE_EQ(convertLength(250.0, LengthUnit::Centimetres, LengthUnit::Millimetres), 2500.0);
    for (LengthUnit a : allLengthUnits())
        for (LengthUnit b : allLengthUnits()) EXPECT_NEAR(convertLength(convertLength(3.7, a, b), b, a), 3.7, 1e-12);
}

TEST(Units, ChoicesFollowTheOriginalOrder) {
    const auto& all = allLengthUnits();
    EXPECT_STREQ(unitName(all[0]), "Metros");
    EXPECT_STREQ(unitName(all[1]), "Centímetros");
    EXPECT_STREQ(unitName(all[2]), "Milímetros");
    EXPECT_STREQ(unitSymbol(all[2]), "mm");
}

TEST(Units, ParseAcceptsSymbolsAndSpanishNames) {
    LengthUnit u{};
    EXPECT_TRUE(parseUnit("cm", u));
    EXPECT_EQ(u, LengthUnit::Centimetres);
    EXPECT_TRUE(parseUnit("milímetros", u));
    EXPECT_EQ(u, LengthUnit::Millimetres);
    EXPECT_TRUE(parseUnit("Metros", u));
    EXPECT_EQ(u, LengthUnit::Metres);
    EXPECT_FALSE(parseUnit("pulgadas", u));
    EXPECT_FALSE(parseUnit("", u));
}

TEST(Units, FormatUsesDotAndNeverNegativeZero) {
    EXPECT_EQ(formatLength(0.15, LengthUnit::Centimetres, 1), "15.0");
    EXPECT_EQ(formatLengthWithUnit(0.15, LengthUnit::Centimetres, 1), "15.0 cm");
    EXPECT_EQ(formatLength(184.315, LengthUnit::Metres, 3), "184.315");  // coordinates seen in the original tooltip
    EXPECT_EQ(formatLength(-0.00001, LengthUnit::Metres, 2), "0.00");
    EXPECT_EQ(formatLength(-1.5, LengthUnit::Metres, 1), "-1.5");
}
