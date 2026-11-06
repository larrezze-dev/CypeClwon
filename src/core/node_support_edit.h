#pragma once
#include <map>
#include <memory>
#include <optional>
#include <vector>

#include "core/node_support.h"
#include "core/project.h"
#include "core/undo_stack.h"

namespace cc::core {

/// Undoable assignment / removal of node supports (T-P6-01). See node_support.h for the data and the validation.
///
/// ExternalSupportCommand / InternalLinkCommand set (or, with nullopt, remove) the support of every given node that exists. Nodes that
/// do not exist are skipped; the command is invalid when no node remains. Assigning type Free (external) or Fixed (internal) is the same as
/// removing: no entry is stored. The generic internal link is only applied to the nodes whose element count equals its table size
/// (#65473); the others keep their link and are listed in skippedNodes().
class ExternalSupportCommand : public UndoCommand {
public:
    ExternalSupportCommand(Project& project, std::vector<int> nodeIds, std::optional<ExternalSupport> support);
    bool valid() const { return !m_before.empty(); }
    void redo() override;
    void undo() override;
    std::string text() const override { return m_remove ? "Quitar vinculación exterior" : "Asignar vinculación exterior"; }

private:
    Project& m_project;
    bool m_remove;
    std::optional<ExternalSupport> m_support;
    std::map<int, std::optional<ExternalSupport>> m_before;  ///< node -> previous support (nullopt = none)
};

class InternalLinkCommand : public UndoCommand {
public:
    InternalLinkCommand(Project& project, std::vector<int> nodeIds, std::optional<InternalLink> link);
    bool valid() const { return !m_before.empty(); }
    const std::vector<int>& skippedNodes() const { return m_skipped; }
    void redo() override;
    void undo() override;
    std::string text() const override { return m_remove ? "Quitar vinculación interior" : "Asignar vinculación interior"; }

private:
    Project& m_project;
    bool m_remove;
    std::optional<InternalLink> m_link;
    std::map<int, std::optional<InternalLink>> m_before;
    std::vector<int> m_skipped;
};

/// Wrappers: build the command, push it and return the number of nodes changed (0 = nothing pushed). The assign functions also return 0 when
/// the data has validation errors (checkExternalSupport / checkInternalLink); warnings and questions never block.
int assignExternalSupport(Project& project, UndoStack& stack, const std::vector<int>& nodeIds, const ExternalSupport& support);
int removeExternalSupport(Project& project, UndoStack& stack, const std::vector<int>& nodeIds);
int assignInternalLink(Project& project, UndoStack& stack, const std::vector<int>& nodeIds, const InternalLink& link);
int removeInternalLink(Project& project, UndoStack& stack, const std::vector<int>& nodeIds);

}  // namespace cc::core
