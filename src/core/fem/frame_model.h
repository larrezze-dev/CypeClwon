#pragma once
#include <array>
#include <map>
#include <span>
#include <string>
#include <vector>

#include "core/fem/shell_model.h"

namespace cc::fem {

/// Linear static analysis of 3D frames (T-P8-01). Behaviour source: docs/research/solver_lineal_portico.md.
///
/// The original runs an embedded OpenSees (element ElasticBeam3dCype, 6 DOF per node, direct sparse solver CYPEMumpsSolver, one load
/// case per pattern, "algorithm Linear -factorOnce"). The clone reproduces it with Eigen: the stiffness matrix is factorised once
/// (sparse LDLT) and every load case ("hipótesis") is solved with that factorisation.
///
/// This header is the project-independent model: plain data in kN, m, rad and ºC. The adapter from a cc::core::Project lives in
/// fem/frame_builder.h. Eigen is an implementation detail of the .cpp files.
///
/// Conventions
///  - DOFs per node: Dx, Dy, Dz, Gx, Gy, Gz in GLOBAL axes (results, nodal loads, supports, reactions).
///  - Each bar runs from `nodeI` (origin of its local x axis) to `nodeJ`. Its local axes (ex, ey, ez: unit vectors in global
///    components, right-handed) are given already resolved (the adapter takes them from core::barLocalAxes). Bar loads and end
///    forces are in those local axes unless a load says it is global; positions along the bar are metres from nodeI.
///  - Bending about local y (plane xz: Dz, Gy) uses Iy and Avz; bending about local z (plane xy: Dy, Gz) uses Iz and Avy.
///  - Shear deformation (Timoshenko, phi = 12 E I / (G Av L^2)) when SolverOptions::shearDeformation is on and the shear area of that
///    plane is > 0; otherwise Euler-Bernoulli for that plane. [V] the original applies it with the section shear areas (measured: C1, C2, C3, C6).
///  - Torsion: St. Venant (G J / L); warping is not part of the linear element (verified: Iw is not passed to the element).
///  - Section forces: see applySignConvention() in fem/linear_solver.h (the ONLY place where the sign convention is decided).
///
/// Shells (T-P8-02): FrameModel::shells, meshed and solved with the bars (fem/shell_model.h).
///
/// Relations between DOFs (T-P8-03, docs/research/solver_diafragmas_ligaduras_apoyos.md): ligatures (FrameModel::equalDofs), rigid
/// diaphragms (FrameModel::diaphragms) and oblique supports (FemNode::oblique) are exact linear relations "slave = sum coef * master"
/// eliminated by transformation (u = T u_r, K_r = T' K T), like the expanded dofRelation of the original. Springs to the ground with a
/// "Sentido" (FemNode::springSense) are bilateral in solveLinear (the linear analysis of the original ignores the sense [V]) and unilateral
/// in solveNonlinear (non-linear combinations, active set).
///
/// Hooks for later tasks (NOT implemented here, see .tickets.md TK-106): rigid end offsets / "tamaño de nudo" (-jntOffset).

using Vec3 = std::array<double, 3>;
using Dof6 = std::array<double, 6>;

/// Mechanical properties of the bar (the average of the start and end values of the stretch, like the original:
/// (value_start + value_end) * 0.5). Shear areas 0 = no shear deformation in that plane. Depths are only used by the thermal
/// gradient (0 = unknown: a gradient across that depth is then ignored).
struct SectionData {
    double area = 0.0;     ///< m2
    double iy = 0.0;       ///< m4, about local y
    double iz = 0.0;       ///< m4, about local z
    double torsion = 0.0;  ///< m4, St. Venant
    double avy = 0.0;      ///< m2, shear area along local y
    double avz = 0.0;      ///< m2, shear area along local z
    double depthY = 0.0;   ///< m, extent of the section along local y
    double depthZ = 0.0;   ///< m, extent of the section along local z
};

/// Connection of one rotational DOF of a bar end to its node.
struct EndRelease {
    enum class Kind { Rigid, Free, Spring };
    Kind kind = Kind::Rigid;
    double stiffness = 0.0;  ///< kN.m/rad, Spring only (> 0)
};

// SpringSense and ObliqueSupport are declared in fem/shell_model.h (shared with the edge bonds of the shells).

struct FemNode {
    int id = 0;
    Vec3 position{};
    std::array<bool, 6> fixed{};  ///< blocked global DOFs (prescribed values per load case in LoadCaseInput::prescribed)
    Dof6 spring{};                ///< springs to the ground, kN/m and kN.m/rad (0 = none) [V, 10.4]
    std::array<SpringSense, 6> springSense{};  ///< per DOF; ignored by solveLinear (bilateral), honoured by solveNonlinear
    /// Line: the translation is free only along `obliqueDirection` (u parallel to it); Plane: free in the plane normal to `obliqueDirection`
    /// (n . u = 0). The translations of the node must not be `fixed` then; its rotations follow `fixed`.
    ObliqueSupport oblique = ObliqueSupport::None;
    Vec3 obliqueDirection{0, 0, 1};
};

/// Ligature ("equalDOF"): u_slave,k = u_master,k for every DOF k with dofs[k] [REA + V 10.3]. Which node is the master does not change the
/// results (the solver swaps them when only the slave DOF is blocked).
struct FemEqualDof {
    int master = 0, slave = 0;  ///< node ids
    std::array<bool, 6> dofs{};
};

/// Rigid diaphragm, horizontal (every node at the same Z within 1e-3 m) [REA 2]: for every node s other than the master m = nodes.front():
/// Dx_s = Dx_m - (y_s - y_m) Gz_m, Dy_s = Dy_m + (x_s - x_m) Gz_m, Gz_s = Gz_m; Dz, Gx, Gy stay free [V 10.2]. A released bar end keeps its own
/// rotation (internal DOF of the element), so it is not tied to Gz_m: the clone's choice, close to the rotation groups of the original [REA 2.5]
/// but not measured (Gz of a pinned bar at a diaphragm node is [N], 10.6).
struct FemDiaphragm {
    int id = 0;
    std::vector<int> nodes;  ///< node ids, the first one is the master (its position does not change the results [REA 2.6, V 10.2])
    /// FemShell ids: every mesh node of these shells is a slave of the diaphragm with the same relations [N: not measured]. Without `nodes`
    /// the master is the first mesh node of the first shell.
    std::vector<int> shells;
};

/// Load on a diaphragm: Fx, Fy (kN) at the plan point (x, y) (m); applied to the master as Fx, Fy and Mz = (x - xm) Fy - (y - ym) Fx [V 10.2].
struct FemDiaphragmLoad {
    int diaphragm = 0;  ///< FemDiaphragm::id
    double fx = 0.0, fy = 0.0, x = 0.0, y = 0.0;
};

struct FemBar {
    int id = 0;
    int nodeI = 0, nodeJ = 0;  ///< node ids; nodeI = origin of the local x axis
    double e = 0.0;            ///< kN/m2
    double g = 0.0;            ///< kN/m2
    double alpha = 0.0;        ///< thermal expansion, 1/ºC
    SectionData section;
    Vec3 ex{1, 0, 0}, ey{0, 1, 0}, ez{0, 0, 1};
    /// [end][k]: end 0 = nodeI, 1 = nodeJ; k 0 = Gx (torsion, plane yz), 1 = Gy (plane xz), 2 = Gz (plane xy), local axes.
    std::array<std::array<EndRelease, 3>, 2> releases{};
};

enum class BarLoadKind {
    Distributed,  ///< linear between x1 (value w1) and x2 (value w2), force per metre of bar
    PointForce,   ///< force w1 at x1
    PointMoment,  ///< moment w1 at x1
    Temperature   ///< temperature increments over the whole bar
};

struct FemBarLoad {
    int bar = 0;  ///< bar id
    BarLoadKind kind = BarLoadKind::Distributed;
    bool global = false;  ///< w1 / w2 in global axes (otherwise local); temperatures are always local
    double x1 = 0.0, x2 = 0.0;  ///< metres from nodeI
    Vec3 w1{}, w2{};
    double uniformTemperature = 0.0;  ///< ºC, whole section (axial strain alpha * T)
    double gradientY = 0.0;           ///< ºC, T(face +y) - T(face -y), across depthY
    double gradientZ = 0.0;           ///< ºC, T(face +z) - T(face -z), across depthZ
};

struct FemNodalLoad {
    int node = 0;   ///< node id
    Dof6 values{};  ///< Fx, Fy, Fz (kN), Mx, My, Mz (kN.m), global
};

/// One load case ("hipótesis"): loads plus the prescribed displacements of blocked DOFs (m, rad; absent = 0).
struct LoadCaseInput {
    int id = 0;
    std::string name;  ///< used in the messages ('%s')
    std::vector<FemNodalLoad> nodal;
    std::vector<FemBarLoad> bars;
    std::map<int, Dof6> prescribed;  ///< node id -> values (only the blocked DOFs are used)
    std::vector<FemShellLoad> shellLoads;  ///< loads on shells (shell = FemShell::id)
    std::vector<FemDiaphragmLoad> diaphragmLoads;
};

struct FrameModel {
    std::vector<FemNode> nodes;
    std::vector<FemBar> bars;
    std::vector<FemShell> shells;
    std::vector<FemEqualDof> equalDofs;
    std::vector<FemDiaphragm> diaphragms;
};

struct SolverOptions {
    bool shearDeformation = true;      ///< default on = behaviour of the original [V]; shear area 0 ("ignore") not measured
    double pivotTolerance = 1e-11;     ///< a pivot below tolerance * K_ii means a mechanism
    double orphanTolerance = 1e-13;    ///< a diagonal below tolerance * max diagonal means a DOF without stiffness
    double nullModeTolerance = 1e-9;   ///< relative residual below which a collinear-chain twist is an exact null mode (fixed to 0)
    double maxNodeRotation = 1.5707963267948966;  ///< rad; a rotation of a bar node above it gives ExcessiveDisplacements [V] (shell-only nodes not checked [N]), 0 = no check
};

/// ExcessiveDisplacements: a node rotation exceeded pi/2 [V, solver_medidas_original.md 9.2]; the original then disables the results.
/// NotConverged: the active set of the unilateral springs of a non-linear combination did not settle (#328573).
enum class SolveStatus { Ok, Mechanism, Unstable, Error, ExcessiveDisplacements, NotConverged };

/// Section forces at both ends of a bar, local axes, in the convention of applySignConvention(): N, Vy, Vz, T (Mx), My, Mz.
struct BarEndForces {
    int bar = 0;
    Dof6 origin{};  ///< at nodeI
    Dof6 end{};     ///< at nodeJ
};

/// Displacements of the two ends of a bar (global axes, m / rad) including the rotation of a released / partially fixed end, which differs from
/// the rotation of its node. The original shows THIS value at a node with a partially fixed bar end: with the base node clamped and c = 0.5 it
/// reads Gz(N1) = theta of the spring [V, solver_medidas_original.md 9.4]; which bar it shows at a node with several bars is [N].
struct BarEndDisplacements {
    int bar = 0;
    Dof6 origin{};  ///< at nodeI
    Dof6 end{};     ///< at nodeJ
};

struct LoadCaseResult {
    int id = 0;
    std::string name;
    SolveStatus status = SolveStatus::Ok;
    std::string message;                 ///< es-ES message of the original for a failed case (empty when Ok)
    std::vector<Dof6> displacements;     ///< per node, in FrameModel::nodes order; m and rad, global
    /// node id -> forces of the supports on the structure (global; 0 on free DOFs). Blocked DOFs include the forces transmitted through
    /// ligatures; an oblique support gives the force along its normal (plane) / across its line on Dx..Dz; a spring gives -K u (whether the
    /// original lists spring forces among the reactions is [N]).
    std::map<int, Dof6> reactions;
    std::vector<BarEndForces> barForces; ///< per bar, in FrameModel::bars order
    std::vector<BarEndDisplacements> barDisplacements;  ///< per bar, in FrameModel::bars order
    // Shells (only when the model has shells):
    std::vector<Dof6> meshDisplacements;   ///< per mesh node (LinearResult::mesh.positions order; the first ones repeat `displacements`)
    std::map<int, Dof6> meshReactions;     ///< generated mesh node index -> reactions of the edge bonds (global)
    std::vector<TriangleForces> triangleForces;  ///< per mesh triangle
};

struct LinearResult {
    SolveStatus status = SolveStatus::Ok;   ///< worst status of the model (Error / Mechanism of the stiffness, or Ok)
    std::string message;                    ///< model level error text (invalid input, or the mechanism warning of the original), es-ES
    int mechanismNode = -1;                 ///< Mechanism by a null pivot: node id and DOF (0..5 = Dx..Gz) named by the warning, else -1
    int mechanismDof = -1;
    std::vector<LoadCaseResult> cases;      ///< one per input case, same order
    /// DOFs without any stiffness (every element released there, e.g. the rotations of a node where all bar ends are pinned) that were
    /// locked automatically, as node id -> mask. The original avoids them with springs of stiffness 1e-3 (cyosrelac_cte_infima).
    std::map<int, std::array<bool, 6>> lockedDofs;
    /// Null modes of the stiffness that no load excites (rule [R] of TK-108, refined in solver_medidas_original.md 10.3): one DOF per mode was
    /// fixed to 0 without any warning (node id -> mask; generated mesh nodes are not listed), like the original (Gx = 0.000 along a chain of
    /// collinear bars; a bar pinned at its base with an axial load). A load case that excites a null mode is a mechanism.
    std::map<int, std::array<bool, 6>> nullModeDofs;
    ShellMesh mesh;                         ///< mesh of the shells (empty without shells)
};

}  // namespace cc::fem
