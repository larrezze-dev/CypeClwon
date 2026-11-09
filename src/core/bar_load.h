#pragma once
#include <algorithm>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "core/load_case.h"
#include "core/nodal_load.h"
#include "core/structure_model.h"

namespace cc::core {

/// Bar loads ("Cargas sobre barras", T-P7-03, core part). Behaviour source: docs/research/ui/cargas_barras.md (sections 9-13: strings and
/// validators of ed3dlib.dll / cypeud.dll [REA], measurements in the original [CAP]; [CAP] wins over [REA] when they disagree).
///
/// Model. One BarLoad is one of nine kinds of action on one bar in one load case ("hipótesis"): point force, uniform, strip ("faja"),
/// triangular (left / right), trapezoidal, point moment, uniform temperature increment and variable temperature increment (T1 / T2 on the
/// two faces). Force and moment values are ABSOLUTE (module): the sign comes from the direction. A bar may carry any number of loads, also in
/// the same case: they are never merged (modelled as a list, like the nodal loads). Ids grow and are never reused.
///
/// Axes. Types 0..6 take global axes (default) or the local axes of the bar; the variable temperature takes local axes only; the uniform
/// temperature has no axes and no direction. Direction: the six presets +X..-Z or "Definir dirección" (global: user vector; local: two
/// angles). Positions are measured from the start node of the bar, as a fraction (0..1, "Posición relativa", default) or in metres.
///
/// Public API (for the GUI layer)
///  - Data: Project::barLoads (BarLoads {items, nextId, selfWeightOff}); scales are the shared ones (Project::nodalLoads.scales).
///  - Dialog: BarLoadData (+ barLoadDefaults, barLoadToData), field specs / parsing (barLoadFieldSpec, parseBarLoadField), checkBarLoadData
///    (message + Aviso/Error kind), normaliseBarLoadData, BarLoadSession (last values remembered while the program runs).
///  - Queries: findBarLoad / barLoadsOfBar / barLoadsOfCase / hasBarLoads / barSelfWeightActive.
///  - Undoable edits: bar_load_edit.h (add, edit several, change case, delete, self weight on / off).
///  - Geometry: barLocalAxes, barLoadDirection, barLoadExtent, barLoadDrawing (pure functions, section 12 of the research file).
///  - Structure edits: DeleteBarsCommand / DeleteNodesCommand / MoveNodeCommand / PieceDiffCommand / SnapshotCommand call reconcileBarLoads
///    (deleted bar -> its loads go without warning; shorter bar -> loads that no longer fit are deleted, #65192; divided bar -> loads are
///    split between the stretches); deleting a load case deletes its bar loads (load_case_edit).
///
/// ASSUMPTIONS / NOT MEASURED (TODO(medir), see TK-098): division / merge / move of bars with loads (section 13.2 of the research file is
/// pending): (1) relative positions scale with the bar, absolute ones stay; absolute loads that fall beyond the new length are deleted and the
/// message #65192 is the GUI's to show; (2) a divided bar splits its loads physically between the stretches (uniform and temperature loads are
/// copied, point / moment loads go to the stretch that holds them, distributed loads are clipped with interpolated values); (3) two bars merged
/// by deleting their node lose their loads; (4) triangular LEFT has its maximum at the start of the bar and RIGHT at the end; (5) the local
/// angles of "Definir dirección" are alpha in the XZ plane (from +x towards +z) and beta from that plane towards +y; (6) the vertical bar
/// local frame and the sign of the roll angle; (7) inverting the x axis of a bar does not move its loads; (8) the proportional drawing of
/// the temperature is 0.1 m per degree; (9) the sign of the band offset: the band is drawn from the bar in the direction of the load
/// (research 10.4), point loads follow the nodal convention (arrow tip on the bar, drawn from the side the load comes from).

inline constexpr double kMinBarLoadForceKn = 0.01, kMaxBarLoadForceKn = 49000.0;      ///< point force, 2 decimals
inline constexpr double kMinBarLoadLinear = 0.001, kMaxBarLoadLinear = 10000.0;       ///< kN/m and kN.m, 3 decimals
inline constexpr double kMinBarTemperature = -100.0, kMaxBarTemperature = 1000.0;     ///< degC, 2 decimals
inline constexpr double kMaxBarLoadAbsolutePosition = 1000.0;                         ///< range of the control; the bar length is checked on Aceptar
inline constexpr double kMaxBarLoadVector = 1000.0;                                   ///< -1000..1000, 3 decimals
inline constexpr double kMaxBarLoadAngle = 180.0;                                     ///< -180..180, 3 decimals
inline constexpr double kBarLoadDefaultValue = 9.81;                                  ///< 9.810 (kN, kN/m, kN.m); the dialog proposes it
inline constexpr double kBarLoadDefaultTemperature = 10.0;                            ///< 10.00 degC
inline constexpr double kBarLoadPositionToleranceM = 1e-3;                            ///< i_PRECISION3 (REA)
inline constexpr double kBarLoadValueTolerance = 1e-4;                                ///< i_PRECISION4 (REA)

/// Values of enum ed3tipo_carga_t [REA]; the order of the dialog buttons is Point, Uniform, Strip, TriLeft, TriRight, Trapezoidal, PointMoment,
/// UniformTemperature, VariableTemperature (uniform temperature = 8 comes before variable = 7).
enum class BarLoadType { Point = 0, Uniform = 1, Strip = 2, TriangularLeft = 3, TriangularRight = 4, Trapezoidal = 5, PointMoment = 6,
                         VariableTemperature = 7, UniformTemperature = 8 };
inline constexpr int kBarLoadTypeCount = 9;
/// Types in the order of the nine buttons of "Tipo de carga".
inline constexpr BarLoadType kBarLoadTypeButtonOrder[kBarLoadTypeCount] = {
    BarLoadType::Point, BarLoadType::Uniform, BarLoadType::Strip, BarLoadType::TriangularLeft, BarLoadType::TriangularRight,
    BarLoadType::Trapezoidal, BarLoadType::PointMoment, BarLoadType::UniformTemperature, BarLoadType::VariableTemperature};
/// ed3tipo_ejes_carga_t.
enum class BarLoadAxes { Global = 0, Local = 1 };
/// ed3dirtemp.c: the three buttons of the variable temperature (Y+, Z+, defined by an angle). Default: Z+ (second button).
enum class TemperatureDirection { PlusY = 0, PlusZ = 1, Angle = 2 };

/// Name of a type ("Carga puntual", "Carga uniforme", ... the tooltips of the buttons).
std::string barLoadTypeName(BarLoadType type);
bool isForceLoad(BarLoadType t);          ///< Point
bool isMomentLoad(BarLoadType t);         ///< PointMoment
bool isTemperatureLoad(BarLoadType t);    ///< variable or uniform
bool isPointLikeLoad(BarLoadType t);      ///< Point or PointMoment: one position ("Distancia")
bool hasTwoPositions(BarLoadType t);      ///< Strip, triangulars, trapezoidal: "Posición inicial" / "Posición final"
bool hasDirection(BarLoadType t);         ///< every type but the uniform temperature
bool allowsGlobalAxes(BarLoadType t);     ///< types 0..6
bool hasTwoValues(BarLoadType t);         ///< Trapezoidal and VariableTemperature (value1 and value2)

/// Everything the dialog edits (also what "Editar" writes into EVERY selected load).
struct BarLoadData {
    int loadCaseId = 0;
    BarLoadType type = BarLoadType::Uniform;
    BarLoadAxes axes = BarLoadAxes::Global;
    LoadDirection direction = LoadDirection::MinusZ;           ///< presets +X..-Z or UserVector ("Definir dirección")
    Point3 userVector = kDefaultUserVector;                    ///< global user vector (axes Global, direction UserVector)
    double angleInPlaneXz = 0.0;                               ///< "Ángulo en el plano 'XZ' local" (local, UserVector), degrees
    double angleToPlaneXz = 0.0;                               ///< "Ángulo con el plano 'XZ' local" (local, UserVector), degrees
    TemperatureDirection temperatureDirection = TemperatureDirection::PlusZ;  ///< variable temperature only
    double temperatureAngle = 0.0;                             ///< "Ángulo" of TemperatureDirection::Angle, degrees
    bool relative = true;                                      ///< "Posición relativa": positions are fractions of the length (0..1)
    double start = 0.0;                                        ///< "Distancia" (point / moment) or "Posición inicial"
    double end = 1.0;                                          ///< "Posición final" (two positions types)
    double value1 = kBarLoadDefaultValue;                      ///< value / initial value / peak (triangular) / temperature T1
    double value2 = 0.0;                                       ///< final value (trapezoidal) / temperature T2
    bool operator==(const BarLoadData&) const = default;
};
/// Dialog defaults: Uniform, global axes, -Z, 9.810, relative positions 0 .. 1, case `loadCaseId`.
BarLoadData barLoadDefaults(int loadCaseId = 0);

struct BarLoad : BarLoadData {
    int id = 0;
    int barId = 0;
    bool operator==(const BarLoad&) const = default;
};
BarLoadData barLoadToData(const BarLoad& load);

struct BarLoads {
    std::vector<BarLoad> items;      ///< in creation order
    int nextId = 1;
    std::set<int> selfWeightOff;     ///< bars whose automatic self-weight generation was switched off (peso propio)
    bool empty() const { return items.empty() && selfWeightOff.empty(); }
    bool operator==(const BarLoads&) const = default;
};

const BarLoad* findBarLoad(const BarLoads& loads, int id);
std::vector<int> barLoadsOfBar(const BarLoads& loads, int barId);
std::vector<int> barLoadsOfCase(const BarLoads& loads, int loadCaseId);
/// "Editar", "Hipótesis" and "Borrar" are enabled only when some bar load exists.
inline bool hasBarLoads(const BarLoads& loads) { return !loads.items.empty(); }
/// Peso propio: automatic generation is on unless the bar was switched off.
inline bool barSelfWeightActive(const BarLoads& loads, int barId) { return loads.selfWeightOff.count(barId) == 0; }

// --- the dialog fields: labels, ranges, parsing ---

enum class BarLoadField { Value1, Value2, Start, End, VectorX, VectorY, VectorZ, AngleInPlane, AngleToPlane, TemperatureAngle };
struct BarLoadFieldSpec {
    std::string label;        ///< "Valor", "Valor inicial", "Valor 1", "Distancia", "Posición inicial", "X", ...
    double min = 0.0, max = 0.0;
    int decimals = 3;
    bool blankUnitSpacing = false;  ///< the measured message of unit-less controls has doubled spaces: "entre 0.000  y 1.000 ."
};
/// Label, range and decimals of a field for the current data (type, relative flag).
BarLoadFieldSpec barLoadFieldSpec(const BarLoadData& data, BarLoadField field);
/// «El valor de '%s' debe estar entre %s y %s.» (measured; the quirk of the unit-less controls is reproduced).
std::string barLoadRangeMessage(const BarLoadFieldSpec& spec);
/// «La cadena introducida no es un número.» (shared with the nodal loads).
std::string barLoadNotNumberMessage();
double roundToDecimals(double value, int decimals);

struct BarLoadFieldParse {
    bool ok = false;
    double value = 0.0;       ///< rounded to the decimals of the field; only when ok
    std::string message;      ///< measured message when !ok
};
/// Text of a field: not a number -> «La cadena introducida no es un número.»; out of range after rounding -> the range message
/// (0.0005 in a 3-decimal field is accepted as 0.001, measured).
BarLoadFieldParse parseBarLoadField(const BarLoadData& data, BarLoadField field, const std::string& text);

/// Aviso = yellow icon (range and direction messages); Error = red icon (business rules). Both keep the dialog open.
enum class BarLoadIssueKind { None, Warning, Error };
struct BarLoadIssue {
    BarLoadIssueKind kind = BarLoadIssueKind::None;
    std::string message;
    std::optional<BarLoadField> field;   ///< field to refocus
    bool ok() const { return kind == BarLoadIssueKind::None; }
};
/// «El valor introducido no puede ser cero.» (#65323, only the uniform temperature), «Al menos uno de los valores de la carga debe ser distinto
/// de cero.» (#65324, only the variable temperature with both at zero), «La dirección introducida no es correcta.» (#65319), «El valor de
/// 'Posición inicial' debe ser inferior al de 'Posición final'.»
std::string barLoadZeroMessage();
std::string barLoadBothZeroMessage();
std::string barLoadDirectionMessage();
std::string barLoadStartBeforeEndMessage();
/// Messages whose emission the GUI decides (texts of the REA string table): #65192, #65307, #64002.
std::string barLoadsOutOfLengthMessage();     ///< «Se han borrado cargas incompatibles con la longitud de la barra.»
std::string barLoadsMovedOverBarMessage();    ///< «Al mover una barra sobre otra se pierden sus cargas.»
std::string barSelfWeightDisabledMessage();   ///< «Se ha desactivado la generación automática de las cargas de peso propio para esta barra.» (never seen)

/// Checks the whole dialog in the order of the tab sequence (values, positions, direction). `lengthM` = length of the bar (the shortest of
/// the selected ones); <= 0 or not finite skips the length check of absolute positions ("El valor de 'Distancia' debe estar entre 0.000 y
/// 4.000.", measured at Aceptar on the unrounded value: 4.0001 fails, 4.000 passes).
BarLoadIssue checkBarLoadData(const BarLoadData& data, double lengthM = 0.0);

/// Canonical form stored in the model: values and positions rounded to the decimals of their field, fields that the type does not use reset to
/// their defaults (uniform temperature: local axes, no direction; variable temperature: local axes; uniform / temperatures cover the whole
/// bar; triangular and one-value types keep value2 = 0), global axes drop the local angles, presets drop the user fields.
BarLoadData normaliseBarLoadData(BarLoadData data);

// --- type normalisation of linear loads (REA ed3carsup.c / ed3carbar.c constructor) ---

struct LinearLoadShape {
    BarLoadType type = BarLoadType::Uniform;
    double value1 = 0.0;   ///< see BarLoadData::value1
    double value2 = 0.0;
};
/// Rule of the constructor: a null initial / final value -> triangular (the non-null one is the maximum; left = maximum at the start);
/// |v1| = |v2| (1e-4) -> Strip, or Uniform when it covers the whole bar (start <= 0.001 and end + 0.001 >= L); otherwise Trapezoidal.
/// Positions in metres. Values are absolute (same sign assumed).
LinearLoadShape normaliseLinearLoad(double startM, double endM, double value1, double value2, double lengthM);

// --- session memory ---

/// "El diálogo Nueva recuerda el último tipo y valores de la sesión" and the last option of the Peso propio dialog. Not persisted.
struct BarLoadSession {
    BarLoadData last = barLoadDefaults();
    bool selfWeightActivate = true;   ///< radios Activar (default) / Desactivar
    void remember(const BarLoadData& data) { last = data; }
};

// --- geometry ---

/// Local frame of a bar (unit vectors in global axes). x = start -> end (inverted when xAxisInverted); z = projection of the global Z (vertical
/// bars: global X, NOT MEASURED); y = z x x; then rolled about x by rollAngleDeg (sign NOT MEASURED).
struct BarFrame {
    Point3 origin, x, y, z;
    double length = 0.0;
};
std::optional<BarFrame> barLocalAxes(const StructuralModel& model, int barId);

/// Unit direction of the load in global axes ({0,0,0} when the load has none: uniform temperature, null vector). The variable temperature gives
/// the direction of the variation across the section (local Y+, Z+, or cos(a) z + sin(a) y).
Point3 barLoadDirection(const BarLoadData& data, const BarFrame& frame);

/// Where the load acts along the bar, in metres from the start node, and its values at both ends (absolute; temperature: T1, T2).
struct BarLoadExtent {
    double start = 0.0, end = 0.0;
    double valueStart = 0.0, valueEnd = 0.0;
};
BarLoadExtent barLoadExtent(const BarLoadData& data, double lengthM);

/// Scales (shared LoadScales). Fixed mode: `fixedSizeM` is the MAXIMUM size, every load of the bar reaches it whatever its value (measured: 0.5 m for
/// values 5 and 10); the other end of a wedge is proportional. Proportional mode: 0.1 m per unit (kN, kN/m, kN.m) x scale of the case x scale of
/// the type (5 kN/m -> 0.5 m, 10 -> 1.0 m, trapezoidal 5 -> 10 is a wedge 0.5 -> 1.0 m, moment 5 -> arc of radius 0.5 m).
LoadScaleType barLoadScaleType(BarLoadType type);
double barLoadDrawSizeM(const LoadScales& scales, const BarLoadData& data, double value, double maxValue);

/// Label text: the absolute value without sign or unit, up to 3 decimals without trailing zeros ("20", "7", "9.81"); temperatures "T1/T2".
std::string barLoadValueLabel(double value);
std::string barLoadLabel(const BarLoadData& data);

struct BarLoadLabel {
    Point3 anchor;
    std::string text;
};
/// Pure geometry of one load (research 12.5 / 10.4), global metres. Distributed loads and temperatures: `outline` is the closed polygon (the
/// two base points on the bar, then the two offset points), `ticks` the perpendicular lines at regular intervals (every kBarLoadTickSpacingM),
/// one label per significant end. Temperatures use a constant width. Point load: `arrow` = {tail, tip} with the tip on the bar. Moment: `arc`
/// = polyline of a semicircle around the bar (radius = size), ordered in the sense of the moment, plus the label. The self-weight band is
/// not produced here.
inline constexpr double kBarLoadTickSpacingM = 0.25;   // NOT MEASURED ("intervalos regulares")
struct BarLoadDrawing {
    BarLoadType type = BarLoadType::Uniform;
    bool valid = false;
    std::vector<Point3> outline;
    std::vector<std::pair<Point3, Point3>> ticks;
    std::vector<Point3> arc;
    std::pair<Point3, Point3> arrow;
    std::vector<BarLoadLabel> labels;
    double sizeM = 0.0;     ///< height of the band / length of the arrow / radius of the arc
};
BarLoadDrawing barLoadDrawing(const BarLoadData& data, const BarFrame& frame, const LoadScales& scales);

// --- pure edits on the data (used by the commands; exposed for tests) ---

/// Loads of deleted bars go without warning (also the self-weight flag of those bars).
void applyBarLoadsBarsDeleted(BarLoads& loads, const std::vector<int>& barIds);
/// Deleted load case: its loads are removed.
void applyBarLoadsCaseDeleted(BarLoads& loads, int loadCaseId);
struct BarLoadReconcileInfo {
    int removedOutOfLength = 0;   ///< loads deleted because they no longer fit the shorter bar (#65192)
    int split = 0;                ///< loads produced by dividing bars
};
/// Reconciles the loads after an edit of the structure (see the assumptions in the header comment).
BarLoads reconcileBarLoads(const BarLoads& loads, const StructuralModel& before, const StructuralModel& after, BarLoadReconcileInfo* info = nullptr);

/// Puts `state` back keeping the id counter (ids are never reused, also across undo).
inline void restoreBarLoads(BarLoads& target, const BarLoads& state) {
    const int next = std::max(target.nextId, state.nextId);
    target = state;
    target.nextId = next;
}

/// Internal consistency of the stored data: unique ids below nextId, existing load case, canonical form, valid ranges. Bars that no longer
/// exist are tolerated (not saved). Empty when valid.
std::string validateBarLoads(const BarLoads& loads, const LoadCaseSet& cases);

}  // namespace cc::core
