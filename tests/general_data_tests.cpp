#include <gtest/gtest.h>

#include <filesystem>
#include <memory>

#include "core/general_data.h"
#include "core/project_file.h"
#include "core/project_json.h"

using namespace cc::core;
namespace fs = std::filesystem;

namespace {
GeneralData changed(const Project& p) {
    GeneralData d = generalDataOf(p);
    d.standards.timber = "Eurocódigo 5 (UE)";
    d.ground.persistentStressMPa = 0.35;
    d.actions.dynamicSeismic = true;
    d.concrete.cover.footingSide = 8.5;
    return d;
}
}  // namespace

TEST(GeneralData, ApplyKeepsStructureAndUnits) {
    Project p;
    p.structure.addNode({1, 2, 3});
    p.units.lengths = LengthUnit::Millimetres;
    applyGeneralData(p, changed(p));
    EXPECT_EQ(p.standards.timber, "Eurocódigo 5 (UE)");
    EXPECT_DOUBLE_EQ(p.ground.persistentStressMPa, 0.35);
    EXPECT_EQ(p.structure.nodes.size(), 1u);
    EXPECT_EQ(p.units.lengths, LengthUnit::Millimetres);
}

TEST(GeneralData, SameGeneralDataComparesEverySection) {
    Project p;
    EXPECT_TRUE(sameGeneralData(generalDataOf(p), generalDataOf(Project{})));
    EXPECT_FALSE(sameGeneralData(generalDataOf(p), changed(p)));
    GeneralData d = generalDataOf(p);
    d.options.foundation.footings.minDepthCm += 1;  // deep inside the options tree
    EXPECT_FALSE(sameGeneralData(generalDataOf(p), d));
}

TEST(GeneralData, EditCommandIsUndoableThroughTheStack) {
    Project p;
    const GeneralData original = generalDataOf(p);
    UndoStack stack;
    stack.setClean();
    stack.push(std::make_unique<EditGeneralDataCommand>(p, changed(p)));
    EXPECT_EQ(p.standards.timber, "Eurocódigo 5 (UE)");
    EXPECT_FALSE(stack.isClean());
    EXPECT_EQ(stack.undoText(), "Datos generales");
    stack.undo();
    EXPECT_TRUE(sameGeneralData(generalDataOf(p), original));
    EXPECT_TRUE(stack.isClean());
    stack.redo();
    EXPECT_DOUBLE_EQ(p.ground.persistentStressMPa, 0.35);
    EXPECT_EQ(project_json::validate(p), "");
}

TEST(GeneralData, EditedValuesSurviveSaveAndLoad) {
    const fs::path dir = fs::temp_directory_path() / "cc_general_data_tests";
    fs::remove_all(dir);
    fs::create_directories(dir);
    Project p;
    applyGeneralData(p, changed(p));
    ProjectDocument doc = makeDocument(p, "d");
    ASSERT_TRUE(saveProjectFile(dir / "a.ed3", doc).ok);
    const LoadResult loaded = loadProjectFile(dir / "a.ed3");
    ASSERT_TRUE(loaded.ok) << loaded.error;
    EXPECT_TRUE(sameGeneralData(generalDataOf(loaded.document.project), generalDataOf(p)));
    fs::remove_all(dir);
}
