#pragma once
#include <array>
#include <optional>
#include <string>
#include <vector>

#include "core/project.h"
#include "core/structure_snapshot.h"
#include "core/undo_stack.h"

namespace cc::core {

/// Undoable generators of "Geometría > Herramientas > Generar" (T-P5-10): Planos, Nudos y barras (1 / 2 / 3 segments) and Malla de
/// tetraedros. Behaviour source: docs/research/ui/herramientas.md section 6.2 (measured in the original on 2026-11-05).
/// Measured: the menu has three entries; the ranges of the planes and of the divisions; the defaults of every field; the selection
/// flow (done in the GUI); that two planes with 1 m separation give 2 copies of the plane displaced 1 m; that 3 divisions of one
/// segment give 2 intermediate nodes and 3 bars. NOT measured (PROVISIONAL, marked below): the geometry of the 2D / 3D meshes and
/// of the tetrahedra, what "Agrupar" and "Crear pieza" do exactly, the ranges of the tetrahedra fields.

inline constexpr int kMinPlaneCount = 1;
inline constexpr int kMaxPlaneCount = 100;
inline constexpr double kMinPlaneSeparationM = -10000.0;
inline constexpr double kMaxPlaneSeparationM = 10000.0;
inline constexpr int kMinDivisions = 1;
inline constexpr int kMaxDivisions = 1000;
/// Range of "Distancia entre planos" of the tetrahedra mesh. NOT MEASURED: provisional, the plane separation range.
inline constexpr double kMaxTetrahedraDistanceM = 10000.0;
/// Safety limit on the nodes a single generation may create. NOT in the original: protects the clone from 1000 x 1000 x 1000 meshes.
inline constexpr std::size_t kMaxGeneratedNodes = 200000;
/// Distance under which two coordinates count as the same plane / a point counts as lying on a plane (metres). PROVISIONAL.
inline constexpr double kGenerateToleranceM = 1e-3;

enum class GenerateError {
    None,
    UnknownElement,        ///< a node or bar id does not exist
    LinesParallel,         ///< the two reference lines are parallel or equal (no plane)
    PlaneNotOrthogonal,    ///< the plane is not perpendicular to a global axis ("Solo planos ortogonales a los ejes globales")
    CountOutOfRange,       ///< "Nº de planos a generar" outside 1 .. 100
    SeparationOutOfRange,  ///< separation outside -10000 .. 10000 m
    ZeroSeparation,        ///< separation = 0: the original crashes (internal error); the clone refuses it with a clear warning
    DivisionsOutOfRange,   ///< divisions outside 1 .. 1000
    DegenerateSegments,    ///< a segment has no length, or the segments are collinear / coplanar when they must not be
    DistanceOutOfRange,    ///< tetrahedra distance zero or beyond the range
    TooManyElements,       ///< more than kMaxGeneratedNodes nodes
    NothingGenerated       ///< every element to create already exists
};

/// Spanish text of an error (the warnings of the original for the ranges, the clone's own wording elsewhere).
std::string generateErrorMessage(GenerateError error);
/// "El valor de 'Nº de divisiones del segmento k' debe estar entre 1 y 1000."
std::string divisionsWarning(int segment);

// --- Planos ---------------------------------------------------------------------------------------------------------------

struct PlaneSpec {
    int barA = 0, barB = 0;    ///< the two reference lines: bars that define the plane (any two non-parallel bars of it)
    int count = 1;             ///< "Nº de planos a generar"
    double separationM = 1.0;  ///< "Separación entre planos": + = positive sense of the global axis, - = opposite
    bool group = false;        ///< "Agrupar planos generados"
};

/// Plane defined by two reference bars: perpendicular to a global axis (0 = X, 1 = Y, 2 = Z) at `coordinate`.
struct PlaneOfLines {
    int axis = 0;
    double coordinate = 0.0;
};
/// The plane of two bars, or the reason there is none (LinesParallel, PlaneNotOrthogonal, UnknownElement).
std::optional<PlaneOfLines> planeOfBars(const StructuralModel& model, int barA, int barB, GenerateError* error = nullptr);

/// Elements lying completely on the plane (nodes within kGenerateToleranceM, bars and shells with all their nodes on it).
struct PlaneContent {
    std::vector<int> nodeIds, barIds, shellIds;
};
PlaneContent elementsOnPlane(const StructuralModel& model, const PlaneOfLines& plane);

/// True when the plane of the spec, or one of the planes it would create, already belongs to a group of the work planes: grouping
/// again dissolves those groups (#63970 "Se van a desagrupar todos los planos que intervienen en la generación.").
bool generationUngroups(const Project& project, const PlaneSpec& spec);
inline constexpr const char* kUngroupMessage = "Se van a desagrupar todos los planos que intervienen en la generación.";

/// Replaces the plane groups of the project (undoable); used by "Agrupar planos generados".
class SetPlaneGroupsCommand : public UndoCommand {
public:
    SetPlaneGroupsCommand(Project& project, PlaneGroups after)
        : m_project(project), m_before(project.workPlanes.groups), m_after(std::move(after)) {}
    void redo() override { m_project.workPlanes.groups = m_after; }
    void undo() override { m_project.workPlanes.groups = m_before; }
    std::string text() const override { return "Agrupar planos"; }

private:
    Project& m_project;
    PlaneGroups m_before, m_after;
};

/// "Planos": copies the elements of the plane `count` times, the k-th displaced k * separation along the axis (one undo step).
/// With `group` the original and the generated planes become one group of the work planes (a group of any of them is dissolved first).
GenerateError generatePlanes(Project& project, UndoStack& stack, const PlaneSpec& spec);

// --- Nudos y barras ---------------------------------------------------------------------------------------------------------

/// Selection of "Nudos y barras": `nodeIds` = start and end of segment 1, then the end of segment 2 and of segment 3 (same origin).
/// 2 entries = 1 segment, 3 = 2 segments (2D mesh), 4 = 3 segments (3D mesh).
struct NodeMeshSpec {
    std::vector<int> nodeIds;
    std::array<int, 3> divisions{1, 1, 1};
    bool onlyNodes = false;                                  ///< "Generar sólo los nudos" (2 and 3 segments)
    std::array<bool, 3> groupParallel{false, false, false};  ///< "Agrupar barras paralelas al segmento k"
    std::array<bool, 3> makePieces{false, false, false};     ///< "Crear pieza entre barras alineadas" (segment k)
};

inline int segmentCount(const NodeMeshSpec& spec) { return static_cast<int>(spec.nodeIds.size()) - 1; }

/// Whether `candidateNode` can be the end of the next segment: not collinear with the first segment (2nd) and outside the plane of
/// the first two (3rd). Used by the GUI to ignore a pick, like the original does (a coplanar 3rd segment is not counted).
bool segmentIsIndependent(const StructuralModel& model, const std::vector<int>& nodeIdsSoFar, int candidateNode);

/// "Nudos y barras". 1 segment: n - 1 equally spaced nodes on the segment; a bar that joins its two nodes is divided into n bars (keeps
/// material, section, roll, x inversion and layers; PROVISIONAL: a piece record of the old bar is dropped). 2 / 3 segments: a grid of
/// nodes O + i/n1 * s1 + j/n2 * s2 (+ k/n3 * s3) reusing nodes that already exist, and, unless "sólo nudos", a bar between every pair of
/// neighbours along each segment direction (existing bars are kept). PROVISIONAL: "Crear pieza" makes one multi-bar piece per line of the
/// direction; "Agrupar" shares a group id among the pieces of the direction (single-bar piece records are created for that).
class GenerateNodeMeshCommand : public SnapshotCommand {
public:
    GenerateNodeMeshCommand(Project& project, const NodeMeshSpec& spec);
    bool valid() const { return m_error == GenerateError::None; }
    GenerateError error() const { return m_error; }
    const std::vector<int>& createdNodeIds() const { return m_nodes; }
    const std::vector<int>& createdBarIds() const { return m_bars; }

private:
    GenerateError m_error = GenerateError::None;
    std::vector<int> m_nodes, m_bars;
};

// --- Malla de tetraedros ------------------------------------------------------------------------------------------------------

/// `nodeIds` = start and end of segment 1 and end of segment 2 (same origin). The second plane lies `distanceM` from the first along
/// the right-hand "screw rule" normal s1 x s2 (a negative distance goes the other way).
struct TetrahedraSpec {
    std::array<int, 3> nodeIds{0, 0, 0};
    int divisions1 = 1, divisions2 = 1;
    double distanceM = 1.0;
    bool groupLowerBars = true, groupUpperBars = true, groupDiagonals = true;
};

/// PROVISIONAL geometry (the original result was not measured): lower plane = grid of (n1+1) x (n2+1) nodes with bars along both
/// segments; upper plane = one node above the centre of every cell, joined along both directions to its neighbours; every upper node
/// is joined to the four corners of its cell (the double-layer tetrahedral space frame). "Agrupar" shares a group id (single-bar piece
/// records) among the lower bars, the upper bars and the diagonals respectively.
class GenerateTetrahedraCommand : public SnapshotCommand {
public:
    GenerateTetrahedraCommand(Project& project, const TetrahedraSpec& spec);
    bool valid() const { return m_error == GenerateError::None; }
    GenerateError error() const { return m_error; }
    const std::vector<int>& createdNodeIds() const { return m_nodes; }
    const std::vector<int>& createdBarIds() const { return m_bars; }

private:
    GenerateError m_error = GenerateError::None;
    std::vector<int> m_nodes, m_bars;
};

GenerateError generateNodeMesh(Project& project, UndoStack& stack, const NodeMeshSpec& spec);
GenerateError generateTetrahedra(Project& project, UndoStack& stack, const TetrahedraSpec& spec);

}  // namespace cc::core
