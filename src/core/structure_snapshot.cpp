#include "core/structure_snapshot.h"

#include <algorithm>

namespace cc::core {

void SnapshotCommand::commit() {
    m_before = m_project.structure;
    m_after = std::move(work);
    m_committed = true;
}

void SnapshotCommand::redo() {
    if (!m_committed) return;
    m_project.structure = m_after;
}

void SnapshotCommand::undo() {
    if (!m_committed) return;
    const StructuralModel current = std::move(m_project.structure);
    m_project.structure = m_before;
    StructuralModel& s = m_project.structure;
    s.nextNodeId = std::max(s.nextNodeId, current.nextNodeId);
    s.nextBarId = std::max(s.nextBarId, current.nextBarId);
    s.nextShellId = std::max(s.nextShellId, current.nextShellId);
    s.nextPieceId = std::max(s.nextPieceId, current.nextPieceId);
    s.nextMaterialId = std::max(s.nextMaterialId, current.nextMaterialId);
    s.nextSectionId = std::max(s.nextSectionId, current.nextSectionId);
    s.nextLayerId = std::max(s.nextLayerId, current.nextLayerId);
}

}  // namespace cc::core
