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

    // "Armado"
    auto* reinforcement = new QWidget(tabs);
    auto* rl = new QVBoxLayout(reinforcement);
    m_perimeter = new QCheckBox(tr("Disponer armado perimetral"), reinforcement);
    rl->addWidget(m_perimeter);
    auto* rg = new QGridLayout;
    m_perimeterFrom = gridSpin(rg, 0, tr("A partir de"), 0, 1000000, 2, tr("kN"), reinforcement);
    rl->addLayout(rg);
    rl->addStretch(1);
    connect(m_perimeter, &QCheckBox::toggled, m_perimeterFrom, &QWidget::setEnabled);
    tabs->addTab(reinforcement, tr("Armado"));
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
    m_perimeter->setChecked(all.reinforcement.perimeterReinforcement);
    m_perimeterFrom->setValue(all.reinforcement.fromAxialKn);
    m_perimeterFrom->setEnabled(all.reinforcement.perimeterReinforcement);
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
    all.reinforcement.perimeterReinforcement = m_perimeter->isChecked();
    all.reinforcement.fromAxialKn = m_perimeterFrom->value();
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

    // "Armado perimetral"
    auto* perimeter = new QWidget(tabs);
    auto* pl = new QVBoxLayout(perimeter);
    m_perimeter = new QCheckBox(tr("Disponer armado perimetral de zunchado"), perimeter);
    pl->addWidget(m_perimeter);
    auto* pg = new QGridLayout;
    m_perimeterDiameter = gridSpin(pg, 0, tr("Diámetro mínimo"), 0.01, 100, 2, tr("mm"), perimeter);
    m_perimeterBars = gridSpin(pg, 1, tr("Número mínimo de barras"), 1, 1000, 0, QString(), perimeter);
    m_perimeterAxial = gridSpin(pg, 2, tr("Axil mínimo para disponer"), 0, 1000000, 2, tr("kN"), perimeter);
    pl->addLayout(pg);
    pl->addStretch(1);
    for (QWidget* w : {static_cast<QWidget*>(m_perimeterDiameter), static_cast<QWidget*>(m_perimeterBars), static_cast<QWidget*>(m_perimeterAxial)})
        connect(m_perimeter, &QCheckBox::toggled, w, &QWidget::setEnabled);
    tabs->addTab(perimeter, tr("Armado perimetral"));

    // "Tipos"
    auto* types = new QWidget(tabs);
    auto* tl = new QVBoxLayout(types);
    const auto typeGroup = [&](const QString& title) {
        auto* box = new QGroupBox(title, types);
        auto* grid = new QGridLayout(box);
        tl->addWidget(box);
        return grid;
    };
    auto* square = typeGroup(tr("Zapata cuadrada"));
    m_equalReinforcement = new QCheckBox(tr("Igualar armadura en las dos direcciones"), types);
    square->addWidget(m_equalReinforcement, 0, 0, 1, 3);
    m_squareOverhang = gridSpin(square, 1, tr("Vuelo mínimo"), 0, 1000, 2, tr("cm"), types);
    auto* centred = typeGroup(tr("Zapata rectangular centrada"));
    m_centredOverhang = gridSpin(centred, 0, tr("Vuelo mínimo"), 0, 1000, 2, tr("cm"), types);
    m_centredRatio = gridSpin(centred, 1, tr("Relación máxima de lados"), 1, 100, 2, QString(), types);
    auto* eccentric = typeGroup(tr("Zapata rectangular excéntrica"));
    m_eccentricOverhang = gridSpin(eccentric, 0, tr("Vuelo mínimo"), 0, 1000, 2, tr("cm"), types);
    m_eccentricRatio = gridSpin(eccentric, 1, tr("Relación máxima de lados"), 1, 100, 2, QString(), types);
    auto* pyramid = typeGroup(tr("Zapata piramidal"));
    m_pyramidAngle = gridSpin(pyramid, 0, tr("Ángulo máximo"), 0.01, 89.99, 2, tr("grados"), types);
    m_pyramidPedestal = gridSpin(pyramid, 1, tr("Vuelo mínimo en pedestal"), 0, 1000, 2, tr("cm"), types);
    m_pyramidEdge = gridSpin(pyramid, 2, tr("Canto mínimo en borde"), 0, 1000, 2, tr("cm"), types);
    tl->addStretch(1);
    tabs->addTab(types, tr("Tipos"));

    // "Zapatas combinadas"
    auto* combined = new QWidget(tabs);
    auto* cl = new QVBoxLayout(combined);
    auto* cg = new QGridLayout;
    m_spanRatio = gridSpin(cg, 0, tr("Máxima relación luz máxima entre pilares-canto"), 0.01, 1000, 2, QString(), combined);
    m_overhangRatio = gridSpin(cg, 1, tr("Máxima relación vuelo máximo-canto"), 0.01, 1000, 2, QString(), combined);
    cl->addLayout(cg);
    cl->addStretch(1);
    tabs->addTab(combined, tr("Zapatas combinadas"));

    // "Incremento tensión"
    auto* increment = new QWidget(tabs);
    auto* il = new QVBoxLayout(increment);
    auto* note = new QLabel(tr("Se acepta, como norma general, admitir un incremento en la tensión máxima transmitida al terreno respecto a la tensión admisible definida, cuando hay excentricidad de la carga. Las comprobaciones que se realizan son las siguientes:"), increment);
    note->setWordWrap(true);
    il->addWidget(note);
    const auto incrementGroup = [&](const QString& title, QDoubleSpinBox*& spin) {
        auto* box = new QGroupBox(title, increment);
        auto* row = new QHBoxLayout(box);
        row->addWidget(new QLabel(tr("Tensión máxima cálculo <= ( 1 +"), increment));
        spin = spinBox(0, 0, 100, 2, increment);
        row->addWidget(spin);
        row->addWidget(new QLabel(tr(") tensión admisible"), increment), 1);
        il->addWidget(box);
    };
    incrementGroup(tr("Situaciones persistentes sin viento"), m_incrementPersistent);
    incrementGroup(tr("Situaciones persistentes con viento"), m_incrementWind);
    incrementGroup(tr("Situaciones accidentales"), m_incrementAccidental);
    il->addStretch(1);
    tabs->addTab(increment, tr("Incremento tensión"));
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
    m_perimeter->setChecked(s.perimeter.enabled);
    m_perimeterDiameter->setValue(s.perimeter.minDiameterMm);
    m_perimeterBars->setValue(s.perimeter.minBars);
    m_perimeterAxial->setValue(s.perimeter.minAxialKn);
    for (QWidget* w : {static_cast<QWidget*>(m_perimeterDiameter), static_cast<QWidget*>(m_perimeterBars), static_cast<QWidget*>(m_perimeterAxial)})
        w->setEnabled(s.perimeter.enabled);
    m_equalReinforcement->setChecked(s.types.equalReinforcement);
    m_squareOverhang->setValue(s.types.squareMinOverhangCm);
    m_centredOverhang->setValue(s.types.centredMinOverhangCm);
    m_centredRatio->setValue(s.types.centredMaxSideRatio);
    m_eccentricOverhang->setValue(s.types.eccentricMinOverhangCm);
    m_eccentricRatio->setValue(s.types.eccentricMaxSideRatio);
    m_pyramidAngle->setValue(s.types.pyramidMaxAngleDeg);
    m_pyramidPedestal->setValue(s.types.pyramidMinPedestalOverhangCm);
    m_pyramidEdge->setValue(s.types.pyramidMinEdgeDepthCm);
    m_spanRatio->setValue(s.combined.maxSpanDepthRatio);
    m_overhangRatio->setValue(s.combined.maxOverhangDepthRatio);
    m_incrementPersistent->setValue(s.stressIncrease.persistentNoWind);
    m_incrementWind->setValue(s.stressIncrease.persistentWind);
    m_incrementAccidental->setValue(s.stressIncrease.accidental);
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
    s.perimeter.enabled = m_perimeter->isChecked();
    s.perimeter.minDiameterMm = m_perimeterDiameter->value();
    s.perimeter.minBars = static_cast<int>(m_perimeterBars->value());
    s.perimeter.minAxialKn = m_perimeterAxial->value();
    s.types.equalReinforcement = m_equalReinforcement->isChecked();
    s.types.squareMinOverhangCm = m_squareOverhang->value();
    s.types.centredMinOverhangCm = m_centredOverhang->value();
    s.types.centredMaxSideRatio = m_centredRatio->value();
    s.types.eccentricMinOverhangCm = m_eccentricOverhang->value();
    s.types.eccentricMaxSideRatio = m_eccentricRatio->value();
    s.types.pyramidMaxAngleDeg = m_pyramidAngle->value();
    s.types.pyramidMinPedestalOverhangCm = m_pyramidPedestal->value();
    s.types.pyramidMinEdgeDepthCm = m_pyramidEdge->value();
    s.combined.maxSpanDepthRatio = m_spanRatio->value();
    s.combined.maxOverhangDepthRatio = m_overhangRatio->value();
    s.stressIncrease.persistentNoWind = m_incrementPersistent->value();
    s.stressIncrease.persistentWind = m_incrementWind->value();
    s.stressIncrease.accidental = m_incrementAccidental->value();
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

DeflectionRowDialog::DeflectionRowDialog(const QString& title, const QString& header, const DeflectionRow& initial, QWidget* parent)
    : CypeDialog(title, parent) {
    setFixedSize(283, 197);
    auto* layout = contentLayout();
    layout->addWidget(greyBar(header, this));
    auto* grid = new QGridLayout;
    const QString rangeTip = tr("Debe estar entre %1 y %2.").arg(DeflectionRow::kMinDenominator).arg(DeflectionRow::kMaxDenominator);
    const auto denominatorSpin = [&](int value) {
        auto* spin = new QSpinBox(this);
        spin->setButtonSymbols(QAbstractSpinBox::NoButtons);
        spin->setRange(DeflectionRow::kMinDenominator, DeflectionRow::kMaxDenominator);
        spin->setValue(value);
        spin->setAlignment(Qt::AlignRight);
        spin->setToolTip(rangeTip);
        spin->setFixedWidth(55);
        return spin;
    };
    // Absoluta: [value] cm
    m_absolute = new QCheckBox(tr("Absoluta"), this);
    m_absoluteCm = spinBox(initial.absoluteCm, 0.001, 100000, 3, this);
    grid->addWidget(m_absolute, 0, 0);
    grid->addWidget(m_absoluteCm, 0, 2);
    grid->addWidget(new QLabel(tr("cm"), this), 0, 3);
    // Mixta: L/ [n] + [value] cm
    m_mixed = new QCheckBox(tr("Mixta"), this);
    m_mixedDenominator = denominatorSpin(initial.mixedDenominator);
    m_mixedCm = spinBox(initial.mixedCm, 0, 100000, 3, this);
    auto* mixedRow = new QHBoxLayout;
    mixedRow->addWidget(new QLabel(tr("L/"), this));
    mixedRow->addWidget(m_mixedDenominator);
    mixedRow->addWidget(new QLabel(tr("+"), this));
    grid->addWidget(m_mixed, 1, 0);
    grid->addLayout(mixedRow, 1, 1, 1, 1);
    grid->addWidget(m_mixedCm, 1, 2);
    grid->addWidget(new QLabel(tr("cm"), this), 1, 3);
    // Relativa: L/ [n]
    m_relative = new QCheckBox(tr("Relativa"), this);
    m_relativeDenominator = denominatorSpin(initial.relativeDenominator);
    auto* relativeRow = new QHBoxLayout;
    relativeRow->addWidget(new QLabel(tr("L/"), this));
    relativeRow->addWidget(m_relativeDenominator);
    grid->addWidget(m_relative, 2, 0);
    grid->addLayout(relativeRow, 2, 1, 1, 1);
    grid->setColumnStretch(1, 1);
    layout->addLayout(grid);
    layout->addStretch(1);

    m_absolute->setChecked(initial.absolute);
    m_mixed->setChecked(initial.mixed);
    m_relative->setChecked(initial.relative);
    // Each value field is only editable while its box is checked.
    const auto link = [](QCheckBox* box, const QList<QWidget*>& fields) {
        const auto apply = [box, fields](bool on) {
            for (QWidget* f : fields) f->setEnabled(on);
        };
        QObject::connect(box, &QCheckBox::toggled, box, apply);
        apply(box->isChecked());
    };
    link(m_absolute, {m_absoluteCm});
    link(m_mixed, {m_mixedDenominator, m_mixedCm});
    link(m_relative, {m_relativeDenominator});
    setupFooter(tr("Aceptar"), tr("Cancelar"));
}

DeflectionRow DeflectionRowDialog::value() const {
    DeflectionRow row;
    row.absolute = m_absolute->isChecked();
    row.absoluteCm = m_absoluteCm->value();
    row.mixed = m_mixed->isChecked();
    row.mixedDenominator = m_mixedDenominator->value();
    row.mixedCm = m_mixedCm->value();
    row.relative = m_relative->isChecked();
    row.relativeDenominator = m_relativeDenominator->value();
    return row;
}

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

DeflectionRow* DeflectionEditorDialog::slot(Section section, int index) {
    const auto i = static_cast<std::size_t>(index);
    switch (section) {
        case Section::InstantSimple: return &m_set.instantSimple.at(i);
        case Section::InstantCombination: return &m_set.instantCombination.at(i);
        case Section::Infinite: return &m_set.infinite.at(i);
        case Section::Active: return &m_set.active.at(i);
    }
    return nullptr;
}

const DeflectionRow& DeflectionEditorDialog::row(Section section, int index) const {
    return *const_cast<DeflectionEditorDialog*>(this)->slot(section, index);
}

void DeflectionEditorDialog::setRow(Section section, int index, const DeflectionRow& row) {
    *slot(section, index) = row;
    for (auto& [key, label] : m_labels) {
        if (key.first == section && key.second == index) label->setText(QString::fromStdString(row.summary()));
    }
}

QString DeflectionEditorDialog::rowTitle(Section section) {
    switch (section) {
        case Section::InstantSimple:
        case Section::InstantCombination: return tr("Límites de flecha instantánea");
        case Section::Infinite: return tr("Límites de flecha a plazo infinito");
        case Section::Active: return tr("Límites de flecha activa a largo plazo");
    }
    return {};
}

QString DeflectionEditorDialog::rowHeader(Section section, const QString& name) {
    const QString lower = name.toLower();
    return section == Section::InstantSimple ? tr("Hipótesis: %1").arg(lower) : tr("Combinación de acciones: %1").arg(lower);
}

void DeflectionEditorDialog::addRows(QVBoxLayout* layout, const QString& title, Section section, const QStringList& names) {
    layout->addWidget(greyBar(title, this));
    for (int i = 0; i < names.size(); ++i) {
        auto* line = new QHBoxLayout;
        line->addWidget(new QLabel(names[i], this), 1);
        auto* value = new QLabel(this);
        value->setMinimumWidth(110);
        value->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        line->addWidget(value);
        m_labels.push_back({{section, i}, value});
        auto* pencil = pencilButton(this);
        pencil->setToolTip(tr("Editar límite"));
        connect(pencil, &QToolButton::clicked, this, [this, section, i, name = names[i]] {
            DeflectionRowDialog dialog(rowTitle(section), rowHeader(section, name), row(section, i), this);
            if (dialog.exec() == QDialog::Accepted) setRow(section, i, dialog.value());
        });
        line->addWidget(pencil);
        layout->addLayout(line);
        setRow(section, i, row(section, i));
    }
}

}  // namespace cc::app
