#pragma once
#include <string>

#include "core/project.h"
#include "core/undo_stack.h"

namespace cc::core {

/// Base of the undoable commands that edit several collections of the structural model at once (shell, tools and import
/// operations). The subclass constructor edits the copy `work` of project.structure and calls commit() when the edit is
/// valid; redo() / undo() then swap the whole model, so they are exact by construction. Ids are never reused: undo keeps the
/// id counters of the redone state. A command must be pushed before another one is constructed.
/// Descriptions of bars ("Describir") hang from Bar::sectionId / materialId, so they follow the bars without being copied.
class SnapshotCommand : public UndoCommand {
public:
    void redo() override;
    void undo() override;
    std::string text() const override { return m_text; }
    /// True once the subclass accepted the edit.
    bool committed() const { return m_committed; }

protected:
    SnapshotCommand(Project& project, std::string text) : work(project.structure), m_project(project), m_text(std::move(text)) {}
    /// Freezes `work` as the redone state.
    void commit();

    StructuralModel work;  ///< edited copy, valid until commit()

private:
    Project& m_project;
    std::string m_text;
    bool m_committed = false;
    StructuralModel m_before, m_after;
};

}  // namespace cc::core
