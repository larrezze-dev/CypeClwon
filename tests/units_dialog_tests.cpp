#include <gtest/gtest.h>

#include <QApplication>
#include <QComboBox>
#include <QPushButton>

#include "UnitsDialog.h"

using namespace cc::app;
using cc::core::DimensioningUnits;
using cc::core::LengthUnit;

namespace {
void ensureApp() {
    static int argc = 1;
    static char name[] = "units_dialog_tests";
    static char* argv[] = {name, nullptr};
    static QApplication instance(argc, argv);
}
}  // namespace

TEST(UnitsDialog, ShowsTheOriginalDefaultsAndSize) {
    ensureApp();
    UnitsDialog dialog{DimensioningUnits{}};
    EXPECT_EQ(dialog.size(), QSize(224, 174));
    EXPECT_EQ(dialog.dimensionsCombo()->currentText(), "Centímetros");
    EXPECT_EQ(dialog.reinforcementCombo()->currentText(), "Centímetros");
    EXPECT_EQ(dialog.lengthsCombo()->currentText(), "Metros");
    EXPECT_EQ(dialog.dimensionsCombo()->count(), 3);
    EXPECT_EQ(dialog.dimensionsCombo()->itemText(2), "Milímetros");
    EXPECT_TRUE(dialog.value() == DimensioningUnits{});
}

TEST(UnitsDialog, ValueFollowsTheCombos) {
    ensureApp();
    UnitsDialog dialog{DimensioningUnits{}};
    dialog.lengthsCombo()->setCurrentIndex(2);
    dialog.dimensionsCombo()->setCurrentIndex(0);
    const auto v = dialog.value();
    EXPECT_EQ(v.lengths, LengthUnit::Millimetres);
    EXPECT_EQ(v.dimensions, LengthUnit::Metres);
    EXPECT_EQ(v.reinforcement, LengthUnit::Centimetres);
    EXPECT_EQ(dialog.acceptButton()->text(), "Aceptar");
    EXPECT_EQ(dialog.cancelButton()->text(), "Cancelar");
}

int main(int argc, char** argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
