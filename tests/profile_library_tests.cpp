#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

#include "core/predefined_profiles.h"
#include "core/profile_library.h"
#include "core/project_json.h"

using namespace cc::core;

namespace {

Section flat(const std::string& reference, double h = 0.1, double b = 0.01) {
    Section s;
    s.reference = reference;
    s.kind = ShapeKind::FlatBar;
    s.dims = {h, b};
    return s;
}

ProfileSeries flats(const std::string& reference, std::vector<std::string> refs = {"P1", "P2", "P3"}) {
    ProfileSeries s;
    s.reference = reference;
    s.description = "Pletinas de prueba";
    s.kind = ShapeKind::FlatBar;
    for (const std::string& r : refs) s.profiles.push_back(flat(r));
    return s;
}

std::vector<std::string> refs(const ProfileSeries& s) {
    std::vector<std::string> out;
    for (const Section& p : s.profiles) out.push_back(p.reference);
    return out;
}

}  // namespace

TEST(ProfileSeries, ValidationUsesTheOriginalMessages) {
    ProfileSeries s = flats("PL");
    EXPECT_EQ(validateSeries(s), "");
    s.reference.clear();
    EXPECT_EQ(validateSeries(s), "Debe introducir una referencia para la serie de perfiles.");
    s = flats("PL", {});
    EXPECT_EQ(validateSeries(s), "Debe introducir, al menos, un perfil en la serie.");
    s = flats("PL", {"A", ""});
    EXPECT_EQ(validateSeries(s), "Debe introducir una referencia para el perfil.");
    s = flats("PL", {"A", "B", "A"});
    EXPECT_EQ(validateSeries(s), "El perfil con referencia 'A' está repetido.");
    s = flats("PL");
    s.profiles[1].kind = ShapeKind::RoundBar;
    EXPECT_NE(validateSeries(s).find("no tiene la forma de la serie"), std::string::npos);
    s = flats("PL");
    s.profiles[0].dims = {0.1, -1};
    EXPECT_NE(validateSeries(s).find("Perfil 'P1'"), std::string::npos);

    ProfileLibrary lib;
    lib.series = {flats("A"), flats("B"), flats("A")};
    EXPECT_EQ(validateLibrary(lib), "La serie de perfiles 'A' está repetida.");
}

TEST(ProfileLibrary, SeriesEditingIsUndoable) {
    Project project;
    UndoStack stack;
    ASSERT_EQ(addSeries(project, stack, flats("A")), "");
    ASSERT_EQ(addSeries(project, stack, flats("B")), "");
    EXPECT_EQ(addSeries(project, stack, flats("A")), "La serie de perfiles 'A' está repetida.");
    EXPECT_EQ(project.profileLibrary.series.size(), 2u);

    ASSERT_EQ(moveSeries(project, stack, "B", -1), "");
    EXPECT_EQ(project.profileLibrary.series[0].reference, "B");
    EXPECT_NE(moveSeries(project, stack, "B", -1), "");  // already first
    EXPECT_NE(moveSeries(project, stack, "A", +1), "");  // already last
    ASSERT_EQ(moveSeries(project, stack, "B", +1), "");

    ProfileSeries edited = *project.profileLibrary.find("A");
    edited.reference = "A2";
    edited.description = "otra";
    ASSERT_EQ(editSeries(project, stack, "A", edited), "");
    EXPECT_EQ(project.profileLibrary.series[0].reference, "A2");
    edited.reference = "B";
    EXPECT_EQ(editSeries(project, stack, "A2", edited), "La serie de perfiles 'B' está repetida.");
    EXPECT_EQ(editSeries(project, stack, "zz", edited), "La serie de perfiles no existe en la biblioteca.");

    ASSERT_EQ(duplicateSeries(project, stack, "A2"), "");
    EXPECT_EQ(project.profileLibrary.series.back().reference, "A2 (copia)");
    ASSERT_EQ(duplicateSeries(project, stack, "A2"), "");
    EXPECT_EQ(project.profileLibrary.series.back().reference, "A2 (copia 2)");
    ASSERT_EQ(duplicateSeries(project, stack, "A2", "mia"), "");
    EXPECT_EQ(project.profileLibrary.series.back().reference, "mia");

    const ProfileLibrary full = project.profileLibrary;
    ASSERT_EQ(removeSeries(project, stack, "B"), "");
    EXPECT_EQ(project.profileLibrary.find("B"), nullptr);
    stack.undo();
    EXPECT_EQ(project.profileLibrary, full);
    stack.redo();
    EXPECT_EQ(project.profileLibrary.find("B"), nullptr);
    while (stack.canUndo()) stack.undo();
    EXPECT_TRUE(project.profileLibrary.series.empty());
    while (stack.canRedo()) stack.redo();
    EXPECT_EQ(project.profileLibrary.series.size(), 4u);  // the last command was the removal of B
    EXPECT_NE(removeSeries(project, stack, "nope"), "");
}

TEST(ProfileLibrary, ProfileEditingKeepsOrderAndRules) {
    Project project;
    UndoStack stack;
    ASSERT_EQ(addSeries(project, stack, flats("A")), "");
    const auto names = [&] { return refs(*project.profileLibrary.find("A")); };

    ASSERT_EQ(addProfile(project, stack, "A", flat("P4")), "");
    ASSERT_EQ(addProfile(project, stack, "A", flat("P0"), 0), "");
    EXPECT_EQ(names(), (std::vector<std::string>{"P0", "P1", "P2", "P3", "P4"}));
    EXPECT_EQ(addProfile(project, stack, "A", flat("P2")), "El perfil con referencia 'P2' está repetido.");
    Section wrongShape = flat("R");
    wrongShape.kind = ShapeKind::RoundBar;
    wrongShape.dims = {0.05};
    EXPECT_NE(addProfile(project, stack, "A", wrongShape), "");
    EXPECT_NE(addProfile(project, stack, "zz", flat("Q")), "");

    ASSERT_EQ(moveProfile(project, stack, "A", "P4", -1), "");
    EXPECT_EQ(names(), (std::vector<std::string>{"P0", "P1", "P2", "P4", "P3"}));
    EXPECT_NE(moveProfile(project, stack, "A", "P0", -1), "");
    ASSERT_EQ(duplicateProfile(project, stack, "A", "P1"), "");
    EXPECT_EQ(names(), (std::vector<std::string>{"P0", "P1", "P1 (copia)", "P2", "P4", "P3"}));
    ASSERT_EQ(duplicateProfile(project, stack, "A", "P1", "otro"), "");
    EXPECT_EQ(names()[2], "otro");

    Section changed = flat("P2", 0.2, 0.02);
    ASSERT_EQ(editProfile(project, stack, "A", "P2", changed), "");
    EXPECT_DOUBLE_EQ(project.profileLibrary.find("A")->findProfile("P2")->dims[0], 0.2);
    EXPECT_EQ(editProfile(project, stack, "A", "nope", changed), "La referencia del perfil no existe.");
    EXPECT_EQ(editProfile(project, stack, "A", "P2", flat("P3")), "El perfil con referencia 'P3' está repetido.");

    ASSERT_EQ(removeProfile(project, stack, "A", "P0"), "");
    EXPECT_EQ(project.profileLibrary.find("A")->findProfile("P0"), nullptr);
    stack.undo();
    EXPECT_NE(project.profileLibrary.find("A")->findProfile("P0"), nullptr);
}

TEST(ProfileLibrary, LastProfileCannotBeRemoved) {
    Project project;
    UndoStack stack;
    ASSERT_EQ(addSeries(project, stack, flats("A", {"only"})), "");
    EXPECT_EQ(removeProfile(project, stack, "A", "only"), "Debe introducir, al menos, un perfil en la serie.");
    EXPECT_EQ(project.profileLibrary.find("A")->profiles.size(), 1u);
    EXPECT_EQ(stack.count(), 1u);  // nothing pushed by the refusal
}

TEST(ProfileLibrary, ImportKeepsOrReplacesAsOneUndoStep) {
    Project project;
    UndoStack stack;
    ASSERT_EQ(addSeries(project, stack, flats("A", {"old"})), "");
    const std::vector<ProfileSeries> incoming = {flats("A", {"new1", "new2"}), flats("B")};
    int imported = 0;
    ASSERT_EQ(importSeries(project, stack, incoming, ImportPolicy::KeepExisting, &imported), "");
    EXPECT_EQ(imported, 1);
    EXPECT_EQ(refs(*project.profileLibrary.find("A")), (std::vector<std::string>{"old"}));
    EXPECT_NE(project.profileLibrary.find("B"), nullptr);
    stack.undo();
    EXPECT_EQ(project.profileLibrary.series.size(), 1u);

    ASSERT_EQ(importSeries(project, stack, incoming, ImportPolicy::ReplaceExisting, &imported), "");
    EXPECT_EQ(imported, 2);
    EXPECT_EQ(refs(*project.profileLibrary.find("A")), (std::vector<std::string>{"new1", "new2"}));
    EXPECT_EQ(project.profileLibrary.series[0].reference, "A");  // replaced in place
    EXPECT_EQ(importSeries(project, stack, {}, ImportPolicy::KeepExisting), "Debe seleccionar, al menos, una serie de perfiles.");
    EXPECT_EQ(importSeries(project, stack, incoming, ImportPolicy::KeepExisting),
              "Todas las series ya existen en la biblioteca de perfiles de la obra.");
    ProfileSeries bad = flats("C", {});
    EXPECT_EQ(importSeries(project, stack, {flats("D"), bad}, ImportPolicy::KeepExisting),
              "Debe introducir, al menos, un perfil en la serie.");
    EXPECT_EQ(project.profileLibrary.find("D"), nullptr);  // nothing imported on error
}

TEST(ProfileLibrary, PredefinedLibraryImportsAndSurvivesTheProjectJson) {
    Project project;
    UndoStack stack;
    int imported = 0;
    ASSERT_EQ(importSeries(project, stack, predefinedSeries(), ImportPolicy::KeepExisting, &imported), "");
    EXPECT_EQ(imported, static_cast<int>(predefinedSeries().size()));  // 25 series: IPE, HEA, HEB, HEM, IPN, UPN, L + 18 more of the original
    ASSERT_EQ(validateLibrary(project.profileLibrary), "");
    EXPECT_EQ(project_json::validate(project), "");
    EXPECT_TRUE(project_json::isKnownSection("profile_library"));
    EXPECT_EQ(project_json::sectionTitle("profile_library"), "Biblioteca de perfiles");

    // a user profile with tabulated values must round-trip too
    Section g = flat("T1");
    g.tabulated.area = 0.0012;
    g.tabulated.avz = 0.0009;
    EXPECT_NE(addProfile(project, stack, "IPE", g), "");  // wrong shape for IPE -> refused
    ProfileSeries mine = flats("Mia");
    mine.profiles[0].tabulated.area = 0.0012;
    mine.profiles[0].tabulated.iw = 3.5e-9;
    ASSERT_EQ(addSeries(project, stack, mine), "");

    Project loaded;
    project_json::loadSection(loaded, "profile_library", project_json::dumpSection(project, "profile_library"));
    EXPECT_EQ(loaded.profileLibrary, project.profileLibrary);
    EXPECT_THROW(project_json::loadSection(loaded, "profile_library", "{\"series\": [{\"reference\": \"x\"}]}"), std::runtime_error);
    EXPECT_THROW(project_json::loadSection(loaded, "profile_library", "[]"), std::runtime_error);
}

TEST(ProfileLibrary, ExportAndImportOfSeriesFiles) {
    const std::vector<ProfileSeries> series = {predefinedSeries()[0], flats("Mia")};
    const std::string text = exportSeriesToJson(series);
    const SeriesImport back = importSeriesFromJson(text);
    ASSERT_EQ(back.error, "");
    EXPECT_EQ(back.series, series);

    EXPECT_NE(importSeriesFromJson("not json").error, "");
    EXPECT_NE(importSeriesFromJson("{\"format\":\"other\",\"series\":[]}").error, "");
    EXPECT_NE(importSeriesFromJson("{\"format\":\"cype-clone-profile-series\",\"version\":99,\"series\":[]}").error, "");
    EXPECT_NE(importSeriesFromJson("{\"format\":\"cype-clone-profile-series\",\"version\":1,\"series\":[]}").error, "");

    const auto dir = std::filesystem::temp_directory_path() / "cc_profile_series_test";
    std::filesystem::create_directories(dir);
    const std::string path = (dir / "series.json").string();
    std::filesystem::remove(path);
    ASSERT_EQ(exportSeriesToFile(path, series), "");
    EXPECT_NE(exportSeriesToFile(path, series), "");  // exists: refuse
    EXPECT_EQ(exportSeriesToFile(path, series, true), "");
    EXPECT_EQ(exportSeriesToFile(path, {}), "Debe seleccionar, al menos, una serie de perfiles.");
    const SeriesImport fromFile = importSeriesFromFile(path);
    ASSERT_EQ(fromFile.error, "");
    EXPECT_EQ(fromFile.series, series);
    EXPECT_NE(importSeriesFromFile((dir / "missing.json").string()).error.find("no existe o no se puede leer"), std::string::npos);
    std::filesystem::remove_all(dir);
}
