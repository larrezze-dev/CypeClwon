#include "core/node_ligature_edit.h"

#include <memory>

namespace cc::core {

LigatureCommand::LigatureCommand(Project& project, Mode mode, std::vector<int> nodeIds, const LigatureDirections& directions)
    : m_project(project), m_mode(mode), m_before(project.ligatures), m_after(project.ligatures) {
    if (mode == Mode::Assign) applyLigatureAssign(m_after, nodeIds, directions);
    else applyLigatureRemove(m_after, nodeIds, directions);
}

LigatureCheck assignLigatures(Project& project, UndoStack& stack, const std::vector<int>& nodeIds, const LigatureDirections& directions) {
    const LigatureCheck check = checkLigatureAssign(project, nodeIds, directions);
    if (check.status != LigatureStatus::Ok) return check;
    auto cmd = std::make_unique<LigatureCommand>(project, LigatureCommand::Mode::Assign, ligatureSelection(project, nodeIds), directions);
    if (cmd->valid()) stack.push(std::move(cmd));
    return check;
}

bool removeLigatures(Project& project, UndoStack& stack, const std::vector<int>& nodeIds, const LigatureDirections& directions) {
    // Removal does not filter by capture: any existing node of the selection counts.
    std::vector<int> ids;
    for (int id : nodeIds)
        if (project.structure.findNode(id)) ids.push_back(id);
    if (ids.empty()) return false;
    auto cmd = std::make_unique<LigatureCommand>(project, LigatureCommand::Mode::Remove, std::move(ids), directions);
    if (!cmd->valid()) return false;
    stack.push(std::move(cmd));
    return true;
}

}  // namespace cc::core
