// Input handling tests for the 3D viewport. The widget is never shown, so no OpenGL context is needed.
#include <gtest/gtest.h>

#include <QApplication>
#include <QKeyEvent>
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
    view->setLeftDragAction(DragAction::Orbit);
    const double yaw = view->camera().yawDeg, pitch = view->camera().pitchDeg;
    drag(*view, Qt::LeftButton, {400, 300}, {450, 330});
    EXPECT_NE(view->camera().yawDeg, yaw);
    EXPECT_NE(view->camera().pitchDeg, pitch);
}

TEST_F(ViewportTest, DefaultBindingsMatchTheOriginal) {
    const auto& b = view->bindings();
    EXPECT_EQ(b.left, DragAction::None);
    EXPECT_EQ(b.middle, DragAction::Pan);
    EXPECT_EQ(b.right, DragAction::None);
    drag(*view, Qt::RightButton, {400, 300}, {500, 400});
    EXPECT_EQ(view->camera().yawDeg, 45.0);
}

TEST_F(ViewportTest, OrbitToolMakesTheLeftButtonOrbit) {
    view->setLeftDragAction(DragAction::Orbit);
    drag(*view, Qt::LeftButton, {400, 300}, {440, 300});
    EXPECT_NEAR(view->camera().yawDeg, 45.0 - 40 * 0.25, 1e-9);
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

TEST_F(ViewportTest, NodesAreStoredForTheGuideLines) {
    Viewport3D v;
    EXPECT_TRUE(v.nodes().empty());
    v.setNodes({{0, 0, 0}, {5, 0, 3}});
    ASSERT_EQ(v.nodes().size(), 2u);
    EXPECT_DOUBLE_EQ(v.nodes()[1].z, 3.0);
}

// ---- Selection mode (T-P4-03) ----
namespace {
void click(Viewport3D& v, QPointF pos, Qt::MouseButton button = Qt::LeftButton, Qt::KeyboardModifiers mods = Qt::NoModifier) {
    QMouseEvent press(QEvent::MouseButtonPress, pos, v.mapToGlobal(pos), button, button, mods);
    QApplication::sendEvent(&v, &press);
    QMouseEvent release(QEvent::MouseButtonRelease, pos, v.mapToGlobal(pos), button, Qt::NoButton, mods);
    QApplication::sendEvent(&v, &release);
}

QPointF screenOf(const Viewport3D& v, double x, double z) {
    QPointF p;
    v.worldToScreen({x, 0, z}, p);
    return p;
}

class SelectionTest : public ViewportTest {
protected:
    void SetUp() override {
        ViewportTest::SetUp();
        const int n1 = model.addNode({0, 0, 0}), n2 = model.addNode({4, 0, 0});
        const int n3 = model.addNode({0, 0, 3}), n4 = model.addNode({4, 0, 3});
        model.addBar(n1, n3);
        model.addBar(n2, n4);
        model.addBar(n3, n4);
        view->setStandardView(cc::core::StandardView::Front);
        view->setOrthographic(true);
        view->camera().target = {2, 0, 1.5};
        view->setStructure(&model);
        view->setSelectionMode(true);
    }
    cc::core::StructuralModel model;
};
}  // namespace

TEST_F(SelectionTest, IsOffByDefaultAndAnnouncesItself) {
    Viewport3D v;
    EXPECT_FALSE(v.selectionMode());
    QSignalSpy hint(&v, &Viewport3D::statusHint);
    v.setSelectionMode(true);
    ASSERT_EQ(hint.count(), 1);
    EXPECT_TRUE(hint.at(0).at(0).toString().startsWith(QStringLiteral("LMB:")));
    v.setSelectionMode(false);
    EXPECT_TRUE(hint.at(1).at(0).toString().isEmpty());
}

TEST_F(SelectionTest, PlainModeIgnoresClicks) {
    view->setSelectionMode(false);
    click(*view, screenOf(*view, 0, 0));
    EXPECT_TRUE(view->selection().empty());
}

TEST_F(SelectionTest, ClickSelectsANodeAndHoverFollowsTheCursor) {
    mouse(*view, QEvent::MouseMove, screenOf(*view, 4, 3), Qt::NoButton, Qt::NoButton);
    ASSERT_TRUE(view->hovered());
    EXPECT_EQ(view->hovered()->kind, cc::core::ElementKind::Node);
    EXPECT_EQ(view->hovered()->id, 4);
    mouse(*view, QEvent::MouseMove, screenOf(*view, 2, 1.5), Qt::NoButton, Qt::NoButton);
    EXPECT_FALSE(view->hovered());
    QSignalSpy spy(view.get(), &Viewport3D::selectionChanged);
    click(*view, screenOf(*view, 4, 3));
    EXPECT_EQ(spy.count(), 1);
    EXPECT_TRUE(view->selection().contains({cc::core::ElementKind::Node, 4}));
}

TEST_F(SelectionTest, CtrlAddsAndShiftRemoves) {
    click(*view, screenOf(*view, 0, 0));
    click(*view, screenOf(*view, 4, 0), Qt::LeftButton, Qt::ControlModifier);
    EXPECT_EQ(view->selection().size(), 2u);
    click(*view, screenOf(*view, 0, 0), Qt::LeftButton, Qt::ShiftModifier);
    EXPECT_EQ(view->selection().size(), 1u);
    click(*view, screenOf(*view, 0, 3));  // plain click replaces
    EXPECT_EQ(view->selection().size(), 1u);
    EXPECT_TRUE(view->selection().contains({cc::core::ElementKind::Node, 3}));
}

TEST_F(SelectionTest, TwoClickWindowSelectsWhatIsFullyInside) {
    click(*view, screenOf(*view, -1, 4));
    EXPECT_TRUE(view->windowPending());
    EXPECT_TRUE(view->selection().empty());
    mouse(*view, QEvent::MouseMove, screenOf(*view, 3, -1), Qt::NoButton, Qt::NoButton);
    click(*view, screenOf(*view, 3, -1));
    EXPECT_FALSE(view->windowPending());
    EXPECT_EQ(view->selection().ids(cc::core::ElementKind::Node), (std::vector<int>{1, 3}));  // x = 4 is outside
    EXPECT_EQ(view->selection().ids(cc::core::ElementKind::Bar), std::vector<int>{1});  // the lintel is cut
}

TEST_F(SelectionTest, TwoClickCrossingTouchesTheLintel) {
    click(*view, screenOf(*view, 3, 4));
    click(*view, screenOf(*view, -1, 2));  // right to left, crosses the lintel and the left column
    EXPECT_EQ(view->selection().ids(cc::core::ElementKind::Bar), (std::vector<int>{1, 3}));
    EXPECT_EQ(view->selection().ids(cc::core::ElementKind::Node), std::vector<int>{3});
}

TEST_F(SelectionTest, FilterLimitsWhatIsPicked) {
    view->setSelectionFilter(cc::core::SelectionFilter::only(cc::core::ElementKind::Bar));
    click(*view, screenOf(*view, 0, 0));  // node 1 is rejected, the click lands on bar 1 instead
    EXPECT_EQ(view->selection().ids(cc::core::ElementKind::Bar), std::vector<int>{1});
    EXPECT_EQ(view->selection().count(cc::core::ElementKind::Node), 0u);
    view->setSelectionFilter(cc::core::SelectionFilter::only(cc::core::ElementKind::Node));
    EXPECT_TRUE(view->selection().empty());  // the bar no longer passes the filter
}

TEST_F(SelectionTest, RightButtonCancelsAWindowThenActs) {
    QSignalSpy act(view.get(), &Viewport3D::actOnSelection);
    click(*view, screenOf(*view, -1, 4));
    click(*view, screenOf(*view, 0, 0), Qt::RightButton);
    EXPECT_FALSE(view->windowPending());
    EXPECT_EQ(act.count(), 0);
    click(*view, screenOf(*view, 0, 0), Qt::RightButton);
    EXPECT_EQ(act.count(), 1);
}

TEST_F(SelectionTest, EscapeCancelsTheWindowThenClearsTheSelection) {
    click(*view, screenOf(*view, 0, 0));
    click(*view, screenOf(*view, 2, 1.5));
    ASSERT_TRUE(view->windowPending());
    QKeyEvent esc(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
    QApplication::sendEvent(view.get(), &esc);
    EXPECT_FALSE(view->windowPending());
    EXPECT_EQ(view->selection().size(), 1u);
    QApplication::sendEvent(view.get(), &esc);
    EXPECT_TRUE(view->selection().empty());
}

TEST_F(SelectionTest, MiddleButtonStillPansAndNeverSelects) {
    const QPointF c0 = screenOf(*view, 0, 0);
    drag(*view, Qt::MiddleButton, c0, c0 + QPointF(30, 0));
    EXPECT_TRUE(view->selection().empty());
    EXPECT_FALSE(view->windowPending());
    EXPECT_NE(view->camera().target.x, 2.0);
}

TEST_F(SelectionTest, LeavingSelectionModeClearsTheSelection) {
    click(*view, screenOf(*view, 0, 0));
    view->setSelectionMode(false);
    EXPECT_TRUE(view->selection().empty());
}

int main(int argc, char** argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
