#include <gtest/gtest.h>

#include "core/version.h"

TEST(Version, MatchesProjectVersion) { EXPECT_EQ(cc::core::versionString(), "0.1.0"); }

TEST(Version, ProductNameIsNotEmpty) { EXPECT_FALSE(cc::core::productName().empty()); }

