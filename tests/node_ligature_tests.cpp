#include <gtest/gtest.h>

#include "core/node_edit.h"
#include "core/node_ligature_edit.h"
#include "core/node_support_edit.h"
#include "core/project_json.h"

using namespace cc::core;

namespace {

constexpr LigatureDirections kX{true, false, false};
constexpr LigatureDirections kY{false, true, false};
constexpr LigatureDirections kXYZ{true, true, true};
constexpr LigatureDirections kNone{false, false, false};

/// Six nodes N1..N6 along X (ids 1..6) with a bar between consecutive ones.
Project sixNodes() {
    Project p;
    for (int i = 0; i < 6; ++i) p.structure.addNode({static_cast<double>(i), 0, 0});
    return p;
}

std::vector<int> nodesOf(const Project& p, int dir, int index) { return p.ligatures.dirs[static_cast<std::size_t>(dir)].at(static_cast<std::size_t>(index)).nodeIds; }

}  // namespace

TEST(NodeLigature, AssignCreatesTieAndLabel) {
    Project p = sixNodes();
    UndoStack stack;
    EXPECT_EQ(assignLigatures(p, stack, {1, 2}, kX).status, LigatureStatus::Ok);
    ASSERT_EQ(p.ligatures.dirs[0].size(), 1u);
    EXPECT_EQ(nodesOf(p, 0, 0), (std::vector<int>{1, 2}));
    EXPECT_TRUE(p.ligatures.dirs[1].empty());
    EXPECT_EQ(ligatureLabel(p.ligatures, 1), "Dx(1)");
    EXPECT_EQ(ligatureLabel(p.ligatures, 3), "");
    EXPECT_TRUE(hasLigature(p.ligatures, 2));
    EXPECT_FALSE(hasLigature(p.ligatures, 3));

    // Directions are independent and numbered separately: N3,N4 in X -> Dx(2); N1,N5 in Y and Z -> Dy(1), Dz(1).
    assignLigatures(p, stack, {3, 4}, kX);
    assignLigatures(p, stack, {1, 5}, {false, true, true});
    EXPECT_EQ(ligatureLabel(p.ligatures, 4), "Dx(2)");
    EXPECT_EQ(ligatureLabel(p.ligatures, 5), "Dy(1), Dz(1)");
    EXPECT_EQ(ligatureLabel(p.ligatures, 1), "Dx(1), Dy(1), Dz(1)");
}

TEST(NodeLigature, FirstNodeAbsorbsTheOthers) {
    // Measured: {N1,N2} and {N3,N4} in X; selecting N2, N3 -> N3 joins N2's tie, N4 stays alone as Dx(2).
    Project p = sixNodes();
    UndoStack stack;
    assignLigatures(p, stack, {1, 2}, kX);
    assignLigatures(p, stack, {3, 4}, kX);
    EXPECT_EQ(assignLigatures(p, stack, {2, 3}, kX).status, LigatureStatus::Ok);
    ASSERT_EQ(p.ligatures.dirs[0].size(), 2u);
    EXPECT_EQ(nodesOf(p, 0, 0), (std::vector<int>{1, 2, 3}));
    EXPECT_EQ(nodesOf(p, 0, 1), (std::vector<int>{4}));  // a tie of one node is kept (measured)
    EXPECT_EQ(ligatureLabel(p.ligatures, 4), "Dx(2)");
    EXPECT_EQ(ligatureLabel(p.ligatures, 3), "Dx(1)");
}

TEST(NodeLigature, FirstNodeWithoutTieCreatesNewOneAndTakesOthersAway) {
    Project p = sixNodes();
    UndoStack stack;
    assignLigatures(p, stack, {2, 3}, kX);
    // N1 (free) first, N2 (tied) second: new tie {1,2}; N2 leaves {2,3}, which keeps a single node.
    EXPECT_EQ(assignLigatures(p, stack, {1, 2}, kX).status, LigatureStatus::Ok);
    ASSERT_EQ(p.ligatures.dirs[0].size(), 2u);
    EXPECT_EQ(nodesOf(p, 0, 0), (std::vector<int>{3}));
    EXPECT_EQ(nodesOf(p, 0, 1), (std::vector<int>{1, 2}));
}

TEST(NodeLigature, ValidationOrder) {
    Project p = sixNodes();
    UndoStack stack;
    // 1. No direction -> #388777, even before looking at the number of nodes.
    LigatureCheck c = assignLigatures(p, stack, {1, 2}, kNone);
    EXPECT_EQ(c.status, LigatureStatus::Error);
    EXPECT_EQ(c.messageId, 388777);
    EXPECT_EQ(c.text, "No se ha seleccionado el tipo de ligadura a establecer.");
    EXPECT_EQ(assignLigatures(p, stack, {1}, kNone).messageId, 388777);
    // 2. Fewer than two nodes: silent.
    EXPECT_EQ(assignLigatures(p, stack, {1}, kX).status, LigatureStatus::Silent);
    EXPECT_EQ(assignLigatures(p, stack, {1, 1}, kX).status, LigatureStatus::Silent);  // duplicates collapse
    EXPECT_EQ(assignLigatures(p, stack, {}, kX).status, LigatureStatus::Silent);
    EXPECT_TRUE(p.ligatures.empty());
    EXPECT_EQ(static_cast<int>(stack.count()), 0);
    // 4. Same tie: #64230.
    assignLigatures(p, stack, {1, 2}, kX);
    c = assignLigatures(p, stack, {1, 2}, kX);
    EXPECT_EQ(c.messageId, 64230);
    EXPECT_EQ(c.text, "Los nudos seleccionados ya pertenecen a la misma ligadura.");
    EXPECT_EQ(ligatureMessage(375639), "No es posible asignar ligaduras a nudos que pertenecen a diafragmas");
    // A direction that is still to be linked makes the assignment valid (X is silent, Y links).
    EXPECT_EQ(assignLigatures(p, stack, {1, 2}, {true, true, false}).status, LigatureStatus::Ok);
    EXPECT_EQ(nodesOf(p, 1, 0), (std::vector<int>{1, 2}));
    EXPECT_EQ(p.ligatures.dirs[0].size(), 1u);
}

TEST(NodeLigature, PrescribedDisplacementsMustAgree) {
    Project p = sixNodes();
    UndoStack stack;
    ExternalSupport a;
    a.type = ExternalSupportType::Fixed;
    a.prescribed.push_back({1, {0.001, 0, 0, 0, 0, 0}});
    ExternalSupport b = a;
    b.prescribed[0].values[0] = 0.002;
    ExternalSupport same = a;
    p.nodeSupports.external[1] = a;
    p.nodeSupports.external[2] = b;
    p.nodeSupports.external[3] = same;
    LigatureCheck c = assignLigatures(p, stack, {1, 2}, kX);
    EXPECT_EQ(c.messageId, 64231);
    EXPECT_EQ(c.text, "No es posible asignar ligaduras a nudos cuyos desplazamientos prescritos sean diferentes.");
    EXPECT_TRUE(p.ligatures.empty());
    // Equal values are fine, and a direction without conflict is fine too.
    EXPECT_EQ(assignLigatures(p, stack, {1, 3}, kX).status, LigatureStatus::Ok);
    EXPECT_EQ(assignLigatures(p, stack, {1, 2}, kY).status, LigatureStatus::Ok);
    // A node without prescribed values does not conflict (REA compares the nodes that have them).
    EXPECT_EQ(assignLigatures(p, stack, {4, 5, 3}, kX).status, LigatureStatus::Ok);
}

TEST(NodeLigature, LineAndPlaneRestraintsAreNotCapturable) {
    Project p = sixNodes();
    ExternalSupport line;
    line.type = ExternalSupportType::LineXFixed;
    ExternalSupport plane;
    plane.type = ExternalSupportType::PlaneUserPinned;
    ExternalSupport genericLine;
    genericLine.type = ExternalSupportType::Generic;
    genericLine.generic.mode = GenericDisplacementMode::OnLine;
    ExternalSupport pinned;
    pinned.type = ExternalSupportType::Pinned;
    p.nodeSupports.external[1] = line;
    p.nodeSupports.external[2] = plane;
    p.nodeSupports.external[3] = genericLine;
    p.nodeSupports.external[4] = pinned;
    EXPECT_FALSE(isLigatureCapturable(p.structure, p.nodeSupports, 1));
    EXPECT_FALSE(isLigatureCapturable(p.structure, p.nodeSupports, 2));
    EXPECT_FALSE(isLigatureCapturable(p.structure, p.nodeSupports, 3));
    EXPECT_TRUE(isLigatureCapturable(p.structure, p.nodeSupports, 4));  // an ordinary restraint admits ties
    EXPECT_TRUE(isLigatureCapturable(p.structure, p.nodeSupports, 5));
    EXPECT_FALSE(isLigatureCapturable(p.structure, p.nodeSupports, 99));
    EXPECT_EQ(ligatureSelection(p, {1, 4, 4, 5, 2}), (std::vector<int>{4, 5}));
    UndoStack stack;
    EXPECT_EQ(assignLigatures(p, stack, {1, 4}, kX).status, LigatureStatus::Silent);  // only one capturable node is left
}

TEST(NodeLigature, RemoveWithDirectionsTakesOnlySelectedNodes) {
    Project p = sixNodes();
    UndoStack stack;
    assignLigatures(p, stack, {1, 2, 3}, kX);
    EXPECT_TRUE(removeLigatures(p, stack, {1}, kX));
    ASSERT_EQ(p.ligatures.dirs[0].size(), 1u);
    EXPECT_EQ(nodesOf(p, 0, 0), (std::vector<int>{2, 3}));  // N2 and N3 keep Dx(1)
    EXPECT_EQ(ligatureLabel(p.ligatures, 2), "Dx(1)");
    // Removing a node of a tie of two destroys it.
    EXPECT_TRUE(removeLigatures(p, stack, {2}, kX));
    EXPECT_TRUE(p.ligatures.empty());
    // A node without tie, or a direction without ties: silent and nothing pushed.
    const int before = static_cast<int>(stack.count());
    EXPECT_FALSE(removeLigatures(p, stack, {4}, kX));
    EXPECT_FALSE(removeLigatures(p, stack, {}, kX));
    EXPECT_EQ(static_cast<int>(stack.count()), before);
    // Directions are independent.
    assignLigatures(p, stack, {1, 2}, kXYZ);
    EXPECT_TRUE(removeLigatures(p, stack, {1}, kY));
    EXPECT_EQ(ligatureLabel(p.ligatures, 2), "Dx(1), Dz(1)");
}

TEST(NodeLigature, RemoveWithoutDirectionsRemovesEverything) {
    // Measured: Eliminar with no checkbox deletes all the ties of the work, whatever the selection.
    Project p = sixNodes();
    UndoStack stack;
    assignLigatures(p, stack, {1, 2}, kX);
    assignLigatures(p, stack, {3, 4}, kX);
    assignLigatures(p, stack, {5, 6}, kXYZ);
    EXPECT_TRUE(removeLigatures(p, stack, {1}, kNone));
    EXPECT_TRUE(p.ligatures.empty());
    stack.undo();
    EXPECT_EQ(p.ligatures.dirs[0].size(), 3u);
    EXPECT_EQ(p.ligatures.dirs[2].size(), 1u);
    EXPECT_FALSE(removeLigatures(p, stack, {}, kNone));  // no selection: nothing
}

TEST(NodeLigature, UndoRedoRestoresExactState) {
    Project p = sixNodes();
    UndoStack stack;
    assignLigatures(p, stack, {1, 2}, kX);
    const NodeLigatures one = p.ligatures;
    assignLigatures(p, stack, {3, 4, 5}, kXYZ);
    const NodeLigatures two = p.ligatures;
    stack.undo();
    EXPECT_EQ(p.ligatures, one);
    stack.undo();
    EXPECT_TRUE(p.ligatures.empty());
    stack.redo();
    stack.redo();
    EXPECT_EQ(p.ligatures, two);
}

TEST(NodeLigature, DeletingANodeLeavesTiesAndRenumbers) {
    // Measured: {N1,N2}=Dx(1), {N3,N4}=Dx(2); delete N2 -> N1 loses the tie and {N3,N4} becomes Dx(1).
    Project p = sixNodes();
    UndoStack stack;
    assignLigatures(p, stack, {1, 2}, kX);
    assignLigatures(p, stack, {3, 4}, kX);
    ASSERT_TRUE(deleteNodes(p, stack, {2}));
    EXPECT_EQ(ligatureLabel(p.ligatures, 1), "");
    EXPECT_EQ(ligatureLabel(p.ligatures, 3), "Dx(1)");
    EXPECT_EQ(ligatureLabel(p.ligatures, 4), "Dx(1)");
    ASSERT_EQ(p.ligatures.dirs[0].size(), 1u);
    stack.undo();
    EXPECT_EQ(ligatureLabel(p.ligatures, 1), "Dx(1)");
    EXPECT_EQ(ligatureLabel(p.ligatures, 3), "Dx(2)");
    stack.redo();
    EXPECT_EQ(ligatureLabel(p.ligatures, 3), "Dx(1)");
    // A tie of three loses only the deleted node.
    Project q = sixNodes();
    UndoStack stack2;
    assignLigatures(q, stack2, {1, 2, 3}, kXYZ);
    deleteNodes(q, stack2, {3});
    EXPECT_EQ(ligatureLabel(q.ligatures, 1), "Dx(1), Dy(1), Dz(1)");
    EXPECT_EQ(nodesOf(q, 1, 0), (std::vector<int>{1, 2}));
}

TEST(NodeLigature, MergingNodesDropsTheMovedNodeFromItsTies) {
    Project p;
    p.structure.addNode({0, 0, 0});
    p.structure.addNode({1, 0, 0});
    p.structure.addNode({2, 0, 0});
    UndoStack stack;
    assignLigatures(p, stack, {1, 2}, kX);
    ASSERT_TRUE(moveNode(p, stack, 2, {2, 0, 0}));  // lands on node 3: they merge, node 2 disappears
    EXPECT_FALSE(p.structure.findNode(2));
    EXPECT_TRUE(p.ligatures.empty());
    stack.undo();
    EXPECT_EQ(nodesOf(p, 0, 0), (std::vector<int>{1, 2}));
}

TEST(NodeLigature, JsonRoundTripAndBackwardCompatibility) {
    Project p = sixNodes();
    UndoStack stack;
    assignLigatures(p, stack, {1, 2}, kX);
    assignLigatures(p, stack, {3, 4}, kXYZ);
    EXPECT_TRUE(project_json::isKnownSection("node_ties"));
    const std::string text = project_json::dumpSection(p, "node_ties");
    Project q = sixNodes();
    project_json::loadSection(q, "node_ties", text);
    EXPECT_EQ(q.ligatures, p.ligatures);
    EXPECT_TRUE(project_json::validate(q).empty());
    // New ids keep growing after a reload.
    UndoStack stack2;
    assignLigatures(q, stack2, {5, 6}, kX);
    EXPECT_EQ(q.ligatures.dirs[0].back().id, p.ligatures.nextId);

    // Older files: empty / missing keys.
    Project old = sixNodes();
    project_json::loadSection(old, "node_ties", "{}");
    EXPECT_TRUE(old.ligatures.empty());
    project_json::loadSection(old, "node_ties", R"({"ties":{"dz":[{"id":4,"nodes":[1,2]}]}})");
    EXPECT_EQ(ligatureLabel(old.ligatures, 2), "Dz(1)");
    EXPECT_EQ(old.ligatures.nextId, 5);
    EXPECT_TRUE(project_json::validate(old).empty());
}

TEST(NodeLigature, StoredDataIsValidated) {
    Project p = sixNodes();
    p.ligatures.nextId = 10;
    p.ligatures.dirs[0] = {{1, {1, 2}}, {2, {2, 3}}};  // node 2 in two ties of the same direction
    EXPECT_FALSE(validateLigatures(p.ligatures, p.structure).empty());
    p.ligatures.dirs[0] = {{1, {1, 2}}, {1, {3, 4}}};  // repeated id
    EXPECT_FALSE(validateLigatures(p.ligatures, p.structure).empty());
    p.ligatures.dirs[0] = {{1, {1, 99}}};  // unknown node
    EXPECT_FALSE(validateLigatures(p.ligatures, p.structure).empty());
    p.ligatures.dirs[0] = {{1, {1, 2}}};
    p.ligatures.dirs[1] = {{2, {2, 3}}};  // the same node in different directions is fine
    EXPECT_TRUE(validateLigatures(p.ligatures, p.structure).empty());
}

TEST(NodeLigature, OrphanNodesAreNotSaved) {
    Project p = sixNodes();
    p.ligatures.nextId = 3;
    p.ligatures.dirs[0] = {{1, {1, 2}}, {2, {3, 99}}};  // node 99 does not exist
    Project q = sixNodes();
    project_json::loadSection(q, "node_ties", project_json::dumpSection(p, "node_ties"));
    ASSERT_EQ(q.ligatures.dirs[0].size(), 1u);
    EXPECT_EQ(nodesOf(q, 0, 0), (std::vector<int>{1, 2}));
}
