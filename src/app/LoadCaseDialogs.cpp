#include <QCoreApplication>
#include "LoadCaseDialogs.h"

#include <QCheckBox>
#include <QComboBox>
#include <QEvent>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QTableWidget>
#include <QToolButton>
#include <QVBoxLayout>
#include <algorithm>

#include "Icons.h"
#include "UserMessages.h"

namespace cc::app {

using core::ArrangementCompat;
using core::LoadCase;
using core::LoadCaseError;
using core::LoadNature;
using core::SeismicType;

LoadCaseState LoadCaseState::capture(const core::Project& project) {
    return {project.loadCases, project.nodalLoads, project.barLoads, project.shellLoads, project.panels, project.surfaceLoads, project.nodeSupports};
}

void LoadCaseState::restoreTo(core::Project& project) const {
    const int nextNodalId = std::max(project.nodalLoads.nextId, nodalLoads.nextId);  // ids are never reused, as in the undo commands
    project.loadCases = cases;
    project.nodalLoads = nodalLoads;
    project.nodalLoads.nextId = nextNodalId;
    core::restoreBarLoads(project.barLoads, barLoads);
    core::restoreShellLoads(project.shellLoads, shellLoads);
    core::restorePanels(project.panels, panels);
    core::restoreSurfaceLoads(project.surfaceLoads, surfaceLoads);
    project.nodeSupports = nodeSupports;
}

QString deleteLoadCaseQuestion(const QString& label) {
    return QCoreApplication::translate("cc::app::LoadCaseDialogs", "Se van a borrar todas las cargas definidas en la hipótesis.") + QStringLiteral(": '") + label +
           QStringLiteral("'. ") + QCoreApplication::translate("cc::app::LoadCaseDialogs", "¿Desea continuar?");
}

namespace {

QString qs(const std::string& text) { return QString::fromStdString(text); }

QString natureName(LoadNature nature) { return qs(core::loadNatureName(nature)); }

/// Flat icon button of the bars ("+", copy, X).
QToolButton* barButton(const QString& icon, const QString& tip, QWidget* parent) {
    auto* button = new QToolButton(parent);
    button->setIcon(themedIcon(icon));
    button->setIconSize(QSize(16, 16));
    button->setAutoRaise(true);
    button->setToolTip(tip);
    return button;
}

void showError(QWidget* parent, LoadCaseError error) {
    messages::show(parent, messages::Kind::Warning, qs(core::loadCaseErrorMessage(error)));
}

const QColor kRowGrey(0xE6, 0xE6, 0xE6);

/// Measured table look: header, alternating white / grey rows, no editing, no row numbers.
void styleTable(QTableWidget* table) {
    table->verticalHeader()->setVisible(false);
    table->verticalHeader()->setDefaultSectionSize(22);
    table->horizontalHeader()->setHighlightSections(false);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionMode(QAbstractItemView::NoSelection);
    table->setFocusPolicy(Qt::NoFocus);
    table->setShowGrid(true);
    table->setStyleSheet(QStringLiteral("QTableWidget { background: white; gridline-color: #D0D0D0; border: 1px solid #C8C8C8; }"
                                        "QTableWidget::item { border: none; padding: 0; }"
                                        "QTableWidget::item:selected { background: #3399FF; color: white; }"
                                        "QHeaderView::section { background: #F0F0F0; border: 1px solid #D0D0D0; padding: 2px; }"));
}

}  // namespace

QStringList useCategoryChoices(int normGroup) {
    if (normGroup == 0) return {QObject::tr("A. Domésticos y residenciales")};
    return {QObject::tr("A. Zonas residenciales"),
            QObject::tr("Zonas administrativas"),
            QObject::tr("Zonas de acceso al público"),
            QObject::tr("Zonas comerciales"),
            QObject::tr("Zonas de tráfico y aparcamiento para vehículos ligeros"),
            QObject::tr("Cubiertas accesibles únicamente para mantenimiento")};
}

// ---------------------------------------------------------------- Categoría

UseCategoryChoicesPanel::UseCategoryChoicesPanel(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);
    const QStringList headings[core::kUseCategoryNormGroups] = {
        {tr("Hormigón: Eurocódigo 2"), tr("Cimentación: Eurocódigo 2"), tr("Aluminio: Eurocódigo 9"),
         tr("Tensiones sobre el terreno: Eurocódigo 2")},
        {tr("Acero laminado: Código Estructural"), tr("Acero conformado: CTE DB SE-A"), tr("Madera: CTE DB SE-M")}};
    for (int g = 0; g < core::kUseCategoryNormGroups; ++g) {
        for (const QString& line : headings[g]) {
            auto* label = new QLabel(QStringLiteral("<b>%1</b>").arg(line.toHtmlEscaped()), this);
            layout->addWidget(label);
        }
        auto* combo = new QComboBox(this);
        combo->addItems(useCategoryChoices(g));
        combo->setMinimumHeight(24);
        layout->addWidget(combo);
        layout->addSpacing(6);
        m_combos[static_cast<std::size_t>(g)] = combo;
        connect(combo, &QComboBox::currentTextChanged, this, [this] { emit changed(); });
    }
}

std::array<std::string, core::kUseCategoryNormGroups> UseCategoryChoicesPanel::choices() const {
    std::array<std::string, core::kUseCategoryNormGroups> out;
    for (std::size_t g = 0; g < out.size(); ++g) out[g] = m_combos[g]->currentText().toStdString();
    return out;
}

void UseCategoryChoicesPanel::setChoices(const std::array<std::string, core::kUseCategoryNormGroups>& choices) {
    for (std::size_t g = 0; g < choices.size(); ++g) {
        const QSignalBlocker blocker(m_combos[g]);
        const int index = m_combos[g]->findText(qs(choices[g]));
        if (index >= 0) m_combos[g]->setCurrentIndex(index);
    }
}

UseCategoryDialog::UseCategoryDialog(QWidget* parent) : CypeDialog(tr("Categoría"), parent) {
    setFixedSize(660, 280);
    m_panel = new UseCategoryChoicesPanel(this);
    contentLayout()->setContentsMargins(8, 6, 8, 0);
    contentLayout()->addWidget(m_panel);
    contentLayout()->addStretch(1);
    setupFooter(tr("Aceptar"), tr("Cancelar"));
}

// ---------------------------------------------------------------- Categorías de uso

UseCategoriesDialog::UseCategoriesDialog(core::Project& project, core::UndoStack& stack, QWidget* parent)
    : CypeDialog(tr("Categorías de uso"), parent), m_project(project), m_stack(stack), m_initial(LoadCaseState::capture(project)) {
    setFixedSize(430, 320);
    auto* bar = new QHBoxLayout;
    bar->setContentsMargins(6, 4, 6, 0);
    bar->setSpacing(2);
    auto* add = barButton(QStringLiteral("hyp_plus"), tr("Añadir elemento a la lista"), this);
    m_copy = barButton(QStringLiteral("copy"), tr("Copiar elemento seleccionado de la lista"), this);
    m_delete = barButton(QStringLiteral("hyp_cross"), tr("Borrar / Suprimir elemento seleccionado de la lista"), this);
    bar->addWidget(add);
    bar->addWidget(m_copy);
    bar->addWidget(m_delete);
    bar->addStretch(1);
    contentLayout()->addLayout(bar);

    auto* body = new QHBoxLayout;
    body->setContentsMargins(6, 4, 6, 4);
    m_table = new QTableWidget(0, 2, this);
    m_table->setHorizontalHeaderLabels({tr("Ref."), tr("Descripción")});
    styleTable(m_table);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setFixedWidth(112);
    m_table->setColumnWidth(0, 32);
    m_table->horizontalHeader()->setStretchLastSection(true);
    body->addWidget(m_table);
    m_panel = new UseCategoryChoicesPanel(this);
    body->addWidget(m_panel, 1, Qt::AlignTop);
    contentLayout()->addLayout(body, 1);
    setupFooter(tr("Aceptar"), tr("Cancelar"));

    connect(add, &QToolButton::clicked, this, [this] {
        UseCategoryDialog dialog(this);
        if (dialog.exec() == QDialog::Accepted) addCategory(dialog.choices());
    });
    connect(m_copy, &QToolButton::clicked, this, [this] { copySelected(); });
    connect(m_delete, &QToolButton::clicked, this, [this] { deleteSelected(); });
    connect(m_table, &QTableWidget::itemSelectionChanged, this, [this] {
        if (m_refreshing) return;
        const int row = selectedRow();
        if (row >= 0) m_panel->setChoices(m_project.loadCases.useCategories[static_cast<std::size_t>(row)].choices);
    });
    connect(m_panel, &UseCategoryChoicesPanel::changed, this, [this] {
        const int row = selectedRow();
        if (row < 0 || m_refreshing) return;
        core::setUseCategoryChoices(m_project, m_stack, static_cast<std::size_t>(row), m_panel->choices());
        refresh(row);
    });
    refresh(m_project.loadCases.useCategories.empty() ? -1 : 0);
}

int UseCategoriesDialog::selectedRow() const {
    const auto rows = m_table->selectionModel()->selectedRows();
    return rows.isEmpty() ? -1 : rows.first().row();
}

void UseCategoriesDialog::refresh(int select) {
    m_refreshing = true;
    const auto& categories = m_project.loadCases.useCategories;
    m_table->setRowCount(static_cast<int>(categories.size()));
    for (int r = 0; r < static_cast<int>(categories.size()); ++r) {
        m_table->setItem(r, 0, new QTableWidgetItem(qs(categories[static_cast<std::size_t>(r)].ref)));
        m_table->setItem(r, 1, new QTableWidgetItem(qs(categories[static_cast<std::size_t>(r)].description)));
    }
    if (select >= static_cast<int>(categories.size())) select = static_cast<int>(categories.size()) - 1;
    if (select >= 0) m_table->selectRow(select);
    m_panel->setEnabled(select >= 0);
    m_copy->setEnabled(select >= 0);
    m_delete->setEnabled(select >= 0);
    if (select >= 0) m_panel->setChoices(categories[static_cast<std::size_t>(select)].choices);
    m_refreshing = false;
}

void UseCategoriesDialog::addCategory(const std::array<std::string, core::kUseCategoryNormGroups>& choices) {
    if (core::addUseCategory(m_project, m_stack, choices) == LoadCaseError::None)
        refresh(static_cast<int>(m_project.loadCases.useCategories.size()) - 1);
}

void UseCategoriesDialog::copySelected() {
    const int row = selectedRow();
    if (row < 0) return;
    if (core::copyUseCategory(m_project, m_stack, static_cast<std::size_t>(row)) == LoadCaseError::None)
        refresh(static_cast<int>(m_project.loadCases.useCategories.size()) - 1);
}

void UseCategoriesDialog::deleteSelected() {
    const int row = selectedRow();
    if (row < 0) return;
    if (core::deleteUseCategory(m_project, m_stack, static_cast<std::size_t>(row)) == LoadCaseError::None) refresh(row);
}

void UseCategoriesDialog::done(int result) {
    if (result != QDialog::Accepted) m_initial.restoreTo(m_project);  // Cancelar: nothing of this window stays
    CypeDialog::done(result);
}

// ---------------------------------------------------------------- Nueva / Editar hipótesis adicional

LoadCaseDataDialog::LoadCaseDataDialog(LoadNature nature, const QString& proposed, QWidget* parent)
    : CypeDialog(tr("Nueva hipótesis adicional"), parent), m_nature(nature) {
    init(tr("Nueva hipótesis adicional"), proposed, QString());
}

LoadCaseDataDialog::LoadCaseDataDialog(const LoadCase& existing, QWidget* parent)
    : CypeDialog(tr("Editar hipótesis adicional"), parent), m_nature(existing.nature), m_edit(true) {
    init(tr("Editar hipótesis adicional"), qs(existing.reference), qs(existing.description));
}

void LoadCaseDataDialog::init(const QString&, const QString& reference, const QString& description) {
    auto* grid = new QGridLayout;
    grid->setContentsMargins(8, 6, 8, 4);
    grid->setHorizontalSpacing(8);
    grid->addWidget(new QLabel(tr("Referencia"), this), 0, 0);
    m_reference = new QLineEdit(reference, this);
    m_reference->setToolTip(tr("Nombre corto que identificará la hipótesis en las referencias, listados de combinaciones y asignaciones de cargas."));
    grid->addWidget(m_reference, 0, 1);
    grid->addWidget(new QLabel(tr("Descripción"), this), 1, 0);
    m_description = new QLineEdit(description, this);
    m_description->setToolTip(tr("Texto opcional que describe la hipótesis."));
    grid->addWidget(m_description, 1, 1);
    grid->setColumnStretch(1, 1);
    contentLayout()->addLayout(grid);

    m_reference->installEventFilter(this);
    m_description->installEventFilter(this);

    const bool seismic = m_nature == LoadNature::Seismic;
    if (!m_edit && seismic) {
        m_seismicPanel = new QWidget(this);
        auto* box = new QVBoxLayout(m_seismicPanel);
        box->setContentsMargins(8, 4, 8, 4);
        box->setSpacing(2);
        auto* title = new QLabel(tr("<b>Tipo de sismo estático</b>"), m_seismicPanel);
        box->addWidget(title);
        m_seismic[0] = new QRadioButton(tr("Simple"), m_seismicPanel);
        m_seismic[1] = new QRadioButton(tr("XY"), m_seismicPanel);
        m_seismic[0]->setChecked(true);
        m_seismic[0]->setStyleSheet(QStringLiteral("QRadioButton { margin-left: 14px; }"));
        m_seismic[1]->setStyleSheet(QStringLiteral("QRadioButton { margin-left: 14px; }"));
        box->addWidget(m_seismic[0]);
        box->addWidget(m_seismic[1]);
        contentLayout()->addWidget(m_seismicPanel);
    }
    if (!m_edit && !seismic) {
        m_withArrangements = new QCheckBox(tr("Con distintas disposiciones de cargas"), this);
        m_withArrangements->setToolTip(tr("Permite definir varias disposiciones de la carga (por ejemplo, viento en direcciones distintas) con la combinatoria elegida."));
        auto* row = new QHBoxLayout;
        row->setContentsMargins(8, 0, 8, 0);
        row->addWidget(m_withArrangements);
        contentLayout()->addLayout(row);

        m_arrangementsPanel = new QWidget(this);
        auto* panel = new QVBoxLayout(m_arrangementsPanel);
        panel->setContentsMargins(8, 0, 8, 4);
        panel->setSpacing(2);
        auto* bar = new QHBoxLayout;
        bar->setSpacing(2);
        auto* add = barButton(QStringLiteral("hyp_plus"), tr("Añadir elemento a la lista"), m_arrangementsPanel);
        auto* del = barButton(QStringLiteral("hyp_cross"), tr("Borrar / Suprimir elemento seleccionado de la lista"), m_arrangementsPanel);
        bar->addWidget(add);
        bar->addWidget(del);
        bar->addStretch(1);
        panel->addLayout(bar);
        auto* body = new QHBoxLayout;
        m_table = new QTableWidget(0, 1, m_arrangementsPanel);
        m_table->setHorizontalHeaderLabels({tr("Nombre")});
        m_table->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        styleTable(m_table);
        m_table->setSelectionMode(QAbstractItemView::SingleSelection);
        m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
        m_table->horizontalHeader()->setStretchLastSection(true);
        m_table->setFixedHeight(100);
        body->addWidget(m_table, 1);
        auto* radios = new QVBoxLayout;
        const QString names[3] = {tr("Compatibles"), tr("Incompatibles"), tr("Simultáneas")};
        for (int i = 0; i < 3; ++i) {
            m_compat[static_cast<std::size_t>(i)] = new QRadioButton(names[i], m_arrangementsPanel);
            radios->addWidget(m_compat[static_cast<std::size_t>(i)]);
        }
        m_compat[static_cast<int>(ArrangementCompat::Simultaneous)]->setChecked(true);
        radios->addStretch(1);
        body->addLayout(radios);
        panel->addLayout(body);
        contentLayout()->addWidget(m_arrangementsPanel);
        m_arrangementsPanel->setVisible(false);

        for (int i = 1; i <= core::kDefaultArrangementCount; ++i) m_arrangements.push_back(std::to_string(i));
        fillArrangements();
        connect(m_withArrangements, &QCheckBox::toggled, this, [this](bool on) {
            m_arrangementsPanel->setVisible(on);
            resizeForState();
        });
        connect(add, &QToolButton::clicked, this, [this] { addArrangement(); });
        connect(del, &QToolButton::clicked, this, [this] { deleteSelectedArrangement(); });
    }
    contentLayout()->addStretch(1);
    setupFooter(tr("Aceptar"), tr("Cancelar"));
    resizeForState();
    m_reference->setFocus();
    m_reference->selectAll();
}

void LoadCaseDataDialog::resizeForState() {
    // Measured: 415x189 (reference + description), about 415x323 with the arrangements, 415x223 for the seismic type.
    int height = 189;
    if (m_arrangementsPanel && m_withArrangements->isChecked()) height = 323;
    if (m_seismicPanel) height = 223;
    setFixedSize(415, height);
}

void LoadCaseDataDialog::fillArrangements() {
    if (!m_table) return;
    m_table->setRowCount(static_cast<int>(m_arrangements.size()));
    for (int r = 0; r < static_cast<int>(m_arrangements.size()); ++r)
        m_table->setItem(r, 0, new QTableWidgetItem(qs(m_arrangements[static_cast<std::size_t>(r)])));
}

void LoadCaseDataDialog::addArrangement() {
    if (!m_table) return;
    int highest = 0;  // same rule as core::addLoadArrangement: the next number after the highest one
    for (const std::string& n : m_arrangements) highest = std::max(highest, QString::fromStdString(n).toInt());
    m_arrangements.push_back(std::to_string(highest + 1));
    fillArrangements();
    m_table->selectRow(static_cast<int>(m_arrangements.size()) - 1);  // the new one stays selected
    m_table->scrollToBottom();
}

void LoadCaseDataDialog::deleteSelectedArrangement() {
    if (!m_table || m_arrangements.size() <= 1) return;  // the core keeps at least one entry
    const auto rows = m_table->selectionModel()->selectedRows();
    if (rows.isEmpty()) return;
    const int row = rows.first().row();
    m_arrangements.erase(m_arrangements.begin() + row);
    fillArrangements();
    m_table->selectRow(std::min(row, static_cast<int>(m_arrangements.size()) - 1));
}

std::string LoadCaseDataDialog::reference() const { return m_reference->text().toStdString(); }
std::string LoadCaseDataDialog::description() const { return m_description->text().toStdString(); }
bool LoadCaseDataDialog::withArrangements() const { return m_withArrangements && m_withArrangements->isChecked(); }

ArrangementCompat LoadCaseDataDialog::compat() const {
    for (int i = 0; i < 3; ++i)
        if (m_compat[static_cast<std::size_t>(i)] && m_compat[static_cast<std::size_t>(i)]->isChecked()) return static_cast<ArrangementCompat>(i);
    return ArrangementCompat::Simultaneous;
}

SeismicType LoadCaseDataDialog::seismic() const {
    return m_seismic[1] && m_seismic[1]->isChecked() ? SeismicType::XY : SeismicType::Simple;
}

core::NewLoadCase LoadCaseDataDialog::value() const {
    core::NewLoadCase data;
    data.nature = m_nature;
    data.reference = reference();
    data.description = description();
    data.withArrangements = withArrangements();
    data.compat = compat();
    data.seismic = seismic();
    return data;
}

void LoadCaseDataDialog::accept() {
    if (m_reference->text().isEmpty()) {  // same condition as the core
        showError(this, LoadCaseError::BlankReference);
        m_reference->setFocus();
        return;
    }
    CypeDialog::accept();
}

bool LoadCaseDataDialog::eventFilter(QObject* watched, QEvent* event) {
    // Measured: Intro acts as Tab (Referencia -> Descripción -> Aceptar).
    if (event->type() == QEvent::KeyPress && (watched == m_reference || watched == m_description)) {
        const int key = static_cast<QKeyEvent*>(event)->key();
        if (key == Qt::Key_Return || key == Qt::Key_Enter) {
            if (watched == m_reference) {
                m_description->setFocus();
                m_description->selectAll();
            } else {
                acceptButton()->setFocus();
            }
            return true;
        }
    }
    return CypeDialog::eventFilter(watched, event);
}

// ---------------------------------------------------------------- Ventana por naturaleza

LoadNatureDialog::LoadNatureDialog(LoadNature nature, core::Project& project, core::UndoStack& stack, QWidget* parent)
    : CypeDialog(natureName(nature), parent), m_nature(nature), m_project(project), m_stack(stack), m_initial(LoadCaseState::capture(project)) {
    setMaximizable();
    const bool seismic = nature == LoadNature::Seismic;
    resize(seismic ? 308 : 304, seismic ? 320 : 281);
    contentLayout()->setContentsMargins(4, 4, 4, 0);

    auto* link = new QPushButton(themedIcon(QStringLiteral("hyp_new")), tr("Nueva hipótesis adicional"), this);
    link->setFlat(true);
    link->setCursor(Qt::PointingHandCursor);
    link->setStyleSheet(QStringLiteral("QPushButton { border: none; background: transparent; text-align: left; padding: 3px 4px; }"
                                       "QPushButton:hover { background: #E8EEF7; }"));
    link->setToolTip(tr("Define una nueva hipótesis adicional de esta naturaleza."));
    contentLayout()->addWidget(link, 0, Qt::AlignLeft);

    const int columns = seismic ? 5 : 4;
    m_table = new QTableWidget(0, columns, this);
    QStringList headers{tr("Actúa"), tr("Hipótesis adicionales")};
    if (seismic) headers << tr("Tipo");
    headers << tr("Editar") << tr("Borrar");
    m_table->setHorizontalHeaderLabels(headers);
    styleTable(m_table);
    auto* header = m_table->horizontalHeader();
    header->setSectionResizeMode(0, QHeaderView::Fixed);
    header->setSectionResizeMode(1, QHeaderView::Stretch);
    m_table->setColumnWidth(0, 42);
    for (int c = 2; c < columns; ++c) {
        header->setSectionResizeMode(c, QHeaderView::Fixed);
        m_table->setColumnWidth(c, c == 2 && seismic ? 42 : 40);
    }
    m_table->setIconSize(QSize(16, 16));
    contentLayout()->addWidget(m_table, 1);

    if (seismic) {
        m_secondOrder = new QCheckBox(tr("Considerar efectos de segundo orden"), this);
        m_secondOrder->setChecked(m_project.loadCases.seismicSecondOrder);
        auto* row = new QHBoxLayout;
        row->setContentsMargins(4, 2, 4, 2);
        row->addWidget(m_secondOrder);
        contentLayout()->addLayout(row);
        connect(m_secondOrder, &QCheckBox::toggled, this, [this](bool on) {
            core::setSeismicSecondOrder(m_project, m_stack, on);
        });
    }
    setupFooter(tr("Aceptar"), tr("Cancelar"));
    connect(link, &QPushButton::clicked, this, [this] { createCase(); });
    connect(m_table, &QTableWidget::cellClicked, this, &LoadNatureDialog::onCellClicked);
    refresh();
}

void LoadNatureDialog::refresh() {
    const auto cases = core::loadCasesOfNature(m_project.loadCases, m_nature);
    const bool seismic = m_nature == LoadNature::Seismic;
    const int rows = std::max<int>(5, static_cast<int>(cases.size()));  // measured: five empty rows when nothing is defined
    m_table->clearContents();
    m_table->setRowCount(rows);
    const int editColumn = columnEdit(m_nature);
    for (int r = 0; r < rows; ++r) {
        const bool filled = r < static_cast<int>(cases.size());
        m_table->setRowHeight(r, filled && !cases[static_cast<std::size_t>(r)]->description.empty() ? 40 : 24);
        for (int c = 0; c < m_table->columnCount(); ++c) {
            auto* item = new QTableWidgetItem;
            item->setBackground(r % 2 == 0 ? QColor(Qt::white) : kRowGrey);
            item->setFlags(Qt::ItemIsEnabled);
            m_table->setItem(r, c, item);
        }
        if (!filled) continue;
        const LoadCase& lc = *cases[static_cast<std::size_t>(r)];
        auto* acts = m_table->item(r, 0);
        acts->setIcon(themedIcon(lc.acts ? QStringLiteral("hyp_check") : QStringLiteral("hyp_cross")));
        acts->setToolTip(lc.acts ? tr("Actúa: la hipótesis se considera en el cálculo. Pulse para cambiarlo.")
                                 : tr("No actúa: la hipótesis no se considera en el cálculo. Pulse para cambiarlo."));
        acts->setTextAlignment(Qt::AlignCenter);
        QString text = qs(lc.reference);
        if (!lc.description.empty()) text += QStringLiteral("\n  ") + qs(lc.description);
        auto* hypothesis = m_table->item(r, 1);
        hypothesis->setText(text);
        hypothesis->setData(Qt::UserRole, lc.id);
        if (seismic) m_table->item(r, 2)->setText(lc.seismic == SeismicType::XY ? tr("XY") : tr("Simple"));
        m_table->item(r, editColumn)->setIcon(themedIcon(QStringLiteral("edit_pencil")));
        m_table->item(r, editColumn + 1)->setIcon(themedIcon(QStringLiteral("hyp_cross")));
    }
}

int LoadNatureDialog::caseIdOfRow(int row) const {
    if (row < 0 || row >= m_table->rowCount() || !m_table->item(row, 1)) return -1;
    const QVariant id = m_table->item(row, 1)->data(Qt::UserRole);
    return id.isValid() ? id.toInt() : -1;
}

void LoadNatureDialog::onCellClicked(int row, int column) {
    if (caseIdOfRow(row) < 0) return;
    if (column == 0) toggleRow(row);
    else if (column == columnEdit(m_nature)) editRow(row);
    else if (column == columnEdit(m_nature) + 1) deleteRow(row);
}

bool LoadNatureDialog::createCase() {
    LoadCaseDataDialog dialog(m_nature, qs(core::proposedLoadReference(m_project.loadCases, m_nature)), this);
    if (dialog.exec() != QDialog::Accepted) return false;
    const core::NewLoadCase data = dialog.value();
    m_stack.beginMacro("Nueva hipótesis adicional");
    int id = 0;
    const LoadCaseError error = core::createLoadCase(m_project, m_stack, data, &id);
    if (error == LoadCaseError::None && data.withArrangements && dialog.arrangements() != m_project.loadCases.cases.back().arrangements) {
        core::LoadCaseSet after = m_project.loadCases;  // the names edited with + / X in the dialog replace the default "1", "2"
        after.cases.back().arrangements = dialog.arrangements();
        m_stack.push(std::make_unique<core::LoadCaseCommand>(m_project, std::move(after), "Disposiciones de cargas"));
    }
    m_stack.endMacro();
    if (error != LoadCaseError::None) {
        showError(this, error);
        return false;
    }
    refresh();
    return true;
}

void LoadNatureDialog::editRow(int row) {
    const int id = caseIdOfRow(row);
    const LoadCase* lc = id >= 0 ? core::findLoadCase(m_project.loadCases, id) : nullptr;
    if (!lc) return;
    LoadCaseDataDialog dialog(*lc, this);
    if (dialog.exec() != QDialog::Accepted) return;
    const LoadCaseError error = core::editLoadCase(m_project, m_stack, id, dialog.reference(), dialog.description());
    if (error != LoadCaseError::None && error != LoadCaseError::NothingToChange) showError(this, error);
    refresh();
}

void LoadNatureDialog::toggleRow(int row) {
    const int id = caseIdOfRow(row);
    if (id < 0) return;
    core::toggleLoadCaseActs(m_project, m_stack, id);
    refresh();
}

void LoadNatureDialog::deleteRow(int row) {
    const int id = caseIdOfRow(row);
    if (id < 0) return;
    // Without loads it is immediate (measured); with them the original asks first (#64905).
    if (!core::loadCaseDependents(m_project, id).empty()) {
        // Measured (cargas_laminas.md 13.6): «Se van a borrar todas las cargas definidas en la hipótesis.: 'CM 1'. ¿Desea continuar?» (the
        // «.:» is literal), Sí / No; No keeps the hypothesis and its loads.
        const core::LoadCase* hypothesis = core::findLoadCase(m_project.loadCases, id);
        const QString label = hypothesis ? QString::fromStdString(core::loadCaseLabel(*hypothesis)) : QString();
        if (!messages::show(this, messages::Kind::Question, deleteLoadCaseQuestion(label))) return;
    }
    const LoadCaseError error = core::deleteLoadCase(m_project, m_stack, id);
    if (error != LoadCaseError::None) showError(this, error);
    refresh();
}

void LoadNatureDialog::done(int result) {
    if (result != QDialog::Accepted) m_initial.restoreTo(m_project);  // Cancelar
    CypeDialog::done(result);
}

// ---------------------------------------------------------------- Hipótesis adicionales

AdditionalLoadsDialog::AdditionalLoadsDialog(const core::LoadCaseSet& initial, QWidget* parent)
    : CypeDialog(tr("Hipótesis adicionales"), parent) {
    m_project.loadCases = initial;
    setFixedSize(280, 443);
    contentLayout()->setContentsMargins(8, 6, 8, 0);
    contentLayout()->setSpacing(4);
    const auto band = [this](const QString& text) {  // grey section band of the original
        auto* label = new QLabel(text, this);
        label->setStyleSheet(QStringLiteral("QLabel { background: #E6E6E6; padding: 3px 6px; }"));
        return label;
    };
    contentLayout()->addWidget(band(tr("Categorías de uso")));
    auto* usesLayout = new QHBoxLayout;
    usesLayout->setContentsMargins(4, 2, 4, 2);
    m_categoriesLabel = new QLabel(this);
    usesLayout->addWidget(m_categoriesLabel, 1);
    m_categoriesEdit = barButton(QStringLiteral("edit_pencil"), tr("Definir las categorías de uso de la obra"), this);
    usesLayout->addWidget(m_categoriesEdit);
    contentLayout()->addLayout(usesLayout);
    contentLayout()->addSpacing(6);
    contentLayout()->addWidget(band(tr("Acciones")));

    auto* actions = new QWidget(this);
    auto* grid = new QGridLayout(actions);
    grid->setContentsMargins(2, 2, 0, 2);
    grid->setVerticalSpacing(5);
    grid->setHorizontalSpacing(0);
    grid->setColumnMinimumWidth(0, 112);  // measured: the values sit under the headers at x = 159 (automatic) and 215 (additional)
    grid->setColumnMinimumWidth(1, 70);
    grid->setColumnMinimumWidth(2, 46);
    grid->setColumnMinimumWidth(3, 24);
    grid->addWidget(new QLabel(tr("<b>Automáticas</b>"), actions), 0, 1, Qt::AlignCenter);
    grid->addWidget(new QLabel(tr("<b>Adicionales</b>"), actions), 0, 2, 1, 2, Qt::AlignLeft);
    for (int n = 0; n < core::kLoadNatureCount; ++n) {
        const auto nature = static_cast<LoadNature>(n);
        const int r = n + 1;
        grid->addWidget(new QLabel(natureName(nature), actions), r, 0);
        // Automatic column: only the self weight (1); the rest show "-" (no automatic cases in 3D with these norms, measured).
        grid->addWidget(new QLabel(nature == LoadNature::SelfWeight ? QStringLiteral("1") : QStringLiteral("-"), actions), r, 1, Qt::AlignCenter);
        auto* counter = new QLabel(actions);
        m_counters[static_cast<std::size_t>(n)] = counter;
        grid->addWidget(counter, r, 2, Qt::AlignCenter);
        if (core::loadNatureAcceptsAdditional(nature)) {
            auto* edit = barButton(QStringLiteral("edit_pencil"), tr("Editar las hipótesis adicionales de %1").arg(natureName(nature).toLower()), actions);
            m_edit[static_cast<std::size_t>(n)] = edit;
            grid->addWidget(edit, r, 3);
            connect(edit, &QToolButton::clicked, this, [this, nature] { openNature(nature); });
        }
    }
    grid->setColumnStretch(0, 1);
    contentLayout()->addWidget(actions);
    contentLayout()->addStretch(1);
    setupFooter(tr("Aceptar"), tr("Cancelar"));
    connect(m_categoriesEdit, &QToolButton::clicked, this, [this] { openCategories(); });
    refresh();
}

void AdditionalLoadsDialog::refresh() {
    const auto& set = m_project.loadCases;
    QString text = tr("No hay categorías de uso definidas");
    if (!set.useCategories.empty()) {
        QStringList names;
        for (const core::UseCategory& u : set.useCategories) names << qs(u.description);  // TODO(medir): text with several categories
        text = names.join(QStringLiteral(", "));
    }
    m_categoriesLabel->setText(text);
    for (int n = 0; n < core::kLoadNatureCount; ++n) {
        const auto nature = static_cast<LoadNature>(n);
        m_counters[static_cast<std::size_t>(n)]->setText(nature == LoadNature::SelfWeight
                                                             ? QStringLiteral("---")
                                                             : QString::number(core::loadCasesOfNature(set, nature).size()));
    }
}

void AdditionalLoadsDialog::openCategories() {
    UseCategoriesDialog dialog(m_project, m_stack, this);
    dialog.exec();
    refresh();
}

void AdditionalLoadsDialog::openNature(LoadNature nature) {
    if (nature == LoadNature::LiveLoad && m_project.loadCases.useCategories.empty()) {
        // Measured: the notice (#39206), then "Categorías de uso" opens; the window of the nature only with categories defined.
        showError(this, LoadCaseError::NeedsUseCategories);
        openCategories();
        if (m_project.loadCases.useCategories.empty()) return;
    }
    LoadNatureDialog dialog(nature, m_project, m_stack, this);
    dialog.exec();
    refresh();
}

}  // namespace cc::app
