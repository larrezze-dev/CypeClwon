#include "NodeTool.h"

#include <QAction>
#include <QActionGroup>
#include <algorithm>

#include "NodeCalculatorDialog.h"
#include "Viewport3D.h"
#include "core/node_edit.h"

namespace cc::app {

NodeTool::NodeTool(QObject* parent) : QObject(parent) {
    m_group = new QActionGroup(this);
    m_group->setExclusionPolicy(QActionGroup::ExclusionPolicy::ExclusiveOptional);
    const auto make = [this](const QString& text, const QString& tip, Mode mode) {
        auto* action = new QAction(text, this);
        action->setCheckable(true);
        action->setEnabled(false);
        action->setStatusTip(tip);
        action->setToolTip(text);
        m_group->addAction(action);
        // Turning an action on starts its command; turning it off (Esc, another button, a second click) ends it. When another
        // button is pressed the group unchecks the old one after the new one is on: that late "off" is ignored.
        connect(action, &QAction::toggled, this, [this, mode](bool on) {
            if (on) setMode(mode);
            else if (m_mode == mode) setMode(Mode::None);
        });
        return action;
    };
    m_new = make(tr("Nuevo"), tr("Introducir un nuevo nudo en la estructura."), Mode::New);
    m_delete = make(tr("Borrar"), tr("Permite borrar un nudo individualmente o una selección de nudos."), Mode::Delete);
    m_move = make(tr("Mover"), tr("Cambiar de posición un nudo, independientemente de si está acotado o no."), Mode::Move);
}

QString NodeTool::defaultStatusText() { return tr("Seleccione una opción del menú."); }

QString NodeTool::statusText(Mode mode, bool moveSecondStep) {
    // Separators exactly as the original shows them (n7_borrar.png, orig_nudos_nuevo_01_main.png): "<command> |  - <hints>".
    switch (mode) {
        case Mode::None: return defaultStatusText();
        case Mode::New: return tr("Nudos - Nuevo |  - LMB: Introduzca un punto.");
        case Mode::Delete:
            return tr("Nudos - Borrar |  - LMB: Seleccione un grupo de elementos. - RMB: Actuar sobre los elementos seleccionados.");
        case Mode::Move:  // measured in the original (nudos.md section 9.2)
            return moveSecondStep
                       ? tr("Nudos - Mover |  - LMB: Seleccione la nueva posición del elemento. - RMB: Volver al paso anterior.")
                       : tr("Nudos - Mover |  - LMB: Seleccione un elemento.");
    }
    return {};
}

void NodeTool::pruneViewports() {
    m_viewports.removeIf([](const QPointer<Viewport3D>& v) { return v.isNull(); });
}

void NodeTool::setProject(cc::core::Project* project, cc::core::UndoStack* undo) {
    deactivate();
    m_project = project;
    m_undo = undo;
    updateEnabled();
}

void NodeTool::addViewport(Viewport3D* viewport) {
    if (!viewport || m_viewports.contains(viewport)) return;
    m_viewports.push_back(viewport);
    connect(viewport, &Viewport3D::pointEntered, this, &NodeTool::onPointEntered);
    connect(viewport, &Viewport3D::selectionChanged, this, [this, viewport] { onSelectionChanged(viewport); });
    connect(viewport, &Viewport3D::actOnSelection, this, [this, viewport] { onActOnSelection(viewport); });
    connect(viewport, &Viewport3D::digitTyped, this, [this, viewport](QChar digit) { onDigitTyped(viewport, digit); });
    connect(viewport, &Viewport3D::rightClicked, this, [this] {
        if (movePending()) endMove();  // RMB: "Volver al paso anterior" (the command goes on in its first step, nothing changes)
    });
    connect(viewport, &Viewport3D::escapePressed, this, [this] {
        deactivate();
        for (const QPointer<Viewport3D>& v : m_viewports) v->hideCoordinateTooltip();  // measured: Esc hides the tooltip next to the cursor
    });
    if (m_mode != Mode::None) configureViewports();
}

NodeCalculatorDialog* NodeTool::calculator() const { return m_calculator.data(); }

void NodeTool::deactivate() {
    if (QAction* checked = m_group->checkedAction()) checked->setChecked(false);  // toggled(false) ends the command
    setMode(Mode::None);                                                            // also when the actions were never pressed
}

void NodeTool::updateEnabled() {
    const bool project = m_project != nullptr;
    const bool nodes = project && !m_project->structure.nodes.empty();
    m_new->setEnabled(project);
    m_delete->setEnabled(nodes);
    m_move->setEnabled(nodes);
    // A pressed button that just became disabled (the last node was undone) ends its command.
    for (QAction* action : {m_delete, m_move})
        if (!action->isEnabled() && action->isChecked()) action->setChecked(false);
    if (!project && m_new->isChecked()) m_new->setChecked(false);
}

void NodeTool::structureChanged() {
    updateEnabled();
    pruneViewports();
    for (const QPointer<Viewport3D>& viewport : m_viewports) viewport->refreshStructure();
    if (movePending() && !(m_project && m_project->structure.findNode(*m_moveNode))) endMove();  // the node in motion was removed
}

void NodeTool::setMode(Mode mode) {
    if (mode == m_mode) return;
    closeCalculator();
    m_moveNode.reset();
    if (mode != Mode::None) emit aboutToActivate();
    m_mode = mode;
    configureViewports();
    emit modeChanged(mode);
    emit statusChanged(statusText(mode));  // after the viewports: setSelectionMode announces its own, generic hint
}

void NodeTool::configureViewports() {
    pruneViewports();
    const bool snapping = m_mode == Mode::New || movePending();
    const bool selecting = m_mode == Mode::Delete || (m_mode == Mode::Move && !movePending());
    for (const QPointer<Viewport3D>& viewport : m_viewports) {
        viewport->setSelectionMode(false);
        viewport->setSnapMode(false);
        viewport->setMovingNode(movePending() ? m_moveNode : std::nullopt);
        // The labels "N<id>" are shown by Nuevo (n5_on_bar.png) but neither by Borrar (c3_borrar_hover.png, c4_borrar_sel.png) nor by
        // Mover, in any of its steps (nudos.md section 9.2). Borrar shows no tooltip with the coordinates next to the cursor either.
        viewport->setNodeLabelsVisible(m_mode == Mode::New);
        viewport->setCoordinateTooltipEnabled(m_mode != Mode::Delete);
        if (snapping) viewport->setSnapMode(true);
        if (selecting) {
            viewport->setSelectionFilter(cc::core::SelectionFilter::only(cc::core::ElementKind::Node));
            viewport->setSelectionMode(true);
        }
        if (m_mode != Mode::None) viewport->setFocus();
    }
}

void NodeTool::onSelectionChanged(Viewport3D* viewport) {
    if (m_mode != Mode::Move || movePending()) return;
    const std::vector<int> nodes = viewport->selection().ids(cc::core::ElementKind::Node);
    if (!nodes.empty()) beginMove(nodes.front());
}

void NodeTool::beginMove(int nodeId) {
    m_moveNode = nodeId;
    configureViewports();
    emit statusChanged(statusText(Mode::Move, true));
}

void NodeTool::endMove() {
    m_moveNode.reset();
    configureViewports();
    emit statusChanged(statusText(Mode::Move, false));
}

void NodeTool::onActOnSelection(Viewport3D* viewport) {
    if (m_mode != Mode::Delete || !m_project || !m_undo) return;
    const std::vector<int> nodes = viewport->selection().ids(cc::core::ElementKind::Node);
    if (nodes.empty()) return;
    cc::core::deleteNodes(*m_project, *m_undo, nodes);  // no confirmation (measured); the history callback refreshes the windows
    viewport->clearSelection();
}

void NodeTool::onPointEntered(const cc::core::Vec3& world, const cc::core::SnapResult& snap) {
    if (m_mode != Mode::New && !movePending()) return;
    // The original needs a reference line or a bar under the pointer in a 3D window (text #64743): without a snap the click does nothing.
    if (!snap.snapped()) return;
    place({world.x, world.y, world.z});
}

void NodeTool::place(const cc::core::Point3& position) {
    if (!m_project || !m_undo) return;
    if (m_mode == Mode::New) {
        cc::core::addNodeIfFree(*m_project, *m_undo, position);  // a point that already has a node: nothing happens
        return;
    }
    if (!movePending()) return;
    const int id = *m_moveNode;
    const auto occupied = cc::core::findNodeAt(m_project->structure, position);
    if (occupied && *occupied == id) {  // dropped where it was
        endMove();
        return;
    }
    // Another node there: no message, the nodes merge (MoveNodeCommand). After fixing the node the command stays in its first step.
    if (cc::core::moveNode(*m_project, *m_undo, id, position)) endMove();  // refused (a bar would degenerate): the move goes on
}

void NodeTool::onDigitTyped(Viewport3D* viewport, QChar digit) {
    if (m_mode == Mode::New || movePending()) openCalculator(viewport, digit);
}

void NodeTool::openCalculator(Viewport3D* viewport, QChar first) {
    if (m_calculator) return;
    auto* dialog = new NodeCalculatorDialog(viewport);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    m_calculator = dialog;
    connect(dialog, &NodeCalculatorDialog::pointConfirmed, this, [this](double x, double y, double z) { place({x, y, z}); });
    connect(dialog, &QDialog::finished, this, [guard = QPointer<Viewport3D>(viewport)] {
        if (guard) guard->setFocus();
    });
    dialog->start(first);
    if (movePending() && m_project)  // measured: in Mover Y and Z start with the current coordinates of the chosen node
        if (const cc::core::Node* node = m_project->structure.findNode(*m_moveNode)) dialog->presetYZ(node->position.y, node->position.z);
    dialog->open();
}

void NodeTool::closeCalculator() {
    if (m_calculator) m_calculator->reject();
}

}  // namespace cc::app
