#include <gtest/gtest.h>

#include <limits>

#include "core/bar_edit.h"
#include "core/buckling_edit.h"
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

BucklingEdit planeEdit(BucklingKind kind, double beta = 1.0) {
    BucklingEdit e;
    e.xy.kind = kind;
    e.xy.beta = beta;
    return e;
}

}  // namespace

TEST(Buckling, DefaultsAndKindTable) {
    const BarBuckling b;
    EXPECT_EQ(b.xy.kind, BucklingKind::PinnedPinned);
    EXPECT_EQ(b.xy.beta, 1.0);
    EXPECT_EQ(b.xz.beta, 1.0);
    EXPECT_FALSE(b.xy.reviewed);
    EXPECT_EQ(b.top.kind, LateralKind::None);
    EXPECT_TRUE(isDefaultBuckling(b));
    EXPECT_EQ(*bucklingBetaOfKind(BucklingKind::None), 0.0);
    EXPECT_EQ(*bucklingBetaOfKind(BucklingKind::FixedFixed), 0.5);
    EXPECT_EQ(*bucklingBetaOfKind(BucklingKind::PinnedFixed), 0.7);
    EXPECT_EQ(*bucklingBetaOfKind(BucklingKind::PinnedPinned), 1.0);
    EXPECT_EQ(*bucklingBetaOfKind(BucklingKind::Cantilever), 2.0);
    EXPECT_FALSE(bucklingBetaOfKind(BucklingKind::UserBeta).has_value());
    EXPECT_FALSE(bucklingBetaOfKind(BucklingKind::Computed).has_value());
    EXPECT_EQ(static_cast<int>(BucklingKind::Computed), 7);
    EXPECT_EQ(static_cast<int>(LateralKind::UserSpacing), 5);
}

TEST(Buckling, KindFromBetaUsesTolerance) {
    EXPECT_EQ(bucklingKindFromBeta(0.0), BucklingKind::None);
    EXPECT_EQ(bucklingKindFromBeta(5e-7), BucklingKind::None);
    EXPECT_EQ(bucklingKindFromBeta(0.5), BucklingKind::FixedFixed);
    EXPECT_EQ(bucklingKindFromBeta(0.7 + 5e-7), BucklingKind::PinnedFixed);
    EXPECT_EQ(bucklingKindFromBeta(1.0), BucklingKind::PinnedPinned);
    EXPECT_EQ(bucklingKindFromBeta(2.0 - 5e-7), BucklingKind::Cantilever);
    EXPECT_EQ(bucklingKindFromBeta(1.0 + 2e-6), BucklingKind::UserBeta);
    EXPECT_EQ(bucklingKindFromBeta(1.3), BucklingKind::UserBeta);
}

TEST(Buckling, EffectiveValues) {
    BucklingPlane p;
    p.kind = BucklingKind::UserLength;
    p.lengthM = 3.0;
    EXPECT_DOUBLE_EQ(effectiveBeta(p, 4.0), 0.75);
    EXPECT_EQ(effectiveBeta(p, 0.0), 0.0);
    p.kind = BucklingKind::None;
    EXPECT_EQ(effectiveBeta(p, 4.0), 0.0);
    p = bucklingPlaneFromBeta(0.7);
    EXPECT_EQ(effectiveBeta(p, 4.0), 0.7);

    LateralBuckling f;
    EXPECT_EQ(lateralLengthFactor(f, 4.0), 0.0);
    f.kind = LateralKind::HalfLength;
    EXPECT_EQ(lateralLengthFactor(f, 4.0), 0.5);
    f.kind = LateralKind::Length;
    EXPECT_EQ(lateralLengthFactor(f, 4.0), 1.0);
    f.kind = LateralKind::DoubleLength;
    EXPECT_EQ(lateralLengthFactor(f, 4.0), 2.0);
    f.kind = LateralKind::UserCoeff;
    f.coeff = 0.8;
    EXPECT_EQ(lateralLengthFactor(f, 4.0), 0.8);
    f.kind = LateralKind::UserSpacing;
    f.spacingM = 1.0;
    EXPECT_DOUBLE_EQ(lateralLengthFactor(f, 4.0), 0.25);
}

TEST(Buckling, RangeValidation) {
    BucklingEdit e = planeEdit(BucklingKind::UserBeta, 0.0);
    EXPECT_FALSE(checkBucklingEdit(e).has_value());  // measured range [0.000, 100.000]
    e.xy.beta = 100.0;
    EXPECT_FALSE(checkBucklingEdit(e).has_value());
    for (double bad : {100.1, -0.001, -1.0, std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity()}) {
        e.xy.beta = bad;
        const auto issue = checkBucklingEdit(e);
        ASSERT_TRUE(issue.has_value()) << bad;
        // measured text: two spaces after the first bound and a space before the full stop
        EXPECT_EQ(issue->text, "El valor de 'Coeficiente de pandeo' debe estar entre 0.000  y 100.000 .");
    }
    // a plane that is not assigned is not validated
    e.assignXY = false;
    EXPECT_FALSE(checkBucklingEdit(e).has_value());
    // fixed kinds ignore their fields
    BucklingEdit fixedKind = planeEdit(BucklingKind::FixedFixed, -5.0);
    EXPECT_FALSE(checkBucklingEdit(fixedKind).has_value());

    BucklingEdit len;
    len.xz.kind = BucklingKind::UserLength;
    len.xz.lengthM = 1000.0;
    EXPECT_FALSE(checkBucklingEdit(len).has_value());
    len.xz.lengthM = 1000.5;
    EXPECT_EQ(checkBucklingEdit(len)->text, "El valor de 'Longitud' debe estar entre 0.000 y 1000.000.");  // measured: too high names 0.000
    len.xz.lengthM = 1000.1;
    EXPECT_EQ(checkBucklingEdit(len)->text, "El valor de 'Longitud' debe estar entre 0.000 y 1000.000.");
    len.xz.lengthM = 0.0009;
    EXPECT_EQ(checkBucklingEdit(len)->text, "El valor de 'Longitud' debe estar entre 0.001 y 1000.000.");  // measured: too low names 0.001
    len.xz.lengthM = 0.0;
    EXPECT_EQ(checkBucklingEdit(len)->text, "El valor de 'Longitud' debe estar entre 0.001 y 1000.000.");
    len.xz.lengthM = 0.0005;
    EXPECT_TRUE(checkBucklingEdit(len).has_value());

    BucklingEdit cm;
    cm.assignMomentXZ = true;
    cm.xz.momentCoeff = 10.1;
    EXPECT_EQ(checkBucklingEdit(cm)->text, "El valor de 'Coeficiente de momento equivalente Cm' debe estar entre 0.000  y 10.000 .");
    cm.xz.momentCoeff = 10.0;
    EXPECT_FALSE(checkBucklingEdit(cm).has_value());
    cm.assignMomentXZ = false;  // not assigned: not validated
    cm.xz.momentCoeff = 99.0;
    EXPECT_FALSE(checkBucklingEdit(cm).has_value());

    LateralEdit l;
    l.top.kind = LateralKind::UserSpacing;
    l.top.spacingM = 1000.0;
    EXPECT_FALSE(checkLateralEdit(l).has_value());
    l.top.spacingM = 1001.0;  // too high names 0.000 (measured for "Longitud"; the same message builder is assumed here)
    EXPECT_EQ(checkLateralEdit(l)->text, "El valor de 'Separación entre arriostramientos' debe estar entre 0.000 y 1000.000.");
    l.top.spacingM = 0.0;  // measured: 0 rejected
    ASSERT_TRUE(checkLateralEdit(l).has_value());
    EXPECT_EQ(checkLateralEdit(l)->text, "El valor de 'Separación entre arriostramientos' debe estar entre 0.001 y 1000.000.");
    l.top.spacingM = 2.0;
    l.bottom.kind = LateralKind::Length;
    l.bottom.momentCoeff = 0.0;  // Cm = 0 accepted (range [0, 10])
    EXPECT_FALSE(checkLateralEdit(l).has_value());
    l.bottom.momentCoeff = 10.5;
    EXPECT_TRUE(checkLateralEdit(l).has_value());
    l.bottom.kind = LateralKind::None;  // no check: the moment coefficient is not used
    EXPECT_FALSE(checkLateralEdit(l).has_value());
    l.bottom.kind = LateralKind::UserCoeff;
    l.bottom.momentCoeff = 1.0;
    l.bottom.coeff = 10.0;
    EXPECT_FALSE(checkLateralEdit(l).has_value());
    l.bottom.coeff = 10.1;
    EXPECT_EQ(checkLateralEdit(l)->text, "El valor de 'Coeficiente de pandeo lateral' debe estar entre 0.000  y 10.000 .");
    l.bottom.coeff = 0.0;  // measured: 0 accepted
    EXPECT_FALSE(checkLateralEdit(l).has_value());
    l.c1 = 10.5;
    EXPECT_EQ(checkLateralEdit(l)->text, "El valor de '' debe estar entre 0.000  y 10.000 .");  // empty name: defect of the original
    l.assignC1 = false;
    EXPECT_FALSE(checkLateralEdit(l).has_value());
}

TEST(Buckling, MeasuredDefaults) {
    const BarBuckling b;
    EXPECT_FALSE(b.xy.translational);  // "Intraslacional" is the initial choice
    EXPECT_EQ(b.xy.momentCoeff, 1.0);
    EXPECT_EQ(b.c1, 1.0);
    EXPECT_TRUE(b.considerInAnalysis);
    EXPECT_FALSE(b.updateXY);
    EXPECT_FALSE(b.updateXZ);
    EXPECT_EQ(b.top.spacingM, 3.0);
    EXPECT_EQ(b.top.coeff, 1.0);
}

TEST(Buckling, LabelsAndInfoLines) {
    BarBuckling b;
    EXPECT_EQ(bucklingLabelLines(b, 4.0), (std::vector<std::string>{"xy: 1.00 / 1.00", "xz: 1.00 / 1.00"}));
    EXPECT_EQ(lateralLabelLines(b, 4.0), (std::vector<std::string>{"Sup.: L, 1.00", "Inf.: L, 1.00", "C1: 1.00"}));
    EXPECT_EQ(bucklingInfoLines(b, 4.0), (std::vector<std::string>{"Pandeo", "xy : 1.00 / 1.00", "xz : 1.00 / 1.00", "Análisis de pandeo", "Se considera en el análisis"}));
    b.xy = bucklingPlaneFromBeta(0.5);
    b.xz = bucklingPlaneFromBeta(2.0);
    b.xz.momentCoeff = 0.85;
    b.xy = bucklingPlaneFromBeta(0.0);
    EXPECT_EQ(bucklingLabelLines(b, 4.0), (std::vector<std::string>{"xy: 0.00 / 1.00", "xz: 2.00 / 0.85"}));
    b.xz.kind = BucklingKind::UserLength;  // measured: the label of a user length is "xz: 1.00 m / 1.00"
    b.xz.lengthM = 1.0;
    b.xz.momentCoeff = 1.0;
    EXPECT_EQ(bucklingLabelLines(b, 4.0), (std::vector<std::string>{"xy: 0.00 / 1.00", "xz: 1.00 m / 1.00"}));
    b.top.kind = LateralKind::UserSpacing;
    b.top.spacingM = 3.0;
    b.top.momentCoeff = 0.85;
    b.bottom.kind = LateralKind::DoubleLength;
    b.c1 = 1.5;
    EXPECT_EQ(lateralLabelLines(b, 4.0), (std::vector<std::string>{"Sup.: 3.000 m, 0.85", "Inf.: 2L, 1.00", "C1: 1.50"}));
    b.considerInAnalysis = false;
    EXPECT_EQ(bucklingInfoLines(b, 4.0).back(), "No se considera en el análisis");
}

TEST(Buckling, LateralLabelsMeasuredInTheOriginal) {
    BarBuckling b;
    EXPECT_TRUE(lateralInfoLines(b).empty()) << "a bar without lateral data has no floating block";
    // 0.5L / 0.75L (user coefficient)
    b.top.kind = LateralKind::HalfLength;
    b.top.reviewed = true;
    b.bottom.kind = LateralKind::UserCoeff;
    b.bottom.coeff = 0.75;
    b.bottom.reviewed = true;
    EXPECT_EQ(lateralLabelLines(b, 3.0), (std::vector<std::string>{"Sup.: 0.5L, 1.00", "Inf.: 0.75L, 1.00", "C1: 1.00"}));
    EXPECT_EQ(lateralInfoLines(b), (std::vector<std::string>{"Pandeo lateral", "Sup. : 0.5L, 1.00", "Inf. : 0.75L, 1.00", "C\u2081 : 1.00"}));
    b.top.kind = LateralKind::UserCoeff;
    b.top.coeff = 1.2345;  // two decimals
    EXPECT_EQ(lateralLabelLines(b, 3.0).front(), "Sup.: 1.24L, 1.00");
    b.top.coeff = 1.0;  // a user coefficient of 1 reads "L"
    EXPECT_EQ(lateralLabelLines(b, 3.0).front(), "Sup.: L, 1.00");
    // "No comprueba" assigned: the line disappears (a never assigned flange shows "L")
    b.top.kind = LateralKind::None;
    EXPECT_EQ(lateralLabelLines(b, 3.0), (std::vector<std::string>{"Inf.: 0.75L, 1.00", "C1: 1.00"}));
    EXPECT_EQ(lateralInfoLines(b), (std::vector<std::string>{"Pandeo lateral", "Inf. : 0.75L, 1.00", "C\u2081 : 1.00"}));
    b.top.reviewed = false;
    EXPECT_EQ(lateralLabelLines(b, 3.0).front(), "Sup.: L, 1.00");
}

TEST(Buckling, AssignMomentCriteriaAndC1) {
    Project p = twoBars();
    UndoStack stack;
    BucklingEdit e;
    e.assignMomentXY = true;
    e.xy.momentCoeff = 0.7;
    e.considerInAnalysis = false;
    e.updateXZ = true;
    ASSERT_FALSE(assignBuckling(p, stack, {1}, e).has_value());
    BarBuckling b = bucklingOf(p.buckling, 1);
    EXPECT_EQ(b.xy.momentCoeff, 0.7);
    EXPECT_EQ(b.xz.momentCoeff, 1.0);  // xz moment not assigned
    EXPECT_FALSE(b.considerInAnalysis);
    EXPECT_TRUE(b.updateXZ);
    LateralEdit le;
    le.c1 = 1.5;
    ASSERT_FALSE(assignLateralBuckling(p, stack, {1}, le).has_value());
    EXPECT_EQ(bucklingOf(p.buckling, 1).c1, 1.5);
    le.assignC1 = false;
    le.c1 = 2.5;
    ASSERT_FALSE(assignLateralBuckling(p, stack, {1}, le).has_value());
    EXPECT_EQ(bucklingOf(p.buckling, 1).c1, 1.5);
    const std::string text = project_json::dumpSection(p, "buckling");
    Project q = twoBars();
    project_json::loadSection(q, "buckling", text);
    EXPECT_EQ(q.buckling, p.buckling);
}

TEST(Buckling, AssignRebuildsKindFromBetaAndMarksReviewed) {
    Project p = twoBars();
    UndoStack stack;
    // measured: a typed 0.5 keeps the "Otros valores" option (the radio is persisted), it is not turned into the FixedFixed kind
    ASSERT_FALSE(assignBuckling(p, stack, {1, 2, 99}, planeEdit(BucklingKind::UserBeta, 0.5)).has_value());
    EXPECT_EQ(p.buckling.bars.size(), 2u);
    BarBuckling b = bucklingOf(p.buckling, 1);
    EXPECT_EQ(b.xy.kind, BucklingKind::UserBeta);
    EXPECT_EQ(b.xy.beta, 0.5);
    EXPECT_TRUE(b.xy.reviewed);
    EXPECT_TRUE(b.xz.reviewed);  // xz is assigned too (default PinnedPinned radio)
    EXPECT_EQ(b.xz.beta, 1.0);
}

TEST(Buckling, AssignOnlyChosenPlane) {
    Project p = twoBars();
    UndoStack stack;
    BucklingEdit e = planeEdit(BucklingKind::Cantilever);
    e.assignXZ = false;
    ASSERT_FALSE(assignBuckling(p, stack, {1}, e).has_value());
    const BarBuckling b = bucklingOf(p.buckling, 1);
    EXPECT_EQ(b.xy.beta, 2.0);
    EXPECT_TRUE(b.xy.reviewed);
    EXPECT_EQ(b.xz, BucklingPlane{});
    EXPECT_EQ(unreviewedBucklingBars(p.buckling, p.structure), (std::vector<int>{1, 2}));  // B1 xz and B2 never accepted
    e.assignXZ = true;
    e.xz.kind = BucklingKind::PinnedPinned;
    ASSERT_FALSE(assignBuckling(p, stack, {1}, e).has_value());
    EXPECT_EQ(unreviewedBucklingBars(p.buckling, p.structure), (std::vector<int>{2}));
    stack.undo();
    stack.undo();
    EXPECT_TRUE(p.buckling.empty());
}

TEST(Buckling, UserLengthAndComputedAndNoBuckling) {
    Project p = twoBars();
    UndoStack stack;
    BucklingEdit e;
    e.xy.kind = BucklingKind::UserLength;
    e.xy.lengthM = 2.0;
    e.xz.kind = BucklingKind::Computed;
    e.xz.beta = 1.37;
    e.xz.translational = true;
    ASSERT_FALSE(assignBuckling(p, stack, {1}, e).has_value());
    BarBuckling b = bucklingOf(p.buckling, 1);
    EXPECT_EQ(b.xy.kind, BucklingKind::UserLength);
    EXPECT_DOUBLE_EQ(effectiveBeta(b.xy, 4.0), 0.5);
    EXPECT_EQ(b.xz.kind, BucklingKind::Computed);
    EXPECT_EQ(b.xz.beta, 1.37);
    EXPECT_TRUE(b.xz.translational);
    BucklingEdit none = planeEdit(BucklingKind::None);
    ASSERT_FALSE(assignBuckling(p, stack, {2}, none).has_value());
    EXPECT_EQ(effectiveBeta(bucklingOf(p.buckling, 2).xy, 4.0), 0.0);
}

TEST(Buckling, ErrorPushesNothing) {
    Project p = twoBars();
    UndoStack stack;
    EXPECT_TRUE(assignBuckling(p, stack, {1}, planeEdit(BucklingKind::UserBeta, 100.1)).has_value());
    EXPECT_TRUE(p.buckling.empty());
    EXPECT_FALSE(stack.canUndo());
}

TEST(Buckling, LateralAssignAndUndo) {
    Project p = twoBars();
    UndoStack stack;
    LateralEdit e;
    e.assignBottom = false;
    e.top.kind = LateralKind::DoubleLength;
    e.top.momentCoeff = 1.75;
    ASSERT_FALSE(assignLateralBuckling(p, stack, {1}, e).has_value());
    BarBuckling b = bucklingOf(p.buckling, 1);
    EXPECT_EQ(b.top.kind, LateralKind::DoubleLength);
    EXPECT_EQ(b.top.momentCoeff, 1.75);
    EXPECT_TRUE(b.top.reviewed);
    EXPECT_EQ(b.bottom, LateralBuckling{});
    // same assignment again: nothing changes, nothing pushed
    ASSERT_FALSE(assignLateralBuckling(p, stack, {1}, e).has_value());
    stack.undo();
    EXPECT_TRUE(p.buckling.empty());
    stack.redo();
    EXPECT_EQ(bucklingOf(p.buckling, 1).top.kind, LateralKind::DoubleLength);
}

TEST(Buckling, DeletingABarKeepsItsDataForUndoAndIsNotSaved) {
    Project p = twoBars();
    UndoStack stack;
    ASSERT_FALSE(assignBuckling(p, stack, {1}, planeEdit(BucklingKind::Cantilever)).has_value());
    ASSERT_TRUE(deleteBars(p, stack, {1}));
    EXPECT_TRUE(project_json::validate(p).empty());
    EXPECT_EQ(project_json::dumpSection(p, "buckling").find("\"bar\": 1"), std::string::npos);
    stack.undo();
    EXPECT_EQ(bucklingOf(p.buckling, 1).xy.beta, 2.0);
}

TEST(Buckling, DividingABarGivesEveryStretchTheData) {
    Project p;
    UndoStack stack;
    const int n1 = p.structure.addNode({0, 0, 0}), n2 = p.structure.addNode({4, 0, 0});
    p.structure.addNode({2, 0, 0});
    const int b = p.structure.addBar(n1, n2);
    ASSERT_FALSE(assignBuckling(p, stack, {b}, planeEdit(BucklingKind::FixedFixed)).has_value());
    LateralEdit le;
    le.top.kind = LateralKind::Length;
    ASSERT_FALSE(assignLateralBuckling(p, stack, {b}, le).has_value());
    RepairReport report;
    ASSERT_TRUE(repairModel(p, stack, &report));
    ASSERT_EQ(report.splitBars, 1);
    ASSERT_EQ(p.structure.bars.size(), 2u);
    int tail = 0;
    for (const Bar& bar : p.structure.bars)
        if (bar.id != b) tail = bar.id;
    EXPECT_EQ(bucklingOf(p.buckling, b), bucklingOf(p.buckling, tail));
    EXPECT_EQ(bucklingOf(p.buckling, tail).xy.beta, 0.5);
    EXPECT_EQ(bucklingOf(p.buckling, tail).top.kind, LateralKind::Length);
    stack.undo();
    EXPECT_EQ(p.structure.bars.size(), 1u);
    EXPECT_EQ(p.buckling.bars.size(), 1u);
    EXPECT_EQ(bucklingOf(p.buckling, b).xy.beta, 0.5);
}

TEST(Buckling, InvertingTheAxisKeepsTheData) {
    Project p = twoBars();
    UndoStack stack;
    ASSERT_FALSE(assignBuckling(p, stack, {1}, planeEdit(BucklingKind::PinnedFixed)).has_value());
    const BarBuckling before = bucklingOf(p.buckling, 1);
    ASSERT_EQ(invertBarAxis(p, stack, 1), PieceError::None);
    EXPECT_EQ(bucklingOf(p.buckling, 1), before);
    stack.undo();
    EXPECT_EQ(bucklingOf(p.buckling, 1), before);
}

TEST(Buckling, DeletingANodeBetweenTwoAlignedBarsMergesWithTheFirstBarData) {
    Project p = twoBars();
    UndoStack stack;
    ASSERT_FALSE(assignBuckling(p, stack, {1}, planeEdit(BucklingKind::Cantilever)).has_value());
    ASSERT_FALSE(assignBuckling(p, stack, {2}, planeEdit(BucklingKind::FixedFixed)).has_value());
    ASSERT_TRUE(deleteNodes(p, stack, {2}));
    ASSERT_EQ(p.structure.bars.size(), 1u);
    const int merged = p.structure.bars[0].id;
    EXPECT_EQ(bucklingOf(p.buckling, merged).xy.beta, 2.0);
    stack.undo();
    EXPECT_EQ(p.buckling.bars.count(merged), 0u);
    EXPECT_EQ(bucklingOf(p.buckling, 1).xy.beta, 2.0);
    EXPECT_EQ(bucklingOf(p.buckling, 2).xy.beta, 0.5);
}

TEST(Buckling, JsonRoundTripAndOldFiles) {
    Project p = twoBars();
    UndoStack stack;
    BucklingEdit e;
    e.xy.kind = BucklingKind::UserBeta;
    e.xy.beta = 1.3;
    e.xz.kind = BucklingKind::UserLength;
    e.xz.lengthM = 2.5;
    ASSERT_FALSE(assignBuckling(p, stack, {1}, e).has_value());
    LateralEdit le;
    le.top.kind = LateralKind::UserSpacing;
    le.top.spacingM = 1.5;
    le.top.momentCoeff = 1.2;
    le.bottom.kind = LateralKind::UserCoeff;
    le.bottom.coeff = 0.9;
    ASSERT_FALSE(assignLateralBuckling(p, stack, {1, 2}, le).has_value());
    ASSERT_FALSE(assignBuckling(p, stack, {2}, planeEdit(BucklingKind::Cantilever)).has_value());

    ASSERT_TRUE(project_json::isKnownSection("buckling"));
    const std::string text = project_json::dumpSection(p, "buckling");
    Project q = twoBars();
    project_json::loadSection(q, "buckling", text);
    EXPECT_EQ(q.buckling, p.buckling);
    EXPECT_TRUE(project_json::validate(q).empty());

    Project old = twoBars();
    project_json::loadSection(old, "buckling", "{}");
    EXPECT_TRUE(old.buckling.empty());
    // the radio is persisted as typed (measured: a typed 0.7 stays "Otros valores")
    project_json::loadSection(old, "buckling", R"({"bars":[{"bar":1,"xy":{"kind":5,"beta":0.7,"reviewed":true}}]})");
    EXPECT_EQ(bucklingOf(old.buckling, 1).xy.kind, BucklingKind::UserBeta);
    project_json::loadSection(old, "buckling", R"({"bars":[{"bar":1,"xy":{"kind":0,"beta":0.0,"reviewed":true}}]})");
    EXPECT_EQ(bucklingOf(old.buckling, 1).xy.kind, BucklingKind::None);
    EXPECT_EQ(bucklingOf(old.buckling, 1).xz, BucklingPlane{});
}

TEST(Buckling, ValidateRejectsCorruptData) {
    Project p = twoBars();
    p.buckling.bars[1] = BarBuckling{};
    EXPECT_FALSE(validateBuckling(p.buckling, p.structure).empty());  // default record stored
    BarBuckling b;
    b.xy.beta = 101.0;
    p.buckling.bars[1] = b;
    EXPECT_FALSE(validateBuckling(p.buckling, p.structure).empty());
    b.xy.beta = 2.0;
    b.top.momentCoeff = -1.0;
    p.buckling.bars[1] = b;
    EXPECT_FALSE(validateBuckling(p.buckling, p.structure).empty());
    b.top.momentCoeff = 1.0;
    p.buckling.bars[1] = b;
    EXPECT_TRUE(validateBuckling(p.buckling, p.structure).empty());
}
