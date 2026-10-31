#include <gtest/gtest.h>

#include "core/limit_states.h"
#include "core/standards.h"

using namespace cc::core;

TEST(LimitStates, DefaultRowsMatchOriginal) {
    const LimitStatesSettings s;
    const auto rows = s.rows();
    ASSERT_EQ(rows.size(), 8u);
    EXPECT_EQ(rows[0].caption, "Hormigón");
    EXPECT_EQ(rows[0].value, "EC - Altitud inferior o igual a 1000 m");
    EXPECT_EQ(rows[2].value, "CTE - Altitud inferior o igual a 1000 m");
    EXPECT_EQ(rows[3].value, "CTE - Altitud inferior o igual a 1000 m");
    EXPECT_EQ(rows[6].caption, "Tensiones sobre el terreno");
    EXPECT_EQ(rows[6].value, "EC - Altitud inferior o igual a 1000 m");
    EXPECT_EQ(rows[7].value, "Acciones características");
    EXPECT_TRUE(s.isValid());
}

TEST(LimitStates, HotRolledCodeChangesRow) {
    LimitStatesSettings s;
    s.hotRolledCode = CombinationCode::EC;
    EXPECT_EQ(s.rows()[3].value, "EC - Altitud inferior o igual a 1000 m");
}

TEST(LimitStates, RejectsUnknownSnow) {
    LimitStatesSettings s;
    s.timberSnow = "Altitud inventada";
    EXPECT_FALSE(s.isValid());
}

TEST(Standards, ShortNameDropsScope) {
    EXPECT_EQ(shortStandardName("Eurocódigo 2 (UE)"), "Eurocódigo 2");
    EXPECT_EQ(shortStandardName("Eurocódigo 9"), "Eurocódigo 9");
}
