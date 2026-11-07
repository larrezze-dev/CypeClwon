#include <gtest/gtest.h>

#include "core/node_diaphragm_edit.h"
#include "core/node_edit.h"
#include "core/node_ligature_edit.h"
#include "core/node_support_edit.h"
#include "core/project_json.h"
#include "core/tools_edit.h"

using namespace cc::core;

namespace {

/// Six nodes N1..N6: N1..N3 at z = 0, N4..N6 at z = 4.
Project sixNodes() {
    Project p;
    for (int i = 0; i < 3; ++i) p.structure.addNode({static_cast<double>(i), 0, 0});
    for (int i = 0; i < 3; ++i) p.structure.addNode({static_cast<double>(i), 0, 4});
    return p;
}

Project withDiaphragm(UndoStack& stack, const std::string& ref = "D1") {
    Project p = sixNodes();
    Diaphragm d = newDiaphragm();
    d.reference = ref;
    EXPECT_FALSE(editDiaphragms(p, stack, {d}).has_value());
    return p;
}

}  // namespace

TEST(Diaphragm, NewAndDuplicateFollowTheOriginal) {
    const Diaphragm d = newDiaphragm();
    EXPECT_EQ(d.id, 0);
    EXPECT_TRUE(d.reference.empty());
    EXPECT_EQ(d.color, 0xFF00FFu);
    EXPECT_TRUE(d.nodeIds.empty());

    Diaphragm src;
    src.id = 3;
    src.reference = "D7";
    src.color = 0x123456;
    src.nodeIds = {1, 2};
    src.loads.push_back(defaultDiaphragmLoad(5));
    const Diaphragm dup = duplicateDiaphragm(src);
    EXPECT_EQ(dup.id, 0);
    EXPECT_EQ(dup.reference, "D7");
    EXPECT_EQ(dup.color, 0x123456u);
    EXPECT_TRUE(dup.nodeIds.empty());
    ASSERT_EQ(dup.loads.size(), 1u);
    EXPECT_EQ(dup.loads[0].loadCaseId, 5);
    EXPECT_EQ(dup.loads[0].fxKn, 0.0);
}

TEST(Diaphragm, ReferenceValidation) {
    std::vector<Diaphragm> rows{newDiaphragm()};
    ASSERT_TRUE(checkDiaphragmAdd(rows).has_value());
    EXPECT_EQ(checkDiaphragmAdd(rows)->text, "La referencia no puede quedar vacía.");
    EXPECT_EQ(checkDiaphragmAccept(rows)->messageId, 375658);
    rows[0].reference = "D1";
    EXPECT_FALSE(checkDiaphragmAdd(rows).has_value());
    rows.push_back(newDiaphragm());
    rows[1].reference = "D1";
    const auto repeated = checkDiaphragmAccept(rows);
    ASSERT_TRUE(repeated.has_value());
    EXPECT_EQ(repeated->messageId, 375648);
    EXPECT_EQ(repeated->text, "La referencia 'D1' está repetida.");
    rows[1].reference = "D2";
    EXPECT_FALSE(checkDiaphragmAccept(rows).has_value());
}

TEST(Diaphragm, EditCreatesKeepsNodesAndDeletes) {
    UndoStack stack;
    Project p = withDiaphragm(stack);
    ASSERT_EQ(p.diaphragms.items.size(), 1u);
    const int id = p.diaphragms.items[0].id;
    EXPECT_EQ(assignNodesToDiaphragm(p, stack, id, {1, 2}).status, DiaphragmStatus::Ok);

    // Edit: rename, add a duplicate; the nodes of D1 stay (the dialog cannot change them).
    std::vector<Diaphragm> rows = p.diaphragms.items;
    rows[0].reference = "Forjado";
    rows[0].nodeIds = {5};  // ignored
    rows.push_back(duplicateDiaphragm(rows[0]));
    rows[1].reference = "D2";
    EXPECT_FALSE(editDiaphragms(p, stack, rows).has_value());
    ASSERT_EQ(p.diaphragms.items.size(), 2u);
    EXPECT_EQ(p.diaphragms.items[0].reference, "Forjado");
    EXPECT_EQ(p.diaphragms.items[0].nodeIds, (std::vector<int>{1, 2}));
    EXPECT_EQ(p.diaphragms.items[1].id, 2);
    EXPECT_TRUE(p.diaphragms.items[1].nodeIds.empty());

    // An invalid list is rejected and nothing changes.
    rows[1].reference = "Forjado";
    const std::size_t depth = stack.count();
    const auto issue = editDiaphragms(p, stack, rows);
    ASSERT_TRUE(issue.has_value());
    EXPECT_EQ(stack.count(), depth);

    // Delete the first row: its nodes are free again; undo restores them; ids are not reused.
    EXPECT_FALSE(editDiaphragms(p, stack, {p.diaphragms.items[1]}).has_value());
    EXPECT_EQ(p.diaphragms.items.size(), 1u);
    EXPECT_FALSE(isNodeInDiaphragm(p.diaphragms, 1));
    stack.undo();
    EXPECT_EQ(p.diaphragms.items.size(), 2u);
    EXPECT_TRUE(isNodeInDiaphragm(p.diaphragms, 1));
    EXPECT_GE(p.diaphragms.nextId, 3);
}

TEST(Diaphragm, AssignOneNodeOneDiaphragmAndLabels) {
    UndoStack stack;
    Project p = withDiaphragm(stack);
    Diaphragm second = newDiaphragm();
    second.reference = "D2";
    std::vector<Diaphragm> rows = p.diaphragms.items;
    rows.push_back(second);
    ASSERT_FALSE(editDiaphragms(p, stack, rows).has_value());
    const int d1 = p.diaphragms.items[0].id, d2 = p.diaphragms.items[1].id;

    EXPECT_EQ(assignNodesToDiaphragm(p, stack, d1, {1, 2, 4}).status, DiaphragmStatus::Ok);  // different elevations are accepted
    EXPECT_EQ(diaphragmNodeLabel(p.diaphragms, 1), "D1");
    EXPECT_EQ(diaphragmTooltipLine(p.diaphragms, 4), "Diafragma : D1");
    EXPECT_EQ(diaphragmNodeLabel(p.diaphragms, 3), "");
    EXPECT_EQ(diaphragmTooltipLine(p.diaphragms, 3), "");
    // Moving N2 to D2 takes it out of D1.
    EXPECT_EQ(assignNodesToDiaphragm(p, stack, d2, {2}).status, DiaphragmStatus::Ok);
    EXPECT_EQ(diaphragmNodeLabel(p.diaphragms, 2), "D2");
    EXPECT_EQ(findDiaphragm(p.diaphragms, d1)->nodeIds, (std::vector<int>{1, 4}));
    EXPECT_TRUE(validateDiaphragms(p.diaphragms, p.structure).empty());
    stack.undo();
    EXPECT_EQ(diaphragmNodeLabel(p.diaphragms, 2), "D1");

    // Silent cases: empty / unknown nodes / unknown diaphragm; nothing pushed.
    const std::size_t depth = stack.count();
    EXPECT_EQ(assignNodesToDiaphragm(p, stack, d1, {}).status, DiaphragmStatus::Silent);
    EXPECT_EQ(assignNodesToDiaphragm(p, stack, d1, {99}).status, DiaphragmStatus::Silent);
    EXPECT_EQ(assignNodesToDiaphragm(p, stack, 77, {3}).status, DiaphragmStatus::Silent);
    EXPECT_EQ(stack.count(), depth);
}

TEST(Diaphragm, UnassignKeepsTheOthersAndTheDiaphragm) {
    UndoStack stack;
    Project p = withDiaphragm(stack);
    const int d1 = p.diaphragms.items[0].id;
    assignNodesToDiaphragm(p, stack, d1, {1, 2, 3});
    EXPECT_TRUE(removeNodesFromDiaphragm(p, stack, {1}));
    EXPECT_FALSE(isNodeInDiaphragm(p.diaphragms, 1));
    EXPECT_TRUE(isNodeInDiaphragm(p.diaphragms, 2));
    EXPECT_FALSE(removeNodesFromDiaphragm(p, stack, {1}));  // nothing to remove
    EXPECT_FALSE(removeNodesFromDiaphragm(p, stack, {}));
    EXPECT_TRUE(removeNodesFromDiaphragm(p, stack, {2, 3}));
    EXPECT_EQ(p.diaphragms.items.size(), 1u);
    stack.undo();
    EXPECT_TRUE(isNodeInDiaphragm(p.diaphragms, 3));
}

TEST(Diaphragm, AssignAcceptsRestrainedNodesWithoutAMessage) {
    // REA sec. 9.4: #375640 is an error of the calculation check, not an assignment dialog.
    UndoStack stack;
    Project p = withDiaphragm(stack);
    const int d1 = p.diaphragms.items[0].id;
    ExternalSupport fixed;
    fixed.type = ExternalSupportType::Fixed;
    ASSERT_GT(assignExternalSupport(p, stack, {1}, fixed), 0);
    const auto check = assignNodesToDiaphragm(p, stack, d1, {2, 1});
    EXPECT_EQ(check.status, DiaphragmStatus::Ok);
    EXPECT_TRUE(isNodeInDiaphragm(p.diaphragms, 1));
    EXPECT_TRUE(isNodeInDiaphragm(p.diaphragms, 2));
}

TEST(Diaphragm, AssignStripsTheTiesOfTheNodesAndUndoRestoresThem) {
    // INFERRED from REA FUN_1801bdd20 (to verify in the original).
    UndoStack stack;
    Project p = withDiaphragm(stack);
    const int d1 = p.diaphragms.items[0].id;
    ASSERT_EQ(assignLigatures(p, stack, {5, 6, 4}, {true, false, true}).status, LigatureStatus::Ok);
    ASSERT_TRUE(hasLigature(p.ligatures, 5));
    const NodeLigatures before = p.ligatures;
    EXPECT_EQ(assignNodesToDiaphragm(p, stack, d1, {5}).status, DiaphragmStatus::Ok);
    EXPECT_TRUE(isNodeInDiaphragm(p.diaphragms, 5));
    EXPECT_FALSE(hasLigature(p.ligatures, 5));  // no dx / dz tie any more
    EXPECT_TRUE(hasLigature(p.ligatures, 6));   // the others are kept (ties of one node dissolve: 6 and 4 remain tied)
    stack.undo();
    EXPECT_FALSE(isNodeInDiaphragm(p.diaphragms, 5));
    EXPECT_EQ(p.ligatures, before);
    stack.redo();
    EXPECT_FALSE(hasLigature(p.ligatures, 5));
    // "Eliminar asignación" only leaves the diaphragm: ties are not recreated and nothing else changes.
    const NodeLigatures mid = p.ligatures;
    EXPECT_TRUE(removeNodesFromDiaphragm(p, stack, {5}));
    EXPECT_EQ(p.ligatures, mid);
}

TEST(Diaphragm, LigaturesAreRefusedOnDiaphragmNodes) {
    // The stub of node_ligature is now the real query: #375639 is active.
    UndoStack stack;
    Project p = withDiaphragm(stack);
    assignNodesToDiaphragm(p, stack, p.diaphragms.items[0].id, {1, 2});
    const LigatureCheck check = assignLigatures(p, stack, {1, 3}, {true, false, false});
    EXPECT_EQ(check.status, LigatureStatus::Error);
    EXPECT_EQ(check.messageId, 375639);
    EXPECT_TRUE(p.ligatures.empty());
    EXPECT_EQ(assignLigatures(p, stack, {3, 4}, {true, false, false}).status, LigatureStatus::Ok);
}

TEST(Diaphragm, DeletingAndMergingNodesPurges) {
    UndoStack stack;
    Project p = withDiaphragm(stack);
    assignNodesToDiaphragm(p, stack, p.diaphragms.items[0].id, {1, 2, 3});
    ASSERT_TRUE(deleteNodes(p, stack, {2}));
    EXPECT_EQ(p.diaphragms.items[0].nodeIds, (std::vector<int>{1, 3}));
    stack.undo();
    EXPECT_EQ(p.diaphragms.items[0].nodeIds, (std::vector<int>{1, 2, 3}));

    // Dropping N3 on N1 merges them: N3 leaves the diaphragm, undo restores it.
    ASSERT_TRUE(moveNode(p, stack, 3, p.structure.findNode(1)->position));
    EXPECT_EQ(p.diaphragms.items[0].nodeIds, (std::vector<int>{1, 2}));
    stack.undo();
    EXPECT_EQ(p.diaphragms.items[0].nodeIds, (std::vector<int>{1, 2, 3}));
}

TEST(Diaphragm, MergeKeepsDestinationMembership) {
    // Measured (diafragmas.md 10.5): the destination survives; the moved node's membership is lost, not transferred.
    UndoStack stack;
    Project p = withDiaphragm(stack);
    assignNodesToDiaphragm(p, stack, p.diaphragms.items[0].id, {1});
    ASSERT_TRUE(moveNode(p, stack, 1, p.structure.findNode(2)->position));  // assigned onto free
    EXPECT_EQ(p.structure.findNode(1), nullptr);
    ASSERT_NE(p.structure.findNode(2), nullptr);
    EXPECT_TRUE(p.diaphragms.items[0].nodeIds.empty());
    stack.undo();
    EXPECT_EQ(p.diaphragms.items[0].nodeIds, (std::vector<int>{1}));

    assignNodesToDiaphragm(p, stack, p.diaphragms.items[0].id, {2});  // N1 and N2 assigned: move free N3 onto N2
    ASSERT_TRUE(moveNode(p, stack, 3, p.structure.findNode(2)->position));  // free onto assigned
    EXPECT_EQ(p.diaphragms.items[0].nodeIds, (std::vector<int>{1, 2}));
}

TEST(Diaphragm, MeasuredLevelPalette) {
    const unsigned expected[8] = {0xCC3300, 0xCC6600, 0xCC9900, 0xCCCC00, 0x99CC00, 0x66CC00, 0x33CC00, 0x00CC00};
    for (int i = 1; i <= 8; ++i) EXPECT_EQ(idColorToRgb(diaphragmLevelColorId(i)), expected[i - 1]) << i;
    EXPECT_EQ(idColorToRgb(13), 0xFF00FFu);
}

TEST(Diaphragm, RepairOfNodesPurges) {
    // SnapshotCommand: a node merged by the repair leaves its diaphragm.
    Project p;
    UndoStack stack;
    const int n1 = p.structure.addNode({0, 0, 0}), n2 = p.structure.addNode({4, 0, 0}), n2b = p.structure.addNode({4.0004, 0, 0});
    p.structure.addBar(n1, n2);
    Diaphragm d = newDiaphragm();
    d.reference = "D1";
    ASSERT_FALSE(editDiaphragms(p, stack, {d}).has_value());
    ASSERT_EQ(assignNodesToDiaphragm(p, stack, p.diaphragms.items[0].id, {n1, n2b}).status, DiaphragmStatus::Ok);
    ASSERT_TRUE(repairModel(p, stack));
    EXPECT_EQ(p.structure.findNode(n2b), nullptr);
    EXPECT_EQ(p.diaphragms.items[0].nodeIds, (std::vector<int>{n1}));
    stack.undo();
    EXPECT_EQ(p.diaphragms.items[0].nodeIds, (std::vector<int>{n1, n2b}));
}

TEST(Diaphragm, ElevationAndSameElevation) {
    Project p = sixNodes();
    Diaphragm d;
    d.nodeIds = {1, 2, 4};
    EXPECT_NEAR(*diaphragmElevation(d, p.structure), 4.0 / 3.0, 1e-12);
    EXPECT_FALSE(diaphragmNodesSameElevation(d, p.structure));
    d.nodeIds = {1, 2, 3};
    EXPECT_TRUE(diaphragmNodesSameElevation(d, p.structure));
    EXPECT_NEAR(*diaphragmElevation(d, p.structure), 0.0, 1e-12);
    p.structure.findNode(2)->position.z = 0.001;
    EXPECT_TRUE(diaphragmNodesSameElevation(d, p.structure));  // <= 0.001 accepted
    p.structure.findNode(2)->position.z = 0.0011;
    EXPECT_FALSE(diaphragmNodesSameElevation(d, p.structure));
    EXPECT_FALSE(diaphragmElevation(Diaphragm{}, p.structure).has_value());
    EXPECT_EQ(diaphragmMessage(375642), "No todos los elementos del diafragma están contenidos en el mismo plano horizontal.");

    const std::vector<double> elev{0.0, 4.0, 8.0};
    EXPECT_EQ(*diaphragmBelow(elev, 4.0), 0.0);
    EXPECT_EQ(*diaphragmAbove(elev, 4.0), 8.0);
    EXPECT_FALSE(diaphragmBelow(elev, 0.0).has_value());
    EXPECT_FALSE(diaphragmAbove(elev, 8.0).has_value());
}

TEST(Diaphragm, GenerateByLevelsMessagesAndReplacement) {
    UndoStack stack;
    Project p = sixNodes();
    // No levels: the message and nothing else.
    DiaphragmGeneration g = generateDiaphragmsByLevels(p, stack, false);
    EXPECT_EQ(g.status, DiaphragmGenerationStatus::NoLevels);
    EXPECT_EQ(g.text, "No se ha definido ningún nivel");
    EXPECT_EQ(g.messageId, 25932);

    // One level: the lowest is skipped, so nothing is generated (#375636).
    p.workPlanes.levels = {{"Cimentación", 0.0}};
    g = generateDiaphragmsByLevels(p, stack, false);
    EXPECT_EQ(g.count, 0);
    EXPECT_EQ(g.messageId, 375636);
    EXPECT_TRUE(p.diaphragms.items.empty());

    p.workPlanes.levels = {{"Planta 1", 4.0}, {"Cimentación", 0.0}, {"Cubierta", 8.0}};
    g = generateDiaphragmsByLevels(p, stack, false);
    EXPECT_EQ(g.status, DiaphragmGenerationStatus::Generated);
    EXPECT_EQ(g.count, 2);  // lowest level skipped
    EXPECT_EQ(g.messageId, 375638);
    EXPECT_EQ(g.text, "Se han generado 2 diafragmas");

    // Already defined: ask first (No by default in the GUI); confirm replaces them (ids keep growing).
    g = generateDiaphragmsByLevels(p, stack, false);
    EXPECT_EQ(g.status, DiaphragmGenerationStatus::NeedsConfirmation);
    EXPECT_EQ(g.messageId, 375635);
    EXPECT_EQ(g.text, "Se borrarán los diafragmas previamente definidos. ¿Desea continuar?");
    ASSERT_EQ(p.diaphragms.items.size(), 2u);
    g = generateDiaphragmsByLevels(p, stack, true);
    EXPECT_EQ(g.count, 2);
    ASSERT_EQ(p.diaphragms.items.size(), 2u) << "the new collection replaces the old one";
    EXPECT_GE(p.diaphragms.items[0].id, 3);
    stack.undo();
    EXPECT_EQ(p.diaphragms.items[0].id, 1);

    EXPECT_EQ(diaphragmMessage(375637), "Se ha generado un diafragma");
}

TEST(Diaphragm, GenerateByLevelsOrderNodesAndNames) {
    UndoStack stack;
    Project p = sixNodes();  // N1..N3 at z 0, N4..N6 at z 4
    p.structure.addNode({0, 0, 4.0005});  // N7 within the 0.001 tolerance of the 4 m level
    p.structure.addNode({0, 0, 8.0});     // N8
    p.workPlanes.levels = {{"A", 4.0}, {"Base", 0.0}, {"Top", 8.0}};
    ExternalSupport fixed;
    fixed.type = ExternalSupportType::Fixed;
    ASSERT_GT(assignExternalSupport(p, stack, {4}, fixed), 0);  // restrained nodes are NOT filtered out
    ASSERT_EQ(assignLigatures(p, stack, {5, 6}, {true, false, false}).status, LigatureStatus::Ok);  // nor tied ones
    const auto plan = planDiaphragmsByLevels(p, 10);
    ASSERT_EQ(plan.size(), 2u);  // lowest (Base) skipped; highest first
    EXPECT_EQ(plan[0].id, 10);
    EXPECT_EQ(plan[0].reference, "Top");
    EXPECT_EQ(plan[0].nodeIds, (std::vector<int>{8}));
    EXPECT_EQ(plan[1].id, 11);
    EXPECT_EQ(plan[1].reference, "A");
    EXPECT_EQ(plan[1].nodeIds, (std::vector<int>{4, 5, 6, 7}));
    EXPECT_TRUE(plan[0].loads.empty());

    // Repeated names: the first processed (highest) keeps the name, the next ones get _1, _2.
    p.workPlanes.levels = {{"P", 3.0}, {"P", 6.0}, {"P", 9.0}, {"P", 0.0}};
    const auto rep = planDiaphragmsByLevels(p, 1);
    ASSERT_EQ(rep.size(), 3u);
    EXPECT_EQ(rep[0].reference, "P");
    EXPECT_EQ(rep[1].reference, "P_1");
    EXPECT_EQ(rep[2].reference, "P_2");

    // Levels closer than the tolerance keep entry order (stable) and a lone level generates nothing.
    p.workPlanes.levels = {{"Only", 4.0}};
    EXPECT_TRUE(planDiaphragmsByLevels(p, 1).empty());
}

TEST(Diaphragm, GenerateByLevelsColourFormulaAndSubstitutionPoint) {
    EXPECT_EQ(diaphragmLevelColorId(1), 22);   // 12 + 1 * 10
    EXPECT_EQ(diaphragmLevelColorId(2), 32);
    EXPECT_EQ(diaphragmLevelColorId(23), 12 + 23 * 10);
    EXPECT_EQ(diaphragmLevelColorId(24), 11);  // i >= 24: base 11, (24 mod 24) = 0
    EXPECT_EQ(diaphragmLevelColorId(25), 21);
    EXPECT_EQ(idColorToRgb(13), kDefaultDiaphragmColor);
    UndoStack stack;
    Project p = sixNodes();
    p.workPlanes.levels = {{"A", 0.0}, {"B", 4.0}, {"C", 8.0}};
    const auto plan = planDiaphragmsByLevels(p, 1);
    ASSERT_EQ(plan.size(), 2u);
    EXPECT_EQ(plan[0].color, idColorToRgb(diaphragmLevelColorId(1)));
    EXPECT_EQ(plan[1].color, idColorToRgb(diaphragmLevelColorId(2)));
}

TEST(Diaphragm, GenerateByLevelsTakesShellsWithAllNodesInTheLevel) {
    Project p = sixNodes();
    Shell inLevel;
    inLevel.id = 1;
    inLevel.nodeIds = {4, 5, 6};  // all at z = 4
    Shell mixed;
    mixed.id = 2;
    mixed.nodeIds = {3, 4, 5};
    Shell empty;
    empty.id = 3;
    p.structure.shells = {inLevel, mixed, empty};
    p.workPlanes.levels = {{"Base", 0.0}, {"P1", 4.0}};
    const auto plan = planDiaphragmsByLevels(p, 1);
    ASSERT_EQ(plan.size(), 1u);
    EXPECT_EQ(plan[0].shellIds, (std::vector<int>{1}));
    // Persistence of the shell list.
    Project q = p;
    q.diaphragms.items = plan;
    q.diaphragms.nextId = 2;
    Project loaded;
    project_json::loadSection(loaded, "diaphragms", project_json::dumpSection(q, "diaphragms"));
    ASSERT_EQ(loaded.diaphragms.items.size(), 1u);
    EXPECT_EQ(loaded.diaphragms.items[0].shellIds, (std::vector<int>{1}));
}

TEST(Diaphragm, JsonRoundTripAndOldFiles) {
    UndoStack stack;
    Project p = withDiaphragm(stack, "D1");
    std::vector<Diaphragm> rows = p.diaphragms.items;
    rows[0].color = 0x00AA11;
    DiaphragmLoad l = defaultDiaphragmLoad(2);
    l.fxKn = 1.5;
    l.yM = -2.25;
    rows[0].loads.push_back(l);
    ASSERT_FALSE(editDiaphragms(p, stack, rows).has_value());
    assignNodesToDiaphragm(p, stack, p.diaphragms.items[0].id, {1, 2, 4});

    ASSERT_TRUE(project_json::isKnownSection("diaphragms"));
    const std::string text = project_json::dumpSection(p, "diaphragms");
    Project q = sixNodes();
    project_json::loadSection(q, "diaphragms", text);
    EXPECT_EQ(q.diaphragms, p.diaphragms);
    EXPECT_TRUE(project_json::validate(q).empty());

    Project old = sixNodes();
    project_json::loadSection(old, "diaphragms", "{}");
    EXPECT_TRUE(old.diaphragms.empty());
    EXPECT_EQ(old.diaphragms.nextId, 1);

    // Nodes that disappeared without the command knowing are not saved.
    p.structure.removeNode(4);
    Project r = sixNodes();
    project_json::loadSection(r, "diaphragms", project_json::dumpSection(p, "diaphragms"));
    EXPECT_EQ(r.diaphragms.items[0].nodeIds, (std::vector<int>{1, 2}));
}
