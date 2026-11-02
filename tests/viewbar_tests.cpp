// View bar and projection palette tests; nothing is shown (offscreen platform, no OpenGL context needed).
#include <gtest/gtest.h>

#include <QApplication>
#include <QSignalSpy>
#include <QToolButton>
#include <memory>

#include <QCheckBox>

#include "ViewBar.h"
#include "ViewOptionsDialog.h"
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

TEST_F(ViewBarTest, AllTenButtonsAreEnabled) {
    ViewBar* bar = frame->viewBar();
    for (int i = 0; i < ViewBar::ButtonCount; ++i) EXPECT_TRUE(bar->button(static_cast<ViewBar::Button>(i))->isEnabled()) << i;
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
    EXPECT_TRUE(frame->viewport()->isOrthographic());
    bar->button(ViewBar::ProjectionType)->click();
    EXPECT_FALSE(frame->viewport()->isOrthographic());
    bar->button(ViewBar::ProjectionType)->click();
    EXPECT_TRUE(frame->viewport()->isOrthographic());
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

TEST_F(ViewBarTest, SectionButtonsToggleThePlanesOfTheViewport) {
    ViewBar* bar = frame->viewBar();
    Viewport3D* v = frame->viewport();
    v->setNodes({{0, 0, 0}, {10, 4, 6}});
    bar->button(ViewBar::SectionX)->click();
    EXPECT_TRUE(v->sectionActive(0));
    EXPECT_DOUBLE_EQ(v->clipping().sections[0].position, 5.0);  // starts at the middle of the envelope (plus symmetric margin)
    EXPECT_TRUE(bar->button(ViewBar::SectionX)->isChecked());
    bar->button(ViewBar::SectionX)->click();
    EXPECT_FALSE(v->sectionActive(0));
}

TEST_F(ViewBarTest, ClippingVolumeFollowsTheGeometricEnvelope) {
    Viewport3D* v = frame->viewport();
    v->setNodes({{0, 0, 0}, {10, 4, 6}});
    frame->viewBar()->button(ViewBar::ClippingVolume)->click();
    ASSERT_TRUE(v->clippingVolumeActive());
    EXPECT_DOUBLE_EQ(v->clipping().volume.max.x, 10.0);
    EXPECT_DOUBLE_EQ(v->clipping().volume.min.z, 0.0);
    EXPECT_EQ(v->clipHandles().size(), 6u);
    EXPECT_TRUE(v->visibleAfterClipping({5, 2, 3}));
    EXPECT_FALSE(v->visibleAfterClipping({11, 2, 3}));
}

TEST_F(ViewBarTest, DraggingASectionArrowMovesThePlaneAndDoubleClickFlipsIt) {
    Viewport3D* v = frame->viewport();
    v->setNodes({{0, 0, 0}, {10, 4, 6}});
    v->show();
    v->resize(600, 400);
    v->setSectionActive(0, true);
    const auto handles = v->clipHandles();
    ASSERT_EQ(handles.size(), 1u);
    QPointF a, b;
    ASSERT_TRUE(v->worldToScreen(handles[0].base, a));
    ASSERT_TRUE(v->worldToScreen(handles[0].tip, b));
    const QPointF mid = (a + b) / 2.0;
    ASSERT_TRUE(v->clipHandleAt(mid).has_value());
    EXPECT_FALSE(v->clipHandleAt(QPointF(2, 2)).has_value());
    const double before = v->clipping().sections[0].position;
    v->dragClipHandle(*v->clipHandleAt(mid), mid, mid + (b - a) * 0.5);  // half an arrow along the axis
    EXPECT_GT(v->clipping().sections[0].position, before);
    EXPECT_TRUE(v->flipSection(0));
    EXPECT_TRUE(v->clipping().sections[0].flipped);
    EXPECT_FALSE(v->flipSection(1));  // inactive section
}

TEST_F(ViewBarTest, Button411TogglesTheVisibilityOfThePlanesButKeepsTheCut) {
    ViewBar* bar = frame->viewBar();
    Viewport3D* v = frame->viewport();
    EXPECT_EQ(bar->button(ViewBar::HideSectionPlanes)->toolTip(), QStringLiteral("Mostrar / Ocultar planos de sección"));
    EXPECT_TRUE(v->clipPlanesVisible());
    v->setNodes({{0, 0, 0}, {10, 4, 6}});
    bar->button(ViewBar::SectionX)->click();
    bar->button(ViewBar::HideSectionPlanes)->click();  // pressed = hidden
    EXPECT_FALSE(v->clipPlanesVisible());
    EXPECT_TRUE(v->sectionActive(0));
    EXPECT_FALSE(v->visibleAfterClipping({9, 2, 3}));  // the cut stays
    bar->button(ViewBar::HideSectionPlanes)->click();
    EXPECT_TRUE(v->clipPlanesVisible());
}

TEST_F(ViewBarTest, OptionsButtonOpensTheDialogWithBothBoxesUnchecked) {
    Viewport3D* v = frame->viewport();
    EXPECT_FALSE(v->objectEdges());
    EXPECT_FALSE(v->ssao());
    cc::app::ViewOptionsDialog dialog(v->objectEdges(), v->ssao());
    EXPECT_EQ(dialog.size(), QSize(265, 136));
    EXPECT_FALSE(dialog.edgesBox()->isChecked());
    EXPECT_FALSE(dialog.ssaoBox()->isChecked());
    EXPECT_EQ(dialog.ssaoBox()->text(), QStringLiteral("Iluminación ambiental mejorada (SSAO)"));
    dialog.edgesBox()->setChecked(true);
    EXPECT_TRUE(dialog.objectEdges());
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
