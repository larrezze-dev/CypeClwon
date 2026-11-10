// Carga > Paños and Cargas superficiales (T-P7-05, GUI part): tool, dialogs, picking. Expected values: docs/research/ui/cargas_panos.md (measured in the
// original, sections 10-12). Never shown on screen (offscreen).
#include <gtest/gtest.h>

#include <QAction>
#include <QApplication>
#include <QKeyEvent>
#include <QCheckBox>
#include <QHeaderView>
#include <QComboBox>
#include <QLineEdit>
#include <QLabel>
#include <QMouseEvent>
#include <QRadioButton>
#include <QTabWidget>
#include <QTableWidget>
#include <QToolButton>
#include <memory>

#include "NodalLoadDialogs.h"
#include "NodalLoadTool.h"
#include "PanelDialogs.h"
#include "PanelPicking.h"
#include "PanelTool.h"
#include "UserMessages.h"
#include "Viewport3D.h"
#include "core/load_case_edit.h"

using namespace cc::core;
using cc::app::NodalLoadTool;
using cc::app::PanelLoadsDialog;
using cc::app::PanelTool;
using cc::app::SurfaceLoadDialog;
using cc::app::Viewport3D;
namespace messages = cc::app::messages;
namespace picking = cc::app::picking;
using Mode = PanelTool::Mode;

namespace {

QApplication* app() {
    static int argc = 1;
    static char name[] = "panel_tool_tests";
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

/// Four nodes 4 x 4 m on Z = 0 seen from the top with the four bars of the rectangle; no panels.
class PanelToolTest : public ::testing::Test {
protected:
    void SetUp() override {
        app();
        for (const Point3& p : {Point3{0, 0, 0}, Point3{4, 0, 0}, Point3{4, 4, 0}, Point3{0, 4, 0}}) project.structure.addNode(p);
        project.structure.addBar(1, 2, 0);
        project.structure.addBar(2, 3, 0);
        project.structure.addBar(3, 4, 0);
        project.structure.addBar(4, 1, 0);
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
        tool = std::make_unique<PanelTool>();
        tool->setHost(nodal.get());
        tool->setProject(&project, &undo);
        tool->addViewport(view.get());
        undo.setChangeCallback([this] {
            nodal->structureChanged();
            tool->structureChanged();
        });
        QObject::connect(tool.get(), &PanelTool::statusChanged, [this](const QString& text) { status = text; });
        QObject::connect(tool.get(), &PanelTool::aboutToActivate, nodal.get(), &NodalLoadTool::deactivate);
        shown.clear();
        answer = true;
        messages::handler() = [this](QWidget*, messages::Kind kind, const QString& text) {
            shown.push_back({kind, text});
            return kind == messages::Kind::Question ? answer : true;
        };
        undo.setClean();
    }
    void TearDown() override { messages::handler() = nullptr; }
    QPointF at(double x, double y, double z = 0.0) {
        QPointF p;
        EXPECT_TRUE(view->worldToScreen({x, y, z}, p));
        return p;
    }
    void rmb() { click(*view, QPointF(5, 590), Qt::RightButton); }
    void begin(Mode mode) { tool->action(mode)->setChecked(true); }
    void enterSquare() {
        for (const QPointF& p : {at(0, 0), at(4, 0), at(4, 4), at(0, 4)}) click(*view, p);
    }
    int addPanel(double value = 10.0) {
        std::vector<SurfaceLoadData> rows;
        if (value != 0.0) {
            rows.push_back(surfaceLoadDefaults(0));
            rows.back().value = value;
        }
        int id = 0;
        EXPECT_EQ(cc::core::addPanel(project, undo, {{0, 0, 0}, {4, 0, 0}, {4, 4, 0}, {0, 4, 0}}, 0, PanelDirectionMode::Parallel, rows, &id), PanelError::None);
        return id;
    }
    int addSurface(double value = 8.0) {
        SurfaceLoadData d = surfaceLoadDefaults(0);
        d.value = value;
        int id = 0;
        EXPECT_EQ(addSurfaceLoad(project, undo, {{0, 0, 0}, {2, 0, 0}, {2, 2, 0}, {0, 2, 0}}, d, false, &id), PanelError::None);
        return id;
    }
    template <class D, class F>
    void runner(F body) {
        tool->setDialogRunner([body](QDialog& dialog) {
            auto* d = dynamic_cast<D*>(&dialog);
            if (!d || !body(*d)) return static_cast<int>(QDialog::Rejected);
            d->accept();
            return static_cast<int>(d->result());
        });
    }
    Project project;
    UndoStack undo;
    std::unique_ptr<Viewport3D> view;
    std::unique_ptr<NodalLoadTool> nodal;
    std::unique_ptr<PanelTool> tool;
    QString status;
    std::vector<Shown> shown;
    bool answer = true;
};

}  // namespace

// ---- ribbon ------------------------------------------------------------------------------------------------------------------

TEST_F(PanelToolTest, EnabledStateFollowsTheMeasuredRules) {
    const auto enabled = [&](Mode m) { return tool->action(m)->isEnabled(); };
    // nodes, no panels, no surface loads: Nuevo / Nueva / Asignar only
    EXPECT_TRUE(enabled(Mode::PanelNew));
    EXPECT_TRUE(enabled(Mode::SurfNew));
    EXPECT_TRUE(enabled(Mode::SurfAssign));
    for (Mode m : {Mode::PanelEdit, Mode::PanelDelete, Mode::PanelMove, Mode::PanelDivide, Mode::PanelDirection, Mode::SurfEdit, Mode::SurfDelete, Mode::SurfCase,
                   Mode::SurfCopy, Mode::SurfMove})
        EXPECT_FALSE(enabled(m));
    addPanel();
    for (Mode m : {Mode::PanelEdit, Mode::PanelDelete, Mode::PanelMove, Mode::PanelDivide, Mode::PanelDirection}) EXPECT_TRUE(enabled(m));
    EXPECT_FALSE(enabled(Mode::SurfEdit));
    addSurface();
    for (Mode m : {Mode::SurfEdit, Mode::SurfDelete, Mode::SurfCase, Mode::SurfCopy, Mode::SurfMove}) EXPECT_TRUE(enabled(m));
}

TEST_F(PanelToolTest, WithoutNodesEverythingIsDisabled) {
    Project empty;
    tool->setProject(&empty, &undo);
    for (const auto& a : {Mode::PanelNew, Mode::SurfNew, Mode::SurfAssign, Mode::PanelEdit, Mode::SurfEdit}) EXPECT_FALSE(tool->action(a)->isEnabled());
}

TEST_F(PanelToolTest, TooltipsAreTheMeasuredOnes) {
    EXPECT_TRUE(PanelTool::tipText(Mode::PanelDivide).isEmpty());  // measured: no tooltip
    EXPECT_TRUE(PanelTool::tipText(Mode::SurfCase).isEmpty());
    EXPECT_TRUE(PanelTool::tipText(Mode::PanelNew).contains(QStringLiteral("Permite introducir un paño formado por un polígono cerrado")));
    EXPECT_TRUE(PanelTool::tipText(Mode::PanelDirection).contains(QStringLiteral("Modificar la dirección de reparto de cargas")));
    EXPECT_TRUE(PanelTool::tipText(Mode::SurfAssign).contains(QStringLiteral("Activar/desactivar asignación de cargas superficiales")));
    EXPECT_TRUE(PanelTool::tipText(Mode::SurfNew).contains(QStringLiteral("'Modificar la dirección de reparto de cargas'")));
    EXPECT_TRUE(PanelTool::tipText(Mode::PanelNew).contains(QStringLiteral("F1")));
    EXPECT_FALSE(PanelTool::tipText(Mode::PanelDelete).contains(QStringLiteral("F1")));  // measured: Borrar has no footer
}

TEST_F(PanelToolTest, StatusTextsAreTheMeasuredOnes) {
    EXPECT_EQ(PanelTool::statusText(Mode::PanelNew), QStringLiteral("Paños - Nuevo |  - LMB: Introduzca los puntos de la polilínea."));
    EXPECT_TRUE(PanelTool::statusText(Mode::PanelNew, 1).endsWith(QStringLiteral("asignadas al paño. - RMB: Comenzar otra vez.")));
    EXPECT_EQ(PanelTool::statusText(Mode::PanelDivide), QStringLiteral("Paños - Dividir |  - LMB: Introduzca un punto."));
    EXPECT_EQ(PanelTool::statusText(Mode::SurfAssign),
              QStringLiteral("Cargas superficiales - Asignar |  - LMB: Seleccione un grupo de elementos. - RMB: Actuar sobre los elementos seleccionados."));
    EXPECT_EQ(PanelTool::statusText(Mode::SurfCopy), QStringLiteral("Cargas superficiales - Copiar |  - LMB: Seleccione un elemento."));
    EXPECT_TRUE(PanelTool::statusText(Mode::PanelMove, 1).contains(QStringLiteral("se finaliza la ejecución")));
}

// ---- Nuevo paño --------------------------------------------------------------------------------------------------------------

TEST_F(PanelToolTest, NewPanelFlowCreatesThePanelAndOpensTheLoads) {
    bool opened = false;
    runner<PanelLoadsDialog>([&](PanelLoadsDialog& d) {
        opened = true;
        EXPECT_FALSE(d.tabbed());  // horizontal: a single list
        d.addRow();
        d.setCellText(false, 0, 2, QStringLiteral("10"));
        return true;
    });
    begin(Mode::PanelNew);
    EXPECT_EQ(status, PanelTool::statusText(Mode::PanelNew));
    enterSquare();
    EXPECT_EQ(tool->points().size(), 4u);
    rmb();  // closes the polyline: the box «Información» (#65132) and the step of the side
    ASSERT_EQ(shown.size(), 1u);
    EXPECT_EQ(shown[0].kind, messages::Kind::Information);
    EXPECT_EQ(shown[0].text, QStringLiteral("Seleccione la dirección de reparto para las cargas superficiales asignadas al paño."));
    EXPECT_EQ(tool->step(), 1);
    EXPECT_TRUE(project.panels.empty());
    click(*view, at(2, 0));  // LMB on the prolonged side 1-2 chooses it
    ASSERT_TRUE(tool->selectedSide().has_value());
    EXPECT_EQ(*tool->selectedSide(), 0);
    EXPECT_TRUE(project.panels.empty());  // LMB alone creates nothing (measured)
    rmb();
    EXPECT_TRUE(opened);
    ASSERT_EQ(project.panels.items.size(), 1u);
    const Panel& panel = project.panels.items.front();
    EXPECT_NEAR(panel.direction.x, 1.0, 1e-9);  // parallel to the side 1-2: the arrow along X
    ASSERT_EQ(panel.loads.size(), 1u);
    EXPECT_DOUBLE_EQ(panel.loads[0].value, 10.0);
    EXPECT_EQ(tool->step(), 0);  // back to the points of the polyline
    undo.undo();  // one undo step for the panel and its loads
    EXPECT_TRUE(project.panels.empty());
}

TEST_F(PanelToolTest, NewPanelCancelKeepsThePanelWithoutLoads) {
    runner<PanelLoadsDialog>([](PanelLoadsDialog&) { return false; });  // Cancelar
    begin(Mode::PanelNew);
    enterSquare();
    rmb();
    click(*view, at(0, 2));  // side 4-1
    rmb();
    ASSERT_EQ(project.panels.items.size(), 1u);
    EXPECT_TRUE(project.panels.items.front().loads.empty());
}

TEST_F(PanelToolTest, NewPanelRmbWithoutSideStartsAgain) {
    begin(Mode::PanelNew);
    enterSquare();
    rmb();
    ASSERT_EQ(tool->step(), 1);
    rmb();  // «Comenzar otra vez»
    EXPECT_EQ(tool->step(), 0);
    EXPECT_TRUE(tool->points().empty());
    EXPECT_TRUE(project.panels.empty());
}

TEST_F(PanelToolTest, PerpendicularDirectionFromTheCombo) {
    runner<PanelLoadsDialog>([](PanelLoadsDialog&) { return false; });
    begin(Mode::PanelNew);
    tool->setDirectionMode(PanelDirectionMode::Perpendicular);
    enterSquare();
    rmb();
    click(*view, at(2, 0));
    rmb();
    ASSERT_EQ(project.panels.items.size(), 1u);
    EXPECT_NEAR(std::abs(project.panels.items.front().direction.y), 1.0, 1e-9);
}

TEST_F(PanelToolTest, TwoPointsDoNotMakeAPanel) {
    begin(Mode::PanelNew);
    click(*view, at(0, 0));
    click(*view, at(4, 0));
    rmb();
    ASSERT_EQ(shown.size(), 1u);
    EXPECT_EQ(shown[0].kind, messages::Kind::Error);
    EXPECT_EQ(shown[0].text, QString::fromStdString(panelContourMessage(false, false)));
    EXPECT_EQ(tool->step(), 0);
}

// ---- Editar / Borrar / Dirección ---------------------------------------------------------------------------------------------

TEST_F(PanelToolTest, EditAppliesTheListToEverySelectedPanel) {
    const int a = addPanel(10.0);
    ASSERT_EQ(cc::core::addPanel(project, undo, {{0, 0, 0}, {2, 0, 0}, {2, 4, 0}, {0, 4, 0}}, 0), PanelError::None);
    const int b = project.panels.items.back().id;
    (void)a;
    runner<PanelLoadsDialog>([&](PanelLoadsDialog& d) {
        d.setCellText(false, 0, 2, QStringLiteral("7"));
        return true;
    });
    begin(Mode::PanelEdit);
    click(*view, at(3, 3));   // inside the big panel only
    click(*view, at(1, 2.5, 0));  // inside both: the smallest one (the second)
    ASSERT_EQ(tool->selectedPanels().size(), 2u);
    rmb();
    EXPECT_TRUE(tool->selectedPanels().empty());
    EXPECT_DOUBLE_EQ(findPanel(project.panels, a)->loads[0].value, 7.0);
    ASSERT_EQ(findPanel(project.panels, b)->loads.size(), 1u);
    EXPECT_DOUBLE_EQ(findPanel(project.panels, b)->loads[0].value, 7.0);
}

TEST_F(PanelToolTest, LmbTogglesTheSelection) {
    addPanel();
    begin(Mode::PanelEdit);
    click(*view, at(2, 2));
    EXPECT_EQ(tool->selectedPanels().size(), 1u);
    click(*view, at(2, 2));
    EXPECT_TRUE(tool->selectedPanels().empty());  // measured: every LMB alternates
}

TEST_F(PanelToolTest, DeleteHasNoQuestionAndTakesTheSurfaceLoads) {
    addPanel();
    addSurface();
    begin(Mode::PanelDelete);
    click(*view, at(3, 3));
    rmb();
    EXPECT_TRUE(project.panels.empty());
    EXPECT_TRUE(project.surfaceLoads.items.empty());  // measured: the surface loads of the panel go too
    EXPECT_TRUE(shown.empty());
}

TEST_F(PanelToolTest, DirectionChangesTheArrow) {
    const int id = addPanel();
    begin(Mode::PanelDirection);
    EXPECT_EQ(tool->step(), 0);
    click(*view, at(2, 2));  // the panel: its auxiliary lines appear
    EXPECT_EQ(tool->step(), 1);
    click(*view, at(0, 2));  // side 4-1 (along Y)
    ASSERT_TRUE(tool->selectedSide().has_value());
    rmb();
    EXPECT_NEAR(std::abs(findPanel(project.panels, id)->direction.y), 1.0, 1e-9);
    EXPECT_EQ(tool->step(), 0);
}

// ---- Mover / Dividir ---------------------------------------------------------------------------------------------------------

TEST_F(PanelToolTest, DivideCutsThePanelInTwoWithTheSameDirection) {
    addPanel();
    begin(Mode::PanelDivide);
    click(*view, at(2, 0));  // a point on the bar 1-2
    click(*view, at(2, 4));  // a point on the bar 3-4
    ASSERT_EQ(project.panels.items.size(), 2u);
    for (const Panel& p : project.panels.items) {
        EXPECT_NEAR(p.direction.x, 1.0, 1e-9);
        ASSERT_EQ(p.loads.size(), 1u);  // measured: the loads are copied to both pieces
    }
    EXPECT_TRUE(shown.empty());  // no question
}

TEST_F(PanelToolTest, MovePanelStepsAndInvalidPolygon) {
    const int id = addPanel();
    begin(Mode::PanelMove);
    EXPECT_EQ(tool->step(), 0);
    click(*view, at(2, 2));  // the panel
    EXPECT_EQ(tool->step(), 1);
    click(*view, at(4, 4));  // the vertex 3
    ASSERT_EQ(tool->selectedVertices().size(), 1u);
    rmb();
    ASSERT_EQ(shown.size(), 1u);
    EXPECT_EQ(shown[0].text, QStringLiteral("A continuación, seleccione el punto que se tomará como referencia para el desplazamiento y, posteriormente, seleccione el destino de dicho punto de referencia."));
    EXPECT_EQ(tool->step(), 2);
    click(*view, at(4, 4));  // reference
    EXPECT_EQ(tool->step(), 3);
    click(*view, at(2, 4));  // destination: on the bar 3-4
    EXPECT_EQ(tool->step(), 1);  // back to the vertices (measured)
    // Measured (research 10.4 / 14.8): the destination is only PENDING (green dashed edges); nothing changes until the RMB.
    ASSERT_TRUE(findPanel(project.panels, id));
    EXPECT_NEAR(findPanel(project.panels, id)->vertices[2].x, 4.0, 1e-9);
    rmb();
    const Panel* panel = findPanel(project.panels, id);
    ASSERT_TRUE(panel);
    EXPECT_EQ(shown.size(), 1u);  // no further box
    EXPECT_NEAR(panel->vertices[2].x, 2.0, 1e-6);
    EXPECT_NEAR(panel->vertices[2].y, 4.0, 1e-6);
    undo.undo();
    EXPECT_NEAR(findPanel(project.panels, id)->vertices[2].x, 4.0, 1e-9);
}

TEST_F(PanelToolTest, MoveEndsWithRmbWithoutVertices) {
    addPanel();
    begin(Mode::PanelMove);
    click(*view, at(2, 2));
    ASSERT_EQ(tool->step(), 1);
    rmb();
    EXPECT_EQ(tool->step(), 0);  // «se finaliza la ejecución»
}

// ---- cargas superficiales ----------------------------------------------------------------------------------------------------

TEST_F(PanelToolTest, NewSurfaceLoadAsksForAPanelWhenItOverlapsNone) {
    runner<SurfaceLoadDialog>([](SurfaceLoadDialog& d) {
        EXPECT_TRUE(d.horizontal());
        EXPECT_EQ(d.uniformRadio(), nullptr);  // measured: no radios on a horizontal surface
        EXPECT_TRUE(d.senseLabel()->text().contains(QStringLiteral("Vertical hacia abajo")));
        d.valueEdit()->setText(QStringLiteral("8"));
        return true;
    });
    begin(Mode::SurfNew);
    enterSquare();
    rmb();
    ASSERT_EQ(project.surfaceLoads.items.size(), 1u);
    EXPECT_DOUBLE_EQ(project.surfaceLoads.items[0].data.value, 8.0);
    ASSERT_FALSE(shown.empty());
    EXPECT_EQ(shown.back().kind, messages::Kind::Question);
    EXPECT_EQ(shown.back().text, QStringLiteral("¿Desea crear un paño asociado a la nueva carga superficial?"));
    EXPECT_EQ(project.panels.items.size(), 1u);  // Sí
}

TEST_F(PanelToolTest, NewSurfaceLoadAnsweringNoCreatesNoPanel) {
    answer = false;
    runner<SurfaceLoadDialog>([](SurfaceLoadDialog& d) {
        d.valueEdit()->setText(QStringLiteral("8"));
        return true;
    });
    begin(Mode::SurfNew);
    enterSquare();
    rmb();
    EXPECT_EQ(project.surfaceLoads.items.size(), 1u);
    EXPECT_TRUE(project.panels.empty());
}

TEST_F(PanelToolTest, NewSurfaceLoadOverAPanelDoesNotAsk) {
    addPanel();
    runner<SurfaceLoadDialog>([](SurfaceLoadDialog& d) {
        d.valueEdit()->setText(QStringLiteral("8"));
        return true;
    });
    begin(Mode::SurfNew);
    enterSquare();
    rmb();
    EXPECT_EQ(project.surfaceLoads.items.size(), 1u);
    for (const Shown& s : shown) EXPECT_NE(s.kind, messages::Kind::Question);
}

TEST_F(PanelToolTest, SurfaceDialogKeepsOpenOnNullValue) {
    runner<SurfaceLoadDialog>([](SurfaceLoadDialog& d) {
        d.valueEdit()->setText(QStringLiteral("0"));  // accept() refuses: the window stays open
        return true;
    });
    begin(Mode::SurfNew);
    enterSquare();
    rmb();
    EXPECT_TRUE(project.surfaceLoads.items.empty());
    ASSERT_FALSE(shown.empty());
    EXPECT_EQ(shown[0].kind, messages::Kind::Error);
    EXPECT_EQ(shown[0].text, QStringLiteral("No puede introducir cargas de valor nulo."));
}

TEST_F(PanelToolTest, EditTitleIsCargasSuperficiales) {
    addSurface();
    QString title;
    runner<SurfaceLoadDialog>([&](SurfaceLoadDialog& d) {
        title = d.windowTitle();
        d.valueEdit()->setText(QStringLiteral("3"));
        return true;
    });
    begin(Mode::SurfEdit);
    click(*view, at(1, 1));
    ASSERT_EQ(tool->selectedLoads().size(), 1u);
    rmb();
    EXPECT_EQ(title, QStringLiteral("Cargas superficiales"));
    EXPECT_DOUBLE_EQ(project.surfaceLoads.items[0].data.value, 3.0);
}

TEST_F(PanelToolTest, CopyOpensTheDialogWithOneClick) {
    addSurface();
    QString title;
    double shownValue = 0.0;
    runner<SurfaceLoadDialog>([&](SurfaceLoadDialog& d) {
        title = d.windowTitle();
        shownValue = d.data().value;
        return true;
    });
    begin(Mode::SurfCopy);
    click(*view, at(1, 1));  // no RMB (measured)
    EXPECT_EQ(title, QStringLiteral("Introducir cargas superficiales"));
    EXPECT_DOUBLE_EQ(shownValue, 8.0);
    EXPECT_EQ(project.surfaceLoads.items.size(), 2u);  // two equal loads are allowed
}

TEST_F(PanelToolTest, DeleteRemovesOneOfTwoStackedLoads) {
    addSurface();
    addSurface();
    begin(Mode::SurfDelete);
    click(*view, at(1, 1));
    rmb();
    EXPECT_EQ(project.surfaceLoads.items.size(), 1u);
    EXPECT_TRUE(shown.empty());
}

TEST_F(PanelToolTest, CaseButtonMovesTheLoad) {
    NewLoadCase c;
    c.nature = LoadNature::DeadLoad;
    c.reference = "CM 1";
    int cm = 0;
    ASSERT_EQ(createLoadCase(project, undo, c, &cm), LoadCaseError::None);
    addSurface();
    runner<cc::app::LoadCaseChoiceDialog>([&](cc::app::LoadCaseChoiceDialog& d) {
        d.combo()->setCurrentIndex(d.combo()->findData(cm));
        return true;
    });
    begin(Mode::SurfCase);
    click(*view, at(1, 1));
    rmb();
    EXPECT_EQ(project.surfaceLoads.items[0].data.loadCaseId, cm);
    EXPECT_DOUBLE_EQ(project.surfaceLoads.items[0].data.value, 8.0);  // the value is kept
}

TEST_F(PanelToolTest, MoveSurfaceShowsTheBoxAtOnce) {
    addSurface();
    int boxes = 0;
    bool defaultChecked = false;
    tool->setDialogRunner([&](QDialog& dialog) {
        if (auto* box = dynamic_cast<cc::app::MoveSurfaceLoadsDialog*>(&dialog)) {
            ++boxes;
            defaultChecked = box->check()->isChecked();
            EXPECT_EQ(box->windowTitle(), QStringLiteral("Mover cargas superficiales"));
        }
        return static_cast<int>(QDialog::Accepted);
    });
    begin(Mode::SurfMove);
    EXPECT_EQ(boxes, 1);  // on activation
    EXPECT_TRUE(defaultChecked);  // measured: checked by default
    rmb();                        // no selection: the box again
    EXPECT_EQ(boxes, 2);
}

TEST_F(PanelToolTest, AssignTogglesTheBarsWithoutDialog) {
    bool dialog = false;
    tool->setDialogRunner([&](QDialog&) {
        dialog = true;
        return static_cast<int>(QDialog::Accepted);
    });
    begin(Mode::SurfAssign);
    EXPECT_EQ(status, PanelTool::statusText(Mode::SurfAssign));
    mouse(*view, QEvent::MouseMove, at(2, 0), Qt::NoButton, Qt::NoButton);
    click(*view, at(2, 0));  // bar 1-2
    ASSERT_EQ(view->selection().ids(ElementKind::Bar).size(), 1u);
    rmb();
    EXPECT_EQ(project.surfaceLoads.unassignedBars.size(), 1u);  // each RMB toggles (measured)
    click(*view, at(2, 0));
    rmb();
    EXPECT_TRUE(project.surfaceLoads.unassignedBars.empty());
    EXPECT_FALSE(dialog);
}

// ---- hypothesis deletion and picking -----------------------------------------------------------------------------------------

TEST_F(PanelToolTest, DeletingAHypothesisThatOnlyHasPanelOrSurfaceRowsNeedsTheQuestion) {
    NewLoadCase c;
    c.nature = LoadNature::DeadLoad;
    c.reference = "CM 1";
    int cm = 0;
    ASSERT_EQ(createLoadCase(project, undo, c, &cm), LoadCaseError::None);
    EXPECT_TRUE(loadCaseDependents(project, cm).empty());
    ASSERT_EQ(cc::core::addPanel(project, undo, {{0, 0, 0}, {4, 0, 0}, {4, 4, 0}, {0, 4, 0}}, 0), PanelError::None);
    SurfaceLoadData row = surfaceLoadDefaults(cm);
    row.value = 3.0;
    ASSERT_EQ(setPanelLoads(project, undo, {project.panels.items.front().id}, {row}), PanelError::None);
    EXPECT_FALSE(loadCaseDependents(project, cm).empty());  // panel rows only
    ASSERT_EQ(setPanelLoads(project, undo, {project.panels.items.front().id}, {}), PanelError::None);
    EXPECT_TRUE(loadCaseDependents(project, cm).empty());
    ASSERT_EQ(addSurfaceLoad(project, undo, {{0, 0, 0}, {2, 0, 0}, {2, 2, 0}, {0, 2, 0}}, row), PanelError::None);
    EXPECT_FALSE(loadCaseDependents(project, cm).empty());  // surface load only
}

TEST_F(PanelToolTest, HidesOrGreysByModeAndLeavesTheViewportClean) {
    addPanel();
    begin(Mode::SurfNew);
    EXPECT_TRUE(view->snapMode());
    tool->deactivate();
    EXPECT_FALSE(view->snapMode());
    EXPECT_FALSE(view->pickMode());
    begin(Mode::PanelEdit);
    EXPECT_TRUE(view->pickMode());
    EXPECT_FALSE(view->selectionMode());
}

TEST_F(PanelToolTest, PanelDirectionPanelHasTheMeasuredCombo) {
    cc::app::PanelDirectionPanel panel;
    EXPECT_EQ(panel.windowTitle(), QStringLiteral("Propiedades"));
    ASSERT_EQ(panel.combo()->count(), 2);
    EXPECT_EQ(panel.combo()->itemText(0), QStringLiteral("Paralela al lado seleccionado"));
    EXPECT_EQ(panel.combo()->itemText(1), QStringLiteral("Perpendicular al lado seleccionado"));
    EXPECT_EQ(panel.mode(), PanelDirectionMode::Parallel);
}

// ---- dialogs -----------------------------------------------------------------------------------------------------------------

TEST_F(PanelToolTest, PanelLoadsDialogTabsAndValidation) {
    const std::vector<Point3> inclined{{0, 0, 0}, {4, 0, 0}, {4, 4, 2}, {0, 4, 2}};
    PanelLoadsDialog dialog(project.loadCases, inclined, {}, 0);
    ASSERT_TRUE(dialog.tabbed());
    EXPECT_EQ(dialog.tabs()->count(), 2);
    EXPECT_EQ(dialog.tabs()->tabText(0), QStringLiteral("Uniformes"));
    EXPECT_EQ(dialog.tabs()->tabText(1), QStringLiteral("Variables con la altura"));
    ASSERT_EQ(dialog.variableTable()->columnCount(), 4);
    EXPECT_EQ(dialog.variableTable()->horizontalHeaderItem(2)->text(), QStringLiteral("Valor en la cota inferior"));  // the lower one first (measured)
    EXPECT_EQ(dialog.variableTable()->horizontalHeaderItem(3)->text(), QStringLiteral("Valor en la cota superior"));
    EXPECT_EQ(dialog.uniformTable()->horizontalHeaderItem(1)->text(), QStringLiteral("Sentido positivo"));
    // a row at 0.000 refuses Aceptar (also from the second tab)
    dialog.tabs()->setCurrentIndex(1);
    dialog.addRow();
    dialog.setCellText(true, 0, 3, QStringLiteral("15"));
    dialog.addRow();  // second row left at 0 / 0
    dialog.accept();
    ASSERT_FALSE(shown.empty());
    EXPECT_EQ(shown.back().kind, messages::Kind::Error);
    EXPECT_EQ(shown.back().text, QStringLiteral("No puede introducir cargas de valor nulo."));
    EXPECT_NE(dialog.result(), QDialog::Accepted);
}

TEST_F(PanelToolTest, PanelLoadsDialogRangeAndMultiSelection) {
    PanelLoadsDialog dialog(project.loadCases, {{0, 0, 0}, {4, 0, 0}, {4, 4, 0}, {0, 4, 0}}, {}, 0);
    EXPECT_FALSE(dialog.tabbed());
    dialog.addRow();
    dialog.addRow();  // the new row joins the selection (measured): a typed value goes to both
    dialog.setCellText(false, 1, 2, QStringLiteral("-5"));
    ASSERT_EQ(dialog.rows().size(), 2u);
    EXPECT_DOUBLE_EQ(dialog.rows()[0].value, -5.0);
    EXPECT_DOUBLE_EQ(dialog.rows()[1].value, -5.0);
    dialog.setCellText(false, 1, 2, QStringLiteral("20000"));
    ASSERT_FALSE(shown.empty());
    EXPECT_EQ(shown.back().kind, messages::Kind::Warning);
    EXPECT_EQ(shown.back().text, QStringLiteral("El valor de 'Valor' debe estar entre -10000.000 kN/m² y 10000.000 kN/m²."));
    EXPECT_DOUBLE_EQ(dialog.rows()[1].value, -5.0);  // the old value stays
    dialog.removeRows();  // both selected: Borrar removes both
    EXPECT_TRUE(dialog.rows().empty());
}

TEST_F(PanelToolTest, PanelLoadsDialogSenseFollowsTheHypothesis) {
    NewLoadCase w;
    w.nature = LoadNature::Wind;
    w.reference = "V 1";
    int wind = 0;
    ASSERT_EQ(createLoadCase(project, undo, w, &wind), LoadCaseError::None);
    PanelLoadsDialog dialog(project.loadCases, {{0, 0, 0}, {4, 0, 0}, {4, 4, 0}, {0, 4, 0}}, {}, 0);
    dialog.addRow();
    EXPECT_EQ(dialog.uniformTable()->item(0, 1)->text(), QStringLiteral("Vertical hacia abajo"));
    dialog.setRowCase(false, 0, wind);
    EXPECT_EQ(dialog.uniformTable()->item(0, 1)->text(), QStringLiteral("Vertical hacia arriba"));
}

TEST_F(PanelToolTest, SurfaceDialogInclinedHasRadiosAndUpperFirst) {
    const std::vector<Point3> inclined{{0, 0, 0}, {4, 0, 0}, {4, 4, 2}, {0, 4, 2}};
    SurfaceLoadDialog dialog(QStringLiteral("t"), project.loadCases, inclined, surfaceLoadDefaults(0));
    ASSERT_NE(dialog.uniformRadio(), nullptr);
    EXPECT_TRUE(dialog.uniformRadio()->isChecked());
    EXPECT_EQ(dialog.variableRadio()->text(), QStringLiteral("Variable con la altura"));
    dialog.variableRadio()->setChecked(true);
    dialog.upperEdit()->setText(QStringLiteral("6"));
    dialog.lowerEdit()->setText(QStringLiteral("-4"));  // mixed signs are valid
    dialog.accept();
    EXPECT_EQ(dialog.result(), QDialog::Accepted);
    EXPECT_EQ(dialog.data().type, SurfaceLoadType::Variable);
    EXPECT_DOUBLE_EQ(dialog.data().upper, 6.0);
    EXPECT_DOUBLE_EQ(dialog.data().lower, -4.0);
}

TEST(PanelPicking, BorderBeatsInteriorAndSmallestWins) {
    const QPolygonF big({{0, 0}, {100, 0}, {100, 100}, {0, 100}});
    const QPolygonF small({{0, 0}, {40, 0}, {40, 40}, {0, 40}});
    const std::vector<std::pair<int, QPolygonF>> polys{{1, big}, {2, small}};
    EXPECT_EQ(picking::polygonsAt(polys, QPointF(20, 20)), (std::vector<int>{2}));    // inside both: the smallest
    EXPECT_EQ(picking::polygonsAt(polys, QPointF(70, 70)), (std::vector<int>{1}));
    EXPECT_EQ(picking::polygonsAt(polys, QPointF(0, 20)), (std::vector<int>{1, 2}));  // common border: both (measured)
    EXPECT_TRUE(picking::polygonsAt(polys, QPointF(300, 300)).empty());
    EXPECT_EQ(picking::vertexAt(big, QPointF(97, 3)), std::optional<int>(1));
    EXPECT_FALSE(picking::vertexAt(big, QPointF(50, 50)).has_value());
    EXPECT_EQ(picking::sideLineAt(big, QPointF(500, 2)), std::optional<int>(0));  // the PROLONGED line of the first side
}

int main(int argc, char** argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    app();
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

// ---- help mode and tab order of «Cargas en paño» (research 14.4 / 14.5) ----------------------------------------------------------

TEST_F(PanelToolTest, PanelLoadsDialogHelpModeFramesAndExplains) {
    const std::vector<Point3> inclined{{0, 0, 0}, {4, 0, 0}, {4, 4, 2}, {0, 4, 2}};
    PanelLoadsDialog dialog(project.loadCases, inclined, {}, 0);
    dialog.show();
    QApplication::processEvents();
    ASSERT_NE(dialog.helpButton(), nullptr);
    EXPECT_FALSE(dialog.helpMode());
    EXPECT_EQ(dialog.helpClick(QPoint(50, 50)), -1);  // nothing happens outside the help mode
    dialog.helpButton()->click();
    EXPECT_TRUE(dialog.helpMode());
    // The header «Sentido positivo» explains itself (#65051).
    QHeaderView* header = dialog.uniformTable()->horizontalHeader();
    const QPoint inHeader = header->mapTo(&dialog, QPoint(header->sectionViewportPosition(1) + 5, header->height() / 2));
    const int index = dialog.helpClick(inHeader);
    EXPECT_GE(index, 0);
    EXPECT_EQ(dialog.helpSelected(), index);
    EXPECT_EQ(dialog.helpBalloonText(), QStringLiteral("Sentido positivo de la carga"));
    // The buttons of the list have help too; a click on the plain background clears the selection.
    const QPoint onAdd = dialog.addButton()->mapTo(&dialog, QPoint(4, 4));
    EXPECT_GE(dialog.helpClick(onAdd), 0);
    EXPECT_NE(dialog.helpClick(onAdd), index);
    EXPECT_EQ(dialog.helpClick(QPoint(2, 2)), -1);
    EXPECT_TRUE(dialog.helpBalloonText().isEmpty());
    dialog.helpButton()->click();
    EXPECT_FALSE(dialog.helpMode());
}

TEST_F(PanelToolTest, PanelLoadsDialogTabOrderIsCellsThenButtons) {
    PanelLoadsDialog dialog(project.loadCases, {{0, 0, 0}, {4, 0, 0}, {4, 4, 0}, {0, 4, 0}}, {}, 0);
    dialog.show();
    QApplication::processEvents();
    dialog.addRow();
    dialog.addRow();
    QTableWidget* table = dialog.uniformTable();
    ASSERT_EQ(table->rowCount(), 2);
    // The buttons of the list are outside the sequence.
    EXPECT_EQ(dialog.addButton()->focusPolicy(), Qt::NoFocus);
    EXPECT_EQ(dialog.copyButton()->focusPolicy(), Qt::NoFocus);
    EXPECT_EQ(dialog.removeButton()->focusPolicy(), Qt::NoFocus);
    table->cellWidget(0, 0)->setFocus();
    const auto position = [&] {
        QWidget* f = dialog.focusWidget();
        if (f == dialog.acceptButton()) return std::pair{-1, 1};
        if (f == dialog.cancelButton()) return std::pair{-1, 2};
        for (int r = 0; r < table->rowCount(); ++r)
            if (f == table->cellWidget(r, 0) || table->cellWidget(r, 0)->isAncestorOf(f)) return std::pair{r, 0};
        return std::pair{table->currentRow(), table->currentColumn()};
    };
    EXPECT_EQ(position(), (std::pair{0, 0}));  // Hipótesis
    const std::vector<std::pair<int, int>> expected{{0, 1}, {0, 2}, {1, 0}, {1, 1}, {1, 2}, {-1, 1}, {-1, 2}, {0, 0}};
    for (const auto& step : expected) {
        EXPECT_TRUE(dialog.focusNextPrevChild(true));
        EXPECT_EQ(position(), step);
    }
    EXPECT_TRUE(dialog.focusNextPrevChild(false));  // Shift+Tab from the first cell goes to Cancelar
    EXPECT_EQ(position(), (std::pair{-1, 2}));
}

TEST_F(PanelToolTest, SurfaceDialogHorizontalFocusStartsOnTheCaseComboAndTabCycles) {
    SurfaceLoadDialog dialog(QStringLiteral("t"), project.loadCases, {{0, 0, 0}, {4, 0, 0}, {4, 4, 0}, {0, 4, 0}}, surfaceLoadDefaults(0));
    dialog.show();
    QApplication::processEvents();
    EXPECT_EQ(dialog.focusWidget(), dialog.caseCombo());
    const std::vector<QWidget*> expected{dialog.valueEdit(), dialog.acceptButton(), dialog.cancelButton(), dialog.caseCombo()};
    for (QWidget* step : expected) {
        QKeyEvent tab(QEvent::KeyPress, Qt::Key_Tab, Qt::NoModifier);
        QApplication::sendEvent(dialog.focusWidget(), &tab);
        EXPECT_EQ(dialog.focusWidget(), step) << step->metaObject()->className() << ' ' << dialog.focusWidget()->metaObject()->className() << ' ' << dialog.focusWidget()->objectName().toStdString();
    }
    EXPECT_EQ(dialog.senseLabel()->focusPolicy(), Qt::NoFocus);
}
