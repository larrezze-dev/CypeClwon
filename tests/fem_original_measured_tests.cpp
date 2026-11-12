// Cross-validation of the linear frame solver (project adapter included) against values MEASURED in the original CYPE 3D (T-P8-05):
// docs/research/solver_medidas_original.md (cases C1, C2, C3, C6, inclined bar with a global load, partial fixity, temperature, mechanism
// warning, sign convention). The original shows displacements to 0.001 mm / 0.001 mRad, forces to 0.001 kN and moments to 0.01 kN.m, so the
// tolerance is 1e-3 relative plus the reading resolution.
//
// Common properties: A = 100 cm2, Iyy = 10000 cm4, Izz = 5000 cm4, It = 8000 cm4, E = 210000 MPa, nu = 0.3, Avz = 40 cm2, Avy = 40 cm2
// (80 cm2 in C6), alpha = 1.2e-5, gamma = 77.01 kN/m3 (C1) or 0.01 kN/m3 (the minimum of the original, C2..C6).
#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <span>
#include <vector>

#include "core/bar_ends.h"
#include "core/bar_load.h"
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
    m.nodes[0].fixed = {true, true, true, false, false, false};  // torsion NOT locked: the isolated twist mode is fixed to 0 (TK-108 rule [R])
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
    EXPECT_FALSE(r.nullModeDofs.empty());                      // the twist of the collinear chain was fixed silently
    for (int i = 0; i < 3; ++i) EXPECT_NEAR(c.displacements[i][3], 0.0, 1e-12) << "Gx of node " << i + 1;   // measured: Gx = 0.000 at N1, N2, N3
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
core::Project temperatureCantilever(core::BarLoadType type, double t1, double t2, bool generic,
                                    core::TemperatureDirection direction = core::TemperatureDirection::PlusZ, double angle = 0.0,
                                    std::array<double, 4> dims = {0.025, 0.025, 0.025, 0.025}) {
    Fixture f = makeFixture(0.01);
    const int n1 = addNode(f, 0, 0, 0), n2 = addNode(f, 4, 0, 0);
    const int bar = addBar(f, n1, n2);
    clamp(f, n1);
    if (generic) {
        core::BarDescription d = core::defaultDescription(core::BarMaterialIcon::Concrete);
        d.id = 1;
        core::Section sec{"gen", core::ShapeKind::Generic, {dims[0], dims[1], dims[2], dims[3]}, {}};   // default 25 + 25 mm: h = 50 mm
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
    bl.temperatureDirection = direction;
    bl.temperatureAngle = angle;
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

// --- TK-105c: variable temperature along Y+ and with a free angle (hy = left + right width, hz = lower + upper depth) ---
TEST(FemOriginalMeasured, VariableTemperatureYPlusUsesTheWidth) {
    // Widths 50 + 50 = 100 mm, depths 25 + 25 = 50 mm: Dy = 19.2 mm and Gz = +9.6 mRad (with widths 25 + 25 it is 38.4 / 19.2).
    Solved s = solve(temperatureCantilever(core::BarLoadType::VariableTemperature, 20.0, 0.0, true, core::TemperatureDirection::PlusY, 0.0,
                                           {0.05, 0.05, 0.025, 0.025}));
    expectRead(s.d(2, 1) * 1e3, 19.200, 1e-3, "Dy (width 100 mm)", 2e-3);
    expectRead(s.d(2, 5) * 1e3, 9.600, 1e-3, "Gz mRad", 2e-3);
    expectRead(s.d(2, 0) * 1e3, 0.480, 1e-3, "Dx");
    s = solve(temperatureCantilever(core::BarLoadType::VariableTemperature, 20.0, 0.0, true, core::TemperatureDirection::PlusY));
    expectRead(s.d(2, 1) * 1e3, 38.400, 1e-3, "Dy (width 50 mm)", 2e-3);
    expectRead(s.d(2, 5) * 1e3, 19.200, 1e-3, "Gz mRad", 2e-3);
}

TEST(FemOriginalMeasured, VariableTemperatureAngleRotatesPlusZAboutX) {
    // Angle 0 equals the Z+ icon.
    Solved s = solve(temperatureCantilever(core::BarLoadType::VariableTemperature, 20.0, 0.0, true, core::TemperatureDirection::Angle, 0.0));
    expectRead(s.d(2, 2) * 1e3, 38.400, 1e-3, "Dz at 0 degrees", 2e-3);
    // Angle 90: Dy = -38.4 mm, Gz = -19.2 mRad (widths 50, depths 50).
    s = solve(temperatureCantilever(core::BarLoadType::VariableTemperature, 20.0, 0.0, true, core::TemperatureDirection::Angle, 90.0));
    expectRead(s.d(2, 1) * 1e3, -38.400, 1e-3, "Dy at 90 degrees", 2e-3);
    expectRead(s.d(2, 5) * 1e3, -19.200, 1e-3, "Gz at 90 degrees", 2e-3);
    EXPECT_NEAR(s.d(2, 2) * 1e3, 0.0, 2e-3);
    // Angle 45, widths 100 mm and depths 50 mm: Dz = +27.153, Dy = -13.576 mm, Gy = -13.577, Gz = -6.788 mRad.
    s = solve(temperatureCantilever(core::BarLoadType::VariableTemperature, 20.0, 0.0, true, core::TemperatureDirection::Angle, 45.0,
                                    {0.05, 0.05, 0.025, 0.025}));
    expectRead(s.d(2, 2) * 1e3, 27.153, 1e-3, "Dz at 45 degrees", 2e-3);
    expectRead(s.d(2, 1) * 1e3, -13.576, 1e-3, "Dy at 45 degrees", 2e-3);
    expectRead(s.d(2, 4) * 1e3, -13.577, 1e-3, "Gy at 45 degrees", 2e-3);
    expectRead(s.d(2, 5) * 1e3, -6.788, 1e-3, "Gz at 45 degrees", 2e-3);
    // A negative angle mirrors the y component.
    s = solve(temperatureCantilever(core::BarLoadType::VariableTemperature, 20.0, 0.0, true, core::TemperatureDirection::Angle, -90.0));
    expectRead(s.d(2, 1) * 1e3, 38.400, 1e-3, "Dy at -90 degrees", 2e-3);
}

// --- TK-105d: partial fixity in the xy plane, at the final end, and the "Rigidez rotacional" mode (K used directly) ---
namespace {
Fixture c1Cantilever(double gamma = 0.01) {
    Fixture f = makeFixture(gamma);
    const int n1 = addNode(f, 0, 0, 0), n2 = addNode(f, 4, 0, 0);
    addBar(f, n1, n2);
    clamp(f, n1);
    nodalLoad(f, n2, core::LoadDirection::PlusY, 10.0);
    return f;
}
}  // namespace

TEST(FemOriginalMeasured, PartialFixityXyPlaneOrigin) {
    Fixture f = c1Cantilever();
    core::BarEndsRecord rec;
    rec.ends[core::kBarOrigin].planes[core::kPlaneXY].coefficient = 0.5;
    core::setBarEnds(f.project.barEnds, 1, rec);
    const Solved s = solve(f.project);
    expectRead(s.d(2, 1) * 1e3, 35.956, 1e-3, "Dy", 2e-3);          // with 4 E I / L it would be 35.679
    // The original reads the rotation of the bar end at the clamped node (the spring rotation theta = 40 / 10312); the node itself does not rotate.
    expectRead(s.c().barDisplacements[0].origin[5] * 1e3, 3.879, 1e-3, "Gz N1 mRad (bar end)", 2e-3);
    EXPECT_NEAR(s.d(1, 5), 0.0, 1e-15);
}

TEST(FemOriginalMeasured, RotationalStiffnessIsUsedDirectly) {
    Fixture f = c1Cantilever();
    core::BarEndsRecord rec;
    core::EndPlaneFixity& pl = rec.ends[core::kBarOrigin].planes[core::kPlaneXY];
    pl.kind = core::EndFixityKind::Rotational;
    pl.stiffnessKnMPerRad = 5000.0;
    core::setBarEnds(f.project.barEnds, 1, rec);
    const Solved s = solve(f.project);
    expectRead(s.d(2, 1) * 1e3, 52.441, 1e-3, "Dy", 2e-3);
    expectRead(s.c().barDisplacements[0].origin[5] * 1e3, 8.000, 1e-3, "Gz N1 mRad (bar end)", 2e-3);
}

namespace {
/// C2 through the project adapter: 2 bars of 3 m, N1 "Apoyo fijo" (Pinned), N3 "recta X articulada" (LineXPinned), q = 10 kN/m along -Z.
Fixture c2Project() {
    Fixture f = makeFixture(0.01);
    const int n1 = addNode(f, 0, 0, 0), n2 = addNode(f, 3, 0, 0), n3 = addNode(f, 6, 0, 0);
    const int b1 = addBar(f, n1, n2), b2 = addBar(f, n2, n3);
    core::ExternalSupport pinned, line;
    pinned.type = core::ExternalSupportType::Pinned;
    line.type = core::ExternalSupportType::LineXPinned;
    f.project.nodeSupports.external[n1] = pinned;
    f.project.nodeSupports.external[n3] = line;
    barLoad(f, b1, core::BarLoadType::Uniform, 10.0);
    barLoad(f, b2, core::BarLoadType::Uniform, 10.0);
    return f;
}
}  // namespace

TEST(FemOriginalMeasured, PartialFixityAtTheFinalEndOfBar1) {
    Fixture f = c2Project();
    core::BarEndsRecord rec;
    rec.ends[core::kBarFinal].planes[core::kPlaneXZ].coefficient = 0.5;
    core::setBarEnds(f.project.barEnds, 1, rec);
    const Solved s = solve(f.project);
    expectRead(s.d(2, 2) * 1e3, -10.739, 1e-3, "Dz N2", 2e-3);       // with 4 E I / L: -10.586
    expectRead(s.d(1, 4) * 1e3, 5.140, 1e-3, "Gy N1 mRad", 2e-3);
    // The original shows the rotation of the bar-1 end at N2 (the node rotates -0.855 with bar 2 rigidly attached).
    expectRead(s.c().barDisplacements[0].end[4] * 1e3, 0.855, 1e-3, "Gy N2 mRad (end of bar 1)", 3e-3);
    EXPECT_NEAR(s.d(2, 4) * 1e3, -0.855, 4e-3);
}

// --- TK-108: unexcited rigid twist (fixed to 0 without a warning) versus coupled null modes (mechanism, with or without load) ---
TEST(FemOriginalMeasured, C2WithoutFixingTheTwistCalculatesWithZeroGx) {
    const Fixture f = c2Project();     // N1 Apoyo fijo, N3 recta X articulada: the twist of the 3 collinear nodes is a null mode
    const Solved s = solve(f.project);
    EXPECT_FALSE(s.result.nullModeDofs.empty());
    for (int n = 1; n <= 3; ++n) EXPECT_NEAR(s.d(n, 3), 0.0, 1e-12) << "Gx N" << n;
    expectRead(s.d(2, 2) * 1e3, -8.175, 1e-3, "Dz N2");
    expectRead(s.d(1, 4) * 1e3, 4.286, 1e-3, "Gy N1");
    expectRead(s.d(3, 4) * 1e3, -4.286, 1e-3, "Gy N3");
}

TEST(FemOriginalMeasured, C2BothEndsApoyoFijoAlsoCalculates) {
    Fixture f = c2Project();
    core::ExternalSupport pinned;
    pinned.type = core::ExternalSupportType::Pinned;
    f.project.nodeSupports.external[3] = pinned;
    const Solved s = solve(f.project);
    for (int n = 1; n <= 3; ++n) EXPECT_NEAR(s.d(n, 3), 0.0, 1e-12);
    expectRead(s.d(2, 2) * 1e3, -8.175, 1e-3, "Dz N2");
}

namespace {
/// L-shaped work: N1 (0,0,0), N2 (4,0,0), N3 (4,2,0); bar 1 along X, bar 2 along Y; N2 loaded +X 10 and +Y 10, N3 -Z 10 kN.
Fixture lShape(core::ExternalSupportType n1Type, bool loadN3 = true) {
    Fixture f = makeFixture(0.01, 0.008, 0.004);
    const int n1 = addNode(f, 0, 0, 0), n2 = addNode(f, 4, 0, 0), n3 = addNode(f, 4, 2, 0);
    addBar(f, n1, n2);
    addBar(f, n2, n3);
    core::ExternalSupport a, b;
    a.type = n1Type;
    b.type = core::ExternalSupportType::LineXPinned;
    f.project.nodeSupports.external[n1] = a;
    f.project.nodeSupports.external[n2] = b;
    nodalLoad(f, n2, core::LoadDirection::PlusX, 10.0);
    nodalLoad(f, n2, core::LoadDirection::PlusY, 10.0);
    if (loadN3) nodalLoad(f, n3, core::LoadDirection::MinusZ, 10.0);
    return f;
}
}  // namespace

TEST(FemOriginalMeasured, LShapeFixedAtN1CalculatesWithoutWarning) {
    const Fixture f = lShape(core::ExternalSupportType::Fixed);
    const Solved s = solve(f.project);
    expectRead(s.d(2, 0) * 1e3, 0.019, 1e-3, "Dx N2");
    expectRead(s.d(2, 3) * 1e3, -12.381, 1e-3, "Gx N2 mRad");
    EXPECT_TRUE(s.result.nullModeDofs.empty());
}

TEST(FemOriginalMeasured, LShapeWithApoyoFijoIsAMechanismWithOrWithoutLoad) {
    for (const bool loadN3 : {true, false}) {
        const Fixture f = lShape(core::ExternalSupportType::Pinned, loadN3);
        const FrameBuild fb = buildFrameModel(f.project);
        ASSERT_TRUE(fb.errors.empty());
        const LinearResult r = solveLinear(fb.model, fb.cases);
        ASSERT_EQ(r.status, SolveStatus::Mechanism) << "loadN3 " << loadN3;
        EXPECT_EQ(r.mechanismNode, 3);          // measured: N3, "Desplazamiento según el eje Z", point (4, 2, 0)
        EXPECT_EQ(r.mechanismDof, 2);
        EXPECT_EQ(r.message, mechanismWarning(2, {4, 2, 0}));
        EXPECT_NE(r.message.find("Desplazamiento según el eje Z"), std::string::npos);
        EXPECT_NE(r.message.find("(x = 4.000000, y = 2.000000, z = 0.000000)"), std::string::npos);
        // Without the load of N3 the tiny self weight (0.01 kN/m3) still excites the mode (rule [R], solver_medidas_original.md 10.3).
    }
}

TEST(FemOriginalMeasured, TwistModeLoadedByATorqueIsAMechanismForThatCase) {
    FrameModel m;
    for (int i = 0; i < 2; ++i) {
        FemNode n;
        n.id = i + 1;
        n.position = {4.0 * i, 0, 0};
        m.nodes.push_back(n);
    }
    m.nodes[0].fixed = {true, true, true, false, false, false};
    m.nodes[1].fixed = {false, true, true, false, false, false};
    FemBar b;
    b.id = 1;
    b.nodeI = 1;
    b.nodeJ = 2;
    b.e = 210e6;
    b.g = 210e6 / 2.6;
    b.section = SectionData{0.01, 1e-4, 5e-5, 8e-5, 0.004, 0.004, 0.0, 0.0};
    m.bars.push_back(b);
    LoadCaseInput torque{1, "T", {FemNodalLoad{2, {0, 0, 0, 5.0, 0, 0}}}, {}, {}};
    LoadCaseInput balanced{2, "B", {FemNodalLoad{2, {0, 0, -1.0, 0, 0, 0}}}, {}, {}};
    const LoadCaseInput cases[] = {torque, balanced};
    const LinearResult r = solveLinear(m, cases);
    EXPECT_EQ(r.cases[0].status, SolveStatus::Mechanism);
    EXPECT_EQ(r.cases[1].status, SolveStatus::Ok);
}

// --- TK-105a: a node rotation above pi/2 gives "desplazamientos demasiado grandes" (K = 25.6 calculates, K = 25.4 warns) ---
namespace {
LinearResult elasticBaseCantilever(double k) {
    FrameModel m;
    for (int i = 0; i < 2; ++i) {
        FemNode n;
        n.id = i + 1;
        n.position = {4.0 * i, 0, 0};
        m.nodes.push_back(n);
    }
    m.nodes[0].fixed = {true, true, true, true, false, true};
    m.nodes[0].spring[4] = k;     // Gy elastic, kN.m/rad
    FemBar b;
    b.id = 1;
    b.nodeI = 1;
    b.nodeJ = 2;
    b.e = 210e6;
    b.g = 210e6 / 2.6;
    b.section = SectionData{0.01, 1e-4, 5e-5, 8e-5, 0.004, 0.004, 0.0, 0.0};
    m.bars.push_back(b);
    LoadCaseInput lc{1, "CM 1", {FemNodalLoad{2, {0, 0, -10.0, 0, 0, 0}}}, {}, {}};
    return solveLinear(m, std::span<const LoadCaseInput>(&lc, 1));
}
}  // namespace

TEST(FemOriginalMeasured, ExcessiveRotationThresholdIsPiOverTwo) {
    for (const double k : {100.0, 30.0, 27.0, 26.8, 26.7, 26.6, 25.6}) {
        const LinearResult r = elasticBaseCantilever(k);
        EXPECT_EQ(r.status, SolveStatus::Ok) << "K = " << k;
    }
    for (const double k : {25.4, 25.0, 20.0, 10.0, 1.0}) {
        const LinearResult r = elasticBaseCantilever(k);
        EXPECT_EQ(r.status, SolveStatus::ExcessiveDisplacements) << "K = " << k;
        EXPECT_EQ(r.cases[0].status, SolveStatus::ExcessiveDisplacements);
        EXPECT_EQ(r.cases[0].message, excessiveDisplacementsMessage());
        EXPECT_TRUE(r.cases[0].displacements.empty());
    }
    // K = 30 measured: Dz = -5343.723 mm, Gy(N2) = 1337.170 mRad (rotation of N2 = base rotation + bending).
    const LinearResult r = elasticBaseCantilever(30.0);
    expectRead(r.cases[0].displacements[1][2] * 1e3, -5343.723, 1e-3, "Dz K=30", 2e-3);
    expectRead(r.cases[0].displacements[1][4] * 1e3, 1337.170, 1e-3, "Gy K=30", 2e-3);
    EXPECT_EQ(excessiveDisplacementsMessage(),
              "Se han obtenido desplazamientos demasiado grandes. Puede consultar los mensajes de aviso correspondientes mediante la opción "
              "'Mostrar/Oculta incidencias' del menú 'Cálculo'.");
    // A pure translation of 6.67 m does not warn: only rotations are checked.
}

// --- TK-105g: shear area outside 0.01..1000000 cm2 is an error, never a silent Euler-Bernoulli fallback ---
TEST(FemOriginalMeasured, ShearAreaZeroIsRejectedByTheBuilder) {
    for (const bool yPlane : {true, false}) {
        Fixture f = c1Cantilever();
        core::CrossSection* cs = &f.project.structure.sections.back();
        (yPlane ? cs->shearAreaYM2 : cs->shearAreaZM2) = 0.0;
        const FrameBuild fb = buildFrameModel(f.project);
        ASSERT_FALSE(fb.errors.empty());
        EXPECT_EQ(fb.errors.front(), std::string("Barra 1: ") + shearAreaRangeMessage(yPlane ? "Avy" : "Avz"));
        EXPECT_EQ(shearAreaRangeMessage("Avz"), "El valor de 'Área de cortante (Avz)' debe estar entre 0.01 y 1000000.00.");
    }
    Fixture f = c1Cantilever();
    core::CrossSection* cs = &f.project.structure.sections.back();
    cs->shearAreaYM2 = cs->shearAreaZM2 = kMinShearAreaM2;     // 0.01 cm2 is the smallest admitted
    EXPECT_TRUE(buildFrameModel(f.project).errors.empty());
    cs->shearAreaYM2 = cs->shearAreaZM2 = 1000000.0e-4;        // 1e6 cm2: pure bending
    EXPECT_TRUE(buildFrameModel(f.project).errors.empty());
    cs->shearAreaZM2 = 1000001.0e-4;
    EXPECT_FALSE(buildFrameModel(f.project).errors.empty());
}

// --- third pass (solver_medidas_original.md section 10): per-bar self weight switch, descending vertical bar, refined mechanism rule ---
namespace {
/// Work c7: N1 (0,0,0)-N2 (0,0,-4) descending, N3 (3,0,0)-N4 (3,0,4) ascending, gamma 77.01, bases N1 and N3 with `base`.
Fixture c7(core::ExternalSupportType base) {
    Fixture f = makeFixture(77.01);
    const int n1 = addNode(f, 0, 0, 0), n2 = addNode(f, 0, 0, -4), n3 = addNode(f, 3, 0, 0), n4 = addNode(f, 3, 0, 4);
    addBar(f, n1, n2);
    addBar(f, n3, n4);
    core::ExternalSupport s;
    s.type = base;
    f.project.nodeSupports.external[n1] = s;
    f.project.nodeSupports.external[n3] = s;
    return f;
}
}  // namespace

TEST(FemOriginalMeasured, PerBarSelfWeightSwitchIsHonoured) {
    Fixture f = c7(core::ExternalSupportType::Fixed);
    f.project.barLoads.selfWeightOff.insert(1);
    const Solved s = solve(f.project);
    expectRead(s.c().reactions.at(1)[2], 0.000, 1e-3, "Rz N1 (bar 1 switched off)");
    expectRead(s.c().reactions.at(3)[2], 3.080, 1e-3, "Rz N3 (gamma A L)");
}

TEST(FemOriginalMeasured, DescendingVerticalBarHasTheTriadOfTheAscendingOne) {
    const Fixture f = c7(core::ExternalSupportType::Fixed);
    for (int bar : {1, 2}) {
        const auto frame = core::barLocalAxes(f.project.structure, bar);
        ASSERT_TRUE(frame.has_value());
        EXPECT_NEAR(frame->x.z, 1.0, 1e-12) << bar;  // x = +Z
        EXPECT_NEAR(frame->y.x, 1.0, 1e-12) << bar;  // y = +X
        EXPECT_NEAR(frame->z.y, 1.0, 1e-12) << bar;  // z = +Y
    }
    const FrameBuild fb = buildFrameModel(f.project);
    EXPECT_EQ(fb.model.bars[0].nodeI, 2);  // the lower node is the origin of the local x axis
    EXPECT_EQ(fb.model.bars[1].nodeI, 3);
}

TEST(FemOriginalMeasured, UnexcitedNullModeCalculatesAndExcitedOneWarnsWithTheZone) {
    // "Apoyo fijo" bases and only the axial self weight: every bar can rotate about its support, but nothing excites it [V: no warning].
    Fixture f = c7(core::ExternalSupportType::Pinned);
    const Solved s = solve(f.project);
    EXPECT_FALSE(s.result.nullModeDofs.empty());
    expectRead(s.c().reactions.at(3)[2], 3.080, 1e-3, "Rz N3");
    // +X 9.81 kN at N2 excites the rotation of bar 1: warning naming N2 / Dx with the zone of the vertical bar [V].
    nodalLoad(f, 2, core::LoadDirection::PlusX, 9.81);
    const FrameBuild fb = buildFrameModel(f.project);
    const LinearResult r = solveLinear(fb.model, fb.cases);
    ASSERT_EQ(r.status, SolveStatus::Mechanism);
    EXPECT_EQ(r.mechanismNode, 2);
    EXPECT_EQ(r.mechanismDof, 0);
    EXPECT_EQ(r.message, mechanismWarningZone(0, 0, 0, 0, 0, -4));
    EXPECT_NE(r.message.find("Desplazamiento según el eje X"), std::string::npos);
    EXPECT_NE(r.message.find("en la zona de coordenadas:\n (0.000000,0.000000) a (0.000000,0.000000), en la cota -4.000000"), std::string::npos);
}

TEST(FemOriginalMeasured, DescendingBarLoadPositionsStayMeasuredFromTheStartNode) {
    // Point load +X 10 kN at 25 % from the START node of each bar: the descending bar 1 (start N1 at z = 0) and its ascending mirror bar 2
    // (start N3 at z = 0) give the same reactions (My with the opposite sign: the arm is below / above the base), and the drawing stays on the bar.
    Fixture f = c7(core::ExternalSupportType::Fixed);
    f.project.barLoads.selfWeightOff = {1, 2};
    for (int bar : {1, 2}) {
        core::BarLoad bl;
        bl.id = f.project.barLoads.nextId++;
        bl.barId = bar;
        bl.loadCaseId = 0;
        bl.type = core::BarLoadType::Point;
        bl.axes = core::BarLoadAxes::Global;
        bl.direction = core::LoadDirection::PlusX;
        bl.start = 0.25;
        bl.value1 = 10.0;
        f.project.barLoads.items.push_back(bl);
        const auto frame = core::barLocalAxes(f.project.structure, bar);
        const core::BarLoadDrawing g = core::barLoadDrawing(bl, *frame, f.project.nodalLoads.scales);
        const double z = g.arrow.second.z;
        EXPECT_TRUE(bar == 1 ? (z <= 0.0 && z >= -4.0) : (z >= 0.0 && z <= 4.0)) << bar << " z " << z;
    }
    const Solved s = solve(f.project);
    const Dof6 &r1 = s.c().reactions.at(1), &r3 = s.c().reactions.at(3);
    EXPECT_NEAR(r1[0], -10.0, 1e-9);
    EXPECT_NEAR(r3[0], -10.0, 1e-9);
    EXPECT_NEAR(r1[4], -r3[4], 1e-9);
    EXPECT_GT(std::fabs(r1[4]), 1e-3);
}
