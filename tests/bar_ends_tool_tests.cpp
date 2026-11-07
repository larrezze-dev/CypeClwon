// Propiedades > Barras > Articular and Empotramiento (T-P6-05, GUI part): panel, dialog and tool. Expected values:
// docs/research/ui/articulaciones.md sections 3, 4 and 6 (measured in the original). Never shown on screen (offscreen).
#include <gtest/gtest.h>

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QRadioButton>
#include <memory>

#include "BarEndsTool.h"
#include "UserMessages.h"
#include "Viewport3D.h"
#include "core/bar_ends_edit.h"

using namespace cc::core;
using cc::app::BarEndsTool;
using cc::app::EndFixityDialog;
using cc::app::SelectionTypePanel;
using cc::app::Viewport3D;
namespace messages = cc::app::messages;

namespace {

QApplication* app() {
    static int argc = 1;
    static char name[] = "bar_ends_tool_tests";
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

/// Nodes N1 (0,0,0) .. N3 (8,0,0) along X seen from the front; bars B1 = N1-N2 and B2 = N2-N3.
class BarEndsToolTest : public ::testing::Test {
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
        tool = std::make_unique<BarEndsTool>();
        tool->setProject(&project, &undo);
        tool->addViewport(view.get());
        undo.setChangeCallback([this] { tool->structureChanged(); });
        QObject::connect(tool.get(), &BarEndsTool::statusChanged, [this](const QString& text) { status = text; });
        shown.clear();
        answer = true;
        messages::handler() = [this](QWidget*, messages::Kind kind, const QString& text) {
            shown.push_back({kind, text});
            return answer;
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
    /// Window around both bars (above and below the line so that nothing is picked at the corners).
    void selectAll() {
        const QPointF a = screenOf(1), b = screenOf(3);
        click(*view, QPointF(a.x() - 20, a.y() - 40));
        click(*view, QPointF(b.x() + 20, b.y() + 40));
    }
    Project project;
    UndoStack undo;
    std::unique_ptr<Viewport3D> view;
    std::unique_ptr<BarEndsTool> tool;
    QString status;
    std::vector<Shown> shown;
    bool answer = true;
};

}  // namespace

TEST_F(BarEndsToolTest, ButtonsAndStatusTexts) {
    EXPECT_EQ(tool->articulateAction()->text(), QStringLiteral("Articular"));
    EXPECT_EQ(tool->fixityAction()->text(), QStringLiteral("Empotramiento"));
    EXPECT_TRUE(tool->articulateAction()->isEnabled());
    BarEndsTool other;
    EXPECT_FALSE(other.articulateAction()->isEnabled()) << "no project";
    tool->fixityAction()->trigger();
    EXPECT_EQ(tool->mode(), BarEndsTool::Mode::Fixity);
    EXPECT_EQ(status, QStringLiteral("Barras - Empotramiento |  - LMB: Seleccione un grupo de elementos. - RMB: Actuar sobre los elementos seleccionados."));
    EXPECT_TRUE(view->selectionMode());
    EXPECT_TRUE(view->endFixityLabelsVisible());
    EXPECT_FALSE(view->barAxesVisible());
    tool->articulateAction()->trigger();  // exclusive: leaves Empotramiento
    EXPECT_EQ(tool->mode(), BarEndsTool::Mode::Articulate);
    EXPECT_FALSE(tool->fixityAction()->isChecked());
    EXPECT_FALSE(view->endFixityLabelsVisible());
    EXPECT_TRUE(view->barAxesVisible());
    EXPECT_EQ(status, QStringLiteral("Barras - Articular |  - LMB: Seleccione un elemento."));
    tool->articulateAction()->trigger();
    EXPECT_EQ(tool->mode(), BarEndsTool::Mode::None);
    EXPECT_FALSE(view->selectionMode());
}

TEST_F(BarEndsToolTest, SelectionTypePanel) {
    tool->articulateAction()->trigger();
    SelectionTypePanel* panel = tool->panel();
    ASSERT_NE(panel, nullptr);
    EXPECT_EQ(panel->windowTitle(), QStringLiteral("Propiedades - Tipo de selección"));
    EXPECT_EQ(panel->individualRadio()->text(), QStringLiteral("Individual"));
    EXPECT_EQ(panel->multipleRadio()->text(), QStringLiteral("Múltiple"));
    EXPECT_TRUE(panel->individualRadio()->isChecked());
    EXPECT_FALSE(panel->multiple());
    panel->multipleRadio()->setChecked(true);
    EXPECT_EQ(status, QStringLiteral("Barras - Articular |  - LMB: Seleccione un grupo de elementos. - RMB: Actuar sobre los elementos seleccionados."));
}

TEST_F(BarEndsToolTest, MultipleArticulatesBothEndsOfTheSelectedBars) {
    tool->articulateAction()->trigger();
    tool->panel()->multipleRadio()->setChecked(true);
    selectAll();
    EXPECT_EQ(view->selection().ids(ElementKind::Bar), (std::vector<int>{1, 2}));
    click(*view, QPointF(400, 500), Qt::RightButton);
    EXPECT_EQ(undo.count(), 1u);
    for (int bar : {1, 2}) {
        const BarEndsRecord record = barEndsOf(project.barEnds, bar);
        EXPECT_TRUE(record.ends[kBarOrigin].pinned);
        EXPECT_TRUE(record.ends[kBarFinal].pinned);
        EXPECT_EQ(endFixityLabel(record.ends[kBarOrigin]), "xy: 1.000 xz: 1.000 yz: 1.000") << "the coefficients do not change (measured)";
    }
    EXPECT_TRUE(view->selection().empty());
    tool->deactivate();
    EXPECT_EQ(view->barEnds(), &project.barEnds) << "the symbols stay when the tool ends";
    undo.undo();
    EXPECT_TRUE(project.barEnds.empty());
}

TEST_F(BarEndsToolTest, IndividualPinsTheEndNearestToTheClick) {
    tool->articulateAction()->trigger();
    const QPointF a = screenOf(1), b = screenOf(2);
    click(*view, a + (b - a) * 0.2);  // near N1
    EXPECT_TRUE(barEndsOf(project.barEnds, 1).ends[kBarOrigin].pinned);
    EXPECT_FALSE(barEndsOf(project.barEnds, 1).ends[kBarFinal].pinned);
    EXPECT_TRUE(view->selection().empty());
    EXPECT_EQ(undo.count(), 1u);
}

TEST_F(BarEndsToolTest, DialogTextsAndDefaults) {
    EndFixityDialog dialog;
    EXPECT_EQ(dialog.windowTitle(), QStringLiteral("Empotramiento en extremos"));
    const QStringList checks = {"Asignar (Plano xy)", "Asignar (Plano xz)", "Asignar (Plano yz)"};
    for (int plane = 0; plane < 3; ++plane) {
        EXPECT_EQ(dialog.assignCheck(plane)->text(), checks[plane]);
        EXPECT_TRUE(dialog.assignCheck(plane)->isChecked());
        for (int end = 0; end < 2; ++end) {
            EXPECT_EQ(dialog.coefficientRadio(end, plane)->text(), QStringLiteral("Coef. empotramiento"));
            EXPECT_EQ(dialog.stiffnessRadio(end, plane)->text(), QStringLiteral("Rigidez rotacional"));
            EXPECT_TRUE(dialog.coefficientRadio(end, plane)->isChecked());
        }
    }
    EXPECT_EQ(dialog.field(kBarOrigin, kPlaneXY)->text(), QStringLiteral("1.000"));
    EXPECT_EQ(dialog.yzCombo(kBarFinal)->currentText(), QStringLiteral("1.000"));
    EXPECT_EQ(dialog.edit(), EndFixityEdit{});
    // Rigidez rotacional: 500000.00 and the unit.
    dialog.stiffnessRadio(kBarOrigin, kPlaneXZ)->setChecked(true);
    EXPECT_EQ(dialog.field(kBarOrigin, kPlaneXZ)->text(), QStringLiteral("500000.00"));
    EXPECT_FALSE(dialog.unitLabel(kBarOrigin, kPlaneXZ)->isHidden());
    EXPECT_EQ(dialog.unitLabel(kBarOrigin, kPlaneXZ)->text(), QStringLiteral("kN·m/rad"));
}

TEST_F(BarEndsToolTest, OutOfRangeValueWarnsAndRecoversThePreviousOne) {
    EndFixityDialog dialog;
    dialog.typeField(kBarOrigin, kPlaneXY, QStringLiteral("2"));
    ASSERT_EQ(shown.size(), 1u);
    EXPECT_EQ(shown[0].kind, messages::Kind::Warning);
    EXPECT_EQ(shown[0].text, QStringLiteral("El valor de '' debe estar entre 0.000 y 1.000 ."));
    EXPECT_EQ(dialog.field(kBarOrigin, kPlaneXY)->text(), QStringLiteral("1.000"));
    dialog.typeField(kBarOrigin, kPlaneXY, QStringLiteral("0,5"));
    EXPECT_EQ(shown.size(), 1u);
    EXPECT_EQ(dialog.field(kBarOrigin, kPlaneXY)->text(), QStringLiteral("0.500"));
    EXPECT_DOUBLE_EQ(dialog.edit().value[kBarOrigin][kPlaneXY].coefficient, 0.5);
}

TEST_F(BarEndsToolTest, DefaultStiffnessAsksAndNoKeepsTheDialogOpen) {
    EndFixityDialog dialog;
    dialog.stiffnessRadio(kBarOrigin, kPlaneXY)->setChecked(true);
    answer = false;
    dialog.accept();
    ASSERT_EQ(shown.size(), 1u);
    EXPECT_EQ(shown[0].kind, messages::Kind::Question);
    EXPECT_TRUE(shown[0].text.startsWith(QStringLiteral("El valor de la rigidez rotacional introducido coincide con el valor por defecto.")));
    EXPECT_NE(dialog.result(), QDialog::Accepted);
    answer = true;
    dialog.accept();
    EXPECT_EQ(dialog.result(), QDialog::Accepted);
}

TEST_F(BarEndsToolTest, StiffnessRangeIsCheckedOnAccept) {
    EndFixityDialog dialog;
    dialog.stiffnessRadio(kBarOrigin, kPlaneXY)->setChecked(true);
    answer = true;
    dialog.typeField(kBarOrigin, kPlaneXY, QStringLiteral("-1"));
    EXPECT_TRUE(shown.empty());  // no warning while typing: the original reports on Aceptar
    dialog.accept();
    ASSERT_EQ(shown.size(), 1u);
    EXPECT_EQ(shown[0].kind, messages::Kind::Warning);
    EXPECT_EQ(shown[0].text, QStringLiteral("El valor de '' debe estar entre 0.00 y 10000000.00."));
    EXPECT_NE(dialog.result(), QDialog::Accepted);
    dialog.typeField(kBarOrigin, kPlaneXY, QStringLiteral("1000000000000"));
    dialog.accept();
    EXPECT_EQ(shown.size(), 2u);
    EXPECT_NE(dialog.result(), QDialog::Accepted);
    dialog.typeField(kBarOrigin, kPlaneXY, QStringLiteral("0.001"));  // rounds to 0.00 and is accepted
    EXPECT_EQ(dialog.field(kBarOrigin, kPlaneXY)->text(), QStringLiteral("0.00"));
    dialog.accept();
    EXPECT_EQ(shown.size(), 2u);
    EXPECT_EQ(dialog.result(), QDialog::Accepted);
}

TEST_F(BarEndsToolTest, EmpotramientoAssignsToTheSelectedBarsAsOneStep) {
    tool->setDialogRunner([](QDialog& dialog) {
        auto& d = static_cast<EndFixityDialog&>(dialog);
        d.typeField(kBarOrigin, kPlaneXY, QStringLiteral("0.5"));
        d.accept();
        return static_cast<int>(d.result());
    });
    tool->fixityAction()->trigger();
    selectAll();
    click(*view, QPointF(400, 500), Qt::RightButton);
    EXPECT_EQ(undo.count(), 1u);
    for (int bar : {1, 2}) {
        const BarEndsRecord record = barEndsOf(project.barEnds, bar);
        EXPECT_DOUBLE_EQ(record.ends[kBarOrigin].planes[kPlaneXY].coefficient, 0.5);
        EXPECT_DOUBLE_EQ(record.ends[kBarFinal].planes[kPlaneXY].coefficient, 1.0);
        EXPECT_DOUBLE_EQ(record.ends[kBarOrigin].planes[kPlaneXZ].coefficient, 1.0);
    }
    EXPECT_EQ(endFixityLabel(barEndsOf(project.barEnds, 1).ends[kBarOrigin]), "xy: 0.500 xz: 1.000 yz: 1.000");
    undo.undo();
    EXPECT_TRUE(project.barEnds.empty());
    // Cancelar changes nothing.
    tool->setDialogRunner([](QDialog&) { return static_cast<int>(QDialog::Rejected); });
    EXPECT_FALSE(tool->editFixity({1, 2}));
    EXPECT_TRUE(project.barEnds.empty());
}

TEST_F(BarEndsToolTest, ChangingProjectEndsTheCommand) {
    tool->fixityAction()->trigger();
    tool->setProject(nullptr, nullptr);
    EXPECT_EQ(tool->mode(), BarEndsTool::Mode::None);
    EXPECT_FALSE(tool->fixityAction()->isEnabled());
    EXPECT_EQ(view->barEnds(), nullptr);
}

int main(int argc, char** argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    app();
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

TEST_F(BarEndsToolTest, IndividualUsesTheRuleOfThirdsAndTogglesAndRmbDoesNothing) {
    tool->articulateAction()->trigger();
    const QPointF a = screenOf(1), b = screenOf(2);
    click(*view, a + (b - a) * 0.5);  // central third: both ends
    EXPECT_TRUE(barEndsOf(project.barEnds, 1).ends[kBarOrigin].pinned);
    EXPECT_TRUE(barEndsOf(project.barEnds, 1).ends[kBarFinal].pinned);
    click(*view, a + (b - a) * 0.5);  // again: both free
    EXPECT_TRUE(project.barEnds.empty());
    click(*view, a + (b - a) * 0.9);  // last third: end only
    EXPECT_FALSE(barEndsOf(project.barEnds, 1).ends[kBarOrigin].pinned);
    EXPECT_TRUE(barEndsOf(project.barEnds, 1).ends[kBarFinal].pinned);
    click(*view, a + (b - a) * 0.5);  // mixed bar: "both" leaves it unchanged, no warning
    EXPECT_TRUE(barEndsOf(project.barEnds, 1).ends[kBarFinal].pinned);
    EXPECT_FALSE(barEndsOf(project.barEnds, 1).ends[kBarOrigin].pinned);
    click(*view, a + (b - a) * 0.9);  // re-applying removes the articulation
    EXPECT_TRUE(project.barEnds.empty());
    EXPECT_TRUE(shown.empty());
    const std::size_t count = undo.count();
    click(*view, QPointF(400, 500), Qt::RightButton);
    EXPECT_EQ(undo.count(), count) << "RMB has no effect in Individual";
}
