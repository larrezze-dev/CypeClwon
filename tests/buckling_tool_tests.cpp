// Propiedades > Barras > Pandeo and Pandeo lateral (T-P6-04, GUI part): dialogs and tool. Expected values: docs/research/ui/pandeo.md
// section 9 (measured in the original). Never shown on screen (offscreen).
#include <gtest/gtest.h>

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QLabel>
#include <QMouseEvent>
#include <QRadioButton>
#include <QToolButton>
#include <memory>

#include "BucklingTool.h"
#include "RangedEdit.h"
#include "UserMessages.h"
#include "Viewport3D.h"
#include "core/buckling_edit.h"

using namespace cc::core;
using cc::app::BucklingDialog;
using cc::app::BucklingTool;
using cc::app::LateralBucklingDialog;
using cc::app::RangedEdit;
using cc::app::Viewport3D;
namespace messages = cc::app::messages;

namespace {

QApplication* app() {
    static int argc = 1;
    static char name[] = "buckling_tool_tests";
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

/// Nodes N1 (0,0,0) .. N3 (8,0,0) along X seen from the front; bars B1 = N1-N2 and B2 = N2-N3.
class BucklingToolTest : public ::testing::Test {
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
        tool = std::make_unique<BucklingTool>();
        tool->setProject(&project, &undo);
        tool->addViewport(view.get());
        undo.setChangeCallback([this] { tool->structureChanged(); });
        QObject::connect(tool.get(), &BucklingTool::statusChanged, [this](const QString& text) { status = text; });
        warnings.clear();
        RangedEdit::setWarningHandler([this](QWidget*, const QString& text) { warnings.push_back(text); });
        messages::handler() = [this](QWidget*, messages::Kind, const QString& text) {
            notices.push_back(text);
            return true;
        };
        notices.clear();
        undo.setClean();
    }
    void TearDown() override {
        RangedEdit::setWarningHandler(nullptr);
        messages::handler() = nullptr;
    }
    QPointF screenOf(int id) {
        const Node& n = *project.structure.findNode(id);
        QPointF p;
        EXPECT_TRUE(view->worldToScreen({n.position.x, n.position.y, n.position.z}, p));
        return p;
    }
    void selectAll() {
        const QPointF a = screenOf(1), b = screenOf(3);
        click(*view, QPointF(a.x() - 20, a.y() - 40));
        click(*view, QPointF(b.x() + 20, b.y() + 40));
    }
    Project project;
    UndoStack undo;
    std::unique_ptr<Viewport3D> view;
    std::unique_ptr<BucklingTool> tool;
    QString status;
    QStringList warnings, notices;
};

}  // namespace

TEST_F(BucklingToolTest, ButtonsStatusAndViewportState) {
    EXPECT_EQ(tool->betaAction()->text(), QStringLiteral("Pandeo"));
    EXPECT_EQ(tool->lateralAction()->text(), QStringLiteral("Pandeo lateral"));
    EXPECT_EQ(tool->betaAction()->toolTip(), QStringLiteral("Pandeo (Ctrl+p)"));
    EXPECT_TRUE(tool->betaAction()->isEnabled());
    BucklingTool other;
    EXPECT_FALSE(other.betaAction()->isEnabled()) << "no project";
    tool->betaAction()->trigger();
    EXPECT_EQ(tool->mode(), BucklingTool::Mode::Beta);
    EXPECT_EQ(status, QStringLiteral("Barras - Pandeo |  - LMB: Seleccione un grupo de elementos. - RMB: Actuar sobre los elementos seleccionados."));
    EXPECT_TRUE(view->selectionMode());
    EXPECT_TRUE(view->barAxesVisible());
    EXPECT_EQ(view->bucklingLabels(), Viewport3D::BucklingLabels::Beta);
    tool->lateralAction()->trigger();  // exclusive
    EXPECT_EQ(tool->mode(), BucklingTool::Mode::Lateral);
    EXPECT_FALSE(tool->betaAction()->isChecked());
    EXPECT_EQ(status, QStringLiteral("Barras - Pandeo lateral |  - LMB: Seleccione un grupo de elementos. - RMB: Actuar sobre los elementos seleccionados."));
    EXPECT_EQ(view->bucklingLabels(), Viewport3D::BucklingLabels::Lateral);
    tool->lateralAction()->trigger();
    EXPECT_EQ(tool->mode(), BucklingTool::Mode::None);
    EXPECT_EQ(view->bucklingLabels(), Viewport3D::BucklingLabels::None);
    EXPECT_FALSE(view->barAxesVisible());
    EXPECT_FALSE(view->selectionMode());
}

TEST_F(BucklingToolTest, BetaDialogTextsAndDefaults) {
    BucklingDialog dialog(false);
    EXPECT_EQ(dialog.windowTitle(), QStringLiteral("Pandeo"));
    EXPECT_EQ(dialog.assignCheck(0)->text(), QStringLiteral("Asignar longitud de pandeo (Plano xy)"));
    EXPECT_EQ(dialog.assignCheck(1)->text(), QStringLiteral("Asignar longitud de pandeo (Plano xz)"));
    EXPECT_EQ(dialog.criteriaCheck()->text(), QStringLiteral("Asignar criterios para el análisis de pandeo"));
    EXPECT_EQ(dialog.considerCheck()->text(), QStringLiteral("Considerar en el análisis de pandeo"));
    EXPECT_EQ(dialog.updateCheck(0)->text(), QStringLiteral("Permitir actualizar los coeficientes de pandeo en el plano xy"));
    EXPECT_EQ(dialog.updateCheck(1)->text(), QStringLiteral("Permitir actualizar los coeficientes de pandeo en el plano xz"));
    EXPECT_TRUE(dialog.assignCheck(0)->isChecked());
    EXPECT_TRUE(dialog.criteriaCheck()->isChecked());
    EXPECT_TRUE(dialog.considerCheck()->isChecked());
    EXPECT_FALSE(dialog.updateCheck(0)->isChecked());
    EXPECT_FALSE(dialog.updateCheck(1)->isChecked());
    EXPECT_EQ(dialog.assignMomentCheck(0), nullptr) << "aluminium: no Cm block";
    const QStringList tips = {"No considerar pandeo", "Barra biempotrada", "Barra apoyada en un extremo y empotrada en el otro.", "Barra biapoyada",
                              "Barra en voladizo", "Otros valores del coeficiente de pandeo", "Asignar longitud de pandeo"};
    for (int plane = 0; plane < 2; ++plane) {
        for (int k = 0; k < 7; ++k) EXPECT_EQ(dialog.kindButton(plane, static_cast<BucklingKind>(k))->toolTip(), tips[k]);
        EXPECT_TRUE(dialog.kindButton(plane, BucklingKind::Computed)->toolTip().contains(QStringLiteral("Cálculo aproximado de longitudes de pandeo")));
        EXPECT_EQ(dialog.kind(plane), BucklingKind::PinnedPinned) << "default: beta 1.0";
        EXPECT_TRUE(dialog.kindButton(plane, BucklingKind::PinnedPinned)->isChecked());
        EXPECT_TRUE(dialog.fieldRow(plane)->isEnabled());
        EXPECT_TRUE(dialog.betaEdit(plane)->parentWidget()->isHidden());
        EXPECT_EQ(dialog.betaEdit(plane)->text(), QStringLiteral("1.000"));
        EXPECT_EQ(dialog.lengthEdit(plane)->text(), QStringLiteral("1.000"));
        EXPECT_TRUE(dialog.intraslationalRadio(plane)->isChecked()) << "measured default";
    }
    EXPECT_EQ(dialog.edit(), [] {
        BucklingEdit e;
        e.assignMomentXY = e.assignMomentXZ = false;
        return e;
    }());
    // field rows of the options 6 and 7 and 8 replace each other
    dialog.chooseKind(0, BucklingKind::UserBeta);
    EXPECT_FALSE(dialog.betaEdit(0)->parentWidget()->isHidden());
    EXPECT_TRUE(dialog.lengthEdit(0)->parentWidget()->isHidden());
    EXPECT_TRUE(dialog.betaEdit(1)->parentWidget()->isHidden()) << "the planes are independent";
    dialog.chooseKind(0, BucklingKind::UserLength);
    EXPECT_TRUE(dialog.betaEdit(0)->parentWidget()->isHidden());
    EXPECT_FALSE(dialog.lengthEdit(0)->parentWidget()->isHidden());
    dialog.chooseKind(0, BucklingKind::FixedFixed);
    EXPECT_TRUE(dialog.lengthEdit(0)->parentWidget()->isHidden());
    EXPECT_EQ(dialog.kind(0), BucklingKind::FixedFixed);
}

TEST_F(BucklingToolTest, ApproximateCalculationIsNotAvailableYet) {
    BucklingDialog dialog(false);
    dialog.chooseKind(1, BucklingKind::UserBeta);
    dialog.kindButton(1, BucklingKind::Computed)->click();  // as the user does
    ASSERT_EQ(notices.size(), 1u);
    EXPECT_EQ(dialog.kind(1), BucklingKind::UserBeta) << "the previous option stays";
    // a plane loaded from a bar with that kind keeps it
    BarBuckling record;
    record.xz.kind = BucklingKind::Computed;
    record.xz.beta = 1.37;
    record.xz.translational = true;
    dialog.setBar(record);
    EXPECT_EQ(dialog.kind(1), BucklingKind::Computed);
    EXPECT_FALSE(dialog.translationalRadio(1)->isHidden());
    EXPECT_TRUE(dialog.translationalRadio(1)->isChecked());
    EXPECT_EQ(dialog.edit().xz.beta, 1.37);
}

TEST_F(BucklingToolTest, BetaFieldRangeAndRounding) {
    BucklingDialog dialog(false);
    dialog.chooseKind(0, BucklingKind::UserBeta);
    RangedEdit* edit = dialog.betaEdit(0);
    EXPECT_EQ(edit->toolTip(), QStringLiteral("Debe estar entre 0.000  y 100.000 ."));
    edit->setText(QStringLiteral("100.1"));
    EXPECT_FALSE(edit->commit());
    ASSERT_EQ(warnings.size(), 1u);
    EXPECT_EQ(warnings[0], QStringLiteral("El valor de 'Coeficiente de pandeo' debe estar entre 0.000  y 100.000 ."));
    EXPECT_EQ(edit->text(), QStringLiteral("1.000")) << "the previous value comes back";
    edit->setText(QStringLiteral("0.7234"));
    EXPECT_TRUE(edit->commit());
    EXPECT_EQ(edit->text(), QStringLiteral("0.723"));
    EXPECT_EQ(dialog.edit().xy.beta, 0.723);
    edit->setText(QStringLiteral("0.0009"));
    EXPECT_TRUE(edit->commit());
    EXPECT_EQ(edit->text(), QStringLiteral("0.001"));
    EXPECT_EQ(dialog.edit().xy.beta, 0.001);
    dialog.chooseKind(1, BucklingKind::UserLength);
    dialog.lengthEdit(1)->setText(QStringLiteral("0"));
    EXPECT_FALSE(dialog.lengthEdit(1)->commit());
    EXPECT_EQ(warnings.back(), QStringLiteral("El valor de 'Longitud' debe estar entre 0.001 y 1000.000."));
}

TEST_F(BucklingToolTest, SteelVariantShowsMomentBlocks) {
    BucklingDialog dialog(true);
    ASSERT_NE(dialog.assignMomentCheck(0), nullptr);
    EXPECT_EQ(dialog.assignMomentCheck(0)->text(), QStringLiteral("Asignar coeficiente de momentos (Plano xy)"));
    EXPECT_EQ(dialog.assignMomentCheck(1)->text(), QStringLiteral("Asignar coeficiente de momentos (Plano xz)"));
    EXPECT_TRUE(dialog.assignMomentCheck(0)->isChecked());
    EXPECT_EQ(dialog.momentEdit(0)->text(), QStringLiteral("1.000"));
    dialog.momentEdit(1)->setText(QStringLiteral("10.5"));
    EXPECT_FALSE(dialog.momentEdit(1)->commit());
    EXPECT_EQ(warnings.back(), QStringLiteral("El valor de 'Coeficiente de momento equivalente Cm' debe estar entre 0.000  y 10.000 ."));
    dialog.momentEdit(1)->setText(QStringLiteral("0.85"));
    EXPECT_TRUE(dialog.momentEdit(1)->commit());
    const BucklingEdit edit = dialog.edit();
    EXPECT_TRUE(edit.assignMomentXZ);
    EXPECT_EQ(edit.xz.momentCoeff, 0.85);
}

TEST_F(BucklingToolTest, RmbAssignsTheDialogToTheSelectedBarsAsOneUndoableStep) {
    tool->betaAction()->trigger();
    int runs = 0;
    tool->setDialogRunner([&runs](QDialog& d) {
        ++runs;
        auto& dialog = static_cast<BucklingDialog&>(d);
        dialog.chooseKind(0, BucklingKind::Cantilever);
        dialog.chooseKind(1, BucklingKind::UserLength);
        dialog.lengthEdit(1)->setValue(2.0);
        dialog.considerCheck()->setChecked(false);
        return static_cast<int>(QDialog::Accepted);
    });
    selectAll();
    EXPECT_EQ(view->selection().ids(ElementKind::Bar), (std::vector<int>{1, 2}));
    click(*view, QPointF(400, 500), Qt::RightButton);
    EXPECT_EQ(runs, 1);
    EXPECT_EQ(undo.count(), 1u);
    for (int bar : {1, 2}) {
        const BarBuckling b = bucklingOf(project.buckling, bar);
        EXPECT_EQ(b.xy.beta, 2.0);
        EXPECT_EQ(b.xy.kind, BucklingKind::Cantilever);
        EXPECT_EQ(b.xz.kind, BucklingKind::UserLength);
        EXPECT_FALSE(b.considerInAnalysis);
        EXPECT_TRUE(b.xy.reviewed);
    }
    EXPECT_TRUE(view->selection().empty());
    undo.undo();
    EXPECT_TRUE(project.buckling.empty());
    undo.redo();
    EXPECT_EQ(bucklingOf(project.buckling, 1).xy.beta, 2.0);
}

TEST_F(BucklingToolTest, CancelChangesNothingAndTheDialogOpensWithTheFirstBar) {
    BarBuckling stored;
    stored.xy = bucklingPlaneFromBeta(0.5);
    stored.xz = bucklingPlaneFromBeta(1.3);
    project.buckling.bars[1] = stored;
    BucklingKind first = BucklingKind::None, second = BucklingKind::None;
    tool->setDialogRunner([&](QDialog& d) {
        auto& dialog = static_cast<BucklingDialog&>(d);
        first = dialog.kind(0);
        second = dialog.kind(1);
        EXPECT_EQ(dialog.betaEdit(1)->text(), QStringLiteral("1.300"));
        return static_cast<int>(QDialog::Rejected);
    });
    EXPECT_FALSE(tool->editBuckling({1, 2}));
    EXPECT_EQ(first, BucklingKind::FixedFixed);
    EXPECT_EQ(second, BucklingKind::UserBeta);
    EXPECT_EQ(undo.count(), 0u);
}

TEST_F(BucklingToolTest, LateralDialogTextsAndDefaults) {
    LateralBucklingDialog dialog;
    EXPECT_EQ(dialog.windowTitle(), QStringLiteral("Pandeo lateral"));
    EXPECT_EQ(dialog.assignCheck(0)->text(), QStringLiteral("Ala superior (S)"));
    EXPECT_EQ(dialog.assignCheck(1)->text(), QStringLiteral("Ala inferior (I)"));
    EXPECT_EQ(dialog.assignC1Check()->text(), QStringLiteral("Asignar factor de momento crítico"));
    EXPECT_EQ(dialog.c1Edit()->text(), QStringLiteral("1.000"));
    const QStringList tips = {"", "Barra biempotrada", "Barra biapoyada", "Barra en voladizo", "Coeficiente de pandeo lateral", "Separación entre arriostramientos"};
    for (int flange = 0; flange < 2; ++flange) {
        EXPECT_TRUE(dialog.kindButton(flange, LateralKind::None)->toolTip().contains(QStringLiteral("No comprueba pandeo lateral")));
        EXPECT_TRUE(dialog.kindButton(flange, LateralKind::None)->toolTip().contains(QStringLiteral("no se realice la comprobación de pandeo lateral.")));
        for (int k = 1; k < 6; ++k) EXPECT_EQ(dialog.kindButton(flange, static_cast<LateralKind>(k))->toolTip(), tips[k]);
        EXPECT_EQ(dialog.kind(flange), LateralKind::None);
        EXPECT_EQ(dialog.coeffEdit(flange)->text(), QStringLiteral("1.000"));
        EXPECT_EQ(dialog.spacingEdit(flange)->text(), QStringLiteral("3.000"));
        EXPECT_EQ(dialog.momentEdit(flange)->text(), QStringLiteral("1.000"));
        EXPECT_TRUE(dialog.momentEdit(flange)->parentWidget()->isHidden()) << "option 1: no fields";
    }
    dialog.chooseKind(0, LateralKind::Length);
    EXPECT_FALSE(dialog.momentEdit(0)->parentWidget()->isHidden());
    EXPECT_TRUE(dialog.coeffEdit(0)->parentWidget()->isHidden());
    dialog.chooseKind(0, LateralKind::UserCoeff);
    EXPECT_FALSE(dialog.coeffEdit(0)->isHidden());
    EXPECT_TRUE(dialog.spacingEdit(0)->isHidden());
    EXPECT_EQ(dialog.firstFieldLabel(0)->text(), QStringLiteral("Coeficiente de pandeo lateral"));
    dialog.chooseKind(0, LateralKind::UserSpacing);
    EXPECT_FALSE(dialog.spacingEdit(0)->isHidden());
    EXPECT_FALSE(dialog.unitLabel(0)->isHidden());
    EXPECT_EQ(dialog.firstFieldLabel(0)->text(), QStringLiteral("Separación entre arriostramientos"));
    EXPECT_EQ(dialog.unitLabel(0)->text(), QStringLiteral("m"));
}

TEST_F(BucklingToolTest, LateralFieldRanges) {
    LateralBucklingDialog dialog;
    dialog.chooseKind(0, LateralKind::UserCoeff);
    dialog.coeffEdit(0)->setText(QStringLiteral("10.1"));
    EXPECT_FALSE(dialog.coeffEdit(0)->commit());
    EXPECT_EQ(warnings.back(), QStringLiteral("El valor de 'Coeficiente de pandeo lateral' debe estar entre 0.000  y 10.000 ."));
    dialog.coeffEdit(0)->setText(QStringLiteral("0"));
    EXPECT_TRUE(dialog.coeffEdit(0)->commit()) << "0 accepted (measured)";
    dialog.chooseKind(1, LateralKind::UserSpacing);
    dialog.spacingEdit(1)->setText(QStringLiteral("0"));
    EXPECT_FALSE(dialog.spacingEdit(1)->commit());
    EXPECT_EQ(warnings.back(), QStringLiteral("El valor de 'Separación entre arriostramientos' debe estar entre 0.001 y 1000.000."));
    dialog.c1Edit()->setText(QStringLiteral("100"));
    EXPECT_FALSE(dialog.c1Edit()->commit());
    EXPECT_EQ(warnings.back(), QStringLiteral("El valor de '' debe estar entre 0.000  y 10.000 ."));
    dialog.chooseKind(1, LateralKind::Length);
    dialog.momentEdit(1)->setText(QStringLiteral("10.5"));
    EXPECT_FALSE(dialog.momentEdit(1)->commit());
    EXPECT_EQ(warnings.back(), QStringLiteral("El valor de 'Coeficiente de momento equivalente Cm' debe estar entre 0.000  y 10.000 ."));
}

TEST_F(BucklingToolTest, LateralAssignViaTool) {
    tool->lateralAction()->trigger();
    tool->setDialogRunner([](QDialog& d) {
        auto& dialog = static_cast<LateralBucklingDialog&>(d);
        dialog.chooseKind(0, LateralKind::UserSpacing);
        dialog.spacingEdit(0)->setValue(2.5);
        dialog.momentEdit(0)->setValue(0.85);
        dialog.chooseKind(1, LateralKind::DoubleLength);
        dialog.c1Edit()->setValue(1.5);
        return static_cast<int>(QDialog::Accepted);
    });
    selectAll();
    click(*view, QPointF(400, 500), Qt::RightButton);
    EXPECT_EQ(undo.count(), 1u);
    const BarBuckling b = bucklingOf(project.buckling, 2);
    EXPECT_EQ(b.top.kind, LateralKind::UserSpacing);
    EXPECT_EQ(b.top.spacingM, 2.5);
    EXPECT_EQ(b.top.momentCoeff, 0.85);
    EXPECT_EQ(b.bottom.kind, LateralKind::DoubleLength);
    EXPECT_EQ(b.c1, 1.5);
    EXPECT_TRUE(b.top.reviewed);
    undo.undo();
    EXPECT_TRUE(project.buckling.empty());
}

TEST_F(BucklingToolTest, LabelsAreDrawnOnlyWithTheTools) {
    // Paint offscreen: the labels must not crash and the tool states are the viewport contract tested above.
    tool->betaAction()->trigger();
    EXPECT_FALSE(view->grab().isNull());
    tool->lateralAction()->trigger();
    EXPECT_FALSE(view->grab().isNull());
    EXPECT_EQ(view->buckling(), &project.buckling);
}

int main(int argc, char** argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    app();
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
