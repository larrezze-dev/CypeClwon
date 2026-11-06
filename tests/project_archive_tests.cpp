#include <gtest/gtest.h>
#include <miniz.h>

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

void putU32(std::string& out, unsigned v) {
    for (int i = 0; i < 4; ++i) out.push_back(static_cast<char>(v >> (8 * i)));
}

/// gzip member around `raw`, built independently of the production code.
std::string gzipOf(const std::string& raw) {
    std::size_t n = 0;
    void* deflated = tdefl_compress_mem_to_heap(raw.data(), raw.size(), &n, 128);
    std::string gz("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\x0b", 10);
    gz.append(static_cast<const char*>(deflated), n);
    mz_free(deflated);
    putU32(gz, static_cast<unsigned>(mz_crc32(MZ_CRC32_INIT, reinterpret_cast<const unsigned char*>(raw.data()), raw.size())));
    putU32(gz, static_cast<unsigned>(raw.size()));
    return gz;
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
    write(d.path / "obra.ed$", "123");
    EXPECT_EQ(archiveSourceBytes(d.path / "obra.ed3", false), 5u);
    EXPECT_EQ(archiveSourceBytes(d.path / "obra.ed3", true), 8u);
    const auto withBackup = compressProject(d.path / "obra.ed3", d.path / "a.cyp", true);
    EXPECT_EQ(withBackup.files.size(), 2u);
    EXPECT_EQ(compressProject(d.path / "obra.ed3", d.path / "b.cyp", false).files.size(), 1u);
    const auto out = extractProject(d.path / "a.cyp", d.path / "out");
    ASSERT_TRUE(out.ok) << out.error;
    EXPECT_EQ(read(d.path / "out" / "obra.ed$"), "123");
}

TEST(ProjectArchive, WritesAGzipStreamWithTheOriginalContainer) {
    TempDir d;
    write(d.path / "obra.ed3", "hello");
    ASSERT_TRUE(compressProject(d.path / "obra.ed3", d.path / "obra.cyp", false).ok);
    const std::string gz = read(d.path / "obra.cyp");
    ASSERT_GT(gz.size(), 18u);
    EXPECT_EQ(static_cast<unsigned char>(gz[0]), 0x1f);
    EXPECT_EQ(static_cast<unsigned char>(gz[1]), 0x8b);
    EXPECT_EQ(static_cast<unsigned char>(gz[2]), 8);
    EXPECT_NE(gz.substr(0, 2), "PK");  // no longer a ZIP
    // Inflate by hand and check the header fields of the container.
    const std::size_t isize = static_cast<unsigned char>(gz[gz.size() - 4]) | (static_cast<unsigned char>(gz[gz.size() - 3]) << 8) |
                              (static_cast<unsigned char>(gz[gz.size() - 2]) << 16);
    std::string raw(isize, '\0');
    const std::size_t n = tinfl_decompress_mem_to_mem(raw.data(), raw.size(), gz.data() + 10, gz.size() - 18, 0);
    ASSERT_EQ(n, isize);
    auto u32 = [&](std::size_t at) {
        return static_cast<unsigned char>(raw[at]) | (static_cast<unsigned char>(raw[at + 1]) << 8) |
               (static_cast<unsigned char>(raw[at + 2]) << 16) | (static_cast<unsigned char>(raw[at + 3]) << 24);
    };
    EXPECT_EQ(u32(0), 3);  // container version, UTF-16LE strings
    EXPECT_EQ(u32(4), 1);
    EXPECT_EQ(u32(8), 2 * 9);  // "CypeClwon" in UTF-16LE
    EXPECT_EQ(raw.substr(raw.size() - 5), "hello");
}

TEST(ProjectArchive, ReadsTheLegacyAnsiContainer) {
    TempDir d;
    std::string raw;  // version 1: user "AB" / 7, one file "\dir\x.ed3" holding "data"
    putU32(raw, 1);
    putU32(raw, 1);
    putU32(raw, 2);
    raw += "AB";
    putU32(raw, 7);
    const std::string path = R"(\dir\x.ed3)";
    putU32(raw, 4);
    putU32(raw, 0);
    putU32(raw, static_cast<unsigned>(path.size()));
    raw += path;
    raw += "data";
    write(d.path / "old.cyp", gzipOf(raw));
    const auto r = extractProject(d.path / "old.cyp", d.path / "out");
    ASSERT_TRUE(r.ok) << r.error;
    EXPECT_EQ(read(d.path / "out" / "x.ed3"), "data");
}

TEST(ProjectArchive, HostilePathsAreReducedToTheFileName) {
    TempDir d;
    std::string raw;
    putU32(raw, 1);
    putU32(raw, 1);
    putU32(raw, 0);
    putU32(raw, 0);
    const std::string path = R"(..\..\evil.ed3)";
    putU32(raw, 1);
    putU32(raw, 0);
    putU32(raw, static_cast<unsigned>(path.size()));
    raw += path;
    raw += "x";
    write(d.path / "h.cyp", gzipOf(raw));
    ASSERT_TRUE(extractProject(d.path / "h.cyp", d.path / "out").ok);
    EXPECT_TRUE(fs::exists(d.path / "out" / "evil.ed3"));
    EXPECT_FALSE(fs::exists(d.path / "evil.ed3"));
}

TEST(ProjectArchive, RejectsCorruptStreams) {
    TempDir d;
    write(d.path / "obra.ed3", std::string(2000, 'q'));
    ASSERT_TRUE(compressProject(d.path / "obra.ed3", d.path / "obra.cyp", false).ok);
    std::string gz = read(d.path / "obra.cyp");
    gz[gz.size() / 2] ^= 0x55;
    write(d.path / "bad.cyp", gz);
    EXPECT_FALSE(extractProject(d.path / "bad.cyp", d.path / "o").ok);
    EXPECT_FALSE(fs::exists(d.path / "o"));
}

TEST(ProjectArchive, BackupNamingAndCreation) {
    EXPECT_EQ(backupPathOf("a/obra.ed3").filename().string(), "obra.ed$");
    TempDir d;
    EXPECT_TRUE(createBackup(d.path / "none.ed3"));  // nothing to preserve
    write(d.path / "obra.ed3", "v1");
    EXPECT_TRUE(backupFilesOf(d.path / "obra.ed3").empty());
    ASSERT_TRUE(createBackup(d.path / "obra.ed3"));
    write(d.path / "obra.ed3", "v2");
    EXPECT_EQ(read(d.path / "obra.ed$"), "v1");
    EXPECT_EQ(backupFilesOf(d.path / "obra.ed3").size(), 1u);
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
