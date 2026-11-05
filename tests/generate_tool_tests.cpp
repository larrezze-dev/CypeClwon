// Geometria > Herramientas > Generar, Asignar referencia and Buscar (T-P5-10, GUI part). Never shown on screen (offscreen).
#include <gtest/gtest.h>

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMenu>
#include <QPushButton>
#include <QRadioButton>
#include <QSignalSpy>
#include <memory>

#include "GenerateDialogs.h"
#include "GenerateTool.h"
#include "ReferenceTool.h"
#include "Viewport3D.h"

using cc::app::GenerateTool;
using cc::app::ReferenceTool;
using cc::app::Viewport3D;
using namespace cc::core;

namespace {
QApplication* app() {
    static int argc = 1;
    static char name[] = "generate_tool_tests";
    static char* argv[] = {name, nullptr};
    static QApplication instance(argc, argv);
    return &instance;
}

/// Portal frame in the xz plane plus a node above the plane: N1 (0,0,0), N2 (0,0,3), N3 (4,0,3), N4 (4,0,0), N5 (0,3,0), N6 (0,0,2).
class GenerateToolTest : public ::testing::Test {
protected:
    void SetUp() override {
        app();
        auto& m = project.structure;
        m.addNode({0, 0, 0});
        m.addNode({0, 0, 3});
        m.addNode({4, 0, 3});
        m.addNode({4, 0, 0});
        m.addNode({0, 3, 0});
        m.addNode({0, 0, 2});
        m.addBar(1, 2);
        m.addBar(2, 3);
        m.addBar(3, 4);
        view = std::make_unique<Viewport3D>();
        view->resize(800, 600);
        view->setStructure(&m);
        tool = std::make_unique<GenerateTool>();
        tool->setProject(&project, &undo);
        tool->addViewport(view.get());
        undo.setChangeCallback([this] { tool->structureChanged(); });
        QObject::connect(tool.get(), &GenerateTool::statusChanged, [this](const QString& text) { status = text; });
        QObject::connect(tool.get(), &GenerateTool::messageRequested, [this](const QString& text) { message = text; });
        tool->setDialogRunner([this](QDialog& d) {
            titles << d.windowTitle();
            if (onDialog) onDialog(d);
            return static_cast<int>(accept ? QDialog::Accepted : QDialog::Rejected);
        });
        tool->setConfirmer([this](const QString& question) {
            questions << question;
            return answer;
        });
        cc::app::RangedEdit::setWarningHandler([this](QWidget*, const QString& text) { warnings << text; });
        undo.setClean();
    }
    /// A click on an element, as the selection of the viewport reports it.
    void pick(ElementKind kind, int id) { view->setSelectionItems({{kind, id}}); }
    void rightClick() { emit view->actOnSelection(); }
    Project project;
    UndoStack undo;
    std::unique_ptr<Viewport3D> view;
    std::unique_ptr<GenerateTool> tool;
    QString status, message;
    QStringList titles, questions, warnings;
    std::function<void(QDialog&)> onDialog;
    bool accept = true;
    bool answer = true;
};
}  // namespace

TEST_F(GenerateToolTest, TheDropDownHasTheThreeMeasuredEntries) {
    ASSERT_EQ(tool->menu()->actions().size(), 3);
    EXPECT_EQ(tool->menu()->actions().at(0)->text(), QStringLiteral("Planos"));
    EXPECT_EQ(tool->menu()->actions().at(1)->text(), QStringLiteral("Nudos y barras"));
    EXPECT_EQ(tool->menu()->actions().at(2)->text(), QStringLiteral("Malla de tetraedros"));
    GenerateTool other;
    EXPECT_FALSE(other.menu()->isEnabled()) << "greyed on an empty work";
    other.setProject(&project, &undo);
    EXPECT_TRUE(other.menu()->isEnabled());
}

TEST_F(GenerateToolTest, ActivatingAnnouncesItselfAndEscapeStops) {
    QSignalSpy activating(tool.get(), &GenerateTool::aboutToActivate);
    tool->planesAction()->trigger();
    EXPECT_EQ(activating.count(), 1);
    EXPECT_EQ(tool->mode(), GenerateTool::Mode::Planes);
    EXPECT_TRUE(view->selectionMode());
    EXPECT_EQ(status, QStringLiteral("Generar - Planos | - LMB: Seleccione dos líneas de referencia ortogonales para definir el plano sobre el que desea realizar la generación. - RMB: Comenzar otra vez."));
    QKeyEvent esc(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
    QApplication::sendEvent(view.get(), &esc);
    EXPECT_EQ(tool->mode(), GenerateTool::Mode::None);
    EXPECT_FALSE(view->selectionMode());
    tool->nodeMeshAction()->trigger();
    EXPECT_EQ(status, QStringLiteral("Generar - Nudos y barras | - LMB: Seleccione un elemento."));
}

TEST_F(GenerateToolTest, PlanosTwoLinesThenTheDialogAndTheCommandStaysActive) {
    onDialog = [](QDialog& d) {
        auto& dialog = static_cast<cc::app::PlanesDialog&>(d);
        EXPECT_EQ(dialog.count(), 1) << "measured defaults";
        EXPECT_DOUBLE_EQ(dialog.separationM(), 1.0);
        EXPECT_FALSE(dialog.group());
        EXPECT_EQ(dialog.separationEdit()->toolTip(), QStringLiteral("Debe estar entre -10000.000 y 10000.000."));
        dialog.countEdit()->setText(QStringLiteral("2"));
        dialog.countEdit()->commit();
    };
    tool->planesAction()->trigger();
    pick(ElementKind::Bar, 1);
    EXPECT_EQ(tool->picks().size(), 1u);
    pick(ElementKind::Bar, 2);
    EXPECT_EQ(titles, QStringList{QStringLiteral("Generación de planos")});
    EXPECT_EQ(project.structure.nodes.size(), 6u + 2u * 5u) << "the plane y = 0 holds five nodes (N5 is at y = 3); two copies";
    EXPECT_EQ(tool->mode(), GenerateTool::Mode::Planes) << "the command goes on";
    EXPECT_TRUE(tool->picks().empty());
    undo.undo();
    EXPECT_EQ(project.structure.nodes.size(), 6u);
}

TEST_F(GenerateToolTest, PlanosRefusesParallelLinesAndTheSameLineTwice) {
    tool->planesAction()->trigger();
    pick(ElementKind::Bar, 1);
    pick(ElementKind::Bar, 1);
    EXPECT_EQ(tool->picks().size(), 1u) << "the second line must be another bar";
    pick(ElementKind::Bar, 3);  // two columns: parallel
    EXPECT_EQ(message, QString::fromStdString(generateErrorMessage(GenerateError::LinesParallel)));
    EXPECT_TRUE(titles.isEmpty()) << "no dialog";
}

TEST_F(GenerateToolTest, PlanosZeroSeparationIsBlockedWithAClearWarning) {
    cc::app::PlanesDialog dialog;
    dialog.separationEdit()->setText(QStringLiteral("0"));
    dialog.acceptButton()->click();
    EXPECT_EQ(dialog.result(), 0) << "the dialog stays open";
    ASSERT_FALSE(warnings.isEmpty());
    EXPECT_EQ(warnings.back(), QString::fromStdString(generateErrorMessage(GenerateError::ZeroSeparation)));
    dialog.separationEdit()->setText(QStringLiteral("10001"));
    dialog.acceptButton()->click();
    EXPECT_EQ(warnings.back(), QStringLiteral("El valor de 'Separación entre planos' debe estar entre -10000.000 y 10000.000."));
    dialog.countEdit()->setText(QStringLiteral("101"));
    dialog.separationEdit()->setText(QStringLiteral("1"));
    dialog.acceptButton()->click();
    EXPECT_EQ(warnings.back(), QStringLiteral("El valor de 'Nº de planos a generar' debe estar entre 1 y 100."));
    dialog.countEdit()->setText(QStringLiteral("100"));
    dialog.acceptButton()->click();
    EXPECT_EQ(dialog.result(), QDialog::Accepted);
}

TEST_F(GenerateToolTest, PlanosGroupingAsksBeforeDissolvingAnExistingGroup) {
    project.workPlanes.groups.y = {{0.0, 9.0}};
    onDialog = [](QDialog& d) { static_cast<cc::app::PlanesDialog&>(d).groupBox()->setChecked(true); };
    answer = false;
    tool->planesAction()->trigger();
    pick(ElementKind::Bar, 1);
    pick(ElementKind::Bar, 2);
    ASSERT_EQ(questions.size(), 1);
    EXPECT_EQ(questions.front(), QStringLiteral("Se van a desagrupar todos los planos que intervienen en la generación."));
    EXPECT_EQ(project.structure.nodes.size(), 6u) << "declined: nothing generated";
    answer = true;
    pick(ElementKind::Bar, 1);
    pick(ElementKind::Bar, 2);
    EXPECT_EQ(project.workPlanes.groups.y.size(), 1u);
    EXPECT_EQ(project.workPlanes.groups.y[0], (std::vector<double>{0.0, 1.0}));
}

TEST_F(GenerateToolTest, NudosYBarrasTitleFollowsTheNumberOfSegments) {
    EXPECT_EQ(cc::app::NodeMeshDialog::titleFor(1), QStringLiteral("Generación de nudos sobre un segmento"));
    EXPECT_EQ(cc::app::NodeMeshDialog::titleFor(2), QStringLiteral("Generación de una malla de nudos bidimensional"));
    EXPECT_EQ(cc::app::NodeMeshDialog::titleFor(3), QStringLiteral("Generación de una malla de nudos tridimensional"));
    tool->nodeMeshAction()->trigger();
    pick(ElementKind::Node, 1);
    pick(ElementKind::Node, 5);  // segment 1: N1 -> N5
    rightClick();
    EXPECT_EQ(titles.last(), QStringLiteral("Generación de nudos sobre un segmento"));
    tool->nodeMeshAction()->trigger();
    pick(ElementKind::Node, 1);
    pick(ElementKind::Node, 5);
    pick(ElementKind::Node, 4);
    rightClick();
    EXPECT_EQ(titles.last(), QStringLiteral("Generación de una malla de nudos bidimensional"));
    tool->nodeMeshAction()->trigger();
    pick(ElementKind::Node, 1);
    pick(ElementKind::Node, 5);
    pick(ElementKind::Node, 4);
    pick(ElementKind::Node, 6);
    rightClick();
    EXPECT_EQ(titles.last(), QStringLiteral("Generación de una malla de nudos tridimensional"));
}

TEST_F(GenerateToolTest, NudosYBarrasIgnoresAPickThatIsCoplanarOrCollinear) {
    tool->nodeMeshAction()->trigger();
    pick(ElementKind::Node, 1);
    pick(ElementKind::Node, 2);  // segment 1 along z
    pick(ElementKind::Node, 6);  // N6 (0,0,2) is collinear with it: ignored
    EXPECT_EQ(tool->picks().size(), 2u);
    pick(ElementKind::Node, 4);  // N4 (4,0,0): segment 2
    pick(ElementKind::Node, 3);  // N3 (4,0,3): in the plane y = 0 of the first two: ignored (the dialog stays the 2D one)
    EXPECT_EQ(tool->picks().size(), 3u);
    pick(ElementKind::Node, 5);  // N5 (0,3,0): outside that plane
    EXPECT_EQ(tool->picks().size(), 4u);
    pick(ElementKind::Node, 5);
    EXPECT_EQ(tool->picks().size(), 4u) << "no more than three segments";
}

TEST_F(GenerateToolTest, NudosYBarrasOneSegmentDividesAndCancelLeavesNothing) {
    onDialog = [](QDialog& d) {
        auto& dialog = static_cast<cc::app::NodeMeshDialog&>(d);
        EXPECT_EQ(dialog.divisionsEdit(0)->text(), QStringLiteral("1")) << "measured default";
        EXPECT_EQ(dialog.divisionsEdit(0)->toolTip(), QStringLiteral("Debe estar entre 1 y 1000."));
        dialog.divisionsEdit(0)->setText(QStringLiteral("3"));
        dialog.divisionsEdit(0)->commit();
    };
    tool->nodeMeshAction()->trigger();
    pick(ElementKind::Node, 2);
    pick(ElementKind::Node, 3);
    rightClick();
    EXPECT_EQ(project.structure.nodes.size(), 8u) << "two intermediate nodes";
    EXPECT_EQ(project.structure.bars.size(), 5u) << "the lintel became three bars";
    EXPECT_TRUE(tool->picks().empty());
    undo.undo();
    EXPECT_EQ(project.structure.bars.size(), 3u);
    accept = false;
    pick(ElementKind::Node, 2);
    pick(ElementKind::Node, 3);
    rightClick();
    EXPECT_EQ(project.structure.nodes.size(), 6u);
    // Right click with fewer than two picks only starts again.
    titles.clear();
    pick(ElementKind::Node, 2);
    rightClick();
    EXPECT_TRUE(titles.isEmpty());
    EXPECT_TRUE(tool->picks().empty());
}

TEST_F(GenerateToolTest, NodeMeshDialogHasTheMeasuredFieldsPerSegment) {
    cc::app::NodeMeshDialog two(2);
    EXPECT_NE(two.onlyNodesBox(), nullptr);
    EXPECT_FALSE(two.onlyNodesBox()->isChecked());
    for (int k = 0; k < 2; ++k) {
        EXPECT_EQ(two.divisionsEdit(k)->text(), QStringLiteral("1"));
        EXPECT_EQ(two.groupBox(k)->text(), QStringLiteral("Agrupar barras paralelas al segmento %1").arg(k + 1));
        EXPECT_EQ(two.pieceBox(k)->text(), QStringLiteral("Crear pieza entre barras alineadas"));
        EXPECT_FALSE(two.groupBox(k)->isChecked());
        EXPECT_FALSE(two.pieceBox(k)->isChecked());
    }
    two.onlyNodesBox()->setChecked(true);
    EXPECT_FALSE(two.groupBox(0)->isEnabled());
    cc::app::NodeMeshDialog one(1);
    EXPECT_EQ(one.onlyNodesBox(), nullptr);
    EXPECT_EQ(one.groupBox(0), nullptr);
    one.divisionsEdit(0)->setText(QStringLiteral("1001"));
    one.acceptButton()->click();
    EXPECT_EQ(warnings.last(), QStringLiteral("El valor de 'Nº de divisiones del segmento 1' debe estar entre 1 y 1000."));
    NodeMeshSpec spec;
    cc::app::NodeMeshDialog three(3);
    three.divisionsEdit(2)->setText(QStringLiteral("4"));
    three.divisionsEdit(2)->commit();
    three.groupBox(1)->setChecked(true);
    three.fill(spec);
    EXPECT_EQ(spec.divisions[2], 4);
    EXPECT_TRUE(spec.groupParallel[1]);
    EXPECT_FALSE(spec.groupParallel[0]);
}

TEST_F(GenerateToolTest, TetrahedraFlowAndDialogDefaults) {
    onDialog = [](QDialog& d) {
        auto& dialog = static_cast<cc::app::TetrahedraDialog&>(d);
        EXPECT_EQ(dialog.divisions1Edit()->text(), QStringLiteral("1"));
        EXPECT_EQ(dialog.divisions2Edit()->text(), QStringLiteral("1"));
        EXPECT_EQ(dialog.distanceEdit()->text(), QStringLiteral("1.000"));
        for (QCheckBox* box : dialog.groupBoxes()) EXPECT_TRUE(box->isChecked()) << "measured: the three groupings are on";
        EXPECT_EQ(dialog.groupBoxes()[0]->text(), QStringLiteral("Agrupar barras de plano inferior"));
        EXPECT_EQ(dialog.groupBoxes()[1]->text(), QStringLiteral("Agrupar barras de plano superior"));
        EXPECT_EQ(dialog.groupBoxes()[2]->text(), QStringLiteral("Agrupar barras entre planos (diagonales)"));
    };
    tool->tetrahedraAction()->trigger();
    EXPECT_EQ(status, QStringLiteral("Generar - Malla de tetraedros | - LMB: Seleccione un elemento."));
    pick(ElementKind::Node, 1);  // origin
    pick(ElementKind::Node, 4);  // end of segment 1: (4,0,0)
    pick(ElementKind::Node, 5);  // end of segment 2: (0,3,0)
    rightClick();
    EXPECT_EQ(titles.last(), QStringLiteral("Generación de una malla de tetraedros"));
    EXPECT_EQ(project.structure.nodes.size(), 6u + 2u) << "the fourth corner of the base and the upper node";
    EXPECT_EQ(project.structure.bars.size(), 3u + 4u + 4u);
    undo.undo();
    EXPECT_EQ(project.structure.nodes.size(), 6u);
}

TEST_F(GenerateToolTest, UndoOfAPickedElementBreaksThePicks) {
    tool->nodeMeshAction()->trigger();
    pick(ElementKind::Node, 1);
    EXPECT_EQ(tool->picks().size(), 1u);
    project.structure.removeBar(1);
    project.structure.removeBar(2);
    project.structure.removeBar(3);
    project.structure.removeNode(1);
    tool->structureChanged();
    EXPECT_TRUE(tool->picks().empty());
}

// ---- Asignar referencia / Buscar -------------------------------------------------------------------------------------------------

class ReferenceToolTest : public GenerateToolTest {
protected:
    void SetUp() override {
        GenerateToolTest::SetUp();
        refs = std::make_unique<ReferenceTool>();
        refs->setProject(&project, &undo);
        refs->addViewport(view.get());
        refs->setDialogRunner([this](QDialog& d) {
            auto* text = dynamic_cast<cc::app::ReferenceTextDialog*>(&d);
            if (text) {
                firstText = text->text();
                text->edit()->setText(typed);
            }
            return static_cast<int>(accept ? QDialog::Accepted : QDialog::Rejected);
        });
        QObject::connect(refs.get(), &ReferenceTool::statusChanged, [this](const QString& text) { status = text; });
    }
    std::unique_ptr<ReferenceTool> refs;
    QString typed = QStringLiteral("P1"), firstText;
};

TEST_F(ReferenceToolTest, ButtonsAreGreyOnAnEmptyWorkAndHaveOnlyTheTitleAsTooltip) {
    ReferenceTool other;
    EXPECT_FALSE(other.assignAction()->isEnabled());
    EXPECT_FALSE(other.findAction()->isEnabled());
    EXPECT_TRUE(refs->assignAction()->isEnabled());
    EXPECT_EQ(refs->assignAction()->text(), QStringLiteral("Asignar referencia"));
    EXPECT_EQ(refs->assignAction()->toolTip(), QStringLiteral("Asignar referencia"));
    EXPECT_EQ(refs->findAction()->toolTip(), QStringLiteral("Buscar"));
}

TEST_F(ReferenceToolTest, AsignarReferenciaOpensThePropertiesWindowWithTheMeasuredDefaults) {
    refs->assignAction()->trigger();
    EXPECT_EQ(refs->mode(), ReferenceTool::Mode::Assign);
    EXPECT_EQ(status, QStringLiteral("Herramientas - Asignar referencia | - LMB: Seleccione un elemento."));
    auto* panel = refs->assignPanel();
    ASSERT_NE(panel, nullptr);
    EXPECT_EQ(panel->windowTitle(), QStringLiteral("Propiedades"));
    EXPECT_EQ(panel->kindCombo()->currentText(), QStringLiteral("Nudos"));
    EXPECT_TRUE(panel->individualRadio()->isChecked());
    EXPECT_EQ(panel->individualRadio()->text(), QStringLiteral("Individual"));
    EXPECT_EQ(panel->multipleRadio()->text(), QStringLiteral("Múltiple"));
    EXPECT_TRUE(view->nodeLabelsVisible()) << "the nodes show their reference";
    refs->deactivate();
    EXPECT_EQ(refs->mode(), ReferenceTool::Mode::None);
    EXPECT_FALSE(view->selectionMode());
}

TEST_F(ReferenceToolTest, IndividualAssignsTheTypedReferenceToTheClickedNode) {
    refs->assignAction()->trigger();
    pick(ElementKind::Node, 3);
    EXPECT_EQ(firstText, QStringLiteral("N3")) << "the dialog starts with the current reference";
    EXPECT_EQ(project.structure.findNode(3)->reference, "P1");
    EXPECT_EQ(refs->mode(), ReferenceTool::Mode::Assign) << "the command goes on";
    undo.undo();
    EXPECT_TRUE(project.structure.findNode(3)->reference.empty());
    accept = false;
    pick(ElementKind::Node, 3);
    EXPECT_TRUE(project.structure.findNode(3)->reference.empty());
}

TEST_F(ReferenceToolTest, MultipleNumbersTheSelectionConsecutively) {
    refs->assignAction()->trigger();
    refs->assignPanel()->multipleRadio()->setChecked(true);
    typed = QStringLiteral("A");
    view->setSelectionItems({{ElementKind::Node, 4}, {ElementKind::Node, 2}, {ElementKind::Node, 3}});
    EXPECT_TRUE(project.structure.findNode(2)->reference.empty()) << "nothing happens before the right click";
    rightClick();
    EXPECT_EQ(project.structure.findNode(2)->reference, "A");
    EXPECT_EQ(project.structure.findNode(3)->reference, "B");
    EXPECT_EQ(project.structure.findNode(4)->reference, "C");
    EXPECT_EQ(undo.count(), 1u);
}

TEST_F(ReferenceToolTest, ShellsHaveReferencesToo) {
    ASSERT_EQ(project.structure.addShell({1, 2, 3, 4}, 0.1) > 0, true);
    refs->assignAction()->trigger();
    refs->assignPanel()->kindCombo()->setCurrentText(QStringLiteral("Láminas"));
    pick(ElementKind::Shell, 1);
    EXPECT_EQ(firstText, QStringLiteral("L1"));
    EXPECT_EQ(project.structure.findShell(1)->reference, "P1");
}

TEST_F(ReferenceToolTest, BuscarSelectsWhatItFindsAndReportsIt) {
    project.structure.findNode(2)->reference = "Pilar A";
    project.structure.findNode(3)->reference = "Pilar B";
    refs->findAction()->trigger();
    auto* panel = refs->findPanel();
    ASSERT_NE(panel, nullptr);
    EXPECT_EQ(panel->windowTitle(), QStringLiteral("Propiedades - Buscar"));
    EXPECT_EQ(panel->kindCombo()->currentText(), QStringLiteral("Nudos"));
    EXPECT_FALSE(panel->wholeWordBox()->isChecked());
    EXPECT_EQ(panel->wholeWordBox()->text(), QStringLiteral("Sólo palabra completa"));
    panel->textEdit()->setText(QStringLiteral("pilar"));
    EXPECT_EQ(view->selection().ids(ElementKind::Node), (std::vector<int>{2, 3}));
    panel->wholeWordBox()->setChecked(true);
    EXPECT_TRUE(view->selection().empty()) << "'pilar' alone is not a whole reference";
    panel->textEdit()->setText(QStringLiteral("Pilar B"));
    EXPECT_EQ(view->selection().ids(ElementKind::Node), (std::vector<int>{3}));
    EXPECT_EQ(refs->search(ReferenceKind::Node, QStringLiteral("zzz"), false), 0);
    EXPECT_EQ(status, QStringLiteral("No se ha encontrado ningún elemento."));
    EXPECT_EQ(refs->search(ReferenceKind::Node, QStringLiteral("N"), false), 4);
}

int main(int argc, char** argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
