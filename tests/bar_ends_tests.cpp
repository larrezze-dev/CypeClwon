#include <gtest/gtest.h>

#include "core/bar_edit.h"
#include "core/bar_ends_edit.h"
#include "core/node_edit.h"
#include "core/piece_edit.h"
#include "core/project_json.h"
#include "core/tools_edit.h"

using namespace cc::core;

namespace {

/// Nodes N1 (0,0,0), N2 (4,0,0), N3 (8,0,0); bars B1 = N1-N2, B2 = N2-N3.
Project twoBars() {
    Project p;
    p.structure.addNode({0, 0, 0});
    p.structure.addNode({4, 0, 0});
    p.structure.addNode({8, 0, 0});
    p.structure.addBar(1, 2);
    p.structure.addBar(2, 3);
    return p;
}

EndFixityEdit originXY(double value) {
    EndFixityEdit e;
    e.value[kBarOrigin][kPlaneXY].coefficient = value;
    return e;
}

}  // namespace

TEST(BarEnds, DefaultsAndLabels) {
    BarEndsRecord rec;
    EXPECT_EQ(endFixityLabel(rec.ends[0]), "xy: 1.000 xz: 1.000 yz: 1.000");
    EXPECT_EQ(endTooltipLine(rec, kBarOrigin), "Origen : xy: 1.000 xz: 1.000 yz: 1.000");
    EXPECT_EQ(endTooltipLine(rec, kBarFinal), "Extremo : xy: 1.000 xz: 1.000 yz: 1.000");
    EXPECT_TRUE(isDefaultEnd(rec.ends[0]));
    EXPECT_FALSE(rec.ends[0].pinned);
    EXPECT_EQ(kDefaultEndRotationalStiffness, 500000.0);
}

TEST(BarEnds, EmpotramientoAppliesToAllSelectedBarsAndUndoes) {
    Project p = twoBars();
    UndoStack stack;
    EXPECT_FALSE(assignEndFixity(p, stack, {1, 2, 99, 1}, originXY(0.5)).has_value());
    EXPECT_EQ(p.barEnds.bars.size(), 2u);
    for (int id : {1, 2}) {
        const BarEndsRecord r = barEndsOf(p.barEnds, id);
        EXPECT_EQ(endFixityLabel(r.ends[kBarOrigin]), "xy: 0.500 xz: 1.000 yz: 1.000");
        EXPECT_EQ(endFixityLabel(r.ends[kBarFinal]), "xy: 1.000 xz: 1.000 yz: 1.000");
    }
    // Restoring the default removes the entries.
    EXPECT_FALSE(assignEndFixity(p, stack, {1, 2}, originXY(1.0)).has_value());
    EXPECT_TRUE(p.barEnds.empty());
    stack.undo();
    EXPECT_EQ(p.barEnds.bars.size(), 2u);
    stack.undo();
    EXPECT_TRUE(p.barEnds.empty());
    stack.redo();
    EXPECT_EQ(p.barEnds.bars.size(), 2u);
}

TEST(BarEnds, PlanesWithTheBoxOffAreUntouched) {
    Project p = twoBars();
    UndoStack stack;
    EndFixityEdit e = originXY(0.25);
    e.value[kBarFinal][kPlaneXZ].coefficient = 0.75;
    e.assignPlane = {false, true, true};  // xy not assigned
    ASSERT_FALSE(assignEndFixity(p, stack, {1}, e).has_value());
    const BarEndsRecord r = barEndsOf(p.barEnds, 1);
    EXPECT_EQ(r.ends[kBarOrigin].planes[kPlaneXY].coefficient, 1.0);
    EXPECT_EQ(r.ends[kBarFinal].planes[kPlaneXZ].coefficient, 0.75);
}

TEST(BarEnds, RangeAndTorsionValidation) {
    Project p = twoBars();
    UndoStack stack;
    const std::size_t depth = stack.count();
    for (double bad : {2.0, -0.1, 1.0001}) {
        const auto issue = assignEndFixity(p, stack, {1}, originXY(bad));
        ASSERT_TRUE(issue.has_value());
        EXPECT_EQ(issue->text, "El valor de '' debe estar entre 0.000 y 1.000 .");
    }
    EXPECT_EQ(stack.count(), depth);
    // A plane not being assigned is not checked.
    EndFixityEdit skipped = originXY(5.0);
    skipped.assignPlane[kPlaneXY] = false;
    EXPECT_FALSE(checkEndFixityEdit(skipped).has_value());
    // Torsion: binary.
    EndFixityEdit torsion;
    torsion.value[kBarFinal][kPlaneYZ].coefficient = 0.5;
    EXPECT_TRUE(checkEndFixityEdit(torsion).has_value());
    torsion.value[kBarFinal][kPlaneYZ].coefficient = 0.0;
    EXPECT_FALSE(checkEndFixityEdit(torsion).has_value());
}

TEST(BarEnds, RotationalStiffness) {
    EndFixityEdit e;
    e.value[kBarOrigin][kPlaneXZ].kind = EndFixityKind::Rotational;
    EXPECT_TRUE(endFixityEditAsksDefaultStiffness(e));  // 500000 is the default: the dialog asks first
    EXPECT_NE(endFixityDefaultStiffnessQuestion().find("¿Desea continuar?"), std::string::npos);
    e.value[kBarOrigin][kPlaneXZ].stiffnessKnMPerRad = 1234.5;
    EXPECT_FALSE(endFixityEditAsksDefaultStiffness(e));
    EXPECT_FALSE(checkEndFixityEdit(e).has_value());
    e.assignPlane[kPlaneXZ] = false;  // the plane is not assigned: nothing to ask
    e.value[kBarOrigin][kPlaneXZ].stiffnessKnMPerRad = kDefaultEndRotationalStiffness;
    EXPECT_FALSE(endFixityEditAsksDefaultStiffness(e));
    e.assignPlane[kPlaneXZ] = true;
    e.value[kBarOrigin][kPlaneXZ].stiffnessKnMPerRad = -1.0;
    EXPECT_TRUE(checkEndFixityEdit(e).has_value());

    Project p = twoBars();
    UndoStack stack;
    e.value[kBarOrigin][kPlaneXZ].stiffnessKnMPerRad = 1234.5;
    ASSERT_FALSE(assignEndFixity(p, stack, {1}, e).has_value());
    EXPECT_EQ(endFixityLabel(barEndsOf(p.barEnds, 1).ends[kBarOrigin]), "xy: 1.000 xz: 1234.50 kN·m/rad yz: 1.000");
}

TEST(BarEnds, ArticulationIsADatumApartFromTheCoefficients) {
    Project p = twoBars();
    UndoStack stack;
    EXPECT_TRUE(articulateBars(p, stack, {1, 2}));
    for (int id : {1, 2}) {
        const BarEndsRecord r = barEndsOf(p.barEnds, id);
        EXPECT_TRUE(r.ends[kBarOrigin].pinned);
        EXPECT_TRUE(r.ends[kBarFinal].pinned);
        EXPECT_EQ(endFixityLabel(r.ends[kBarOrigin]), "xy: 1.000 xz: 1.000 yz: 1.000");  // measured: coefficients unchanged
    }
    EXPECT_FALSE(articulateBars(p, stack, {1, 2}));  // already pinned: nothing to do
    EXPECT_FALSE(articulateBars(p, stack, {99}));
    EXPECT_TRUE(removeArticulations(p, stack, {1}, {true, false}));
    EXPECT_FALSE(barEndsOf(p.barEnds, 1).ends[kBarOrigin].pinned);
    EXPECT_TRUE(barEndsOf(p.barEnds, 1).ends[kBarFinal].pinned);
    EXPECT_TRUE(removeArticulations(p, stack, {1}));
    EXPECT_EQ(p.barEnds.bars.count(1), 0u);
    stack.undo();
    stack.undo();
    stack.undo();
    EXPECT_TRUE(p.barEnds.empty());
}

TEST(BarEnds, DeletingABarKeepsItsDataForUndoAndIsNotSaved) {
    Project p = twoBars();
    UndoStack stack;
    articulateBars(p, stack, {1});
    ASSERT_TRUE(deleteBars(p, stack, {1}));
    EXPECT_TRUE(project_json::validate(p).empty());  // an orphan entry does not block saving
    EXPECT_EQ(project_json::dumpSection(p, "bar_ends").find("\"bar\": 1"), std::string::npos);
    stack.undo();
    EXPECT_TRUE(barEndsOf(p.barEnds, 1).ends[kBarOrigin].pinned);
}

TEST(BarEnds, DividingABarHandsTheEndToTheLastStretch) {
    // Bar B1 (0..4) is divided by the node at x = 2 (repair divides bars at the nodes lying on them).
    Project p;
    UndoStack stack;
    const int n1 = p.structure.addNode({0, 0, 0}), n2 = p.structure.addNode({4, 0, 0}), mid = p.structure.addNode({2, 0, 0});
    const int b = p.structure.addBar(n1, n2);
    EndFixityEdit e = originXY(0.5);
    e.value[kBarFinal][kPlaneXZ].coefficient = 0.25;
    ASSERT_FALSE(assignEndFixity(p, stack, {b}, e).has_value());
    ASSERT_TRUE(articulateBars(p, stack, {b}));
    RepairReport report;
    ASSERT_TRUE(repairModel(p, stack, &report));
    ASSERT_EQ(report.splitBars, 1);
    ASSERT_EQ(p.structure.bars.size(), 2u);
    int tail = 0;
    for (const Bar& bar : p.structure.bars)
        if (bar.id != b) tail = bar.id;
    ASSERT_EQ(p.structure.findBar(b)->endNode, mid);
    const BarEndsRecord head = barEndsOf(p.barEnds, b), last = barEndsOf(p.barEnds, tail);
    EXPECT_EQ(head.ends[kBarOrigin].planes[kPlaneXY].coefficient, 0.5);
    EXPECT_TRUE(head.ends[kBarOrigin].pinned);
    EXPECT_TRUE(isDefaultEnd(head.ends[kBarFinal]));  // the new interior end is the default
    EXPECT_TRUE(isDefaultEnd(last.ends[kBarOrigin]));
    EXPECT_EQ(last.ends[kBarFinal].planes[kPlaneXZ].coefficient, 0.25);
    EXPECT_TRUE(last.ends[kBarFinal].pinned);
    stack.undo();
    EXPECT_EQ(p.structure.bars.size(), 1u);
    EXPECT_EQ(barEndsOf(p.barEnds, b).ends[kBarFinal].planes[kPlaneXZ].coefficient, 0.25);
    EXPECT_EQ(p.barEnds.bars.size(), 1u);
}

TEST(BarEnds, InvertingTheAxisSwapsTheEnds) {
    Project p = twoBars();
    UndoStack stack;
    ASSERT_FALSE(assignEndFixity(p, stack, {1}, originXY(0.5)).has_value());
    articulateBars(p, stack, {1}, {false, true});
    ASSERT_EQ(invertBarAxis(p, stack, 1), PieceError::None);
    EXPECT_EQ(p.structure.findBar(1)->startNode, 2);
    const BarEndsRecord r = barEndsOf(p.barEnds, 1);
    EXPECT_EQ(r.ends[kBarFinal].planes[kPlaneXY].coefficient, 0.5);  // the physical end N1 keeps its 0.5
    EXPECT_TRUE(r.ends[kBarOrigin].pinned);                          // the pinned end N2 is now the origin
    stack.undo();
    EXPECT_EQ(barEndsOf(p.barEnds, 1).ends[kBarOrigin].planes[kPlaneXY].coefficient, 0.5);
    EXPECT_TRUE(barEndsOf(p.barEnds, 1).ends[kBarFinal].pinned);
}

TEST(BarEnds, DeletingANodeBetweenTwoAlignedBarsMergesTheFarEnds) {
    Project p = twoBars();  // B1 N1-N2, B2 N2-N3; deleting N2 replaces them by one bar N1-N3
    UndoStack stack;
    EndFixityEdit e;
    e.value[kBarOrigin][kPlaneXY].coefficient = 0.5;  // origin of B1 (at N1)
    e.value[kBarFinal][kPlaneXZ].coefficient = 0.25;  // end of B1 (at N2, vanishes)
    ASSERT_FALSE(assignEndFixity(p, stack, {1}, e).has_value());
    EndFixityEdit e2;
    e2.value[kBarFinal][kPlaneXY].coefficient = 0.75;  // end of B2 (at N3)
    ASSERT_FALSE(assignEndFixity(p, stack, {2}, e2).has_value());
    ASSERT_TRUE(deleteNodes(p, stack, {2}));
    ASSERT_EQ(p.structure.bars.size(), 1u);
    const int merged = p.structure.bars[0].id;
    const Bar& bar = *p.structure.findBar(merged);
    ASSERT_EQ(bar.startNode, 1);
    ASSERT_EQ(bar.endNode, 3);
    const BarEndsRecord r = barEndsOf(p.barEnds, merged);
    EXPECT_EQ(r.ends[kBarOrigin].planes[kPlaneXY].coefficient, 0.5);
    EXPECT_EQ(r.ends[kBarFinal].planes[kPlaneXY].coefficient, 0.75);
    EXPECT_EQ(r.ends[kBarOrigin].planes[kPlaneXZ].coefficient, 1.0);
    stack.undo();
    EXPECT_EQ(barEndsOf(p.barEnds, 1).ends[kBarFinal].planes[kPlaneXZ].coefficient, 0.25);
    EXPECT_EQ(p.barEnds.bars.count(merged), 0u);
}

TEST(BarEnds, JsonRoundTripAndOldFiles) {
    Project p = twoBars();
    UndoStack stack;
    EndFixityEdit e = originXY(0.5);
    e.value[kBarFinal][kPlaneXZ].kind = EndFixityKind::Rotational;
    e.value[kBarFinal][kPlaneXZ].stiffnessKnMPerRad = 2500.25;
    e.value[kBarFinal][kPlaneYZ].coefficient = 0.0;
    ASSERT_FALSE(assignEndFixity(p, stack, {1, 2}, e).has_value());
    articulateBars(p, stack, {2}, {true, false});

    ASSERT_TRUE(project_json::isKnownSection("bar_ends"));
    const std::string text = project_json::dumpSection(p, "bar_ends");
    Project q = twoBars();
    project_json::loadSection(q, "bar_ends", text);
    EXPECT_EQ(q.barEnds, p.barEnds);
    EXPECT_TRUE(project_json::validate(q).empty());

    Project old = twoBars();
    project_json::loadSection(old, "bar_ends", "{}");
    EXPECT_TRUE(old.barEnds.empty());
    // Missing keys take the defaults.
    project_json::loadSection(old, "bar_ends", R"({"bars":[{"bar":1,"ends":[{"pinned":true}]}]})");
    EXPECT_TRUE(barEndsOf(old.barEnds, 1).ends[kBarOrigin].pinned);
    EXPECT_TRUE(isDefaultEnd(barEndsOf(old.barEnds, 1).ends[kBarFinal]));
}
