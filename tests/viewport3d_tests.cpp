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

TEST_F(SelectionTest, HiddenCategoriesCannotBePickedNorStayHovered) {
    click(*view, screenOf(*view, 0, 0));  // node 1
    ASSERT_TRUE(view->selection().contains({cc::core::ElementKind::Node, 1}));
    cc::core::CategorySettings settings;
    settings.visible[static_cast<int>(cc::core::Category::Nodes)] = false;
    view->setCategorySettings(settings);
    EXPECT_TRUE(view->selection().empty());  // already selected nodes are dropped
    mouse(*view, QEvent::MouseMove, screenOf(*view, 4, 3), Qt::NoButton, Qt::NoButton);
    EXPECT_FALSE(view->hovered() && view->hovered()->kind == cc::core::ElementKind::Node);
    click(*view, screenOf(*view, 0, 0));
    EXPECT_EQ(view->selection().count(cc::core::ElementKind::Node), 0u);
    // Hiding the bars hides the nodes too: nothing can be picked on a bar.
    settings.visible[static_cast<int>(cc::core::Category::Nodes)] = true;
    settings.visible[static_cast<int>(cc::core::Category::Bars)] = false;
    view->setCategorySettings(settings);
    EXPECT_FALSE(view->effectiveFilter().nodes);
    EXPECT_FALSE(view->effectiveFilter().bars);
    EXPECT_TRUE(view->effectiveFilter().shells);
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

TEST_F(SelectionTest, SnapModeSnapsAndEntersPoints) {
    view->setSelectionMode(false);
    view->setSnapMode(true);
    QSignalSpy changed(view.get(), &Viewport3D::snapChanged);
    QSignalSpy entered(view.get(), &Viewport3D::pointEntered);
    QPointF near = screenOf(*view, 4, 3) + QPointF(6, -5);
    mouse(*view, QEvent::MouseMove, near, Qt::NoButton, Qt::NoButton);
    EXPECT_EQ(view->currentSnap().kind, cc::core::SnapKind::Endpoint);
    EXPECT_GE(changed.count(), 1);
    EXPECT_EQ(view->cursorWorld(), (cc::core::Vec3{4, 0, 3}));
    click(*view, near);
    ASSERT_EQ(entered.count(), 1);
    EXPECT_EQ(entered.at(0).at(0).value<cc::core::Vec3>(), (cc::core::Vec3{4, 0, 3}));
    // Far from everything: free position on the target plane.
    mouse(*view, QEvent::MouseMove, screenOf(*view, 2, 1.5), Qt::NoButton, Qt::NoButton);
    EXPECT_FALSE(view->currentSnap().snapped());
    view->setSnapMode(false);
    click(*view, near);
    EXPECT_EQ(entered.count(), 1);
}

TEST(ViewportTooltips, CoordinatesUseThreeDecimalsAndTheUnit) {
    EXPECT_EQ(Viewport3D::coordinatesText({187.5964, -186.2149, 153.7561}), QStringLiteral("187.596, -186.215, 153.756 m"));
    EXPECT_EQ(Viewport3D::coordinatesText({-0.0001, 0, 3}), QStringLiteral("0.000, 0.000, 3.000 m"));
}

TEST(ViewportTooltips, AnchoredBoxesSitInTheCornerOrAboveTheElement) {
    const QSizeF size(100, 50), area(800, 600);
    using cc::app::TooltipPosition;
    EXPECT_EQ(Viewport3D::anchoredRect(size, area, TooltipPosition::TopRightCorner), QRectF(700, 0, 100, 50));
    // Callout: bottom-left corner 32 px right of and 2 px above the element (measured in the original).
    EXPECT_EQ(Viewport3D::anchoredRect(size, area, TooltipPosition::OnElement, {400, 300}), QRectF(432, 252, 100, 50));
    // Near the right edge it flips to the left; near the top it goes below; always inside the area.
    EXPECT_EQ(Viewport3D::anchoredRect(size, area, TooltipPosition::OnElement, {790, 300}), QRectF(658, 252, 100, 50));
    const QRectF top = Viewport3D::anchoredRect(size, area, TooltipPosition::OnElement, {400, 10});
    EXPECT_GE(top.top(), 0.0);
    EXPECT_TRUE(QRectF(QPointF(0, 0), area).contains(Viewport3D::anchoredRect(size, area, TooltipPosition::OnElement, {795, 590})));
}

TEST(ViewportTooltips, CoordinateTooltipFollowsTheCursorAndStaysInside) {
    const QSizeF size(150, 28), area(800, 600);
    EXPECT_EQ(Viewport3D::coordinateTooltipOrigin({100, 100}, size, area), QPointF(140, 143));
    const QPointF flipped = Viewport3D::coordinateTooltipOrigin({780, 590}, size, area);  // would overflow right and bottom
    EXPECT_LT(flipped.x(), 780);
    EXPECT_LT(flipped.y(), 590);
    EXPECT_GE(flipped.x(), 0);
}

TEST_F(SelectionTest, BarInfoLinesDescribeTheBar) {
    const int mat = model.addMaterial({0, "S275 (UNE-EN 10025-2)", cc::core::MaterialKind::Steel});
    cc::core::CrossSection ipe;
    ipe.name = "IPE 80";
    ipe.areaM2 = 7.64e-4;
    const int sec = model.addSection(ipe);
    const int bar = model.addBar(model.nodes[0].id, model.nodes[1].id, mat, sec);
    const QStringList lines = Viewport3D::barInfoLines(model, bar);
    ASSERT_EQ(lines.size(), 3);
    EXPECT_EQ(lines[0], QStringLiteral("Barra N1/N2: L. 4 m"));
    EXPECT_EQ(lines[1], QStringLiteral("Capa : por defecto"));
    EXPECT_EQ(lines[2], QStringLiteral("IPE 80 (S275 (UNE-EN 10025-2))"));
    EXPECT_EQ(Viewport3D::barInfoLines(model, model.bars[0].id).size(), 2);  // unassigned section: no third row
    EXPECT_TRUE(Viewport3D::barInfoLines(model, 999).isEmpty());
}

TEST_F(SelectionTest, TooltipAndInfoBoxFollowTheSnap) {
    view->setSelectionMode(false);
    view->setSnapMode(true);
    EXPECT_FALSE(view->coordinateTooltipVisible());  // the cursor has not entered yet
    mouse(*view, QEvent::MouseMove, screenOf(*view, 2, 1.5), Qt::NoButton, Qt::NoButton);
    EXPECT_TRUE(view->coordinateTooltipVisible());
    EXPECT_FALSE(view->infoBarId());  // free point: no bar
    const int topBar = model.bars[2].id;  // N3-N4, along the top at Z = 3
    mouse(*view, QEvent::MouseMove, screenOf(*view, 2, 3) + QPointF(0, 4), Qt::NoButton, Qt::NoButton);
    EXPECT_EQ(view->infoBarId(), topBar);
    mouse(*view, QEvent::MouseMove, screenOf(*view, 0, 0) + QPointF(3, 2), Qt::NoButton, Qt::NoButton);
    EXPECT_EQ(view->currentSnap().kind, cc::core::SnapKind::Endpoint);
    EXPECT_EQ(view->infoBarId(), model.bars[0].id);  // the bar that meets at N1
    view->setShowElementInfo(false);
    EXPECT_FALSE(view->infoBarId());
    view->setShowElementInfo(true);
    view->setSnapMode(false);
    EXPECT_TRUE(view->coordinateTooltipVisible());  // free coordinates without a command, as in the original
    EXPECT_FALSE(view->infoBarId());
}

TEST_F(SelectionTest, HoveredBarFeedsTheInfoBoxInSelectionMode) {
    mouse(*view, QEvent::MouseMove, screenOf(*view, 2, 3) + QPointF(0, 3), Qt::NoButton, Qt::NoButton);
    ASSERT_TRUE(view->hovered());
    EXPECT_EQ(view->infoBarId(), model.bars[2].id);
}

TEST_F(SelectionTest, NodeSquareFollowsTheNodeSizeOption) {
    EXPECT_NEAR(view->nodeSquarePixels(), 13.5, 1e-9);  // 15 cm: ~14 px as in the original
    view->setNodeSizeCm(40.0);
    EXPECT_NEAR(view->nodeSquarePixels(), 36.0, 1e-9);  // original measured ~35 px
    view->setNodeSizeCm(-3.0);  // ignored
    EXPECT_EQ(view->nodeSizeCm(), 40.0);
}

TEST_F(SelectionTest, TooltipPositionIsConfigurable) {
    EXPECT_EQ(view->tooltipPosition(), cc::app::TooltipPosition::TopRightCorner);
    view->setTooltipPosition(cc::app::TooltipPosition::OnElement);
    EXPECT_EQ(view->tooltipPosition(), cc::app::TooltipPosition::OnElement);
    EXPECT_EQ(Viewport3D::tooltipPositionLabel(cc::app::TooltipPosition::TopRightCorner), QStringLiteral("En la esquina superior derecha"));
    EXPECT_EQ(Viewport3D::tooltipPositionLabel(cc::app::TooltipPosition::OnElement), QStringLiteral("Sobre el elemento"));
}

int main(int argc, char** argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

TEST(Viewport3DLayers, BarInfoShowsTheLayerNames) {
    cc::core::StructuralModel model;
    const int a = model.addNode({0, 0, 0}), b = model.addNode({4, 0, 0});
    const int bar = model.addBar(a, b);
    EXPECT_EQ(Viewport3D::barInfoLines(model, bar)[1], QStringLiteral("Capa : por defecto"));
    const int other = model.addLayer("Nueva capa 2");
    ASSERT_TRUE(model.assignLayers({bar}, {}, {other}, cc::core::LayerOperation::Assign));
    EXPECT_EQ(Viewport3D::barInfoLines(model, bar)[1], QStringLiteral("Capa : por defecto y Nueva capa 2"));
}

// --- View history, framing and modes (T-P4-09) ---------------------------------------------------------------------------
TEST_F(ViewportTest, PresetCommitsHistoryAndPreviousViewRestores) {
    const auto before = view->camera().view();
    EXPECT_FALSE(view->canGoPrevious());
    ASSERT_TRUE(view->applyProjectionPreset(1));  // plan view
    EXPECT_EQ(view->camera().pitchDeg, 90.0);
    EXPECT_TRUE(view->canGoPrevious());
    ASSERT_TRUE(view->previousView());
    EXPECT_EQ(view->camera().view(), before);
    EXPECT_FALSE(view->canGoPrevious());
    EXPECT_FALSE(view->previousView());
}

TEST_F(ViewportTest, UnavailablePresetLeavesHistoryAlone) {
    EXPECT_FALSE(view->applyProjectionPreset(6));
    EXPECT_FALSE(view->canGoPrevious());
}

TEST_F(ViewportTest, WheelPanAndOrbitGesturesAreOneHistoryEntryEach) {
    wheel(*view, {400, 300}, 120);
    EXPECT_EQ(view->historySize(), 1);
    drag(*view, Qt::MiddleButton, {100, 100}, {160, 130});
    EXPECT_EQ(view->historySize(), 2);
    view->setLeftDragAction(DragAction::Orbit);
    drag(*view, Qt::LeftButton, {100, 100}, {140, 100});
    EXPECT_EQ(view->historySize(), 3);
    while (view->previousView()) {}
    EXPECT_EQ(view->camera().view(), cc::core::Camera{}.view());
}

TEST_F(ViewportTest, ClickWithoutMovementDoesNotPushHistory) {
    drag(*view, Qt::MiddleButton, {100, 100}, {100, 100});
    EXPECT_EQ(view->historySize(), 0);
}

TEST_F(ViewportTest, HistoryIsBounded) {
    for (int i = 0; i < Viewport3D::kMaxViewHistory + 20; ++i) wheel(*view, {400, 300}, i % 2 ? -120 : 120);
    EXPECT_EQ(view->historySize(), Viewport3D::kMaxViewHistory);
}

TEST_F(ViewportTest, FrameAllCentresOnTheNodes) {
    view->setNodes({{0, 0, 0}, {10, 4, 6}});
    view->frameAll();
    const auto& c = view->camera();
    EXPECT_NEAR(c.target.x, 5.0, 1e-9);
    EXPECT_NEAR(c.target.y, 2.0, 1e-9);
    EXPECT_NEAR(c.target.z, 3.0, 1e-9);
    EXPECT_GT(c.distance, 0.0);
    for (const auto& n : view->nodes()) {  // every node is inside the viewport
        QPointF s;
        ASSERT_TRUE(view->worldToScreen(n, s));
        EXPECT_GE(s.x(), 0.0);
        EXPECT_LE(s.x(), view->width());
        EXPECT_GE(s.y(), 0.0);
        EXPECT_LE(s.y(), view->height());
    }
    EXPECT_TRUE(view->canGoPrevious());
}

TEST_F(ViewportTest, FrameAllWithoutNodesRestoresTheDefaultView) {
    wheel(*view, {400, 300}, 240);
    view->frameAll();
    EXPECT_EQ(view->camera().view(), cc::core::Camera{}.view());
}

TEST_F(ViewportTest, DoubleWindowIsTwiceAsFarAsTheFullWindow) {
    view->setNodes({{0, 0, 0}, {10, 4, 6}});
    view->frameAll();
    const double full = view->camera().distance;
    const auto target = view->camera().target;
    view->doubleWindow();
    EXPECT_NEAR(view->camera().distance, 2.0 * full, 1e-9);
    EXPECT_EQ(view->camera().target, target);
}

TEST_F(ViewportTest, MiddleDoubleClickFramesAll) {
    view->setNodes({{0, 0, 0}, {10, 4, 6}});
    mouse(*view, QEvent::MouseButtonDblClick, {200, 200}, Qt::MiddleButton, Qt::MiddleButton);
    EXPECT_NEAR(view->camera().target.x, 5.0, 1e-9);
}

TEST_F(ViewportTest, ZoomMarkClickZoomsInCentredOnThePoint) {
    view->setZoomMarkMode(true);
    const double distance = view->camera().distance;
    const QPointF click(300, 200);
    const auto world = view->screenToTargetPlane(click);
    mouse(*view, QEvent::MouseButtonPress, click, Qt::LeftButton, Qt::LeftButton);
    mouse(*view, QEvent::MouseButtonRelease, click, Qt::LeftButton, Qt::NoButton);
    EXPECT_NEAR(view->camera().distance, distance / Viewport3D::kZoomMarkFactor, 1e-9);
    EXPECT_NEAR(view->camera().target.x, world.x, 1e-9);
    EXPECT_NEAR(view->camera().target.y, world.y, 1e-9);
    EXPECT_NEAR(view->camera().target.z, world.z, 1e-9);
    EXPECT_EQ(view->historySize(), 1);
}

TEST_F(ViewportTest, ZoomMarkOffDoesNotZoomOnClick) {
    const double distance = view->camera().distance;
    mouse(*view, QEvent::MouseButtonPress, {300, 200}, Qt::LeftButton, Qt::LeftButton);
    mouse(*view, QEvent::MouseButtonRelease, {300, 200}, Qt::LeftButton, Qt::NoButton);
    EXPECT_EQ(view->camera().distance, distance);
}

TEST_F(ViewportTest, PanToolDragMovesTheViewWithTheLeftButton) {
    view->setLeftDragAction(DragAction::Pan);
    const auto target = view->camera().target;
    drag(*view, Qt::LeftButton, {100, 100}, {160, 130});
    EXPECT_FALSE(view->camera().target == target);
}

TEST_F(ViewportTest, RotationModesEmitSignalsOnlyOnChange) {
    QSignalSpy point(view.get(), &Viewport3D::rotateAroundPointChanged);
    QSignalSpy camera(view.get(), &Viewport3D::rotateAroundCameraChanged);
    view->setRotateAroundPoint(true);
    view->setRotateAroundPoint(true);
    view->setRotateAroundCamera(true);
    EXPECT_EQ(point.count(), 1);
    EXPECT_EQ(camera.count(), 1);
    EXPECT_TRUE(view->rotateAroundPoint());
    EXPECT_TRUE(view->rotateAroundCamera());
}
