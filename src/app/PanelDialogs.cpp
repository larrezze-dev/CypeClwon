#include "PanelDialogs.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QRadioButton>
#include <QTabWidget>
#include <QTableWidget>
#include <QToolButton>
#include <QVBoxLayout>
#include <algorithm>

#include "NodalLoadDialogs.h"
#include "UserMessages.h"

namespace cc::app {

using namespace cc::core;

namespace {

QString threeDecimals(double value) { return QString::number(value, 'f', kSurfaceLoadDecimals); }

QFrame* separator(QWidget* parent) {
    auto* line = new QFrame(parent);
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);
    return line;
}

/// Glyph of the list buttons: a green plus, two overlapped sheets or a red cross (the buttons of every list of the original).
QIcon listGlyph(int kind) {
    QPixmap pixmap(40, 40);
    pixmap.fill(Qt::transparent);
    QPainter p(&pixmap);
    p.setRenderHint(QPainter::Antialiasing, true);
    if (kind == 0) {
        p.setPen(QPen(QColor(30, 30, 30), 5, Qt::SolidLine, Qt::FlatCap));  // measured: a black plus
        p.drawLine(20, 7, 20, 33);
        p.drawLine(7, 20, 33, 20);
    } else if (kind == 1) {
        p.setPen(QPen(QColor(90, 90, 90), 2));
        p.setBrush(QColor(235, 235, 235));
        p.drawRect(7, 7, 18, 22);
        p.setBrush(Qt::white);
        p.drawRect(15, 12, 18, 22);
    } else {
        p.setPen(QPen(QColor(0xD0, 0x10, 0x10), 6, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(9, 9, 31, 31);
        p.drawLine(31, 9, 9, 31);
    }
    p.end();
    return QIcon(pixmap);
}

QToolButton* listButton(QWidget* parent, int kind, const QString& tip) {
    auto* b = new QToolButton(parent);
    b->setIcon(listGlyph(kind));
    b->setIconSize(QSize(20, 20));
    b->setFixedSize(24, 24);
    b->setAutoRaise(true);
    b->setToolTip(tip);
    return b;
}

/// «Añadir»: «Añadir nuevo elemento a la lista» ... measured tooltips (title, then the text).
QString tip(const QString& title, const QString& text) { return QStringLiteral("<b>") + title.toHtmlEscaped() + QStringLiteral("</b><br>") + text.toHtmlEscaped(); }

messages::Kind kindOf(const SurfaceLoadIssue& issue) { return issue.kind == SurfaceLoadIssueKind::Error ? messages::Kind::Error : messages::Kind::Warning; }

bool readNumber(const QString& text, SurfaceLoadField field, double& value, QString* message) {
    const SurfaceLoadFieldParse parsed = parseSurfaceLoadField(field, text.trimmed().toStdString());
    if (!parsed.ok) {
        if (message) *message = QString::fromStdString(parsed.message);
        return false;
    }
    value = parsed.value;
    return true;
}

QLineEdit* makeValueEdit(QWidget* parent) {
    auto* edit = new QLineEdit(parent);
    edit->setAlignment(Qt::AlignRight);
    edit->setFixedWidth(70);
    return edit;
}

}  // namespace

// ---- PanelLoadsDialog --------------------------------------------------------------------------------------------------------

PanelLoadsDialog::PanelLoadsDialog(const LoadCaseSet& cases, const std::vector<Point3>& polygon, const std::vector<SurfaceLoadData>& initial, int defaultCaseId,
                                   QWidget* parent)
    : CypeDialog(tr("Cargas en paño"), parent), m_cases(cases), m_polygon(polygon), m_defaultCase(defaultCaseId) {
    setHelpable();
    setMaximizable();
    m_variable.variable = true;
    for (const SurfaceLoadData& row : initial) (row.type == SurfaceLoadType::Variable ? m_variable : m_uniform).rows.push_back(row);
    const bool horizontal = isHorizontalPolygon(m_polygon);
    if (horizontal) m_variable.rows.clear();  // only the uniform list exists on a horizontal panel

    auto* body = new QWidget(this);
    auto* outer = new QVBoxLayout(body);
    outer->setContentsMargins(6, 0, 6, 0);
    outer->setSpacing(4);
    auto* bar = new QHBoxLayout();
    bar->setSpacing(0);
    m_add = listButton(body, 0, tip(tr("Añadir"), tr("Añadir nuevo elemento a la lista")));
    m_copy = listButton(body, 1, tip(tr("Copiar"), tr("Copiar elemento seleccionado y añadirlo a la lista")));
    m_remove = listButton(body, 2, tip(tr("Borrar"), tr("Suprimir elemento seleccionado de la lista")));
    m_add->setObjectName(QStringLiteral("panelAdd"));
    m_copy->setObjectName(QStringLiteral("panelCopy"));
    m_remove->setObjectName(QStringLiteral("panelRemove"));
    for (QToolButton* b : {m_add, m_copy, m_remove}) {
        b->setFocusPolicy(Qt::NoFocus);  // measured: the buttons of the list are not in the tab sequence
        bar->addWidget(b);
    }
    bar->addStretch(1);
    outer->addLayout(bar);

    QWidget* uniform = buildTable(m_uniform, body);
    m_uniformTable = m_uniform.widget;
    m_uniformTable->setObjectName(QStringLiteral("panelUniformTable"));
    if (horizontal) {
        outer->addWidget(uniform, 1);
    } else {
        m_tabs = new QTabWidget(body);
        m_tabs->setObjectName(QStringLiteral("panelTabs"));
        m_tabs->tabBar()->setFocusPolicy(Qt::NoFocus);  // measured: the tabs are not in the tab sequence
        m_tabs->addTab(uniform, tr("Uniformes"));
        QWidget* variable = buildTable(m_variable, m_tabs);
        m_variableTable = m_variable.widget;
        m_variableTable->setObjectName(QStringLiteral("panelVariableTable"));
        m_tabs->addTab(variable, tr("Variables con la altura"));
        outer->addWidget(m_tabs, 1);
    }
    contentLayout()->setContentsMargins(0, 0, 0, 0);
    contentLayout()->addWidget(body, 1);
    setupFooter(tr("Aceptar"), tr("Cancelar"));
    resize(450, 370);  // measured (cp_30_m)
    // Contextual help (measured: +, copy, delete, the header «Sentido positivo» and Aceptar are framed; the header says «Sentido positivo de la carga»,
    // #65051). Texts of the three buttons = their tooltips and of the value headers #65055 / #65056: ASSUMED (the balloons were not captured); Aceptar
    // is framed in the original but its text is NOT MEASURED, so it is left out.
    const auto areaOf = [this](QWidget* w) { return [this, w] { return QRect(w->mapTo(this, QPoint(0, 0)), w->size()); }; };
    addHelpItem(areaOf(m_add), tr("Añadir nuevo elemento a la lista"));
    addHelpItem(areaOf(m_copy), tr("Copiar elemento seleccionado y añadirlo a la lista"));
    addHelpItem(areaOf(m_remove), tr("Suprimir elemento seleccionado de la lista"));
    const auto headerArea = [this](int section) {
        return [this, section] {
            QTableWidget* t = currentTable();
            QHeaderView* header = t->horizontalHeader();
            return QRect(header->mapTo(this, QPoint(header->sectionViewportPosition(section), 0)), QSize(header->sectionSize(section), header->height()));
        };
    };
    addHelpItem(headerArea(1), tr("Sentido positivo de la carga"));
    addHelpItem(
        [this, headerArea] {
            QTableWidget* t = currentTable();
            return t == m_variableTable ? headerArea(2)() : QRect();
        },
        tr("Valor de la carga en el punto más bajo de la superficie."));
    addHelpItem(
        [this, headerArea] {
            QTableWidget* t = currentTable();
            return t == m_variableTable ? headerArea(3)() : QRect();
        },
        tr("Valor de la carga en el punto más alto de la superficie."));
    connect(m_add, &QToolButton::clicked, this, [this] { addRow(); });
    connect(m_copy, &QToolButton::clicked, this, [this] { copyRow(); });
    connect(m_remove, &QToolButton::clicked, this, [this] { removeRows(); });
    refresh(false, {});
    if (m_tabs) refresh(true, {});
    if (auto* first = qobject_cast<QComboBox*>(m_uniformTable->cellWidget(0, 0))) first->setFocus();  // measured: the focus starts on the first row
}

bool PanelLoadsDialog::focusNextPrevChild(bool next) {
    QTableWidget* t = currentTable();
    const int rows = t->rowCount(), columns = t->columnCount();
    const int cells = rows * columns;
    QWidget* focus = focusWidget();
    int position = -1;  // index in the sequence row * columns + column; -1 = outside the cells
    if (focus) {
        for (int r = 0; r < rows && position < 0; ++r)
            if (QWidget* combo = t->cellWidget(r, 0); combo && (focus == combo || combo->isAncestorOf(focus))) position = r * columns;
        if (position < 0 && (focus == t || t->isAncestorOf(focus)) && t->currentRow() >= 0) position = t->currentRow() * columns + std::max(0, t->currentColumn());
    }
    const auto focusCell = [&](int index) {
        const int row = index / columns, column = index % columns;
        t->setCurrentCell(row, column);
        if (column == 0) {
            if (auto* combo = qobject_cast<QComboBox*>(t->cellWidget(row, 0))) combo->setFocus();
        } else {
            t->setFocus();
            if (column >= 2)
                if (QTableWidgetItem* item = t->item(row, column)) t->editItem(item);  // a value cell takes the typing at once
        }
    };
    if (position >= 0) {
        const int target = position + (next ? 1 : -1);
        if (target >= 0 && target < cells) {
            focusCell(target);
        } else {
            QWidget* button = next ? static_cast<QWidget*>(acceptButton()) : static_cast<QWidget*>(cancelButton());
            t->clearFocus();
            button->setFocus();
        }
        return true;
    }
    if (focus == acceptButton()) {
        if (next) cancelButton()->setFocus();
        else if (cells > 0) focusCell(cells - 1);
        else cancelButton()->setFocus();
        return true;
    }
    if (focus == cancelButton()) {
        if (!next) acceptButton()->setFocus();
        else if (cells > 0) focusCell(0);
        else acceptButton()->setFocus();
        return true;
    }
    if (cells > 0) {
        focusCell(next ? 0 : cells - 1);
        return true;
    }
    return CypeDialog::focusNextPrevChild(next);
}

QWidget* PanelLoadsDialog::buildTable(Table& t, QWidget* parent) {
    QStringList labels{tr("Hipótesis"), tr("Sentido positivo")};
    if (t.variable) {
        labels << tr("Valor en la cota inferior") << tr("Valor en la cota superior");
    } else {
        labels << tr("Valor");
    }
    t.widget = new QTableWidget(0, static_cast<int>(labels.size()), parent);
    t.widget->setHorizontalHeaderLabels(labels);
    t.widget->verticalHeader()->hide();
    t.widget->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);  // measured: left-aligned headers
    t.widget->setSelectionBehavior(QAbstractItemView::SelectRows);
    t.widget->setSelectionMode(QAbstractItemView::ExtendedSelection);
    t.widget->setTabKeyNavigation(false);  // the sequence is driven by focusNextPrevChild
    t.widget->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    t.widget->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    for (int c = 2; c < static_cast<int>(labels.size()); ++c) t.widget->horizontalHeader()->setSectionResizeMode(c, QHeaderView::ResizeToContents);
    t.widget->setStyleSheet(QStringLiteral("QTableWidget{background:white;} QTableWidget::item:selected{background:#CDE6F7;color:black;}"));
    const bool variable = t.variable;
    connect(t.widget, &QTableWidget::cellChanged, this, [this, variable](int row, int column) { onCellChanged(variable, row, column); });
    return t.widget;
}

QString PanelLoadsDialog::senseText(int loadCaseId) const {
    return QString::fromStdString(panelPositiveSenseText(m_polygon, isWindCase(m_cases, loadCaseId)));
}

QTableWidget* PanelLoadsDialog::currentTable() const { return m_tabs && m_tabs->currentIndex() == 1 ? m_variableTable : m_uniformTable; }

std::vector<int> PanelLoadsDialog::selectedRows(bool variable) const {
    std::vector<int> rows;
    QTableWidget* w = variable ? m_variableTable : m_uniformTable;
    if (!w) return rows;
    for (const QModelIndex& index : w->selectionModel()->selectedRows()) rows.push_back(index.row());
    std::sort(rows.begin(), rows.end());
    return rows;
}

void PanelLoadsDialog::refresh(bool variable, const std::vector<int>& select) {
    Table& t = table(variable);
    if (!t.widget) return;
    m_loading = true;
    t.widget->clearSelection();
    t.widget->setRowCount(static_cast<int>(t.rows.size()));
    for (int r = 0; r < static_cast<int>(t.rows.size()); ++r) {
        const SurfaceLoadData& row = t.rows[static_cast<std::size_t>(r)];
        auto* combo = new QComboBox(t.widget);  // measured: a combo in every row
        fillLoadCaseCombo(combo, m_cases, row.loadCaseId);
        connect(combo, &QComboBox::currentIndexChanged, this, [this, combo, variable](int) {
            Table& tt = table(variable);
            for (int rr = 0; rr < tt.widget->rowCount(); ++rr)
                if (tt.widget->cellWidget(rr, 0) == combo) {
                    // A hypothesis chosen in a selected row goes to every selected row (same rule as the values).
                    std::vector<int> targets = selectedRows(variable);
                    if (std::find(targets.begin(), targets.end(), rr) == targets.end()) targets = {rr};
                    for (int target : targets) tt.rows[static_cast<std::size_t>(target)].loadCaseId = combo->currentData().toInt();
                    refresh(variable, targets);
                    return;
                }
        });
        t.widget->setCellWidget(r, 0, combo);
        auto* sense = new QTableWidgetItem(senseText(row.loadCaseId));
        sense->setFlags(sense->flags() & ~Qt::ItemIsEditable);  // measured: informative column
        t.widget->setItem(r, 1, sense);
        if (variable) {
            auto* lower = new QTableWidgetItem(threeDecimals(row.lower));
            lower->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            t.widget->setItem(r, 2, lower);
            auto* upper = new QTableWidgetItem(threeDecimals(row.upper));
            upper->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            t.widget->setItem(r, 3, upper);
        } else {
            auto* value = new QTableWidgetItem(threeDecimals(row.value));
            value->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            t.widget->setItem(r, 2, value);
        }
    }
    m_loading = false;
    if (!select.empty()) {
        t.widget->setSelectionMode(QAbstractItemView::MultiSelection);  // programmatic multi-selection without Ctrl
        for (int r : select)
            if (r >= 0 && r < t.widget->rowCount()) t.widget->selectRow(r);
        t.widget->setSelectionMode(QAbstractItemView::ExtendedSelection);
    }
}

void PanelLoadsDialog::onCellChanged(bool variable, int row, int column) {
    Table& t = table(variable);
    if (m_loading || row < 0 || row >= static_cast<int>(t.rows.size()) || column < 2) return;
    QTableWidgetItem* item = t.widget->item(row, column);
    if (!item) return;
    const SurfaceLoadField field = !variable ? SurfaceLoadField::Value : (column == 2 ? SurfaceLoadField::Lower : SurfaceLoadField::Upper);
    double value = 0.0;
    QString message;
    const bool ok = readNumber(item->text(), field, value, &message);
    // Measured: the typed value goes to every selected row.
    std::vector<int> targets = selectedRows(variable);
    if (std::find(targets.begin(), targets.end(), row) == targets.end()) targets = {row};
    if (ok)
        for (int target : targets) {
            SurfaceLoadData& data = t.rows[static_cast<std::size_t>(target)];
            (field == SurfaceLoadField::Value ? data.value : (field == SurfaceLoadField::Lower ? data.lower : data.upper)) = value;
        }
    refresh(variable, targets);
    if (!ok) messages::show(this, messages::Kind::Warning, message);
}

void PanelLoadsDialog::setCellText(bool variable, int row, int column, const QString& text) {
    Table& t = table(variable);
    if (QTableWidgetItem* item = t.widget ? t.widget->item(row, column) : nullptr) item->setText(text);  // cellChanged does the rest
}

void PanelLoadsDialog::setRowCase(bool variable, int row, int loadCaseId) {
    Table& t = table(variable);
    if (!t.widget) return;
    if (auto* combo = qobject_cast<QComboBox*>(t.widget->cellWidget(row, 0))) {
        const int index = combo->findData(loadCaseId);
        if (index >= 0) combo->setCurrentIndex(index);
    }
}

void PanelLoadsDialog::addRow() {
    const bool variable = m_tabs && m_tabs->currentIndex() == 1;
    Table& t = table(variable);
    SurfaceLoadData row = surfaceLoadDefaults(m_defaultCase, variable ? SurfaceLoadType::Variable : SurfaceLoadType::Uniform);
    t.rows.push_back(row);
    std::vector<int> select = selectedRows(variable);
    select.push_back(static_cast<int>(t.rows.size()) - 1);  // measured: the new row joins the selection
    refresh(variable, select);
}

void PanelLoadsDialog::copyRow() {
    const bool variable = m_tabs && m_tabs->currentIndex() == 1;
    Table& t = table(variable);
    const std::vector<int> selected = selectedRows(variable);
    if (selected.empty()) return;
    std::vector<int> select;
    for (int r : selected) {
        t.rows.push_back(t.rows[static_cast<std::size_t>(r)]);
        select.push_back(static_cast<int>(t.rows.size()) - 1);
    }
    refresh(variable, select);
}

void PanelLoadsDialog::removeRows() {
    const bool variable = m_tabs && m_tabs->currentIndex() == 1;
    Table& t = table(variable);
    const std::vector<int> selected = selectedRows(variable);
    if (selected.empty()) return;
    for (auto it = selected.rbegin(); it != selected.rend(); ++it) t.rows.erase(t.rows.begin() + *it);  // measured: Borrar removes all the selected rows
    refresh(variable, {});
}

std::vector<SurfaceLoadData> PanelLoadsDialog::rows() const {
    std::vector<SurfaceLoadData> out = m_uniform.rows;
    for (SurfaceLoadData row : m_variable.rows) {
        row.type = SurfaceLoadType::Variable;
        out.push_back(row);
    }
    return out;
}

void PanelLoadsDialog::accept() {
    for (const SurfaceLoadData& row : rows()) {
        const SurfaceLoadIssue issue = checkSurfaceLoadData(row);
        if (!issue.ok()) {
            if (m_tabs) m_tabs->setCurrentIndex(row.type == SurfaceLoadType::Variable ? 1 : 0);
            messages::show(this, kindOf(issue), QString::fromStdString(issue.message));  // the window stays open (measured)
            return;
        }
    }
    CypeDialog::accept();
}

// ---- SurfaceLoadDialog -------------------------------------------------------------------------------------------------------

SurfaceLoadDialog::SurfaceLoadDialog(const QString& title, const LoadCaseSet& cases, const std::vector<Point3>& polygon, const SurfaceLoadData& initial,
                                     QWidget* parent)
    : CypeDialog(title, parent), m_cases(cases), m_polygon(polygon), m_horizontal(isHorizontalPolygon(polygon)), m_last(initial) {
    setHelpable();  // measured: the book icon is in the title bar of this dialog too (texts of its balloons NOT MEASURED)
    if (m_horizontal) m_last.type = SurfaceLoadType::Uniform;  // measured: a horizontal surface only has uniform loads
    auto* body = new QWidget(this);
    auto* column = new QVBoxLayout(body);
    column->setContentsMargins(6, 0, 6, 0);
    column->setSpacing(5);

    auto* caseRow = new QHBoxLayout();
    caseRow->addWidget(new QLabel(tr("Hipótesis"), body));
    m_case = new QComboBox(body);
    m_case->setObjectName(QStringLiteral("loadCase"));
    m_case->setMinimumWidth(98);
    fillLoadCaseCombo(m_case, m_cases, initial.loadCaseId);
    caseRow->addWidget(m_case);
    caseRow->addStretch(1);
    column->addLayout(caseRow);
    m_sense = new QLabel(body);
    m_sense->setObjectName(QStringLiteral("senseLabel"));
    column->addWidget(m_sense);
    addHelpItem([this] { return QRect(m_sense->mapTo(this, QPoint(0, 0)), m_sense->size()); }, tr("Sentido positivo de la carga"));  // balloon text ASSUMED (#65051)
    column->addWidget(separator(body));

    if (!m_horizontal) {
        m_uniformRadio = new QRadioButton(tr("Uniforme"), body);
        m_uniformRadio->setObjectName(QStringLiteral("uniformRadio"));
        m_variableRadio = new QRadioButton(tr("Variable con la altura"), body);
        m_variableRadio->setObjectName(QStringLiteral("variableRadio"));
        column->addWidget(m_uniformRadio);
        column->addWidget(m_variableRadio);
    }
    const auto fieldRow = [&](QWidget* parent, SurfaceLoadField field, QLineEdit*& edit) {
        auto* row = new QWidget(parent);
        auto* layout = new QHBoxLayout(row);
        layout->setContentsMargins(0, 0, 0, 0);
        const SurfaceLoadFieldSpec spec = surfaceLoadFieldSpec(field);
        layout->addWidget(new QLabel(QString::fromStdString(spec.label), row));
        edit = makeValueEdit(row);
        layout->addWidget(edit);
        layout->addWidget(new QLabel(QString::fromStdString(spec.unit), row));
        layout->addStretch(1);
        return row;
    };
    m_uniformBlock = new QWidget(body);
    auto* uniformLayout = new QVBoxLayout(m_uniformBlock);
    uniformLayout->setContentsMargins(0, 0, 0, 0);
    uniformLayout->addWidget(fieldRow(m_uniformBlock, SurfaceLoadField::Value, m_value));
    m_value->setObjectName(QStringLiteral("valueEdit"));
    column->addWidget(m_uniformBlock);
    m_variableBlock = new QWidget(body);
    auto* variableLayout = new QVBoxLayout(m_variableBlock);
    variableLayout->setContentsMargins(0, 0, 0, 0);
    variableLayout->setSpacing(4);
    // Measured: here the upper value comes first (the opposite of the table of the panel).
    variableLayout->addWidget(fieldRow(m_variableBlock, SurfaceLoadField::Upper, m_upper));
    variableLayout->addWidget(fieldRow(m_variableBlock, SurfaceLoadField::Lower, m_lower));
    m_upper->setObjectName(QStringLiteral("upperEdit"));
    m_lower->setObjectName(QStringLiteral("lowerEdit"));
    column->addWidget(m_variableBlock);
    column->addStretch(1);
    contentLayout()->setContentsMargins(0, 0, 0, 0);
    contentLayout()->addWidget(body, 1);
    setupFooter(tr("Aceptar"), tr("Cancelar"));
    setFixedSize(280, m_horizontal ? 185 : 215);  // measured

    m_value->setText(threeDecimals(m_last.value));
    m_upper->setText(threeDecimals(m_last.upper));
    m_lower->setText(threeDecimals(m_last.lower));
    if (!m_horizontal) (m_last.type == SurfaceLoadType::Variable ? m_variableRadio : m_uniformRadio)->setChecked(true);
    connect(m_case, &QComboBox::currentIndexChanged, this, [this](int) { m_sense->setText(tr("Sentido positivo: ") + senseText()); });
    if (!m_horizontal) {
        connect(m_uniformRadio, &QRadioButton::toggled, this, [this](bool) { refresh(); });
        connect(m_variableRadio, &QRadioButton::toggled, this, [this](bool) { refresh(); });
    }
    m_sense->setText(tr("Sentido positivo: ") + senseText());
    refresh();
}

QString SurfaceLoadDialog::senseText() const {
    const int caseId = m_case->currentIndex() < 0 ? 0 : m_case->currentData().toInt();
    return QString::fromStdString(panelPositiveSenseText(m_polygon, isWindCase(m_cases, caseId)));
}

void SurfaceLoadDialog::refresh() {
    const bool variable = !m_horizontal && m_variableRadio->isChecked();
    m_uniformBlock->setVisible(!variable);
    m_variableBlock->setVisible(variable);
}

SurfaceLoadData SurfaceLoadDialog::current() const {
    SurfaceLoadData data = m_last;
    data.loadCaseId = m_case->currentIndex() < 0 ? 0 : m_case->currentData().toInt();
    data.type = (!m_horizontal && m_variableRadio->isChecked()) ? SurfaceLoadType::Variable : SurfaceLoadType::Uniform;
    double v = 0.0;
    if (readNumber(m_value->text(), SurfaceLoadField::Value, v, nullptr)) data.value = v;
    if (readNumber(m_upper->text(), SurfaceLoadField::Upper, v, nullptr)) data.upper = v;
    if (readNumber(m_lower->text(), SurfaceLoadField::Lower, v, nullptr)) data.lower = v;
    return data;
}

SurfaceLoadData SurfaceLoadDialog::data() const { return current(); }

void SurfaceLoadDialog::accept() {
    const bool variable = !m_horizontal && m_variableRadio->isChecked();
    struct Field {
        QLineEdit* edit;
        SurfaceLoadField field;
        double SurfaceLoadData::*slot;
    };
    std::vector<Field> fields;
    if (variable) {
        fields = {{m_upper, SurfaceLoadField::Upper, &SurfaceLoadData::upper}, {m_lower, SurfaceLoadField::Lower, &SurfaceLoadData::lower}};
    } else {
        fields = {{m_value, SurfaceLoadField::Value, &SurfaceLoadData::value}};
    }
    for (const Field& f : fields) {
        double v = 0.0;
        QString message;
        if (!readNumber(f.edit->text(), f.field, v, &message)) {
            f.edit->setText(threeDecimals(m_last.*f.slot));  // the field recovers its previous value
            f.edit->setFocus();
            messages::show(this, messages::Kind::Warning, message);
            return;
        }
        m_last.*f.slot = v;
        f.edit->setText(threeDecimals(v));
    }
    const SurfaceLoadData data = current();
    const SurfaceLoadIssue issue = checkSurfaceLoadData(data);
    if (!issue.ok()) {
        messages::show(this, kindOf(issue), QString::fromStdString(issue.message));  // the window stays open
        return;
    }
    CypeDialog::accept();
}

// ---- MoveSurfaceLoadsDialog --------------------------------------------------------------------------------------------------

MoveSurfaceLoadsDialog::MoveSurfaceLoadsDialog(bool alsoOtherCases, QWidget* parent) : CypeDialog(tr("Mover cargas superficiales"), parent) {
    auto* body = new QWidget(this);
    auto* column = new QVBoxLayout(body);
    column->setContentsMargins(10, 6, 10, 6);
    m_check = new QCheckBox(tr("Mover también las cargas aplicadas sobre la misma zona pertenecientes a otras hipótesis"), body);
    m_check->setObjectName(QStringLiteral("alsoOtherCases"));
    m_check->setChecked(alsoOtherCases);
    column->addWidget(m_check);
    contentLayout()->setContentsMargins(0, 0, 0, 0);
    contentLayout()->addWidget(body, 1);
    QHBoxLayout* footer = setupCustomFooter();  // measured: only Aceptar
    auto* accept = new QPushButton(tr("Aceptar"), this);
    accept->setDefault(true);
    footer->addWidget(accept);
    footer->addStretch(1);
    connect(accept, &QPushButton::clicked, this, &QDialog::accept);
    setFixedWidth(330);
}

bool MoveSurfaceLoadsDialog::alsoOtherCases() const { return m_check->isChecked(); }

// ---- PanelDirectionPanel -----------------------------------------------------------------------------------------------------

PanelDirectionPanel::PanelDirectionPanel(QWidget* parent) : CypeDialog(tr("Propiedades"), parent) {
    setModal(false);
    setMaximizable();
    contentLayout()->setContentsMargins(0, 0, 0, 0);
    contentLayout()->setSpacing(0);
    auto* body = new QWidget(this);
    auto* column = new QVBoxLayout(body);
    column->setContentsMargins(6, 6, 6, 6);
    column->setSpacing(6);
    column->addWidget(new QLabel(tr("Dirección de reparto de las cargas"), body));
    m_combo = new QComboBox(body);
    m_combo->setObjectName(QStringLiteral("directionMode"));
    m_combo->addItem(tr("Paralela al lado seleccionado"), static_cast<int>(PanelDirectionMode::Parallel));
    m_combo->addItem(tr("Perpendicular al lado seleccionado"), static_cast<int>(PanelDirectionMode::Perpendicular));
    column->addWidget(m_combo);
    column->addStretch(1);
    contentLayout()->addWidget(body, 1);
    setMinimumSize(277, 368);  // measured (cp_np)
    connect(m_combo, &QComboBox::currentIndexChanged, this, [this](int) { emit modeChanged(mode()); });
}

PanelDirectionMode PanelDirectionPanel::mode() const {
    return m_combo->currentData().toInt() == static_cast<int>(PanelDirectionMode::Perpendicular) ? PanelDirectionMode::Perpendicular : PanelDirectionMode::Parallel;
}

void PanelDirectionPanel::setMode(PanelDirectionMode mode) { m_combo->setCurrentIndex(m_combo->findData(static_cast<int>(mode))); }

void PanelDirectionPanel::done(int result) {
    CypeDialog::done(result);
    emit closed();
}

}  // namespace cc::app
