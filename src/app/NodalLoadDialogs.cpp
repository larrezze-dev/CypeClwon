#include "NodalLoadDialogs.h"

#include <QAbstractItemView>
#include <QCheckBox>
#include <QComboBox>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPixmap>
#include <QPolygonF>
#include <QKeyEvent>
#include <QRadioButton>
#include <QShowEvent>
#include <QStyledItemDelegate>
#include <QTableWidget>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>
#include <functional>

#include "UserMessages.h"
#include "core/nodal_load_edit.h"

namespace cc::app {

using namespace cc::core;

namespace {

QString twoDecimals(double v) { return QString::number(v, 'f', 2); }
QString threeDecimals(double v) { return QString::number(v, 'f', 3); }

/// Reads a number written with "." or "," (the field texts of the dialogs); false when the whole text is not a number.
bool readNumber(const QString& text, double& value) {
    QString t = text.trimmed();
    t.replace(QLatin1Char(','), QLatin1Char('.'));
    bool ok = false;
    value = t.toDouble(&ok);
    return ok && std::isfinite(value);
}

void setBold(QWidget* widget, bool bold) {
    QFont font = widget->font();
    font.setBold(bold);
    widget->setFont(font);
}

QFrame* separator(QWidget* parent) {
    auto* line = new QFrame(parent);
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);
    return line;
}

/// Toolbar glyphs of the original lists (as in the diaphragm dialog): black plus, grey sheets, red cross.
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

QString tipText(const QString& title, const QString& text) { return title + QStringLiteral(" / ") + text; }

}  // namespace

QString loadCaseText(const LoadCase& loadCase) { return QString::fromStdString(loadCaseLabel(loadCase)); }

void fillLoadCaseCombo(QComboBox* combo, const LoadCaseSet& cases, int selectedId) {
    combo->clear();
    for (const LoadCase* c : loadCasesInListOrder(cases)) combo->addItem(loadCaseText(*c), c->id);  // Peso propio, then by nature and number
    const int index = combo->findData(selectedId);
    combo->setCurrentIndex(index >= 0 ? index : (combo->count() > 0 ? 0 : -1));
}

// ---- direction pictograms ---------------------------------------------------------------------------------------------------

QIcon loadDirectionIcon(LoadDirection direction) {
    constexpr int size = 44;
    constexpr int scale = 2;
    QPixmap pm(size * scale, size * scale);
    pm.setDevicePixelRatio(scale);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    const QPointF o(22, 24);
    const QColor blue(0, 40, 220);
    // Screen directions of the global axes in the pictogram: X to the right, Y diagonal towards the top left, Z up.
    const QPointF ax[3] = {QPointF(1, 0), QPointF(-0.72, -0.69), QPointF(0, -1)};
    QPen axisPen(blue, 1.4);
    p.setPen(axisPen);
    p.drawLine(o - QPointF(19, 0), o + QPointF(19, 0));
    p.drawLine(o - QPointF(-0.72, -0.69) * 17.0, o + QPointF(-0.72, -0.69) * 17.0);
    p.drawLine(o - QPointF(0, -1) * 18.0, o + QPointF(0, -1) * 18.0);
    p.setBrush(blue);
    const auto head = [&](const QPointF& tip, const QPointF& dir, double len, double half, const QColor& colour) {
        const QPointF n(-dir.y(), dir.x());
        QPolygonF poly;
        poly << tip << tip - dir * len + n * half << tip - dir * len - n * half;
        p.setBrush(colour);
        p.setPen(colour);
        p.drawPolygon(poly);
    };
    head(o + ax[0] * 19.0, ax[0], 5, 2.2, blue);
    head(o + ax[1] * 17.0, ax[1], 5, 2.2, blue);
    head(o + ax[2] * 18.0, ax[2], 5, 2.2, blue);
    QFont font = p.font();
    font.setPixelSize(9);
    font.setBold(true);
    p.setFont(font);
    p.setPen(blue);
    p.drawText(QPointF(o.x() + 17, o.y() + 11), QStringLiteral("x"));
    p.drawText(QPointF(o.x() - 20, o.y() - 12), QStringLiteral("Y"));
    p.drawText(QPointF(o.x() + 4, o.y() - 15), QStringLiteral("z"));
    const QColor red(235, 20, 20);
    int axis = 0;
    double sign = 1.0;
    switch (direction) {
        case LoadDirection::PlusX: axis = 0; sign = 1; break;
        case LoadDirection::PlusY: axis = 1; sign = 1; break;
        case LoadDirection::PlusZ: axis = 2; sign = 1; break;
        case LoadDirection::MinusX: axis = 0; sign = -1; break;
        case LoadDirection::MinusY: axis = 1; sign = -1; break;
        case LoadDirection::MinusZ: axis = 2; sign = -1; break;
        case LoadDirection::UserVector: axis = 1; sign = 1; break;
    }
    if (direction == LoadDirection::UserVector) {
        p.setPen(QPen(QColor(255, 235, 0), 1.2));
        p.setBrush(QColor(255, 255, 160, 120));
        p.drawRect(QRectF(o.x() - 4, o.y() - 22, 24, 16));  // the yellow box of the user vector
        const QPointF dir = QPointF(0.6, -0.8);
        const QPointF from = o - dir * 16.0;
        p.setPen(QPen(red, 3.2, Qt::SolidLine, Qt::FlatCap));
        p.drawLine(from, o - dir * 3.0);
        head(o, dir, 7, 4, red);
        return QIcon(pm);
    }
    const QPointF dir = ax[axis] * sign;  // direction of the force: the arrow arrives at the origin
    const QPointF from = o - dir * 16.0;
    p.setPen(QPen(red, 3.2, Qt::SolidLine, Qt::FlatCap));
    p.drawLine(from, o - dir * 3.0);
    head(o, dir, 7, 4, red);
    return QIcon(pm);
}

// ---- NodalLoadDialog --------------------------------------------------------------------------------------------------------

NodalLoadDialog::NodalLoadDialog(const QString& title, const LoadCaseSet& cases, const NodalLoadData& initial, QWidget* parent)
    : CypeDialog(title, parent), m_cases(cases), m_direction(initial.direction) {
    auto* body = new QWidget(this);
    auto* column = new QVBoxLayout(body);
    column->setContentsMargins(6, 0, 6, 0);
    column->setSpacing(6);

    auto* caseRow = new QHBoxLayout();
    caseRow->addWidget(new QLabel(tr("Hipótesis"), body));
    m_case = new QComboBox(body);
    m_case->setObjectName(QStringLiteral("loadCase"));
    m_case->setMinimumWidth(98);
    fillLoadCaseCombo(m_case, m_cases, initial.loadCaseId);
    caseRow->addWidget(m_case);
    caseRow->addStretch(1);
    column->addLayout(caseRow);
    column->addWidget(separator(body));

    auto* valueRow = new QHBoxLayout();
    auto* valueLabel = new QLabel(tr("Valor"), body);
    valueLabel->setToolTip(tr("Valor de la carga puntual"));
    valueRow->addWidget(valueLabel);
    m_value = new QLineEdit(twoDecimals(initial.magnitude), body);
    m_value->setObjectName(QStringLiteral("loadValue"));
    m_value->setAlignment(Qt::AlignRight);
    m_value->setFixedWidth(62);
    m_value->setToolTip(tr("Valor de la carga puntual"));
    valueRow->addWidget(m_value);
    valueRow->addWidget(new QLabel(tr("kN"), body));
    valueRow->addStretch(1);
    column->addLayout(valueRow);
    column->addWidget(separator(body));

    auto* heading = new QLabel(tr("Dirección y sentido de aplicación de la carga"), body);
    setBold(heading, true);
    column->addWidget(heading);
    auto* buttons = new QHBoxLayout();
    buttons->setSpacing(6);
    for (int i = 0; i < kLoadDirectionCount; ++i) {
        auto* button = new QToolButton(body);
        button->setObjectName(QStringLiteral("direction%1").arg(i));
        button->setIcon(loadDirectionIcon(static_cast<LoadDirection>(i)));
        button->setIconSize(QSize(44, 44));
        button->setFixedSize(46, 46);
        button->setCheckable(true);
        button->setAutoExclusive(false);
        button->setStyleSheet(QStringLiteral("QToolButton{background:#F4F4F4;border:1px solid #D0D0D0;}"
                                             "QToolButton:checked{background:#BDBDBD;border:1px solid #8A8A8A;}"));
        if (i == kLoadDirectionCount - 1) {  // only the last button has a tooltip in the original
            button->setToolTip(tr("Definir dirección") + QLatin1Char('\n') +
                               tr("La carga se aplica en la dirección definida por el sistema de referencia global introducido por el usuario."));
        }
        connect(button, &QToolButton::clicked, this, [this, i] { setDirection(static_cast<LoadDirection>(i)); });
        m_buttons[static_cast<std::size_t>(i)] = button;
        buttons->addWidget(button);
    }
    buttons->addStretch(1);
    column->addLayout(buttons);

    m_vectorGroup = new QWidget(body);
    auto* vector = new QVBoxLayout(m_vectorGroup);
    vector->setContentsMargins(0, 0, 0, 0);
    vector->setSpacing(4);
    vector->addWidget(separator(m_vectorGroup));
    auto* vectorTitle = new QLabel(tr("Vector director"), m_vectorGroup);
    setBold(vectorTitle, true);
    vector->addWidget(vectorTitle);
    auto* fields = new QHBoxLayout();
    const char* names[3] = {"X", "Y", "Z"};
    const double values[3] = {initial.userVector.x, initial.userVector.y, initial.userVector.z};
    for (int i = 0; i < 3; ++i) {
        fields->addWidget(new QLabel(QLatin1String(names[i]), m_vectorGroup));
        auto* edit = new QLineEdit(threeDecimals(values[i]), m_vectorGroup);
        edit->setObjectName(QStringLiteral("vector%1").arg(QLatin1String(names[i])));
        edit->setAlignment(Qt::AlignRight);
        edit->setFixedWidth(70);
        m_vector[static_cast<std::size_t>(i)] = edit;
        fields->addWidget(edit);
        fields->addSpacing(8);
    }
    fields->addStretch(1);
    vector->addLayout(fields);
    column->addWidget(m_vectorGroup);
    column->addStretch(1);

    contentLayout()->setContentsMargins(0, 6, 0, 0);
    contentLayout()->addWidget(body, 1);
    setupFooter(tr("Aceptar"), tr("Cancelar"));
    setDirection(initial.direction);
    m_value->setFocus();
    m_value->selectAll();
}

void NodalLoadDialog::updateSize() {
    // 390 x 295 as measured (cn_c1); the "Vector director" group makes the window grow (cn_d_montage).
    const bool user = m_direction == LoadDirection::UserVector;
    m_vectorGroup->setVisible(user);
    setFixedSize(390, user ? 390 : 295);
}

void NodalLoadDialog::setDirection(LoadDirection direction) {
    m_direction = direction;
    for (int i = 0; i < kLoadDirectionCount; ++i) m_buttons[static_cast<std::size_t>(i)]->setChecked(i == static_cast<int>(direction));
    updateSize();
    m_value->setFocus();  // measured: after a direction button the field has the focus with its text selected
    m_value->selectAll();
}

NodalLoadData NodalLoadDialog::data() const {
    NodalLoadData out;
    out.loadCaseId = m_case->currentIndex() < 0 ? 0 : m_case->currentData().toInt();
    double v = 0.0;
    out.magnitude = readNumber(m_value->text(), v) ? v : 0.0;
    out.direction = m_direction;
    double c[3] = {kDefaultUserVector.x, kDefaultUserVector.y, kDefaultUserVector.z};
    for (int i = 0; i < 3; ++i) readNumber(m_vector[static_cast<std::size_t>(i)]->text(), c[i]);
    out.userVector = {c[0], c[1], c[2]};
    return out;
}

void NodalLoadDialog::accept() {
    const LoadValueParse parsed = parseLoadValue(m_value->text().toStdString());
    if (!parsed.ok) {
        messages::show(this, messages::Kind::Warning, QString::fromStdString(parsed.message));
        m_value->setFocus();
        return;
    }
    if (m_direction == LoadDirection::UserVector) {
        for (const QLineEdit* edit : m_vector) {
            double v = 0.0;
            if (!readNumber(edit->text(), v)) {
                messages::show(this, messages::Kind::Warning, QString::fromStdString(nodalLoadNotNumberMessage()));
                return;
            }
        }
    }
    NodalLoadData d = data();
    d.magnitude = parsed.value;
    if (const std::string issue = checkNodalLoadData(d); !issue.empty()) {
        messages::show(this, messages::Kind::Warning, QString::fromStdString(issue));
        return;
    }
    m_value->setText(twoDecimals(parsed.value));
    CypeDialog::accept();
}

// ---- LoadCaseChoiceDialog ---------------------------------------------------------------------------------------------------

LoadCaseChoiceDialog::LoadCaseChoiceDialog(const LoadCaseSet& cases, int selectedId, QWidget* parent) : CypeDialog(tr("Hipótesis"), parent) {
    auto* body = new QWidget(this);
    auto* row = new QHBoxLayout(body);
    row->setContentsMargins(4, 4, 4, 0);
    m_combo = new QComboBox(body);
    m_combo->setObjectName(QStringLiteral("loadCase"));
    m_combo->setMinimumWidth(110);
    fillLoadCaseCombo(m_combo, cases, selectedId);
    row->addWidget(m_combo);
    row->addStretch(1);
    contentLayout()->addWidget(body, 1);
    setupFooter(tr("Aceptar"), tr("Cancelar"));
    setFixedSize(180, 122);  // measured (cn_i1)
}

int LoadCaseChoiceDialog::loadCaseId() const { return m_combo->currentIndex() < 0 ? 0 : m_combo->currentData().toInt(); }

// ---- ViewedCaseDialog -------------------------------------------------------------------------------------------------------

ViewedCaseDialog::ViewedCaseDialog(const LoadCaseSet& cases, int viewedCaseId, bool viewAll, QWidget* parent) : CypeDialog(tr("Hipótesis vista"), parent) {
    auto* body = new QWidget(this);
    auto* column = new QVBoxLayout(body);
    column->setContentsMargins(10, 10, 10, 0);
    column->setSpacing(10);
    auto* row = new QHBoxLayout();
    row->setSpacing(8);
    row->addWidget(new QLabel(tr("Hipótesis"), body));
    m_combo = new QComboBox(body);
    m_combo->setObjectName(QStringLiteral("viewCase"));
    m_combo->setMinimumHeight(26);
    fillLoadCaseCombo(m_combo, cases, viewedCaseId);
    row->addWidget(m_combo, 1);
    column->addLayout(row);
    m_all = new QCheckBox(tr("Ver todas"), body);
    m_all->setObjectName(QStringLiteral("viewAll"));
    m_all->setChecked(viewAll);
    column->addWidget(m_all);
    column->addStretch(1);
    contentLayout()->addWidget(body, 1);
    setupFooter(tr("Aceptar"), tr("Cancelar"));
    // Measured (vh_04c): 185 px wide with «V(0°) H1», wider for a longer text of the combo.
    const QFontMetrics metrics(m_combo->font());
    int longest = metrics.horizontalAdvance(QStringLiteral("V(0°) H1"));
    for (int i = 0; i < m_combo->count(); ++i) longest = std::max(longest, metrics.horizontalAdvance(m_combo->itemText(i)));
    setFixedSize(185 + std::max(0, longest - metrics.horizontalAdvance(QStringLiteral("V(0°) H1"))), 156);
    m_combo->setFocus();
}

int ViewedCaseDialog::loadCaseId() const { return m_combo->currentIndex() < 0 ? 0 : m_combo->currentData().toInt(); }
bool ViewedCaseDialog::viewAll() const { return m_all->isChecked(); }

// ---- LoadScalesDialog -------------------------------------------------------------------------------------------------------

namespace {

/// Delegate of the scale cells: the editor opens with the text selected and TAB / Shift+TAB move to the next / previous cell of the table (row by
/// row, as measured), leaving the table after the last / before the first one.
class ScaleCellDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    std::function<void(bool)> step;

    void setEditorData(QWidget* editor, const QModelIndex& index) const override {
        QStyledItemDelegate::setEditorData(editor, index);
        if (auto* edit = qobject_cast<QLineEdit*>(editor)) edit->selectAll();
    }
    bool eventFilter(QObject* object, QEvent* event) override {
        if (event->type() == QEvent::KeyPress) {
            const int key = static_cast<QKeyEvent*>(event)->key();
            if (key == Qt::Key_Tab || key == Qt::Key_Backtab) {
                auto* editor = qobject_cast<QWidget*>(object);
                if (editor) {
                    emit commitData(editor);
                    emit closeEditor(editor, QAbstractItemDelegate::NoHint);
                    if (step) step(key == Qt::Key_Tab);
                    return true;
                }
            }
        }
        return QStyledItemDelegate::eventFilter(object, event);
    }
};

}  // namespace

LoadScalesDialog::LoadScalesDialog(const LoadCaseSet& cases, const LoadScales& initial, QWidget* parent)
    : CypeDialog(tr("Escalas"), parent), m_cases(cases), m_scales(initial) {
    auto* body = new QWidget(this);
    auto* column = new QVBoxLayout(body);
    column->setContentsMargins(6, 0, 6, 0);
    column->setSpacing(6);
    m_fixed = new QRadioButton(tr("Proporcionales a un valor fijo"), body);
    m_proportional = new QRadioButton(tr("Proporcionales al valor de la carga"), body);
    column->addWidget(m_fixed);
    column->addWidget(m_proportional);
    column->addWidget(separator(body));
    // Measured (cn_j1, oc_*): only the block of the selected radio is shown.
    m_fixedBlock = new QWidget(body);
    auto* fixedColumn = new QVBoxLayout(m_fixedBlock);
    fixedColumn->setContentsMargins(0, 0, 0, 0);
    auto* sizeRow = new QHBoxLayout();
    sizeRow->setContentsMargins(0, 0, 0, 0);
    sizeRow->setSpacing(4);
    sizeRow->addWidget(new QLabel(tr("Tamaño máximo para representación"), m_fixedBlock));
    m_size = new QLineEdit(threeDecimals(initial.fixedSizeM), m_fixedBlock);
    m_size->setObjectName(QStringLiteral("fixedSize"));
    m_size->setAlignment(Qt::AlignRight);
    m_size->setFixedWidth(52);
    m_size->setProperty("lastValid", m_size->text());
    sizeRow->addWidget(m_size);
    sizeRow->addWidget(new QLabel(tr("m"), m_fixedBlock));
    sizeRow->addStretch(1);
    fixedColumn->addLayout(sizeRow);
    fixedColumn->addStretch(1);
    column->addWidget(m_fixedBlock, 1);
    m_proportionalBlock = new QWidget(body);
    auto* proportionalColumn = new QVBoxLayout(m_proportionalBlock);
    proportionalColumn->setContentsMargins(0, 0, 0, 0);
    proportionalColumn->setSpacing(4);
    std::vector<const LoadCase*> ordered;  // measured: creation order (Peso propio, then CM 1, R 1, S 1, T 1 when T 1 was recreated), NOT the order of the combos
    for (const LoadCase& c : cases.cases) ordered.push_back(&c);
    m_table = new QTableWidget(static_cast<int>(ordered.size()), 2, m_proportionalBlock);
    m_table->setObjectName(QStringLiteral("scalesTable"));
    m_table->setHorizontalHeaderLabels({tr("Hipótesis"), tr("Escala")});
    m_table->verticalHeader()->hide();
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_loadingTable = true;
    for (int r = 0; r < m_table->rowCount(); ++r) {
        const LoadCase& c = *ordered[static_cast<std::size_t>(r)];
        auto* name = new QTableWidgetItem(loadCaseText(c));
        name->setFlags(name->flags() & ~Qt::ItemIsEditable);
        name->setData(Qt::UserRole, c.id);
        m_table->setItem(r, 0, name);
        auto* scale = new QTableWidgetItem(threeDecimals(loadScaleOfCase(initial, c.id)));
        scale->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_table->setItem(r, 1, scale);
        m_lastCell.push_back(scale->text());
    }
    m_loadingTable = false;
    m_table->setFixedHeight(135);
    auto* delegate = new ScaleCellDelegate(m_table);
    delegate->step = [this](bool next) {
        int row = m_table->currentRow(), col = m_table->currentColumn();
        if (row < 0) {
            row = 0;
            col = next ? -1 : 2;
        }
        const int index = row * 2 + col + (next ? 1 : -1);
        if (index < 0 || index >= m_table->rowCount() * 2) {
            focusNextPrevChild(next);  // out of the table: the next / previous control of the cycle
            return;
        }
        m_table->setCurrentCell(index / 2, index % 2);
        m_table->setFocus(Qt::TabFocusReason);
    };
    m_table->setItemDelegate(delegate);
    m_table->installEventFilter(this);
    connect(m_table, &QTableWidget::cellChanged, this, [this](int row, int column) { validateCell(row, column); });
    proportionalColumn->addWidget(m_table);
    auto* typesHeading = new QLabel(tr("Escalas de cargas por tipos"), m_proportionalBlock);
    setBold(typesHeading, true);
    proportionalColumn->addWidget(typesHeading);
    const char* typeNames[kLoadScaleTypeCount] = {"Uniforme", "Puntual", "Faja", "Triangular", "Trapecial", "Momento puntual", "Superficial",
                                                  "Temperatura", "Retracción"};
    auto* grid = new QGridLayout();
    for (int t = 0; t < kLoadScaleTypeCount; ++t) {
        grid->addWidget(new QLabel(tr(typeNames[t]), m_proportionalBlock), t, 0);
        auto* edit = new QLineEdit(threeDecimals(initial.perType[static_cast<std::size_t>(t)]), m_proportionalBlock);
        edit->setObjectName(QStringLiteral("typeScale%1").arg(t));
        edit->setAlignment(Qt::AlignRight);
        edit->setFixedWidth(62);
        edit->setProperty("lastValid", edit->text());
        edit->setToolTip(tr("Debe estar entre %1  y %2 .").arg(threeDecimals(kMinTypeScale), threeDecimals(kMaxTypeScale)));
        m_types[static_cast<std::size_t>(t)] = edit;
        grid->addWidget(edit, t, 1);
    }
    grid->setColumnStretch(0, 1);
    proportionalColumn->addLayout(grid);
    proportionalColumn->addStretch(1);
    column->addWidget(m_proportionalBlock, 1);
    contentLayout()->setContentsMargins(0, 6, 0, 0);
    contentLayout()->addWidget(body, 1);
    setupFooter(tr("Aceptar"), tr("Cancelar"));
    m_fixed->setChecked(initial.mode == LoadScaleMode::Fixed);
    m_proportional->setChecked(initial.mode == LoadScaleMode::Proportional);
    const auto sync = [this] {
        const bool fixed = m_fixed->isChecked();
        m_fixedBlock->setVisible(fixed);
        m_proportionalBlock->setVisible(!fixed);
        // Only the active radio is in the TAB cycle (measured).
        m_fixed->setFocusPolicy(fixed ? Qt::StrongFocus : Qt::ClickFocus);
        m_proportional->setFocusPolicy(fixed ? Qt::ClickFocus : Qt::StrongFocus);
    };
    connect(m_fixed, &QRadioButton::toggled, this, sync);
    sync();
    // Validation when a field is left (TAB or a click on Aceptar): editingFinished.
    connect(m_size, &QLineEdit::editingFinished, this, [this] { validateEdit(m_size); });
    for (QLineEdit* edit : m_types) connect(edit, &QLineEdit::editingFinished, this, [this, edit] { validateEdit(edit); });
    // Cycle of TAB (measured): table cells -> Uniforme .. Retracción -> Aceptar -> Cancelar -> active radio -> table.
    setTabOrder(m_fixed, m_proportional);
    setTabOrder(m_proportional, m_size);
    setTabOrder(m_size, m_table);
    QWidget* previous = m_table;
    for (QLineEdit* edit : m_types) {
        setTabOrder(previous, edit);
        previous = edit;
    }
    setTabOrder(previous, acceptButton());
    setTabOrder(acceptButton(), cancelButton());
    for (QToolButton* button : findChildren<QToolButton*>()) button->setFocusPolicy(Qt::NoFocus);  // the title-bar buttons are not in the cycle
    setFixedSize(306, 579);  // measured (vh_07c, vh_09: 306 x 579)
    // Initial focus: the size field (fixed) or the first cell of the table (proportional), with the text selected.
    if (m_fixed->isChecked()) {
        m_size->setFocus(Qt::TabFocusReason);
        m_size->selectAll();
    } else {
        m_table->setCurrentCell(0, 0);
        m_table->setFocus(Qt::TabFocusReason);
    }
}

std::vector<QWidget*> LoadScalesDialog::tabChain() const {
    std::vector<QWidget*> chain{m_table};
    for (QLineEdit* edit : m_types) chain.push_back(edit);
    chain.push_back(acceptButton());
    chain.push_back(cancelButton());
    chain.push_back(m_fixed->isChecked() ? static_cast<QWidget*>(m_fixed) : static_cast<QWidget*>(m_proportional));
    return chain;
}

void LoadScalesDialog::showEvent(QShowEvent* event) {
    CypeDialog::showEvent(event);
    // QDialog gives the initial focus to the default button; the original puts it in the first field.
    if (m_fixed->isChecked()) {
        m_size->setFocus(Qt::TabFocusReason);
        m_size->selectAll();
    } else {
        m_table->setFocus(Qt::TabFocusReason);
    }
}

bool LoadScalesDialog::eventFilter(QObject* watched, QEvent* event) {
    if (watched == m_table && event->type() == QEvent::KeyPress) {
        const int key = static_cast<QKeyEvent*>(event)->key();
        if (key == Qt::Key_Tab || key == Qt::Key_Backtab) {
            if (auto* delegate = dynamic_cast<ScaleCellDelegate*>(m_table->itemDelegate())) delegate->step(key == Qt::Key_Tab);
            return true;
        }
    }
    return CypeDialog::eventFilter(watched, event);
}

void LoadScalesDialog::validateEdit(QLineEdit* edit) {
    if (m_validating) return;
    m_validating = true;
    const bool isSize = edit == m_size;
    double v = 0.0;
    const bool number = readNumber(edit->text(), v);
    const bool inRange = number && (isSize ? loadFixedSizeInRange(v) : loadScaleValueInRange(v));
    if (inRange) {
        edit->setText(threeDecimals(v));
        edit->setProperty("lastValid", edit->text());
    } else {
        std::string text;
        if (!number) {
            text = nodalLoadNotNumberMessage();
        } else if (isSize) {
            text = loadFixedSizeRangeMessage();
        } else {
            const int t = static_cast<int>(std::find(m_types.begin(), m_types.end(), edit) - m_types.begin());
            text = loadScaleRangeMessage(loadScaleTypeName(static_cast<LoadScaleType>(t)));
        }
        edit->setText(edit->property("lastValid").toString());  // back to the last valid value; the focus is already in the next field
        messages::show(this, messages::Kind::Warning, QString::fromStdString(text));
    }
    m_validating = false;
}

void LoadScalesDialog::validateCell(int row, int column) {
    if (m_loadingTable || m_validating || column != 1) return;
    QTableWidgetItem* item = m_table->item(row, column);
    if (!item || row >= static_cast<int>(m_lastCell.size())) return;
    m_validating = true;
    double v = 0.0;
    const bool number = readNumber(item->text(), v);
    const bool inRange = number && loadScaleValueInRange(v);
    m_loadingTable = true;
    if (inRange) {
        item->setText(threeDecimals(v));
        m_lastCell[static_cast<std::size_t>(row)] = item->text();
        m_loadingTable = false;
    } else {
        item->setText(m_lastCell[static_cast<std::size_t>(row)]);
        m_loadingTable = false;
        // Measured (vh_o3): the name in the message is the column header, 'Escala'.
        const QString text = number ? QString::fromStdString(loadScaleRangeMessage("Escala")) : QString::fromStdString(nodalLoadNotNumberMessage());
        messages::show(this, messages::Kind::Warning, text);
    }
    m_validating = false;
}

void LoadScalesDialog::accept() {
    LoadScales out = m_scales;
    out.mode = m_fixed->isChecked() ? LoadScaleMode::Fixed : LoadScaleMode::Proportional;
    double size = 0.0;
    if (!readNumber(m_size->text(), size)) {
        messages::show(this, messages::Kind::Warning, QString::fromStdString(nodalLoadNotNumberMessage()));
        return;
    }
    if (!loadFixedSizeInRange(size)) {  // measured: 0.010 .. 100.000
        messages::show(this, messages::Kind::Warning, QString::fromStdString(loadFixedSizeRangeMessage()));
        return;
    }
    out.fixedSizeM = size;
    for (int t = 0; t < kLoadScaleTypeCount; ++t) {
        double v = 0.0;
        if (!readNumber(m_types[static_cast<std::size_t>(t)]->text(), v)) {
            messages::show(this, messages::Kind::Warning, QString::fromStdString(nodalLoadNotNumberMessage()));
            return;
        }
        if (!loadScaleValueInRange(v)) {
            messages::show(this, messages::Kind::Warning,
                           QString::fromStdString(loadScaleRangeMessage(loadScaleTypeName(static_cast<LoadScaleType>(t)))));
            return;
        }
        out.perType[static_cast<std::size_t>(t)] = v;
    }
    // Measured (cargas_laminas.md 13.5): accepting in "fixed" mode does not store the table of hypotheses (the table is hidden then).
    for (int r = 0; r < m_table->rowCount() && !m_fixed->isChecked(); ++r) {
        double scale = 0.0;
        if (!readNumber(m_table->item(r, 1)->text(), scale)) {
            messages::show(this, messages::Kind::Warning, QString::fromStdString(nodalLoadNotNumberMessage()));
            return;
        }
        if (!loadScaleValueInRange(scale)) {
            messages::show(this, messages::Kind::Warning, QString::fromStdString(loadScaleRangeMessage("Escala")));
            return;
        }
        const int id = m_table->item(r, 0)->data(Qt::UserRole).toInt();
        if (scale == kDefaultLoadCaseScale) out.perCase.erase(id);
        else out.perCase[id] = scale;
    }
    m_scales = out;
    CypeDialog::accept();
}

// ---- PrescribedDisplacementsDialog ------------------------------------------------------------------------------------------

PrescribedDisplacementsDialog::PrescribedDisplacementsDialog(const LoadCaseSet& cases, const std::array<bool, 6>& columns,
                                                             const std::vector<PrescribedDisplacement>& initial, QWidget* parent)
    : CypeDialog(tr("Desplazamientos prescritos"), parent), m_cases(cases), m_columns(columns), m_rows(initial) {
    auto* body = new QWidget(this);
    auto* outer = new QVBoxLayout(body);
    outer->setContentsMargins(6, 0, 6, 0);
    outer->setSpacing(4);
    auto* bar = new QHBoxLayout();
    bar->setSpacing(0);
    m_add = listButton(body, 0, tipText(tr("Añadir"), tr("Añadir nuevo elemento a la lista")));
    m_copy = listButton(body, 1, tipText(tr("Copiar"), tr("Copiar elemento seleccionado y añadirlo a la lista")));
    m_remove = listButton(body, 2, tipText(tr("Eliminar"), tr("Suprimir elemento seleccionado de la lista")));
    for (QToolButton* b : {m_add, m_copy, m_remove}) bar->addWidget(b);
    bar->addStretch(1);
    outer->addLayout(bar);
    static const char* headers[6] = {"Dx (mm)", "Dy (mm)", "Dz (mm)", "Gx (mRad)", "Gy (mRad)", "Gz (mRad)"};
    QStringList labels{tr("Hipótesis")};
    for (int dof = 0; dof < 6; ++dof)
        if (m_columns[static_cast<std::size_t>(dof)]) {
            m_dofOfColumn.push_back(dof);
            labels << QString::fromLatin1(headers[dof]);
        }
    m_table = new QTableWidget(0, static_cast<int>(labels.size()), body);
    m_table->setObjectName(QStringLiteral("prescribedTable"));
    m_table->setHorizontalHeaderLabels(labels);
    m_table->verticalHeader()->hide();
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_table->setStyleSheet(QStringLiteral("QTableWidget{background:white;} QTableWidget::item:selected{background:#CDE6F7;color:black;}"));
    outer->addWidget(m_table, 1);
    contentLayout()->setContentsMargins(0, 0, 0, 0);
    contentLayout()->addWidget(body, 1);
    setupFooter(tr("Aceptar"), tr("Cancelar"));
    setFixedSize(530, 370);  // measured (cn_n1)
    connect(m_add, &QToolButton::clicked, this, [this] { addRow(); });
    connect(m_copy, &QToolButton::clicked, this, [this] { copyRow(); });
    connect(m_remove, &QToolButton::clicked, this, [this] { removeRow(); });
    connect(m_table, &QTableWidget::cellChanged, this, [this](int row, int column) { onCellChanged(row, column); });
    refresh(m_rows.empty() ? -1 : 0);
}

int PrescribedDisplacementsDialog::currentRow() const {
    const int row = m_table->currentRow();
    return row >= 0 && row < static_cast<int>(m_rows.size()) ? row : -1;
}

int PrescribedDisplacementsDialog::columnOfDof(int dof) const {
    for (std::size_t i = 0; i < m_dofOfColumn.size(); ++i)
        if (m_dofOfColumn[i] == dof) return static_cast<int>(i) + 1;
    return -1;
}

void PrescribedDisplacementsDialog::refresh(int select) {
    m_loading = true;
    m_table->setRowCount(static_cast<int>(m_rows.size()));
    for (int r = 0; r < static_cast<int>(m_rows.size()); ++r) {
        const PrescribedDisplacement& row = m_rows[static_cast<std::size_t>(r)];
        auto* combo = new QComboBox(m_table);  // "hipótesis": a combo in the cell (measured)
        fillLoadCaseCombo(combo, m_cases, row.loadCaseId);
        connect(combo, &QComboBox::currentIndexChanged, this, [this, combo](int) {
            for (int rr = 0; rr < m_table->rowCount(); ++rr)
                if (m_table->cellWidget(rr, 0) == combo) onCellChanged(rr, 0);
        });
        m_table->setCellWidget(r, 0, combo);
        for (std::size_t i = 0; i < m_dofOfColumn.size(); ++i) {
            auto* item = new QTableWidgetItem(threeDecimals(row.values[static_cast<std::size_t>(m_dofOfColumn[i])]));
            item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            m_table->setItem(r, static_cast<int>(i) + 1, item);
        }
    }
    m_loading = false;
    if (select >= 0 && select < m_table->rowCount()) m_table->selectRow(select);
}

void PrescribedDisplacementsDialog::onCellChanged(int row, int column) {
    if (m_loading || row < 0 || row >= static_cast<int>(m_rows.size())) return;
    PrescribedDisplacement& target = m_rows[static_cast<std::size_t>(row)];
    if (column == 0) {
        if (auto* combo = qobject_cast<QComboBox*>(m_table->cellWidget(row, 0))) target.loadCaseId = combo->currentData().toInt();
        return;
    }
    QTableWidgetItem* item = m_table->item(row, column);
    if (!item || column - 1 >= static_cast<int>(m_dofOfColumn.size())) return;
    double& slot = target.values[static_cast<std::size_t>(m_dofOfColumn[static_cast<std::size_t>(column) - 1])];
    double value = 0.0;
    const bool ok = readNumber(item->text(), value);
    if (ok) slot = value;
    m_loading = true;  // always rewrite the cell with the stored value (3 decimals)
    item->setText(threeDecimals(slot));
    m_loading = false;
    if (!ok) messages::show(this, messages::Kind::Warning, QString::fromStdString(nodalLoadNotNumberMessage()));
}

void PrescribedDisplacementsDialog::setCellText(int row, int column, const QString& text) {
    if (QTableWidgetItem* item = m_table->item(row, column)) item->setText(text);  // cellChanged does the rest
}

void PrescribedDisplacementsDialog::addRow() {
    PrescribedDisplacement row;
    row.loadCaseId = m_cases.cases.empty() ? 0 : m_cases.cases.front().id;  // measured: the first load case, values 0.000
    m_rows.push_back(row);
    refresh(static_cast<int>(m_rows.size()) - 1);
}

void PrescribedDisplacementsDialog::copyRow() {
    const int row = currentRow();
    if (row < 0) return;
    m_rows.push_back(m_rows[static_cast<std::size_t>(row)]);
    refresh(static_cast<int>(m_rows.size()) - 1);
}

void PrescribedDisplacementsDialog::removeRow() {
    const int row = currentRow();
    if (row < 0) return;
    m_rows.erase(m_rows.begin() + row);
    refresh(m_rows.empty() ? -1 : std::min(row, static_cast<int>(m_rows.size()) - 1));
}

void PrescribedDisplacementsDialog::accept() {
    ExternalSupport probe;
    probe.type = ExternalSupportType::Fixed;
    probe.prescribed = m_rows;
    for (const SupportIssue& issue : checkExternalSupport(probe, m_cases))
        if (issue.severity == IssueSeverity::Error) {
            messages::show(this, messages::Kind::Warning, QString::fromStdString(issue.text));  // the window stays open (measured)
            return;
        }
    CypeDialog::accept();
}

// ---- LoadViewPanel ----------------------------------------------------------------------------------------------------------

LoadViewPanel::LoadViewPanel(QWidget* parent) : CypeDialog(tr("Propiedades - Hipótesis vista"), parent) {
    setModal(false);
    setMaximizable();
    contentLayout()->setContentsMargins(0, 0, 0, 0);
    contentLayout()->setSpacing(0);
    auto* body = new QWidget(this);
    auto* column = new QVBoxLayout(body);
    column->setContentsMargins(6, 6, 6, 6);
    column->setSpacing(6);
    auto* row = new QHBoxLayout();
    auto* label = new QLabel(tr("Hipótesis"), body);
    label->setToolTip(tr("Las cargas sobre nudos y barras que se introduzcan, por defecto, se asignarán a dicha hipótesis"));
    row->addWidget(label);
    m_combo = new QComboBox(body);
    m_combo->setObjectName(QStringLiteral("viewCase"));
    m_combo->setMinimumWidth(98);
    row->addWidget(m_combo);
    row->addStretch(1);
    column->addLayout(row);
    m_all = new QCheckBox(tr("Ver todas"), body);
    m_all->setObjectName(QStringLiteral("viewAll"));
    column->addWidget(m_all);
    column->addStretch(1);
    contentLayout()->addWidget(body, 1);
    connect(m_combo, &QComboBox::currentIndexChanged, this, [this](int) {
        if (!m_loading) emit viewChanged(caseId(), viewAll());
    });
    connect(m_all, &QCheckBox::toggled, this, [this](bool) {
        if (!m_loading) emit viewChanged(caseId(), viewAll());
    });
    setMinimumSize(277, 370);  // measured (cn_c1)
}

void LoadViewPanel::setCases(const LoadCaseSet& cases) {
    const int keep = caseId();
    m_loading = true;
    fillLoadCaseCombo(m_combo, cases, keep);
    m_loading = false;
}

int LoadViewPanel::caseId() const { return m_combo->currentIndex() < 0 ? 0 : m_combo->currentData().toInt(); }
bool LoadViewPanel::viewAll() const { return m_all->isChecked(); }

void LoadViewPanel::setCaseId(int id) {
    const int index = m_combo->findData(id);
    if (index >= 0) m_combo->setCurrentIndex(index);
}

void LoadViewPanel::setViewAll(bool on) { m_all->setChecked(on); }

void LoadViewPanel::done(int result) {
    CypeDialog::done(result);
    emit closed();
}

}  // namespace cc::app
