#pragma once
#include <array>
#include <optional>
#include <vector>

#include "core/bar_ends.h"
#include "core/project.h"
#include "core/undo_stack.h"

namespace cc::core {

/// Undoable edits of the bar end data (T-P6-05). See bar_ends.h for the data. The command keeps a snapshot of the whole BarEnds before and
/// after (small), so undo / redo are exact.
class BarEndsCommand : public UndoCommand {
public:
    enum class Mode { Fixity, Articulate, RemoveArticulation };
    BarEndsCommand(Project& project, Mode mode, std::vector<int> barIds, const EndFixityEdit& edit, const std::array<bool, 2>& ends);
    /// Command that stores `after` as the new data (used by compound edits).
    BarEndsCommand(Project& project, BarEnds after, std::string text);
    /// False when the operation changes nothing.
    bool valid() const { return m_before != m_after; }
    void redo() override { m_project.barEnds = m_after; }
    void undo() override { m_project.barEnds = m_before; }
    std::string text() const override { return m_text; }

private:
    Project& m_project;
    BarEnds m_before, m_after;
    std::string m_text;
};

/// Existing bars of a selection, duplicates dropped (order kept).
std::vector<int> existingBars(const Project& project, const std::vector<int>& barIds);

/// "Empotramiento" (RMB on the selected bars, dialog Aceptar). Runs checkEndFixityEdit: on error nothing is pushed and the issue is returned.
/// The confirmation question about the default stiffness (endFixityEditAsksDefaultStiffness) is asked by the GUI BEFORE calling this.
/// Returns nullopt on success (also when nothing changed; then nothing is pushed).
std::optional<EndFixityIssue> assignEndFixity(Project& project, UndoStack& stack, const std::vector<int>& barIds, const EndFixityEdit& edit);
/// "Articular" (Múltiple: RMB on the selected bars): marks the given ends (both by default, as measured) as pinned. The coefficients are
/// NOT changed (measured). Returns true when something changed.
bool articulateBars(Project& project, UndoStack& stack, const std::vector<int>& barIds, const std::array<bool, 2>& ends = {true, true});
/// Removes the articulation of the given ends. (How the original removes an articulation is NOT measured; this is the inverse operation.)
bool removeArticulations(Project& project, UndoStack& stack, const std::vector<int>& barIds, const std::array<bool, 2>& ends = {true, true});

}  // namespace cc::core
