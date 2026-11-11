#include "UserSituationsDialogs.h"

#include <QComboBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QRadioButton>
#include <QTableWidget>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <algorithm>

#include "ClickTooltip.h"
#include "Icons.h"
#include "RangedEdit.h"
#include "UserMessages.h"

namespace cc::app {

using namespace cc::core;

namespace {

QString qs(const std::string& s) { return QString::fromStdString(s); }

QToolButton* barButton(const QString& icon, const QString& text, QWidget* parent) {
    auto* b = new QToolButton(parent);
    b->setIcon(themedIcon(icon));
    b->setIconSize(QSize(16, 16));
    b->setAutoRaise(true);
    ClickTooltip::attach(b, text);  // the tooltips of the original appear after a click, not while resting
    return b;
}

/// Coefficient cell: selects its whole text on entering (measured) and shows "Debe estar entre 0.000 y 10.000 ." as tooltip.
class CoefficientEdit : public RangedEdit {
public:
    CoefficientEdit(const QString& label, const ValueRange& range, QWidget* parent) : RangedEdit(label, range, 3, parent) {
        setMaximumWidth(16777215);
        setFixedWidth(62);
        setToolTip(tr("Debe estar entre %1 y %2 .").arg(QString::number(range.min, 'f', 3), QString::number(range.max, 'f', 3)));
    }

protected:
    void focusInEvent(QFocusEvent* event) override {
        RangedEdit::focusInEvent(event);
        QTimer::singleShot(0, this, [this] { selectAll(); });
    }
};

/// Shows the message of an error: "Error" (red icon) for the empty reference and the empty selection, "Aviso" for the rest.
void showError(QWidget* parent, SituationError e) {
    messages::show(parent, situationErrorIsError(e) ? messages::Kind::Error : messages::Kind::Warning,
                   QObject::tr(situationErrorMessage(e)));
}

void warn(QWidget* parent, const QString& text) { messages::show(parent, messages::Kind::Warning, text); }

/// Extra height of the windows per use category beyond the first (one more "Sobrecarga (Uso n)" row) [N: pitch of the grid rows].
constexpr int kCategoryRowPitch = 28;

}  // namespace

// ------------------------------------------------------------------------------ Tabla de coeficientes de una situación

SituationEditor::SituationEditor(std::size_t categories, CoefficientMode mode, bool service, QWidget* parent)
    : QWidget(parent),
      m_situation(UserSituation::withDefaults(categories)),
      m_categories(categories),
      m_both(!service && mode == CoefficientMode::PartialAndCombination),
      m_service(service) {
    if (service) m_situation.type = SituationType::Service;
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(6);

    m_type = new QComboBox(this);
    for (SituationType t : {SituationType::PersistentOrTransient, SituationType::Accidental, SituationType::Seismic,
                            SituationType::PrestressInVacuum})
        m_type->addItem(tr(situationTypeCaption(t)), static_cast<int>(t));
    m_type->setFixedWidth(210);
    if (service) m_type->hide();  // E.L.S. situations have no type combo [V, research 14.4]
    else root->addWidget(m_type);

    auto* grid = new QGridLayout;
    grid->setHorizontalSpacing(8);
    grid->setVerticalSpacing(6);
    const auto heading = [this, grid](const QString& text, int column, int span) {
        auto* label = new QLabel(text, this);
        label->setAlignment(Qt::AlignHCenter | Qt::AlignBottom);
        label->setWordWrap(true);
        grid->addWidget(label, 0, column, 1, span);
    };
    heading(tr("Coeficientes parciales de seguridad"), 1, 2);
    if (m_both) heading(tr("Coeficientes de combinación"), 3, 2);
    const QString columnTitles[] = {tr("Favorable"), tr("Desfavorable"), tr("Principal"), tr("Acompañamiento")};
    for (int c = 0; c < (m_both ? 4 : 2); ++c) {
        auto* label = new QLabel(columnTitles[c], this);
        label->setAlignment(Qt::AlignHCenter);
        grid->addWidget(label, 1, c + 1);
    }

    struct RowSpec {
        QString label;
        SituationAction action;
    };
    std::vector<RowSpec> specs = {{tr("Carga permanente"), SituationAction::Permanent},
                                  {tr("Pretensado"), SituationAction::Prestress},
                                  {tr("Retracción"), SituationAction::Shrinkage}};
    for (std::size_t i = 0; i < categories; ++i) specs.push_back({tr("Sobrecarga (Uso %1)").arg(i + 1), SituationAction::Imposed});
    specs.push_back({tr("Viento"), SituationAction::Wind});
    specs.push_back({tr("Nieve"), SituationAction::Snow});
    specs.push_back({tr("Temperatura"), SituationAction::Temperature});
    specs.push_back({tr("Empujes del terreno"), SituationAction::EarthThrust});

    const ValueRange coefficientRange{kMinCoefficient, kMaxCoefficient};
    for (std::size_t r = 0; r < specs.size(); ++r) {
        grid->addWidget(new QLabel(specs[r].label, this), static_cast<int>(r) + 2, 0);
        std::vector<RangedEdit*> cells(4, nullptr);
        for (int c = 0; c < (m_both ? 4 : 2); ++c) {
            const bool enabled = c < 2 || hasCombinationFactors(specs[r].action);
            if (!enabled) {
                auto* dash = new QLabel(QStringLiteral("—"), this);
                dash->setAlignment(Qt::AlignCenter);
                dash->setStyleSheet(QStringLiteral("color: #B0B0B0;"));
                grid->addWidget(dash, static_cast<int>(r) + 2, c + 1);
                continue;
            }
            auto* edit = new CoefficientEdit(specs[r].label, coefficientRange, this);
            grid->addWidget(edit, static_cast<int>(r) + 2, c + 1);
            cells[static_cast<std::size_t>(c)] = edit;
        }
        m_cells.push_back(cells);
    }

    // "Sismo" row of the seismic situations [V, research 14.1]: favourable -1.000, unfavourable 1.000, principal 1.000 (fixed) and the
    // fraction of the orthogonal direction in "Acompañamiento" (0..1) marked with (*). In the E.L.U. grid the cells sit in their columns;
    // with partial factors only [N] the fraction takes the third column.
    const int seismicRow = static_cast<int>(specs.size()) + 2;
    std::vector<RangedEdit*> seismic(4, nullptr);
    auto* seismicLabel = new QLabel(tr("Sismo"), this);
    grid->addWidget(seismicLabel, seismicRow, 0);
    m_seismicWidgets.push_back(seismicLabel);
    const ValueRange seismicRange{-kMaxSeismicCoefficient, kMaxSeismicCoefficient};
    for (int c = 0; c < 2; ++c) {
        auto* edit = new CoefficientEdit(tr("Sismo"), seismicRange, this);
        grid->addWidget(edit, seismicRow, c + 1);
        seismic[static_cast<std::size_t>(c)] = edit;
        m_seismicWidgets.push_back(edit);
    }
    if (m_both) {
        auto* fixed = new QLabel(QStringLiteral("1.000"), this);
        fixed->setAlignment(Qt::AlignCenter);
        grid->addWidget(fixed, seismicRow, 3);
        m_seismicWidgets.push_back(fixed);
    }
    auto* fraction = new CoefficientEdit(tr("Sismo"), ValueRange{kMinOrthogonalFraction, kMaxOrthogonalFraction}, this);
    const int fractionColumn = m_both ? 4 : 3;
    grid->addWidget(fraction, seismicRow, fractionColumn);
    seismic[3] = fraction;
    m_seismicWidgets.push_back(fraction);
    auto* star = new QLabel(QStringLiteral("(*)"), this);
    grid->addWidget(star, seismicRow, fractionColumn + 1);
    m_seismicWidgets.push_back(star);
    m_cells.push_back(seismic);
    auto* note = new QLabel(tr("(*) Porcentaje de las solicitaciones producidas por el sismo en la dirección ortogonal"), this);
    note->setWordWrap(true);
    grid->addWidget(note, seismicRow + 1, 0, 1, fractionColumn + 2);
    m_seismicWidgets.push_back(note);
    grid->setColumnStretch(0, 1);

    auto* centred = new QHBoxLayout;  // with partial factors only the omitted columns leave the table centred [V]
    centred->addStretch(m_both ? 0 : 1);
    centred->addLayout(grid, m_both ? 1 : 0);
    centred->addStretch(m_both ? 0 : 1);
    root->addLayout(centred);
    root->addStretch(1);

    connect(m_type, &QComboBox::currentIndexChanged, this, [this] { updateSeismicRow(); });
    setSituation(m_situation);
}

bool SituationEditor::seismicRowVisible() const { return !m_service && m_type->currentData().toInt() == static_cast<int>(SituationType::Seismic); }

void SituationEditor::updateSeismicRow() {
    const bool visible = seismicRowVisible();
    for (QWidget* w : m_seismicWidgets) w->setVisible(visible);
}

void SituationEditor::setSituation(const UserSituation& situation) {
    m_situation = situation;
    if (m_service) m_situation.type = SituationType::Service;
    m_situation.imposed.resize(m_categories, UserSituation::withDefaults(1).imposed.front());
    if (!m_service) m_type->setCurrentIndex(std::max(0, m_type->findData(static_cast<int>(situation.type))));
    UserSituation working = m_situation;
    const auto rows = working.rows();
    for (std::size_t r = 0; r < rows.size(); ++r) {
        const double values[4] = {rows[r]->favourable, rows[r]->unfavourable, rows[r]->principal, rows[r]->accompanying};
        for (int c = 0; c < 4; ++c)
            if (RangedEdit* e = m_cells[r][static_cast<std::size_t>(c)]) e->setValue(values[c]);
    }
    const SituationRow& seismic = m_situation.seismic;
    m_cells.back()[0]->setValue(seismic.favourable);
    m_cells.back()[1]->setValue(seismic.unfavourable);
    m_cells.back()[3]->setValue(seismic.accompanying);
    updateSeismicRow();
}

UserSituation SituationEditor::situation() const {
    UserSituation s = m_situation;
    s.type = m_service ? SituationType::Service : static_cast<SituationType>(m_type->currentData().toInt());
    const auto rows = s.rows();
    for (std::size_t r = 0; r < rows.size(); ++r) {
        if (m_cells[r][0]) rows[r]->favourable = m_cells[r][0]->value();
        if (m_cells[r][1]) rows[r]->unfavourable = m_cells[r][1]->value();
        if (m_cells[r][2]) rows[r]->principal = m_cells[r][2]->value();
        if (m_cells[r][3]) rows[r]->accompanying = m_cells[r][3]->value();
    }
    s.seismic.favourable = m_cells.back()[0]->value();
    s.seismic.unfavourable = m_cells.back()[1]->value();
    s.seismic.accompanying = m_cells.back()[3]->value();
    return s;
}

bool SituationEditor::commitCells() {
    for (const auto& row : m_cells)
        for (RangedEdit* cell : row)
            if (cell && !cell->commit()) return false;
    return true;
}

std::vector<QWidget*> SituationEditor::tabChain() const {
    std::vector<QWidget*> chain;
    if (!m_service) chain.push_back(m_type);
    for (std::size_t r = 0; r < m_cells.size(); ++r) {
        if (r + 1 == m_cells.size() && !seismicRowVisible()) break;
        for (RangedEdit* cell : m_cells[r])
            if (cell) chain.push_back(cell);
    }
    return chain;
}

RangedEdit* SituationEditor::cell(int row, int column) const {
    if (row < 0 || row >= static_cast<int>(m_cells.size()) || column < 0 || column > 3) return nullptr;
    return m_cells[static_cast<std::size_t>(row)][static_cast<std::size_t>(column)];
}

// ------------------------------------------------------------------------------ Situación de proyecto definida por el usuario

SituationEditDialog::SituationEditDialog(const UserSituation& initial, std::size_t categories, CoefficientMode mode,
                                         const std::vector<std::string>& otherNames, const std::vector<std::string>& categoryNames,
                                         QWidget* parent)
    : CypeDialog(tr("Situación de proyecto definida por el usuario"), parent),
      m_initial(initial),
      m_mode(initial.type == SituationType::Service ? CoefficientMode::PartialFactors : mode),
      m_otherNames(otherNames) {
    (void)categoryNames;
    const bool service = initial.type == SituationType::Service;
    // Measured: 488x451 for E.L.U. (with or without combination factors) and 475x357 without type combo for E.L.S.; one category.
    const int extra = kCategoryRowPitch * (static_cast<int>(categories) - 1);
    setFixedSize(service ? 475 : 488, (service ? 357 : 451) + extra);
    QVBoxLayout* body = contentLayout();
    body->setSpacing(6);

    auto* nameRow = new QHBoxLayout;
    nameRow->addWidget(new QLabel(tr("Nombre de la situación"), this));
    m_name = new QLineEdit(qs(initial.name), this);
    nameRow->addWidget(m_name, 1);
    body->addLayout(nameRow);
    auto* line = new QFrame(this);
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);
    body->addWidget(line);

    m_editor = new SituationEditor(categories, m_mode, service, this);
    m_editor->setSituation(initial);
    body->addWidget(m_editor, 1);
    setupFooter(tr("Aceptar"), tr("Cancelar"));
    // Tab order: name, then the editor (type, grid row by row, disabled cells skipped); the footer follows.
    QWidget* previous = m_name;
    for (QWidget* w : m_editor->tabChain()) {
        setTabOrder(previous, w);
        previous = w;
    }
    setTabOrder(previous, acceptButton());
    setTabOrder(acceptButton(), cancelButton());
}

std::vector<QWidget*> SituationEditDialog::tabChain() const { return m_editor->tabChain(); }

UserSituation SituationEditDialog::situation() const {
    UserSituation s = m_editor->situation();
    s.name = m_name->text().toStdString();
    return s;
}

SituationError SituationEditDialog::check() {
    if (!m_editor->commitCells()) return SituationError::OutOfRange;  // the cell already warned and recovered its value
    const UserSituation s = situation();
    SituationError error = SituationError::None;
    if (s.name.empty()) error = SituationError::EmptyName;
    else if (std::find(m_otherNames.begin(), m_otherNames.end(), s.name) != m_otherNames.end()) error = SituationError::DuplicateName;
    else error = validateSituation(s, m_mode);
    if (error != SituationError::None) showError(this, error);
    return error;
}

void SituationEditDialog::accept() {
    if (check() == SituationError::None) CypeDialog::accept();
}

// ------------------------------------------------------------------------------ Coeficientes de combinación

CoefficientModeDialog::CoefficientModeDialog(CoefficientMode initial, QWidget* parent)
    : CypeDialog(tr("Coeficientes de combinación"), parent) {
    setFixedSize(350, 207);
    auto* body = contentLayout();
    body->setSpacing(4);
    m_partial = new QRadioButton(tr("Introducir coeficientes de mayoración"), this);
    m_both = new QRadioButton(tr("Introducir coeficientes de mayoración y combinación"), this);
    for (QRadioButton* r : {m_partial, m_both}) r->setStyleSheet(QStringLiteral("font-weight: bold;"));
    const auto formula = [this](const QString& html) {
        auto* l = new QLabel(html, this);
        l->setTextFormat(Qt::RichText);
        l->setContentsMargins(24, 0, 0, 0);
        return l;
    };
    body->addWidget(m_partial);
    body->addWidget(formula(QStringLiteral("∑<sub>j≥1</sub> γ<sub>G,j</sub>G<sub>k,j</sub> + γ<sub>P</sub>P<sub>k</sub> + ∑<sub>i≥1</sub> γ<sub>Q,i</sub>Q<sub>k,i</sub>")));
    body->addWidget(m_both);
    body->addWidget(formula(QStringLiteral(
        "∑<sub>j≥1</sub> γ<sub>G,j</sub>G<sub>k,j</sub> + γ<sub>P</sub>P<sub>k</sub> + γ<sub>Q,1</sub>Ψ<sub>p,1</sub>Q<sub>k,1</sub> + ∑<sub>i&gt;1</sub> γ<sub>Q,i</sub>Ψ<sub>a,i</sub>Q<sub>k,i</sub>")));
    (initial == CoefficientMode::PartialFactors ? m_partial : m_both)->setChecked(true);
    body->addStretch(1);
    setupFooter(tr("Aceptar"), tr("Cancelar"));
}

CoefficientMode CoefficientModeDialog::mode() const {
    return m_both->isChecked() ? CoefficientMode::PartialAndCombination : CoefficientMode::PartialFactors;
}

// ------------------------------------------------------------------------------ Categorías de uso

SituationCategoriesDialog::SituationCategoriesDialog(const std::vector<std::string>& initial, QWidget* parent)
    : CypeDialog(tr("Categorías de uso"), parent), m_initial(initial) {
    const bool offerExisting = !initial.empty();
    setFixedSize(offerExisting ? 260 : 180, offerExisting ? 280 : 222);
    if (offerExisting) {  // radios of research 14.4 (#39081 / #39080): the already defined categories are the default
        m_new = new QRadioButton(tr("Definir nuevas categorías de uso"), this);
        m_existing = new QRadioButton(tr("Categorías de uso ya definidas"), this);
        for (QRadioButton* r : {m_new, m_existing}) {
            r->setStyleSheet(QStringLiteral("font-weight: bold;"));
            contentLayout()->addWidget(r);
        }
        m_existing->setChecked(true);
    }
    auto* bar = new QWidget(this);
    auto* barLayout = new QHBoxLayout(bar);
    barLayout->setContentsMargins(0, 0, 0, 0);
    barLayout->setSpacing(2);
    auto* add = barButton(QStringLiteral("hyp_plus"), tr("Nuevo"), this);
    auto* copy = barButton(QStringLiteral("copy"), tr("Copiar"), this);
    auto* del = barButton(QStringLiteral("hyp_cross"), tr("Borrar"), this);
    for (QToolButton* b : {add, copy, del}) barLayout->addWidget(b);
    barLayout->addStretch(1);
    contentLayout()->addWidget(bar);
    m_bar = bar;
    m_table = new QTableWidget(0, 1, this);
    m_table->setHorizontalHeaderLabels({tr("Categoría de uso")});
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->verticalHeader()->setVisible(false);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed | QAbstractItemView::AnyKeyPressed);
    contentLayout()->addWidget(m_table, 1);
    setupFooter(tr("Aceptar"), tr("Cancelar"));
    for (std::size_t i = 0; i < initial.size(); ++i) appendRow(qs(initial[i]), static_cast<int>(i));
    connect(add, &QToolButton::clicked, this, &SituationCategoriesDialog::addRow);
    connect(copy, &QToolButton::clicked, this, &SituationCategoriesDialog::copyRow);
    connect(del, &QToolButton::clicked, this, &SituationCategoriesDialog::deleteRow);
    if (offerExisting) {
        connect(m_existing, &QRadioButton::toggled, this, [this] { updateEnabled(); });
        updateEnabled();
    }
}

void SituationCategoriesDialog::updateEnabled() {
    const bool editable = m_new && m_new->isChecked();
    m_bar->setEnabled(editable);
    m_table->setEnabled(editable);
}

void SituationCategoriesDialog::appendRow(const QString& text, int origin) {
    const int row = m_table->rowCount();
    m_table->insertRow(row);
    auto* item = new QTableWidgetItem(text);
    item->setData(Qt::UserRole, origin);
    m_table->setItem(row, 0, item);
}

void SituationCategoriesDialog::addRow() {
    appendRow(QString(), -1);
    m_table->selectRow(m_table->rowCount() - 1);
    m_table->setFocus();
    m_table->editItem(m_table->item(m_table->rowCount() - 1, 0));
}

void SituationCategoriesDialog::copyRow() {
    const int row = m_table->currentRow();
    if (row < 0) return;
    appendRow(m_table->item(row, 0)->text(), m_table->item(row, 0)->data(Qt::UserRole).toInt());
    m_table->selectRow(m_table->rowCount() - 1);
}

void SituationCategoriesDialog::deleteRow() {
    const int row = m_table->currentRow();
    if (row < 0) return;
    m_table->removeRow(row);
    if (m_table->rowCount() > 0) m_table->selectRow(std::min(row, m_table->rowCount() - 1));
}

std::vector<std::string> SituationCategoriesDialog::categories() const {
    if (m_existing && m_existing->isChecked()) return m_initial;
    std::vector<std::string> out;
    for (int r = 0; r < m_table->rowCount(); ++r) out.push_back(m_table->item(r, 0)->text().toStdString());
    return out;
}

std::vector<int> SituationCategoriesDialog::origin() const {
    std::vector<int> out;
    if (m_existing && m_existing->isChecked()) {
        for (std::size_t i = 0; i < m_initial.size(); ++i) out.push_back(static_cast<int>(i));
        return out;
    }
    for (int r = 0; r < m_table->rowCount(); ++r) out.push_back(m_table->item(r, 0)->data(Qt::UserRole).toInt());
    return out;
}

void SituationCategoriesDialog::accept() {
    if (categories().empty()) {  // empty table + Aceptar: Aviso "La lista no puede quedar vacía." [V, research 14.4]
        warn(this, tr(situationErrorMessage(SituationError::NoSituations)));
        return;
    }
    CypeDialog::accept();
}

// ------------------------------------------------------------------------------ Crear - [Situaciones]

SituationSetDialog::SituationSetDialog(const UserSituationSet& initial, const std::vector<std::string>& otherReferences, bool creating,
                                       QWidget* parent)
    : CypeDialog(creating ? tr("Crear - [Situaciones]") : tr("Editar - [Situaciones]"), parent), m_set(initial) {
    (void)otherReferences;  // the original does not look for repeated references here, only when accepting the limit state (#94676)
    auto* refRow = new QHBoxLayout;
    refRow->addWidget(new QLabel(tr("Referencia"), this));
    m_reference = new QLineEdit(qs(initial.reference), this);
    refRow->addWidget(m_reference, 1);
    contentLayout()->addLayout(refRow);

    auto* bar = new QHBoxLayout;
    bar->setSpacing(2);
    m_add = barButton(QStringLiteral("hyp_plus"), tr("Nuevo"), this);  // [N] by the neighbouring strings #74721 "Nuevo" / #74722 "Borrar"
    m_copy = barButton(QStringLiteral("copy"), tr("Copiar"), this);
    m_delete = barButton(QStringLiteral("hyp_cross"), tr("Borrar"), this);
    for (QToolButton* b : {m_add, m_copy, m_delete}) bar->addWidget(b);
    bar->addStretch(1);
    contentLayout()->addLayout(bar);

    auto* bodyLayout = new QHBoxLayout;
    m_body = bodyLayout;
    m_listColumn = new QWidget(this);
    auto* column = new QVBoxLayout(m_listColumn);
    column->setContentsMargins(0, 0, 0, 0);
    column->addWidget(new QLabel(tr("Situaciones"), m_listColumn));
    m_list = new QListWidget(m_listColumn);
    m_list->setEditTriggers(QAbstractItemView::DoubleClicked);  // double click renames in the list [V]
    column->addWidget(m_list, 1);
    bodyLayout->addWidget(m_listColumn, 1);
    contentLayout()->addLayout(bodyLayout, 1);
    setupFooter(tr("Aceptar"), tr("Cancelar"));
    connect(m_add, &QToolButton::clicked, this, &SituationSetDialog::addSituation);
    connect(m_copy, &QToolButton::clicked, this, &SituationSetDialog::copySituation);
    connect(m_delete, &QToolButton::clicked, this, &SituationSetDialog::deleteSituation);
    // Selecting another row stores the edited one and loads the selected situation into the editor.
    connect(m_list, &QListWidget::currentRowChanged, this, [this](int current) {
        if (m_refreshing || !m_editor) return;
        const int previous = m_loadedRow;
        if (previous >= 0 && previous < static_cast<int>(m_set.situations.size())) {
            UserSituation s = m_editor->situation();
            s.name = m_set.situations[static_cast<std::size_t>(previous)].name;
            m_set.situations[static_cast<std::size_t>(previous)] = s;
        }
        if (current >= 0 && current < static_cast<int>(m_set.situations.size()))
            m_editor->setSituation(m_set.situations[static_cast<std::size_t>(current)]);
        m_loadedRow = current;
    });
    connect(m_list, &QListWidget::itemChanged, this, [this](QListWidgetItem* item) {
        if (m_refreshing) return;
        const int row = m_list->row(item);
        if (row >= 0 && row < static_cast<int>(m_set.situations.size())) m_set.situations[static_cast<std::size_t>(row)].name = item->text().toStdString();
    });
    refresh();
}

void SituationSetDialog::updateSize() {
    if (!m_editor) {
        setFixedSize(406, 355);
        m_listColumn->setMaximumWidth(QWIDGETSIZE_MAX);
        return;
    }
    const int extra = kCategoryRowPitch * (static_cast<int>(m_set.categories.size()) - 1);
    setFixedSize(m_set.service ? 406 : 566, (m_set.service ? 380 : 475) + extra);  // measured [V, research 14.4]
    m_listColumn->setFixedWidth(80);  // narrow list on the left (about 75 px in the original, capture c14_98)
}

void SituationSetDialog::rebuildEditor() {
    if (m_editor) {
        m_editor->deleteLater();
        m_editor = nullptr;
    }
    if (m_set.situations.empty()) return;
    m_editor = new SituationEditor(m_set.categories.size(), m_set.mode, m_set.service, this);
    static_cast<QHBoxLayout*>(m_body)->addWidget(m_editor, 1);
    m_editor->show();
    // Tab order: reference, bar, list, then the editor, then the footer.
    QWidget* previous = m_list;
    for (QWidget* w : m_editor->tabChain()) {
        setTabOrder(previous, w);
        previous = w;
    }
    setTabOrder(previous, acceptButton());
}

void SituationSetDialog::commitEditor() {
    const int row = m_list->currentRow();
    if (!m_editor || row < 0 || row >= static_cast<int>(m_set.situations.size())) return;
    UserSituation s = m_editor->situation();
    s.name = m_set.situations[static_cast<std::size_t>(row)].name;
    m_set.situations[static_cast<std::size_t>(row)] = s;
}

void SituationSetDialog::refresh(int select) {
    m_refreshing = true;
    m_list->clear();
    for (const UserSituation& s : m_set.situations) {
        auto* item = new QListWidgetItem(qs(s.name));
        item->setFlags(item->flags() | Qt::ItemIsEditable);
        m_list->addItem(item);
    }
    if (m_set.situations.empty() != (m_editor == nullptr)) rebuildEditor();
    if (!m_set.situations.empty()) {
        if (select < 0 || select >= m_list->count()) select = 0;
        m_list->setCurrentRow(select);
        if (m_editor) m_editor->setSituation(m_set.situations[static_cast<std::size_t>(select)]);
    }
    m_loadedRow = m_editor ? m_list->currentRow() : -1;
    m_refreshing = false;
    updateSize();
}

UserSituationSet SituationSetDialog::set() const {
    UserSituationSet out = m_set;
    out.reference = m_reference->text().toStdString();
    const int row = m_list->currentRow();
    if (m_editor && row >= 0 && row < static_cast<int>(out.situations.size())) {
        UserSituation s = m_editor->situation();
        s.name = out.situations[static_cast<std::size_t>(row)].name;
        out.situations[static_cast<std::size_t>(row)] = s;
    }
    return out;
}

void SituationSetDialog::addSituation() {
    commitEditor();
    if (m_set.situations.empty()) {  // the first one fixes the kind of coefficients and the use categories of the set
        CoefficientMode mode = CoefficientMode::PartialFactors;
        if (!m_set.service) {  // an E.L.S. set has no combination factors: no such question [N]
            CoefficientModeDialog modeDialog(m_set.mode, this);
            if (modeDialog.exec() != QDialog::Accepted) return;
            mode = modeDialog.mode();
        }
        SituationCategoriesDialog categories(m_set.categories.empty() ? m_existingCategories : m_set.categories, this);
        if (categories.exec() != QDialog::Accepted) return;
        m_set.mode = mode;
        m_set.remapCategories(categories.categories(), categories.origin());
    }
    std::vector<std::string> names;
    for (const UserSituation& s : m_set.situations) names.push_back(s.name);
    UserSituation start = UserSituation::withDefaults(m_set.categories.size());
    if (m_set.service) start.type = SituationType::Service;
    SituationEditDialog dialog(start, m_set.categories.size(), m_set.mode, names, m_set.categories, this);
    if (dialog.exec() != QDialog::Accepted) return;
    m_set.situations.push_back(dialog.situation());
    refresh(static_cast<int>(m_set.situations.size()) - 1);
}

void SituationSetDialog::editSituation(int row) {
    if (row < 0 || row >= static_cast<int>(m_set.situations.size())) return;
    commitEditor();
    std::vector<std::string> names;
    for (int i = 0; i < static_cast<int>(m_set.situations.size()); ++i)
        if (i != row) names.push_back(m_set.situations[static_cast<std::size_t>(i)].name);
    SituationEditDialog dialog(m_set.situations[static_cast<std::size_t>(row)], m_set.categories.size(), m_set.mode, names, m_set.categories, this);
    if (dialog.exec() != QDialog::Accepted) return;
    m_set.situations[static_cast<std::size_t>(row)] = dialog.situation();
    refresh(row);
}

void SituationSetDialog::copySituation() {
    commitEditor();
    const int row = m_list->currentRow();
    if (row < 0) {
        warn(this, tr("Debe seleccionar una situación de proyecto."));
        return;
    }
    UserSituation copy = m_set.situations[static_cast<std::size_t>(row)];
    copy.name = tr("Copia de %1").arg(qs(copy.name)).toStdString();  // "Copia de %s" is a string of the original (#74963)
    m_set.situations.push_back(copy);
    refresh(static_cast<int>(m_set.situations.size()) - 1);
}

void SituationSetDialog::deleteSituation() {
    commitEditor();
    const int row = m_list->currentRow();
    if (row < 0) {
        warn(this, tr("Debe seleccionar una situación de proyecto."));
        return;
    }
    if (!messages::show(this, messages::Kind::Question, tr("¿Desea borrar la situación de proyecto seleccionada?"))) return;
    m_set.situations.erase(m_set.situations.begin() + row);
    refresh(std::min(row, static_cast<int>(m_set.situations.size()) - 1));
}

SituationError SituationSetDialog::check() {
    if (m_editor && !m_editor->commitCells()) return SituationError::OutOfRange;
    SetChecks checks;
    checks.projectHasSeismic = m_projectHasSeismic;
    // Order of the original [V, research 14.1]: empty reference, empty list, no persistent situation, no seismic situation.
    const SituationError error = validateSet(set(), {}, checks);
    if (error != SituationError::None) showError(this, error);
    return error;
}

void SituationSetDialog::accept() {
    if (check() == SituationError::None) CypeDialog::accept();
}

}  // namespace cc::app
