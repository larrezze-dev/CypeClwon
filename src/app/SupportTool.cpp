#include "SupportTool.h"

#include <QAction>
#include <QActionGroup>
#include <QDialog>
#include <algorithm>
#include <cmath>

#include "SupportDialogs.h"
#include "Viewport3D.h"
#include "core/node_support_edit.h"

namespace cc::app {

using namespace cc::core;

SupportTool::SupportTool(QObject* parent) : QObject(parent) {
    m_group = new QActionGroup(this);
    m_group->setExclusionPolicy(QActionGroup::ExclusionPolicy::ExclusiveOptional);
    const auto make = [this](const QString& text, const QString& tip, Mode mode) {
        auto* action = new QAction(text, this);
        action->setCheckable(true);
        action->setEnabled(false);
        action->setStatusTip(tip);
        action->setToolTip(tip);
        m_group->addAction(action);
        connect(action, &QAction::toggled, this, [this, mode](bool on) {
            if (on) setMode(mode);
            else if (m_mode == mode) setMode(Mode::None);
        });
        return action;
    };
    // Help texts #64752 and #64751 of the original.
    m_external = make(tr("Vinculación exterior"), tr("Permite definir el tipo de vinculación exterior en los nudos de la estructura."), Mode::External);
    m_internal = make(tr("Vinculación interior"), tr("Permite definir la vinculación entre los extremos de las barras que acometen a un mismo nudo."), Mode::Internal);
}

QString SupportTool::statusText(Mode mode) {
    const QString hints = tr(" - LMB: Seleccione un grupo de elementos. - RMB: Actuar sobre los elementos seleccionados.");
    switch (mode) {
        case Mode::External: return tr("Nudos - Vinculación exterior |") + QLatin1Char(' ') + hints;
        case Mode::Internal: return tr("Nudos - Vinculación interior |") + QLatin1Char(' ') + hints;  // text inferred from the exterior one
        case Mode::None: break;
    }
    return tr("Seleccione una opción del menú.");
}

void SupportTool::pruneViewports() {
    m_viewports.removeIf([](const QPointer<Viewport3D>& v) { return v.isNull(); });
}

void SupportTool::setProject(Project* project, UndoStack* undo) {
    deactivate();
    m_project = project;
    m_undo = undo;
    updateEnabled();
}

void SupportTool::addViewport(Viewport3D* viewport) {
    if (!viewport || m_viewports.contains(viewport)) return;
    m_viewports.push_back(viewport);
    connect(viewport, &Viewport3D::actOnSelection, this, [this, viewport] { onActOnSelection(viewport); });
    connect(viewport, &Viewport3D::escapePressed, this, [this] { deactivate(); });
    if (m_mode != Mode::None) configureViewports();
}

void SupportTool::deactivate() {
    if (QAction* checked = m_group->checkedAction()) checked->setChecked(false);
    setMode(Mode::None);
}

void SupportTool::updateEnabled() {
    const bool nodes = m_project && !m_project->structure.nodes.empty();
    m_external->setEnabled(nodes);
    m_internal->setEnabled(nodes);
    for (QAction* action : {m_external, m_internal})
        if (!action->isEnabled() && action->isChecked()) action->setChecked(false);
}

void SupportTool::structureChanged() {
    updateEnabled();
    pruneViewports();
    for (const QPointer<Viewport3D>& viewport : m_viewports) viewport->refreshStructure();
}

void SupportTool::setMode(Mode mode) {
    if (mode == m_mode) return;
    if (mode != Mode::None) emit aboutToActivate();
    m_mode = mode;
    configureViewports();
    emit modeChanged(mode);
    emit statusChanged(statusText(mode));  // after the viewports: setSelectionMode announces its own, generic hint
}

void SupportTool::configureViewports() {
    pruneViewports();
    for (const QPointer<Viewport3D>& viewport : m_viewports) {
        viewport->setSelectionMode(false);
        viewport->setNodeLabelsVisible(false);
        if (m_mode != Mode::None) {
            viewport->setSelectionFilter(SelectionFilter::only(ElementKind::Node));
            viewport->setSelectionMode(true);
            viewport->setFocus();
        }
    }
}

int SupportTool::run(QDialog& dialog) { return m_runner ? m_runner(dialog) : dialog.exec(); }

void SupportTool::onActOnSelection(Viewport3D* viewport) {
    if (m_mode == Mode::None || !m_project || !m_undo) return;
    const std::vector<int> nodes = viewport->selection().ids(ElementKind::Node);
    if (nodes.empty()) return;
    if (m_mode == Mode::External) editExternal(nodes);
    else editInternal(nodes);
    viewport->clearSelection();
    emit statusChanged(statusText(m_mode));
}

bool SupportTool::editExternal(const std::vector<int>& nodeIds) {
    if (!m_project || !m_undo || nodeIds.empty()) return false;
    const int first = *std::min_element(nodeIds.begin(), nodeIds.end());
    const ExternalSupport* current = externalSupportOf(m_project->structure, m_project->nodeSupports, first);
    ExternalSupportDialog dialog(current ? *current : ExternalSupport{}, m_dialogParent);
    if (run(dialog) != QDialog::Accepted) return false;
    return assignExternalSupport(*m_project, *m_undo, nodeIds, dialog.support()) > 0;
}

bool SupportTool::editInternal(const std::vector<int>& nodeIds) {
    if (!m_project || !m_undo || nodeIds.empty()) return false;
    const StructuralModel& model = m_project->structure;
    std::vector<int> counts;
    int reference = 0, referenceCount = -1;
    std::vector<int> sorted = nodeIds;
    std::sort(sorted.begin(), sorted.end());
    for (int id : sorted) {
        if (!model.findNode(id)) continue;
        const int n = elementCountAtNode(model, id);
        counts.push_back(n);
        if (n > referenceCount) {
            referenceCount = n;
            reference = id;
        }
    }
    if (counts.empty()) return false;
    // Directions of the pieces that reach the reference node, bars first and then shells (the order of elementRow).
    std::vector<Point3> directions;
    const Node* node = model.findNode(reference);
    const auto push = [&](const Point3& to) {
        Point3 d{to.x - node->position.x, to.y - node->position.y, to.z - node->position.z};
        const double len = std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
        directions.push_back(len < 1e-12 ? Point3{1, 0, 0} : Point3{d.x / len, d.y / len, d.z / len});
    };
    for (int barId : model.barsUsingNode(reference)) {
        const Bar* bar = model.findBar(barId);
        const Node* other = model.findNode(bar->startNode == reference ? bar->endNode : bar->startNode);
        if (other) push(other->position);
    }
    for (int shellId : model.shellsUsingNode(reference)) {
        const Shell* shell = model.findShell(shellId);
        Point3 c{0, 0, 0};
        int n = 0;
        for (int id : shell->nodeIds)
            if (const Node* p = model.findNode(id)) {
                c = {c.x + p->position.x, c.y + p->position.y, c.z + p->position.z};
                ++n;
            }
        if (n > 0) push({c.x / n, c.y / n, c.z / n});
    }
    const bool mixed = !checkGenericInternalSelection(counts).empty();
    const InternalLink current = internalLinkOf(m_project->nodeSupports, reference);
    InternalLinkDialog dialog(current, directions, mixed, m_dialogParent);
    if (run(dialog) != QDialog::Accepted) return false;
    return assignInternalLink(*m_project, *m_undo, nodeIds, dialog.link()) > 0;
}

}  // namespace cc::app
