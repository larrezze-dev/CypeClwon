// Geometria > Herramientas (T-P5-10, GUI part): the TransformTool commands, Reparar and the "Generar" entries.
// Never shown on screen (offscreen).
#include <gtest/gtest.h>

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QDoubleSpinBox>
#include <QKeyEvent>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QMouseEvent>
#include <QSignalSpy>
#include <QSpinBox>
#include <memory>

#include "TransformTool.h"
#include "Viewport3D.h"

using cc::app::TransformTool;
using cc::app::Viewport3D;
using Mode = cc::app::TransformTool::Mode;
using namespace cc::core;

namespace {
QApplication* app() {
    static int argc = 1;
    static char name[] = "transform_tool_tests";
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

/// Front view: N1 (0,0,0), N2 (4,0,0), N3 (4,0,3), N4 (0,0,3) with bars B1 N1-N2 and B2 N2-N3, and a lone node N5 (0,0,6).
class TransformToolTest : public ::testing::Test {
protected:
    void SetUp() override {
        app();
        auto& model = project.structure;
        model.addNode({0, 0, 0});
        model.addNode({4, 0, 0});
        model.addNode({4, 0, 3});
        model.addNode({0, 0, 3});
        model.addNode({0, 0, 6});
        model.addBar(1, 2);
        model.addBar(2, 3);
        view = std::make_unique<Viewport3D>();
        view->resize(800, 600);
        view->setStandardView(cc::core::StandardView::Front);
        view->setOrthographic(true);
        view->camera().target = {3, 0, 3};
        view->setStructure(&model);
        tool = std::make_unique<TransformTool>();
        tool->setProject(&project, &undo);
        tool->addViewport(view.get());
        undo.setChangeCallback([this] { tool->structureChanged(); });
        QObject::connect(tool.get(), &TransformTool::statusChanged, [this](const QString& text) { status = text; });
        QObject::connect(tool.get(), &TransformTool::messageRequested, [this](const QString& text) { message = text; });
        tool->setDialogRunner([this](QDialog& dialog) {
            ++dialogs;
            if (auto* angle = dialog.findChild<QDoubleSpinBox*>()) angle->setValue(angleToSet);
            if (auto* copies = dialog.findChild<QSpinBox*>()) copies->setValue(copiesToSet);
            if (auto* label = dialog.findChild<QLabel*>()) lastLabel = label->text();
            if (auto* list = dialog.findChild<QListWidget*>())
                for (int i = 0; i < list->count(); ++i) lastList << list->item(i)->text();
            return static_cast<int>(accept ? QDialog::Accepted : QDialog::Rejected);
        });
        undo.setClean();
    }
    QPointF nodeScreen(int id) {
        const Node& n = *project.structure.findNode(id);
        QPointF p;
        EXPECT_TRUE(view->worldToScreen({n.position.x, n.position.y, n.position.z}, p));
        return p;
    }
    Point3 pos(int id) { return project.structure.findNode(id)->position; }
    Project project;
    UndoStack undo;
    std::unique_ptr<Viewport3D> view;
    std::unique_ptr<TransformTool> tool;
    QString status, message, lastLabel;
    QStringList lastList, questions;
    int dialogs = 0;
    double angleToSet = 90.0;
    int copiesToSet = 1;
    bool accept = true;
    bool answer = true;
};
}  // namespace

TEST_F(TransformToolTest, SevenModesInAnExclusiveOptionalGroup) {
    EXPECT_EQ(tool->moveAction()->text(), QStringLiteral("Mover elementos"));
    EXPECT_EQ(tool->copyAction()->text(), QStringLiteral("Copiar elementos"));
    EXPECT_EQ(tool->rotateAxisAction()->text(), QStringLiteral("Girar elementos alrededor de un eje definido por dos puntos"));
    EXPECT_EQ(tool->mirrorPlaneAction()->text(), QStringLiteral("Simetría de elementos respecto de un plano"));
    EXPECT_EQ(tool->actionGroup()->actions().size(), 7);
    EXPECT_EQ(tool->actionGroup()->exclusionPolicy(), QActionGroup::ExclusionPolicy::ExclusiveOptional);
    EXPECT_TRUE(tool->copyAction()->toolTip().contains(QStringLiteral("origen del desplazamiento y su magnitud")));
    tool->moveAction()->trigger();
    EXPECT_EQ(tool->mode(), Mode::Move);
    tool->copyAction()->trigger();
    EXPECT_FALSE(tool->moveAction()->isChecked());
    EXPECT_EQ(tool->mode(), Mode::Copy);
    tool->copyAction()->trigger();
    EXPECT_EQ(tool->mode(), Mode::None);
}

TEST_F(TransformToolTest, GreyOnAnEmptyWorkAsInTheOriginal) {
    TransformTool other;
    EXPECT_FALSE(other.moveAction()->isEnabled());
    EXPECT_FALSE(other.repairAction()->isEnabled());
    Project empty;
    UndoStack stack;
    other.setProject(&empty, &stack);
    EXPECT_FALSE(other.moveAction()->isEnabled());
    EXPECT_FALSE(other.generateMenu()->isEnabled());
    other.setProject(&project, &undo);
    EXPECT_TRUE(other.moveAction()->isEnabled());
    EXPECT_TRUE(other.repairAction()->isEnabled());
    EXPECT_TRUE(other.generateMenu()->isEnabled());
}

TEST_F(TransformToolTest, GenerarHasFiveEntriesThatOnlyReportNotAvailable) {
    ASSERT_EQ(tool->generateActions().size(), 5);
    EXPECT_EQ(tool->generateActions().at(0)->text(), QStringLiteral("Generación de una malla de nudos tridimensional"));
    QSignalSpy pending(tool.get(), &TransformTool::notAvailable);
    for (QAction* action : tool->generateActions()) action->trigger();
    EXPECT_EQ(pending.count(), 5);
    EXPECT_EQ(dialogs, 0);
    EXPECT_EQ(tool->mode(), Mode::None);
}

TEST_F(TransformToolTest, ActivatingAnnouncesItselfAndEscapeStops) {
    QSignalSpy activating(tool.get(), &TransformTool::aboutToActivate);
    tool->moveAction()->trigger();
    EXPECT_EQ(activating.count(), 1);
    EXPECT_TRUE(view->selectionMode());
    EXPECT_EQ(status, QStringLiteral("Herramientas - Mover elementos |  - LMB: Seleccione un elemento."));
    key(*view, Qt::Key_Escape);
    EXPECT_EQ(tool->mode(), Mode::None);
    EXPECT_FALSE(view->selectionMode());
    EXPECT_FALSE(tool->moveAction()->isChecked());
}

TEST_F(TransformToolTest, MoverSelectThenTwoPoints) {
    tool->moveAction()->trigger();
    click(*view, nodeScreen(5));
    click(*view, nodeScreen(5), Qt::RightButton);
    EXPECT_EQ(tool->step(), 1);
    EXPECT_TRUE(view->snapMode());
    click(*view, nodeScreen(1));
    EXPECT_EQ(tool->points().size(), 1u);
    click(*view, nodeScreen(4));  // vector (0, 0, 3)
    EXPECT_DOUBLE_EQ(pos(5).z, 9.0);
    EXPECT_EQ(tool->step(), 0);
    EXPECT_EQ(tool->mode(), Mode::Move) << "the command goes on";
    undo.undo();
    EXPECT_DOUBLE_EQ(pos(5).z, 6.0);
}

TEST_F(TransformToolTest, RightClickGoesBackOneStepAtATime) {
    tool->moveAction()->trigger();
    click(*view, nodeScreen(5));
    click(*view, nodeScreen(5), Qt::RightButton);
    click(*view, nodeScreen(1));
    click(*view, nodeScreen(1), Qt::RightButton);
    EXPECT_TRUE(tool->points().empty());
    EXPECT_EQ(tool->step(), 1);
    click(*view, nodeScreen(1), Qt::RightButton);
    EXPECT_EQ(tool->step(), 0);
    click(*view, nodeScreen(1), Qt::RightButton);  // selecting mode, empty selection: nothing
    EXPECT_EQ(tool->step(), 0);
}

TEST_F(TransformToolTest, CopiarAsksForTheNumberOfCopies) {
    copiesToSet = 2;
    tool->copyAction()->trigger();
    click(*view, nodeScreen(5));
    click(*view, nodeScreen(5), Qt::RightButton);
    click(*view, nodeScreen(1));
    click(*view, nodeScreen(4));
    EXPECT_EQ(dialogs, 1);
    ASSERT_EQ(project.structure.nodes.size(), 7u);
    EXPECT_DOUBLE_EQ(project.structure.nodes[5].position.z, 9.0);
    EXPECT_DOUBLE_EQ(project.structure.nodes[6].position.z, 12.0);
}

TEST_F(TransformToolTest, CancellingTheDialogChangesNothing) {
    accept = false;
    tool->copyAction()->trigger();
    click(*view, nodeScreen(5));
    click(*view, nodeScreen(5), Qt::RightButton);
    click(*view, nodeScreen(1));
    click(*view, nodeScreen(4));
    EXPECT_EQ(dialogs, 1);
    EXPECT_EQ(project.structure.nodes.size(), 5u);
    EXPECT_EQ(tool->step(), 1) << "waits for other points";
    EXPECT_TRUE(tool->points().empty());
}

TEST_F(TransformToolTest, GirarAlrededorDeUnEjeAsksForTheAngle) {
    ToolSelection selection;
    selection.nodeIds = {2};
    angleToSet = 90.0;
    // axis: the vertical line through the origin; N2 (4, 0, 0) goes to (0, 4, 0)
    EXPECT_TRUE(tool->apply(Mode::RotateAxis, selection, {{0, 0, 0}, {0, 0, 1}}));
    EXPECT_EQ(dialogs, 1);
    EXPECT_NEAR(pos(2).x, 0.0, 1e-9);
    EXPECT_NEAR(pos(2).y, 4.0, 1e-9);
}

TEST_F(TransformToolTest, GirarEnElPlanoXyBothVariants) {
    project.structure.addNode({5, 0, 8});  // N6
    ToolSelection selection;
    selection.nodeIds = {6};
    EXPECT_TRUE(tool->apply(Mode::RotateXY, selection, {{0, 0, 0}, {0, 3, 0}}));  // 90 degrees about the vertical through the origin
    EXPECT_NEAR(pos(6).x, 0.0, 1e-9);
    EXPECT_NEAR(pos(6).y, 5.0, 1e-9);
    EXPECT_DOUBLE_EQ(pos(6).z, 8.0);
    // origin, then the angle at the second point from (second -> first) = +X to (second -> third) = +Y: 90 degrees
    EXPECT_TRUE(tool->apply(Mode::RotateXY3, selection, {{0, 0, 0}, {1, 0, 0}, {0, 0, 0}, {0, 1, 0}}));
    EXPECT_NEAR(pos(6).x, -5.0, 1e-9);
    EXPECT_NEAR(pos(6).y, 0.0, 1e-9);
    EXPECT_EQ(dialogs, 0);
    message.clear();
    EXPECT_FALSE(tool->apply(Mode::RotateXY, selection, {{1, 1, 0}, {1, 1, 5}}));  // the vector is vertical: no angle
    EXPECT_FALSE(message.isEmpty());
}

TEST_F(TransformToolTest, SimetriaCopiesMirrorImage) {
    ToolSelection selection;
    selection.nodeIds = {5};
    EXPECT_TRUE(tool->apply(Mode::MirrorPlane, selection, {{2, 0, 0}, {2, 1, 0}, {2, 0, 1}}));  // plane x = 2
    ASSERT_EQ(project.structure.nodes.size(), 6u);
    EXPECT_DOUBLE_EQ(project.structure.nodes[5].position.x, 4.0);
    EXPECT_TRUE(tool->apply(Mode::MirrorXY, selection, {{0, 3, 0}, {1, 3, 0}}));  // vertical plane y = 3 containing the line
    ASSERT_EQ(project.structure.nodes.size(), 7u);
    EXPECT_DOUBLE_EQ(project.structure.nodes[6].position.y, 6.0);
    message.clear();
    EXPECT_FALSE(tool->apply(Mode::MirrorPlane, selection, {{0, 0, 0}, {1, 0, 0}, {2, 0, 0}}));  // collinear
    EXPECT_FALSE(message.isEmpty());
}

TEST_F(TransformToolTest, MirrorViaTheGestureReportsWhenNothingIsCopied) {
    tool->mirrorPlaneAction()->trigger();
    click(*view, nodeScreen(5));
    click(*view, nodeScreen(5), Qt::RightButton);
    // plane through N1, N2 and N4: the plane y = 0 (the view plane) holds N5, whose mirror image is itself
    click(*view, nodeScreen(1));
    click(*view, nodeScreen(2));
    click(*view, nodeScreen(4));
    EXPECT_EQ(message, QString::fromStdString(toolErrorMessage(ToolError::NothingCopied)));
}

TEST_F(TransformToolTest, ReparaConfirmsMergesAndListsIncidences) {
    project.structure.addNode({4, 0, 3});  // N6 on top of N3
    tool->setConfirmer([this](const QString& question) {
        questions << question;
        return answer;
    });
    answer = false;
    EXPECT_FALSE(tool->repair());
    ASSERT_EQ(questions.size(), 1);
    EXPECT_EQ(questions.front(), QString::fromStdString(repairConfirmationMessage()));
    EXPECT_EQ(dialogs, 0) << "declined: nothing else happens";
    EXPECT_EQ(project.structure.nodes.size(), 6u);
    answer = true;
    tool->repairAction()->trigger();
    EXPECT_EQ(dialogs, 1);
    EXPECT_EQ(lastLabel, QString::fromStdString(repairResultMessage(true)));
    EXPECT_FALSE(lastList.isEmpty());
    EXPECT_EQ(project.structure.nodes.size(), 5u);
    lastList.clear();
    tool->repairAction()->trigger();  // second time: nothing to do
    EXPECT_EQ(lastLabel, QString::fromStdString(repairResultMessage(false)));
}

TEST_F(TransformToolTest, WithoutConfirmerReparaAssumesYes) {
    tool->repairAction()->trigger();
    EXPECT_EQ(dialogs, 1);
    EXPECT_EQ(lastLabel, QString::fromStdString(repairResultMessage(false)));
}

int main(int argc, char** argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
