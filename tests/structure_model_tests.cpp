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
    bad.poissonRatio = 0.5;
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
    ASSERT_GE(names.size(), 5u);
    EXPECT_EQ(names[names.size() - 5], "nodes");
    EXPECT_EQ(names.back(), "shells");
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
