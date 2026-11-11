#pragma once
#include <array>
#include <map>
#include <string>
#include <vector>

#include "core/load_case.h"
#include "core/structure_model.h"

namespace cc::core {

/// Nodal loads ("Cargas sobre nudos", T-P7-02, core part). Behaviour source: docs/research/ui/cargas_nudos.md (strings of ed3dlib.dll and
/// measurements in the original).
///
/// Model. One NodalLoad is a point FORCE of module `magnitude` (kN, always positive: the sign comes from the direction) applied at a node in
/// one load case ("hipótesis"), along one of six global axes or a user vector expressed in global axes. There are no moments. A node may carry
/// any number of loads, also in the same case: they are never merged or added up (measured: two loads of one node are drawn and edited
/// separately). Ids grow and are never reused.
///
/// Public API (for the GUI layer)
///  - Data: Project::nodalLoads (NodalLoads {items, nextId, scales}).
///  - Queries: findNodalLoad / nodalLoadsOfNode / nodalLoadsOfCase / hasNodalLoads, nodalLoadDirection (unit vector), nodalLoadForce (kN vector).
///  - Dialog values: parseLoadValue (text -> value, the two measured messages), checkNodalLoadData, roundLoadValue.
///  - Undoable edits: nodal_load_edit.h (add to a set of nodes, edit several, change case, delete, scales).
///  - Persistence: JSON section "nodal_loads" (versioned, tolerant); deleting a node purges its loads, merging two nodes moves the loads of the
///    node that goes away to the survivor, deleting a load case deletes its loads (load_case_edit).
///
/// ASSUMPTIONS / NOT MEASURED (TODO(medir)): the value is stored rounded to two decimals (the field shows 2; the stored precision was not
/// measured) and the range is checked on the rounded value; the user vector has no range check (the -1000..1000 of the support dialog was not
/// verified here), only "not finite" and "null" are rejected with #65319 (REA, not reproduced in the original for loads); loads of a node that
/// is deleted are deleted with it and the loads of a merged node move to the survivor (neither case was measured: tickets); ranges of the
/// scales (only > 0 and finite are enforced).

inline constexpr double kMinNodalLoadKn = 0.01;
inline constexpr double kMaxNodalLoadKn = 49000.0;
inline constexpr double kDefaultLoadDrawSizeM = 0.5;    ///< "Tamaño máximo para representación" (0.500 m)
inline constexpr double kDefaultLoadCaseScale = 1.0;    ///< "Escala" per case (1.000)

/// Order of the seven buttons of the dialog (the last one is "Definir dirección"). Default of the dialog: MinusZ.
enum class LoadDirection { PlusX = 0, PlusY, PlusZ, MinusX, MinusY, MinusZ, UserVector };
inline constexpr int kLoadDirectionCount = 7;
/// Default vector of "Definir dirección" (1.000, 0.000, 0.000).
inline constexpr Point3 kDefaultUserVector{1.0, 0.0, 0.0};

struct NodalLoad {
    int id = 0;
    int nodeId = 0;
    int loadCaseId = 0;
    double magnitude = 0.0;  ///< kN, kMinNodalLoadKn..kMaxNodalLoadKn, two decimals
    LoadDirection direction = LoadDirection::MinusZ;
    Point3 userVector = kDefaultUserVector;  ///< global axes, only used when direction == UserVector
    bool operator==(const NodalLoad&) const = default;
};

/// Radios of Carga > Visualización > Escalas.
enum class LoadScaleMode { Fixed = 0, Proportional };

/// Rows of "Escalas de cargas por tipos" (proportional mode), in the order of the dialog. A nodal load is a "Puntual".
enum class LoadScaleType { Uniform = 0, Punctual, Strip, Triangular, Trapezoidal, PointMoment, Surface, Temperature, Shrinkage };
inline constexpr int kLoadScaleTypeCount = 9;
inline constexpr double kMinTypeScale = 0.001;   ///< tooltip of the field: "Debe estar entre 0.001 y 1000.000 ." (measured)
inline constexpr double kMaxTypeScale = 1000.0;
/// Proportional mode, measured: a load is drawn 0.1 m long per kN, times the scale of its case, times the scale of its type (5 kN -> 0.5 m, 20 kN
/// -> 2 m; case 2.000 or "Puntual" 2.000 doubles it). Fixed mode: every load is `fixedSizeM` long whatever its value (0.5 m -> 50 px, 1.0 m -> 100 px
/// at 100 px/m).
inline constexpr double kLoadMetresPerKn = 0.1;

struct LoadScales {
    LoadScaleMode mode = LoadScaleMode::Fixed;     ///< default: "Proporcionales a un valor fijo"
    double fixedSizeM = kDefaultLoadDrawSizeM;     ///< "Tamaño máximo para representación" (m)
    std::map<int, double> perCase;                 ///< load case id -> "Escala"; missing = kDefaultLoadCaseScale
    /// "Escalas de cargas por tipos": Uniforme .. Temperatura 1.000, Retracción 100.000 (defaults measured).
    std::array<double, kLoadScaleTypeCount> perType{1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 100.0};
    bool operator==(const LoadScales&) const = default;
};
/// Scale of a case in proportional mode (the stored one or the default).
double loadScaleOfCase(const LoadScales& scales, int loadCaseId);

struct NodalLoads {
    std::vector<NodalLoad> items;  ///< in creation order
    int nextId = 1;
    LoadScales scales;
    bool empty() const { return items.empty(); }
    bool operator==(const NodalLoads&) const = default;
};

/// Single resolution of the drawing size of any load (equivalent of FUN_1802dd290 of ed3dlib.dll). Fixed mode: the largest value of the load reaches
/// `maxM` (= fixedSizeM). Proportional mode: `factor` = scale of the case x scale of the type, applied to 0.1 m per unit.
struct LoadDrawSize {
    bool fixed = true;
    double maxM = kDefaultLoadDrawSizeM;
    double factor = 1.0;
};
LoadDrawSize resolveLoadDrawSize(const LoadScales& scales, LoadScaleType type, int loadCaseId);
/// Size in metres of a value of a load of the given scale type. `maxValue` = largest value of the SAME load (a wedge draws its smaller end in
/// proportion); in fixed mode a zero `maxValue` gives the full fixed size. Always non-negative (the sign is the caller's).
double loadDrawSizeM(const LoadScales& scales, LoadScaleType type, int loadCaseId, double value, double maxValue);

/// Valid ranges (measured / REA, research visualizacion_hipotesis_escalas): every scale of a type and of a case 0.001 .. 1000, fixed size
/// 0.010 .. 100.000 m (0 and 20000 are refused).
inline constexpr double kMinFixedLoadSizeM = 0.010;
inline constexpr double kMaxFixedLoadSizeM = 100.0;
bool loadScaleValueInRange(double value);
bool loadFixedSizeInRange(double value);
enum class LoadScaleIssue { None, FixedSize, TypeScale, CaseScale };
struct LoadScaleValidation {
    LoadScaleIssue issue = LoadScaleIssue::None;
    int index = -1;   ///< LoadScaleType for TypeScale, load case id for CaseScale
    bool ok() const { return issue == LoadScaleIssue::None; }
};
LoadScaleValidation validateLoadScales(const LoadScales& scales);
/// Row label of "Escalas de cargas por tipos" ("Uniforme", ..., "Retracción").
std::string loadScaleTypeName(LoadScaleType type);
/// Literal warning texts of the original (double space before "y" and space before the final dot are measured).
std::string loadScaleRangeMessage(const std::string& fieldName);   ///< "El valor de 'Uniforme' debe estar entre 0.001  y 1000.000 ."
std::string loadFixedSizeRangeMessage();                           ///< "El valor de 'Tamaño máximo para representación' debe estar entre 0.010 y 100.000."

/// Length in metres at which a nodal load is drawn (see kLoadMetresPerKn).
double nodalLoadDrawLengthM(const LoadScales& scales, const NodalLoad& load);

const NodalLoad* findNodalLoad(const NodalLoads& loads, int id);
/// Ids of the loads of a node, in creation order (what "Editar" selects when the node is clicked).
std::vector<int> nodalLoadsOfNode(const NodalLoads& loads, int nodeId);
std::vector<int> nodalLoadsOfCase(const NodalLoads& loads, int loadCaseId);
/// "Editar", "Hipótesis" and "Borrar" are enabled only when some nodal load exists.
inline bool hasNodalLoads(const NodalLoads& loads) { return !loads.items.empty(); }

/// Unit direction in global axes (the user vector is normalised; {0,0,0} for a null user vector).
Point3 nodalLoadDirection(const NodalLoad& load);
/// Force in kN in global axes: magnitude * direction.
Point3 nodalLoadForce(const NodalLoad& load);

// --- messages (literal es-ES) ---

/// «El valor de 'Valor' debe estar entre 0.01 y 49000.00.» (measured).
std::string nodalLoadRangeMessage();
/// «La cadena introducida no es un número.» (measured).
std::string nodalLoadNotNumberMessage();
/// «La dirección introducida no es correcta.» (#65319, REA; the reproduction for loads is not measured).
std::string nodalLoadDirectionMessage();

/// Rounds to two decimals (what the dialog shows and what is stored).
double roundLoadValue(double value);
/// True when the rounded value is in range.
bool nodalLoadValueInRange(double value);

struct LoadValueParse {
    bool ok = false;
    double value = 0.0;    ///< rounded to two decimals; only when ok
    std::string message;   ///< measured message when !ok
};
/// Text of the field "Valor": trims blanks, accepts "." or "," as decimal separator; no number (also "inf" / "nan" / trailing text) ->
/// «La cadena introducida no es un número.»; out of range (after rounding) -> «El valor de 'Valor' debe estar entre 0.01 y 49000.00.».
LoadValueParse parseLoadValue(const std::string& text);

/// Plain decimal text (trims blanks, "." or ",", optional leading sign; no exponent, "inf" or "nan") -> value. False when it is no number.
bool parsePlainDecimal(const std::string& text, double& value);

/// Data of the dialog (also the data written by "Editar", which overwrites every selected load).
struct NodalLoadData {
    int loadCaseId = 0;
    double magnitude = 9.81;  ///< the dialog proposes 9.81 kN
    LoadDirection direction = LoadDirection::MinusZ;
    Point3 userVector = kDefaultUserVector;
};

/// Empty when valid; otherwise the message of the first problem: value (range message), user vector (finite and not null, only when
/// direction == UserVector).
std::string checkNodalLoadData(const NodalLoadData& data);

// --- pure edits on the data (used by the commands; exposed for tests) ---

/// Deleted nodes lose their loads.
void applyNodalLoadsNodesDeleted(NodalLoads& loads, const std::vector<int>& nodeIds);
/// Node `from` merged into `into`: its loads move to `into` (ids kept; not measured).
void applyNodalLoadsNodesMerged(NodalLoads& loads, int from, int into);
/// Deleted load case: its loads and its scale are removed.
void applyNodalLoadsCaseDeleted(NodalLoads& loads, int loadCaseId);

/// Internal consistency of the stored data: ids unique and below nextId, value in range (two decimals), valid direction and user vector,
/// existing load case, scales finite and positive. Nodes that no longer exist are tolerated (not saved). Empty when valid.
std::string validateNodalLoads(const NodalLoads& loads, const LoadCaseSet& cases);

}  // namespace cc::core
