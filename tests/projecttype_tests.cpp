// Tests for the project-type dialog, run on Qt's offscreen platform (nothing is displayed).
#include <gtest/gtest.h>

#include <QApplication>
#include <QRadioButton>

#include "ProjectTypeDialog.h"

using cc::app::ProjectTypeDialog;

namespace {
void ensureApp() {
    static int argc = 1;
    static char name[] = "projecttype_tests";
    static char* argv[] = {name, nullptr};
    static QApplication instance(argc, argv);
}
}  // namespace

TEST(ProjectTypeDialog, DefaultsToEmptyProject) {
    ensureApp();
    ProjectTypeDialog dialog;
    EXPECT_EQ(dialog.projectType(), ProjectTypeDialog::ProjectType::Empty);
}

TEST(ProjectTypeDialog, IfcIsSelectableButNotYetAvailable) {
    ensureApp();
    ProjectTypeDialog dialog;
    for (QRadioButton* r : dialog.findChildren<QRadioButton*>()) {
        if (r->text().contains("IFC")) r->setChecked(true);
    }
    EXPECT_EQ(dialog.projectType(), ProjectTypeDialog::ProjectType::Ifc);
    EXPECT_FALSE(ProjectTypeDialog::isAvailable(ProjectTypeDialog::ProjectType::Ifc));
    EXPECT_TRUE(ProjectTypeDialog::isAvailable(ProjectTypeDialog::ProjectType::Empty));
}

int main(int argc, char** argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
