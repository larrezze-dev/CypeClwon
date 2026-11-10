#pragma once
#include <map>
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
    /// Bar loads deleted because a bar became shorter than their absolute position (#65192, the GUI shows the message); valid after commit.
    int barLoadsRemovedOutOfLength() const { return m_barLoadInfo.removedOutOfLength; }

protected:
    SnapshotCommand(Project& project, std::string text) : work(project.structure), m_project(project), m_text(std::move(text)) {}
    /// Freezes `work` as the redone state.
    void commit();

    StructuralModel work;  ///< edited copy, valid until commit()
    /// Nodes merged by the edit (gone -> survivor): their nodal loads move to the survivor (set before commit(); not measured).
    std::map<int, int> nodeRemap;

private:
    Project& m_project;
    std::string m_text;
    bool m_committed = false;
    StructuralModel m_before, m_after;
    // Data hanging from nodes / bars that the edit may orphan or divide: end data (divided bars hand their end to the last stretch) and
    // diaphragms (vanished nodes leave them).
    BarEnds m_endsBefore, m_endsAfter;
    BucklingData m_bucklingBefore, m_bucklingAfter;
    DeflectionData m_deflectionBefore, m_deflectionAfter;
    ShellProperties m_shellPropsBefore, m_shellPropsAfter;
    Diaphragms m_diaphragmsBefore, m_diaphragmsAfter;
    BarLoads m_barLoadsBefore, m_barLoadsAfter;  ///< bar loads reconciled with the new bars (divided bars split their loads, vanished bars lose them)
    BarLoadReconcileInfo m_barLoadInfo;
    ShellLoads m_shellLoadsBefore, m_shellLoadsAfter;  ///< shell loads: vanished shells lose them, the pieces of a divided shell get a copy (not measured)
    NodalLoads m_nodalLoadsBefore, m_nodalLoadsAfter;  ///< loads of vanished nodes go with them
    Panels m_panelsBefore, m_panelsAfter;  ///< panels follow the nodes they sit on (merged / moved); a vanished node takes its panels (not measured)
    SurfaceLoads m_surfaceLoadsBefore, m_surfaceLoadsAfter;  ///< surface loads: same rule; bars switched off for them follow divided bars
};

}  // namespace cc::core
