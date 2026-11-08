// Carga > Nudos (T-P7-02, GUI part): dialogs, tool and 3D drawing data. Expected values: docs/research/ui/cargas_nudos.md (measured in the
// original). Never shown on screen (offscreen).
#include <gtest/gtest.h>

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QLineEdit>
#include <QMouseEvent>
#include <QRadioButton>
#include <QTableWidget>
#include <QToolButton>
#include <memory>

#include "DiaphragmTool.h"
#include "NodalLoadDialogs.h"
#include "NodalLoadTool.h"
#include "UserMessages.h"
#include "Viewport3D.h"
#include "core/load_case_edit.h"
#include "core/node_diaphragm_edit.h"
#include "core/node_support_edit.h"

using namespace cc::core;
using cc::app::LoadCaseChoiceDialog;
using cc::app::LoadScalesDialog;
using cc::app::NodalLoadDialog;
using cc::app::NodalLoadTool;
using cc::app::PrescribedDisplacementsDialog;
using cc::app::Viewport3D;
namespace messages = cc::app::messages;

namespace {

QApplication* app() {
    static int argc = 1;
    static char name[] = "nodal_load_tool_tests";
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
class NodalLoadToolTest : public ::testing::Test {
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
        tool = std::make_unique<NodalLoadTool>();
        tool->setProject(&project, &undo);
        tool->addViewport(view.get());
        tool->setLoadsVisible(true);
        undo.setChangeCallback([this] { tool->structureChanged(); });
        QObject::connect(tool.get(), &NodalLoadTool::statusChanged, [this](const QString& text) { status = text; });
        shown.clear();
        messages::handler() = [this](QWidget*, messages::Kind kind, const QString& text) {
            shown.push_back({kind, text});
            return true;
        };
        undo.setClean();
    }
    void TearDown() override { messages::handler() = nullptr; }
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
    /// Runs `body` on the dialog of type D and accepts it unless `body` returns false.
    template <class D, class F>
    void runner(F body) {
        tool->setDialogRunner([body](QDialog& dialog) {
            auto& d = static_cast<D&>(dialog);
            if (!body(d)) return static_cast<int>(QDialog::Rejected);
            d.accept();
            return static_cast<int>(d.result());
        });
    }
    int addCase() {
        NewLoadCase d;
        d.nature = LoadNature::DeadLoad;
        d.reference = "CM 1";
        int id = 0;
        EXPECT_EQ(createLoadCase(project, undo, d, &id), LoadCaseError::None);
        return id;
    }
    NodalLoadData data(double kn, LoadDirection dir = LoadDirection::MinusZ, int loadCase = 0) {
        NodalLoadData d;
        d.magnitude = kn;
        d.direction = dir;
        d.loadCaseId = loadCase;
        return d;
    }
    void addLoad(int node, double kn, LoadDirection dir = LoadDirection::MinusZ, int loadCase = 0) {
        ASSERT_EQ(addNodalLoads(project, undo, {node}, data(kn, dir, loadCase)), NodalLoadError::None);
    }
    void fixNode(int node) {
        ExternalSupport s;
        s.type = ExternalSupportType::Fixed;
        ASSERT_EQ(assignExternalSupport(project, undo, {node}, s), 1);
    }
    Project project;
    UndoStack undo;
    std::unique_ptr<Viewport3D> view;
    std::unique_ptr<NodalLoadTool> tool;
    QString status;
    std::vector<Shown> shown;
};

}  // namespace

// ---- buttons and status ------------------------------------------------------------------------------------------------------

TEST_F(NodalLoadToolTest, ButtonsFollowTheData) {
    EXPECT_EQ(tool->prescribedAction()->text(), QStringLiteral("Desplazamientos prescritos"));
    EXPECT_EQ(tool->newAction()->text(), QStringLiteral("Nueva"));
    EXPECT_EQ(tool->editAction()->text(), QStringLiteral("Editar"));
    EXPECT_EQ(tool->caseAction()->text(), QStringLiteral("Hipótesis"));
    EXPECT_EQ(tool->deleteAction()->text(), QStringLiteral("Borrar"));
    EXPECT_EQ(tool->viewAction()->text(), QStringLiteral("Hipótesis vista"));
    EXPECT_TRUE(tool->viewAction()->toolTip().contains(QStringLiteral("Permite seleccionar la hipótesis de carga activa. Las cargas sobre nudos y barras que se introduzcan, por defecto, se asignarán a dicha hipótesis.")));
    EXPECT_EQ(tool->scalesAction()->text(), QStringLiteral("Escalas"));
    EXPECT_TRUE(tool->newAction()->isEnabled());
    EXPECT_FALSE(tool->editAction()->isEnabled());
    EXPECT_FALSE(tool->caseAction()->isEnabled());
    EXPECT_FALSE(tool->deleteAction()->isEnabled());
    EXPECT_FALSE(tool->prescribedAction()->isEnabled()) << "no support yet";
    fixNode(1);
    EXPECT_TRUE(tool->prescribedAction()->isEnabled());
    addLoad(2, 5.0);
    EXPECT_TRUE(tool->editAction()->isEnabled());
    EXPECT_TRUE(tool->caseAction()->isEnabled());
    EXPECT_TRUE(tool->deleteAction()->isEnabled());
    undo.undo();
    EXPECT_FALSE(tool->editAction()->isEnabled());
    NodalLoadTool other;
    EXPECT_FALSE(other.newAction()->isEnabled()) << "no project";
}

TEST_F(NodalLoadToolTest, StatusTextAndViewPanel) {
    tool->newAction()->trigger();
    EXPECT_EQ(tool->mode(), NodalLoadTool::Mode::New);
    EXPECT_EQ(status, QStringLiteral("Nudos - Nueva |  - LMB: Seleccione un grupo de elementos. - RMB: Actuar sobre los elementos seleccionados."));
    EXPECT_TRUE(view->selectionMode());
    ASSERT_NE(tool->panel(), nullptr);
    EXPECT_EQ(tool->panel()->windowTitle(), QStringLiteral("Propiedades - Hipótesis vista"));
    EXPECT_EQ(tool->panel()->combo()->currentText(), QStringLiteral("Peso propio"));
    EXPECT_EQ(tool->panel()->allCheck()->text(), QStringLiteral("Ver todas"));
    EXPECT_FALSE(tool->panel()->allCheck()->isChecked());
    EXPECT_EQ(NodalLoadTool::statusText(NodalLoadTool::Mode::Prescribed),
              QStringLiteral("Nudos - Desplazamientos prescritos |  - LMB: Seleccione un grupo de elementos. - RMB: Actuar sobre los elementos seleccionados."));
    tool->newAction()->trigger();
    EXPECT_EQ(tool->mode(), NodalLoadTool::Mode::None);
    EXPECT_FALSE(view->selectionMode());
    EXPECT_EQ(tool->panel(), nullptr) << "the window closes with the command";
    tool->newAction()->trigger();  // and a new one opens with the next command, also in the same event loop pass
    ASSERT_NE(tool->panel(), nullptr);
    EXPECT_TRUE(tool->panel()->isVisible());
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    ASSERT_NE(tool->panel(), nullptr);
    EXPECT_TRUE(tool->panel()->isVisible());
}

TEST_F(NodalLoadToolTest, PrescribedShowsNoViewPanel) {
    fixNode(1);
    tool->prescribedAction()->trigger();
    EXPECT_EQ(tool->mode(), NodalLoadTool::Mode::Prescribed);
    EXPECT_EQ(tool->panel(), nullptr);
}

// ---- Nueva ------------------------------------------------------------------------------------------------------------------

TEST_F(NodalLoadToolTest, NewDialogDefaultsAndMeasuredLayout) {
    NodalLoadDialog dialog(QStringLiteral("Introducir cargas sobre nudos"), project.loadCases, NodalLoadData{});
    EXPECT_EQ(dialog.windowTitle(), QStringLiteral("Introducir cargas sobre nudos"));
    EXPECT_EQ(dialog.caseCombo()->currentText(), QStringLiteral("Peso propio"));
    EXPECT_EQ(dialog.valueEdit()->text(), QStringLiteral("9.81"));
    EXPECT_EQ(dialog.direction(), LoadDirection::MinusZ);
    EXPECT_TRUE(dialog.directionButton(5)->isChecked());
    for (int i = 0; i < 5; ++i) EXPECT_FALSE(dialog.directionButton(i)->isChecked());
    EXPECT_EQ(dialog.size(), QSize(390, 295));
    EXPECT_TRUE(dialog.vectorGroup()->isHidden());
    for (int i = 0; i < 6; ++i) EXPECT_TRUE(dialog.directionButton(i)->toolTip().isEmpty()) << "only the last button has a tooltip";
    EXPECT_TRUE(dialog.directionButton(6)->toolTip().startsWith(QStringLiteral("Definir dirección")));
    dialog.directionButton(6)->click();
    EXPECT_EQ(dialog.direction(), LoadDirection::UserVector);
    EXPECT_FALSE(dialog.vectorGroup()->isHidden());
    EXPECT_GT(dialog.height(), 295);
    EXPECT_EQ(dialog.vectorEdit(0)->text(), QStringLiteral("1.000"));
    EXPECT_EQ(dialog.vectorEdit(1)->text(), QStringLiteral("0.000"));
    EXPECT_EQ(dialog.vectorEdit(2)->text(), QStringLiteral("0.000"));
    dialog.directionButton(0)->click();
    EXPECT_EQ(dialog.direction(), LoadDirection::PlusX);
    EXPECT_TRUE(dialog.vectorGroup()->isHidden());
    EXPECT_FALSE(dialog.directionButton(6)->isChecked());
}

TEST_F(NodalLoadToolTest, NewDialogValidationKeepsTheWindowOpen) {
    NodalLoadDialog dialog(QStringLiteral("x"), project.loadCases, NodalLoadData{});
    for (const char* bad : {"0", "-7.256", "49000.01"}) {
        shown.clear();
        dialog.valueEdit()->setText(QString::fromLatin1(bad));
        dialog.accept();
        ASSERT_EQ(shown.size(), 1u) << bad;
        EXPECT_EQ(shown[0].kind, messages::Kind::Warning);
        EXPECT_EQ(shown[0].text, QStringLiteral("El valor de 'Valor' debe estar entre 0.01 y 49000.00."));
        EXPECT_NE(dialog.result(), QDialog::Accepted);
    }
    shown.clear();
    dialog.valueEdit()->setText(QStringLiteral("abc"));
    dialog.accept();
    ASSERT_EQ(shown.size(), 1u);
    EXPECT_EQ(shown[0].text, QStringLiteral("La cadena introducida no es un número."));
    dialog.valueEdit()->setText(QStringLiteral("7.256"));
    dialog.accept();
    EXPECT_EQ(dialog.result(), QDialog::Accepted);
    EXPECT_EQ(dialog.valueEdit()->text(), QStringLiteral("7.26"));
    EXPECT_DOUBLE_EQ(dialog.data().magnitude, 7.26);
}

TEST_F(NodalLoadToolTest, NewDialogRejectsANullVector) {
    NodalLoadDialog dialog(QStringLiteral("x"), project.loadCases, NodalLoadData{});
    dialog.directionButton(6)->click();
    for (int i = 0; i < 3; ++i) dialog.vectorEdit(i)->setText(QStringLiteral("0"));
    dialog.accept();
    ASSERT_EQ(shown.size(), 1u);
    EXPECT_EQ(shown[0].text, QStringLiteral("La dirección introducida no es correcta."));
    EXPECT_NE(dialog.result(), QDialog::Accepted);
}

TEST_F(NodalLoadToolTest, NewCreatesOneLoadPerNodeAsOneUndoStepAndRemembersTheLast) {
    runner<NodalLoadDialog>([](NodalLoadDialog& d) {
        d.valueEdit()->setText(QStringLiteral("5"));
        d.directionButton(0)->click();  // +X
        return true;
    });
    tool->newAction()->trigger();
    selectWindow(1, 2);
    click(*view, QPointF(400, 500), Qt::RightButton);
    ASSERT_EQ(project.nodalLoads.items.size(), 2u);
    EXPECT_EQ(project.nodalLoads.items[0].nodeId, 1);
    EXPECT_DOUBLE_EQ(project.nodalLoads.items[0].magnitude, 5.0);
    EXPECT_EQ(project.nodalLoads.items[0].direction, LoadDirection::PlusX);
    EXPECT_EQ(undo.count(), 1u);
    EXPECT_TRUE(view->selection().empty()) << "the selection is cleared";
    EXPECT_EQ(tool->mode(), NodalLoadTool::Mode::New) << "the command stays active";
    // The next dialog starts with the last value and direction.
    NodalLoadData seen;
    tool->setDialogRunner([&seen](QDialog& dialog) {
        auto& d = static_cast<NodalLoadDialog&>(dialog);
        seen = d.data();
        return static_cast<int>(QDialog::Rejected);
    });
    selectWindow(3, 4);
    click(*view, QPointF(400, 500), Qt::RightButton);
    EXPECT_DOUBLE_EQ(seen.magnitude, 5.0);
    EXPECT_EQ(seen.direction, LoadDirection::PlusX);
    EXPECT_EQ(project.nodalLoads.items.size(), 2u) << "Cancelar changes nothing";
    undo.undo();
    EXPECT_TRUE(project.nodalLoads.empty());
}

TEST_F(NodalLoadToolTest, NewTakesTheShownHypothesisByDefault) {
    const int cm1 = addCase();
    tool->newAction()->trigger();
    tool->panel()->setCaseId(cm1);  // emits viewChanged
    EXPECT_EQ(tool->viewCaseId(), cm1);
    int offered = -1;
    tool->setDialogRunner([&offered](QDialog& dialog) {
        offered = static_cast<NodalLoadDialog&>(dialog).data().loadCaseId;
        return static_cast<int>(QDialog::Rejected);
    });
    EXPECT_FALSE(tool->createLoads({1}));
    EXPECT_EQ(offered, cm1);
    NodalLoadDialog dialog(QStringLiteral("x"), project.loadCases, NodalLoadData{});
    ASSERT_EQ(dialog.caseCombo()->count(), 2);
    EXPECT_EQ(dialog.caseCombo()->itemText(1), QStringLiteral("CM 1"));
}

// ---- Editar / Hipótesis / Borrar ---------------------------------------------------------------------------------------------

TEST_F(NodalLoadToolTest, OnlyNodesWithLoadsCanBeCapturedAndEditOverwritesAll) {
    addLoad(2, 5.0, LoadDirection::PlusX);
    addLoad(2, 3.0, LoadDirection::PlusZ);
    undo.setClean();
    const std::size_t before = undo.count();
    NodalLoadData offered;
    runner<NodalLoadDialog>([&offered](NodalLoadDialog& d) {
        offered = d.data();
        EXPECT_EQ(d.windowTitle(), QStringLiteral("Descripción de la carga"));
        d.valueEdit()->setText(QStringLiteral("8"));
        d.directionButton(3)->click();  // -X
        return true;
    });
    tool->editAction()->trigger();
    selectWindow(1, 4);  // N1, N3 and N4 have no load: only N2 is captured
    EXPECT_EQ(view->selection().ids(ElementKind::Node), (std::vector<int>{2}));
    click(*view, QPointF(400, 500), Qt::RightButton);
    EXPECT_DOUBLE_EQ(offered.magnitude, 5.0) << "filled with the first selected load";
    EXPECT_EQ(offered.direction, LoadDirection::PlusX);
    ASSERT_EQ(project.nodalLoads.items.size(), 2u);
    for (const NodalLoad& l : project.nodalLoads.items) {
        EXPECT_DOUBLE_EQ(l.magnitude, 8.0);
        EXPECT_EQ(l.direction, LoadDirection::MinusX);
    }
    EXPECT_EQ(undo.count(), before + 1);
    EXPECT_TRUE(view->selection().empty());
}

TEST_F(NodalLoadToolTest, HypothesisChangesTheCaseOfTheSelectedLoads) {
    const int cm1 = addCase();
    addLoad(1, 5.0);
    tool->caseAction()->trigger();
    int shownCase = -1;
    runner<LoadCaseChoiceDialog>([&](LoadCaseChoiceDialog& d) {
        EXPECT_EQ(d.windowTitle(), QStringLiteral("Hipótesis"));
        EXPECT_EQ(d.combo()->count(), 2);
        shownCase = d.loadCaseId();
        d.combo()->setCurrentIndex(d.combo()->findData(cm1));
        return true;
    });
    selectWindow(1, 1);
    click(*view, QPointF(400, 500), Qt::RightButton);
    EXPECT_EQ(shownCase, 0);
    EXPECT_EQ(project.nodalLoads.items[0].loadCaseId, cm1);
    // The load is now in another hypothesis: with "Peso propio" in view nothing can be captured.
    EXPECT_TRUE(tool->loadsOfNodes({1}).empty());
    tool->setView(0, true);  // Ver todas
    EXPECT_EQ(tool->loadsOfNodes({1}).size(), 1u);
}

TEST_F(NodalLoadToolTest, DeleteActsAtOnceWithoutAQuestion) {
    addLoad(1, 5.0);
    addLoad(3, 2.0);
    undo.setClean();
    tool->deleteAction()->trigger();
    selectWindow(1, 1);
    click(*view, QPointF(400, 500), Qt::RightButton);
    ASSERT_EQ(project.nodalLoads.items.size(), 1u);
    EXPECT_EQ(project.nodalLoads.items[0].nodeId, 3);
    EXPECT_TRUE(shown.empty());
    undo.undo();
    EXPECT_EQ(project.nodalLoads.items.size(), 2u);
    undo.redo();
    // Deleting the last load disables the three buttons and ends the command.
    selectWindow(3, 3);
    click(*view, QPointF(400, 500), Qt::RightButton);
    EXPECT_TRUE(project.nodalLoads.empty());
    EXPECT_FALSE(tool->deleteAction()->isEnabled());
    EXPECT_EQ(tool->mode(), NodalLoadTool::Mode::None);
}

// ---- Escalas ----------------------------------------------------------------------------------------------------------------

TEST_F(NodalLoadToolTest, ScalesDialogMatchesTheMeasuredOne) {
    LoadScalesDialog dialog(project.loadCases, project.nodalLoads.scales);
    EXPECT_EQ(dialog.windowTitle(), QStringLiteral("Escalas"));
    EXPECT_EQ(dialog.fixedRadio()->text(), QStringLiteral("Proporcionales a un valor fijo"));
    EXPECT_EQ(dialog.proportionalRadio()->text(), QStringLiteral("Proporcionales al valor de la carga"));
    EXPECT_TRUE(dialog.fixedRadio()->isChecked());
    EXPECT_EQ(dialog.sizeEdit()->text(), QStringLiteral("0.500"));
    EXPECT_FALSE(dialog.fixedBlock()->isHidden()) << "only the block of the selected radio is shown (measured)";
    EXPECT_TRUE(dialog.proportionalBlock()->isHidden());
    ASSERT_EQ(dialog.table()->rowCount(), 1);
    EXPECT_EQ(dialog.table()->horizontalHeaderItem(0)->text(), QStringLiteral("Hipótesis"));
    EXPECT_EQ(dialog.table()->horizontalHeaderItem(1)->text(), QStringLiteral("Escala"));
    EXPECT_EQ(dialog.table()->item(0, 0)->text(), QStringLiteral("Peso propio"));
    EXPECT_EQ(dialog.table()->item(0, 1)->text(), QStringLiteral("1.000"));
    dialog.proportionalRadio()->setChecked(true);
    EXPECT_TRUE(dialog.fixedBlock()->isHidden());
    EXPECT_FALSE(dialog.proportionalBlock()->isHidden());
    const QStringList typeValues{"1.000", "1.000", "1.000", "1.000", "1.000", "1.000", "1.000", "1.000", "100.000"};
    for (int t = 0; t < 9; ++t) EXPECT_EQ(dialog.typeEdit(t)->text(), typeValues[t]) << t;
    EXPECT_EQ(dialog.typeEdit(1)->toolTip(), QStringLiteral("Debe estar entre 0.001  y 1000.000 ."));
    dialog.typeEdit(1)->setText(QStringLiteral("0"));
    dialog.accept();
    EXPECT_NE(dialog.result(), QDialog::Accepted) << "type scale outside 0.001..1000.000";
    dialog.typeEdit(1)->setText(QStringLiteral("2"));
    shown.clear();
    dialog.table()->item(0, 1)->setText(QStringLiteral("2"));
    dialog.accept();
    ASSERT_EQ(dialog.result(), QDialog::Accepted);
    EXPECT_DOUBLE_EQ(dialog.scales().perType[1], 2.0);
    EXPECT_EQ(dialog.scales().mode, LoadScaleMode::Proportional);
    EXPECT_DOUBLE_EQ(loadScaleOfCase(dialog.scales(), 0), 2.0);
    dialog.fixedRadio()->setChecked(true);
    dialog.sizeEdit()->setText(QStringLiteral("0"));
    dialog.accept();
    EXPECT_EQ(shown.size(), 1u) << "a null size is refused";
}

TEST_F(NodalLoadToolTest, EscalasThroughTheToolIsOneUndoStep) {
    runner<LoadScalesDialog>([](LoadScalesDialog& d) {
        d.sizeEdit()->setText(QStringLiteral("0.75"));
        return true;
    });
    EXPECT_TRUE(tool->editScales());
    EXPECT_DOUBLE_EQ(project.nodalLoads.scales.fixedSizeM, 0.75);
    EXPECT_EQ(undo.count(), 1u);
}

// ---- Desplazamientos prescritos ---------------------------------------------------------------------------------------------

TEST_F(NodalLoadToolTest, PrescribedDialogShowsOnlyTheConstrainedColumns) {
    std::array<bool, 6> all;
    all.fill(true);
    PrescribedDisplacementsDialog full(project.loadCases, all, {});
    EXPECT_EQ(full.windowTitle(), QStringLiteral("Desplazamientos prescritos"));
    EXPECT_EQ(full.size(), QSize(530, 370));
    const QStringList expected{"Hipótesis", "Dx (mm)", "Dy (mm)", "Dz (mm)", "Gx (mRad)", "Gy (mRad)", "Gz (mRad)"};
    ASSERT_EQ(full.table()->columnCount(), 7);
    for (int c = 0; c < 7; ++c) EXPECT_EQ(full.table()->horizontalHeaderItem(c)->text(), expected[c]);
    EXPECT_EQ(full.table()->rowCount(), 0) << "empty on opening";
    std::array<bool, 6> some{true, false, true, false, false, false};
    PrescribedDisplacementsDialog part(project.loadCases, some, {});
    ASSERT_EQ(part.table()->columnCount(), 3);
    EXPECT_EQ(part.table()->horizontalHeaderItem(1)->text(), QStringLiteral("Dx (mm)"));
    EXPECT_EQ(part.table()->horizontalHeaderItem(2)->text(), QStringLiteral("Dz (mm)"));
    EXPECT_EQ(part.columnOfDof(2), 2);
    EXPECT_EQ(part.columnOfDof(1), -1);
}

TEST_F(NodalLoadToolTest, PrescribedDialogRowsAndWarnings) {
    std::array<bool, 6> all;
    all.fill(true);
    PrescribedDisplacementsDialog dialog(project.loadCases, all, {});
    EXPECT_EQ(dialog.addButton()->toolTip(), QStringLiteral("Añadir / Añadir nuevo elemento a la lista"));
    dialog.addRow();
    ASSERT_EQ(dialog.table()->rowCount(), 1);
    auto* combo = qobject_cast<QComboBox*>(dialog.table()->cellWidget(0, 0));
    ASSERT_NE(combo, nullptr);
    EXPECT_EQ(combo->currentText(), QStringLiteral("Peso propio"));
    for (int c = 1; c <= 6; ++c) EXPECT_EQ(dialog.table()->item(0, c)->text(), QStringLiteral("0.000"));
    // All null: the first warning (#65466) and the window stays open.
    dialog.accept();
    ASSERT_EQ(shown.size(), 1u);
    EXPECT_EQ(shown[0].kind, messages::Kind::Warning);
    EXPECT_EQ(shown[0].text, QStringLiteral("Se ha introducido un conjunto de desplazamientos prescritos con todos los valores nulos."));
    EXPECT_NE(dialog.result(), QDialog::Accepted);
    dialog.setCellText(0, 1, QStringLiteral("7.5"));
    dialog.setCellText(0, 3, QStringLiteral("-2.25"));
    EXPECT_EQ(dialog.table()->item(0, 1)->text(), QStringLiteral("7.500"));
    EXPECT_EQ(dialog.table()->item(0, 3)->text(), QStringLiteral("-2.250"));
    shown.clear();
    dialog.setCellText(0, 2, QStringLiteral("abc"));
    ASSERT_EQ(shown.size(), 1u);
    EXPECT_EQ(shown[0].text, QStringLiteral("La cadena introducida no es un número."));
    EXPECT_EQ(dialog.table()->item(0, 2)->text(), QStringLiteral("0.000"));
    // Copiar creates a second set in the same hypothesis: refused (#65465).
    dialog.copyRow();
    ASSERT_EQ(dialog.table()->rowCount(), 2);
    EXPECT_EQ(dialog.table()->item(1, 1)->text(), QStringLiteral("7.500"));
    shown.clear();
    dialog.accept();
    ASSERT_EQ(shown.size(), 1u);
    EXPECT_EQ(shown[0].text, QStringLiteral("Se ha introducido más de un conjunto de desplazamientos prescritos en la misma hipótesis de carga."));
    dialog.removeRow();
    ASSERT_EQ(dialog.rows().size(), 1u);
    shown.clear();
    dialog.accept();
    EXPECT_TRUE(shown.empty());
    EXPECT_EQ(dialog.result(), QDialog::Accepted);
    EXPECT_DOUBLE_EQ(dialog.rows()[0].values[0], 7.5);
    EXPECT_DOUBLE_EQ(dialog.rows()[0].values[2], -2.25);
}

TEST_F(NodalLoadToolTest, PrescribedThroughTheToolStoresTheSetsAsOneUndoStep) {
    fixNode(1);
    fixNode(3);
    undo.setClean();
    const std::size_t before = undo.count();
    runner<PrescribedDisplacementsDialog>([](PrescribedDisplacementsDialog& d) {
        EXPECT_EQ(d.table()->columnCount(), 7) << "a fixed node blocks the six DOFs";
        d.addRow();
        d.setCellText(0, 3, QStringLiteral("-12"));
        return true;
    });
    tool->prescribedAction()->trigger();
    selectWindow(1, 4);  // N2 and N4 have no support: only N1 and N3 are captured
    EXPECT_EQ(view->selection().ids(ElementKind::Node), (std::vector<int>{1, 3}));
    click(*view, QPointF(400, 500), Qt::RightButton);
    EXPECT_EQ(undo.count(), before + 1);
    for (int node : {1, 3}) {
        const ExternalSupport* s = externalSupportOf(project.structure, project.nodeSupports, node);
        ASSERT_NE(s, nullptr);
        EXPECT_EQ(s->type, ExternalSupportType::Fixed) << "the support itself is untouched";
        ASSERT_EQ(s->prescribed.size(), 1u);
        EXPECT_DOUBLE_EQ(s->prescribed[0].values[2], -12.0);
    }
    undo.undo();
    EXPECT_TRUE(externalSupportOf(project.structure, project.nodeSupports, 1)->prescribed.empty());
    EXPECT_TRUE(externalSupportOf(project.structure, project.nodeSupports, 3)->prescribed.empty());
}

TEST_F(NodalLoadToolTest, PrescribedDialogReopensWithTheStoredSet) {
    fixNode(1);
    ExternalSupport s = *externalSupportOf(project.structure, project.nodeSupports, 1);
    PrescribedDisplacement p;
    p.loadCaseId = 0;
    p.values[1] = 4.0;
    s.prescribed = {p};
    ASSERT_EQ(assignExternalSupport(project, undo, {1}, s), 1);
    int rows = -1;
    QString dy;
    runner<PrescribedDisplacementsDialog>([&](PrescribedDisplacementsDialog& d) {
        rows = d.table()->rowCount();
        dy = d.table()->item(0, 2)->text();
        return false;
    });
    EXPECT_FALSE(tool->editPrescribed({1}));
    EXPECT_EQ(rows, 1);
    EXPECT_EQ(dy, QStringLiteral("4.000"));
}

// ---- 3D ---------------------------------------------------------------------------------------------------------------------

TEST_F(NodalLoadToolTest, LoadLineComesFromTheSideOfOrigin) {
    addLoad(2, 5.0, LoadDirection::PlusX);
    addLoad(2, 5.0, LoadDirection::MinusZ);
    addLoad(2, 5.0, LoadDirection::PlusZ);
    const QPointF node = screenOf(2);
    QPointF start, tip;
    // +X: the line starts to the left of the node and its head touches the node.
    ASSERT_TRUE(view->nodalLoadSegment(project.nodalLoads.items[0], start, tip));
    EXPECT_NEAR(tip.x(), node.x(), 1e-6);
    EXPECT_LT(start.x(), node.x());
    EXPECT_NEAR(start.y(), node.y(), 1e-6);
    // -Z (down) is drawn above the node (measured), +Z below it.
    ASSERT_TRUE(view->nodalLoadSegment(project.nodalLoads.items[1], start, tip));
    EXPECT_LT(start.y(), node.y());
    // Fixed mode (default 0.500 m): every load is 0.5 m long in the model, whatever its value.
    const double pxPerMetre = std::hypot(screenOf(3).x() - node.x(), screenOf(3).y() - node.y()) / 2.0;
    EXPECT_NEAR(std::hypot(tip.x() - start.x(), tip.y() - start.y()), 0.5 * pxPerMetre, 1e-6);
    ASSERT_TRUE(view->nodalLoadSegment(project.nodalLoads.items[2], start, tip));
    EXPECT_GT(start.y(), node.y());
}

TEST_F(NodalLoadToolTest, LoadLengthFollowsEscalasLikeTheOriginal) {
    addLoad(1, 5.0, LoadDirection::PlusX);
    addLoad(2, 20.0, LoadDirection::PlusX);
    const double pxPerMetre = std::hypot(screenOf(3).x() - screenOf(2).x(), screenOf(3).y() - screenOf(2).y()) / 2.0;
    const auto lengthPx = [&](int index) {
        QPointF start, tip;
        EXPECT_TRUE(view->nodalLoadSegment(project.nodalLoads.items[static_cast<std::size_t>(index)], start, tip));
        return std::hypot(tip.x() - start.x(), tip.y() - start.y());
    };
    // Measured in the original: 0.5 m -> 50 px, 1.0 m -> 100 px for every value (100 px per metre there).
    EXPECT_NEAR(lengthPx(0), 0.5 * pxPerMetre, 1e-6);
    EXPECT_NEAR(lengthPx(1), 0.5 * pxPerMetre, 1e-6);
    LoadScales s = project.nodalLoads.scales;
    s.fixedSizeM = 1.0;
    ASSERT_EQ(setLoadScales(project, undo, s), NodalLoadError::None);
    EXPECT_NEAR(lengthPx(0), pxPerMetre, 1e-6);
    // Proportional: 5 kN -> 0.5 m, 20 kN -> 2 m; case 2.000 or "Puntual" 2.000 doubles both.
    s.mode = LoadScaleMode::Proportional;
    ASSERT_EQ(setLoadScales(project, undo, s), NodalLoadError::None);
    EXPECT_NEAR(lengthPx(0), 0.5 * pxPerMetre, 1e-6);
    EXPECT_NEAR(lengthPx(1), 2.0 * pxPerMetre, 1e-6);
    s.perCase[0] = 2.0;
    ASSERT_EQ(setLoadScales(project, undo, s), NodalLoadError::None);
    EXPECT_NEAR(lengthPx(0), pxPerMetre, 1e-6);
    s.perCase.clear();
    s.perType[static_cast<std::size_t>(LoadScaleType::Punctual)] = 2.0;
    ASSERT_EQ(setLoadScales(project, undo, s), NodalLoadError::None);
    EXPECT_NEAR(lengthPx(1), 4.0 * pxPerMetre, 1e-6);
}

TEST_F(NodalLoadToolTest, LoadTextHasNoUnitNorTrailingZeros) {
    EXPECT_EQ(Viewport3D::nodalLoadText(5.0), QStringLiteral("5"));
    EXPECT_EQ(Viewport3D::nodalLoadText(7.256), QStringLiteral("7.26"));
    EXPECT_EQ(Viewport3D::nodalLoadText(9.81), QStringLiteral("9.81"));
    EXPECT_EQ(Viewport3D::nodalLoadText(12.5), QStringLiteral("12.5"));
}

TEST_F(NodalLoadToolTest, ViewStateReachesTheViewports) {
    const int cm1 = addCase();
    EXPECT_TRUE(view->loadsVisible());
    tool->setLoadsVisible(false);
    EXPECT_FALSE(view->loadsVisible());
    tool->setLoadsVisible(true);
    ASSERT_TRUE(view->loadViewCase().has_value());
    EXPECT_EQ(*view->loadViewCase(), 0);
    tool->setView(cm1, false);
    EXPECT_EQ(*view->loadViewCase(), cm1);
    tool->setView(cm1, true);
    EXPECT_FALSE(view->loadViewCase().has_value()) << "Ver todas";
    EXPECT_EQ(view->nodalLoads(), &project.nodalLoads);
}

TEST_F(NodalLoadToolTest, DeletingTheShownCaseFallsBackToSelfWeight) {
    const int cm1 = addCase();
    tool->setView(cm1, false);
    EXPECT_EQ(deleteLoadCase(project, undo, cm1), LoadCaseError::None);
    EXPECT_EQ(tool->viewCaseId(), 0);
}

// ---- Diafragmas: hipótesis reales (TK-089) -----------------------------------------------------------------------------------

TEST_F(NodalLoadToolTest, DiaphragmLoadsListTheRealLoadCases) {
    const int cm1 = addCase();
    Diaphragm d = newDiaphragm();
    d.reference = "D1";
    DiaphragmLoad stale = defaultDiaphragmLoad(77);  // a case that does not exist
    d.loads.push_back(stale);
    cc::app::DiaphragmEditDialog dialog({d}, project.loadCases);
    ASSERT_EQ(dialog.loadTable()->rowCount(), 1);
    auto* combo = qobject_cast<QComboBox*>(dialog.loadTable()->cellWidget(0, 0));
    ASSERT_NE(combo, nullptr);
    ASSERT_EQ(combo->count(), 2);
    EXPECT_EQ(combo->itemText(0), QStringLiteral("Peso propio"));
    EXPECT_EQ(combo->itemText(1), QStringLiteral("CM 1"));
    EXPECT_EQ(combo->currentData().toInt(), 0) << "a stale case becomes the first one";
    EXPECT_EQ(dialog.rows()[0].loads[0].loadCaseId, 0);
    dialog.addLoad();
    EXPECT_EQ(dialog.rows()[0].loads[1].loadCaseId, 0);
    combo = qobject_cast<QComboBox*>(dialog.loadTable()->cellWidget(1, 0));
    combo->setCurrentIndex(1);
    EXPECT_EQ(dialog.rows()[0].loads[1].loadCaseId, cm1);
}

int main(int argc, char** argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    app();
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
