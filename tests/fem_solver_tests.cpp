// Linear static frame solver (T-P8-01): reference cases of docs/research/solver_lineal_portico.md section 8 and analytical checks.
#include <gtest/gtest.h>

#include <cmath>
#include <vector>

#include "core/bar_ends.h"
#include "core/fem/frame_builder.h"
#include "core/fem/linear_solver.h"
#include "core/project.h"

using namespace cc::fem;

namespace {

constexpr double kE = 210e6;                 // kN/m2
constexpr double kG = kE / (2.0 * 1.3);      // nu = 0.3
constexpr double kAlpha = 1.2e-5;

SectionData refSection() { return SectionData{0.01, 1e-4, 5e-5, 8e-5, 0.004, 0.004, 0.0, 0.0}; }

FemNode node(int id, double x, double y, double z, bool fixedAll = false) {
    FemNode n;
    n.id = id;
    n.position = {x, y, z};
    if (fixedAll) n.fixed.fill(true);
    return n;
}

/// Bar along global X (local z = global Z) or along global Z (local z = global X, local y = -Y).
FemBar bar(int id, int i, int j, bool vertical = false) {
    FemBar b;
    b.id = id;
    b.nodeI = i;
    b.nodeJ = j;
    b.e = kE;
    b.g = kG;
    b.alpha = kAlpha;
    b.section = refSection();
    if (vertical) {
        b.ex = {0, 0, 1};
        b.ey = {0, -1, 0};
        b.ez = {1, 0, 0};
    }
    return b;
}

void expectRel(double actual, double expected, double rel = 1e-6) { EXPECT_NEAR(actual, expected, rel * std::fabs(expected)) << expected; }

LinearResult solveOne(const FrameModel& m, const LoadCaseInput& lc, bool shear) {
    SolverOptions o;
    o.shearDeformation = shear;
    return solveLinear(m, std::span<const LoadCaseInput>(&lc, 1), o);
}

int nodeIdx(const FrameModel& m, int id) {
    for (std::size_t i = 0; i < m.nodes.size(); ++i)
        if (m.nodes[i].id == id) return static_cast<int>(i);
    return -1;
}

}  // namespace

// --- C1: cantilever L = 4 m, P = 10 kN along -Z at the free end ---
TEST(FemSolver, C1CantileverEulerBernoulliAndTimoshenko) {
    FrameModel m;
    m.nodes = {node(0, 0, 0, 0, true), node(1, 4, 0, 0)};
    m.bars = {bar(1, 0, 1)};
    LoadCaseInput lc{1, "Q 1", {FemNodalLoad{1, {0, 0, -10, 0, 0, 0}}}, {}, {}};

    const LinearResult eb = solveOne(m, lc, false);
    ASSERT_EQ(eb.status, SolveStatus::Ok);
    const auto& c = eb.cases[0];
    expectRel(c.displacements[1][2], -1.015873016e-2);
    expectRel(c.displacements[1][4], 3.809523810e-3);
    expectRel(c.reactions.at(0)[2], 10.0);
    expectRel(c.reactions.at(0)[4], -40.0);
    // Section forces at the clamp (convention measured in the original: canonical resultant with My inverted).
    expectRel(c.barForces[0].origin[2], -10.0);
    expectRel(c.barForces[0].origin[4], -40.0);
    EXPECT_NEAR(c.barForces[0].end[4], 0.0, 1e-9);

    const LinearResult ti = solveOne(m, lc, true);
    ASSERT_EQ(ti.status, SolveStatus::Ok);
    expectRel(ti.cases[0].displacements[1][2], -1.028253968e-2);
    expectRel(ti.cases[0].displacements[1][4], 3.809523810e-3);  // the tip rotation has no shear part
}

// --- C2: simply supported beam L = 6 m, q = 10 kN/m, 12 elements ---
namespace {
FrameModel c2Model(int elements) {
    FrameModel m;
    const double le = 6.0 / elements;
    for (int i = 0; i <= elements; ++i) m.nodes.push_back(node(i, i * le, 0, 0));
    m.nodes.front().fixed = {true, true, true, true, false, false};
    m.nodes.back().fixed = {false, true, true, false, false, false};
    for (int i = 0; i < elements; ++i) m.bars.push_back(bar(i + 1, i, i + 1));
    return m;
}
}  // namespace

TEST(FemSolver, C2SimplySupportedUniformLoadAsBarLoads) {
    const FrameModel m = c2Model(12);
    LoadCaseInput lc{1, "CM 1", {}, {}, {}};
    for (const FemBar& b : m.bars) lc.bars.push_back(FemBarLoad{b.id, BarLoadKind::Distributed, true, 0.0, 0.5, {0, 0, -10}, {0, 0, -10}});
    for (const bool shear : {false, true}) {
        const LinearResult r = solveOne(m, lc, shear);
        ASSERT_EQ(r.status, SolveStatus::Ok);
        const auto& c = r.cases[0];
        expectRel(c.displacements[6][2], shear ? -8.175e-3 : -8.035714286e-3);
        expectRel(c.reactions.at(0)[2], 30.0);
        expectRel(c.reactions.at(12)[2], 30.0);
        expectRel(c.barForces[5].end[4], 45.0);    // midspan bending moment qL^2/8 (sign measured in the original: My > 0 in the span)
        expectRel(c.barForces[6].origin[4], 45.0);
    }
}

TEST(FemSolver, C2SimplySupportedConsistentNodalLoads) {
    const FrameModel m = c2Model(12);
    const double le = 0.5, q = 10.0;
    LoadCaseInput lc{1, "CM 1", {}, {}, {}};
    for (int i = 0; i <= 12; ++i) {
        Dof6 v{0, 0, -q * le * ((i == 0 || i == 12) ? 0.5 : 1.0), 0, 0, 0};
        if (i == 0) v[4] = q * le * le / 12.0;   // equivalent end moment of the first element (about +y)
        if (i == 12) v[4] = -q * le * le / 12.0;
        lc.nodal.push_back(FemNodalLoad{i, v});
    }
    expectRel(solveOne(m, lc, false).cases[0].displacements[6][2], -8.035714286e-3);
    expectRel(solveOne(m, lc, true).cases[0].displacements[6][2], -8.175e-3);
}

TEST(FemSolver, C2SingleElementSectionForcesAlongTheBar) {
    const FrameModel m = c2Model(1);
    const std::vector<FemBarLoad> loads{FemBarLoad{1, BarLoadKind::Distributed, false, 0.0, 6.0, {0, 0, -10}, {0, 0, -10}}};
    LoadCaseInput lc{1, "CM 1", {}, loads, {}};
    const LinearResult r = solveOne(m, lc, true);
    ASSERT_EQ(r.status, SolveStatus::Ok);
    const BarEndForces& f = r.cases[0].barForces[0];
    expectRel(sectionForcesAt(m.bars[0], 6.0, f, loads, 3.0)[4], 45.0);
    EXPECT_NEAR(sectionForcesAt(m.bars[0], 6.0, f, loads, 3.0)[2], 0.0, 1e-9);
    expectRel(sectionForcesAt(m.bars[0], 6.0, f, loads, 0.0)[2], -30.0);
    expectRel(sectionForcesAt(m.bars[0], 6.0, f, loads, 1.5)[4], 30.0 * 1.5 - 10.0 * 1.5 * 0.75);
    EXPECT_NEAR(f.origin[4], 0.0, 1e-9);
}

// --- C3: plane portal frame in XZ ---
TEST(FemSolver, C3PortalFrame) {
    FrameModel m;
    m.nodes = {node(0, 0, 0, 0, true), node(1, 0, 0, 4), node(2, 6, 0, 4), node(3, 6, 0, 0, true)};
    m.bars = {bar(1, 0, 1, true), bar(2, 1, 2), bar(3, 3, 2, true)};
    LoadCaseInput lc{1, "V 1", {FemNodalLoad{1, {20, 0, 0, 0, 0, 0}}}, {}, {}};

    const LinearResult eb = solveOne(m, lc, false);
    ASSERT_EQ(eb.status, SolveStatus::Ok);
    const auto& c = eb.cases[0];
    expectRel(c.displacements[1][0], 4.083155886e-3);
    expectRel(c.displacements[2][0], 4.054654597e-3);
    expectRel(c.displacements[1][4], 7.686193445e-4);
    const Dof6 r0 = c.reactions.at(0), r3 = c.reactions.at(3);
    EXPECT_NEAR(r0[0], -10.0245, 1e-4);
    EXPECT_NEAR(r0[2], -5.3286, 1e-4);
    EXPECT_NEAR(r0[4], -24.0843, 1e-4);
    EXPECT_NEAR(r3[0], -9.9755, 1e-4);
    EXPECT_NEAR(r3[2], 5.3286, 1e-4);
    EXPECT_NEAR(r3[4], -23.9441, 1e-4);
    // Global equilibrium: forces and moments about the origin.
    EXPECT_NEAR(r0[0] + r3[0] + 20.0, 0.0, 1e-9);
    EXPECT_NEAR(r0[2] + r3[2], 0.0, 1e-9);
    EXPECT_NEAR(r0[4] + r3[4] + 20.0 * 4.0 - 6.0 * r3[2], 0.0, 1e-8);

    const LinearResult ti = solveOne(m, lc, true);
    expectRel(ti.cases[0].displacements[1][0], 4.233218287e-3);
    expectRel(ti.cases[0].displacements[2][0], 4.204715537e-3);
}

// --- global equilibrium of a 3D frame with nodal and bar loads ---
TEST(FemSolver, GlobalEquilibrium3D) {
    FrameModel m;
    m.nodes = {node(1, 0, 0, 0, true), node(2, 5, 0, 0, true), node(3, 5, 4, 0, true), node(4, 0, 4, 0, true),
               node(5, 0, 0, 3), node(6, 5, 0, 3), node(7, 5, 4, 3), node(8, 0, 4, 3)};
    const auto beam = [&](int id, int i, int j, std::array<double, 3> ex) {
        FemBar b = bar(id, i, j);
        b.ex = ex;
        b.ez = {0, 0, 1};
        b.ey = {-ex[1], ex[0], 0.0};  // ez x ex
        return b;
    };
    m.bars = {bar(1, 1, 5, true), bar(2, 2, 6, true), bar(3, 3, 7, true), bar(4, 4, 8, true),
              beam(5, 5, 6, {1, 0, 0}), beam(6, 6, 7, {0, 1, 0}), beam(7, 8, 7, {1, 0, 0}), beam(8, 5, 8, {0, 1, 0})};
    LoadCaseInput lc{1, "Q 1", {}, {}, {}};
    lc.nodal.push_back(FemNodalLoad{7, {3, -2, -15, 1, 2, -3}});
    lc.bars.push_back(FemBarLoad{5, BarLoadKind::Distributed, true, 1.0, 4.0, {0, 1, -4}, {0, 2, -8}});
    lc.bars.push_back(FemBarLoad{6, BarLoadKind::PointForce, false, 1.0, 1.0, {0.5, 0, -12}, {}});
    lc.bars.push_back(FemBarLoad{8, BarLoadKind::PointMoment, false, 2.0, 2.0, {0, 3, 0}, {}});

    // Applied resultant (global), moments about the origin.
    double F[3] = {3, -2, -15}, M[3] = {1, 2, -3};
    const auto addForce = [&](std::array<double, 3> p, std::array<double, 3> f) {
        for (int k = 0; k < 3; ++k) F[k] += f[k];
        M[0] += p[1] * f[2] - p[2] * f[1];
        M[1] += p[2] * f[0] - p[0] * f[2];
        M[2] += p[0] * f[1] - p[1] * f[0];
    };
    // Moment of the nodal force at node 7 (5, 4, 3) about the origin.
    M[0] += 4 * -15 - 3 * -2;
    M[1] += 3 * 3 - 5 * -15;
    M[2] += 5 * -2 - 4 * 3;
    // Trapezoid on bar 5 (x from 1 to 4 along X at y = 0, z = 3): resultant and centroid of each component.
    {
        const double len = 3.0, w1y = 1, w2y = 2, w1z = -4, w2z = -8;
        const double ry = 0.5 * len * (w1y + w2y), rz = 0.5 * len * (w1z + w2z);
        const double cy = 1.0 + len * (w1y + 2 * w2y) / (3 * (w1y + w2y)), cz = 1.0 + len * (w1z + 2 * w2z) / (3 * (w1z + w2z));
        addForce({cy, 0, 3}, {0, ry, 0});
        addForce({cz, 0, 3}, {0, 0, rz});
    }
    // Bar 6 runs along +Y from (5,0,3): local x = +Y, local y = ez x ex = -X, local z = +Z. Local (0.5, 0, -12) at 1 m.
    addForce({5, 1, 3}, {0, 0.5, -12});
    // Bar 8 runs along +Y from node 5 (0, 0, 3); local y = -X: a moment of 3 about local y is -3 about global X.
    M[0] += -3;

    const LinearResult r = solveOne(m, lc, true);
    ASSERT_EQ(r.status, SolveStatus::Ok);
    double RF[3] = {0, 0, 0}, RM[3] = {0, 0, 0};
    for (const auto& [id, rv] : r.cases[0].reactions) {
        const auto p = m.nodes[nodeIdx(m, id)].position;
        for (int k = 0; k < 3; ++k) { RF[k] += rv[k]; RM[k] += rv[3 + k]; }
        RM[0] += p[1] * rv[2] - p[2] * rv[1];
        RM[1] += p[2] * rv[0] - p[0] * rv[2];
        RM[2] += p[0] * rv[1] - p[1] * rv[0];
    }
    for (int k = 0; k < 3; ++k) {
        EXPECT_NEAR(RF[k] + F[k], 0.0, 1e-8) << k;
        EXPECT_NEAR(RM[k] + M[k], 0.0, 1e-7) << k;
    }
}

// --- mechanism detection ---
TEST(FemSolver, MechanismIsDetectedWithTheMessageOfTheOriginal) {
    FrameModel m;
    m.nodes = {node(0, 0, 0, 0), node(1, 5, 0, 0)};
    // Torsion locked at the start: the free twist of a collinear chain is NOT a mechanism (it is fixed to 0, TK-108), the rotation about Y is.
    m.nodes[0].fixed = {true, true, true, true, false, false};
    m.nodes[1].fixed = {false, true, false, false, false, false};  // Dz of the far end free: pinned bar with only Dy there, rotation about Y is a mechanism
    m.bars = {bar(1, 0, 1)};
    LoadCaseInput lc{3, "CM 1", {FemNodalLoad{1, {0, 0, -1, 0, 0, 0}}}, {}, {}};
    const LinearResult r = solveOne(m, lc, false);
    EXPECT_EQ(r.status, SolveStatus::Mechanism);
    EXPECT_EQ(r.cases[0].status, SolveStatus::Mechanism);
    // The user sees the warning measured in the original (null pivot: DOF type + node coordinates), not the #214001 text.
    ASSERT_GE(r.mechanismNode, 0);
    ASSERT_GE(r.mechanismDof, 0);
    EXPECT_EQ(r.cases[0].message, r.message);
    EXPECT_EQ(r.message, mechanismWarning(r.mechanismDof, m.nodes[nodeIdx(m, r.mechanismNode)].position));
    EXPECT_EQ(mechanismMessage("CM 1"),
              "No es posible calcular el caso de carga 'CM 1'. La estructura es un mecanismo. Revise la definición de cargas y las condiciones "
              "de vinculación del modelo.");
    EXPECT_EQ(unstableMessage("Q 2"),
              "No es posible calcular el caso de carga 'Q 2', ya que la estructura es inestable. Revise la definición de cargas y las condiciones "
              "de contorno introducidas.");
}

TEST(FemSolver, LoadedDofWithoutStiffnessIsAMechanismOnlyForThatCase) {
    FrameModel m;
    m.nodes = {node(0, 0, 0, 0, true), node(1, 4, 0, 0), node(9, 10, 10, 10)};  // node 9: no bars
    m.bars = {bar(1, 0, 1)};
    std::vector<LoadCaseInput> cases{{1, "A", {FemNodalLoad{1, {0, 0, -10, 0, 0, 0}}}, {}, {}},
                                     {2, "B", {FemNodalLoad{9, {0, 0, -10, 0, 0, 0}}}, {}, {}}};
    const LinearResult r = solveLinear(m, cases);
    EXPECT_EQ(r.cases[0].status, SolveStatus::Ok);
    EXPECT_EQ(r.cases[1].status, SolveStatus::Mechanism);
    EXPECT_EQ(r.cases[1].message, mechanismMessage("B"));
    EXPECT_EQ(r.lockedDofs.count(9), 1u);
}

TEST(FemSolver, InvalidSectionIsAnError) {
    FrameModel m;
    m.nodes = {node(0, 0, 0, 0, true), node(1, 4, 0, 0)};
    m.bars = {bar(1, 0, 1)};
    m.bars[0].section.iy = 0.0;
    LoadCaseInput lc{1, "A", {}, {}, {}};
    const LinearResult r = solveOne(m, lc, false);
    EXPECT_EQ(r.status, SolveStatus::Error);
    EXPECT_EQ(r.cases[0].message, caseErrorMessage("A"));
}

// --- releases ---
TEST(FemSolver, InternalHingeByEndRelease) {
    for (const bool both : {false, true}) {
        FrameModel m;
        m.nodes = {node(0, 0, 0, 0, true), node(1, 3, 0, 0), node(2, 6, 0, 0, true)};
        m.bars = {bar(1, 0, 1), bar(2, 1, 2)};
        m.bars[0].releases[1][1] = m.bars[0].releases[1][2] = EndRelease{EndRelease::Kind::Free, 0.0};
        if (both) m.bars[1].releases[0][1] = m.bars[1].releases[0][2] = EndRelease{EndRelease::Kind::Free, 0.0};
        LoadCaseInput lc{1, "A", {FemNodalLoad{1, {0, 0, -10, 0, 0, 0}}}, {}, {}};
        const LinearResult r = solveOne(m, lc, false);
        ASSERT_EQ(r.status, SolveStatus::Ok);
        const auto& c = r.cases[0];
        expectRel(c.displacements[1][2], -10.0 * 27.0 / (6.0 * kE * 1e-4));  // two cantilevers of 3 m in parallel
        EXPECT_NEAR(c.barForces[0].end[4], 0.0, 1e-8);
        EXPECT_NEAR(c.barForces[0].end[5], 0.0, 1e-8);
        expectRel(c.reactions.at(0)[2], 5.0);
        EXPECT_EQ(r.lockedDofs.count(1), both ? 1u : 0u);
        if (both) {
            EXPECT_TRUE(r.lockedDofs.at(1)[4]);
            EXPECT_TRUE(r.lockedDofs.at(1)[5]);
            EXPECT_FALSE(r.lockedDofs.at(1)[3]);  // torsion stays connected
        }
    }
}

TEST(FemSolver, RotationalSpringAtTheClamp) {
    FrameModel m;
    m.nodes = {node(0, 0, 0, 0, true), node(1, 4, 0, 0)};
    m.bars = {bar(1, 0, 1)};
    m.bars[0].releases[0][1] = EndRelease{EndRelease::Kind::Spring, 1e4};
    LoadCaseInput lc{1, "A", {FemNodalLoad{1, {0, 0, -10, 0, 0, 0}}}, {}, {}};
    for (const bool shear : {false, true}) {
        const LinearResult r = solveOne(m, lc, shear);
        ASSERT_EQ(r.status, SolveStatus::Ok);
        const double expected = -(1.015873016e-2 + 10.0 * 16.0 / 1e4 + (shear ? 4.0 * 10.0 / (kG * 0.004) : 0.0));
        expectRel(r.cases[0].displacements[1][2], expected);
        expectRel(r.cases[0].barForces[0].origin[4], -40.0);
    }
}

// --- exact fixed-end forces with shear deformation ---
TEST(FemSolver, OffCentrePointAndPartialTrapezoidMatchARefinedMesh) {
    for (const bool shear : {false, true}) {
        // Point load at 1.5 m of a 6 m clamped-clamped beam: one element vs two elements with a node at the load.
        FrameModel one;
        one.nodes = {node(0, 0, 0, 0, true), node(1, 6, 0, 0, true)};
        one.bars = {bar(1, 0, 1)};
        LoadCaseInput l1{1, "A", {}, {FemBarLoad{1, BarLoadKind::PointForce, false, 1.5, 1.5, {2, 3, -10}, {}}}, {}};
        FrameModel two;
        two.nodes = {node(0, 0, 0, 0, true), node(2, 1.5, 0, 0), node(1, 6, 0, 0, true)};
        two.bars = {bar(1, 0, 2), bar(2, 2, 1)};
        LoadCaseInput l2{1, "A", {FemNodalLoad{2, {2, 3, -10, 0, 0, 0}}}, {}, {}};
        const auto a = solveOne(one, l1, shear).cases[0], b = solveOne(two, l2, shear).cases[0];
        for (int k = 0; k < 6; ++k) {
            EXPECT_NEAR(a.reactions.at(0)[k], b.reactions.at(0)[k], 1e-9) << k;
            EXPECT_NEAR(a.reactions.at(1)[k], b.reactions.at(1)[k], 1e-9) << k;
        }
        // Partial trapezoid 1..5 m, -5 -> -15 kN/m: one element vs four elements split at 1, 3 and 5 m.
        LoadCaseInput t1{1, "A", {}, {FemBarLoad{1, BarLoadKind::Distributed, false, 1.0, 5.0, {0, 0, -5}, {0, 0, -15}}}, {}};
        FrameModel four;
        four.nodes = {node(0, 0, 0, 0, true), node(2, 1, 0, 0), node(3, 3, 0, 0), node(4, 5, 0, 0), node(1, 6, 0, 0, true)};
        four.bars = {bar(1, 0, 2), bar(2, 2, 3), bar(3, 3, 4), bar(4, 4, 1)};
        LoadCaseInput t4{1, "A", {}, {FemBarLoad{2, BarLoadKind::Distributed, false, 0.0, 2.0, {0, 0, -5}, {0, 0, -10}},
                                      FemBarLoad{3, BarLoadKind::Distributed, false, 0.0, 2.0, {0, 0, -10}, {0, 0, -15}}}, {}};
        const auto c = solveOne(one, t1, shear).cases[0], d = solveOne(four, t4, shear).cases[0];
        for (int k = 0; k < 6; ++k) {
            EXPECT_NEAR(c.reactions.at(0)[k], d.reactions.at(0)[k], 1e-9) << k;
            EXPECT_NEAR(c.reactions.at(1)[k], d.reactions.at(1)[k], 1e-9) << k;
        }
        EXPECT_NEAR(c.reactions.at(0)[2] + c.reactions.at(1)[2], 40.0, 1e-9);
    }
}

// --- temperature ---
TEST(FemSolver, TemperatureLoads) {
    // Restrained bar, uniform increment: N = -E A alpha dT.
    FrameModel fixedBoth;
    fixedBoth.nodes = {node(0, 0, 0, 0, true), node(1, 4, 0, 0, true)};
    fixedBoth.bars = {bar(1, 0, 1)};
    FemBarLoad t{1, BarLoadKind::Temperature, false, 0, 0, {}, {}, 10.0, 0.0, 0.0};
    LoadCaseInput lc{1, "T 1", {}, {t}, {}};
    auto r = solveOne(fixedBoth, lc, true);
    ASSERT_EQ(r.status, SolveStatus::Ok);
    expectRel(r.cases[0].barForces[0].origin[0], -kE * 0.01 * kAlpha * 10.0);
    expectRel(r.cases[0].barForces[0].end[0], -kE * 0.01 * kAlpha * 10.0);
    expectRel(r.cases[0].reactions.at(0)[0], 252.0);
    expectRel(r.cases[0].reactions.at(1)[0], -252.0);

    // Free cantilever: uniform increment elongates it, a gradient bends it without forces.
    FrameModel cant;
    cant.nodes = {node(0, 0, 0, 0, true), node(1, 4, 0, 0)};
    cant.bars = {bar(1, 0, 1)};
    cant.bars[0].section.depthZ = 0.2;
    cant.bars[0].section.depthY = 0.1;
    LoadCaseInput lu{1, "T 1", {}, {t}, {}};
    expectRel(solveOne(cant, lu, true).cases[0].displacements[1][0], kAlpha * 10.0 * 4.0);
    FemBarLoad g{1, BarLoadKind::Temperature, false, 0, 0, {}, {}, 0.0, 0.0, 20.0};
    LoadCaseInput lg{1, "T 1", {}, {g}, {}};
    r = solveOne(cant, lg, true);
    const double kappa = kAlpha * 20.0 / 0.2;
    expectRel(r.cases[0].displacements[1][4], kappa * 4.0);
    expectRel(r.cases[0].displacements[1][2], -kappa * 16.0 / 2.0);
    for (int k = 0; k < 6; ++k) EXPECT_NEAR(r.cases[0].reactions.at(0)[k], 0.0, 1e-8);
    FemBarLoad gy{1, BarLoadKind::Temperature, false, 0, 0, {}, {}, 0.0, 20.0, 0.0};
    LoadCaseInput lgy{1, "T 1", {}, {gy}, {}};
    r = solveOne(cant, lgy, true);
    expectRel(r.cases[0].displacements[1][1], -(kAlpha * 20.0 / 0.1) * 16.0 / 2.0);  // hotter +y face bends towards -y
}

// --- prescribed displacement ---
TEST(FemSolver, PrescribedSettlementOfAClampedBeam) {
    FrameModel m;
    m.nodes = {node(0, 0, 0, 0, true), node(1, 6, 0, 0, true)};
    m.bars = {bar(1, 0, 1)};
    LoadCaseInput lc{1, "A", {}, {}, {{1, Dof6{0, 0, -0.01, 0, 0, 0}}}};
    const auto r = solveOne(m, lc, false);
    ASSERT_EQ(r.status, SolveStatus::Ok);
    expectRel(r.cases[0].displacements[1][2], -0.01);
    expectRel(std::fabs(r.cases[0].reactions.at(0)[2]), 12.0 * kE * 1e-4 * 0.01 / 216.0);
    expectRel(std::fabs(r.cases[0].reactions.at(0)[4]), 6.0 * kE * 1e-4 * 0.01 / 36.0);
    EXPECT_NEAR(r.cases[0].reactions.at(0)[2] + r.cases[0].reactions.at(1)[2], 0.0, 1e-9);
}

// --- the project adapter ---
namespace {
const FrameBuildOptions kHonour{true};  // the tests below switch the self weight off per bar

cc::core::Project cantileverProject(double rollDeg = 0.0, bool inverted = false) {
    cc::core::Project p;
    auto& s = p.structure;
    const int n1 = s.addNode({0, 0, 0}), n2 = s.addNode({4, 0, 0});
    cc::core::Material mat;
    mat.name = "S275";
    mat.kind = cc::core::MaterialKind::Steel;
    const int mid = s.addMaterial(mat);
    cc::core::CrossSection cs;
    cs.name = "ref";
    cs.areaM2 = 0.01;
    cs.inertiaYM4 = 1e-4;
    cs.inertiaZM4 = 5e-5;
    cs.torsionM4 = 8e-5;
    cs.shearAreaYM2 = cs.shearAreaZM2 = 100.0;  // 1e6 cm2, the largest admitted by the original: Euler-Bernoulli to 1e-7 (area 0 is refused)
    const int sid = s.addSection(cs);
    const int b = s.addBar(n1, n2, mid, sid);
    s.findBar(b)->rollAngleDeg = rollDeg;
    s.findBar(b)->xAxisInverted = inverted;
    cc::core::ExternalSupport fixed;
    fixed.type = cc::core::ExternalSupportType::Fixed;
    p.nodeSupports.external[n1] = fixed;
    return p;
}
}  // namespace

TEST(FemBuilder, SelfWeightOfTheLoadCasePesoPropio) {
    cc::core::Project p = cantileverProject();
    FrameBuild fb = buildFrameModel(p);
    ASSERT_TRUE(fb.errors.empty());
    ASSERT_TRUE(fb.unsupported.empty());
    ASSERT_EQ(fb.cases.size(), 1u);
    EXPECT_EQ(fb.cases[0].id, 0);
    auto r = solveLinear(fb.model, fb.cases);
    ASSERT_EQ(r.status, SolveStatus::Ok);
    const double w = 77.01 * 0.01;
    expectRel(r.cases[0].reactions.at(1)[2], w * 4.0);
    expectRel(r.cases[0].reactions.at(1)[4], -w * 16.0 / 2.0);
    expectRel(r.cases[0].displacements[1][2], -w * 256.0 / (8.0 * kE * 1e-4));

    // Measured in the original (solver_medidas_original.md 10.1): switching the generation of a bar off removes its self weight.
    p.barLoads.selfWeightOff.insert(1);
    fb = buildFrameModel(p);
    r = solveLinear(fb.model, fb.cases);
    EXPECT_NEAR(r.cases[0].reactions.at(1)[2], 0.0, 1e-12);
    FrameBuildOptions ignore;
    ignore.honourSelfWeightSwitch = false;
    fb = buildFrameModel(p, ignore);
    r = solveLinear(fb.model, fb.cases);
    expectRel(r.cases[0].reactions.at(1)[2], w * 4.0);
}

TEST(FemBuilder, RollAngleBendsAboutLocalZ) {
    cc::core::Project p = cantileverProject(90.0);
    p.barLoads.selfWeightOff.insert(1);
    cc::core::NodalLoad nl;
    nl.id = 1;
    nl.nodeId = 2;
    nl.loadCaseId = 0;
    nl.magnitude = 10.0;
    nl.direction = cc::core::LoadDirection::MinusZ;
    p.nodalLoads.items.push_back(nl);
    p.nodalLoads.nextId = 2;
    const FrameBuild fb = buildFrameModel(p, kHonour);
    const auto r = solveLinear(fb.model, fb.cases);
    ASSERT_EQ(r.status, SolveStatus::Ok);
    expectRel(r.cases[0].displacements[1][2], -10.0 * 64.0 / (3.0 * kE * 5e-5));
    // Without roll the strong axis works.
    cc::core::Project q = cantileverProject(0.0);
    q.barLoads.selfWeightOff.insert(1);
    q.nodalLoads = p.nodalLoads;
    const FrameBuild fq = buildFrameModel(q, kHonour);
    expectRel(solveLinear(fq.model, fq.cases).cases[0].displacements[1][2], -10.0 * 64.0 / (3.0 * kE * 1e-4));
}

TEST(FemBuilder, PartialFixityCoefficientAndPinnedEnd) {
    cc::core::Project p = cantileverProject();
    p.barLoads.selfWeightOff.insert(1);
    cc::core::NodalLoad nl{1, 2, 0, 10.0, cc::core::LoadDirection::MinusZ, cc::core::kDefaultUserVector};
    p.nodalLoads.items.push_back(nl);
    cc::core::BarEndsRecord rec;
    rec.ends[cc::core::kBarOrigin].planes[cc::core::kPlaneXZ].coefficient = 0.5;
    cc::core::setBarEnds(p.barEnds, 1, rec);
    FrameBuild fb = buildFrameModel(p, kHonour);
    auto r = solveLinear(fb.model, fb.cases);
    ASSERT_EQ(r.status, SolveStatus::Ok);
    const double k = 4.0 * kE * 1e-4 / 4.0;
    expectRel(r.cases[0].displacements[1][2], -(1.015873016e-2 + 10.0 * 16.0 / k));

    // Pinned clamp end: the cantilever becomes a mechanism.
    rec = {};
    rec.ends[cc::core::kBarOrigin].pinned = true;
    cc::core::setBarEnds(p.barEnds, 1, rec);
    fb = buildFrameModel(p, kHonour);
    r = solveLinear(fb.model, fb.cases);
    EXPECT_EQ(r.cases[0].status, SolveStatus::Mechanism);
    EXPECT_EQ(r.cases[0].message, r.message);
    EXPECT_EQ(r.message.rfind("Esta estructura es un mecanismo. Revise los datos introducidos.\n", 0), 0u);  // warning measured in the original
}

TEST(FemBuilder, InvertedAxisKeepsLoadPositionsFromTheStartNode) {
    const auto reactions = [](bool inverted) {
        cc::core::Project p = cantileverProject(0.0, inverted);
        p.barLoads.selfWeightOff.insert(1);
        cc::core::ExternalSupport fixed;
        fixed.type = cc::core::ExternalSupportType::Fixed;
        p.nodeSupports.external[2] = fixed;
        cc::core::BarLoad bl;
        bl.id = 1;
        bl.barId = 1;
        bl.loadCaseId = 0;
        bl.type = cc::core::BarLoadType::Point;
        bl.start = 0.25;
        bl.value1 = 10.0;
        p.barLoads.items.push_back(bl);
        cc::core::BarLoad tr = bl;
        tr.id = 2;
        tr.type = cc::core::BarLoadType::Trapezoidal;
        tr.start = 0.1;
        tr.end = 0.6;
        tr.value1 = 2.0;
        tr.value2 = 6.0;
        p.barLoads.items.push_back(tr);
        const FrameBuild fb = buildFrameModel(p, kHonour);
        return solveLinear(fb.model, fb.cases).cases[0].reactions;
    };
    const auto a = reactions(false), b = reactions(true);
    for (const int n : {1, 2})
        for (int k = 0; k < 6; ++k) EXPECT_NEAR(a.at(n)[k], b.at(n)[k], 1e-9) << n << " " << k;
    EXPECT_GT(std::fabs(a.at(1)[2]), std::fabs(a.at(2)[2]));  // the point load is nearer the start node
}

TEST(FemBuilder, PrescribedDisplacementInMillimetres) {
    cc::core::Project p = cantileverProject();
    p.barLoads.selfWeightOff.insert(1);
    cc::core::ExternalSupport fixed;
    fixed.type = cc::core::ExternalSupportType::Fixed;
    fixed.prescribed.push_back(cc::core::PrescribedDisplacement{0, {0, 0, -10.0, 0, 0, 0}});
    p.nodeSupports.external[2] = fixed;
    const FrameBuild fb = buildFrameModel(p, kHonour);
    const auto r = solveLinear(fb.model, fb.cases);
    ASSERT_EQ(r.status, SolveStatus::Ok);
    expectRel(r.cases[0].displacements[1][2], -0.01);
    expectRel(std::fabs(r.cases[0].reactions.at(1)[2]), 12.0 * kE * 1e-4 * 0.01 / 64.0, 1e-5);  // Av = 1e6 cm2: phi ~ 2e-8 (the original has no Av = 0)
}

TEST(FemBuilder, VariableDepthSectionIsTheAverageOfBothEnds) {
    cc::core::Project p = cantileverProject();
    cc::core::BarDescription d = cc::core::defaultDescription(cc::core::BarMaterialIcon::Concrete);
    d.id = 1;
    d.inlineSection = cc::core::Section{"var", cc::core::ShapeKind::ConcreteRectangularVariable, {0.3, 0.4, 0.8}, {}};
    d.sectionId = p.structure.bars[0].sectionId;
    d.materialId = p.structure.bars[0].materialId;
    p.barDescriptions.items.push_back(d);
    p.barDescriptions.next = 2;
    const auto p0 = cc::core::propertiesAt(*d.inlineSection, 0.0), p1 = cc::core::propertiesAt(*d.inlineSection, 1.0),
               pm = cc::core::propertiesAt(*d.inlineSection, 0.5);
    ASSERT_TRUE(p0 && p1 && pm);
    const FrameBuild fb = buildFrameModel(p, kHonour);
    ASSERT_EQ(fb.model.bars.size(), 1u);
    const SectionData& s = fb.model.bars[0].section;
    expectRel(s.area, 0.5 * (p0->area + p1->area));
    expectRel(s.iy, 0.5 * (p0->iy + p1->iy));
    expectRel(s.depthZ, 0.5 * (p0->zNeg + p0->zPos + p1->zNeg + p1->zPos));
    EXPECT_GT(std::fabs(s.iy - pm->iy), 1e-6 * pm->iy);  // the original's average, not the section at mid-length
    expectRel(s.area, 0.3 * 0.6);

    // Without a known depth (bare CrossSection) a temperature gradient is reported, not silently dropped.
    cc::core::Project q = cantileverProject();
    cc::core::BarLoad t;
    t.id = 1;
    t.barId = 1;
    t.loadCaseId = 0;
    t.type = cc::core::BarLoadType::VariableTemperature;
    t.axes = cc::core::BarLoadAxes::Local;
    t.value1 = 20.0;
    t.value2 = 0.0;
    q.barLoads.items.push_back(t);
    EXPECT_FALSE(buildFrameModel(q, kHonour).warnings.empty());
}

TEST(FemBuilder, ReportsUndescribedBarsAndUnsupportedData) {
    cc::core::Project p = cantileverProject();
    p.structure.addBar(1, p.structure.addNode({0, 0, 3}));
    cc::core::ExternalSupport elastic;
    elastic.type = cc::core::ExternalSupportType::Generic;
    elastic.generic.translations = {cc::core::DofMode::Elastic, cc::core::DofMode::Fixed, cc::core::DofMode::Fixed};
    p.nodeSupports.external[2] = elastic;
    const FrameBuild fb = buildFrameModel(p, kHonour);
    ASSERT_FALSE(fb.errors.empty());
    EXPECT_EQ(fb.errors.front(), "Existen barras sin describir.");
    EXPECT_TRUE(fb.unsupported.empty());  // elastic supports are modelled since T-P8-03
    EXPECT_DOUBLE_EQ(fb.model.nodes[1].spring[0], cc::core::kDefaultElasticConstant);
    EXPECT_TRUE(fb.model.nodes[1].fixed[1] && !fb.model.nodes[1].fixed[0]);
    // Internal links not measured in the original (pinned / generic) stay unsupported.
    cc::core::InternalLink pinned;
    pinned.type = cc::core::InternalLinkType::Pinned;
    p.nodeSupports.internal[1] = pinned;
    EXPECT_FALSE(buildFrameModel(p, kHonour).unsupported.empty());
    p.nodeSupports.internal.clear();
    p.structure.addNode({9, 9, 9});
    EXPECT_EQ(buildFrameModel(p, kHonour).warnings.front(), unconnectedNodeMessage());
}
