#pragma once
#include <algorithm>
#include <string>
#include <vector>

#include "core/nodal_load.h"
#include "core/project.h"
#include "core/undo_stack.h"

namespace cc::core {

/// Undoable edits of the nodal loads (T-P7-02). See nodal_load.h for the data and the checks. The command keeps a snapshot of the whole
/// NodalLoads before and after (small), so undo / redo are exact; ids are never reused (nextId only grows).
class NodalLoadCommand : public UndoCommand {
public:
    NodalLoadCommand(Project& project, NodalLoads after, std::string text)
        : m_project(project), m_before(project.nodalLoads), m_after(std::move(after)), m_text(std::move(text)) {}
    bool valid() const { return m_before != m_after; }
    void redo() override { restore(m_after); }
    void undo() override { restore(m_before); }
    std::string text() const override { return m_text; }

private:
    void restore(const NodalLoads& state) {
        const int next = std::max(m_project.nodalLoads.nextId, state.nextId);
        m_project.nodalLoads = state;
        m_project.nodalLoads.nextId = next;
    }
    Project& m_project;
    NodalLoads m_before, m_after;
    std::string m_text;
};

enum class NodalLoadError {
    None,
    NoNodes,         ///< no given node exists (nothing to load)
    UnknownLoad,     ///< no given load id exists
    UnknownLoadCase, ///< the load case does not exist
    InvalidValue,    ///< value outside 0.01..49000.00 (nodalLoadRangeMessage)
    InvalidDirection,///< null / not finite user vector (nodalLoadDirectionMessage)
    InvalidScale,    ///< scale not finite or not positive
    NothingToChange,
};
/// Literal text for InvalidValue / InvalidDirection (the measured messages); a short es-ES text for the others.
std::string nodalLoadErrorMessage(NodalLoadError error);

/// "Nueva": one load per existing node (duplicates dropped), all with the same data, appended in the order of `nodeIds`. Checks in order:
/// value, direction, load case, nodes. Never merges with loads already on the node. `ids` receives the new load ids.
NodalLoadError addNodalLoads(Project& project, UndoStack& stack, const std::vector<int>& nodeIds, const NodalLoadData& data,
                             std::vector<int>* ids = nullptr);
/// "Editar": EVERY selected load takes the data of the dialog (measured: multiple edit overwrites all, hypothesis included). Unknown ids are
/// skipped; none left -> UnknownLoad. Identical data -> NothingToChange.
NodalLoadError editNodalLoads(Project& project, UndoStack& stack, const std::vector<int>& loadIds, const NodalLoadData& data);
/// "Hipótesis": moves the selected loads to another case.
NodalLoadError changeNodalLoadCase(Project& project, UndoStack& stack, const std::vector<int>& loadIds, int loadCaseId);
/// "Borrar": immediate, no question (measured).
NodalLoadError deleteNodalLoads(Project& project, UndoStack& stack, const std::vector<int>& loadIds);
/// Carga > Visualización > Escalas (Aceptar).
NodalLoadError setLoadScales(Project& project, UndoStack& stack, const LoadScales& scales);

}  // namespace cc::core
