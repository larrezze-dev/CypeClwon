#pragma once
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "core/structure_model.h"

namespace cc::core {

/// Member buckling data (T-P6-04, core part): "Pandeo" (buckling coefficient beta in the local planes xy and xz) and "Pandeo lateral"
/// (lateral-torsional buckling per flange). Behaviour source: docs/research/ui/pandeo.md (REA of ed3dlib.dll: ed3pandeo.c, ed3panlat.c).
///
/// Public API (for the GUI layer)
///  - Data: Project::buckling (BucklingData): map barId -> BarBuckling {xy, xz, top, bottom}. Only bars with non-default data are stored; the
///    default is beta = 1.0 (kind PinnedPinned, unreviewed) in both planes and "no lateral check" in both flanges. The original keeps the data
///    per PIECE; the clone stores it per bar (every bar of a piece receives the same assignment).
///  - Plane (BucklingPlane): `kind` (8 options, enum order = the radio order of the original), `beta` (the persisted number; the radio is rebuilt
///    from it with tolerance 1e-6 as the original does: bucklingKindFromBeta), `lengthM` (user buckling length Lk, kind UserLength),
///    `translational` (kind Computed), `reviewed` (the user accepted the dialog for this bar; false -> "sin revisar" warning).
///  - Lateral flange (LateralBuckling): `kind` (6 options), `coeff` (user coefficient), `spacingM` (distance between bracings),
///    `momentCoeff` (equivalent moment coefficient), `reviewed`.
///  - Queries: bucklingOf(data, barId), effectiveBeta(plane, barLengthM), lateralLengthFactor(flange, barLengthM), unreviewedBucklingBars.
///  - Validation (pure): checkBucklingEdit / checkLateralEdit -> optional issue with the text; validateBuckling (stored data).
///  - Undoable edits (buckling_edit.h): assignBuckling / assignLateralBuckling.
///  - Dividing / deleting bars: see buckling_edit.h (division by SnapshotCommand -> reconcileBuckling; node deletion merge by DeleteNodesCommand).
///    Inverting the bar axis leaves the data untouched (nothing in it depends on the direction of x; the effect of an inverted axis on the
///    xy / xz planes and on the top / bottom flange is NOT measured).
///  - Persistence: JSON section "buckling" (absent in older files = empty).
///
/// PROVISIONAL / NOT MEASURED (TODO(medir)): the field labels of the range messages (here "Coeficiente de pandeo", "Longitud de pandeo",
/// "Separación entre arriostramientos", "Coeficiente de momentos"); decimals of the range text; default of the lateral kind ("No comprueba"
/// here); default of the traslacional / intraslacional choice (traslacional here); range of the moment coefficient (finite and > 0 here);
/// range of the lateral user coefficient (same as beta); the equivalence 0.5 / 1 / 2 of the lateral kinds (inferred from the REA names
/// `coef_mitad_long_barra` ...); the computed beta of the "Cálculo aproximado" kind (needs elements.dll, separate ticket: the caller
/// supplies the value).

inline constexpr double kBucklingBetaMin = 0.0, kBucklingBetaMax = 100.0;        ///< user coefficient range, measured [0.000, 100.000] (pandeo.md 9.2; REA 0.001 is internal)
inline constexpr double kBucklingLengthMin = 0.001, kBucklingLengthMax = 1000.0;  ///< user length range, m (REA 0x1804aa920 / 0x1804b4ee0; measured for the lateral spacing)
inline constexpr double kLateralCoeffMin = 0.0, kLateralCoeffMax = 10.0;          ///< lateral user coefficient, measured (pandeo.md 9.4)
inline constexpr double kMomentCoeffMin = 0.0, kMomentCoeffMax = 10.0;            ///< Cm and C1, measured (pandeo.md 9.4)
inline constexpr double kDefaultLateralSpacingM = 3.0;                            ///< initial "Separación entre arriostramientos", measured
inline constexpr double kBucklingKindTolerance = 1e-6;                            ///< beta -> kind reconstruction (REA FUN_1804aa5c0)

/// Order and values = i_tipo_t of the original (ed3pandeo.c).
enum class BucklingKind {
    None = 0,         ///< sin_pandeo: "No considerar pandeo" (beta = 0)
    FixedFixed,       ///< coef_barra_biempotrada: beta 0.5
    PinnedFixed,      ///< coef_barra_apoyada_empotrada: beta 0.7
    PinnedPinned,     ///< coef_barra_biapoyada: beta 1.0 (default)
    Cantilever,       ///< coef_barra_voladizo: beta 2.0
    UserBeta,         ///< coef_definido_usuario: typed beta
    UserLength,       ///< longitud_definida_por_usuario: Lk typed (beta = Lk / L)
    Computed,         ///< longitud_calculada: approximate calculation (traslacional / intraslacional)
};

struct BucklingPlane {
    BucklingKind kind = BucklingKind::PinnedPinned;
    double beta = 1.0;          ///< persisted coefficient (0 = no buckling); for UserLength see effectiveBeta
    double lengthM = 1.0;       ///< user buckling length Lk (m), used when kind == UserLength
    bool translational = false;  ///< Computed only: translational (true) / intraslacional (false, default measured)
    double momentCoeff = 1.0;    ///< equivalent moment coefficient Cm of the plane (steel; the label always shows it), range [0, 10]
    bool reviewed = false;
    bool operator==(const BucklingPlane&) const = default;
};

/// Order and values = i_tipo_t of ed3panlat.c.
enum class LateralKind {
    None = 0,       ///< pan_lat_sin_pandeo: "No comprueba pandeo lateral"
    HalfLength,     ///< coef_mitad_long_barra: Lc = 0.5 L
    Length,         ///< coef_long_barra: Lc = 1.0 L
    DoubleLength,   ///< coef_doble_long_barra: Lc = 2.0 L
    UserCoeff,      ///< coef_usuario: Lc = coeff L
    UserSpacing,    ///< longitud_usuario: Lc = spacing between bracings
};

struct LateralBuckling {
    LateralKind kind = LateralKind::None;
    double coeff = 1.0;        ///< UserCoeff
    double spacingM = kDefaultLateralSpacingM;  ///< UserSpacing, m
    double momentCoeff = 1.0;  ///< equivalent moment coefficient (Cm / Cb according to the standard)
    bool reviewed = false;
    bool operator==(const LateralBuckling&) const = default;
};

struct BarBuckling {
    BucklingPlane xy, xz;
    double c1 = 1.0;                 ///< "Factor de modificación para el momento crítico C1" (one value per bar), range [0, 10]
    bool considerInAnalysis = true;  ///< "Considerar en el análisis de pandeo"
    bool updateXY = false;           ///< "Permitir actualizar los coeficientes de pandeo en el plano xy"
    bool updateXZ = false;           ///< "... en el plano xz"
    LateralBuckling top, bottom;  ///< top = flange on the positive side of the axis perpendicular to the strong axis (local system), REA #65042
    bool operator==(const BarBuckling&) const = default;
};

struct BucklingData {
    std::map<int, BarBuckling> bars;  ///< only bars whose data differs from the default
    bool empty() const { return bars.empty(); }
    bool operator==(const BucklingData&) const = default;
};

/// Beta of the five fixed kinds (0 / 0.5 / 0.7 / 1 / 2); nullopt for UserBeta, UserLength and Computed.
std::optional<double> bucklingBetaOfKind(BucklingKind kind);
/// Rebuilds the radio from a stored beta (REA FUN_1804aa5c0): |beta| < 1e-6 -> None, 0.5 / 0.7 / 1 / 2 (tol 1e-6), otherwise UserBeta.
BucklingKind bucklingKindFromBeta(double beta);
/// Plane with the given beta and the kind rebuilt from it.
BucklingPlane bucklingPlaneFromBeta(double beta, bool reviewed = true);

/// Beta in force for a bar of length `barLengthM`: None -> 0; UserLength -> lengthM / barLengthM (0 when the length is not positive);
/// otherwise `beta`.
double effectiveBeta(const BucklingPlane& plane, double barLengthM);
/// Lc / L of a flange (0 for None; 0.5, 1, 2; coeff; spacing / L).
double lateralLengthFactor(const LateralBuckling& flange, double barLengthM);

bool isDefaultBuckling(const BarBuckling& b);
BarBuckling bucklingOf(const BucklingData& data, int barId);
/// Stores a record, erasing the entry when it is the default.
void setBuckling(BucklingData& data, int barId, const BarBuckling& record);

/// Bars of the model whose plane xy or xz has never been accepted by the user (warning of REA #64961).
std::vector<int> unreviewedBucklingBars(const BucklingData& data, const StructuralModel& model);

/// Dialog "Pandeo": per plane, the chosen radio and its fields. For the fixed kinds the fields are ignored. For Computed, `beta` carries the
/// value computed by the caller (not computed in the core).
struct BucklingPlaneEdit {
    BucklingKind kind = BucklingKind::PinnedPinned;
    double beta = 1.0;       ///< UserBeta (typed) or Computed (calculated)
    double lengthM = 1.0;    ///< UserLength
    bool translational = false;
    double momentCoeff = 1.0;  ///< Cm, applied when the matching `assignMoment*` flag is on
    bool operator==(const BucklingPlaneEdit&) const = default;
};
struct BucklingEdit {
    bool assignXY = true, assignXZ = true;
    bool assignMomentXY = false, assignMomentXZ = false;  ///< "Asignar coeficiente de momentos (Plano ..)" (steel variant of the dialog)
    bool assignCriteria = true;                           ///< "Asignar criterios para el análisis de pandeo"
    bool considerInAnalysis = true, updateXY = false, updateXZ = false;
    BucklingPlaneEdit xy, xz;
    bool operator==(const BucklingEdit&) const = default;
};
/// Dialog "Pandeo lateral": per flange. A flag off leaves that flange untouched.
struct LateralEdit {
    bool assignTop = true, assignBottom = true;
    bool assignC1 = true;  ///< "Asignar factor de momento crítico"
    double c1 = 1.0;
    LateralBuckling top, bottom;
    bool operator==(const LateralEdit&) const = default;
};

struct BucklingIssue {
    int messageId = 0;  ///< original id (0: text built with the measured pattern, id not located)
    std::string text;
};

/// "El valor de '<field>' debe estar entre A y B." Measured (pandeo.md 9.2 / 9.4): the coefficient fields (beta, lateral coefficient, Cm, C1) say
/// "entre 0.000  y 100.000 ." (two spaces after the first bound and a space before the full stop: `spaced`), the length fields
/// "entre 0.001 y 1000.000." (`spaced` = false). The field of C1 is empty in the original ("El valor de '' ...").
std::string bucklingRangeMessage(const std::string& field, double lo, double hi, bool spaced = true);

/// Label lines drawn on the bar by the tool "Pandeo": "xy: 1.00 / 1.00", "xz: ..." (beta, Cm, 2 decimals; pandeo.md 9.5).
std::vector<std::string> bucklingLabelLines(const BarBuckling& b, double barLengthM);
/// Label lines of the tool "Pandeo lateral": "Sup.: L, 1.00", "Inf.: 2L, 1.00", "C1: 1.00" (Lc as L / nL / "3.000 m"; pandeo.md 9.5).
std::vector<std::string> lateralLabelLines(const BarBuckling& b, double barLengthM);
/// Lines of the floating panel of a bar under the tool "Pandeo lateral" (pandeo.md 9.6): "Pandeo lateral", "Sup. : 0.5L, 1.00", "Inf. : 2L, 1.00",
/// "C\u2081 : 1.00" (C with a subscript one); empty for a bar without lateral data; a flange assigned "No comprueba" has no line.
std::vector<std::string> lateralInfoLines(const BarBuckling& b);
/// Lines of the floating information panel of a bar under the tool "Pandeo" (pandeo.md 9.5): heading "Pandeo", "xy : 1.00 / 1.00",
/// "xz : ...", heading "Análisis de pandeo", "Se considera en el análisis" (the negative text is TODO(medir)).
std::vector<std::string> bucklingInfoLines(const BarBuckling& b, double barLengthM);

/// First error of the edit (xy then xz): UserBeta beta outside [0.001, 100] or not finite; UserLength outside [0.001, 1000]; Computed beta
/// not finite or outside [0, 100]. Fixed kinds never fail. nullopt when valid.
std::optional<BucklingIssue> checkBucklingEdit(const BucklingEdit& edit);
/// First error of the lateral edit (top then bottom): UserCoeff outside [0.001, 100], UserSpacing outside [0.001, 1000], and for every kind but
/// None the moment coefficient not finite or <= 0.
std::optional<BucklingIssue> checkLateralEdit(const LateralEdit& edit);

/// Result of a plane edit applied to a plane (pure; no validation; marks it reviewed).
BucklingPlane applyBucklingPlaneEdit(const BucklingPlane& current, const BucklingPlaneEdit& edit);
/// Applies the dialog to a record (only the planes to assign).
void applyBucklingEdit(BarBuckling& record, const BucklingEdit& edit);
void applyLateralEdit(BarBuckling& record, const LateralEdit& edit);

/// Internal consistency of the stored data; empty when valid. Entries of bars that no longer exist are tolerated (not saved).
std::string validateBuckling(const BucklingData& data, const StructuralModel& model);

/// Reconciles the data after an edit of the structure that may have divided bars (SnapshotCommand): every new stretch that continues a
/// shortened bar (same start, new end node, chain up to the original end node) inherits ALL the data of that bar (buckling coefficients are
/// properties of the whole member; a user length Lk is kept as typed, TODO(medir)). Entries of bars that no longer exist are dropped.
BucklingData reconcileBuckling(const BucklingData& data, const StructuralModel& before, const StructuralModel& after);

}  // namespace cc::core
