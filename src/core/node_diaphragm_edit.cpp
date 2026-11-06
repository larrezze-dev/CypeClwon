#include "core/node_diaphragm_edit.h"

#include <algorithm>
#include <memory>

namespace cc::core {

DiaphragmCommand::DiaphragmCommand(Project& project, Diaphragms after, std::string text)
    : m_project(project), m_before(project.diaphragms), m_after(std::move(after)), m_text(std::move(text)) {}

namespace {

bool push(Project& project, UndoStack& stack, Diaphragms after, const char* text) {
    auto cmd = std::make_unique<DiaphragmCommand>(project, std::move(after), text);
    if (!cmd->valid()) return false;
    stack.push(std::move(cmd));
    return true;
}

}  // namespace

std::optional<DiaphragmIssue> editDiaphragms(Project& project, UndoStack& stack, const std::vector<Diaphragm>& edited) {
    if (auto issue = checkDiaphragmAccept(edited)) return issue;
    Diaphragms after;
    after.nextId = project.diaphragms.nextId;
    for (const Diaphragm& row : edited) {
        Diaphragm d = row;
        if (const Diaphragm* old = d.id != 0 ? findDiaphragm(project.diaphragms, d.id) : nullptr) {
            d.nodeIds = old->nodeIds;
        } else {
            d.id = after.nextId++;
            d.nodeIds.clear();
        }
        after.items.push_back(std::move(d));
    }
    push(project, stack, std::move(after), "Editar diafragmas");
    return std::nullopt;
}

DiaphragmCheck assignNodesToDiaphragm(Project& project, UndoStack& stack, int diaphragmId, const std::vector<int>& nodeIds) {
    const DiaphragmCheck check = checkDiaphragmAssign(project, diaphragmId, nodeIds);
    if (check.status != DiaphragmStatus::Ok) return check;
    Diaphragms after = project.diaphragms;
    applyDiaphragmAssign(after, diaphragmId, diaphragmSelection(project, nodeIds));
    push(project, stack, std::move(after), "Asignar nudos a diafragma");
    return check;
}

bool removeNodesFromDiaphragm(Project& project, UndoStack& stack, const std::vector<int>& nodeIds) {
    const std::vector<int> ids = diaphragmSelection(project, nodeIds);
    if (ids.empty()) return false;
    Diaphragms after = project.diaphragms;
    applyDiaphragmUnassign(after, ids);
    return push(project, stack, std::move(after), "Eliminar asignación de diafragma");
}

DiaphragmGeneration generateDiaphragmsByLevels(Project& project, UndoStack& stack, bool confirmedReplace) {
    DiaphragmGeneration out;
    if (project.workPlanes.levels.empty()) {
        out.status = DiaphragmGenerationStatus::NoLevels;
        out.text = diaphragmNoLevelsMessage();
        return out;
    }
    if (!project.diaphragms.items.empty() && !confirmedReplace) {
        out.status = DiaphragmGenerationStatus::NeedsConfirmation;
        out.messageId = 375635;
        out.text = diaphragmMessage(375635);
        return out;
    }
    Diaphragms after;
    after.nextId = project.diaphragms.nextId;
    after.items = planDiaphragmsByLevels(project, after.nextId);
    after.nextId += static_cast<int>(after.items.size());
    out.status = DiaphragmGenerationStatus::Generated;
    out.count = static_cast<int>(after.items.size());
    out.messageId = out.count == 0 ? 375636 : out.count == 1 ? 375637 : 375638;
    out.text = diaphragmMessage(out.messageId, std::to_string(out.count));
    push(project, stack, std::move(after), "Generar diafragmas por niveles");
    return out;
}

}  // namespace cc::core
