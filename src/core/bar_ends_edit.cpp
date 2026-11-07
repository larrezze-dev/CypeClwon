#include "core/bar_ends_edit.h"

#include <algorithm>
#include <memory>

namespace cc::core {

BarEndsCommand::BarEndsCommand(Project& project, Mode mode, std::vector<int> barIds, const EndFixityEdit& edit, const std::array<bool, 2>& ends)
    : m_project(project), m_before(project.barEnds), m_after(project.barEnds) {
    for (int id : barIds) {
        BarEndsRecord rec = barEndsOf(m_after, id);
        switch (mode) {
            case Mode::Fixity: applyEndFixityEdit(rec, edit); break;
            case Mode::Articulate:
                for (int e = 0; e < 2; ++e)
                    if (ends[static_cast<std::size_t>(e)]) rec.ends[static_cast<std::size_t>(e)].pinned = true;
                break;
            case Mode::RemoveArticulation:
                for (int e = 0; e < 2; ++e)
                    if (ends[static_cast<std::size_t>(e)]) rec.ends[static_cast<std::size_t>(e)].pinned = false;
                break;
        }
        setBarEnds(m_after, id, rec);
    }
    m_text = mode == Mode::Fixity ? "Empotramiento en extremos" : mode == Mode::Articulate ? "Articular extremos" : "Quitar articulaciones";
}

BarEndsCommand::BarEndsCommand(Project& project, BarEnds after, std::string text)
    : m_project(project), m_before(project.barEnds), m_after(std::move(after)), m_text(std::move(text)) {}

std::vector<int> existingBars(const Project& project, const std::vector<int>& barIds) {
    std::vector<int> out;
    for (int id : barIds)
        if (project.structure.findBar(id) && std::find(out.begin(), out.end(), id) == out.end()) out.push_back(id);
    return out;
}

namespace {

bool push(Project& project, UndoStack& stack, BarEndsCommand::Mode mode, const std::vector<int>& barIds, const EndFixityEdit& edit,
          const std::array<bool, 2>& ends) {
    std::vector<int> ids = existingBars(project, barIds);
    if (ids.empty()) return false;
    auto cmd = std::make_unique<BarEndsCommand>(project, mode, std::move(ids), edit, ends);
    if (!cmd->valid()) return false;
    stack.push(std::move(cmd));
    return true;
}

}  // namespace

std::optional<EndFixityIssue> assignEndFixity(Project& project, UndoStack& stack, const std::vector<int>& barIds, const EndFixityEdit& edit) {
    if (auto issue = checkEndFixityEdit(edit)) return issue;
    push(project, stack, BarEndsCommand::Mode::Fixity, barIds, edit, {true, true});
    return std::nullopt;
}

bool articulateBars(Project& project, UndoStack& stack, const std::vector<int>& barIds, const std::array<bool, 2>& ends) {
    return push(project, stack, BarEndsCommand::Mode::Articulate, barIds, {}, ends);
}

bool toggleBarArticulations(Project& project, UndoStack& stack, const std::vector<int>& barIds, ArticulationEnd end) {
    const std::vector<int> ids = existingBars(project, barIds);
    if (ids.empty()) return false;
    BarEnds after = project.barEnds;
    for (int id : ids) {
        BarEndsRecord rec = barEndsOf(after, id);
        toggleArticulation(rec, end);
        setBarEnds(after, id, rec);
    }
    auto cmd = std::make_unique<BarEndsCommand>(project, std::move(after), "Articular extremos");
    if (!cmd->valid()) return false;
    stack.push(std::move(cmd));
    return true;
}

bool removeArticulations(Project& project, UndoStack& stack, const std::vector<int>& barIds, const std::array<bool, 2>& ends) {
    return push(project, stack, BarEndsCommand::Mode::RemoveArticulation, barIds, {}, ends);
}

}  // namespace cc::core
