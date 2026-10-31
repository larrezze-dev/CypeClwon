#include <gtest/gtest.h>
#include <miniz.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

#include "core/project_file.h"
#include "core/project_json.h"

using namespace cc::core;
namespace fs = std::filesystem;

namespace {

class ProjectFileTest : public ::testing::Test {
protected:
    void SetUp() override {
        dir = fs::temp_directory_path() / ("cc_project_file_tests_" + std::to_string(::testing::UnitTest::GetInstance()->random_seed()) +
                                           "_" + ::testing::UnitTest::GetInstance()->current_test_info()->name());
        fs::remove_all(dir);
        fs::create_directories(dir);
    }
    void TearDown() override {
        std::error_code ec;
        fs::remove_all(dir, ec);
    }

    /// Writes a ZIP with the given entries (name, content) straight to disk, bypassing saveProjectFile.
    void writeZip(const fs::path& path, const std::vector<std::pair<std::string, std::string>>& entries) {
        mz_zip_archive zip{};
        ASSERT_TRUE(mz_zip_writer_init_file(&zip, path.string().c_str(), 0));
        for (const auto& [name, content] : entries)
            ASSERT_TRUE(mz_zip_writer_add_mem(&zip, name.c_str(), content.data(), content.size(), MZ_DEFAULT_LEVEL));
        ASSERT_TRUE(mz_zip_writer_finalize_archive(&zip));
        mz_zip_writer_end(&zip);
    }

    static std::string manifest(int version) {
        return R"({"format":"cypeclwon.project","formatVersion":)" + std::to_string(version) +
               R"(,"appVersion":"9.9.9","description":"d","createdAt":"2026-01-01T00:00:00Z","modifiedAt":"2026-01-02T00:00:00Z"})";
    }

    static std::string dumpAll(const Project& project) {
        std::string all;
        for (const std::string& name : project_json::sectionNames()) all += name + "=" + project_json::dumpSection(project, name) + "\n";
        return all;
    }

    /// A project with at least one non-default value in every kind of field (strings, numbers, enums, arrays, nested).
    static Project customProject() {
        Project p;
        p.standards.concrete = "Eurocódigo 2 (UE)";
        p.profiles.timber.serviceClass = 2;
        p.profiles.aluminium.temper = "O/H111";
        p.concrete.cover.footingSide = 9.5;
        p.actions.dynamicSeismic = true;
        p.actions.steelFire = SteelFireMode::CriticalTemperature;
        p.ground.persistentStressMPa = 0.35;
        p.limitStates.hotRolledCode = CombinationCode::EC;
        p.piles.lateralStability = true;
        p.piles.advanced.structural.precastBasis = StrengthBasis::Absolute;
        p.micropiles.corrosionThicknessMm = 1.25;
        p.options.beams.numbering = BeamNumbering::ByLevel;
        p.options.beams.continuousNumbering.firstAxis = ContinuousBeamNumbering::Axis::Y;
        p.options.beams.continuousNumbering.xFramesAlongY = ContinuousBeamNumbering::Sense::Negative;
        p.options.beams.deflection.materials[2].instantSimple[1] = DeflectionRow::relativeLimit(400);
        p.options.beams.tables.topReinforcement.assemblyVsFirst[3][2] = true;
        p.options.beams.tables.transverse.diameters = {8, 12};
        p.options.joints.preloaded.frictionClass = 'B';
        p.options.joints.bolts.diameters[7] = true;
        p.options.foundation.footings.defaultType = "Zapata de canto constante";
        p.options.foundation.tieBeamTable = ReinforcementTableSource::Predefined;
        p.units.dimensions = LengthUnit::Millimetres;
        p.units.lengths = LengthUnit::Centimetres;
        return p;
    }

    fs::path dir;
};

}  // namespace

TEST_F(ProjectFileTest, RoundTripKeepsEverySection) {
    ProjectDocument doc = makeDocument(customProject(), "Nave industrial");
    const fs::path path = dir / "obra.ed3";
    ASSERT_TRUE(saveProjectFile(path, doc).ok);

    const LoadResult loaded = loadProjectFile(path);
    ASSERT_TRUE(loaded.ok) << loaded.error;
    EXPECT_EQ(dumpAll(loaded.document.project), dumpAll(doc.project));
    EXPECT_EQ(loaded.document.project.options.joints.preloaded.frictionClass, 'B');
    EXPECT_EQ(loaded.document.project.units.dimensions, LengthUnit::Millimetres);
    EXPECT_EQ(loaded.document.manifest.description, "Nave industrial");
    EXPECT_EQ(loaded.document.manifest.formatVersion, kProjectFormatVersion);
    EXPECT_FALSE(loaded.document.manifest.appVersion.empty());
    EXPECT_EQ(loaded.document.manifest.createdAt, doc.manifest.createdAt);
    EXPECT_TRUE(loaded.document.unknownSections.empty());
}

TEST_F(ProjectFileTest, DefaultProjectRoundTrips) {
    ProjectDocument doc = makeDocument(Project{}, "");
    const fs::path path = dir / "vacia.ed3";
    ASSERT_TRUE(saveProjectFile(path, doc).ok);
    const LoadResult loaded = loadProjectFile(path);
    ASSERT_TRUE(loaded.ok) << loaded.error;
    EXPECT_EQ(dumpAll(loaded.document.project), dumpAll(Project{}));
}

TEST_F(ProjectFileTest, ManifestIsReadWithoutLoadingTheProject) {
    ProjectDocument doc = makeDocument(Project{}, "Descripción con acentos: ñ, é");
    const fs::path path = dir / "m.ed3";
    ASSERT_TRUE(saveProjectFile(path, doc).ok);
    const ManifestResult m = readProjectManifest(path);
    ASSERT_TRUE(m.ok) << m.error;
    EXPECT_EQ(m.manifest.description, "Descripción con acentos: ñ, é");
    EXPECT_EQ(m.manifest.formatVersion, 1);
    EXPECT_EQ(m.manifest.modifiedAt, doc.manifest.modifiedAt);

    // A file whose sections are broken still yields its manifest (the list of files must not choke on it).
    writeZip(dir / "broken.ed3", {{"manifest.json", manifest(1)}, {"sections/ground.json", "{ not json"}});
    EXPECT_TRUE(readProjectManifest(dir / "broken.ed3").ok);
    EXPECT_FALSE(loadProjectFile(dir / "broken.ed3").ok);
}

TEST_F(ProjectFileTest, SaveUpdatesTheManifestAndLeavesNoTemporaryFile) {
    ProjectDocument doc = makeDocument(Project{}, "x");
    doc.manifest.createdAt = "2020-01-01T00:00:00Z";
    doc.manifest.modifiedAt = "2020-01-01T00:00:00Z";
    const fs::path path = dir / "t.ed3";
    ASSERT_TRUE(saveProjectFile(path, doc).ok);
    EXPECT_NE(doc.manifest.modifiedAt, "2020-01-01T00:00:00Z");
    EXPECT_EQ(doc.manifest.createdAt, "2020-01-01T00:00:00Z");
    EXPECT_FALSE(fs::exists(dir / "t.ed3.tmp"));
    // Overwriting an existing file works and stays loadable.
    doc.project.units.lengths = LengthUnit::Millimetres;
    ASSERT_TRUE(saveProjectFile(path, doc).ok);
    EXPECT_EQ(loadProjectFile(path).document.project.units.lengths, LengthUnit::Millimetres);
    EXPECT_FALSE(fs::exists(dir / "t.ed3.tmp"));
}

TEST_F(ProjectFileTest, FailedSaveKeepsTheExistingFile) {
    ProjectDocument doc = makeDocument(Project{}, "original");
    const fs::path path = dir / "keep.ed3";
    ASSERT_TRUE(saveProjectFile(path, doc).ok);
    const auto before = fs::file_size(path);

    // An invalid model is refused before anything is written.
    ProjectDocument bad = makeDocument(Project{}, "bad");
    bad.project.ground.persistentStressMPa = -1.0;
    const SaveResult refused = saveProjectFile(path, bad);
    EXPECT_FALSE(refused.ok);
    EXPECT_NE(refused.error.find("Terreno de cimentación"), std::string::npos) << refused.error;
    EXPECT_EQ(fs::file_size(path), before);
    EXPECT_EQ(readProjectManifest(path).manifest.description, "original");

    // A target that is a directory cannot be replaced.
    fs::create_directories(dir / "folder.ed3");
    const SaveResult blocked = saveProjectFile(dir / "folder.ed3", doc);
    EXPECT_FALSE(blocked.ok);
    EXPECT_FALSE(blocked.error.empty());
    EXPECT_FALSE(fs::exists(dir / "folder.ed3.tmp"));
}

TEST_F(ProjectFileTest, FutureFormatVersionIsRefusedWithAClearMessage) {
    writeZip(dir / "future.ed3", {{"manifest.json", manifest(kProjectFormatVersion + 1)}});
    const LoadResult r = loadProjectFile(dir / "future.ed3");
    EXPECT_FALSE(r.ok);
    EXPECT_NE(r.error.find("más reciente"), std::string::npos) << r.error;
    // The manifest is still readable so the file list can show its version.
    EXPECT_EQ(readProjectManifest(dir / "future.ed3").manifest.formatVersion, kProjectFormatVersion + 1);
}

TEST_F(ProjectFileTest, CorruptFilesAreRejected) {
    {
        std::ofstream(dir / "garbage.ed3", std::ios::binary) << "this is not a zip archive at all";
        const LoadResult r = loadProjectFile(dir / "garbage.ed3");
        EXPECT_FALSE(r.ok);
        EXPECT_NE(r.error.find("garbage.ed3"), std::string::npos);
        EXPECT_FALSE(readProjectManifest(dir / "garbage.ed3").ok);
    }
    {
        std::ofstream(dir / "empty.ed3", std::ios::binary);
        EXPECT_FALSE(loadProjectFile(dir / "empty.ed3").ok);
    }
    {  // A valid archive cut in half.
        ProjectDocument doc = makeDocument(customProject(), "t");
        ASSERT_TRUE(saveProjectFile(dir / "whole.ed3", doc).ok);
        std::ifstream in(dir / "whole.ed3", std::ios::binary);
        std::string bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        std::ofstream(dir / "cut.ed3", std::ios::binary) << bytes.substr(0, bytes.size() / 2);
        EXPECT_FALSE(loadProjectFile(dir / "cut.ed3").ok);
    }
    {  // Zip without manifest, or with a foreign manifest.
        writeZip(dir / "nomanifest.ed3", {{"sections/ground.json", "{}"}});
        EXPECT_FALSE(loadProjectFile(dir / "nomanifest.ed3").ok);
        writeZip(dir / "foreign.ed3", {{"manifest.json", R"({"format":"other","formatVersion":1})"}});
        EXPECT_FALSE(loadProjectFile(dir / "foreign.ed3").ok);
        writeZip(dir / "badmanifest.ed3", {{"manifest.json", "[1,2"}});
        EXPECT_FALSE(loadProjectFile(dir / "badmanifest.ed3").ok);
    }
    {  // Missing file.
        const LoadResult r = loadProjectFile(dir / "missing.ed3");
        EXPECT_FALSE(r.ok);
        EXPECT_NE(r.error.find("No se puede abrir"), std::string::npos);
    }
}

TEST_F(ProjectFileTest, BadSectionContentNamesTheSection) {
    writeZip(dir / "a.ed3", {{"manifest.json", manifest(1)}, {"sections/ground.json", "{ not json"}});
    LoadResult r = loadProjectFile(dir / "a.ed3");
    EXPECT_FALSE(r.ok);
    EXPECT_NE(r.error.find("Terreno de cimentación"), std::string::npos) << r.error;

    writeZip(dir / "b.ed3", {{"manifest.json", manifest(1)}, {"sections/ground.json", R"({"persistentStressMPa":"alto"})"}});
    r = loadProjectFile(dir / "b.ed3");
    EXPECT_FALSE(r.ok);
    EXPECT_NE(r.error.find("Terreno de cimentación"), std::string::npos) << r.error;

    writeZip(dir / "c.ed3", {{"manifest.json", manifest(1)}, {"sections/units.json", R"({"dimensions":"furlongs"})"}});
    r = loadProjectFile(dir / "c.ed3");
    EXPECT_FALSE(r.ok);
    EXPECT_NE(r.error.find("furlongs"), std::string::npos) << r.error;

    // Well-formed but out of range: validated like the dialogs do.
    writeZip(dir / "d.ed3", {{"manifest.json", manifest(1)}, {"sections/ground.json", R"({"persistentStressMPa":-3.0})"}});
    r = loadProjectFile(dir / "d.ed3");
    EXPECT_FALSE(r.ok);
    EXPECT_NE(r.error.find("no es válida"), std::string::npos) << r.error;
}

TEST_F(ProjectFileTest, MissingSectionsAndFieldsTakeDefaults) {
    // No "units" section at all: cm / cm / m. A partial section keeps defaults for the absent fields.
    writeZip(dir / "old.ed3", {{"manifest.json", manifest(1)}, {"sections/ground.json", R"({"checkFootingSliding":true})"}});
    const LoadResult r = loadProjectFile(dir / "old.ed3");
    ASSERT_TRUE(r.ok) << r.error;
    EXPECT_EQ(r.document.project.units, DimensioningUnits{});
    EXPECT_EQ(r.document.project.units.dimensions, LengthUnit::Centimetres);
    EXPECT_EQ(r.document.project.units.reinforcement, LengthUnit::Centimetres);
    EXPECT_EQ(r.document.project.units.lengths, LengthUnit::Metres);
    EXPECT_TRUE(r.document.project.ground.checkFootingSliding);
    EXPECT_DOUBLE_EQ(r.document.project.ground.persistentStressMPa, GroundSettings{}.persistentStressMPa);
}

TEST_F(ProjectFileTest, UnknownSectionsAndFieldsAreTolerated) {
    writeZip(dir / "new.ed3",
             {{"manifest.json", manifest(1)},
              {"sections/ground.json", R"({"checkFootingSliding":true,"fieldFromTheFuture":[1,2,3]})"},
              {"sections/future_module.json", R"({"items":[{"id":1,"x":0.0}]})"},
              {"notes.txt", "ignored"}});
    LoadResult r = loadProjectFile(dir / "new.ed3");
    ASSERT_TRUE(r.ok) << r.error;
    ASSERT_EQ(r.document.unknownSections.size(), 1u);
    EXPECT_TRUE(r.document.unknownSections.count("future_module"));

    // The unknown section survives a save and a second load.
    ASSERT_TRUE(saveProjectFile(dir / "resaved.ed3", r.document).ok);
    const LoadResult again = loadProjectFile(dir / "resaved.ed3");
    ASSERT_TRUE(again.ok) << again.error;
    ASSERT_TRUE(again.document.unknownSections.count("future_module"));
    EXPECT_EQ(again.document.unknownSections.at("future_module"), R"({"items":[{"id":1,"x":0.0}]})");
    EXPECT_TRUE(again.document.project.ground.checkFootingSliding);
}

TEST_F(ProjectFileTest, OlderFilesAreMigratedStepByStep) {
    // Pretend the current format is 3 and that version 1 stored the ground stress under another key, version 2 as kPa.
    writeZip(dir / "v1.ed3", {{"manifest.json", manifest(1)}, {"sections/ground.json", R"({"stress":0.25})"}});
    LoadOptions options;
    options.currentVersion = 3;
    options.migrations.push_back([](SectionTexts& s) {  // 1 -> 2: rename the key
        std::string& text = s["ground"];
        text = R"({"persistentStressMPa":0.25})";
    });
    options.migrations.push_back([](SectionTexts& s) {  // 2 -> 3: add a field
        s["ground"] = R"({"persistentStressMPa":0.25,"checkFootingSliding":true})";
    });
    const LoadResult r = loadProjectFile(dir / "v1.ed3", options);
    ASSERT_TRUE(r.ok) << r.error;
    EXPECT_DOUBLE_EQ(r.document.project.ground.persistentStressMPa, 0.25);
    EXPECT_TRUE(r.document.project.ground.checkFootingSliding);
    EXPECT_EQ(r.document.manifest.formatVersion, 3);

    // A gap in the chain is reported, not guessed.
    LoadOptions gap;
    gap.currentVersion = 3;
    gap.migrations.push_back(nullptr);
    EXPECT_FALSE(loadProjectFile(dir / "v1.ed3", gap).ok);
}

TEST_F(ProjectFileTest, TimestampIsIso8601Utc) {
    const std::string t = currentTimestamp();
    ASSERT_EQ(t.size(), 20u);
    EXPECT_EQ(t[4], '-');
    EXPECT_EQ(t[10], 'T');
    EXPECT_EQ(t.back(), 'Z');
}

TEST_F(ProjectFileTest, AccentedPathsWorkAndAppearAsUtf8InMessages) {
    const fs::path folder = dir / fs::path(u8"obra_ñó");
    const fs::path path = folder / fs::path(u8"Nave pequeña.ed3");
    ProjectDocument doc = makeDocument(Project{}, "acentos");
    ASSERT_TRUE(saveProjectFile(path, doc).ok);  // creates the folder
    EXPECT_TRUE(loadProjectFile(path).ok);
    EXPECT_TRUE(readProjectManifest(path).ok);
    EXPECT_FALSE(fs::exists(folder / fs::path(u8"Nave pequeña.ed3.tmp")));

    const LoadResult missing = loadProjectFile(folder / fs::path(u8"Nave inexistente.ed3"));
    EXPECT_FALSE(missing.ok);
    EXPECT_NE(missing.error.find("«Nave inexistente.ed3»"), std::string::npos) << missing.error;
    const std::string utf8Name = "«Nave inexistente.ed3»";
    EXPECT_NE(missing.error.find(utf8Name), std::string::npos);
}
