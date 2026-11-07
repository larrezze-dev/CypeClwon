// Propiedades > Diafragmas (T-P6-03, GUI part): dialog "Editar", panel "Propiedades - Asignar", tool and 3D texts. Expected values:
// docs/research/ui/diafragmas.md sections 3, 4, 5 and 8 (measured in the original). Never shown on screen (offscreen).
#include <gtest/gtest.h>

#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QLineEdit>
#include <QMouseEvent>
#include <QRadioButton>
#include <QTableWidget>
#include <QToolButton>
#include <memory>

#include "DiaphragmTool.h"
#include "UserMessages.h"
#include "Viewport3D.h"
#include "core/node_diaphragm_edit.h"
#include "core/node_support_edit.h"

using namespace cc::core;
using cc::app::DiaphragmEditDialog;
using cc::app::DiaphragmPanel;
using cc::app::DiaphragmTool;
using cc::app::Viewport3D;
namespace messages = cc::app::messages;

namespace {

QApplication* app() {
    static int argc = 1;
    static char name[] = "diaphragm_tool_tests";
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

struct Shown {
    messages::Kind kind;
    QString text;
};

/// Four nodes along X seen from the front: N1 (0,0,0) .. N4 (6,0,0).
class DiaphragmToolTest : public ::testing::Test {
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
        tool = std::make_unique<DiaphragmTool>();
        tool->setProject(&project, &undo);
        tool->addViewport(view.get());
        undo.setChangeCallback([this] { tool->structureChanged(); });
        QObject::connect(tool.get(), &DiaphragmTool::statusChanged, [this](const QString& text) { status = text; });
        shown.clear();
        answer = true;
        messages::handler() = [this](QWidget*, messages::Kind kind, const QString& text) {
            shown.push_back({kind, text});
            return answer;
        };
        DiaphragmEditDialog::setColourPicker([this](QWidget*, unsigned& rgb) {
            if (!pickColour) return false;
            rgb = *pickColour;
            return true;
        });
        undo.setClean();
    }
    void TearDown() override {
        messages::handler() = nullptr;
        DiaphragmEditDialog::setColourPicker(nullptr);
    }
    QPointF screenOf(int id) {
        const Node& n = *project.structure.findNode(id);
        QPointF p;
        EXPECT_TRUE(view->worldToScreen({n.position.x, n.position.y, n.position.z}, p));
        return p;
    }
    void selectWindow(int first, int last) {
        const QPointF a = screenOf(first), b = screenOf(last);
        click(*view, QPointF(a.x() - 20, a.y() - 40));
        click(*view, QPointF(b.x() + 20, b.y() + 40));
    }
    /// A diaphragm D1 defined through the same path as the dialog.
    void defineD1() {
        Diaphragm d = newDiaphragm();
        d.reference = "D1";
        ASSERT_FALSE(editDiaphragms(project, undo, {d}).has_value());
    }
    Project project;
    UndoStack undo;
    std::unique_ptr<Viewport3D> view;
    std::unique_ptr<DiaphragmTool> tool;
    QString status;
    std::vector<Shown> shown;
    bool answer = true;
    std::optional<unsigned> pickColour;
};

}  // namespace

TEST_F(DiaphragmToolTest, ButtonsAreEnabledOnAnEmptyWork) {
    EXPECT_EQ(tool->editAction()->text(), QStringLiteral("Editar"));
    EXPECT_EQ(tool->assignAction()->text(), QStringLiteral("Diafragma"));
    EXPECT_EQ(tool->generateAction()->text(), QStringLiteral("Generar por niveles"));
    EXPECT_TRUE(tool->editAction()->isEnabled());
    EXPECT_TRUE(tool->assignAction()->isEnabled());
    EXPECT_TRUE(tool->generateAction()->isEnabled());
    DiaphragmTool other;
    EXPECT_FALSE(other.editAction()->isEnabled()) << "no project";
}

TEST_F(DiaphragmToolTest, DiafragmaWithoutDefinitionsShowsTheInformationAndDoesNotStart) {
    tool->assignAction()->trigger();
    ASSERT_EQ(shown.size(), 1u);
    EXPECT_EQ(shown[0].kind, messages::Kind::Information);
    EXPECT_EQ(shown[0].text, QStringLiteral("No se ha definido ningún diafragma."));
    EXPECT_EQ(tool->mode(), DiaphragmTool::Mode::None);
    EXPECT_FALSE(tool->assignAction()->isChecked());
    EXPECT_FALSE(view->selectionMode());
}

TEST_F(DiaphragmToolTest, EditDialogNewRowDuplicateAndDelete) {
    DiaphragmEditDialog dialog({});
    EXPECT_EQ(dialog.windowTitle(), QStringLiteral("Editar"));
    EXPECT_EQ(dialog.table()->rowCount(), 0);
    EXPECT_EQ(dialog.table()->horizontalHeaderItem(0)->text(), QStringLiteral("Referencia"));
    EXPECT_EQ(dialog.table()->horizontalHeaderItem(1)->text(), QStringLiteral("Color"));
    EXPECT_EQ(dialog.addButton()->toolTip(), QStringLiteral("Añadir / Añadir nuevo elemento a la lista"));
    const QStringList heads = {"Hipótesis", "Fx (kN)", "Fy (kN)", "Coord. x (m)", "Coord. y (m)"};
    for (int c = 0; c < 5; ++c) EXPECT_EQ(dialog.loadTable()->horizontalHeaderItem(c)->text(), heads[c]);
    EXPECT_TRUE(dialog.addRow());
    ASSERT_EQ(dialog.table()->rowCount(), 1);
    EXPECT_EQ(dialog.table()->item(0, 1)->background().color().rgb() & 0xFFFFFFu, 0xFF00FFu) << "new rows are magenta";
    EXPECT_TRUE(dialog.rows()[0].reference.empty());
    // Another row with an empty reference: the warning of the original, and nothing is added.
    EXPECT_FALSE(dialog.addRow());
    ASSERT_EQ(shown.size(), 1u);
    EXPECT_EQ(shown[0].kind, messages::Kind::Warning);
    EXPECT_EQ(shown[0].text, QStringLiteral("La referencia no puede quedar vacía."));
    dialog.referenceEdit()->setText(QStringLiteral("D1"));
    emit dialog.referenceEdit()->textEdited(QStringLiteral("D1"));
    EXPECT_EQ(dialog.table()->item(0, 0)->text(), QStringLiteral("D1"));
    // A load: the first load case, everything else 0.000.
    EXPECT_TRUE(dialog.addLoad());
    ASSERT_EQ(dialog.loadTable()->rowCount(), 1);
    EXPECT_EQ(qobject_cast<QComboBox*>(dialog.loadTable()->cellWidget(0, 0))->currentText(), QStringLiteral("Peso propio"));
    for (int c = 1; c < 5; ++c) EXPECT_EQ(dialog.loadTable()->item(0, c)->text(), QStringLiteral("0.000"));
    dialog.loadTable()->item(0, 1)->setText(QStringLiteral("12,5"));
    EXPECT_DOUBLE_EQ(dialog.rows()[0].loads[0].fxKn, 12.5);
    EXPECT_EQ(dialog.loadTable()->item(0, 1)->text(), QStringLiteral("12.500"));
    // Duplicate: same reference, colour and loads (the nodes are not copied).
    dialog.duplicateRow();
    ASSERT_EQ(dialog.rows().size(), 2u);
    EXPECT_EQ(dialog.rows()[1].reference, "D1");
    EXPECT_EQ(dialog.rows()[1].loads, dialog.rows()[0].loads);
    // Delete.
    dialog.removeRow();
    EXPECT_EQ(dialog.rows().size(), 1u);
}

TEST_F(DiaphragmToolTest, EditDialogAcceptChecksReferences) {
    DiaphragmEditDialog dialog({});
    dialog.addRow();
    dialog.accept();  // empty reference
    ASSERT_EQ(shown.size(), 1u);
    EXPECT_EQ(shown[0].text, QStringLiteral("No ha introducido la referencia de todos los diafragmas introducidos."));
    EXPECT_NE(dialog.result(), QDialog::Accepted);
    dialog.referenceEdit()->setText(QStringLiteral("D1"));
    emit dialog.referenceEdit()->textEdited(QStringLiteral("D1"));
    dialog.duplicateRow();  // two "D1"
    dialog.accept();
    ASSERT_EQ(shown.size(), 2u);
    EXPECT_EQ(shown[1].text, QStringLiteral("La referencia 'D1' está repetida."));
    EXPECT_EQ(shown[1].kind, messages::Kind::Warning);
}

TEST_F(DiaphragmToolTest, ColourCellOpensThePickerAndStoresTheColour) {
    DiaphragmEditDialog dialog({});
    dialog.addRow();
    pickColour = 0x0000FFu;
    emit dialog.table()->cellClicked(0, 1);
    EXPECT_EQ(dialog.rows()[0].color, 0x0000FFu);
    EXPECT_EQ(dialog.table()->item(0, 1)->background().color().rgb() & 0xFFFFFFu, 0x0000FFu);
    pickColour.reset();  // Cancelar
    emit dialog.table()->cellClicked(0, 1);
    EXPECT_EQ(dialog.rows()[0].color, 0x0000FFu);
}

TEST_F(DiaphragmToolTest, EditThroughTheToolAppliesAsOneUndoStep) {
    tool->setDialogRunner([](QDialog& dialog) {
        auto& d = static_cast<DiaphragmEditDialog&>(dialog);
        d.addRow();
        d.referenceEdit()->setText(QStringLiteral("D1"));
        emit d.referenceEdit()->textEdited(QStringLiteral("D1"));
        d.accept();
        return static_cast<int>(d.result());
    });
    EXPECT_TRUE(tool->edit());
    EXPECT_EQ(undo.count(), 1u);
    ASSERT_EQ(project.diaphragms.items.size(), 1u);
    EXPECT_EQ(project.diaphragms.items[0].reference, "D1");
    EXPECT_EQ(status, QStringLiteral("Seleccione una opción del menú."));
    undo.undo();
    EXPECT_TRUE(project.diaphragms.empty());
    // Cancelar changes nothing.
    tool->setDialogRunner([](QDialog& dialog) {
        static_cast<DiaphragmEditDialog&>(dialog).addRow();
        return static_cast<int>(QDialog::Rejected);
    });
    EXPECT_FALSE(tool->edit());
    EXPECT_TRUE(project.diaphragms.empty());
}

TEST_F(DiaphragmToolTest, PanelAndStatusText) {
    defineD1();
    tool->assignAction()->trigger();
    EXPECT_EQ(tool->mode(), DiaphragmTool::Mode::Active);
    EXPECT_EQ(status, QStringLiteral("Diafragmas - Diafragma |  - LMB: Seleccione un grupo de elementos. - RMB: Actuar sobre los elementos seleccionados."));
    EXPECT_TRUE(view->selectionMode());
    EXPECT_TRUE(view->diaphragmLabelsVisible());
    DiaphragmPanel* panel = tool->panel();
    ASSERT_NE(panel, nullptr);
    EXPECT_EQ(panel->windowTitle(), QStringLiteral("Propiedades - Asignar"));
    EXPECT_EQ(panel->assignRadio()->text(), QStringLiteral("Asignar"));
    EXPECT_EQ(panel->removeRadio()->text(), QStringLiteral("Eliminar asignación"));
    EXPECT_TRUE(panel->assignRadio()->isChecked());
    EXPECT_EQ(panel->combo()->currentText(), QStringLiteral("D1"));
    EXPECT_TRUE(panel->comboVisible());
    panel->removeRadio()->setChecked(true);
    EXPECT_FALSE(panel->comboVisible()) << "Eliminar asignación hides the combo";
    tool->assignAction()->trigger();
    EXPECT_EQ(tool->mode(), DiaphragmTool::Mode::None);
    EXPECT_FALSE(view->selectionMode());
    EXPECT_FALSE(view->diaphragmLabelsVisible());
}

TEST_F(DiaphragmToolTest, RightClickAssignsAndEliminarRemoves) {
    defineD1();
    undo.setClean();
    tool->assignAction()->trigger();
    selectWindow(1, 2);
    EXPECT_EQ(view->selection().ids(ElementKind::Node), (std::vector<int>{1, 2}));
    click(*view, QPointF(400, 500), Qt::RightButton);
    EXPECT_EQ(diaphragmNodeLabel(project.diaphragms, 1), "D1");
    EXPECT_EQ(diaphragmNodeLabel(project.diaphragms, 2), "D1");
    EXPECT_EQ(diaphragmNodeLabel(project.diaphragms, 3), "");
    EXPECT_TRUE(view->selection().empty());
    undo.undo();
    EXPECT_FALSE(isNodeInDiaphragm(project.diaphragms, 1));
    undo.redo();
    // Eliminar asignación on N1 only.
    tool->panel()->removeRadio()->setChecked(true);
    selectWindow(1, 1);
    click(*view, QPointF(400, 500), Qt::RightButton);
    EXPECT_FALSE(isNodeInDiaphragm(project.diaphragms, 1));
    EXPECT_TRUE(isNodeInDiaphragm(project.diaphragms, 2));
    // Nothing selected: nothing happens.
    const std::size_t count = undo.count();
    click(*view, QPointF(400, 500), Qt::RightButton);
    EXPECT_EQ(undo.count(), count);
}

TEST_F(DiaphragmToolTest, AssigningARestrainedNodeIsAcceptedWithoutAMessage) {
    // REA diafragmas.md sec. 9.4: the restraint is only reported by the calculation check.
    defineD1();
    ExternalSupport support;
    support.type = ExternalSupportType::Fixed;
    ASSERT_EQ(assignExternalSupport(project, undo, {1}, support), 1);
    tool->assignAction()->trigger();
    EXPECT_TRUE(tool->execute({1}));
    EXPECT_TRUE(shown.empty());
    EXPECT_TRUE(isNodeInDiaphragm(project.diaphragms, 1));
}

TEST_F(DiaphragmToolTest, GenerateByLevelsMessages) {
    // Without levels: «Aviso» without a previous question.
    defineD1();
    EXPECT_FALSE(tool->generate());
    ASSERT_EQ(shown.size(), 1u);
    EXPECT_EQ(shown[0].kind, messages::Kind::Warning);
    EXPECT_EQ(shown[0].text, QStringLiteral("No se ha definido ningún nivel"));
    shown.clear();
    // With levels and diaphragms already there: the question, then the replacement (provisional algorithm of the core).
    project.workPlanes.levels = {{"Base", -10.0}, {"Planta 1", 0.0}};
    answer = false;
    EXPECT_FALSE(tool->generate());
    ASSERT_EQ(shown.size(), 1u);
    EXPECT_EQ(shown[0].kind, messages::Kind::QuestionDefaultNo);
    EXPECT_EQ(shown[0].text, QStringLiteral("Se borrarán los diafragmas previamente definidos. ¿Desea continuar?"));
    shown.clear();
    answer = true;
    EXPECT_TRUE(tool->generate());
    ASSERT_EQ(shown.size(), 2u);
    EXPECT_EQ(shown[0].kind, messages::Kind::QuestionDefaultNo);
    EXPECT_EQ(shown[1].kind, messages::Kind::Information);
    EXPECT_EQ(shown[1].text, QStringLiteral("Se ha generado un diafragma"));
}

TEST_F(DiaphragmToolTest, ChangingProjectEndsTheCommand) {
    defineD1();
    tool->assignAction()->trigger();
    tool->setProject(nullptr, nullptr);
    EXPECT_EQ(tool->mode(), DiaphragmTool::Mode::None);
    EXPECT_FALSE(tool->assignAction()->isEnabled());
    EXPECT_EQ(view->nodeDiaphragms(), nullptr);
}

TEST_F(DiaphragmToolTest, ViewportReceivesTheDiaphragmsWhileTheToolIsActive) {
    defineD1();
    ASSERT_EQ(assignNodesToDiaphragm(project, undo, 1, {1}).status, DiaphragmStatus::Ok);
    // The colour of the square and the text "N1: D1" are checked on the clone screenshots; here the state that drives them.
    EXPECT_EQ(view->nodeDiaphragms(), &project.diaphragms);
    EXPECT_FALSE(view->diaphragmLabelsVisible());
    tool->assignAction()->trigger();
    EXPECT_TRUE(view->diaphragmLabelsVisible());
    EXPECT_TRUE(view->nodeLabelsVisible());
    const Diaphragm* d = diaphragmOfNode(*view->nodeDiaphragms(), 1);
    ASSERT_NE(d, nullptr);
    EXPECT_EQ(d->color, 0xFF00FFu);
}

int main(int argc, char** argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    app();
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
