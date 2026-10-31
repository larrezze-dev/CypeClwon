// GUI tests for the MDI work area. They run on Qt's "offscreen" platform, so no window is shown.
#include <gtest/gtest.h>

#include <memory>

#include <QAction>
#include <QApplication>
#include <QMdiArea>
#include <QMdiSubWindow>
#include <QWidget>

#include "WorkArea.h"

using cc::app::WorkArea;

namespace {
QApplication* app() {
    static int argc = 1;
    static char name[] = "workarea_tests";
    static char* argv[] = {name, nullptr};
    static QApplication instance(argc, argv);
    return &instance;
}

class WorkAreaTest : public ::testing::Test {
protected:
    // QApplication must exist before any widget, so the work area is built in SetUp.
    void SetUp() override {
        app();
        holder = std::make_unique<WorkArea>();
        holder->resize(800, 600);
        holder->show();
    }
    void TearDown() override { holder.reset(); }
    std::unique_ptr<WorkArea> holder;
};
#define area (*holder)
}  // namespace

TEST_F(WorkAreaTest, StartsEmptyWithActionsDisabled) {
    EXPECT_EQ(area.windowCount(), 0);
    EXPECT_FALSE(area.cascadeAction()->isEnabled());
    EXPECT_FALSE(area.nextAction()->isEnabled());
    EXPECT_FALSE(area.closeActiveAction()->isEnabled());
    EXPECT_FALSE(area.closeAllAction()->isEnabled());
}

TEST_F(WorkAreaTest, FirstWindowOpensMaximisedAndEnablesClose) {
    QMdiSubWindow* w = area.addDocumentWindow("3D", new QWidget);
    EXPECT_EQ(area.windowCount(), 1);
    EXPECT_TRUE(w->isMaximized());
    EXPECT_TRUE(area.closeActiveAction()->isEnabled());
    EXPECT_FALSE(area.tileAction()->isEnabled());  // needs at least two windows
}

TEST_F(WorkAreaTest, TwoWindowsEnableArrangeAndNavigation) {
    area.addDocumentWindow("3D", new QWidget);
    area.addDocumentWindow("3D (2)", new QWidget);
    for (QAction* a : {area.cascadeAction(), area.tileAction(), area.tileHorizontallyAction(),
                       area.tileVerticallyAction(), area.nextAction(), area.previousAction()}) {
        EXPECT_TRUE(a->isEnabled()) << a->text().toStdString();
    }
}

TEST_F(WorkAreaTest, TileHorizontallyStacksWindowsVertically) {
    QMdiSubWindow* a = area.addDocumentWindow("A", new QWidget);
    QMdiSubWindow* b = area.addDocumentWindow("B", new QWidget);
    area.tileHorizontallyAction()->trigger();
    EXPECT_EQ(a->width(), b->width());
    EXPECT_EQ(a->x(), b->x());
    EXPECT_LT(a->y(), b->y());
    EXPECT_NEAR(a->height(), b->height(), 1);
}

TEST_F(WorkAreaTest, TileVerticallyPlacesWindowsSideBySide) {
    QMdiSubWindow* a = area.addDocumentWindow("A", new QWidget);
    QMdiSubWindow* b = area.addDocumentWindow("B", new QWidget);
    area.tileVerticallyAction()->trigger();
    EXPECT_EQ(a->height(), b->height());
    EXPECT_EQ(a->y(), b->y());
    EXPECT_LT(a->x(), b->x());
    EXPECT_NEAR(a->width(), b->width(), 1);
}

TEST_F(WorkAreaTest, NextAndPreviousChangeActiveWindow) {
    QMdiSubWindow* a = area.addDocumentWindow("A", new QWidget);
    QMdiSubWindow* b = area.addDocumentWindow("B", new QWidget);
    EXPECT_EQ(area.mdiArea()->activeSubWindow(), b);
    area.nextAction()->trigger();
    EXPECT_EQ(area.mdiArea()->activeSubWindow(), a);
    area.previousAction()->trigger();
    EXPECT_EQ(area.mdiArea()->activeSubWindow(), b);
}

TEST_F(WorkAreaTest, CloseAndCloseAll) {
    area.addDocumentWindow("A", new QWidget);
    area.addDocumentWindow("B", new QWidget);
    area.closeActiveAction()->trigger();
    EXPECT_EQ(area.windowCount(), 1);
    area.closeAllAction()->trigger();
    EXPECT_EQ(area.windowCount(), 0);
    EXPECT_FALSE(area.closeAllAction()->isEnabled());
}

int main(int argc, char** argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
