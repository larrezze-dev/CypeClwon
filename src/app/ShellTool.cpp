#include "ShellTool.h"

#include <QAction>
#include <QActionGroup>
#include <QDialog>
#include <algorithm>
#include <map>

#include "ShellDialogs.h"
#include "Viewport3D.h"
#include "core/node_edit.h"

namespace cc::app {

using namespace cc::core;

namespace {
Point3 toPoint(const Vec3& v) { return Point3{v.x, v.y, v.z}; }
}  // namespace

ShellTool::ShellTool(QObject* parent) : QObject(parent) {
    m_actions = new QActionGroup(this);
    m_actions->setExclusionPolicy(QActionGroup::ExclusionPolicy::ExclusiveOptional);
    const auto make = [this](const QString& text, const QString& tip, const QString& tooltip, Mode mode) {
        auto* action = new QAction(text, this);
        action->setCheckable(true);
        action->setEnabled(false);
        action->setStatusTip(tip);
        action->setToolTip(tooltip);
        m_actions->addAction(action);
        connect(action, &QAction::toggled, this, [this, mode](bool on) {
            if (on)
                setMode(mode);
            else if (m_mode == mode)
                setMode(Mode::None);
        });
        return action;
    };
    // Tooltips: help texts #64705 / #64706, #64708 / #64709, #64710, #64724, #64725 / #64726, #64715, #64716 / #64717 (laminas.md
    // section 1); the status tip is the first sentence.
    m_new = make(tr("Nueva"), tr("Permite crear una nueva lámina."),
                 tr("Permite crear una nueva lámina.\nPara crear una nueva lámina, introduzca con el botón izquierdo del ratón los puntos que definen el perímetro de la lámina y, a continuación, pulse el botón derecho del ratón."),
                 Mode::New);
    m_delete = make(tr("Borrar"), tr("Permite borrar un grupo de láminas."),
                    tr("Permite borrar un grupo de láminas. Seleccione las láminas que desea borrar con el botón izquierdo del ratón o mediante selección por rectángulo y, a continuación, pulse el botón derecho del ratón."),
                    Mode::Delete);
    m_move = make(tr("Mover"), tr("Permite mover los nudos del perímetro de la lámina a una nueva posición."),
                  tr("Permite mover los nudos del perímetro de la lámina a una nueva posición.\nSeleccione primero la lámina que desea mover con el botón izquierdo del ratón. A continuación, seleccione los nudos de la lámina que desea desplazar y pulse el botón derecho del ratón. Marque ahora dos puntos que definen la dirección y magnitud del desplazamiento con el botón izquierdo del ratón."),
                  Mode::Move);
    m_join = make(tr("Unir"), tr("Permite unir varias láminas que tengan alguna arista en común y estén contenidas en el mismo plano."),
                  tr("Permite unir varias láminas que tengan alguna arista en común y estén contenidas en el mismo plano. Si se han definido planos agrupados, el programa unirá las láminas homólogas a las seleccionadas en dichos planos. Seleccione las láminas que desea unir con el botón izquierdo del ratón o mediante selección por rectángulo y, a continuación, pulse el botón derecho del ratón."),
                  Mode::Join);
    m_divide = make(tr("Dividir"), tr("Permite dividir varias láminas."),
                    tr("Permite dividir varias láminas.\nSeleccione con el botón izquierdo del ratón dos puntos que definirán el segmento de división y pulse el botón derecho del ratón. Las láminas se dividen completamente aunque el segmento penetre en ellas de forma parcial. Si se han definido planos agrupados, el programa dividirá las láminas que intersecten con el segmento homólogo en dichos planos."),
                    Mode::Divide);
    m_invertZ = make(tr("Invertir eje 'z'"), tr("Permite invertir la dirección del eje Z local de un grupo de láminas."),
                     tr("Permite invertir la dirección del eje Z local de un grupo de láminas."), Mode::InvertZ);
    m_defineX = make(tr("Definir eje 'x'"), tr("Permite definir la dirección del eje X local de una lámina."),
                     tr("Permite definir la dirección del eje X local de una lámina.\nSeleccione con el botón izquierdo del ratón la arista de la lámina que se utilizará para definir la dirección del eje X local."),
                     Mode::DefineX);

    // "Ejes locales" (#392577): a toggle of the 3D windows, not a command of the group.
    m_localAxes = new QAction(tr("Ejes locales"), this);
    m_localAxes->setCheckable(true);
    m_localAxes->setEnabled(false);
    m_localAxes->setStatusTip(tr("Muestra u oculta los ejes locales de las láminas."));
    m_localAxes->setToolTip(tr("Muestra u oculta los ejes locales de las láminas."));  // no help text of the original found
    connect(m_localAxes, &QAction::toggled, this, [this](bool on) {
        pruneViewports();
        for (const QPointer<Viewport3D>& viewport : m_viewports) viewport->setShowShellAxes(on);
    });

    // Seccion (#65122): selects shells, RMB opens the dialog (laminas.md section 5).
    m_section = make(tr("Sección"), tr("Sección de la lámina"), tr("Sección de la lámina"), Mode::Section);

    // Material (#64718) and Disposición (#64713): select shells + RMB, then the dialog (laminas.md 7.2).
    m_layout = make(tr("Disposición"), tr("Permite ajustar la posición de varias láminas a la vez con respecto a su plano de definición."),
                    tr("Permite ajustar la posición de varias láminas a la vez con respecto a su plano de definición.\nSeleccione las láminas que desea editar con el botón izquierdo del ratón o mediante selección por rectángulo y, a continuación, pulse el botón derecho del ratón."),
                    Mode::Layout);
    m_material = make(tr("Material"), tr("Permite editar el material de un grupo de láminas."), tr("Permite editar el material de un grupo de láminas."), Mode::Material);
}

QString ShellTool::statusText(Mode mode, int step, int selected) {
    // PROVISIONAL: same layout as the measured Nudos texts, generic pieces of entopane.
    const QString lmbSelect = tr(" - LMB: Seleccione un elemento.");
    const QString act = tr(" - RMB: Actuar sobre los elementos seleccionados.");
    const QString back = tr(" - RMB: Volver al paso anterior.");
    const auto selectable = [&](const QString& name) { return name + QStringLiteral(" | ") + lmbSelect + (selected > 0 ? act : QString()); };
    switch (mode) {
        case Mode::None: return tr("Seleccione una opción del menú.");
        case Mode::New:
            // The first part is measured (laminas.md 7): "LMB: Introduzca los puntos de la polilínea"; the closing RMB text is PROVISIONAL.
            return step > 0 ? tr("Láminas - Nueva |  - LMB: Introduzca los puntos de la polilínea. - RMB: Actuar sobre los elementos seleccionados.")
                            : tr("Láminas - Nueva |  - LMB: Introduzca los puntos de la polilínea.");
        case Mode::Delete: return selectable(tr("Láminas - Borrar"));
        case Mode::Join: return selectable(tr("Láminas - Unir"));
        case Mode::InvertZ: return selectable(tr("Láminas - Invertir eje 'z'"));
        case Mode::Section: return selectable(tr("Láminas - Sección"));
        case Mode::Material: return selectable(tr("Láminas - Material"));
        case Mode::Layout: return selectable(tr("Láminas - Disposición"));
        case Mode::Move:
            if (step == 0) return tr("Láminas - Mover |  - LMB: Seleccione un elemento.");
            if (step == 1) return tr("Láminas - Mover |  - LMB: Seleccione un elemento.") + (selected > 0 ? act : back);
            return tr("Láminas - Mover |  - LMB: Introduzca un punto.") + back;
        case Mode::Divide:
            if (step == 0) return selectable(tr("Láminas - Dividir"));
            return tr("Láminas - Dividir |  - LMB: Introduzca un punto.") + back;
        case Mode::DefineX:
            return step > 0 ? tr("Láminas - Definir eje 'x' |  - LMB: Seleccione un elemento.") + back : tr("Láminas - Definir eje 'x' |  - LMB: Seleccione un elemento.");
    }
    return {};
}

void ShellTool::pruneViewports() {
    m_viewports.removeIf([](const QPointer<Viewport3D>& v) { return v.isNull(); });
}

void ShellTool::setProject(Project* project, UndoStack* undo) {
    deactivate();
    m_project = project;
    m_undo = undo;
    updateEnabled();
}

void ShellTool::addViewport(Viewport3D* viewport) {
    if (!viewport || m_viewports.contains(viewport)) return;
    m_viewports.push_back(viewport);
    viewport->setShowShellAxes(m_localAxes->isChecked());
    connect(viewport, &Viewport3D::selectionChanged, this, [this, viewport] { onSelectionChanged(viewport); });
    connect(viewport, &Viewport3D::actOnSelection, this, [this, viewport] { onActOnSelection(viewport); });
    connect(viewport, &Viewport3D::pointEntered, this, &ShellTool::onPointEntered);
    connect(viewport, &Viewport3D::rightClicked, this, &ShellTool::onRightClicked);
    connect(viewport, &Viewport3D::escapePressed, this, [this] { deactivate(); });
    if (m_mode != Mode::None) configureViewports();
}

void ShellTool::deactivate() {
    if (QAction* checked = m_actions->checkedAction()) checked->setChecked(false);
    setMode(Mode::None);
}

void ShellTool::updateEnabled() {
    const bool project = m_project != nullptr;
    const bool shells = project && !m_project->structure.shells.empty();
    m_new->setEnabled(project);  // points can be entered freely: a project is enough
    for (QAction* action : {m_delete, m_move, m_join, m_divide, m_invertZ, m_defineX, m_layout, m_section, m_material}) action->setEnabled(shells);
    m_localAxes->setEnabled(project);
    // A pressed button that just became disabled (the last shell was undone) ends its command.
    for (QAction* action : m_actions->actions())
        if (!action->isEnabled() && action->isChecked()) action->setChecked(false);
    if (!project && m_localAxes->isChecked()) m_localAxes->setChecked(false);
}

void ShellTool::structureChanged() {
    updateEnabled();
    pruneViewports();
    for (const QPointer<Viewport3D>& viewport : m_viewports) viewport->refreshStructure();
    if (!m_project || m_mode == Mode::None) return;
    // A chosen element that no longer exists (undo...) breaks the steps.
    bool broken = m_shellId != 0 && !m_project->structure.findShell(m_shellId);
    for (int id : m_shellIds) broken = broken || !m_project->structure.findShell(id);
    if (m_mode == Mode::Move || m_mode == Mode::DefineX)
        for (int id : m_nodeIds) broken = broken || !m_project->structure.findNode(id);
    if (broken) resetSteps();
}

bool ShellTool::snapping() const {
    switch (m_mode) {
        case Mode::New: return true;
        case Mode::Move: return m_step == 2;
        case Mode::Divide: return m_step == 1;
        default: return false;
    }
}

ElementKind ShellTool::pickKind() const {
    if ((m_mode == Mode::Move && m_step == 1) || (m_mode == Mode::DefineX && m_step > 0)) return ElementKind::Node;
    return ElementKind::Shell;
}

bool ShellTool::consumesPick() const {
    return (m_mode == Mode::Move && m_step == 0) || m_mode == Mode::DefineX;
}

void ShellTool::setMode(Mode mode) {
    if (mode == m_mode) return;
    // "Nueva" asks for the section and the material first, then for the points (measured). Cancelling releases the button.
    if (mode == Mode::New && !askNewProperties()) {
        if (QAction* checked = m_actions->checkedAction()) checked->setChecked(false);
        return;
    }
    if (mode != Mode::None) emit aboutToActivate();
    m_mode = mode;
    m_step = 0;
    m_points.clear();
    m_shellId = 0;
    m_shellIds.clear();
    m_nodeIds.clear();
    configureViewports();
    emit modeChanged(mode);
    emit statusChanged(statusText(mode));
}

void ShellTool::configureViewports() {
    pruneViewports();
    for (const QPointer<Viewport3D>& viewport : m_viewports) {
        viewport->setSelectionMode(false);
        viewport->setSnapMode(false);
        viewport->setRubberChain({});
        viewport->setRubberPoints(m_points);
        viewport->setMovingNode(std::nullopt);
        viewport->setNodeLabelsVisible(m_mode != Mode::None);
        viewport->setCoordinateTooltipEnabled(true);
        if (m_mode == Mode::None) continue;
        if (snapping()) {
            viewport->setSnapMode(true);
        } else {
            viewport->setSelectionFilter(SelectionFilter::only(pickKind()));
            viewport->setSelectionMode(true);
        }
        viewport->setFocus();
    }
}

void ShellTool::resetSteps() {
    m_step = 0;
    m_points.clear();
    m_shellId = 0;
    m_shellIds.clear();
    m_nodeIds.clear();
    configureViewports();
    if (m_mode != Mode::None) emit statusChanged(statusText(m_mode));
}

bool ShellTool::report(ShellError error) {
    if (error == ShellError::None) return true;
    emit messageRequested(QString::fromStdString(shellErrorMessage(error)));
    return false;
}

// ---- selection -------------------------------------------------------------------------------------------------------------

void ShellTool::onSelectionChanged(Viewport3D* viewport) {
    if (m_internalSelection || m_mode == Mode::None || snapping()) return;
    if (!consumesPick()) {  // the selection stays until the RMB acts on it
        emit statusChanged(statusText(m_mode, m_step, static_cast<int>(viewport->selection().count(pickKind()))));
        return;
    }
    const std::vector<int> ids = viewport->selection().ids(pickKind());
    if (ids.empty()) return;
    m_internalSelection = true;  // the pick is consumed: the selection is cleared so that the same element can be clicked again
    viewport->clearSelection();
    m_internalSelection = false;
    if (ids.size() != 1) return;  // a window that catches several elements does nothing
    if (pickKind() == ElementKind::Shell)
        pickShell(ids.front());
    else
        pickNode(ids.front());
}

void ShellTool::pickShell(int shellId) {
    if (!m_project || !m_project->structure.findShell(shellId)) return;
    m_shellId = shellId;
    m_step = 1;
    configureViewports();
    emit statusChanged(statusText(m_mode, m_step));
}

void ShellTool::pickNode(int nodeId) {
    if (!m_project || !m_undo || m_mode != Mode::DefineX || m_step == 0) return;
    const Shell* shell = m_project->structure.findShell(m_shellId);
    if (!shell || std::find(shell->nodeIds.begin(), shell->nodeIds.end(), nodeId) == shell->nodeIds.end()) return;  // a node of the shell only
    if (m_step == 1) {
        m_nodeIds = {nodeId};
        m_step = 2;
        emit statusChanged(statusText(m_mode, m_step));
        return;
    }
    if (m_nodeIds.front() == nodeId) return;  // the same node twice: waits for another one
    const int first = m_nodeIds.front();
    const int shellId = m_shellId;
    if (report(setShellXEdge(*m_project, *m_undo, shellId, first, nodeId))) resetSteps();  // refused: waits for another node
}

void ShellTool::onActOnSelection(Viewport3D* viewport) {
    if (!m_project || !m_undo) return;
    switch (m_mode) {
        case Mode::Delete:
        case Mode::Join:
        case Mode::InvertZ:
        case Mode::Section:
        case Mode::Material:
        case Mode::Layout: {
            const std::vector<int> shells = viewport->selection().ids(ElementKind::Shell);
            if (shells.empty()) return;
            if (m_mode == Mode::Delete) deleteShells(shells);
            else if (m_mode == Mode::Join) joinShells(shells);
            else if (m_mode == Mode::Section) editSection(shells);
            else if (m_mode == Mode::Material) editMaterial(shells);
            else if (m_mode == Mode::Layout) editLayout(shells);
            else invertZ(shells);
            m_internalSelection = true;
            viewport->clearSelection();
            m_internalSelection = false;
            emit statusChanged(statusText(m_mode));
            break;
        }
        case Mode::Divide:
            if (m_step == 0) {
                m_shellIds = viewport->selection().ids(ElementKind::Shell);
                if (m_shellIds.empty()) return;
                m_internalSelection = true;
                viewport->clearSelection();
                m_internalSelection = false;
                m_step = 1;
                configureViewports();
                emit statusChanged(statusText(m_mode, m_step));
            }
            break;
        case Mode::Move:
            if (m_step == 1) {
                const std::vector<int> nodes = viewport->selection().ids(ElementKind::Node);
                const Shell* shell = m_project->structure.findShell(m_shellId);
                m_nodeIds.clear();
                for (int id : nodes)
                    if (shell && std::find(shell->nodeIds.begin(), shell->nodeIds.end(), id) != shell->nodeIds.end()) m_nodeIds.push_back(id);
                if (m_nodeIds.empty()) return resetSteps();  // RMB on nothing: back to the first step
                m_internalSelection = true;
                viewport->clearSelection();
                m_internalSelection = false;
                m_step = 2;
                m_points.clear();
                configureViewports();
                emit statusChanged(statusText(m_mode, m_step));
            }
            break;
        default: break;
    }
    if (m_mode == Mode::DefineX && m_step > 0) resetSteps();  // RMB: back to the first step
}

void ShellTool::onRightClicked() {
    // Snap mode only: Nueva closes the contour, the point steps go back.
    if (m_mode == Mode::New) {
        if (m_points.empty()) return;
        const std::vector<Vec3> contour = m_points;
        m_points.clear();
        configureViewports();
        createShell(contour);
        emit statusChanged(statusText(m_mode));
    } else if (m_mode == Mode::Move && m_step == 2) {
        if (!m_points.empty()) {
            m_points.clear();
        } else {
            m_step = 1;
        }
        configureViewports();
        emit statusChanged(statusText(m_mode, m_step));
    } else if (m_mode == Mode::Divide && m_step == 1) {
        if (!m_points.empty()) {
            m_points.clear();
            configureViewports();
        } else {
            resetSteps();
        }
        emit statusChanged(statusText(m_mode, m_step));
    }
}

void ShellTool::onPointEntered(const Vec3& world, const SnapResult& snap) {
    if (!m_project || !m_undo || !snapping() || !snap.snapped()) return;  // like Nudos: the original needs a snap under the pointer
    if (!m_points.empty() && m_points.back() == world) return;
    m_points.push_back(world);
    if (m_mode == Mode::New) {
        for (const QPointer<Viewport3D>& viewport : m_viewports) viewport->setRubberPoints(m_points);
        emit statusChanged(statusText(m_mode, static_cast<int>(m_points.size())));
        return;
    }
    if (m_points.size() < 2) {
        for (const QPointer<Viewport3D>& viewport : m_viewports) viewport->setRubberPoints(m_points);
        return;
    }
    const Vec3 a = m_points[0], b = m_points[1];
    if (m_mode == Mode::Move) {
        const Point3 delta{b.x - a.x, b.y - a.y, b.z - a.z};
        const int shellId = m_shellId;
        const std::vector<int> nodes = m_nodeIds;
        if (report(moveShellNodes(*m_project, *m_undo, shellId, nodes, delta))) {
            resetSteps();
        } else {  // refused: waits for another pair of points
            m_points.clear();
            configureViewports();
        }
    } else if (m_mode == Mode::Divide) {
        const std::vector<int> shells = m_shellIds;
        if (report(divideShells(*m_project, *m_undo, shells, toPoint(a), toPoint(b)))) {
            resetSteps();
        } else {
            m_points.clear();
            configureViewports();
        }
    }
}

// ---- operations ------------------------------------------------------------------------------------------------------------

bool ShellTool::createShell(const std::vector<Vec3>& contour) {
    if (!m_project || !m_undo) return false;
    if (contour.size() < 3) return report(ShellError::InvalidContour);
    // The contour is checked on a copy of the model with its points added as nodes, so that a refusal leaves no node behind.
    StructuralModel scratch = m_project->structure;
    std::vector<int> scratchIds;
    for (const Vec3& p : contour) {
        const Point3 position = toPoint(p);
        const std::optional<int> existing = findNodeAt(scratch, position);
        scratchIds.push_back(existing ? *existing : scratch.addNode(position));
    }
    std::vector<std::vector<int>> contours;
    if (scratchIds.size() <= 4) {
        const ShellError error = checkContour(scratch, scratchIds);
        if (error != ShellError::None) return report(error);
        contours.push_back(scratchIds);
    } else {
        contours = decomposeContour(scratch, scratchIds);
        if (contours.empty()) return report(ShellError::InvalidContour);
        for (const std::vector<int>& piece : contours)
            if (checkContour(scratch, piece) != ShellError::None) return report(ShellError::InvalidContour);
    }
    // One undo step: the nodes the points need, then the shell(s).
    m_undo->beginMacro("Nueva lámina");
    std::map<int, int> realId;  // id in the scratch model -> id in the project
    for (std::size_t i = 0; i < contour.size(); ++i) {
        const Point3 position = toPoint(contour[i]);
        const std::optional<int> existing = findNodeAt(m_project->structure, position);
        realId[scratchIds[i]] = existing ? *existing : addNode(*m_project, *m_undo, position);
    }
    bool ok = true;
    for (const std::vector<int>& piece : contours) {
        std::vector<int> ids;
        for (int id : piece) ids.push_back(realId.at(id));
        ok = report(addShell(*m_project, *m_undo, ids, m_newSection.thicknessM, 0, nullptr, &m_newSection, &m_newMaterial)) && ok;
    }
    m_undo->endMacro();
    return ok;
}

int ShellTool::run(QDialog& dialog) { return m_runner ? m_runner(dialog) : dialog.exec(); }

bool ShellTool::editSection(const std::vector<int>& shellIds) {
    if (!m_project || !m_undo || shellIds.empty()) return false;
    const Shell* first = m_project->structure.findShell(*std::min_element(shellIds.begin(), shellIds.end()));
    if (!first) return report(ShellError::UnknownElement);
    ShellSection initial{first->thicknessM, first->ballastEnabled, first->ballastKnM3 > 0.0 ? first->ballastKnM3 : kDefaultShellBallastKnM3,
                         first->ballastBehaviour};
    ShellSectionDialog dialog(initial, m_project->units.dimensions, m_dialogParent);
    if (run(dialog) != QDialog::Accepted) return false;
    return report(setShellSection(*m_project, *m_undo, shellIds, dialog.section()));
}

bool ShellTool::askNewProperties() {
    if (!m_project) return false;
    ShellNewDialog dialog(m_newSection, m_newMaterial, *m_project, m_dialogParent);
    if (run(dialog) != QDialog::Accepted) return false;
    m_newSection = dialog.section();
    m_newMaterial = dialog.material();
    return true;
}

bool ShellTool::editMaterial(const std::vector<int>& shellIds) {
    if (!m_project || !m_undo || shellIds.empty()) return false;
    const Shell* first = m_project->structure.findShell(*std::min_element(shellIds.begin(), shellIds.end()));
    if (!first) return report(ShellError::UnknownElement);
    ShellMaterialDialog dialog(first->material, *m_project, m_dialogParent);
    if (run(dialog) != QDialog::Accepted) return false;
    std::string message;
    const ShellError error = setShellMaterial(*m_project, *m_undo, shellIds, dialog.material(), &message);
    if (error == ShellError::InvalidMaterial) {
        emit messageRequested(QString::fromStdString(message));
        return false;
    }
    return report(error);
}

bool ShellTool::editLayout(const std::vector<int>& shellIds) {
    if (!m_project || !m_undo || shellIds.empty()) return false;
    const Shell* first = m_project->structure.findShell(*std::min_element(shellIds.begin(), shellIds.end()));
    if (!first) return report(ShellError::UnknownElement);
    ShellLayoutDialog dialog(first->layout, first->thicknessM, m_dialogParent);
    if (run(dialog) != QDialog::Accepted) return false;
    return report(setShellLayout(*m_project, *m_undo, shellIds, dialog.layout()));
}

bool ShellTool::deleteShells(const std::vector<int>& shellIds) {
    return m_project && m_undo && report(cc::core::deleteShells(*m_project, *m_undo, shellIds));
}

bool ShellTool::joinShells(const std::vector<int>& shellIds) {
    return m_project && m_undo && report(cc::core::joinShells(*m_project, *m_undo, shellIds));
}

bool ShellTool::invertZ(const std::vector<int>& shellIds) {
    return m_project && m_undo && report(invertShellZ(*m_project, *m_undo, shellIds));
}

}  // namespace cc::app
