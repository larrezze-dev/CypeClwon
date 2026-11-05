#include "GenerateTool.h"

#include <QAction>
#include <QMenu>

#include "GenerateDialogs.h"
#include "Viewport3D.h"

namespace cc::app {

using namespace cc::core;

GenerateTool::GenerateTool(QObject* parent) : QObject(parent) {
    // "Generar" (#392581): the drop-down with the three entries measured in the original.
    m_menu = new QMenu(tr("Generar"));
    connect(this, &QObject::destroyed, m_menu, &QObject::deleteLater);
    const auto make = [this](const QString& text, const QString& tip, Mode mode) {
        QAction* action = m_menu->addAction(text);
        action->setStatusTip(tip);
        action->setToolTip(tip);
        connect(action, &QAction::triggered, this, [this, mode] { start(mode); });
        return action;
    };
    m_planes = make(tr("Planos"), tr("Generación de planos"), Mode::Planes);
    m_nodeMesh = make(tr("Nudos y barras"), tr("Generación de nudos y barras"), Mode::NodeMesh);
    m_tetrahedra = make(tr("Malla de tetraedros"), tr("Generación de una malla de tetraedros"), Mode::Tetrahedra);
    updateEnabled();
}

QString GenerateTool::statusText(Mode mode, int picks) {
    switch (mode) {
        case Mode::None: return tr("Seleccione una opción del menú.");
        case Mode::Planes:
            // Measured (herramientas.md 6.2).
            return tr("Generar - Planos | - LMB: Seleccione dos líneas de referencia ortogonales para definir el plano sobre el que desea realizar la generación. - RMB: Comenzar otra vez.");
        case Mode::NodeMesh:
            return picks >= 2 ? tr("Generar - Nudos y barras | - LMB: Seleccione un elemento. - RMB: Actuar sobre los elementos seleccionados.")
                              : tr("Generar - Nudos y barras | - LMB: Seleccione un elemento.");
        case Mode::Tetrahedra:
            return picks >= 3 ? tr("Generar - Malla de tetraedros | - LMB: Seleccione un elemento. - RMB: Actuar sobre los elementos seleccionados.")
                              : tr("Generar - Malla de tetraedros | - LMB: Seleccione un elemento.");
    }
    return {};
}

void GenerateTool::pruneViewports() {
    m_viewports.removeIf([](const QPointer<Viewport3D>& v) { return v.isNull(); });
}

int GenerateTool::run(QDialog& dialog) { return m_runner ? m_runner(dialog) : dialog.exec(); }

void GenerateTool::setProject(Project* project, UndoStack* undo) {
    deactivate();
    m_project = project;
    m_undo = undo;
    updateEnabled();
}

void GenerateTool::addViewport(Viewport3D* viewport) {
    if (!viewport || m_viewports.contains(viewport)) return;
    m_viewports.push_back(viewport);
    connect(viewport, &Viewport3D::selectionChanged, this, [this, viewport] { onSelectionChanged(viewport); });
    connect(viewport, &Viewport3D::actOnSelection, this, [this] { onActOnSelection(); });
    connect(viewport, &Viewport3D::escapePressed, this, [this] { deactivate(); });
    if (m_mode != Mode::None) configureViewports();
}

void GenerateTool::updateEnabled() {
    // The original greys the group out on an empty work: there is nothing to pick.
    const bool elements = m_project && !m_project->structure.nodes.empty();
    for (QAction* action : {m_planes, m_nodeMesh, m_tetrahedra}) action->setEnabled(elements);
    m_menu->setEnabled(elements);
    m_menu->menuAction()->setEnabled(elements);
    if (!elements && m_mode != Mode::None) deactivate();
}

void GenerateTool::structureChanged() {
    updateEnabled();
    pruneViewports();
    for (const QPointer<Viewport3D>& viewport : m_viewports) viewport->refreshStructure();
    if (!m_project || m_mode == Mode::None) return;
    // A picked element that no longer exists (undo...) breaks the picks.
    for (int id : m_picks) {
        const bool gone = m_mode == Mode::Planes ? !m_project->structure.findBar(id) : !m_project->structure.findNode(id);
        if (gone) {
            resetPicks();
            return;
        }
    }
}

void GenerateTool::start(Mode mode) {
    if (!m_project) return;
    deactivate();
    emit aboutToActivate();
    m_mode = mode;
    m_picks.clear();
    configureViewports();
    emit modeChanged(mode);
    emit statusChanged(statusText(mode));
}

void GenerateTool::deactivate() {
    if (m_mode == Mode::None) return;
    m_mode = Mode::None;
    m_picks.clear();
    configureViewports();
    emit modeChanged(Mode::None);
    emit statusChanged(statusText(Mode::None));
}

void GenerateTool::configureViewports() {
    pruneViewports();
    for (const QPointer<Viewport3D>& viewport : m_viewports) {
        viewport->setSelectionMode(false);
        viewport->setSnapMode(false);
        viewport->setRubberChain({});
        std::vector<Vec3> rubber;
        if (m_project && m_mode != Mode::Planes)
            for (int id : m_picks)
                if (const Node* n = m_project->structure.findNode(id)) rubber.push_back({n->position.x, n->position.y, n->position.z});
        viewport->setRubberPoints(rubber);
        viewport->setMovingNode(std::nullopt);
        viewport->setNodeLabelsVisible(m_mode != Mode::None);
        viewport->setCoordinateTooltipEnabled(true);
        if (m_mode == Mode::None) continue;
        viewport->setSelectionFilter(SelectionFilter::only(m_mode == Mode::Planes ? ElementKind::Bar : ElementKind::Node));
        viewport->setSelectionMode(true);
        viewport->setFocus();
    }
}

void GenerateTool::resetPicks() {
    m_picks.clear();
    configureViewports();
    if (m_mode != Mode::None) emit statusChanged(statusText(m_mode));
}

bool GenerateTool::report(GenerateError error) {
    if (error == GenerateError::None) return true;
    emit messageRequested(QString::fromStdString(generateErrorMessage(error)));
    return false;
}

// ---- picking ---------------------------------------------------------------------------------------------------------------------

void GenerateTool::onSelectionChanged(Viewport3D* viewport) {
    if (m_internalSelection || m_mode == Mode::None) return;
    const ElementKind kind = m_mode == Mode::Planes ? ElementKind::Bar : ElementKind::Node;
    const std::vector<int> ids = viewport->selection().ids(kind);
    if (ids.empty()) return;
    m_internalSelection = true;  // a pick is consumed at once so that the same element can be clicked again
    viewport->clearSelection();
    m_internalSelection = false;
    if (ids.size() == 1) onPick(ids.front());
}

void GenerateTool::onPick(int id) {
    if (!m_project || !m_undo) return;
    const StructuralModel& model = m_project->structure;
    if (m_mode == Mode::Planes) {
        if (!model.findBar(id)) return;
        if (!m_picks.empty() && m_picks.front() == id) return;  // the second line must be another bar
        m_picks.push_back(id);
        if (m_picks.size() == 2) {
            runPlanes();
            resetPicks();  // the command stays active for another plane
        }
        return;
    }
    if (!model.findNode(id)) return;
    if (!m_picks.empty() && m_picks.back() == id) return;
    if (m_mode == Mode::NodeMesh && m_picks.size() >= 2 && m_picks.size() < 4 && !segmentIsIndependent(model, m_picks, id)) return;
    if (m_mode == Mode::Tetrahedra && m_picks.size() == 2 && !segmentIsIndependent(model, m_picks, id)) return;
    const std::size_t limit = m_mode == Mode::NodeMesh ? 4 : 3;
    if (m_picks.size() >= limit) return;
    m_picks.push_back(id);
    configureViewports();
    emit statusChanged(statusText(m_mode, static_cast<int>(m_picks.size())));
}

void GenerateTool::onActOnSelection() {
    if (m_mode == Mode::None) return;
    if (m_mode == Mode::NodeMesh && m_picks.size() >= 2) {
        runNodeMesh();
    } else if (m_mode == Mode::Tetrahedra && m_picks.size() >= 3) {
        runTetrahedra();
    }
    resetPicks();  // RMB: the dialog (when the picks allow it) and then start again
}

// ---- operations -------------------------------------------------------------------------------------------------------------------

bool GenerateTool::runPlanes() {
    if (!m_project || !m_undo || m_picks.size() < 2) return false;
    GenerateError error = GenerateError::None;
    if (!planeOfBars(m_project->structure, m_picks[0], m_picks[1], &error)) return report(error);
    PlanesDialog dialog(m_dialogParent);
    if (run(dialog) != QDialog::Accepted) return false;
    PlaneSpec spec;
    spec.barA = m_picks[0];
    spec.barB = m_picks[1];
    spec.count = dialog.count();
    spec.separationM = dialog.separationM();
    spec.group = dialog.group();
    if (spec.group && generationUngroups(*m_project, spec) && m_confirmer && !m_confirmer(QString::fromUtf8(kUngroupMessage))) return false;
    return report(generatePlanes(*m_project, *m_undo, spec));
}

bool GenerateTool::runNodeMesh() {
    if (!m_project || !m_undo || m_picks.size() < 2) return false;
    const int segments = static_cast<int>(m_picks.size()) - 1;
    NodeMeshDialog dialog(segments, m_dialogParent);
    if (run(dialog) != QDialog::Accepted) return false;
    NodeMeshSpec spec;
    spec.nodeIds = m_picks;
    dialog.fill(spec);
    return report(generateNodeMesh(*m_project, *m_undo, spec));
}

bool GenerateTool::runTetrahedra() {
    if (!m_project || !m_undo || m_picks.size() < 3) return false;
    TetrahedraDialog dialog(m_dialogParent);
    if (run(dialog) != QDialog::Accepted) return false;
    TetrahedraSpec spec;
    spec.nodeIds = {m_picks[0], m_picks[1], m_picks[2]};
    dialog.fill(spec);
    return report(generateTetrahedra(*m_project, *m_undo, spec));
}

}  // namespace cc::app
