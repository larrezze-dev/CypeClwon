// Geometria > Barras (T-P5-03, GUI part): the BarTool commands Barra, Pilar and Viga. Never shown on screen (offscreen).
#include <gtest/gtest.h>

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QDialogButtonBox>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPushButton>
#include <QSignalSpy>
#include <memory>

#include "BarTool.h"
#include "Viewport3D.h"
#include "core/bar_edit.h"
#include "core/node_edit.h"

using cc::app::BarTool;
using cc::app::Viewport3D;
using Mode = cc::app::BarTool::Mode;

namespace {
QApplication* app() {
    static int argc = 1;
    static char name[] = "bar_tool_tests";
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
void key(QWidget& w, int k, const QString& text = {}) {
    QKeyEvent press(QEvent::KeyPress, k, Qt::NoModifier, text);
    QApplication::sendEvent(&w, &press);
    QKeyEvent release(QEvent::KeyRelease, k, Qt::NoModifier, text);
    QApplication::sendEvent(&w, &release);
}

class BarToolTest : public ::testing::Test {
protected:
    void SetUp() override {
        app();
        auto& model = project.structure;
        // A 4 x 3 m frame seen from the front: N1 (0,0,0) N2 (4,0,0) N3 (0,0,3) N4 (4,0,3), no bars yet.
        model.addNode({0, 0, 0});
        model.addNode({4, 0, 0});
        model.addNode({0, 0, 3});
        model.addNode({4, 0, 3});
        view = std::make_unique<Viewport3D>();
        view->resize(800, 600);
        view->setStandardView(cc::core::StandardView::Front);
        view->setOrthographic(true);
        view->camera().target = {2, 0, 1.5};
        view->setStructure(&model);
        tool = std::make_unique<BarTool>();
        tool->setProject(&project, &undo);
        tool->addViewport(view.get());
        undo.setChangeCallback([this] { tool->structureChanged(); });
        QObject::connect(tool.get(), &BarTool::statusChanged, [this](const QString& text) { status = text; });
        QObject::connect(tool.get(), &BarTool::messageRequested, [this](const QString& text) { message = text; });
        undo.setClean();
    }
    QPointF screenOf(double x, double y, double z) {
        QPointF p;
        EXPECT_TRUE(view->worldToScreen({x, y, z}, p));
        return p;
    }
    std::size_t barCount() const { return project.structure.bars.size(); }
    void addLevels() { project.workPlanes.levels = {{"N1", 0.0}, {"N2", 3.0}, {"N3", 6.0}}; }
    static void flush() { QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete); }

    cc::core::Project project;
    cc::core::UndoStack undo;
    std::unique_ptr<Viewport3D> view;
    std::unique_ptr<BarTool> tool;
    QString status, message;
};
}  // namespace

TEST_F(BarToolTest, ThreeButtonsInAnExclusiveOptionalGroup) {
    EXPECT_EQ(tool->barAction()->text(), QStringLiteral("Barra"));
    EXPECT_EQ(tool->columnAction()->text(), QStringLiteral("Pilar"));
    EXPECT_EQ(tool->beamAction()->text(), QStringLiteral("Viga"));
    EXPECT_EQ(tool->group()->exclusionPolicy(), QActionGroup::ExclusionPolicy::ExclusiveOptional);
    EXPECT_TRUE(tool->barAction()->toolTip().startsWith(QStringLiteral("Introducción de una nueva barra")));
    tool->barAction()->trigger();
    EXPECT_TRUE(tool->barAction()->isChecked());
    tool->beamAction()->trigger();
    EXPECT_FALSE(tool->barAction()->isChecked());
    EXPECT_EQ(tool->mode(), Mode::Beam);
    tool->beamAction()->trigger();  // second click releases it
    EXPECT_EQ(tool->mode(), Mode::None);
}

TEST_F(BarToolTest, DisabledWithoutProject) {
    BarTool other;
    EXPECT_FALSE(other.barAction()->isEnabled());
    other.setProject(&project, &undo);
    EXPECT_TRUE(other.barAction()->isEnabled());
    EXPECT_TRUE(other.columnAction()->isEnabled());
    other.barAction()->trigger();
    other.setProject(nullptr, &undo);
    EXPECT_EQ(other.mode(), Mode::None);
    EXPECT_FALSE(other.beamAction()->isEnabled());
}

TEST_F(BarToolTest, BarraTwoNodesCreateABarAndTheCommandGoesOn) {
    QSignalSpy activating(tool.get(), &BarTool::aboutToActivate);
    tool->barAction()->trigger();
    EXPECT_EQ(activating.count(), 1);
    EXPECT_TRUE(view->selectionMode());
    EXPECT_TRUE(view->nodeLabelsVisible());
    EXPECT_EQ(status, QStringLiteral("Barras - Barra |  - LMB: Seleccione un elemento."));
    click(*view, screenOf(0, 0, 0));
    EXPECT_EQ(tool->chain(), (std::vector<int>{1}));
    EXPECT_EQ(view->rubberChain(), (std::vector<int>{1}));
    EXPECT_EQ(status, QStringLiteral("Barras - Barra |  - LMB: Seleccione un elemento. - RMB: Volver al paso anterior."));
    EXPECT_EQ(barCount(), 0u);
    click(*view, screenOf(4, 0, 0));
    ASSERT_EQ(barCount(), 1u);
    EXPECT_EQ(project.structure.bars.front().startNode, 1);
    EXPECT_EQ(project.structure.bars.front().endNode, 2);
    EXPECT_TRUE(tool->chain().empty());
    EXPECT_TRUE(view->rubberChain().empty());
    EXPECT_EQ(tool->mode(), Mode::Bar);
    EXPECT_EQ(project.structure.bars.front().materialId, 0) << "new bars are not described";
    // Another bar from the same node (the selection was consumed, so the node can be clicked again).
    click(*view, screenOf(0, 0, 0));
    click(*view, screenOf(0, 0, 3));
    EXPECT_EQ(barCount(), 2u);
    undo.undo();
    EXPECT_EQ(barCount(), 1u);
    undo.redo();
    EXPECT_EQ(barCount(), 2u);
}

TEST_F(BarToolTest, BarraSameNodeTwiceWaitsAndRmbGoesBack) {
    tool->barAction()->trigger();
    click(*view, screenOf(0, 0, 0));
    click(*view, screenOf(0, 0, 0));
    EXPECT_EQ(tool->chain().size(), 1u);
    EXPECT_EQ(barCount(), 0u);
    click(*view, screenOf(2, 0, 1.5), Qt::RightButton);  // RMB: "Volver al paso anterior"
    EXPECT_TRUE(tool->chain().empty());
    EXPECT_EQ(tool->mode(), Mode::Bar);
    EXPECT_EQ(status, QStringLiteral("Barras - Barra |  - LMB: Seleccione un elemento."));
    EXPECT_TRUE(undo.isClean());
}

TEST_F(BarToolTest, BarraDuplicateCreatesNothing) {
    cc::core::addBar(project, undo, 1, 2);
    undo.setClean();
    tool->barAction()->trigger();
    click(*view, screenOf(4, 0, 0));
    click(*view, screenOf(0, 0, 0));
    EXPECT_EQ(barCount(), 1u);
    EXPECT_TRUE(undo.isClean());
    EXPECT_TRUE(tool->chain().empty());
}

TEST_F(BarToolTest, VigaChainsNodesAndRmbCreatesTheContinuousBeamInOneStep) {
    tool->beamAction()->trigger();
    EXPECT_EQ(status, QStringLiteral("Barras - Viga |  - LMB: Seleccione un elemento."));
    click(*view, screenOf(0, 0, 3));
    click(*view, screenOf(4, 0, 3));
    click(*view, screenOf(4, 0, 0));
    EXPECT_EQ(tool->chain(), (std::vector<int>{3, 4, 2}));
    EXPECT_EQ(view->rubberChain().size(), 3u) << "the elastic preview is fed by the chain";
    EXPECT_EQ(barCount(), 0u) << "nothing is created until the RMB";
    EXPECT_EQ(status, QStringLiteral("Barras - Viga |  - LMB: Seleccione un elemento. - RMB: Actuar sobre los elementos seleccionados."));
    click(*view, screenOf(2, 0, 1.5), Qt::RightButton);
    EXPECT_EQ(barCount(), 2u);
    EXPECT_TRUE(tool->chain().empty());
    EXPECT_EQ(tool->mode(), Mode::Beam) << "the command stays active";
    undo.undo();
    EXPECT_EQ(barCount(), 0u) << "one undoable step for the whole beam";
    undo.redo();
    EXPECT_EQ(barCount(), 2u);
}

TEST_F(BarToolTest, VigaWithOneNodeOnlyDropsTheChain) {
    tool->beamAction()->trigger();
    click(*view, screenOf(0, 0, 3));
    click(*view, screenOf(0, 0, 3));  // the same node again is ignored
    EXPECT_EQ(tool->chain().size(), 1u);
    click(*view, screenOf(2, 0, 1.5), Qt::RightButton);
    EXPECT_EQ(barCount(), 0u);
    EXPECT_TRUE(tool->chain().empty());
    EXPECT_TRUE(undo.isClean());
}

TEST_F(BarToolTest, UndoOfAChosenNodeBreaksTheChain) {
    const int id = cc::core::addNode(project, undo, {2, 0, 3});
    ASSERT_NE(id, 0);
    tool->beamAction()->trigger();
    click(*view, screenOf(2, 0, 3));
    ASSERT_EQ(tool->chain().size(), 1u);
    undo.undo();
    EXPECT_TRUE(tool->chain().empty());
    EXPECT_TRUE(view->rubberChain().empty());
}

TEST_F(BarToolTest, EscLeavesTheCommand) {
    tool->barAction()->trigger();
    click(*view, screenOf(0, 0, 0));
    key(*view, Qt::Key_Escape);
    EXPECT_EQ(tool->mode(), Mode::None);
    EXPECT_FALSE(tool->barAction()->isChecked());
    EXPECT_FALSE(view->selectionMode());
    EXPECT_TRUE(tool->chain().empty());
    EXPECT_TRUE(view->rubberChain().empty());
    EXPECT_EQ(status, QStringLiteral("Seleccione una opción del menú."));
}

TEST_F(BarToolTest, PilarWithFewerThanTwoLevelsShowsTheNotice) {
    tool->columnAction()->trigger();
    EXPECT_EQ(tool->mode(), Mode::None);
    EXPECT_FALSE(tool->columnAction()->isChecked());
    EXPECT_TRUE(message.startsWith(QStringLiteral("Para poder introducir pilares con esta opción")));
    message.clear();
    project.workPlanes.levels = {{"N1", 0.0}};
    tool->columnAction()->trigger();
    EXPECT_FALSE(message.isEmpty());
    EXPECT_EQ(tool->mode(), Mode::None);
}

TEST_F(BarToolTest, PilarWithLevelsOpensTheDialogAndCreatesOneBarPerPair) {
    addLevels();
    tool->columnAction()->trigger();
    EXPECT_EQ(tool->mode(), Mode::Column);
    EXPECT_TRUE(message.isEmpty());
    EXPECT_TRUE(view->snapMode());
    EXPECT_EQ(status, QStringLiteral("Barras - Pilar |  - LMB: Introduzca un punto."));
    click(*view, screenOf(4, 0, 3));  // snaps to N4: plan position (4, 0)
    cc::app::ColumnDialog* dialog = tool->columnDialog();
    ASSERT_NE(dialog, nullptr);
    EXPECT_DOUBLE_EQ(dialog->xSpin()->value(), 4.0);
    EXPECT_DOUBLE_EQ(dialog->ySpin()->value(), 0.0);
    EXPECT_EQ(dialog->startCombo()->currentIndex(), 0);
    EXPECT_EQ(dialog->endCombo()->currentIndex(), 2);
    dialog->xSpin()->setValue(10.0);
    dialog->ySpin()->setValue(5.0);
    const std::size_t nodes = project.structure.nodes.size();
    dialog->accept();
    flush();
    EXPECT_EQ(tool->columnDialog(), nullptr);
    EXPECT_EQ(project.structure.nodes.size(), nodes + 3) << "a node at each of the three levels";
    EXPECT_EQ(barCount(), 2u) << "one piece between each pair of levels";
    EXPECT_EQ(tool->mode(), Mode::Column) << "the command goes on";
    undo.undo();
    EXPECT_EQ(barCount(), 0u);
    EXPECT_EQ(project.structure.nodes.size(), nodes);
}

TEST_F(BarToolTest, PilarDialogNeedsTwoDifferentLevelsAndAcceptsAPartialRun) {
    addLevels();
    tool->columnAction()->trigger();
    click(*view, screenOf(0, 0, 0));
    cc::app::ColumnDialog* dialog = tool->columnDialog();
    ASSERT_NE(dialog, nullptr);
    dialog->endCombo()->setCurrentIndex(0);  // same level at both ends
    EXPECT_FALSE(dialog->canAccept());
    EXPECT_FALSE(dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok)->isEnabled());
    dialog->accept();  // an invalid choice is refused: nothing is created and the dialog stays open
    EXPECT_EQ(barCount(), 0u);
    EXPECT_EQ(tool->columnDialog(), dialog);
    tool->deactivate();
    flush();
    tool->columnAction()->trigger();
    click(*view, screenOf(0, 0, 0));
    dialog = tool->columnDialog();
    ASSERT_NE(dialog, nullptr);
    dialog->startCombo()->setCurrentIndex(1);  // from the second to the third level only
    EXPECT_EQ(dialog->chosenElevations(), (std::vector<double>{3.0, 6.0}));
    dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok)->click();
    flush();
    EXPECT_EQ(barCount(), 1u);
}

TEST_F(BarToolTest, PilarTypedDigitOpensTheDialogAndFreeClicksDoNothing) {
    addLevels();
    tool->columnAction()->trigger();
    click(*view, QPointF(30, 30));  // no snap
    EXPECT_EQ(tool->columnDialog(), nullptr);
    key(*view, Qt::Key_5, QStringLiteral("5"));
    EXPECT_NE(tool->columnDialog(), nullptr);
    tool->deactivate();
    flush();
    EXPECT_EQ(tool->columnDialog(), nullptr) << "leaving the command closes the dialog";
}

TEST_F(BarToolTest, ToolsExcludeEachOther) {
    QSignalSpy activating(tool.get(), &BarTool::aboutToActivate);
    tool->barAction()->trigger();
    tool->beamAction()->trigger();
    EXPECT_EQ(activating.count(), 2) << "MainWindow uses it to stop Nudos and Asignar elementos";
    EXPECT_FALSE(tool->barAction()->isChecked());
    EXPECT_EQ(tool->mode(), Mode::Beam);
    EXPECT_FALSE(view->snapMode());
    tool->deactivate();
    EXPECT_EQ(tool->mode(), Mode::None);
}

TEST_F(BarToolTest, ProjectClosedEndsTheCommand) {
    tool->barAction()->trigger();
    tool->setProject(nullptr, &undo);
    EXPECT_EQ(tool->mode(), Mode::None);
    EXPECT_FALSE(tool->barAction()->isEnabled());
    EXPECT_FALSE(view->selectionMode());
}

int main(int argc, char** argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
