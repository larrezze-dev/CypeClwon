#pragma once
#include <optional>
#include <string>
#include <vector>

#include "core/project.h"
#include "core/shell_properties.h"
#include "core/undo_stack.h"

namespace cc::core {

/// Undoable edits of the shell properties (T-P6-07). See shell_properties.h for the data. The command keeps a snapshot of the whole
/// ShellProperties before and after, so undo / redo are exact.
///
/// Integration with structure edits: SnapshotCommand (every shell tool: delete, divide, join, ...), DeleteNodesCommand and MoveNodeCommand
/// reconcile with reconcileShellProperties (entries of vanished shells are dropped; cut / joined / moved shells keep what still fits).
class ShellPropertiesCommand : public UndoCommand {
public:
    ShellPropertiesCommand(Project& project, ShellProperties after, std::string text)
        : m_project(project), m_before(project.shellProps), m_after(std::move(after)), m_text(std::move(text)) {}
    /// False when the operation changes nothing.
    bool valid() const { return m_before != m_after; }
    void redo() override { m_project.shellProps = m_after; }
    void undo() override { m_project.shellProps = m_before; }
    std::string text() const override { return m_text; }

private:
    Project& m_project;
    ShellProperties m_before, m_after;
    std::string m_text;
};

/// Shells of the model among `ids`, without repetitions (in the order given).
std::vector<int> existingShells(const Project& project, const std::vector<int>& ids);
/// Edges (key = shell + nodes) that are contour edges of an existing shell, without repetitions.
std::vector<ShellEdgeKey> existingShellEdges(const Project& project, const std::vector<ShellEdgeKey>& keys);

/// All the following run their check first: on error nothing is pushed and the issue is returned; nullopt on success (also when nothing
/// changes: then nothing is pushed). Unknown shells / edges are ignored.

/// "Discretizacion" (RMB on the selected shells, dialog Aceptar).
std::optional<ShellPropIssue> assignShellDiscretisation(Project& project, UndoStack& stack, const std::vector<int>& shellIds, double maxSideM);
/// "Factores de rigidez".
std::optional<ShellPropIssue> assignShellStiffness(Project& project, UndoStack& stack, const std::vector<int>& shellIds, const ShellStiffnessFactors& factors);
/// "Pandeo" / "Analisis modal" of shells.
std::optional<ShellPropIssue> assignShellBuckling(Project& project, UndoStack& stack, const std::vector<int>& shellIds, bool considered);
std::optional<ShellPropIssue> assignShellModal(Project& project, UndoStack& stack, const std::vector<int>& shellIds, bool considered);
/// "Vinculacion interior" of a group of edges.
std::optional<ShellPropIssue> assignEdgeInterior(Project& project, UndoStack& stack, const std::vector<ShellEdgeKey>& edges, const EdgeInteriorBond& bond);
/// "Vinculacion exterior" of a group of edges: runs checkExternalSupport (first Error issue is returned); a support with nothing blocked
/// leaves the edge "Libre".
std::optional<ShellPropIssue> assignEdgeExterior(Project& project, UndoStack& stack, const std::vector<ShellEdgeKey>& edges, const ExternalSupport& support);
/// "Bandas de integracion", closing the dialog with Aceptar: the whole band list of the edited shells. `bands` was edited on the geometry of
/// the first shell of `shellIds`; every shell receives a copy (raw 2D coordinates); bands that do not fit in a shell are dropped for it
/// (TODO(medir)); `dropped` receives the number of bands dropped over all the shells. A band that is invalid for the FIRST shell is an error.
std::optional<ShellPropIssue> assignShellBands(Project& project, UndoStack& stack, const std::vector<int>& shellIds,
                                               const std::vector<ShellIntegrationBand>& bands, int* dropped = nullptr);
/// "Integracion de esfuerzos en bandas".
std::optional<ShellPropIssue> assignBandForceOptions(Project& project, UndoStack& stack, const BandForceOptions& options);

}  // namespace cc::core
