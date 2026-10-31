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

namespace {

/// Tab page with a note for the tabs that were not opened in the original yet (ticket TK-019).
QWidget* placeholderPage(QWidget* parent) {
    auto* page = new QWidget(parent);
    auto* l = new QVBoxLayout(page);
    auto* note = new QLabel(QObject::tr("Pestaña pendiente de capturar en el original (ticket TK-019)."), page);
    note->setWordWrap(true);
    l->addWidget(note);
    l->addStretch(1);
    return page;
}

/// Label + spin box + unit row in a grid.
QDoubleSpinBox* gridSpin(QGridLayout* grid, int r, const QString& label, double min, double max, int decimals,
                         const QString& unit, QWidget* parent) {
    grid->addWidget(new QLabel(label, parent), r, 0);
    auto* spin = spinBox(0, min, max, decimals, parent);
    grid->addWidget(spin, r, 1);
    grid->addWidget(new QLabel(unit, parent), r, 2);
    grid->setColumnStretch(0, 1);
    return spin;
}

}  // namespace

PileCapOptionsDialog::PileCapOptionsDialog(const PileCapOptions& initial, QWidget* parent)
    : DefaultsDialog(tr("Encepados"), parent) {
    setFixedSize(428, 631);
    auto* tabs = new QTabWidget(this);
    auto* general = new QWidget(tabs);
    auto* column = new QVBoxLayout(general);
    auto* spans = new QGroupBox(tr("Vuelos"), general);
    auto* g1 = new QGridLayout(spans);
    m_overPile = gridSpin(g1, 0, tr("Vuelo mínimo sobre pilote"), 0, 10, 2, tr("m"), general);
    m_overSupport = gridSpin(g1, 1, tr("Vuelo mínimo sobre apoyo"), 0, 10, 2, tr("m"), general);
    column->addWidget(spans);

    auto* depths = new QGroupBox(tr("Cantos"), general);
    auto* g2 = new QGridLayout(depths);
    g2->addWidget(new QLabel(tr("Criterios de canto mínimo"), general), 0, 0);
    m_criterion = new QComboBox(general);
    m_criterion->addItem(tr("Canto mínimo según norma EHE"));  // the only entry seen (ticket TK-019)
    g2->addWidget(m_criterion, 0, 1, 1, 2);
    m_minDepth = gridSpin(g2, 1, tr("Canto mínimo"), 0.01, 20, 2, tr("m"), general);
    m_maxDepth = gridSpin(g2, 2, tr("Canto máximo"), 0.01, 20, 2, tr("m"), general);
    m_minDepth->setEnabled(false);  // enabled only by the other criteria, which were not captured
    column->addWidget(depths);

    auto* bars = new QGroupBox(tr("Barras"), general);
    auto* g3 = new QGridLayout(bars);
    m_barSpacing = gridSpin(g3, 0, tr("Separación mínima entre barras"), 0.001, 5, 2, tr("m"), general);
    m_maxDiameter = gridSpin(g3, 1, tr("Diámetro máximo"), 1, 100, 1, tr("mm"), general);
    column->addWidget(bars);

    auto* loads = new QGroupBox(tr("Cargas por pilote"), general);
    auto* g4 = new QGridLayout(loads);
    m_withLoads = new QCheckBox(tr("Con cargas por pilote"), general);
    g4->addWidget(m_withLoads, 0, 0, 1, 3);
    m_fraction = gridSpin(g4, 1, tr("Fracción de carga"), 0.01, 100, 2, QString(), general);
    m_factor = gridSpin(g4, 2, tr("Coeficiente de mayoración de carga"), 0.01, 100, 2, QString(), general);
    column->addWidget(loads);
    column->addStretch(1);
    connect(m_withLoads, &QCheckBox::toggled, m_fraction, &QWidget::setEnabled);
    connect(m_withLoads, &QCheckBox::toggled, m_factor, &QWidget::setEnabled);
    tabs->addTab(general, tr("Generales"));

    // "Un pilote"
    auto* single = new QWidget(tabs);
    auto* sl = new QVBoxLayout(single);
    auto* sg = new QGridLayout;
    m_armingDepth = gridSpin(sg, 0, tr("Porcentaje de profundidad de armado"), 0.01, 100, 2, QString(), single);
    sl->addLayout(sg);
    m_diagonalStirrups = new QCheckBox(tr("Disponer estribos diagonales"), single);
    sl->addWidget(m_diagonalStirrups);
    sl->addStretch(1);
    tabs->addTab(single, tr("Un pilote"));

    // "Vigas de encepado"
    auto* beams = new QWidget(tabs);
    auto* bl = new QVBoxLayout(beams);
    const auto hookGroup = [&](const QString& title, QComboBox*& combo, QDoubleSpinBox*& length) {
        auto* box = new QGroupBox(title, beams);
        auto* grid = new QGridLayout(box);
        grid->addWidget(new QLabel(tr("Tipo de patilla"), beams), 0, 0);
        combo = new QComboBox(beams);
        combo->addItem(tr("Colocar patilla mínima"));  // the only entry seen (ticket TK-019)
        grid->addWidget(combo, 0, 1, 1, 2);
        length = gridSpin(grid, 1, tr("Longitud máxima de patilla"), 0.01, 100, 2, tr("m"), beams);
        length->setEnabled(false);  // enabled only by the other hook types, not captured
        bl->addWidget(box);
    };
    hookGroup(tr("Armadura superior"), m_topHook, m_topHookLength);
    hookGroup(tr("Armadura inferior"), m_bottomHook, m_bottomHookLength);
    auto* bg = new QGridLayout;
    m_hoopDiameter = gridSpin(bg, 0, tr("Diámetro máximo de cercos"), 1, 100, 1, tr("mm"), beams);
    m_strutAngle = gridSpin(bg, 1, tr("Ángulo bielas de hormigón"), 1, 89.99, 2, tr("grados"), beams);
    bl->addLayout(bg);
    bl->addStretch(1);
    tabs->addTab(beams, tr("Vigas de encepado"));

    // "Vigas y parrillas"
    auto* grid = new QWidget(tabs);
    auto* gl = new QVBoxLayout(grid);
    m_topGrid = new QCheckBox(tr("Dimensionar parrilla superior"), grid);
    gl->addWidget(m_topGrid);
    gl->addWidget(new QLabel(tr("Criterio de dimensionamiento"), grid));
    m_gridCriterion = new QComboBox(grid);
    m_gridCriterion->addItem(tr("Armado según norma EHE"));  // the only entry seen (ticket TK-019)
    gl->addWidget(m_gridCriterion);
    auto* alternative = new QLabel(tr("Criterio alternativo a vigas diagonales y armado perimetral"), grid);
    alternative->setEnabled(false);
    gl->addWidget(alternative);
    m_gridAlternative = new QComboBox(grid);
    m_gridAlternative->addItem(tr("Armado según norma EHE"));
    m_gridAlternative->setEnabled(false);
    gl->addWidget(m_gridAlternative);
    gl->addStretch(1);
    tabs->addTab(grid, tr("Vigas y parrillas"));

    tabs->addTab(placeholderPage(tabs), tr("Armado"));
    contentLayout()->addWidget(tabs, 1);
    setupDefaultsFooter();
    load(initial);
}

void PileCapOptionsDialog::load(const PileCapOptions& all) {
    const PileCapGeneralSettings& s = all.general;
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
    m_armingDepth->setValue(all.single.armingDepthPercent);
    m_diagonalStirrups->setChecked(all.single.diagonalStirrups);
    m_topHook->setCurrentText(QString::fromStdString(all.beams.topHook));
    m_bottomHook->setCurrentText(QString::fromStdString(all.beams.bottomHook));
    m_topHookLength->setValue(all.beams.topHookMaxLengthM);
    m_bottomHookLength->setValue(all.beams.bottomHookMaxLengthM);
    m_hoopDiameter->setValue(all.beams.maxHoopDiameterMm);
    m_strutAngle->setValue(all.beams.strutAngleDeg);
    m_topGrid->setChecked(all.grid.designTopGrid);
    m_gridCriterion->setCurrentText(QString::fromStdString(all.grid.criterion));
    m_gridAlternative->setCurrentText(QString::fromStdString(all.grid.alternativeCriterion));
}

PileCapOptions PileCapOptionsDialog::value() const {
    PileCapOptions all;
    PileCapGeneralSettings& s = all.general;
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
    all.single.armingDepthPercent = m_armingDepth->value();
    all.single.diagonalStirrups = m_diagonalStirrups->isChecked();
    all.beams.topHook = m_topHook->currentText().toStdString();
    all.beams.bottomHook = m_bottomHook->currentText().toStdString();
    all.beams.topHookMaxLengthM = m_topHookLength->value();
    all.beams.bottomHookMaxLengthM = m_bottomHookLength->value();
    all.beams.maxHoopDiameterMm = m_hoopDiameter->value();
    all.beams.strutAngleDeg = m_strutAngle->value();
    all.grid.designTopGrid = m_topGrid->isChecked();
    all.grid.criterion = m_gridCriterion->currentText().toStdString();
    all.grid.alternativeCriterion = m_gridAlternative->currentText().toStdString();
    return all;
}

void PileCapOptionsDialog::resetToInstallationValues() { load(PileCapOptions{}); }

// ---------------------------------------------------------------- Zapatas aisladas

FootingOptionsDialog::FootingOptionsDialog(const FootingSettings& initial, QWidget* parent)
    : DefaultsDialog(tr("Zapatas aisladas"), parent) {
    setFixedSize(498, 518);
    auto* tabs = new QTabWidget(this);
    auto* general = new QWidget(tabs);
    auto* column = new QVBoxLayout(general);
    auto* grid = new QGridLayout;
    m_widthIncrement = gridSpin(grid, 0, tr("Incremento de ancho"), 0, 1000, 2, tr("cm"), general);
    m_depthIncrement = gridSpin(grid, 1, tr("Incremento de canto"), 0, 1000, 2, tr("cm"), general);
    m_minDepth = gridSpin(grid, 2, tr("Canto mínimo"), 0.01, 1000, 2, tr("cm"), general);
    column->addLayout(grid);
    auto* geometric = new QHBoxLayout;
    m_minGeometric = new QCheckBox(tr("Mínimos geométricos de armadura"), general);
    m_minRatio = spinBox(0, 0, 1, 4, general);
    geometric->addWidget(m_minGeometric, 1);
    geometric->addWidget(m_minRatio);
    column->addLayout(geometric);
    connect(m_minGeometric, &QCheckBox::toggled, m_minRatio, &QWidget::setEnabled);
    m_reduce = new QCheckBox(tr("Aplicar reducción a los mínimos de flexión"), general);
    m_topGrid = new QCheckBox(tr("Colocar parrilla superior si es necesaria en zapatas aisladas"), general);
    m_hooks = new QCheckBox(tr("Igualar las patillas para cada dirección"), general);
    m_liftOff = new QCheckBox(tr("Permitir despegues en la zapata"), general);
    m_noTopGrid = new QCheckBox(tr("No colocar parrilla superior si cumple como hormigón en masa"), general);
    for (QCheckBox* box : {m_reduce, m_topGrid, m_hooks, m_liftOff, m_noTopGrid}) column->addWidget(box);
    auto* typeRow = new QHBoxLayout;
    typeRow->addWidget(new QLabel(tr("Tipo por defecto de zapata"), general), 1);
    m_type = new QComboBox(general);
    m_type->addItem(tr("Zapata de canto constante"));  // the list of the combo was not opened (ticket TK-019)
    typeRow->addWidget(m_type);
    column->addLayout(typeRow);
    column->addWidget(new QLabel(tr("Criterio canto"), general));
    m_criterion = new QComboBox(general);
    m_criterion->addItem(tr("Canto zapatas >= canto viga sin excepción"));  // idem
    column->addWidget(m_criterion);
    column->addStretch(1);
    tabs->addTab(general, tr("Generales"));
    // The other tabs were not opened in the original yet (ticket TK-019). The last title is cut in the capture.
    for (const QString& title : {tr("Armado perimetral"), tr("Tipos"), tr("Zapatas combinadas"), tr("Incremento te...")})
        tabs->addTab(placeholderPage(tabs), title);
    contentLayout()->addWidget(tabs, 1);
    setupDefaultsFooter();
    load(initial);
}

void FootingOptionsDialog::load(const FootingSettings& s) {
    m_widthIncrement->setValue(s.widthIncrementCm);
    m_depthIncrement->setValue(s.depthIncrementCm);
    m_minDepth->setValue(s.minDepthCm);
    m_minGeometric->setChecked(s.minGeometricReinforcement);
    m_minRatio->setValue(s.minGeometricRatio);
    m_minRatio->setEnabled(s.minGeometricReinforcement);
    m_reduce->setChecked(s.reduceFlexureMinimums);
    m_topGrid->setChecked(s.topGridIfNeeded);
    m_hooks->setChecked(s.equalHooksPerDirection);
    m_liftOff->setChecked(s.allowLiftOff);
    m_noTopGrid->setChecked(s.noTopGridIfPlainConcrete);
    m_type->setCurrentText(QString::fromStdString(s.defaultType));
    m_criterion->setCurrentText(QString::fromStdString(s.depthCriterion));
}

FootingSettings FootingOptionsDialog::value() const {
    FootingSettings s;
    s.widthIncrementCm = m_widthIncrement->value();
    s.depthIncrementCm = m_depthIncrement->value();
    s.minDepthCm = m_minDepth->value();
    s.minGeometricReinforcement = m_minGeometric->isChecked();
    s.minGeometricRatio = m_minRatio->value();
    s.reduceFlexureMinimums = m_reduce->isChecked();
    s.topGridIfNeeded = m_topGrid->isChecked();
    s.equalHooksPerDirection = m_hooks->isChecked();
    s.allowLiftOff = m_liftOff->isChecked();
    s.noTopGridIfPlainConcrete = m_noTopGrid->isChecked();
    s.defaultType = m_type->currentText().toStdString();
    s.depthCriterion = m_criterion->currentText().toStdString();
    return s;
}

void FootingOptionsDialog::resetToInstallationValues() { load(FootingSettings{}); }

// ---------------------------------------------------------------- Vigas de atado

TieBeamOptionsDialog::TieBeamOptionsDialog(const TieBeamSettings& initial, QWidget* parent)
    : DefaultsDialog(tr("Vigas de atado"), parent) {
    setFixedSize(403, 189);
    auto* grid = new QGridLayout;
    m_surcharge = gridSpin(grid, 0, tr("Sobrecarga por compactación del terreno (mayorada)"), 0, 1000, 2, tr("kN/m"), this);
    m_minAxial = gridSpin(grid, 1, tr("Porcentaje mínimo del axil"), 0, 100, 0, tr("%"), this);
    contentLayout()->addLayout(grid);
    m_stirrups = new QCheckBox(tr("Disponer estribos dentro de la cimentación"), this);
    contentLayout()->addWidget(m_stirrups);
    contentLayout()->addStretch(1);
    setupDefaultsFooter();
    load(initial);
}

void TieBeamOptionsDialog::load(const TieBeamSettings& s) {
    m_surcharge->setValue(s.compactionSurchargeKnPerM);
    m_minAxial->setValue(s.minAxialPercent);
    m_stirrups->setChecked(s.stirrupsInsideFoundation);
}

TieBeamSettings TieBeamOptionsDialog::value() const {
    TieBeamSettings s;
    s.compactionSurchargeKnPerM = m_surcharge->value();
    s.minAxialPercent = m_minAxial->value();
    s.stirrupsInsideFoundation = m_stirrups->isChecked();
    return s;
}

void TieBeamOptionsDialog::resetToInstallationValues() { load(TieBeamSettings{}); }

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
    : CypeDialog(kind == Kind::Footings ? tr("Zapatas aisladas")
                 : kind == Kind::CenteringBeams ? tr("Vigas centradoras")
                                                : tr("Vigas de atado"),
                 parent) {
    const bool footings = kind == Kind::Footings;
    setFixedSize(footings ? 481 : kind == Kind::CenteringBeams ? 489 : 491, 267);
    auto* intro = new QLabel(footings ? tr("Esta tabla de armado se utiliza para dimensionar el armado inferior y superior de las "
                                           "zapatas aisladas.")
                             : kind == Kind::CenteringBeams
                                 ? tr("Esta tabla de armado se utiliza para el dimensionamiento de las secciones de vigas "
                                      "centradoras.")
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
    line(m_predefined, tr("Tabla predefinida"), footings ? tr("Tabla de armado para zapatas")
                              : kind == Kind::CenteringBeams ? tr("Tabla de vigas centradoras") : tr("Tabla de vigas de atado"),
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
