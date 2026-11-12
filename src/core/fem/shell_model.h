#pragma once
#include <array>
#include <map>
#include <string>
#include <vector>

namespace cc::fem {

/// Shells of the linear model (T-P8-02). Behaviour source: docs/research/elemento_lamina_bandas.md (REA sections 1-8, measurements of the
/// original sections 9 and 13).
///
/// The original meshes every shell into 6-node triangles («element ShellCype», section ElasticMembranePlateSectionCype: E, nu, h, the ballast
/// modulus X and the eight stiffness factors) and solves them with the bars in the same OpenSees model. The clone:
///  - Mesh (shell_mesh.cpp, rule of section 15.1 [V]): straight-sided 6-node triangles from a Delaunay triangulation of the contour points
///    (0 intermediate points if L <= 1.01 s, else floor((L - 0.001 s) / s)) refined by bisection of the interior edges (length > 1.01 s or the
///    shape test k = 2.0, m = 0.35 s). s = maximum size of the shells of a plane; an edge between planes takes the minimum. Contour chains
///    are never split, so neighbouring shells and bars along the edges conform. 4 x 4 m plate, s = 1: 49 vertices / 80 triangles (original
///    57 / 87: the contour rule of the original gives more contour nodes, not read).
///  - Bars along a shell edge (both bar nodes are the corners of the edge) are split at every mesh node of the edge (internal sub-elements;
///    results are still given per bar).
///  - Element: membrane = plane-stress quadratic triangle; plate = Reissner-Mindlin quadratic triangle with assumed (MITC6a-type) transverse
///    shear strains, which reproduces the "soft" simple support measured in the original (+4 % in w over Navier at h/a = 0.05, section 13.2);
///    drilling rotation tied to the in-plane rotation by a small penalty (1e-3 G h x Fxy). Ballast: Winkler springs on the local w.
///  - Stiffness factors [V law, section 13.3]: D' = S D S with S = diag(sqrt Fxx, sqrt Fyy) on the membrane (sqrt Mxx, sqrt Myy on bending)
///    and the factor applied directly to the shear / torsion term (Fxy -> D33, Mxy -> D66); Vxz, Vyz scale the transverse shear stiffness.
///    A factor 0 calculates without a message [V]: it is replaced by 1e-9 in the stiffness [V measured, section 15.4] and the forces are
///    recovered with the nominal factors (Mxx = 0 -> Mx = 0 everywhere).
///
/// Local axes of a shell: ex, ey in the plane, ez normal (core::shellLocalAxes); local 2D coordinates are measured from the first contour
/// node (the system of the integration bands, core::shellFrame).
///
/// Shell forces per unit width in the shell axes (T-P8-02 conventions, measured where marked):
///  - Nx, Ny, Nxy (kN/m) tension positive; Mx, My, Mxy (kN.m/m) = integral of z * sigma over the thickness, so a positive moment stretches the
///    +z face (sigma_top = N/h + 6 M/h^2 [V]); with a load along +z the deflection is +z and Mx, My > 0 in the span [V, section 9.0];
///    Qx, Qy (kN/m) = integral of tau_xz, tau_yz.
using Vec3S = std::array<double, 3>;

/// Indices of the eight stiffness factors in dialog order (Membrana Fxx, Fxy, Fyy; Flexión Mxx, Mxy, Myy; Cortante Vxz, Vyz).
enum ShellFactor { kFxx = 0, kFxy, kFyy, kMxx, kMxy, kMyy, kVxz, kVyz };

/// "Sentido" of a spring to the ground [V, solver_diafragmas_ligaduras_apoyos.md 10.1]: Positive acts only while the displacement of that
/// global DOF is > 0, Negative only while it is < 0 (non-linear combinations only); Both = always (linear spring).
enum class SpringSense { Both, Positive, Negative };

/// Oblique support of the translations of a node or of an edge (user line / plane not parallel to a global axis). Always rigid [V, 10.4, 11.3].
enum class ObliqueSupport { None, Line, Plane };

/// Interior bond of one side of a shell edge (Propiedades > Láminas > Vinculación interior [V 11.4]). It only acts on an edge shared with another
/// shell or with a bar; the shell side with a bond other than Fixed gets its own copy of the mesh nodes of the edge, joined to the original
/// nodes (which stay with the other elements of the edge):
///  - Fixed («Empotrada», default): no copy, full continuity.
///  - Free («Libre»): no joint at all. When every element leaves an original node of the edge, that node is reported as unconnected
///    (ShellMesh::orphanModelNodes) like the original («Se ha encontrado un nudo que no está conectado...» [V]).
///  - Pinned («Articulada»): translations tied, rotations free [V 11.4: -0.96 / -0.71 mm].
///  - Generic («Genérica»): per DOF in the local axes of the edge (x along the edge from nodes[i] to nodes[i+1], z = shell z, y = z x x):
///    Fixed = tied, Elastic = spring k per metre of edge (kN/m2 displacements, kN.m/rad.m rotations), Free = nothing [N law, not measured].
enum class EdgeLinkMode { Free, Fixed, Elastic };
struct FemEdgeInterior {
    enum class Kind { Fixed, Free, Pinned, Generic };
    Kind kind = Kind::Fixed;
    std::array<EdgeLinkMode, 6> mode{};  ///< Generic only
    std::array<double, 6> k{};           ///< Generic, Elastic DOFs only
};

struct FemShell {
    int id = 0;
    std::vector<int> nodes;         ///< 3 or 4 FemNode ids in contour order
    Vec3S ex{1, 0, 0}, ey{0, 1, 0}, ez{0, 0, 1};  ///< local axes (unit, right-handed)
    double thickness = 0.0;         ///< m
    bool orthotropic = false;
    double e = 0.0, nu = 0.0;       ///< isotropic, kN/m2
    double e1 = 0.0, e2 = 0.0, nu12 = 0.0, g12 = 0.0, g13 = 0.0, g23 = 0.0;  ///< orthotropic (material axes = shell axes), kN/m2
    double alpha1 = 0.0, alpha2 = 0.0;  ///< thermal expansion along local x / y (isotropic: equal), 1/ºC
    std::array<double, 8> factors{1, 1, 1, 1, 1, 1, 1, 1};
    double ballast = 0.0;           ///< Winkler modulus along local z, kN/m3 (0 = none) [V: parameter X of the section]
    double maxSide = 1.0;           ///< "Tamaño máximo de los lados de un triángulo", m
    /// Exterior bond of each contour edge (edge i joins nodes[i] and nodes[i+1]): blocked GLOBAL DOFs of the mesh nodes of the edge.
    std::vector<std::array<bool, 6>> edgeFixed;
    /// Elastic exterior bond of each edge (optional, same indexing; missing = none): springs to the ground per metre of edge, global axes,
    /// kN/m2 (displacements) and kN.m/rad.m (rotations) [V 11.2: K per metre, independent of the mesh], and their "Sentido".
    std::vector<std::array<double, 6>> edgeSpring;
    std::vector<std::array<SpringSense, 6>> edgeSpringSense;
    /// Oblique exterior bond of each edge (optional; missing = None): rigid relation on the translations of its mesh nodes [V 11.3].
    std::vector<ObliqueSupport> edgeOblique;
    std::vector<Vec3S> edgeObliqueDirection;
    /// Interior bond of each edge of this shell (optional; missing = Fixed).
    std::vector<FemEdgeInterior> edgeInterior;
    /// Corner nodes (FemNode id) where the shell joins the node through rotational springs (kN.m/rad) about its local x and y axes, the
    /// translations and the rotation about its normal being tied: the shell as one "piece" of a node with «empotramiento parcial para todas
    /// las piezas» (TK-106) [N for shells: measured with bars only].
    std::map<int, double> cornerRotationSpring;
};

enum class ShellLoadKind { Pressure, Temperature, Shrinkage };

/// Load on a shell in one load case. Pressure: force per square metre of shell surface (not projected [V, wall of section 13.4]) along the
/// unit vector `direction` (global axes when `global`, else shell local axes); its value is the affine field p0 + px x + py y in the local 2D
/// coordinates of the shell (uniform: px = py = 0; "variable" loads: linear along local x or y). Temperature: uniform increment `value` ºC
/// (membrane strain alpha * T). Shrinkage: imposed isotropic membrane strain -value ([N] sign: a positive "retracción" shortens).
struct FemShellLoad {
    int shell = 0;
    ShellLoadKind kind = ShellLoadKind::Pressure;
    bool global = true;
    Vec3S direction{0, 0, -1};
    double p0 = 0.0, px = 0.0, py = 0.0;
    double value = 0.0;
};

/// The mesh of all the shells. Nodes: the first ones are the FemNodes of the model in FrameModel::nodes order, then the generated nodes.
struct ShellMesh {
    struct Triangle {
        int shell = 0;                 ///< index into FrameModel::shells
        std::array<int, 6> nodes{};    ///< mesh node indices: corners (counter-clockwise about the shell z) then mid-sides 01, 12, 20
    };
    std::vector<Vec3S> positions;      ///< every mesh node
    int modelNodes = 0;                ///< nodes [0, modelNodes) are FrameModel::nodes
    std::vector<Triangle> triangles;
    std::vector<std::array<bool, 6>> edgeFixed;  ///< per mesh node, DOFs blocked by edge bonds (corner rule: see shell_mesh.cpp)
    /// Bars split along shell edges: bar index -> mesh nodes from nodeI to nodeJ (both ends included) and their distances from nodeI.
    std::map<int, std::vector<std::pair<int, double>>> barSplits;
    std::vector<std::string> issues;   ///< es-ES descriptions of geometry that could not be connected (bar nodes inside a shell, ...)

    /// Springs to the ground of the elastic edge bonds, already integrated over the edge: K per metre x tributary length of the node. Each
    /// quadratic segment of length h gives h/6, 4h/6, h/6 to its vertex, mid-side and vertex nodes (row sums of the consistent edge matrix) [N
    /// distribution; the total K x L is measured, V 11.2].
    struct GroundSpring {
        int node = 0;  ///< mesh node
        int dof = 0;   ///< 0..5, global
        double k = 0.0;
        SpringSense sense = SpringSense::Both;
    };
    std::vector<GroundSpring> edgeSprings;
    /// Oblique edge bonds per mesh node (None for the others).
    std::vector<ObliqueSupport> oblique;
    std::vector<Vec3S> obliqueDirection;
    /// Homogeneous relations sum(coef * u_dof) = 0 between mesh DOFs (dof = 6 * node + k) created by the interior edge bonds and the partial
    /// fixity of shell corners; `preferred` = the DOF of the copy, eliminated when possible.
    struct Relation {
        int preferred = -1;
        std::vector<std::pair<int, double>> terms;
    };
    std::vector<Relation> relations;
    /// Two-node springs between a copy and its original node: stiffness k along the unit vector `axis` of the translations (rotation = false)
    /// or of the rotations (rotation = true).
    struct Link {
        int a = 0, b = 0;
        bool rotation = false;
        Vec3S axis{1, 0, 0};
        double k = 0.0;
    };
    std::vector<Link> links;
    /// Copy -> original mesh node of every node duplicated by an interior edge bond or a partially fixed corner.
    std::map<int, int> copies;
    /// FrameModel::nodes indices that were shell corners but are left without any element (interior bonds «Libre» on every side).
    std::vector<int> orphanModelNodes;
};

/// Shell forces at the six nodes of a triangle, shell local axes: Nx, Ny, Nxy, Mx, My, Mxy, Qx, Qy.
using ShellForces = std::array<double, 8>;
struct TriangleForces {
    std::array<ShellForces, 6> atNodes{};   ///< the field of the element at its nodes (unsmoothed)
    std::array<ShellForces, 3> atGauss{};   ///< at the points (r, s) = (1/6, 1/6), (2/3, 1/6), (1/6, 2/3) (base of the smoothing)
    std::array<double, 36> nodalForces{};  ///< forces of the 6 nodes on the element (K u - f_load), global axes, 6 per node
};

// --- post-processing helpers ---

/// Face stresses [V, cefbandint_calcula_tensiones_en_caras]: sigma = N/h +- 6 M/h^2, tau_xz = Qx/h, tau_yz = Qy/h (kN/m2).
struct FaceStress {
    double sx = 0.0, sy = 0.0, txy = 0.0, txz = 0.0, tyz = 0.0;
};
FaceStress topFaceStress(const ShellForces& f, double thickness);
FaceStress bottomFaceStress(const ShellForces& f, double thickness);
/// sqrt(sx^2 - sx sy + sy^2 + 3 (txy^2 + txz^2 + tyz^2)) [V, cefbandint_tension_von_mises].
double vonMises(const FaceStress& s);
/// Principal in-plane stresses of a face (max, min) and the angle of the first one from local x (rad).
std::array<double, 3> principalStresses(const FaceStress& s);

/// Rotation of the shell forces to axes turned `angle` (rad) about the shell z: N_tt, N_nn, N_tn, M_tt, M_nn, M_tn, Q_t, Q_n.
ShellForces rotateShellForces(const ShellForces& f, double angle);

}  // namespace cc::fem
