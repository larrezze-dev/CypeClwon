#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

#include "core/project_archive.h"

namespace fs = std::filesystem;
using namespace cc::core;

namespace {
struct TempDir {
    fs::path path = fs::temp_directory_path() / ("cc_archive_" + std::to_string(std::rand()));
    TempDir() { fs::create_directories(path); }
    ~TempDir() {
        std::error_code ec;
        fs::remove_all(path, ec);
    }
};
void write(const fs::path& p, const std::string& text) { std::ofstream(p, std::ios::binary) << text; }
std::string read(const fs::path& p) {
    std::ifstream f(p, std::ios::binary);
    std::stringstream s;
    s << f.rdbuf();
    return s.str();
}
}  // namespace

TEST(ProjectArchive, RoundTripKeepsContents) {
    TempDir d;
    write(d.path / "obra.ed3", std::string(5000, 'a') + "end");
    ASSERT_TRUE(compressProject(d.path / "obra.ed3", d.path / "obra.cyp", false).ok);
    const auto r = extractProject(d.path / "obra.cyp", d.path / "out");
    ASSERT_TRUE(r.ok) << r.error;
    EXPECT_EQ(read(d.path / "out" / "obra.ed3"), std::string(5000, 'a') + "end");
}

TEST(ProjectArchive, BackupsAreOptional) {
    TempDir d;
    write(d.path / "obra.ed3", "12345");
    write(d.path / "obra.bak", "123");
    EXPECT_EQ(archiveSourceBytes(d.path / "obra.ed3", false), 5u);
    EXPECT_EQ(archiveSourceBytes(d.path / "obra.ed3", true), 8u);
    EXPECT_EQ(compressProject(d.path / "obra.ed3", d.path / "a.cyp", true).files.size(), 2u);
    EXPECT_EQ(compressProject(d.path / "obra.ed3", d.path / "b.cyp", false).files.size(), 1u);
}

TEST(ProjectArchive, MissingProjectFails) {
    TempDir d;
    const auto r = compressProject(d.path / "no.ed3", d.path / "no.cyp", false);
    EXPECT_FALSE(r.ok);
    EXPECT_FALSE(r.error.empty());
    EXPECT_FALSE(fs::exists(d.path / "no.cyp"));
}

TEST(ProjectArchive, ExtractRefusesToOverwriteAndRejectsGarbage) {
    TempDir d;
    write(d.path / "obra.ed3", "x");
    ASSERT_TRUE(compressProject(d.path / "obra.ed3", d.path / "obra.cyp", false).ok);
    EXPECT_FALSE(extractProject(d.path / "obra.cyp", d.path).ok);  // obra.ed3 already there
    EXPECT_TRUE(extractProject(d.path / "obra.cyp", d.path, true).ok);
    write(d.path / "junk.cyp", "not a zip");
    EXPECT_FALSE(extractProject(d.path / "junk.cyp", d.path / "o2").ok);
}
