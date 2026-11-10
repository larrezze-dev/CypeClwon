#pragma once
#include <algorithm>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "core/load_case.h"
#include "core/nodal_load.h"
#include "core/shell_properties.h"
#include "core/structure_model.h"

namespace cc::core {

/// Panels ("Paños") and surface loads ("Cargas superficiales"), T-P7-05, core part. Behaviour source: docs/research/ui/cargas_panos.md (sections 1-9 REA,
/// 10-12 measured in the original [CAP]; [CAP] wins over [REA] when they disagree). The "Notas de implementación" at the end of that file list the
/// assumptions that were NOT measured.
///
/// Model. A Panel is a closed planar polygon (3 or more free points, not tied to nodes: the original lets the user pick points on bars, divide with
/// free points and move vertices to free destinations) that carries loads over its whole surface and gives them a direction of distribution (a unit
/// vector in its plane: the arrow of the panel). The loads of a panel are rows (SurfaceLoadData): per load case, uniform or variable with the
/// height ("cota inferior" / "cota superior"; variable ones exist only on non-horizontal panels). A SurfaceLoad is a polygon of its own, in ONE load
/// case, uniform or variable, that belongs to the panel that contains it completely (computed, not stored). Two identical surface loads are allowed
/// and add up. Ids grow and are never reused.
///
/// Public API (for the GUI layer)
///  - Data: Project::panels (Panels) and Project::surfaceLoads (SurfaceLoads {items, nextId, unassignedBars}); drawing scales are the shared ones.
///  - Dialog: SurfaceLoadData (+ surfaceLoadDefaults), surfaceLoadFieldSpec / parseSurfaceLoadField, checkSurfaceLoadData, normaliseSurfaceLoadData.
///  - Geometry: panelFrame, validatePanelContour (+ messages #65130/#65133/#65131/#65134), panelDirectionFromSide, panelSideLines, panelArrow,
///    panelPositiveDirection, panelPositiveSenseText, panelContainsPolygon, panelsContaining, panelLoadDrawing.
///  - Undoable edits: panel_edit.h. Pure reparto on bars: panel_distribution.h.
///  - Cascades: reconcilePanels (SnapshotCommand / DeleteNodesCommand / MoveNodeCommand), applyPanelsCaseDeleted (load_case_edit), applyPanelsDeleted.

inline constexpr double kMinSurfaceLoad = -10000.0, kMaxSurfaceLoad = 10000.0;  ///< kN/m2, 3 decimals (measured; REA +-1000 base units)
inline constexpr int kSurfaceLoadDecimals = 3;
inline constexpr double kSurfaceLoadDefaultValue = 0.0;     ///< new rows / dialogs start at 0.000 (measured; REA found no default)
inline constexpr double kPanelPlaneToleranceM = 0.001;      ///< coplanarity of the contour (same as the shells, [REA] 0x3f50624dd2f1a9fc)
inline constexpr double kPanelGeomToleranceM = 1e-4;        ///< [REA] i_PRECISION4
inline constexpr double kPanelPointToleranceM = 1e-6;       ///< two points closer than this are the same point (node following, duplicates)
inline constexpr double kPanelTickSpacingM = 0.2;           ///< measured: vertical lines about every 0.2 m of the contour
inline constexpr double kPanelMinAreaM2 = 1e-6;             ///< [REA] minimum area

enum class SurfaceLoadType { Uniform = 0, Variable = 1 };  ///< [REA] ed3carsup_tipo_t

/// One load as the dialogs edit it: a row of "Cargas en paño" or the whole dialog of a surface load. Values are SIGNED (kN/m2).
struct SurfaceLoadData {
    int loadCaseId = 0;
    SurfaceLoadType type = SurfaceLoadType::Uniform;
    double value = kSurfaceLoadDefaultValue;  ///< Uniform
    double lower = 0.0;                       ///< Variable: value at the lowest point ("cota inferior")
    double upper = 0.0;                       ///< Variable: value at the highest point ("cota superior")
    bool operator==(const SurfaceLoadData&) const = default;
};
/// A new row / dialog: case = the visible hypothesis, value 0.000 (measured).
SurfaceLoadData surfaceLoadDefaults(int loadCaseId = 0, SurfaceLoadType type = SurfaceLoadType::Uniform);

// --- the dialog fields: labels, ranges, parsing, checks ---

enum class SurfaceLoadField { Value, Lower, Upper };
struct SurfaceLoadFieldSpec {
    std::string label;  ///< "Valor", "Valor en la cota inferior", "Valor en la cota superior"
    double min = kMinSurfaceLoad, max = kMaxSurfaceLoad;
    int decimals = kSurfaceLoadDecimals;
    std::string unit = "kN/m²";
};
SurfaceLoadFieldSpec surfaceLoadFieldSpec(SurfaceLoadField field);
/// «El valor de 'Valor' debe estar entre -10000.000 kN/m² y 10000.000 kN/m².» (measured, an "Aviso").
std::string surfaceLoadRangeMessage(const SurfaceLoadFieldSpec& spec);
/// «No puede introducir cargas de valor nulo.» (#65050, measured, an "Error").
std::string surfaceLoadZeroMessage();
/// «La cadena introducida no es un número.» (shared).
std::string surfaceLoadNotNumberMessage();
/// «Las cargas variables con la altura sólo pueden aplicarse a superficies no horizontales.» (own wording: the original does not offer them).
std::string surfaceLoadVariableOnHorizontalMessage();

struct SurfaceLoadFieldParse {
    bool ok = false;
    double value = 0.0;   ///< rounded to 3 decimals; only when ok
    std::string message;  ///< measured message when !ok
};
SurfaceLoadFieldParse parseSurfaceLoadField(SurfaceLoadField field, const std::string& text);

enum class SurfaceLoadIssueKind { None, Warning, Error };
struct SurfaceLoadIssue {
    SurfaceLoadIssueKind kind = SurfaceLoadIssueKind::None;
    std::string message;
    std::optional<SurfaceLoadField> field;
    bool ok() const { return kind == SurfaceLoadIssueKind::None; }
};
/// Order (measured): range of each field (Aviso), then the null rule (Error, #65050): a uniform value that rounds to zero, a variable pair with BOTH
/// values zero (one null value is accepted: 0 / 6 is valid). Negative values and mixed signs are valid.
SurfaceLoadIssue checkSurfaceLoadData(const SurfaceLoadData& data);
/// Canonical form: values rounded to 3 decimals, the fields that the type does not use reset to 0.
SurfaceLoadData normaliseSurfaceLoadData(SurfaceLoadData data);

// --- geometry of a planar polygon ---

/// Plane of a polygon: origin = first vertex, w = unit normal (right-hand rule of the vertex order), u = unit direction of the first edge, v = w x u.
struct PanelFrame {
    Point3 origin, u, v, w;
    Point2 toLocal(const Point3& world) const;
    Point3 toWorld(const Point2& local) const;
    double heightOf(const Point3& world) const;  ///< signed distance to the plane
};
std::optional<PanelFrame> panelFrame(const std::vector<Point3>& polygon);
/// [REA] ed3geom_es_plano_horizontal: the plane is parallel to the global XY plane.
bool isHorizontalPlane(const PanelFrame& frame);
bool isHorizontalPolygon(const std::vector<Point3>& polygon);
/// Local 2D polygon in the frame of the polygon itself.
std::vector<Point2> polygonLocal(const PanelFrame& frame, const std::vector<Point3>& polygon);
double polygonArea(const std::vector<Point3>& polygon);
Point3 polygonCentroid(const std::vector<Point3>& polygon);
bool pointInsidePolygon(const std::vector<Point3>& polygon, const Point3& p, double toleranceM = kPanelGeomToleranceM);

enum class ContourError { None, TooFewPoints, CoincidentPoints, NotPlanar, Degenerate, SelfIntersecting };
/// A valid contour: 3 or more points, no two coincident, coplanar within 0.001 m, area > 1e-6 m2 and a simple polygon (edges do not cross or touch).
/// What exactly the original refuses is NOT measured except non-planarity (#65133): TODO(medir).
ContourError validatePanelContour(const std::vector<Point3>& points);
/// #65130 «Los puntos introducidos no permiten definir correctamente un paño.» (typed points) / #65133 «Los puntos indicados ...» (picked or moved);
/// #65131 / #65134 the same for «el contorno de una carga superficial».
std::string panelContourMessage(bool forSurfaceLoad, bool indicated);

// --- the data ---

struct Panel {
    int id = 0;
    std::vector<Point3> vertices;  ///< closed polygon (the last point joins the first)
    Point3 direction{1.0, 0.0, 0.0};  ///< unit vector in the plane of the panel: direction of distribution (the arrow)
    std::vector<SurfaceLoadData> loads;  ///< all rows; the "Uniformes" tab shows the uniform ones, "Variables con la altura" the others
    bool operator==(const Panel&) const = default;
};
struct Panels {
    std::vector<Panel> items;  ///< in creation order
    int nextId = 1;
    bool empty() const { return items.empty(); }
    bool operator==(const Panels&) const = default;
};

struct SurfaceLoad {
    int id = 0;
    std::vector<Point3> contour;
    SurfaceLoadData data;
    bool operator==(const SurfaceLoad&) const = default;
};
struct SurfaceLoads {
    std::vector<SurfaceLoad> items;  ///< in creation order
    int nextId = 1;
    /// Bars that "Asignar" switched off: they take no panel nor surface loads (drawn dashed). Tie bars are never in the list.
    std::set<int> unassignedBars;
    bool empty() const { return items.empty() && unassignedBars.empty(); }
    bool operator==(const SurfaceLoads&) const = default;
};

const Panel* findPanel(const Panels& panels, int id);
Panel* findPanel(Panels& panels, int id);
const SurfaceLoad* findSurfaceLoad(const SurfaceLoads& loads, int id);
std::vector<int> surfaceLoadsOfCase(const SurfaceLoads& loads, int loadCaseId);
/// Panels with at least one row in the case.
std::vector<int> panelsWithCase(const Panels& panels, int loadCaseId);
/// Enabled-state rules (measured): panel tools need panels; surface-load tools need surface loads; "Nuevo" / "Nueva" need nodes, "Asignar" nodes too.
inline bool hasPanels(const Panels& panels) { return !panels.items.empty(); }
inline bool hasSurfaceLoads(const SurfaceLoads& loads) { return !loads.items.empty(); }
inline bool surfaceBarAssigned(const SurfaceLoads& loads, int barId) { return loads.unassignedBars.count(barId) == 0; }

// --- direction of distribution ---

enum class PanelDirectionMode { Parallel = 0, Perpendicular = 1 };  ///< [REA] ed3tipo_direccion_reparto_panyo_t; combo "Paralela / Perpendicular al lado seleccionado"
/// Unit vector in the plane of the polygon: parallel to the side `side` (vertex side -> side + 1) or perpendicular to it (in the plane). Null vector
/// when the polygon has no plane or the side does not exist. "Nuevo" and "Nueva carga superficial + paño": side 0, Parallel (first two points).
Point3 panelDirectionFromSide(const std::vector<Point3>& polygon, int side, PanelDirectionMode mode = PanelDirectionMode::Parallel);
/// Sides of the polygon (the "rectas auxiliares" of the direction mode): a -> b for side i.
std::vector<std::pair<Point3, Point3>> panelSideLines(const std::vector<Point3>& polygon);
/// Direction kept valid for a polygon: projects `direction` onto its plane and normalises it; when it is parallel to the normal or null, the first side.
Point3 panelDirectionInPlane(const std::vector<Point3>& polygon, const Point3& direction);
/// Double arrow of the panel: centre = centroid, `from` / `to` along the direction, length half the extent of the polygon along it (at least 0.2 m).
struct PanelArrow {
    Point3 from, to;
    bool valid = false;
};
PanelArrow panelArrow(const Panel& panel);

// --- sense of the load, colours ---

/// [REA, measured] Wind hypotheses act perpendicular to the panel along the positive vector; the others act vertically and a load is positive when it
/// points DOWN. Positive vector of wind: the unit normal turned to +Z (a vertical panel keeps the right-hand normal of its vertex order: NOT measured).
bool isWindCase(const LoadCaseSet& cases, int loadCaseId);
Point3 panelPositiveDirection(const std::vector<Point3>& polygon, bool wind);
/// "Vertical hacia abajo" / "Vertical hacia arriba" (the wind vector of a horizontal panel is +Z) or the vector «(0, -0.447, 0.894)» (3 decimals, zeros
/// trimmed, comma + space): the column «Sentido positivo» of "Cargas en paño" and the row of the surface-load dialog.
std::string panelPositiveSenseText(const std::vector<Point3>& polygon, bool wind);
/// Direction in which a value PUSHES (positive vector times the sign of the value); null vector for a zero value.
Point3 panelLoadActionDirection(const std::vector<Point3>& polygon, bool wind, double value);

/// Colour slot of the hypothesis (measured: by POSITION in the list of additional hypotheses, not by nature): 0 = Peso propio (yellow), 1 = first
/// additional (cyan), 2 = second additional (green). The third and later: NOT measured, ASSUMED to cycle 1, 2, 1, 2...
int panelLoadColourSlot(const LoadCaseSet& cases, int loadCaseId);
/// 0xRRGGBB of a slot: yellow, cyan, green (approximations of the measured colours: see the viewport constants).
unsigned panelLoadSlotRgb(int slot);

// --- drawing ---

/// Scale of the surface loads is the shared "Superficial". Fixed mode: the largest value of the load reaches `fixedSizeM` (measured 0.5 m; wedge 2/6 gives
/// 1/3 .. 1); proportional: 0.1 m per unit x case scale x "Superficial" scale.
double panelLoadDrawSizeM(const LoadScales& scales, int loadCaseId, double value, double maxValueAbs);
double panelLoadMaxSizeM(const LoadScales& scales, const SurfaceLoadData& data);

/// What the drawing of one load needs: the data and the positive direction of its hypothesis (panelPositiveDirection).
struct PanelLoadView {
    SurfaceLoadData data;
    Point3 positiveDirection;
};
/// Several loads on the same panel / polygon, in creation order: the prism of each one lies on the side from which the load comes (the side opposite to
/// the action direction) and the next one with the same side goes BEYOND the previous ones (measured with two equal surface loads: double height), so
/// the offset of load i is the sum of the maximum sizes of the previous ones that share its side.
std::vector<double> panelLoadStackOffsets(const std::vector<PanelLoadView>& loads, const LoadScales& scales);

std::string panelLoadLabelText(double value);  ///< «10 kN/m²» (up to 3 decimals, trailing zeros removed; signed)
struct PanelLoadLabel {
    Point3 anchor;  ///< middle of the vertical edge of the prism at a corner
    std::string text;
};
/// Pure geometry of one load on a polygon: `base` = the polygon moved `stackOffsetM` towards the side of the load; `top` = base moved by the SIGNED
/// height at each corner (a variable load of mixed sign crosses the plane, the "lazo" measured); `ticks` the lines every 0.2 m along the contour; one label
/// at the first corner (uniform) or one per corner (variable).
struct PanelLoadDrawing {
    bool valid = false;
    std::vector<Point3> base, top;
    std::vector<std::pair<Point3, Point3>> ticks;
    std::vector<PanelLoadLabel> labels;
    double sizeM = 0.0;  ///< maximum height
};
PanelLoadDrawing panelLoadDrawing(const PanelLoadView& load, const std::vector<Point3>& polygon, const LoadScales& scales, double stackOffsetM = 0.0);
/// Value of a load at a point of the polygon: uniform = value; variable = linear in the z coordinate between the lowest (lower) and highest (upper)
/// point of the polygon (constant `lower` when the polygon is horizontal).
double surfaceLoadValueAt(const SurfaceLoadData& data, const std::vector<Point3>& polygon, const Point3& p);

// --- containment ---

/// True when every point of `inner` lies in the plane of `outer` (0.001 m) and inside it (tolerance 1e-4).
bool panelContainsPolygon(const std::vector<Point3>& outer, const std::vector<Point3>& inner);
/// Panels that contain the polygon completely, in creation order. A surface load "belongs to the panel that contains it" (#64677): none = no panel,
/// several = overlapping panels (#65275 appears only in the calculation; creating, editing and moving never warned: measured / not enforced).
std::vector<int> panelsContaining(const Panels& panels, const std::vector<Point3>& polygon);
/// #65275 «La carga superficial se solapa con varios paños.» / #65276 «La superficie de la carga debe estar contenida completamente en un paño.»
std::string surfaceLoadSeveralPanelsMessage();
std::string surfaceLoadNotContainedMessage();
/// Messages of the calculation (#64203 / #64205 / #64204 / #64206 / #64202).
std::string panelWarningNoBarsMessage(bool withShells);
std::string panelWarningOverlapMessage();
std::string panelWarningNotCoveredByShellsMessage();
std::string panelWarningNotAssignedMessage();

// --- pure edits on the data (used by the commands; exposed for tests) ---

/// Deleted panels take their surface loads with them: those contained in a deleted panel and in no panel that remains (measured: two overlapping panels
/// and their two loads went together; whether one panel of two overlapping takes the loads is NOT measured). Returns the ids of the removed loads.
std::vector<int> applyPanelsDeleted(Panels& panels, SurfaceLoads& loads, const std::vector<int>& panelIds);
/// Deleted load case: its surface loads go, and so do the rows of the panels (the panel stays, measured).
void applyPanelsCaseDeleted(Panels& panels, SurfaceLoads& loads, int loadCaseId);

struct PanelReconcileInfo {
    int panelsRemoved = 0;
    int surfaceLoadsRemoved = 0;
};
/// Reconciles panels and surface loads with an edit of the structure. A vertex that coincides (1e-6 m) with a node of `before` follows it: moved nodes
/// drag the vertex, a node merged into another (`nodeRemap`: gone -> survivor) drags it to the survivor, and a node that vanished without survivor removes
/// the panel / surface load (consistent with the shells; NOT measured). Vertices that collapse onto each other are fused and a polygon left with fewer
/// than 3 points disappears; the direction is kept in the new plane. Bars that vanished leave `unassignedBars`; a new bar lying on a bar of the list joins it.
void reconcilePanels(Panels& panels, SurfaceLoads& loads, const StructuralModel& before, const StructuralModel& after, const std::map<int, int>& nodeRemap = {},
                     PanelReconcileInfo* info = nullptr);

/// Puts `state` back keeping the id counters (ids are never reused, also across undo).
inline void restorePanels(Panels& target, const Panels& state) {
    const int next = std::max(target.nextId, state.nextId);
    target = state;
    target.nextId = next;
}
inline void restoreSurfaceLoads(SurfaceLoads& target, const SurfaceLoads& state) {
    const int next = std::max(target.nextId, state.nextId);
    target = state;
    target.nextId = next;
}

/// Internal consistency: unique ids below nextId, valid contours, unit directions in the plane, canonical rows in existing cases, variable rows only on
/// non-horizontal panels. Empty when valid.
std::string validatePanels(const Panels& panels, const LoadCaseSet& cases);
std::string validateSurfaceLoads(const SurfaceLoads& loads, const LoadCaseSet& cases);

}  // namespace cc::core
