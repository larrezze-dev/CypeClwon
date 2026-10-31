// Tests for the "Obra vacía" wizard and its sub-dialogs, run on Qt's offscreen platform.
#include <gtest/gtest.h>

#include <QApplication>
#include <QComboBox>
#include <QLabel>
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QListWidget>
#include <QPushButton>
#include <QRadioButton>
#include <QTabWidget>
#include <QToolButton>
#include <memory>

#include "EmptyProjectWizard.h"
#include "OptionsDialogs.h"
#include "OptionsLeafDialogs.h"
#include "WizardSubDialogs.h"

using namespace cc::app;

namespace {
void ensureApp() {
    static int argc = 1;
    static char name[] = "emptywizard_tests";
    static char* argv[] = {name, nullptr};
    static QApplication instance(argc, argv);
}

QPushButton* buttonWithText(QWidget* parent, const QString& text) {
    for (QPushButton* b : parent->findChildren<QPushButton*>()) {
        if (b->text() == text) return b;
    }
    return nullptr;
}
}  // namespace

TEST(EmptyProjectWizard, FooterAndSize) {
    ensureApp();
    EmptyProjectWizard wizard;
    EXPECT_EQ(wizard.size(), QSize(848, 774));
    EXPECT_FALSE(wizard.previousButton()->isEnabled());
    EXPECT_FALSE(wizard.nextButton()->isEnabled());
    EXPECT_TRUE(wizard.finishButton()->isDefault());
    ASSERT_NE(buttonWithText(&wizard, "Cancelar"), nullptr);
}

TEST(EmptyProjectWizard, ShowsSummariesAndDefaults) {
    ensureApp();
    EmptyProjectWizard wizard;
    EXPECT_NE(buttonWithText(&wizard, "Eurocódigo 2, CTE DB SE-A, Código Estructural, CTE DB SE-M y Eurocódigo 9"), nullptr);
    EXPECT_NE(buttonWithText(&wizard, "EN AW-5083 - F"), nullptr);
    EXPECT_NE(buttonWithText(&wizard, "S-400"), nullptr);
    EXPECT_NE(buttonWithText(&wizard, "Cuarcita (15 mm), 30 mm"), nullptr);
    EXPECT_TRUE(wizard.profiles().isValid());
    EXPECT_TRUE(wizard.reinforcedConcrete().isValid());
}

TEST(EmptyProjectWizard, ComboChoicesUpdateModel) {
    ensureApp();
    EmptyProjectWizard wizard;
    bool found = false;
    for (QComboBox* combo : wizard.findChildren<QComboBox*>()) {
        if (combo->findText(QStringLiteral("S355 HISTAR®")) >= 0) {
            combo->setCurrentText(QStringLiteral("S355 HISTAR®"));
            found = true;
        }
    }
    ASSERT_TRUE(found);
    EXPECT_EQ(wizard.profiles().hotRolledSteel, "S355 HISTAR®");
}

TEST(WizardSubDialogs, TimberRoundTrip) {
    ensureApp();
    cc::core::TimberSettings t;
    t.serviceClass = 2;
    TimberDialog dialog(t);
    EXPECT_EQ(dialog.value().serviceClass, 2);
    EXPECT_EQ(dialog.value().strengthClass, "C24");
    dialog.findChildren<QRadioButton*>().at(2)->setChecked(true);
    EXPECT_EQ(dialog.value().serviceClass, 3);
}

TEST(WizardSubDialogs, AluminiumRoundTrip) {
    ensureApp();
    AluminiumDialog dialog{cc::core::AluminiumSettings{}};
    EXPECT_EQ(dialog.value().alloy, "EN AW-5083");
    EXPECT_EQ(dialog.value().temper, "F");
    for (QRadioButton* r : dialog.findChildren<QRadioButton*>()) {
        if (r->text() == "EN AW-6082" || r->text() == "H112") r->setChecked(true);
    }
    EXPECT_EQ(dialog.value().alloy, "EN AW-6082");
    EXPECT_EQ(dialog.value().temper, "H112");
}

TEST(WizardSubDialogs, CoverFoundationPreservesOtherFamilies) {
    ensureApp();
    cc::core::CoverSettings c;
    c.columns = 2.5;
    c.tieBeamBottom = 9.0;
    CoverDialog dialog(CoverDialog::Kind::Foundation, c);
    EXPECT_DOUBLE_EQ(dialog.value().tieBeamBottom, 9.0);
    EXPECT_DOUBLE_EQ(dialog.value().columns, 2.5);
    buttonWithText(&dialog, QStringLiteral("Valores de instalación"))->click();
    EXPECT_DOUBLE_EQ(dialog.value().tieBeamBottom, 5.0);
    EXPECT_DOUBLE_EQ(dialog.value().footingSide, 7.5);
    EXPECT_DOUBLE_EQ(dialog.value().columns, 2.5);
}

TEST(WizardSubDialogs, WasteDefaultsAndReset) {
    ensureApp();
    cc::core::WasteSettings w;
    w.pileCaps = 15.0;
    WasteDialog dialog(w);
    EXPECT_DOUBLE_EQ(dialog.value().pileCaps, 15.0);
    EXPECT_FALSE(dialog.saveAsDefault());
    buttonWithText(&dialog, QStringLiteral("Valores de instalación"))->click();
    EXPECT_DOUBLE_EQ(dialog.value().pileCaps, 10.0);
}

TEST(WizardSubDialogs, RebarAndAggregateRoundTrip) {
    ensureApp();
    RebarSteelDialog rebar{cc::core::RebarSteelSettings{}};
    EXPECT_TRUE(rebar.value().isValid());
    EXPECT_EQ(rebar.findChildren<QComboBox*>().size(), 10);
    AggregateDialog aggregate{cc::core::AggregateSettings{}};
    EXPECT_EQ(aggregate.value().summary(), "Cuarcita (15 mm), 30 mm");
}

TEST(WizardSubDialogs, ActionsDialogsRoundTrip) {
    ensureApp();
    FireDialog fire(cc::core::SteelFireMode::Ignore);
    EXPECT_EQ(fire.value(), cc::core::SteelFireMode::Ignore);
    fire.findChildren<QRadioButton*>().at(2)->setChecked(true);
    EXPECT_EQ(fire.value(), cc::core::SteelFireMode::CriticalTemperature);
    FoundationActionsDialog foundation(true, false);
    EXPECT_TRUE(foundation.wind());
    EXPECT_FALSE(foundation.seism());
    ConstructionProcessDialog process(14.0);
    EXPECT_DOUBLE_EQ(process.days(), 14.0);
    buttonWithText(&process, QStringLiteral("Valores de instalación"))->click();
    EXPECT_DOUBLE_EQ(process.days(), 28.0);
}

TEST(EmptyProjectWizard, ActionsDefaults) {
    ensureApp();
    EmptyProjectWizard wizard;
    EXPECT_FALSE(wizard.actions().dynamicSeismic);
    EXPECT_TRUE(wizard.actions().foundationWind);
    EXPECT_TRUE(wizard.actions().isValid());
    EXPECT_NE(buttonWithText(&wizard, QStringLiteral("Hipótesis adicionales")), nullptr);
    EXPECT_TRUE(buttonWithText(&wizard, QStringLiteral("Estados límite (combinaciones)"))->isEnabled());
    EXPECT_TRUE(wizard.limitStates().isValid());
}

TEST(WizardSubDialogs, EnvironmentAndGround) {
    ensureApp();
    BeamEnvironmentDialog env{cc::core::EnvironmentSettings{}};
    EXPECT_EQ(env.value().beams, cc::core::ExposureClass::NoRisk);
    EXPECT_DOUBLE_EQ(env.value().maxCrackWidthMm, 0.40);
    UsualGroundDialog ground;
    EXPECT_EQ(ground.selectedIndex(), -1);
    EXPECT_EQ(cc::core::usualGroundValues().size(), 8u);
    EXPECT_TRUE(cc::core::GroundSettings::isValidStress(0.2));
    EXPECT_FALSE(cc::core::GroundSettings::isValidStress(-1.0));
    EXPECT_FALSE(cc::core::GroundSettings::isValidStress(50.5));
}

TEST(EmptyProjectWizard, GroundAndEnvironmentDefaults) {
    ensureApp();
    EmptyProjectWizard wizard;
    EXPECT_DOUBLE_EQ(wizard.ground().persistentStressMPa, 0.200);
    EXPECT_DOUBLE_EQ(wizard.ground().seismicStressMPa, 0.300);
    EXPECT_FALSE(wizard.ground().checkFootingSliding);
    EXPECT_NE(buttonWithText(&wizard, QStringLiteral("X0")), nullptr);
}

TEST(EmptyProjectWizard, PileDefaultsAndEdits) {
    ensureApp();
    EmptyProjectWizard wizard;
    EXPECT_DOUBLE_EQ(wizard.piles().minMomentEccentricityM, 0.10);
    EXPECT_TRUE(wizard.piles().reducedAxial);
    EXPECT_DOUBLE_EQ(wizard.piles().shaftAxialFactor, 0.00);
    EXPECT_DOUBLE_EQ(wizard.piles().tipOnlyAxialFactor, 0.80);
    EXPECT_DOUBLE_EQ(wizard.piles().bucklingCoefficient, 1.00);
    EXPECT_FALSE(wizard.piles().lateralStability);
    EXPECT_DOUBLE_EQ(wizard.micropiles().corrosionThicknessMm, 0.60);
    EXPECT_TRUE(wizard.piles().isValid());
    EXPECT_TRUE(wizard.micropiles().isValid());
    auto* tabs = wizard.findChild<QTabWidget*>("pilesTabs");
    ASSERT_NE(tabs, nullptr);
    EXPECT_EQ(tabs->count(), 2);
    EXPECT_EQ(tabs->tabText(0), "Pilotes (Hormigón armado)");
    EXPECT_EQ(tabs->tabText(1), "Micropilotes");
    // Editing a field writes through to the model.
    tabs->widget(0)->findChildren<QDoubleSpinBox*>().at(1)->setValue(0.30);
    EXPECT_DOUBLE_EQ(wizard.piles().shaftAxialFactor, 0.30);
    // The micropile arrow restores the installation value.
    auto* micro = tabs->widget(1);
    micro->findChildren<QDoubleSpinBox*>().at(2)->setValue(0.90);
    EXPECT_DOUBLE_EQ(wizard.micropiles().corrosionThicknessMm, 0.90);
    micro->findChildren<QToolButton*>().at(0)->click();
    EXPECT_DOUBLE_EQ(wizard.micropiles().corrosionThicknessMm, 0.60);
    EXPECT_TRUE(buttonWithText(micro, "Configuración avanzada")->isEnabled());
}

TEST(WizardSubDialogs, PileAdvancedRoundTrip) {
    ensureApp();
    PileAdvancedDialog dialog{cc::core::PileAdvancedSettings{}};
    EXPECT_EQ(dialog.size(), QSize(556, 786));
    const auto v = dialog.value();
    EXPECT_EQ(v.sinking.reference, "A2 + M2 + R3");
    EXPECT_DOUBLE_EQ(v.sinking.frictionAngle, 1.25);
    EXPECT_DOUBLE_EQ(v.sinking.undrainedShear, 1.40);
    EXPECT_DOUBLE_EQ(v.sinking.geotechnicalShaftTension, 1.10);
    EXPECT_DOUBLE_EQ(v.sinking.unitShaftTension, 1.10);
    EXPECT_DOUBLE_EQ(v.sinking.efficiencyK, 2.00);
    EXPECT_TRUE(v.sinking.ignoreEfficiencyWhenFar);
    EXPECT_FALSE(v.structuralCap);
    dialog.findChild<QTabWidget*>()->widget(0)->findChildren<QDoubleSpinBox*>().at(0)->setValue(1.50);
    EXPECT_DOUBLE_EQ(dialog.value().sinking.frictionAngle, 1.50);
    buttonWithText(&dialog, QStringLiteral("Valores de instalación"))->click();
    EXPECT_DOUBLE_EQ(dialog.value().sinking.frictionAngle, 1.25);
}

TEST(WizardSubDialogs, PileStructuralTabRoundTrip) {
    ensureApp();
    PileAdvancedDialog dialog{cc::core::PileAdvancedSettings{}};
    auto v = dialog.value();
    EXPECT_TRUE(v.structural.reduceConcreteStrength);
    EXPECT_EQ(v.structural.precastBasis, cc::core::StrengthBasis::Relative);
    EXPECT_DOUBLE_EQ(v.structural.precastValue, 1.00);
    EXPECT_EQ(v.structural.castInSituBasis, cc::core::StrengthBasis::Absolute);
    EXPECT_DOUBLE_EQ(v.structural.castInSituValue, 18.00);
    EXPECT_DOUBLE_EQ(v.structural.undrainedShearFactor, 1.40);
    EXPECT_DOUBLE_EQ(v.structural.unitWeightFactor, 1.00);
    EXPECT_EQ(v.cap.precastBasis, cc::core::StrengthBasis::Relative);
    EXPECT_DOUBLE_EQ(v.cap.precastValue, 0.30);
    EXPECT_EQ(v.cap.castInSituBasis, cc::core::StrengthBasis::Absolute);
    EXPECT_DOUBLE_EQ(v.cap.castInSituValue, 3.50);
    cc::core::PileAdvancedSettings custom;
    custom.cap.castInSituValue = 4.25;
    custom.structural.reduceConcreteStrength = false;
    custom.structural.castInSituBasis = cc::core::StrengthBasis::Relative;
    custom.structural.castInSituValue = 0.85;
    custom.structuralCap = true;
    PileAdvancedDialog other{custom};
    v = other.value();
    EXPECT_FALSE(v.structural.reduceConcreteStrength);
    EXPECT_EQ(v.structural.castInSituBasis, cc::core::StrengthBasis::Relative);
    EXPECT_DOUBLE_EQ(v.structural.castInSituValue, 0.85);
    EXPECT_TRUE(v.structuralCap);
    EXPECT_DOUBLE_EQ(v.cap.castInSituValue, 4.25);
    buttonWithText(&other, QStringLiteral("Valores de instalación"))->click();
    EXPECT_TRUE(other.value().structural.reduceConcreteStrength);
    EXPECT_FALSE(other.value().structuralCap);
}

TEST(WizardSubDialogs, MicropileAdvancedRoundTrip) {
    ensureApp();
    MicropileAdvancedDialog dialog{cc::core::MicropileAdvancedSettings{}};
    EXPECT_EQ(dialog.size(), QSize(587, 728));
    auto v = dialog.value();
    EXPECT_DOUBLE_EQ(v.sinking.geotechnicalShaft, 1.00);
    EXPECT_DOUBLE_EQ(v.sinking.geotechnicalShaftTension, 1.10);
    EXPECT_DOUBLE_EQ(v.sinking.unitTip, 1.00);
    EXPECT_DOUBLE_EQ(v.sinking.efficiencyK, 2.00);
    EXPECT_DOUBLE_EQ(v.undrainedShearFactor, 1.40);
    EXPECT_DOUBLE_EQ(v.unitWeightFactor, 1.00);
    EXPECT_EQ(dialog.findChildren<QDoubleSpinBox*>().size(), 14);  // 5 ground + 2 + 4 unit + K + 2 p-y factors
    dialog.findChild<QTabWidget*>()->widget(0)->findChildren<QDoubleSpinBox*>().at(0)->setValue(1.30);
    EXPECT_DOUBLE_EQ(dialog.value().sinking.frictionAngle, 1.30);
    buttonWithText(&dialog, QStringLiteral("Valores de instalación"))->click();
    EXPECT_DOUBLE_EQ(dialog.value().sinking.frictionAngle, 1.25);
}

TEST(WizardSubDialogs, OptionsMenusLayoutAndEnabledButtons) {
    ensureApp();
    using Family = OptionsMenuDialog::Family;
    struct Case {
        Family family;
        QSize size;
        int buttons;  // including Aceptar
    };
    cc::core::OptionsSettings settings;
    const Case cases[] = {{Family::Foundation, QSize(150, 158), 3},
                          {Family::Beams, QSize(223, 257), 6},
                          {Family::Joints, QSize(117, 158), 3}};
    for (const Case& c : cases) {
        std::unique_ptr<OptionsMenuDialog> menu(OptionsMenuDialog::create(c.family, settings));
        EXPECT_EQ(menu->size(), c.size);
        EXPECT_EQ(menu->findChildren<QPushButton*>().size(), c.buttons);
        // Every button of these three menus opens a captured dialog, so all of them are enabled.
        for (QPushButton* button : menu->findChildren<QPushButton*>()) EXPECT_TRUE(button->isEnabled());
    }
    std::unique_ptr<OptionsMenuDialog> columns(OptionsMenuDialog::create(Family::Columns, settings));
    EXPECT_EQ(columns->size(), QSize(575, 372));
    EXPECT_EQ(columns->findChild<QListWidget*>()->count(), 4);
    // 1 + 7 + 8 + 3 page buttons plus Aceptar; the Pilares leaves are not captured yet, so they are disabled.
    EXPECT_EQ(columns->findChildren<QPushButton*>().size(), 20);
    EXPECT_FALSE(columns->findChildren<QPushButton*>().at(0)->isEnabled());
}

TEST(OptionsDialogs, BeamErrorsDefaultsAndRoundTrip) {
    ensureApp();
    BeamErrorsDialog dialog{cc::core::BeamErrorSettings{}};
    EXPECT_EQ(dialog.size(), QSize(437, 514));
    auto v = dialog.value();
    EXPECT_TRUE(v.compressionHook);
    EXPECT_FALSE(v.skinAnchorage);
    EXPECT_FALSE(v.topMultiLayer);
    EXPECT_FALSE(v.bottomMultiLayer);
    EXPECT_TRUE(v.minStirrupDiameter);
    EXPECT_EQ(v.minStirrupDiameterMm, 16);
    EXPECT_EQ(dialog.findChildren<QCheckBox*>().size(), 16);  // 15 error checks + "Grabar como opciones por defecto"
    cc::core::BeamErrorSettings custom;
    custom.skinAnchorage = true;
    custom.compressionHook = false;
    custom.minStirrupDiameter = false;
    BeamErrorsDialog other{custom};
    v = other.value();
    EXPECT_TRUE(v.skinAnchorage);
    EXPECT_FALSE(v.compressionHook);
    EXPECT_FALSE(v.minStirrupDiameter);
    buttonWithText(&other, QStringLiteral("Valores de instalación"))->click();
    EXPECT_FALSE(other.value().skinAnchorage);
    EXPECT_TRUE(other.value().compressionHook);
}

TEST(OptionsDialogs, DeflectionLimitsSummary) {
    ensureApp();
    DeflectionLimitsDialog dialog;
    EXPECT_EQ(dialog.size(), QSize(309, 371));
    bool found = false;
    for (QLabel* label : dialog.findChildren<QLabel*>()) {
        found = found || label->text() == QStringLiteral("Activa a largo plazo (Cuasipermanente): L/ 500");
    }
    EXPECT_TRUE(found);
}

TEST(OptionsDialogs, ContinuousBeamNumberingRoundTrip) {
    ensureApp();
    ContinuousBeamNumberingDialog dialog{cc::core::ContinuousBeamNumbering{}};
    EXPECT_EQ(dialog.size(), QSize(531, 372));
    auto v = dialog.value();
    EXPECT_EQ(v.baseReference, "Pórtico 1");
    EXPECT_EQ(v.firstAxis, cc::core::ContinuousBeamNumbering::Axis::X);
    EXPECT_EQ(v.xFramesAlongY, cc::core::ContinuousBeamNumbering::Sense::Positive);
    cc::core::ContinuousBeamNumbering custom;
    custom.baseReference = "Eje A";
    custom.firstAxis = cc::core::ContinuousBeamNumbering::Axis::Y;
    custom.yFramesAlongX = cc::core::ContinuousBeamNumbering::Sense::Negative;
    ContinuousBeamNumberingDialog other{custom};
    v = other.value();
    EXPECT_EQ(v.baseReference, "Eje A");
    EXPECT_EQ(v.firstAxis, cc::core::ContinuousBeamNumbering::Axis::Y);
    EXPECT_EQ(v.yFramesAlongX, cc::core::ContinuousBeamNumbering::Sense::Negative);
    buttonWithText(&other, QStringLiteral("Valores de instalación"))->click();
    EXPECT_EQ(other.value().baseReference, "Pórtico 1");
}

TEST(OptionsDialogs, BeamNumberingModes) {
    ensureApp();
    BeamNumberingDialog dialog{cc::core::BeamNumbering::None};
    EXPECT_EQ(dialog.size(), QSize(397, 233));
    EXPECT_EQ(dialog.value(), cc::core::BeamNumbering::None);
    BeamNumberingDialog other{cc::core::BeamNumbering::ByLevel};
    EXPECT_EQ(other.value(), cc::core::BeamNumbering::ByLevel);
    buttonWithText(&other, QStringLiteral("Valores de instalación"))->click();
    EXPECT_EQ(other.value(), cc::core::BeamNumbering::None);
}

TEST(OptionsDialogs, JointMaterialsAndBolts) {
    ensureApp();
    JointMaterialsDialog materials{cc::core::JointMaterials{}};
    EXPECT_EQ(materials.size(), QSize(291, 181));
    auto m = materials.value();
    EXPECT_EQ(m.rolledSteel, "S275 (UNE-EN 10025-2)");
    EXPECT_EQ(m.anchorBoltSteel, "B 400 S, Ys = 1.15");
    EXPECT_EQ(m.concrete, "C25/30");

    JointOptionsDialog dialog{cc::core::JointOptionsSettings{}};
    EXPECT_EQ(dialog.size(), QSize(671, 460));
    auto b = dialog.value().bolts;
    EXPECT_TRUE(b.iso4014);
    EXPECT_TRUE(b.iso4017);
    EXPECT_EQ(b.steelClass, "8.8");
    EXPECT_EQ(dialog.value().preloaded.series, "EN 14399-3 (HR)");
    EXPECT_TRUE(b.diameters[3]);   // M24
    EXPECT_FALSE(b.diameters[4]);  // M30
    EXPECT_FALSE(b.unusualDiameters);
    cc::core::JointOptionsSettings custom;
    custom.bolts.steelClass = "10.9";
    custom.bolts.diameters[9] = true;
    custom.bolts.iso4017 = false;
    custom.preloaded.frictionClass = 'B';
    custom.preloaded.diameters[6] = false;
    custom.stiffeners.withCutouts = false;
    custom.basePlate.groutThicknessCm = 3.5;
    JointOptionsDialog other{custom};
    b = other.value().bolts;
    EXPECT_EQ(other.value().preloaded.frictionClass, 'B');
    EXPECT_FALSE(other.value().preloaded.diameters[6]);
    EXPECT_FALSE(other.value().stiffeners.withCutouts);
    EXPECT_DOUBLE_EQ(other.value().basePlate.groutThicknessCm, 3.5);
    EXPECT_EQ(b.steelClass, "10.9");
    EXPECT_TRUE(b.diameters[9]);
    EXPECT_FALSE(b.iso4017);
    buttonWithText(&other, QStringLiteral("Valores de instalación"))->click();
    EXPECT_EQ(other.value().bolts.steelClass, "8.8");
    EXPECT_EQ(other.value().preloaded.frictionClass, 'C');
    EXPECT_TRUE(other.value().stiffeners.withCutouts);
    EXPECT_DOUBLE_EQ(other.value().basePlate.groutThicknessCm, 2.0);
}

TEST(EmptyProjectWizard, OptionsButtonsPresent) {
    ensureApp();
    EmptyProjectWizard wizard;
    for (const char* text : {"Pilares", "Cimentación", "Vigas", "Uniones"}) {
        EXPECT_NE(buttonWithText(&wizard, QString::fromUtf8(text)), nullptr) << text;
    }
}

int main(int argc, char** argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

// ---------------------------------------------------------------- TK-019: Cimentación and beam tables

TEST(OptionsLeafDialogs, CleaningAndCrackingRoundTrip) {
    ensureApp();
    cc::core::CleaningConcreteSettings cleaning;
    CleaningConcreteDialog c1{cleaning};
    EXPECT_EQ(c1.size(), QSize(277, 178));
    EXPECT_FALSE(c1.value().footings);
    cleaning.pileCaps = true;
    CleaningConcreteDialog c2{cleaning};
    EXPECT_TRUE(c2.value().pileCaps);
    EXPECT_FALSE(c2.value().tieAndCenteringBeams);

    CrackingDialog cracking{cc::core::CrackingSettings{}};
    EXPECT_EQ(cracking.size(), QSize(589, 435));
    const auto spins = cracking.findChildren<QDoubleSpinBox*>();
    ASSERT_EQ(spins.size(), 2);
    EXPECT_FALSE(spins[0]->isEnabled());  // limit disabled until the check is ticked
    cracking.findChildren<QCheckBox*>().at(0)->setChecked(true);
    EXPECT_TRUE(spins[0]->isEnabled());
    EXPECT_TRUE(cracking.value().checkFootings);
    EXPECT_DOUBLE_EQ(cracking.value().footingLimitMm, 0.30);
}

TEST(OptionsLeafDialogs, PileCapGeneralAndCenteringBeams) {
    ensureApp();
    PileCapOptionsDialog pileCap{cc::core::PileCapGeneralSettings{}};
    EXPECT_EQ(pileCap.size(), QSize(428, 631));
    auto v = pileCap.value();
    EXPECT_DOUBLE_EQ(v.minOverhangOverPileM, 0.25);
    EXPECT_DOUBLE_EQ(v.maxDepthM, 2.50);
    EXPECT_DOUBLE_EQ(v.maxBarDiameterMm, 25.0);
    EXPECT_DOUBLE_EQ(v.loadFactor, 1.50);
    EXPECT_FALSE(v.withPileLoads);
    EXPECT_EQ(pileCap.findChild<QTabWidget*>()->count(), 4);
    pileCap.findChildren<QDoubleSpinBox*>().at(0)->setValue(0.30);
    EXPECT_DOUBLE_EQ(pileCap.value().minOverhangOverPileM, 0.30);
    buttonWithText(&pileCap, QStringLiteral("Valores de instalación"))->click();
    EXPECT_DOUBLE_EQ(pileCap.value().minOverhangOverPileM, 0.25);

    CenteringBeamDialog beam{cc::core::CenteringBeamSettings{}};
    EXPECT_EQ(beam.size(), QSize(399, 245));
    EXPECT_DOUBLE_EQ(beam.value().toleranceAngleDeg, 15.0);
    EXPECT_FALSE(beam.value().minTensionGeometric);
    EXPECT_DOUBLE_EQ(beam.value().minTensionRatio, 0.001);
}

TEST(OptionsLeafDialogs, FoundationTablesOnlyPredefinedSelectable) {
    ensureApp();
    FoundationTableDialog footings(FoundationTableDialog::Kind::Footings, cc::core::ReinforcementTableSource::Predefined);
    EXPECT_EQ(footings.size(), QSize(481, 267));
    EXPECT_EQ(footings.value(), cc::core::ReinforcementTableSource::Predefined);
    FoundationTableDialog ties(FoundationTableDialog::Kind::TieBeams, cc::core::ReinforcementTableSource::Predefined);
    EXPECT_EQ(ties.size(), QSize(491, 267));
    EXPECT_EQ(ties.findChildren<QRadioButton*>().size(), 3);
}

TEST(OptionsLeafDialogs, BeamTablesDefaultsAndEditing) {
    ensureApp();
    cc::core::BeamTablesSettings tables;
    TransverseTableDialog transverse{tables.transverse};
    EXPECT_EQ(transverse.size(), QSize(343, 229));
    bool diameters = false, spacings = false;
    for (QLabel* l : transverse.findChildren<QLabel*>()) {
        diameters = diameters || l->text() == QStringLiteral("Diámetros admisibles: Ø6, Ø8, Ø10, Ø12, Ø16");
        spacings = spacings || l->text() == QStringLiteral("Separaciones (cm): 5.0, 6.0, 7.0, 8.0, ..., 30.0");
    }
    EXPECT_TRUE(diameters);
    EXPECT_TRUE(spacings);

    DiameterAdmissibilityDialog holder(DiameterAdmissibilityDialog::Kind::StirrupHolder, tables.stirrupHolder);
    EXPECT_EQ(holder.size(), QSize(359, 347));
    EXPECT_TRUE(holder.value().admissible[6]);   // O25
    EXPECT_FALSE(holder.value().admissible[7]);  // O32
    holder.toggleRow(7);
    EXPECT_TRUE(holder.value().admissible[7]);
    DiameterAdmissibilityDialog skin(DiameterAdmissibilityDialog::Kind::Skin, tables.skin);
    EXPECT_EQ(skin.size(), QSize(229, 315));
    EXPECT_FALSE(skin.value().admissible[0]);  // O6 not admissible for skin reinforcement
    EXPECT_TRUE(skin.value().admissible[1]);
    EXPECT_EQ(skin.findChildren<QCheckBox*>().size(), 0);  // only the stirrup holder has the assembly check

    ReinforcementBarDialog top(QStringLiteral("Armadura superior de refuerzo"), tables.topReinforcement);
    EXPECT_EQ(top.size(), QSize(356, 582));
    auto v = top.value();
    EXPECT_DOUBLE_EQ(v.minClearSpacingCm, 2.0);
    EXPECT_FALSE(v.assemblyVsFirst[5][0]);  // montaje O20 with refuerzo O10
    EXPECT_TRUE(v.assemblyVsFirst[5][1]);
    EXPECT_FALSE(v.assemblyVsFirst[7][1]);  // montaje O32 with refuerzo O12
    EXPECT_FALSE(v.firstVsSecond[4][1]);    // refuerzo O25 with O12
    top.toggleCell(0, 0, 0);
    EXPECT_FALSE(top.value().assemblyVsFirst[0][0]);
}

TEST(OptionsLeafDialogs, DeflectionEditorsAndSummary) {
    ensureApp();
    cc::core::DeflectionLimitsSettings limits;
    EXPECT_TRUE(limits.isValid());
    DeflectionEditorDialog concrete(QStringLiteral("Hormigón"), limits.materials[0]);
    EXPECT_EQ(concrete.size(), QSize(405, 481));
    EXPECT_EQ(concrete.limit(DeflectionEditorDialog::Section::Infinite, 2), 250);
    EXPECT_EQ(concrete.limit(DeflectionEditorDialog::Section::InstantSimple, 1), 0);
    concrete.setLimit(DeflectionEditorDialog::Section::InstantSimple, 1, 400);
    EXPECT_EQ(concrete.value().instantSimple[1], 400);
    EXPECT_EQ(concrete.value().damageableMonths, 3);
    DeflectionEditorDialog rolled(QStringLiteral("Acero laminado"), limits.materials[1]);
    EXPECT_EQ(rolled.size(), QSize(385, 356));
    DeflectionEditorDialog cold(QStringLiteral("Acero conformado"), limits.materials[2]);
    EXPECT_EQ(cold.size(), QSize(385, 391));
    DeflectionEditorDialog timber(QStringLiteral("Madera"), limits.materials[3]);
    EXPECT_EQ(timber.size(), QSize(405, 481));

    // The summary dialog follows the edited values and enables one pencil per material.
    limits.materials[1].instantSimple[1] = 300;
    DeflectionLimitsDialog summary{limits};
    int pencils = 0;
    for (QToolButton* b : summary.findChildren<QToolButton*>()) pencils += b->toolTip() == QStringLiteral("Editar límites de flecha");
    EXPECT_EQ(pencils, 4);
    bool found = false;
    for (QLabel* l : summary.findChildren<QLabel*>()) found = found || l->text() == QStringLiteral("Instantánea de sobrecarga: L/ 300");
    EXPECT_TRUE(found);
    EXPECT_EQ(summary.value().materials[1].instantSimple[1], 300);
}

TEST(OptionsSettings, FoundationAndBeamTableDefaults) {
    cc::core::OptionsSettings o;
    EXPECT_TRUE(o.isValid());
    EXPECT_FALSE(o.foundation.cleaning.footings);
    EXPECT_DOUBLE_EQ(o.foundation.cracking.footingLimitMm, 0.30);
    EXPECT_EQ(o.beams.tables.transverse.spacingsCm.size(), 26u);
    EXPECT_EQ(cc::app::TransverseTableDialog::summarise({1.0, 2.0}, 1).toStdString(), "1.0, 2.0");
    o.foundation.pileCap.maxDepthM = 0.1;  // below the minimum depth
    EXPECT_FALSE(o.isValid());
}
