// Tests for the "Estados límite" dialogs, run on Qt's offscreen platform.
#include <gtest/gtest.h>

#include <QApplication>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>

#include "LimitStatesDialog.h"

using namespace cc::app;
using namespace cc::core;

namespace {
void ensureApp() {
    static int argc = 1;
    static char name[] = "limitstates_dialog_tests";
    static char* argv[] = {name, nullptr};
    static QApplication instance(argc, argv);
}

bool hasLabel(QWidget* parent, const QString& text) {
    for (QLabel* l : parent->findChildren<QLabel*>()) {
        if (l->text().contains(text)) return true;
    }
    return false;
}
}  // namespace

TEST(LimitStatesDialog, SizeAndHeadersFollowStandards) {
    ensureApp();
    LimitStatesDialog dialog{StandardsSelection{}, LimitStatesSettings{}};
    EXPECT_EQ(dialog.size(), QSize(358, 438));
    EXPECT_TRUE(hasLabel(&dialog, "Hormigón: Eurocódigo 2"));
    EXPECT_TRUE(hasLabel(&dialog, "Hormigón en cimentaciones: Eurocódigo 2"));
    EXPECT_TRUE(hasLabel(&dialog, "Aluminio: Eurocódigo 9"));
    EXPECT_TRUE(hasLabel(&dialog, "Acero conformado: CTE DB SE-A"));
    EXPECT_TRUE(hasLabel(&dialog, "Madera: CTE DB SE-M"));
    EXPECT_TRUE(hasLabel(&dialog, "Acero laminado: Código Estructural"));
    EXPECT_EQ(dialog.findChildren<QComboBox*>().size(), 3);
}

TEST(LimitStatesDialog, ValueRoundTrip) {
    ensureApp();
    LimitStatesSettings s;
    s.hotRolledCode = CombinationCode::EC;
    LimitStatesDialog dialog{StandardsSelection{}, s};
    EXPECT_EQ(dialog.value().hotRolledCode, CombinationCode::EC);
    for (QRadioButton* r : dialog.findChildren<QRadioButton*>()) {
        if (r->text() == "CTE") r->setChecked(true);
    }
    EXPECT_EQ(dialog.value().hotRolledCode, CombinationCode::CTE);
    EXPECT_TRUE(dialog.value().isValid());
}

TEST(LimitStatesDialog, VerifyDialogListsEightRows) {
    ensureApp();
    LimitStatesVerifyDialog dialog{LimitStatesSettings{}};
    EXPECT_EQ(dialog.size(), QSize(398, 356));
    // Eight row buttons plus Aceptar and Cancelar.
    EXPECT_EQ(dialog.findChildren<QPushButton*>().size(), 10);
    EXPECT_TRUE(hasLabel(&dialog, "EC - Altitud inferior o igual a 1000 m"));
    EXPECT_TRUE(hasLabel(&dialog, "Acciones características"));
}

int main(int argc, char** argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
