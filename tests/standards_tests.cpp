#include <gtest/gtest.h>

#include <algorithm>

#include "core/standards.h"

using cc::core::StandardFamily;
using cc::core::StandardsSelection;

TEST(Standards, DefaultsAreValidAndSummaryMatchesOriginal) {
    StandardsSelection s;
    EXPECT_TRUE(s.isValid());
    EXPECT_EQ(s.summary(), "Eurocódigo 2, CTE DB SE-A, Código Estructural, CTE DB SE-M y Eurocódigo 9");
}

TEST(Standards, ListsStartAsInOriginalAndAreUnique) {
    EXPECT_EQ(cc::core::availableStandards(StandardFamily::Concrete).front(), "Código Estructural (España)");
    EXPECT_EQ(cc::core::availableStandards(StandardFamily::Aluminium).size(), 1u);
    for (auto f : {StandardFamily::Concrete, StandardFamily::ColdFormedSteel, StandardFamily::HotRolledSteel,
                   StandardFamily::Timber}) {
        auto v = cc::core::availableStandards(f);
        std::sort(v.begin(), v.end());
        EXPECT_EQ(std::adjacent_find(v.begin(), v.end()), v.end());
    }
}

TEST(Standards, InvalidSelectionIsDetected) {
    StandardsSelection s;
    s.set(StandardFamily::Timber, "no existe");
    EXPECT_FALSE(s.isValid());
}

TEST(Standards, SummaryKeepsInnerParenthesesOnlyWhenNotTrailingScope) {
    StandardsSelection s;
    s.set(StandardFamily::Concrete, "EHE-08");
    EXPECT_EQ(s.summary(), "EHE-08, CTE DB SE-A, Código Estructural, CTE DB SE-M y Eurocódigo 9");
}
