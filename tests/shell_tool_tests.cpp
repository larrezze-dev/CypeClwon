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
#include <QCheckBox>
#include <QComboBox>
#include <QRadioButton>
#include <QToolButton>
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
        tool->setDialogRunner([this](QDialog& d) {
            if (dynamic_cast<cc::app::ShellNewDialog*>(&d)) return static_cast<int>(QDialog::Accepted);  // "Nueva": accept the defaults
            ++dialogs;
            return static_cast<int>(QDialog::Rejected);
        });
        cc::app::RangedEdit::setWarningHandler([this](QWidget*, const QString& text) { warnings << text; });
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
    QStringList warnings;
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
    EXPECT_EQ(status, QStringLiteral("Láminas - Nueva |  - LMB: Introduzca los puntos de la polilínea."));
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

TEST_F(ShellToolTest, NuevaOpensTheDialogFirstAndCancellingReleasesTheButton) {
    int newDialogs = 0;
    tool->setDialogRunner([&](QDialog& d) {
        if (dynamic_cast<cc::app::ShellNewDialog*>(&d)) ++newDialogs;
        EXPECT_EQ(d.windowTitle(), QStringLiteral("Nueva"));
        return static_cast<int>(QDialog::Rejected);
    });
    tool->newAction()->setChecked(true);
    EXPECT_EQ(newDialogs, 1);
    EXPECT_EQ(tool->mode(), Mode::None) << "cancelled: no points are asked";
    EXPECT_FALSE(tool->newAction()->isChecked());
    EXPECT_FALSE(view->snapMode());
}

TEST_F(ShellToolTest, NuevaGivesTheDialogValuesToTheShellsItCreates) {
    tool->setDialogRunner([&](QDialog& d) {
        auto& dialog = static_cast<cc::app::ShellNewDialog&>(d);
        // Measured defaults: 100 mm (always shown in mm), ballast off, concrete from the general data.
        EXPECT_NEAR(dialog.section().thicknessM, 0.1, 1e-12);
        EXPECT_FALSE(dialog.section().ballastEnabled);
        EXPECT_EQ(dialog.material().kind, ShellMaterialKind::Concrete);
        EXPECT_TRUE(dialog.material().useGeneralData);
        dialog.sectionPanel()->setThickness(300.0);  // always mm
        dialog.materialPanel()->setKind(ShellMaterialKind::GenericIsotropic);
        return static_cast<int>(QDialog::Accepted);
    });
    tool->newAction()->trigger();
    EXPECT_EQ(tool->mode(), Mode::New);
    EXPECT_NEAR(tool->newSection().thicknessM, 0.3, 1e-12);
    ASSERT_TRUE(tool->createShell({{4, 0, 3}, {0, 0, 3}, {0, 0, 6}}));
    const Shell& created = project.structure.shells.back();
    EXPECT_NEAR(created.thicknessM, 0.3, 1e-12);
    EXPECT_EQ(created.material.kind, ShellMaterialKind::GenericIsotropic);
    EXPECT_NEAR(project.structure.shells.front().thicknessM, kDefaultShellThicknessM, 1e-12) << "the other shells keep their values";
}

TEST_F(ShellToolTest, MaterialAppliesTheDialogToTheSelectedShellsAsOneUndoStep) {
    std::string title;
    tool->setDialogRunner([&](QDialog& d) {
        auto& dialog = static_cast<cc::app::ShellMaterialDialog&>(d);
        title = d.windowTitle().toStdString();
        EXPECT_EQ(dialog.material().kind, ShellMaterialKind::Concrete);
        dialog.panel()->setKind(ShellMaterialKind::Aluminium);
        dialog.panel()->ownRadio()->setChecked(true);
        dialog.panel()->alloyCombo()->setCurrentText(QStringLiteral("EN AW-6061"));
        return static_cast<int>(QDialog::Accepted);
    });
    const std::size_t before = undo.count();
    tool->materialAction()->trigger();
    EXPECT_EQ(tool->mode(), Mode::Material);
    click(*view, screenOf(1, 0, 1.2));
    click(*view, screenOf(1, 0, 1.2), Qt::RightButton);
    EXPECT_EQ(title, "Describir material");
    EXPECT_EQ(undo.count(), before + 1);
    EXPECT_EQ(project.structure.findShell(1)->material.kind, ShellMaterialKind::Aluminium);
    EXPECT_EQ(project.structure.findShell(1)->material.alloy, "EN AW-6061");
    EXPECT_EQ(project.structure.findShell(2)->material.kind, ShellMaterialKind::Concrete);
    undo.undo();
    EXPECT_EQ(project.structure.findShell(1)->material.kind, ShellMaterialKind::Concrete);
}

TEST_F(ShellToolTest, DisposicionAppliesTheLayoutAndCancelChangesNothing) {
    const std::size_t before = undo.count();
    tool->layoutAction()->trigger();
    EXPECT_EQ(tool->mode(), Mode::Layout);
    click(*view, screenOf(1, 0, 1.2));
    click(*view, screenOf(1, 0, 1.2), Qt::RightButton);
    EXPECT_EQ(dialogs, 1) << "the default runner cancels";
    EXPECT_EQ(undo.count(), before);
    tool->setDialogRunner([&](QDialog& d) {
        auto& dialog = static_cast<cc::app::ShellLayoutDialog&>(d);
        EXPECT_EQ(d.windowTitle(), QStringLiteral("Describir disposición"));
        EXPECT_TRUE(dialog.layout().considerEccentricity);
        dialog.eccentricityBox()->setChecked(false);
        dialog.offsetEdit()->setText(QStringLiteral("-40"));
        dialog.offsetEdit()->commit();
        return static_cast<int>(QDialog::Accepted);
    });
    ASSERT_TRUE(tool->editLayout({1, 2}));
    EXPECT_EQ(undo.count(), before + 1);
    for (const Shell& sh : project.structure.shells) {
        EXPECT_FALSE(sh.layout.considerEccentricity);
        EXPECT_NEAR(sh.layout.centralPlaneOffsetM, -0.04, 1e-12);
    }
    undo.undo();
    for (const Shell& sh : project.structure.shells) EXPECT_TRUE(sh.layout.considerEccentricity);
}

TEST_F(ShellToolTest, SectionPanelValidatesWithTheMeasuredWarnings) {
    cc::app::ShellSectionPanel panel(ShellSection{}, LengthUnit::Millimetres);
    panel.thicknessEdit()->setText(QStringLiteral("20000"));
    EXPECT_FALSE(panel.validate());
    ASSERT_EQ(warnings.size(), 1);
    EXPECT_EQ(warnings.front(), QStringLiteral("El valor de 'Espesor' debe estar entre 0.10 y 10000.00."));
    EXPECT_EQ(panel.thicknessEdit()->text(), QStringLiteral("100.00")) << "the previous value is recovered";
    EXPECT_EQ(panel.thicknessEdit()->toolTip(), QStringLiteral("Debe estar entre 0.10 y 10000.00."));
    panel.setBallastEnabled(true);
    EXPECT_TRUE(panel.behaviourEditable());
    EXPECT_EQ(panel.ballastEdit()->text(), QStringLiteral("29430.00"));
    panel.ballastEdit()->setText(QStringLiteral("0.001"));
    EXPECT_FALSE(panel.validate());
    EXPECT_EQ(warnings.back(), QStringLiteral("El valor de 'Módulo de balasto en Z local' debe estar entre 0.01 y 1000000.00."));
    EXPECT_EQ(panel.behaviourCombo()->itemText(0), QStringLiteral("Lineal"));
    EXPECT_EQ(panel.behaviourCombo()->itemText(1), QStringLiteral("Sólo compresión"));
    panel.ballastEdit()->setText(QStringLiteral("5000"));
    panel.thicknessEdit()->setText(QStringLiteral("150"));
    EXPECT_TRUE(panel.validate());
    EXPECT_NEAR(panel.section().thicknessM, 0.15, 1e-12);
    EXPECT_DOUBLE_EQ(panel.section().ballastKnM3, 5000.0);
}

TEST_F(ShellToolTest, MaterialPanelHasTheSixTypesWithTheirDefaults) {
    cc::app::ShellMaterialPanel panel(ShellMaterial{}, project);
    EXPECT_EQ(panel.kind(), ShellMaterialKind::Concrete);
    EXPECT_EQ(panel.generalDataRadio()->text(), QStringLiteral("Material seleccionado en datos generales (C25/30)"));
    EXPECT_TRUE(panel.generalDataRadio()->isChecked());
    EXPECT_FALSE(panel.gradeCombo()->isEnabled()) << "the combo follows the second radio";
    panel.ownRadio()->setChecked(true);
    EXPECT_TRUE(panel.gradeCombo()->isEnabled());
    EXPECT_EQ(panel.gradeCombo()->currentText(), QStringLiteral("C12/15"));
    panel.setKind(ShellMaterialKind::RolledSteel);
    EXPECT_EQ(panel.generalDataRadio()->text(), QStringLiteral("Material seleccionado en datos generales (S275 (UNE-EN 10025-2))"));
    EXPECT_EQ(panel.gradeCombo()->currentText(), QStringLiteral("S275 (UNE-EN 10025-2)"));
    panel.setKind(ShellMaterialKind::ColdFormedSteel);
    ASSERT_EQ(panel.coldFormedRadios().size(), 4u);
    EXPECT_TRUE(panel.coldFormedRadios().front()->isChecked());
    EXPECT_EQ(panel.coldFormedRadios().front()->text(), QStringLiteral("S235"));
    panel.setKind(ShellMaterialKind::Aluminium);
    EXPECT_EQ(panel.alloyCombo()->currentText(), QStringLiteral("EN AW-5083"));
    EXPECT_EQ(panel.temperCombo()->currentText(), QStringLiteral("F"));
    EXPECT_EQ(panel.alloyCombo()->count(), 8);
    EXPECT_EQ(panel.temperCombo()->count(), 3);
    EXPECT_EQ(panel.material().alloy, "EN AW-5083");
    panel.setKind(ShellMaterialKind::GenericIsotropic);
    ASSERT_EQ(panel.isotropicEdits().size(), 4u);
    EXPECT_EQ(panel.isotropicEdits()[0]->text(), QStringLiteral("206010.00"));
    EXPECT_EQ(panel.isotropicEdits()[1]->text(), QStringLiteral("0.30"));
    EXPECT_EQ(panel.isotropicEdits()[2]->text(), QStringLiteral("0.000012"));
    EXPECT_EQ(panel.isotropicEdits()[3]->text(), QStringLiteral("24.53"));
    panel.isotropicEdits()[1]->setText(QStringLiteral("0.6"));
    EXPECT_FALSE(panel.validate());
    EXPECT_EQ(warnings.back(), QStringLiteral("El valor de 'Módulo de Poisson' debe estar entre 0.01 y 0.49."));
    EXPECT_EQ(panel.isotropicEdits()[1]->text(), QStringLiteral("0.30"));
    panel.setKind(ShellMaterialKind::GenericOrthotropic);
    ASSERT_EQ(panel.orthotropicEdits().size(), 9u);
    EXPECT_EQ(panel.orthotropicEdits()[0]->text(), QStringLiteral("1098.72"));
    EXPECT_EQ(panel.orthotropicEdits()[1]->text(), QStringLiteral("878.98"));
    EXPECT_EQ(panel.orthotropicEdits()[2]->text(), QStringLiteral("0.25"));
    EXPECT_EQ(panel.orthotropicEdits()[5]->text(), QStringLiteral("39.73"));
    EXPECT_EQ(panel.orthotropicEdits()[6]->text(), QStringLiteral("0.000005"));
    EXPECT_EQ(panel.orthotropicEdits()[8]->text(), QStringLiteral("14.72"));
    EXPECT_EQ(panel.orthotropicEdits()[2]->toolTip(), QStringLiteral("Debe estar entre -10000.00 y 10000.00."));
    EXPECT_TRUE(panel.validate());
    EXPECT_EQ(panel.material().kind, ShellMaterialKind::GenericOrthotropic);
    EXPECT_DOUBLE_EQ(panel.material().orthoShearModulusYZMPa, 39.73);
    // An initial material is shown as it is.
    ShellMaterial initial;
    initial.kind = ShellMaterialKind::Aluminium;
    initial.useGeneralData = false;
    initial.alloy = "EN AW-7020";
    initial.temper = "H112";
    cc::app::ShellMaterialPanel again(initial, project);
    EXPECT_EQ(again.kind(), ShellMaterialKind::Aluminium);
    EXPECT_EQ(again.material(), initial);
}

TEST_F(ShellToolTest, LayoutDialogHasThreeGroupsSevenIconsAndTheMeasuredRange) {
    cc::app::ShellLayoutDialog dialog(ShellLayout{}, 0.1);
    EXPECT_TRUE(dialog.eccentricityBox()->isChecked());
    EXPECT_EQ(dialog.offsetEdit()->text(), QStringLiteral("0"));
    EXPECT_EQ(dialog.viewerButtons().size(), 7u);
    dialog.offsetEdit()->setText(QStringLiteral("6000"));
    EXPECT_FALSE(dialog.offsetEdit()->commit());
    EXPECT_EQ(warnings.back(), QStringLiteral("El valor de 'Z' debe estar entre -5000 y 5000."));
    EXPECT_EQ(dialog.offsetEdit()->text(), QStringLiteral("0"));
    dialog.offsetEdit()->setText(QStringLiteral("-5000"));
    EXPECT_TRUE(dialog.offsetEdit()->commit());
    EXPECT_DOUBLE_EQ(dialog.layout().centralPlaneOffsetM, -5.0);
    // The viewer follows the committed value: the crosses are the top face, the central plane and the bottom face.
    auto* viewer = dialog.viewer();
    viewer->resize(340, 340);
    const auto crosses = viewer->crossPositions();
    EXPECT_LT(crosses[0].y(), crosses[1].y());
    EXPECT_LT(crosses[1].y(), crosses[2].y());
    EXPECT_NEAR(crosses[1].y() - crosses[0].y(), crosses[2].y() - crosses[1].y(), 1e-6);
    // Zoom buttons: Ventana doble zooms out, Ventana completa restores, Ventana anterior goes back.
    const double fit = viewer->viewHeightMm();
    dialog.viewerButtons()[2]->click();
    EXPECT_GT(viewer->viewHeightMm(), fit);
    const double doubled = viewer->viewHeightMm();
    dialog.viewerButtons()[1]->click();
    EXPECT_DOUBLE_EQ(viewer->viewHeightMm(), 0.0);
    dialog.viewerButtons()[0]->click();
    EXPECT_DOUBLE_EQ(viewer->viewHeightMm(), doubled);
    dialog.viewerButtons()[4]->setChecked(true);
    EXPECT_EQ(viewer->tool(), cc::app::ShellSectionViewer::Tool::ZoomWindow);
    dialog.viewerButtons()[5]->setChecked(true);
    EXPECT_EQ(viewer->tool(), cc::app::ShellSectionViewer::Tool::Pan);
    EXPECT_FALSE(dialog.viewerButtons()[4]->isChecked()) << "zoom and pan exclude each other";
}

TEST_F(ShellToolTest, SeccionAppliesTheDialogToTheSelectedShellsAsOneUndoStep) {
    ShellSection seen;
    tool->setDialogRunner([&](QDialog& d) {
        auto& dialog = static_cast<cc::app::ShellSectionDialog&>(d);
        seen = dialog.section();
        EXPECT_EQ(d.windowTitle(), QStringLiteral("Describir sección"));
        EXPECT_FALSE(dialog.ballastEditable()) << "the editor follows the checkbox";
        EXPECT_FALSE(dialog.behaviourEditable());
        dialog.setThickness(250.0);  // mm, whatever the project unit
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

TEST_F(ShellToolTest, ThicknessIsAlwaysInMillimetresWhateverTheWorkUnit) {
    cc::app::ShellSectionPanel panel(ShellSection{0.15, false, 0.0, BallastBehaviour::Linear}, LengthUnit::Centimetres);
    EXPECT_EQ(panel.thicknessEdit()->text(), QStringLiteral("150.00"));
    EXPECT_NEAR(panel.thicknessEdit()->range().min, 0.10, 1e-12);
    EXPECT_NEAR(panel.thicknessEdit()->range().max, 10000.0, 1e-9);
    panel.thicknessEdit()->setText(QStringLiteral("20000"));
    EXPECT_FALSE(panel.validate());
    EXPECT_EQ(warnings.back(), QStringLiteral("El valor de 'Espesor' debe estar entre 0.10 y 10000.00."));
    panel.thicknessEdit()->setText(QStringLiteral("250"));
    EXPECT_TRUE(panel.validate());
    EXPECT_NEAR(panel.section().thicknessM, 0.25, 1e-12);
}

TEST_F(ShellToolTest, CltAssistantWritesTheOrthotropicValues) {
    ShellMaterial m;
    m.kind = ShellMaterialKind::GenericOrthotropic;
    cc::app::ShellMaterialPanel panel(m, project);
    ASSERT_NE(panel.cltButton(), nullptr);
    EXPECT_EQ(panel.cltButton()->toolTip(), QStringLiteral("Asistente para la introducción de propiedades de paneles CLT"));
    cc::app::CltAssistantDialog::setRunner([](QDialog& d) {
        const auto& edits = static_cast<cc::app::CltAssistantDialog&>(d).edits();
        EXPECT_EQ(edits.size(), 5u);
        EXPECT_DOUBLE_EQ(edits[0]->value(), 11000.0);
        EXPECT_DOUBLE_EQ(edits[1]->value(), 360.0);
        EXPECT_DOUBLE_EQ(edits[2]->value(), 690.0);
        EXPECT_DOUBLE_EQ(edits[3]->value(), 50.0);
        EXPECT_DOUBLE_EQ(edits[4]->value(), 450.0);
        return true;
    });
    panel.cltButton()->click();
    cc::app::CltAssistantDialog::setRunner(nullptr);
    const ShellMaterial out = panel.material();
    EXPECT_DOUBLE_EQ(out.orthoElasticModulusXMPa, 11000.0);
    EXPECT_DOUBLE_EQ(out.orthoElasticModulusYMPa, 360.0);
    EXPECT_DOUBLE_EQ(out.orthoShearModulusXYMPa, 690.0);
    EXPECT_DOUBLE_EQ(out.orthoShearModulusXZMPa, 690.0);
    EXPECT_DOUBLE_EQ(out.orthoShearModulusYZMPa, 50.0);
    EXPECT_DOUBLE_EQ(out.orthoPoissonRatioXY, 0.0);
    EXPECT_DOUBLE_EQ(out.orthoThermalExpansionX, 5.0e-6);
    EXPECT_NEAR(out.orthoDensityKnM3, 0.45 * 9.81, 1e-12);
}

TEST_F(ShellToolTest, CltAssistantCancelChangesNothing) {
    ShellMaterial m;
    m.kind = ShellMaterialKind::GenericOrthotropic;
    cc::app::ShellMaterialPanel panel(m, project);
    cc::app::CltAssistantDialog::setRunner([](QDialog&) { return false; });
    panel.cltButton()->click();
    cc::app::CltAssistantDialog::setRunner(nullptr);
    EXPECT_EQ(panel.material().orthoElasticModulusXMPa, m.orthoElasticModulusXMPa);
}
