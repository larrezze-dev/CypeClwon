#pragma once
#include <optional>
#include <string>
#include <vector>

#include "core/project.h"
#include "core/structure_snapshot.h"
#include "core/undo_stack.h"

namespace cc::core {

/// Undoable operations of "Geometria > Herramientas" (T-P5-10, core only). Behaviour source: docs/research/ui/herramientas.md.
/// Mover / Girar move the selection, Copiar / Simetria copy it (Simetria can also move). Every command edits a copy of the
/// model and swaps it on redo / undo (SnapshotCommand). Nothing here is measured against the original (herramientas.md
/// section 4: the algorithms were not found); the points below marked PROVISIONAL are the clone's decisions.

/// Rigid or mirroring transform p' = R p + t of space (metres). Built with the factory functions below.
struct Transform3 {
    double r[9] = {1, 0, 0, 0, 1, 0, 0, 0, 1};  ///< row-major 3x3 matrix
    Point3 t;
    Point3 apply(const Point3& p) const;
    /// True when the matrix has a negative determinant (a mirror image): the contour order of shells must be reversed.
    bool reflects() const;
    /// this applied after `first`.
    Transform3 after(const Transform3& first) const;
};

/// "Mover" / "Copiar": translation from point a to point b (#64803).
Transform3 translation(const Point3& from, const Point3& to);
/// "Girar elementos alrededor de un eje definido por dos puntos" (#64805): right-handed rotation about the line through
/// a and b by `angleDeg` (degrees; PROVISIONAL sign convention: right-hand rule along the direction a -> b). nullopt when a and b coincide.
std::optional<Transform3> rotationAboutAxis(const Point3& a, const Point3& b, double angleDeg);
/// "Girar en el plano xy" (#64808): rotation about the vertical line through `origin`.
Transform3 rotationInXY(const Point3& origin, double angleDeg);
/// "Simetria respecto de un plano" (#64820): reflection across the plane through three points; nullopt if collinear.
std::optional<Transform3> reflectionAcrossPlane(const Point3& a, const Point3& b, const Point3& c);
/// "Simetria en el plano xy" (#64817): reflection across the vertical plane that contains the line a-b; nullopt if a, b
/// coincide or a-b is vertical.
std::optional<Transform3> reflectionVertical(const Point3& a, const Point3& b);

/// Elements the tools act on. The nodes of the selected bars and shells are implied.
struct ToolSelection {
    std::vector<int> nodeIds, barIds, shellIds;
};

enum class ToolError {
    None,
    NothingSelected,   ///< empty selection
    UnknownElement,    ///< an id does not exist
    InvalidTransform,  ///< non-finite transform or copies < 1
    InvalidResult,     ///< the move would make a bar shorter than kMinBarLengthM or a shell invalid
    NothingCopied      ///< every copy would coincide with existing elements
};
std::string toolErrorMessage(ToolError error);

/// Moves (copy = false) or copies (copy = true) the selection through `transform`.
///
/// Move: the nodes of the selection (selected nodes plus those of selected bars and shells) are transformed in place; bars
/// and shells that use them follow. The result must keep every touched bar at least kMinBarLengthM long and every touched
/// shell a valid planar contour (InvalidResult otherwise). No node merge happens (use "Reparar"). A mirror image reverses the contour
/// order of the shells that move completely (and swaps their x edge) so their normal keeps pointing to the same side. Pieces whose bars
/// stop being aligned are split into one-bar pieces (type and group kept).
///
/// Copy (`copies` times, the k-th copy applies the transform k times, so a translation gives an array): every node of the
/// closure is copied, reusing the existing node found at the destination (within kCoincidentNodeToleranceM, PROVISIONAL); the
/// selected bars and shells are copied with their material, section (so the "Describir" description is shared, no new description),
/// roll angle, x-axis inversion, thickness, z inversion, x edge and layers. A bar or shell that would repeat an existing one is skipped.
/// Mirror copies reverse the contour order of shells (PROVISIONAL). Pieces whose bars were all copied are copied (type kept; groups
/// map to new groups of the copies). Continuous elements, references and deflection groups are not copied (ticket).
class TransformElementsCommand : public SnapshotCommand {
public:
    TransformElementsCommand(Project& project, const ToolSelection& selection, const Transform3& transform, bool copy, int copies = 1);
    bool valid() const { return m_error == ToolError::None; }
    ToolError error() const { return m_error; }
    /// Ids created by a copy, in creation order.
    const std::vector<int>& createdNodeIds() const { return m_nodes; }
    const std::vector<int>& createdBarIds() const { return m_bars; }
    const std::vector<int>& createdShellIds() const { return m_shells; }

private:
    ToolError m_error = ToolError::None;
    std::vector<int> m_nodes, m_bars, m_shells;
};

ToolError transformElements(Project& project, UndoStack& stack, const ToolSelection& selection, const Transform3& transform, bool copy,
                            int copies = 1);

// --- Reparar ---------------------------------------------------------------------------------------------------------

/// Distance (metres) under which nodes are merged and a node counts as lying on a bar or a shell edge. NOT MEASURED
/// (herramientas.md section 3: the tolerance of the original is a parameter whose value was not read): provisional.
inline constexpr double kRepairToleranceM = 1e-3;

struct RepairIssue {
    enum class Kind { NodesTooClose, NodeOnShellEdge, UnneededNode, BarsOverlap, NodeOnBar };
    Kind kind = Kind::NodesTooClose;
    std::string message;  ///< original text with its %s filled in (#399480, #400344, #400345, #400346, #400347)
    std::vector<int> ids; ///< nodes / bars named by the message
};

struct RepairReport {
    int mergedNodes = 0;       ///< nodes removed because they coincide with another one
    int removedBars = 0;       ///< degenerate or repeated bars removed
    int splitBars = 0;         ///< extra bars created by dividing bars at intermediate nodes
    int splitShellEdges = 0;   ///< nodes inserted into shell contours
    int removedShells = 0;     ///< shells that collapsed after merging nodes
    std::vector<RepairIssue> issues;  ///< what was found in the model before repairing (unneeded nodes are only reported)
    bool modified() const { return mergedNodes + removedBars + splitBars + splitShellEdges + removedShells > 0; }
};

/// #399477 (confirmation), #399478 (modified) and #399479 (not modified).
std::string repairConfirmationMessage();
std::string repairResultMessage(bool modified);

/// Looks for the situations "Reparar" corrects or reports, without changing anything.
std::vector<RepairIssue> analyseGeometry(const StructuralModel& model, double toleranceM = kRepairToleranceM);

/// "Reparar": (1) merges nodes closer than the tolerance into the lowest-id one, dropping bars that collapse or repeat and
/// shells that collapse; (2) divides bars at the nodes lying on them; (3) removes repeated bars (bars overlapped over
/// a stretch end up repeated after step 2); (4) inserts the nodes lying on shell edges into the contours (a contour of
/// more than 4 nodes becomes several shells). Unneeded nodes are only reported (PROVISIONAL: which of #400345-#400347
/// the original repairs automatically is not known). Pieces that stay consistent are kept and extended; others are dropped. Not committed
/// (nothing to push) when nothing changed: check committed() / report().modified().
class RepairModelCommand : public SnapshotCommand {
public:
    explicit RepairModelCommand(Project& project, double toleranceM = kRepairToleranceM);
    const RepairReport& report() const { return m_report; }

private:
    RepairReport m_report;
};

/// Runs the repair and pushes it when it changed the model; returns whether it did. `report` receives the findings either way.
bool repairModel(Project& project, UndoStack& stack, RepairReport* report = nullptr, double toleranceM = kRepairToleranceM);

}  // namespace cc::core
