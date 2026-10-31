// Input handling tests for the 3D viewport. The widget is never shown, so no OpenGL context is needed.
#include <gtest/gtest.h>

#include <QApplication>
#include <QMouseEvent>
#include <QSignalSpy>
#include <QWheelEvent>

#include "Viewport3D.h"

using cc::app::DragAction;
using cc::app::Viewport3D;

namespace {
QApplication* app() {
    static int argc = 1;
    static char name[] = "viewport3d_tests";
    static char* argv[] = {name, nullptr};
    static QApplication instance(argc, argv);
    return &instance;
}

void mouse(Viewport3D& v, QEvent::Type type, QPointF pos, Qt::MouseButton button, Qt::MouseButtons buttons) {
    QMouseEvent e(type, pos, v.mapToGlobal(pos), button, buttons, Qt::NoModifier);
    QApplication::sendEvent(&v, &e);
}

void drag(Viewport3D& v, Qt::MouseButton button, QPointF from, QPointF to) {
    mouse(v, QEvent::MouseButtonPress, from, button, button);
    mouse(v, QEvent::MouseMove, to, Qt::NoButton, button);
    mouse(v, QEvent::MouseButtonRelease, to, button, Qt::NoButton);
}

void wheel(Viewport3D& v, QPointF pos, int delta) {
    QWheelEvent e(pos, v.mapToGlobal(pos), QPoint(), QPoint(0, delta), Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
    QApplication::sendEvent(&v, &e);
}

class ViewportTest : public ::testing::Test {
protected:
    void SetUp() override {
        app();
        view = std::make_unique<Viewport3D>();
        view->resize(800, 600);
    }
    std::unique_ptr<Viewport3D> view;
};
}  // namespace

TEST_F(ViewportTest, OrbitDragChangesYawAndPitch) {
    view->bindings().left = DragAction::Orbit;
    const double yaw = view->camera().yawDeg, pitch = view->camera().pitchDeg;
    drag(*view, Qt::LeftButton, {400, 300}, {450, 330});
    EXPECT_NE(view->camera().yawDeg, yaw);
    EXPECT_NE(view->camera().pitchDeg, pitch);
}

TEST_F(ViewportTest, UnboundButtonDoesNothing) {
    view->bindings().left = DragAction::None;
    const auto before = view->camera().yawDeg;
    drag(*view, Qt::LeftButton, {400, 300}, {500, 400});
    EXPECT_EQ(view->camera().yawDeg, before);
}

TEST_F(ViewportTest, PanDragMovesTheTarget) {
    view->bindings().middle = DragAction::Pan;
    const auto target = view->camera().target;
    drag(*view, Qt::MiddleButton, {400, 300}, {450, 300});
    EXPECT_NE(view->camera().target, target);
}

TEST_F(ViewportTest, WheelZoomsInAndOut) {
    const double d0 = view->camera().distance;
    wheel(*view, {400, 300}, 120);
    const double d1 = view->camera().distance;
    EXPECT_LT(d1, d0);
    wheel(*view, {400, 300}, -120);
    EXPECT_GT(view->camera().distance, d1);
}

TEST_F(ViewportTest, WheelAtCursorKeepsThePointUnderTheCursor) {
    const QPointF cursor(600, 200);
    const auto before = view->screenToTargetPlane(cursor);
    wheel(*view, cursor, 240);
    QPointF screen;
    ASSERT_TRUE(view->worldToScreen(before, screen));
    EXPECT_NEAR(screen.x(), cursor.x(), 1.0);
    EXPECT_NEAR(screen.y(), cursor.y(), 1.0);
}

TEST_F(ViewportTest, ProjectionToggleEmitsOnlyOnChange) {
    QSignalSpy spy(view.get(), &Viewport3D::projectionChanged);
    EXPECT_FALSE(view->isOrthographic());
    view->setOrthographic(true);
    view->setOrthographic(true);
    EXPECT_TRUE(view->isOrthographic());
    EXPECT_EQ(spy.count(), 1);
    view->setOrthographic(false);
    EXPECT_EQ(spy.count(), 2);
}

TEST_F(ViewportTest, TargetProjectsToTheCentre) {
    QPointF screen;
    ASSERT_TRUE(view->worldToScreen(view->camera().target, screen));
    EXPECT_NEAR(screen.x(), 400.0, 1e-6);
    EXPECT_NEAR(screen.y(), 300.0, 1e-6);
}

TEST_F(ViewportTest, MouseMoveReportsCursorWorldPosition) {
    QSignalSpy spy(view.get(), &Viewport3D::cursorMoved);
    mouse(*view, QEvent::MouseMove, {400, 300}, Qt::NoButton, Qt::NoButton);
    ASSERT_EQ(spy.count(), 1);
}

TEST_F(ViewportTest, StandardViewIsApplied) {
    view->setStandardView(cc::core::StandardView::Top);
    EXPECT_NEAR(view->camera().pitchDeg, 90.0, 1e-9);
}

int main(int argc, char** argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
