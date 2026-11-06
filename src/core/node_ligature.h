#pragma once
#include <array>
#include <optional>
#include <string>
#include <vector>

#include "core/node_support.h"
#include "core/structure_model.h"

namespace cc::core {

/// Node ties ("Ligaduras", T-P6-02, core part): two or more nodes that share the same displacement along a global axis in every load case.
/// Behaviour source: docs/research/ui/ligaduras.md (REA of ed3dlib.dll: ed3ligadura.c, ed3arligadura.c, ed3comun.c; measured in the original).
///
/// Public API (for the GUI layer)
///  - Data: Project::ligatures (NodeLigatures): three independent collections, one per global direction (X, Y, Z). A tie is {id, nodeIds}.
///    The number shown in the 3D label is the 1-based POSITION of the tie in the collection of its direction (renumbered on removals).
///  - Queries: ligatureIndexOf / ligatureLabel (text "Dx(1), Dz(2)" appended to the node label after ": ") / hasLigature.
///  - Capture: isLigatureCapturable(project, nodeId) (nodes with an external restraint on a line or plane are never captured, silently).
///  - Validation (pure): checkLigatureAssign(project, orderedNodeIds, directions) -> LigatureCheck (status Ok / Silent / Error + literal text).
///  - Undoable edits (node_ligature_edit.h): assignLigatures / removeLigatures (+ node deletion / merge handled by node_edit).
///  - Persistence: JSON section "node_ties" (absent in older files = empty).
///
/// PROVISIONAL / NOT MEASURED (TODO(medir)): the diaphragm check is a stub (no diaphragms yet, T-P6-04); the prescribed-displacement
/// conflict (#64231) compares only nodes that store a prescribed set for the load case with the DOF blocked (REA FUN_180043f60);
/// the warnings #64198 / #64199 (prescribed vs tie on already existing data) are not reproduced; ties of a single node left over by an
/// assignment are kept (measured) and disappear on the next removal / node deletion (REA regeneration).

/// Direction index: 0 = X, 1 = Y, 2 = Z (also the DOF index Dx, Dy, Dz).
using LigatureDirections = std::array<bool, 3>;

struct Ligature {
    int id = 0;                   ///< unique, growing id (not an index)
    std::vector<int> nodeIds;     ///< no repeats; the first node is the one that was the representative when created
    bool operator==(const Ligature&) const = default;
};

struct NodeLigatures {
    std::array<std::vector<Ligature>, 3> dirs;  ///< dx, dy, dz
    int nextId = 1;
    bool empty() const { return dirs[0].empty() && dirs[1].empty() && dirs[2].empty(); }
    bool operator==(const NodeLigatures&) const = default;
};

/// 0-based position of the tie of `nodeId` in direction `dir`, or nullopt.
std::optional<int> ligatureIndexOf(const NodeLigatures& ligatures, int dir, int nodeId);
bool hasLigature(const NodeLigatures& ligatures, int nodeId);
/// "Dx(1), Dz(2)" (only the tied directions, in X, Y, Z order); empty when the node has no tie.
std::string ligatureLabel(const NodeLigatures& ligatures, int nodeId);

/// False when the node does not exist or has an external restraint defined on a line or plane (types Line* / Plane*, generic "sobre una
/// recta / plano"): the original never captures such nodes and says nothing.
bool isLigatureCapturable(const StructuralModel& model, const NodeSupports& supports, int nodeId);
/// True when the node belongs to a diaphragm. STUB: always false until the diaphragms exist (T-P6-04).
bool isNodeInDiaphragm(int nodeId);

enum class LigatureStatus { Ok, Silent, Error };
struct LigatureCheck {
    LigatureStatus status = LigatureStatus::Ok;
    int messageId = 0;   ///< original string id of an Error
    std::string text;    ///< literal es-ES text of an Error
};

/// Literal texts: 388777, 64230, 64231, 375639, 64200, 64139. Empty for another id.
std::string ligatureMessage(int messageId);

struct Project;
/// Selection as the original captures it: duplicates dropped keeping the first occurrence, unknown and non-capturable nodes removed.
std::vector<int> ligatureSelection(const Project& project, const std::vector<int>& nodeIds);

/// Checks of "Asignar", in the order of the original: no direction (#388777) ; fewer than 2 nodes (Silent) ; diaphragm node (#375639) ;
/// nodes already in one single tie in every marked direction (#64230) ; different prescribed displacements (#64231).
/// `nodeIds` are in selection order (the first one is the representative); unknown nodes are ignored.
LigatureCheck checkLigatureAssign(const Project& project, const std::vector<int>& nodeIds, const LigatureDirections& directions);

// --- pure edits on the data (used by the commands; exposed for tests) ---

/// Assign (REA FUN_180186bf0): per marked direction that needs it, the first node is the representative; the others leave their ties and join
/// the tie of the first one or a new tie. Ties left with < 2 nodes are NOT removed here (measured).
void applyLigatureAssign(NodeLigatures& ligatures, const std::vector<int>& nodeIds, const LigatureDirections& directions);
/// Remove with marked directions: the given nodes leave their ties of those directions; every tie of those directions left with < 2 nodes
/// is destroyed. Without any marked direction ALL ties of the work are removed (measured).
void applyLigatureRemove(NodeLigatures& ligatures, const std::vector<int>& nodeIds, const LigatureDirections& directions);
/// Deleted / merged nodes leave their ties in X, Y and Z; ties with < 2 nodes are destroyed in every direction.
void applyLigatureNodesDeleted(NodeLigatures& ligatures, const std::vector<int>& nodeIds);

/// Consistency of stored data: ids unique and < nextId per direction, no repeated node inside a tie, a node in at most one tie per direction,
/// ties of >= 1 node, every node exists. Empty when valid.
std::string validateLigatures(const NodeLigatures& ligatures, const StructuralModel& model);

}  // namespace cc::core
