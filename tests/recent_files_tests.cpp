#include <gtest/gtest.h>

#include "core/recent_files.h"

using cc::core::RecentFiles;

TEST(RecentFiles, AddPutsNewestFirstAndDeduplicates) {
    RecentFiles r;
    r.add("C:/a/one.ed3");
    r.add("C:/a/two.ed3");
    r.add("c:/A/ONE.ed3");  // same file, different case
    ASSERT_EQ(r.paths().size(), 2u);
    EXPECT_EQ(r.paths()[0], "c:/A/ONE.ed3");
    EXPECT_EQ(r.paths()[1], "C:/a/two.ed3");
}

TEST(RecentFiles, NormalisesBackslashes) {
    RecentFiles r;
    r.add("C:\\a\\one.ed3");
    r.add("C:/a/one.ed3");
    EXPECT_EQ(r.paths().size(), 1u);
    EXPECT_EQ(r.paths()[0], "C:/a/one.ed3");
}

TEST(RecentFiles, CapacityAndTop) {
    RecentFiles r;
    for (int i = 0; i < 30; ++i) r.add("C:/f" + std::to_string(i) + ".ed3");
    EXPECT_EQ(r.paths().size(), RecentFiles::kCapacity);
    EXPECT_EQ(r.paths().front(), "C:/f29.ed3");
    EXPECT_EQ(r.top().size(), RecentFiles::kStartPageSlots);
    EXPECT_EQ(r.top(100).size(), RecentFiles::kCapacity);
}

TEST(RecentFiles, ConstructorKeepsOrder) {
    RecentFiles r({"C:/b.ed3", "C:/a.ed3", "C:/B.ed3"});
    ASSERT_EQ(r.paths().size(), 2u);
    EXPECT_EQ(r.paths()[0], "C:/b.ed3");
    EXPECT_EQ(r.paths()[1], "C:/a.ed3");
}

TEST(RecentFiles, PruneAndRemove) {
    RecentFiles r;
    r.add("C:/keep.ed3");
    r.add("C:/gone.ed3");
    EXPECT_EQ(r.pruneMissing([](const std::string& p) { return p == "C:/keep.ed3"; }), 1u);
    EXPECT_EQ(r.paths().size(), 1u);
    r.remove("c:/KEEP.ed3");
    EXPECT_TRUE(r.paths().empty());
    r.add("");
    EXPECT_TRUE(r.paths().empty());
}
