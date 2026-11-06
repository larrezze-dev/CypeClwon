#include "LigatureTool.h"

#include <QAction>
#include <QCheckBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QRadioButton>
#include <QStackedWidget>
#include <QVBoxLayout>

#include "Viewport3D.h"
#include "core/node_ligature_edit.h"

namespace cc::app {

using namespace cc::core;

namespace {
/// Grey band with the title of a group, like the original's group headers; `help` adds the blue "i" at the right end.
QWidget* groupHeader(const QString& title, bool help, QWidget* parent) {
    auto* band = new QWidget(parent);
    band->setStyleSheet(QStringLiteral("background:#E8E8E8;"));
    auto* row = new QHBoxLayout(band);
    row->setContentsMargins(6, 2, 6, 2);
    auto* label = new QLabel(title, band);
    row->addWidget(label, 1);
    if (help) {
        auto* info = new QLabel(QStringLiteral("i"), band);
        info->setAlignment(Qt::AlignCenter);
        info->setFixedSize(16, 16);
        info->setStyleSheet(QStringLiteral("background:#1E88E5;color:white;border-radius:8px;font-weight:bold;"));
        row->addWidget(info);
    }
    return band;
}

void setBold(QWidget* widget, bool bold) {
    QFont font = widget->font();
    font.setBold(bold);
    widget->setFont(font);
}
}  // namespace

// ---- panel ------------------------------------------------------------------------------------------------------------------

LigaturePanel::LigaturePanel(QWidget* parent) : CypeDialog(tr("Propiedades - Ligaduras"), parent) {
    setModal(false);
    setMaximizable();
    contentLayout()->setContentsMargins(0, 0, 0, 0);
    contentLayout()->setSpacing(0);
    contentLayout()->addWidget(groupHeader(tr("Opciones"), false, this));
    auto* options = new QVBoxLayout();
    options->setContentsMargins(6, 4, 6, 6);
    options->setSpacing(2);
    m_assign = new QRadioButton(tr("Asignar"), this);
    m_remove = new QRadioButton(tr("Eliminar"), this);
    m_assign->setChecked(true);
    setBold(m_assign, true);
    options->addWidget(m_assign);
    options->addWidget(m_remove);
    contentLayout()->addLayout(options);
    contentLayout()->addWidget(groupHeader(tr("Desplazamientos"), true, this));
    // One set of "Desplazamiento en X / Y / Z" per radio (measured), all unchecked at the start.
    m_pages = new QStackedWidget(this);
    const QString names[3] = {tr("Desplazamiento en X"), tr("Desplazamiento en Y"), tr("Desplazamiento en Z")};
    for (int set = 0; set < 2; ++set) {
        auto* page = new QWidget(m_pages);
        auto* column = new QVBoxLayout(page);
        column->setContentsMargins(6, 4, 6, 4);
        column->setSpacing(2);
        for (int dir = 0; dir < 3; ++dir) {
            m_checks[set][dir] = new QCheckBox(names[dir], page);
            column->addWidget(m_checks[set][dir]);
        }
        m_pages->addWidget(page);
    }
    contentLayout()->addWidget(m_pages);
    contentLayout()->addStretch(1);
    connect(m_assign, &QRadioButton::toggled, this, [this](bool on) {
        setBold(m_assign, on);
        setBold(m_remove, !on);
        m_pages->setCurrentIndex(on ? 0 : 1);
    });
    setMinimumSize(270, 360);
}

bool LigaturePanel::assign() const { return m_assign->isChecked(); }

LigatureDirections LigaturePanel::directions() const {
    const int set = assign() ? 0 : 1;
    return {m_checks[set][0]->isChecked(), m_checks[set][1]->isChecked(), m_checks[set][2]->isChecked()};
}

void LigaturePanel::done(int result) {
    CypeDialog::done(result);
    emit closed();
}

// ---- tool -------------------------------------------------------------------------------------------------------------------

LigatureTool::LigatureTool(QObject* parent) : QObject(parent) {
    m_action = new QAction(tr("Ligaduras"), this);
    m_action->setCheckable(true);
    m_action->setEnabled(false);
    // Description #64790 of the original (the tooltip also shows the title in bold above it).
    const QString tip = tr("Permite asignar o eliminar ligaduras de nudos para desplazamientos en los ejes globales X, Y y Z.");
    m_action->setStatusTip(tip);
    m_action->setToolTip(tip);
    connect(m_action, &QAction::toggled, this, [this](bool on) {
        if (on) start();
        else if (m_mode != Mode::None) deactivate();
    });
}

QString LigatureTool::statusText(Mode mode) {
    if (mode == Mode::None) return tr("Seleccione una opción del menú.");
    return tr("Nudos - Ligaduras |  - LMB: Seleccione un grupo de elementos.");  // measured: no "- RMB" hint
}

void LigatureTool::pruneViewports() {
    m_viewports.removeIf([](const QPointer<Viewport3D>& v) { return v.isNull(); });
}

void LigatureTool::setProject(Project* project, UndoStack* undo) {
    deactivate();
    m_project = project;
    m_undo = undo;
    for (const QPointer<Viewport3D>& viewport : m_viewports)
        if (viewport) viewport->setNodeLigatures(m_project ? &m_project->ligatures : nullptr);
    updateEnabled();
}

void LigatureTool::addViewport(Viewport3D* viewport) {
    if (!viewport || m_viewports.contains(viewport)) return;
    m_viewports.push_back(viewport);
    viewport->setNodeLigatures(m_project ? &m_project->ligatures : nullptr);
    connect(viewport, &Viewport3D::actOnSelection, this, [this, viewport] { onActOnSelection(viewport); });
    connect(viewport, &Viewport3D::escapePressed, this, [this] { deactivate(); });
    if (m_mode != Mode::None) configureViewports();
}

void LigatureTool::updateEnabled() {
    const bool nodes = m_project && !m_project->structure.nodes.empty();
    m_action->setEnabled(nodes);
    if (!nodes && m_action->isChecked()) m_action->setChecked(false);
}

void LigatureTool::structureChanged() {
    updateEnabled();
    pruneViewports();
    for (const QPointer<Viewport3D>& viewport : m_viewports) viewport->refreshStructure();
}

void LigatureTool::start() {
    if (!m_project || m_mode != Mode::None) return;
    emit aboutToActivate();
    m_mode = Mode::Active;
    if (!m_panel) {
        m_panel = new LigaturePanel(m_dialogParent);
        m_panel->setAttribute(Qt::WA_DeleteOnClose);
        connect(m_panel, &LigaturePanel::closed, this, [this] {
            if (m_action->isChecked()) m_action->setChecked(false);
        });
    }
    m_panel->show();
    configureViewports();
    emit modeChanged(m_mode);
    emit statusChanged(statusText(m_mode));  // after the viewports: setSelectionMode announces its own, generic hint
}

void LigatureTool::deactivate() {
    if (m_action->isChecked()) m_action->setChecked(false);
    if (m_mode == Mode::None) return;
    m_mode = Mode::None;
    if (m_panel) m_panel->close();
    configureViewports();
    emit modeChanged(m_mode);
    emit statusChanged(statusText(m_mode));
}

void LigatureTool::configureViewports() {
    pruneViewports();
    const bool on = m_mode != Mode::None;
    for (const QPointer<Viewport3D>& viewport : m_viewports) {
        viewport->setSelectionMode(false);
        viewport->setNodeLabelsVisible(on);
        viewport->setLigatureLabelsVisible(on);
        viewport->setNodeExclusion({});
        if (!on) continue;
        // Nodes on a line / plane restraint are never captured, silently (measured).
        const Project* project = m_project;
        viewport->setNodeExclusion([project](int id) {
            return project && !isLigatureCapturable(project->structure, project->nodeSupports, id);
        });
        viewport->setSelectionFilter(SelectionFilter::only(ElementKind::Node));
        viewport->setSelectionMode(true);
        viewport->setFocus();
    }
}

bool LigatureTool::execute(const std::vector<int>& nodeIds) {
    if (!m_project || !m_undo || !m_panel) return false;
    const std::vector<int> nodes = ligatureSelection(*m_project, nodeIds);
    if (nodes.empty()) return false;
    const LigatureDirections directions = m_panel->directions();
    if (m_panel->assign()) {
        const LigatureCheck check = assignLigatures(*m_project, *m_undo, nodes, directions);
        if (check.status == LigatureStatus::Error) emit errorRequested(QString::fromStdString(check.text));
        return check.status == LigatureStatus::Ok;
    }
    return removeLigatures(*m_project, *m_undo, nodes, directions);
}

void LigatureTool::onActOnSelection(Viewport3D* viewport) {
    if (m_mode == Mode::None || !m_project || !m_undo) return;
    const std::vector<int> nodes = viewport->selection().ids(ElementKind::Node);
    if (nodes.empty()) return;
    const std::size_t before = m_undo->count();
    const bool ok = execute(nodes);
    // An error or a silent check keeps the selection (measured); an executed option empties it.
    if (ok || m_undo->count() != before || !m_panel || !m_panel->assign()) viewport->clearSelection();
    emit statusChanged(statusText(m_mode));
}

}  // namespace cc::app
