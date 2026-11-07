#pragma once
#include <array>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "core/structure_model.h"

namespace cc::core {

/// Bar end data (T-P6-05, core part): articulations ("Articular") and partial fixity ("Empotramiento") at the ends of a bar.
/// Behaviour source: docs/research/ui/articulaciones.md (REA of ed3dlib.dll: coef_empotramiento_xy/xz/torsion_inicial/final,
/// i_rigidez_asignar_t; measured in the original).
///
/// Public API (for the GUI layer)
///  - Data: Project::barEnds (BarEnds): map barId -> BarEndsRecord {origin, end} (index 0 = "Origen de la pieza" = START node of the bar, 1 = "Extremo de la pieza" = END node;
///    InvertBarAxisCommand swaps the nodes AND the two records, so the index always follows the nodes).
///    Only bars with non-default data are stored. Each end is a BarEndFixity: `pinned` (articulation symbol; a datum APART from the
///    coefficients, measured) plus the fixity of the planes xy, xz and yz (yz = torsion).
///  - Fixity of a plane (EndPlaneFixity): either a coefficient 0..1 (1.000 = fixed, default; 0 = pinned) or a rotational stiffness
///    (kN.m/rad, default 500000, which makes the dialog ask a confirmation question).
///  - Queries: barEndsOf(ends, barId), endFixityLabel(end) ("xy: 1.000 xz: 1.000 yz: 1.000"), endTooltipLine(...), isDefaultEnd.
///  - Validation (pure): checkEndFixityEdit(edit) -> optional issue with the literal text; endFixityEditAsksDefaultStiffness(edit)
///    -> the confirmation question text (#65010-like, measured).
///  - Undoable edits (bar_ends_edit.h): assignEndFixity / articulateBars / removeArticulations.
///  - Splitting / deleting / inverting bars: see bar_ends_edit.h (splitting is reconciled by SnapshotCommand, inversion by InvertBarAxisCommand).
///  - Persistence: JSON section "bar_ends" (absent in older files = empty).
///
/// PROVISIONAL / NOT MEASURED (TODO(medir)): effect of an articulation on the solver (a pinned end releases the bending moments; the
/// coefficient 0 is equivalent, REA); which end the "Individual" mode of Articular articulates; the valid range of the rotational stiffness
/// (finite and > 0 is required here); how a pinned end shows in the label; combination with the partial fixity of the node (internal link);
/// the "Parejas" (bracing pairs, #387777) have nothing to do with this data and are another task.

/// Index of the end of a bar: 0 = origin (start of the local x axis), 1 = end.
inline constexpr int kBarOrigin = 0, kBarFinal = 1;
/// Plane index: 0 = xy, 1 = xz, 2 = yz (torsion).
inline constexpr int kPlaneXY = 0, kPlaneXZ = 1, kPlaneYZ = 2;

/// Default rotational stiffness proposed by the dialog (kN.m/rad, measured: 500000.00).
inline constexpr double kDefaultEndRotationalStiffness = 500000.0;

enum class EndFixityKind { Coefficient, Rotational };

struct EndPlaneFixity {
    EndFixityKind kind = EndFixityKind::Coefficient;
    double coefficient = 1.0;                                 ///< 0 (pinned) .. 1 (fixed); used when kind == Coefficient
    double stiffnessKnMPerRad = kDefaultEndRotationalStiffness;  ///< used when kind == Rotational
    bool operator==(const EndPlaneFixity&) const = default;
};

struct BarEndFixity {
    bool pinned = false;                       ///< articulation symbol of "Articular"
    std::array<EndPlaneFixity, 3> planes{};    ///< xy, xz, yz
    bool operator==(const BarEndFixity&) const = default;
};

struct BarEndsRecord {
    std::array<BarEndFixity, 2> ends{};  ///< origin, end
    bool operator==(const BarEndsRecord&) const = default;
};

struct BarEnds {
    std::map<int, BarEndsRecord> bars;  ///< only bars whose data differs from the default (all fixed, nothing pinned)
    bool empty() const { return bars.empty(); }
    bool operator==(const BarEnds&) const = default;
};

bool isDefaultEnd(const BarEndFixity& end);
/// Record of a bar (the default one when none is stored).
BarEndsRecord barEndsOf(const BarEnds& ends, int barId);
/// Stores a record, erasing the entry when it is the default.
void setBarEnds(BarEnds& ends, int barId, const BarEndsRecord& record);
/// Swaps origin and end (the bar axis is inverted: the physical structure must not change, #64641).
BarEndsRecord swappedEnds(const BarEndsRecord& record);

/// Which end(s) an Articular action reaches (REA ed3barra.c FUN_1800c03c0 modes 0 / 1 / 2, articulaciones.md sec. 8.1).
enum class ArticulationEnd { Start, End, Both };

/// Individual mode, end chosen by the click (REA FUN_1800cf4b0, sec. 8.3): d0 / d1 = distance of the click to the start / end, length = L:
/// Start if `d1 - L/3 > d0`; End if `d0 - L/3 > d1`; otherwise Both.
ArticulationEnd articulationEndForClick(double distanceToStart, double distanceToEnd, double length);
/// Same rule for a click at parameter t in [0, 1] along a straight bar (d0 = t L, d1 = (1 - t) L): first third -> Start, last third -> End,
/// central third -> Both.
ArticulationEnd articulationEndForParameter(double t);
/// Toggle of the articulation flags (REA, sec. 8.1). Start / End: flips that flag. Both: both pinned -> both free; none pinned -> both pinned;
/// exactly one pinned (mixed) -> no change. Removing an articulation is applying Articular again; there is no "already articulated" warning.
void toggleArticulation(BarEndsRecord& record, ArticulationEnd end);

/// "xy: 1.000 xz: 1.000 yz: 1.000" (3 decimals; a rotational plane shows its stiffness with 2 decimals and the unit, TODO(medir)).
std::string endFixityLabel(const BarEndFixity& end);
/// Line of the tooltip of the Empotramiento tool: "Origen : xy: 1.000 xz: 1.000 yz: 1.000" / "Extremo : ...".
std::string endTooltipLine(const BarEndsRecord& record, int endIndex);

/// Contents of the dialog "Empotramiento en extremos": the three check boxes "Asignar (Plano xy / xz / yz)" (checked by default) and, per end,
/// the fixity of each plane. Planes whose box is off are left untouched.
struct EndFixityEdit {
    std::array<bool, 3> assignPlane{true, true, true};
    std::array<std::array<EndPlaneFixity, 3>, 2> value{};  ///< [end][plane]
    bool operator==(const EndFixityEdit&) const = default;
};

struct EndFixityIssue {
    int messageId = 0;   ///< original id (0 when the text was measured but its id was not located)
    std::string text;
};

/// "El valor de '' debe estar entre 0.000 y 1.000 ." (measured: the field name comes out empty).
std::string endFixityRangeMessage();
/// Question shown when a rotational stiffness equals the default (measured; Sí default / No).
std::string endFixityDefaultStiffnessQuestion();

/// First error of the edit, in the order end origin / end, planes xy / xz / yz, only for the planes to assign: coefficient outside 0..1
/// (or not finite), yz coefficient other than 0 / 1 (the original offers a binary popup), stiffness not finite or <= 0 (PROVISIONAL).
/// nullopt when valid.
std::optional<EndFixityIssue> checkEndFixityEdit(const EndFixityEdit& edit);
/// True when some assigned plane uses a rotational stiffness equal to the default: the dialog asks the question before accepting.
bool endFixityEditAsksDefaultStiffness(const EndFixityEdit& edit);
/// Applies the edit to a record (pure; no validation).
void applyEndFixityEdit(BarEndsRecord& record, const EndFixityEdit& edit);

/// Internal consistency of the stored data; empty when valid. Entries of bars that no longer exist are tolerated (not saved).
std::string validateBarEnds(const BarEnds& ends, const StructuralModel& model);

/// Reconciles the data after an edit of the structure that may have divided bars (SnapshotCommand): a bar that kept its id but was
/// shortened (same start, new end node) hands its END data to the last of the new stretches that continue it (the chain of new bars starting
/// at its new end node); the intermediate ends take the default. Entries of bars that no longer exist are dropped.
BarEnds reconcileBarEnds(const BarEnds& ends, const StructuralModel& before, const StructuralModel& after);

}  // namespace cc::core
