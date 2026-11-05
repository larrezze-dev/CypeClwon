#include "ReferenceTool.h"

#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QRadioButton>
#include <QVBoxLayout>
#include <algorithm>

#include "Viewport3D.h"

namespace cc::app {

using namespace cc::core;

namespace {
void fillKinds(QComboBox* combo) {
    // Measured: the default is "Nudos". PROVISIONAL: "Láminas" is the other kind that has a reference in the model.
    combo->addItem(QObject::tr("Nudos"), static_cast<int>(ReferenceKind::Node));
    combo->addItem(QObject::tr("Láminas"), static_cast<int>(ReferenceKind::Shell));
}
ReferenceKind kindOf(const QComboBox* combo) { return static_cast<ReferenceKind>(combo->currentData().toInt()); }
}  // namespace

// ---- panels -----------------------------------------------------------------------------------------------------------------

ReferencePanel::ReferencePanel(QWidget* parent) : CypeDialog(tr("Propiedades"), parent) {
    setModal(false);
    auto* row = new QHBoxLayout();
    row->addWidget(new QLabel(tr("Tipo de elemento"), this));
    m_kind = new QComboBox(this);
    fillKinds(m_kind);
    row->addWidget(m_kind, 1);
    contentLayout()->addLayout(row);
    m_individual = new QRadioButton(tr("Individual"), this);
    m_multiple = new QRadioButton(tr("Múltiple"), this);
    m_individual->setChecked(true);
    QFont bold = m_individual->font();
    bold.setBold(true);
    m_individual->setFont(bold);
    contentLayout()->addSpacing(8);
    contentLayout()->addWidget(m_individual);
    contentLayout()->addWidget(m_multiple);
    contentLayout()->addStretch(1);
    connect(m_kind, &QComboBox::currentIndexChanged, this, &ReferencePanel::kindChanged);
    connect(m_individual, &QRadioButton::toggled, this, [this](bool on) {
        QFont f = m_individual->font();
        f.setBold(on);
        m_individual->setFont(f);
        QFont g = m_multiple->font();
        g.setBold(!on);
        m_multiple->setFont(g);
        emit kindChanged();
    });
    setMinimumSize(240, 150);
}

ReferenceKind ReferencePanel::kind() const { return kindOf(m_kind); }
bool ReferencePanel::multiple() const { return m_multiple->isChecked(); }

void ReferencePanel::done(int result) {
    CypeDialog::done(result);
    emit closed();
}

FindPanel::FindPanel(QWidget* parent) : CypeDialog(tr("Propiedades - Buscar"), parent) {
    setModal(false);
    auto* grid = new QGridLayout();
    grid->addWidget(new QLabel(tr("Tipo de elemento"), this), 0, 0);
    m_kind = new QComboBox(this);
    fillKinds(m_kind);
    grid->addWidget(m_kind, 0, 1);
    grid->addWidget(new QLabel(tr("Referencia"), this), 1, 0);
    m_text = new QLineEdit(this);
    grid->addWidget(m_text, 1, 1);
    contentLayout()->addLayout(grid);
    m_whole = new QCheckBox(tr("Sólo palabra completa"), this);
    contentLayout()->addWidget(m_whole);
    contentLayout()->addStretch(1);
    connect(m_kind, &QComboBox::currentIndexChanged, this, &FindPanel::changed);
    connect(m_text, &QLineEdit::textChanged, this, &FindPanel::changed);
    connect(m_whole, &QCheckBox::toggled, this, &FindPanel::changed);
    setMinimumSize(300, 140);
}

ReferenceKind FindPanel::kind() const { return kindOf(m_kind); }
QString FindPanel::text() const { return m_text->text(); }
bool FindPanel::wholeWord() const { return m_whole->isChecked(); }

ReferenceTextDialog::ReferenceTextDialog(const QString& title, const QString& text, QWidget* parent) : CypeDialog(title, parent) {
    auto* row = new QHBoxLayout();
    row->addWidget(new QLabel(tr("Referencia"), this));
    m_edit = new QLineEdit(text, this);
    row->addWidget(m_edit, 1);
    contentLayout()->addLayout(row);
    setupFooter(tr("Aceptar"), tr("Cancelar"));
    setMinimumWidth(320);
    m_edit->selectAll();
}

QString ReferenceTextDialog::text() const { return m_edit->text().trimmed(); }

// ---- ReferenceTool ----------------------------------------------------------------------------------------------------------

ReferenceTool::ReferenceTool(QObject* parent) : QObject(parent) {
    m_assign = new QAction(tr("Asignar referencia"), this);
    m_assign->setCheckable(true);
    m_assign->setEnabled(false);
    m_assign->setStatusTip(tr("Asignar referencia"));
    m_assign->setToolTip(tr("Asignar referencia"));  // measured: the tooltip is only the title
    connect(m_assign, &QAction::toggled, this, [this](bool on) {
        if (on) start();
        else if (m_mode != Mode::None) deactivate();
    });
    m_find = new QAction(tr("Buscar"), this);
    m_find->setEnabled(false);
    m_find->setStatusTip(tr("Buscar"));
    m_find->setToolTip(tr("Buscar"));
    connect(m_find, &QAction::triggered, this, [this] { openFind(); });
}

QString ReferenceTool::statusText(Mode mode) {
    if (mode == Mode::None) return tr("Seleccione una opción del menú.");
    return tr("Herramientas - Asignar referencia | - LMB: Seleccione un elemento.");  // measured
}

void ReferenceTool::pruneViewports() {
    m_viewports.removeIf([](const QPointer<Viewport3D>& v) { return v.isNull(); });
}

int ReferenceTool::run(QDialog& dialog) { return m_runner ? m_runner(dialog) : dialog.exec(); }

void ReferenceTool::setProject(Project* project, UndoStack* undo) {
    deactivate();
    if (m_findPanel) m_findPanel->close();
    m_project = project;
    m_undo = undo;
    updateEnabled();
}

void ReferenceTool::addViewport(Viewport3D* viewport) {
    if (!viewport || m_viewports.contains(viewport)) return;
    m_viewports.push_back(viewport);
    connect(viewport, &Viewport3D::selectionChanged, this, [this, viewport] { onSelectionChanged(viewport); });
    connect(viewport, &Viewport3D::actOnSelection, this, [this, viewport] { onActOnSelection(viewport); });
    connect(viewport, &Viewport3D::escapePressed, this, [this] { deactivate(); });
    if (m_mode != Mode::None) configureViewports();
}

void ReferenceTool::updateEnabled() {
    // Greyed on an empty work, active with elements (measured).
    const bool elements = m_project && !m_project->structure.nodes.empty();
    m_assign->setEnabled(elements);
    m_find->setEnabled(elements);
    if (!elements && m_assign->isChecked()) m_assign->setChecked(false);
}

void ReferenceTool::structureChanged() {
    updateEnabled();
    pruneViewports();
    for (const QPointer<Viewport3D>& viewport : m_viewports) viewport->refreshStructure();
}

ElementKind ReferenceTool::elementKind() const {
    return m_panel && m_panel->kind() == ReferenceKind::Shell ? ElementKind::Shell : ElementKind::Node;
}

void ReferenceTool::start() {
    if (!m_project) return;
    emit aboutToActivate();
    m_mode = Mode::Assign;
    if (!m_panel) {
        m_panel = new ReferencePanel(m_dialogParent);
        m_panel->setAttribute(Qt::WA_DeleteOnClose);
        connect(m_panel, &ReferencePanel::kindChanged, this, [this] { configureViewports(); });
        connect(m_panel, &ReferencePanel::closed, this, [this] {
            if (m_assign->isChecked()) m_assign->setChecked(false);
        });
    }
    m_panel->show();
    configureViewports();
    emit modeChanged(m_mode);
    emit statusChanged(statusText(m_mode));
}

void ReferenceTool::deactivate() {
    if (m_assign->isChecked()) m_assign->setChecked(false);
    if (m_mode == Mode::None) return;
    m_mode = Mode::None;
    if (m_panel) m_panel->close();
    configureViewports();
    emit modeChanged(m_mode);
    emit statusChanged(statusText(m_mode));
}

void ReferenceTool::configureViewports() {
    pruneViewports();
    for (const QPointer<Viewport3D>& viewport : m_viewports) {
        viewport->setSelectionMode(false);
        viewport->setSnapMode(false);
        viewport->setRubberChain({});
        viewport->setRubberPoints({});
        viewport->setMovingNode(std::nullopt);
        // The nodes show their reference while the command is active (measured: they pass to N1..N4).
        viewport->setNodeLabelsVisible(m_mode != Mode::None);
        viewport->setCoordinateTooltipEnabled(true);
        if (m_mode == Mode::None) continue;
        viewport->setSelectionFilter(SelectionFilter::only(elementKind()));
        viewport->setSelectionMode(true);
        viewport->setFocus();
    }
}

void ReferenceTool::onSelectionChanged(Viewport3D* viewport) {
    if (m_internalSelection || m_mode != Mode::Assign || !m_panel || m_panel->multiple()) return;
    const std::vector<int> ids = viewport->selection().ids(elementKind());
    if (ids.empty()) return;
    m_internalSelection = true;  // Individual: the pick is consumed at once
    viewport->clearSelection();
    m_internalSelection = false;
    if (ids.size() == 1) assignOne(m_panel->kind(), ids.front());
}

void ReferenceTool::onActOnSelection(Viewport3D* viewport) {
    if (m_mode != Mode::Assign || !m_panel || !m_panel->multiple()) return;
    const std::vector<int> ids = viewport->selection().ids(elementKind());
    if (ids.empty()) return;
    assignMultiple(m_panel->kind(), ids);
    m_internalSelection = true;
    viewport->clearSelection();
    m_internalSelection = false;
}

bool ReferenceTool::assignOne(ReferenceKind kind, int id) {
    if (!m_project || !m_undo) return false;
    ReferenceTextDialog dialog(tr("Asignar referencia"), QString::fromStdString(elementReference(m_project->structure, kind, id)), m_dialogParent);
    if (run(dialog) != QDialog::Accepted) return false;
    const ReferenceError error = assignReferences(*m_project, *m_undo, kind, {id}, {dialog.text().toStdString()});
    if (error != ReferenceError::None && error != ReferenceError::NothingToChange)
        emit messageRequested(QString::fromStdString(referenceErrorMessage(error)));
    return error == ReferenceError::None;
}

bool ReferenceTool::assignMultiple(ReferenceKind kind, const std::vector<int>& idsIn) {
    if (!m_project || !m_undo || idsIn.empty()) return false;
    std::vector<int> ids = idsIn;
    std::sort(ids.begin(), ids.end());
    ReferenceTextDialog dialog(tr("Asignar referencia"), QString::fromStdString(elementReference(m_project->structure, kind, ids.front())), m_dialogParent);
    if (run(dialog) != QDialog::Accepted) return false;
    const ReferenceError error =
        assignReferences(*m_project, *m_undo, kind, ids, consecutiveReferences(dialog.text().toStdString(), ids.size()));
    if (error != ReferenceError::None && error != ReferenceError::NothingToChange)
        emit messageRequested(QString::fromStdString(referenceErrorMessage(error)));
    return error == ReferenceError::None;
}

void ReferenceTool::openFind() {
    if (!m_project) return;
    if (!m_findPanel) {
        m_findPanel = new FindPanel(m_dialogParent);
        m_findPanel->setAttribute(Qt::WA_DeleteOnClose);
        connect(m_findPanel, &FindPanel::changed, this, [this] {
            if (m_findPanel) search(m_findPanel->kind(), m_findPanel->text(), m_findPanel->wholeWord());
        });
    }
    m_findPanel->show();
    m_findPanel->raise();
    m_findPanel->textEdit()->setFocus();
}

int ReferenceTool::search(ReferenceKind kind, const QString& text, bool wholeWord) {
    if (!m_project) return 0;
    const std::vector<int> found = findByReference(m_project->structure, kind, text.toStdString(), wholeWord);
    std::vector<ElementRef> items;
    for (int id : found) items.push_back({kind == ReferenceKind::Node ? ElementKind::Node : ElementKind::Shell, id});
    pruneViewports();
    m_internalSelection = true;
    for (const QPointer<Viewport3D>& viewport : m_viewports) viewport->setSelectionItems(items);
    m_internalSelection = false;
    if (!text.trimmed().isEmpty())
        emit statusChanged(found.empty() ? tr("No se ha encontrado ningún elemento.") : tr("Se han encontrado %n elemento(s).", nullptr, static_cast<int>(found.size())));
    return static_cast<int>(found.size());
}

}  // namespace cc::app
