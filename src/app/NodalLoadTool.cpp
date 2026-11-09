#include "NodalLoadTool.h"

#include <QAction>
#include <QActionGroup>
#include <QDialog>
#include <QSignalBlocker>
#include <algorithm>

#include "UserMessages.h"
#include "Viewport3D.h"
#include "core/node_support_edit.h"

namespace cc::app {

using namespace cc::core;

NodalLoadTool::NodalLoadTool(QObject* parent) : QObject(parent) {
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
    // Help texts of the original (#65318 / #64658 for the prescribed displacements, #64345 / #64649 / #64656 for the others).
    m_prescribed = make(tr("Desplazamientos prescritos"),
                        tr("Permite aplicar desplazamientos impuestos a un nudo o a una arista de lámina con coacciones exteriores."), Mode::Prescribed);
    m_new = make(tr("Nueva"), tr("Introducir cargas sobre nudos"), Mode::New);
    m_edit = make(tr("Editar"), tr("Editar cargas sobre nudos"), Mode::Edit);
    m_case = make(tr("Hipótesis"), tr("Cambiar la hipótesis de las cargas sobre nudos"), Mode::Case);
    m_delete = make(tr("Borrar"), tr("Borrar cargas sobre nudos"), Mode::Delete);
    m_view = new QAction(tr("Hipótesis vista"), this);
    // Tooltip of the original (measured): title and text.
    m_view->setStatusTip(tr("Permite seleccionar la hipótesis de carga activa. Las cargas sobre nudos y barras que se introduzcan, por defecto, se asignarán a dicha hipótesis."));
    m_view->setToolTip(QStringLiteral("<b>") + tr("Hipótesis vista") + QStringLiteral("</b><br>") + m_view->statusTip());
    m_scales = new QAction(tr("Escalas"), this);
    m_scales->setStatusTip(tr("Visualización - Escalas. Permite indicar la escala de dibujo para las cargas introducidas."));
    m_scales->setToolTip(m_scales->statusTip());
    m_view->setEnabled(false);
    m_scales->setEnabled(false);
    connect(m_view, &QAction::triggered, this, [this] { showViewPanel(); });
    connect(m_scales, &QAction::triggered, this, [this] { editScales(); });
}

QString NodalLoadTool::statusText(Mode mode) {
    const QString hints = tr(" - LMB: Seleccione un grupo de elementos. - RMB: Actuar sobre los elementos seleccionados.");
    switch (mode) {
        case Mode::Prescribed: return tr("Nudos - Desplazamientos prescritos |") + QLatin1Char(' ') + hints;
        case Mode::New: return tr("Nudos - Nueva |") + QLatin1Char(' ') + hints;
        case Mode::Edit: return tr("Nudos - Editar |") + QLatin1Char(' ') + hints;
        case Mode::Case: return tr("Nudos - Hipótesis |") + QLatin1Char(' ') + hints;
        case Mode::Delete: return tr("Nudos - Borrar |") + QLatin1Char(' ') + hints;
        case Mode::None: break;
    }
    return tr("Seleccione una opción del menú.");
}

void NodalLoadTool::pruneViewports() {
    m_viewports.removeIf([](const QPointer<Viewport3D>& v) { return v.isNull(); });
}

int NodalLoadTool::run(QDialog& dialog) { return m_runner ? m_runner(dialog) : dialog.exec(); }

void NodalLoadTool::warn(const QString& text) { messages::show(m_dialogParent, messages::Kind::Warning, text); }

void NodalLoadTool::setProject(Project* project, UndoStack* undo) {
    deactivate();
    m_project = project;
    m_undo = undo;
    m_viewCase = 0;
    m_viewAll = false;
    m_last = NodalLoadData{};
    pruneViewports();
    for (const QPointer<Viewport3D>& viewport : m_viewports) viewport->setNodalLoads(m_project ? &m_project->nodalLoads : nullptr);
    applyView();
    updateEnabled();
}

void NodalLoadTool::addViewport(Viewport3D* viewport) {
    if (!viewport || m_viewports.contains(viewport)) return;
    m_viewports.push_back(viewport);
    viewport->setNodalLoads(m_project ? &m_project->nodalLoads : nullptr);
    viewport->setLoadsVisible(m_loadsVisible);
    viewport->setLoadViewCase(m_viewAll ? std::nullopt : std::optional<int>(m_viewCase));
    connect(viewport, &Viewport3D::actOnSelection, this, [this, viewport] { onActOnSelection(viewport); });
    connect(viewport, &Viewport3D::escapePressed, this, [this] { deactivate(); });
    if (m_mode != Mode::None) configureViewports();
}

void NodalLoadTool::setLoadsVisible(bool on) {
    m_loadsVisible = on;
    pruneViewports();
    for (const QPointer<Viewport3D>& viewport : m_viewports) viewport->setLoadsVisible(on);
}

void NodalLoadTool::applyView() {
    pruneViewports();
    for (const QPointer<Viewport3D>& viewport : m_viewports)
        viewport->setLoadViewCase(m_viewAll ? std::nullopt : std::optional<int>(m_viewCase));
    if (m_panel) {
        QSignalBlocker block(m_panel.data());
        m_panel->setCases(m_project ? m_project->loadCases : LoadCaseSet{});
        m_panel->setCaseId(m_viewCase);
        m_panel->setViewAll(m_viewAll);
    }
}

void NodalLoadTool::setView(int caseId, bool all) {
    m_viewCase = caseId;
    m_viewAll = all;
    applyView();
    configureViewports();  // the nodes that can be captured depend on the loads in view
    emit viewChanged();
}

void NodalLoadTool::updateEnabled() {
    const bool open = m_project != nullptr;
    const bool nodes = open && !m_project->structure.nodes.empty();
    const bool loads = open && hasNodalLoads(m_project->nodalLoads);
    bool supported = false;
    if (open)
        for (const auto& [id, support] : m_project->nodeSupports.external)
            if (m_project->structure.findNode(id) && restraintOf(support).any()) {
                supported = true;
                break;
            }
    m_prescribed->setEnabled(supported);
    m_new->setEnabled(nodes);
    m_edit->setEnabled(loads);
    m_case->setEnabled(loads);
    m_delete->setEnabled(loads);
    m_view->setEnabled(open);
    m_scales->setEnabled(open);
    for (QAction* action : {m_prescribed, m_new, m_edit, m_case, m_delete})
        if (!action->isEnabled() && action->isChecked()) action->setChecked(false);
}

void NodalLoadTool::structureChanged() {
    if (m_project && !findLoadCase(m_project->loadCases, m_viewCase)) m_viewCase = 0;  // the shown case was deleted
    updateEnabled();
    pruneViewports();
    applyView();
    configureViewports();
    for (const QPointer<Viewport3D>& viewport : m_viewports) viewport->refreshStructure();
}

void NodalLoadTool::setMode(Mode mode) {
    if (mode == m_mode) return;
    if (mode != Mode::None) emit aboutToActivate();
    m_mode = mode;
    if (mode == Mode::None || mode == Mode::Prescribed) {
        closeViewPanel();
    } else {
        showViewPanel();
    }
    configureViewports();
    emit modeChanged(mode);
    emit statusChanged(statusText(mode));  // after the viewports: setSelectionMode announces its own, generic hint
}

void NodalLoadTool::closeViewPanel() {
    if (!m_panel) return;
    QPointer<LoadViewPanel> closing = m_panel;
    m_panel.clear();  // the window deletes itself when it closes
    closing->close();
}

void NodalLoadTool::deactivate() {
    if (QAction* checked = m_group->checkedAction()) checked->setChecked(false);
    setMode(Mode::None);
}

void NodalLoadTool::showViewPanel() {
    if (!m_project) return;
    if (m_panel && !m_panel->isVisible()) {  // closed, its deletion is still pending: never reuse it
        m_panel->deleteLater();
        m_panel.clear();
    }
    if (!m_panel) {
        m_panel = new LoadViewPanel(m_dialogParent);
        m_panel->setAttribute(Qt::WA_DeleteOnClose);
        connect(m_panel, &LoadViewPanel::viewChanged, this, [this](int caseId, bool all) { setView(caseId, all); });
        connect(m_panel, &LoadViewPanel::closed, this, [this] {
            emit viewPanelClosed();
            if (m_mode != Mode::None && m_mode != Mode::Prescribed) deactivate();  // closing the window ends the command (like the others)
        });
    }
    applyView();
    if (!m_panel->isVisible()) {
        if (m_dialogParent) {  // lower left of the work area, as in the original (cn_c1)
            const QWidget* parent = m_dialogParent;
            m_panel->move(parent->mapToGlobal(QPoint(410, std::max(40, parent->height() - 500))));
        }
        m_panel->show();
    }
    m_panel->raise();
}

void NodalLoadTool::configureViewports() {
    pruneViewports();
    const bool on = m_mode != Mode::None;
    if (!on && !m_configured) return;  // idle: the viewports belong to another command (Carga > Barras, ...)
    m_configured = on;
    const bool onLoads = m_mode == Mode::Edit || m_mode == Mode::Case || m_mode == Mode::Delete;
    const Project* project = m_project;
    const bool all = m_viewAll;
    const int caseId = m_viewCase;
    for (const QPointer<Viewport3D>& viewport : m_viewports) {
        viewport->setSelectionMode(false);
        viewport->setNodeLabelsVisible(false);
        viewport->setNodeExclusion({});
        viewport->setLoadSelectionHighlight(onLoads);
        if (!on) continue;
        if (onLoads) {
            // Nodes without a load in view cannot be captured (the loads are what is selected).
            viewport->setNodeExclusion([project, all, caseId](int id) {
                if (!project) return true;
                for (const NodalLoad& load : project->nodalLoads.items)
                    if (load.nodeId == id && (all || load.loadCaseId == caseId)) return false;
                return true;
            });
        } else if (m_mode == Mode::Prescribed) {
            viewport->setNodeExclusion([project](int id) {
                if (!project) return true;
                const ExternalSupport* support = externalSupportOf(project->structure, project->nodeSupports, id);
                return !support || !restraintOf(*support).any();
            });
        }
        viewport->setSelectionFilter(SelectionFilter::only(ElementKind::Node));
        viewport->setSelectionMode(true);
        viewport->setFocus();
    }
}

std::vector<int> NodalLoadTool::loadsOfNodes(const std::vector<int>& nodeIds) const {
    std::vector<int> out;
    if (!m_project) return out;
    std::vector<int> nodes = nodeIds;
    std::sort(nodes.begin(), nodes.end());
    nodes.erase(std::unique(nodes.begin(), nodes.end()), nodes.end());
    for (int node : nodes)
        for (int id : nodalLoadsOfNode(m_project->nodalLoads, node)) {
            const NodalLoad* load = findNodalLoad(m_project->nodalLoads, id);
            if (load && (m_viewAll || load->loadCaseId == m_viewCase)) out.push_back(id);
        }
    return out;
}

bool NodalLoadTool::createLoads(const std::vector<int>& nodeIds) {
    if (!m_project || !m_undo || nodeIds.empty()) return false;
    NodalLoadData initial = m_last;
    initial.loadCaseId = m_viewCase;  // by default the shown hypothesis (tooltip #64651)
    NodalLoadDialog dialog(tr("Introducir cargas sobre nudos"), m_project->loadCases, initial, m_dialogParent);
    if (run(dialog) != QDialog::Accepted) return false;
    const NodalLoadData data = dialog.data();
    m_last = data;
    m_last.loadCaseId = m_viewCase;
    const NodalLoadError error = addNodalLoads(*m_project, *m_undo, nodeIds, data);
    if (error != NodalLoadError::None) {
        warn(QString::fromStdString(nodalLoadErrorMessage(error)));
        return false;
    }
    return true;
}

bool NodalLoadTool::editLoads(const std::vector<int>& nodeIds) {
    const std::vector<int> ids = loadsOfNodes(nodeIds);
    if (!m_project || !m_undo || ids.empty()) return false;
    const NodalLoad* first = findNodalLoad(m_project->nodalLoads, ids.front());
    NodalLoadData initial;
    initial.loadCaseId = first->loadCaseId;
    initial.magnitude = first->magnitude;
    initial.direction = first->direction;
    initial.userVector = first->userVector;
    NodalLoadDialog dialog(tr("Descripción de la carga"), m_project->loadCases, initial, m_dialogParent);
    if (run(dialog) != QDialog::Accepted) return false;
    const NodalLoadError error = editNodalLoads(*m_project, *m_undo, ids, dialog.data());  // every selected load takes the data (measured)
    if (error == NodalLoadError::NothingToChange) return false;
    if (error != NodalLoadError::None) {
        warn(QString::fromStdString(nodalLoadErrorMessage(error)));
        return false;
    }
    return true;
}

bool NodalLoadTool::changeCase(const std::vector<int>& nodeIds) {
    const std::vector<int> ids = loadsOfNodes(nodeIds);
    if (!m_project || !m_undo || ids.empty()) return false;
    const NodalLoad* first = findNodalLoad(m_project->nodalLoads, ids.front());
    LoadCaseChoiceDialog dialog(m_project->loadCases, first->loadCaseId, m_dialogParent);
    if (run(dialog) != QDialog::Accepted) return false;
    const NodalLoadError error = changeNodalLoadCase(*m_project, *m_undo, ids, dialog.loadCaseId());
    if (error == NodalLoadError::NothingToChange) return false;
    if (error != NodalLoadError::None) {
        warn(QString::fromStdString(nodalLoadErrorMessage(error)));
        return false;
    }
    return true;
}

bool NodalLoadTool::deleteLoads(const std::vector<int>& nodeIds) {
    const std::vector<int> ids = loadsOfNodes(nodeIds);
    if (!m_project || !m_undo || ids.empty()) return false;
    return deleteNodalLoads(*m_project, *m_undo, ids) == NodalLoadError::None;  // immediate, no question (measured)
}

bool NodalLoadTool::editPrescribed(const std::vector<int>& nodeIds) {
    if (!m_project || !m_undo) return false;
    std::vector<int> nodes;
    for (int id : nodeIds) {
        const ExternalSupport* support = externalSupportOf(m_project->structure, m_project->nodeSupports, id);
        if (support && restraintOf(*support).any()) nodes.push_back(id);
    }
    std::sort(nodes.begin(), nodes.end());
    nodes.erase(std::unique(nodes.begin(), nodes.end()), nodes.end());
    if (nodes.empty()) return false;
    // Columns: the degrees of freedom blocked in the nodes (a restraint along a skew direction offers the three translations).
    std::array<bool, 6> columns{};
    const auto prescribable = [this](int id) {
        std::array<bool, 6> out{};
        const Restraint r = restraintOf(*externalSupportOf(m_project->structure, m_project->nodeSupports, id));
        out = r.fixed;
        if (r.oblique != Restraint::Oblique::None) out[0] = out[1] = out[2] = true;
        return out;
    };
    for (int id : nodes) {
        const std::array<bool, 6> mine = prescribable(id);
        for (std::size_t i = 0; i < 6; ++i) columns[i] = columns[i] || mine[i];
    }
    const ExternalSupport* firstSupport = externalSupportOf(m_project->structure, m_project->nodeSupports, nodes.front());
    PrescribedDisplacementsDialog dialog(m_project->loadCases, columns, effectivePrescribed(*firstSupport), m_dialogParent);
    if (run(dialog) != QDialog::Accepted) return false;
    // One undoable step for all the nodes; the values of the DOFs a node does not block are dropped (#65467).
    const std::size_t before = m_undo->count();
    m_undo->beginMacro("Desplazamientos prescritos");
    for (int id : nodes) {
        ExternalSupport support = *externalSupportOf(m_project->structure, m_project->nodeSupports, id);
        const std::array<bool, 6> mine = prescribable(id);
        support.prescribed.clear();
        for (PrescribedDisplacement row : dialog.rows()) {
            bool any = false;
            for (std::size_t i = 0; i < 6; ++i) {
                if (!mine[i]) row.values[i] = 0.0;
                any = any || row.values[i] != 0.0;
            }
            if (any) support.prescribed.push_back(row);
        }
        assignExternalSupport(*m_project, *m_undo, {id}, support);
    }
    m_undo->endMacro();
    return m_undo->count() != before;
}

bool NodalLoadTool::editScales() {
    if (!m_project || !m_undo) return false;
    LoadScalesDialog dialog(m_project->loadCases, m_project->nodalLoads.scales, m_dialogParent);
    if (run(dialog) != QDialog::Accepted) return false;
    const NodalLoadError error = setLoadScales(*m_project, *m_undo, dialog.scales());
    if (error == NodalLoadError::NothingToChange) return false;
    if (error != NodalLoadError::None) {
        warn(QString::fromStdString(nodalLoadErrorMessage(error)));
        return false;
    }
    return true;
}

void NodalLoadTool::onActOnSelection(Viewport3D* viewport) {
    if (m_mode == Mode::None || !m_project || !m_undo) return;
    const std::vector<int> nodes = viewport->selection().ids(ElementKind::Node);
    if (nodes.empty()) return;
    switch (m_mode) {
        case Mode::Prescribed: editPrescribed(nodes); break;
        case Mode::New: createLoads(nodes); break;
        case Mode::Edit: editLoads(nodes); break;
        case Mode::Case: changeCase(nodes); break;
        case Mode::Delete: deleteLoads(nodes); break;
        case Mode::None: break;
    }
    viewport->clearSelection();
    if (m_mode != Mode::None) emit statusChanged(statusText(m_mode));
}

}  // namespace cc::app
