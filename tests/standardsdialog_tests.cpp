// Tests for the standards-selection dialog, run on Qt's offscreen platform (nothing is displayed).
#include <gtest/gtest.h>

#include <QApplication>
#include <QComboBox>

#include "StandardsDialog.h"

using cc::app::StandardsDialog;
using cc::core::StandardFamily;
using cc::core::StandardsSelection;

namespace {
void ensureApp() {
    static int argc = 1;
    static char name[] = "standardsdialog_tests";
    static char* argv[] = {name, nullptr};
    static QApplication instance(argc, argv);
}
}  // namespace

TEST(StandardsDialog, RoundTripsInitialSelection) {
    ensureApp();
    StandardsSelection initial;
    StandardsDialog dialog(initial);
    EXPECT_EQ(dialog.selection().concrete, initial.concrete);
    EXPECT_EQ(dialog.selection().timber, initial.timber);
    EXPECT_TRUE(dialog.selection().isValid());
}

TEST(StandardsDialog, ReflectsUserChoice) {
    ensureApp();
    StandardsDialog dialog{StandardsSelection{}};
    auto combos = dialog.findChildren<QComboBox*>();
    ASSERT_EQ(combos.size(), 5);
    combos.front()->setCurrentText("EHE-08");
    EXPECT_EQ(dialog.selection().concrete, "EHE-08");
}

int main(int argc, char** argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
