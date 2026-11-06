#pragma once
#include <array>
#include <map>
#include <string>
#include <vector>

#include "core/structure_model.h"

namespace cc::core {

/// Node supports (T-P6-01, core part): external restraint ("Vinculación exterior") and internal link ("Vinculación interior") of a node.
/// Behaviour source: docs/research/ui/vinculaciones.md (REA of ed3dlib.dll: ed3tipo_vinculacion_externa_t, ed3vincext_t, ed3vincintgen).
///
/// Public API (for the GUI layer)
///  - Data: Project::nodeSupports (NodeSupports): external / internal maps keyed by node id. A node without an entry is "libre"
///    (no external restraint) and "empotrado" (default internal link). Free / Fixed entries are never stored.
///  - Types: ExternalSupportType (20 values, the integer value is the index of the original enum), DofMode, SupportSense,
///    GenericDisplacementMode, GenericSupportData, ExternalSupport, PrescribedDisplacement, InternalLinkType, InternalLink.
///  - Semantics: restraintOf(ExternalSupport) -> Restraint (global DOFs blocked, elastic ones, oblique line / plane for a user vector that is
///    not parallel to a global axis); supportTypeInfo(type) -> kind / axis / rotations; isSupported(project, nodeId); constrainedDofs(project, nodeId).
///  - Validation (pure): checkExternalSupport(support) -> issues with the literal messages of the original; elasticConstantIsDefault();
///    checkInternalLink(link, elementCount); checkGenericInternalSelection(counts); elementCountAtNode(model, nodeId).
///  - Undoable edits (node_support_edit.h): assignExternalSupport / removeExternalSupport / assignInternalLink / removeInternalLink.
///  - Persistence: JSON section "node_supports" (absent in older files = empty; missing keys take defaults).
///
/// Degrees of freedom are always ordered Dx, Dy, Dz, Gx, Gy, Gz, in global axes.
/// PROVISIONAL / NOT MEASURED (TODO(medir)): units and range of the rotational spring constants (stored kN.m/rad by inference), the
/// meaning of the per-DOF modes of a generic restraint while it is dependent on a line or plane (the modes are kept but ignored: the vector decides),
/// (the default partial fixity coefficient is 0.00, measured), what counts as an "element" of a node for the
/// generic internal link (bars + shells are counted), prescribed displacements (units mm / mRad measured; they depend on load cases, T-P7-01).

/// The 20 values of ed3tipo_vinculacion_externa_t (REA). The integer value is the index of the original enum.
enum class ExternalSupportType {
    Free = 0,                ///< vinc_ext_libre
    Pinned = 1,              ///< vinc_ext_articulada: displacements blocked, rotations free
    Generic = 2,             ///< vinc_ext_generica: user defined
    Fixed = 3,               ///< vinc_ext_empotrada: displacements and rotations blocked
    LineXFixed = 4,          ///< free along X, blocked in the YZ plane; rotations blocked
    LineYFixed = 5,
    LineZFixed = 6,
    LineUserFixed = 7,       ///< free along a user vector
    PlaneXYFixed = 8,        ///< free in the XY plane, blocked along Z; rotations blocked
    PlaneXZFixed = 9,
    PlaneYZFixed = 10,
    PlaneUserFixed = 11,     ///< free in a plane given by its normal vector
    LineXPinned = 12,        ///< same as 4..11 with the rotations free
    LineYPinned = 13,
    LineZPinned = 14,
    LineUserPinned = 15,
    PlaneXYPinned = 16,
    PlaneXZPinned = 17,
    PlaneYZPinned = 18,
    PlaneUserPinned = 19
};
inline constexpr int kExternalSupportTypeCount = 20;

/// Every value of the enumeration in index order (for the 20-button grid).
const std::array<ExternalSupportType, kExternalSupportTypeCount>& allExternalSupportTypes();

enum class SupportKind { Free, Pinned, Generic, Fixed, Line, Plane };

/// Static description of a type.
struct SupportTypeInfo {
    SupportKind kind = SupportKind::Free;
    int axis = -1;               ///< 0 = X, 1 = Y, 2 = Z for line / plane types about a global axis; -1 = user vector or not applicable
    bool userVector = false;     ///< the displacement freedom is given by the user vector
    bool rotationsFixed = false; ///< "empotrada" family (false for the "articulada" family)
};
SupportTypeInfo supportTypeInfo(ExternalSupportType type);
/// Radio value of the original (0..19).
inline int supportTypeIndex(ExternalSupportType type) { return static_cast<int>(type); }

/// "Libre / Fijo / Elástico" radios of the generic restraint.
enum class DofMode { Free, Fixed, Elastic };
/// Combo "Sentido" of the elastic support, measured in the original: Ambos (default), Positivo, Negativo.
enum class SupportSense { Both, Positive, Negative };
/// Radios "Desplazamientos" of the generic restraint: Independientes (#65009), Sobre una recta (#65366), Sobre un plano (#65367).
enum class GenericDisplacementMode { Independent, OnLine, OnPlane };

/// Default spring constant of the elastic support (kN/m for displacements, kN.m/rad for rotations), measured in the original.
inline constexpr double kDefaultElasticConstant = 100000.0;
/// Range of the integer constant "K" (measured: "El valor de 'K' debe estar entre 1 y 10000000.").
inline constexpr double kMinElasticConstant = 1.0;
inline constexpr double kMaxElasticConstant = 10000000.0;
/// Range of the vector fields of the dialog (tooltip "Debe estar entre -1000.000 y 1000.000 .") and of "Coef.Emp.Parc." (0.00 .. 10000000.00).
inline constexpr double kMaxVectorComponent = 1000.0;
inline constexpr double kMaxPartialCoefficient = 10000000.0;

struct GenericSupportData {
    GenericDisplacementMode mode = GenericDisplacementMode::Independent;
    std::array<DofMode, 3> translations{DofMode::Free, DofMode::Free, DofMode::Free};  ///< Dx, Dy, Dz (Independent mode only)
    Point3 vector{0.0, 0.0, 1.0};  ///< director vector of the line / normal vector of the plane (OnLine / OnPlane)
    std::array<DofMode, 3> rotations{DofMode::Free, DofMode::Free, DofMode::Free};  ///< Gx, Gy, Gz
    bool operator==(const GenericSupportData&) const = default;
};

/// Prescribed displacements / rotations of one load case ("hipótesis"). `loadCaseId` is opaque here: the load cases arrive with T-P7-01.
/// Values in metres / radians (provisional units, TODO(medir)); only the constrained DOFs are taken into account.
struct PrescribedDisplacement {
    int loadCaseId = 0;
    std::array<double, 6> values{};
    bool operator==(const PrescribedDisplacement&) const = default;
};

struct ExternalSupport {
    ExternalSupportType type = ExternalSupportType::Free;
    GenericSupportData generic;           ///< used when type == Generic
    Point3 userVector{0.0, 0.0, 1.0};     ///< director vector (user line) or normal vector (user plane) of the predefined user types
    /// Predefined types (and the dependent modes of the generic one): the blocked DOFs whose radio is "Elástico" instead of "Fijo".
    /// For the generic independent mode the springs come from the DofMode of each DOF and this array is ignored.
    std::array<bool, 6> elasticDofs{};
    std::array<double, 6> elasticConstants{kDefaultElasticConstant, kDefaultElasticConstant, kDefaultElasticConstant,
                                           kDefaultElasticConstant, kDefaultElasticConstant, kDefaultElasticConstant};  ///< K per DOF
    std::array<SupportSense, 6> senses{SupportSense::Both, SupportSense::Both, SupportSense::Both,
                                       SupportSense::Both, SupportSense::Both, SupportSense::Both};  ///< "Sentido" per DOF
    std::vector<PrescribedDisplacement> prescribed;  ///< at most one set per load case
    bool operator==(const ExternalSupport&) const = default;
};

/// Mechanical meaning of an external support in global axes.
struct Restraint {
    enum class Oblique { None, LineFree, PlaneFree };
    std::array<bool, 6> fixed{};    ///< DOF completely blocked (a spring counts as blocked: see `elastic`)
    std::array<bool, 6> elastic{};  ///< subset of `fixed` held by a spring instead of rigidly
    /// A user vector that is not parallel to a global axis: translations are restricted along a skew direction. LineFree = free only along
    /// `direction`; PlaneFree = free in the plane normal to `direction`. In that case `fixed` has no translation flag set.
    Oblique oblique = Oblique::None;
    Point3 direction{};             ///< unit vector, only when oblique != None
    bool any() const;               ///< any blocked DOF or oblique restriction
};

/// Restraint of a support. A user vector within 1e-9 of a global axis behaves exactly as the corresponding axis type.
Restraint restraintOf(const ExternalSupport& support);

/// Internal link ("Vinculación interior") enumeration: Nudo empotrado (#65468, default), con coeficiente de empotramiento parcial para
/// todas las piezas (#65470), articulado (#65469), genérico (#65471).
enum class InternalLinkType { Fixed, PartialFixity, Pinned, Generic };

/// One row ("Vínculo n") of the generic table.
struct GenericLinkRow {
    bool partialToNode = false;   ///< checkbox of partial fixity to the node (red line in the sketch)
    double coefficient = 0.0;     ///< "Coef.Emp.Parc." 0..10000000 (0 = pinned, default measured); without the checkbox it has no effect
    bool operator==(const GenericLinkRow&) const = default;
};

struct InternalLink {
    InternalLinkType type = InternalLinkType::Fixed;
    double partialCoefficient = 0.0;  ///< PartialFixity: kN.m/rad for all pieces, 0..10000000 (0 = pinned; default measured)
    /// Generic: one entry per element (bar / shell) at the node, in order: the index of its row. Elements with the same row are fixed to
    /// each other and pinned to the rest. rows.size() = number of rows ("Vínculo 0, 1, ...").
    std::vector<int> elementRow;
    std::vector<GenericLinkRow> rows;
    bool operator==(const InternalLink&) const = default;
};

/// Supports of a project. Sorted maps keep the saved files deterministic.
struct NodeSupports {
    std::map<int, ExternalSupport> external;  ///< only nodes with a restraint (type != Free)
    std::map<int, InternalLink> internal;     ///< only nodes with a link other than the default (type != Fixed)
    bool empty() const { return external.empty() && internal.empty(); }
    bool operator==(const NodeSupports&) const = default;
};

// --- queries (free functions on NodeSupports + StructuralModel so they do not depend on Project) ---

/// The external support of a node, or nullptr when it is free (or the node does not exist).
const ExternalSupport* externalSupportOf(const StructuralModel& model, const NodeSupports& supports, int nodeId);
/// True when the node exists and has any blocked DOF (an oblique restriction counts).
bool isSupported(const StructuralModel& model, const NodeSupports& supports, int nodeId);
/// Blocked DOFs of the node in global axes (Dx..Gz); all false for a free or unknown node. An oblique restriction sets no flag: use restraintOf.
std::array<bool, 6> constrainedDofs(const StructuralModel& model, const NodeSupports& supports, int nodeId);
/// Internal link of a node (the default "empotrado" when none is stored).
InternalLink internalLinkOf(const NodeSupports& supports, int nodeId);
/// Elements (bars + shells) that arrive at the node.
int elementCountAtNode(const StructuralModel& model, int nodeId);

// --- validation (pure) ---

enum class IssueSeverity { Error, Warning, Question };
struct SupportIssue {
    int messageId = 0;   ///< id of the original string table (ed3dlib)
    IssueSeverity severity = IssueSeverity::Error;
    std::string text;    ///< literal es-ES text of the original (TODO(medir) texts are marked in the code)
};

/// Literal es-ES texts of the original (strings table ids): 65006, 65010, 65465, 65466, 65467, 65473. Empty for another id.
std::string supportMessage(int messageId);

/// Checks on pressing Aceptar. Error = blocks (K outside 1..10000000 "El valor de 'K' debe estar entre 1 y 10000000."; zero user vector
/// "La dirección introducida no es correcta."; prescribed sets with duplicated load case #65465 or all-zero #65466). Warning = accepted but
/// values are ignored (#65467). A generic restraint with nothing blocked is NOT an error: the original closes silently and the node stays free
/// (#65006 was not reproduced), so the assignment command treats it as "libre".
/// The question #65010 depends on the entered K text: see elasticConstantIsDefault.
std::vector<SupportIssue> checkExternalSupport(const ExternalSupport& support);
/// True when the support is elastic and some K equals kDefaultElasticConstant: the dialog then asks #65010 (Sí / No).
bool elasticConstantIsDefault(const ExternalSupport& support);
/// Text of #65010 with %s replaced by `constantText`; the original always writes the literal "K" (also for rotations).
std::string elasticDefaultQuestion(const std::string& constantText = "K");
/// Prescribed values that survive: the values of unblocked DOFs are zeroed (#65467 says they are not taken into account).
std::vector<PrescribedDisplacement> effectivePrescribed(const ExternalSupport& support);

/// Checks an internal link for a node with `elementCount` elements (0 = do not check the table size). Generic: every element has exactly one
/// valid row, at least one element, coefficients >= 0 and finite. Messages without original id (TODO(medir)) have messageId 0.
std::vector<SupportIssue> checkInternalLink(const InternalLink& link, int elementCount = 0);
/// #65473 when the nodes of the selection do not all have the same number of elements (an empty list when they do).
std::vector<SupportIssue> checkGenericInternalSelection(const std::vector<int>& elementCounts);

bool hasError(const std::vector<SupportIssue>& issues);

/// Internal consistency of the stored data (used by the file validation); empty when valid.
std::string validateSupports(const NodeSupports& supports);

}  // namespace cc::core
