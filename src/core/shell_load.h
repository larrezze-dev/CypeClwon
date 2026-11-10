#pragma once
#include <algorithm>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "core/bar_load.h"
#include "core/load_case.h"
#include "core/nodal_load.h"
#include "core/structure_model.h"

namespace cc::core {

/// Shell loads ("Cargas en láminas", T-P7-04, core part). Behaviour source: docs/research/ui/cargas_laminas.md (sections 3-4 REA, sections 8-10
/// measured in the original [CAP]; [CAP] wins over [REA] when they disagree). Section "Backend implementado" of that file lists the assumptions.
///
/// Model. One ShellLoad is one surface action on one shell in one load case ("hipótesis"): uniform, variable along the local x or y axis,
/// uniform temperature increment or shrinkage ("retracción"). Surface values are ABSOLUTE (module): the sign comes from the sense (button 1 = +Z,
/// button 2 = -Z). A shell may carry any number of loads, also in the same case: they are never merged (list, like nodal and bar loads).
/// Ids grow and are never reused.
///
/// Axes / sense. Uniform and variable loads choose "Ejes globales" or "Ejes locales de la lámina" (default local) and the sense +Z / -Z of that
/// axis system; temperature and shrinkage have neither. Variable loads vary linearly along the LOCAL x (VariableX) or y (VariableY) axis from the
/// initial value (smaller coordinate) to the final value (larger coordinate).
///
/// Public API (for the GUI layer)
///  - Data: Project::shellLoads (ShellLoads {items, nextId, selfWeightOff}); drawing scales are the shared ones (Project::nodalLoads.scales).
///  - Dialog: ShellLoadData (+ shellLoadDefaults(type)), field specs / parsing (shellLoadFieldSpec, parseShellLoadField), checkShellLoadData
///    (message + Aviso/Error kind), normaliseShellLoadData, ShellLoadSession.
///  - Queries: findShellLoad / shellLoadsOfShell / shellLoadsOfCase / hasShellLoads / shellSelfWeightActive.
///  - Undoable edits: shell_load_edit.h (add, edit several, change case, delete, self weight on / off).
///  - Self weight: shellSpecificWeightKnM3 / shellSelfWeightKnM2 / shellSelfWeightLoad (the load to draw in the Peso propio mode).
///  - Geometry: shellLoadFrame, shellLoadDirection, shellLoadCornerValues, shellLoadStackOffsets, shellLoadDrawing (pure functions).
///  - Structure edits: SnapshotCommand / DeleteNodesCommand call reconcileShellLoads / applyShellLoadsShellsDeleted (deleted shell -> its loads
///    go without warning; divided shell -> the pieces get a copy of the loads); deleting a load case deletes its shell loads (load_case_edit).

inline constexpr double kMinShellSurfaceLoad = 0.0, kMaxShellSurfaceLoad = 100000.0;       ///< kN/m2, 3 decimals (measured)
inline constexpr double kMinShellTemperature = -100.0, kMaxShellTemperature = 1000.0;      ///< degC, 2 decimals
inline constexpr double kMinShellShrinkage = -1000.0, kMaxShellShrinkage = 1000.0;         ///< no unit, 4 decimals
inline constexpr double kShellLoadDefaultSurface = 9.81;       ///< 9.810 (uniform and both values of the variable ones)
inline constexpr double kShellLoadDefaultTemperature = 10.0;   ///< 10.00 degC
inline constexpr double kShellLoadDefaultShrinkage = 0.0001;   ///< 0.0001 (measured; the REA said 0)
inline constexpr double kShellLoadTickSpacingM = 0.5;          // measured by counting the lines of cl3_two2.png (about 0.5 m on a 4 m edge)
/// Specific weight (kN/m3) of the concrete taken from the general data: 25 (measured: 100 mm of C25/30 -> 2.5 kN/m2).
inline constexpr double kShellConcreteWeightKnM3 = 25.0;
inline constexpr double kShellSteelWeightKnM3 = 77.01;         // NOT MEASURED for shells (same as the steel bars: 7.85 T/m3 x 9.81)
inline constexpr double kShellAluminiumWeightKnM3 = 27.0;      // NOT MEASURED for shells (same as the aluminium bars)

/// Values of enum i_tipo_carga_t [REA] = order of the five buttons of "Tipo de carga".
enum class ShellLoadType { Uniform = 0, VariableX = 1, VariableY = 2, UniformTemperature = 3, Shrinkage = 4 };
inline constexpr int kShellLoadTypeCount = 5;
inline constexpr ShellLoadType kShellLoadTypeButtonOrder[kShellLoadTypeCount] = {
    ShellLoadType::Uniform, ShellLoadType::VariableX, ShellLoadType::VariableY, ShellLoadType::UniformTemperature, ShellLoadType::Shrinkage};
/// "Ejes sobre los que se define la dirección de la carga": global / local of the shell (default local).
enum class ShellLoadAxes { Global = 0, Local = 1 };
/// The two buttons of "Dirección y sentido de aplicación de la carga": +Z (default) / -Z of the axis system.
enum class ShellLoadSense { PlusZ = 0, MinusZ = 1 };

/// Tooltips / names of the types ("Uniforme", "Variable en eje x local", "Variable en eje y local", "Incremento de temperatura uniforme", "Retracción").
std::string shellLoadTypeName(ShellLoadType type);
bool isSurfaceLoad(ShellLoadType t);        ///< Uniform, VariableX, VariableY (kN/m2, with axes and sense)
bool isVariableShellLoad(ShellLoadType t);  ///< VariableX, VariableY (value1 = initial, value2 = final)
bool hasShellAxesAndSense(ShellLoadType t); ///< = isSurfaceLoad

/// Everything the dialog edits (also what "Editar" writes into EVERY selected load).
struct ShellLoadData {
    int loadCaseId = 0;
    ShellLoadType type = ShellLoadType::Uniform;
    ShellLoadAxes axes = ShellLoadAxes::Local;
    ShellLoadSense sense = ShellLoadSense::PlusZ;
    double value1 = kShellLoadDefaultSurface;   ///< value / initial value / temperature / shrinkage
    double value2 = 0.0;                        ///< final value (variable types only)
    bool operator==(const ShellLoadData&) const = default;
};
/// Dialog defaults of a type (what the panel shows after pressing its button): uniform 9.810, variable 9.810 / 9.810, temperature 10.00, shrinkage
/// 0.0001; local axes, +Z.
ShellLoadData shellLoadDefaults(int loadCaseId = 0, ShellLoadType type = ShellLoadType::Uniform);

struct ShellLoad : ShellLoadData {
    int id = 0;
    int shellId = 0;
    bool operator==(const ShellLoad&) const = default;
};
ShellLoadData shellLoadToData(const ShellLoad& load);

struct ShellLoads {
    std::vector<ShellLoad> items;    ///< in creation order
    int nextId = 1;
    std::set<int> selfWeightOff;     ///< shells whose automatic self-weight generation was switched off (peso propio)
    bool empty() const { return items.empty() && selfWeightOff.empty(); }
    bool operator==(const ShellLoads&) const = default;
};

const ShellLoad* findShellLoad(const ShellLoads& loads, int id);
std::vector<int> shellLoadsOfShell(const ShellLoads& loads, int shellId);
std::vector<int> shellLoadsOfCase(const ShellLoads& loads, int loadCaseId);
/// "Editar", "Hipótesis" and "Borrar" are enabled only when some shell load exists (Nueva and Peso propio need only shells).
inline bool hasShellLoads(const ShellLoads& loads) { return !loads.items.empty(); }
inline bool shellSelfWeightActive(const ShellLoads& loads, int shellId) { return loads.selfWeightOff.count(shellId) == 0; }

// --- the dialog fields: labels, ranges, parsing ---

enum class ShellLoadField { Value1, Value2 };
struct ShellLoadFieldSpec {
    std::string label;        ///< "Valor", "Valor inicial", "Valor final"
    double min = 0.0, max = 0.0;
    int decimals = 3;
    std::string unit;         ///< "kN/m²" for the surface loads (the measured message carries it); empty otherwise
    bool blankUnitSpacing = false;  ///< shrinkage: "entre -1000.0000  y 1000.0000 ." (doubled spaces of the unit-less control)
};
ShellLoadFieldSpec shellLoadFieldSpec(const ShellLoadData& data, ShellLoadField field);
/// «El valor de 'Valor' debe estar entre 0.000 kN/m² y 100000.000 kN/m².» / «... entre -100.00 y 1000.00.» / «... entre -1000.0000  y 1000.0000 .»
std::string shellLoadRangeMessage(const ShellLoadFieldSpec& spec);
/// «La cadena introducida no es un número.» (shared).
std::string shellLoadNotNumberMessage();
/// «El valor introducido no puede ser cero.» (#65323, the four kinds; #65324 does not appear for shells).
std::string shellLoadZeroMessage();
/// «La dirección introducida no es correcta.» (only for corrupt axes / sense values).
std::string shellLoadDirectionMessage();

struct ShellLoadFieldParse {
    bool ok = false;
    double value = 0.0;       ///< rounded to the decimals of the field; only when ok
    std::string message;      ///< measured message when !ok
};
ShellLoadFieldParse parseShellLoadField(const ShellLoadData& data, ShellLoadField field, const std::string& text);

/// Aviso = yellow icon (range); Error = red icon (null value). Both keep the dialog open.
enum class ShellLoadIssueKind { None, Warning, Error };
struct ShellLoadIssue {
    ShellLoadIssueKind kind = ShellLoadIssueKind::None;
    std::string message;
    std::optional<ShellLoadField> field;   ///< field to refocus
    bool ok() const { return kind == ShellLoadIssueKind::None; }
};
/// Order of the checks (measured): range of each field in tab order (Aviso), then the null rule (Error): uniform / temperature / shrinkage zero
/// after rounding, variable with BOTH values zero (one null value is accepted). Negative surface values are a range Aviso (the range starts at 0).
ShellLoadIssue checkShellLoadData(const ShellLoadData& data);

/// Canonical form stored in the model: values rounded to the decimals of their field; fields that the type does not use reset to their
/// defaults (value2 outside the variable types; temperature and shrinkage: local axes, +Z).
ShellLoadData normaliseShellLoadData(ShellLoadData data);

// --- session memory ---

/// "El diálogo Nueva recuerda el último tipo y valores de la sesión" and the last option of the Peso propio dialog. Not persisted.
struct ShellLoadSession {
    ShellLoadData last = shellLoadDefaults();
    bool selfWeightActivate = true;   ///< radios Activar (default) / Desactivar
    bool hasLastCase = false;         ///< the dialog has been accepted at least once (last.loadCaseId is the last hypothesis used there)
    void remember(const ShellLoadData& data) {
        last = data;
        hasLastCase = true;
    }
};

// --- self weight ---

/// Specific weight of the material of a shell (kN/m3): concrete 25; generic materials their own value; steel / aluminium as the bars (NOT MEASURED).
double shellSpecificWeightKnM3(const ShellMaterial& material);
/// kN/m2 = thickness x specific weight (measured: 100 mm of C25/30 -> 2.5).
double shellSelfWeightKnM2(const Shell& shell);
/// The automatic load of the shell as something drawable: uniform, global axes, -Z, case 0 (not rounded or checked). nullopt when the shell has no
/// thickness. The GUI draws it only in the Peso propio mode and for the shells that have the generation active.
std::optional<ShellLoadData> shellSelfWeightLoad(const Shell& shell);

// --- geometry ---

/// Corners of a shell as drawn (shellDrawnCorners) and its unit local axes (shellLocalAxes).
struct ShellLoadFrame {
    std::vector<Point3> corners;
    Point3 origin, x, y, z;
};
std::optional<ShellLoadFrame> shellLoadFrame(const StructuralModel& model, int shellId);

/// Unit direction in which the load PUSHES (global axes): the local z (or global Z) times the sense; temperature / shrinkage have none and give
/// the local +z (they are drawn on the same side as the sense-1 loads, measured).
Point3 shellLoadDirection(const ShellLoadData& data, const ShellLoadFrame& frame);

/// Absolute value of the load at each corner of the frame (same order): constant for the uniform / temperature / shrinkage, linear between
/// value1 (smaller local x / y) and value2 (larger) for the variable ones.
std::vector<double> shellLoadCornerValues(const ShellLoadData& data, const ShellLoadFrame& frame);

/// Scales (shared LoadScales). Fixed mode: `fixedSizeM` is the MAXIMUM size, reached by the largest value of the load whatever its magnitude
/// (measured: 0.5 m for 5, wedge 5 -> 15 gives 1/3 .. 1). Proportional mode: 0.1 m per unit x scale of the case x scale of the type (Surface for the
/// uniform and variable loads: measured 5 kN/m2 -> 0.5 m, self weight 2.5 -> 0.25 m; Temperature / Shrinkage: NOT MEASURED, same rule).
LoadScaleType shellLoadScaleType(ShellLoadType type);
double shellLoadDrawSizeM(const LoadScales& scales, const ShellLoadData& data, double value, double maxValue);
/// Height of the tallest part of the prism (the largest value).
double shellLoadMaxSizeM(const LoadScales& scales, const ShellLoadData& data);

/// Several loads on one shell (given in creation order): the prism of each one is drawn on the side from which the load comes, and the second one
/// BELOW (beyond) the first on the same side (measured), so the offset of load i is the sum of the maximum sizes of the previous loads that share
/// its side. Different sides do not stack.
std::vector<double> shellLoadStackOffsets(const std::vector<ShellLoadData>& loads, const LoadScales& scales);

/// "20" "7" "9.81": absolute value, up to `decimals` decimals, trailing zeros removed.
std::string shellLoadValueLabel(double value, int decimals = 3);
/// Texts of the labels: surface «<value> kN/m²», temperature «20° C», shrinkage «0.0005» (the last two literal forms are partly NOT MEASURED).
std::string shellLoadLabelText(const ShellLoadData& data, double value);

struct ShellLoadLabel {
    Point3 anchor;      ///< middle of the vertical edge of the prism at a corner
    std::string text;
};
/// Pure geometry of one load: the prism stands on the shell plane shifted `stackOffsetM` towards the side the load comes from; `base` = the corners
/// of the footprint, `top` = the same corners moved by the height of the load there, `ticks` the vertical lines at regular intervals along the
/// contour, one label at the first corner (uniform, temperature, shrinkage) or one per corner (variable).
struct ShellLoadDrawing {
    ShellLoadType type = ShellLoadType::Uniform;
    bool valid = false;
    std::vector<Point3> base, top;
    std::vector<std::pair<Point3, Point3>> ticks;
    std::vector<ShellLoadLabel> labels;
    Point3 direction;       ///< unit direction in which the load pushes (the prism is on the opposite side)
    double sizeM = 0.0;     ///< maximum height
};
ShellLoadDrawing shellLoadDrawing(const ShellLoadData& data, const ShellLoadFrame& frame, const LoadScales& scales, double stackOffsetM = 0.0);

// --- pure edits on the data (used by the commands; exposed for tests) ---

/// Loads (and flags) of deleted shells go without warning.
void applyShellLoadsShellsDeleted(ShellLoads& loads, const std::vector<int>& shellIds);
/// Deleted load case: its loads are removed.
void applyShellLoadsCaseDeleted(ShellLoads& loads, int loadCaseId);
struct ShellLoadReconcileInfo {
    int removed = 0;   ///< loads of shells that vanished
    int copied = 0;    ///< loads copied to the pieces of a divided shell
};
/// Reconciles the loads after an edit of the structure: a vanished shell loses its loads (also an absorbed one in a union: the result keeps its own);
/// a NEW shell whose centroid lies in a shell of `before` (division) receives a copy of that shell's loads and self-weight flag (ASSUMPTION, not
/// measured).
ShellLoads reconcileShellLoads(const ShellLoads& loads, const StructuralModel& before, const StructuralModel& after, ShellLoadReconcileInfo* info = nullptr);

/// Puts `state` back keeping the id counter (ids are never reused, also across undo).
inline void restoreShellLoads(ShellLoads& target, const ShellLoads& state) {
    const int next = std::max(target.nextId, state.nextId);
    target = state;
    target.nextId = next;
}

/// Internal consistency: unique ids below nextId, existing load case, canonical form, valid ranges. Shells that no longer exist are tolerated (not
/// saved). Empty when valid.
std::string validateShellLoads(const ShellLoads& loads, const LoadCaseSet& cases);

}  // namespace cc::core
