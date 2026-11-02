// View bar and projection palette tests; nothing is shown (offscreen platform, no OpenGL context needed).
#include <gtest/gtest.h>

#include <QApplication>
#include <QSignalSpy>
#include <QToolButton>
#include <memory>

#include "ViewBar.h"
#include "Viewport3D.h"

using cc::app::ProjectionPalette;
using cc::app::ViewBar;
using cc::app::Viewport3D;
using cc::app::ViewportFrame;

namespace {
QApplication* app() {
    static int argc = 1;
    static char name[] = "viewbar_tests";
    static char* argv[] = {name, nullptr};
    static QApplication instance(argc, argv);
    return &instance;
}
class ViewBarTest : public ::testing::Test {
protected:
    void SetUp() override {
        app();
        frame = std::make_unique<ViewportFrame>();
        frame->resize(800, 600);
    }
    std::unique_ptr<ViewportFrame> frame;
};
}  // namespace

TEST_F(ViewBarTest, HasTenButtonsWithTheTooltipsOfTheOriginal) {
    ViewBar* bar = frame->viewBar();
    EXPECT_EQ(bar->width(), 24);
    EXPECT_TRUE(bar->button(ViewBar::Projection)->toolTip().startsWith(QStringLiteral("Proyección: abre una ventana")));
    EXPECT_EQ(bar->button(ViewBar::ProjectionType)->toolTip(),
              QStringLiteral("Tipo de proyección: cambia el tipo alternativamente (axonométrica/cónica)."));
    EXPECT_TRUE(bar->button(ViewBar::RotateAroundCamera)->toolTip().contains(QStringLiteral("eje vertical de la cámara")));
    EXPECT_TRUE(bar->button(ViewBar::SectionY)->toolTip().contains(QStringLiteral("eje 'Y' global")));
    EXPECT_TRUE(bar->button(ViewBar::ClippingVolume)->toolTip().contains(QStringLiteral("6 planos de corte")));
    int buttons = 0;
    for (QToolButton* b : bar->findChildren<QToolButton*>(Qt::FindDirectChildrenOnly)) buttons += b ? 1 : 0;
    EXPECT_EQ(buttons, ViewBar::ButtonCount);
}

TEST_F(ViewBarTest, OnlyTheImplementedButtonsAreEnabled) {
    ViewBar* bar = frame->viewBar();
    for (int i = 0; i < ViewBar::ButtonCount; ++i) {
        const auto b = static_cast<ViewBar::Button>(i);
        EXPECT_EQ(bar->button(b)->isEnabled(), i <= ViewBar::RotateAroundCamera) << i;
    }
}

TEST_F(ViewBarTest, PaletteHasTwentyTwoEnabledCells) {
    ProjectionPalette* palette = frame->viewBar()->palette();
    ASSERT_EQ(palette->cellCount(), 22);
    EXPECT_EQ(palette->enabledCellCount(), 22);
}

TEST_F(ViewBarTest, ProjectionButtonTogglesThePalette) {
    ViewBar* bar = frame->viewBar();
    QSignalSpy spy(bar, &ViewBar::paletteToggled);
    bar->button(ViewBar::Projection)->click();
    EXPECT_TRUE(bar->paletteVisible());
    bar->button(ViewBar::Projection)->click();
    EXPECT_FALSE(bar->paletteVisible());
    ASSERT_EQ(spy.count(), 2);
    EXPECT_TRUE(spy.at(0).at(0).toBool());
    EXPECT_FALSE(spy.at(1).at(0).toBool());
}

TEST_F(ViewBarTest, ClickingACellEmitsAndAppliesThePreset) {
    ViewBar* bar = frame->viewBar();
    QSignalSpy spy(bar, &ViewBar::presetChosen);
    bar->palette()->cell(1)->click();
    ASSERT_EQ(spy.count(), 1);
    EXPECT_EQ(spy.at(0).at(0).toInt(), 1);
    EXPECT_EQ(frame->viewport()->camera().pitchDeg, 90.0);
    EXPECT_TRUE(frame->viewport()->canGoPrevious());
}

TEST_F(ViewBarTest, ProjectionTypeButtonSwitchesBetweenConicalAndAxonometric) {
    ViewBar* bar = frame->viewBar();
    QSignalSpy spy(bar, &ViewBar::projectionTypeToggled);
    EXPECT_FALSE(frame->viewport()->isOrthographic());
    bar->button(ViewBar::ProjectionType)->click();
    EXPECT_TRUE(frame->viewport()->isOrthographic());
    bar->button(ViewBar::ProjectionType)->click();
    EXPECT_FALSE(frame->viewport()->isOrthographic());
    EXPECT_EQ(spy.count(), 2);
}

TEST_F(ViewBarTest, RotationButtonsEmitAndDriveTheViewport) {
    ViewBar* bar = frame->viewBar();
    QSignalSpy point(bar, &ViewBar::rotateAroundPointToggled);
    QSignalSpy camera(bar, &ViewBar::rotateAroundCameraToggled);
    bar->button(ViewBar::RotateAroundPoint)->click();
    bar->button(ViewBar::RotateAroundCamera)->click();
    EXPECT_EQ(point.count(), 1);
    EXPECT_EQ(camera.count(), 1);
    EXPECT_TRUE(frame->viewport()->rotateAroundPoint());
    EXPECT_TRUE(frame->viewport()->rotateAroundCamera());
    bar->button(ViewBar::RotateAroundPoint)->click();
    EXPECT_FALSE(frame->viewport()->rotateAroundPoint());
}

TEST_F(ViewBarTest, DisabledButtonsEmitNothing) {
    ViewBar* bar = frame->viewBar();
    QSignalSpy spy(bar, &ViewBar::paletteToggled);
    bar->button(ViewBar::SectionX)->click();
    bar->button(ViewBar::ClippingVolume)->click();
    EXPECT_EQ(spy.count(), 0);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
