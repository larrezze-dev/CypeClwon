#include "core/structure_snapshot.h"

#include <algorithm>

namespace cc::core {

void SnapshotCommand::commit() {
    m_before = m_project.structure;
    m_after = std::move(work);
    m_endsBefore = m_project.barEnds;
    m_endsAfter = reconcileBarEnds(m_endsBefore, m_before, m_after);
    m_bucklingBefore = m_project.buckling;
    m_bucklingAfter = reconcileBuckling(m_bucklingBefore, m_before, m_after);
    m_deflectionBefore = m_project.deflection;
    m_deflectionAfter = reconcileDeflection(m_deflectionBefore, m_before, m_after);
    m_shellPropsBefore = m_project.shellProps;
    m_shellPropsAfter = reconcileShellProperties(m_shellPropsBefore, m_before, m_after);
    m_diaphragmsBefore = m_project.diaphragms;
    m_diaphragmsAfter = m_diaphragmsBefore;
    for (Diaphragm& d : m_diaphragmsAfter.items)
        d.nodeIds.erase(std::remove_if(d.nodeIds.begin(), d.nodeIds.end(), [&](int id) { return !m_after.findNode(id); }), d.nodeIds.end());
    m_nodalLoadsBefore = m_project.nodalLoads;
    m_nodalLoadsAfter = m_nodalLoadsBefore;
    for (const auto& [gone, keep] : nodeRemap) applyNodalLoadsNodesMerged(m_nodalLoadsAfter, gone, keep);
    m_nodalLoadsAfter.items.erase(std::remove_if(m_nodalLoadsAfter.items.begin(), m_nodalLoadsAfter.items.end(),
                                                 [&](const NodalLoad& l) { return !m_after.findNode(l.nodeId); }),
                                  m_nodalLoadsAfter.items.end());
    m_barLoadsBefore = m_project.barLoads;
    m_barLoadsAfter = reconcileBarLoads(m_barLoadsBefore, m_before, m_after, &m_barLoadInfo);
    m_shellLoadsBefore = m_project.shellLoads;
    m_shellLoadsAfter = reconcileShellLoads(m_shellLoadsBefore, m_before, m_after);
    m_panelsBefore = m_project.panels;
    m_surfaceLoadsBefore = m_project.surfaceLoads;
    m_panelsAfter = m_panelsBefore;
    m_surfaceLoadsAfter = m_surfaceLoadsBefore;
    reconcilePanels(m_panelsAfter, m_surfaceLoadsAfter, m_before, m_after, nodeRemap);
    m_committed = true;
}

void SnapshotCommand::redo() {
    if (!m_committed) return;
    m_project.structure = m_after;
    m_project.barEnds = m_endsAfter;
    m_project.buckling = m_bucklingAfter;
    m_project.deflection = m_deflectionAfter;
    m_project.shellProps = m_shellPropsAfter;
    const int next = std::max(m_project.diaphragms.nextId, m_diaphragmsAfter.nextId);
    m_project.diaphragms = m_diaphragmsAfter;
    m_project.diaphragms.nextId = next;
    const int nextLoad = std::max(m_project.nodalLoads.nextId, m_nodalLoadsAfter.nextId);
    m_project.nodalLoads = m_nodalLoadsAfter;
    m_project.nodalLoads.nextId = nextLoad;
    restoreBarLoads(m_project.barLoads, m_barLoadsAfter);
    restoreShellLoads(m_project.shellLoads, m_shellLoadsAfter);
    restorePanels(m_project.panels, m_panelsAfter);
    restoreSurfaceLoads(m_project.surfaceLoads, m_surfaceLoadsAfter);
}

void SnapshotCommand::undo() {
    if (!m_committed) return;
    const StructuralModel current = std::move(m_project.structure);
    m_project.structure = m_before;
    m_project.barEnds = m_endsBefore;
    m_project.buckling = m_bucklingBefore;
    m_project.deflection = m_deflectionBefore;
    m_project.shellProps = m_shellPropsBefore;
    const int nextDiaphragm = std::max(m_project.diaphragms.nextId, m_diaphragmsBefore.nextId);
    m_project.diaphragms = m_diaphragmsBefore;
    m_project.diaphragms.nextId = nextDiaphragm;
    const int nextLoad = std::max(m_project.nodalLoads.nextId, m_nodalLoadsBefore.nextId);
    m_project.nodalLoads = m_nodalLoadsBefore;
    m_project.nodalLoads.nextId = nextLoad;
    restoreBarLoads(m_project.barLoads, m_barLoadsBefore);
    restoreShellLoads(m_project.shellLoads, m_shellLoadsBefore);
    restorePanels(m_project.panels, m_panelsBefore);
    restoreSurfaceLoads(m_project.surfaceLoads, m_surfaceLoadsBefore);
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
