#include <gtest/gtest.h>

#include <filesystem>
#include <limits>
#include <memory>

#include "core/general_data.h"
#include "core/project_file.h"
#include "core/project_json.h"
#include "core/work_planes.h"

using namespace cc::core;
namespace fs = std::filesystem;

TEST(WorkPlanes, DefaultsAreThoseOfTheOriginal) {
    const WorkPlanes w;
    EXPECT_TRUE(w.gridX.empty());
    EXPECT_TRUE(w.gridY.empty());
    EXPECT_TRUE(w.levels.empty());
    EXPECT_TRUE(w.adjustElements);
    EXPECT_TRUE(w.isValid());
    const GridLine line = newGridLine();
    EXPECT_EQ(line.label, "");
    EXPECT_DOUBLE_EQ(line.coordinate, 0.0);
    EXPECT_EQ(line.labelPosition, LabelPosition::Origin);
    const Level level = newLevel();
    EXPECT_EQ(level.name, "Nuevo nivel");
    EXPECT_DOUBLE_EQ(level.elevation, 0.0);
    const GridSpec spec;
    EXPECT_EQ(spec.x.count, 4);
    EXPECT_DOUBLE_EQ(spec.x.spacing, 3.0);
    EXPECT_EQ(spec.x.firstLabel, "A");
    EXPECT_EQ(spec.y.firstLabel, "1");
    EXPECT_EQ(spec.y.position, LabelPosition::Origin);
}

TEST(WorkPlanes, NextReferenceFollowsLettersAndNumbers) {
    EXPECT_EQ(nextReference("A"), "B");
    EXPECT_EQ(nextReference("Y"), "Z");
    EXPECT_EQ(nextReference("Z"), "AA");
    EXPECT_EQ(nextReference("AA"), "AB");
    EXPECT_EQ(nextReference("AZ"), "BA");
    EXPECT_EQ(nextReference("ZZ"), "AAA");
    EXPECT_EQ(nextReference("c"), "d");
    EXPECT_EQ(nextReference("z"), "aa");
    EXPECT_EQ(nextReference("1"), "2");
    EXPECT_EQ(nextReference("9"), "10");
    EXPECT_EQ(nextReference("99"), "100");
    EXPECT_EQ(nextReference("09"), "10");
    EXPECT_EQ(nextReference("A9"), "A10");
    EXPECT_EQ(nextReference(""), "1");
    EXPECT_EQ(nextReference("-"), "1");
}

TEST(WorkPlanes, GridGenerationUsesDefaultsAndStartsAtZero) {
    WorkPlanes w;
    generateGrid(w, GridSpec{});
    ASSERT_EQ(w.gridX.size(), 4u);
    ASSERT_EQ(w.gridY.size(), 4u);
    const char* labelsX[] = {"A", "B", "C", "D"};
    const char* labelsY[] = {"1", "2", "3", "4"};
    for (int i = 0; i < 4; ++i) {
        EXPECT_EQ(w.gridX[i].label, labelsX[i]);
        EXPECT_EQ(w.gridY[i].label, labelsY[i]);
        EXPECT_DOUBLE_EQ(w.gridX[i].coordinate, 3.0 * i);
        EXPECT_DOUBLE_EQ(w.gridY[i].coordinate, 3.0 * i);
        EXPECT_EQ(w.gridX[i].labelPosition, LabelPosition::Origin);
    }
    EXPECT_EQ(validateGridLines(w.gridX), "");
    EXPECT_TRUE(w.isValid());
}

TEST(WorkPlanes, GridGenerationOf30LinesRunsPastZ) {
    GridSpec spec;
    spec.x.count = 30;
    spec.x.spacing = 2.5;
    spec.x.position = LabelPosition::End;
    spec.y.count = 30;
    WorkPlanes w;
    generateGrid(w, spec);
    ASSERT_EQ(w.gridX.size(), 30u);
    EXPECT_EQ(w.gridX[0].label, "A");
    EXPECT_EQ(w.gridX[25].label, "Z");
    EXPECT_EQ(w.gridX[26].label, "AA");
    EXPECT_EQ(w.gridX[29].label, "AD");
    EXPECT_DOUBLE_EQ(w.gridX[29].coordinate, 29 * 2.5);
    EXPECT_EQ(w.gridX[29].labelPosition, LabelPosition::End);
    EXPECT_EQ(w.gridY[29].label, "30");
    EXPECT_EQ(validateGridLines(w.gridX), "");
    EXPECT_EQ(validateGridLines(w.gridY), "");
}

TEST(WorkPlanes, GridGenerationRejectsBadParametersAndLeavesTheModelUntouched) {
    WorkPlanes w;
    generateGrid(w, GridSpec{});
    const WorkPlanes before = w;
    GridSpec spec;
    spec.x.count = 1;
    EXPECT_THROW(generateGrid(w, spec), std::invalid_argument);
    spec.x.count = 1001;
    EXPECT_THROW(generateGrid(w, spec), std::invalid_argument);
    spec = GridSpec{};
    spec.y.spacing = 60000.0;  // 3 * 60000 > 50000
    EXPECT_THROW(generateGrid(w, spec), std::invalid_argument);
    spec = GridSpec{};
    spec.x.spacing = std::numeric_limits<double>::quiet_NaN();
    EXPECT_THROW(generateGrid(w, spec), std::invalid_argument);
    EXPECT_EQ(w, before);
    spec = GridSpec{};
    spec.x.count = 2;
    spec.y.count = 1000;
    spec.y.spacing = 50.0;  // 999 * 50 = 49950, inside the limit
    EXPECT_NO_THROW(generateGrid(w, spec));
    EXPECT_EQ(w.gridY.size(), 1000u);
}

TEST(WorkPlanes, LevelGenerationNamesFromZeroAndAddsSpacing) {
    const std::vector<Level> levels = generateLevels("Nivel", 3, 1.5, 3.0);
    ASSERT_EQ(levels.size(), 3u);
    EXPECT_EQ(levels[0].name, "Nivel 0");
    EXPECT_EQ(levels[2].name, "Nivel 2");
    EXPECT_DOUBLE_EQ(levels[0].elevation, 1.5);
    EXPECT_DOUBLE_EQ(levels[2].elevation, 7.5);
    const std::vector<Level> defaults = generateLevels("Nivel", 2, 0.0, 3.0);
    EXPECT_EQ(defaults[1].elevation, 3.0);
    EXPECT_EQ(validateLevels(defaults), "");
    EXPECT_THROW(generateLevels("Nivel", 1, 0.0, 3.0), std::invalid_argument);
    try {
        generateLevels("Nivel", 0, 0.0, 3.0);
    } catch (const std::invalid_argument& e) {
        EXPECT_STREQ(e.what(), "Debe definir al menos dos niveles.");
    }
}

TEST(WorkPlanes, ZeroSpacingGivesLevelsThatFailValidation) {
    EXPECT_EQ(validateLevels(generateLevels("N", 2, 0.0, 0.0)), "Los niveles 'N 0' y 'N 1' tienen asignada la misma cota.");
}

TEST(WorkPlanes, GridValidationMessages) {
    std::vector<GridLine> lines = {{"A", 0.0, LabelPosition::Origin}, {"B", 3.0, LabelPosition::Origin}};
    EXPECT_EQ(validateGridLines(lines), "");
    lines.push_back({"A", 6.0, LabelPosition::End});
    EXPECT_EQ(validateGridLines(lines), "La etiqueta 'A' está repetida.");
    lines.back().label = "C";
    lines.back().coordinate = 3.0;
    EXPECT_EQ(validateGridLines(lines), "Las líneas 'B' y 'C' están situadas en la misma coordenada.");
    lines.back().coordinate = 50001.0;
    EXPECT_NE(validateGridLines(lines), "");
    // New lines have an empty label and coordinate 0: several empty labels are not reported as repeated.
    std::vector<GridLine> fresh = {newGridLine(), newGridLine()};
    fresh[1].coordinate = 1.0;
    EXPECT_EQ(validateGridLines(fresh), "");
    fresh[1].coordinate = 0.0;
    EXPECT_NE(validateGridLines(fresh), "");
}

TEST(WorkPlanes, LevelValidationMessages) {
    EXPECT_EQ(validateLevels({}), "Debe definir al menos dos niveles.");
    EXPECT_EQ(validateLevels({{"A", 0.0}}), "Debe definir al menos dos niveles.");
    EXPECT_EQ(validateLevels({{"A", 0.0}, {"A", 3.0}}), "La referencia 'A' está repetida.");
    EXPECT_EQ(validateLevels({{"A", 3.0}, {"B", 3.0}}), "Los niveles 'A' y 'B' tienen asignada la misma cota.");
    EXPECT_EQ(validateLevels({{"A", 0.0}, {"B", -3.0}}), "") << "negative elevations are allowed";
}

TEST(WorkPlanes, LevelsAreShownSortedByElevationAndGridKeepsInsertionOrder) {
    WorkPlanes w;
    w.levels = {{"Cubierta", 6.0}, {"Planta baja", 0.0}, {"Planta 1", 3.0}};
    const std::vector<Level> shown = sortedLevels(w.levels);
    EXPECT_EQ(shown[0].name, "Planta baja");
    EXPECT_EQ(shown[2].name, "Cubierta");
    EXPECT_EQ(w.levels[0].name, "Cubierta") << "the stored order is untouched";
    w.gridX = {{"B", 3.0, LabelPosition::Origin}, {"A", 0.0, LabelPosition::Origin}};
    EXPECT_EQ(w.gridX[0].label, "B");
    EXPECT_EQ(sortedGridLines(w.gridX)[0].label, "A");
}

TEST(WorkPlanes, IsValidAcceptsNoLevelsButNotOne) {
    WorkPlanes w;
    EXPECT_TRUE(w.isValid());
    w.levels = {{"A", 0.0}};
    EXPECT_FALSE(w.isValid());
    w.levels.push_back({"B", 3.0});
    EXPECT_TRUE(w.isValid());
    w.groups.x = {{0.0, 3.0}, {3.0}};  // a plane in two groups
    EXPECT_FALSE(w.isValid());
    w.groups.x = {{0.0, 3.0}, {6.0}};
    EXPECT_TRUE(w.isValid());
}

TEST(WorkPlanes, JsonRoundTrip) {
    WorkPlanes w;
    GridSpec spec;
    spec.x.position = LabelPosition::End;
    generateGrid(w, spec);
    w.levels = generateLevels("Nivel", 3, 0.0, 3.2);
    w.adjustElements = false;
    w.groups.y = {{0.0, 3.0}, {6.0}};
    w.groups.z = {{3.2}};
    const auto back = workPlanesFromJson(toJson(w));
    ASSERT_TRUE(back.has_value());
    EXPECT_EQ(*back, w);
}

TEST(WorkPlanes, JsonIsTolerantAboutMissingFieldsAndStrictAboutTypes) {
    const auto empty = workPlanesFromJson("{}");
    ASSERT_TRUE(empty.has_value());
    EXPECT_EQ(*empty, WorkPlanes{});
    const auto partial = workPlanesFromJson("{\"levels\":[{\"name\":\"N\"}],\"future\":1}");
    ASSERT_TRUE(partial.has_value());
    EXPECT_EQ(partial->levels.size(), 1u);
    EXPECT_DOUBLE_EQ(partial->levels[0].elevation, 0.0);
    EXPECT_FALSE(workPlanesFromJson("[1]").has_value());
    EXPECT_FALSE(workPlanesFromJson("{").has_value());
    EXPECT_FALSE(workPlanesFromJson("{\"gridX\":{}}").has_value());
    EXPECT_FALSE(workPlanesFromJson("{\"gridX\":[{\"labelPosition\":\"middle\"}]}").has_value());
    EXPECT_FALSE(workPlanesFromJson("{\"gridX\":[{\"coordinate\":\"a\"}]}").has_value());
    EXPECT_FALSE(workPlanesFromJson("{\"groups\":{\"x\":[1]}}").has_value());
    EXPECT_FALSE(workPlanesFromJson("{\"adjustElements\":3}").has_value());
}

TEST(WorkPlanes, SectionIsPartOfTheProjectAndValidated) {
    EXPECT_TRUE(project_json::isKnownSection("work_planes"));
    EXPECT_EQ(project_json::sectionTitle("work_planes"), "Planos");
    Project p;
    generateGrid(p.workPlanes, GridSpec{});
    p.workPlanes.levels = generateLevels("Nivel", 2, 0.0, 3.0);
    Project loaded;
    project_json::loadSection(loaded, "work_planes", project_json::dumpSection(p, "work_planes"));
    EXPECT_EQ(loaded.workPlanes, p.workPlanes);
    EXPECT_EQ(project_json::validate(loaded), "");
    loaded.workPlanes.gridX[1].coordinate = loaded.workPlanes.gridX[0].coordinate;
    EXPECT_EQ(project_json::validate(loaded), "La sección «Planos» contiene valores no válidos.");
    EXPECT_THROW(project_json::loadSection(loaded, "work_planes", "[]"), std::runtime_error);
}

TEST(WorkPlanes, EditCommandIsUndoableAndNamed) {
    Project p;
    UndoStack stack;
    stack.setClean();
    WorkPlanes edited = p.workPlanes;
    generateGrid(edited, GridSpec{});
    stack.push(std::make_unique<EditWorkPlanesCommand>(p, edited, WorkPlanesEdit::Grid));
    EXPECT_EQ(p.workPlanes.gridX.size(), 4u);
    EXPECT_EQ(stack.undoText(), "Rejilla");
    EXPECT_FALSE(stack.isClean());
    WorkPlanes withLevels = p.workPlanes;
    withLevels.levels = generateLevels("Nivel", 2, 0.0, 3.0);
    stack.push(std::make_unique<EditWorkPlanesCommand>(p, withLevels, WorkPlanesEdit::Levels));
    EXPECT_EQ(stack.undoText(), "Niveles");
    EXPECT_EQ(p.workPlanes.levels.size(), 2u);
    stack.undo();
    EXPECT_TRUE(p.workPlanes.levels.empty());
    EXPECT_EQ(p.workPlanes.gridX.size(), 4u) << "the grid edit is still applied";
    stack.undo();
    EXPECT_EQ(p.workPlanes, WorkPlanes{});
    EXPECT_TRUE(stack.isClean());
    stack.redo();
    stack.redo();
    EXPECT_EQ(p.workPlanes, withLevels);
}

TEST(WorkPlanes, SurvivesSaveAndLoadAndAbsentSectionMeansEmpty) {
    const fs::path dir = fs::temp_directory_path() / "cc_work_planes_tests";
    fs::remove_all(dir);
    fs::create_directories(dir);
    Project p;
    generateGrid(p.workPlanes, GridSpec{});
    p.workPlanes.levels = generateLevels("Nivel", 4, 0.0, 3.0);
    p.workPlanes.gridY[0].labelPosition = LabelPosition::End;
    ProjectDocument doc = makeDocument(p, "d");
    ASSERT_TRUE(saveProjectFile(dir / "a.ed3", doc).ok);
    const LoadResult loaded = loadProjectFile(dir / "a.ed3");
    ASSERT_TRUE(loaded.ok) << loaded.error;
    EXPECT_EQ(loaded.document.project.workPlanes, p.workPlanes);

    // A project that never set the work planes loads them empty (and keeps the format version).
    Project empty;
    ProjectDocument emptyDoc = makeDocument(empty, "d");
    ASSERT_TRUE(saveProjectFile(dir / "b.ed3", emptyDoc).ok);
    const LoadResult again = loadProjectFile(dir / "b.ed3");
    ASSERT_TRUE(again.ok) << again.error;
    EXPECT_EQ(again.document.project.workPlanes, WorkPlanes{});
    fs::remove_all(dir);
}
