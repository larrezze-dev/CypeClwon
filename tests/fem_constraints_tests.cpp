// T-P8-03: rigid diaphragms, ligatures, oblique and elastic supports (with "Sentido": non-linear combinations) and the partial fixity internal
// link of nodes, validated against the values MEASURED in the original (docs/research/solver_diafragmas_ligaduras_apoyos.md section 10).
// The original shows displacements to 0.001 mm / 0.001 mRad, forces to 0.001 kN and moments to 0.01 kN.m.
//
// Generic section of the measured works: A = 100 cm2, Avy = Avz = 40 cm2, Iyy = 1e4 cm4, Izz = 5e3 cm4, It = 8e3 cm4, E = 210000 MPa,
// nu = 0.3, gamma = 0.01 kN/m3 (minimum of the original).
#include <gtest/gtest.h>

#include <cmath>
#include <span>
#include <vector>

#include "core/fem/frame_builder.h"
#include "core/fem/linear_solver.h"
#include "core/project.h"

using namespace cc::fem;
namespace core = cc::core;

namespace {

void expectRead(double actual, double expected, double resolution, const char* what, double rel = 1e-3) {
    EXPECT_NEAR(actual, expected, rel * std::fabs(expected) + resolution) << what;}

struct Fixture {
    core::Project project;
    int material = 0, section = 0;
};

Fixture makeFixture() {
    Fixture f;
    core::Material mat;
    mat.name = "ref";
    mat.kind = core::MaterialKind::Steel;
    mat.elasticModulusMPa = 210000.0;
    mat.poissonRatio = 0.3;
    mat.densityKnM3 = 0.01;
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

int indexOf(const FrameModel& m, int id) {
    for (std::size_t i = 0; i < m.nodes.size(); ++i)
        if (m.nodes[i].id == id) return static_cast<int>(i);
    ADD_FAILURE() << "node " << id;
    return 0;
}

struct Solved {
    FrameBuild build;
    LinearResult result;
    const LoadCaseResult& c(std::size_t k = 0) const { return result.cases.at(k); }
    double d(int id, int dof, std::size_t k = 0) const { return c(k).displacements.at(indexOf(build.model, id))[dof]; }
    double r(int id, int dof, std::size_t k = 0) const { return c(k).reactions.at(id)[dof]; }
};

FrameBuild build(const core::Project& p) {
    FrameBuild b = buildFrameModel(p);
    EXPECT_TRUE(b.errors.empty());
    EXPECT_TRUE(b.unsupported.empty()) << (b.unsupported.empty() ? "" : b.unsupported.front());
    return b;
}

Solved solve(const core::Project& p) {
    Solved s;
    s.build = build(p);
    s.result = solveLinear(s.build.model, s.build.cases);
    EXPECT_EQ(s.result.status, SolveStatus::Ok) << s.result.message;
    return s;
}

/// Non-linear combinations of the load case 0 ("Peso propio") with the given coefficients, named like the original ("[1] PP").
Solved solveCombinations(const core::Project& p, const std::vector<std::pair<double, std::string>>& combos) {
    Solved s;
    s.build = build(p);
    std::vector<LoadCaseInput> cases;
    for (std::size_t i = 0; i < combos.size(); ++i) {
        std::vector<double> coef(s.build.cases.size(), 0.0);
        coef[0] = combos[i].first;
        cases.push_back(combineLoadCases(s.build.cases, coef, static_cast<int>(i + 1), "[" + std::to_string(i + 1) + "] " + combos[i].second));
    }
    s.result = solveNonlinear(s.build.model, cases);
    return s;
}

// Cantilever N1-N2 (L = 4 m along X, N1 clamped); N2 "Genérica": Dx, Dy Fijo, Dz Elástico K = 1000 kN/m with `sense`, rotations free.
Fixture springCantilever(core::SupportSense sense, core::LoadDirection dir, int& n2) {
    Fixture f = makeFixture();
    const int n1 = addNode(f, 0, 0, 0);
    n2 = addNode(f, 4, 0, 0);
    addBar(f, n1, n2);
    clamp(f, n1);
    core::ExternalSupport g;
    g.type = core::ExternalSupportType::Generic;
    g.generic.translations = {core::DofMode::Fixed, core::DofMode::Fixed, core::DofMode::Elastic};
    g.elasticConstants[2] = 1000.0;
    g.senses[2] = sense;
    f.project.nodeSupports.external[n2] = g;
    nodalLoad(f, n2, dir, 10.0);
    return f;
}

}  // namespace

// --- 10.4 / 10.1: linear spring (the linear analysis ignores the "Sentido": bilateral) ---
TEST(FemConstraints, ElasticSupportLinearIgnoresSense) {
    int n2 = 0;
    const Fixture f = springCantilever(core::SupportSense::Positive, core::LoadDirection::MinusZ, n2);
    const Solved s = solve(f.project);
    EXPECT_TRUE(hasUnilateralSprings(s.build.model));
    expectRead(s.d(n2, 2) * 1e3, -5.070, 1e-3, "Dz N2 mm (10 / (972.5 + 1000))");
    expectRead(s.d(n2, 4) * 1e3, 1.878, 1e-3, "Gy N2 mRad");
    expectRead(s.r(n2, 2), 1000.0 * 5.070e-3, 2e-3, "Rz N2 = -K u");
}

// --- 10.1: springs with "Sentido" in the non-linear combinations ---
TEST(FemConstraints, SpringSenseNonLinearCombinations) {
    int n2 = 0;
    {
        const Fixture f = springCantilever(core::SupportSense::Positive, core::LoadDirection::MinusZ, n2);
        const Solved s = solveCombinations(f.project, {{1.0, "PP"}, {1.35, "1.35·PP"}});
        ASSERT_EQ(s.result.status, SolveStatus::Ok) << s.result.message;
        expectRead(s.d(n2, 2, 0) * 1e3, -10.283, 1e-3, "Positive, -Z: spring released");
        expectRead(s.d(n2, 4, 0) * 1e3, 3.810, 1e-3, "Gy");
        expectRead(s.d(n2, 2, 1) * 1e3, -13.882, 1e-3, "[2] 1.35·PP");
        EXPECT_EQ(s.c(0).name, "[1] PP");
    }
    {
        const Fixture f = springCantilever(core::SupportSense::Negative, core::LoadDirection::MinusZ, n2);
        const Solved s = solveCombinations(f.project, {{1.0, "PP"}});
        expectRead(s.d(n2, 2) * 1e3, -5.070, 1e-3, "Negative, -Z: active");
        expectRead(s.d(n2, 4) * 1e3, 1.878, 1e-3, "Gy");
    }
    {
        const Fixture f = springCantilever(core::SupportSense::Negative, core::LoadDirection::PlusZ, n2);
        const Solved s = solveCombinations(f.project, {{1.0, "PP"}});
        expectRead(s.d(n2, 2) * 1e3, 10.282, 1e-3, "Negative, +Z: released");
        expectRead(s.d(n2, 4) * 1e3, -3.809, 1e-3, "Gy");
    }
    {
        const Fixture f = springCantilever(core::SupportSense::Positive, core::LoadDirection::PlusZ, n2);
        const Solved s = solveCombinations(f.project, {{1.0, "PP"}});
        expectRead(s.d(n2, 2) * 1e3, 5.070, 1e-3, "Positive, +Z: active");
        expectRead(s.d(n2, 4) * 1e3, -1.878, 1e-3, "Gy");
    }
}

// --- 10.1: the released spring leaves a mechanism: message of the original for the non-linear combination ---
TEST(FemConstraints, SpringSenseMechanism) {
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
    const Solved s = solveCombinations(f.project, {{1.0, "PP"}});
    EXPECT_EQ(s.c().status, SolveStatus::Mechanism);
    EXPECT_EQ(s.c().message, "No es posible calcular el caso de carga '[1] PP'. La estructura es un mecanismo. Revise la definición de cargas "
                             "y las condiciones de vinculación del modelo.");
    EXPECT_EQ(nonConvergenceMessage("[1] PP"),
              "Con las condiciones impuestas, no se alcanza la convergencia para el caso de carga '[1] PP'. Revise las condiciones del modelo.");
}

// #328573: an active set that does not settle within the iteration limit (one iteration: the set changes after the first solve).
TEST(FemConstraints, SpringSenseNotConverged) {
    int n2 = 0;
    const Fixture f = springCantilever(core::SupportSense::Positive, core::LoadDirection::MinusZ, n2);
    const FrameBuild b = build(f.project);
    const std::vector<double> coef{1.0};
    const std::vector<LoadCaseInput> cases{combineLoadCases(b.cases, coef, 1, "[1] PP")};
    const LinearResult r = solveNonlinear(b.model, cases, {}, 1);
    EXPECT_EQ(r.status, SolveStatus::NotConverged);
    EXPECT_EQ(r.cases[0].status, SolveStatus::NotConverged);
    EXPECT_EQ(r.cases[0].message, nonConvergenceMessage("[1] PP"));
    EXPECT_EQ(solveNonlinear(b.model, cases, {}, 2).status, SolveStatus::Ok);
}

// --- 10.2: rigid diaphragm with three columns and a diaphragm load Fx = 30, Fy = 20 kN at (1, 2) ---
TEST(FemConstraints, RigidDiaphragmThreeColumns) {
    Fixture f = makeFixture();
    const int n1 = addNode(f, 0, 0, 0), n2 = addNode(f, 0, 0, 4);
    const int n3 = addNode(f, 6, 0, 0), n4 = addNode(f, 6, 0, 4);
    const int n5 = addNode(f, 6, 3, 0), n6 = addNode(f, 6, 3, 4);
    addBar(f, n1, n2);
    addBar(f, n3, n4);
    addBar(f, n5, n6);
    clamp(f, n1);
    clamp(f, n3);
    clamp(f, n5);
    core::Diaphragm d;
    d.id = f.project.diaphragms.nextId++;
    d.reference = "D1";
    d.nodeIds = {n2, n4, n6};
    d.loads.push_back(core::DiaphragmLoad{0, 30.0, 20.0, 1.0, 2.0});
    f.project.diaphragms.items.push_back(d);
    const Solved s = solve(f.project);
    struct Row { int node; double dx, dy, gx, gy, gz; };
    for (const Row& w : {Row{n2, 17.549, 18.422, -6.825, 6.541, -2.892}, Row{n4, 17.549, 1.071, -0.397, 6.541, -2.892},
                         Row{n6, 26.225, 1.071, -0.397, 9.775, -2.892}}) {
        expectRead(s.d(w.node, 0) * 1e3, w.dx, 1e-3, "Dx");
        expectRead(s.d(w.node, 1) * 1e3, w.dy, 1e-3, "Dy");
        expectRead(s.d(w.node, 3) * 1e3, w.gx, 1e-3, "Gx (free)");
        expectRead(s.d(w.node, 4) * 1e3, w.gy, 1e-3, "Gy (free)");
        expectRead(s.d(w.node, 5) * 1e3, w.gz, 1e-3, "Gz");
        EXPECT_NEAR(s.d(w.node, 2) * 1e3, 0.0, 1e-3) << "Dz";
    }
    expectRead(s.r(n1, 0), -8.585, 1e-3, "Rx N1");
    expectRead(s.r(n1, 1), -17.916, 1e-3, "Ry N1");
    expectRead(s.r(n1, 5), 4.67, 1e-2, "Mz N1");
    expectRead(s.r(n1, 3), 71.66, 1e-2, "Mx N1");
    expectRead(s.r(n1, 4), -34.34, 1e-2, "My N1");
    expectRead(s.r(n3, 1), -1.042, 1e-3, "Ry N3");
    expectRead(s.r(n5, 0), -12.829, 1e-3, "Rx N5");
    expectRead(s.r(n5, 4), -51.32, 1e-2, "My N5");
    EXPECT_NEAR(s.r(n1, 0) + s.r(n3, 0) + s.r(n5, 0), -30.0, 1e-6);
    EXPECT_NEAR(s.r(n1, 1) + s.r(n3, 1) + s.r(n5, 1), -20.0, 1e-6);
    EXPECT_TRUE(s.result.lockedDofs.empty());

    // The master (first node) does not change the results [REA 2.6]: N6 as master.
    FrameModel other = s.build.model;
    other.diaphragms.front().nodes = {n6, n2, n4};
    const LinearResult r2 = solveLinear(other, s.build.cases);
    ASSERT_EQ(r2.status, SolveStatus::Ok);
    for (std::size_t i = 0; i < other.nodes.size(); ++i)
        for (int k = 0; k < 6; ++k) EXPECT_NEAR(r2.cases[0].displacements[i][k], s.c().displacements[i][k], 1e-12);
}

TEST(FemConstraints, DiaphragmMustBeHorizontal) {
    FrameModel m;
    for (int i = 0; i < 2; ++i) {
        FemNode n;
        n.id = i + 1;
        n.position = {0.0, 0.0, i * 0.01};
        n.fixed = {true, true, true, true, true, true};
        m.nodes.push_back(n);
    }
    m.diaphragms.push_back(FemDiaphragm{1, {1, 2}});
    const LinearResult r = solveLinear(m, std::vector<LoadCaseInput>{LoadCaseInput{}});
    EXPECT_EQ(r.status, SolveStatus::Error);
    EXPECT_EQ(r.message, "Los nudos del diafragma 1 no están a la misma cota.");
}

// --- 10.3: ligature Dz between the tips of two cantilevers loaded with 10 and 20 kN ---
TEST(FemConstraints, LigatureEqualDz) {
    Fixture f = makeFixture();
    const int n1 = addNode(f, 0, 0, 0), n2 = addNode(f, 4, 0, 0);
    const int n3 = addNode(f, 0, 3, 0), n4 = addNode(f, 4, 3, 0);
    addBar(f, n1, n2);
    addBar(f, n3, n4);
    clamp(f, n1);
    clamp(f, n3);
    f.project.ligatures.dirs[2].push_back(core::Ligature{f.project.ligatures.nextId++, {n2, n4}});
    nodalLoad(f, n2, core::LoadDirection::MinusZ, 10.0);
    nodalLoad(f, n4, core::LoadDirection::MinusZ, 20.0);
    const Solved s = solve(f.project);
    expectRead(s.d(n2, 2) * 1e3, -15.424, 1e-3, "Dz N2");
    expectRead(s.d(n4, 2) * 1e3, -15.424, 1e-3, "Dz N4");
    expectRead(s.d(n2, 4) * 1e3, 5.714, 1e-3, "Gy N2");
    expectRead(s.d(n4, 4) * 1e3, 5.714, 1e-3, "Gy N4");
    expectRead(s.r(n1, 2), 15.000, 1e-3, "Rz N1");
    expectRead(s.r(n3, 2), 15.000, 1e-3, "Rz N3");
    expectRead(s.r(n1, 4), -60.00, 1e-2, "My N1");
    EXPECT_EQ(s.c().reactions.count(n2), 0u);  // a tied node is not a support
}

// A ligature with a blocked node: the free node becomes the slave and the support receives the transmitted force.
TEST(FemConstraints, LigatureToBlockedNodeTransmitsReaction) {
    FrameModel m;
    const auto node = [&](int id, double x, bool fix) {
        FemNode n;
        n.id = id;
        n.position = {x, 0.0, 0.0};
        if (fix) n.fixed = {true, true, true, true, true, true};
        m.nodes.push_back(n);
    };
    node(1, 0.0, true);
    node(2, 4.0, false);
    node(3, 8.0, true);  // no bars: only the tie reaches it
    FemBar b;
    b.id = 1;
    b.nodeI = 1;
    b.nodeJ = 2;
    b.e = 2.1e8;
    b.g = 8.0769e7;
    b.section = SectionData{0.01, 1e-4, 5e-5, 8e-5, 0.0, 0.0};
    m.bars.push_back(b);
    m.equalDofs.push_back(FemEqualDof{2, 3, {false, false, true, false, false, false}});  // the slave is blocked: roles swap
    LoadCaseInput lc;
    lc.name = "H1";
    lc.nodal.push_back(FemNodalLoad{2, {0, 0, -10, 0, 0, 0}});
    const LinearResult r = solveLinear(m, std::vector<LoadCaseInput>{lc});
    ASSERT_EQ(r.status, SolveStatus::Ok) << r.message;
    EXPECT_NEAR(r.cases[0].displacements[1][2], 0.0, 1e-15);
    EXPECT_NEAR(r.cases[0].reactions.at(3)[2], 10.0, 1e-9);
    EXPECT_NEAR(r.cases[0].reactions.at(1)[2], 0.0, 1e-9);
}

// --- 10.4: oblique supports (user plane / line with vector (1, 0, 1)), rotations free ---
TEST(FemConstraints, ObliquePlaneAndLine) {
    for (const bool plane : {true, false}) {
        Fixture f = makeFixture();
        const int n1 = addNode(f, 0, 0, 0), n2 = addNode(f, 4, 0, 0);
        addBar(f, n1, n2);
        clamp(f, n1);
        core::ExternalSupport g;
        g.type = core::ExternalSupportType::Generic;
        g.generic.mode = plane ? core::GenericDisplacementMode::OnPlane : core::GenericDisplacementMode::OnLine;
        g.generic.vector = {1.0, 0.0, 1.0};
        f.project.nodeSupports.external[n2] = g;
        nodalLoad(f, n2, core::LoadDirection::MinusZ, 10.0);
        const Solved s = solve(f.project);
        ASSERT_EQ(s.build.model.nodes[1].oblique, plane ? ObliqueSupport::Plane : ObliqueSupport::Line);
        expectRead(s.d(n2, 0) * 1e3, plane ? 0.019 : -0.019, 1e-3, "Dx N2");
        expectRead(s.d(n2, 2) * 1e3, -0.019, 1e-3, "Dz N2");
        expectRead(s.r(n2, 0), plane ? 9.982 : -9.982, 1e-3, "Rx N2");
        expectRead(s.r(n2, 1), 0.0, 1e-3, "Ry N2");
        expectRead(s.r(n2, 2), 9.982, 1e-3, "Rz N2");
        for (int k = 3; k < 6; ++k) EXPECT_NEAR(s.r(n2, k), 0.0, 1e-9);
        // Exact relation: u . n = 0 (plane) / u parallel to d (line).
        if (plane) EXPECT_NEAR(s.d(n2, 0) + s.d(n2, 2), 0.0, 1e-15);
        else EXPECT_NEAR(s.d(n2, 0) - s.d(n2, 2), 0.0, 1e-15);
    }
}

// --- 10.5: internal link «empotramiento parcial para todas las piezas»: K = c / 2 between the two bars ---
TEST(FemConstraints, PartialFixityInternalLink) {
    struct Case { double c, dz3, gyOrigin, gyEnd; };
    for (const Case& k : {Case{5000.0, -70.473, 18.429, 20.572}, Case{20000.0, -43.472, 9.429, 11.572}}) {
        Fixture f = makeFixture();
        const int n1 = addNode(f, 0, 0, 0), n2 = addNode(f, 3, 0, 0), n3 = addNode(f, 6, 0, 0);
        addBar(f, n1, n2);
        addBar(f, n2, n3);
        clamp(f, n1);
        core::InternalLink link;
        link.type = core::InternalLinkType::PartialFixity;
        link.partialCoefficient = k.c;
        f.project.nodeSupports.internal[n2] = link;
        nodalLoad(f, n3, core::LoadDirection::MinusZ, 10.0);
        const Solved s = solve(f.project);
        expectRead(s.d(n2, 2) * 1e3, -10.807, 1e-3, "Dz N2");
        expectRead(s.d(n3, 2) * 1e3, k.dz3, 1e-3, "Dz N3");
        expectRead(s.c().barDisplacements[1].origin[4] * 1e3, k.gyOrigin, 1e-3, "Gy bar 2 at N2");
        expectRead(s.c().barDisplacements[1].end[4] * 1e3, k.gyEnd, 1e-3, "Gy bar 2 at N3");
        const double jump = (s.c().barDisplacements[1].origin[4] - s.c().barDisplacements[0].end[4]) * 1e3;
        expectRead(jump, 30.0 / (k.c / 2.0) * 1e3, 1e-3, "rotation jump = M / (c / 2)");
    }
}

TEST(FemConstraints, CombineLoadCasesScalesEverything) {
    LoadCaseInput a;
    a.nodal.push_back(FemNodalLoad{1, {1, 2, 3, 4, 5, 6}});
    FemBarLoad bl;
    bl.w1 = {0, 0, -2};
    bl.uniformTemperature = 10.0;
    a.bars.push_back(bl);
    a.diaphragmLoads.push_back(FemDiaphragmLoad{1, 3.0, 4.0, 1.0, 2.0});
    a.prescribed[1] = {0.001, 0, 0, 0, 0, 0};
    LoadCaseInput b;
    b.nodal.push_back(FemNodalLoad{1, {0, 0, 10, 0, 0, 0}});
    const std::vector<LoadCaseInput> cases{a, b};
    const std::vector<double> coef{1.35, 1.5};
    const LoadCaseInput c = combineLoadCases(cases, coef, 7, "[1] 1.35·PP+1.5·Q");
    EXPECT_EQ(c.id, 7);
    ASSERT_EQ(c.nodal.size(), 2u);
    EXPECT_DOUBLE_EQ(c.nodal[0].values[2], 1.35 * 3);
    EXPECT_DOUBLE_EQ(c.nodal[1].values[2], 15.0);
    EXPECT_DOUBLE_EQ(c.bars[0].w1[2], -2.7);
    EXPECT_DOUBLE_EQ(c.bars[0].uniformTemperature, 13.5);
    EXPECT_DOUBLE_EQ(c.diaphragmLoads[0].fy, 5.4);
    EXPECT_DOUBLE_EQ(c.diaphragmLoads[0].x, 1.0);
    EXPECT_DOUBLE_EQ(c.prescribed.at(1)[0], 0.00135);
}
