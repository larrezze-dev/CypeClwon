#include "OptionsLeafDialogs.h"

#include <functional>

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QRadioButton>
#include <QSpinBox>
#include <QTabWidget>
#include <QTableWidget>
#include <QToolButton>
#include <QVBoxLayout>

#include "Icons.h"

namespace cc::app {

using namespace cc::core;

namespace {

QDoubleSpinBox* spinBox(double value, double min, double max, int decimals, QWidget* parent) {
    auto* spin = new QDoubleSpinBox(parent);
    spin->setButtonSymbols(QAbstractSpinBox::NoButtons);
    spin->setDecimals(decimals);
    spin->setRange(min, max);
    spin->setValue(value);
    spin->setAlignment(Qt::AlignRight);
    spin->setLocale(QLocale::c());  // the original shows a decimal point
    spin->setFixedWidth(70);
    return spin;
}

QToolButton* pencilButton(QWidget* parent) {
    auto* button = new QToolButton(parent);
    button->setIcon(themedIcon(QStringLiteral("edit_pencil")));
    return button;
}

QLabel* greyBar(const QString& text, QWidget* parent) {
    auto* label = new QLabel(text, parent);
    label->setStyleSheet(QStringLiteral("background: #E4E4E4; padding: 2px 4px;"));
    return label;
}

/// "Ø6, Ø8, ..." text of a diameter list.
QString diameterText(const std::vector<int>& values) {
    QStringList parts;
    for (int v : values) parts << QStringLiteral("Ø%1").arg(v);
    return parts.join(QStringLiteral(", "));
}

/// Asks for a comma separated list of positive numbers; returns false when cancelled or invalid.
bool askList(QWidget* parent, const QString& title, const QString& label, const QString& current, std::vector<double>& out) {
    bool accepted = false;
    const QString text = QInputDialog::getText(parent, title, label, QLineEdit::Normal, current, &accepted);
    if (!accepted) return false;
    std::vector<double> values;
    for (const QString& part : text.split(QLatin1Char(','), Qt::SkipEmptyParts)) {
        bool ok = false;
        const double v = part.trimmed().toDouble(&ok);
        if (!ok || v <= 0) return false;
        values.push_back(v);
    }
    if (values.empty()) return false;
    out = values;
    return true;
}

}  // namespace

// ---------------------------------------------------------------- Hormigón de limpieza

CleaningConcreteDialog::CleaningConcreteDialog(const CleaningConcreteSettings& initial, QWidget* parent)
    : CypeDialog(tr("Hormigón de limpieza"), parent) {
    setFixedSize(277, 178);
    m_footings = new QCheckBox(tr("Zapatas"), this);
    m_pileCaps = new QCheckBox(tr("Encepados"), this);
    m_beams = new QCheckBox(tr("Vigas centradoras y de atado"), this);
    m_footings->setChecked(initial.footings);
    m_pileCaps->setChecked(initial.pileCaps);
    m_beams->setChecked(initial.tieAndCenteringBeams);
    for (QCheckBox* box : {m_footings, m_pileCaps, m_beams}) contentLayout()->addWidget(box);
    contentLayout()->addStretch(1);
    setupFooter(tr("Aceptar"), tr("Cancelar"));
}

CleaningConcreteSettings CleaningConcreteDialog::value() const {
    return {m_footings->isChecked(), m_pileCaps->isChecked(), m_beams->isChecked()};
}

// ---------------------------------------------------------------- Comprobación de fisuración

CrackingDialog::CrackingDialog(const CrackingSettings& initial, QWidget* parent)
    : CypeDialog(tr("Comprobación de fisuración"), parent) {
    setFixedSize(589, 435);
    const auto group = [&](const QString& title, QCheckBox*& check, QDoubleSpinBox*& limit, bool on, double value) {
        auto* box = new QGroupBox(title, this);
        auto* grid = new QGridLayout(box);
        check = new QCheckBox(tr("Comprobar fisuración"), box);
        check->setChecked(on);
        grid->addWidget(check, 0, 0, 1, 3);
        grid->addWidget(new QLabel(tr("Abertura límite de fisuras"), box), 1, 0);
        limit = spinBox(value, 0.001, 10.0, 2, box);
        limit->setEnabled(on);
        grid->addWidget(limit, 1, 1);
        grid->addWidget(new QLabel(tr("mm"), box), 1, 2);
        grid->setColumnStretch(0, 1);
        connect(check, &QCheckBox::toggled, limit, &QWidget::setEnabled);
        contentLayout()->addWidget(box);
    };
    group(tr("Zapatas"), m_footingCheck, m_footingLimit, initial.checkFootings, initial.footingLimitMm);
    group(tr("Vigas centradoras y de atado"), m_beamCheck, m_beamLimit, initial.checkBeams, initial.beamLimitMm);

    auto* recommended = new QLabel(this);
    recommended->setWordWrap(true);
    recommended->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    recommended->setStyleSheet(QStringLiteral("background: #EDEDED; padding: 6px;"));
    recommended->setText(
        tr("<b>Valores recomendados</b><br>Piezas protegidas: 0.3 mm<br>Piezas no protegidas (en medio no agresivo): 0.2 mm<br>"
           "Piezas no protegidas (en medio agresivo): 0.1 mm<br><br>Nota: si los recubrimientos empleados son superiores a los "
           "mínimos de la norma, se puede aumentar la abertura multiplicándola por la relación entre el recubrimiento empleado "
           "y el mínimo."));
    contentLayout()->addWidget(recommended, 1);
    setupFooter(tr("Aceptar"), tr("Cancelar"));
}

CrackingSettings CrackingDialog::value() const {
    CrackingSettings s;
    s.checkFootings = m_footingCheck->isChecked();
    s.checkBeams = m_beamCheck->isChecked();
    s.footingLimitMm = m_footingLimit->value();
    s.beamLimitMm = m_beamLimit->value();
    return s;
}

// ---------------------------------------------------------------- Encepados

PileCapOptionsDialog::PileCapOptionsDialog(const PileCapGeneralSettings& initial, QWidget* parent)
    : DefaultsDialog(tr("Encepados"), parent) {
    setFixedSize(428, 631);
    auto* tabs = new QTabWidget(this);
    auto* general = new QWidget(tabs);
    auto* column = new QVBoxLayout(general);
    const auto row = [&](QGridLayout* grid, int r, const QString& label, QDoubleSpinBox*& spin, double min, double max,
                         int decimals, const QString& unit) {
        grid->addWidget(new QLabel(label, general), r, 0);
        spin = spinBox(0, min, max, decimals, general);
        grid->addWidget(spin, r, 1);
        grid->addWidget(new QLabel(unit, general), r, 2);
        grid->setColumnStretch(0, 1);
    };
    auto* spans = new QGroupBox(tr("Vuelos"), general);
    auto* g1 = new QGridLayout(spans);
    row(g1, 0, tr("Vuelo mínimo sobre pilote"), m_overPile, 0, 10, 2, tr("m"));
    row(g1, 1, tr("Vuelo mínimo sobre apoyo"), m_overSupport, 0, 10, 2, tr("m"));
    column->addWidget(spans);

    auto* depths = new QGroupBox(tr("Cantos"), general);
    auto* g2 = new QGridLayout(depths);
    g2->addWidget(new QLabel(tr("Criterios de canto mínimo"), general), 0, 0);
    m_criterion = new QComboBox(general);
    m_criterion->addItem(tr("Canto mínimo según norma EHE"));  // the only entry seen (ticket TK-019)
    g2->addWidget(m_criterion, 0, 1, 1, 2);
    row(g2, 1, tr("Canto mínimo"), m_minDepth, 0.01, 20, 2, tr("m"));
    row(g2, 2, tr("Canto máximo"), m_maxDepth, 0.01, 20, 2, tr("m"));
    m_minDepth->setEnabled(false);  // enabled only by the other criteria, which were not captured
    column->addWidget(depths);

    auto* bars = new QGroupBox(tr("Barras"), general);
    auto* g3 = new QGridLayout(bars);
    row(g3, 0, tr("Separación mínima entre barras"), m_barSpacing, 0.001, 5, 2, tr("m"));
    row(g3, 1, tr("Diámetro máximo"), m_maxDiameter, 1, 100, 1, tr("mm"));
    column->addWidget(bars);

    auto* loads = new QGroupBox(tr("Cargas por pilote"), general);
    auto* g4 = new QGridLayout(loads);
    m_withLoads = new QCheckBox(tr("Con cargas por pilote"), general);
    g4->addWidget(m_withLoads, 0, 0, 1, 3);
    row(g4, 1, tr("Fracción de carga"), m_fraction, 0.01, 100, 2, QString());
    row(g4, 2, tr("Coeficiente de mayoración de carga"), m_factor, 0.01, 100, 2, QString());
    column->addWidget(loads);
    column->addStretch(1);
    connect(m_withLoads, &QCheckBox::toggled, m_fraction, &QWidget::setEnabled);
    connect(m_withLoads, &QCheckBox::toggled, m_factor, &QWidget::setEnabled);

    tabs->addTab(general, tr("Generales"));
    // The remaining tabs were not opened in the original yet (ticket TK-019); empty placeholders.
    for (const QString& title : {tr("Un pilote"), tr("Vigas de encepado"), tr("Vigas y parrillas")}) {
        auto* page = new QWidget(tabs);
        auto* l = new QVBoxLayout(page);
        auto* note = new QLabel(tr("Pestaña pendiente de capturar en el original (ticket TK-019)."), page);
        note->setWordWrap(true);
        l->addWidget(note);
        l->addStretch(1);
        tabs->addTab(page, title);
    }
    contentLayout()->addWidget(tabs, 1);
    setupDefaultsFooter();
    load(initial);
}

void PileCapOptionsDialog::load(const PileCapGeneralSettings& s) {
    m_overPile->setValue(s.minOverhangOverPileM);
    m_overSupport->setValue(s.minOverhangOverSupportM);
    m_criterion->setCurrentText(QString::fromStdString(s.depthCriterion));
    m_minDepth->setValue(s.minDepthM);
    m_maxDepth->setValue(s.maxDepthM);
    m_barSpacing->setValue(s.minBarSpacingM);
    m_maxDiameter->setValue(s.maxBarDiameterMm);
    m_withLoads->setChecked(s.withPileLoads);
    m_fraction->setValue(s.loadFraction);
    m_factor->setValue(s.loadFactor);
    m_fraction->setEnabled(s.withPileLoads);
    m_factor->setEnabled(s.withPileLoads);
}

PileCapGeneralSettings PileCapOptionsDialog::value() const {
    PileCapGeneralSettings s;
    s.minOverhangOverPileM = m_overPile->value();
    s.minOverhangOverSupportM = m_overSupport->value();
    s.depthCriterion = m_criterion->currentText().toStdString();
    s.minDepthM = m_minDepth->value();
    s.maxDepthM = m_maxDepth->value();
    s.minBarSpacingM = m_barSpacing->value();
    s.maxBarDiameterMm = m_maxDiameter->value();
    s.withPileLoads = m_withLoads->isChecked();
    s.loadFraction = m_fraction->value();
    s.loadFactor = m_factor->value();
    return s;
}

void PileCapOptionsDialog::resetToInstallationValues() { load(PileCapGeneralSettings{}); }

// ---------------------------------------------------------------- Vigas centradoras

CenteringBeamDialog::CenteringBeamDialog(const CenteringBeamSettings& initial, QWidget* parent)
    : DefaultsDialog(tr("Vigas centradoras"), parent) {
    setFixedSize(399, 245);
    auto* grid = new QGridLayout;
    grid->addWidget(new QLabel(tr("Ángulo tolerancia centrado"), this), 0, 0);
    m_angle = spinBox(0, 0, 89.9, 1, this);
    grid->addWidget(m_angle, 0, 1);
    grid->addWidget(new QLabel(tr("grados"), this), 0, 2);
    m_stirrups = new QCheckBox(tr("Disponer estribos dentro de la cimentación"), this);
    grid->addWidget(m_stirrups, 1, 0, 1, 3);
    grid->addWidget(new QLabel(tr("Tipo centrado"), this), 2, 0);
    m_type = new QComboBox(this);
    m_type->addItem(tr("Ley rectangular de tensiones bajo la zapata"));  // the only entry seen (ticket TK-019)
    grid->addWidget(m_type, 3, 0, 1, 2);
    auto* help = new QToolButton(this);
    help->setText(QStringLiteral("?"));
    help->setEnabled(false);  // the help of the original was not captured (ticket TK-019)
    grid->addWidget(help, 3, 2);
    m_minGeometric = new QCheckBox(tr("Mínimos geométricos a tracción"), this);
    grid->addWidget(m_minGeometric, 4, 0);
    m_minTension = spinBox(0, 0, 1, 5, this);
    grid->addWidget(m_minTension, 4, 1);
    grid->setColumnStretch(0, 1);
    contentLayout()->addLayout(grid);
    contentLayout()->addStretch(1);
    connect(m_minGeometric, &QCheckBox::toggled, m_minTension, &QWidget::setEnabled);
    setupDefaultsFooter();
    load(initial);
}

void CenteringBeamDialog::load(const CenteringBeamSettings& s) {
    m_angle->setValue(s.toleranceAngleDeg);
    m_stirrups->setChecked(s.stirrupsInsideFoundation);
    m_type->setCurrentText(QString::fromStdString(s.centeringType));
    m_minGeometric->setChecked(s.minTensionGeometric);
    m_minTension->setValue(s.minTensionRatio);
    m_minTension->setEnabled(s.minTensionGeometric);
}

CenteringBeamSettings CenteringBeamDialog::value() const {
    CenteringBeamSettings s;
    s.toleranceAngleDeg = m_angle->value();
    s.stirrupsInsideFoundation = m_stirrups->isChecked();
    s.centeringType = m_type->currentText().toStdString();
    s.minTensionGeometric = m_minGeometric->isChecked();
    s.minTensionRatio = m_minTension->value();
    return s;
}

void CenteringBeamDialog::resetToInstallationValues() { load(CenteringBeamSettings{}); }

// ---------------------------------------------------------------- Tablas de armado de cimentación

FoundationTableDialog::FoundationTableDialog(Kind kind, ReinforcementTableSource initial, QWidget* parent)
    : CypeDialog(tr("Tablas de armado"), parent) {
    const bool footings = kind == Kind::Footings;
    setFixedSize(footings ? 481 : 491, 267);
    auto* intro = new QLabel(footings ? tr("Esta tabla de armado se utiliza para dimensionar el armado inferior y superior de las "
                                           "zapatas aisladas.")
                                      : tr("Esta tabla de armado se utiliza para el dimensionamiento de las secciones de vigas de "
                                           "atado."),
                             this);
    intro->setWordWrap(true);
    contentLayout()->addWidget(intro);
    const auto line = [&](QRadioButton*& radio, const QString& text, const QString& button, bool enabled, const QString& tip) {
        auto* row = new QHBoxLayout;
        radio = new QRadioButton(text, this);
        radio->setToolTip(tip);
        row->addWidget(radio, 1);
        auto* action = new QPushButton(button, this);
        action->setEnabled(enabled);
        row->addWidget(action);
        contentLayout()->addLayout(row);
        return action;
    };
    // The tables themselves were not captured (ticket TK-019): the buttons that would open them stay disabled.
    line(m_predefined, tr("Tabla predefinida"), footings ? tr("Tabla de armado para zapatas") : tr("Tabla de vigas de atado"),
         false,
         tr("Seleccionar una tabla predefinida. Permite seleccionar una tabla predefinida. En la mayoría de los casos solamente "
            "existirá una tabla predefinida."));
    line(m_library, tr("Tabla de biblioteca"), tr("..."), false, QString());
    line(m_special, tr("Tabla especial"), tr("Editar tabla"), false, QString());
    m_library->setEnabled(false);
    m_special->setEnabled(false);
    (initial == ReinforcementTableSource::Library ? m_library : initial == ReinforcementTableSource::Special ? m_special : m_predefined)
        ->setChecked(true);
    contentLayout()->addStretch(1);
    setupFooter(tr("Aceptar"), tr("Cancelar"));
}

ReinforcementTableSource FoundationTableDialog::value() const {
    return m_library->isChecked() ? ReinforcementTableSource::Library
           : m_special->isChecked() ? ReinforcementTableSource::Special
                                    : ReinforcementTableSource::Predefined;
}

// ---------------------------------------------------------------- Armadura transversal

QString TransverseTableDialog::summarise(const std::vector<double>& values, int decimals) {
    QStringList parts;
    const auto text = [&](double v) { return QString::number(v, 'f', decimals); };
    if (values.size() <= 5) {
        for (double v : values) parts << text(v);
    } else {
        for (int i = 0; i < 4; ++i) parts << text(values[static_cast<std::size_t>(i)]);
        parts << QStringLiteral("...") << text(values.back());
    }
    return parts.join(QStringLiteral(", "));
}

TransverseTableDialog::TransverseTableDialog(const TransverseReinforcementTable& initial, QWidget* parent)
    : CypeDialog(tr("Armadura transversal"), parent), m_table(initial) {
    setFixedSize(343, 229);
    const auto line = [&](QLabel*& label, const std::function<void()>& edit) {
        auto* row = new QHBoxLayout;
        label = new QLabel(this);
        label->setWordWrap(true);
        row->addWidget(label, 1);
        auto* button = pencilButton(this);
        connect(button, &QToolButton::clicked, this, edit);
        row->addWidget(button, 0, Qt::AlignVCenter);
        contentLayout()->addLayout(row);
    };
    line(m_diameters, [this] {
        std::vector<double> values;
        QStringList current;
        for (int d : m_table.diameters) current << QString::number(d);
        if (!askList(this, tr("Armadura transversal"), tr("Diámetros admisibles (mm, separados por comas)"),
                     current.join(QStringLiteral(", ")), values)) {
            return;
        }
        m_table.diameters.assign(values.begin(), values.end());
        refresh();
    });
    line(m_spacings, [this] {
        std::vector<double> values;
        QStringList current;
        for (double s : m_table.spacingsCm) current << QString::number(s);
        if (!askList(this, tr("Armadura transversal"), tr("Separaciones (cm, separadas por comas)"),
                     current.join(QStringLiteral(", ")), values)) {
            return;
        }
        m_table.spacingsCm = values;
        refresh();
    });
    contentLayout()->addStretch(1);
    setupFooter(tr("Aceptar"), tr("Cancelar"));
    refresh();
}

void TransverseTableDialog::refresh() {
    m_diameters->setText(tr("Diámetros admisibles: %1").arg(diameterText(m_table.diameters)));
    m_spacings->setText(tr("Separaciones (cm): %1").arg(summarise(m_table.spacingsCm, 1)));
}

// ---------------------------------------------------------------- Portaestribos / piel

namespace {
QTableWidgetItem* flagItem(bool on) {
    auto* item = new QTableWidgetItem(on ? QStringLiteral("✓") : QStringLiteral("✗"));
    item->setTextAlignment(Qt::AlignCenter);
    item->setFlags(Qt::ItemIsEnabled);
    return item;
}
}  // namespace

DiameterAdmissibilityDialog::DiameterAdmissibilityDialog(Kind kind, const DiameterAdmissibility& initial, QWidget* parent)
    : CypeDialog(kind == Kind::StirrupHolder ? tr("Armadura portaestribos") : tr("Armadura de piel"), parent) {
    if (kind == Kind::StirrupHolder) setFixedSize(359, 347); else setFixedSize(229, 315);
    const auto& diameters = DiameterAdmissibility::diameters();
    m_table = new QTableWidget(static_cast<int>(diameters.size()), 2, this);
    m_table->setHorizontalHeaderLabels({tr("Diámetros"), tr("Admisible")});
    m_table->verticalHeader()->hide();
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_table->setSelectionMode(QAbstractItemView::NoSelection);
    for (std::size_t i = 0; i < diameters.size(); ++i) {
        auto* name = new QTableWidgetItem(QStringLiteral("Ø%1").arg(diameters[i]));
        name->setFlags(Qt::ItemIsEnabled);
        m_table->setItem(static_cast<int>(i), 0, name);
        m_table->setItem(static_cast<int>(i), 1, flagItem(initial.admissible[i]));
    }
    connect(m_table, &QTableWidget::cellClicked, this, [this](int row, int column) {
        if (column == 1) toggleRow(row);
    });
    contentLayout()->addWidget(m_table, 1);
    if (kind == Kind::StirrupHolder) {
        m_assembly = new QCheckBox(tr("Disponer armadura de montaje en alas"), this);
        m_assembly->setChecked(initial.assemblyInFlanges);
        contentLayout()->addWidget(m_assembly);
    }
    setupFooter(tr("Aceptar"), tr("Cancelar"));
}

void DiameterAdmissibilityDialog::toggleRow(int row) {
    const bool now = m_table->item(row, 1)->text() == QStringLiteral("✓");
    m_table->item(row, 1)->setText(now ? QStringLiteral("✗") : QStringLiteral("✓"));
}

DiameterAdmissibility DiameterAdmissibilityDialog::value() const {
    DiameterAdmissibility s;
    for (std::size_t i = 0; i < s.admissible.size(); ++i) {
        s.admissible[i] = m_table->item(static_cast<int>(i), 1)->text() == QStringLiteral("✓");
    }
    s.assemblyInFlanges = m_assembly && m_assembly->isChecked();
    return s;
}

// ---------------------------------------------------------------- Refuerzos superior / inferior

ReinforcementBarDialog::ReinforcementBarDialog(const QString& title, const ReinforcementBarTable& initial, QWidget* parent)
    : CypeDialog(title, parent), m_table(initial) {
    setFixedSize(356, 582);
    auto* row = new QHBoxLayout;
    m_diameters = new QLabel(this);
    row->addWidget(m_diameters, 1);
    auto* pencil = pencilButton(this);
    connect(pencil, &QToolButton::clicked, this, [this] {
        std::vector<double> values;
        QStringList current;
        for (int d : m_table.diameters) current << QString::number(d);
        if (!askList(this, windowTitle(), tr("Diámetros admisibles (mm, separados por comas)"), current.join(QStringLiteral(", ")),
                     values)) {
            return;
        }
        m_table.diameters.assign(values.begin(), values.end());
        refresh();
    });
    row->addWidget(pencil);
    contentLayout()->addLayout(row);

    auto* spacing = new QHBoxLayout;
    spacing->addWidget(new QLabel(tr("Separación libre mínima entre barras"), this), 1);
    m_spacing = spinBox(initial.minClearSpacingCm, 0, 100, 1, this);
    spacing->addWidget(m_spacing);
    spacing->addWidget(new QLabel(tr("cm"), this));
    contentLayout()->addLayout(spacing);

    const auto& bars = ReinforcementBarTable::reinforcementDiameters();
    const auto& assembly = DiameterAdmissibility::diameters();
    const auto matrix = [&](QTableWidget*& table, const QString& caption, int rows, int index) {
        contentLayout()->addWidget(greyBar(caption, this));
        table = new QTableWidget(rows, static_cast<int>(bars.size()), this);
        QStringList columns;
        for (int d : bars) columns << QStringLiteral("Ø%1").arg(d);
        table->setHorizontalHeaderLabels(columns);
        QStringList rowNames;
        for (int r = 0; r < rows; ++r) {
            rowNames << QStringLiteral("Ø%1").arg(index == 0 ? assembly[static_cast<std::size_t>(r)] : bars[static_cast<std::size_t>(r)]);
        }
        table->setVerticalHeaderLabels(rowNames);
        table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
        table->setSelectionMode(QAbstractItemView::NoSelection);
        for (int r = 0; r < rows; ++r) {
            for (int c = 0; c < static_cast<int>(bars.size()); ++c) {
                table->setItem(r, c, flagItem(index == 0 ? m_table.assemblyVsFirst[static_cast<std::size_t>(r)][static_cast<std::size_t>(c)]
                                                         : m_table.firstVsSecond[static_cast<std::size_t>(r)][static_cast<std::size_t>(c)]));
            }
        }
        connect(table, &QTableWidget::cellClicked, this, [this, index](int r, int c) { toggleCell(index, r, c); });
        contentLayout()->addWidget(table, 1);
    };
    matrix(m_first, tr("Montaje (filas) / Refuerzo 1 (columnas)"), static_cast<int>(assembly.size()), 0);
    matrix(m_second, tr("Refuerzo 1 (filas) / Refuerzo 2 (columnas)"), static_cast<int>(bars.size()), 1);
    setupFooter(tr("Aceptar"), tr("Cancelar"));
    refresh();
}

void ReinforcementBarDialog::refresh() { m_diameters->setText(tr("Diámetros admisibles: %1").arg(diameterText(m_table.diameters))); }

void ReinforcementBarDialog::toggleCell(int matrix, int row, int column) {
    QTableWidget* table = matrix == 0 ? m_first : m_second;
    bool& flag = matrix == 0 ? m_table.assemblyVsFirst[static_cast<std::size_t>(row)][static_cast<std::size_t>(column)]
                             : m_table.firstVsSecond[static_cast<std::size_t>(row)][static_cast<std::size_t>(column)];
    flag = !flag;
    table->item(row, column)->setText(flag ? QStringLiteral("✓") : QStringLiteral("✗"));
}

ReinforcementBarTable ReinforcementBarDialog::value() const {
    ReinforcementBarTable s = m_table;
    s.minClearSpacingCm = m_spacing->value();
    return s;
}

// ---------------------------------------------------------------- Límites de flecha: editores

DeflectionEditorDialog::DeflectionEditorDialog(const QString& material, const DeflectionLimitSet& initial, QWidget* parent)
    : CypeDialog(tr("Límites de flecha (%1)").arg(material), parent), m_set(initial) {
    if (initial.hasInfinite) setFixedSize(405, 481);
    else if (initial.hasDamageable) setFixedSize(385, 391);
    else setFixedSize(385, 356);
    const QStringList combinations = {tr("Característica"), tr("Frecuente"), tr("Cuasipermanente")};
    auto* layout = contentLayout();
    layout->setSpacing(3);
    addRows(layout, tr("Instantánea, por tipo de hipótesis simple"), Section::InstantSimple, {tr("Carga permanente"), tr("Sobrecarga")});
    addRows(layout, tr("Instantánea, por tipo de combinación de acciones"), Section::InstantCombination, combinations);
    if (initial.hasInfinite) addRows(layout, tr("A plazo infinito"), Section::Infinite, combinations);
    addRows(layout, tr("Activa a largo plazo"), Section::Active, combinations);
    if (initial.hasDamageable) {
        auto* row = new QHBoxLayout;
        row->addWidget(new QLabel(tr("Construcción del elemento dañable"), this), 1);
        m_months = new QSpinBox(this);
        m_months->setRange(0, 120);
        m_months->setValue(initial.damageableMonths);
        row->addWidget(m_months);
        row->addWidget(new QLabel(tr("x meses"), this));
        layout->addLayout(row);
    }
    layout->addStretch(1);
    setupFooter(tr("Aceptar"), tr("Cancelar"));
}

DeflectionLimitSet DeflectionEditorDialog::value() const {
    DeflectionLimitSet s = m_set;
    if (m_months) s.damageableMonths = m_months->value();
    return s;
}

int* DeflectionEditorDialog::slot(Section section, int index) {
    const auto i = static_cast<std::size_t>(index);
    switch (section) {
        case Section::InstantSimple: return &m_set.instantSimple.at(i);
        case Section::InstantCombination: return &m_set.instantCombination.at(i);
        case Section::Infinite: return &m_set.infinite.at(i);
        case Section::Active: return &m_set.active.at(i);
    }
    return nullptr;
}

const int* DeflectionEditorDialog::slot(Section section, int index) const {
    return const_cast<DeflectionEditorDialog*>(this)->slot(section, index);
}

int DeflectionEditorDialog::limit(Section section, int index) const { return *slot(section, index); }

void DeflectionEditorDialog::setLimit(Section section, int index, int denominator) {
    *slot(section, index) = denominator < 0 ? 0 : denominator;
    for (auto& [key, label] : m_labels) {
        if (key.first == section && key.second == index) {
            label->setText(denominator > 0 ? QStringLiteral("L/ %1").arg(denominator) : QStringLiteral("----"));
        }
    }
}

void DeflectionEditorDialog::addRows(QVBoxLayout* layout, const QString& title, Section section, const QStringList& names) {
    layout->addWidget(greyBar(title, this));
    for (int i = 0; i < names.size(); ++i) {
        auto* row = new QHBoxLayout;
        row->addWidget(new QLabel(names[i], this), 1);
        auto* value = new QLabel(this);
        value->setMinimumWidth(60);
        row->addWidget(value);
        m_labels.push_back({{section, i}, value});
        auto* pencil = pencilButton(this);
        connect(pencil, &QToolButton::clicked, this, [this, section, i] {
            // The input window of the original was not captured (ticket TK-019): a plain integer prompt.
            bool ok = false;
            const int v = QInputDialog::getInt(this, windowTitle(), tr("Límite L/ (0: sin límite)"), limit(section, i), 0, 100000, 1, &ok);
            if (ok) setLimit(section, i, v);
        });
        row->addWidget(pencil);
        layout->addLayout(row);
        setLimit(section, i, limit(section, i));
    }
}

}  // namespace cc::app
