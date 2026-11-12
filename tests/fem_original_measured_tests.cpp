// Cross-validation of the linear frame solver (project adapter included) against values MEASURED in the original CYPE 3D (T-P8-05):
// docs/research/solver_medidas_original.md (cases C1, C2, C3, C6, inclined bar with a global load, partial fixity, temperature, mechanism
// warning, sign convention). The original shows displacements to 0.001 mm / 0.001 mRad, forces to 0.001 kN and moments to 0.01 kN.m, so the
// tolerance is 1e-3 relative plus the reading resolution.
//
// Common properties: A = 100 cm2, Iyy = 10000 cm4, Izz = 5000 cm4, It = 8000 cm4, E = 210000 MPa, nu = 0.3, Avz = 40 cm2, Avy = 40 cm2
// (80 cm2 in C6), alpha = 1.2e-5, gamma = 77.01 kN/m3 (C1) or 0.01 kN/m3 (the minimum of the original, C2..C6).
#include <gtest/gtest.h>

#include <cmath>
#include <vector>

#include "core/bar_ends.h"
#include "core/bar_description.h"
#include "core/fem/frame_builder.h"
#include "core/fem/linear_solver.h"
#include "core/project.h"
#include "core/section_properties.h"

using namespace cc::fem;
namespace core = cc::core;

namespace {

/// |actual - expected| <= 1e-3 |expected| + resolution (the last digit shown by the original).
void expectRead(double actual, double expected, double resolution, const char* what, double rel = 1e-3) {
    EXPECT_NEAR(actual, expected, rel * std::fabs(expected) + resolution) << what;
}

struct Fixture {
    core::Project project;
    int material = 0, section = 0;
};

Fixture makeFixture(double gamma, double avy = 0.004, double avz = 0.004) {
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
    cs.shearAreaYM2 = avy;
    cs.shearAreaZM2 = avz;
    f.section = f.project.structure.addSection(cs);
    return f;
}

int addNode(Fixture& f, double x, double y, double z) { return f.project.structure.addNode({x, y, z}); }
int addBar(Fixture& f, int a, int b) { return f.project.structure.addBar(a, b, f.material, f.section); }
void clamp(Fixture& f, int node) {
    core::ExternalSupport s;
    s.type = core::ExternalSupportType::Fixed;
    f.project.nodeSupports.external[node] = s;
}
void nodalLoad(Fixture& f, int node, core::LoadDirection dir, double kn) {
    core::NodalLoad nl;
    nl.id = f.project.nodalLoads.nextId++;
    nl.nodeId = node;
    nl.loadCaseId = 0;
    nl.magnitude = kn;
    nl.direction = dir;
    f.project.nodalLoads.items.push_back(nl);
}
core::BarLoad& barLoad(Fixture& f, int bar, core::BarLoadType type, double v1, double v2 = 0.0) {
    core::BarLoad bl;
    bl.id = f.project.barLoads.nextId++;
    bl.barId = bar;
    bl.loadCaseId = 0;
    bl.type = type;
    bl.value1 = v1;
    bl.value2 = v2;
    f.project.barLoads.items.push_back(bl);
    return f.project.barLoads.items.back();
}

struct Solved {
    FrameBuild build;
    LinearResult result;
    const LoadCaseResult& c() const { return result.cases.at(0); }
    /// Displacement DOF k of node `id` (m / rad).
    double d(int id, int k) const {
        for (std::size_t i = 0; i < build.model.nodes.size(); ++i)
            if (build.model.nodes[i].id == id) return c().displacements[i][k];
        ADD_FAILURE() << "node " << id;
        return 0.0;
    }
    double r(int id, int k) const { return c().reactions.at(id)[k]; }
};

Solved solve(const core::Project& p) {
    Solved s;
    s.build = buildFrameModel(p);
    EXPECT_TRUE(s.build.errors.empty());
    EXPECT_TRUE(s.build.unsupported.empty());
    s.result = solveLinear(s.build.model, s.build.cases);
    EXPECT_EQ(s.result.status, SolveStatus::Ok);
    return s;
}

}  // namespace

// --- C1: cantilever L = 4 m (node 1 clamped), P = 10 kN along -Z at the tip, self weight on (q = 0.7701 kN/m) ---
TEST(FemOriginalMeasured, C1CantileverWithSelfWeight) {
    Fixture f = makeFixture(77.01);
    const int n1 = addNode(f, 0, 0, 0), n2 = addNode(f, 4, 0, 0);
    addBar(f, n1, n2);
    clamp(f, n1);
    nodalLoad(f, n2, core::LoadDirection::MinusZ, 10.0);
    const Solved s = solve(f.project);
    expectRead(s.d(n2, 2) * 1e3, -11.475, 1e-3, "Dz N2 mm");      // Timoshenko; Euler-Bernoulli would give -11.333
    expectRead(s.d(n2, 4) * 1e3, 4.201, 1e-3, "Gy N2 mRad");
    expectRead(s.r(n1, 2), 13.080, 1e-3, "Rz N1");
    expectRead(s.r(n1, 4), -46.16, 1e-2, "My N1 (reaction)");
    // Section forces of the original: Vz = -11.542 and My = -21.57 read near x = 2 m (the tooltip interpolates the law: 0.1 % of sampling).
    const FemBar& bar = s.build.model.bars[0];
    const std::vector<FemBarLoad>& loads = s.build.cases[0].bars;
    const Dof6 mid = sectionForcesAt(bar, 4.0, s.c().barForces[0], loads, 2.0);
    expectRead(mid[2], -11.542, 1e-3, "Vz x=2");
    expectRead(mid[4], -21.57, 1e-2, "My x=2", 2e-3);
    EXPECT_LT(s.c().barForces[0].origin[4], 0.0);  // My < 0 at the clamp of a cantilever loaded along -Z
}

// --- C2: simply supported L = 6 m (two bars of 3 m), q = 10 kN/m global -Z ---
TEST(FemOriginalMeasured, C2SimplySupportedBeam) {
    FrameModel m;
    for (int i = 0; i < 3; ++i) {
        FemNode n;
        n.id = i + 1;
        n.position = {3.0 * i, 0, 0};
        m.nodes.push_back(n);
    }
    m.nodes[0].fixed = {true, true, true, true, false, false};   // torsion locked: see the ticket on unexcited rigid modes
    m.nodes[2].fixed = {false, true, true, false, false, false};
    for (int i = 0; i < 2; ++i) {
        FemBar b;
        b.id = i + 1;
        b.nodeI = i + 1;
        b.nodeJ = i + 2;
        b.e = 210e6;
        b.g = 210e6 / 2.6;
        b.alpha = 1.2e-5;
        b.section = SectionData{0.01, 1e-4, 5e-5, 8e-5, 0.004, 0.004, 0.0, 0.0};
        m.bars.push_back(b);
    }
    LoadCaseInput lc{1, "CM 1", {}, {}, {}};
    for (int i = 1; i <= 2; ++i) lc.bars.push_back(FemBarLoad{i, BarLoadKind::Distributed, true, 0.0, 3.0, {0, 0, -10}, {0, 0, -10}});
    const LinearResult r = solveLinear(m, std::span<const LoadCaseInput>(&lc, 1));
    ASSERT_EQ(r.status, SolveStatus::Ok);
    const LoadCaseResult& c = r.cases[0];
    expectRead(c.displacements[1][2] * 1e3, -8.175, 1e-3, "Dz N2 mm");  // Timoshenko 8.175000, Euler-Bernoulli 8.036
    expectRead(c.displacements[0][4] * 1e3, 4.286, 1e-3, "Gy N1 mRad");
    expectRead(c.reactions.at(1)[2], 30.000, 1e-3, "Rz N1");
    // Section forces at X = 2.934 m of bar 1: Vz = -0.658, My = +44.93 (sampling of the law: exact 44.98).
    const Dof6 f = sectionForcesAt(m.bars[0], 3.0, c.barForces[0], std::span<const FemBarLoad>(lc.bars.data(), 1), 2.934);
    expectRead(f[2], -0.658, 1e-3, "Vz", 3e-3);
    expectRead(f[4], 44.93, 1e-2, "My", 2e-3);
    EXPECT_GT(f[4], 0.0);  // simply supported beam loaded along -Z: My > 0 in the span
}

// --- C3: plane portal frame XZ (pillars 4 m, span 6 m, clamped bases, H = 20 kN along +X at N2) ---
TEST(FemOriginalMeasured, C3PortalFramePillarsBendAboutIzz) {
    Fixture f = makeFixture(0.01);
    const int n1 = addNode(f, 0, 0, 0), n2 = addNode(f, 0, 0, 4), n3 = addNode(f, 6, 0, 4), n4 = addNode(f, 6, 0, 0);
    addBar(f, n1, n2);
    addBar(f, n2, n3);
    addBar(f, n4, n3);
    clamp(f, n1);
    clamp(f, n4);
    nodalLoad(f, n2, core::LoadDirection::PlusX, 20.0);
    const Solved s = solve(f.project);
    expectRead(s.d(n2, 0) * 1e3, 6.950, 1e-3, "ux N2");   // all-Iy pillars would give 4.233, Euler-Bernoulli 4.083
    expectRead(s.d(n3, 0) * 1e3, 6.921, 1e-3, "ux N3");
    EXPECT_NEAR(std::fabs(s.d(n2, 2)) * 1e3, 0.011, 1e-3);
    EXPECT_NEAR(std::fabs(s.d(n3, 2)) * 1e3, 0.011, 1e-3);
    expectRead(s.d(n2, 4) * 1e3, 0.869, 1e-3, "Gy N2");
    expectRead(s.d(n3, 4) * 1e3, 0.863, 1e-3, "Gy N3");
    expectRead(s.r(n1, 0), -10.015, 1e-3, "Rx N1");
    expectRead(s.r(n1, 2), -5.908, 1e-3, "Rz N1");
    expectRead(s.r(n1, 4), -22.31, 1e-2, "My N1");
    expectRead(s.r(n4, 0), -9.985, 1e-3, "Rx N4");
    expectRead(s.r(n4, 2), 5.909, 1e-3, "Rz N4");
    expectRead(s.r(n4, 4), -22.23, 1e-2, "My N4");
    // Local frame of a pillar going up: x = +Z, y = +X, z = +Y (drawn axes of the original).
    const FemBar& pillar = s.build.model.bars[0];
    EXPECT_NEAR(pillar.ex[2], 1.0, 1e-12);
    EXPECT_NEAR(pillar.ey[0], 1.0, 1e-12);
    EXPECT_NEAR(pillar.ez[1], 1.0, 1e-12);
    // Section forces of the pillar of N1 (tooltip: N = +5.908 for a reaction pointing down, Vy = +10.015, Mz = +3.80 at the sampled point):
    // N and Vy keep the canonical sign.
    EXPECT_NEAR(s.c().barForces[0].origin[0], 5.908, 6e-3);
    EXPECT_NEAR(s.c().barForces[0].origin[1], 10.015, 1e-2);
}

// --- C6: two bars (X, L = 4 and Y, L = 2), Avy = 80 cm2 / Avz = 40 cm2; torsion and the shear area of each plane ---
TEST(FemOriginalMeasured, C6EachPlaneUsesItsShearArea) {
    Fixture f = makeFixture(0.01, 0.008, 0.004);
    const int n1 = addNode(f, 0, 0, 0), n2 = addNode(f, 4, 0, 0), n3 = addNode(f, 4, 2, 0);
    addBar(f, n1, n2);
    addBar(f, n2, n3);
    clamp(f, n1);
    nodalLoad(f, n2, core::LoadDirection::PlusX, 10.0);
    nodalLoad(f, n2, core::LoadDirection::PlusY, 10.0);
    nodalLoad(f, n3, core::LoadDirection::MinusZ, 10.0);
    const Solved s = solve(f.project);
    expectRead(s.d(n2, 0) * 1e3, 0.019, 1e-3, "Dx");
    expectRead(s.d(n2, 1) * 1e3, 20.379, 1e-3, "Dy");     // with Avz instead of Avy it would be 20.441
    expectRead(s.d(n2, 2) * 1e3, -10.283, 1e-3, "Dz");
    expectRead(s.d(n2, 3) * 1e3, -12.381, 1e-3, "Gx");    // torsion G It
    expectRead(s.d(n2, 4) * 1e3, 3.810, 1e-3, "Gy");
    expectRead(s.d(n2, 5) * 1e3, 7.619, 1e-3, "Gz");
    // Reactions (forces of the supports on the structure, global axes).
    expectRead(s.r(n1, 0), -10.000, 1e-3, "Rx");
    expectRead(s.r(n1, 1), -10.000, 1e-3, "Ry");
    expectRead(s.r(n1, 2), 10.001, 1e-3, "Rz");
    expectRead(s.r(n1, 3), 20.00, 1e-2, "Mx");
    expectRead(s.r(n1, 4), -40.00, 1e-2, "My");
    expectRead(s.r(n1, 5), -40.00, 1e-2, "Mz");
    // Section forces at the end of bar 1: N = +10.000, Vy = +10.000, Mt = -20.00 (canonical sign), My = -20.03 (inverted), Mz = +20.03 near its
    // sampled points; the signs are the point (N, Vy, Mt, Mz canonical; My opposite).
    const Dof6& e1 = s.c().barForces[0].end;
    EXPECT_NEAR(e1[0], 10.0, 1e-3);
    EXPECT_NEAR(e1[1], 10.0, 1e-3);
    EXPECT_NEAR(e1[3], -20.0, 1e-2);
}

// --- inclined bar (0,0,0)-(4,0,3), L = 5, q = 10 kN/m global -Z, clamped at the origin: the global load acts per metre of bar ---
namespace {
Fixture inclinedCantilever(double* extraCoefficient = nullptr) {
    Fixture f = makeFixture(0.01);
    const int n1 = addNode(f, 0, 0, 0), n2 = addNode(f, 4, 0, 3);
    const int bar = addBar(f, n1, n2);
    clamp(f, n1);
    barLoad(f, bar, core::BarLoadType::Uniform, 10.0);
    if (extraCoefficient) {
        core::BarEndsRecord rec;
        rec.ends[core::kBarOrigin].planes[core::kPlaneXZ].coefficient = *extraCoefficient;
        core::setBarEnds(f.project.barEnds, bar, rec);
    }
    return f;
}
}  // namespace

TEST(FemOriginalMeasured, InclinedBarGlobalLoadActsPerMetreOfBar) {
    const Fixture f = inclinedCantilever();
    const Solved s = solve(f.project);
    expectRead(s.r(1, 2), 50.001, 1e-3, "Rz");   // by horizontal projection it would be 40 kN
    expectRead(s.r(1, 4), -100.00, 1e-2, "My");
    expectRead(s.d(2, 0) * 1e3, 18.014, 1e-3, "Dx");
    expectRead(s.d(2, 2) * 1e3, -24.079, 1e-3, "Dz");
    expectRead(s.d(2, 4) * 1e3, 7.937, 1e-3, "Gy");
}

// --- partial fixity K = c / (1 - c) k_ii, k_ii = (4 + phi) E I / ((1 + phi) L) (xz plane, origin end) ---
TEST(FemOriginalMeasured, PartialFixityLawWithShearTerm) {
    for (const auto& [c, tip] : {std::pair{0.5, 14.027}, std::pair{0.8, 7.937 + 1.523}}) {
        double coefficient = c;
        const Fixture f = inclinedCantilever(&coefficient);
        const Solved s = solve(f.project);
        expectRead(s.d(2, 4) * 1e3, tip, 1e-3, "Gy N2 (cantilever rotation + spring rotation)");
    }
    // The 4 E I / L law would give 6.2 % ... 2.3 % different spring rotations: the clone must not use it.
    const double e = 210e6, i = 1e-4, l = 5.0, g = e / 2.6, phi = 12.0 * e * i / (g * 0.004 * l * l);
    const double kii = (4.0 + phi) * e * i / ((1.0 + phi) * l);
    EXPECT_NEAR(0.5 / 0.5 * kii, 16418.0 * 1.0, 20.0);   // c = 0.5: the K measured in the original, 16420 kN.m/rad
    EXPECT_NEAR(0.8 / 0.2 * kii, 65672.0, 20.0);          // c = 0.8: 65660 measured
}

// --- temperature ---
namespace {
core::Project temperatureCantilever(core::BarLoadType type, double t1, double t2, bool generic) {
    Fixture f = makeFixture(0.01);
    const int n1 = addNode(f, 0, 0, 0), n2 = addNode(f, 4, 0, 0);
    const int bar = addBar(f, n1, n2);
    clamp(f, n1);
    if (generic) {
        core::BarDescription d = core::defaultDescription(core::BarMaterialIcon::Concrete);
        d.id = 1;
        core::Section sec{"gen", core::ShapeKind::Generic, {0.025, 0.025, 0.025, 0.025}, {}};   // lower 25 + upper 25 mm: h = 50 mm
        sec.tabulated.area = 0.01;
        sec.tabulated.iy = 1e-4;
        sec.tabulated.iz = 5e-5;
        sec.tabulated.it = 8e-5;
        sec.tabulated.avy = 0.004;
        sec.tabulated.avz = 0.004;
        d.inlineSection = sec;
        d.sectionId = f.section;
        d.materialId = f.material;
        f.project.barDescriptions.items.push_back(d);
        f.project.barDescriptions.next = 2;
    }
    core::BarLoad& bl = barLoad(f, bar, type, t1, t2);
    bl.axes = core::BarLoadAxes::Local;
    return f.project;
}
}  // namespace

TEST(FemOriginalMeasured, UniformTemperatureElongatesTheBar) {
    const Solved s = solve(temperatureCantilever(core::BarLoadType::UniformTemperature, 10.0, 0.0, false));
    expectRead(s.d(2, 0) * 1e3, 0.480, 1e-3, "Dx");
    EXPECT_NEAR(s.d(2, 2) * 1e3, 0.0, 2e-4);  // (self weight of 1e-4 kN/m: 1.5e-4 mm)
}

TEST(FemOriginalMeasured, VariableTemperatureMeanPlusCurvatureOverTheGenericDepth) {
    const Solved s = solve(temperatureCantilever(core::BarLoadType::VariableTemperature, 20.0, 0.0, true));
    ASSERT_NEAR(s.build.model.bars[0].section.depthZ, 0.05, 1e-12);   // h = Canto inferior + Canto superior of the generic section
    expectRead(s.d(2, 0) * 1e3, 0.480, 1e-3, "Dx (mean (T1 + T2) / 2 = 10 degC as uniform increment)");
    expectRead(s.d(2, 2) * 1e3, 38.400, 1e-3, "Dz (T1 = 20 on the -z face: the tip goes up)", 2e-3);
    expectRead(s.d(2, 4) * 1e3, -19.200, 1e-3, "Gy mRad", 2e-3);
}

// --- mechanism: warning of the original with the DOF type and the node coordinates ---
TEST(FemOriginalMeasured, MechanismWarningHasTheTextOfTheOriginal) {
    Fixture f = makeFixture(0.01);
    const int n1 = addNode(f, 0, 0, 0), n2 = addNode(f, 4, 0, 0);
    addBar(f, n1, n2);
    core::ExternalSupport pinned;
    pinned.type = core::ExternalSupportType::Pinned;  // "Apoyo fijo": displacements blocked, rotations free
    f.project.nodeSupports.external[n1] = pinned;
    nodalLoad(f, n2, core::LoadDirection::MinusZ, 10.0);
    const FrameBuild fb = buildFrameModel(f.project);
    const LinearResult r = solveLinear(fb.model, fb.cases);
    ASSERT_EQ(r.status, SolveStatus::Mechanism);
    EXPECT_EQ(r.cases[0].message, r.message);
    ASSERT_GE(r.mechanismNode, 1);
    ASSERT_GE(r.mechanismDof, 0);
    const Vec3 pos = r.mechanismNode == n1 ? Vec3{0, 0, 0} : Vec3{4, 0, 0};
    EXPECT_EQ(r.message, mechanismWarning(r.mechanismDof, pos));
    // The text measured in the original for the base node (x = 0, y = 0, z = 0) and a rotation about Y.
    EXPECT_EQ(mechanismWarning(4, {0, 0, 0}),
              "Esta estructura es un mecanismo. Revise los datos introducidos.\n"
              "Giro alrededor del eje Y\n"
              "El error se encuentra en el punto de coordenadas:\n"
              " (x = 0.000000, y = 0.000000, z = 0.000000)\n"
              "Alguna de las barras de la estructura no está vinculada correctamente en sus extremos, es decir, no es estable.");
    EXPECT_NE(mechanismWarning(0, {1, 2, 3}).find("Desplazamiento según el eje X"), std::string::npos);
    EXPECT_NE(mechanismWarning(5, {1, 2, 3}).find("(x = 1.000000, y = 2.000000, z = 3.000000)"), std::string::npos);
}

// --- sign convention (TK-107): only My is the opposite of the canonical resultant ---
TEST(FemOriginalMeasured, SignConventionInvertsOnlyMy) {
    const Dof6 canonical{1, 2, 3, 4, 5, 6};
    const Dof6 conv = applySignConvention(canonical);
    EXPECT_EQ(conv, (Dof6{1, 2, 3, 4, -5, 6}));
    EXPECT_EQ(removeSignConvention(conv), canonical);
}
