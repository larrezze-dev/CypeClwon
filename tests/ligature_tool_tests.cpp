// Propiedades > Nudos > Ligaduras (T-P6-02, GUI part): panel, tool and 3D texts. Expected values: docs/research/ui/ligaduras.md section 7
// (measured in the original). Never shown on screen (offscreen).
#include <gtest/gtest.h>

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QImage>
#include <QMouseEvent>
#include <QRadioButton>
#include <memory>

#include "LigatureTool.h"
#include "Viewport3D.h"
#include "core/node_ligature_edit.h"
#include "core/node_support_edit.h"

using namespace cc::core;
using cc::app::LigaturePanel;
using cc::app::LigatureTool;
using cc::app::Viewport3D;

namespace {

QApplication* app() {
    static int argc = 1;
    static char name[] = "ligature_tool_tests";
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

/// Four nodes along X seen from the front: N1 (0,0,0) .. N4 (6,0,0).
class LigatureToolTest : public ::testing::Test {
protected:
    void SetUp() override {
        app();
        for (int i = 0; i < 4; ++i) project.structure.addNode({2.0 * i, 0, 0});
        view = std::make_unique<Viewport3D>();
        view->resize(800, 600);
        view->setStandardView(cc::core::StandardView::Front);
        view->setOrthographic(true);
        view->camera().target = {3, 0, 0};
        view->setStructure(&project.structure);
        view->setNodeSupports(&project.nodeSupports);
        tool = std::make_unique<LigatureTool>();
        tool->setProject(&project, &undo);
        tool->addViewport(view.get());
        undo.setChangeCallback([this] { tool->structureChanged(); });
        QObject::connect(tool.get(), &LigatureTool::statusChanged, [this](const QString& text) { status = text; });
        QObject::connect(tool.get(), &LigatureTool::errorRequested, [this](const QString& text) { errors << text; });
        undo.setClean();
    }
    QPointF screenOf(int id) {
        const Node& n = *project.structure.findNode(id);
        QPointF p;
        EXPECT_TRUE(view->worldToScreen({n.position.x, n.position.y, n.position.z}, p));
        return p;
    }
    /// Window of two clicks around the given nodes' screen positions (above and below the line, so that nothing is picked at the corners).
    void selectWindow(int first, int last) {
        const QPointF a = screenOf(first), b = screenOf(last);
        click(*view, QPointF(a.x() - 20, a.y() - 40));
        click(*view, QPointF(b.x() + 20, b.y() + 40));
    }
    Project project;
    UndoStack undo;
    std::unique_ptr<Viewport3D> view;
    std::unique_ptr<LigatureTool> tool;
    QString status;
    QStringList errors;
};

}  // namespace

TEST_F(LigatureToolTest, ButtonTooltipAndStatusText) {
    EXPECT_EQ(tool->action()->text(), QStringLiteral("Ligaduras"));
    EXPECT_EQ(tool->action()->toolTip(),
              QStringLiteral("Permite asignar o eliminar ligaduras de nudos para desplazamientos en los ejes globales X, Y y Z."));
    LigatureTool other;
    EXPECT_FALSE(other.action()->isEnabled()) << "no project";
    tool->action()->trigger();
    EXPECT_EQ(tool->mode(), LigatureTool::Mode::Active);
    EXPECT_EQ(status, QStringLiteral("Nudos - Ligaduras |  - LMB: Seleccione un grupo de elementos."));
    EXPECT_TRUE(view->selectionMode());
    EXPECT_TRUE(view->ligatureLabelsVisible());
    EXPECT_TRUE(view->nodeLabelsVisible());
    tool->action()->trigger();
    EXPECT_EQ(tool->mode(), LigatureTool::Mode::None);
    EXPECT_FALSE(view->selectionMode());
    EXPECT_FALSE(view->ligatureLabelsVisible());
}

TEST_F(LigatureToolTest, PanelTextsAndInitialState) {
    tool->action()->trigger();
    LigaturePanel* panel = tool->panel();
    ASSERT_NE(panel, nullptr);
    EXPECT_EQ(panel->windowTitle(), QStringLiteral("Propiedades - Ligaduras"));
    EXPECT_EQ(panel->assignRadio()->text(), QStringLiteral("Asignar"));
    EXPECT_EQ(panel->removeRadio()->text(), QStringLiteral("Eliminar"));
    EXPECT_TRUE(panel->assignRadio()->isChecked());
    EXPECT_EQ(panel->check(true, 0)->text(), QStringLiteral("Desplazamiento en X"));
    EXPECT_EQ(panel->check(true, 1)->text(), QStringLiteral("Desplazamiento en Y"));
    EXPECT_EQ(panel->check(true, 2)->text(), QStringLiteral("Desplazamiento en Z"));
    for (int set = 0; set < 2; ++set)
        for (int dir = 0; dir < 3; ++dir) EXPECT_FALSE(panel->check(set == 0, dir)->isChecked()) << "all unchecked at the start";
    // Each radio has its own set of checkboxes.
    panel->check(true, 0)->setChecked(true);
    panel->removeRadio()->setChecked(true);
    EXPECT_EQ(panel->directions(), (LigatureDirections{false, false, false}));
    panel->assignRadio()->setChecked(true);
    EXPECT_EQ(panel->directions(), (LigatureDirections{true, false, false}));
}

TEST_F(LigatureToolTest, RightClickAssignsAsOneUndoStepAndEmptiesTheSelection) {
    tool->action()->trigger();
    tool->panel()->check(true, 0)->setChecked(true);
    selectWindow(1, 2);
    EXPECT_EQ(view->selection().ids(ElementKind::Node), (std::vector<int>{1, 2}));
    click(*view, QPointF(400, 500), Qt::RightButton);
    EXPECT_EQ(undo.count(), 1u);
    EXPECT_EQ(ligatureLabel(project.ligatures, 1), "Dx(1)");
    EXPECT_EQ(ligatureLabel(project.ligatures, 2), "Dx(1)");
    EXPECT_TRUE(view->selection().empty());
    undo.undo();
    EXPECT_TRUE(project.ligatures.empty());
    undo.redo();
    EXPECT_FALSE(project.ligatures.empty());
}

TEST_F(LigatureToolTest, ErrorsAreAnnouncedAndTheSelectionIsKept) {
    tool->action()->trigger();
    EXPECT_FALSE(tool->execute({1, 2})) << "no direction ticked";
    ASSERT_EQ(errors.size(), 1);
    EXPECT_EQ(errors.front(), QStringLiteral("No se ha seleccionado el tipo de ligadura a establecer."));
    tool->panel()->check(true, 2)->setChecked(true);
    EXPECT_TRUE(tool->execute({1, 2}));
    EXPECT_FALSE(tool->execute({1, 2}));
    ASSERT_EQ(errors.size(), 2);
    EXPECT_EQ(errors.back(), QStringLiteral("Los nudos seleccionados ya pertenecen a la misma ligadura."));
    EXPECT_EQ(undo.count(), 1u);
    // Through the viewport the selection survives an error.
    selectWindow(1, 2);
    click(*view, QPointF(400, 500), Qt::RightButton);
    EXPECT_EQ(errors.size(), 3);
    EXPECT_EQ(view->selection().size(), 2u);
    // Fewer than two nodes: silent.
    EXPECT_FALSE(tool->execute({3}));
    EXPECT_EQ(errors.size(), 3);
}

TEST_F(LigatureToolTest, RemoveWithoutDirectionsClearsEveryTie) {
    ASSERT_EQ(assignLigatures(project, undo, {1, 2}, {true, false, false}).status, LigatureStatus::Ok);
    ASSERT_EQ(assignLigatures(project, undo, {3, 4}, {true, false, false}).status, LigatureStatus::Ok);
    tool->action()->trigger();
    tool->panel()->removeRadio()->setChecked(true);
    EXPECT_TRUE(tool->execute({1}));
    EXPECT_TRUE(project.ligatures.empty()) << "measured: Eliminar without any box removes every tie";
}

TEST_F(LigatureToolTest, RemoveWithDirectionLeavesTheOthers) {
    ASSERT_EQ(assignLigatures(project, undo, {1, 2, 3}, {true, false, false}).status, LigatureStatus::Ok);
    tool->action()->trigger();
    tool->panel()->removeRadio()->setChecked(true);
    tool->panel()->check(false, 0)->setChecked(true);
    EXPECT_TRUE(tool->execute({1}));
    EXPECT_EQ(ligatureLabel(project.ligatures, 1), "");
    EXPECT_EQ(ligatureLabel(project.ligatures, 2), "Dx(1)");
    EXPECT_EQ(ligatureLabel(project.ligatures, 3), "Dx(1)");
}

TEST_F(LigatureToolTest, NodesOnALineOrPlaneRestraintAreNotCaptured) {
    ExternalSupport support;
    support.type = ExternalSupportType::LineXFixed;
    ASSERT_EQ(assignExternalSupport(project, undo, {2}, support), 1);
    tool->action()->trigger();
    selectWindow(1, 3);
    EXPECT_EQ(view->selection().ids(ElementKind::Node), (std::vector<int>{1, 3})) << "N2 is silently skipped";
    click(*view, screenOf(2));
    EXPECT_EQ(view->selection().ids(ElementKind::Node), (std::vector<int>{1, 3})) << "a click on N2 does nothing";
}

TEST_F(LigatureToolTest, TiedNodesAreWrittenInTealWithTheirTies) {
    ASSERT_EQ(assignLigatures(project, undo, {1, 2}, {true, false, false}).status, LigatureStatus::Ok);
    tool->action()->trigger();
    selectWindow(3, 4);
    view->clearSelection();
    // The colour itself (0, 160, 176) is checked on the clone screenshot; here the state that drives it.
    EXPECT_TRUE(view->ligatureLabelsVisible());
    ASSERT_NE(view->structure(), nullptr);
    EXPECT_EQ(ligatureLabel(project.ligatures, 1), "Dx(1)");
}

TEST_F(LigatureToolTest, ChangingToolOrProjectEndsTheCommand) {
    tool->action()->trigger();
    tool->setProject(nullptr, nullptr);
    EXPECT_EQ(tool->mode(), LigatureTool::Mode::None);
    EXPECT_FALSE(tool->action()->isEnabled());
    EXPECT_EQ(view->nodeLigatures(), nullptr);
}

int main(int argc, char** argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    app();
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
