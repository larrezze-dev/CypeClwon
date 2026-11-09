// Carga > Barras (T-P7-03, GUI part): dialogs, tool and viewport data. Expected values: docs/research/ui/cargas_barras.md (measured in the
// original, sections 10-12). Never shown on screen (offscreen).
#include <gtest/gtest.h>

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QRadioButton>
#include <QToolButton>
#include <memory>

#include "BarLoadDialogs.h"
#include "BarLoadTool.h"
#include "NodalLoadDialogs.h"
#include "NodalLoadTool.h"
#include "UserMessages.h"
#include "Viewport3D.h"
#include "core/load_case_edit.h"

using namespace cc::core;
using cc::app::BarLoadDialog;
using cc::app::BarLoadTool;
using cc::app::BarSelfWeightDialog;
using cc::app::LoadCaseChoiceDialog;
using cc::app::NodalLoadTool;
using cc::app::Viewport3D;
namespace messages = cc::app::messages;

namespace {

QApplication* app() {
    static int argc = 1;
    static char name[] = "bar_load_tool_tests";
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

/// Three nodes along X seen from the front: N1 (0,0,0), N2 (4,0,0), N3 (8,0,0); bars B1 (N1-N2) and B2 (N2-N3), 4 m long.
class BarLoadToolTest : public ::testing::Test {
protected:
    void SetUp() override {
        app();
        for (int i = 0; i < 3; ++i) project.structure.addNode({4.0 * i, 0, 0});
        project.structure.addBar(1, 2);
        project.structure.addBar(2, 3);
        view = std::make_unique<Viewport3D>();
        view->resize(800, 600);
        view->setStandardView(cc::core::StandardView::Front);
        view->setOrthographic(true);
        view->camera().target = {4, 0, 0};
        view->setStructure(&project.structure);
        nodal = std::make_unique<NodalLoadTool>();
        nodal->setProject(&project, &undo);
        nodal->addViewport(view.get());
        nodal->setLoadsVisible(true);
        tool = std::make_unique<BarLoadTool>();
        tool->setHost(nodal.get());
        tool->setProject(&project, &undo);
        tool->addViewport(view.get());
        undo.setChangeCallback([this] {
            nodal->structureChanged();
            tool->structureChanged();
        });
        QObject::connect(tool.get(), &BarLoadTool::statusChanged, [this](const QString& text) { status = text; });
        QObject::connect(tool.get(), &BarLoadTool::aboutToActivate, nodal.get(), &NodalLoadTool::deactivate);
        QObject::connect(nodal.get(), &NodalLoadTool::aboutToActivate, tool.get(), &BarLoadTool::deactivate);
        shown.clear();
        messages::handler() = [this](QWidget*, messages::Kind kind, const QString& text) {
            shown.push_back({kind, text});
            return true;
        };
        undo.setClean();
    }
    void TearDown() override { messages::handler() = nullptr; }
    QPointF midOf(int barId) {
        const Bar& b = *project.structure.findBar(barId);
        const Node& a = *project.structure.findNode(b.startNode);
        const Node& e = *project.structure.findNode(b.endNode);
        QPointF p;
        EXPECT_TRUE(view->worldToScreen({(a.position.x + e.position.x) / 2, 0, 0}, p));
        return p;
    }
    void selectBar(int barId) { click(*view, midOf(barId)); }
    void act() { click(*view, QPointF(400, 560), Qt::RightButton); }
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
    void addUniform(int bar, double value = 5.0, int loadCase = 0) {
        BarLoadData d = barLoadDefaults(loadCase);
        d.value1 = value;
        ASSERT_EQ(addBarLoads(project, undo, {bar}, d), BarLoadError::None);
    }
    Project project;
    UndoStack undo;
    std::unique_ptr<Viewport3D> view;
    std::unique_ptr<NodalLoadTool> nodal;
    std::unique_ptr<BarLoadTool> tool;
    QString status;
    std::vector<Shown> shown;
};

}  // namespace

// ---- buttons, tooltips and status --------------------------------------------------------------------------------------------

TEST_F(BarLoadToolTest, ButtonsFollowTheData) {
    EXPECT_EQ(tool->newAction()->text(), QStringLiteral("Nueva"));
    EXPECT_EQ(tool->editAction()->text(), QStringLiteral("Editar"));
    EXPECT_EQ(tool->deleteAction()->text(), QStringLiteral("Borrar"));
    EXPECT_EQ(tool->caseAction()->text(), QStringLiteral("Hipótesis"));
    EXPECT_EQ(tool->selfWeightAction()->text(), QStringLiteral("Peso propio"));
    // Measured (cb_btn_state): without loads Editar, Borrar and Hipótesis are grey; Nueva and Peso propio are active.
    EXPECT_TRUE(tool->newAction()->isEnabled());
    EXPECT_TRUE(tool->selfWeightAction()->isEnabled());
    EXPECT_FALSE(tool->editAction()->isEnabled());
    EXPECT_FALSE(tool->deleteAction()->isEnabled());
    EXPECT_FALSE(tool->caseAction()->isEnabled());
    addUniform(1);
    EXPECT_TRUE(tool->editAction()->isEnabled());
    EXPECT_TRUE(tool->deleteAction()->isEnabled());
    EXPECT_TRUE(tool->caseAction()->isEnabled());
    undo.undo();
    EXPECT_FALSE(tool->editAction()->isEnabled());
    BarLoadTool other;
    EXPECT_FALSE(other.newAction()->isEnabled()) << "no project";
}

TEST_F(BarLoadToolTest, TooltipsAreTheMeasuredOnes) {
    EXPECT_TRUE(tool->newAction()->toolTip().contains(QStringLiteral("Permite introducir cargas sobre un grupo de barras.")));
    EXPECT_TRUE(tool->newAction()->toolTip().contains(QStringLiteral("'Introducir cargas sobre barras'")));
    EXPECT_TRUE(tool->editAction()->toolTip().contains(QStringLiteral("Permite modificar los datos de las cargas ya introducidas.")));
    EXPECT_TRUE(tool->editAction()->toolTip().contains(QStringLiteral("'Editar cargas sobre barras'")));
    EXPECT_TRUE(tool->deleteAction()->toolTip().contains(QStringLiteral("Permite eliminar una o varias cargas vistas en pantalla.")));
    EXPECT_FALSE(tool->caseAction()->toolTip().contains(QStringLiteral("<br>"))) << "Hipótesis has only its title";
    const QString pp = tool->selfWeightAction()->toolTip();
    EXPECT_TRUE(pp.contains(QStringLiteral("Generación de las cargas de peso propio de las barras")));
    EXPECT_TRUE(pp.contains(QStringLiteral("Permite activar / desactivar la generación automática de la carga de peso propio.")));
    EXPECT_TRUE(pp.contains(QStringLiteral("Pulse 'F1' alternativamente para mostrar u ocultar los textos de ayuda.")));
}

TEST_F(BarLoadToolTest, StatusTextAndSharedViewPanel) {
    tool->newAction()->trigger();
    EXPECT_EQ(tool->mode(), BarLoadTool::Mode::New);
    EXPECT_EQ(status, QStringLiteral("Barras - Nueva |  - LMB: Seleccione un grupo de elementos. - RMB: Actuar sobre los elementos seleccionados."));
    EXPECT_TRUE(view->selectionMode());
    ASSERT_NE(nodal->panel(), nullptr);
    EXPECT_EQ(nodal->panel()->windowTitle(), QStringLiteral("Propiedades - Hipótesis vista"));
    EXPECT_EQ(BarLoadTool::statusText(BarLoadTool::Mode::Edit), QStringLiteral("Barras - Editar |  - LMB: Seleccione un grupo de elementos. - RMB: Actuar sobre los elementos seleccionados."));
    EXPECT_EQ(BarLoadTool::statusText(BarLoadTool::Mode::Delete).left(15), QStringLiteral("Barras - Borrar"));
    EXPECT_EQ(BarLoadTool::statusText(BarLoadTool::Mode::Case).left(18), QStringLiteral("Barras - Hipótesis"));
    tool->newAction()->trigger();  // toggles off
    EXPECT_EQ(tool->mode(), BarLoadTool::Mode::None);
    EXPECT_FALSE(view->selectionMode());
    EXPECT_EQ(nodal->panel(), nullptr) << "the window closes with the command";
    // Closing the window ends the command.
    tool->newAction()->trigger();
    ASSERT_NE(nodal->panel(), nullptr);
    nodal->panel()->close();
    EXPECT_EQ(tool->mode(), BarLoadTool::Mode::None);
    EXPECT_FALSE(tool->newAction()->isChecked());
}

TEST_F(BarLoadToolTest, OnlyBarsAreCapturedAndChangingTheViewKeepsTheCommandRunning) {
    tool->newAction()->trigger();
    selectBar(1);
    EXPECT_EQ(view->selection().ids(ElementKind::Bar), (std::vector<int>{1}));
    nodal->setView(0, true);  // the host reconfigures its own viewports: the bar command must stay in selection mode
    EXPECT_TRUE(view->selectionMode());
    nodal->structureChanged();
    EXPECT_TRUE(view->selectionMode());
    EXPECT_EQ(view->selectionFilter().bars, true);
    EXPECT_EQ(view->selectionFilter().nodes, false);
}

// ---- the dialog ------------------------------------------------------------------------------------------------------------

TEST_F(BarLoadToolTest, DialogDefaultsAndMeasuredLayout) {
    BarLoadDialog dialog(QStringLiteral("Introducir cargas sobre barras"), project.loadCases, barLoadDefaults(), 4.0);
    EXPECT_EQ(dialog.size(), QSize(402, 516));
    EXPECT_EQ(dialog.caseCombo()->currentText(), QStringLiteral("Peso propio"));
    EXPECT_EQ(dialog.type(), BarLoadType::Uniform);
    EXPECT_EQ(dialog.axes(), BarLoadAxes::Global);
    EXPECT_EQ(dialog.direction(), LoadDirection::MinusZ);
    EXPECT_TRUE(dialog.typeButton(1)->isChecked()) << "uniform is the second button";
    EXPECT_TRUE(dialog.directionButton(5)->isChecked());
    EXPECT_EQ(dialog.edit(BarLoadField::Value1)->text(), QStringLiteral("9.810"));
    EXPECT_EQ(dialog.label(BarLoadField::Value1)->text(), QStringLiteral("Valor"));
    EXPECT_EQ(dialog.unitLabel(BarLoadField::Value1)->text(), QStringLiteral("kN/m"));
    EXPECT_TRUE(dialog.relativeCheck()->isHidden()) << "a uniform load covers the whole bar";
    EXPECT_FALSE(dialog.axesGroup()->isHidden());
    EXPECT_TRUE(dialog.vectorGroup()->isHidden());
    EXPECT_TRUE(dialog.anglesGroup()->isHidden());
    // The nine buttons in the order of the original, only the last direction button has a tooltip, no button is a tab stop.
    const char* names[9] = {"Carga puntual", "Carga uniforme", "Carga en faja", "Carga triangular izquierda", "Carga triangular derecha",
                            "Carga trapecial", "Momento puntual", "Incremento de temperatura uniforme", "Incremento de temperatura variable"};
    for (int i = 0; i < 9; ++i) {
        EXPECT_EQ(dialog.typeButton(i)->toolTip(), QString::fromUtf8(names[i])) << i;
        EXPECT_EQ(dialog.typeButton(i)->focusPolicy(), Qt::NoFocus);
    }
    for (int i = 0; i < 7; ++i) EXPECT_EQ(dialog.directionButton(i)->focusPolicy(), Qt::NoFocus);
    EXPECT_TRUE(dialog.directionButton(6)->toolTip().startsWith(QStringLiteral("Definir dirección")));
}

TEST_F(BarLoadToolTest, FieldsPerTypeAreTheMeasuredOnes) {
    BarLoadDialog dialog(QStringLiteral("x"), project.loadCases, barLoadDefaults(), 4.0);
    dialog.typeButton(0)->click();  // Puntual
    EXPECT_EQ(dialog.type(), BarLoadType::Point);
    EXPECT_EQ(dialog.edit(BarLoadField::Value1)->text(), QStringLiteral("9.81"));
    EXPECT_EQ(dialog.unitLabel(BarLoadField::Value1)->text(), QStringLiteral("kN"));
    EXPECT_FALSE(dialog.relativeCheck()->isHidden());
    EXPECT_TRUE(dialog.relativeCheck()->isChecked());
    EXPECT_EQ(dialog.label(BarLoadField::Start)->text(), QStringLiteral("Distancia"));
    EXPECT_EQ(dialog.edit(BarLoadField::Start)->text(), QStringLiteral("0.000"));
    EXPECT_EQ(dialog.unitLabel(BarLoadField::Start)->text(), QStringLiteral("(0..1)"));
    EXPECT_TRUE(dialog.edit(BarLoadField::End)->parentWidget()->isHidden());
    dialog.relativeCheck()->setChecked(false);
    EXPECT_EQ(dialog.unitLabel(BarLoadField::Start)->text(), QStringLiteral("m"));
    EXPECT_EQ(dialog.edit(BarLoadField::Start)->text(), QStringLiteral("0.000"));
    dialog.relativeCheck()->setChecked(true);

    dialog.typeButton(2)->click();  // Faja
    EXPECT_EQ(dialog.label(BarLoadField::Start)->text(), QStringLiteral("Posición inicial"));
    EXPECT_EQ(dialog.label(BarLoadField::End)->text(), QStringLiteral("Posición final"));
    EXPECT_EQ(dialog.edit(BarLoadField::Value1)->text(), QStringLiteral("9.810"));
    EXPECT_EQ(dialog.edit(BarLoadField::End)->text(), QStringLiteral("1.000"));
    EXPECT_FALSE(dialog.edit(BarLoadField::End)->parentWidget()->isHidden());

    dialog.typeButton(3)->click();  // Triangular izq.
    EXPECT_EQ(dialog.label(BarLoadField::Value1)->text(), QStringLiteral("Valor inicial"));
    dialog.typeButton(5)->click();  // Trapecial
    EXPECT_EQ(dialog.label(BarLoadField::Value1)->text(), QStringLiteral("Valor inicial"));
    EXPECT_EQ(dialog.label(BarLoadField::Value2)->text(), QStringLiteral("Valor final"));
    EXPECT_EQ(dialog.edit(BarLoadField::Value2)->text(), QStringLiteral("9.810"));
    EXPECT_FALSE(dialog.edit(BarLoadField::Value2)->parentWidget()->isHidden());

    dialog.typeButton(6)->click();  // Momento puntual
    EXPECT_EQ(dialog.unitLabel(BarLoadField::Value1)->text(), QStringLiteral("kN·m"));
    EXPECT_EQ(dialog.label(BarLoadField::Start)->text(), QStringLiteral("Distancia"));
    EXPECT_TRUE(dialog.edit(BarLoadField::Value2)->parentWidget()->isHidden());
}

TEST_F(BarLoadToolTest, AxesAndDirectionGroupsFollowTheType) {
    BarLoadDialog dialog(QStringLiteral("x"), project.loadCases, barLoadDefaults(), 4.0);
    EXPECT_FALSE(dialog.globalRadio()->isHidden());
    dialog.localRadio()->click();
    EXPECT_EQ(dialog.axes(), BarLoadAxes::Local);
    dialog.typeButton(0)->click();
    EXPECT_EQ(dialog.axes(), BarLoadAxes::Local) << "the choice of axes is kept when the type changes";
    // Definir dirección: vector for global axes, two angles for local axes.
    dialog.directionButton(6)->click();
    EXPECT_TRUE(dialog.vectorGroup()->isHidden());
    EXPECT_FALSE(dialog.anglesGroup()->isHidden());
    EXPECT_EQ(dialog.edit(BarLoadField::AngleInPlane)->text(), QStringLiteral("0.000"));
    EXPECT_EQ(dialog.label(BarLoadField::AngleInPlane)->text(), QStringLiteral("Ángulo en el plano 'XZ' local"));
    EXPECT_EQ(dialog.label(BarLoadField::AngleToPlane)->text(), QStringLiteral("Ángulo con el plano 'XZ' local"));
    dialog.globalRadio()->click();
    EXPECT_FALSE(dialog.vectorGroup()->isHidden());
    EXPECT_TRUE(dialog.anglesGroup()->isHidden());
    EXPECT_EQ(dialog.edit(BarLoadField::VectorX)->text(), QStringLiteral("1.000"));
    EXPECT_EQ(dialog.edit(BarLoadField::VectorY)->text(), QStringLiteral("0.000"));
    // Variable temperature: only local axes, three buttons, second one by default.
    dialog.typeButton(8)->click();
    EXPECT_EQ(dialog.type(), BarLoadType::VariableTemperature);
    EXPECT_TRUE(dialog.globalRadio()->isHidden());
    EXPECT_EQ(dialog.axes(), BarLoadAxes::Local);
    EXPECT_FALSE(dialog.temperatureGroup()->isHidden());
    EXPECT_TRUE(dialog.temperatureButton(1)->isChecked());
    EXPECT_EQ(dialog.edit(BarLoadField::Value1)->text(), QStringLiteral("10.00"));
    EXPECT_EQ(dialog.edit(BarLoadField::Value2)->text(), QStringLiteral("10.00"));
    EXPECT_EQ(dialog.label(BarLoadField::Value2)->text(), QStringLiteral("Valor 2"));
    EXPECT_EQ(dialog.unitLabel(BarLoadField::Value1)->text(), QStringLiteral("°C"));
    EXPECT_TRUE(dialog.edit(BarLoadField::TemperatureAngle)->parentWidget()->isHidden());
    dialog.temperatureButton(2)->click();
    EXPECT_FALSE(dialog.edit(BarLoadField::TemperatureAngle)->parentWidget()->isHidden());
    EXPECT_EQ(dialog.label(BarLoadField::TemperatureAngle)->text(), QStringLiteral("Ángulo"));
    // Uniform temperature: neither axes nor direction.
    dialog.typeButton(7)->click();
    EXPECT_TRUE(dialog.axesGroup()->isHidden());
    EXPECT_TRUE(dialog.directionGroup()->isHidden());
    EXPECT_EQ(dialog.edit(BarLoadField::Value1)->text(), QStringLiteral("10.00"));
}

TEST_F(BarLoadToolTest, DialogHeightIsFixedWhenItOpensAccordingToTheType) {
    BarLoadData d = barLoadDefaults();
    d.type = BarLoadType::VariableTemperature;
    EXPECT_EQ(BarLoadDialog(QStringLiteral("x"), project.loadCases, d, 4.0).height(), 416);
    d.type = BarLoadType::UniformTemperature;
    BarLoadDialog uniformTemperature(QStringLiteral("x"), project.loadCases, d, 4.0);
    EXPECT_EQ(uniformTemperature.height(), 262);
    uniformTemperature.typeButton(0)->click();
    EXPECT_EQ(uniformTemperature.height(), 262) << "the height does not change with the type inside the window";
    d.type = BarLoadType::PointMoment;
    EXPECT_EQ(BarLoadDialog(QStringLiteral("x"), project.loadCases, d, 4.0).size(), QSize(402, 516));
}

TEST_F(BarLoadToolTest, TabSequenceFollowsTheMeasuredOrder) {
    BarLoadDialog dialog(QStringLiteral("x"), project.loadCases, barLoadDefaults(), 4.0);
    dialog.typeButton(5)->click();  // Trapecial
    dialog.directionButton(6)->click();
    using F = BarLoadField;
    EXPECT_EQ(dialog.visibleFields(), (std::vector<F>{F::Value1, F::Value2, F::Start, F::End, F::VectorX, F::VectorY, F::VectorZ}));
    dialog.localRadio()->click();
    EXPECT_EQ(dialog.visibleFields(), (std::vector<F>{F::Value1, F::Value2, F::Start, F::End, F::AngleInPlane, F::AngleToPlane}));
    dialog.typeButton(8)->click();
    EXPECT_EQ(dialog.visibleFields(), (std::vector<F>{F::Value1, F::Value2}));
    dialog.typeButton(7)->click();
    EXPECT_EQ(dialog.visibleFields(), (std::vector<F>{F::Value1}));
}

TEST_F(BarLoadToolTest, AcceptWarnsWithTheMeasuredMessagesAndKeepsTheWindowOpen) {
    BarLoadDialog dialog(QStringLiteral("x"), project.loadCases, barLoadDefaults(), 4.0);
    const auto expect = [&](const char* text, messages::Kind kind, const QString& message) {
        shown.clear();
        const QString before = dialog.edit(BarLoadField::Value1)->text();
        dialog.edit(BarLoadField::Value1)->setText(QString::fromLatin1(text));
        dialog.accept();
        ASSERT_EQ(shown.size(), 1u) << text;
        EXPECT_EQ(shown[0].kind, kind) << text;
        EXPECT_EQ(shown[0].text, message) << text;
        EXPECT_NE(dialog.result(), QDialog::Accepted);
        EXPECT_EQ(dialog.edit(BarLoadField::Value1)->text(), before) << "the field recovers its value";
    };
    expect("0", messages::Kind::Warning, QStringLiteral("El valor de 'Valor' debe estar entre 0.001 y 10000.000."));
    expect("-1", messages::Kind::Warning, QStringLiteral("El valor de 'Valor' debe estar entre 0.001 y 10000.000."));
    expect("10000.001", messages::Kind::Warning, QStringLiteral("El valor de 'Valor' debe estar entre 0.001 y 10000.000."));
    expect("abc", messages::Kind::Warning, QStringLiteral("La cadena introducida no es un número."));
    dialog.typeButton(0)->click();
    expect("0", messages::Kind::Warning, QStringLiteral("El valor de 'Valor' debe estar entre 0.01 y 49000.00."));
    // Positions.
    shown.clear();
    dialog.edit(BarLoadField::Value1)->setText(QStringLiteral("5"));
    dialog.edit(BarLoadField::Start)->setText(QStringLiteral("1.5"));
    dialog.accept();
    ASSERT_EQ(shown.size(), 1u);
    EXPECT_EQ(shown[0].text, QStringLiteral("El valor de 'Distancia' debe estar entre 0.000  y 1.000 ."));
    dialog.relativeCheck()->setChecked(false);
    dialog.edit(BarLoadField::Start)->setText(QStringLiteral("5"));
    shown.clear();
    dialog.accept();
    ASSERT_EQ(shown.size(), 1u);
    EXPECT_EQ(shown[0].text, QStringLiteral("El valor de 'Distancia' debe estar entre 0.000 y 4.000."));
    dialog.edit(BarLoadField::Start)->setText(QStringLiteral("4"));
    shown.clear();
    dialog.accept();
    EXPECT_TRUE(shown.empty()) << "4.000 is accepted";
    EXPECT_EQ(dialog.result(), QDialog::Accepted);
}

TEST_F(BarLoadToolTest, BusinessRulesGiveAnErrorBox) {
    BarLoadData start = barLoadDefaults();
    start.type = BarLoadType::Strip;
    BarLoadDialog strip(QStringLiteral("x"), project.loadCases, start, 4.0);
    strip.edit(BarLoadField::Start)->setText(QStringLiteral("0.6"));
    strip.edit(BarLoadField::End)->setText(QStringLiteral("0.4"));
    strip.accept();
    ASSERT_EQ(shown.size(), 1u);
    EXPECT_EQ(shown[0].kind, messages::Kind::Error) << "red icon";
    EXPECT_EQ(shown[0].text, QStringLiteral("El valor de 'Posición inicial' debe ser inferior al de 'Posición final'."));
    EXPECT_NE(strip.result(), QDialog::Accepted);

    start.type = BarLoadType::UniformTemperature;
    BarLoadDialog uniform(QStringLiteral("x"), project.loadCases, start, 4.0);
    shown.clear();
    uniform.edit(BarLoadField::Value1)->setText(QStringLiteral("0"));
    uniform.accept();
    ASSERT_EQ(shown.size(), 1u);
    EXPECT_EQ(shown[0].kind, messages::Kind::Error);
    EXPECT_EQ(shown[0].text, QStringLiteral("El valor introducido no puede ser cero."));
    uniform.edit(BarLoadField::Value1)->setText(QStringLiteral("1001"));
    shown.clear();
    uniform.accept();
    ASSERT_EQ(shown.size(), 1u);
    EXPECT_EQ(shown[0].kind, messages::Kind::Warning);
    EXPECT_EQ(shown[0].text, QStringLiteral("El valor de 'Valor' debe estar entre -100.00 y 1000.00."));

    start.type = BarLoadType::VariableTemperature;
    BarLoadDialog variable(QStringLiteral("x"), project.loadCases, start, 4.0);
    shown.clear();
    variable.edit(BarLoadField::Value1)->setText(QStringLiteral("0"));
    variable.edit(BarLoadField::Value2)->setText(QStringLiteral("0"));
    variable.accept();
    ASSERT_EQ(shown.size(), 1u);
    EXPECT_EQ(shown[0].kind, messages::Kind::Error);
    EXPECT_EQ(shown[0].text, QStringLiteral("Al menos uno de los valores de la carga debe ser distinto de cero."));
    variable.edit(BarLoadField::Value1)->setText(QStringLiteral("30"));
    variable.edit(BarLoadField::Value2)->setText(QStringLiteral("-30"));
    shown.clear();
    variable.accept();
    EXPECT_TRUE(shown.empty()) << "different signs are accepted";

    BarLoadDialog direction(QStringLiteral("x"), project.loadCases, barLoadDefaults(), 4.0);
    direction.directionButton(6)->click();
    for (BarLoadField f : {BarLoadField::VectorX, BarLoadField::VectorY, BarLoadField::VectorZ}) direction.edit(f)->setText(QStringLiteral("0"));
    shown.clear();
    direction.accept();
    ASSERT_EQ(shown.size(), 1u);
    EXPECT_EQ(shown[0].text, QStringLiteral("La dirección introducida no es correcta."));
    direction.edit(BarLoadField::VectorX)->setText(QStringLiteral("1001"));
    shown.clear();
    direction.accept();
    ASSERT_EQ(shown.size(), 1u);
    EXPECT_EQ(shown[0].text, QStringLiteral("El valor de 'X' debe estar entre -1000.000  y 1000.000 ."));
}

// ---- Nueva ------------------------------------------------------------------------------------------------------------------

TEST_F(BarLoadToolTest, NewCreatesOneLoadPerBarAsOneUndoStepAndRemembersTheLast) {
    runner<BarLoadDialog>([](BarLoadDialog& d) {
        d.typeButton(0)->click();  // Puntual
        d.edit(BarLoadField::Value1)->setText(QStringLiteral("20"));
        d.edit(BarLoadField::Start)->setText(QStringLiteral("0.5"));
        return true;
    });
    tool->newAction()->trigger();
    selectBar(1);
    click(*view, midOf(2), Qt::LeftButton);  // replaces the selection: act on one bar only first
    act();
    ASSERT_EQ(project.barLoads.items.size(), 1u);
    const BarLoad& load = project.barLoads.items[0];
    EXPECT_EQ(load.barId, 2);
    EXPECT_EQ(load.type, BarLoadType::Point);
    EXPECT_DOUBLE_EQ(load.value1, 20.0);
    EXPECT_DOUBLE_EQ(load.start, 0.5);
    EXPECT_EQ(undo.count(), 1u);
    EXPECT_TRUE(view->selection().empty());
    EXPECT_EQ(tool->mode(), BarLoadTool::Mode::New) << "the command stays active";
    // The next dialog starts with the last type and values (measured, cb_ml_n3).
    BarLoadData seen;
    tool->setDialogRunner([&seen](QDialog& dialog) {
        seen = static_cast<BarLoadDialog&>(dialog).data();
        return static_cast<int>(QDialog::Rejected);
    });
    selectBar(1);
    act();
    EXPECT_EQ(seen.type, BarLoadType::Point);
    EXPECT_DOUBLE_EQ(seen.value1, 20.0);
    EXPECT_TRUE(seen.relative);
    EXPECT_EQ(project.barLoads.items.size(), 1u) << "Cancelar changes nothing";
    undo.undo();
    EXPECT_TRUE(project.barLoads.items.empty());
}

TEST_F(BarLoadToolTest, NewWithTwoBarsGivesTwoLoadsAndAnAbsolutePositionIsCheckedAgainstTheShortestBar) {
    runner<BarLoadDialog>([](BarLoadDialog& d) {
        d.edit(BarLoadField::Value1)->setText(QStringLiteral("5"));
        return true;
    });
    EXPECT_TRUE(tool->createLoads({1, 2}));
    EXPECT_EQ(project.barLoads.items.size(), 2u);
    EXPECT_EQ(undo.count(), 1u);
    double lengthSeen = -1;
    project.structure.findNode(3)->position = {10, 0, 0};  // B2 is now 6 m long, B1 stays 4 m
    tool->setDialogRunner([&](QDialog& dialog) {
        auto& d = static_cast<BarLoadDialog&>(dialog);
        d.typeButton(0)->click();
        d.relativeCheck()->setChecked(false);
        d.edit(BarLoadField::Start)->setText(QStringLiteral("5"));
        lengthSeen = 1;
        d.accept();
        return static_cast<int>(d.result());
    });
    shown.clear();
    EXPECT_FALSE(tool->createLoads({1, 2}));
    ASSERT_EQ(shown.size(), 1u);
    EXPECT_EQ(shown[0].text, QStringLiteral("El valor de 'Distancia' debe estar entre 0.000 y 4.000."));
    EXPECT_EQ(lengthSeen, 1);
}

TEST_F(BarLoadToolTest, NewTakesTheShownHypothesisByDefault) {
    const int cm1 = addCase();
    tool->newAction()->trigger();
    nodal->panel()->setCaseId(cm1);
    int offered = -1;
    tool->setDialogRunner([&offered](QDialog& dialog) {
        offered = static_cast<BarLoadDialog&>(dialog).data().loadCaseId;
        return static_cast<int>(QDialog::Rejected);
    });
    EXPECT_FALSE(tool->createLoads({1}));
    EXPECT_EQ(offered, cm1);
}

// ---- Editar / Hipótesis / Borrar ---------------------------------------------------------------------------------------------

TEST_F(BarLoadToolTest, OnlyBarsWithLoadsCanBeCapturedAndEditOverwritesAll) {
    addUniform(1, 3.0);
    BarLoadData point = barLoadDefaults();
    point.type = BarLoadType::Point;
    point.value1 = 20.0;
    point.start = 0.5;
    ASSERT_EQ(addBarLoads(project, undo, {1}, point), BarLoadError::None);
    undo.setClean();
    const std::size_t before = undo.count();
    BarLoadData offered;
    runner<BarLoadDialog>([&offered](BarLoadDialog& d) {
        offered = d.data();
        EXPECT_EQ(d.windowTitle(), QStringLiteral("Descripción de la carga"));
        d.edit(BarLoadField::Value1)->setText(QStringLiteral("7"));
        return true;
    });
    tool->editAction()->trigger();
    selectBar(2);
    EXPECT_TRUE(view->selection().empty()) << "B2 has no load: it cannot be captured";
    selectBar(1);
    EXPECT_EQ(view->selection().ids(ElementKind::Bar), (std::vector<int>{1}));
    act();
    EXPECT_EQ(offered.type, BarLoadType::Uniform) << "the dialog shows the data of the first load";
    EXPECT_DOUBLE_EQ(offered.value1, 3.0);
    ASSERT_EQ(project.barLoads.items.size(), 2u);
    for (const BarLoad& l : project.barLoads.items) {
        EXPECT_EQ(l.type, BarLoadType::Uniform) << "a point load and a uniform one both became uniform (measured)";
        EXPECT_DOUBLE_EQ(l.value1, 7.0);
    }
    EXPECT_EQ(undo.count(), before + 1);
    EXPECT_TRUE(view->selection().empty());
}

TEST_F(BarLoadToolTest, HypothesisChangesTheCaseOfTheSelectedLoads) {
    const int cm1 = addCase();
    addUniform(1);
    tool->caseAction()->trigger();
    runner<LoadCaseChoiceDialog>([&](LoadCaseChoiceDialog& d) {
        EXPECT_EQ(d.windowTitle(), QStringLiteral("Hipótesis"));
        d.combo()->setCurrentIndex(d.combo()->findData(cm1));
        return true;
    });
    selectBar(1);
    act();
    EXPECT_EQ(project.barLoads.items[0].loadCaseId, cm1);
    EXPECT_TRUE(tool->loadsOfBars({1}).empty()) << "with 'Peso propio' in view nothing can be captured now";
    nodal->setView(0, true);
    EXPECT_EQ(tool->loadsOfBars({1}).size(), 1u);
}

TEST_F(BarLoadToolTest, DeleteActsAtOnceWithoutAQuestionAndDisablesTheButtons) {
    addUniform(1);
    addUniform(2);
    undo.setClean();
    tool->deleteAction()->trigger();
    selectBar(1);
    act();
    ASSERT_EQ(project.barLoads.items.size(), 1u);
    EXPECT_EQ(project.barLoads.items[0].barId, 2);
    EXPECT_TRUE(shown.empty());
    undo.undo();
    EXPECT_EQ(project.barLoads.items.size(), 2u);
    undo.redo();
    selectBar(2);
    act();
    EXPECT_TRUE(project.barLoads.items.empty());
    EXPECT_FALSE(tool->deleteAction()->isEnabled());
    EXPECT_EQ(tool->mode(), BarLoadTool::Mode::None) << "without loads the command ends";
}

TEST_F(BarLoadToolTest, SelectedLoadsAreHighlightedOnlyInTheLoadCommands) {
    addUniform(1);
    tool->editAction()->trigger();
    EXPECT_TRUE(view->selectionMode());
    EXPECT_TRUE(view->selectionFilter().bars);
    tool->deactivate();
    EXPECT_FALSE(view->selectionMode());
    tool->newAction()->trigger();
    EXPECT_TRUE(view->selectionMode());
}

// ---- Peso propio -------------------------------------------------------------------------------------------------------------

TEST_F(BarLoadToolTest, SelfWeightDialogOpensAtOnceAndRemembersTheOption) {
    BarSelfWeightDialog dialog(true);
    EXPECT_EQ(dialog.windowTitle(), QStringLiteral("Generación de cargas"));
    EXPECT_EQ(dialog.size(), QSize(186, 134));
    EXPECT_EQ(dialog.activateRadio()->text(), QStringLiteral("Activar"));
    EXPECT_EQ(dialog.deactivateRadio()->text(), QStringLiteral("Desactivar"));
    EXPECT_TRUE(dialog.activateRadio()->isChecked());
    EXPECT_EQ(dialog.acceptButton()->text(), QStringLiteral("Aceptar"));
    EXPECT_EQ(dialog.cancelButton()->text(), QStringLiteral("Cancelar"));

    // Cancelar: the command does not start.
    int opened = 0;
    tool->setDialogRunner([&](QDialog&) {
        ++opened;
        return static_cast<int>(QDialog::Rejected);
    });
    tool->selfWeightAction()->trigger();
    EXPECT_EQ(opened, 1);
    EXPECT_EQ(tool->mode(), BarLoadTool::Mode::None);
    EXPECT_FALSE(tool->selfWeightAction()->isChecked());
    // Aceptar with Desactivar: the command is active and shows the drawing of the self weight.
    runner<BarSelfWeightDialog>([](BarSelfWeightDialog& d) {
        d.deactivateRadio()->setChecked(true);
        return true;
    });
    tool->selfWeightAction()->trigger();
    EXPECT_EQ(tool->mode(), BarLoadTool::Mode::SelfWeight);
    EXPECT_EQ(status, QStringLiteral("Barras - Peso propio |  - LMB: Seleccione un grupo de elementos. - RMB: Actuar sobre los elementos seleccionados."));
    EXPECT_TRUE(view->selfWeightDrawn());
    EXPECT_FALSE(tool->session().selfWeightActivate);
    // RMB without a selection opens the dialog again with the remembered option.
    bool remembered = true;
    tool->setDialogRunner([&](QDialog& dialog) {
        remembered = static_cast<BarSelfWeightDialog&>(dialog).activate();
        return static_cast<int>(QDialog::Rejected);
    });
    act();
    EXPECT_FALSE(remembered);
    // RMB with a selection applies it without a question or a warning.
    selectBar(1);
    act();
    EXPECT_TRUE(shown.empty());
    EXPECT_FALSE(barSelfWeightActive(project.barLoads, 1));
    EXPECT_TRUE(barSelfWeightActive(project.barLoads, 2));
    EXPECT_FALSE(tool->editAction()->isEnabled()) << "self weight is not a load";
    EXPECT_EQ(undo.count(), 1u);
    EXPECT_EQ(tool->mode(), BarLoadTool::Mode::SelfWeight);
    tool->selfWeightAction()->trigger();
    EXPECT_EQ(tool->mode(), BarLoadTool::Mode::None);
    EXPECT_FALSE(view->selfWeightDrawn()) << "the self weight is drawn only inside the command";
}

TEST_F(BarLoadToolTest, SwitchingBetweenNodalAndBarCommandsStopsTheOtherOne) {
    tool->newAction()->trigger();
    EXPECT_TRUE(view->selectionMode());
    nodal->newAction()->trigger();  // the nodal command starts: the bar one stops (like MainWindow wires them)
    EXPECT_EQ(nodal->mode(), NodalLoadTool::Mode::New);
    EXPECT_EQ(tool->mode(), BarLoadTool::Mode::None);
    EXPECT_FALSE(tool->newAction()->isChecked());
    EXPECT_TRUE(view->selectionMode()) << "the nodal command owns the window now";
    EXPECT_FALSE(view->selectionFilter().bars);
    tool->newAction()->trigger();  // and back
    EXPECT_EQ(tool->mode(), BarLoadTool::Mode::New);
    EXPECT_EQ(nodal->mode(), NodalLoadTool::Mode::None);
    EXPECT_TRUE(view->selectionFilter().bars);
    EXPECT_FALSE(view->selectionFilter().nodes);
    tool->deactivate();
    EXPECT_FALSE(view->selectionMode());
}

TEST_F(BarLoadToolTest, OutOfLengthWarningUsesTheMeasuredText) {
    tool->showOutOfLengthWarning();
    ASSERT_EQ(shown.size(), 1u);
    EXPECT_EQ(shown[0].kind, messages::Kind::Warning);
    EXPECT_EQ(shown[0].text, QStringLiteral("Se han borrado cargas incompatibles con la longitud de la barra."));
}

int main(int argc, char** argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    app();
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
