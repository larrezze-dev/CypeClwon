// T-P8-06 GUI, offscreen: «Cálculo» dialog, report, «Combinaciones no lineales» (generation #378563), result panels, the calculation tool
// (results enabled after a calculation, invalidated by an edit, not by saving; write-back of regenerated non-linear combinations as an undoable
// step) and the hypothesis deletion questions #64905 / #380751 / #380752 with the Cancelar round trip.
#include <gtest/gtest.h>

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QImage>
#include <QPainter>
#include <QComboBox>
#include <QLineEdit>
#include <QRadioButton>
#include <QTableWidget>
#include <QTextBrowser>
#include <QToolButton>

#include "CalculationDialogs.h"
#include "CalculationTool.h"
#include "LoadCaseDialogs.h"
#include "ResultsPanel.h"
#include "UserMessages.h"
#include "Viewport3D.h"
#include "core/load_case_edit.h"
#include "core/nonlinear_combinations_edit.h"
#include "core/project.h"
#include "core/undo_stack.h"

using namespace cc::app;
namespace core = cc::core;

namespace {

void ensureApp() {
    static int argc = 1;
    static char name[] = "calculation_gui_tests";
    static char* argv[] = {name, nullptr};
    static QApplication instance(argc, argv);
}

struct Notices {
    std::vector<QString> texts;
    Notices() {
        messages::handler() = [this](QWidget*, messages::Kind, const QString& text) {
            texts.push_back(text);
            return true;
        };
    }
    ~Notices() { messages::handler() = nullptr; }
};

core::Project cantilever(bool springWithSense) {
    core::Project p;
    core::Material mat;
    mat.name = "ref";
    mat.kind = core::MaterialKind::Steel;
    mat.elasticModulusMPa = 210000.0;
    mat.poissonRatio = 0.3;
    mat.densityKnM3 = 77.01;
    const int m = p.structure.addMaterial(mat);
    core::CrossSection cs;
    cs.name = "ref";
    cs.areaM2 = 0.01;
    cs.inertiaYM4 = 1e-4;
    cs.inertiaZM4 = 5e-5;
    cs.torsionM4 = 8e-5;
    cs.shearAreaYM2 = 0.004;
    cs.shearAreaZM2 = 0.004;
    const int s = p.structure.addSection(cs);
    const int n1 = p.structure.addNode({0, 0, 0});
    const int n2 = p.structure.addNode({4, 0, 0});
    p.structure.addBar(n1, n2, m, s);
    core::ExternalSupport sup;
    sup.type = core::ExternalSupportType::Fixed;
    p.nodeSupports.external[n1] = sup;
    if (springWithSense) {
        // N2: Dz elastic K = 1000 kN/m with Sentido Positivo (solver_diafragmas_ligaduras_apoyos.md 10.1).
        core::ExternalSupport spring;
        spring.type = core::ExternalSupportType::Generic;
        spring.generic.translations[2] = core::DofMode::Elastic;
        spring.elasticConstants[2] = 1000.0;
        spring.senses[2] = core::SupportSense::Positive;
        p.nodeSupports.external[n2] = spring;
    }
    return p;
}

}  // namespace

TEST(CalculationDialog, LayoutWithAndWithoutNonlinearElements) {
    ensureApp();
    CalculationDialog plain(false, false, {});
    EXPECT_EQ(plain.linearBox(), nullptr);
    EXPECT_TRUE(plain.checkBarsBox()->isChecked());  // cp_61_calcdlg.png
    EXPECT_FALSE(plain.finiteNodesBox()->isChecked());
    EXPECT_TRUE(plain.noDesignRadio()->isChecked());
    CalculationDialog nl(true, true, {});
    ASSERT_NE(nl.linearBox(), nullptr);
    ASSERT_NE(nl.updateBox(), nullptr);
    EXPECT_EQ(nl.nonlinearBox()->text(), QStringLiteral("Análisis no lineal de tensión / deformación"));
    EXPECT_EQ(nl.updateBox()->text(), QStringLiteral("Actualizar las combinaciones no lineales tras el análisis lineal"));
    nl.nonlinearBox()->setChecked(false);
    EXPECT_FALSE(nl.updateBox()->isEnabled());
    EXPECT_FALSE(nl.choices().nonlinear);
    CalculationDialog noCombinations(true, false, {});
    EXPECT_EQ(noCombinations.updateBox(), nullptr);  // shown only once the work has combinations [V 12.5]
}

TEST(CalculationReport, OneLinePerMessageWithItsIcon) {
    ensureApp();
    const std::vector<cc::fem::ReportEntry> entries{{cc::fem::ReportLevel::Error, "No se ha realizado el análisis de tensión/deformación no lineal"},
                                                    {cc::fem::ReportLevel::Warning, "La obra ha sido calculada sin realizar la comprobación de las barras."}};
    const QString html = CalculationReportDialog::pageHtml(entries);
    EXPECT_TRUE(html.contains(QStringLiteral("Informe final de cálculo")));
    EXPECT_TRUE(html.contains(QStringLiteral("Cálculo de la estructura")));
    EXPECT_TRUE(html.contains(QStringLiteral("report_error")));
    EXPECT_TRUE(html.contains(QStringLiteral("report_warning")));
    CalculationReportDialog dialog(entries);
    EXPECT_TRUE(dialog.page()->toPlainText().contains(QStringLiteral("sin realizar la comprobación")));
}

TEST(NonlinearCombinationsDialog, GenerationEditingAndValidation) {
    ensureApp();
    Notices notices;
    core::Project p = cantilever(true);
    NonlinearCombinationsDialog dialog(p);
    EXPECT_EQ(dialog.table()->rowCount(), 0);
    EXPECT_EQ(dialog.generateButton()->toolTip(), QStringLiteral("Generar combinaciones para las distintas situaciones de proyecto"));
    dialog.setDialogRunner([](QDialog& d) {
        auto* g = qobject_cast<NonlinearGenerateDialog*>(&d);
        EXPECT_NE(g, nullptr);
        EXPECT_FALSE(g->box(core::NonlinearCombinationKind::Cracking)->isEnabled());  // not generated by the clone
        g->box(core::NonlinearCombinationKind::Strength)->setChecked(true);
        g->box(core::NonlinearCombinationKind::Displacements)->setChecked(true);
        return int(QDialog::Accepted);
    });
    dialog.generate();
    ASSERT_EQ(dialog.table()->rowCount(), 2);  // s_10.png: PP, 1.35·PP, both acting, 10 steps
    EXPECT_EQ(dialog.table()->item(0, 0)->text(), QStringLiteral("PP"));
    EXPECT_EQ(dialog.table()->item(1, 0)->text(), QStringLiteral("1.35·PP"));
    EXPECT_EQ(dialog.table()->item(1, 2)->text(), QStringLiteral("10"));
    ASSERT_EQ(notices.texts.size(), 1u);
    EXPECT_EQ(notices.texts[0], QStringLiteral("Se han generado '2' combinaciones."));
    EXPECT_EQ(dialog.kindsTable()->rowCount(), 8);
    EXPECT_EQ(dialog.kindsTable()->item(0, 1)->checkState(), Qt::Checked);  // Resistencia for PP
    EXPECT_EQ(dialog.factorsTable()->rowCount(), 1);
    // Edit: steps and «Actúa».
    dialog.table()->item(1, 2)->setText(QStringLiteral("5"));
    dialog.table()->item(1, 1)->setCheckState(Qt::Unchecked);
    EXPECT_EQ(dialog.value().items[1].loadSteps, 5);
    EXPECT_FALSE(dialog.value().items[1].acts);
    dialog.table()->item(1, 2)->setText(QStringLiteral("0"));  // refused
    EXPECT_EQ(dialog.value().items[1].loadSteps, 5);
    // Up / down / delete.
    dialog.table()->selectRow(1);
    dialog.upButton()->click();
    EXPECT_EQ(dialog.value().items[0].reference, "1.35·PP");
    dialog.deleteButton()->click();
    EXPECT_EQ(dialog.value().items.size(), 1u);
    dialog.accept();
    EXPECT_EQ(dialog.result(), QDialog::Accepted);
    EXPECT_TRUE(dialog.value().generateKinds[0]);
}

TEST(ResultsPanel, LayoutsMeasuredInTheOriginal) {
    ensureApp();
    ResultsPanel forces(ResultsPanel::Kind::Forces);
    for (int r = 0; r < ResultsPanel::kForceRows; ++r) EXPECT_FALSE(forces.rowBox(r)->isChecked());
    EXPECT_EQ(forces.scaleEdit(4)->text(), QStringLiteral("0.100"));
    EXPECT_EQ(forces.scaleEdit(ResultsPanel::kDeformedRow)->text(), QStringLiteral("100.000"));
    EXPECT_FALSE(forces.queryBox()->isEnabled());
    forces.selectedOnlyRadio()->setChecked(true);
    EXPECT_TRUE(forces.queryBox()->isChecked());  // it checks itself [V]
    EXPECT_TRUE(forces.queryValues());
    EXPECT_EQ(ResultsPanel::rowColour(4), 0x008080u);
    ResultsPanel displacements(ResultsPanel::Kind::Displacements);
    for (int k = 0; k < 6; ++k) EXPECT_TRUE(displacements.componentBox(k)->isChecked());
    EXPECT_EQ(ResultsPanel::title(ResultsPanel::Kind::Displacements), QStringLiteral("Propiedades - Desplazamientos"));  // s_lin.png
    ResultsPanel reactions(ResultsPanel::Kind::Reactions);
    for (int k = 0; k < 6; ++k) EXPECT_TRUE(reactions.componentBox(k)->isChecked());  // rp_03_react_open.png
}

TEST(CalculationTool, CalculateEnablesResultsAndAnEditInvalidatesThem) {
    ensureApp();
    core::Project p = cantilever(false);
    core::UndoStack stack;
    CalculationTool tool;
    tool.setSynchronous(true);
    tool.setDialogRunner([](QDialog& d) {
        if (qobject_cast<CalculationDialog*>(&d)) return int(QDialog::Accepted);
        return int(QDialog::Rejected);  // the report
    });
    tool.setProject(&p, &stack);
    EXPECT_TRUE(tool.calculateAction()->isEnabled());
    EXPECT_FALSE(tool.displacementsAction()->isEnabled());
    ASSERT_TRUE(tool.calculate());
    ASSERT_TRUE(tool.calculated());
    EXPECT_TRUE(tool.displacementsAction()->isEnabled());
    EXPECT_TRUE(tool.forcesAction()->isEnabled());
    ASSERT_FALSE(tool.lastReport().empty());
    EXPECT_EQ(tool.lastReport().back().text, "La obra ha sido calculada sin realizar la comprobación de las barras.");
    // Saving does not invalidate.
    stack.setClean();
    tool.structureChanged();
    EXPECT_TRUE(tool.calculated());
    // Opening a mode shows its panel with the load case.
    tool.forcesAction()->setChecked(true);
    ASSERT_NE(tool.panel(), nullptr);
    EXPECT_EQ(tool.panel()->itemCombo()->currentText(), QStringLiteral("Peso propio"));
    // An edit invalidates the results and ends the mode.
    core::NonlinearCombinations other;
    other.updateAfterLinear = false;
    core::setNonlinearCombinations(p, stack, other);
    tool.structureChanged();
    EXPECT_FALSE(tool.calculated());
    EXPECT_FALSE(tool.forcesAction()->isEnabled());
    EXPECT_EQ(tool.mode(), CalculationTool::Mode::None);
}

TEST(CalculationTool, NonlinearCombinationsAreUpdatedAsOneUndoableStep) {
    ensureApp();
    core::Project p = cantilever(true);
    // Combinations generated before: Resistencia only gives the two of the measured work.
    p.nonlinearCombinations.generateKinds[0] = true;
    p.nonlinearCombinations.generateKinds[3] = true;
    core::NonlinearCombination stale;
    stale.reference = "old";
    stale.factors[0] = 2.0;
    p.nonlinearCombinations.items.push_back(stale);
    core::UndoStack stack;
    CalculationTool tool;
    tool.setSynchronous(true);
    tool.setDialogRunner([](QDialog& d) { return qobject_cast<CalculationDialog*>(&d) ? int(QDialog::Accepted) : int(QDialog::Rejected); });
    tool.setProject(&p, &stack);
    ASSERT_TRUE(tool.calculate());
    ASSERT_TRUE(tool.calculated());
    ASSERT_EQ(p.nonlinearCombinations.items.size(), 2u);
    EXPECT_EQ(p.nonlinearCombinations.items[0].reference, "PP");
    EXPECT_EQ(stack.count(), 1u);
    tool.structureChanged();
    EXPECT_TRUE(tool.calculated());  // the tool's own write does not invalidate
    ASSERT_NE(tool.analysis(), nullptr);
    EXPECT_TRUE(tool.analysis()->nonlinearCalculated);
    stack.undo();
    EXPECT_EQ(p.nonlinearCombinations.items.front().reference, "old");
}

TEST(LoadCaseDeletion, QuestionDependsOnWhatGoesWithTheHypothesis) {
    ensureApp();
    core::LoadCaseDependents none;
    EXPECT_TRUE(deleteLoadCaseQuestion(none, QStringLiteral("CM 1")).isEmpty());
    core::LoadCaseDependents loads;
    loads.loads.push_back(1);
    EXPECT_EQ(deleteLoadCaseQuestion(loads, QStringLiteral("CM 1")),
              QStringLiteral("Se van a borrar todas las cargas definidas en la hipótesis.: 'CM 1'. ¿Desea continuar?"));
    core::LoadCaseDependents combos;
    combos.nonlinearCombinations.push_back(0);
    EXPECT_TRUE(deleteLoadCaseQuestion(combos, QStringLiteral("CM 1"))
                    .startsWith(QStringLiteral("Se van a borrar las combinaciones no lineales y de pandeo en las que interviene la hipótesis.")));
    combos.loads.push_back(1);
    EXPECT_TRUE(deleteLoadCaseQuestion(combos, QStringLiteral("CM 1"))
                    .startsWith(QStringLiteral("Se van a borrar todas las cargas y combinaciones no lineales y de pandeo asociadas a la hipótesis.")));
}

TEST(LoadCaseDeletion, CancelRestoresTheNonlinearCombinations) {
    core::Project p;
    core::NonlinearCombination c;
    c.reference = "PP";
    c.factors[0] = 1.0;
    p.nonlinearCombinations.items.push_back(c);
    const LoadCaseState state = LoadCaseState::capture(p);
    p.nonlinearCombinations.items.clear();
    state.restoreTo(p);
    EXPECT_EQ(p.nonlinearCombinations.items.size(), 1u);
}

int main(int argc, char** argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

namespace {

/// A calculated cantilever (tip loads in Y and Z so that the deflection has both components), its viewport and tool, ready to open result modes.
struct ResultsFixture {
    ResultsFixture() {
        ensureApp();
        project = cantilever(false);
        for (const auto& [dir, magnitude] : {std::pair{core::LoadDirection::PlusY, 10.0}, std::pair{core::LoadDirection::MinusZ, 10.0}}) {
            core::NodalLoad nl;
            nl.id = project.nodalLoads.nextId++;
            nl.nodeId = 2;
            nl.loadCaseId = 0;
            nl.magnitude = magnitude;
            nl.direction = dir;
            project.nodalLoads.items.push_back(nl);
        }
        view = std::make_unique<Viewport3D>();
        view->resize(800, 600);
        view->setOrthographic(true);
        view->camera().target = {2, 0, 0};
        view->setStructure(&project.structure);
        view->setNodeSupports(&project.nodeSupports);
        tool = std::make_unique<CalculationTool>();
        tool->setSynchronous(true);
        tool->setDialogRunner([](QDialog& d) { return qobject_cast<CalculationDialog*>(&d) ? int(QDialog::Accepted) : int(QDialog::Rejected); });
        tool->setProject(&project, &stack);
        tool->addViewport(view.get());
        EXPECT_TRUE(tool->calculate());
    }
    /// Number of pixels of the overlay within a few units of `colour`.
    int pixels(const QColor& colour) {
        QImage image(800, 600, QImage::Format_ARGB32);
        image.fill(Qt::white);
        QPainter painter(&image);
        tool->paint(painter, *view);
        painter.end();
        int n = 0;
        for (int y = 0; y < image.height(); ++y)
            for (int x = 0; x < image.width(); ++x) {
                const QColor c = image.pixelColor(x, y);
                if (std::abs(c.red() - colour.red()) < 60 && std::abs(c.green() - colour.green()) < 60 && std::abs(c.blue() - colour.blue()) < 60) ++n;
            }
        return n;
    }
    void setBox(ResultsPanel* panel, const QString& text, bool on) {
        for (QCheckBox* box : panel->findChildren<QCheckBox*>())
            if (box->text() == text) box->setChecked(on);
    }
    core::Project project;
    core::UndoStack stack;
    std::unique_ptr<Viewport3D> view;
    std::unique_ptr<CalculationTool> tool;
};

}  // namespace

TEST(CalculationTool, FirstPanelOfAWorkOpensMaximisedAndTheNextOnesDoNot) {
    ResultsFixture f;
    f.tool->reactionsAction()->setChecked(true);  // [V rp_03_react_open.png]
    ASSERT_NE(f.tool->panel(), nullptr);
    EXPECT_TRUE(f.tool->panel()->isMaximized());
    f.tool->forcesAction()->setChecked(true);
    ASSERT_NE(f.tool->panel(), nullptr);
    EXPECT_FALSE(f.tool->panel()->isMaximized());
    f.tool->deactivate();
    // A new work starts again with a maximised panel.
    core::Project other = cantilever(false);
    core::UndoStack otherStack;
    f.tool->setProject(&other, &otherStack);
    ASSERT_TRUE(f.tool->calculate());
    f.tool->displacementsAction()->setChecked(true);
    EXPECT_TRUE(f.tool->panel()->isMaximized());
}

TEST(ResultsPanel, DeflectionRowsAndOptionsAreAvailable) {
    ensureApp();
    ResultsPanel forces(ResultsPanel::Kind::Forces);
    for (int r = ResultsPanel::kDeflectionRow; r < ResultsPanel::kForceRows; ++r) {
        EXPECT_TRUE(forces.rowBox(r)->isEnabled());
        EXPECT_TRUE(forces.scaleEdit(r)->isEnabled());
        EXPECT_FALSE(forces.rowChecked(r));
    }
    EXPECT_FALSE(forces.anyRowChecked());
    forces.rowBox(8)->setChecked(true);
    EXPECT_TRUE(forces.rowChecked(8));
    EXPECT_TRUE(forces.anyRowChecked());
    EXPECT_FALSE(forces.axesOnWindow());
    EXPECT_TRUE(forces.showStructure());
    forces.rowBox(ResultsPanel::kDeformedRow)->setChecked(true);
    forces.selectedOnlyRadio()->setChecked(true);  // the deformed shape and the axes option leave with «Sólo las barras seleccionadas»
    EXPECT_FALSE(forces.rowChecked(ResultsPanel::kDeformedRow));
    EXPECT_FALSE(forces.axesOnWindow());
}

TEST(CalculationTool, DeflectionCurveGizmoAndHiddenStructureAreDrawn) {
    ResultsFixture f;
    f.tool->forcesAction()->setChecked(true);
    ResultsPanel* panel = f.tool->panel();
    ASSERT_NE(panel, nullptr);
    const QColor flecha = QColor::fromRgb(ResultsPanel::rowColour(9));
    const int baseline = f.pixels(flecha);
    EXPECT_GT(f.pixels(QColor(0xFF, 0, 0)), 5);  // gizmo: x axis arrow in red [V rp_07]
    EXPECT_GT(f.pixels(QColor(0, 0, 0xFF)), 5);  // z axis arrow in blue
    panel->rowBox(9)->setChecked(true);
    EXPECT_GT(f.pixels(flecha), baseline);
    panel->rowBox(8)->setChecked(true);
    EXPECT_GT(f.pixels(QColor::fromRgb(ResultsPanel::rowColour(8))), 5);
    // «Dibujar valores máximos y mínimos» adds the dashed dimension line, the marker and the label.
    const int before = f.pixels(flecha);
    f.setBox(panel, QStringLiteral("Dibujar valores máximos y mínimos"), true);
    EXPECT_GT(f.pixels(flecha), before);
    // «Ver la estructura» unchecked hides the bars while a row is drawn.
    EXPECT_FALSE(f.view->barsHidden());
    f.setBox(panel, QStringLiteral("Ver la estructura"), false);
    EXPECT_TRUE(f.view->barsHidden());
    panel->rowBox(9)->setChecked(false);
    panel->rowBox(8)->setChecked(false);
    EXPECT_FALSE(f.view->barsHidden());  // nothing drawn: the structure stays
    f.tool->deactivate();
    EXPECT_FALSE(f.view->barsHidden());
}

TEST(CalculationTool, ContourOnlyWithoutFillAndWindowAxesDrawTheLaws) {
    ResultsFixture f;
    f.tool->forcesAction()->setChecked(true);
    ResultsPanel* panel = f.tool->panel();
    const QColor my = QColor::fromRgb(ResultsPanel::rowColour(4));
    panel->rowBox(4)->setChecked(true);
    const int filled = f.pixels(my);
    f.setBox(panel, QStringLiteral("Dibujar las leyes con relleno"), false);
    const int contour = f.pixels(my);
    EXPECT_GT(contour, 5);
    EXPECT_LE(contour, filled);  // the semi-transparent fill never makes an exact-colour pixel
    f.setBox(panel, QStringLiteral("Ejes sobre el plano de la ventana"), true);
    EXPECT_TRUE(panel->axesOnWindow());
    EXPECT_GT(f.pixels(my), 5);  // the law is still drawn, now in the plane of the window
}

TEST(CalculationTool, SupportsAreTurquoiseAndLightBrownWithBalloonInReactions) {
    ResultsFixture f;
    EXPECT_FALSE(f.view->supportColour(1).has_value());
    f.tool->reactionsAction()->setChecked(true);
    ASSERT_TRUE(f.view->supportColour(1).has_value());
    EXPECT_EQ(*f.view->supportColour(1), QColor(0x00, 0x9A, 0xDC));  // [≈V rp_05]
    QPointF at;
    ASSERT_TRUE(f.view->worldToScreen({0, 0, 0}, at));
    f.tool->clickAt(f.view.get(), at);
    ASSERT_TRUE(f.tool->shownNodes().count(1));
    EXPECT_EQ(*f.view->supportColour(1), QColor(0xB0, 0x9A, 0x84));  // [≈V rp_04]
    f.tool->forcesAction()->setChecked(true);
    EXPECT_FALSE(f.view->supportColour(1).has_value());
}
