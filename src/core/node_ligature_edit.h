#pragma once
#include <vector>

#include "core/node_ligature.h"
#include "core/project.h"
#include "core/undo_stack.h"

namespace cc::core {

/// Undoable assignment / removal of node ties (T-P6-02). See node_ligature.h for the data and the checks.
/// The command keeps a snapshot of the whole NodeLigatures before and after (they are small), so undo / redo are exact.
class LigatureCommand : public UndoCommand {
public:
    enum class Mode { Assign, Remove };
    /// `nodeIds` in selection order (assign: the first one is the representative).
    LigatureCommand(Project& project, Mode mode, std::vector<int> nodeIds, const LigatureDirections& directions);
    /// False when the operation changes nothing.
    bool valid() const { return m_before != m_after; }
    void redo() override { m_project.ligatures = m_after; }
    void undo() override { m_project.ligatures = m_before; }
    std::string text() const override { return m_mode == Mode::Assign ? "Asignar ligaduras" : "Eliminar ligaduras"; }

private:
    Project& m_project;
    Mode m_mode;
    NodeLigatures m_before, m_after;
};

/// "Asignar". Runs checkLigatureAssign; on Ok pushes the command and returns Ok. Error / Silent: nothing is pushed and the check is returned
/// so the GUI can show the message (Error) or do nothing (Silent).
LigatureCheck assignLigatures(Project& project, UndoStack& stack, const std::vector<int>& nodeIds, const LigatureDirections& directions);
/// "Eliminar". With marked directions the selected nodes leave their ties of those directions; without any, ALL ties of the work are removed
/// (measured), provided at least one existing node was selected. Returns true when something changed (and a command was pushed).
bool removeLigatures(Project& project, UndoStack& stack, const std::vector<int>& nodeIds, const LigatureDirections& directions);

}  // namespace cc::core
