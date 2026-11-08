#include "core/node_support_edit.h"

#include <algorithm>

namespace cc::core {

namespace {

std::vector<int> existingNodes(const Project& project, std::vector<int> ids) {
    std::sort(ids.begin(), ids.end());
    ids.erase(std::unique(ids.begin(), ids.end()), ids.end());
    ids.erase(std::remove_if(ids.begin(), ids.end(), [&](int id) { return !project.structure.findNode(id); }), ids.end());
    return ids;
}

template <class T>
void restore(std::map<int, T>& target, const std::map<int, std::optional<T>>& snapshot) {
    for (const auto& [id, value] : snapshot) {
        if (value) target[id] = *value;
        else target.erase(id);
    }
}

}  // namespace

// --- external ---------------------------------------------------------------------------------------------------

ExternalSupportCommand::ExternalSupportCommand(Project& project, std::vector<int> nodeIds, std::optional<ExternalSupport> support)
    // A generic restraint with every DOF free is "libre" (measured: the original closes silently and leaves the node free).
    : m_project(project),
      m_remove(!support || support->type == ExternalSupportType::Free || !restraintOf(*support).any()),
      m_support(std::move(support)) {
    if (m_remove) m_support.reset();
    for (int id : existingNodes(project, std::move(nodeIds))) {
        const auto it = project.nodeSupports.external.find(id);
        std::optional<ExternalSupport> previous;
        if (it != project.nodeSupports.external.end()) previous = it->second;
        m_before[id] = previous;
    }
}

void ExternalSupportCommand::redo() {
    for (const auto& entry : m_before) {
        if (m_support) m_project.nodeSupports.external[entry.first] = *m_support;
        else m_project.nodeSupports.external.erase(entry.first);
    }
}

void ExternalSupportCommand::undo() { restore(m_project.nodeSupports.external, m_before); }

// --- internal ---------------------------------------------------------------------------------------------------

InternalLinkCommand::InternalLinkCommand(Project& project, std::vector<int> nodeIds, std::optional<InternalLink> link)
    : m_project(project), m_remove(!link || link->type == InternalLinkType::Fixed), m_link(std::move(link)) {
    if (m_remove) m_link.reset();
    for (int id : existingNodes(project, std::move(nodeIds))) {
        if (m_link && m_link->type == InternalLinkType::Generic &&
            static_cast<int>(m_link->elementRow.size()) != elementCountAtNode(project.structure, id)) {
            m_skipped.push_back(id);
            continue;
        }
        const auto it = project.nodeSupports.internal.find(id);
        std::optional<InternalLink> previous;
        if (it != project.nodeSupports.internal.end()) previous = it->second;
        m_before[id] = previous;
    }
}

void InternalLinkCommand::redo() {
    for (const auto& entry : m_before) {
        if (m_link) m_project.nodeSupports.internal[entry.first] = *m_link;
        else m_project.nodeSupports.internal.erase(entry.first);
    }
}

void InternalLinkCommand::undo() { restore(m_project.nodeSupports.internal, m_before); }

// --- wrappers ---------------------------------------------------------------------------------------------------

namespace {

template <class Command>
int push(UndoStack& stack, std::unique_ptr<Command> cmd, std::size_t changed) {
    if (!cmd->valid()) return 0;
    stack.push(std::move(cmd));
    return static_cast<int>(changed);
}

}  // namespace

int assignExternalSupport(Project& project, UndoStack& stack, const std::vector<int>& nodeIds, const ExternalSupport& support) {
    if (hasError(checkExternalSupport(support, project.loadCases))) return 0;
    auto cmd = std::make_unique<ExternalSupportCommand>(project, nodeIds, support);
    const std::size_t n = existingNodes(project, nodeIds).size();
    return push(stack, std::move(cmd), n);
}

int removeExternalSupport(Project& project, UndoStack& stack, const std::vector<int>& nodeIds) {
    auto cmd = std::make_unique<ExternalSupportCommand>(project, nodeIds, std::nullopt);
    const std::size_t n = existingNodes(project, nodeIds).size();
    return push(stack, std::move(cmd), n);
}

int assignInternalLink(Project& project, UndoStack& stack, const std::vector<int>& nodeIds, const InternalLink& link) {
    if (hasError(checkInternalLink(link))) return 0;
    auto cmd = std::make_unique<InternalLinkCommand>(project, nodeIds, link);
    const std::size_t n = existingNodes(project, nodeIds).size() - cmd->skippedNodes().size();
    return push(stack, std::move(cmd), n);
}

int removeInternalLink(Project& project, UndoStack& stack, const std::vector<int>& nodeIds) {
    auto cmd = std::make_unique<InternalLinkCommand>(project, nodeIds, std::nullopt);
    const std::size_t n = existingNodes(project, nodeIds).size();
    return push(stack, std::move(cmd), n);
}

}  // namespace cc::core
