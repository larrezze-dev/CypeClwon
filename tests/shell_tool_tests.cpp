// Geometria > Laminas (T-P5-09, GUI part): the ShellTool commands. Never shown on screen (offscreen).
#include <gtest/gtest.h>

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QDialog>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QSignalSpy>
#include <memory>

#include "ShellDialogs.h"
#include "ShellTool.h"
#include "Viewport3D.h"
#include "core/shell_edit.h"

using cc::app::ShellTool;
using cc::app::Viewport3D;
using Mode = cc::app::ShellTool::Mode;
using namespace cc::core;

namespace {
QApplication* app() {
    static int argc = 1;
    static char name[] = "shell_tool_tests";
    static char* argv[] = {name, nullptr};
    static QApplication instance(argc, argv);
    return &instance;
}

void mouse(Viewport3D& v, QEvent::Type type, QPointF pos, Qt::MouseButton button, Qt::MouseButtons buttons) {
    QMouseEvent e(type, pos, v.mapToGlobal(pos), button, buttons, Qt::NoModifier);
    QApplication::sendEvent(&v, &e);
}
void click(Viewport3D& v, QPointF pos, Qt::MouseButton button = Qt::LeftButton) {
    mouse(v, QEvent::MouseMove, pos, Qt::NoButton, Qt::NoButton);
    mouse(v, QEvent::MouseButtonPress, pos, button, button);
    mouse(v, QEvent::MouseButtonRelease, pos, button, Qt::NoButton);
}
void key(QWidget& w, int k) {
    QKeyEvent press(QEvent::KeyPress, k, Qt::NoModifier);
    QApplication::sendEvent(&w, &press);
}

/// Seen from the front: two 4 m x 3 m shells side by side in the xz plane, S1 on N1..N4 (x 0..4) and S2 on N2 N5 N6 N3 (x 4..8),
/// plus a lone node N7 at (0, 0, 6).
class ShellToolTest : public ::testing::Test {
protected:
    void SetUp() override {
        app();
        auto& model = project.structure;
        model.addNode({0, 0, 0});  // N1
        model.addNode({4, 0, 0});  // N2
        model.addNode({4, 0, 3});  // N3
        model.addNode({0, 0, 3});  // N4
        model.addNode({8, 0, 0});  // N5
        model.addNode({8, 0, 3});  // N6
        model.addNode({0, 0, 6});  // N7
        ASSERT_EQ(addShell(project, undo, {1, 2, 3, 4}), ShellError::None);
        ASSERT_EQ(addShell(project, undo, {2, 5, 6, 3}), ShellError::None);
        view = std::make_unique<Viewport3D>();
        view->resize(800, 600);
        view->setStandardView(cc::core::StandardView::Front);
        view->setOrthographic(true);
        view->camera().target = {4, 0, 3};
        view->setStructure(&model);
        tool = std::make_unique<ShellTool>();
        tool->setProject(&project, &undo);
        tool->addViewport(view.get());
        undo.setChangeCallback([this] { tool->structureChanged(); });
        QObject::connect(tool.get(), &ShellTool::statusChanged, [this](const QString& text) { status = text; });
        QObject::connect(tool.get(), &ShellTool::messageRequested, [this](const QString& text) { message = text; });
        tool->setDialogRunner([this](QDialog&) {
            ++dialogs;
            return static_cast<int>(QDialog::Rejected);
        });
        undo.setClean();
    }
    QPointF screenOf(double x, double y, double z) {
        QPointF p;
        EXPECT_TRUE(view->worldToScreen({x, y, z}, p));
        return p;
    }
    QPointF nodeScreen(int id) {
        const Node& n = *project.structure.findNode(id);
        return screenOf(n.position.x, n.position.y, n.position.z);
    }
    Project project;
    UndoStack undo;
    std::unique_ptr<Viewport3D> view;
    std::unique_ptr<ShellTool> tool;
    QString status, message;
    int dialogs = 0;
};
}  // namespace

TEST_F(ShellToolTest, ButtonsAndTooltipsFollowTheOriginalTexts) {
    EXPECT_EQ(tool->newAction()->text(), QStringLiteral("Nueva"));
    EXPECT_EQ(tool->invertZAction()->text(), QStringLiteral("Invertir eje 'z'"));
    EXPECT_EQ(tool->defineXAction()->text(), QStringLiteral("Definir eje 'x'"));
    EXPECT_EQ(tool->localAxesAction()->text(), QStringLiteral("Ejes locales"));
    EXPECT_EQ(tool->actionGroup()->exclusionPolicy(), QActionGroup::ExclusionPolicy::ExclusiveOptional);
    EXPECT_TRUE(tool->newAction()->toolTip().contains(QStringLiteral("pulse el botón derecho del ratón")));
    EXPECT_TRUE(tool->divideAction()->toolTip().contains(QStringLiteral("dos puntos que definirán el segmento de división")));
    tool->newAction()->trigger();
    EXPECT_EQ(tool->mode(), Mode::New);
    tool->joinAction()->trigger();
    EXPECT_FALSE(tool->newAction()->isChecked());
    EXPECT_EQ(tool->mode(), Mode::Join);
    tool->joinAction()->trigger();  // second click releases it
    EXPECT_EQ(tool->mode(), Mode::None);
}

TEST_F(ShellToolTest, DisabledWithoutProjectAndWithoutShells) {
    ShellTool other;
    EXPECT_FALSE(other.newAction()->isEnabled());
    Project empty;
    UndoStack stack;
    other.setProject(&empty, &stack);
    EXPECT_TRUE(other.newAction()->isEnabled());
    EXPECT_FALSE(other.deleteAction()->isEnabled()) << "nothing to delete";
    EXPECT_FALSE(other.materialAction()->isEnabled());
    other.setProject(&project, &undo);
    EXPECT_TRUE(other.deleteAction()->isEnabled());
    EXPECT_TRUE(other.materialAction()->isEnabled());
    other.setProject(nullptr, &undo);
    EXPECT_FALSE(other.newAction()->isEnabled());
}

TEST_F(ShellToolTest, ActivatingAnnouncesItselfAndSetsTheViewport) {
    QSignalSpy activating(tool.get(), &ShellTool::aboutToActivate);
    tool->newAction()->trigger();
    EXPECT_EQ(activating.count(), 1);
    EXPECT_TRUE(view->snapMode());
    EXPECT_TRUE(view->nodeLabelsVisible());
    EXPECT_EQ(status, QStringLiteral("Láminas - Nueva |  - LMB: Introduzca un punto."));
    tool->deleteAction()->trigger();
    EXPECT_FALSE(view->snapMode());
    EXPECT_TRUE(view->selectionMode());
    key(*view, Qt::Key_Escape);
    EXPECT_EQ(tool->mode(), Mode::None);
    EXPECT_FALSE(view->selectionMode());
}

TEST_F(ShellToolTest, NuevaWithNodeClicksAndRightClickCreatesTheShell) {
    tool->newAction()->trigger();
    click(*view, nodeScreen(1));
    click(*view, nodeScreen(4));
    click(*view, nodeScreen(7));
    ASSERT_EQ(tool->points().size(), 3u);
    EXPECT_EQ(view->rubberPoints().size(), 3u);
    click(*view, nodeScreen(1), Qt::RightButton);  // closes it: N1, N4 and N7 are collinear (x = 0): not a shell
    EXPECT_EQ(message, QString::fromStdString(shellErrorMessage(ShellError::InvalidContour)));
    EXPECT_EQ(project.structure.shells.size(), 2u);
    EXPECT_TRUE(tool->points().empty());
    // N4, N3 and N7: a triangle
    message.clear();
    click(*view, nodeScreen(4));
    click(*view, nodeScreen(3));
    click(*view, nodeScreen(7));
    click(*view, nodeScreen(1), Qt::RightButton);
    EXPECT_TRUE(message.isEmpty());
    ASSERT_EQ(project.structure.shells.size(), 3u);
    EXPECT_EQ(project.structure.shells.back().nodeIds.size(), 3u);
    EXPECT_EQ(tool->mode(), Mode::New) << "the command goes on";
    undo.undo();
    EXPECT_EQ(project.structure.shells.size(), 2u);
}

TEST_F(ShellToolTest, CreateShellWithFreePointsMakesTheNodesInOneUndoStep) {
    const std::size_t nodes = project.structure.nodes.size();
    EXPECT_TRUE(tool->createShell({{0, 0, 7}, {4, 0, 7}, {4, 0, 9}, {0, 0, 9}}));
    EXPECT_EQ(project.structure.nodes.size(), nodes + 4);
    EXPECT_EQ(project.structure.shells.size(), 3u);
    undo.undo();
    EXPECT_EQ(project.structure.nodes.size(), nodes) << "nodes and shell are one undo step";
    EXPECT_EQ(project.structure.shells.size(), 2u);
    // Reuses the existing node at (0, 0, 6)
    EXPECT_TRUE(tool->createShell({{0, 0, 6}, {3, 0, 6}, {0, 0, 9}}));
    EXPECT_EQ(project.structure.nodes.size(), nodes + 2);
}

TEST_F(ShellToolTest, CreateShellRefusalLeavesNothingBehind) {
    const std::size_t nodes = project.structure.nodes.size();
    EXPECT_FALSE(tool->createShell({{0, 0, 7}, {1, 0, 7}, {2, 0, 7}}));  // collinear
    EXPECT_FALSE(tool->createShell({{0, 0, 7}, {1, 0, 7}}));            // two points
    EXPECT_EQ(project.structure.nodes.size(), nodes);
    EXPECT_EQ(project.structure.shells.size(), 2u);
    EXPECT_EQ(message, QString::fromStdString(shellErrorMessage(ShellError::InvalidContour)));
}

TEST_F(ShellToolTest, MoreThanFourPointsAreCutIntoShells) {
    EXPECT_TRUE(tool->createShell({{0, 0, 10}, {3, 0, 10}, {5, 0, 12}, {3, 0, 14}, {0, 0, 14}}));
    EXPECT_GE(project.structure.shells.size(), 4u);  // two or three shells for the pentagon
    undo.undo();
    EXPECT_EQ(project.structure.shells.size(), 2u);
}

TEST_F(ShellToolTest, BorrarSelectsAndRightClickDeletes) {
    tool->deleteAction()->trigger();
    click(*view, screenOf(2, 0, 1.5));
    EXPECT_EQ(view->selection().count(ElementKind::Shell), 1u);
    click(*view, screenOf(2, 0, 1.5), Qt::RightButton);
    ASSERT_EQ(project.structure.shells.size(), 1u);
    EXPECT_EQ(project.structure.shells.front().id, 2);
    EXPECT_EQ(tool->mode(), Mode::Delete);
}

TEST_F(ShellToolTest, UnirOfTwoCoplanarShellsAndInvertZ) {
    tool->invertZAction()->trigger();
    click(*view, screenOf(2, 0, 1.5));
    click(*view, screenOf(2, 0, 1.5), Qt::RightButton);
    EXPECT_TRUE(project.structure.findShell(1)->zAxisInverted);
    tool->joinAction()->trigger();
    click(*view, screenOf(2, 0, 1.5));
    mouse(*view, QEvent::MouseMove, screenOf(6, 0, 1.5), Qt::NoButton, Qt::NoButton);
    QMouseEvent press(QEvent::MouseButtonPress, screenOf(6, 0, 1.5), view->mapToGlobal(screenOf(6, 0, 1.5)), Qt::LeftButton, Qt::LeftButton, Qt::ControlModifier);
    QApplication::sendEvent(view.get(), &press);
    QMouseEvent release(QEvent::MouseButtonRelease, screenOf(6, 0, 1.5), view->mapToGlobal(screenOf(6, 0, 1.5)), Qt::LeftButton, Qt::NoButton, Qt::ControlModifier);
    QApplication::sendEvent(view.get(), &release);  // Ctrl adds the second shell
    EXPECT_EQ(view->selection().count(ElementKind::Shell), 2u);
    click(*view, screenOf(2, 0, 1.5), Qt::RightButton);
    ASSERT_EQ(project.structure.shells.size(), 1u);
    EXPECT_EQ(project.structure.shells.front().nodeIds.size(), 4u);
}

TEST_F(ShellToolTest, UnirOfASingleShellShowsTheOriginalMessage) {
    tool->joinAction()->trigger();
    click(*view, screenOf(2, 0, 1.5));
    click(*view, screenOf(2, 0, 1.5), Qt::RightButton);
    EXPECT_EQ(message, QString::fromStdString(shellErrorMessage(ShellError::TooFewShells)));
    EXPECT_EQ(project.structure.shells.size(), 2u);
}

TEST_F(ShellToolTest, DividirSelectsThenTwoPoints) {
    // Two extra nodes on the edges of S1, at (2, 0, 0) and (2, 0, 3): the division segment
    project.structure.addNode({2, 0, 0});  // N8
    project.structure.addNode({2, 0, 3});  // N9
    view->refreshStructure();
    tool->divideAction()->trigger();
    click(*view, screenOf(1, 0, 1.5));
    click(*view, screenOf(1, 0, 1.5), Qt::RightButton);
    EXPECT_EQ(tool->step(), 1);
    EXPECT_TRUE(view->snapMode());
    click(*view, nodeScreen(8));
    click(*view, nodeScreen(9));
    ASSERT_EQ(project.structure.shells.size(), 3u);
    EXPECT_EQ(tool->step(), 0);
    undo.undo();
    EXPECT_EQ(project.structure.shells.size(), 2u);
}

TEST_F(ShellToolTest, DividirRightClickGoesBack) {
    tool->divideAction()->trigger();
    click(*view, screenOf(1, 0, 1.5));
    click(*view, screenOf(1, 0, 1.5), Qt::RightButton);
    click(*view, nodeScreen(1));
    EXPECT_EQ(tool->points().size(), 1u);
    click(*view, nodeScreen(1), Qt::RightButton);
    EXPECT_TRUE(tool->points().empty());
    EXPECT_EQ(tool->step(), 1);
    click(*view, nodeScreen(1), Qt::RightButton);
    EXPECT_EQ(tool->step(), 0);
}

TEST_F(ShellToolTest, MoverThreeSteps) {
    // S1 contour N1 N2 N3 N4: N4 (0, 0, 3) moves by the vector N1 -> N7 (0, 0, 6) to (0, 0, 9): still a valid quad
    tool->moveAction()->trigger();
    click(*view, screenOf(1, 0, 1.2));  // step 1: the shell
    EXPECT_EQ(tool->step(), 1);
    click(*view, nodeScreen(4));        // step 2: the node, then RMB
    click(*view, nodeScreen(4), Qt::RightButton);
    EXPECT_EQ(tool->step(), 2);
    click(*view, nodeScreen(1));        // step 3: two points
    click(*view, nodeScreen(7));
    EXPECT_EQ(tool->step(), 0);
    EXPECT_DOUBLE_EQ(project.structure.findNode(4)->position.z, 9.0);
    undo.undo();
    EXPECT_DOUBLE_EQ(project.structure.findNode(4)->position.z, 3.0);
}

TEST_F(ShellToolTest, DefinirEjeXTakesTheShellAndTwoNodes) {
    tool->defineXAction()->trigger();
    click(*view, screenOf(1, 0, 1.2));
    click(*view, nodeScreen(2));
    click(*view, nodeScreen(3));
    const Shell& shell = *project.structure.findShell(1);
    EXPECT_EQ(shell.xEdgeStart, 2);
    EXPECT_EQ(shell.xEdgeEnd, 3);
    EXPECT_EQ(tool->step(), 0);
}

TEST_F(ShellToolTest, DefinirEjeXRefusesNodesThatAreNotAnEdge) {
    tool->defineXAction()->trigger();
    click(*view, screenOf(1, 0, 1.2));
    click(*view, nodeScreen(1));
    click(*view, nodeScreen(3));  // diagonal
    EXPECT_FALSE(message.isEmpty());
    EXPECT_EQ(project.structure.findShell(1)->xEdgeStart, 0);
}

TEST_F(ShellToolTest, LocalAxesToggleDrivesTheViewports) {
    EXPECT_FALSE(view->showShellAxes());
    tool->localAxesAction()->setChecked(true);
    EXPECT_TRUE(view->showShellAxes());
    view->grab();  // paints without crashing
    tool->localAxesAction()->setChecked(false);
    EXPECT_FALSE(view->showShellAxes());
}

TEST_F(ShellToolTest, PendingDialogsOnlyReportThatTheyAreNotAvailable) {
    QSignalSpy pending(tool.get(), &ShellTool::notAvailable);
    for (QAction* action : {tool->layoutAction(), tool->materialAction()}) {
        EXPECT_TRUE(action->isEnabled());
        action->trigger();
    }
    EXPECT_EQ(pending.count(), 2);
    EXPECT_EQ(dialogs, 0) << "the runner is never reached (nothing blocks)";
    EXPECT_EQ(tool->mode(), Mode::None);
}

TEST_F(ShellToolTest, SeccionAppliesTheDialogToTheSelectedShellsAsOneUndoStep) {
    ShellSection seen;
    tool->setDialogRunner([&](QDialog& d) {
        auto& dialog = static_cast<cc::app::ShellSectionDialog&>(d);
        seen = dialog.section();
        EXPECT_EQ(d.windowTitle(), QStringLiteral("Sección de la lámina"));
        EXPECT_FALSE(dialog.ballastEditable()) << "the editor follows the checkbox";
        EXPECT_FALSE(dialog.behaviourEditable());
        dialog.setThickness(25.0);  // cm (project unit)
        dialog.setBallastEnabled(true);
        EXPECT_TRUE(dialog.ballastEditable());
        EXPECT_TRUE(dialog.behaviourEditable());
        dialog.setBallastValue(5000.0);
        dialog.setBehaviour(BallastBehaviour::CompressionOnly);
        return static_cast<int>(QDialog::Accepted);
    });
    const std::size_t before = undo.count();
    tool->sectionAction()->trigger();
    EXPECT_EQ(tool->mode(), Mode::Section);
    click(*view, screenOf(1, 0, 1.2));
    click(*view, screenOf(7, 0, 1.2), Qt::LeftButton);  // plain click replaces the selection: use the direct entry for several shells below
    click(*view, screenOf(7, 0, 1.2), Qt::RightButton);
    EXPECT_NEAR(seen.thicknessM, kDefaultShellThicknessM, 1e-12);
    EXPECT_FALSE(seen.ballastEnabled);
    EXPECT_EQ(undo.count(), before + 1);
    EXPECT_NEAR(project.structure.findShell(2)->thicknessM, 0.25, 1e-12);
    EXPECT_TRUE(project.structure.findShell(2)->ballastEnabled);
    EXPECT_DOUBLE_EQ(project.structure.findShell(2)->ballastKnM3, 5000.0);
    EXPECT_EQ(project.structure.findShell(2)->ballastBehaviour, BallastBehaviour::CompressionOnly);
    EXPECT_FALSE(project.structure.findShell(1)->ballastEnabled);
    undo.undo();
    EXPECT_FALSE(project.structure.findShell(2)->ballastEnabled);
    // Several shells at once: one undo step.
    const std::size_t again = undo.count();
    ASSERT_TRUE(tool->editSection({1, 2}));
    EXPECT_EQ(undo.count(), again) << "the undone step is replaced by one new step";
    for (const Shell& sh : project.structure.shells) EXPECT_TRUE(sh.ballastEnabled);
    undo.undo();
    for (const Shell& sh : project.structure.shells) EXPECT_FALSE(sh.ballastEnabled);
}

TEST_F(ShellToolTest, SeccionCancelChangesNothing) {
    const std::size_t before = undo.count();
    EXPECT_FALSE(tool->editSection({1, 2}));
    EXPECT_EQ(dialogs, 1);
    EXPECT_EQ(undo.count(), before);
}

TEST_F(ShellToolTest, UndoOfTheChosenShellBreaksTheSteps) {
    tool->moveAction()->trigger();
    click(*view, screenOf(1, 0, 1.2));
    ASSERT_EQ(tool->step(), 1);
    undo.push(std::make_unique<DeleteShellsCommand>(project, std::vector<int>{1}));
    EXPECT_EQ(tool->step(), 0);
}

int main(int argc, char** argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
