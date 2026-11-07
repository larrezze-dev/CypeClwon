// Propiedades > Laminas (T-P6-07, GUI part): dialogs, tool, band editor and the viewport state. Expected values: docs/research/ui/propiedades_laminas.md
// (REA + measured in the original). Never shown on screen (offscreen).
#include <gtest/gtest.h>

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QLabel>
#include <QMouseEvent>
#include <QRadioButton>
#include <QToolButton>
#include <memory>

#include "RangedEdit.h"
#include "ShellBandsDialog.h"
#include "ShellPropertiesDialogs.h"
#include "ShellPropertiesTool.h"
#include "SupportDialogs.h"
#include "UserMessages.h"
#include "Viewport3D.h"
#include "core/picking.h"
#include "core/shell_properties_edit.h"

using namespace cc::core;
using namespace cc::app;
namespace messages = cc::app::messages;

namespace {

QApplication* app() {
    static int argc = 1;
    static char name[] = "shell_properties_tool_tests";
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

/// Shell L1: 4 x 4 m square in the XY plane (nodes 1..4), seen from above.
class ShellPropertiesToolTest : public ::testing::Test {
protected:
    void SetUp() override {
        app();
        project.structure.addNode({0, 0, 0});
        project.structure.addNode({4, 0, 0});
        project.structure.addNode({4, 4, 0});
        project.structure.addNode({0, 4, 0});
        project.structure.addShell({1, 2, 3, 4}, 0.1);
        view = std::make_unique<Viewport3D>();
        view->resize(800, 600);
        view->setStandardView(StandardView::Top);
        view->setOrthographic(true);
        view->camera().target = {2, 2, 0};
        view->setStructure(&project.structure);
        tool = std::make_unique<ShellPropertiesTool>();
        tool->setProject(&project, &undo);
        tool->addViewport(view.get());
        undo.setChangeCallback([this] { tool->structureChanged(); });
        QObject::connect(tool.get(), &ShellPropertiesTool::statusChanged, [this](const QString& text) { status = text; });
        RangedEdit::setWarningHandler([this](QWidget*, const QString& text) { warnings.push_back(text); });
        messages::handler() = [this](QWidget*, messages::Kind, const QString& text) {
            notices.push_back(text);
            return true;
        };
        undo.setClean();
    }
    void TearDown() override {
        RangedEdit::setWarningHandler(nullptr);
        messages::handler() = nullptr;
        ShellBandsDialog::setDialogRunner(nullptr);
    }
    QPointF screenOf(double x, double y) {
        QPointF p;
        EXPECT_TRUE(view->worldToScreen({x, y, 0}, p));
        return p;
    }
    template <typename Dialog, typename Fill>
    void answer(bool accept, Fill fill) {
        tool->setDialogRunner([=](QDialog& dialog) {
            fill(static_cast<Dialog&>(dialog));
            if (accept) dialog.accept();
            return accept ? static_cast<int>(QDialog::Accepted) : static_cast<int>(QDialog::Rejected);
        });
    }
    ShellRecord record() const { return shellRecordOf(project.shellProps, 1); }
    Project project;
    UndoStack undo;
    std::unique_ptr<Viewport3D> view;
    std::unique_ptr<ShellPropertiesTool> tool;
    QString status;
    QStringList warnings, notices;
};

}  // namespace

// ---- dialogs ------------------------------------------------------------------------------------------------------------------

TEST_F(ShellPropertiesToolTest, DiscretisationDialogMeasured) {
    ShellDiscretisationDialog dialog(1.0);
    EXPECT_EQ(dialog.windowTitle(), QStringLiteral("Discretización"));
    EXPECT_TRUE(dialog.infoLabel()->text().startsWith(QStringLiteral("Las láminas se discretizan mediante una malla de triángulos")));
    EXPECT_TRUE(dialog.infoLabel()->text().contains(QStringLiteral("\nTenga en cuenta que el programa realiza la compatibilidad entre mallas")));
    EXPECT_EQ(dialog.edit()->text(), QStringLiteral("1.00"));
    dialog.edit()->setText(QStringLiteral("0.001"));
    EXPECT_FALSE(dialog.edit()->commit());
    ASSERT_EQ(warnings.size(), 1);
    EXPECT_EQ(warnings[0], QStringLiteral("El valor de 'Tamaño máximo de los lados de un triángulo' debe estar entre 0.01 m y 100.00 m."));
    EXPECT_DOUBLE_EQ(dialog.maxSide(), 1.0) << "the previous value is recovered";
    dialog.edit()->setText(QStringLiteral("0.5"));
    dialog.accept();
    EXPECT_DOUBLE_EQ(dialog.maxSide(), 0.5);
    EXPECT_EQ(dialog.result(), QDialog::Accepted);
}

TEST_F(ShellPropertiesToolTest, StiffnessDialogOrderAndWarning) {
    ShellStiffnessFactors f;
    ShellStiffnessDialog dialog(f);
    EXPECT_EQ(dialog.windowTitle(), QStringLiteral("Factores de rigidez"));
    EXPECT_EQ(dialog.edit(0)->text(), QStringLiteral("1.000"));
    dialog.edit(0)->setText(QStringLiteral("100.5"));
    EXPECT_FALSE(dialog.edit(0)->commit());
    ASSERT_EQ(warnings.size(), 1);
    EXPECT_EQ(warnings[0], QStringLiteral("El valor de 'Membrana Fxx' debe estar entre 0.000  y 100.000 ."));
    dialog.edit(2)->setText(QStringLiteral("0.12345"));
    dialog.edit(7)->setText(QStringLiteral("0"));
    dialog.accept();
    EXPECT_DOUBLE_EQ(dialog.factors().fyy, 0.123) << "third row = Membrana Fyy; 0.12345 -> 0.123 (measured: 3 decimals)";
    EXPECT_DOUBLE_EQ(dialog.factors().vyz, 0.0);
    EXPECT_DOUBLE_EQ(dialog.factors().fxx, 1.0) << "the rejected 100.5 left the previous value";
}

TEST_F(ShellPropertiesToolTest, EdgeInteriorDialogDefaultsAndGeneric) {
    EdgeInteriorDialog dialog;
    EXPECT_TRUE(dialog.kindRadio(EdgeInteriorKind::Fixed)->isChecked()) << "Empotrada by default";
    EXPECT_TRUE(dialog.kindRadio(EdgeInteriorKind::Fixed)->font().bold());
    dialog.selectKind(EdgeInteriorKind::Generic);
    EXPECT_FALSE(dialog.genericPanel()->isHidden());
    EXPECT_TRUE(dialog.modeRadio(0, DofMode::Free)->isChecked());
    EXPECT_TRUE(dialog.kEdit(3)->isHidden()) << "K only appears with Elástico";
    dialog.modeRadio(3, DofMode::Elastic)->setChecked(true);
    EXPECT_FALSE(dialog.kEdit(3)->isHidden());
    EXPECT_EQ(dialog.kEdit(3)->text(), QStringLiteral("100000"));
    dialog.modeRadio(1, DofMode::Fixed)->setChecked(true);
    const EdgeInteriorBond bond = dialog.bond();
    EXPECT_EQ(bond.kind, EdgeInteriorKind::Generic);
    EXPECT_EQ(bond.dof[1], DofMode::Fixed);
    EXPECT_EQ(bond.dof[3], DofMode::Elastic);
    dialog.selectKind(EdgeInteriorKind::Pinned);
    EXPECT_TRUE(dialog.genericPanel()->isHidden());
}

TEST_F(ShellPropertiesToolTest, ConsiderAndBandForceDialogs) {
    ShellConsiderDialog buckling(ShellConsiderDialog::Kind::Buckling, false);
    EXPECT_EQ(buckling.windowTitle(), QStringLiteral("Pandeo"));
    EXPECT_EQ(buckling.check()->text(), QStringLiteral("Considerar en el análisis de pandeo"));
    EXPECT_FALSE(buckling.considered());
    ShellConsiderDialog modal(ShellConsiderDialog::Kind::Modal, true);
    EXPECT_EQ(modal.windowTitle(), QStringLiteral("Análisis modal"));
    EXPECT_EQ(modal.check()->text(), QStringLiteral("Considerar en el análisis"));
    EXPECT_TRUE(modal.considered());

    BandForceDialog force;
    EXPECT_TRUE(force.internalRadio()->isChecked());
    EXPECT_TRUE(force.smoothCheck()->isChecked());
    EXPECT_TRUE(force.smoothCheck()->isEnabled());
    force.nodalRadio()->setChecked(true);
    EXPECT_FALSE(force.smoothCheck()->isEnabled()) << "the smoothing only applies to the internal stresses";
    EXPECT_EQ(force.options().method, BandForceMethod::NodalForces);

    BandWidthDialog width(QStringLiteral("Nueva"));
    EXPECT_EQ(width.edit()->text(), QStringLiteral("15.0"));
    width.edit()->setText(QStringLiteral("10001"));
    EXPECT_FALSE(width.edit()->commit());
    EXPECT_EQ(warnings.back(), QStringLiteral("El valor de 'Anchura' debe estar entre 1.0 y 10000.0."));
    width.edit()->setText(QStringLiteral("20"));
    width.accept();
    EXPECT_DOUBLE_EQ(width.widthM(), 0.2);
    BandDivisionsDialog divisions;
    EXPECT_EQ(divisions.edit()->text(), QStringLiteral("2"));
}

// ---- tool ---------------------------------------------------------------------------------------------------------------------

TEST_F(ShellPropertiesToolTest, RibbonActionsStatusAndDisabledWithoutShells) {
    EXPECT_EQ(tool->discretisationAction()->text(), QStringLiteral("Discretización"));
    EXPECT_EQ(tool->stiffnessAction()->text(), QStringLiteral("Factores de rigidez"));
    EXPECT_EQ(tool->bandsAction()->text(), QStringLiteral("Bandas de integración"));
    EXPECT_EQ(tool->interiorAction()->text(), QStringLiteral("Vinculación interior"));
    EXPECT_EQ(tool->exteriorAction()->text(), QStringLiteral("Vinculación exterior"));
    EXPECT_EQ(tool->bucklingAction()->text(), QStringLiteral("Pandeo"));
    EXPECT_EQ(tool->modalAction()->text(), QStringLiteral("Análisis modal"));
    EXPECT_EQ(tool->discretisationAction()->toolTip(), QStringLiteral("Edición de los parámetros para configurar la discretización de un grupo de láminas."));
    EXPECT_TRUE(tool->discretisationAction()->isEnabled());
    ShellPropertiesTool other;
    EXPECT_FALSE(other.discretisationAction()->isEnabled()) << "no project";
    EXPECT_EQ(ShellPropertiesTool::statusText(ShellPropertiesTool::Mode::Discretisation).trimmed(),
              QStringLiteral("Láminas - Discretización |  - LMB: Seleccione un grupo de elementos. - RMB: Actuar sobre los elementos seleccionados."));
    tool->discretisationAction()->trigger();
    EXPECT_EQ(tool->mode(), ShellPropertiesTool::Mode::Discretisation);
    EXPECT_TRUE(status.startsWith(QStringLiteral("Láminas - Discretización |")));
    EXPECT_TRUE(view->selectionMode());
    EXPECT_TRUE(view->shellContourPick());
    tool->deactivate();
    EXPECT_FALSE(view->selectionMode());
}

TEST_F(ShellPropertiesToolTest, ShellIsSelectedByItsOutlineNotByItsCentre) {
    tool->discretisationAction()->trigger();
    click(*view, screenOf(2, 2));  // centre: nothing
    EXPECT_TRUE(view->selection().ids(ElementKind::Shell).empty());
    EXPECT_TRUE(view->windowPending()) << "a click on empty space starts a window";
    view->cancelWindow();
    const QPointF onEdge = screenOf(2, 0);
    click(*view, onEdge);
    EXPECT_EQ(view->selection().ids(ElementKind::Shell), std::vector<int>{1});
}

TEST_F(ShellPropertiesToolTest, DiscretisationOneUndoStepAndInfoBoxLines) {
    tool->discretisationAction()->trigger();
    click(*view, screenOf(2, 0));
    answer<ShellDiscretisationDialog>(true, [](ShellDiscretisationDialog& d) { d.edit()->setText(QStringLiteral("0.25")); });
    click(*view, screenOf(2, 0), Qt::RightButton);
    EXPECT_DOUBLE_EQ(record().maxTriangleSideM, 0.25);
    EXPECT_EQ(undo.count(), 1u);
    EXPECT_TRUE(view->selection().empty());
    const QStringList lines = tool->infoLines(1, std::nullopt);
    EXPECT_EQ(lines[0], QStringLiteral("L1"));
    EXPECT_TRUE(lines.contains(QStringLiteral("\x01") + QStringLiteral("Tamaño máximo de los lados de un triángulo : 0.25 m")));
    EXPECT_TRUE(lines.contains(QStringLiteral("\x02") + QStringLiteral("Análisis de pandeo")));
    EXPECT_TRUE(lines.contains(QStringLiteral("No se considera en el análisis")));
    undo.undo();
    EXPECT_DOUBLE_EQ(record().maxTriangleSideM, 1.0);
    // Cancelar changes nothing.
    click(*view, screenOf(2, 0));
    answer<ShellDiscretisationDialog>(false, [](ShellDiscretisationDialog&) {});
    click(*view, screenOf(2, 0), Qt::RightButton);
    EXPECT_DOUBLE_EQ(record().maxTriangleSideM, 1.0) << "Cancelar changes nothing";
}

TEST_F(ShellPropertiesToolTest, StiffnessBucklingAndModalAssign) {
    answer<ShellStiffnessDialog>(true, [](ShellStiffnessDialog& d) { d.edit(4)->setText(QStringLiteral("0.5")); });
    EXPECT_TRUE(tool->editStiffness({1}));
    EXPECT_DOUBLE_EQ(record().stiffness.mxy, 0.5);
    answer<ShellConsiderDialog>(true, [](ShellConsiderDialog& d) { d.check()->setChecked(true); });
    EXPECT_TRUE(tool->editBuckling({1}));
    EXPECT_TRUE(record().bucklingConsidered);
    answer<ShellConsiderDialog>(true, [](ShellConsiderDialog& d) { d.check()->setChecked(false); });
    EXPECT_TRUE(tool->editModal({1}));
    EXPECT_FALSE(record().modalConsidered);
    EXPECT_EQ(undo.count(), 3u);
    EXPECT_FALSE(tool->editModal({99})) << "unknown shell";
}

TEST_F(ShellPropertiesToolTest, EdgeSelectionAndInteriorBond) {
    tool->interiorAction()->trigger();
    EXPECT_TRUE(view->edgeSelectionMode());
    click(*view, screenOf(4, 2));  // edge N2-N3
    ASSERT_EQ(view->edgeSelection().size(), 1u);
    EXPECT_EQ(view->edgeSelection()[0], makeEdgeKey(1, 2, 3));
    // Shift removes, Ctrl adds
    answer<EdgeInteriorDialog>(true, [](EdgeInteriorDialog& d) { d.selectKind(EdgeInteriorKind::Pinned); });
    click(*view, screenOf(4, 2), Qt::RightButton);
    EXPECT_EQ(edgeInteriorOf(project.shellProps, makeEdgeKey(1, 2, 3)).kind, EdgeInteriorKind::Pinned);
    EXPECT_EQ(edgeInteriorOf(project.shellProps, makeEdgeKey(1, 1, 2)).kind, EdgeInteriorKind::Fixed);
    EXPECT_EQ(undo.count(), 1u);
    EXPECT_TRUE(view->edgeSelection().empty());
    const std::optional<ShellEdgeKey> edge = makeEdgeKey(1, 2, 3);
    const QStringList lines = tool->infoLines(1, edge);
    EXPECT_EQ(lines[2], QStringLiteral("Vinc. interior : Articulada"));
}

TEST_F(ShellPropertiesToolTest, ExteriorUsesNodeDialogAndEdgeTitle) {
    tool->exteriorAction()->trigger();
    const std::optional<ShellEdgeKey> last = makeEdgeKey(1, 4, 1);
    EXPECT_EQ(tool->infoLines(1, last), QStringList{QStringLiteral("Arista N4 / N1")}) << "contour order, measured";
    // window over the whole shell selects the four edges
    click(*view, screenOf(-0.5, 4.5));
    click(*view, screenOf(4.5, -0.5));
    EXPECT_EQ(view->edgeSelection().size(), 4u);
    answer<ExternalSupportDialog>(true, [](ExternalSupportDialog& d) { d.selectType(ExternalSupportType::Pinned); });
    click(*view, screenOf(2, 0), Qt::RightButton);
    EXPECT_EQ(project.shellProps.exterior.size(), 4u);
    EXPECT_EQ(undo.count(), 1u);
    undo.undo();
    EXPECT_TRUE(project.shellProps.exterior.empty());
}

// ---- picking (core) -----------------------------------------------------------------------------------------------------------

TEST_F(ShellPropertiesToolTest, PickingEdgesAndContour) {
    ScreenMapper map{&view->camera(), 800.0, 600.0};
    const QPointF p = screenOf(2, 4);
    const auto edge = pickShellEdgeAt(project.structure, map, p.x(), p.y() + 2);
    ASSERT_TRUE(edge.has_value());
    EXPECT_EQ(*edge, makeEdgeKey(1, 3, 4));
    const QPointF c = screenOf(2, 2);
    EXPECT_FALSE(pickShellEdgeAt(project.structure, map, c.x(), c.y()).has_value());
    PickOptions options;
    options.shellContourOnly = true;
    SelectionFilter filter = SelectionFilter::only(ElementKind::Shell);
    EXPECT_FALSE(pickAt(project.structure, map, filter, c.x(), c.y(), options).has_value());
    EXPECT_TRUE(pickAt(project.structure, map, filter, c.x(), c.y()).has_value()) << "default behaviour unchanged";
}

// ---- band editor ----------------------------------------------------------------------------------------------------------------

namespace {
struct BandsFixture {
    std::vector<Point2> contour{{0, 0}, {4, 0}, {4, 4}, {0, 4}};
};
}  // namespace

TEST_F(ShellPropertiesToolTest, BandsDialogNewGenerateEditDivideJoinDeleteAndHistory) {
    const BandsFixture fx;
    ShellBandsDialog dialog(fx.contour, {}, BandForceOptions{});
    int widthDialogs = 0;
    ShellBandsDialog::setDialogRunner([&](QDialog& d) {
        if (auto* w = dynamic_cast<BandWidthDialog*>(&d)) {
            ++widthDialogs;
            if (d.windowTitle() != QStringLiteral("Editar")) w->edit()->setText(QStringLiteral("20"));
            else w->edit()->setText(QStringLiteral("30"));
            d.accept();
            return static_cast<int>(QDialog::Accepted);
        }
        if (auto* n = dynamic_cast<BandDivisionsDialog*>(&d)) {
            n->edit()->setText(QStringLiteral("3"));
            d.accept();
            return static_cast<int>(QDialog::Accepted);
        }
        return static_cast<int>(QDialog::Rejected);
    });
    EXPECT_EQ(dialog.windowTitle(), QStringLiteral("Bandas de integración"));
    EXPECT_FALSE(dialog.referencesCheck()->isChecked());
    EXPECT_EQ(dialog.referencesCheck()->text(), QStringLiteral("Referencias de bandas"));
    EXPECT_FALSE(dialog.button(QStringLiteral("undo"))->isEnabled());
    EXPECT_EQ(dialog.button(QStringLiteral("undo"))->toolTip(), QStringLiteral("Deshacer (Ctrl+Z)"));

    // Nueva: the width dialog first, two points, RMB creates and asks again.
    dialog.button(QStringLiteral("new"))->click();
    EXPECT_EQ(widthDialogs, 1);
    EXPECT_EQ(dialog.viewer()->tool(), BandViewer::Tool::New);
    dialog.viewer()->click({1, 2});
    dialog.viewer()->click({3, 2});
    dialog.viewer()->rightClick();
    ASSERT_EQ(dialog.bands().size(), 1u);
    EXPECT_DOUBLE_EQ(dialog.bands()[0].widthM, 0.2);
    EXPECT_EQ(widthDialogs, 2) << "the right button opens «Nueva» again";
    EXPECT_TRUE(dialog.button(QStringLiteral("undo"))->isEnabled());

    // A point outside the shell is not taken.
    dialog.viewer()->click({9, 9});
    EXPECT_TRUE(dialog.viewer()->points().empty());

    // Editar: select by click, RMB, dialog «Editar» (30 cm).
    dialog.button(QStringLiteral("edit"))->click();
    dialog.viewer()->click({2, 2});
    ASSERT_EQ(dialog.viewer()->selected().size(), 1u);
    dialog.viewer()->rightClick();
    EXPECT_DOUBLE_EQ(dialog.bands()[0].widthM, 0.3);

    // Dividir: 3 parts.
    dialog.button(QStringLiteral("divide"))->click();
    dialog.viewer()->click({2, 2});
    dialog.viewer()->rightClick();
    EXPECT_EQ(dialog.bands().size(), 3u);

    // Unir the three again (window from left to right around them).
    dialog.button(QStringLiteral("join"))->click();
    dialog.viewer()->click({0.5, 1});
    dialog.viewer()->click({3.5, 3});
    EXPECT_EQ(dialog.viewer()->selected().size(), 3u);
    dialog.viewer()->rightClick();
    EXPECT_EQ(dialog.bands().size(), 1u);

    // Mover extremo: click the end, then the new place.
    dialog.button(QStringLiteral("move"))->click();
    const ShellIntegrationBand b = dialog.bands()[0];
    dialog.viewer()->click({b.x2, b.y2});
    dialog.viewer()->click({3, 3});
    EXPECT_NEAR(dialog.bands()[0].y2, 3.0, 1e-9);

    // Borrar.
    dialog.button(QStringLiteral("delete"))->click();
    dialog.viewer()->click({1.5, 2.25});
    dialog.viewer()->rightClick();
    EXPECT_TRUE(dialog.bands().empty());

    // Deshacer / Rehacer walk the history.
    dialog.undo();
    EXPECT_EQ(dialog.bands().size(), 1u);
    dialog.redo();
    EXPECT_TRUE(dialog.bands().empty());
    EXPECT_FALSE(dialog.canRedo());

    // Generar: four clicks, RMB.
    dialog.button(QStringLiteral("generate"))->click();
    dialog.viewer()->click({0, 0});
    dialog.viewer()->click({4, 0});
    dialog.viewer()->click({4, 1});
    dialog.viewer()->click({0, 1});
    dialog.viewer()->rightClick();
    EXPECT_EQ(dialog.bands().size(), 5u) << "1 m / 0.20 m";
}

TEST_F(ShellPropertiesToolTest, BandsDialogErrorsShowTheOriginalText) {
    const BandsFixture fx;
    ShellBandsDialog dialog(fx.contour, {}, BandForceOptions{});
    ShellBandsDialog::setDialogRunner([](QDialog& d) { d.accept(); return static_cast<int>(QDialog::Accepted); });
    dialog.button(QStringLiteral("new"))->click();
    dialog.viewer()->click({1, 0.05});
    dialog.viewer()->click({3, 0.05});
    dialog.viewer()->rightClick();  // 15 cm wide strip crossing the edge: not completely contained
    EXPECT_TRUE(dialog.bands().empty());
    ASSERT_FALSE(notices.isEmpty());
    EXPECT_EQ(notices.last(), QStringLiteral("La banda de integración no está completamente contenida en la lámina."));
}

TEST_F(ShellPropertiesToolTest, BandsCommandAssignsBandsAndMethodAsOneStep) {
    tool->bandsAction()->trigger();
    ShellBandsDialog::setDialogRunner([](QDialog& d) { d.accept(); return static_cast<int>(QDialog::Accepted); });
    tool->setDialogRunner([](QDialog& d) {
        auto& dialog = static_cast<ShellBandsDialog&>(d);
        dialog.button(QStringLiteral("new"))->click();
        dialog.viewer()->click({1, 2});
        dialog.viewer()->click({3, 2});
        dialog.viewer()->rightClick();
        dialog.button(QStringLiteral("new"))->click();  // leave the command
        d.accept();
        return static_cast<int>(QDialog::Accepted);
    });
    EXPECT_TRUE(tool->editBands({1}));
    ASSERT_EQ(record().bands.size(), 1u);
    EXPECT_DOUBLE_EQ(record().bands[0].widthM, 0.15);
    EXPECT_EQ(undo.count(), 1u);
    undo.undo();
    EXPECT_TRUE(record().bands.empty());
}

int main(int argc, char** argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    app();
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
