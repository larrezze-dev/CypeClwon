// Propiedades > Barras > Grupos de flecha (T-P6-06, GUI part): dialogs, tool and the viewport state. Expected values: docs/research/ui/grupos_flecha.md
// (REA + measured in the original). Never shown on screen (offscreen).
#include <gtest/gtest.h>

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QMenu>
#include <QMouseEvent>
#include <QRadioButton>
#include <memory>

#include "DeflectionTool.h"
#include "RangedEdit.h"
#include "UserMessages.h"
#include "Viewport3D.h"
#include "core/deflection_group.h"
#include "core/deflection_group_edit.h"

using namespace cc::core;
using cc::app::DeflectionGroupLimitDialog;
using cc::app::DeflectionTool;
using cc::app::DeflectionTypeDialog;
using cc::app::RangedEdit;
using cc::app::Viewport3D;
namespace messages = cc::app::messages;

namespace {

QApplication* app() {
    static int argc = 1;
    static char name[] = "deflection_tool_tests";
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

/// Nodes N1 (0,0,0) .. N3 (8,0,0) and N4 (8,0,4); bars B1 = N1-N2 and B2 = N2-N3 (aligned), B3 = N3-N4 (not aligned).
class DeflectionToolTest : public ::testing::Test {
protected:
    void SetUp() override {
        app();
        for (int i = 0; i < 3; ++i) project.structure.addNode({4.0 * i, 0, 0});
        project.structure.addNode({8, 0, 4});
        project.structure.addBar(1, 2);
        project.structure.addBar(2, 3);
        project.structure.addBar(3, 4);
        view = std::make_unique<Viewport3D>();
        view->resize(800, 600);
        view->setStandardView(cc::core::StandardView::Front);
        view->setOrthographic(true);
        view->camera().target = {4, 0, 2};
        view->setStructure(&project.structure);
        tool = std::make_unique<DeflectionTool>();
        tool->setProject(&project, &undo);
        tool->addViewport(view.get());
        undo.setChangeCallback([this] { tool->structureChanged(); });
        QObject::connect(tool.get(), &DeflectionTool::statusChanged, [this](const QString& text) { status = text; });
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
    /// Crossing window over bar `id` (its two nodes).
    void selectBar(int id) {
        const Bar& bar = *project.structure.findBar(id);
        const QPointF a = screenOf(bar.startNode), b = screenOf(bar.endNode);
        click(*view, QPointF(std::min(a.x(), b.x()) - 10, std::min(a.y(), b.y()) - 10));
        click(*view, QPointF(std::max(a.x(), b.x()) + 10, std::max(a.y(), b.y()) + 10));
    }
    /// Runs the dialogs of the tool through `fill`, then accepts (true) or rejects.
    template <typename Dialog, typename Fill>
    void answer(bool accept, Fill fill) {
        tool->setDialogRunner([=](QDialog& dialog) {
            fill(static_cast<Dialog&>(dialog));
            if (accept) dialog.accept();
            return accept ? static_cast<int>(QDialog::Accepted) : static_cast<int>(QDialog::Rejected);
        });
    }
    Project project;
    UndoStack undo;
    std::unique_ptr<Viewport3D> view;
    std::unique_ptr<DeflectionTool> tool;
    QString status;
    QStringList warnings, notices;
};

}  // namespace

TEST_F(DeflectionToolTest, MenuEntriesStatusAndViewportState) {
    EXPECT_EQ(tool->menu()->title(), QStringLiteral("Grupos de flecha"));
    EXPECT_EQ(tool->createAction()->text(), QStringLiteral("Crear"));
    EXPECT_EQ(tool->editAction()->text(), QStringLiteral("Editar"));
    EXPECT_EQ(tool->limitAction()->text(), QStringLiteral("Flecha Límite"));
    EXPECT_EQ(tool->editAction()->toolTip(), QStringLiteral("Permite editar un grupo de flecha existente para cambiar el tipo de cálculo (tangente o secante)."));
    EXPECT_TRUE(tool->createAction()->isEnabled());
    DeflectionTool other;
    EXPECT_FALSE(other.createAction()->isEnabled()) << "no project";
    EXPECT_EQ(DeflectionTool::statusText(DeflectionTool::Mode::None), QStringLiteral("Seleccione una opción del menú."));

    tool->createAction()->trigger();
    EXPECT_EQ(tool->mode(), DeflectionTool::Mode::Create);
    EXPECT_EQ(status, QStringLiteral("Grupos de flecha - Crear |  - LMB: Seleccione un elemento."));
    EXPECT_TRUE(view->selectionMode());
    EXPECT_TRUE(view->deflectionLabelsVisible());
    EXPECT_TRUE(view->barAxesVisible());
    EXPECT_TRUE(view->selectionFilter().nodes);
    EXPECT_FALSE(view->selectionFilter().bars);
    tool->editAction()->trigger();
    EXPECT_EQ(tool->mode(), DeflectionTool::Mode::Edit);
    EXPECT_EQ(status, QStringLiteral("Grupos de flecha - Editar |  - LMB: Seleccione un grupo de elementos. - RMB: Actuar sobre los elementos seleccionados."));
    EXPECT_TRUE(view->selectionFilter().bars);
    EXPECT_FALSE(view->selectionFilter().nodes);
    tool->limitAction()->trigger();
    EXPECT_EQ(tool->mode(), DeflectionTool::Mode::Limit);
    EXPECT_EQ(status, QStringLiteral("Grupos de flecha - Flecha Límite |  - LMB: Seleccione un grupo de elementos. - RMB: Actuar sobre los elementos seleccionados."));
    tool->deactivate();
    EXPECT_EQ(tool->mode(), DeflectionTool::Mode::None);
    EXPECT_FALSE(view->deflectionLabelsVisible());
    EXPECT_FALSE(view->barAxesVisible());
    EXPECT_FALSE(view->selectionMode());
    EXPECT_EQ(status, QStringLiteral("Seleccione una opción del menú."));
}

TEST_F(DeflectionToolTest, TypeDialogTextsAndDefaults) {
    DeflectionTypeDialog dialog;
    EXPECT_EQ(dialog.windowTitle(), QStringLiteral("Grupo de flecha"));
    EXPECT_EQ(dialog.width(), 206);
    for (int plane = 0; plane < 2; ++plane) {
        EXPECT_EQ(dialog.type(plane), DeflectionType::Secant) << "default: secante";
        EXPECT_TRUE(dialog.radio(plane, DeflectionType::Secant)->isChecked());
        EXPECT_EQ(dialog.radio(plane, DeflectionType::Secant)->text(), QStringLiteral("Secante"));
        EXPECT_EQ(dialog.radio(plane, DeflectionType::TangentToNode1)->text(), QStringLiteral("Tangente a nudo 1"));
        EXPECT_EQ(dialog.radio(plane, DeflectionType::TangentToNode2)->text(), QStringLiteral("Tangente a nudo 2"));
        EXPECT_TRUE(dialog.radio(plane, DeflectionType::Secant)->font().bold()) << "the chosen radio is bold";
        EXPECT_FALSE(dialog.radio(plane, DeflectionType::TangentToNode1)->font().bold());
    }
    dialog.setType(0, DeflectionType::TangentToNode1);
    dialog.setType(1, DeflectionType::TangentToNode2);
    EXPECT_EQ(dialog.type(0), DeflectionType::TangentToNode1);
    EXPECT_EQ(dialog.type(1), DeflectionType::TangentToNode2);
    EXPECT_FALSE(dialog.radio(0, DeflectionType::Secant)->isChecked()) << "the planes are independent groups";
    EXPECT_TRUE(dialog.radio(0, DeflectionType::TangentToNode1)->font().bold());
    EXPECT_FALSE(dialog.radio(0, DeflectionType::Secant)->font().bold());
    EXPECT_EQ(dialog.acceptButton()->text(), QStringLiteral("Aceptar"));
    EXPECT_EQ(dialog.cancelButton()->text(), QStringLiteral("Cancelar"));
}

TEST_F(DeflectionToolTest, LimitDialogTextsAndDefaults) {
    DeflectionGroupLimitDialog dialog;
    EXPECT_EQ(dialog.windowTitle(), QStringLiteral("Flecha Límite"));
    EXPECT_EQ(dialog.width(), 390);
    const char* names[8] = {"Flecha máxima absoluta xy", "Flecha máxima absoluta xz", "Flecha activa absoluta xy", "Flecha activa absoluta xz",
                            "Flecha máxima relativa xy", "Flecha máxima relativa xz", "Flecha activa relativa xy", "Flecha activa relativa xz"};
    for (int s = 0; s < 8; ++s) {
        const auto slot = static_cast<LimitSlot>(s);
        EXPECT_EQ(dialog.check(slot)->text(), QString::fromUtf8(names[s]));
        EXPECT_FALSE(dialog.check(slot)->isChecked()) << "all unticked by default (gf_30_limit0)";
    }
    EXPECT_EQ(dialog.limits(), DeflectionLimits{});
    // absolute block: the field in mm appears when ticked (default 1.0)
    const LimitSlot abs = LimitSlot::ActiveAbsXZ;
    EXPECT_EQ(dialog.edit(abs)->text(), QStringLiteral("1.0"));
    EXPECT_TRUE(dialog.edit(abs)->parentWidget()->isHidden());
    dialog.check(abs)->setChecked(true);
    EXPECT_FALSE(dialog.edit(abs)->parentWidget()->isHidden());
    // relative block: radios L / 250 (default) .. L / ?
    const LimitSlot rel = LimitSlot::MaxRelXY;
    const char* radios[5] = {"L / 250", "L / 300", "L / 400", "L / 500", "L / ?"};
    for (int k = 0; k < 5; ++k) EXPECT_EQ(dialog.radio(rel, k)->text(), QString::fromUtf8(radios[k]));
    EXPECT_TRUE(dialog.radio(rel, 0)->isChecked());
    EXPECT_TRUE(dialog.radio(rel, 0)->font().bold());
    EXPECT_TRUE(dialog.radio(rel, 0)->parentWidget()->isHidden());
    dialog.check(rel)->setChecked(true);
    EXPECT_FALSE(dialog.radio(rel, 0)->parentWidget()->isHidden());
    EXPECT_TRUE(dialog.edit(rel)->isHidden()) << "the field of L / ? only with that radio";
    dialog.radio(rel, 2)->setChecked(true);
    DeflectionLimits got = dialog.limits();
    EXPECT_TRUE(got.item(rel).enabled);
    EXPECT_EQ(got.item(rel).relativeDivisor, 400.0);
    EXPECT_TRUE(got.item(abs).enabled);
    EXPECT_EQ(got.item(abs).absoluteMm, 1.0);
    dialog.radio(rel, 4)->setChecked(true);
    EXPECT_FALSE(dialog.edit(rel)->isHidden());
    dialog.edit(rel)->setValue(350.0);
    EXPECT_EQ(dialog.limits().item(rel).relativeDivisor, 350.0);
    // loading a divisor that is not a preset selects L / ?
    DeflectionLimits in;
    in.item(LimitSlot::ActiveRelXZ) = {true, 1.0, 600.0};
    in.item(LimitSlot::MaxAbsXY) = {true, 12.5, 250.0};
    dialog.setLimits(in);
    EXPECT_TRUE(dialog.radio(LimitSlot::ActiveRelXZ, 4)->isChecked());
    EXPECT_EQ(dialog.limits().item(LimitSlot::ActiveRelXZ).relativeDivisor, 600.0);
    EXPECT_EQ(dialog.limits().item(LimitSlot::MaxAbsXY).absoluteMm, 12.5);
    EXPECT_FALSE(dialog.check(rel)->isChecked());
}

TEST_F(DeflectionToolTest, LimitDialogRefusesInvalidValues) {
    DeflectionGroupLimitDialog dialog;
    dialog.check(LimitSlot::MaxAbsXY)->setChecked(true);
    dialog.edit(LimitSlot::MaxAbsXY)->setValue(0.0);  // below the minimum 0.00001
    dialog.accept();
    EXPECT_EQ(dialog.result(), QDialog::Rejected) << "the dialog stays open";
    EXPECT_EQ(notices.size(), 1);
    // a value typed outside the range is refused by the field itself
    dialog.edit(LimitSlot::MaxAbsXY)->setValue(2.0);
    dialog.edit(LimitSlot::MaxAbsXY)->setText(QStringLiteral("0"));
    EXPECT_FALSE(dialog.edit(LimitSlot::MaxAbsXY)->commit());
    EXPECT_EQ(warnings.size(), 1);
    EXPECT_EQ(dialog.edit(LimitSlot::MaxAbsXY)->text(), QStringLiteral("2.0")) << "the field recovers its previous value";
    // a disabled invalid block is not checked
    dialog.edit(LimitSlot::MaxAbsXY)->setValue(0.0);
    dialog.check(LimitSlot::MaxAbsXY)->setChecked(false);
    notices.clear();
    dialog.accept();
    EXPECT_TRUE(notices.isEmpty());
    EXPECT_EQ(dialog.result(), QDialog::Accepted);
}

TEST_F(DeflectionToolTest, CreateByClickingTwoNodes) {
    answer<DeflectionTypeDialog>(true, [](DeflectionTypeDialog& d) {
        d.setType(0, DeflectionType::TangentToNode1);
        d.setType(1, DeflectionType::Secant);
    });
    tool->createAction()->trigger();
    click(*view, screenOf(1));
    ASSERT_TRUE(tool->pendingNode().has_value());
    EXPECT_EQ(*tool->pendingNode(), 1);
    EXPECT_EQ(view->selection().ids(ElementKind::Node), std::vector<int>({1})) << "the first node stays selected (orange)";
    EXPECT_EQ(undo.count(), 0u);
    click(*view, screenOf(3));
    EXPECT_FALSE(tool->pendingNode().has_value());
    EXPECT_TRUE(view->selection().ids(ElementKind::Node).empty());
    ASSERT_EQ(project.deflection.groups.size(), 1u);
    const DeflectionBarGroup& group = project.deflection.groups.front();
    EXPECT_EQ(group.bars, std::vector<int>({1, 2}));
    EXPECT_EQ(group.xy, DeflectionType::TangentToNode1);
    EXPECT_EQ(group.xz, DeflectionType::Secant);
    EXPECT_EQ(group.node1, 1) << "node 1 is the first node chosen";
    EXPECT_EQ(undo.count(), 1u) << "one undoable step";
    EXPECT_EQ(tool->mode(), DeflectionTool::Mode::Create) << "the command stays active";
    EXPECT_EQ(status, QStringLiteral("Grupos de flecha - Crear |  - LMB: Seleccione un elemento."));
    undo.undo();
    EXPECT_TRUE(project.deflection.empty());
    undo.redo();
    EXPECT_EQ(project.deflection.groups.size(), 1u);
}

TEST_F(DeflectionToolTest, CreateCancelledDoesNothing) {
    answer<DeflectionTypeDialog>(false, [](DeflectionTypeDialog&) {});
    EXPECT_FALSE(tool->createBetween(1, 3));
    EXPECT_TRUE(project.deflection.empty());
    EXPECT_EQ(undo.count(), 0u);
}

TEST_F(DeflectionToolTest, CreateShowsTheAvisoBeforeTheDialog) {
    bool dialogShown = false;
    tool->setDialogRunner([&](QDialog&) {
        dialogShown = true;
        return static_cast<int>(QDialog::Accepted);
    });
    // N1 -> N4: no chain of aligned bars (#64227)
    EXPECT_FALSE(tool->createBetween(1, 4));
    ASSERT_EQ(notices.size(), 1);
    EXPECT_EQ(notices.back(), QStringLiteral("No se ha encontrado un grupo de barras alineadas que conecte los dos nudos seleccionados."));
    EXPECT_FALSE(dialogShown);
    // the same group twice (#64228)
    answer<DeflectionTypeDialog>(true, [](DeflectionTypeDialog&) {});
    EXPECT_TRUE(tool->createBetween(1, 3));
    notices.clear();
    tool->setDialogRunner([&](QDialog&) {
        dialogShown = true;
        return static_cast<int>(QDialog::Accepted);
    });
    EXPECT_FALSE(tool->createBetween(1, 3));
    ASSERT_EQ(notices.size(), 1);
    EXPECT_EQ(notices.back(), QStringLiteral("Ya existe un grupo de flecha equivalente."));
    EXPECT_FALSE(dialogShown);
    EXPECT_EQ(undo.count(), 1u);
}

TEST_F(DeflectionToolTest, EditTypesOfTheSelectedGroup) {
    answer<DeflectionTypeDialog>(true, [](DeflectionTypeDialog& d) { d.setType(1, DeflectionType::TangentToNode2); });
    ASSERT_TRUE(tool->createBetween(1, 3));
    const std::size_t created = undo.count();
    tool->editAction()->trigger();
    selectBar(1);  // any bar of the group
    ASSERT_EQ(view->selection().ids(ElementKind::Bar), std::vector<int>({1}));
    DeflectionType shownXY = DeflectionType::TangentToNode1, shownXZ = DeflectionType::Secant;
    answer<DeflectionTypeDialog>(true, [&](DeflectionTypeDialog& d) {
        shownXY = d.type(0);
        shownXZ = d.type(1);
        d.setType(0, DeflectionType::TangentToNode1);
    });
    click(*view, QPointF(5, 5), Qt::RightButton);  // RMB acts
    EXPECT_EQ(shownXY, DeflectionType::Secant) << "the dialog opens with the types of the group";
    EXPECT_EQ(shownXZ, DeflectionType::TangentToNode2);
    ASSERT_EQ(project.deflection.groups.size(), 1u);
    EXPECT_EQ(project.deflection.groups.front().xy, DeflectionType::TangentToNode1);
    EXPECT_EQ(project.deflection.groups.front().xz, DeflectionType::TangentToNode2);
    EXPECT_EQ(undo.count(), created + 1) << "one undoable step";
    EXPECT_TRUE(view->selection().items().empty()) << "the selection is cleared after acting";
    EXPECT_EQ(status, QStringLiteral("Grupos de flecha - Editar |  - LMB: Seleccione un grupo de elementos. - RMB: Actuar sobre los elementos seleccionados."));
    undo.undo();
    EXPECT_EQ(project.deflection.groups.front().xy, DeflectionType::Secant);
}

TEST_F(DeflectionToolTest, EditWithoutChangesPushesNothing) {
    answer<DeflectionTypeDialog>(true, [](DeflectionTypeDialog&) {});
    EXPECT_FALSE(tool->editTypes({1}));
    EXPECT_EQ(undo.count(), 0u);
    EXPECT_FALSE(tool->editTypes({99})) << "unknown bar";
}

TEST_F(DeflectionToolTest, LimitsAssignedToTheSelectedGroups) {
    tool->limitAction()->trigger();
    const std::vector<int> bars = {1, 3};
    DeflectionLimits shown;
    answer<DeflectionGroupLimitDialog>(true, [&](DeflectionGroupLimitDialog& d) {
        shown = d.limits();
        d.check(LimitSlot::MaxAbsXY)->setChecked(true);
        d.edit(LimitSlot::MaxAbsXY)->setValue(7.5);
        d.check(LimitSlot::ActiveRelXZ)->setChecked(true);
        d.radio(LimitSlot::ActiveRelXZ, 3)->setChecked(true);
    });
    EXPECT_TRUE(tool->editLimits(bars));
    EXPECT_EQ(shown, DeflectionLimits{}) << "the dialog opens with the limits of the first group (none)";
    const auto list = deflectionGroups(project.deflection, project.structure);
    for (int bar : bars) {
        const DeflectionGroupInfo& g = list[static_cast<std::size_t>(deflectionGroupIndexOfBar(list, bar))];
        EXPECT_TRUE(g.limits.item(LimitSlot::MaxAbsXY).enabled);
        EXPECT_EQ(g.limits.item(LimitSlot::MaxAbsXY).absoluteMm, 7.5);
        EXPECT_TRUE(g.limits.item(LimitSlot::ActiveRelXZ).enabled);
        EXPECT_EQ(g.limits.item(LimitSlot::ActiveRelXZ).relativeDivisor, 500.0);
    }
    EXPECT_EQ(undo.count(), 1u) << "one undoable step for both groups";
    // the second opening shows what was assigned
    answer<DeflectionGroupLimitDialog>(false, [&](DeflectionGroupLimitDialog& d) { shown = d.limits(); });
    EXPECT_FALSE(tool->editLimits({1}));
    EXPECT_TRUE(shown.item(LimitSlot::MaxAbsXY).enabled);
    EXPECT_EQ(shown.item(LimitSlot::MaxAbsXY).absoluteMm, 7.5);
    EXPECT_EQ(undo.count(), 1u);
}

TEST_F(DeflectionToolTest, ViewportLabelsFollowTheData) {
    EXPECT_EQ(view->deflection(), &project.deflection);
    answer<DeflectionTypeDialog>(true, [](DeflectionTypeDialog&) {});
    ASSERT_TRUE(tool->createBetween(1, 3));
    const auto list = deflectionGroups(project.deflection, project.structure);
    ASSERT_GE(list.size(), 2u);
    EXPECT_EQ(deflectionGroupLabel(list.front()), "G1(1-2 xy, 1-2 xz)") << "the label of the original";
    view->grab();  // paints with the tool off and on without crashing
    tool->editAction()->trigger();
    selectBar(2);
    view->grab();
    EXPECT_TRUE(view->deflectionLabelsVisible());
}

TEST_F(DeflectionToolTest, StructureChangesDropTheDeletedPendingNode) {
    tool->createAction()->trigger();
    click(*view, screenOf(4));
    ASSERT_TRUE(tool->pendingNode().has_value());
    project.structure.removeBar(3);
    project.structure.removeNode(4);
    tool->structureChanged();
    EXPECT_FALSE(tool->pendingNode().has_value());
}

int main(int argc, char** argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    app();
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
