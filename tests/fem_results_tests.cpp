// T-P8-06: results layer (displacements, reactions, section forces and deformed shape along bars, combinations by superposition), non-linear
// combinations of the project and the calculation flow with its final report. Reference values MEASURED in the original:
// docs/research/solver_medidas_original.md (C1, C2) and docs/research/solver_diafragmas_ligaduras_apoyos.md (sections 10.1 and 12.5); the
// report texts from docs/screenshots/s_5.png, rr_e2.png and the string table of ed3dlib.dll (docs/research/resultados_calculo.md).
#include <gtest/gtest.h>

#include <algorithm>
#include <cstdio>
#include <cmath>
#include <vector>

#include "core/fem/frame_builder.h"
#include "core/fem/linear_solver.h"
#include "core/fem/project_analysis.h"
#include "core/fem/results.h"
#include "core/fem/shell_analysis.h"
#include "core/load_case_edit.h"
#include "core/project.h"
#include "core/project_json.h"
#include "core/shell_load.h"
#include "core/shell_properties.h"
#include "core/undo_stack.h"

using namespace cc::fem;
namespace core = cc::core;

namespace {

void expectRead(double actual, double expected, double resolution, const char* what, double rel = 1e-3) {
    EXPECT_NEAR(actual, expected, rel * std::fabs(expected) + resolution) << what;
}

struct Fixture {
    core::Project project;
    int material = 0, section = 0;
};

// Generic section of the measured works: A = 100 cm2, Avy = Avz = 40 cm2, Iyy = 1e4, Izz = 5e3, It = 8e3 cm4, E = 210000 MPa, nu = 0.3.
Fixture makeFixture(double gamma = 0.01) {
    Fixture f;
    core::Material mat;
    mat.name = "ref";
    mat.kind = core::MaterialKind::Steel;
    mat.elasticModulusMPa = 210000.0;
    mat.poissonRatio = 0.3;
    mat.densityKnM3 = gamma;
    mat.thermalExpansion = 1.2e-5;
    f.material = f.project.structure.addMaterial(mat);
    core::CrossSection cs;
    cs.name = "ref";
    cs.areaM2 = 0.01;
    cs.inertiaYM4 = 1e-4;
    cs.inertiaZM4 = 5e-5;
    cs.torsionM4 = 8e-5;
    cs.shearAreaYM2 = 0.004;
    cs.shearAreaZM2 = 0.004;
    f.section = f.project.structure.addSection(cs);
    return f;
}

int addNode(Fixture& f, double x, double y, double z) { return f.project.structure.addNode({x, y, z}); }
int addBar(Fixture& f, int a, int b) { return f.project.structure.addBar(a, b, f.material, f.section); }
void support(Fixture& f, int node, core::ExternalSupportType type) {
    core::ExternalSupport s;
    s.type = type;
    f.project.nodeSupports.external[node] = s;
}
void nodalLoad(Fixture& f, int node, core::LoadDirection dir, double kn, int caseId = 0) {
    core::NodalLoad nl;
    nl.id = f.project.nodalLoads.nextId++;
    nl.nodeId = node;
    nl.loadCaseId = caseId;
    nl.magnitude = kn;
    nl.direction = dir;
    f.project.nodalLoads.items.push_back(nl);
}
void uniformLoad(Fixture& f, int bar, double q, int caseId = 0) {
    core::BarLoad bl;
    bl.id = f.project.barLoads.nextId++;
    bl.barId = bar;
    bl.loadCaseId = caseId;
    bl.type = core::BarLoadType::Uniform;
    bl.value1 = q;
    bl.direction = core::LoadDirection::MinusZ;
    f.project.barLoads.items.push_back(bl);
}
int addCase(core::Project& p, core::LoadNature nature, const std::string& ref) {
    core::LoadCase c;
    c.id = p.loadCases.nextId++;
    c.nature = nature;
    c.reference = ref;
    p.loadCases.cases.push_back(c);
    ++p.loadCases.created[static_cast<std::size_t>(nature)];
    return c.id;
}

std::size_t barIndex(const FrameModel& m, int id) {
    for (std::size_t i = 0; i < m.bars.size(); ++i)
        if (m.bars[i].id == id) return i;
    ADD_FAILURE() << "bar " << id;
    return 0;
}

bool reportHas(const ProjectAnalysis& a, ReportLevel level, const std::string& text) {
    return std::any_of(a.report.begin(), a.report.end(), [&](const ReportEntry& e) { return e.level == level && e.text == text; });
}

ProjectAnalysis analyse(const core::Project& p) {
    ProjectAnalysis a = analyseProject(p);
    EXPECT_TRUE(a.calculated) << (a.report.empty() ? "" : a.report.front().text);
    return a;
}

// C1 [V]: cantilever L = 4 m along X (N1 clamped), P = 10 kN along -Z at the tip, self weight 77.01 kN/m3 (q = 0.7701 kN/m).
Fixture c1(int& bar) {
    Fixture f = makeFixture(77.01);
    const int n1 = addNode(f, 0, 0, 0), n2 = addNode(f, 4, 0, 0);
    bar = addBar(f, n1, n2);
    support(f, n1, core::ExternalSupportType::Fixed);
    nodalLoad(f, n2, core::LoadDirection::MinusZ, 10.0);
    return f;
}

// C2 [V] as ONE bar of 6 m: N1 «Apoyo fijo», N3 «recta X articulada» (Dy, Dz blocked), q = 10 kN/m global -Z.
Fixture c2Single(int& bar, int& n3) {
    Fixture f = makeFixture();
    const int n1 = addNode(f, 0, 0, 0);
    n3 = addNode(f, 6, 0, 0);
    bar = addBar(f, n1, n3);
    support(f, n1, core::ExternalSupportType::Pinned);
    core::ExternalSupport roller;
    roller.type = core::ExternalSupportType::Generic;
    roller.generic.translations = {core::DofMode::Free, core::DofMode::Fixed, core::DofMode::Fixed};
    f.project.nodeSupports.external[n3] = roller;
    uniformLoad(f, bar, 10.0);
    return f;
}

constexpr double kEI = 210e6 * 1e-4, kGAv = 210e6 / 2.6 * 0.004;

}  // namespace

// --- deformed shape: the exact field of the element, not a cubic ---
TEST(FemResults, DeformedShapeOfSimplySupportedBeamIsExact) {
    int bar = 0, n3 = 0;
    const Fixture f = c2Single(bar, n3);
    const ProjectAnalysis a = analyse(f.project);
    const FrameModel& m = a.build.model;
    const std::size_t b = barIndex(m, bar);
    const LoadCaseResult& r = a.linear.cases.at(0);
    const LoadCaseInput& in = a.build.cases.at(0);
    const double q = 10.0 + 1e-4, l = 6.0;
    const Dof6 mid = barDisplacementAt(m, r, in, b, 3.0);
    // 5 q L^4 / 384 EI + q L^2 / 8 G Av = 8.0357 + 0.1393 = 8.175 mm [V C2: -8.175 mm]
    EXPECT_NEAR(mid[2] * 1e3, -(5.0 * q * l * l * l * l / (384.0 * kEI) + q * l * l / (8.0 * kGAv)) * 1e3, 1e-6);
    expectRead(mid[2] * 1e3, -8.175, 1e-3, "midspan Dz mm [V]");
    EXPECT_NEAR(mid[4], 0.0, 1e-12);
    // Ends: Gy(N1) = q L^3 / 24 EI = 4.286 mRad [V]; closure at x = L on the displacement of the other end.
    expectRead(barDisplacementAt(m, r, in, b, 0.0)[4] * 1e3, 4.286, 1e-3, "Gy N1");
    const Dof6 end = barDisplacementAt(m, r, in, b, l);
    for (int k = 0; k < 6; ++k) EXPECT_NEAR(end[k], r.barDisplacements[b].end[k], 1e-12) << k;
    // A quarter point (x = 1.5): q x (L^3 - 2 L x^2 + x^3) / 24 EI + q x (L - x) / 2 G Av.
    const double x = 1.5;
    EXPECT_NEAR(barDisplacementAt(m, r, in, b, x)[2], -(q * x * (l * l * l - 2 * l * x * x + x * x * x) / (24 * kEI) + q * x * (l - x) / (2 * kGAv)), 1e-12);
}

TEST(FemResults, DeformedShapeOfCantileverWithSelfWeight) {
    int bar = 0;
    const Fixture f = c1(bar);
    const ProjectAnalysis a = analyse(f.project);
    const FrameModel& m = a.build.model;
    const std::size_t b = barIndex(m, bar);
    const LoadCaseResult& r = a.linear.cases.at(0);
    const LoadCaseInput& in = a.build.cases.at(0);
    const double p = 10.0, q = 0.7701, l = 4.0;
    expectRead(barDisplacementAt(m, r, in, b, l)[2] * 1e3, -11.475, 1e-3, "tip Dz [V C1]");
    expectRead(barDisplacementAt(m, r, in, b, l)[4] * 1e3, 4.201, 1e-3, "tip Gy [V C1]");
    for (double x : {1.0, 2.0, 3.3}) {
        const double w = p * x * x * (3 * l - x) / (6 * kEI) + q * x * x * (6 * l * l - 4 * l * x + x * x) / (24 * kEI) + (p * x + q * (l * x - x * x / 2)) / kGAv;
        EXPECT_NEAR(barDisplacementAt(m, r, in, b, x)[2], -w, 1e-12) << x;
    }
}

TEST(FemResults, DeformedShapeHonoursReleasedEnds) {
    // Both nodes clamped, both bar ends «articulado» (Gy, Gz free): a simply supported beam; the ends of the BAR rotate, the nodes do not.
    Fixture f = makeFixture();
    const int n1 = addNode(f, 0, 0, 0), n2 = addNode(f, 6, 0, 0);
    const int bar = addBar(f, n1, n2);
    support(f, n1, core::ExternalSupportType::Fixed);
    support(f, n2, core::ExternalSupportType::Fixed);
    core::BarEndsRecord ends;
    ends.ends[0].pinned = ends.ends[1].pinned = true;
    f.project.barEnds.bars[bar] = ends;
    uniformLoad(f, bar, 10.0);
    const ProjectAnalysis a = analyse(f.project);
    const std::size_t b = barIndex(a.build.model, bar);
    const LoadCaseResult& r = a.linear.cases.at(0);
    const double q = 10.0 + 1e-4, l = 6.0;
    EXPECT_NEAR(barDisplacementAt(a.build.model, r, a.build.cases[0], b, 3.0)[2],
                -(5.0 * q * l * l * l * l / (384.0 * kEI) + q * l * l / (8.0 * kGAv)), 1e-9);
    EXPECT_NEAR(barDisplacementAt(a.build.model, r, a.build.cases[0], b, 0.0)[4], q * l * l * l / (24 * kEI), 1e-9);
    EXPECT_NEAR(nodeDisplacement(a.build.model, r, n1).value()[4], 0.0, 1e-15);
}

TEST(FemResults, DeformedShapeWithTemperature) {
    // Model level: cantilever 4 m with depthZ = 50 mm, T1 = 20 on -z, T2 = 0 on +z (gradientZ = T(+z) - T(-z) = -20) and the mean 10 as uniform
    // [V 4: tip +38.400 mm, Dx +0.480 mm]; the curvature is constant, so w(x) = kappa x^2 / 2.
    FrameModel m;
    m.nodes = {FemNode{1, {0, 0, 0}, {true, true, true, true, true, true}}, FemNode{2, {4, 0, 0}}};
    FemBar bar;
    bar.id = 1;
    bar.nodeI = 1;
    bar.nodeJ = 2;
    bar.e = 210e6;
    bar.g = 210e6 / 2.6;
    bar.alpha = 1.2e-5;
    bar.section = {0.01, 1e-4, 5e-5, 8e-5, 0.004, 0.004, 0.1, 0.05};
    m.bars = {bar};
    LoadCaseInput lc;
    FemBarLoad t;
    t.bar = 1;
    t.kind = BarLoadKind::Temperature;
    t.uniformTemperature = 10.0;
    t.gradientZ = -20.0;
    lc.bars = {t};
    const LinearResult r = solveLinear(m, std::span<const LoadCaseInput>(&lc, 1));
    ASSERT_EQ(r.status, SolveStatus::Ok) << r.message;
    const Dof6 tip = barDisplacementAt(m, r.cases[0], lc, 0, 4.0);
    expectRead(tip[2] * 1e3, 38.400, 1e-3, "tip Dz [V]");
    expectRead(tip[0] * 1e3, 0.480, 1e-3, "tip Dx [V]");
    EXPECT_NEAR(barDisplacementAt(m, r.cases[0], lc, 0, 2.0)[2], tip[2] / 4.0, 1e-12);
    EXPECT_NEAR(barDisplacementAt(m, r.cases[0], lc, 0, 2.0)[0], tip[0] / 2.0, 1e-12);
}

// --- section forces along the bar (TK-107 convention) ---
TEST(FemResults, SectionForcesMatchTheMeasuredTooltips) {
    {
        int bar = 0;
        const Fixture f = c1(bar);
        const ProjectAnalysis a = analyse(f.project);
        const Dof6 s = barForcesAt(a.build.model, a.linear.cases[0], a.build.cases[0], barIndex(a.build.model, bar), 1.9976);
        expectRead(s[2], -11.542, 2e-3, "C1 Vz [V]");
        expectRead(s[4], -21.57, 0.01, "C1 My [V]");
        const Dof6 clamp = barForcesAt(a.build.model, a.linear.cases[0], a.build.cases[0], barIndex(a.build.model, bar), 0.0);
        expectRead(clamp[4], -46.16, 0.01, "C1 My at the clamp = reaction My [V]");
    }
    {
        int bar = 0, n3 = 0;
        const Fixture f = c2Single(bar, n3);
        const ProjectAnalysis a = analyse(f.project);
        const Dof6 s = barForcesAt(a.build.model, a.linear.cases[0], a.build.cases[0], barIndex(a.build.model, bar), 2.934);
        expectRead(s[2], -0.658, 3e-3, "C2 Vz [V]", 0.0);
        // Measured 44.93: the tooltip of the original interpolates sampled points of the law (exact 44.976, solver_medidas_original.md 3.2).
        EXPECT_NEAR(s[4], 30.0003 * 2.934 - 0.5 * 10.0001 * 2.934 * 2.934, 1e-6);
        EXPECT_NEAR(s[4], 44.93, 0.05);
    }
}

TEST(FemResults, BarLawSamplesEndsLoadsAndJumps) {
    Fixture f = makeFixture();
    const int n1 = addNode(f, 0, 0, 0), n2 = addNode(f, 4, 0, 0);
    const int bar = addBar(f, n1, n2);
    support(f, n1, core::ExternalSupportType::Fixed);
    core::BarLoad bl;
    bl.id = f.project.barLoads.nextId++;
    bl.barId = bar;
    bl.type = core::BarLoadType::Point;
    bl.value1 = 10.0;
    bl.relative = false;
    bl.start = 1.3;
    bl.direction = core::LoadDirection::MinusZ;
    f.project.barLoads.items.push_back(bl);
    const ProjectAnalysis a = analyse(f.project);
    const std::vector<BarLawPoint> law = barLaw(a.build.model, a.linear.cases[0], a.build.cases[0], 0, 4);
    ASSERT_GE(law.size(), 7u);  // 0, 1, 2, 3, 4 and the two sides of 1.3
    EXPECT_DOUBLE_EQ(law.front().x, 0.0);
    EXPECT_DOUBLE_EQ(law.back().x, 4.0);
    const auto before = std::find_if(law.begin(), law.end(), [](const BarLawPoint& p) { return p.x > 1.29 && p.x < 1.3; });
    const auto after = std::find_if(law.begin(), law.end(), [](const BarLawPoint& p) { return p.x > 1.3 && p.x < 1.31; });
    ASSERT_NE(before, law.end());
    ASSERT_NE(after, law.end());
    EXPECT_NEAR(before->forces[2] - after->forces[2], -10.0, 1e-3);  // the jump of Vz
    EXPECT_NEAR(after->forces[2], 0.0, 1e-3);
}

// --- combinations by superposition ---
TEST(FemResults, SuperpositionEqualsTheCombinedCase) {
    int bar = 0;
    Fixture f = c1(bar);
    const int cm = addCase(f.project, core::LoadNature::DeadLoad, "CM 1");
    uniformLoad(f, bar, 3.0, cm);
    nodalLoad(f, f.project.structure.nodes.back().id, core::LoadDirection::PlusY, 4.0, cm);
    const ProjectAnalysis a = analyse(f.project);
    core::Combination c;
    c.coefficients = {1.35, 1.5};
    c.name = "1.35·PP+1.5·CM1";
    const CombinationResults cr = combinationResults(a, std::span<const core::Combination>(&c, 1));
    ASSERT_EQ(cr.result.cases.size(), 1u);
    const LinearResult direct = solveLinear(a.build.model, cr.inputs);
    const LoadCaseResult &s = cr.result.cases[0], &d = direct.cases[0];
    EXPECT_EQ(s.name, "1.35·PP+1.5·CM1");
    for (std::size_t n = 0; n < d.displacements.size(); ++n)
        for (int k = 0; k < 6; ++k) EXPECT_NEAR(s.displacements[n][k], d.displacements[n][k], 1e-12);
    for (const auto& [id, rd] : d.reactions)
        for (int k = 0; k < 6; ++k) EXPECT_NEAR(s.reactions.at(id)[k], rd[k], 1e-9);
    for (double x : {0.0, 1.1, 2.5, 4.0}) {
        const Dof6 fs = barForcesAt(a.build.model, s, cr.inputs[0], 0, x), fd = barForcesAt(a.build.model, d, cr.inputs[0], 0, x);
        const Dof6 us = barDisplacementAt(a.build.model, s, cr.inputs[0], 0, x), ud = barDisplacementAt(a.build.model, d, cr.inputs[0], 0, x);
        for (int k = 0; k < 6; ++k) {
            EXPECT_NEAR(fs[k], fd[k], 1e-9);
            EXPECT_NEAR(us[k], ud[k], 1e-12);
        }
    }
    // A failed case makes the combination unavailable.
    std::vector<LoadCaseResult> cases = a.linear.cases;
    cases[1].status = SolveStatus::Mechanism;
    cases[1].message = "m";
    const std::vector<double> coef{1.0, 1.0};
    EXPECT_EQ(superposeCases(cases, coef, 1, "x").status, SolveStatus::Mechanism);
    const std::vector<double> only{1.0, 0.0};
    EXPECT_EQ(superposeCases(cases, only, 1, "x").status, SolveStatus::Ok);
}

// --- bars split along a shell edge: forces and deformed shape per piece ---
TEST(FemResults, BarsAlongShellEdgesAreEvaluatedPerPiece) {
    // FemShellBars.BeamsAlongTheEdgesCarryAFlexibleSlab: 4 x 1 m slab spanning across, two simply supported edge beams (q = 5 kN/m each).
    core::Project p;
    core::StructuralModel& sm = p.structure;
    core::Material mat;
    mat.name = "s";
    mat.elasticModulusMPa = 210000.0;
    mat.poissonRatio = 0.3;
    mat.densityKnM3 = 0.01;
    const int m = sm.addMaterial(mat);
    core::CrossSection cs;
    cs.name = "s";
    cs.areaM2 = 0.01;
    cs.inertiaYM4 = 1e-4;
    cs.inertiaZM4 = 5e-5;
    cs.torsionM4 = 8e-5;
    cs.shearAreaYM2 = 100.0;
    cs.shearAreaZM2 = 100.0;
    const int sec = sm.addSection(cs);
    const std::vector<int> n = {sm.addNode({0, 0, 0}), sm.addNode({4, 0, 0}), sm.addNode({4, 1, 0}), sm.addNode({0, 1, 0})};
    const int b1 = sm.addBar(n[0], n[1], m, sec), b2 = sm.addBar(n[3], n[2], m, sec);
    const int sh = sm.addShell(n, 0.1);
    core::Shell& shell = sm.shells.back();
    shell.material.kind = core::ShellMaterialKind::GenericIsotropic;
    shell.material.useGeneralData = false;
    shell.material.isoElasticModulusMPa = 30000.0;
    shell.material.isoPoissonRatio = 0.2;
    shell.material.isoDensityKnM3 = 0.0001;
    core::ShellRecord rec;
    for (int k : {kFxx, kFxy, kFyy, kMxx, kMxy}) rec.stiffness.set(k, 0.0);
    core::setShellRecord(p.shellProps, sh, rec);
    p.shellLoads.selfWeightOff.insert(sh);
    p.barLoads.selfWeightOff = {b1, b2};
    core::ExternalSupport pin;
    pin.type = core::ExternalSupportType::Pinned;
    for (int k : n) p.nodeSupports.external[k] = pin;
    core::ShellLoad l;
    l.id = p.shellLoads.nextId++;
    l.shellId = sh;
    l.value1 = 10.0;
    l.axes = core::ShellLoadAxes::Global;
    l.sense = core::ShellLoadSense::MinusZ;
    p.shellLoads.items.push_back(l);
    const ProjectAnalysis a = analyse(p);
    const FrameModel& model = a.build.model;
    const LoadCaseResult& r = a.linear.cases.at(0);
    const std::size_t b = barIndex(model, b1);
    ASSERT_GT(r.barSegments.at(b).size(), 1u);
    // Continuity of the deformed shape at every piece boundary, and agreement with the mesh node there.
    for (std::size_t k = 0; k + 1 < r.barSegments[b].size(); ++k) {
        const double x = r.barSegments[b][k].x1;
        const Dof6 u = barDisplacementAt(model, r, a.build.cases[0], b, x);
        for (int q = 0; q < 6; ++q) EXPECT_NEAR(u[q], r.barSegments[b][k + 1].originDisplacement[q], 1e-12) << k << " " << q;
    }
    // The law samples both sides of every piece boundary: the shell force at the boundary node makes Vz jump.
    {
        const std::vector<BarLawPoint> law = barLaw(model, r, a.build.cases[0], b, 4);
        const double x = r.barSegments[b].front().x1;
        const auto at = std::find_if(law.begin(), law.end(), [&](const BarLawPoint& p) { return std::fabs(p.x - x) < 1e-12; });
        ASSERT_NE(at, law.end());
        ASSERT_NE(at + 1, law.end());
        EXPECT_NEAR((at + 1)->x - x, 1e-9, 1e-12);
        EXPECT_GT(std::fabs((at + 1)->forces[2] - at->forces[2]), 1e-3);
    }
    // Midspan moment of the beam close to q L^2 / 8 = 10 kN.m (the slab corner takes a little straight to the support), My > 0 (TK-107).
    const Dof6 mid = barForcesAt(model, r, a.build.cases[0], b, 2.0);
    EXPECT_GT(mid[4], 8.0);
    EXPECT_LT(mid[4], 10.5);
    // The deformed shape at midspan equals the mesh displacement of the edge node at (2, 0, 0).
    double best = 1e300;
    std::size_t node = 0;
    for (std::size_t i = 0; i < a.linear.mesh.positions.size(); ++i) {
        const auto& pos = a.linear.mesh.positions[i];
        const double d = std::hypot(pos[0] - 2.0, pos[1], pos[2]);
        if (d < best) { best = d; node = i; }
    }
    ASSERT_LT(best, 1e-9);
    EXPECT_NEAR(barDisplacementAt(model, r, a.build.cases[0], b, 2.0)[2], r.meshDisplacements[node][2], 1e-12);
    // Shell deformed shape: at a mesh node it returns the node displacement.
    const std::optional<Dof6> sd = shellDisplacementAt(model, a.linear, 0, model.shells.front().id, 2.0, 0.0);
    ASSERT_TRUE(sd.has_value());
    EXPECT_NEAR((*sd)[2], r.meshDisplacements[node][2], 1e-12);
    EXPECT_FALSE(shellDisplacementAt(model, a.linear, 0, model.shells.front().id, 5.0, 0.5).has_value());
}

// --- non-linear combinations of the project ---
namespace {
Fixture springCantilever(core::SupportSense sense, core::LoadDirection dir, int& n2) {
    Fixture f = makeFixture();
    const int n1 = addNode(f, 0, 0, 0);
    n2 = addNode(f, 4, 0, 0);
    addBar(f, n1, n2);
    support(f, n1, core::ExternalSupportType::Fixed);
    core::ExternalSupport g;
    g.type = core::ExternalSupportType::Generic;
    g.generic.translations = {core::DofMode::Fixed, core::DofMode::Fixed, core::DofMode::Elastic};
    g.elasticConstants[2] = 1000.0;
    g.senses[2] = sense;
    f.project.nodeSupports.external[n2] = g;
    nodalLoad(f, n2, dir, 10.0);
    return f;
}
core::NonlinearKinds strengthAndDisplacements() {
    core::NonlinearKinds k{};
    k[static_cast<std::size_t>(core::NonlinearCombinationKind::Strength)] = true;
    k[static_cast<std::size_t>(core::NonlinearCombinationKind::Displacements)] = true;
    return k;
}
void generateInto(core::Project& p) {
    const core::NonlinearGeneration g = core::generateNonlinearCombinations(p, strengthAndDisplacements());
    ASSERT_TRUE(g.error.empty());
    p.nonlinearCombinations.items = g.items;
    p.nonlinearCombinations.generateKinds = strengthAndDisplacements();
}
}  // namespace

TEST(FemNonlinearCombinations, GenerationOfTheMeasuredWork) {
    int n2 = 0;
    const Fixture f = springCantilever(core::SupportSense::Positive, core::LoadDirection::MinusZ, n2);
    const core::NonlinearGeneration g = core::generateNonlinearCombinations(f.project, strengthAndDisplacements());
    ASSERT_TRUE(g.error.empty()) << g.error;
    ASSERT_EQ(g.items.size(), 2u);  // [V s_10.png]: PP, 1.35·PP, 10 load steps
    EXPECT_EQ(g.items[0].reference, "PP");
    EXPECT_EQ(g.items[1].reference, "1.35·PP");
    EXPECT_EQ(g.items[0].loadSteps, 10);
    EXPECT_TRUE(g.items[0].acts);
    EXPECT_DOUBLE_EQ(g.items[0].factors.at(0), 1.0);
    EXPECT_DOUBLE_EQ(g.items[1].factors.at(0), 1.35);
    EXPECT_TRUE(g.items[0].kinds[0] && g.items[0].kinds[3]);  // PP: Resistencia and Desplazamientos
    EXPECT_TRUE(g.items[1].kinds[0] && !g.items[1].kinds[3]);
    EXPECT_TRUE(g.unsupported.empty());
    core::NonlinearCombinations set;
    set.items = g.items;
    EXPECT_EQ(core::nonlinearCombinationLabel(set, 1), "[2] 1.35·PP");
    core::NonlinearKinds cracking{};
    cracking[1] = true;
    const core::NonlinearGeneration c = core::generateNonlinearCombinations(f.project, cracking);
    EXPECT_TRUE(c.items.empty());
    ASSERT_EQ(c.unsupported.size(), 1u);
    EXPECT_EQ(core::nonlinearCombinationKindName(c.unsupported[0]), "Fisuración");
    EXPECT_EQ(core::generatedCombinationsMessage(2), "Se han generado '2' combinaciones.");
}

TEST(FemNonlinearCombinations, WithoutCombinationsTheReportSaysSo) {
    int n2 = 0;
    const Fixture f = springCantilever(core::SupportSense::Positive, core::LoadDirection::MinusZ, n2);
    const ProjectAnalysis a = analyse(f.project);
    EXPECT_TRUE(a.hasNonlinearElements);
    EXPECT_FALSE(a.nonlinearCalculated);
    // [V s_5.png]: red line + warning line.
    EXPECT_TRUE(reportHas(a, ReportLevel::Error, "No se ha realizado el análisis de tensión/deformación no lineal"));
    EXPECT_TRUE(reportHas(a, ReportLevel::Warning, "La obra ha sido calculada sin realizar la comprobación de las barras."));
    // The linear analysis ignores the «Sentido» [V 10.1].
    expectRead(nodeDisplacement(a.build.model, a.linear.cases[0], n2).value()[2] * 1e3, -5.070, 1e-3, "linear Dz");
}

TEST(FemNonlinearCombinations, SolvedCombinationsMatchTheOriginal) {
    int n2 = 0;
    Fixture f = springCantilever(core::SupportSense::Positive, core::LoadDirection::MinusZ, n2);
    generateInto(f.project);
    const ProjectAnalysis a = analyse(f.project);
    ASSERT_TRUE(a.nonlinearCalculated);
    ASSERT_EQ(a.nonlinear.cases.size(), 2u);
    EXPECT_EQ(a.nonlinear.cases[0].name, "[1] PP");
    EXPECT_EQ(a.nonlinear.cases[1].name, "[2] 1.35·PP");
    expectRead(nodeDisplacement(a.build.model, a.nonlinear.cases[0], n2).value()[2] * 1e3, -10.283, 1e-3, "[1] PP spring released [V]");
    expectRead(nodeDisplacement(a.build.model, a.nonlinear.cases[1], n2).value()[2] * 1e3, -13.882, 1e-3, "[2] 1.35·PP [V]");
    expectRead(nodeDisplacement(a.build.model, a.linear.cases[0], n2).value()[2] * 1e3, -5.070, 1e-3, "linear [V]");
    // Bar queries on a non-linear combination use its combined loads.
    const Dof6 tip = barDisplacementAt(a.build.model, a.nonlinear.cases[1], a.nonlinearInputs[1], 0, 4.0);
    EXPECT_NEAR(tip[2], a.nonlinear.cases[1].displacements[1][2], 1e-12);
    EXPECT_FALSE(reportHas(a, ReportLevel::Error, core::nonlinearAnalysisNotDoneMessage()));
    EXPECT_FALSE(a.nonlinearCombinationsUpdated);  // regenerated identical
}

TEST(FemNonlinearCombinations, RotationalSpringWithSense) {
    // Section 12.5 [V]: cantilever N1 clamped, N2 Dx Dy Dz fixed, Gy «Elástico» K = 10000 kN.m/rad; self weight 770 kN/m3 (7.70 kN/m).
    for (const core::SupportSense sense : {core::SupportSense::Negative, core::SupportSense::Positive}) {
        Fixture f = makeFixture(770.0);
        const int n1 = addNode(f, 0, 0, 0), n2 = addNode(f, 4, 0, 0);
        addBar(f, n1, n2);
        support(f, n1, core::ExternalSupportType::Fixed);
        core::ExternalSupport g;
        g.type = core::ExternalSupportType::Generic;
        g.generic.translations = {core::DofMode::Fixed, core::DofMode::Fixed, core::DofMode::Fixed};
        g.generic.rotations = {core::DofMode::Free, core::DofMode::Elastic, core::DofMode::Free};
        g.elasticConstants[4] = 10000.0;
        g.senses[4] = sense;
        f.project.nodeSupports.external[n2] = g;
        generateInto(f.project);
        const ProjectAnalysis a = analyse(f.project);
        ASSERT_TRUE(a.nonlinearCalculated);
        expectRead(nodeDisplacement(a.build.model, a.linear.cases[0], n2).value()[4] * 1e3, -0.339, 1e-3, "linear Gy [V]", 3e-3);
        expectRead(nodeDisplacement(a.build.model, a.nonlinear.cases[0], n2).value()[4] * 1e3,
                   sense == core::SupportSense::Negative ? -0.339 : -0.507, 1e-3, "[1] PP Gy [V]", 3e-3);
    }
}

TEST(FemNonlinearCombinations, MechanismOfACombinationKeepsTheLinearResults) {
    Fixture f = makeFixture();
    const int n1 = addNode(f, 0, 0, 0), n2 = addNode(f, 4, 0, 0);
    addBar(f, n1, n2);
    core::ExternalSupport g;
    g.type = core::ExternalSupportType::Generic;
    g.generic.translations = {core::DofMode::Fixed, core::DofMode::Fixed, core::DofMode::Elastic};
    g.generic.rotations = {core::DofMode::Fixed, core::DofMode::Fixed, core::DofMode::Fixed};
    g.elasticConstants[2] = 1000.0;
    g.senses[2] = core::SupportSense::Positive;
    f.project.nodeSupports.external[n1] = g;
    nodalLoad(f, n2, core::LoadDirection::MinusZ, 10.0);
    generateInto(f.project);
    const ProjectAnalysis a = analyse(f.project);
    EXPECT_TRUE(reportHas(a, ReportLevel::Error,
                          "No es posible calcular el caso de carga '[1] PP'. La estructura es un mecanismo. Revise la definición de cargas y las "
                          "condiciones de vinculación del modelo."));  // [V rr_e2.png]
    EXPECT_EQ(a.nonlinear.cases[0].status, SolveStatus::Mechanism);
    EXPECT_TRUE(a.calculated);
}

TEST(FemNonlinearCombinations, CaseThatDoesNotActStopsTheNonlinearAnalysis) {
    int n2 = 0;
    Fixture f = springCantilever(core::SupportSense::Positive, core::LoadDirection::MinusZ, n2);
    const int q = addCase(f.project, core::LoadNature::DeadLoad, "CM 1");
    generateInto(f.project);
    f.project.nonlinearCombinations.updateAfterLinear = false;
    f.project.nonlinearCombinations.items[0].factors[q] = 1.0;
    f.project.loadCases.cases.back().acts = false;
    const ProjectAnalysis a = analyse(f.project);
    EXPECT_TRUE(reportHas(a, ReportLevel::Error, core::nonlinearCaseNotActingMessage()));
    EXPECT_FALSE(a.nonlinearCalculated);
    EXPECT_EQ(core::nonlinearCaseNotActingMessage(),
              "No es posible realizar el cálculo ya que hay combinaciones no lineales en las que intervienen hipótesis que no actúan. Elimine las "
              "combinaciones afectadas o borre las hipótesis que no actúan.");
}

TEST(FemNonlinearCombinations, DeletingALoadCaseDeletesItsCombinations) {
    core::Project p;
    const int cm = addCase(p, core::LoadNature::DeadLoad, "CM 1");
    core::NonlinearCombination a, b;
    a.reference = "PP";
    a.factors = {{0, 1.0}};
    b.reference = "PP+CM1";
    b.factors = {{0, 1.0}, {cm, 1.0}};
    p.nonlinearCombinations.items = {a, b};
    EXPECT_TRUE(core::nonlinearCombinationsUseCase(p.nonlinearCombinations, cm));
    EXPECT_EQ(core::loadCaseDependents(p, cm).nonlinearCombinations, std::vector<int>{1});
    core::UndoStack stack;
    ASSERT_EQ(core::deleteLoadCase(p, stack, cm), core::LoadCaseError::None);
    ASSERT_EQ(p.nonlinearCombinations.items.size(), 1u);
    EXPECT_EQ(p.nonlinearCombinations.items[0].reference, "PP");
    stack.undo();
    EXPECT_EQ(p.nonlinearCombinations.items.size(), 2u);
    EXPECT_EQ(core::nonlinearCombinationsOfCaseWillBeDeletedMessage(),
              "Se van a borrar las combinaciones no lineales y de pandeo en las que interviene la hipótesis.");
}

TEST(FemNonlinearCombinations, JsonRoundTrip) {
    core::Project p;
    const int cm = addCase(p, core::LoadNature::DeadLoad, "CM 1");
    core::NonlinearCombination c;
    c.reference = "1.35·PP+1.5·CM1";
    c.acts = false;
    c.loadSteps = 20;
    c.factors = {{0, 1.35}, {cm, 1.5}, {99, 2.0}};  // case 99 does not exist: not saved
    c.kinds[0] = true;
    p.nonlinearCombinations.items = {c};
    p.nonlinearCombinations.generateKinds[3] = true;
    p.nonlinearCombinations.updateAfterLinear = false;
    const std::string text = core::project_json::dumpSection(p, "nonlinear_combinations");
    core::Project q;
    q.loadCases = p.loadCases;
    core::project_json::loadSection(q, "nonlinear_combinations", text);
    c.factors.erase(99);
    ASSERT_EQ(q.nonlinearCombinations.items.size(), 1u);
    EXPECT_EQ(q.nonlinearCombinations.items[0], c);
    EXPECT_EQ(q.nonlinearCombinations.generateKinds, p.nonlinearCombinations.generateKinds);
    EXPECT_FALSE(q.nonlinearCombinations.updateAfterLinear);
}

// --- calculation flow ---
TEST(FemProjectAnalysis, EmptyWorkAndMechanism) {
    {
        const core::Project p;
        const ProjectAnalysis a = analyseProject(p);
        EXPECT_FALSE(a.calculated);
        EXPECT_TRUE(reportHas(a, ReportLevel::Error, "La obra está vacía. Para poder calcular debe introducir, al menos, una barra o una lámina."));
    }
    {
        // Section 6 [V]: base «Apoyo fijo» only and a tip load: mechanism, results disabled.
        Fixture f = makeFixture();
        const int n1 = addNode(f, 0, 0, 0), n2 = addNode(f, 4, 0, 0);
        addBar(f, n1, n2);
        support(f, n1, core::ExternalSupportType::Pinned);
        nodalLoad(f, n2, core::LoadDirection::MinusZ, 10.0);
        const ProjectAnalysis a = analyseProject(f.project);
        EXPECT_FALSE(a.calculated);
        EXPECT_TRUE(reportHas(a, ReportLevel::Error, "Esta estructura es un mecanismo. Revise los datos introducidos."));
        EXPECT_FALSE(reportHas(a, ReportLevel::Warning, calculatedWithoutChecksMessage()));
    }
}
