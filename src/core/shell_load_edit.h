#pragma once
#include <algorithm>
#include <string>
#include <vector>

#include "core/project.h"
#include "core/shell_load.h"
#include "core/undo_stack.h"

namespace cc::core {

/// Undoable edits of the shell loads (T-P7-04). See shell_load.h for the data and the checks. The command keeps a snapshot of the whole ShellLoads
/// before and after (small), so undo / redo are exact; ids are never reused (nextId only grows).
class ShellLoadCommand : public UndoCommand {
public:
    ShellLoadCommand(Project& project, ShellLoads after, std::string text)
        : m_project(project), m_before(project.shellLoads), m_after(std::move(after)), m_text(std::move(text)) {}
    bool valid() const { return m_before != m_after; }
    void redo() override { restoreShellLoads(m_project.shellLoads, m_after); }
    void undo() override { restoreShellLoads(m_project.shellLoads, m_before); }
    std::string text() const override { return m_text; }

private:
    Project& m_project;
    ShellLoads m_before, m_after;
    std::string m_text;
};

enum class ShellLoadError {
    None,
    NoShells,         ///< no given shell exists (nothing to load)
    UnknownLoad,      ///< no given load id exists
    UnknownLoadCase,  ///< the load case does not exist
    InvalidData,      ///< the dialog data fail checkShellLoadData (the issue carries the message)
    NothingToChange,
};
/// Literal es-ES text for the errors that have none of their own (InvalidData: use the ShellLoadIssue message).
std::string shellLoadErrorMessage(ShellLoadError error);

/// "Nueva": one load per existing shell (duplicates dropped), all with the same canonical data, appended in the order of `shellIds`; never merges
/// with loads already on the shell. Checks in order: data, load case, shells. `issue` receives the dialog message on InvalidData; `ids` the new load
/// ids. The data are remembered in `session` when given and the command succeeds.
ShellLoadError addShellLoads(Project& project, UndoStack& stack, const std::vector<int>& shellIds, const ShellLoadData& data,
                             std::vector<int>* ids = nullptr, ShellLoadIssue* issue = nullptr, ShellLoadSession* session = nullptr);
/// "Editar": EVERY selected load takes the whole definition of the dialog (type, values, axes, sense AND hipótesis). Unknown ids are skipped; none
/// left -> UnknownLoad. Identical data -> NothingToChange.
ShellLoadError editShellLoads(Project& project, UndoStack& stack, const std::vector<int>& loadIds, const ShellLoadData& data,
                              ShellLoadIssue* issue = nullptr, ShellLoadSession* session = nullptr);
/// "Hipótesis": moves the selected loads to another case.
ShellLoadError changeShellLoadCase(Project& project, UndoStack& stack, const std::vector<int>& loadIds, int loadCaseId);
/// "Borrar": immediate, no question (measured).
ShellLoadError deleteShellLoads(Project& project, UndoStack& stack, const std::vector<int>& loadIds);
/// "Peso propio": RMB on the selected shells applies Activar / Desactivar without question or warning (measured: no message). Existing shells only;
/// nothing changes -> NothingToChange.
ShellLoadError setShellSelfWeight(Project& project, UndoStack& stack, const std::vector<int>& shellIds, bool active);

}  // namespace cc::core
