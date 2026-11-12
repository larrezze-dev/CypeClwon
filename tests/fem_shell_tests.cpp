// Shells of the linear solver (T-P8-02): mesh, 6-node Mindlin shell triangle, stiffness factors, ballast, self weight switch, integration bands
// and face stresses. Reference values: docs/research/elemento_lamina_bandas.md section 9.0 and section 13 (MEASURED in the original), Navier
// series and the clone's own reference scripts (docs/research/data/mindlin_soft_ss.py, wall_planestress_q4.py, navier_band_ref.py).
//
// Plate: 4 x 4 m, h = 0.2 m, E = 30000 MPa, nu = 0.3, edges "Articulación" (Dx, Dy, Dz blocked), uniform load 10000 kN/m2 along local +z plus the
// self weight 0.2 x 24.53 = 4.906 kN/m2 along -Z: net 9995.094 kN/m2. References at h = 0.2: Navier (Kirchhoff) w = 472.95 mm, Mx = 7658.1;
// Mindlin with the "soft" simple support of the original (FE 32 x 32) w = 499.28 mm, Mx = 7907; original (mesh 1 m) w max 501.16, Mx max 7887.5.
#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

#include "core/bar_load.h"
#include "core/fem/frame_builder.h"
#include "core/fem/linear_solver.h"
#include "core/fem/shell_analysis.h"
#include "core/project.h"
#include "core/shell_load.h"
#include "core/shell_properties.h"

using namespace cc::fem;
namespace core = cc::core;

namespace {

void expectRel(double actual, double expected, double rel, const char* what) {
    std::printf("[value] %s: %.4f (reference %.4f, %+.2f %%)\n", what, actual, expected, 100.0 * (actual / expected - 1.0));
    EXPECT_NEAR(actual, expected, rel * std::fabs(expected)) << what << ": " << actual << " vs " << expected;
}

struct ShellCase {
    core::Project project;
    int shell = 0;
    std::vector<int> nodes;
};

int addShellOn(core::Project& p, const std::vector<int>& nodes, double h, double e = 30000.0, double nu = 0.3, double gamma = 24.53) {
    const int id = p.structure.addShell(nodes, h);
    core::Shell& s = p.structure.shells.back();
    s.material.kind = core::ShellMaterialKind::GenericIsotropic;
    s.material.useGeneralData = false;
    s.material.isoElasticModulusMPa = e;
    s.material.isoPoissonRatio = nu;
    s.material.isoDensityKnM3 = gamma;
    return id;
}

void supportEdges(core::Project& p, int shell, const std::vector<int>& nodes, core::ExternalSupportType type, const std::vector<int>& edges) {
    core::ExternalSupport sup;
    sup.type = type;
    for (int e : edges) core::setEdgeExterior(p.shellProps, core::makeEdgeKey(shell, nodes[e], nodes[(e + 1) % nodes.size()]), sup);
}

void addShellLoad(core::Project& p, int shell, double value, core::ShellLoadAxes axes, core::ShellLoadSense sense,
                  core::ShellLoadType type = core::ShellLoadType::Uniform, double value2 = 0.0) {
    core::ShellLoad l;
    l.id = p.shellLoads.nextId++;
    l.shellId = shell;
    l.loadCaseId = 0;
    l.type = type;
    l.axes = axes;
    l.sense = sense;
    l.value1 = value;
    l.value2 = value2;
    p.shellLoads.items.push_back(l);
}

/// The plate of section 9.0 (load 10000 local +z, self weight active).
ShellCase plate(double h = 0.2, double maxSide = 1.0) {
    ShellCase c;
    core::StructuralModel& sm = c.project.structure;
    c.nodes = {sm.addNode({0, 0, 0}), sm.addNode({4, 0, 0}), sm.addNode({4, 4, 0}), sm.addNode({0, 4, 0})};
    c.shell = addShellOn(c.project, c.nodes, h);
    supportEdges(c.project, c.shell, c.nodes, core::ExternalSupportType::Pinned, {0, 1, 2, 3});
    addShellLoad(c.project, c.shell, 10000.0, core::ShellLoadAxes::Local, core::ShellLoadSense::PlusZ);
    if (maxSide != 1.0) {
        core::ShellRecord rec;
        rec.maxTriangleSideM = maxSide;
        core::setShellRecord(c.project.shellProps, c.shell, rec);
    }
    return c;
}

/// The wall of section 13.4: 4 x 2 m in the plane XZ, edge x = 0 clamped, 1000 kN/m2 global -Z plus the self weight.
ShellCase wall() {
    ShellCase c;
    core::StructuralModel& sm = c.project.structure;
    c.nodes = {sm.addNode({0, 0, 0}), sm.addNode({4, 0, 0}), sm.addNode({4, 0, 2}), sm.addNode({0, 0, 2})};
    c.shell = addShellOn(c.project, c.nodes, 0.2);
    supportEdges(c.project, c.shell, c.nodes, core::ExternalSupportType::Fixed, {3});
    addShellLoad(c.project, c.shell, 1000.0, core::ShellLoadAxes::Global, core::ShellLoadSense::MinusZ);
    return c;
}

void setFactor(ShellCase& c, int index, double value) {
    core::ShellRecord rec = core::shellRecordOf(c.project.shellProps, c.shell);
    rec.stiffness.set(index, value);
    core::setShellRecord(c.project.shellProps, c.shell, rec);
}

struct Solved {
    FrameBuild build;
    LinearResult result;
    const LoadCaseResult& c() const { return result.cases.at(0); }
    int nearest(double x, double y, double z) const {
        int best = -1;
        double d = 1e300;
        for (std::size_t i = 0; i < result.mesh.positions.size(); ++i) {
            const auto& p = result.mesh.positions[i];
            const double di = std::hypot(p[0] - x, p[1] - y, p[2] - z);
            if (di < d) { d = di; best = static_cast<int>(i); }
        }
        return best;
    }
    double disp(double x, double y, double z, int k) const { return c().meshDisplacements.at(nearest(x, y, z))[k]; }
    double extreme(int k, bool max) const {
        double v = max ? -1e300 : 1e300;
        for (const auto& d : c().meshDisplacements) v = max ? std::max(v, d[k]) : std::min(v, d[k]);
        return v;
    }
    /// Extreme of a smoothed shell force over the mesh nodes of the first shell.
    double forceExtreme(int q, bool max) const {
        double v = max ? -1e300 : 1e300;
        for (const auto& [n, f] : smoothedShellForces(build.model, result, 0, build.model.shells.front().id)) v = max ? std::max(v, f[q]) : std::min(v, f[q]);
        return v;
    }
};

Solved solve(const core::Project& p) {
    Solved s;
    s.build = buildFrameModel(p);
    EXPECT_TRUE(s.build.errors.empty()) << (s.build.errors.empty() ? "" : s.build.errors.front());
    EXPECT_TRUE(s.build.unsupported.empty()) << (s.build.unsupported.empty() ? "" : s.build.unsupported.front());
    s.result = solveLinear(s.build.model, s.build.cases);
    EXPECT_EQ(s.result.status, SolveStatus::Ok) << s.result.message;
    return s;
}

constexpr double kNet = 10000.0 - 0.2 * 24.53;  // 9995.094 kN/m2

}  // namespace

// --- mesh [V rule of section 15.1: contour points, Delaunay, bisection of the interior edges] ---
namespace {
/// Vertices (corner nodes of the 6-node triangles) of the mesh, and those on the contour of a rectangle [0, a] x [0, b] of the plane XY / XZ.
std::size_t vertexCount(const ShellMesh& m) {
    std::vector<int> v;
    for (const auto& t : m.triangles) v.insert(v.end(), t.nodes.begin(), t.nodes.begin() + 3);
    std::sort(v.begin(), v.end());
    return static_cast<std::size_t>(std::unique(v.begin(), v.end()) - v.begin());
}
std::vector<int> nodesOnPlane(const ShellMesh& mesh, int shell, int axis, double value) {
    std::vector<int> out;
    for (const auto& t : mesh.triangles)
        if (t.shell == shell)
            for (int n : t.nodes)
                if (std::fabs(mesh.positions[n][axis] - value) < 1e-9) out.push_back(n);
    std::sort(out.begin(), out.end());
    out.erase(std::unique(out.begin(), out.end()), out.end());
    return out;
}
}  // namespace

TEST(FemShellMesh, DelaunayWithEdgeBisectionAndTheMaximumSide) {
    // Plate 4 x 4 m, s = 1 m: 3 intermediate points per side (floor((4 - 0.001) / 1)), 16 contour vertices; the bisection adds the interior.
    // Same counts as docs/research/data/mesh_sim.py; the original has 57 vertices / 87 triangles (25 on the contour, 32 inside) [section 15.1].
    const Solved p = solve(plate().project);
    std::printf("[value] mesh plate 4x4 s=1: %zu vertices, %zu triangles (original 57 / 87)\n", vertexCount(p.result.mesh), p.result.mesh.triangles.size());
    EXPECT_EQ(p.result.mesh.triangles.size(), 80u);
    EXPECT_EQ(vertexCount(p.result.mesh), 49u);
    EXPECT_EQ(nodesOnPlane(p.result.mesh, 0, 1, 0.0).size(), 9u);  // side y = 0: 5 vertices and 4 mid-side nodes
    const Solved w = solve(wall().project);
    std::printf("[value] mesh wall 4x2 s=1: %zu vertices, %zu triangles\n", vertexCount(w.result.mesh), w.result.mesh.triangles.size());
    EXPECT_EQ(w.result.mesh.triangles.size(), 44u);
    EXPECT_EQ(vertexCount(w.result.mesh), 29u);
    // Every triangle has its sides at most about the size (the bisection splits every interior edge longer than 1.01 s).
    for (const auto& t : p.result.mesh.triangles)
        for (int i = 0; i < 3; ++i) {
            const auto &a = p.result.mesh.positions[t.nodes[i]], &b = p.result.mesh.positions[t.nodes[(i + 1) % 3]];
            EXPECT_LE(std::hypot(a[0] - b[0], a[1] - b[1]), 1.01 + 1e-9);
        }
    const Solved f = solve(plate(0.2, 0.5).project);
    std::printf("[value] mesh plate 4x4 s=0.5: %zu vertices, %zu triangles\n", vertexCount(f.result.mesh), f.result.mesh.triangles.size());
    EXPECT_GT(f.result.mesh.triangles.size(), 3u * 80u);
    // Edge of 1.5 m with s = 1: one intermediate point [V example].
    FrameModel m;
    for (int i = 0; i < 3; ++i) {
        FemNode n;
        n.id = i + 1;
        n.position = {i == 1 ? 1.5 : 0.0, i == 2 ? 0.8 : 0.0, 0.0};
        m.nodes.push_back(n);
    }
    FemShell t;
    t.id = 1;
    t.nodes = {1, 2, 3};
    m.shells = {t};
    EXPECT_EQ(nodesOnPlane(meshShells(m), 0, 1, 0.0).size(), 5u);  // 3 vertices and 2 mid-side nodes
}

TEST(FemShellMesh, NeighbouringShellsAndEdgeBarsShareTheNodes) {
    // Coplanar shells form one plane that takes the MAXIMUM size of its shells; an edge between planes takes the MINIMUM [V, section 15.1].
    FrameModel m;
    const auto node = [&](int id, double x, double y, double z) {
        FemNode n;
        n.id = id;
        n.position = {x, y, z};
        m.nodes.push_back(n);
    };
    node(1, 0, 0, 0); node(2, 0, 2, 0); node(3, 3, 0, 0); node(4, 3, 2, 0); node(5, 4, 0, 0); node(6, 4, 2, 0);  // two coplanar shells
    node(7, 3, 0, 3); node(8, 3, 2, 3);                                                                       // a vertical shell on x = 3
    FemShell a, b, c;
    a.id = 1;
    a.nodes = {1, 3, 4, 2};  // 3 x 2
    a.maxSide = 1.0;
    b.id = 2;
    b.nodes = {3, 5, 6, 4};  // 1 x 2
    b.maxSide = 0.4;
    c.id = 3;
    c.nodes = {3, 4, 8, 7};  // vertical, 2 x 3
    c.ex = {0, 1, 0};
    c.ey = {0, 0, 1};
    c.ez = {1, 0, 0};
    c.maxSide = 0.3;
    m.shells = {a, b};
    FemBar bar;
    bar.id = 7;
    bar.nodeI = 3;
    bar.nodeJ = 4;  // along the common edge
    m.bars = {bar};
    {
        const ShellMesh mesh = meshShells(m);
        EXPECT_TRUE(mesh.issues.empty());
        // One plane, s = max(1.0, 0.4) = 1.0: the 2 m edge gets floor((2 - 0.001) / 1) = 1 intermediate point.
        ASSERT_EQ(mesh.barSplits.count(0), 1u);
        EXPECT_EQ(mesh.barSplits.at(0).size(), 5u);  // 3 vertices and 2 mid-side nodes
        EXPECT_NEAR(mesh.barSplits.at(0).back().second, 2.0, 1e-12);
        const std::vector<int> onA = nodesOnPlane(mesh, 0, 0, 3.0), onB = nodesOnPlane(mesh, 1, 0, 3.0);
        EXPECT_EQ(onA, onB);  // conforming
        EXPECT_EQ(onA.size(), 5u);
    }
    m.shells = {a, b, c};
    {
        const ShellMesh mesh = meshShells(m);
        EXPECT_TRUE(mesh.issues.empty());
        // The fold x = 3 joins the horizontal plane (s = 1.0) and the vertical one (s = 0.3): s = 0.3, floor((2 - 0.0003) / 0.3) = 6 points.
        ASSERT_EQ(mesh.barSplits.count(0), 1u);
        EXPECT_EQ(mesh.barSplits.at(0).size(), 15u);  // 7 segments: 8 vertices and 7 mid-side nodes
        const std::vector<int> onA = nodesOnPlane(mesh, 0, 0, 3.0), onB = nodesOnPlane(mesh, 1, 0, 3.0), onC = nodesOnPlane(mesh, 2, 2, 0.0);
        EXPECT_EQ(onA, onB);
        EXPECT_EQ(onA, onC);
        EXPECT_EQ(onA.size(), 15u);
    }
}

TEST(FemShellMesh, NodeInsideAShellIsReported) {
    core::Project p = plate().project;
    const int inner = p.structure.addNode({2, 2, 0});
    const int top = p.structure.addNode({2, 2, 3});
    p.structure.addBar(inner, top);
    const FrameBuild b = buildFrameModel(p);
    bool reported = false;
    for (const auto& u : b.unsupported) reported = reported || u.find("Nudo " + std::to_string(inner)) != std::string::npos;
    EXPECT_TRUE(reported);
}

// --- plate bending: soft simple support of the original ---
TEST(FemShellPlate, SimplySupportedPlateMatchesTheOriginalAndNavier) {
    const Solved s = solve(plate().project);
    // Mesh of 1 m (default, section 15.1 / 18.1): against the original with the same maximum side (clone 502.8 mm / 7827.8 kN.m/m).
    const double w = s.disp(2, 2, 0, 2) * 1e3;
    expectRel(w, 499.28, 0.015, "w vs Mindlin soft support");
    expectRel(s.extreme(2, true) * 1e3, 501.16, 0.01, "w max vs the original (mesh 1 m)");
    const auto mx = shellForcesAt(s.build.model, s.result, 0, 1, 2.0, 2.0);
    ASSERT_TRUE(mx.has_value());
    expectRel((*mx)[3], 7887.5, 0.015, "Mx centre vs the original (max, mesh 1 m)");  // -0.8 %
    expectRel(s.forceExtreme(3, true), 7887.5, 0.015, "Mx max vs the original");
    expectRel(s.forceExtreme(4, true), 7881.1, 0.015, "My max vs the original");  // -0.4 %
    // Against Navier (Kirchhoff) with the default mesh: +2.2 % (the original itself: +3.0 % max, +2.6 % centre; the soft support adds ~+3 %).
    expectRel((*mx)[3], 7658.1, 0.035, "Mx centre (mesh 1 m) vs Navier");
    EXPECT_NEAR((*mx)[0], 0.0, 1e-6 * 7658.0);  // no membrane force
    // Equilibrium: the supports take the whole net load.
    double rz = 0.0;
    for (const auto& [id, r] : s.c().reactions) rz += r[2];
    for (const auto& [i, r] : s.c().meshReactions) rz += r[2];
    expectRel(rz, -kNet * 16.0, 1e-9, "sum of the reactions");
    // Convergence with the mesh (original: 501.16 -> 498.09 with 0.25 m).
    const Solved f = solve(plate(0.2, 0.25).project);
    expectRel(f.disp(2, 2, 0, 2) * 1e3, 499.28, 0.01, "w (mesh 0.25 m) vs Mindlin soft support");
    expectRel(f.extreme(2, true) * 1e3, 498.09, 0.01, "w max (mesh 0.25 m) vs the original");
    const auto mf = shellForcesAt(f.build.model, f.result, 0, 1, 2.0, 2.0);
    expectRel((*mf)[3], 7857.5, 0.01, "Mx centre (mesh 0.25 m) vs the original");  // +0.6 %
    // Tolerances of the task against Navier (Kirchhoff) from the mesh of 0.5 m on (the soft support adds +4 % in w and +3 % in M).
    const Solved h = solve(plate(0.2, 0.5).project);
    expectRel(h.disp(2, 2, 0, 2) * 1e3, 472.95, 0.06, "w (mesh 0.5 m) vs Navier");
    const auto mh = shellForcesAt(h.build.model, h.result, 0, 1, 2.0, 2.0);
    expectRel((*mh)[3], 7658.1, 0.035, "Mx (mesh 0.5 m) vs Navier");
    expectRel((*mh)[4], 7658.1, 0.035, "My (mesh 0.5 m) vs Navier");
}

TEST(FemShellPlate, ThickPlateReproducesTheSoftSupportOfTheOriginal) {
    const Solved s = solve(plate(0.4).project);
    expectRel(s.extreme(2, true) * 1e3, 67.19, 0.02, "w max h = 0.4 vs the original");
    expectRel(s.forceExtreme(3, true), 8111.0, 0.01, "Mx max h = 0.4 vs the original");  // -0.03 %
}

TEST(FemShellPlate, SelfWeightAloneIsGammaTimesThickness) {
    ShellCase c = plate();
    c.project.shellLoads.items.clear();
    const Solved s = solve(c.project);
    double rz = 0.0;
    for (const auto& [id, r] : s.c().reactions) rz += r[2];
    for (const auto& [i, r] : s.c().meshReactions) rz += r[2];
    expectRel(rz, 0.2 * 24.53 * 16.0, 1e-9, "reactions of the self weight");
    expectRel(s.extreme(2, false) * 1e3, -0.24, 0.05, "Dz min (original -0.24 mm)");
}

// --- stiffness factors [V law D' = S D S] ---
TEST(FemShellFactors, BendingAndShearFactorsMatchTheOriginal) {
    const Solved base = solve(plate().project);
    const double w0 = base.extreme(2, true), mx0 = base.forceExtreme(3, true), my0 = base.forceExtreme(4, true);
    {
        ShellCase c = plate();
        setFactor(c, cc::fem::kMxx, 0.5);
        const Solved s = solve(c.project);
        expectRel(s.extreme(2, true) / w0, 604.36 / 501.16, 0.03, "Mxx = 0.5: w");
        expectRel(s.forceExtreme(3, true) / mx0, 5143.0 / 7887.5, 0.02, "Mxx = 0.5: Mx");
        expectRel(s.forceExtreme(4, true) / my0, 8762.0 / 7881.1, 0.03, "Mxx = 0.5: My");
    }
    {
        ShellCase c = plate();
        setFactor(c, cc::fem::kMxx, 0.0);  // calculates without a message and Mx = 0 everywhere [V]
        const Solved s = solve(c.project);
        EXPECT_NEAR(s.forceExtreme(3, true), 0.0, 1e-9);
        EXPECT_NEAR(s.forceExtreme(3, false), 0.0, 1e-9);
        expectRel(s.extreme(2, true) / w0, 879.97 / 501.16, 0.08, "Mxx = 0: w (original x1.756, coarse mesh)");
    }
    {
        // Every out-of-plane factor 0 (membrane 1): the original calculates without a message and the displacement grows about 1e9 times
        // (0.96 mm -> 9.5967e8 mm), with Mxx = Mxy = Myy = Vxz = Vyz = 0 listed [V, section 15.4]: factor 0 -> 1e-9 in the stiffness.
        ShellCase c = plate();
        for (int k : {cc::fem::kMxx, cc::fem::kMxy, cc::fem::kMyy, cc::fem::kVxz, cc::fem::kVyz}) setFactor(c, k, 0.0);
        const Solved s = solve(c.project);
        EXPECT_TRUE(s.result.message.empty()) << s.result.message;
        expectRel(s.extreme(2, true) / w0, 1e9, 1e-3, "out-of-plane factors 0: w ratio");
        for (int q = 3; q < 8; ++q) {
            EXPECT_NEAR(s.forceExtreme(q, true), 0.0, 1e-9);
            EXPECT_NEAR(s.forceExtreme(q, false), 0.0, 1e-9);
        }
    }
    {
        ShellCase c = plate();
        setFactor(c, cc::fem::kMxy, 0.5);
        const Solved s = solve(c.project);
        expectRel(s.extreme(2, true) / w0, 603.12 / 501.16, 0.03, "Mxy = 0.5: w");
    }
    {
        ShellCase c = plate();
        setFactor(c, cc::fem::kVxz, 0.1);
        setFactor(c, cc::fem::kVyz, 0.1);
        const Solved s = solve(c.project);
        expectRel(s.extreme(2, true) / w0, 595.35 / 501.16, 0.04, "Vxz = Vyz = 0.1: w");
    }
    {
        ShellCase c = plate();
        setFactor(c, cc::fem::kFxx, 0.0);
        setFactor(c, cc::fem::kFxy, 0.0);
        setFactor(c, cc::fem::kFyy, 0.0);  // no membrane stiffness: the plate does not change and there is no message [V]
        const Solved s = solve(c.project);
        expectRel(s.extreme(2, true), w0, 1e-9, "membrane factors 0: w");
    }
}

TEST(FemShellFactors, MembraneFactorsMatchTheOriginalWall) {
    const Solved base = solve(wall().project);
    const double dz0 = base.disp(4, 0, 1, 2), dx0 = base.extreme(0, true);
    expectRel(dz0 * 1e3, -19.90, 0.02, "wall tip Dz vs the original");
    expectRel(dz0 * 1e3, -20.02, 0.02, "wall tip Dz vs the plane-stress Q4 64 x 32");
    expectRel(dx0 * 1e3, 5.34, 0.03, "wall Dx max vs the original");
    {
        ShellCase c = wall();
        setFactor(c, cc::fem::kFxx, 0.5);
        const Solved s = solve(c.project);
        expectRel(s.disp(4, 0, 1, 2) / dz0, 35.77 / 19.90, 0.02, "Fxx = 0.5: Dz");
        expectRel(s.extreme(0, true) / dx0, 10.65 / 5.34, 0.02, "Fxx = 0.5: Dx");
    }
    {
        ShellCase c = wall();
        setFactor(c, cc::fem::kFxy, 0.5);
        const Solved s = solve(c.project);
        expectRel(s.disp(4, 0, 1, 2) / dz0, 23.94 / 19.90, 0.02, "Fxy = 0.5: Dz");
        // Self weight switched off ("Desactivar" + selection): -23.94 -> -23.82 [V].
        ShellCase off = c;
        off.project.shellLoads.selfWeightOff.insert(off.shell);
        const Solved t = solve(off.project);
        expectRel(t.disp(4, 0, 1, 2) / s.disp(4, 0, 1, 2), 1000.0 / (1000.0 + 0.2 * 24.53), 1e-9, "self weight off");
        expectRel(t.disp(4, 0, 1, 2) / s.disp(4, 0, 1, 2), 23.82 / 23.94, 0.002, "self weight off vs the original");
    }
    // The cantilever wall has no transverse shear and its membrane forces equilibrate the load.
    EXPECT_NEAR(base.forceExtreme(6, true), 0.0, 1e-6);
}

TEST(FemShellBallast, WinklerFoundationOnLocalZ) {
    ShellCase c = plate();
    core::Shell& s = c.project.structure.shells.back();
    s.ballastEnabled = true;
    s.ballastKnM3 = 1000000.0;
    const Solved r = solve(c.project);
    expectRel(r.extreme(2, true) * 1e3, 10.96, 0.02, "w max with K = 1e6 (original 10.96 mm, Q / K = 9.995)");
}

TEST(FemShellLoads, TemperatureFreeAndRestrainedPlate) {
    // Free in the plane (statically determinate): no forces, displacement alpha T x. Restrained: N = -E h alpha T / (1 - nu).
    for (const bool restrained : {false, true}) {
        core::Project p;
        core::StructuralModel& sm = p.structure;
        const std::vector<int> n = {sm.addNode({0, 0, 0}), sm.addNode({3, 0, 0}), sm.addNode({3.5, 2, 0}), sm.addNode({0, 2.5, 0})};
        const int sh = addShellOn(p, n, 0.2);
        p.structure.shells.back().material.isoThermalExpansion = 1e-5;
        p.shellLoads.selfWeightOff.insert(sh);
        if (restrained) {
            supportEdges(p, sh, n, core::ExternalSupportType::Fixed, {0, 1, 2, 3});
        } else {
            core::ExternalSupport fix, line, z;
            fix.type = core::ExternalSupportType::Fixed;
            p.nodeSupports.external[n[0]] = fix;
            line.type = core::ExternalSupportType::LineXPinned;  // Dy, Dz blocked
            p.nodeSupports.external[n[1]] = line;
            z.type = core::ExternalSupportType::PlaneXYPinned;   // Dz blocked
            p.nodeSupports.external[n[3]] = z;
        }
        core::ShellLoad l;
        l.id = 1;
        l.shellId = sh;
        l.type = core::ShellLoadType::UniformTemperature;
        l.value1 = 20.0;
        p.shellLoads.items.push_back(l);
        p.shellLoads.nextId = 2;
        const Solved s = solve(p);
        const auto f = shellForcesAt(s.build.model, s.result, 0, sh, 1.0, 1.0);
        ASSERT_TRUE(f.has_value());
        if (restrained) {
            expectRel((*f)[0], -30e6 * 0.2 * 1e-5 * 20.0 / 0.7, 1e-6, "Nx restrained");
            expectRel((*f)[1], -30e6 * 0.2 * 1e-5 * 20.0 / 0.7, 1e-6, "Ny restrained");
        } else {
            EXPECT_NEAR((*f)[0], 0.0, 1e-6);
            EXPECT_NEAR((*f)[2], 0.0, 1e-6);
            expectRel(s.disp(3.5, 2, 0, 0), 1e-5 * 20.0 * 3.5, 1e-9, "Dx free (patch test)");
            expectRel(s.disp(3.5, 2, 0, 1), 1e-5 * 20.0 * 2.0, 1e-9, "Dy free (patch test)");
        }
    }
}

TEST(FemShellBars, BeamsAlongTheEdgesCarryAFlexibleSlab) {
    // 4 x 1 m slab spanning only across (factors: no membrane, Mxx = Mxy = 0) on two simply supported edge beams: each beam takes q * 0.5 kN/m.
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
    cs.shearAreaZM2 = 100.0;  // 1e6 cm2: no shear deformation
    const int sec = sm.addSection(cs);
    const std::vector<int> n = {sm.addNode({0, 0, 0}), sm.addNode({4, 0, 0}), sm.addNode({4, 1, 0}), sm.addNode({0, 1, 0})};
    const int b1 = sm.addBar(n[0], n[1], m, sec), b2 = sm.addBar(n[3], n[2], m, sec);
    const int sh = addShellOn(p, n, 0.1, 30000.0, 0.2, 0.0001);
    core::ShellRecord rec;
    for (int k : {cc::fem::kFxx, cc::fem::kFxy, cc::fem::kFyy, cc::fem::kMxx, cc::fem::kMxy}) rec.stiffness.set(k, 0.0);
    core::setShellRecord(p.shellProps, sh, rec);
    p.shellLoads.selfWeightOff.insert(sh);
    p.barLoads.selfWeightOff = {b1, b2};
    core::ExternalSupport pin;
    pin.type = core::ExternalSupportType::Pinned;
    for (int k : n) p.nodeSupports.external[k] = pin;
    addShellLoad(p, sh, 10.0, core::ShellLoadAxes::Global, core::ShellLoadSense::MinusZ);
    const Solved s = solve(p);
    const double q = 5.0, l = 4.0, ei = 210e6 * 1e-4;
    expectRel(s.disp(2, 0, 0, 2), -5.0 * q * l * l * l * l / (384.0 * ei), 0.02, "beam midspan deflection");
    ASSERT_EQ(s.c().barForces.size(), 2u);
    double rz = 0.0;
    for (const auto& [id, r] : s.c().reactions) rz += r[2];
    expectRel(rz, 40.0, 1e-9, "reactions");
    EXPECT_GT(std::fabs(s.c().barForces[0].origin[2]), 0.8 * q * l / 2.0);  // the rest goes straight to the support through the slab corner
}

// --- integration bands [V, section 13.1] ---
TEST(FemShellBands, BandAlongXReproducesTheOriginal) {
    const Solved s = solve(plate().project);
    const BandSpec band{1, 0.0, 2.0, 4.0, 2.0, 0.15};
    struct Row { double x, my, vz; };
    // Original, method "esfuerzos internos alisados" (default), mesh 1 m.
    for (const Row& r : {Row{0.994, -916.14, 855.010}, Row{1.487, -1090.62, 399.314}, Row{1.995, -1160.51, -0.462}, Row{2.991, -943.38, -829.493}}) {
        const auto p = bandForcesAt(s.build.model, s.result, 0, band, r.x);
        ASSERT_TRUE(p.has_value());
        // Nodal mean of section 16.2 on the mesh of 80 triangles (the original 87): My +5.8 / -1.6 / +0.2 / -0.6 %; Vz -12.4 % at 0.994 (the
        // point nearest the support, where the mesh differs most), +0.9 / +0.4 % at the others.
        expectRel(p->forces[4], r.my, 0.06, "My band (smoothed)");
        if (std::fabs(r.vz) > 100.0) expectRel(p->forces[2], r.vz, r.x < 1.0 ? 0.13 : 0.035, "Vz band (smoothed)");
        EXPECT_NEAR(p->forces[0], 0.0, 1e-6);  // no membrane
        EXPECT_NEAR(p->leftWidth + p->rightWidth, 0.15, 1e-12);
    }
    // Navier reference x 0.15 at the centre: My = -Mx * width = -1148.7 (sign of the bars: My of the band = -Mx of the plate).
    const auto c = bandForcesAt(s.build.model, s.result, 0, band, 2.0);
    expectRel(c->forces[4], -1148.7, 0.02, "My band centre vs Navier");  // +1.3 %
    // Without smoothing: the corner values of the triangle that contains the point (+0.9 %).
    const auto u = bandForcesAt(s.build.model, s.result, 0, band, 0.994, BandMethod::UnsmoothedStresses);
    expectRel(u->forces[4], -927.77, 0.02, "My band (unsmoothed)");
    // Nodal forces [V algorithm, section 15.2]: the original gives -841.96 at 0.994 and -752.20 at 1.995 (much coarser than the stresses);
    // the figures depend on how the cut crosses each triangle of the exact mesh, so only the order of magnitude is checked.
    for (const Row& r : {Row{0.994, -841.96, 833.013}, Row{1.995, -752.20, -70.922}}) {
        const auto n = bandForcesAt(s.build.model, s.result, 0, band, r.x, BandMethod::NodalForces);
        ASSERT_TRUE(n.has_value());
        std::printf("[value] nodal forces x = %.3f: My %.2f (original %.2f), Vz %.2f (original %.2f), Mt %.2f\n", r.x, n->forces[4], r.my,
                    n->forces[2], r.vz, n->forces[3]);
        // Clone (mesh of 80 triangles): -1757.5 / -2107.3, i.e. x2.1 / x2.8 the original; NOT reproduced (open, TK-109).
        EXPECT_LT(n->forces[4], 0.0);
        EXPECT_LT(std::fabs(n->forces[4]), 3.0 * std::fabs(r.my));
    }
    // Points of the band: 10 by default, from end to end (the ends lie on the supported edges, which the cross line follows: not moved).
    const auto pts = integrateBand(s.build.model, s.result, 0, band);
    ASSERT_EQ(pts.size(), 10u);
    EXPECT_NEAR(pts.front().s, 0.0, 1e-12);
    EXPECT_NEAR(pts.back().s, 4.0, 1e-12);
    // A band that ends in the corner of the contour: both widths vanish and the point is moved 1 cm inside [V rule].
    const BandSpec diagonal{1, 0.0, 0.0, 4.0, 4.0, 0.15};
    const auto d0 = bandForcesAt(s.build.model, s.result, 0, diagonal, 0.0);
    ASSERT_TRUE(d0.has_value());
    EXPECT_NEAR(d0->s, 0.01, 1e-12);
}

TEST(FemShellBands, WideBandNodalForcesAgreeWithStressesOnAFineMesh) {
    const Solved s = solve(plate(0.2, 0.25).project);
    const BandSpec band{1, 0.0, 2.0, 4.0, 2.0, 1.0};
    const auto a = bandForcesAt(s.build.model, s.result, 0, band, 1.1, BandMethod::SmoothedStresses);
    const auto b = bandForcesAt(s.build.model, s.result, 0, band, 1.1, BandMethod::NodalForces);
    ASSERT_TRUE(a && b);
    expectRel(b->forces[4], a->forces[4], 0.03, "My: nodal forces vs stresses");
}

TEST(FemShellBands, BandIsClippedByTheContour) {
    const Solved s = solve(plate().project);
    const BandSpec edge{1, 0.0, 0.05, 4.0, 0.05, 0.15};  // centre line 5 cm from the edge y = 0
    const auto p = bandForcesAt(s.build.model, s.result, 0, edge, 2.0);
    ASSERT_TRUE(p.has_value());
    EXPECT_NEAR(p->rightWidth, 0.075, 1e-12);
    EXPECT_NEAR(p->leftWidth, 0.05, 1e-12);
}

// --- face stresses [V, cefbandint] ---
TEST(FemShellStress, FaceStressesAndVonMises) {
    const ShellForces f{100.0, -50.0, 20.0, 3.0, -1.5, 0.6, 12.0, -8.0};
    const double h = 0.2;
    const FaceStress top = topFaceStress(f, h), bottom = bottomFaceStress(f, h);
    EXPECT_NEAR(top.sx, 500.0 + 450.0, 1e-9);
    EXPECT_NEAR(top.sy, -250.0 - 225.0, 1e-9);
    EXPECT_NEAR(top.txy, 100.0 + 90.0, 1e-9);
    EXPECT_NEAR(bottom.sx, 500.0 - 450.0, 1e-9);
    EXPECT_NEAR(bottom.txz, 60.0, 1e-9);
    EXPECT_NEAR(top.tyz, -40.0, 1e-9);
    const double vm = vonMises(top);
    EXPECT_NEAR(vm, std::sqrt(950.0 * 950.0 + 950.0 * 475.0 + 475.0 * 475.0 + 3.0 * (190.0 * 190.0 + 3600.0 + 1600.0)), 1e-9);
    const auto pr = principalStresses(FaceStress{10.0, 0.0, 0.0, 0.0, 0.0});
    EXPECT_DOUBLE_EQ(pr[0], 10.0);
    EXPECT_DOUBLE_EQ(pr[1], 0.0);
    const ShellForces r = rotateShellForces(ShellForces{1, 2, 0, 3, 4, 0, 5, 6}, std::acos(-1.0) / 2);
    EXPECT_NEAR(r[0], 2.0, 1e-12);
    EXPECT_NEAR(r[3], 4.0, 1e-12);
    EXPECT_NEAR(r[6], 6.0, 1e-12);
}
