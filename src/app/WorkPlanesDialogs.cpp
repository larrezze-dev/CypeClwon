#include "WorkPlanesDialogs.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QIcon>
#include <QPainter>
#include <QPixmap>
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
        // The original rounds the coordinate to three decimals when it is typed (3.0004 -> 3.000, 3.0009 -> 3.001).
        if (parseNumber(item->text(), value) && std::abs(value) <= kMaxGridCoordinate) line.coordinate = roundCoordinate(value);
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
    line.coordinate = roundCoordinate(m_coordinate->value());
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

// ---------------------------------------------------------------------------------------------------------------------
// ReferencesDialog

ReferencesDialog::ReferencesDialog(const ReferenceOptions& initial, QWidget* parent) : CypeDialog(tr("Referencias"), parent) {
    setFixedSize(246, 262);
    const auto box = [this](const QString& text, bool on, const QString& tip = QString()) {
        auto* check = new QCheckBox(text, this);
        check->setChecked(on);
        if (!tip.isEmpty()) check->setToolTip(tip);
        contentLayout()->addWidget(check);
        return check;
    };
    m_showGrid = box(tr("Mostrar la rejilla"), initial.showGrid);
    m_lines = box(tr("Líneas de referencia"), initial.referenceLines);
    auto* separator = new QFrame(this);
    separator->setFrameShape(QFrame::HLine);
    separator->setFrameShadow(QFrame::Sunken);
    contentLayout()->addWidget(separator);
    m_onlyWithNode = box(tr("Sólo las que tienen nudo"), initial.onlyWithNode,
                         tr("Activa o desactiva las líneas de referencia que no tienen ningún nudo."));
    m_parallel[0] = box(tr("Paralelas a X"), initial.parallelX, tr("Activa o desactiva las líneas de referencia paralelas al eje global X."));
    m_parallel[1] = box(tr("Paralelas a Y"), initial.parallelY, tr("Activa o desactiva las líneas de referencia paralelas al eje global Y."));
    m_parallel[2] = box(tr("Paralelas a Z"), initial.parallelZ, tr("Activa o desactiva las líneas de referencia paralelas al eje global Z."));
    m_colour = box(tr("Dibujar con color según dirección"), initial.colourByDirection,
                   tr("Dibuja las líneas de referencia con el mismo código de colores que los ejes principales de la obra."));
    contentLayout()->addStretch(1);
    setupFooter(tr("Aceptar"), tr("Cancelar"));
    // Clearing "Líneas de referencia" greys the five boxes below (measured in the original).
    const auto sync = [this](bool on) {
        for (QCheckBox* dependent : {m_onlyWithNode, m_parallel[0], m_parallel[1], m_parallel[2], m_colour}) dependent->setEnabled(on);
    };
    connect(m_lines, &QCheckBox::toggled, this, sync);
    sync(m_lines->isChecked());
}

ReferenceOptions ReferencesDialog::value() const {
    ReferenceOptions o;
    o.showGrid = m_showGrid->isChecked();
    o.referenceLines = m_lines->isChecked();
    o.onlyWithNode = m_onlyWithNode->isChecked();
    o.parallelX = m_parallel[0]->isChecked();
    o.parallelY = m_parallel[1]->isChecked();
    o.parallelZ = m_parallel[2]->isChecked();
    o.colourByDirection = m_colour->isChecked();
    return o;
}

// ---------------------------------------------------------------------------------------------------------------------
// DimensionsBar

namespace {

/// Simple drawn icon of a "Cotas" button (the original's artwork is not available): a dimension line with a plus, a cross,
/// or a struck-out look.
QIcon dimensionIcon(DimensionsBar::Command command) {
    QPixmap pixmap(24, 24);
    pixmap.fill(Qt::transparent);
    QPainter p(&pixmap);
    p.setRenderHint(QPainter::Antialiasing, true);
    const bool dimmed = command == DimensionsBar::Command::Hide;
    p.setPen(QPen(QColor(dimmed ? QStringLiteral("#8A8A8A") : QStringLiteral("#2060A0")), 1.5));
    p.drawLine(3, 15, 21, 15);  // the dimension line
    p.drawLine(3, 11, 3, 19);   // its two end ticks
    p.drawLine(21, 11, 21, 19);
    if (command == DimensionsBar::Command::Add) {
        p.setPen(QPen(QColor(QStringLiteral("#20A020")), 2));
        p.drawLine(12, 3, 12, 11);
        p.drawLine(8, 7, 16, 7);
    } else if (command == DimensionsBar::Command::Delete) {
        p.setPen(QPen(QColor(QStringLiteral("#D02020")), 2));
        p.drawLine(8, 3, 16, 11);
        p.drawLine(16, 3, 8, 11);
    } else if (command == DimensionsBar::Command::Hide) {
        p.setPen(QPen(QColor(QStringLiteral("#D02020")), 2));
        p.drawLine(4, 21, 20, 9);
    }
    p.end();
    return QIcon(pixmap);
}

}  // namespace

QString DimensionsBar::title(Command command) {
    switch (command) {
        case Command::Add: return tr("Añadir");
        case Command::Delete: return tr("Borrar");
        case Command::Show: return tr("Mostrar");
        case Command::Hide: return tr("Ocultar");
    }
    return QString();
}

QString DimensionsBar::tooltipBody(Command command) {
    switch (command) {
        case Command::Add:
            return tr("Definición de las dimensiones de la obra mediante la introducción de cotas entre los elementos que definen su geometría (lineas de referencia, nudos, planos, etc.).");
        case Command::Delete: return QString();
        case Command::Show:
            return tr("Permite introducir cotas adicionales para mejorar la acotación en planos o en pantalla. Las líneas de cota introducidas mediante esta opción no modifican las dimensiones de la obra, sólo se muestran como información calculada por el programa.");
        case Command::Hide: return tr("Selección de cotas para ocultarlas en planos.");
    }
    return QString();
}

QString DimensionsBar::statusText(Command command) {
    switch (command) {
        case Command::Add:
            return tr("Cotas - Añadir | - LMB: Seleccione las dos líneas de referencia o planos entre los que se define la cota que desea introducir. - RMB: Comenzar otra vez.");
        case Command::Delete:
            return tr("Cotas - Borrar | - LMB: Seleccione un grupo de elementos. - RMB: Actuar sobre los elementos seleccionados.");
        case Command::Show:
            return tr("Cotas - Mostrar | - LMB: Seleccione las dos líneas de referencia o planos entre los que se define la cota que desea introducir. - RMB: Comenzar otra vez.");
        case Command::Hide:
            return tr("Cotas - Ocultar | - LMB: Seleccione un grupo de elementos. - RMB: Actuar sobre los elementos seleccionados.");
    }
    return QString();
}

DimensionsBar::DimensionsBar(QWidget* parent) : QFrame(parent) {
    setObjectName(QStringLiteral("dimensionsBar"));
    setFrameShape(QFrame::Box);
    setStyleSheet(QStringLiteral("QFrame#dimensionsBar { background: #F4F4F4; border: 1px solid #B0B0B0; }"
                                 "QToolButton { border: 1px solid transparent; padding: 2px; }"
                                 "QToolButton:hover { border: 1px solid #7AA7D6; background: #E6F0FA; }"));
    setFixedSize(115, 45);
    auto* column = new QVBoxLayout(this);
    column->setContentsMargins(2, 0, 2, 2);
    column->setSpacing(0);
    auto* header = new QHBoxLayout;
    auto* caption = new QLabel(tr("Cotas"), this);
    caption->setStyleSheet(QStringLiteral("font-size: 10px; color: #404040;"));
    auto* close = new QToolButton(this);
    close->setText(QStringLiteral("✕"));
    close->setAutoRaise(true);
    close->setFixedSize(14, 14);
    close->setToolTip(tr("Cerrar"));
    header->addWidget(caption);
    header->addStretch(1);
    header->addWidget(close);
    column->addLayout(header);
    auto* row = new QHBoxLayout;
    row->setSpacing(0);
    for (Command command : {Command::Add, Command::Delete, Command::Show, Command::Hide}) {
        auto* b = new QToolButton(this);
        b->setIcon(dimensionIcon(command));
        b->setIconSize(QSize(20, 20));
        const QString body = tooltipBody(command);
        b->setToolTip(body.isEmpty() ? title(command) : QStringLiteral("<b>%1</b><br>%2").arg(title(command), body.toHtmlEscaped()));
        b->setStatusTip(statusText(command));  // shown while the pointer is over the button
        m_buttons[static_cast<int>(command)] = b;
        row->addWidget(b);
        connect(b, &QToolButton::clicked, this, [this, command] { emit commandChosen(command); });
    }
    column->addLayout(row);
    connect(close, &QToolButton::clicked, this, &QWidget::hide);
}

}  // namespace cc::app
