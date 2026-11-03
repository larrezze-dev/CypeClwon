// Geometria > Nudos (T-P5-02, GUI part): the NodeTool commands, the "Calculadora" and the viewport behaviour they rely on.
// The widgets are never shown on screen (offscreen platform).
#include <gtest/gtest.h>

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QEnterEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPushButton>
#include <QRegularExpression>
#include <QSignalSpy>
#include <memory>

#include "NodeCalculatorDialog.h"
#include "NodeTool.h"
#include "Viewport3D.h"
#include "core/node_edit.h"

using cc::app::NodeCalculatorDialog;
using cc::app::NodeTool;
using cc::app::Viewport3D;
using Mode = cc::app::NodeTool::Mode;

namespace {
QApplication* app() {
    static int argc = 1;
    static char name[] = "node_tool_tests";
    static char* argv[] = {name, nullptr};
    static QApplication instance(argc, argv);
    return &instance;
}

void mouse(Viewport3D& v, QEvent::Type type, QPointF pos, Qt::MouseButton button, Qt::MouseButtons buttons) {
    QMouseEvent e(type, pos, v.mapToGlobal(pos), button, buttons, Qt::NoModifier);
    QApplication::sendEvent(&v, &e);
}
void click(Viewport3D& v, QPointF pos, Qt::MouseButton button = Qt::LeftButton) {
    mouse(v, QEvent::MouseMove, pos, Qt::NoButton, Qt::NoButton);  // the original snaps while the pointer travels
    mouse(v, QEvent::MouseButtonPress, pos, button, button);
    mouse(v, QEvent::MouseButtonRelease, pos, button, Qt::NoButton);
}
void hover(Viewport3D& v, QPointF pos) { mouse(v, QEvent::MouseMove, pos, Qt::NoButton, Qt::NoButton); }

void key(QWidget& w, int k, const QString& text = {}) {
    QKeyEvent press(QEvent::KeyPress, k, Qt::NoModifier, text);
    QApplication::sendEvent(&w, &press);
    QKeyEvent release(QEvent::KeyRelease, k, Qt::NoModifier, text);
    QApplication::sendEvent(&w, &release);
}
void digit(QWidget& w, char c) { key(w, Qt::Key_0 + (c - '0'), QString(QLatin1Char(c))); }

class NodeToolTest : public ::testing::Test {
protected:
    void SetUp() override {
        app();
        auto& model = project.structure;
        const int n1 = model.addNode({0, 0, 0}), n2 = model.addNode({4, 0, 0});
        const int n3 = model.addNode({0, 0, 3}), n4 = model.addNode({4, 0, 3});
        model.addBar(n1, n3);
        model.addBar(n2, n4);
        model.addBar(n3, n4);
        view = std::make_unique<Viewport3D>();
        view->resize(800, 600);
        view->setStandardView(cc::core::StandardView::Front);
        view->setOrthographic(true);
        view->camera().target = {2, 0, 1.5};
        view->setStructure(&model);
        tool = std::make_unique<NodeTool>();
        tool->setProject(&project, &undo);
        tool->addViewport(view.get());
        undo.setChangeCallback([this] { tool->structureChanged(); });
        QObject::connect(view.get(), &Viewport3D::statusHint, [this](const QString& text) { status = text; });  // as MainWindow does
        QObject::connect(tool.get(), &NodeTool::statusChanged, [this](const QString& text) { status = text; });
        undo.setClean();
    }
    QPointF screenOf(double x, double y, double z) {
        QPointF p;
        EXPECT_TRUE(view->worldToScreen({x, y, z}, p));
        return p;
    }
    std::size_t nodeCount() const { return project.structure.nodes.size(); }
    /// Lets the deleteLater() of a closed dialog run.
    static void flush() { QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete); }

    cc::core::Project project;
    cc::core::UndoStack undo;
    std::unique_ptr<Viewport3D> view;
    std::unique_ptr<NodeTool> tool;
    QString status;
};
}  // namespace

TEST_F(NodeToolTest, ThreeButtonsInAnExclusiveOptionalGroup) {
    EXPECT_EQ(tool->newAction()->text(), QStringLiteral("Nuevo"));
    EXPECT_EQ(tool->deleteAction()->text(), QStringLiteral("Borrar"));
    EXPECT_EQ(tool->moveAction()->text(), QStringLiteral("Mover"));
    EXPECT_TRUE(tool->newAction()->isCheckable());
    EXPECT_EQ(tool->group()->exclusionPolicy(), QActionGroup::ExclusionPolicy::ExclusiveOptional);
    tool->newAction()->trigger();
    EXPECT_TRUE(tool->newAction()->isChecked());
    tool->newAction()->trigger();  // a second click on the pressed button releases it
    EXPECT_FALSE(tool->newAction()->isChecked());
    EXPECT_EQ(tool->mode(), Mode::None);
}

TEST_F(NodeToolTest, OnAnEmptyProjectOnlyNuevoIsEnabledAndTheOthersFollowTheHistory) {
    cc::core::Project empty;
    cc::core::UndoStack history;
    NodeTool other;
    other.setProject(&empty, &history);
    history.setChangeCallback([&] { other.structureChanged(); });
    EXPECT_TRUE(other.newAction()->isEnabled());
    EXPECT_FALSE(other.deleteAction()->isEnabled());
    EXPECT_FALSE(other.moveAction()->isEnabled());
    ASSERT_NE(cc::core::addNode(empty, history, {1, 2, 3}), 0);
    EXPECT_TRUE(other.deleteAction()->isEnabled());
    EXPECT_TRUE(other.moveAction()->isEnabled());
    history.undo();
    EXPECT_FALSE(other.deleteAction()->isEnabled());
    EXPECT_FALSE(other.moveAction()->isEnabled());
    history.redo();
    EXPECT_TRUE(other.deleteAction()->isEnabled());
    other.setProject(nullptr, &history);
    EXPECT_FALSE(other.newAction()->isEnabled());
}

TEST_F(NodeToolTest, LastNodeUndoneEndsAnActiveBorrar) {
    cc::core::Project empty;
    cc::core::UndoStack history;
    NodeTool other;
    other.setProject(&empty, &history);
    history.setChangeCallback([&] { other.structureChanged(); });
    ASSERT_NE(cc::core::addNode(empty, history, {0, 0, 0}), 0);
    other.deleteAction()->trigger();
    EXPECT_EQ(other.mode(), Mode::Delete);
    history.undo();
    EXPECT_EQ(other.mode(), Mode::None);
    EXPECT_FALSE(other.deleteAction()->isChecked());
}

TEST_F(NodeToolTest, ACommandStaysPressedUntilEscOrAnotherButton) {
    QSignalSpy activating(tool.get(), &NodeTool::aboutToActivate);
    tool->newAction()->trigger();
    EXPECT_EQ(activating.count(), 1);
    EXPECT_EQ(tool->mode(), Mode::New);
    EXPECT_TRUE(view->snapMode());
    EXPECT_TRUE(view->nodeLabelsVisible());
    EXPECT_NE(view->focusPolicy(), Qt::NoFocus);  // Esc and the digits need the keyboard focus
    tool->deleteAction()->trigger();
    EXPECT_EQ(tool->mode(), Mode::Delete);
    EXPECT_FALSE(tool->newAction()->isChecked());
    EXPECT_TRUE(tool->deleteAction()->isChecked());
    EXPECT_FALSE(view->snapMode());
    EXPECT_TRUE(view->selectionMode());
    EXPECT_EQ(view->selectionFilter().bars, false);
    EXPECT_FALSE(view->nodeLabelsVisible());  // not shown by Borrar (c3_borrar_hover.png)
    key(*view, Qt::Key_Escape);
    EXPECT_EQ(tool->mode(), Mode::None);
    EXPECT_FALSE(tool->deleteAction()->isChecked());
    EXPECT_FALSE(view->selectionMode());
    EXPECT_FALSE(view->snapMode());
    EXPECT_EQ(status, QStringLiteral("Seleccione una opción del menú."));  // restored, not an empty bar
}

TEST_F(NodeToolTest, StatusBarTextsHaveTheSeparatorsOfTheOriginal) {
    tool->newAction()->trigger();
    EXPECT_EQ(status, QStringLiteral("Nudos - Nuevo |  - LMB: Introduzca un punto."));
    tool->deleteAction()->trigger();
    EXPECT_EQ(status, QStringLiteral("Nudos - Borrar |  - LMB: Seleccione un grupo de elementos. - RMB: Actuar sobre los elementos seleccionados."));
    tool->moveAction()->trigger();
    EXPECT_EQ(status, QStringLiteral("Nudos - Mover |  - LMB: Seleccione un elemento."));
    click(*view, screenOf(4, 0, 3));
    EXPECT_EQ(status, QStringLiteral("Nudos - Mover |  - LMB: Seleccione la nueva posición del elemento. - RMB: Volver al paso anterior."));
}

TEST_F(NodeToolTest, MoverShowsNoNodeLabelsAndBorrarNoCoordinateTooltip) {
    tool->moveAction()->trigger();
    EXPECT_FALSE(view->nodeLabelsVisible());
    click(*view, screenOf(4, 0, 3));
    ASSERT_TRUE(tool->movePending());
    EXPECT_FALSE(view->nodeLabelsVisible()) << "not even in the second step";
    tool->deleteAction()->trigger();
    hover(*view, screenOf(2, 0, 3));
    EXPECT_FALSE(view->coordinateTooltipVisible());
    tool->newAction()->trigger();
    QEnterEvent enter(QPointF(10, 10), QPointF(10, 10), QPointF(10, 10));
    QApplication::sendEvent(view.get(), &enter);
    EXPECT_TRUE(view->coordinateTooltipVisible());
    key(*view, Qt::Key_Escape);
    EXPECT_FALSE(view->coordinateTooltipVisible()) << "Esc hides the tooltip";
}

TEST_F(NodeToolTest, NuevoEntersANodeOnABarAndIgnoresClicksWithoutSnap) {
    tool->newAction()->trigger();
    click(*view, QPointF(30, 30));  // far from every bar and reference line
    ASSERT_FALSE(view->currentSnap().snapped());
    EXPECT_EQ(nodeCount(), 4u);
    EXPECT_TRUE(undo.isClean());
    click(*view, screenOf(1.3, 0, 3));  // on the bar N3-N4
    ASSERT_EQ(nodeCount(), 5u);
    EXPECT_NEAR(project.structure.nodes.back().position.x, 1.3, 0.05);
    EXPECT_NEAR(project.structure.nodes.back().position.z, 3.0, 0.05);
    EXPECT_EQ(tool->mode(), Mode::New) << "the command stays active";
    EXPECT_FALSE(undo.isClean());
    // The same point again: the new node is now a snap target, no message, no change, no "*" in the title.
    undo.setClean();
    click(*view, screenOf(1.3, 0, 3));
    EXPECT_EQ(nodeCount(), 5u);
    EXPECT_TRUE(undo.isClean());
}

TEST_F(NodeToolTest, NuevoShowsTheCoordinateTooltipWithThreeDecimals) {
    tool->newAction()->trigger();
    hover(*view, screenOf(1.3, 0, 3));
    EXPECT_TRUE(view->coordinateTooltipVisible());
    const QString text = Viewport3D::coordinatesText(view->cursorWorld());
    EXPECT_TRUE(QRegularExpression(QStringLiteral(R"(^-?\d+\.\d{3}, -?\d+\.\d{3}, -?\d+\.\d{3} m$)")).match(text).hasMatch())
        << text.toStdString();
}

TEST_F(NodeToolTest, TheViewportFollowsEveryPushUndoAndRedo) {
    tool->newAction()->trigger();
    EXPECT_EQ(view->nodes().size(), 4u);
    click(*view, screenOf(1.3, 0, 3));
    EXPECT_EQ(view->nodes().size(), 5u);  // the reference lines through the new point are drawn from this list
    undo.undo();
    EXPECT_EQ(view->nodes().size(), 4u);
    undo.redo();
    EXPECT_EQ(view->nodes().size(), 5u);
}

TEST_F(NodeToolTest, ADigitOpensTheCalculatorAndIntroWalksXYZ) {
    tool->newAction()->trigger();
    EXPECT_EQ(tool->calculator(), nullptr);
    digit(*view, '5');
    NodeCalculatorDialog* calc = tool->calculator();
    ASSERT_NE(calc, nullptr);
    EXPECT_EQ(calc->fieldText(0), QStringLiteral("5"));  // the digit that opened it goes to X
    digit(*calc, '6');
    digit(*calc, '7');
    EXPECT_EQ(calc->fieldText(0), QStringLiteral("567"));
    key(*calc, Qt::Key_Tab);  // Tab does NOT change the field
    EXPECT_EQ(calc->activeField(), 0);
    key(*calc, Qt::Key_Return);
    EXPECT_EQ(calc->activeField(), 1);
    digit(*calc, '8');
    key(*calc, Qt::Key_Return);
    EXPECT_EQ(calc->activeField(), 2);
    EXPECT_EQ(nodeCount(), 4u) << "the node appears with the Intro of Z";
    digit(*calc, '9');
    key(*calc, Qt::Key_Return);
    ASSERT_EQ(nodeCount(), 5u);
    const auto& made = project.structure.nodes.back().position;
    EXPECT_DOUBLE_EQ(made.x, 567);
    EXPECT_DOUBLE_EQ(made.y, 8);
    EXPECT_DOUBLE_EQ(made.z, 9);
    flush();
    EXPECT_EQ(tool->calculator(), nullptr) << "the Intro of Z closes the window";
    EXPECT_EQ(tool->mode(), Mode::New);
    // The command is still active: another digit opens it again; the same point changes nothing.
    undo.setClean();
    digit(*view, '5');
    calc = tool->calculator();
    ASSERT_NE(calc, nullptr);
    digit(*calc, '6');
    digit(*calc, '7');
    key(*calc, Qt::Key_Return);
    digit(*calc, '8');
    key(*calc, Qt::Key_Return);
    digit(*calc, '9');
    key(*calc, Qt::Key_Return);
    EXPECT_EQ(nodeCount(), 5u);
    EXPECT_TRUE(undo.isClean());
}

TEST_F(NodeToolTest, CalculatorKeysDecimalPointBackspaceAndEscape) {
    tool->newAction()->trigger();
    digit(*view, '1');
    NodeCalculatorDialog* calc = tool->calculator();
    ASSERT_NE(calc, nullptr);
    calc->button(QStringLiteral("."))->click();
    calc->button(QStringLiteral("2"))->click();
    calc->button(QStringLiteral("5"))->click();
    EXPECT_EQ(calc->fieldText(0), QStringLiteral("1.25"));
    calc->button(QStringLiteral("."))->click();  // a second point is ignored
    EXPECT_EQ(calc->fieldText(0), QStringLiteral("1.25"));
    calc->backspaceButton()->click();
    EXPECT_EQ(calc->fieldText(0), QStringLiteral("1.2"));
    key(*calc, Qt::Key_Backspace);
    key(*calc, Qt::Key_Backspace);
    key(*calc, Qt::Key_Backspace);
    EXPECT_EQ(calc->fieldText(0), QStringLiteral("0"));
    digit(*calc, '3');
    calc->button(QStringLiteral("C"))->click();  // not measured: clears the active field
    EXPECT_EQ(calc->fieldText(0), QStringLiteral("0"));
    calc->button(QStringLiteral("+"))->click();  // not measured: nothing happens
    calc->button(QStringLiteral("MR"))->click();
    EXPECT_EQ(calc->fieldText(0), QStringLiteral("0"));
    EXPECT_NE(calc->button(QStringLiteral("+/-")), nullptr);
    EXPECT_NE(calc->button(QStringLiteral("???")), nullptr);
    EXPECT_NE(calc->button(QStringLiteral("M-")), nullptr);
    key(*calc, Qt::Key_Escape);
    flush();
    EXPECT_EQ(tool->calculator(), nullptr);
    EXPECT_EQ(nodeCount(), 4u) << "Esc closes the calculator without a node";
    EXPECT_EQ(tool->mode(), Mode::New) << "and the command goes on";
}

TEST_F(NodeToolTest, BorrarMarksWithLmbAndDeletesWithRmbAtOnce) {
    tool->deleteAction()->trigger();
    click(*view, screenOf(4, 0, 3));  // N4
    ASSERT_EQ(view->selection().size(), 1u);
    EXPECT_EQ(view->selection().ids(cc::core::ElementKind::Node).front(), 4);
    click(*view, screenOf(2, 0, 3), Qt::RightButton);  // RMB anywhere: act on the marked nodes
    EXPECT_EQ(nodeCount(), 3u);
    EXPECT_EQ(project.structure.bars.size(), 1u) << "the bars that reached N4 go with it";
    EXPECT_TRUE(view->selection().empty());
    EXPECT_EQ(view->nodes().size(), 3u);
    EXPECT_EQ(tool->mode(), Mode::Delete) << "the command stays active";
    undo.undo();
    EXPECT_EQ(nodeCount(), 4u);
    EXPECT_EQ(project.structure.bars.size(), 3u);
    EXPECT_EQ(view->nodes().size(), 4u);
}

TEST_F(NodeToolTest, BorrarFiltersNodesOnlyAndRmbWithoutSelectionDoesNothing) {
    tool->deleteAction()->trigger();
    click(*view, screenOf(2, 0, 3));  // the middle of a bar: not selectable in Borrar
    EXPECT_TRUE(view->selection().empty());
    click(*view, screenOf(2, 0, 3), Qt::RightButton);
    EXPECT_EQ(nodeCount(), 4u);
    EXPECT_TRUE(undo.isClean());
}

TEST_F(NodeToolTest, MoverNeedsANodeThenFollowsThePointerAndSetsTheNewPosition) {
    tool->moveAction()->trigger();
    EXPECT_FALSE(tool->movePending());
    EXPECT_TRUE(view->selectionMode());
    click(*view, screenOf(4, 0, 3));  // N4
    ASSERT_TRUE(tool->movePending());
    EXPECT_EQ(*tool->movingNode(), 4);
    EXPECT_EQ(*view->movingNode(), 4);
    EXPECT_FALSE(view->selectionMode());
    EXPECT_TRUE(view->snapMode());
    // Preview: the moving node and its bars are not snap targets, so it cannot snap to itself.
    hover(*view, screenOf(4, 0, 3));
    const auto& snap = view->currentSnap();
    if (snap.element && snap.element->kind == cc::core::ElementKind::Node) EXPECT_NE(snap.element->id, 4);
    if (snap.element && snap.element->kind == cc::core::ElementKind::Bar) {
        const cc::core::Bar* bar = project.structure.findBar(snap.element->id);
        ASSERT_NE(bar, nullptr);
        EXPECT_NE(bar->startNode, 4);
        EXPECT_NE(bar->endNode, 4);
    }
    hover(*view, screenOf(6, 0, 3));  // on the X reference line of N3
    ASSERT_TRUE(view->currentSnap().snapped());
    EXPECT_NEAR(view->cursorWorld().x, 6.0, 0.05);
    EXPECT_EQ(nodeCount(), 4u);
    EXPECT_TRUE(undo.isClean()) << "nothing is pushed while the node only follows the pointer";
    click(*view, screenOf(6, 0, 3));
    EXPECT_FALSE(tool->movePending());
    EXPECT_EQ(tool->mode(), Mode::Move);
    EXPECT_NEAR(project.structure.findNode(4)->position.x, 6.0, 0.05);
    EXPECT_TRUE(view->selectionMode()) << "back to the first step for another node";
    undo.undo();
    EXPECT_NEAR(project.structure.findNode(4)->position.x, 4.0, 1e-9);
}

TEST_F(NodeToolTest, MoverIgnoresClicksWithoutSnapAndRmbGoesBackToTheFirstStep) {
    tool->moveAction()->trigger();
    click(*view, screenOf(4, 0, 3));
    ASSERT_TRUE(tool->movePending());
    click(*view, QPointF(30, 30));  // no snap: ignored, the move goes on
    ASSERT_FALSE(view->currentSnap().snapped());
    EXPECT_TRUE(tool->movePending());
    EXPECT_TRUE(undo.isClean());
    click(*view, QPointF(30, 30), Qt::RightButton);  // RMB: "Volver al paso anterior"
    EXPECT_FALSE(tool->movePending());
    EXPECT_FALSE(view->movingNode().has_value());
    EXPECT_TRUE(undo.isClean());
    EXPECT_EQ(tool->mode(), Mode::Move) << "RMB does not leave the command";
    EXPECT_EQ(status, QStringLiteral("Nudos - Mover |  - LMB: Seleccione un elemento."));
    click(*view, QPointF(30, 30), Qt::RightButton);  // a second RMB in the first step: still in the command
    EXPECT_EQ(tool->mode(), Mode::Move);
    click(*view, screenOf(4, 0, 3));  // and another node can be chosen
    EXPECT_TRUE(tool->movePending());
}

TEST_F(NodeToolTest, MoverOntoAnotherNodeMergesThemWithoutMessage) {
    tool->moveAction()->trigger();
    click(*view, screenOf(0, 0, 3));  // N3 (0,0,3), bars N1-N3 and N3-N4
    ASSERT_TRUE(tool->movePending());
    click(*view, screenOf(4, 0, 3));  // onto N4
    EXPECT_EQ(nodeCount(), 3u);
    EXPECT_EQ(project.structure.findNode(3), nullptr);
    EXPECT_EQ(project.structure.bars.size(), 2u) << "the bar of length zero disappears";
    EXPECT_FALSE(tool->movePending());
    EXPECT_EQ(tool->mode(), Mode::Move) << "after fixing, the command is in its first step";
    EXPECT_EQ(status, QStringLiteral("Nudos - Mover |  - LMB: Seleccione un elemento."));
    undo.undo();
    EXPECT_EQ(nodeCount(), 4u);
    EXPECT_EQ(project.structure.bars.size(), 3u);
}

TEST_F(NodeToolTest, MoverAcceptsTheCalculatorInTheSecondStep) {
    tool->moveAction()->trigger();
    click(*view, screenOf(4, 0, 3));
    ASSERT_TRUE(tool->movePending());
    digit(*view, '7');
    NodeCalculatorDialog* calc = tool->calculator();
    ASSERT_NE(calc, nullptr);
    key(*calc, Qt::Key_Return);
    key(*calc, Qt::Key_Return);
    digit(*calc, '2');
    key(*calc, Qt::Key_Return);
    EXPECT_FALSE(tool->movePending());
    const auto* moved = project.structure.findNode(4);
    EXPECT_DOUBLE_EQ(moved->position.x, 7);
    EXPECT_DOUBLE_EQ(moved->position.y, 0);
    EXPECT_DOUBLE_EQ(moved->position.z, 2);
}

TEST_F(NodeToolTest, MoverCalculatorStartsWithTheCurrentYAndZOfTheNode) {
    tool->moveAction()->trigger();
    click(*view, screenOf(4, 0, 3));  // N4 at (4, 0, 3)
    ASSERT_TRUE(tool->movePending());
    digit(*view, '9');
    NodeCalculatorDialog* calc = tool->calculator();
    ASSERT_NE(calc, nullptr);
    EXPECT_EQ(calc->fieldText(0), QStringLiteral("9"));
    EXPECT_EQ(calc->fieldText(1), QStringLiteral("0"));
    EXPECT_EQ(calc->fieldText(2), QStringLiteral("3"));
    key(*calc, Qt::Key_Return);
    key(*calc, Qt::Key_Return);
    key(*calc, Qt::Key_Return);  // Y and Z untouched: only X changes
    const auto* moved = project.structure.findNode(4);
    EXPECT_DOUBLE_EQ(moved->position.x, 9);
    EXPECT_DOUBLE_EQ(moved->position.z, 3);
}

TEST_F(NodeToolTest, ProjectClosedEndsTheCommand) {
    tool->newAction()->trigger();
    tool->setProject(nullptr, &undo);
    EXPECT_EQ(tool->mode(), Mode::None);
    EXPECT_FALSE(tool->newAction()->isEnabled());
    EXPECT_FALSE(view->snapMode());
}

int main(int argc, char** argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

// Developer aid: CC_SNAPSHOT_DIR=<folder> writes the calculator as a PNG to compare with orig_nudos_nuevo_02_calculadora.png.
TEST_F(NodeToolTest, CalculatorSnapshotForVisualComparison) {
    const QString folder = qEnvironmentVariable("CC_SNAPSHOT_DIR");
    if (folder.isEmpty()) GTEST_SKIP() << "CC_SNAPSHOT_DIR not set";
    NodeCalculatorDialog dialog;
    dialog.start(QLatin1Char('5'));
    EXPECT_TRUE(dialog.grab().save(folder + QStringLiteral("/calculator.png")));
}
