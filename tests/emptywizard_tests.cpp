// Tests for the "Obra vacía" wizard and its sub-dialogs, run on Qt's offscreen platform.
#include <gtest/gtest.h>

#include <QApplication>
#include <QComboBox>
#include <QPushButton>
#include <QRadioButton>

#include "EmptyProjectWizard.h"
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
    EXPECT_FALSE(buttonWithText(&wizard, QStringLiteral("Estados límite (combinaciones)"))->isEnabled());
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

int main(int argc, char** argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
