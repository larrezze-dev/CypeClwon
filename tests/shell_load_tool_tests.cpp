// Carga > Láminas (T-P7-04, GUI part): dialogs, tool and viewport data. Expected values: docs/research/ui/cargas_laminas.md (measured in the
// original, sections 8-10). Never shown on screen (offscreen).
#include <gtest/gtest.h>

#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QRadioButton>
#include <QToolButton>
#include <memory>

#include "NodalLoadDialogs.h"
#include "NodalLoadTool.h"
#include "ShellLoadDialogs.h"
#include "ShellLoadTool.h"
#include "UserMessages.h"
#include "Viewport3D.h"
#include "core/load_case_edit.h"

using namespace cc::core;
using cc::app::NodalLoadTool;
using cc::app::ShellLoadDialog;
using cc::app::ShellLoadTool;
using cc::app::ShellSelfWeightDialog;
using cc::app::Viewport3D;
namespace messages = cc::app::messages;

namespace {

QApplication* app() {
    static int argc = 1;
    static char name[] = "shell_load_tool_tests";
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

/// Four nodes 4 x 4 m on Z = 0 seen from the top and one shell L1 of 100 mm of concrete over them.
class ShellLoadToolTest : public ::testing::Test {
protected:
    void SetUp() override {
        app();
        project.structure.addNode({0, 0, 0});
        project.structure.addNode({4, 0, 0});
        project.structure.addNode({4, 4, 0});
        project.structure.addNode({0, 4, 0});
        ASSERT_TRUE(project.structure.addShell({1, 2, 3, 4}, 0.1) > 0);
        view = std::make_unique<Viewport3D>();
        view->resize(800, 600);
        view->setStandardView(cc::core::StandardView::Top);
        view->setOrthographic(true);
        view->camera().target = {2, 2, 0};
        view->setStructure(&project.structure);
        nodal = std::make_unique<NodalLoadTool>();
        nodal->setProject(&project, &undo);
        nodal->addViewport(view.get());
        nodal->setLoadsVisible(true);
        tool = std::make_unique<ShellLoadTool>();
        tool->setHost(nodal.get());
        tool->setProject(&project, &undo);
        tool->addViewport(view.get());
        undo.setChangeCallback([this] {
            nodal->structureChanged();
            tool->structureChanged();
        });
        QObject::connect(tool.get(), &ShellLoadTool::statusChanged, [this](const QString& text) { status = text; });
        QObject::connect(tool.get(), &ShellLoadTool::aboutToActivate, nodal.get(), &NodalLoadTool::deactivate);
        QObject::connect(nodal.get(), &NodalLoadTool::aboutToActivate, tool.get(), &ShellLoadTool::deactivate);
        shown.clear();
        messages::handler() = [this](QWidget*, messages::Kind kind, const QString& text) {
            shown.push_back({kind, text});
            return true;
        };
        undo.setClean();
    }
    void TearDown() override { messages::handler() = nullptr; }
    int shellId() const { return project.structure.shells.front().id; }
    QPointF corner(int nodeId) {
        QPointF p;
        const Node& n = *project.structure.findNode(nodeId);
        EXPECT_TRUE(view->worldToScreen({n.position.x, n.position.y, n.position.z}, p));
        return p;
    }
    QPointF centre() {
        QPointF p;
        EXPECT_TRUE(view->worldToScreen({2, 2, 0}, p));
        return p;
    }
    void selectShell() { click(*view, centre()); }
    void act() { click(*view, QPointF(5, 590), Qt::RightButton); }
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
    void addUniform(double value = 5.0, int loadCase = 0) {
        ShellLoadData d = shellLoadDefaults(loadCase);
        d.value1 = value;
        ASSERT_EQ(addShellLoads(project, undo, {shellId()}, d), ShellLoadError::None);
    }
    Project project;
    UndoStack undo;
    std::unique_ptr<Viewport3D> view;
    std::unique_ptr<NodalLoadTool> nodal;
    std::unique_ptr<ShellLoadTool> tool;
    QString status;
    std::vector<Shown> shown;
};

}  // namespace

// ---- buttons, tooltips and status --------------------------------------------------------------------------------------------

TEST_F(ShellLoadToolTest, ButtonsFollowTheData) {
    EXPECT_EQ(tool->newAction()->text(), QStringLiteral("Nueva"));
    EXPECT_EQ(tool->editAction()->text(), QStringLiteral("Editar"));
    EXPECT_EQ(tool->deleteAction()->text(), QStringLiteral("Borrar"));
    EXPECT_EQ(tool->caseAction()->text(), QStringLiteral("Hipótesis"));
    EXPECT_EQ(tool->selfWeightAction()->text(), QStringLiteral("Peso propio"));
    // Measured (cl_a): without loads Editar, Borrar and Hipótesis are grey; Nueva and Peso propio are active.
    EXPECT_TRUE(tool->newAction()->isEnabled());
    EXPECT_TRUE(tool->selfWeightAction()->isEnabled());
    EXPECT_FALSE(tool->editAction()->isEnabled());
    EXPECT_FALSE(tool->deleteAction()->isEnabled());
    EXPECT_FALSE(tool->caseAction()->isEnabled());
    addUniform();
    EXPECT_TRUE(tool->editAction()->isEnabled());
    EXPECT_TRUE(tool->deleteAction()->isEnabled());
    EXPECT_TRUE(tool->caseAction()->isEnabled());
    undo.undo();
    EXPECT_FALSE(tool->editAction()->isEnabled());
    ShellLoadTool other;
    EXPECT_FALSE(other.newAction()->isEnabled()) << "no project";
    Project empty;
    UndoStack emptyUndo;
    other.setProject(&empty, &emptyUndo);
    EXPECT_FALSE(other.newAction()->isEnabled()) << "no shells";
    EXPECT_FALSE(other.selfWeightAction()->isEnabled());
}

TEST_F(ShellLoadToolTest, TooltipsAreTheMeasuredOnes) {
    const QString nueva = tool->newAction()->toolTip();
    EXPECT_TRUE(nueva.contains(QStringLiteral("Introducción de cargas sobre un grupo de láminas. Seleccione una o varias láminas y pulse el botón derecho del ratón.")));
    EXPECT_TRUE(nueva.contains(QStringLiteral("'Introducir cargas sobre láminas'")));
    EXPECT_FALSE(tool->caseAction()->toolTip().contains(QStringLiteral("<br>"))) << "Hipótesis has only its title";
    const QString pp = tool->selfWeightAction()->toolTip();
    EXPECT_TRUE(pp.contains(QStringLiteral("Generación de las cargas de peso propio de las láminas")));
    EXPECT_TRUE(pp.contains(QStringLiteral("seleccione las láminas para las que desea activar o desactivar")));
}

TEST_F(ShellLoadToolTest, StatusTextAndSharedViewPanel) {
    tool->newAction()->trigger();
    EXPECT_EQ(tool->mode(), ShellLoadTool::Mode::New);
    EXPECT_EQ(status, QStringLiteral("Láminas - Nueva |  - LMB: Seleccione un grupo de elementos. - RMB: Actuar sobre los elementos seleccionados."));
    EXPECT_TRUE(view->selectionMode());
    ASSERT_NE(nodal->panel(), nullptr);
    EXPECT_EQ(nodal->panel()->windowTitle(), QStringLiteral("Propiedades - Hipótesis vista"));
    EXPECT_EQ(ShellLoadTool::statusText(ShellLoadTool::Mode::Edit).left(15), QStringLiteral("Láminas - Edita"));
    tool->newAction()->trigger();  // toggles off
    EXPECT_EQ(tool->mode(), ShellLoadTool::Mode::None);
    EXPECT_FALSE(view->selectionMode());
    EXPECT_EQ(nodal->panel(), nullptr);
    tool->newAction()->trigger();
    ASSERT_NE(nodal->panel(), nullptr);
    nodal->panel()->close();
    EXPECT_EQ(tool->mode(), ShellLoadTool::Mode::None) << "closing the window ends the command";
}

TEST_F(ShellLoadToolTest, OnlyShellsAreCaptured) {
    tool->newAction()->trigger();
    selectShell();
    EXPECT_EQ(view->selection().ids(ElementKind::Shell), (std::vector<int>{shellId()}));
    EXPECT_EQ(view->selectionFilter().shells, true);
    EXPECT_EQ(view->selectionFilter().nodes, false);
    EXPECT_EQ(view->selectionFilter().bars, false);
    nodal->setView(0, true);  // the host reconfigures its own viewports: the command must stay in selection mode
    EXPECT_TRUE(view->selectionMode());
}

// ---- the dialog ------------------------------------------------------------------------------------------------------------

TEST_F(ShellLoadToolTest, DialogDefaultsAndMeasuredLayout) {
    ShellLoadDialog dialog(QStringLiteral("Introducir cargas sobre láminas"), project.loadCases, shellLoadDefaults(), nullptr);
    EXPECT_EQ(dialog.size(), QSize(310, 420));
    EXPECT_EQ(dialog.caseCombo()->currentText(), QStringLiteral("Peso propio"));
    EXPECT_EQ(dialog.type(), ShellLoadType::Uniform);
    EXPECT_EQ(dialog.axes(), ShellLoadAxes::Local);
    EXPECT_EQ(dialog.sense(), ShellLoadSense::PlusZ);
    EXPECT_TRUE(dialog.typeButton(0)->isChecked());
    EXPECT_TRUE(dialog.senseButton(0)->isChecked());
    EXPECT_FALSE(dialog.senseButton(1)->isChecked());
    EXPECT_EQ(dialog.edit(ShellLoadField::Value1)->text(), QStringLiteral("9.810"));
    EXPECT_EQ(dialog.label(ShellLoadField::Value1)->text(), QStringLiteral("Valor"));
    EXPECT_EQ(dialog.unitLabel(ShellLoadField::Value1)->text(), QStringLiteral("kN/m²"));
    EXPECT_TRUE(dialog.edit(ShellLoadField::Value2)->parentWidget()->isHidden());
    EXPECT_TRUE(dialog.localRadio()->isChecked());
    EXPECT_FALSE(dialog.axesGroup()->isHidden());
    EXPECT_FALSE(dialog.senseGroup()->isHidden());
    // Measured: 5 type buttons (tooltips), two sense buttons, none of them a tab stop.
    const char* names[5] = {"Carga uniforme", "Carga variable en el eje x local", "Carga variable en el eje y local",
                            "Incremento de temperatura uniforme", "Retracción"};
    for (int i = 0; i < 5; ++i) {
        EXPECT_EQ(dialog.typeButton(i)->toolTip(), QString::fromUtf8(names[i])) << i;
        EXPECT_EQ(dialog.typeButton(i)->focusPolicy(), Qt::NoFocus);
    }
    EXPECT_EQ(dialog.senseButton(0)->toolTip(), QStringLiteral("Dirección según el eje Z, en sentido positivo."));
    for (int i = 0; i < 2; ++i) EXPECT_EQ(dialog.senseButton(i)->focusPolicy(), Qt::NoFocus);
    EXPECT_EQ(dialog.globalRadio()->font().bold(), false);
    EXPECT_EQ(dialog.localRadio()->font().bold(), true) << "the active radio is in bold";
}

TEST_F(ShellLoadToolTest, FieldsPerTypeAreTheMeasuredOnes) {
    ShellLoadDialog dialog(QStringLiteral("x"), project.loadCases, shellLoadDefaults(), nullptr);
    dialog.typeButton(1)->click();  // variable x
    EXPECT_EQ(dialog.type(), ShellLoadType::VariableX);
    EXPECT_EQ(dialog.label(ShellLoadField::Value1)->text(), QStringLiteral("Valor inicial"));
    EXPECT_EQ(dialog.label(ShellLoadField::Value2)->text(), QStringLiteral("Valor final"));
    EXPECT_EQ(dialog.edit(ShellLoadField::Value1)->text(), QStringLiteral("9.810"));
    EXPECT_EQ(dialog.edit(ShellLoadField::Value2)->text(), QStringLiteral("9.810"));
    EXPECT_FALSE(dialog.edit(ShellLoadField::Value2)->parentWidget()->isHidden());
    EXPECT_EQ(dialog.unitLabel(ShellLoadField::Value2)->text(), QStringLiteral("kN/m²"));
    dialog.typeButton(2)->click();  // variable y
    EXPECT_EQ(dialog.type(), ShellLoadType::VariableY);
    EXPECT_FALSE(dialog.axesGroup()->isHidden());
    dialog.typeButton(3)->click();  // temperature: neither axes nor sense
    EXPECT_TRUE(dialog.axesGroup()->isHidden());
    EXPECT_TRUE(dialog.senseGroup()->isHidden());
    EXPECT_TRUE(dialog.edit(ShellLoadField::Value2)->parentWidget()->isHidden());
    EXPECT_EQ(dialog.edit(ShellLoadField::Value1)->text(), QStringLiteral("10.00"));
    EXPECT_EQ(dialog.unitLabel(ShellLoadField::Value1)->text(), QStringLiteral("°C"));
    EXPECT_EQ(dialog.size(), QSize(310, 420)) << "the height does not change with the type";
    dialog.typeButton(4)->click();  // shrinkage
    EXPECT_EQ(dialog.edit(ShellLoadField::Value1)->text(), QStringLiteral("0.0001"));
    EXPECT_EQ(dialog.unitLabel(ShellLoadField::Value1)->text(), QString());
    EXPECT_TRUE(dialog.axesGroup()->isHidden());
    EXPECT_EQ(dialog.edit(ShellLoadField::Value1)->toolTip(), QStringLiteral("Debe estar entre -1000.0000  y 1000.0000 ."));
    dialog.typeButton(0)->click();
    EXPECT_EQ(dialog.edit(ShellLoadField::Value1)->text(), QStringLiteral("9.810"));
    EXPECT_FALSE(dialog.axesGroup()->isHidden());
    EXPECT_TRUE(dialog.typeButton(0)->isChecked());
    EXPECT_FALSE(dialog.typeButton(4)->isChecked());
}

TEST_F(ShellLoadToolTest, AxesAndSenseButtons) {
    ShellLoadDialog dialog(QStringLiteral("x"), project.loadCases, shellLoadDefaults(), nullptr);
    dialog.globalRadio()->click();
    EXPECT_EQ(dialog.axes(), ShellLoadAxes::Global);
    EXPECT_TRUE(dialog.globalRadio()->font().bold());
    EXPECT_FALSE(dialog.localRadio()->font().bold());
    dialog.senseButton(1)->click();
    EXPECT_EQ(dialog.sense(), ShellLoadSense::MinusZ);
    EXPECT_TRUE(dialog.senseButton(1)->isChecked());
    EXPECT_FALSE(dialog.senseButton(0)->isChecked());
    const ShellLoadData d = dialog.data();
    EXPECT_EQ(d.axes, ShellLoadAxes::Global);
    EXPECT_EQ(d.sense, ShellLoadSense::MinusZ);
    // The choice is kept when switching between the uniform and the variable types.
    dialog.typeButton(1)->click();
    EXPECT_EQ(dialog.axes(), ShellLoadAxes::Global);
    EXPECT_EQ(dialog.sense(), ShellLoadSense::MinusZ);
}

TEST_F(ShellLoadToolTest, TabSequenceFollowsTheMeasuredOrder) {
    ShellLoadDialog dialog(QStringLiteral("x"), project.loadCases, shellLoadDefaults(), nullptr);
    using F = ShellLoadField;
    EXPECT_EQ(dialog.visibleFields(), (std::vector<F>{F::Value1}));
    dialog.typeButton(2)->click();
    EXPECT_EQ(dialog.visibleFields(), (std::vector<F>{F::Value1, F::Value2}));
    dialog.typeButton(4)->click();
    EXPECT_EQ(dialog.visibleFields(), (std::vector<F>{F::Value1}));
    // Measured cycle: Valor -> radios (one stop) -> Aceptar -> Cancelar -> Hipótesis -> Valor. The buttons are not stops.
    dialog.typeButton(0)->click();
    std::vector<QWidget*> order;
    QWidget* w = dialog.edit(F::Value1);
    for (int i = 0; i < 40 && order.size() < 5; ++i) {
        w = w->nextInFocusChain();
        if ((w->focusPolicy() & Qt::TabFocus) && w->isVisibleTo(&dialog) && w->isEnabled()) order.push_back(w);
    }
    ASSERT_EQ(order.size(), 5u);
    EXPECT_TRUE(order[0] == dialog.globalRadio() || order[0] == dialog.localRadio());
    EXPECT_TRUE(order[1] == dialog.globalRadio() || order[1] == dialog.localRadio());
    EXPECT_EQ(order[2], dialog.acceptButton());
    EXPECT_EQ(order[3], dialog.cancelButton());
    EXPECT_EQ(order[4], dialog.caseCombo());
}

TEST_F(ShellLoadToolTest, AcceptWarnsWithTheMeasuredMessagesAndKeepsTheWindowOpen) {
    ShellLoadDialog dialog(QStringLiteral("x"), project.loadCases, shellLoadDefaults(), nullptr);
    const auto expect = [&](const char* text, messages::Kind kind, const QString& message) {
        shown.clear();
        const QString before = dialog.edit(ShellLoadField::Value1)->text();
        dialog.edit(ShellLoadField::Value1)->setText(QString::fromLatin1(text));
        dialog.accept();
        ASSERT_EQ(shown.size(), 1u) << text;
        EXPECT_EQ(shown[0].kind, kind) << text;
        EXPECT_EQ(shown[0].text, message) << text;
        EXPECT_NE(dialog.result(), QDialog::Accepted);
        if (kind == messages::Kind::Warning) EXPECT_EQ(dialog.edit(ShellLoadField::Value1)->text(), before) << "the field recovers its value";
        dialog.edit(ShellLoadField::Value1)->setText(before);
    };
    const QString range = QStringLiteral("El valor de 'Valor' debe estar entre 0.000 kN/m² y 100000.000 kN/m².");
    expect("-1", messages::Kind::Warning, range);
    expect("100000.001", messages::Kind::Warning, range);
    expect("abc", messages::Kind::Warning, QStringLiteral("La cadena introducida no es un número."));
    expect("0", messages::Kind::Error, QStringLiteral("El valor introducido no puede ser cero."));
    expect("0.0004", messages::Kind::Error, QStringLiteral("El valor introducido no puede ser cero."));
    dialog.typeButton(3)->click();
    expect("1000.01", messages::Kind::Warning, QStringLiteral("El valor de 'Valor' debe estar entre -100.00 y 1000.00."));
    expect("0", messages::Kind::Error, QStringLiteral("El valor introducido no puede ser cero."));
    dialog.typeButton(4)->click();
    expect("1000.0001", messages::Kind::Warning, QStringLiteral("El valor de 'Valor' debe estar entre -1000.0000  y 1000.0000 ."));
    expect("0", messages::Kind::Error, QStringLiteral("El valor introducido no puede ser cero."));
    // Variable: only both null values are an error; the warning names the offending field.
    dialog.typeButton(1)->click();
    shown.clear();
    dialog.edit(ShellLoadField::Value1)->setText(QStringLiteral("0"));
    dialog.edit(ShellLoadField::Value2)->setText(QStringLiteral("0"));
    dialog.accept();
    ASSERT_EQ(shown.size(), 1u);
    EXPECT_EQ(shown[0].kind, messages::Kind::Error);
    dialog.edit(ShellLoadField::Value2)->setText(QStringLiteral("100001"));
    shown.clear();
    dialog.accept();
    ASSERT_EQ(shown.size(), 1u);
    EXPECT_EQ(shown[0].text, QStringLiteral("El valor de 'Valor final' debe estar entre 0.000 kN/m² y 100000.000 kN/m²."));
    dialog.edit(ShellLoadField::Value2)->setText(QStringLiteral("5"));
    shown.clear();
    dialog.accept();
    EXPECT_TRUE(shown.empty()) << "one null value is accepted";
    EXPECT_EQ(dialog.result(), QDialog::Accepted);
}

TEST_F(ShellLoadToolTest, SelfWeightDialogIsTheMeasuredOne) {
    ShellSelfWeightDialog dialog(true);
    EXPECT_EQ(dialog.size(), QSize(186, 134));
    EXPECT_TRUE(dialog.activateRadio()->isChecked());
    EXPECT_EQ(dialog.windowTitle(), QStringLiteral("Generación de cargas"));
    ShellSelfWeightDialog off(false);
    EXPECT_TRUE(off.deactivateRadio()->isChecked());
    EXPECT_FALSE(off.activate());
}

// ---- Nueva ------------------------------------------------------------------------------------------------------------------

TEST_F(ShellLoadToolTest, NewCreatesTheLoadAsOneUndoStepAndRemembersTheLast) {
    runner<ShellLoadDialog>([](ShellLoadDialog& d) {
        d.typeButton(3)->click();  // temperature
        d.edit(ShellLoadField::Value1)->setText(QStringLiteral("20"));
        return true;
    });
    tool->newAction()->trigger();
    selectShell();
    act();
    ASSERT_EQ(project.shellLoads.items.size(), 1u);
    const ShellLoad& load = project.shellLoads.items[0];
    EXPECT_EQ(load.shellId, shellId());
    EXPECT_EQ(load.type, ShellLoadType::UniformTemperature);
    EXPECT_DOUBLE_EQ(load.value1, 20.0);
    EXPECT_EQ(undo.count(), 1u);
    EXPECT_TRUE(view->selection().empty());
    EXPECT_EQ(tool->mode(), ShellLoadTool::Mode::New) << "the command stays active";
    ShellLoadData seen;
    tool->setDialogRunner([&seen](QDialog& dialog) {
        seen = static_cast<ShellLoadDialog&>(dialog).data();
        return static_cast<int>(QDialog::Rejected);
    });
    selectShell();
    act();
    EXPECT_EQ(seen.type, ShellLoadType::UniformTemperature) << "the last type is remembered";
    EXPECT_DOUBLE_EQ(seen.value1, 10.0) << "the value is NOT remembered (measured, tanda 4): the default of the type";
    EXPECT_EQ(project.shellLoads.items.size(), 1u) << "Cancelar changes nothing";
    undo.undo();
    EXPECT_TRUE(project.shellLoads.items.empty());
}

TEST_F(ShellLoadToolTest, NewTakesTheShownHypothesisByDefault) {
    const int cm1 = addCase();
    tool->newAction()->trigger();
    nodal->panel()->setCaseId(cm1);
    int offered = -1;
    tool->setDialogRunner([&offered](QDialog& dialog) {
        offered = static_cast<ShellLoadDialog&>(dialog).data().loadCaseId;
        return static_cast<int>(QDialog::Rejected);
    });
    EXPECT_FALSE(tool->createLoads({shellId()}));
    EXPECT_EQ(offered, cm1);
}

TEST_F(ShellLoadToolTest, NewDefaultsToTheLastHypothesisUsedInTheDialogNotTheShownOne) {
    const int cm1 = addCase();
    NewLoadCase wind;
    wind.nature = LoadNature::Wind;
    wind.reference = "V 1";
    int v1 = 0;
    ASSERT_EQ(createLoadCase(project, undo, wind, &v1), LoadCaseError::None);
    tool->newAction()->trigger();
    nodal->panel()->setCaseId(cm1);
    std::vector<int> offered;
    QStringList items;
    runner<ShellLoadDialog>([&](ShellLoadDialog& d) {
        offered.push_back(d.data().loadCaseId);
        if (items.isEmpty())
            for (int i = 0; i < d.caseCombo()->count(); ++i) items << d.caseCombo()->itemText(i);
        if (offered.size() == 1) d.caseCombo()->setCurrentIndex(d.caseCombo()->findData(v1));
        return offered.size() <= 2;  // the third opening is cancelled
    });
    ASSERT_TRUE(tool->createLoads({shellId()}));
    ASSERT_EQ(offered.size(), 1u);
    EXPECT_EQ(offered[0], cm1) << "first time: the shown hypothesis";
    EXPECT_EQ(project.shellLoads.items.back().loadCaseId, v1);
    ASSERT_EQ(items.size(), 3) << "Peso propio, CM 1, V 1 (measured order)";
    EXPECT_EQ(items[0], QStringLiteral("Peso propio"));
    EXPECT_TRUE(items[1].startsWith(QStringLiteral("CM 1")));
    EXPECT_TRUE(items[2].startsWith(QStringLiteral("V 1")));
    EXPECT_TRUE(tool->createLoads({shellId()}));
    ASSERT_EQ(offered.size(), 2u);
    EXPECT_EQ(offered[1], v1) << "afterwards: the last one used in the dialog, although CM 1 is still shown";
    EXPECT_EQ(nodal->viewCaseId(), cm1);
    // A wind hypothesis changes nothing else in the dialog (measured: no eccentricity variant).
    ShellLoadData d = shellLoadDefaults(v1);
    EXPECT_EQ(d.sense, ShellLoadSense::PlusZ);
    EXPECT_EQ(d.axes, ShellLoadAxes::Local);
    // Deleting the remembered hypothesis falls back to the shown one.
    undo.undo();
    undo.undo();
    ASSERT_EQ(deleteLoadCase(project, undo, v1), LoadCaseError::None);
    tool->setDialogRunner([&offered](QDialog& dialog) {
        offered.push_back(static_cast<ShellLoadDialog&>(dialog).data().loadCaseId);
        return static_cast<int>(QDialog::Rejected);
    });
    EXPECT_FALSE(tool->createLoads({shellId()}));
    EXPECT_EQ(offered.back(), cm1);
}

TEST_F(ShellLoadToolTest, ViewHypothesisFiltersAndColoursTheDrawing) {
    const int cm1 = addCase();
    addUniform(3.0, 0);
    addUniform(4.0, cm1);
    tool->newAction()->trigger();
    nodal->panel()->setCaseId(cm1);
    ASSERT_TRUE(view->loadViewCase().has_value());
    EXPECT_EQ(*view->loadViewCase(), cm1) << "only the loads of the shown hypothesis are drawn";
    nodal->panel()->setViewAll(true);
    EXPECT_FALSE(view->loadViewCase().has_value()) << "Ver todas draws every hypothesis";
    EXPECT_FALSE(view->grab().isNull());
}

// ---- Editar / Hipótesis / Borrar ---------------------------------------------------------------------------------------------

TEST_F(ShellLoadToolTest, EditOverwritesTheFirstLoadAndOnlyShellsWithLoadsInViewAreCaptured) {
    EXPECT_FALSE(tool->editAction()->isEnabled());
    addUniform(3.0);
    ShellLoadData variable = shellLoadDefaults();
    variable.type = ShellLoadType::VariableX;
    variable.value1 = 5.0;
    variable.value2 = 15.0;
    ASSERT_EQ(addShellLoads(project, undo, {shellId()}, variable), ShellLoadError::None);
    tool->editAction()->trigger();
    EXPECT_EQ(tool->mode(), ShellLoadTool::Mode::Edit);
    selectShell();
    EXPECT_EQ(view->selection().ids(ElementKind::Shell).size(), 1u) << "the shell has loads in view";
    EXPECT_EQ(tool->loadsOfShells({shellId()}), (std::vector<int>{1})) << "one load per shell: the first created";
    ShellLoadData offered;
    QString title;
    runner<ShellLoadDialog>([&](ShellLoadDialog& d) {
        offered = d.data();
        title = d.windowTitle();
        d.edit(ShellLoadField::Value1)->setText(QStringLiteral("7"));
        return true;
    });
    act();
    EXPECT_EQ(title, QStringLiteral("Descripción de la carga"));
    EXPECT_DOUBLE_EQ(offered.value1, 3.0) << "the dialog shows the first load";
    ASSERT_EQ(project.shellLoads.items.size(), 2u);
    EXPECT_DOUBLE_EQ(project.shellLoads.items[0].value1, 7.0);
    EXPECT_EQ(project.shellLoads.items[1].type, ShellLoadType::VariableX) << "the second load is untouched";
    // A shell whose loads are not in the shown hypothesis cannot be captured.
    const int cm1 = addCase();
    nodal->panel()->setCaseId(cm1);
    view->clearSelection();
    selectShell();
    EXPECT_TRUE(view->selection().empty());
}

TEST_F(ShellLoadToolTest, DeleteRemovesOneLoadPerRmbWithoutAsking) {
    addUniform(3.0);
    addUniform(4.0);
    tool->deleteAction()->trigger();
    selectShell();
    act();
    ASSERT_EQ(project.shellLoads.items.size(), 1u);
    EXPECT_DOUBLE_EQ(project.shellLoads.items[0].value1, 4.0) << "the first created goes first";
    EXPECT_TRUE(shown.empty());
    selectShell();
    act();
    EXPECT_TRUE(project.shellLoads.items.empty());
    EXPECT_FALSE(tool->deleteAction()->isEnabled());
    EXPECT_EQ(tool->mode(), ShellLoadTool::Mode::None) << "without loads the command ends";
}

TEST_F(ShellLoadToolTest, CaseMovesTheLoad) {
    const int cm1 = addCase();
    addUniform(3.0);
    tool->caseAction()->trigger();
    EXPECT_EQ(tool->mode(), ShellLoadTool::Mode::Case);
    selectShell();
    runner<cc::app::LoadCaseChoiceDialog>([cm1](cc::app::LoadCaseChoiceDialog& d) {
        d.combo()->setCurrentIndex(d.combo()->findData(cm1));
        return true;
    });
    act();
    EXPECT_EQ(project.shellLoads.items.at(0).loadCaseId, cm1);
    EXPECT_EQ(tool->mode(), ShellLoadTool::Mode::Case) << "the command stays active, without any warning";
    EXPECT_TRUE(shown.empty());
}

// ---- Peso propio ------------------------------------------------------------------------------------------------------------

TEST_F(ShellLoadToolTest, SelfWeightAsksFirstAppliesByWindowAndRemembersTheOption) {
    int asked = 0;
    tool->setDialogRunner([&](QDialog& dialog) {
        auto* d = dynamic_cast<ShellSelfWeightDialog*>(&dialog);
        EXPECT_NE(d, nullptr);
        ++asked;
        if (asked == 1) d->deactivateRadio()->setChecked(true);
        d->accept();
        return static_cast<int>(QDialog::Accepted);
    });
    tool->selfWeightAction()->trigger();
    EXPECT_EQ(asked, 1) << "the dialog opens at once";
    EXPECT_EQ(tool->mode(), ShellLoadTool::Mode::SelfWeight);
    EXPECT_TRUE(view->selectionMode());
    EXPECT_TRUE(view->shellSelfWeightDrawn());
    // Measured: a plain click does not select in this mode; the window does.
    selectShell();
    EXPECT_TRUE(view->selection().empty());
    act();  // the click started a window: RMB cancels it
    EXPECT_TRUE(view->selection().empty());
    EXPECT_EQ(undo.count(), 0u);
    click(*view, corner(1) + QPointF(-30, 30));
    click(*view, corner(3) + QPointF(30, -30));
    EXPECT_EQ(view->selection().ids(ElementKind::Shell), (std::vector<int>{shellId()}));
    act();
    EXPECT_FALSE(shellSelfWeightActive(project.shellLoads, shellId()));
    EXPECT_TRUE(shown.empty()) << "no box (measured)";
    EXPECT_EQ(undo.count(), 1u);
    // RMB without a selection brings the dialog back.
    act();
    EXPECT_EQ(asked, 2);
    EXPECT_EQ(tool->session().selfWeightActivate, false) << "the dialog remembers the last option";
    undo.undo();
    EXPECT_TRUE(shellSelfWeightActive(project.shellLoads, shellId()));
}

TEST_F(ShellLoadToolTest, CancellingTheSelfWeightDialogLeavesTheCommand) {
    tool->setDialogRunner([](QDialog&) { return static_cast<int>(QDialog::Rejected); });
    tool->selfWeightAction()->trigger();
    EXPECT_EQ(tool->mode(), ShellLoadTool::Mode::None);
    EXPECT_FALSE(tool->selfWeightAction()->isChecked());
    EXPECT_FALSE(view->selectionMode());
}

// ---- viewport ---------------------------------------------------------------------------------------------------------------

TEST_F(ShellLoadToolTest, ViewportFollowsTheDataAndTheHistory) {
    EXPECT_EQ(view->shellLoads(), &project.shellLoads);
    addUniform(5.0);
    EXPECT_EQ(view->shellLoads()->items.size(), 1u);
    undo.undo();
    EXPECT_TRUE(view->shellLoads()->items.empty());
    undo.redo();
    EXPECT_EQ(view->shellLoads()->items.size(), 1u);
    // The window still paints (smoke test of the drawing code with every type).
    tool->newAction()->trigger();
    for (ShellLoadType t : kShellLoadTypeButtonOrder) {
        ShellLoadData d = shellLoadDefaults(0, t);
        d.sense = t == ShellLoadType::VariableX ? ShellLoadSense::MinusZ : ShellLoadSense::PlusZ;
        ASSERT_EQ(addShellLoads(project, undo, {shellId()}, d), ShellLoadError::None);
    }
    view->grab();
    EXPECT_FALSE(view->grab().isNull());
    tool->deactivate();
    EXPECT_FALSE(view->shellSelfWeightDrawn());
}

// Visual comparison aid: CC_SNAPSHOT_DIR=<dir> writes PNGs (offscreen) to compare with docs/screenshots/cl4_*.png.
TEST_F(ShellLoadToolTest, SnapshotsForVisualComparison) {
    const QByteArray dir = qgetenv("CC_SNAPSHOT_DIR");
    if (dir.isEmpty()) GTEST_SKIP() << "CC_SNAPSHOT_DIR not set";
    const QString base = QString::fromLocal8Bit(dir);
    const int cm1 = addCase();
    NewLoadCase wind;
    wind.nature = LoadNature::Wind;
    wind.reference = "V 1";
    int v1 = 0;
    ASSERT_EQ(createLoadCase(project, undo, wind, &v1), LoadCaseError::None);
    addUniform(9.81, cm1);
    addUniform(5.0, v1);
    tool->newAction()->trigger();
    nodal->panel()->setCaseId(cm1);
    view->grab().save(base + "/t4_view_cm1.png");
    nodal->panel()->setCaseId(v1);
    view->grab().save(base + "/t4_view_v1.png");
    nodal->panel()->setViewAll(true);
    view->grab().save(base + "/t4_view_all.png");
    ShellLoadDialog dialog(QStringLiteral("Introducir cargas sobre láminas"), project.loadCases, shellLoadDefaults(v1));
    dialog.grab().save(base + "/t4_dialog_new.png");
    cc::app::LoadCaseChoiceDialog choice(project.loadCases, cm1);
    choice.grab().save(base + "/t4_dialog_case.png");
    // Proportional scales: V 1 x2.
    auto scales = project.nodalLoads.scales;
    scales.mode = cc::core::LoadScaleMode::Proportional;
    scales.perCase[v1] = 2.0;
    project.nodalLoads.scales = scales;
    nodal->structureChanged();
    view->update();
    view->grab().save(base + "/t4_view_all_proportional.png");
}

int main(int argc, char** argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    app();
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
