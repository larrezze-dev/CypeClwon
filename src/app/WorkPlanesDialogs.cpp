#include "WorkPlanesDialogs.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMessageBox>
#include <QPushButton>
#include <QRadioButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include "Icons.h"

namespace cc::app {

using namespace cc::core;

namespace {

QString qs(const std::string& s) { return QString::fromStdString(s); }

QString plainNumber(double value, int decimals) { return QLocale::c().toString(value, 'f', decimals); }

/// Parses a number typed by the user: point or comma as the decimal separator.
bool parseNumber(QString text, double& out) {
    text = text.trimmed();
    text.replace(QLatin1Char(','), QLatin1Char('.'));
    bool ok = false;
    const double v = QLocale::c().toDouble(text, &ok);
    if (ok) out = v;
    return ok;
}

QDoubleSpinBox* makeSpin(QWidget* parent, double min, double max, int decimals, double value) {
    auto* spin = new QDoubleSpinBox(parent);
    spin->setLocale(QLocale::c());  // the original shows "3.00" and "0.000"
    spin->setDecimals(decimals);
    spin->setRange(min, max);
    spin->setValue(value);
    spin->setButtonSymbols(QAbstractSpinBox::NoButtons);
    spin->setAlignment(Qt::AlignRight);
    spin->setStyleSheet(QStringLiteral("QDoubleSpinBox { background: white; border: 1px solid #DADADA; padding: 2px; }"));
    return spin;
}

QToolButton* toolButton(QWidget* parent, const QString& text, const QString& tip, const QString& colour = QString()) {
    auto* button = new QToolButton(parent);
    button->setText(text);
    button->setToolTip(tip);
    button->setAutoRaise(true);
    QString style = QStringLiteral("QToolButton { font-size: 18px; font-weight: bold; }");
    if (!colour.isEmpty()) style += QStringLiteral("QToolButton { color: %1; }").arg(colour);
    button->setStyleSheet(style);
    return button;
}

QComboBox* makePositionCombo(QWidget* parent, LabelPosition position) {
    auto* combo = new QComboBox(parent);
    combo->addItem(QObject::tr("Origen"));
    combo->addItem(QObject::tr("Extremo"));
    combo->setCurrentIndex(position == LabelPosition::Origin ? 0 : 1);
    return combo;
}

LabelPosition positionOf(const QComboBox* combo) {
    return combo->currentIndex() == 1 ? LabelPosition::End : LabelPosition::Origin;
}

QTableWidget* makeTable(QWidget* parent, const QStringList& headers) {
    auto* table = new QTableWidget(0, headers.size(), parent);
    table->setHorizontalHeaderLabels(headers);
    table->verticalHeader()->hide();
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed | QAbstractItemView::AnyKeyPressed);
    table->horizontalHeader()->setStretchLastSection(true);
    table->setStyleSheet(QStringLiteral("QTableWidget { background: white; border: 1px solid #DADADA; gridline-color: #E6E6E6; }"
                                        "QHeaderView::section { background: #F0F0F0; border: none; padding: 3px; }"));
    return table;
}

/// "Etiqueta: [edit]" style row: label on the left, editor on the right.
QHBoxLayout* labelledRow(QWidget* parent, const QString& label, QWidget* editor, int labelWidth, const QString& unit = QString()) {
    auto* row = new QHBoxLayout;
    auto* text = new QLabel(label, parent);
    text->setMinimumWidth(labelWidth);
    row->addWidget(text);
    row->addWidget(editor);
    if (!unit.isEmpty()) row->addWidget(new QLabel(unit, parent));
    row->addStretch(1);
    return row;
}

}  // namespace

PlanesPrompts PlanesPrompts::interactive() {
    PlanesPrompts p;
    p.warn = [](QWidget* parent, const QString& message) {
        QMessageBox box(QMessageBox::Warning, QObject::tr("Planos"), message, QMessageBox::Ok, parent);
        box.button(QMessageBox::Ok)->setText(QObject::tr("Aceptar"));
        box.exec();
    };
    p.askNoDefault = [](QWidget* parent, const QString& message) {
        QMessageBox box(QMessageBox::Question, QObject::tr("Planos"), message, QMessageBox::Yes | QMessageBox::No, parent);
        box.button(QMessageBox::Yes)->setText(QObject::tr("Sí"));
        box.button(QMessageBox::No)->setText(QObject::tr("No"));
        box.setDefaultButton(QMessageBox::No);
        return box.exec() == QMessageBox::Yes;
    };
    return p;
}

// ---------------------------------------------------------------------------------------------------------------------
// GridDialog

GridDialog::GridDialog(const WorkPlanes& initial, QWidget* parent) : CypeDialog(tr("Rejilla"), parent), m_planes(initial) {
    setMinimumSize(678, 621);
    resize(678, 621);
    setMaximizable();
    QVBoxLayout* body = contentLayout();

    auto* top = new QHBoxLayout;
    top->addStretch(1);
    auto* generate = new QPushButton(tr("Generación de líneas de rejilla"), this);
    generate->setAutoDefault(false);
    top->addWidget(generate);
    body->addLayout(top);
    connect(generate, &QPushButton::clicked, this, [this] { runGenerator(); });

    auto* panels = new QHBoxLayout;
    panels->setSpacing(10);
    const QString titles[2] = {tr("Líneas en la dirección X"), tr("Líneas en la dirección Y")};
    for (int axis = 0; axis < 2; ++axis) {
        auto* group = new QGroupBox(titles[axis], this);
        auto* column = new QVBoxLayout(group);
        auto* bar = new QHBoxLayout;
        auto* add = toolButton(group, QStringLiteral("+"), tr("Nueva línea"));
        auto* duplicate = new QToolButton(group);
        duplicate->setIcon(themedIcon(QStringLiteral("copy")));
        duplicate->setToolTip(tr("Duplicar la línea seleccionada"));
        duplicate->setAutoRaise(true);
        duplicate->setEnabled(false);  // dimmed until a row is chosen
        auto* remove = toolButton(group, QStringLiteral("✕"), tr("Borrar la línea seleccionada"), QStringLiteral("#D02020"));
        bar->addWidget(add);
        bar->addWidget(duplicate);
        bar->addWidget(remove);
        bar->addStretch(1);
        column->addLayout(bar);
        m_tables[axis] = makeTable(group, {tr("Etiqueta"), tr("Coordenada"), tr("Posición de la etiqueta")});
        m_tables[axis]->setColumnWidth(0, 70);
        m_tables[axis]->setColumnWidth(1, 90);
        column->addWidget(m_tables[axis], 1);
        panels->addWidget(group, 1);

        connect(add, &QToolButton::clicked, this, [this, axis] { askNewLine(axis); });
        connect(duplicate, &QToolButton::clicked, this, [this, axis] { duplicateLine(axis, m_tables[axis]->currentRow()); });
        connect(remove, &QToolButton::clicked, this, [this, axis] { deleteLine(axis, m_tables[axis]->currentRow()); });
        connect(m_tables[axis], &QTableWidget::currentCellChanged, this,
                [this, duplicate, axis](int row, int, int, int) { duplicate->setEnabled(row >= 0 && row < m_tables[axis]->rowCount()); });
        connect(m_tables[axis], &QTableWidget::itemChanged, this, [this, axis](QTableWidgetItem* item) { cellEdited(axis, item); });
    }
    body->addLayout(panels, 1);

    m_adjust = new QCheckBox(tr("Ajustar los elementos del modelo por cambios en la posición de las líneas de la rejilla"), this);
    m_adjust->setChecked(m_planes.adjustElements);
    connect(m_adjust, &QCheckBox::toggled, this, [this](bool on) { m_planes.adjustElements = on; });
    body->addWidget(m_adjust);
    setupFooter(tr("Aceptar"), tr("Cancelar"));
    rebuild(0);
    rebuild(1);
}

void GridDialog::rebuild(int axis) {
    m_rebuilding = true;
    QTableWidget* table = m_tables[axis];
    const std::vector<GridLine>& lines = linesOf(axis);
    table->setRowCount(static_cast<int>(lines.size()));
    for (int row = 0; row < static_cast<int>(lines.size()); ++row) {
        const GridLine& line = lines[static_cast<std::size_t>(row)];
        table->setItem(row, 0, new QTableWidgetItem(qs(line.label)));
        auto* coordinate = new QTableWidgetItem(plainNumber(line.coordinate, 3));
        coordinate->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        table->setItem(row, 1, coordinate);
        QComboBox* combo = makePositionCombo(table, line.labelPosition);
        table->setCellWidget(row, 2, combo);
        connect(combo, &QComboBox::currentIndexChanged, this, [this, axis, combo] {
            // Locate the row of this combo (rows move when lines are deleted).
            for (int r = 0; r < m_tables[axis]->rowCount(); ++r)
                if (m_tables[axis]->cellWidget(r, 2) == combo) linesOf(axis)[static_cast<std::size_t>(r)].labelPosition = positionOf(combo);
        });
    }
    m_rebuilding = false;
}

void GridDialog::cellEdited(int axis, QTableWidgetItem* item) {
    if (m_rebuilding || item->row() < 0 || item->row() >= static_cast<int>(linesOf(axis).size())) return;
    GridLine& line = linesOf(axis)[static_cast<std::size_t>(item->row())];
    if (item->column() == 0) {
        line.label = item->text().trimmed().toStdString();
    } else if (item->column() == 1) {
        double value = 0.0;
        if (parseNumber(item->text(), value) && std::abs(value) <= kMaxGridCoordinate) line.coordinate = value;
        // An unreadable or out-of-range value is discarded: the cell goes back to the stored coordinate.
        m_rebuilding = true;
        item->setText(plainNumber(line.coordinate, 3));
        m_rebuilding = false;
    }
}

void GridDialog::addLine(int axis, const GridLine& line) {
    linesOf(axis).push_back(line);
    rebuild(axis);
    m_tables[axis]->scrollToBottom();
}

void GridDialog::duplicateLine(int axis, int row) {
    std::vector<GridLine>& lines = linesOf(axis);
    if (row < 0 || row >= static_cast<int>(lines.size())) return;
    GridLine copy = lines[static_cast<std::size_t>(row)];
    copy.label = nextReference(copy.label);  // the duplicate's coordinate is left for the user to change (unverified in the original)
    lines.insert(lines.begin() + row + 1, copy);
    rebuild(axis);
    m_tables[axis]->selectRow(row + 1);
}

void GridDialog::deleteLine(int axis, int row) {
    std::vector<GridLine>& lines = linesOf(axis);
    if (row < 0 || row >= static_cast<int>(lines.size())) return;
    lines.erase(lines.begin() + row);
    rebuild(axis);
}

void GridDialog::askNewLine(int axis) {
    NewGridLineDialog dialog(this);
    if (dialog.exec() == QDialog::Accepted) addLine(axis, dialog.value());
}

bool GridDialog::applyGeneration(const GridSpec& spec, const std::function<bool()>& confirm) {
    WorkPlanes candidate = m_planes;
    try {
        generateGrid(candidate, spec);
    } catch (const std::invalid_argument& error) {
        prompts.warn(this, qs(error.what()));
        return false;
    }
    if ((!m_planes.gridX.empty() || !m_planes.gridY.empty()) && !confirm()) return false;
    m_planes.gridX = std::move(candidate.gridX);
    m_planes.gridY = std::move(candidate.gridY);
    rebuild(0);
    rebuild(1);
    return true;
}

void GridDialog::runGenerator() {
    GridGenerationDialog dialog(m_spec, this);
    if (dialog.exec() != QDialog::Accepted) return;
    m_spec = dialog.value();  // kept for the next opening
    applyGeneration(m_spec, [this] {
        return prompts.askNoDefault(this, tr("Se borrarán las líneas de la rejilla previamente definida. ¿Desea continuar?"));
    });
}

void GridDialog::accept() {
    for (int axis = 0; axis < 2; ++axis) {
        const std::string problem = validateGridLines(linesOf(axis));
        if (!problem.empty()) {
            prompts.warn(this, qs(problem));
            return;
        }
    }
    CypeDialog::accept();
}

// ---------------------------------------------------------------------------------------------------------------------
// GridGenerationDialog

GridGenerationDialog::GridGenerationDialog(const GridSpec& initial, QWidget* parent)
    : CypeDialog(tr("Generación de líneas de rejilla"), parent) {
    setFixedSize(487, 227);
    auto* both = new QHBoxLayout;
    both->addWidget(buildAxis(tr("Líneas en la dirección X"), initial.x, m_x), 1);
    both->addWidget(buildAxis(tr("Líneas en la dirección Y"), initial.y, m_y), 1);
    contentLayout()->addLayout(both);
    contentLayout()->addStretch(1);
    setupFooter(tr("Aceptar"), tr("Cancelar"));
}

QWidget* GridGenerationDialog::buildAxis(const QString& title, const GridAxisSpec& spec, AxisControls& c) {
    auto* group = new QGroupBox(title, this);
    auto* column = new QVBoxLayout(group);
    c.count = new QSpinBox(group);
    c.count->setRange(kMinGridLines, kMaxGridLines);
    c.count->setValue(spec.count);
    c.count->setButtonSymbols(QAbstractSpinBox::NoButtons);
    c.count->setAlignment(Qt::AlignRight);
    c.count->setFixedWidth(60);
    column->addLayout(labelledRow(group, tr("Número de líneas"), c.count, 120));
    // 0.05 .. 1000 m: the probable limits of the "longitud" unit of the original (unconfirmed, planos.md 10.3).
    c.spacing = makeSpin(group, 0.05, 1000.0, 2, spec.spacing);
    c.spacing->setFixedWidth(60);
    column->addLayout(labelledRow(group, tr("Separación entre líneas"), c.spacing, 120, tr("m")));
    c.label = new QLineEdit(spec.firstLabel.empty() ? QString() : qs(spec.firstLabel), group);
    c.label->setFixedWidth(60);
    column->addLayout(labelledRow(group, tr("Etiqueta inicial"), c.label, 120));
    c.position = makePositionCombo(group, spec.position);
    column->addLayout(labelledRow(group, tr("Posición de la etiqueta"), c.position, 120));
    return group;
}

GridSpec GridGenerationDialog::value() const {
    GridSpec spec;
    const auto read = [](const AxisControls& c) {
        GridAxisSpec axis;
        axis.count = c.count->value();
        axis.spacing = c.spacing->value();
        axis.firstLabel = c.label->text().trimmed().toStdString();
        axis.position = positionOf(c.position);
        return axis;
    };
    spec.x = read(m_x);
    spec.y = read(m_y);
    return spec;
}

// ---------------------------------------------------------------------------------------------------------------------
// NewGridLineDialog

NewGridLineDialog::NewGridLineDialog(QWidget* parent) : CypeDialog(tr("Nueva línea"), parent) {
    setFixedSize(300, 190);
    const GridLine defaults = newGridLine();
    m_coordinate = makeSpin(this, -kMaxGridCoordinate, kMaxGridCoordinate, 3, defaults.coordinate);
    m_coordinate->setFixedWidth(80);
    contentLayout()->addLayout(labelledRow(this, tr("Coordenada"), m_coordinate, 140, tr("m")));
    m_label = new QLineEdit(qs(defaults.label), this);
    m_label->setFixedWidth(80);
    contentLayout()->addLayout(labelledRow(this, tr("Etiqueta"), m_label, 140));
    m_position = makePositionCombo(this, defaults.labelPosition);
    contentLayout()->addLayout(labelledRow(this, tr("Posición de la etiqueta"), m_position, 140));
    contentLayout()->addStretch(1);
    setupFooter(tr("Aceptar"), tr("Cancelar"));
}

GridLine NewGridLineDialog::value() const {
    GridLine line;
    line.coordinate = m_coordinate->value();
    line.label = m_label->text().trimmed().toStdString();
    line.labelPosition = positionOf(m_position);
    return line;
}

// ---------------------------------------------------------------------------------------------------------------------
// LevelsDialog

LevelsDialog::LevelsDialog(const WorkPlanes& initial, QWidget* parent) : CypeDialog(tr("Niveles"), parent), m_planes(initial) {
    setMinimumSize(280, 420);
    resize(280, 420);
    setMaximizable();
    m_planes.levels = sortedLevels(m_planes.levels);
    QVBoxLayout* body = contentLayout();
    body->setSpacing(4);
    auto* bar = new QHBoxLayout;
    auto* add = toolButton(this, QStringLiteral("+"), tr("Nuevo nivel"));
    auto* remove = toolButton(this, QStringLiteral("✕"), tr("Borrar el nivel seleccionado"), QStringLiteral("#D02020"));
    auto* generate = new QToolButton(this);
    generate->setIcon(themedIcon(QStringLiteral("levels_generate")));
    generate->setToolTip(tr("Generar niveles"));
    generate->setAutoRaise(true);
    bar->addWidget(add);
    bar->addWidget(remove);
    bar->addWidget(generate);
    bar->addStretch(1);
    body->addLayout(bar);
    m_table = makeTable(this, {tr("Nivel"), tr("Cota absoluta (m)")});
    m_table->setColumnWidth(0, 100);
    body->addWidget(m_table, 1);
    setupFooter(tr("Aceptar"), tr("Cancelar"));

    connect(add, &QToolButton::clicked, this, [this] {
        NewLevelDialog dialog(this);
        if (dialog.exec() == QDialog::Accepted) addLevel(dialog.value());
    });
    connect(remove, &QToolButton::clicked, this, [this] { deleteLevel(m_table->currentRow()); });
    connect(generate, &QToolButton::clicked, this, [this] { runGenerator(); });
    connect(m_table, &QTableWidget::itemChanged, this, [this](QTableWidgetItem* item) { cellEdited(item); });
    rebuild();
}

void LevelsDialog::rebuild() {
    m_rebuilding = true;
    m_table->setRowCount(static_cast<int>(m_planes.levels.size()));
    for (int row = 0; row < static_cast<int>(m_planes.levels.size()); ++row) {
        const Level& level = m_planes.levels[static_cast<std::size_t>(row)];
        m_table->setItem(row, 0, new QTableWidgetItem(qs(level.name)));
        auto* elevation = new QTableWidgetItem(plainNumber(level.elevation, 3));
        elevation->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_table->setItem(row, 1, elevation);
    }
    m_rebuilding = false;
}

void LevelsDialog::cellEdited(QTableWidgetItem* item) {
    if (m_rebuilding || item->row() < 0 || item->row() >= static_cast<int>(m_planes.levels.size())) return;
    Level& level = m_planes.levels[static_cast<std::size_t>(item->row())];
    if (item->column() == 0) {
        level.name = item->text().trimmed().toStdString();
        return;
    }
    double value = 0.0;
    if (parseNumber(item->text(), value) && std::abs(value) <= kMaxGridCoordinate) level.elevation = value;
    m_planes.levels = sortedLevels(m_planes.levels);  // the list is shown by increasing elevation
    rebuild();
}

void LevelsDialog::addLevel(const Level& level) {
    m_planes.levels.push_back(level);
    m_planes.levels = sortedLevels(m_planes.levels);
    rebuild();
}

void LevelsDialog::deleteLevel(int row) {
    if (row < 0 || row >= static_cast<int>(m_planes.levels.size())) return;
    m_planes.levels.erase(m_planes.levels.begin() + row);
    rebuild();
}

bool LevelsDialog::applyGeneration(const std::string& prefix, int count, double start, double spacing, const std::function<bool()>& confirm) {
    std::vector<Level> generated;
    try {
        generated = generateLevels(prefix, count, start, spacing);
    } catch (const std::invalid_argument& error) {
        prompts.warn(this, qs(error.what()));
        return false;
    }
    if (!m_planes.levels.empty() && !confirm()) return false;
    m_planes.levels = sortedLevels(generated);
    rebuild();
    return true;
}

void LevelsDialog::runGenerator() {
    GenerateLevelsDialog dialog(this);
    if (dialog.exec() != QDialog::Accepted) return;
    applyGeneration(dialog.prefix(), dialog.count(), dialog.start(), dialog.spacing(), [this] {
        return prompts.askNoDefault(this, tr("Se borrarán los niveles previamente definidos. ¿Desea continuar?"));
    });
}

void LevelsDialog::accept() {
    // Levels are optional ("No es necesario definir niveles para introducir vigas"): only a started list is checked.
    if (!m_planes.levels.empty()) {
        const std::string problem = validateLevels(m_planes.levels);
        if (!problem.empty()) {
            prompts.warn(this, qs(problem));
            return;
        }
    }
    CypeDialog::accept();
}

// ---------------------------------------------------------------------------------------------------------------------
// NewLevelDialog, GenerateLevelsDialog

NewLevelDialog::NewLevelDialog(QWidget* parent) : CypeDialog(tr("Nuevo nivel"), parent) {
    setFixedSize(220, 155);
    const Level defaults = newLevel();
    m_name = new QLineEdit(qs(defaults.name), this);
    m_name->selectAll();
    contentLayout()->addLayout(labelledRow(this, tr("Referencia"), m_name, 70));
    m_elevation = makeSpin(this, -kMaxGridCoordinate, kMaxGridCoordinate, 3, defaults.elevation);
    m_elevation->setFixedWidth(70);
    contentLayout()->addLayout(labelledRow(this, tr("Cota absoluta"), m_elevation, 70, tr("m")));
    contentLayout()->addStretch(1);
    setupFooter(tr("Aceptar"), tr("Cancelar"));
}

Level NewLevelDialog::value() const { return Level{m_name->text().trimmed().toStdString(), m_elevation->value()}; }

GenerateLevelsDialog::GenerateLevelsDialog(QWidget* parent) : CypeDialog(tr("Generar niveles"), parent) {
    setFixedSize(385, 180);
    m_prefix = new QLineEdit(tr("Nivel"), this);  // "Nivel" (ed3dlib.msg 131655)
    m_prefix->setFixedWidth(80);
    m_prefix->selectAll();
    contentLayout()->addLayout(labelledRow(this, tr("Prefijo"), m_prefix, 120));
    m_count = new QSpinBox(this);
    m_count->setRange(kMinLevels, 10000);
    m_count->setValue(kMinLevels);  // default of the original: 2
    m_count->setButtonSymbols(QAbstractSpinBox::NoButtons);
    m_count->setAlignment(Qt::AlignRight);
    m_count->setFixedWidth(70);
    contentLayout()->addLayout(labelledRow(this, tr("Número de niveles"), m_count, 120));
    auto* row = new QHBoxLayout;
    row->addWidget(new QLabel(tr("Cota inicial"), this));
    row->addSpacing(34);
    m_start = makeSpin(this, -kMaxGridCoordinate, kMaxGridCoordinate, 3, 0.0);
    m_start->setFixedWidth(70);
    row->addWidget(m_start);
    row->addWidget(new QLabel(tr("m"), this));
    row->addSpacing(8);
    row->addWidget(new QLabel(tr("Separación"), this));
    m_spacing = makeSpin(this, -kMaxGridCoordinate, kMaxGridCoordinate, 3, 3.0);
    m_spacing->setFixedWidth(70);
    row->addWidget(m_spacing);
    row->addWidget(new QLabel(tr("m"), this));
    row->addStretch(1);
    contentLayout()->addLayout(row);
    contentLayout()->addStretch(1);
    setupFooter(tr("Aceptar"), tr("Cancelar"));
}

std::string GenerateLevelsDialog::prefix() const { return m_prefix->text().trimmed().toStdString(); }
int GenerateLevelsDialog::count() const { return m_count->value(); }
double GenerateLevelsDialog::start() const { return m_start->value(); }
double GenerateLevelsDialog::spacing() const { return m_spacing->value(); }

// ---------------------------------------------------------------------------------------------------------------------
// PlanesVisibilityDialog

PlanesVisibilityDialog::PlanesVisibilityDialog(QWidget* parent) : CypeDialog(tr("Mostrar/ocultar planos"), parent) {
    setFixedSize(243, 135);
    m_show = new QRadioButton(tr("Mostrar"), this);
    m_hide = new QRadioButton(tr("Ocultar"), this);
    m_hide->setChecked(true);  // default of the original
    contentLayout()->addWidget(m_show);
    contentLayout()->addWidget(m_hide);
    contentLayout()->addStretch(1);
    setupFooter(tr("Aceptar"), tr("Cancelar"));
}

bool PlanesVisibilityDialog::showPlanes() const { return m_show->isChecked(); }

void PlanesVisibilityDialog::setShowPlanes(bool show) { (show ? m_show : m_hide)->setChecked(true); }

}  // namespace cc::app
