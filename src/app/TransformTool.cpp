#include "TransformTool.h"

#include <QAction>
#include <QActionGroup>
#include <QDialogButtonBox>
#include <QCursor>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QListWidget>
#include <QToolButton>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>

#include "NodeCalculatorDialog.h"
#include "Viewport3D.h"

namespace cc::app {

using namespace cc::core;

// ---- dialogs ---------------------------------------------------------------------------------------------------------------

InlineAngleEditor::InlineAngleEditor(QWidget* parent) : QFrame(parent) {
    setObjectName(QStringLiteral("inlineAngleEditor"));
    setFrameShape(QFrame::Box);
    setAutoFillBackground(true);
    setStyleSheet(QStringLiteral("QFrame#inlineAngleEditor { background: white; border: 1px solid #6A6A6A; } QLabel { background: transparent; }"));
    auto* row = new QHBoxLayout(this);
    row->setContentsMargins(6, 3, 3, 3);
    row->setSpacing(2);
    row->addWidget(new QLabel(tr("Ángulo de giro ["), this));
    // Measured: default 0.000, range -360.000 to 360.000 grados (herramientas.md 6.3).
    m_edit = new RangedEdit(tr("Ángulo de giro"), cc::core::ValueRange{-360.0, 360.0}, 3, this);
    m_edit->setBoundUnit(tr("grados"));
    m_edit->setValue(0.0);
    m_edit->setMaximumWidth(70);
    row->addWidget(m_edit);
    row->addWidget(new QLabel(tr("] grados"), this));
    m_accept = new QToolButton(this);
    m_accept->setText(QStringLiteral("\u2713"));
    m_accept->setToolTip(tr("Aceptar"));
    m_accept->setStyleSheet(QStringLiteral("QToolButton { color: white; background: #2E9B3A; border: none; min-width: 22px; min-height: 22px; font-weight: bold; }"));
    m_cancel = new QToolButton(this);
    m_cancel->setText(QStringLiteral("\u2717"));
    m_cancel->setToolTip(tr("Cancelar"));
    m_cancel->setStyleSheet(QStringLiteral("QToolButton { color: white; background: #C0392B; border: none; min-width: 22px; min-height: 22px; font-weight: bold; }"));
    row->addWidget(m_accept);
    row->addWidget(m_cancel);
    connect(m_accept, &QToolButton::clicked, this, [this] { tryAccept(); });
    connect(m_cancel, &QToolButton::clicked, this, &InlineAngleEditor::cancelled);
    connect(m_edit, &QLineEdit::returnPressed, this, [this] { tryAccept(); });
}

bool InlineAngleEditor::tryAccept() {
    if (!m_edit->commit()) return false;  // out of range: the warning was shown and the previous value restored
    emit acceptedValue(m_edit->value());
    return true;
}

void InlineAngleEditor::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Escape) {
        emit cancelled();
        return;
    }
    QFrame::keyPressEvent(event);
}

RepairResultDialog::RepairResultDialog(const QString& result, const std::vector<RepairIssue>& issues, QWidget* parent) : QDialog(parent) {
    setWindowTitle(tr("Reparar"));
    auto* layout = new QVBoxLayout(this);
    auto* label = new QLabel(result, this);
    label->setWordWrap(true);
    layout->addWidget(label);
    if (!issues.empty()) {
        auto* list = new QListWidget(this);
        for (const RepairIssue& issue : issues) list->addItem(QString::fromStdString(issue.message));
        layout->addWidget(list);
    }
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok, this);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
}

// ---- TransformTool ---------------------------------------------------------------------------------------------------------

TransformTool::TransformTool(QObject* parent) : QObject(parent) {
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
    // Titles #64802, #64814, #64805, #64808, #64811, #64817, #64820; help texts #64803 / #64804, #64815 / #64816, #64806 / #64807,
    // #64809 / #64810, #64812 / #64813, #64818 / #64819, #64821 / #64822 (herramientas.md section 1). The status tip is the title.
    m_move = make(tr("Mover elementos"), tr("Permite mover una selección de elementos a una nueva posición."),
                  tr("Mover elementos\nPermite mover una selección de elementos a una nueva posición.\nSeleccione los elementos a mover con el botón izquierdo del ratón. Cuando haya terminado la selección, pulse el botón derecho del ratón y, de nuevo con el izquierdo, marque los dos puntos que definirán el origen del desplazamiento y su magnitud."),
                  Mode::Move);
    m_copy = make(tr("Copiar elementos"), tr("Permite realizar la copia de una selección de elementos en una nueva posición."),
                  tr("Copiar elementos\nPermite realizar la copia de una selección de elementos en una nueva posición.\nSeleccione los elementos a copiar con el botón izquierdo del ratón. Cuando haya terminado la selección, pulse el botón derecho del ratón y, de nuevo con el izquierdo, marque los dos puntos que definirán el origen del desplazamiento y su magnitud."),
                  Mode::Copy);
    m_rotateAxis = make(tr("Girar elementos alrededor de un eje definido por dos puntos"),
                        tr("Permite realizar el giro de una selección de elementos alrededor de un eje definido por dos puntos."),
                        tr("Girar elementos alrededor de un eje definido por dos puntos\nPermite realizar el giro de una selección de elementos alrededor de un eje definido por dos puntos. Si realiza la operación en una vista 2D, el giro se realiza en el plano de la vista.\nSeleccione los elementos a girar con el botón izquierdo del ratón. Cuando haya terminado la selección, pulse el botón derecho del ratón y de nuevo con el izquierdo, marque los dos puntos que definirán el eje de giro y, a continuación, introduzca el ángulo de giro alrededor del eje."),
                        Mode::RotateAxis);
    m_rotateXY = make(tr("Girar elementos en el plano xy"), tr("Permite realizar el giro de una selección de elementos alrededor del eje z global."),
                      tr("Girar elementos en el plano xy\nPermite realizar el giro de una selección de elementos alrededor del eje z global. Si realiza la operación en una vista 2D, el giro se realiza en el plano de la vista.\nSeleccione los elementos a girar con el botón izquierdo del ratón. Cuando haya terminado la selección, pulse el botón derecho del ratón y, de nuevo con el izquierdo, marque los dos puntos que definirán el ángulo de giro."),
                      Mode::RotateXY);
    m_rotateXY3 = make(tr("Girar elementos en el plano xy dados tres puntos"),
                       tr("Permite realizar el giro de una selección de elementos alrededor del eje z global respecto del origen indicado por el usuario y con el ángulo definido por tres puntos cualesquiera."),
                       tr("Girar elementos en el plano xy dados tres puntos\nPermite realizar el giro de una selección de elementos alrededor del eje z global respecto del origen indicado por el usuario y con el ángulo definido por tres puntos cualesquiera. Si realiza la operación en una vista 2D, el giro se realiza en el plano de la vista.\nSeleccione los elementos a girar con el botón izquierdo del ratón. Cuando haya terminado la selección, pulse el botón derecho del ratón y de nuevo con el izquierdo, marque el punto de origen del giro y los tres puntos que definirán la magnitud del mismo."),
                       Mode::RotateXY3);
    m_mirrorXY = make(tr("Simetría de elementos en el plano xy"),
                      tr("Permite realizar la copia simétrica de una selección de elementos respecto del plano definido por un vector introducido por el usuario y el eje z global."),
                      tr("Simetría de elementos en el plano xy\nPermite realizar la copia simétrica de una selección de elementos respecto del plano definido por un vector introducido por el usuario y el eje z global. Si realiza la operación en una vista 2D, la simetría se realiza respecto a la recta definida por el vector.\nSeleccione los elementos a copiar con el botón izquierdo del ratón. Cuando haya terminado la selección, pulse el botón derecho del ratón y de nuevo con el izquierdo, marque los dos puntos que definirán el origen de la simetría y la dirección del vector en el plano xy global."),
                      Mode::MirrorXY);
    m_mirrorPlane = make(tr("Simetría de elementos respecto de un plano"),
                         tr("Permite realizar la copia simétrica de una selección de elementos respecto de un plano definido por tres puntos."),
                         tr("Simetría de elementos respecto de un plano\nPermite realizar la copia simétrica de una selección de elementos respecto de un plano definido por tres puntos. Si realiza la operación en una vista 2D, la simetría se realiza respecto a la recta definida por los dos primeros puntos.\nSeleccione los elementos a copiar con el botón izquierdo del ratón. Cuando haya terminado la selección, pulse el botón derecho del ratón y de nuevo con el izquierdo, y marque los tres puntos que definirán el plano de simetría."),
                         Mode::MirrorPlane);

    // "Reparar" (#399486): an immediate command, not a mode.
    m_repair = new QAction(tr("Reparar"), this);
    m_repair->setEnabled(false);
    m_repair->setStatusTip(tr("Reparar"));
    m_repair->setToolTip(tr("Reparar"));
    connect(m_repair, &QAction::triggered, this, [this] {
        deactivate();
        repair();
    });

    updateEnabled();
}

int TransformTool::pointsNeeded(Mode mode) {
    switch (mode) {
        case Mode::Move:
        case Mode::Copy:
        case Mode::RotateAxis:
        case Mode::RotateXY:
        case Mode::MirrorXY: return 2;
        case Mode::MirrorPlane: return 3;
        case Mode::RotateXY3: return 4;
        case Mode::None: return 0;
    }
    return 0;
}

QString TransformTool::statusText(Mode mode, int step, int selected) {
    // PROVISIONAL: same layout as the measured Nudos texts, generic pieces of entopane.
    QString name;
    switch (mode) {
        case Mode::None: return tr("Seleccione una opción del menú.");
        case Mode::Move: name = tr("Herramientas - Mover elementos"); break;
        case Mode::Copy: name = tr("Herramientas - Copiar elementos"); break;
        case Mode::RotateAxis: name = tr("Herramientas - Girar elementos alrededor de un eje"); break;
        case Mode::RotateXY: name = tr("Herramientas - Girar elementos en el plano xy"); break;
        case Mode::RotateXY3: name = tr("Herramientas - Girar elementos en el plano xy dados tres puntos"); break;
        case Mode::MirrorXY: name = tr("Herramientas - Simetría de elementos en el plano xy"); break;
        case Mode::MirrorPlane: name = tr("Herramientas - Simetría de elementos respecto de un plano"); break;
    }
    if (step == 0) return name + tr(" |  - LMB: Seleccione un elemento.") + (selected > 0 ? tr(" - RMB: Actuar sobre los elementos seleccionados.") : QString());
    return name + tr(" |  - LMB: Introduzca un punto. - RMB: Volver al paso anterior.");
}

void TransformTool::pruneViewports() {
    m_viewports.removeIf([](const QPointer<Viewport3D>& v) { return v.isNull(); });
}

int TransformTool::run(QDialog& dialog) { return m_runner ? m_runner(dialog) : dialog.exec(); }

void TransformTool::setProject(Project* project, UndoStack* undo) {
    deactivate();
    m_project = project;
    m_undo = undo;
    updateEnabled();
}

void TransformTool::addViewport(Viewport3D* viewport) {
    if (!viewport || m_viewports.contains(viewport)) return;
    m_viewports.push_back(viewport);
    connect(viewport, &Viewport3D::selectionChanged, this, [this, viewport] { onSelectionChanged(viewport); });
    connect(viewport, &Viewport3D::actOnSelection, this, [this, viewport] { onActOnSelection(viewport); });
    connect(viewport, &Viewport3D::pointEntered, this, &TransformTool::onPointEntered);
    connect(viewport, &Viewport3D::rightClicked, this, &TransformTool::onRightClicked);
    connect(viewport, &Viewport3D::escapePressed, this, [this] { deactivate(); });
    connect(viewport, &Viewport3D::digitTyped, this, [this, viewport](QChar digit) { onDigitTyped(viewport, digit); });
    if (m_mode != Mode::None) configureViewports();
}

void TransformTool::closeEditors() {
    if (m_angleEditor) m_angleEditor->deleteLater();
    m_angleEditor.clear();
    if (m_calculator) m_calculator->reject();
}

void TransformTool::deactivate() {
    closeEditors();
    if (QAction* checked = m_actions->checkedAction()) checked->setChecked(false);
    setMode(Mode::None);
}

void TransformTool::updateEnabled() {
    // The original greys the whole group out on an empty work (orig_geometria_ribbon.png): there is nothing to select.
    const bool elements = m_project && !m_project->structure.nodes.empty();
    for (QAction* action : m_actions->actions()) {
        action->setEnabled(elements);
        if (!elements && action->isChecked()) action->setChecked(false);
    }
    m_repair->setEnabled(elements);
}

void TransformTool::structureChanged() {
    updateEnabled();
    pruneViewports();
    for (const QPointer<Viewport3D>& viewport : m_viewports) viewport->refreshStructure();
}

void TransformTool::setMode(Mode mode) {
    if (mode == m_mode) return;
    if (mode != Mode::None) emit aboutToActivate();
    m_mode = mode;
    m_step = 0;
    m_points.clear();
    m_selection = {};
    configureViewports();
    emit modeChanged(mode);
    emit statusChanged(statusText(mode));
}

void TransformTool::configureViewports() {
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
        if (m_step == 1) {
            viewport->setSnapMode(true);
        } else {
            viewport->setSelectionFilter({true, true, true});
            viewport->setSelectionMode(true);
        }
        viewport->setFocus();
    }
}

void TransformTool::resetSteps() {
    closeEditors();
    m_step = 0;
    m_points.clear();
    m_selection = {};
    configureViewports();
    if (m_mode != Mode::None) emit statusChanged(statusText(m_mode));
}

void TransformTool::onSelectionChanged(Viewport3D* viewport) {
    if (m_internalSelection || m_mode == Mode::None || m_step != 0) return;
    const Selection& selection = viewport->selection();
    emit statusChanged(statusText(m_mode, 0, static_cast<int>(selection.size())));  // the selection stays until RMB acts on it
}

void TransformTool::onActOnSelection(Viewport3D* viewport) {
    if (m_mode == Mode::None || m_step != 0 || !m_project) return;
    const Selection& selection = viewport->selection();
    if (selection.empty()) return;
    m_selection.nodeIds = selection.ids(ElementKind::Node);
    m_selection.barIds = selection.ids(ElementKind::Bar);
    m_selection.shellIds = selection.ids(ElementKind::Shell);
    m_internalSelection = true;
    viewport->clearSelection();
    m_internalSelection = false;
    m_step = 1;
    m_points.clear();
    configureViewports();
    emit statusChanged(statusText(m_mode, 1));
}

void TransformTool::onRightClicked() {
    // Snap mode only (step 1): "Volver al paso anterior".
    if (m_mode == Mode::None || m_step != 1) return;
    if (m_angleEditor) {  // RMB closes the inline editor and goes back to the points
        closeEditors();
        m_points.clear();
        configureViewports();
        emit statusChanged(statusText(m_mode, 1));
        return;
    }
    if (!m_points.empty()) {
        m_points.clear();
        configureViewports();
        emit statusChanged(statusText(m_mode, 1));
    } else {
        resetSteps();
    }
}

void TransformTool::onPointEntered(const Vec3& world, const SnapResult& snap) {
    if (m_mode == Mode::None || m_step != 1 || !snap.snapped()) return;  // like Nudos: a snap is needed under the pointer
    addPoint(world);
}

void TransformTool::onDigitTyped(Viewport3D* viewport, QChar digit) {
    // Measured: typing a digit while a point is expected opens the "Calculadora" (Coordenada X / Y / Z).
    if (m_mode == Mode::None || m_step != 1 || m_angleEditor || m_calculator) return;
    auto* dialog = new NodeCalculatorDialog(viewport);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    m_calculator = dialog;
    connect(dialog, &NodeCalculatorDialog::pointConfirmed, this, [this](double x, double y, double z) { addPoint({x, y, z}); });
    connect(dialog, &QDialog::finished, this, [guard = QPointer<Viewport3D>(viewport)] {
        if (guard) guard->setFocus();
    });
    dialog->start(digit);
    dialog->open();
}

void TransformTool::addPoint(const Vec3& world) {
    if (m_mode == Mode::None || m_step != 1 || m_angleEditor) return;
    if (!m_points.empty() && m_points.back() == world) return;
    m_points.push_back(world);
    if (static_cast<int>(m_points.size()) < pointsNeeded(m_mode)) {
        for (const QPointer<Viewport3D>& viewport : m_viewports) viewport->setRubberPoints(m_points);
        emit statusChanged(statusText(m_mode, 1));
        return;
    }
    if (m_mode == Mode::RotateAxis) {  // the two points of the axis are known: the angle comes from the inline editor
        askAngle(m_viewports.isEmpty() ? nullptr : m_viewports.front().data());
        return;
    }
    const std::vector<Vec3> points = m_points;
    const ToolSelection selection = m_selection;
    if (apply(m_mode, selection, points)) {
        resetSteps();
    } else {  // refused or cancelled: waits for other points
        m_points.clear();
        configureViewports();
    }
}

void TransformTool::askAngle(Viewport3D* viewport) {
    if (!viewport) return;
    auto* editor = new InlineAngleEditor(viewport);
    m_angleEditor = editor;
    // Next to the cursor, kept inside the window.
    QPoint at = viewport->mapFromGlobal(QCursor::pos()) + QPoint(16, 16);
    editor->adjustSize();
    at.setX(std::clamp(at.x(), 0, std::max(0, viewport->width() - editor->width())));
    at.setY(std::clamp(at.y(), 0, std::max(0, viewport->height() - editor->height())));
    editor->move(at);
    connect(editor, &InlineAngleEditor::acceptedValue, this, [this](double degrees) {
        const std::vector<Vec3> points = m_points;
        const ToolSelection selection = m_selection;
        closeEditors();
        if (apply(Mode::RotateAxis, selection, points, degrees)) {
            resetSteps();
        } else {
            m_points.clear();
            configureViewports();
        }
    });
    connect(editor, &InlineAngleEditor::cancelled, this, [this, guard = QPointer<Viewport3D>(viewport)] {
        closeEditors();
        m_points.clear();
        configureViewports();
        if (guard) guard->setFocus();
    });
    editor->show();
    editor->raise();
    editor->edit()->setFocus();
    editor->edit()->selectAll();
}

bool TransformTool::apply(Mode mode, const ToolSelection& selection, const std::vector<Vec3>& points, std::optional<double> angleDegrees) {
    if (!m_project || !m_undo || static_cast<int>(points.size()) < pointsNeeded(mode)) return false;
    const auto point = [&](std::size_t i) { return Point3{points[i].x, points[i].y, points[i].z}; };
    constexpr double kPi = 3.14159265358979323846;
    const auto fail = [this](ToolError error) {
        emit messageRequested(QString::fromStdString(toolErrorMessage(error)));
        return false;
    };
    std::optional<Transform3> transform;
    bool copy = false;
    switch (mode) {
        case Mode::Move: transform = translation(point(0), point(1)); break;
        case Mode::Copy: {
            transform = translation(point(0), point(1));
            copy = true;  // measured: no number of copies, a single copy
            break;
        }
        case Mode::RotateAxis: {
            if (!angleDegrees) return false;
            transform = rotationAboutAxis(point(0), point(1), *angleDegrees);
            break;
        }
        case Mode::RotateXY: {
            const double dx = points[1].x - points[0].x, dy = points[1].y - points[0].y;
            if (std::hypot(dx, dy) == 0.0) return fail(ToolError::InvalidTransform);
            transform = rotationInXY(point(0), std::atan2(dy, dx) * 180.0 / kPi);
            break;
        }
        case Mode::RotateXY3: {
            const double ax = points[1].x - points[2].x, ay = points[1].y - points[2].y;  // second -> first
            const double bx = points[3].x - points[2].x, by = points[3].y - points[2].y;  // second -> third
            if (std::hypot(ax, ay) == 0.0 || std::hypot(bx, by) == 0.0) return fail(ToolError::InvalidTransform);
            transform = rotationInXY(point(0), std::atan2(ax * by - ay * bx, ax * bx + ay * by) * 180.0 / kPi);
            break;
        }
        case Mode::MirrorXY:
            transform = reflectionVertical(point(0), point(1));
            copy = true;
            break;
        case Mode::MirrorPlane:
            transform = reflectionAcrossPlane(point(0), point(1), point(2));
            copy = true;
            break;
        case Mode::None: return false;
    }
    if (!transform) return fail(ToolError::InvalidTransform);
    const ToolError error = transformElements(*m_project, *m_undo, selection, *transform, copy, 1);
    if (error != ToolError::None) return fail(error);
    return true;
}

bool TransformTool::repair() {
    if (!m_project || !m_undo) return false;
    const QString question = QString::fromStdString(repairConfirmationMessage());  // #399477
    if (m_confirmer && !m_confirmer(question)) return false;
    RepairReport report;
    const bool modified = repairModel(*m_project, *m_undo, &report);
    // #399478 / #399479, with the incidences found listed below.
    RepairResultDialog dialog(QString::fromStdString(repairResultMessage(modified)), report.issues, m_dialogParent);
    run(dialog);
    return modified;
}

}  // namespace cc::app
