#include "DiaphragmTool.h"

#include <QAbstractItemView>
#include <QAction>
#include <QColorDialog>
#include <QComboBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPixmap>
#include <QRadioButton>
#include <QSplitter>
#include <QStyle>
#include <QStyleOptionViewItem>
#include <QStyledItemDelegate>
#include <QTableWidget>
#include <QToolButton>
#include <QVBoxLayout>

#include "UserMessages.h"
#include "Viewport3D.h"
#include "core/node_diaphragm_edit.h"

namespace cc::app {

using namespace cc::core;

namespace {

std::function<bool(QWidget*, unsigned&)>& colourPickerHook() {
    static std::function<bool(QWidget*, unsigned&)> hook;
    return hook;
}

/// Toolbar glyphs of the original lists: black plus, grey "duplicate" sheets, red cross.
QIcon glyph(int kind) {
    constexpr int size = 20;
    QPixmap pm(size, size);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    switch (kind) {
        case 0:
            p.setPen(QPen(Qt::black, 2.2, Qt::SolidLine, Qt::FlatCap));
            p.drawLine(QPointF(10, 3.5), QPointF(10, 16.5));
            p.drawLine(QPointF(3.5, 10), QPointF(16.5, 10));
            break;
        case 1:
            p.setPen(QPen(QColor(120, 120, 120), 1.4));
            p.setBrush(QColor(235, 235, 235));
            p.drawRect(QRectF(3.5, 3.5, 9, 9));
            p.drawRect(QRectF(7.5, 7.5, 9, 9));
            break;
        default:
            p.setPen(QPen(QColor(205, 25, 25), 2.6, Qt::SolidLine, Qt::FlatCap));
            p.drawLine(QPointF(4.5, 4.5), QPointF(15.5, 15.5));
            p.drawLine(QPointF(15.5, 4.5), QPointF(4.5, 15.5));
            break;
    }
    return QIcon(pm);
}

QToolButton* listButton(QWidget* parent, int kind, const QString& tip) {
    auto* b = new QToolButton(parent);
    b->setIcon(glyph(kind));
    b->setIconSize(QSize(20, 20));
    b->setFixedSize(24, 24);
    b->setAutoRaise(true);
    b->setToolTip(tip);
    return b;
}

QWidget* groupHeader(const QString& title, QWidget* parent) {
    auto* band = new QWidget(parent);
    band->setStyleSheet(QStringLiteral("background:#E8E8E8;"));
    auto* row = new QHBoxLayout(band);
    row->setContentsMargins(6, 2, 6, 2);
    row->addWidget(new QLabel(title, band), 1);
    return band;
}

void setBold(QWidget* widget, bool bold) {
    QFont font = widget->font();
    font.setBold(bold);
    widget->setFont(font);
}

QString threeDecimals(double v) { return QString::number(v, 'f', 3); }

QTableWidgetItem* cell(const QString& text, bool editable) {
    auto* item = new QTableWidgetItem(text);
    item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    if (!editable) item->setFlags(item->flags() & ~Qt::ItemIsEditable);
    return item;
}

/// Paints the colour cell with its colour even when the row is selected (the selection highlight would hide it).
class ColourDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        painter->fillRect(option.rect.adjusted(1, 1, -1, -1), QColor::fromRgb(index.data(Qt::UserRole).toUInt()));
        if (option.state & QStyle::State_Selected) {
            painter->setPen(QColor(0x3A, 0x8F, 0xD8));
            painter->drawRect(option.rect.adjusted(1, 1, -2, -2));
        }
    }
};

QString tipText(const QString& title, const QString& text) { return title + QStringLiteral(" / ") + text; }

}  // namespace

// ---- "Editar" dialog --------------------------------------------------------------------------------------------------------

QString DiaphragmEditDialog::loadCaseName(int loadCaseId) {
    Q_UNUSED(loadCaseId)
    return tr("Peso propio");
}

void DiaphragmEditDialog::setColourPicker(std::function<bool(QWidget*, unsigned&)> picker) { colourPickerHook() = std::move(picker); }

DiaphragmEditDialog::DiaphragmEditDialog(const std::vector<Diaphragm>& initial, const LoadCaseSet& cases, QWidget* parent)
    : DiaphragmEditDialog(initial, parent) {
    m_cases = cases;
    for (Diaphragm& d : m_rows)
        for (DiaphragmLoad& load : d.loads)
            if (!findLoadCase(cases, load.loadCaseId) && !cases.cases.empty()) load.loadCaseId = cases.cases.front().id;
    refreshRight();
}

DiaphragmEditDialog::DiaphragmEditDialog(const std::vector<Diaphragm>& initial, QWidget* parent)
    : CypeDialog(tr("Editar"), parent), m_rows(initial) {
    setMaximizable();
    setMinimumSize(697, 520);
    resize(697, 520);
    auto* body = new QWidget(this);
    auto* outer = new QVBoxLayout(body);
    outer->setContentsMargins(8, 6, 8, 6);
    outer->setSpacing(4);
    auto* top = new QHBoxLayout();
    top->setSpacing(0);
    m_add = listButton(body, 0, tipText(tr("Añadir"), tr("Añadir nuevo elemento a la lista")));
    m_duplicate = listButton(body, 1, tipText(tr("Duplicar"), tr("Duplicar el elemento seleccionado de la lista")));
    m_remove = listButton(body, 2, tipText(tr("Borrar"), tr("Suprimir elemento seleccionado de la lista")));
    for (QToolButton* b : {m_add, m_duplicate, m_remove}) top->addWidget(b);
    top->addStretch(1);
    outer->addLayout(top);

    auto* splitter = new QSplitter(Qt::Horizontal, body);
    splitter->setChildrenCollapsible(false);
    m_table = new QTableWidget(0, 2, splitter);
    m_table->setObjectName(QStringLiteral("diaphragmTable"));
    m_table->setHorizontalHeaderLabels({tr("Referencia"), tr("Color")});
    m_table->verticalHeader()->hide();
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
    m_table->setColumnWidth(1, 60);
    m_table->setMinimumWidth(150);
    m_table->setItemDelegateForColumn(1, new ColourDelegate(m_table));
    m_table->setStyleSheet(QStringLiteral("QTableWidget{background:white;} QTableWidget::item:selected{background:#CDE6F7;color:black;}"));

    m_rightPane = new QWidget(splitter);
    auto* right = new QVBoxLayout(m_rightPane);
    right->setContentsMargins(6, 0, 0, 0);
    right->setSpacing(6);
    auto* referenceRow = new QHBoxLayout();
    referenceRow->addWidget(new QLabel(tr("Referencia"), m_rightPane));
    m_reference = new QLineEdit(m_rightPane);
    m_reference->setObjectName(QStringLiteral("diaphragmReference"));
    referenceRow->addWidget(m_reference, 1);
    right->addLayout(referenceRow);

    m_loadGroup = new QWidget(m_rightPane);
    auto* group = new QVBoxLayout(m_loadGroup);
    group->setContentsMargins(0, 0, 0, 0);
    group->setSpacing(2);
    group->addWidget(groupHeader(tr("Cargas introducidas"), m_loadGroup));
    auto* loadBar = new QHBoxLayout();
    loadBar->setContentsMargins(6, 0, 0, 0);
    loadBar->setSpacing(0);
    m_addLoad = listButton(m_loadGroup, 0, tipText(tr("Añadir"), tr("Añadir nuevo elemento a la lista")));
    m_duplicateLoad = listButton(m_loadGroup, 1, tipText(tr("Duplicar"), tr("Duplicar el elemento seleccionado de la lista")));
    m_removeLoad = listButton(m_loadGroup, 2, tipText(tr("Borrar"), tr("Suprimir elemento seleccionado de la lista")));
    for (QToolButton* b : {m_addLoad, m_duplicateLoad, m_removeLoad}) loadBar->addWidget(b);
    loadBar->addStretch(1);
    group->addLayout(loadBar);
    m_loadTable = new QTableWidget(0, 5, m_loadGroup);
    m_loadTable->setObjectName(QStringLiteral("diaphragmLoadTable"));
    m_loadTable->setHorizontalHeaderLabels({tr("Hipótesis"), tr("Fx (kN)"), tr("Fy (kN)"), tr("Coord. x (m)"), tr("Coord. y (m)")});
    m_loadTable->verticalHeader()->hide();
    m_loadTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_loadTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_loadTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    group->addWidget(m_loadTable, 1);
    right->addWidget(m_loadGroup, 1);
    splitter->addWidget(m_table);
    splitter->addWidget(m_rightPane);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({160, 520});
    outer->addWidget(splitter, 1);
    contentLayout()->setContentsMargins(0, 0, 0, 0);
    contentLayout()->addWidget(body, 1);
    setupFooter(tr("Aceptar"), tr("Cancelar"));

    connect(m_add, &QToolButton::clicked, this, [this] { addRow(); });
    connect(m_duplicate, &QToolButton::clicked, this, [this] { duplicateRow(); });
    connect(m_remove, &QToolButton::clicked, this, [this] { removeRow(); });
    connect(m_addLoad, &QToolButton::clicked, this, [this] { addLoad(); });
    connect(m_duplicateLoad, &QToolButton::clicked, this, [this] { duplicateLoad(); });
    connect(m_removeLoad, &QToolButton::clicked, this, [this] { removeLoad(); });
    connect(m_table, &QTableWidget::currentCellChanged, this, [this](int, int, int, int) { refreshRight(); });
    connect(m_table, &QTableWidget::cellClicked, this, [this](int row, int column) {
        if (column == 1) pickColour(row);
    });
    connect(m_reference, &QLineEdit::textEdited, this, [this](const QString& text) {
        const int row = currentRow();
        if (row < 0) return;
        m_rows[static_cast<std::size_t>(row)].reference = text.toStdString();
        if (auto* item = m_table->item(row, 0)) item->setText(text);
    });
    connect(m_loadTable, &QTableWidget::cellChanged, this, [this](int row, int column) { onLoadChanged(row, column); });
    refreshTable(m_rows.empty() ? -1 : 0);
}

int DiaphragmEditDialog::currentRow() const {
    const int row = m_table->currentRow();
    return row >= 0 && row < static_cast<int>(m_rows.size()) ? row : -1;
}

void DiaphragmEditDialog::refreshTable(int selectRow) {
    m_loading = true;
    m_table->setRowCount(static_cast<int>(m_rows.size()));
    for (int r = 0; r < static_cast<int>(m_rows.size()); ++r) {
        const Diaphragm& d = m_rows[static_cast<std::size_t>(r)];
        auto* reference = new QTableWidgetItem(QString::fromStdString(d.reference));
        reference->setFlags(reference->flags() & ~Qt::ItemIsEditable);
        m_table->setItem(r, 0, reference);
        auto* colour = new QTableWidgetItem();
        colour->setFlags(colour->flags() & ~Qt::ItemIsEditable);
        colour->setBackground(QColor::fromRgb(d.color));
        colour->setData(Qt::UserRole, d.color);
        m_table->setItem(r, 1, colour);
    }
    m_loading = false;
    if (selectRow >= 0 && selectRow < m_table->rowCount()) m_table->setCurrentCell(selectRow, 0);
    refreshRight();
}

void DiaphragmEditDialog::refreshRight() {
    const int row = currentRow();
    m_rightPane->setEnabled(row >= 0);
    m_loading = true;
    m_reference->setText(row >= 0 ? QString::fromStdString(m_rows[static_cast<std::size_t>(row)].reference) : QString());
    m_loading = false;
    refreshLoads(-1);
}

void DiaphragmEditDialog::refreshLoads(int selectRow) {
    const int row = currentRow();
    m_loading = true;
    m_loadTable->setRowCount(0);
    if (row >= 0) {
        const std::vector<DiaphragmLoad>& loads = m_rows[static_cast<std::size_t>(row)].loads;
        m_loadTable->setRowCount(static_cast<int>(loads.size()));
        for (int r = 0; r < static_cast<int>(loads.size()); ++r) {
            const DiaphragmLoad& load = loads[static_cast<std::size_t>(r)];
            auto* combo = new QComboBox(m_loadTable);  // "hipótesis": a combo in the cell (measured)
            if (m_cases) {
                for (const LoadCase& c : m_cases->cases) combo->addItem(QString::fromStdString(loadCaseLabel(c)), c.id);
                combo->setCurrentIndex(std::max(0, combo->findData(load.loadCaseId)));
            } else {
                combo->addItem(loadCaseName(load.loadCaseId), load.loadCaseId);
            }
            connect(combo, &QComboBox::currentIndexChanged, this, [this, combo](int) {
                for (int rr = 0; rr < m_loadTable->rowCount(); ++rr)
                    if (m_loadTable->cellWidget(rr, 0) == combo) onLoadChanged(rr, 0);
            });
            m_loadTable->setCellWidget(r, 0, combo);
            m_loadTable->setItem(r, 1, cell(threeDecimals(load.fxKn), true));
            m_loadTable->setItem(r, 2, cell(threeDecimals(load.fyKn), true));
            m_loadTable->setItem(r, 3, cell(threeDecimals(load.xM), true));
            m_loadTable->setItem(r, 4, cell(threeDecimals(load.yM), true));
        }
    }
    m_loading = false;
    if (selectRow >= 0 && selectRow < m_loadTable->rowCount()) m_loadTable->selectRow(selectRow);
}

void DiaphragmEditDialog::onLoadChanged(int row, int column) {
    if (m_loading) return;
    const int d = currentRow();
    if (d < 0) return;
    std::vector<DiaphragmLoad>& loads = m_rows[static_cast<std::size_t>(d)].loads;
    if (row < 0 || row >= static_cast<int>(loads.size())) return;
    DiaphragmLoad& load = loads[static_cast<std::size_t>(row)];
    if (column == 0) {
        if (auto* combo = qobject_cast<QComboBox*>(m_loadTable->cellWidget(row, 0))) load.loadCaseId = combo->currentData().toInt();
        return;
    }
    QTableWidgetItem* item = m_loadTable->item(row, column);
    if (!item) return;
    bool ok = false;
    const double value = QString(item->text()).replace(',', '.').toDouble(&ok);
    double* target[5] = {nullptr, &load.fxKn, &load.fyKn, &load.xM, &load.yM};
    if (ok && target[column]) *target[column] = value;
    m_loading = true;  // always rewrite the cell with the stored value (3 decimals)
    item->setText(threeDecimals(*target[column]));
    m_loading = false;
}

bool DiaphragmEditDialog::addRow() {
    if (const auto issue = checkDiaphragmAdd(m_rows)) {
        messages::show(this, messages::Kind::Warning, QString::fromStdString(issue->text));
        return false;
    }
    m_rows.push_back(newDiaphragm());
    refreshTable(static_cast<int>(m_rows.size()) - 1);
    return true;
}

void DiaphragmEditDialog::duplicateRow() {
    const int row = currentRow();
    if (row < 0) return;
    m_rows.push_back(duplicateDiaphragm(m_rows[static_cast<std::size_t>(row)]));
    refreshTable(static_cast<int>(m_rows.size()) - 1);
}

void DiaphragmEditDialog::removeRow() {
    const int row = currentRow();
    if (row < 0) return;
    m_rows.erase(m_rows.begin() + row);
    refreshTable(m_rows.empty() ? -1 : std::min(row, static_cast<int>(m_rows.size()) - 1));
}

bool DiaphragmEditDialog::addLoad() {
    const int row = currentRow();
    if (row < 0) return false;
    std::vector<DiaphragmLoad>& loads = m_rows[static_cast<std::size_t>(row)].loads;
    loads.push_back(defaultDiaphragmLoad(newLoadCaseId()));
    refreshLoads(static_cast<int>(loads.size()) - 1);
    return true;
}

void DiaphragmEditDialog::duplicateLoad() {
    const int row = currentRow();
    const int load = m_loadTable->currentRow();
    if (row < 0 || load < 0) return;
    std::vector<DiaphragmLoad>& loads = m_rows[static_cast<std::size_t>(row)].loads;
    if (load >= static_cast<int>(loads.size())) return;
    loads.push_back(loads[static_cast<std::size_t>(load)]);
    refreshLoads(static_cast<int>(loads.size()) - 1);
}

void DiaphragmEditDialog::removeLoad() {
    const int row = currentRow();
    const int load = m_loadTable->currentRow();
    if (row < 0 || load < 0) return;
    std::vector<DiaphragmLoad>& loads = m_rows[static_cast<std::size_t>(row)].loads;
    if (load >= static_cast<int>(loads.size())) return;
    loads.erase(loads.begin() + load);
    refreshLoads(loads.empty() ? -1 : std::min(load, static_cast<int>(loads.size()) - 1));
}

void DiaphragmEditDialog::setColour(int row, unsigned rgb) {
    if (row < 0 || row >= static_cast<int>(m_rows.size())) return;
    m_rows[static_cast<std::size_t>(row)].color = rgb;
    if (auto* item = m_table->item(row, 1)) {
        item->setBackground(QColor::fromRgb(rgb));
        item->setData(Qt::UserRole, rgb);
    }
}

void DiaphragmEditDialog::pickColour(int row) {
    if (row < 0 || row >= static_cast<int>(m_rows.size())) return;
    unsigned rgb = m_rows[static_cast<std::size_t>(row)].color;
    if (colourPickerHook()) {
        if (!colourPickerHook()(this, rgb)) return;
    } else {
        QColorDialog picker(QColor::fromRgb(rgb), this);
        picker.setWindowTitle(tr("Selección del color"));
        if (picker.exec() != QDialog::Accepted) return;
        rgb = picker.selectedColor().rgb() & 0xFFFFFFu;
    }
    setColour(row, rgb & 0xFFFFFFu);
}

void DiaphragmEditDialog::accept() {
    if (const auto issue = checkDiaphragmAccept(m_rows)) {
        messages::show(this, messages::Kind::Warning, QString::fromStdString(issue->text));
        return;
    }
    CypeDialog::accept();
}

// ---- panel ------------------------------------------------------------------------------------------------------------------

DiaphragmPanel::DiaphragmPanel(QWidget* parent) : CypeDialog(tr("Propiedades - Asignar"), parent) {
    setModal(false);
    setMaximizable();
    contentLayout()->setContentsMargins(0, 0, 0, 0);
    contentLayout()->setSpacing(0);
    auto* options = new QVBoxLayout();
    options->setContentsMargins(6, 4, 6, 6);
    options->setSpacing(2);
    m_assign = new QRadioButton(tr("Asignar"), this);
    m_remove = new QRadioButton(tr("Eliminar asignación"), this);
    m_assign->setChecked(true);
    setBold(m_assign, true);
    options->addWidget(m_assign);
    options->addWidget(m_remove);
    contentLayout()->addLayout(options);
    auto* line = new QFrame(this);
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);
    contentLayout()->addWidget(line);
    m_comboRow = new QWidget(this);
    auto* row = new QHBoxLayout(m_comboRow);
    row->setContentsMargins(6, 10, 0, 4);
    row->addWidget(new QLabel(tr("Diafragma"), m_comboRow));
    row->addStretch(1);
    m_combo = new QComboBox(m_comboRow);
    m_combo->setMinimumWidth(48);
    row->addWidget(m_combo);
    contentLayout()->addWidget(m_comboRow);
    contentLayout()->addStretch(1);
    connect(m_assign, &QRadioButton::toggled, this, [this](bool on) {
        setBold(m_assign, on);
        setBold(m_remove, !on);
        m_comboRow->setVisible(on);  // "Eliminar asignación" hides the combo (measured)
    });
    setMinimumSize(270, 360);
}

bool DiaphragmPanel::assign() const { return m_assign->isChecked(); }
int DiaphragmPanel::diaphragmId() const { return m_combo->currentIndex() < 0 ? 0 : m_combo->currentData().toInt(); }
bool DiaphragmPanel::comboVisible() const { return !m_comboRow->isHidden(); }

void DiaphragmPanel::setDiaphragms(const Diaphragms& diaphragms) {
    const int keep = diaphragmId();
    m_combo->clear();
    for (const Diaphragm& d : diaphragms.items) m_combo->addItem(QString::fromStdString(d.reference), d.id);
    const int index = m_combo->findData(keep);
    m_combo->setCurrentIndex(index >= 0 ? index : (m_combo->count() > 0 ? 0 : -1));
}

void DiaphragmPanel::done(int result) {
    CypeDialog::done(result);
    emit closed();
}

// ---- tool -------------------------------------------------------------------------------------------------------------------

DiaphragmTool::DiaphragmTool(QObject* parent) : QObject(parent) {
    m_edit = new QAction(tr("Editar"), this);
    m_assign = new QAction(tr("Diafragma"), this);
    m_assign->setCheckable(true);
    m_generate = new QAction(tr("Generar por niveles"), this);
    for (QAction* action : {m_edit, m_assign, m_generate}) action->setEnabled(false);
    connect(m_edit, &QAction::triggered, this, [this] { edit(); });
    connect(m_generate, &QAction::triggered, this, [this] { generate(); });
    connect(m_assign, &QAction::toggled, this, [this](bool on) {
        if (on) start();
        else if (m_mode != Mode::None) deactivate();
    });
}

QString DiaphragmTool::statusText(Mode mode) {
    if (mode == Mode::None) return tr("Seleccione una opción del menú.");
    return tr("Diafragmas - Diafragma |  - LMB: Seleccione un grupo de elementos. - RMB: Actuar sobre los elementos seleccionados.");
}

void DiaphragmTool::pruneViewports() {
    m_viewports.removeIf([](const QPointer<Viewport3D>& v) { return v.isNull(); });
}

int DiaphragmTool::run(QDialog& dialog) { return m_runner ? m_runner(dialog) : dialog.exec(); }

void DiaphragmTool::setProject(Project* project, UndoStack* undo) {
    deactivate();
    m_project = project;
    m_undo = undo;
    for (const QPointer<Viewport3D>& viewport : m_viewports)
        if (viewport) viewport->setNodeDiaphragms(m_project ? &m_project->diaphragms : nullptr);
    updateEnabled();
}

void DiaphragmTool::addViewport(Viewport3D* viewport) {
    if (!viewport || m_viewports.contains(viewport)) return;
    m_viewports.push_back(viewport);
    viewport->setNodeDiaphragms(m_project ? &m_project->diaphragms : nullptr);
    connect(viewport, &Viewport3D::actOnSelection, this, [this, viewport] { onActOnSelection(viewport); });
    connect(viewport, &Viewport3D::escapePressed, this, [this] { deactivate(); });
    if (m_mode != Mode::None) configureViewports();
}

void DiaphragmTool::updateEnabled() {
    // Measured: the three buttons are enabled even on an empty work.
    const bool open = m_project != nullptr;
    for (QAction* action : {m_edit, m_assign, m_generate}) action->setEnabled(open);
    if (!open && m_assign->isChecked()) m_assign->setChecked(false);
}

void DiaphragmTool::structureChanged() {
    updateEnabled();
    pruneViewports();
    if (m_panel && m_project) m_panel->setDiaphragms(m_project->diaphragms);
    for (const QPointer<Viewport3D>& viewport : m_viewports) viewport->refreshStructure();
}

void DiaphragmTool::start() {
    if (!m_project || m_mode != Mode::None) return;
    if (m_project->diaphragms.empty()) {  // «No se ha definido ningún diafragma.» and the tool does not start (measured)
        messages::show(m_dialogParent, messages::Kind::Information, QString::fromStdString(diaphragmNoneDefinedMessage()));
        QSignalBlocker block(m_assign);
        m_assign->setChecked(false);
        return;
    }
    emit aboutToActivate();
    m_mode = Mode::Active;
    if (!m_panel) {
        m_panel = new DiaphragmPanel(m_dialogParent);
        m_panel->setAttribute(Qt::WA_DeleteOnClose);
        connect(m_panel, &DiaphragmPanel::closed, this, [this] {
            if (m_assign->isChecked()) m_assign->setChecked(false);
        });
    }
    m_panel->setDiaphragms(m_project->diaphragms);
    m_panel->show();
    configureViewports();
    emit modeChanged(m_mode);
    emit statusChanged(statusText(m_mode));
}

void DiaphragmTool::deactivate() {
    if (m_assign->isChecked()) m_assign->setChecked(false);
    if (m_mode == Mode::None) return;
    m_mode = Mode::None;
    if (m_panel) m_panel->close();
    configureViewports();
    emit modeChanged(m_mode);
    emit statusChanged(statusText(m_mode));
}

void DiaphragmTool::configureViewports() {
    pruneViewports();
    const bool on = m_mode != Mode::None;
    for (const QPointer<Viewport3D>& viewport : m_viewports) {
        viewport->setSelectionMode(false);
        viewport->setNodeLabelsVisible(on);
        viewport->setDiaphragmLabelsVisible(on);
        if (!on) continue;
        viewport->setSelectionFilter(SelectionFilter::only(ElementKind::Node));
        viewport->setSelectionMode(true);
        viewport->setFocus();
    }
}

bool DiaphragmTool::edit() {
    if (!m_project || !m_undo) return false;
    emit aboutToActivate();
    emit statusChanged(tr("Diafragmas - Editar"));
    DiaphragmEditDialog dialog(m_project->diaphragms.items, m_project->loadCases, m_dialogParent);
    bool changed = false;
    if (run(dialog) == QDialog::Accepted) {
        const std::size_t before = m_undo->count();
        if (const auto issue = editDiaphragms(*m_project, *m_undo, dialog.rows()))
            messages::show(m_dialogParent, messages::Kind::Warning, QString::fromStdString(issue->text));
        changed = m_undo->count() != before;
    }
    emit statusChanged(statusText(Mode::None));
    return changed;
}

bool DiaphragmTool::generate() {
    if (!m_project || !m_undo) return false;
    emit aboutToActivate();
    emit statusChanged(tr("Diafragmas - Generar por niveles"));
    bool generated = false;
    DiaphragmGeneration result = generateDiaphragmsByLevels(*m_project, *m_undo, false);
    if (result.status == DiaphragmGenerationStatus::NeedsConfirmation) {
        if (messages::show(m_dialogParent, messages::Kind::QuestionDefaultNo, QString::fromStdString(result.text)))
            result = generateDiaphragmsByLevels(*m_project, *m_undo, true);
        else
            result.status = DiaphragmGenerationStatus::NeedsConfirmation;  // declined: nothing else to say
    }
    if (result.status == DiaphragmGenerationStatus::NoLevels)
        messages::show(m_dialogParent, messages::Kind::Warning, QString::fromStdString(result.text));
    else if (result.status == DiaphragmGenerationStatus::Generated) {
        generated = result.count > 0;
        messages::show(m_dialogParent, messages::Kind::Information, QString::fromStdString(result.text));
    }
    emit statusChanged(statusText(Mode::None));
    return generated;
}

bool DiaphragmTool::execute(const std::vector<int>& nodeIds) {
    if (!m_project || !m_undo || !m_panel) return false;
    const std::vector<int> nodes = diaphragmSelection(*m_project, nodeIds);
    if (nodes.empty()) return false;
    if (m_panel->assign()) {
        const DiaphragmCheck check = assignNodesToDiaphragm(*m_project, *m_undo, m_panel->diaphragmId(), nodes);
        if (check.status == DiaphragmStatus::Error)
            messages::show(m_dialogParent, messages::Kind::Error, QString::fromStdString(check.text));
        return check.status == DiaphragmStatus::Ok;
    }
    return removeNodesFromDiaphragm(*m_project, *m_undo, nodes);
}

void DiaphragmTool::onActOnSelection(Viewport3D* viewport) {
    if (m_mode == Mode::None || !m_project || !m_undo) return;
    const std::vector<int> nodes = viewport->selection().ids(ElementKind::Node);
    if (nodes.empty()) return;
    const std::size_t before = m_undo->count();
    const bool ok = execute(nodes);
    // An error keeps the selection (like Ligaduras); an executed option empties it.
    if (ok || m_undo->count() != before || !m_panel || !m_panel->assign()) viewport->clearSelection();
    emit statusChanged(statusText(m_mode));
}

}  // namespace cc::app
