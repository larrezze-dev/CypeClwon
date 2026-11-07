#include <gtest/gtest.h>

#include <limits>

#include "core/bar_edit.h"
#include "core/deflection_group_edit.h"
#include "core/node_edit.h"
#include "core/piece_edit.h"
#include "core/project_json.h"
#include "core/tools_edit.h"

using namespace cc::core;

namespace {

/// Nodes N1 (0,0,0) .. N4 (12,0,0) on the X axis; bars B1 = N1-N2, B2 = N2-N3, B3 = N3-N4.
Project line(int bars = 2) {
    Project p;
    for (int i = 0; i <= bars; ++i) p.structure.addNode({4.0 * i, 0, 0});
    for (int i = 1; i <= bars; ++i) p.structure.addBar(i, i + 1);
    return p;
}

std::vector<DeflectionGroupInfo> list(const Project& p) { return deflectionGroups(p.deflection, p.structure); }

}  // namespace

TEST(DeflectionGroup, EveryBarIsItsOwnSecantGroupByDefault) {
    const Project p = line(2);
    const auto groups = list(p);
    ASSERT_EQ(groups.size(), 2u);
    EXPECT_EQ(groups[0].number, 1);
    EXPECT_EQ(groups[0].bars, std::vector<int>{1});
    EXPECT_EQ(groups[0].node1, 1);
    EXPECT_EQ(groups[0].node2, 2);
    EXPECT_EQ(groups[0].xy, DeflectionType::Secant);
    EXPECT_EQ(groups[0].limits, DeflectionLimits{});
    EXPECT_EQ(groups[1].number, 2);
    EXPECT_EQ(deflectionGroupLabel(groups[0]), "G1(1-2 xy, 1-2 xz)");
    EXPECT_EQ(deflectionGroupLabel(groups[1]), "G2(1-2 xy, 1-2 xz)");
    EXPECT_TRUE(p.deflection.empty());
    EXPECT_EQ(static_cast<int>(DeflectionType::TangentToNode2), 2);
    EXPECT_EQ(deflectionTypeName(DeflectionType::TangentToNode1), "Tangente a nudo 1");
}

TEST(DeflectionGroup, InvertedAxisBarStartsAtItsEnd) {
    Project p = line(1);
    p.structure.findBar(1)->xAxisInverted = true;
    const auto groups = list(p);
    EXPECT_EQ(groups[0].node1, 2);
    EXPECT_EQ(groups[0].node2, 1);
}

TEST(DeflectionGroup, CreateAlignedChainAndUndo) {
    Project p = line(2);
    UndoStack stack;
    int number = 0;
    ASSERT_EQ(createDeflectionGroup(p, stack, 1, 3, DeflectionType::Secant, DeflectionType::TangentToNode1, &number), DeflectionError::None);
    EXPECT_EQ(number, 1);
    const auto groups = list(p);
    ASSERT_EQ(groups.size(), 1u);
    EXPECT_EQ(groups[0].bars, (std::vector<int>{1, 2}));
    EXPECT_EQ(groups[0].node1, 1);
    EXPECT_EQ(groups[0].node2, 3);
    EXPECT_EQ(groups[0].xz, DeflectionType::TangentToNode1);
    EXPECT_EQ(deflectionGroupLabel(groups[0]), "G1(1-2 xy, 1 xz)");
    EXPECT_EQ(groups[0].limits, DeflectionLimits{});
    stack.undo();
    EXPECT_TRUE(p.deflection.empty());
    EXPECT_EQ(list(p).size(), 2u);
    stack.redo();
    EXPECT_EQ(list(p).size(), 1u);
}

TEST(DeflectionGroup, NodeOneIsTheFirstNodeChosen) {
    Project p = line(2);
    UndoStack stack;
    ASSERT_EQ(createDeflectionGroup(p, stack, 3, 1, DeflectionType::TangentToNode1, DeflectionType::Secant), DeflectionError::None);
    const auto groups = list(p);
    EXPECT_EQ(groups[0].bars, (std::vector<int>{2, 1}));
    EXPECT_EQ(groups[0].node1, 3);
    EXPECT_EQ(groups[0].node2, 1);
}

TEST(DeflectionGroup, CreateErrors) {
    Project p = line(2);
    UndoStack stack;
    const int off = p.structure.addNode({4, 3, 0});
    p.structure.addBar(2, off);
    // No chain of aligned bars between the nodes (#64227).
    EXPECT_EQ(createDeflectionGroup(p, stack, 1, off, DeflectionType::Secant, DeflectionType::Secant), DeflectionError::NoAlignedGroup);
    EXPECT_EQ(checkCreateDeflectionGroup(p, 1, off), DeflectionError::NoAlignedGroup);
    EXPECT_EQ(createDeflectionGroup(p, stack, 1, 99, DeflectionType::Secant, DeflectionType::Secant), DeflectionError::UnknownElement);
    EXPECT_EQ(deflectionErrorMessage(DeflectionError::NoAlignedGroup),
              "No se ha encontrado un grupo de barras alineadas que conecte los dos nudos seleccionados.");
    // A lone bar is already a group (#64228).
    EXPECT_EQ(createDeflectionGroup(p, stack, 1, 2, DeflectionType::TangentToNode1, DeflectionType::Secant), DeflectionError::EquivalentExists);
    EXPECT_EQ(deflectionErrorMessage(DeflectionError::EquivalentExists), "Ya existe un grupo de flecha equivalente.");
    // Same bars twice.
    ASSERT_EQ(createDeflectionGroup(p, stack, 1, 3, DeflectionType::Secant, DeflectionType::Secant), DeflectionError::None);
    EXPECT_EQ(createDeflectionGroup(p, stack, 1, 3, DeflectionType::TangentToNode2, DeflectionType::Secant), DeflectionError::EquivalentExists);
    EXPECT_EQ(createDeflectionGroup(p, stack, 3, 1, DeflectionType::TangentToNode2, DeflectionType::Secant), DeflectionError::EquivalentExists);
    EXPECT_EQ(stack.count(), 1u);
}

TEST(DeflectionGroup, BarsWithDifferentRollOrDirectionAreRefused) {
    Project p = line(2);
    UndoStack stack;
    p.structure.findBar(2)->rollAngleDeg = 90.0;
    EXPECT_EQ(createDeflectionGroup(p, stack, 1, 3, DeflectionType::Secant, DeflectionType::Secant), DeflectionError::DifferentArrangement);
    p.structure.findBar(2)->rollAngleDeg = 0.0;
    p.structure.findBar(2)->xAxisInverted = true;
    EXPECT_EQ(createDeflectionGroup(p, stack, 1, 3, DeflectionType::Secant, DeflectionType::Secant), DeflectionError::DifferentArrangement);
    EXPECT_EQ(stack.count(), 0u);
}

TEST(DeflectionGroup, NewGroupDissolvesTheOverlappingOnes) {
    Project p = line(3);
    UndoStack stack;
    ASSERT_EQ(createDeflectionGroup(p, stack, 1, 3, DeflectionType::TangentToNode1, DeflectionType::Secant), DeflectionError::None);
    int number = 0;
    ASSERT_EQ(createDeflectionGroup(p, stack, 2, 4, DeflectionType::Secant, DeflectionType::TangentToNode2, &number), DeflectionError::None);
    const auto groups = list(p);
    ASSERT_EQ(groups.size(), 2u);
    // B1 is a default group again; B2 + B3 are the new group.
    EXPECT_EQ(groups[0].bars, std::vector<int>{1});
    EXPECT_EQ(groups[0].xy, DeflectionType::Secant);
    EXPECT_EQ(groups[1].bars, (std::vector<int>{2, 3}));
    EXPECT_EQ(groups[1].xz, DeflectionType::TangentToNode2);
    EXPECT_EQ(number, 2);
    EXPECT_EQ(p.deflection.groups.size(), 1u);
    stack.undo();
    EXPECT_EQ(list(p).size(), 2u);
    EXPECT_EQ(list(p)[0].bars, (std::vector<int>{1, 2}));
}

TEST(DeflectionGroup, EditTypesOfSeveralGroupsAndBackToDefault) {
    Project p = line(3);
    UndoStack stack;
    ASSERT_EQ(editDeflectionTypes(p, stack, {1, 3}, DeflectionType::TangentToNode2, DeflectionType::TangentToNode1), DeflectionError::None);
    auto groups = list(p);
    ASSERT_EQ(groups.size(), 3u);
    EXPECT_EQ(groups[0].xy, DeflectionType::TangentToNode2);
    EXPECT_EQ(groups[0].xz, DeflectionType::TangentToNode1);
    EXPECT_EQ(groups[1].xy, DeflectionType::Secant);
    EXPECT_EQ(groups[2].xy, DeflectionType::TangentToNode2);
    EXPECT_EQ(p.deflection.groups.size(), 2u);
    EXPECT_EQ(deflectionGroupLabel(groups[0]), "G1(2 xy, 1 xz)");
    // Nothing changes.
    EXPECT_EQ(editDeflectionTypes(p, stack, {1}, DeflectionType::TangentToNode2, DeflectionType::TangentToNode1), DeflectionError::NothingToChange);
    EXPECT_EQ(editDeflectionTypes(p, stack, {99}, DeflectionType::Secant, DeflectionType::Secant), DeflectionError::UnknownElement);
    // Back to secant: the explicit record disappears.
    ASSERT_EQ(editDeflectionTypes(p, stack, {1, 3}, DeflectionType::Secant, DeflectionType::Secant), DeflectionError::None);
    EXPECT_TRUE(p.deflection.empty());
    stack.undo();
    EXPECT_EQ(p.deflection.groups.size(), 2u);
}

TEST(DeflectionGroup, EditAGroupFromAnyOfItsBars) {
    Project p = line(3);
    UndoStack stack;
    ASSERT_EQ(createDeflectionGroup(p, stack, 1, 3, DeflectionType::Secant, DeflectionType::Secant), DeflectionError::None);
    ASSERT_EQ(editDeflectionTypes(p, stack, {2}, DeflectionType::TangentToNode1, DeflectionType::Secant), DeflectionError::None);
    const auto groups = list(p);
    EXPECT_EQ(groups[0].bars, (std::vector<int>{1, 2}));
    EXPECT_EQ(groups[0].xy, DeflectionType::TangentToNode1);
    EXPECT_EQ(groups[1].xy, DeflectionType::Secant);
}

TEST(DeflectionGroup, LimitsValidation) {
    DeflectionLimits l;
    EXPECT_FALSE(checkDeflectionLimits(l).has_value());  // nothing enabled
    l.item(LimitSlot::MaxAbsXY).absoluteMm = 0.0;        // disabled blocks are not checked
    EXPECT_FALSE(checkDeflectionLimits(l).has_value());
    l.item(LimitSlot::MaxAbsXY).enabled = true;
    auto issue = checkDeflectionLimits(l);
    ASSERT_TRUE(issue.has_value());
    EXPECT_NE(issue->text.find("Flecha máxima absoluta xy"), std::string::npos);
    l.item(LimitSlot::MaxAbsXY).absoluteMm = 0.00001;  // must be strictly greater
    EXPECT_TRUE(checkDeflectionLimits(l).has_value());
    l.item(LimitSlot::MaxAbsXY).absoluteMm = 0.00002;
    EXPECT_FALSE(checkDeflectionLimits(l).has_value());
    l.item(LimitSlot::ActiveRelXZ).enabled = true;
    l.item(LimitSlot::ActiveRelXZ).relativeDivisor = 0.0001;
    issue = checkDeflectionLimits(l);
    ASSERT_TRUE(issue.has_value());
    EXPECT_NE(issue->text.find("Flecha activa relativa xz"), std::string::npos);
    l.item(LimitSlot::ActiveRelXZ).relativeDivisor = 0.0002;
    EXPECT_FALSE(checkDeflectionLimits(l).has_value());
    l.item(LimitSlot::ActiveRelXZ).relativeDivisor = std::numeric_limits<double>::quiet_NaN();
    EXPECT_TRUE(checkDeflectionLimits(l).has_value());
    // Defaults of the dialog and the preset radios.
    EXPECT_EQ(DeflectionLimitItem{}.absoluteMm, 1.0);
    EXPECT_EQ(DeflectionLimitItem{}.relativeDivisor, 250.0);
    EXPECT_TRUE(isPresetDeflectionDivisor(250.0));
    EXPECT_TRUE(isPresetDeflectionDivisor(500.0));
    EXPECT_FALSE(isPresetDeflectionDivisor(350.0));
    EXPECT_EQ(deflectionLimitName(LimitSlot::ActiveAbsXZ), "Flecha activa absoluta xz");
    EXPECT_TRUE(isAbsoluteLimitSlot(LimitSlot::ActiveAbsXZ));
    EXPECT_FALSE(isAbsoluteLimitSlot(LimitSlot::MaxRelXY));
    EXPECT_EQ(kLimitSlotCount, 8);
}

TEST(DeflectionGroup, SetLimitsOnSeveralGroups) {
    Project p = line(2);
    UndoStack stack;
    DeflectionLimits l;
    l.item(LimitSlot::MaxRelXY).enabled = true;
    l.item(LimitSlot::MaxRelXY).relativeDivisor = 300.0;
    l.item(LimitSlot::MaxAbsXZ).enabled = true;
    l.item(LimitSlot::MaxAbsXZ).absoluteMm = 12.5;
    ASSERT_FALSE(setDeflectionLimits(p, stack, {1, 2}, l).has_value());
    for (const auto& g : list(p)) EXPECT_EQ(g.limits, l);
    EXPECT_EQ(p.deflection.groups.size(), 2u);
    // An invalid block: nothing is pushed.
    DeflectionLimits bad = l;
    bad.item(LimitSlot::ActiveAbsXY).enabled = true;
    bad.item(LimitSlot::ActiveAbsXY).absoluteMm = 0.0;
    EXPECT_TRUE(setDeflectionLimits(p, stack, {1}, bad).has_value());
    EXPECT_EQ(stack.count(), 1u);
    // Same data again: nothing to push.
    EXPECT_FALSE(setDeflectionLimits(p, stack, {1}, l).has_value());
    EXPECT_EQ(stack.count(), 1u);
    // Clearing the limits removes the explicit groups.
    ASSERT_FALSE(setDeflectionLimits(p, stack, {1, 2}, DeflectionLimits{}).has_value());
    EXPECT_TRUE(p.deflection.empty());
    stack.undo();
    EXPECT_EQ(list(p)[0].limits, l);
}

TEST(DeflectionGroup, DividingABarExtendsItsExplicitGroup) {
    Project p;
    UndoStack stack;
    const int n1 = p.structure.addNode({0, 0, 0}), n2 = p.structure.addNode({4, 0, 0});
    p.structure.addNode({2, 0, 0});
    const int b = p.structure.addBar(n1, n2);
    ASSERT_EQ(editDeflectionTypes(p, stack, {b}, DeflectionType::TangentToNode1, DeflectionType::Secant), DeflectionError::None);
    RepairReport report;
    ASSERT_TRUE(repairModel(p, stack, &report));
    ASSERT_EQ(report.splitBars, 1);
    const auto groups = list(p);
    ASSERT_EQ(groups.size(), 1u);
    EXPECT_EQ(groups[0].bars.size(), 2u);
    EXPECT_EQ(groups[0].xy, DeflectionType::TangentToNode1);
    EXPECT_EQ(groups[0].node1, n1);
    EXPECT_EQ(groups[0].node2, n2);
    stack.undo();
    EXPECT_EQ(p.structure.bars.size(), 1u);
    EXPECT_EQ(list(p).size(), 1u);
}

TEST(DeflectionGroup, DividingABarWithoutGroupDataGivesSeparateGroups) {
    Project p;
    UndoStack stack;
    const int n1 = p.structure.addNode({0, 0, 0}), n2 = p.structure.addNode({4, 0, 0});
    p.structure.addNode({2, 0, 0});
    p.structure.addBar(n1, n2);
    RepairReport report;
    ASSERT_TRUE(repairModel(p, stack, &report));
    EXPECT_EQ(list(p).size(), 2u);
}

TEST(DeflectionGroup, DeletingABarDissolvesItsGroup) {
    Project p = line(3);
    UndoStack stack;
    ASSERT_EQ(createDeflectionGroup(p, stack, 1, 4, DeflectionType::TangentToNode1, DeflectionType::Secant), DeflectionError::None);
    ASSERT_TRUE(deleteBars(p, stack, {2}));
    EXPECT_EQ(list(p).size(), 2u);  // B1 and B3 are separate groups again (the stale record is kept for undo, never listed or saved)
    EXPECT_EQ(project_json::dumpSection(p, "deflection_groups").find("\"bars\": ["), std::string::npos);
    EXPECT_TRUE(project_json::validate(p).empty());
    stack.undo();
    ASSERT_EQ(list(p).size(), 1u);
    EXPECT_EQ(list(p)[0].bars, (std::vector<int>{1, 2, 3}));
}

TEST(DeflectionGroup, DeletingAnIntermediateNodeMergesTheBars) {
    Project p = line(2);
    UndoStack stack;
    ASSERT_EQ(createDeflectionGroup(p, stack, 1, 3, DeflectionType::TangentToNode1, DeflectionType::Secant), DeflectionError::None);
    ASSERT_TRUE(deleteNodes(p, stack, {2}));
    ASSERT_EQ(p.structure.bars.size(), 1u);
    auto groups = list(p);
    ASSERT_EQ(groups.size(), 1u);
    EXPECT_EQ(groups[0].xy, DeflectionType::TangentToNode1);
    EXPECT_EQ(groups[0].bars, std::vector<int>{p.structure.bars[0].id});
    stack.undo();
    groups = list(p);
    ASSERT_EQ(groups.size(), 1u);
    EXPECT_EQ(groups[0].bars, (std::vector<int>{1, 2}));
}

TEST(DeflectionGroup, MergeTakesTheGroupOfTheFirstBarAndLeavesTheRest) {
    Project p = line(3);
    UndoStack stack;
    ASSERT_EQ(editDeflectionTypes(p, stack, {1}, DeflectionType::TangentToNode2, DeflectionType::Secant), DeflectionError::None);
    ASSERT_TRUE(deleteNodes(p, stack, {2}));  // merges B1 and B2 into a new bar
    const auto groups = list(p);
    ASSERT_EQ(groups.size(), 2u);
    // Sorted by lowest bar id: B3 first, then the merged bar (B4), which inherited the group of B1.
    EXPECT_EQ(groups[0].bars, std::vector<int>{3});
    EXPECT_EQ(groups[0].xy, DeflectionType::Secant);
    EXPECT_EQ(groups[1].xy, DeflectionType::TangentToNode2);
    EXPECT_EQ(groups[1].bars.size(), 1u);
}

TEST(DeflectionGroup, InvertingASingleBarSwapsTheTangentTypes) {
    Project p = line(2);
    UndoStack stack;
    ASSERT_EQ(editDeflectionTypes(p, stack, {1}, DeflectionType::TangentToNode1, DeflectionType::Secant), DeflectionError::None);
    EXPECT_EQ(list(p)[0].node1, 1);
    ASSERT_EQ(invertBarAxis(p, stack, 1), PieceError::None);
    const auto groups = list(p);
    EXPECT_EQ(groups[0].node1, 2);  // the x origin of the bar is now N2
    EXPECT_EQ(groups[0].xy, DeflectionType::TangentToNode2);  // same physical node (N1)
    stack.undo();
    EXPECT_EQ(list(p)[0].node1, 1);
    EXPECT_EQ(list(p)[0].xy, DeflectionType::TangentToNode1);
}

TEST(DeflectionGroup, InvertingAWholeGroupSwapsNodes) {
    Project p = line(2);
    UndoStack stack;
    ASSERT_EQ(createPiece(p, stack, 1, 3), PieceError::None);
    ASSERT_EQ(createDeflectionGroup(p, stack, 1, 3, DeflectionType::TangentToNode1, DeflectionType::TangentToNode2), DeflectionError::None);
    ASSERT_EQ(invertBarAxis(p, stack, 1), PieceError::None);  // inverts the whole piece
    const auto groups = list(p);
    ASSERT_EQ(groups.size(), 1u);
    EXPECT_EQ(groups[0].node1, 3);
    EXPECT_EQ(groups[0].node2, 1);
    EXPECT_EQ(groups[0].xy, DeflectionType::TangentToNode2);
    EXPECT_EQ(groups[0].xz, DeflectionType::TangentToNode1);
    EXPECT_EQ(groups[0].bars, (std::vector<int>{2, 1}));
    stack.undo();
    EXPECT_EQ(list(p)[0].node1, 1);
    EXPECT_EQ(list(p)[0].xy, DeflectionType::TangentToNode1);
}

TEST(DeflectionGroup, MovingANodeOffTheLineDissolvesTheGroup) {
    Project p = line(2);
    UndoStack stack;
    ASSERT_EQ(createDeflectionGroup(p, stack, 1, 3, DeflectionType::TangentToNode1, DeflectionType::Secant), DeflectionError::None);
    ASSERT_TRUE(moveNode(p, stack, 2, {4, 1, 0}));
    EXPECT_EQ(list(p).size(), 2u);
    EXPECT_TRUE(p.deflection.groups.empty() || list(p).size() == 2u);
    EXPECT_EQ(list(p)[0].xy, DeflectionType::Secant);
    stack.undo();
    ASSERT_EQ(list(p).size(), 1u);
    EXPECT_EQ(list(p)[0].xy, DeflectionType::TangentToNode1);
    // Sliding it along the line keeps the group.
    ASSERT_TRUE(moveNode(p, stack, 2, {6, 0, 0}));
    EXPECT_EQ(list(p).size(), 1u);
}

TEST(DeflectionGroup, MovingTheEndOfABarDissolvesTheGroup) {
    Project p = line(2);
    UndoStack stack;
    const int off = p.structure.addNode({4, 3, 0});
    ASSERT_EQ(createDeflectionGroup(p, stack, 1, 3, DeflectionType::TangentToNode1, DeflectionType::Secant), DeflectionError::None);
    ASSERT_EQ(moveBarEnd(p, stack, 2, BarEnd::End, off), PieceError::None);
    EXPECT_EQ(list(p).size(), 2u);
    EXPECT_EQ(list(p)[0].xy, DeflectionType::Secant);
    stack.undo();
    EXPECT_EQ(list(p).size(), 1u);
}

TEST(DeflectionGroup, JsonRoundTripAndOldFiles) {
    Project p = line(3);
    UndoStack stack;
    ASSERT_EQ(createDeflectionGroup(p, stack, 1, 3, DeflectionType::TangentToNode2, DeflectionType::Secant), DeflectionError::None);
    DeflectionLimits l;
    l.item(LimitSlot::MaxAbsXY).enabled = true;
    l.item(LimitSlot::MaxAbsXY).absoluteMm = 7.5;
    l.item(LimitSlot::ActiveRelXZ).enabled = true;
    l.item(LimitSlot::ActiveRelXZ).relativeDivisor = 350.0;
    ASSERT_FALSE(setDeflectionLimits(p, stack, {1}, l).has_value());
    ASSERT_EQ(editDeflectionTypes(p, stack, {3}, DeflectionType::Secant, DeflectionType::TangentToNode1), DeflectionError::None);
    ASSERT_TRUE(project_json::validate(p).empty());
    ASSERT_TRUE(project_json::isKnownSection("deflection_groups"));
    const std::string text = project_json::dumpSection(p, "deflection_groups");
    Project q = line(3);
    project_json::loadSection(q, "deflection_groups", text);
    EXPECT_EQ(q.deflection, p.deflection);
    EXPECT_EQ(list(q), list(p));
    EXPECT_EQ(list(q)[0].limits, l);
    EXPECT_EQ(list(q)[0].xy, DeflectionType::TangentToNode2);
    // An old file without the section (or an empty one) is "one secant group per bar".
    Project old = line(3);
    project_json::loadSection(old, "deflection_groups", "{}");
    EXPECT_TRUE(old.deflection.empty());
    EXPECT_EQ(list(old).size(), 3u);
    EXPECT_EQ(project_json::sectionTitle("deflection_groups"), "Grupos de flecha");
}

TEST(DeflectionGroup, NormaliseCleansStaleData) {
    Project p = line(3);
    DeflectionData data;
    DeflectionBarGroup g;
    g.bars = {1, 3, 99};  // not a chain (gap) once 99 is dropped
    g.xy = DeflectionType::TangentToNode1;
    data.groups.push_back(g);
    DeflectionBarGroup d;  // a default one-bar group
    d.bars = {2};
    data.groups.push_back(d);
    DeflectionBarGroup t;  // one bar, tangent
    t.bars = {2};
    t.xz = DeflectionType::TangentToNode2;
    data.groups.push_back(t);
    const DeflectionData clean = normaliseDeflection(data, p.structure);
    ASSERT_EQ(clean.groups.size(), 1u);
    EXPECT_EQ(clean.groups[0].bars, std::vector<int>{2});
    EXPECT_EQ(clean.groups[0].xz, DeflectionType::TangentToNode2);
    EXPECT_TRUE(validateDeflection(clean).empty());
    EXPECT_FALSE(validateDeflection(data).empty());  // a default group stored
}

TEST(DeflectionGroup, ValidationDetectsBadStoredData) {
    DeflectionData data;
    DeflectionBarGroup g;
    g.bars = {1};
    g.xy = static_cast<DeflectionType>(7);
    data.groups.push_back(g);
    EXPECT_FALSE(validateDeflection(data).empty());
    data.groups[0].xy = DeflectionType::TangentToNode1;
    EXPECT_TRUE(validateDeflection(data).empty());
    data.groups[0].limits.item(LimitSlot::MaxRelXY).enabled = true;
    data.groups[0].limits.item(LimitSlot::MaxRelXY).relativeDivisor = 0.0;
    EXPECT_FALSE(validateDeflection(data).empty());
    data.groups[0].limits = {};
    data.groups.push_back(data.groups[0]);  // the bar in two groups
    EXPECT_FALSE(validateDeflection(data).empty());
}
