// Tests for the "Nueva obra" dialog logic. Run on Qt's offscreen platform (nothing is displayed).
#include <gtest/gtest.h>

#include <QApplication>

#include "NewProjectDialog.h"

using cc::app::NewProjectDialog;

namespace {
void ensureApp() {
    static int argc = 1;
    static char name[] = "newproject_tests";
    static char* argv[] = {name, nullptr};
    static QApplication instance(argc, argv);
}
}  // namespace

TEST(NewProjectDialog, AcceptsOrdinaryNames) {
    ensureApp();
    EXPECT_TRUE(NewProjectDialog::validateFileName("nuevo").isEmpty());
    EXPECT_TRUE(NewProjectDialog::validateFileName("Nave industrial 2").isEmpty());
    EXPECT_TRUE(NewProjectDialog::validateFileName("obra.v2").isEmpty());
}

TEST(NewProjectDialog, RejectsEmptyAndBlank) {
    ensureApp();
    EXPECT_FALSE(NewProjectDialog::validateFileName("").isEmpty());
    EXPECT_FALSE(NewProjectDialog::validateFileName("   ").isEmpty());
}

TEST(NewProjectDialog, RejectsInvalidCharacters) {
    ensureApp();
    for (const char* bad : {"a/b", "a\b", "a:b", "a*b", "a?b", "a\"b", "a<b", "a>b", "a|b"}) {
        EXPECT_FALSE(NewProjectDialog::validateFileName(bad).isEmpty()) << bad;
    }
}

TEST(NewProjectDialog, RejectsReservedNamesAndTrailingDot) {
    ensureApp();
    for (const char* bad : {"CON", "nul", "Com1", "LPT9", "aux.txt", "obra."}) {
        EXPECT_FALSE(NewProjectDialog::validateFileName(bad).isEmpty()) << bad;
    }
    EXPECT_TRUE(NewProjectDialog::validateFileName("console").isEmpty());
}

TEST(NewProjectDialog, FilePathCombinesFolderNameAndSuffix) {
    ensureApp();
    NewProjectDialog dialog("D:/obras/test");
    EXPECT_EQ(dialog.fileName(), QString("nuevo"));
    EXPECT_EQ(dialog.filePath(), QString("D:/obras/test/nuevo.ed3"));
    EXPECT_EQ(dialog.description(), QString());
}

int main(int argc, char** argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
