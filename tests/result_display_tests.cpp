// T-P8-06 (GUI part, Qt-free helpers): texts of the result balloons, result sources of the panels, deformed shape and law sampling for the
// drawing, and the undoable write of the non-linear combinations. References: docs/research/resultados_calculo.md, docs/research/ui/resultados_paneles.md
// (balloon formats measured: s_lin.png, rp_04_react_node.png, rp_11_my_query.png) and the C1 measurement (solver_medidas_original.md).
#include <gtest/gtest.h>

#include <cmath>

#include "core/fem/project_analysis.h"
#include "core/fem/result_display.h"
#include "core/fem/results.h"
#include "core/nonlinear_combinations_edit.h"
#include "core/project.h"
#include "core/undo_stack.h"

using namespace cc::fem;
namespace core = cc::core;

namespace {

// C1 [V]: cantilever L = 4 m along X (N1 clamped), P = 10 kN along -Z at the tip, self weight 77.01 kN/m3.
core::Project c1(int& n1, int& n2, int& bar) {
    core::Project p;
    core::Material mat;
    mat.name = "ref";
    mat.kind = core::MaterialKind::Steel;
    mat.elasticModulusMPa = 210000.0;
    mat.poissonRatio = 0.3;
    mat.densityKnM3 = 77.01;
    mat.thermalExpansion = 1.2e-5;
    const int m = p.structure.addMaterial(mat);
    core::CrossSection cs;
    cs.name = "ref";
    cs.areaM2 = 0.01;
    cs.inertiaYM4 = 1e-4;
    cs.inertiaZM4 = 5e-5;
    cs.torsionM4 = 8e-5;
    cs.shearAreaYM2 = 0.004;
    cs.shearAreaZM2 = 0.004;
    const int s = p.structure.addSection(cs);
    n1 = p.structure.addNode({0, 0, 0});
    n2 = p.structure.addNode({4, 0, 0});
    bar = p.structure.addBar(n1, n2, m, s);
    core::ExternalSupport sup;
    sup.type = core::ExternalSupportType::Fixed;
    p.nodeSupports.external[n1] = sup;
    core::NodalLoad nl;
    nl.id = p.nodalLoads.nextId++;
    nl.nodeId = n2;
    nl.loadCaseId = 0;
    nl.magnitude = 10.0;
    nl.direction = core::LoadDirection::MinusZ;
    p.nodalLoads.items.push_back(nl);
    return p;
}

}  // namespace

TEST(ResultDisplay, FixedTextNeverShowsMinusZero) {
    EXPECT_EQ(formatFixed(-0.0004, 3), "0.000");
    EXPECT_EQ(formatFixed(-0.0, 2), "0.00");
    EXPECT_EQ(formatFixed(-0.339, 3), "-0.339");
    EXPECT_EQ(formatFixed(44.976, 2), "44.98");
    EXPECT_EQ(formatFixed(6666.6667, 3), "6666.667");
}

TEST(ResultDisplay, BalloonLinesFollowTheOriginal) {
    // s_lin.png: «Gy: -0.339 mRad», «Dx: 0.000 mm».
    const auto d = displacementLines({0, 0, -0.0, 0, -0.000339, 0});
    ASSERT_EQ(d.size(), 6u);
    EXPECT_EQ(d[0].label, "Dx:");
    EXPECT_EQ(d[0].value, " 0.000 mm");
    EXPECT_EQ(d[2].value, " 0.000 mm");
    EXPECT_EQ(d[4].label, "Gy:");
    EXPECT_EQ(d[4].value, " -0.339 mRad");
    // rp_04_react_node.png: forces with three decimals, moments with two.
    const auto r = reactionLines({-10, -10, 10, 0, -40, -40});
    ASSERT_EQ(r.size(), 6u);
    EXPECT_EQ(r[0].label + r[0].value, "Rx: -10.000 kN");
    EXPECT_EQ(r[3].label + r[3].value, "Mx: 0.00 kN·m");
    EXPECT_EQ(r[5].label + r[5].value, "Mz: -40.00 kN·m");
    // Unchecked components are left out.
    ComponentMask onlyMy{};
    onlyMy[4] = true;
    const auto f = forceLines(1.999, {0, 0, 0, 0, -20.01, 0}, onlyMy);
    ASSERT_EQ(f.size(), 2u);
    EXPECT_EQ(f[0].label + f[0].value, "X: 1.999 m");
    EXPECT_EQ(f[1].label + f[1].value, "My: -20.01 kN·m");
}

TEST(ResultDisplay, SourcesAndQueriesOfACalculatedWork) {
    int n1 = 0, n2 = 0, bar = 0;
    const core::Project p = c1(n1, n2, bar);
    const ProjectAnalysis a = analyseProject(p);
    ASSERT_TRUE(a.calculated);
    const auto families = availableFamilies(a);
    ASSERT_EQ(families.size(), 1u);  // no springs with «Sentido»: no non-linear combinations
    EXPECT_EQ(resultFamilyName(families[0]), "Hipótesis simple");
    const auto names = resultItemNames(a, ResultFamily::LoadCases);
    ASSERT_EQ(names.size(), 1u);
    EXPECT_EQ(names[0], "Peso propio");  // s_lin.png
    EXPECT_FALSE(resultOf(a, ResultFamily::LoadCases, 1));
    EXPECT_FALSE(resultOf(a, ResultFamily::Nonlinear, 0));
    const auto ref = resultOf(a, ResultFamily::LoadCases, 0);
    ASSERT_TRUE(ref);
    const auto b = barIndexOf(a.build.model, bar);
    ASSERT_TRUE(b);
    EXPECT_FALSE(barIndexOf(a.build.model, bar + 100));
    // Tip of the bar = node N2 [V C1: Dz = -11.475 mm].
    EXPECT_NEAR(ref->result->barDisplacements[*b].end[2] * 1000.0, -11.475, 0.002);
    const auto shape = deformedBar(a.build.model, *ref, *b, 100.0, 10);
    ASSERT_EQ(shape.size(), 11u);
    EXPECT_NEAR(shape.back()[0], 4.0 + 100.0 * ref->result->barDisplacements[*b].end[0], 1e-9);
    EXPECT_NEAR(shape.back()[2], 100.0 * ref->result->barDisplacements[*b].end[2], 1e-9);
    EXPECT_NEAR(shape.front()[2], 0.0, 1e-12);
    EXPECT_NEAR(maxTranslation(a.build.model, *ref), 0.011475, 2e-6);
    // Largest |My| on the bar = clamped end [V C1: -46.16 kN·m].
    EXPECT_NEAR(maxLawValue(a.build.model, *ref, {*b}, 4), 46.16, 0.01);
    // Bar along X: Vy and Mz are drawn along local y, the others along local z.
    EXPECT_EQ(lawDirection(a.build.model, *b, 5), a.build.model.bars[*b].ey);
    EXPECT_EQ(lawDirection(a.build.model, *b, 4), a.build.model.bars[*b].ez);
    const Vec3 mid = barPointAt(a.build.model, *b, 2.0);
    EXPECT_NEAR(mid[0], 2.0, 1e-12);
    EXPECT_NEAR(modelDiagonal(a.build.model), 4.0, 1e-12);
}

TEST(ResultDisplay, ScalesOfThePanel) {
    // 0.100 -> about 0.01 m per kN·m [≈V rp_10]; deformed shape scale 100 multiplies the displacement.
    EXPECT_NEAR(lawDrawLength(40.0, 0.1), 0.4, 1e-12);
    EXPECT_DOUBLE_EQ(automaticScale(0.0, 10.0), 0.0);
    EXPECT_NEAR(automaticScale(2.0, 10.0), 0.5, 1e-12);
}

TEST(NonlinearCombinationsEdit, WriteIsOneUndoableStep) {
    core::Project p;
    core::UndoStack stack;
    core::NonlinearCombinations after;
    core::NonlinearCombination c;
    c.reference = "PP";
    c.factors[0] = 1.0;
    after.items.push_back(c);
    EXPECT_FALSE(core::setNonlinearCombinations(p, stack, p.nonlinearCombinations));  // nothing to change
    EXPECT_TRUE(core::setNonlinearCombinations(p, stack, after));
    EXPECT_EQ(p.nonlinearCombinations, after);
    EXPECT_EQ(stack.count(), 1u);
    stack.undo();
    EXPECT_TRUE(p.nonlinearCombinations.items.empty());
    stack.redo();
    EXPECT_EQ(p.nonlinearCombinations.items.size(), 1u);
    EXPECT_TRUE(core::validateNonlinearCombinations(after).empty());
    after.items[0].reference = "  ";
    EXPECT_FALSE(core::validateNonlinearCombinations(after).empty());
    after.items[0].reference = "PP";
    after.items[0].loadSteps = 0;
    EXPECT_FALSE(core::validateNonlinearCombinations(after).empty());
}
