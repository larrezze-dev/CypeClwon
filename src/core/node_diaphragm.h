#pragma once
#include <optional>
#include <string>
#include <vector>

#include "core/structure_model.h"

namespace cc::core {

struct Project;

/// Rigid diaphragms ("Diafragmas", T-P6-03, core part). Behaviour source: docs/research/ui/diafragmas.md (REA of ed3dlib.dll:
/// ed3diafragma.c, ed3cardiafragma.c, ed3rcdiafrag1.c; measured in the original).
///
/// Public API (for the GUI layer)
///  - Data: Project::diaphragms (Diaphragms): list of Diaphragm {id, reference, colour (magenta by default), nodeIds, loads}. A node belongs to
///    at most ONE diaphragm. Ids grow and are never reused (nextId).
///  - Queries: findDiaphragm / diaphragmOfNode / isNodeInDiaphragm / diaphragmNodeLabel ("D1": the 3D label of the node is "N4: D1")
///    / diaphragmTooltipLine ("Diafragma : D1") / diaphragmElevation (mean Z of the nodes) / diaphragmNodesSameElevation.
///  - Dialog "Editar" (modal, Aceptar / Cancelar): the GUI edits a copy `std::vector<Diaphragm>` with newDiaphragm / duplicateDiaphragm /
///    defaultDiaphragmLoad and the fields of the struct, asks checkDiaphragmAdd before adding another one and, on Aceptar,
///    editDiaphragms(project, stack, edited) (checkDiaphragmAccept inside; id 0 = new). The nodes of existing diaphragms cannot be changed there.
///  - Tool "Diafragma" (Asignar / Eliminar asignación + RMB on the selected nodes): assignNodesToDiaphragm / removeNodesFromDiaphragm.
///    If no diaphragm exists the GUI shows the information #184611 (diaphragmNoneDefinedMessage) and does not enter the tool.
///  - "Generar por niveles": generateDiaphragmsByLevels(project, stack, confirmedReplace) -> DiaphragmGeneration.
///  - Persistence: JSON section "diaphragms" (absent in older files = empty). Deleting / merging nodes purges them (node_edit).
///
/// PROVISIONAL / NOT MEASURED (TODO(medir)): order of the checks when a node has both a restraint and a tie; id of the "La referencia no puede
/// quedar vacía." text; the text of #375641 (shell edges: shells are not in diaphragms yet); the algorithm of "Generar por niveles" (which nodes
/// of a level enter, naming of the references, tolerance); where cota / eccentricities are edited (#375643 / #375649 / #375650); #375642 in the
/// calculation; diaphragm shells (ids_laminas_ef) and the export to the solver.

/// Magenta: idcolor_colorRGB(0xd) of the original ([CAP] default colour of a new diaphragm), 0xRRGGBB.
inline constexpr unsigned kDefaultDiaphragmColor = 0xFF00FF;
/// Two nodes are "in the same plane" when their Z differ by at most this (REA FUN_1807eb6a0), metres.
inline constexpr double kDiaphragmSameElevationM = 0.001;

/// Load introduced on a diaphragm for one load case ("hipótesis"): forces Fx, Fy (kN) at the point (x, y) (m). `loadCaseId` is opaque here
/// (the load cases arrive with T-P7-01), like PrescribedDisplacement.
struct DiaphragmLoad {
    int loadCaseId = 0;
    double fxKn = 0.0, fyKn = 0.0;
    double xM = 0.0, yM = 0.0;
    bool operator==(const DiaphragmLoad&) const = default;
};

struct Diaphragm {
    int id = 0;                      ///< 0 = not stored yet (new row of the dialog)
    std::string reference;           ///< "Referencia"; empty only while it is being edited
    unsigned color = kDefaultDiaphragmColor;
    std::vector<int> nodeIds;        ///< sorted ascending, no repeats
    std::vector<int> shellIds;       ///< shells (ids_laminas_ef) with all their nodes in the diaphragm; set by "Generar por niveles" (sorted)
    std::vector<DiaphragmLoad> loads;  ///< "Cargas introducidas"
    bool operator==(const Diaphragm&) const = default;
};

struct Diaphragms {
    std::vector<Diaphragm> items;  ///< in creation order
    int nextId = 1;
    bool empty() const { return items.empty(); }
    bool operator==(const Diaphragms&) const = default;
};

const Diaphragm* findDiaphragm(const Diaphragms& d, int id);
const Diaphragm* diaphragmOfNode(const Diaphragms& d, int nodeId);
bool isNodeInDiaphragm(const Diaphragms& d, int nodeId);
/// Reference of the diaphragm of the node ("D1"), empty when it has none or its reference is empty.
std::string diaphragmNodeLabel(const Diaphragms& d, int nodeId);
/// "Diafragma : D1" (second line of the tooltip of a node); empty when the node is in no diaphragm.
std::string diaphragmTooltipLine(const Diaphragms& d, int nodeId);

/// Cota of the diaphragm: mean of the Z of its (existing) nodes; nullopt without nodes (REA FUN_1807eb6a0).
std::optional<double> diaphragmElevation(const Diaphragm& diaphragm, const StructuralModel& model);
/// True when every pair of consecutive nodes differs in Z by at most kDiaphragmSameElevationM (message #375642 otherwise, at calculation time).
bool diaphragmNodesSameElevation(const Diaphragm& diaphragm, const StructuralModel& model);
/// Nearest diaphragm elevation below / above `z` (strictly, with 1e-5 tolerance) among `elevations` (REA FUN_1807eba40).
std::optional<double> diaphragmBelow(const std::vector<double>& elevations, double z);
std::optional<double> diaphragmAbove(const std::vector<double>& elevations, double z);

// --- messages (literal es-ES texts of the original) ---

/// 184611, 375635, 375636, 375637, 375638 (%lu replaced by `argument`), 375640, 375642, 375648 (%s replaced by `argument`), 375658, 404575.
/// Empty for another id.
std::string diaphragmMessage(int messageId, const std::string& argument = {});
/// «La referencia no puede quedar vacía.» (measured; id not located).
std::string diaphragmEmptyReferenceMessage();
/// «No se ha definido ningún diafragma.» (#184611).
std::string diaphragmNoneDefinedMessage();
/// «No se ha definido ningún nivel» (measured, Generar por niveles without levels).
std::string diaphragmNoLevelsMessage();

struct DiaphragmIssue {
    int messageId = 0;  ///< original id (0 = measured text without a located id)
    std::string text;
};

// --- dialog "Editar" ---

/// New row of the dialog: empty reference, magenta, no nodes, no loads, id 0.
Diaphragm newDiaphragm();
/// Duplicate: same reference, colour and loads, WITHOUT nodes, id 0 (REA FUN_1807e8f50).
Diaphragm duplicateDiaphragm(const Diaphragm& source);
/// New load row: first load case of the work (`firstLoadCaseId`), everything else 0.000 (measured).
DiaphragmLoad defaultDiaphragmLoad(int firstLoadCaseId);
/// Check made when pressing "+" with rows already there: any empty reference -> «La referencia no puede quedar vacía.» (measured).
std::optional<DiaphragmIssue> checkDiaphragmAdd(const std::vector<Diaphragm>& rows);
/// Check made on Aceptar: any empty reference -> #375658; repeated reference -> #375648 (first repeat; measured at Aceptar). Order of the
/// two checks not measured (TODO(medir)).
std::optional<DiaphragmIssue> checkDiaphragmAccept(const std::vector<Diaphragm>& rows);

// --- node assignment ---

enum class DiaphragmStatus { Ok, Silent, Error };
struct DiaphragmCheck {
    DiaphragmStatus status = DiaphragmStatus::Ok;
    int messageId = 0;
    std::string text;
};

/// Selection as the tool captures it: existing nodes, duplicates dropped keeping the first occurrence.
std::vector<int> diaphragmSelection(const Project& project, const std::vector<int>& nodeIds);
/// Checks of the assignment: no node or unknown diaphragm -> Silent, otherwise Ok. REA (diafragmas.md sec. 9.4): #375640 (external restraint),
/// #375642 (different elevations) and #404575 (ties) are NOT assignment dialogs but errors of the calculation check; the assignment itself
/// accepts the node (and strips its ties: assignNodesToDiaphragm). Nodes at different elevations are accepted (measured).
DiaphragmCheck checkDiaphragmAssign(const Project& project, int diaphragmId, const std::vector<int>& nodeIds);

// --- pure edits on the data (used by the commands; exposed for tests) ---

/// Assigns the nodes to the diaphragm: they leave any other diaphragm first (one node, one diaphragm).
void applyDiaphragmAssign(Diaphragms& d, int diaphragmId, const std::vector<int>& nodeIds);
/// "Eliminar asignación": the nodes leave their diaphragm (the diaphragm stays even when it has no nodes left).
void applyDiaphragmUnassign(Diaphragms& d, const std::vector<int>& nodeIds);
/// Deleted / merged nodes leave their diaphragms.
void applyDiaphragmNodesDeleted(Diaphragms& d, const std::vector<int>& nodeIds);

// --- generation by levels ---

/// Real algorithm (REA FUN_18017e240, diafragmas.md sec. 9.2). Levels sorted by increasing elevation (tolerance 0.001); one diaphragm per level
/// EXCEPT the lowest, starting from the highest (i = 1 is the highest level). Reference = level name; a repeated name gets "_1", "_2", ... (the
/// first processed keeps the plain name). Nodes: every node whose Z equals the level (0.001). Shells: those with ALL their nodes there. No
/// filter by restraints or ties. Colour: idColor = (i < 24 ? 12 : 11) + (i % 24) * 10 -> idColorToRgb. Ids are assigned from `firstId`.
std::vector<Diaphragm> planDiaphragmsByLevels(const Project& project, int firstId = 1);
/// idcolor index used by "Generar por niveles" for the i-th diaphragm (i >= 1, 1 = highest level).
int diaphragmLevelColorId(int i);
/// SINGLE SUBSTITUTION POINT: RGB (0xRRGGBB) of the original's palette entry `idcolor_colorRGB(id)`. PROVISIONAL (TODO(medir): the RGB table is
/// not decoded, diafragmas.md sec. 9.7.1): 13 is the measured magenta; the rest is a placeholder hue ramp.
unsigned idColorToRgb(int idColor);

/// Validation of the stored data: ids unique and < nextId, node in one diaphragm only (nodes that no longer exist are tolerated: not saved). Empty when valid.
std::string validateDiaphragms(const Diaphragms& d, const StructuralModel& model);

}  // namespace cc::core
