#pragma once
#include <optional>
#include <string>
#include <vector>

#include "core/buckling.h"
#include "core/project.h"
#include "core/undo_stack.h"

namespace cc::core {

/// Undoable edits of the buckling data (T-P6-04). See buckling.h for the data. The command keeps a snapshot of the whole BucklingData before
/// and after (small), so undo / redo are exact.
///
/// Integration with structure edits (same scheme as bar_ends): SnapshotCommand reconciles with reconcileBuckling (a divided bar: every new
/// stretch inherits the data); DeleteNodesCommand gives the bar that merges two aligned bars the data of the first (lower id) of them; an
/// inverted axis keeps the data; deleting a bar drops its entry from the saved file (kept in the undo snapshots).
class BucklingCommand : public UndoCommand {
public:
    BucklingCommand(Project& project, BucklingData after, std::string text)
        : m_project(project), m_before(project.buckling), m_after(std::move(after)), m_text(std::move(text)) {}
    /// False when the operation changes nothing.
    bool valid() const { return m_before != m_after; }
    void redo() override { m_project.buckling = m_after; }
    void undo() override { m_project.buckling = m_before; }
    std::string text() const override { return m_text; }

private:
    Project& m_project;
    BucklingData m_before, m_after;
    std::string m_text;
};

/// "Pandeo" (RMB on the selected bars, dialog Aceptar). Runs checkBucklingEdit: on error nothing is pushed and the issue is returned. Marks the
/// assigned planes as reviewed. Returns nullopt on success (also when nothing changed; then nothing is pushed). Unknown bar ids are ignored.
std::optional<BucklingIssue> assignBuckling(Project& project, UndoStack& stack, const std::vector<int>& barIds, const BucklingEdit& edit);
/// "Pandeo lateral": same contract for the flanges.
std::optional<BucklingIssue> assignLateralBuckling(Project& project, UndoStack& stack, const std::vector<int>& barIds, const LateralEdit& edit);

}  // namespace cc::core
