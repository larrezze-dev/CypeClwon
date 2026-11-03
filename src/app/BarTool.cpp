#include "BarTool.h"

#include <QAction>
#include <QActionGroup>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QPushButton>
#include <algorithm>

#include "Viewport3D.h"
#include "core/bar_edit.h"

namespace cc::app {

// ---- ColumnDialog -----------------------------------------------------------------------------------------------------------

ColumnDialog::ColumnDialog(const std::vector<cc::core::Level>& levels, QWidget* parent) : QDialog(parent), m_levels(levels) {
    std::sort(m_levels.begin(), m_levels.end(), [](const auto& a, const auto& b) { return a.elevation < b.elevation; });
    setWindowTitle(tr("Barras - Pilar"));
    auto* form = new QFormLayout(this);
    const auto spin = [this] {
        auto* box = new QDoubleSpinBox(this);
        box->setRange(-1e6, 1e6);
        box->setDecimals(3);
        box->setSuffix(tr(" m"));
        return box;
    };
    m_x = spin();
    m_y = spin();
    m_start = new QComboBox(this);
    m_end = new QComboBox(this);
    for (const cc::core::Level& level : m_levels) {
        const QString text = tr("%1 (%2 m)").arg(QString::fromStdString(level.name)).arg(level.elevation, 0, 'f', 3);
        m_start->addItem(text);
        m_end->addItem(text);
    }
    if (m_end->count() > 0) m_end->setCurrentIndex(m_end->count() - 1);
    m_buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    form->addRow(tr("Coordenada X"), m_x);
    form->addRow(tr("Coordenada Y"), m_y);
    form->addRow(tr("Nivel inicial"), m_start);
    form->addRow(tr("Nivel final"), m_end);
    form->addRow(m_buttons);
    connect(m_buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(m_start, &QComboBox::currentIndexChanged, this, [this] { updateOk(); });
    connect(m_end, &QComboBox::currentIndexChanged, this, [this] { updateOk(); });
    updateOk();
}

void ColumnDialog::accept() {
    if (!canAccept()) return;  // the levels must differ (#64844)
    emit columnConfirmed(m_x->value(), m_y->value(), chosenElevations());
    QDialog::accept();
}

void ColumnDialog::setPosition(double x, double y) {
    m_x->setValue(x);
    m_y->setValue(y);
}

bool ColumnDialog::canAccept() const {
    const int a = m_start->currentIndex(), b = m_end->currentIndex();
    return a >= 0 && b >= 0 && m_levels[static_cast<std::size_t>(a)].elevation < m_levels[static_cast<std::size_t>(b)].elevation;
}

void ColumnDialog::updateOk() { m_buttons->button(QDialogButtonBox::Ok)->setEnabled(canAccept()); }

std::vector<double> ColumnDialog::chosenElevations() const {
    std::vector<double> out;
    if (!canAccept()) return out;
    const int a = m_start->currentIndex(), b = m_end->currentIndex();
    for (int i = a; i <= b; ++i) out.push_back(m_levels[static_cast<std::size_t>(i)].elevation);
    return out;
}

// ---- BarTool ----------------------------------------------------------------------------------------------------------------

BarTool::BarTool(QObject* parent) : QObject(parent) {
    m_group = new QActionGroup(this);
    m_group->setExclusionPolicy(QActionGroup::ExclusionPolicy::ExclusiveOptional);
    const auto make = [this](const QString& text, const QString& tip, Mode mode) {
        auto* action = new QAction(text, this);
        action->setCheckable(true);
        action->setEnabled(false);
        action->setStatusTip(tip);
        action->setToolTip(text);
        m_group->addAction(action);
        connect(action, &QAction::toggled, this, [this, action, mode](bool on) {
            if (on) {
                if (mode == Mode::Column && !hasTwoLevels()) {  // #65140: the command does not start
                    emit messageRequested(tr("Para poder introducir pilares con esta opción debe definir previamente al menos 2 niveles en la obra."));
                    action->setChecked(false);
                    return;
                }
                setMode(mode);
            } else if (m_mode == mode) {
                setMode(Mode::None);
            }
        });
        return action;
    };
    // Rotulos #50415 / #30203 / #29511, tooltips #64616 / #64850 / #64882 (barras.md section 1); the status tip is the first sentence.
    m_bar = make(tr("Barra"), tr("Introducción de una nueva barra en la estructura."), Mode::Bar);
    m_column = make(tr("Pilar"), tr("Permite introducir un pilar indicando la posición en planta y el nivel inicial y final del mismo."), Mode::Column);
    m_beam = make(tr("Viga"), tr("Permite introducir una nueva viga."), Mode::Beam);
    m_bar->setToolTip(tr("Introducción de una nueva barra en la estructura.\nLa barra queda definida por sus nudos inicial y final, por lo que, para introducirla, deberá seleccionar dos nudos de la estructura."));
    m_column->setToolTip(tr("Permite introducir un pilar indicando la posición en planta y el nivel inicial y final del mismo. El programa creará una pieza entre cada par de niveles que recorra el pilar."));
    m_beam->setToolTip(tr("Permite introducir una nueva viga. Si introduce más de una de forma consecutiva el programa creará, si es posible, una viga continua con todas las introducidas. Cada una de las vigas tiene su propio grupo de flecha."));
}

QString BarTool::statusText(Mode mode, int chosen) {
    // PROVISIONAL (barras.md section 6, item 1): same layout as the measured Nudos texts, pieces of entopane.
    switch (mode) {
        case Mode::None: return tr("Seleccione una opción del menú.");
        case Mode::Bar:
            return chosen > 0 ? tr("Barras - Barra |  - LMB: Seleccione un elemento. - RMB: Volver al paso anterior.")
                              : tr("Barras - Barra |  - LMB: Seleccione un elemento.");
        case Mode::Column: return tr("Barras - Pilar |  - LMB: Introduzca un punto.");
        case Mode::Beam:
            return chosen > 0 ? tr("Barras - Viga |  - LMB: Seleccione un elemento. - RMB: Actuar sobre los elementos seleccionados.")
                              : tr("Barras - Viga |  - LMB: Seleccione un elemento.");
    }
    return {};
}

void BarTool::pruneViewports() {
    m_viewports.removeIf([](const QPointer<Viewport3D>& v) { return v.isNull(); });
}

bool BarTool::hasTwoLevels() const { return m_project && m_project->workPlanes.levels.size() >= 2; }

void BarTool::setProject(cc::core::Project* project, cc::core::UndoStack* undo) {
    deactivate();
    m_project = project;
    m_undo = undo;
    updateEnabled();
}

void BarTool::addViewport(Viewport3D* viewport) {
    if (!viewport || m_viewports.contains(viewport)) return;
    m_viewports.push_back(viewport);
    connect(viewport, &Viewport3D::selectionChanged, this, [this, viewport] { onSelectionChanged(viewport); });
    connect(viewport, &Viewport3D::actOnSelection, this, [this] { onActOnSelection(); });
    connect(viewport, &Viewport3D::pointEntered, this, &BarTool::onPointEntered);
    connect(viewport, &Viewport3D::digitTyped, this, [this, viewport](QChar digit) { onDigitTyped(viewport, digit); });
    connect(viewport, &Viewport3D::escapePressed, this, [this] {
        const bool active = m_mode != Mode::None;
        deactivate();
        if (active)
            for (const QPointer<Viewport3D>& v : m_viewports) v->hideCoordinateTooltip();
    });
    if (m_mode != Mode::None) configureViewports();
}

void BarTool::deactivate() {
    if (QAction* checked = m_group->checkedAction()) checked->setChecked(false);
    setMode(Mode::None);
}

void BarTool::updateEnabled() {
    const bool project = m_project != nullptr;
    for (QAction* action : {m_bar, m_column, m_beam}) {
        action->setEnabled(project);
        if (!project && action->isChecked()) action->setChecked(false);
    }
}

void BarTool::structureChanged() {
    updateEnabled();
    pruneViewports();
    for (const QPointer<Viewport3D>& viewport : m_viewports) viewport->refreshStructure();
    if (!m_chain.empty() && m_project) {  // a chosen node that no longer exists (undo of its creation, deletion...) breaks the chain
        const bool gone = std::any_of(m_chain.begin(), m_chain.end(), [this](int id) { return !m_project->structure.findNode(id); });
        if (gone) resetChain();
    }
}

void BarTool::setMode(Mode mode) {
    if (mode == m_mode) return;
    if (m_columnDialog) m_columnDialog->reject();
    m_chain.clear();
    if (mode != Mode::None) emit aboutToActivate();
    m_mode = mode;
    configureViewports();
    emit modeChanged(mode);
    emit statusChanged(statusText(mode));
}

void BarTool::configureViewports() {
    pruneViewports();
    const bool selecting = m_mode == Mode::Bar || m_mode == Mode::Beam;
    for (const QPointer<Viewport3D>& viewport : m_viewports) {
        viewport->setSelectionMode(false);
        viewport->setSnapMode(false);
        viewport->setRubberChain(m_chain);
        viewport->setMovingNode(std::nullopt);
        viewport->setNodeLabelsVisible(m_mode != Mode::None);  // the labels "N<id>" as in Nuevo (the task asks for it)
        viewport->setCoordinateTooltipEnabled(true);
        if (selecting) {
            viewport->setSelectionFilter(cc::core::SelectionFilter::only(cc::core::ElementKind::Node));
            viewport->setSelectionMode(true);
        }
        if (m_mode == Mode::Column) viewport->setSnapMode(true);
        if (m_mode != Mode::None) viewport->setFocus();
    }
}

void BarTool::resetChain() {
    m_chain.clear();
    for (const QPointer<Viewport3D>& viewport : m_viewports) viewport->setRubberChain(m_chain);
    if (m_mode != Mode::None) emit statusChanged(statusText(m_mode, 0));
}

void BarTool::onSelectionChanged(Viewport3D* viewport) {
    if (m_internalSelection || (m_mode != Mode::Bar && m_mode != Mode::Beam)) return;
    const std::vector<int> nodes = viewport->selection().ids(cc::core::ElementKind::Node);
    if (nodes.empty()) return;
    m_internalSelection = true;  // the pick is consumed: the selection is cleared so that the same node can be clicked again
    viewport->clearSelection();
    m_internalSelection = false;
    if (nodes.size() == 1) pickNode(nodes.front());  // a window that catches several nodes does nothing
}

void BarTool::pickNode(int nodeId) {
    if (!m_project || !m_undo) return;
    if (m_mode == Mode::Bar) {
        if (m_chain.empty()) {
            m_chain.push_back(nodeId);
        } else {
            if (m_chain.front() == nodeId) return;  // the same node twice: waits for another one
            cc::core::addBar(*m_project, *m_undo, m_chain.front(), nodeId);  // an existing bar between the two: nothing is created
            m_chain.clear();
        }
    } else if (m_mode == Mode::Beam) {
        if (!m_chain.empty() && m_chain.back() == nodeId) return;
        m_chain.push_back(nodeId);
    } else {
        return;
    }
    for (const QPointer<Viewport3D>& viewport : m_viewports) viewport->setRubberChain(m_chain);
    emit statusChanged(statusText(m_mode, static_cast<int>(m_chain.size())));
}

void BarTool::onActOnSelection() {
    if (m_chain.empty()) return;
    if (m_mode == Mode::Beam && m_chain.size() >= 2 && m_project && m_undo)
        cc::core::addBeamChain(*m_project, *m_undo, m_chain);  // RMB ends the introduction (#64883)
    resetChain();                                              // Barra: RMB = "Volver al paso anterior"
}

void BarTool::onPointEntered(const cc::core::Vec3& world, const cc::core::SnapResult& snap) {
    if (m_mode != Mode::Column || !snap.snapped()) return;
    if (auto* viewport = qobject_cast<Viewport3D*>(sender())) openColumnDialog(viewport, world.x, world.y, -1);
}

void BarTool::onDigitTyped(Viewport3D* viewport, QChar digit) {
    if (m_mode != Mode::Column) return;
    openColumnDialog(viewport, digit.digitValue(), 0.0, digit.digitValue());
}

void BarTool::openColumnDialog(Viewport3D* viewport, double x, double y, int) {
    if (m_columnDialog || !m_project || !hasTwoLevels()) return;
    auto* dialog = new ColumnDialog(m_project->workPlanes.levels, viewport);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setPosition(x, y);
    m_columnDialog = dialog;
    connect(dialog, &ColumnDialog::columnConfirmed, this, [this](double px, double py, const std::vector<double>& levels) {
        if (m_project && m_undo) cc::core::addColumn(*m_project, *m_undo, px, py, levels);
    });
    connect(dialog, &QDialog::finished, this, [guard = QPointer<Viewport3D>(viewport)] {
        if (guard) guard->setFocus();
    });
    dialog->open();
}

}  // namespace cc::app
