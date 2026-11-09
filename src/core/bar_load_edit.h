#pragma once
#include <algorithm>
#include <string>
#include <vector>

#include "core/bar_load.h"
#include "core/project.h"
#include "core/undo_stack.h"

namespace cc::core {

/// Undoable edits of the bar loads (T-P7-03). See bar_load.h for the data and the checks. The command keeps a snapshot of the whole BarLoads
/// before and after (small), so undo / redo are exact; ids are never reused (nextId only grows).
class BarLoadCommand : public UndoCommand {
public:
    BarLoadCommand(Project& project, BarLoads after, std::string text)
        : m_project(project), m_before(project.barLoads), m_after(std::move(after)), m_text(std::move(text)) {}
    bool valid() const { return m_before != m_after; }
    void redo() override { restore(m_after); }
    void undo() override { restore(m_before); }
    std::string text() const override { return m_text; }

private:
    void restore(const BarLoads& state) {
        const int next = std::max(m_project.barLoads.nextId, state.nextId);
        m_project.barLoads = state;
        m_project.barLoads.nextId = next;
    }
    Project& m_project;
    BarLoads m_before, m_after;
    std::string m_text;
};

enum class BarLoadError {
    None,
    NoBars,           ///< no given bar exists (nothing to load)
    UnknownLoad,      ///< no given load id exists
    UnknownLoadCase,  ///< the load case does not exist
    InvalidData,      ///< the dialog data fail checkBarLoadData (the issue carries the message)
    NothingToChange,
};
/// Literal es-ES text for the errors that have none of their own (InvalidData: use the BarLoadIssue message).
std::string barLoadErrorMessage(BarLoadError error);

/// Shortest length among the existing bars (0 when none): the length against which an absolute position is checked.
double shortestBarLength(const StructuralModel& model, const std::vector<int>& barIds);

/// "Nueva": one load per existing bar (duplicates dropped), all with the same canonical data, appended in the order of `barIds`; never merges with
/// loads already on the bar. Checks in order: data (against the shortest bar), load case, bars. `issue` receives the dialog message on
/// InvalidData; `ids` the new load ids. The data are remembered in `session` when given and the command succeeds.
BarLoadError addBarLoads(Project& project, UndoStack& stack, const std::vector<int>& barIds, const BarLoadData& data,
                         std::vector<int>* ids = nullptr, BarLoadIssue* issue = nullptr, BarLoadSession* session = nullptr);
/// "Editar": EVERY selected load takes the whole definition of the dialog (type, values, positions, axes, direction; measured: a point and a
/// uniform load edited together both became uniform). Unknown ids are skipped; none left -> UnknownLoad. Identical data -> NothingToChange.
BarLoadError editBarLoads(Project& project, UndoStack& stack, const std::vector<int>& loadIds, const BarLoadData& data,
                          BarLoadIssue* issue = nullptr, BarLoadSession* session = nullptr);
/// "Hipótesis": moves the selected loads to another case.
BarLoadError changeBarLoadCase(Project& project, UndoStack& stack, const std::vector<int>& loadIds, int loadCaseId);
/// "Borrar": immediate, no question (measured).
BarLoadError deleteBarLoads(Project& project, UndoStack& stack, const std::vector<int>& loadIds);
/// "Peso propio": RMB on the selected bars applies Activar / Desactivar without question or warning (the message #64002 was never seen).
/// Existing bars only; nothing changes -> NothingToChange.
BarLoadError setBarSelfWeight(Project& project, UndoStack& stack, const std::vector<int>& barIds, bool active);

}  // namespace cc::core
