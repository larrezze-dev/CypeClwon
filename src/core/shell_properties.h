#pragma once
#include <array>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "core/node_support.h"
#include "core/structure_model.h"

namespace cc::core {

/// Shell properties (T-P6-07, core part): "Propiedades > Laminas" = Discretizacion, Factores de rigidez, Bandas de integracion, Vinculacion
/// interior / exterior de aristas, Pandeo and Analisis modal. Behaviour source: docs/research/ui/propiedades_laminas.md (REA of ed3dlib.dll +
/// measurements of the original).
///
/// Public API (for the GUI layer)
///  - Data: Project::shellProps (ShellProperties). Per shell (map shellId -> ShellRecord, only shells whose record differs from the default):
///    `maxTriangleSideM` (def. 1.00 m, 0.01..100.00), `stiffness` (8 factors, def. 1.000, 0..100), `bands` (integration bands in the local
///    2D system of the shell), `bucklingConsidered` (def. false) and `modalConsidered` (def. true). Per edge (key = shell + the two node
///    ids, normalised so that nodeA < nodeB): interior bond (map, def. "Empotrada" = not stored) and exterior bond (reuses ExternalSupport
///    of node_support.h, def. "Libre" = not stored). Per project: `bandForce` (BandForceOptions: method + smoothing).
///  - Local 2D system of a shell (shellFrame / shellContour2D): origin = first contour node, axes = shellLocalAxes (x, y); metres.
///  - Pure validation: checkTriangleSize, checkStiffnessFactors, checkBandWidth, checkEdgeInterior (exterior: checkExternalSupport of
///    node_support.h), checkBand; messages are the literals of the original (ShellPropIssue::messageId = original id, 0 = no original text).
///  - Pure band operations on one shell (band list + 2D contour): newBand, editBands, deleteBands, moveBandEnd, joinBands, divideBands,
///    generateBands. All return BandResult { issue, bands } and never mutate their input.
///  - Undoable edits (shell_properties_edit.h): assignShellDiscretisation / assignShellStiffness / assignShellBands / assignEdgeInterior /
///    assignEdgeExterior / assignShellBuckling / assignShellModal / assignBandForceOptions.
///  - Integration with structure edits: reconcileShellProperties (SnapshotCommand, DeleteNodesCommand and MoveNodeCommand call it).
///  - Info-panel lines ("ficha"): shellInfoLines, edgeInteriorLine.
///  - Persistence: JSON section "shell_properties" (absent in older files = empty; missing keys take the defaults).
///
/// PROVISIONAL / NOT MEASURED (TODO(medir)): origin of the local 2D system (first contour node); minimum band length (#64191, value
/// unknown: kBandMinLengthM) and its text; maximum / default of "Numero de divisiones" (min 2, max kBandMaxDivisions); range of the elastic
/// constant K of an interior bond (same as the node supports); the behaviour of "Unir" for parallel bands that are not collinear (here: one
/// band along the axis of the first selected band covering all of them, the widest width); what "Mover extremo" does with a point outside the
/// shell (here: error #64190); the copy of bands to the other shells of a multiple edit (raw 2D coordinates, those that do not fit in the
/// target shell are dropped); the inheritance of bands / edge bonds when a shell is divided, joined or moved (world position preserved, bands
/// that no longer fit are dropped); text of the info panel for modal = false and for non-unit stiffness factors (formatted with 3 decimals
/// here); the number of bands in the info panel; how the zone of "Generar" is clipped when it is not a parallelogram (here: each stripe is
/// clipped to the zone and to the shell, stripes shorter than the minimum are skipped).

// --- ranges and defaults ---
inline constexpr double kShellTriangleSideMinM = 0.01, kShellTriangleSideMaxM = 100.0, kShellTriangleSideDefaultM = 1.0;  ///< measured (m)
inline constexpr double kStiffnessFactorMin = 0.0, kStiffnessFactorMax = 100.0, kStiffnessFactorDefault = 1.0;          ///< measured
inline constexpr double kBandWidthMinM = 0.01, kBandWidthMaxM = 100.0, kBandWidthDefaultM = 0.15;                       ///< 1..10000 cm, def. 15 cm (measured)
/// Provisional minimum length of a band (m): "Nueva" silently rejects shorter ones; measured between 0.16 and 0.17 m (docs 11.3). TODO(medir): the exact
/// value and the text of #64191 (never shown by the original in the flows tried).
inline constexpr double kBandMinLengthM = 0.16;
inline constexpr int kBandMinDivisions = 2, kBandMaxDivisions = 10, kBandDefaultDivisions = 2;  ///< "Numero de divisiones", measured (docs 11.2)
inline constexpr int kBandForcePointsMin = 3, kBandForcePointsMax = 1000, kBandForcePointsDefault = 10;  ///< "Numero de puntos de esfuerzo" (docs 11.1)
inline constexpr double kEdgeElasticConstantDefault = 100000.0;  ///< interior bond, kN/m2 (displacements) or kN.m/rad.m (rotations), measured
inline constexpr double kShellGeomToleranceM = 1e-6;

/// 2D point in the local system of a shell (metres).
struct Point2 {
    double x = 0.0, y = 0.0;
    bool operator==(const Point2&) const = default;
};

/// The eight stiffness factors. Dialog order (top to bottom): Membrana Fxx, Fxy, Fyy; Flexion Mxx, Mxy, Myy; Cortante Vxz, Vyz.
struct ShellStiffnessFactors {
    double fxx = 1.0, fxy = 1.0, fyy = 1.0;
    double mxx = 1.0, mxy = 1.0, myy = 1.0;
    double vxz = 1.0, vyz = 1.0;
    double value(int dialogIndex) const;  ///< 0..7 in dialog order
    void set(int dialogIndex, double v);
    bool operator==(const ShellStiffnessFactors&) const = default;
};
inline constexpr int kStiffnessFactorCount = 8;
/// Field label of the dialog / range message: "Membrana Fxx", ..., "Cortante Vyz" (group + coefficient, dialog order).
std::string stiffnessFieldLabel(int dialogIndex);

/// Integration band: centre line (x1,y1)-(x2,y2) in the local 2D system (stored with the first end lexicographically <= the second) and its width.
struct ShellIntegrationBand {
    double x1 = 0, y1 = 0, x2 = 0, y2 = 0;
    double widthM = kBandWidthDefaultM;
    int forcePoints = kBandForcePointsDefault;  ///< "Numero de puntos de esfuerzo" (3..1000, default 10)
    double length() const;
    bool operator==(const ShellIntegrationBand&) const = default;
};
/// Orders the ends lexicographically (REA: ed3math_compara_puntos2D == PRIMERO_MENOR).
ShellIntegrationBand normaliseBand(ShellIntegrationBand band);

enum class EdgeInteriorKind { Free, Fixed, Pinned, Generic };  ///< Libre, Empotrada (default), Articulada, Generica
/// Interior bond of an edge. For `Generic`, dof[i] / k[i] (Dx Dy Dz Gx Gy Gz in the local axes of the edge, x along the edge, z = z of the
/// shell) apply; k only when the mode is Elastic. For the other kinds dof / k are kept at their defaults (normaliseEdgeInterior).
struct EdgeInteriorBond {
    EdgeInteriorKind kind = EdgeInteriorKind::Fixed;
    std::array<DofMode, 6> dof{DofMode::Free, DofMode::Free, DofMode::Free, DofMode::Free, DofMode::Free, DofMode::Free};
    std::array<double, 6> k{kEdgeElasticConstantDefault, kEdgeElasticConstantDefault, kEdgeElasticConstantDefault,
                            kEdgeElasticConstantDefault, kEdgeElasticConstantDefault, kEdgeElasticConstantDefault};
    bool operator==(const EdgeInteriorBond&) const = default;
};
/// Resets dof / k of a non-generic bond, and k of a non-elastic generic DOF, to their defaults (so that equal bonds compare equal).
EdgeInteriorBond normaliseEdgeInterior(EdgeInteriorBond bond);
bool isDefaultEdgeInterior(const EdgeInteriorBond& bond);  ///< "Empotrada"

/// Edge of a shell: the pair of contour nodes, normalised nodeA < nodeB.
struct ShellEdgeKey {
    int shellId = 0, nodeA = 0, nodeB = 0;
    auto operator<=>(const ShellEdgeKey&) const = default;
};
ShellEdgeKey makeEdgeKey(int shellId, int node1, int node2);
/// Edges of a shell in contour order (the key of edge i joins contour nodes i and i+1). Empty for an unknown shell.
std::vector<ShellEdgeKey> shellEdges(const StructuralModel& model, int shellId);

/// "Integracion de esfuerzos en bandas": origin of the internal forces of the bands. Default: internal stresses, smoothed.
enum class BandForceMethod { InternalStresses, NodalForces };
struct BandForceOptions {
    BandForceMethod method = BandForceMethod::InternalStresses;
    bool smoothedStresses = true;  ///< only applicable to InternalStresses
    bool operator==(const BandForceOptions&) const = default;
};

struct ShellRecord {
    double maxTriangleSideM = kShellTriangleSideDefaultM;
    ShellStiffnessFactors stiffness;
    std::vector<ShellIntegrationBand> bands;
    bool bucklingConsidered = false;  ///< "Considerar en el analisis de pandeo"
    bool modalConsidered = true;      ///< "Considerar en el analisis" (modal)
    bool operator==(const ShellRecord&) const = default;
};

struct ShellProperties {
    std::map<int, ShellRecord> shells;                       ///< only shells whose record differs from the default
    std::map<ShellEdgeKey, EdgeInteriorBond> interior;       ///< only edges with a bond other than "Empotrada"
    std::map<ShellEdgeKey, ExternalSupport> exterior;        ///< only edges with a restraint (anything blocked)
    BandForceOptions bandForce;
    bool empty() const { return shells.empty() && interior.empty() && exterior.empty() && bandForce == BandForceOptions{}; }
    bool operator==(const ShellProperties&) const = default;
};

bool isDefaultShellRecord(const ShellRecord& record);
ShellRecord shellRecordOf(const ShellProperties& data, int shellId);                ///< the default when none is stored
void setShellRecord(ShellProperties& data, int shellId, const ShellRecord& record);  ///< erases the entry when it is the default
EdgeInteriorBond edgeInteriorOf(const ShellProperties& data, const ShellEdgeKey& key);  ///< "Empotrada" when none is stored
ExternalSupport edgeExteriorOf(const ShellProperties& data, const ShellEdgeKey& key);   ///< "Libre" when none is stored
void setEdgeInterior(ShellProperties& data, const ShellEdgeKey& key, const EdgeInteriorBond& bond);
void setEdgeExterior(ShellProperties& data, const ShellEdgeKey& key, const ExternalSupport& support);  ///< erases when nothing is blocked

// --- validation ---
struct ShellPropIssue {
    int messageId = 0;  ///< original id (0: the clone's own wording)
    std::string text;
    bool operator==(const ShellPropIssue&) const = default;
};
/// "El valor de '<field>' debe estar entre <lo> y <hi>." generic helper (no units).
std::string shellRangeMessage(const std::string& field, const std::string& lo, const std::string& hi);
/// "El valor de 'Tamano maximo de los lados de un triangulo' debe estar entre 0.01 m y 100.00 m." (measured, literal).
std::string triangleSizeRangeMessage();
/// "El valor de '<Grupo> <Coef>' debe estar entre 0.000  y 100.000 ." (measured literal: double space and a space before the full stop).
std::string stiffnessRangeMessage(const std::string& field);
/// "El valor de 'Anchura' debe estar entre 1.0 y 10000.0." (cm, measured).
std::string bandWidthRangeMessage();

/// "El valor de '' debe estar entre 3  y 1000 ." (measured literal: the label is NOT interpolated, empty quotes, double space, space before the dot).
std::string bandForcePointsRangeMessage();
/// "El valor de 'Número de divisiones' debe estar entre 2  y 10 ." (measured literal: double space after the 2 and a space before the dot).
std::string bandDivisionsRangeMessage();
std::optional<ShellPropIssue> checkBandForcePoints(int points);

std::optional<ShellPropIssue> checkTriangleSize(double sideM);
/// First factor (dialog order) outside [0, 100] or not finite.
std::optional<ShellPropIssue> checkStiffnessFactors(const ShellStiffnessFactors& factors);
/// Width in metres; the dialog limits are 1.0 .. 10000.0 cm.
std::optional<ShellPropIssue> checkBandWidth(double widthM);
/// Generic bond: every Elastic DOF needs a finite K in [kMinElasticConstant, kMaxElasticConstant] (TODO(medir): range of K on edges).
std::optional<ShellPropIssue> checkEdgeInterior(const EdgeInteriorBond& bond);

// --- local 2D system ---
struct ShellFrame {
    Point3 origin;
    Point3 axisX, axisY, axisZ;  ///< local axes of the shell (shellLocalAxes)
    Point2 toLocal(const Point3& world) const;
    Point3 toWorld(const Point2& local) const;
    double heightOf(const Point3& world) const;  ///< signed distance to the plane of the shell
};
std::optional<ShellFrame> shellFrame(const StructuralModel& model, const Shell& shell);
/// Contour corner points in the local system (empty when the frame cannot be resolved).
std::vector<Point2> shellContour2D(const StructuralModel& model, const Shell& shell);
bool pointInPolygon2D(const Point2& p, const std::vector<Point2>& polygon, double toleranceM = kShellGeomToleranceM);
/// The four corners of the rectangle of a band (the centre line is the middle axis).
std::array<Point2, 4> bandCorners(const ShellIntegrationBand& band);

// --- band operations on one shell ---
struct BandResult {
    std::optional<ShellPropIssue> issue;
    std::vector<ShellIntegrationBand> bands;  ///< the whole new list of the shell (empty on error)
    bool ok() const { return !issue.has_value(); }
};
/// #64190 when the rectangle is not completely inside the contour, #64191 when the centre line is shorter than the minimum, width range.
/// `checkLength` = false skips #64191: the original keeps shorter bands made with "Mover extremo" and only flags them as incidences.
std::optional<ShellPropIssue> checkBand(const std::vector<Point2>& contour, const ShellIntegrationBand& band, bool checkLength = true);
/// "Nueva": band between two points inside the shell with the given width (metres).
BandResult newBand(const std::vector<Point2>& contour, const std::vector<ShellIntegrationBand>& current, const Point2& p1, const Point2& p2,
                   double widthM);
/// "Editar": the listed bands (indices into `current`) take the width and, when given, the number of force points. Unknown indices are ignored;
/// no valid index -> issue 0.
BandResult editBands(const std::vector<Point2>& contour, const std::vector<ShellIntegrationBand>& current, const std::vector<int>& indices,
                     double widthM, std::optional<int> forcePoints = std::nullopt);
/// "Borrar".
BandResult deleteBands(const std::vector<ShellIntegrationBand>& current, const std::vector<int>& indices);
/// "Mover extremo": end 0 or 1 of the band (after normalisation) goes to `to`.
BandResult moveBandEnd(const std::vector<Point2>& contour, const std::vector<ShellIntegrationBand>& current, int index, int end, const Point2& to);
/// "Unir": #63999 when the selected bands are not parallel; at least two bands. Parallel bands become ONE band centred between them: length = union of
/// the projections along the direction, width = outer separation + width (measured: 0.61 + 0.15 = 0.76 m). TODO(medir): unequal widths (here: the
/// extent of the rectangles across the direction) and the force points (here: those of the first selected band).
BandResult joinBands(const std::vector<Point2>& contour, const std::vector<ShellIntegrationBand>& current, const std::vector<int>& indices);
/// "Dividir": each selected band is cut into `parts` (>= 2) equal stretches of the same width.
BandResult divideBands(const std::vector<Point2>& contour, const std::vector<ShellIntegrationBand>& current, const std::vector<int>& indices,
                       int parts);
/// "Generar bandas de integracion en zona": the zone is the quadrilateral z[0..3]; contiguous parallel bands of the given width, along the
/// first side (z0-z1), stacked from that side towards z2 with step = width (the number of bands is floor(extent / width)), each clipped to
/// the zone and the shell. #65158 (not a quadrilateral), #65159 (zone not wider than the band), #64476 (nothing generated).
BandResult generateBands(const std::vector<Point2>& contour, const std::vector<ShellIntegrationBand>& current, const std::array<Point2, 4>& zone,
                         double widthM);

// --- info panel ("ficha") ---
/// Lines of the right-hand info panel of a shell: "Membrana Fxx : 1", ..., "Cortante Vyz : 1" (factors; values formatted with %g like the
/// measured "1"), "Tamano maximo de los lados de un triangulo : 1.00 m", "Analisis de pandeo" / "No se considera en el analisis",
/// "Analisis modal" / "Se considera en el analisis". Order of the original panel list (propiedades_laminas.md section 7).
enum class ShellInfoPart { Stiffness, Discretisation, Buckling, Modal };
std::vector<std::string> shellInfoLines(const ShellRecord& record, ShellInfoPart part);
/// "Vinc. interior : Empotrada" (Libre / Empotrada / Articulada / Generica).
std::string edgeInteriorLine(const EdgeInteriorBond& bond);
std::string edgeInteriorKindName(EdgeInteriorKind kind);

/// Internal consistency of the stored data (used by the file validation); empty when valid. Entries of shells / edges that no longer exist are
/// tolerated (they are not saved: deleting a shell leaves its data for undo).
std::string validateShellProperties(const ShellProperties& data);

/// Reconciles the data after an edit of the structure (SnapshotCommand, DeleteNodes, MoveNode): entries of shells that disappeared are
/// dropped; a new shell born from the division of another (its centroid lies in a shell of `before`) inherits the record; a shell that
/// absorbs shells that vanished (union) keeps its own record and receives their bands; bands and edge bonds follow the world geometry (bands
/// that no longer fit in the new contour are dropped; an edge inherits the bond of a source edge that contains it).
ShellProperties reconcileShellProperties(const ShellProperties& data, const StructuralModel& before, const StructuralModel& after);

}  // namespace cc::core
