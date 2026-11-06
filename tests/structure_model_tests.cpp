#include <gtest/gtest.h>

#include <cmath>
#include <filesystem>
#include <limits>

#include "core/project_file.h"
#include "core/project_json.h"
#include "core/structure_model.h"

using namespace cc::core;
namespace fs = std::filesystem;

namespace {

/// Two nodes, one material, one section and a bar between the nodes.
StructuralModel simpleFrame() {
    StructuralModel m;
    const int a = m.addNode({0, 0, 0});
    const int b = m.addNode({0, 0, 3});
    Material steel;
    steel.name = "S275";
    steel.kind = MaterialKind::Steel;
    const int mat = m.addMaterial(steel);
    CrossSection ipe;
    ipe.name = "IPE 200";
    ipe.kind = MaterialKind::Steel;
    ipe.areaM2 = 28.5e-4;
    ipe.inertiaYM4 = 1943e-8;
    ipe.inertiaZM4 = 142e-8;
    ipe.torsionM4 = 6.98e-8;
    const int sec = m.addSection(ipe);
    EXPECT_NE(m.addBar(a, b, mat, sec), 0);
    return m;
}

}  // namespace

TEST(StructureModel, StartsEmptyAndValid) {
    StructuralModel m;
    EXPECT_TRUE(m.empty());
    EXPECT_EQ(m.validate(), "");
}

TEST(StructureModel, IdsAreSequentialAndNeverReused) {
    StructuralModel m;
    EXPECT_EQ(m.addNode({0, 0, 0}), 1);
    EXPECT_EQ(m.addNode({1, 0, 0}), 2);
    EXPECT_TRUE(m.removeNode(2));
    EXPECT_EQ(m.addNode({2, 0, 0}), 3) << "a deleted id must not come back";
    ASSERT_NE(m.findNode(3), nullptr);
    EXPECT_EQ(m.findNode(2), nullptr);
}

TEST(StructureModel, RejectsInvalidNodes) {
    StructuralModel m;
    EXPECT_EQ(m.addNode({std::numeric_limits<double>::quiet_NaN(), 0, 0}), 0);
    EXPECT_EQ(m.addNode({0, std::numeric_limits<double>::infinity(), 0}), 0);
    EXPECT_EQ(m.addNode({2e6, 0, 0}), 0);
    EXPECT_TRUE(m.nodes.empty());
    EXPECT_FALSE(m.insertNode({0, {0, 0, 0}}));
    EXPECT_TRUE(m.insertNode({7, {0, 0, 0}}));
    EXPECT_FALSE(m.insertNode({7, {1, 1, 1}})) << "duplicated id";
    EXPECT_EQ(m.addNode({1, 1, 1}), 8) << "the counter moves past an inserted id";
}

TEST(StructureModel, NodesStaySortedWhenInsertedOutOfOrder) {
    StructuralModel m;
    ASSERT_TRUE(m.insertNode({5, {0, 0, 0}}));
    ASSERT_TRUE(m.insertNode({2, {1, 0, 0}}));
    ASSERT_TRUE(m.insertNode({9, {2, 0, 0}}));
    ASSERT_EQ(m.nodes.size(), 3u);
    EXPECT_EQ(m.nodes[0].id, 2);
    EXPECT_EQ(m.nodes[2].id, 9);
    EXPECT_EQ(m.validate(), "");
}

TEST(StructureModel, BarLengthAndLookup) {
    StructuralModel m = simpleFrame();
    ASSERT_EQ(m.bars.size(), 1u);
    EXPECT_DOUBLE_EQ(m.barLength(1), 3.0);
    EXPECT_DOUBLE_EQ(m.barLength(99), 0.0);
    EXPECT_EQ(m.findBar(1)->startNode, 1);
}

TEST(StructureModel, RejectsInvalidBars) {
    StructuralModel m = simpleFrame();
    const int c = m.addNode({0, 0, 3});  // coincides with node 2
    EXPECT_EQ(m.addBar(1, 1), 0) << "same node twice";
    EXPECT_EQ(m.addBar(1, 99), 0) << "missing node";
    EXPECT_EQ(m.addBar(2, c), 0) << "zero length";
    EXPECT_EQ(m.addBar(1, 2, 99), 0) << "missing material";
    EXPECT_EQ(m.addBar(1, 2, 0, 99), 0) << "missing section";
    EXPECT_EQ(m.bars.size(), 1u);
    EXPECT_NE(m.addBar(1, 2), 0) << "material and section may stay unassigned";
}

TEST(StructureModel, ShellsNeedThreeOrFourDistinctExistingNodes) {
    StructuralModel m;
    const int a = m.addNode({0, 0, 0}), b = m.addNode({1, 0, 0}), c = m.addNode({1, 1, 0}), d = m.addNode({0, 1, 0});
    EXPECT_EQ(m.addShell({a, b}, 0.2), 0);
    EXPECT_EQ(m.addShell({a, b, c, d, a}, 0.2), 0);
    EXPECT_EQ(m.addShell({a, b, b}, 0.2), 0) << "repeated node";
    EXPECT_EQ(m.addShell({a, b, 99}, 0.2), 0) << "missing node";
    EXPECT_EQ(m.addShell({a, b, c}, 0.0), 0) << "zero thickness";
    EXPECT_EQ(m.addShell({a, b, c}, 0.2), 1);
    EXPECT_EQ(m.addShell({a, b, c, d}, 0.25), 2);
    EXPECT_EQ(m.validate(), "");
}

TEST(StructureModel, ReferencedObjectsCannotBeRemoved) {
    StructuralModel m = simpleFrame();
    const int c = m.addNode({1, 0, 0}), d = m.addNode({1, 1, 0});
    ASSERT_NE(m.addShell({1, c, d}, 0.2, 1), 0);

    EXPECT_FALSE(m.removeNode(1)) << "used by the bar and the shell";
    EXPECT_FALSE(m.removeMaterial(1)) << "used by the bar and the shell";
    EXPECT_FALSE(m.removeSection(1)) << "used by the bar";
    EXPECT_EQ(m.barsUsingNode(1), std::vector<int>{1});
    EXPECT_EQ(m.shellsUsingNode(1), std::vector<int>{1});

    // A cascading deletion removes the dependants first, then the node.
    for (int id : m.barsUsingNode(1)) EXPECT_TRUE(m.removeBar(id));
    for (int id : m.shellsUsingNode(1)) EXPECT_TRUE(m.removeShell(id));
    EXPECT_TRUE(m.removeNode(1));
    EXPECT_TRUE(m.removeSection(1));
    EXPECT_TRUE(m.removeMaterial(1));
    EXPECT_EQ(m.validate(), "");
    EXPECT_FALSE(m.removeNode(1)) << "already gone";
}

TEST(StructureModel, RejectsInvalidMaterialsAndSections) {
    StructuralModel m;
    Material bad;
    bad.elasticModulusMPa = 0.0;
    EXPECT_EQ(m.addMaterial(bad), 0);
    bad = Material{};
    bad.poissonRatio = -1.0;  // the original has no upper cap (timber nu = E/(2G)-1 is about 7)
    EXPECT_EQ(m.addMaterial(bad), 0);
    CrossSection s;
    EXPECT_EQ(m.addSection(s), 0) << "zero area";
    s.areaM2 = 1e-3;
    s.inertiaYM4 = -1.0;
    EXPECT_EQ(m.addSection(s), 0) << "negative inertia";
    EXPECT_TRUE(m.empty());
    EXPECT_NEAR(Material{}.shearModulusMPa(), 210000.0 / 2.6, 1e-9);
}

TEST(StructureModel, ValidateDetectsCorruptModels) {
    {
        StructuralModel m = simpleFrame();
        m.bars[0].endNode = 42;
        EXPECT_NE(m.validate(), "");
    }
    {
        StructuralModel m = simpleFrame();
        m.nodes[1].id = 1;  // duplicated
        EXPECT_NE(m.validate(), "");
    }
    {
        StructuralModel m = simpleFrame();
        m.nextNodeId = 1;  // would reuse an id
        EXPECT_NE(m.validate(), "");
    }
    {
        StructuralModel m = simpleFrame();
        m.nodes[1].position = m.nodes[0].position;  // zero-length bar
        EXPECT_NE(m.validate(), "");
    }
    {
        StructuralModel m = simpleFrame();
        m.bars[0].materialId = 5;
        EXPECT_NE(m.validate(), "");
    }
}

// --- persistence in the .ed3 file ---

namespace {

Project projectWithStructure() {
    Project p;
    p.structure = simpleFrame();
    const int c = p.structure.addNode({4.5, -2.25, 0});
    const int d = p.structure.addNode({4.5, 2.25, 0});
    p.structure.addShell({1, c, d}, 0.18, 1);
    Bar& bar = p.structure.bars[0];
    bar.rollAngleDeg = 90.0;
    bar.xAxisInverted = true;
    p.structure.shells[0].zAxisInverted = true;
    // Leave a gap in the ids so the counters matter.
    const int extra = p.structure.addNode({9, 9, 9});
    EXPECT_TRUE(p.structure.removeNode(extra));
    return p;
}

}  // namespace

TEST(StructureProject, SectionsAreRegisteredAfterTheExistingOnes) {
    const auto& names = project_json::sectionNames();
    ASSERT_GE(names.size(), 9u);
    EXPECT_EQ(names[names.size() - 9], "nodes");
    EXPECT_EQ(names[names.size() - 4], "pieces");
    EXPECT_EQ(names[names.size() - 3], "layers");
    EXPECT_EQ(names[names.size() - 2], "continuous_elements");
    EXPECT_EQ(names.back(), "bar_descriptions");
    EXPECT_EQ(project_json::sectionTitle("cross_sections"), "Secciones");
}

TEST(StructureProject, JsonRoundTripPreservesEverything) {
    const Project original = projectWithStructure();
    Project loaded;
    for (const std::string& name : {"nodes", "materials", "cross_sections", "bars", "shells"})
        project_json::loadSection(loaded, name, project_json::dumpSection(original, name));
    EXPECT_EQ(project_json::validate(loaded), "");
    const StructuralModel &a = original.structure, &b = loaded.structure;
    ASSERT_EQ(a.nodes.size(), b.nodes.size());
    for (std::size_t i = 0; i < a.nodes.size(); ++i) {
        EXPECT_EQ(a.nodes[i].id, b.nodes[i].id);
        EXPECT_EQ(a.nodes[i].position, b.nodes[i].position);
    }
    EXPECT_EQ(a.materials, b.materials);
    EXPECT_EQ(a.sections, b.sections);
    EXPECT_EQ(a.bars, b.bars);
    EXPECT_EQ(a.shells, b.shells);
    EXPECT_EQ(a.nextNodeId, b.nextNodeId) << "the id of the deleted node must stay burnt";
    EXPECT_EQ(b.nextNodeId, 6);
}

TEST(StructureProject, JsonUsesNamedEnumsAndRejectsUnknownOnes) {
    const Project p = projectWithStructure();
    EXPECT_NE(project_json::dumpSection(p, "materials").find("\"steel\""), std::string::npos);
    Project q;
    EXPECT_THROW(project_json::loadSection(q, "materials", R"({"items":[{"id":1,"kind":"unobtainium"}]})"), std::runtime_error);
    EXPECT_THROW(project_json::loadSection(q, "nodes", R"({"items":[{"id":"x"}]})"), std::runtime_error);
}

TEST(StructureProject, MissingCounterIsRebuiltFromTheIds) {
    Project q;
    project_json::loadSection(q, "nodes", R"({"items":[{"id":3,"position":{"x":1,"y":2,"z":3}},{"id":8}]})");
    EXPECT_EQ(q.structure.nextNodeId, 9);
    EXPECT_EQ(q.structure.nodes[0].position.z, 3.0);
    EXPECT_EQ(project_json::validate(q), "");
}

TEST(StructureProject, ProjectValidationCatchesDanglingReferencesAcrossSections) {
    Project q;
    project_json::loadSection(q, "bars", R"({"items":[{"id":1,"startNode":1,"endNode":2}]})");
    EXPECT_NE(project_json::validate(q), "") << "the bar refers to nodes that are not in the file";
}

TEST(StructureProject, FileRoundTripAndOldFilesWithoutStructure) {
    const fs::path dir = fs::temp_directory_path() / "cc_structure_tests";
    fs::remove_all(dir);
    fs::create_directories(dir);
    const fs::path file = dir / "frame.ed3";

    ProjectDocument doc = makeDocument(projectWithStructure(), "pórtico");
    ASSERT_TRUE(saveProjectFile(file, doc).ok);
    LoadResult loaded = loadProjectFile(file);
    ASSERT_TRUE(loaded.ok) << loaded.error;
    EXPECT_EQ(loaded.document.project.structure.bars, doc.project.structure.bars);
    EXPECT_EQ(loaded.document.project.structure.shells, doc.project.structure.shells);
    EXPECT_EQ(loaded.document.project.structure.nodes.size(), 4u);
    EXPECT_TRUE(loaded.document.unknownSections.empty()) << "the new sections are known, not preserved blindly";

    // A project saved without structure (as before T-P3-01) opens with an empty model.
    ProjectDocument empty = makeDocument(Project{}, "vacía");
    ASSERT_TRUE(saveProjectFile(dir / "empty.ed3", empty).ok);
    LoadResult e = loadProjectFile(dir / "empty.ed3");
    ASSERT_TRUE(e.ok) << e.error;
    EXPECT_TRUE(e.document.project.structure.empty());
    fs::remove_all(dir);
}

// --- layers (T-P4-06) ---

namespace {
/// simpleFrame() plus a second bar and a shell, so layer operations have several elements to work with.
StructuralModel frameWithShell(int& bar2, int& shell) {
    StructuralModel m = simpleFrame();
    const int c = m.addNode({3, 0, 3});
    const int d = m.addNode({3, 0, 0});
    bar2 = m.addBar(m.nodes[1].id, c, 1, 1);
    shell = m.addShell({m.nodes[0].id, m.nodes[1].id, c, d}, 0.2);
    EXPECT_NE(bar2, 0);
    EXPECT_NE(shell, 0);
    return m;
}
}  // namespace

TEST(Layers, StartWithOneActiveLayer) {
    StructuralModel m;
    ASSERT_EQ(m.layers.size(), 1u);
    EXPECT_EQ(m.layers[0].id, kDefaultLayerId);
    EXPECT_EQ(m.layers[0].name, "por defecto");
    EXPECT_FALSE(m.layers[0].rendered);
    EXPECT_EQ(m.activeLayerId(), kDefaultLayerId);
    EXPECT_TRUE(m.empty());
    EXPECT_EQ(m.validate(), "");
}

TEST(Layers, AddRenameAndNameRules) {
    StructuralModel m;
    const int a = m.addLayer("Pilares", 0xFF0000);
    ASSERT_NE(a, 0);
    EXPECT_EQ(m.findLayer(a)->color, 0xFF0000u);
    EXPECT_EQ(m.addLayer("pilares"), 0);  // case-insensitive duplicate
    EXPECT_EQ(m.addLayer(""), 0);
    const int b = m.addLayer("Vigas");
    EXPECT_FALSE(m.renameLayer(b, "PILARES"));
    EXPECT_TRUE(m.renameLayer(b, "Vigas 1"));
    EXPECT_TRUE(m.renameLayer(b, "VIGAS 1"));  // its own name in another case is fine
    EXPECT_FALSE(m.renameLayer(99, "x"));
    EXPECT_EQ(m.validate(), "");
}

TEST(Layers, SuggestedNameFollowsTheOriginal) {
    StructuralModel m;
    EXPECT_EQ(m.suggestedLayerName(), "Nueva capa 2");  // "+" on a fresh model
    m.addLayer(m.suggestedLayerName());
    EXPECT_EQ(m.suggestedLayerName(), "Nueva capa 3");
    // Only "Nueva capa 2" left after deleting "por defecto": the next one is 3, not the taken 2.
    ASSERT_TRUE(m.removeLayer(kDefaultLayerId, LayerRemoval::MoveElements, 2));
    EXPECT_EQ(m.suggestedLayerName(), "Nueva capa 3");
}

TEST(Layers, ActiveLayerIsUniqueAndNewElementsGoThere) {
    int bar2 = 0, shell = 0;
    StructuralModel m = frameWithShell(bar2, shell);
    const int a = m.addLayer("A");
    EXPECT_TRUE(m.setActiveLayer(a));
    EXPECT_EQ(m.activeLayerId(), a);
    EXPECT_FALSE(m.findLayer(kDefaultLayerId)->active);
    EXPECT_FALSE(m.setActiveLayer(99));
    const int bar3 = m.addBar(m.nodes[0].id, m.nodes[2].id);
    ASSERT_NE(bar3, 0);
    EXPECT_EQ(m.findBar(bar3)->layerIds, std::vector<int>{a});
    EXPECT_EQ(m.findBar(bar2)->layerIds, std::vector<int>{kDefaultLayerId});
    EXPECT_EQ(m.validate(), "");
}

TEST(Layers, RemoveMovesOrphanedElementsToTheChosenLayer) {
    int bar2 = 0, shell = 0;
    StructuralModel m = frameWithShell(bar2, shell);
    const int a = m.addLayer("A");
    const int b = m.addLayer("B");
    ASSERT_TRUE(m.assignLayers({bar2}, {shell}, {a}, LayerOperation::Replace));
    EXPECT_EQ(m.layerUsage(a), 2);
    EXPECT_FALSE(m.removeLayer(a, LayerRemoval::MoveElements, 0));   // no target
    EXPECT_FALSE(m.removeLayer(a, LayerRemoval::MoveElements, a));   // same layer
    EXPECT_FALSE(m.removeLayer(a, LayerRemoval::MoveElements, 99));  // unknown target
    EXPECT_FALSE(m.removeLayer(99));
    EXPECT_TRUE(m.removeLayer(a, LayerRemoval::MoveElements, b));
    EXPECT_EQ(m.findBar(bar2)->layerIds, std::vector<int>{b});
    EXPECT_EQ(m.findShell(shell)->layerIds, std::vector<int>{b});
    EXPECT_EQ(m.layerUsage(b), 2);
    EXPECT_EQ(m.addLayer("C"), b + 1);  // ids are never reused
    EXPECT_EQ(m.validate(), "");
}

TEST(Layers, RemoveCanDeleteTheElementsOfTheLayer) {
    int bar2 = 0, shell = 0;
    StructuralModel m = frameWithShell(bar2, shell);
    const int a = m.addLayer("A");
    const int b = m.addLayer("B");
    ASSERT_TRUE(m.assignLayers({bar2}, {shell}, {a}, LayerOperation::Replace));
    ASSERT_TRUE(m.assignLayers({m.bars[0].id}, {}, {a, b}, LayerOperation::Replace));  // bar 1 also in B
    EXPECT_TRUE(m.removeLayer(a, LayerRemoval::DeleteElements));
    EXPECT_EQ(m.findBar(bar2), nullptr);
    EXPECT_EQ(m.findShell(shell), nullptr);
    ASSERT_NE(m.findBar(1), nullptr);  // it keeps layer B, so it survives
    EXPECT_EQ(m.findBar(1)->layerIds, std::vector<int>{b});
    EXPECT_EQ(m.nodes.size(), 4u);     // nodes are never deleted by a layer
    EXPECT_EQ(m.validate(), "");
}

TEST(Layers, TheLastLayerCannotBeRemovedAndDefaultIsNotSpecial) {
    StructuralModel m = simpleFrame();
    EXPECT_FALSE(m.removeLayer(kDefaultLayerId, LayerRemoval::DeleteElements));  // "Debe definir, al menos, una capa."
    const int a = m.addLayer("A");
    m.setActiveLayer(kDefaultLayerId);
    EXPECT_TRUE(m.removeLayer(kDefaultLayerId, LayerRemoval::MoveElements, a));
    EXPECT_EQ(m.layers.size(), 1u);
    EXPECT_EQ(m.activeLayerId(), a);  // the remaining layer becomes the active one
    EXPECT_EQ(m.bars[0].layerIds, std::vector<int>{a});
    EXPECT_FALSE(m.removeLayer(a));
    EXPECT_EQ(m.validate(), "");
}

TEST(Layers, AssignReplaceAndRemoveOperations) {
    int bar2 = 0, shell = 0;
    StructuralModel m = frameWithShell(bar2, shell);
    const int a = m.addLayer("A");
    const int b = m.addLayer("B");
    EXPECT_FALSE(m.assignLayers({}, {}, {a}, LayerOperation::Assign));         // nothing selected
    EXPECT_FALSE(m.assignLayers({bar2}, {}, {}, LayerOperation::Assign));      // no layer ticked
    EXPECT_FALSE(m.assignLayers({bar2}, {}, {99}, LayerOperation::Assign));    // unknown layer
    EXPECT_FALSE(m.assignLayers({99}, {}, {a}, LayerOperation::Assign));       // unknown bar
    EXPECT_EQ(m.findBar(bar2)->layerIds, std::vector<int>{kDefaultLayerId});   // untouched after failures

    ASSERT_TRUE(m.assignLayers({bar2}, {shell}, {b, a}, LayerOperation::Assign));
    EXPECT_EQ(m.findBar(bar2)->layerIds, (std::vector<int>{kDefaultLayerId, a, b}));  // sorted
    EXPECT_EQ(m.layerNames(m.findBar(bar2)->layerIds), "por defecto, A y B");
    ASSERT_TRUE(m.assignLayers({bar2}, {}, {a}, LayerOperation::Replace));
    EXPECT_EQ(m.findBar(bar2)->layerIds, std::vector<int>{a});
    EXPECT_EQ(m.layerNames(m.findBar(bar2)->layerIds), "A");
    ASSERT_TRUE(m.assignLayers({bar2}, {shell}, {a, b}, LayerOperation::Remove));
    EXPECT_EQ(m.findShell(shell)->layerIds, std::vector<int>{kDefaultLayerId});
    // Removing the only layer leaves the element on the active layer rather than on no layer.
    m.setActiveLayer(b);
    ASSERT_TRUE(m.assignLayers({bar2}, {}, {a}, LayerOperation::Replace));
    ASSERT_TRUE(m.assignLayers({bar2}, {}, {a}, LayerOperation::Remove));
    EXPECT_EQ(m.findBar(bar2)->layerIds, std::vector<int>{b});
    EXPECT_EQ(m.layerNames({a, b}), "A y B");
    EXPECT_EQ(m.validate(), "");
}

TEST(Layers, MoveChangesOrderNotIds) {
    StructuralModel m;
    const int a = m.addLayer("A");
    const int b = m.addLayer("B");
    EXPECT_FALSE(m.moveLayer(kDefaultLayerId, -1));
    EXPECT_FALSE(m.moveLayer(b, 1));
    EXPECT_FALSE(m.moveLayer(b, 2));
    EXPECT_TRUE(m.moveLayer(b, -1));
    EXPECT_EQ(m.layers[1].id, b);
    EXPECT_EQ(m.layers[2].id, a);
    EXPECT_TRUE(m.moveLayer(kDefaultLayerId, 1));
    EXPECT_EQ(m.layers[0].id, b);
    EXPECT_EQ(m.validate(), "");
}

TEST(Layers, ElementFlagsAndColourFollowTheirLayers) {
    StructuralModel m;
    const int a = m.addLayer("A", 0xFF0000);
    const int b = m.addLayer("B", 0x00FF00);
    EXPECT_TRUE(m.elementVisible({a}));
    m.findLayer(a)->visible = false;
    EXPECT_FALSE(m.elementVisible({a}));
    EXPECT_TRUE(m.elementVisible({a, b}));  // visible while any layer is
    m.findLayer(b)->locked = true;
    EXPECT_FALSE(m.elementLocked({a}));
    EXPECT_TRUE(m.elementLocked({a, b}));
    m.findLayer(a)->rendered = true;
    EXPECT_TRUE(m.elementRendered({a, b}));
    EXPECT_FALSE(m.elementRendered({b}));
    EXPECT_EQ(m.elementColor({a, b}), 0xFF0000u);  // first in list order
    EXPECT_EQ(m.layerName(99), "por defecto");     // unknown id -> first layer
    EXPECT_TRUE(m.moveLayer(b, -1));
    EXPECT_TRUE(m.moveLayer(b, -1));
    EXPECT_EQ(m.elementColor({a, b}), 0x00FF00u);
}

TEST(Layers, ValidateRejectsBrokenLayers) {
    StructuralModel m = simpleFrame();
    m.bars[0].layerIds = {7};
    EXPECT_NE(m.validate(), "");
    m.bars[0].layerIds = {};
    EXPECT_NE(m.validate(), "");
    m.bars[0].layerIds = {kDefaultLayerId, kDefaultLayerId};
    EXPECT_NE(m.validate(), "");
    m.bars[0].layerIds = {kDefaultLayerId};
    m.layers[0].active = false;
    EXPECT_NE(m.validate(), "");
    m.layers[0].active = true;
    m.layers.push_back(Layer{2, "POR DEFECTO", 0, false, true, false, false});
    m.nextLayerId = 3;
    EXPECT_NE(m.validate(), "");  // duplicate name
    m.layers.clear();
    EXPECT_NE(m.validate(), "");
}

TEST(Layers, JsonRoundTripKeepsOrderMembershipAndOldFilesGetDefaults) {
    Project original;
    original.structure = simpleFrame();
    const int a = original.structure.addLayer("Pilares", 0x123456);
    original.structure.findLayer(a)->rendered = true;
    ASSERT_TRUE(original.structure.assignLayers({1}, {}, {a}, LayerOperation::Assign));
    original.structure.setActiveLayer(a);
    original.structure.moveLayer(a, -1);
    Project loaded;
    project_json::loadSection(loaded, "layers", project_json::dumpSection(original, "layers"));
    project_json::loadSection(loaded, "bars", project_json::dumpSection(original, "bars"));
    EXPECT_EQ(loaded.structure.layers, original.structure.layers);
    EXPECT_EQ(loaded.structure.nextLayerId, original.structure.nextLayerId);
    EXPECT_EQ(loaded.structure.bars[0].layerIds, (std::vector<int>{kDefaultLayerId, a}));
    Project bars;
    project_json::loadSection(bars, "bars", R"({"items":[{"id":1,"startNode":1,"endNode":2}],"next":2})");
    EXPECT_EQ(bars.structure.bars[0].layerIds, std::vector<int>{kDefaultLayerId});  // file written before layers existed
}
