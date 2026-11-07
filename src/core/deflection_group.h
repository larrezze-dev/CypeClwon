#pragma once
#include <array>
#include <optional>
#include <string>
#include <vector>

#include "core/structure_model.h"

namespace cc::core {

/// Deflection groups (T-P6-06, core part): "Grupos de flecha" of Propiedades > Barras. Behaviour source: docs/research/ui/grupos_flecha.md
/// (REA of ed3dlib.dll: ed3grufle, ed3comun.c, ed3tipo.c; measured in the original).
///
/// Model. EVERY bar belongs to exactly one group; by default each bar is a group of its own, of type "secante" in both planes and without
/// limits. Only the groups that differ from that default are stored (Project::deflection, DeflectionData::groups): groups of several
/// aligned bars, and one-bar groups with a tangent type or a limit. The complete list (implicit groups included) is built by
/// deflectionGroups(): sorted by the lowest bar id of each group, the number "Gn" is the 1-based position in that list (PROVISIONAL: the
/// original does not keep the numbering stable either, grupos_flecha.md section 4.5; no rule measured).
///
/// Public API (for the GUI layer)
///  - Data: DeflectionBarGroup {bars (ordered along the chain from node 1 to node 2), node1, xy, xz, limits}. node1 / node2 are the "nudos 1 y 2" the
///    tangent types refer to; for a one-bar group node 1 is the origin of the local x axis (start node, end node when Bar::xAxisInverted).
///  - List and queries: deflectionGroups(data, model) -> DeflectionGroupInfo (number, bars, node1, node2, types, limits),
///    deflectionGroupIndexOfBar, deflectionGroupLabel (green label "G1(1-2 xy, 1-2 xz)").
///  - Limits: 8 blocks (DeflectionLimits, slots LimitSlot), checkDeflectionLimits (validation of the enabled blocks), deflectionLimitName.
///  - Undoable edits (deflection_group_edit.h): createDeflectionGroup / editDeflectionTypes / setDeflectionLimits and the pre-dialog check
///    checkCreateDeflectionGroup. There is no "Eliminar" command (a group is overridden by creating another one that overlaps it).
///  - Structure edits keep the data consistent (see deflection_group_edit.h): dividing a bar extends its explicit group, deleting an
///    intermediate node merges the bars, inverting the x axis of a whole group swaps node 1 / node 2 (and the tangent types) so the physical
///    meaning is kept, anything that breaks the alignment dissolves the group.
///  - Persistence: JSON section "deflection_groups" (absent in older files = every bar its own secant group).
///
/// PROVISIONAL / NOT MEASURED (TODO(medir)): notation of tangent types in the label ("1" / "2" here); the texts of the limit validation
/// messages; whether the absolute threshold 0.00001 applies to the value typed in mm (here) or to metres; numbering of "Gn"; which node is
/// node 1 of a group created from the nodes (here: the first node chosen); effect of inverting only part of a group (none here).

/// Minimum accepted values of the dialog "Flecha Límite" (REA assertions 0x180a8b688-0x180a8bda8; grupos_flecha.md section 3).
inline constexpr double kMinAbsoluteDeflectionLimit = 0.00001;  ///< absolute limit, as typed (mm)
inline constexpr double kMinDeflectionDivisor = 0.0001;         ///< relative limit divisor (the limit is L / divisor)
inline constexpr double kDefaultAbsoluteDeflectionLimit = 1.0;  ///< initial value of the absolute field (mm), measured
inline constexpr double kDefaultDeflectionDivisor = 250.0;      ///< initial radio "L / 250", measured

/// Order and values = ed3tipo_flecha_t of the original (ed3tipo.c).
enum class DeflectionType {
    Secant = 0,          ///< "Secante" (default)
    TangentToNode1 = 1,  ///< "Tangente a nudo 1"
    TangentToNode2 = 2,  ///< "Tangente a nudo 2"
};

/// The eight blocks of "Flecha Límite", in the order of the dialog (max, active; absolute, relative; xy, xz per pair).
enum class LimitSlot { MaxAbsXY = 0, MaxAbsXZ, ActiveAbsXY, ActiveAbsXZ, MaxRelXY, MaxRelXZ, ActiveRelXY, ActiveRelXZ };
inline constexpr int kLimitSlotCount = 8;

struct DeflectionLimitItem {
    bool enabled = false;  ///< the check box of the block
    double absoluteMm = kDefaultAbsoluteDeflectionLimit;  ///< absolute blocks: the limit, in mm (must be > kMinAbsoluteDeflectionLimit when enabled)
    double relativeDivisor = kDefaultDeflectionDivisor;   ///< relative blocks: the limit is L / divisor (> kMinDeflectionDivisor when enabled)
    bool operator==(const DeflectionLimitItem&) const = default;
};

/// True for the preset radios L / 250, 300, 400, 500; any other divisor is the user radio "L / ?".
bool isPresetDeflectionDivisor(double divisor);

struct DeflectionLimits {
    std::array<DeflectionLimitItem, kLimitSlotCount> items{};
    DeflectionLimitItem& item(LimitSlot s) { return items[static_cast<std::size_t>(s)]; }
    const DeflectionLimitItem& item(LimitSlot s) const { return items[static_cast<std::size_t>(s)]; }
    bool operator==(const DeflectionLimits&) const = default;
};

struct DeflectionBarGroup {
    std::vector<int> bars;  ///< ordered along the chain, from node 1 to node 2
    int node1 = 0;          ///< "nudo 1" (recomputed for one-bar groups)
    DeflectionType xy = DeflectionType::Secant;
    DeflectionType xz = DeflectionType::Secant;
    DeflectionLimits limits;
    bool operator==(const DeflectionBarGroup&) const = default;
};

/// A one-bar group of default types and limits (what every bar is by default).
bool isDefaultDeflectionGroup(const DeflectionBarGroup& group);

struct DeflectionData {
    std::vector<DeflectionBarGroup> groups;  ///< only the groups that differ from "one secant group per bar"
    bool empty() const { return groups.empty(); }
    bool operator==(const DeflectionData&) const = default;
};

/// An entry of the complete list (explicit and implicit groups).
struct DeflectionGroupInfo {
    int number = 0;  ///< "Gn", 1-based
    std::vector<int> bars;
    int node1 = 0, node2 = 0;
    DeflectionType xy = DeflectionType::Secant;
    DeflectionType xz = DeflectionType::Secant;
    DeflectionLimits limits;
    bool operator==(const DeflectionGroupInfo&) const = default;
};

/// Node of the bar at the origin of its local x axis (start node, or end node when Bar::xAxisInverted).
int barXOrigin(const Bar& bar);

/// Cleans the data against the model: unknown bars are dropped, a bar already in an earlier group is dropped from the later ones, a
/// multi-bar group that is not a straight chain any more is dissolved (its bars become implicit groups), the bars are put in chain
/// order, node1 is kept when it is still an end of the chain (otherwise the end with the lower node id), one-bar groups take node1 =
/// x origin, and groups that equal the default are removed.
DeflectionData normaliseDeflection(const DeflectionData& data, const StructuralModel& model);

/// Complete list: every group of the model (implicit one-bar secant groups included), sorted by the lowest bar id, numbered from 1.
std::vector<DeflectionGroupInfo> deflectionGroups(const DeflectionData& data, const StructuralModel& model);
/// Index in `list` of the group that holds the bar; -1 when there is none.
int deflectionGroupIndexOfBar(const std::vector<DeflectionGroupInfo>& list, int barId);

/// Short text of a type for the label: "1-2" secant, "1" tangent to node 1, "2" tangent to node 2 (tangent notation TODO(medir)).
std::string deflectionTypeLabel(DeflectionType type);
/// Label drawn on every bar of the group: "G1(1-2 xy, 1-2 xz)".
std::string deflectionGroupLabel(const DeflectionGroupInfo& group);
/// Texts of the radios: "Secante", "Tangente a nudo 1", "Tangente a nudo 2".
std::string deflectionTypeName(DeflectionType type);
/// Name of a block of the dialog: "Flecha máxima absoluta xy", ...
std::string deflectionLimitName(LimitSlot slot);
/// True for the absolute blocks (value in mm), false for the relative ones (L / divisor).
bool isAbsoluteLimitSlot(LimitSlot slot);

struct DeflectionIssue {
    int messageId = 0;  ///< original id (0: text of the clone, not measured)
    std::string text;
};

/// First invalid ENABLED block (absolute value not finite or <= 0.00001; divisor not finite or <= 0.0001); nullopt when valid.
/// The text is the clone's own wording (TODO(medir)).
std::optional<DeflectionIssue> checkDeflectionLimits(const DeflectionLimits& limits);

/// Internal consistency of the stored data (no unknown types, valid limits, no bar in two groups, no default group stored). Bars that no
/// longer exist are tolerated (they are not saved). Empty when valid.
std::string validateDeflection(const DeflectionData& data);

/// Reconciles the data after an edit of the structure that may have divided bars (SnapshotCommand): the stretches that continue a
/// shortened bar that belongs to an explicit group join that group; the result is normalised against `after`.
DeflectionData reconcileDeflection(const DeflectionData& data, const StructuralModel& before, const StructuralModel& after);

/// Inversion of the x axis of the bars in `inverted` (InvertBarAxisCommand): every explicit group whose bars are ALL in `inverted` reverses its
/// bar order and swaps node 1 / node 2 and the tangent types, so the physical meaning is kept. `after` is the model after the inversion.
DeflectionData invertDeflectionGroups(const DeflectionData& data, const std::vector<int>& inverted, const StructuralModel& after);

}  // namespace cc::core
