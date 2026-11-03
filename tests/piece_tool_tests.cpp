// Geometria > Barras (T-P5-04, GUI part): the PieceTool commands Pieza, Agrupar, Desagrupar, Mover extremo and Invertir
// eje 'x'. Never shown on screen (offscreen).
#include <gtest/gtest.h>

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QSignalSpy>
#include <memory>

#include "BarTool.h"
#include "PieceTool.h"
#include "Viewport3D.h"
#include "core/bar_edit.h"
#include "core/piece_edit.h"

using cc::app::PieceTool;
using cc::app::Viewport3D;
using Mode = cc::app::PieceTool::Mode;

namespace {
QApplication* app() {
    static int argc = 1;
    static char name[] = "piece_tool_tests";
    static char* argv[] = {name, nullptr};
    static QApplication instance(argc, argv);
    return &instance;
}

void mouse(Viewport3D& v, QEvent::Type type, QPointF pos, Qt::MouseButton button, Qt::MouseButtons buttons, Qt::KeyboardModifiers mods = Qt::NoModifier) {
    QMouseEvent e(type, pos, v.mapToGlobal(pos), button, buttons, mods);
    QApplication::sendEvent(&v, &e);
}
void click(Viewport3D& v, QPointF pos, Qt::MouseButton button = Qt::LeftButton, Qt::KeyboardModifiers mods = Qt::NoModifier) {
    mouse(v, QEvent::MouseMove, pos, Qt::NoButton, Qt::NoButton, mods);
    mouse(v, QEvent::MouseButtonPress, pos, button, button, mods);
    mouse(v, QEvent::MouseButtonRelease, pos, button, Qt::NoButton, mods);
}
void key(QWidget& w, int k) {
    QKeyEvent press(QEvent::KeyPress, k, Qt::NoModifier);
    QApplication::sendEvent(&w, &press);
    QKeyEvent release(QEvent::KeyRelease, k, Qt::NoModifier);
    QApplication::sendEvent(&w, &release);
}

class PieceToolTest : public ::testing::Test {
protected:
    void SetUp() override {
        app();
        auto& model = project.structure;
        // Seen from the front: a 6 m beam N1 (0,0,0) - N2 (3,0,0) - N3 (6,0,0), plus N4 (0,0,3) and N5 (6,0,3).
        model.addNode({0, 0, 0});
        model.addNode({3, 0, 0});
        model.addNode({6, 0, 0});
        model.addNode({0, 0, 3});
        model.addNode({6, 0, 3});
        cc::core::addBar(project, undo, 1, 2);  // bar 1
        cc::core::addBar(project, undo, 2, 3);  // bar 2
        cc::core::addBar(project, undo, 1, 4);  // bar 3
        cc::core::addBar(project, undo, 3, 5);  // bar 4
        view = std::make_unique<Viewport3D>();
        view->resize(800, 600);
        view->setStandardView(cc::core::StandardView::Front);
        view->setOrthographic(true);
        view->camera().target = {3, 0, 1.5};
        view->setStructure(&model);
        tool = std::make_unique<PieceTool>();
        tool->setProject(&project, &undo);
        tool->addViewport(view.get());
        undo.setChangeCallback([this] { tool->structureChanged(); });
        QObject::connect(tool.get(), &PieceTool::statusChanged, [this](const QString& text) { status = text; });
        QObject::connect(tool.get(), &PieceTool::messageRequested, [this](const QString& text) { message = text; });
        tool->setConfirmer([this](const QString& q) {
            questions.push_back(q);
            return answer;
        });
        undo.setClean();
    }
    QPointF screenOf(double x, double y, double z) {
        QPointF p;
        EXPECT_TRUE(view->worldToScreen({x, y, z}, p));
        return p;
    }
    const cc::core::Bar& bar(int id) { return *project.structure.findBar(id); }

    cc::core::Project project;
    cc::core::UndoStack undo;
    std::unique_ptr<Viewport3D> view;
    std::unique_ptr<PieceTool> tool;
    QString status, message;
    QStringList questions;
    bool answer = true;
};
}  // namespace

TEST_F(PieceToolTest, FiveButtonsInAnExclusiveOptionalGroup) {
    EXPECT_EQ(tool->pieceAction()->text(), QStringLiteral("Pieza"));
    EXPECT_EQ(tool->groupAction()->text(), QStringLiteral("Agrupar"));
    EXPECT_EQ(tool->ungroupAction()->text(), QStringLiteral("Desagrupar"));
    EXPECT_EQ(tool->moveEndAction()->text(), QStringLiteral("Mover extremo"));
    EXPECT_EQ(tool->invertAction()->text(), QStringLiteral("Invertir eje 'x'"));
    EXPECT_EQ(tool->actionGroup()->exclusionPolicy(), QActionGroup::ExclusionPolicy::ExclusiveOptional);
    EXPECT_TRUE(tool->pieceAction()->toolTip().startsWith(QStringLiteral("Permite describir conjuntamente")));
    EXPECT_TRUE(tool->groupAction()->toolTip().contains(QStringLiteral("seleccione dos o más piezas")));
    tool->pieceAction()->trigger();
    EXPECT_EQ(tool->mode(), Mode::Piece);
    tool->groupAction()->trigger();
    EXPECT_FALSE(tool->pieceAction()->isChecked());
    EXPECT_EQ(tool->mode(), Mode::Group);
    tool->groupAction()->trigger();  // second click releases it
    EXPECT_EQ(tool->mode(), Mode::None);
}

TEST_F(PieceToolTest, DisabledWithoutProject) {
    PieceTool other;
    EXPECT_FALSE(other.pieceAction()->isEnabled());
    other.setProject(&project, &undo);
    for (QAction* a : {other.pieceAction(), other.groupAction(), other.ungroupAction(), other.moveEndAction(), other.invertAction()})
        EXPECT_TRUE(a->isEnabled());
    other.invertAction()->trigger();
    other.setProject(nullptr, &undo);
    EXPECT_EQ(other.mode(), Mode::None);
    EXPECT_FALSE(other.invertAction()->isEnabled());
}

TEST_F(PieceToolTest, ActivatingAnnouncesItselfSoOtherToolsStop) {
    QSignalSpy activating(tool.get(), &PieceTool::aboutToActivate);
    tool->pieceAction()->trigger();
    EXPECT_EQ(activating.count(), 1);
    EXPECT_TRUE(view->selectionMode());
    EXPECT_TRUE(view->nodeLabelsVisible());
    EXPECT_EQ(status, QStringLiteral("Barras - Pieza |  - LMB: Seleccione un elemento."));
    tool->deactivate();
    EXPECT_FALSE(view->selectionMode());
    EXPECT_FALSE(tool->pieceAction()->isChecked());
}

TEST_F(PieceToolTest, PiezaTwoNodesCreateThePieceAndTheCommandGoesOn) {
    tool->pieceAction()->trigger();
    click(*view, screenOf(0, 0, 0));
    EXPECT_EQ(tool->chain(), (std::vector<int>{1}));
    EXPECT_EQ(view->rubberChain(), (std::vector<int>{1}));
    EXPECT_EQ(status, QStringLiteral("Barras - Pieza |  - LMB: Seleccione un elemento. - RMB: Volver al paso anterior."));
    click(*view, screenOf(6, 0, 0));
    EXPECT_TRUE(message.isEmpty());
    EXPECT_FALSE(undo.isClean());
    EXPECT_EQ(project.structure.pieces.size(), 1u);
    EXPECT_TRUE(tool->chain().empty());
    EXPECT_EQ(tool->mode(), Mode::Piece);
    EXPECT_TRUE(questions.isEmpty()) << "Pieza does not ask unless setConfirmPiece(true) (not verified in the original)";
    undo.undo();
    EXPECT_TRUE(project.structure.pieces.empty());
    undo.redo();
    EXPECT_EQ(project.structure.pieces.size(), 1u);
}

TEST_F(PieceToolTest, PiezaErrorsAreReportedAndNothingIsPushed) {
    tool->pieceAction()->trigger();
    click(*view, screenOf(0, 0, 3));  // N4 - N3: no aligned chain
    click(*view, screenOf(6, 0, 0));
    EXPECT_EQ(message, QStringLiteral("No se ha encontrado un grupo de barras alineadas que conecte los dos nudos seleccionados."));
    EXPECT_TRUE(undo.isClean());
    message.clear();
    click(*view, screenOf(0, 0, 0));  // N1 - N3, then again: equivalent piece
    click(*view, screenOf(6, 0, 0));
    ASSERT_TRUE(message.isEmpty());
    undo.setClean();
    click(*view, screenOf(0, 0, 0));
    click(*view, screenOf(6, 0, 0));
    EXPECT_EQ(message, QStringLiteral("Ya existe una pieza equivalente."));
    EXPECT_TRUE(undo.isClean());
}

TEST_F(PieceToolTest, PiezaOptionalConfirmationCanBeDeclined) {
    tool->setConfirmPiece(true);
    answer = false;
    tool->pieceAction()->trigger();
    click(*view, screenOf(0, 0, 0));
    click(*view, screenOf(6, 0, 0));
    ASSERT_EQ(questions.size(), 1u);
    EXPECT_EQ(questions.front(), QStringLiteral("¿Desea crear la pieza?"));
    EXPECT_TRUE(undo.isClean());
    EXPECT_TRUE(tool->chain().empty());
}

TEST_F(PieceToolTest, PiezaSameNodeTwiceWaitsAndRmbGoesBack) {
    tool->pieceAction()->trigger();
    click(*view, screenOf(0, 0, 0));
    click(*view, screenOf(0, 0, 0));
    EXPECT_EQ(tool->chain().size(), 1u);
    click(*view, screenOf(3, 0, 1.5), Qt::RightButton);
    EXPECT_TRUE(tool->chain().empty());
    EXPECT_EQ(tool->mode(), Mode::Piece);
    EXPECT_EQ(status, QStringLiteral("Barras - Pieza |  - LMB: Seleccione un elemento."));
    EXPECT_TRUE(undo.isClean());
}

TEST_F(PieceToolTest, EscCancelsTheCommand) {
    tool->pieceAction()->trigger();
    click(*view, screenOf(0, 0, 0));
    key(*view, Qt::Key_Escape);
    EXPECT_EQ(tool->mode(), Mode::None);
    EXPECT_FALSE(tool->pieceAction()->isChecked());
    EXPECT_FALSE(view->selectionMode());
}

TEST_F(PieceToolTest, AgruparSelectsBarsAndRmbAsksThenGroupsInOneStep) {
    tool->groupAction()->trigger();
    EXPECT_EQ(status, QStringLiteral("Barras - Agrupar |  - LMB: Seleccione un elemento."));
    click(*view, screenOf(1.5, 0, 0));                         // bar 1
    click(*view, screenOf(0, 0, 1.5), Qt::LeftButton, Qt::ControlModifier);  // bar 3 (Ctrl adds)
    EXPECT_EQ(view->selection().count(cc::core::ElementKind::Bar), 2u);
    EXPECT_EQ(status, QStringLiteral("Barras - Agrupar |  - LMB: Seleccione un elemento. - RMB: Actuar sobre los elementos seleccionados."));
    click(*view, screenOf(3, 0, 1.5), Qt::RightButton);
    ASSERT_EQ(questions.size(), 1u);
    EXPECT_EQ(questions.front(), QStringLiteral("¿Desea agrupar las piezas seleccionadas?"));
    EXPECT_TRUE(message.isEmpty());
    EXPECT_FALSE(undo.isClean());
    EXPECT_TRUE(view->selection().empty());
    EXPECT_EQ(tool->mode(), Mode::Group);
    undo.undo();
    EXPECT_TRUE(undo.isClean());
}

TEST_F(PieceToolTest, AgruparDeclinedDoesNothing) {
    answer = false;
    tool->groupAction()->trigger();
    click(*view, screenOf(1.5, 0, 0));
    click(*view, screenOf(0, 0, 1.5), Qt::LeftButton, Qt::ControlModifier);
    click(*view, screenOf(3, 0, 1.5), Qt::RightButton);
    EXPECT_EQ(questions.size(), 1u);
    EXPECT_TRUE(undo.isClean());
}

TEST_F(PieceToolTest, AgruparWithOnePieceReportsTheRefusal) {
    tool->groupAction()->trigger();
    click(*view, screenOf(1.5, 0, 0));
    click(*view, screenOf(3, 0, 1.5), Qt::RightButton);
    EXPECT_FALSE(message.isEmpty()) << "Agrupar needs two or more pieces";
    EXPECT_TRUE(undo.isClean());
}

TEST_F(PieceToolTest, RmbWithNothingSelectedDoesNotAsk) {
    tool->ungroupAction()->trigger();
    click(*view, screenOf(3, 0, 1.5), Qt::RightButton);
    EXPECT_TRUE(questions.isEmpty());
}

TEST_F(PieceToolTest, DesagruparAsksAndUngroups) {
    ASSERT_EQ(cc::core::groupPieces(project, undo, {1, 3}), cc::core::PieceError::None);
    undo.setClean();
    tool->ungroupAction()->trigger();
    EXPECT_EQ(status, QStringLiteral("Barras - Desagrupar |  - LMB: Seleccione un elemento."));
    click(*view, screenOf(1.5, 0, 0));
    click(*view, screenOf(3, 0, 1.5), Qt::RightButton);
    ASSERT_EQ(questions.size(), 1u);
    EXPECT_EQ(questions.front(), QStringLiteral("¿Desea desagrupar las piezas seleccionadas?"));
    EXPECT_FALSE(undo.isClean());
    undo.undo();
    EXPECT_TRUE(undo.isClean());
}

TEST_F(PieceToolTest, DesagruparWithoutGroupReportsNothingToChange) {
    tool->ungroupAction()->trigger();
    click(*view, screenOf(1.5, 0, 0));
    click(*view, screenOf(3, 0, 1.5), Qt::RightButton);
    EXPECT_FALSE(message.isEmpty());
    EXPECT_TRUE(undo.isClean());
}

TEST_F(PieceToolTest, InvertirEjeEveryClickInvertsABar) {
    tool->invertAction()->trigger();
    EXPECT_EQ(status, QStringLiteral("Barras - Invertir eje 'x' |  - LMB: Seleccione un elemento."));
    click(*view, screenOf(0, 0, 1.5));  // bar 3: N1 -> N4
    EXPECT_EQ(bar(3).startNode, 4);
    EXPECT_EQ(bar(3).endNode, 1);
    EXPECT_TRUE(questions.isEmpty()) << "no confirmation";
    click(*view, screenOf(0, 0, 1.5));  // the same bar again goes back (the selection was consumed)
    EXPECT_EQ(bar(3).startNode, 1);
    EXPECT_EQ(tool->mode(), Mode::InvertAxis);
    undo.undo();
    EXPECT_EQ(bar(3).startNode, 4);
    undo.undo();
    EXPECT_EQ(bar(3).startNode, 1);
}

TEST_F(PieceToolTest, MoverExtremoThreeStepsMovesTheChosenEndToAnExistingNode) {
    tool->moveEndAction()->trigger();
    EXPECT_EQ(status, QStringLiteral("Barras - Mover extremo |  - LMB: Seleccione un elemento."));
    EXPECT_EQ(view->selectionFilter().bars, true);
    EXPECT_EQ(view->selectionFilter().nodes, false);
    click(*view, screenOf(0, 0, 1.5));  // bar 3: N1 - N4
    EXPECT_EQ(tool->moveStep(), 1);
    EXPECT_TRUE(view->selectionFilter().nodes);
    EXPECT_FALSE(view->selectionFilter().bars);
    EXPECT_EQ(status, QStringLiteral("Barras - Mover extremo |  - LMB: Seleccione un elemento. - RMB: Volver al paso anterior."));
    click(*view, screenOf(6, 0, 3));  // not a node of the bar: ignored
    EXPECT_EQ(tool->moveStep(), 1);
    click(*view, screenOf(0, 0, 3));  // N4: the end that moves
    EXPECT_EQ(tool->moveStep(), 2);
    click(*view, screenOf(6, 0, 3));  // destination N5
    EXPECT_TRUE(message.isEmpty());
    EXPECT_EQ(bar(3).startNode, 1);
    EXPECT_EQ(bar(3).endNode, 5);
    EXPECT_EQ(tool->moveStep(), 0);
    EXPECT_EQ(tool->mode(), Mode::MoveEnd);
    undo.undo();
    EXPECT_EQ(bar(3).endNode, 4);
}

TEST_F(PieceToolTest, MoverExtremoRefusalKeepsWaitingAndRmbGoesBack) {
    tool->moveEndAction()->trigger();
    click(*view, screenOf(0, 0, 1.5));
    click(*view, screenOf(0, 0, 3));
    click(*view, screenOf(0, 0, 0));  // N1, the other end: refused (InvalidMove)
    EXPECT_FALSE(message.isEmpty());
    EXPECT_EQ(tool->moveStep(), 2) << "the command waits for another destination";
    EXPECT_TRUE(undo.isClean());
    click(*view, screenOf(3, 0, 1.5), Qt::RightButton);
    EXPECT_EQ(tool->moveStep(), 0);
    EXPECT_EQ(tool->mode(), Mode::MoveEnd);
    EXPECT_TRUE(view->selectionFilter().bars);
}

TEST_F(PieceToolTest, UndoOfTheChosenBarDropsTheSteps) {
    tool->moveEndAction()->trigger();
    click(*view, screenOf(0, 0, 1.5));
    ASSERT_EQ(tool->moveStep(), 1);
    undo.undo();  // removes the last bar added (bar 4), not bar 3: steps stay
    EXPECT_EQ(tool->moveStep(), 1);
    undo.undo();  // bar 3 gone
    EXPECT_EQ(tool->moveStep(), 0);
}

TEST_F(PieceToolTest, ExclusiveWithTheBarTool) {
    cc::app::BarTool barTool;
    barTool.setProject(&project, &undo);
    barTool.addViewport(view.get());
    QObject::connect(&barTool, &cc::app::BarTool::aboutToActivate, [this] { tool->deactivate(); });
    QObject::connect(tool.get(), &PieceTool::aboutToActivate, [&barTool] { barTool.deactivate(); });
    barTool.barAction()->trigger();
    tool->invertAction()->trigger();
    EXPECT_EQ(barTool.mode(), cc::app::BarTool::Mode::None);
    EXPECT_EQ(tool->mode(), Mode::InvertAxis);
    EXPECT_TRUE(view->selectionMode()) << "the piece tool configured the viewport last";
    barTool.beamAction()->trigger();
    EXPECT_EQ(tool->mode(), Mode::None);
    EXPECT_FALSE(tool->invertAction()->isChecked());
}

int main(int argc, char** argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
