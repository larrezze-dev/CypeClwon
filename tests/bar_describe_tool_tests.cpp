// Geometria > Barras > Seccion / Material (T-P5-07, GUI part): the BarDescribeTool commands. Never shown on screen (offscreen);
// the dialogs run through a replaceable runner so nothing blocks.
#include <gtest/gtest.h>

#include <QAction>
#include <QDialog>
#include <QPushButton>
#include <QRadioButton>
#include <QToolButton>

#include "BarDescribeDialog.h"
#include "BarDescribeTool.h"
#include "ProfileLibraryDialogs.h"
#include "Viewport3D.h"
#include "core/bar_description_edit.h"
#include "core/bar_edit.h"

using namespace cc::app;
using namespace cc::core;
using Mode = BarDescribeTool::Mode;

namespace {

class DescribeToolTest : public ::testing::Test {
protected:
    void SetUp() override {
        profileMessageSink().warn = [this](QWidget*, const QString&, const QString& text) { warnings << text; };
        n1 = project.structure.addNode({0, 0, 0});
        n2 = project.structure.addNode({4, 0, 0});
        n3 = project.structure.addNode({8, 0, 3});
        bars = addBar(project, undo, n1, n2);
        const auto second = addBar(project, undo, n2, n3);
        bars.insert(bars.end(), second.begin(), second.end());
        undo.clear();
        tool.setProject(&project, &undo);
        tool.setDialogRunner([this](QDialog& dialog) {
            ++runs;
            if (auto* describe = dynamic_cast<BarDescribeDialog*>(&dialog)) {
                entryCount = static_cast<int>(describe->entries().size());
                if (edit) edit(*describe);
            } else if (auto* material = dynamic_cast<DescribeMaterialDialog*>(&dialog)) {
                titles = material->groupTitles();
            }
            if (!accept) return static_cast<int>(QDialog::Rejected);
            dialog.accept();
            return dialog.result();
        });
    }
    Project project;
    UndoStack undo;
    BarDescribeTool tool;
    int n1 = 0, n2 = 0, n3 = 0;
    std::vector<int> bars;
    QStringList warnings;
    int runs = 0, entryCount = 0;
    bool accept = true;
    QStringList titles;
    std::function<void(BarDescribeDialog&)> edit;
};

}  // namespace

TEST_F(DescribeToolTest, ActionsAndStatusText) {
    EXPECT_TRUE(tool.sectionAction()->isEnabled());
    EXPECT_EQ(tool.sectionAction()->text(), "Sección");
    EXPECT_EQ(tool.sectionAction()->toolTip(), "Sección (Ctrl+d)");
    EXPECT_EQ(tool.sectionAction()->shortcut(), QKeySequence(Qt::CTRL | Qt::Key_D));
    EXPECT_EQ(tool.sectionAction()->statusTip(), "Permite seleccionar el tipo de elemento estructural, el material y la geometría para un grupo de barras.");
    QString status;
    QObject::connect(&tool, &BarDescribeTool::statusChanged, [&](const QString& text) { status = text; });
    tool.sectionAction()->trigger();
    EXPECT_EQ(tool.mode(), Mode::Section);
    EXPECT_EQ(status, "Barras - Sección |  - LMB: Seleccione un grupo de elementos. - RMB: Actuar sobre los elementos seleccionados.");
    tool.materialAction()->trigger();  // exclusive
    EXPECT_EQ(tool.mode(), Mode::Material);
    EXPECT_FALSE(tool.sectionAction()->isChecked());
    EXPECT_EQ(status, "Barras - Material |  - LMB: Seleccione un grupo de elementos. - RMB: Actuar sobre los elementos seleccionados.");
    tool.deactivate();
    EXPECT_EQ(tool.mode(), Mode::None);
    tool.setProject(nullptr, &undo);
    EXPECT_FALSE(tool.sectionAction()->isEnabled());
}

TEST_F(DescribeToolTest, ActivatingSelectsBarsInTheViewport) {
    Viewport3D view;
    view.resize(400, 300);
    view.setStructure(&project.structure);
    tool.addViewport(&view);
    tool.sectionAction()->trigger();
    EXPECT_TRUE(view.selectionMode());
    tool.deactivate();
    EXPECT_FALSE(view.selectionMode());
}

TEST_F(DescribeToolTest, DescribingWithTheDefaultsIsOneUndoStep) {
    EXPECT_TRUE(tool.describeBars(bars));
    EXPECT_EQ(runs, 1);
    EXPECT_EQ(entryCount, 1);  // not described yet: one entry with the defaults
    for (int id : bars) {
        ASSERT_TRUE(isBarDescribed(project, id));
        EXPECT_EQ(barLabelText(*descriptionOfBar(project, id)), "IPE 80");
    }
    EXPECT_FALSE(project.profileLibrary.series.empty());  // the seeded standard series were stored with the description
    EXPECT_EQ(undo.count(), 1u);                           // library + description: ONE step
    undo.undo();
    EXPECT_TRUE(project.profileLibrary.series.empty());
    EXPECT_FALSE(isBarDescribed(project, bars.front()));
    EXPECT_FALSE(undo.canUndo());
    undo.redo();
    EXPECT_TRUE(isBarDescribed(project, bars.back()));
}

TEST_F(DescribeToolTest, CancelLeavesEverythingUntouched) {
    accept = false;
    EXPECT_FALSE(tool.describeBars(bars));
    EXPECT_EQ(runs, 1);
    EXPECT_FALSE(isBarDescribed(project, bars.front()));
    EXPECT_TRUE(project.profileLibrary.series.empty());
    EXPECT_EQ(undo.count(), 0u);
}

TEST_F(DescribeToolTest, GenericDescriptionAppliesToEverySelectedBar) {
    edit = [](BarDescribeDialog& d) { d.materialButton(BarMaterialIcon::Generic)->click(); };
    EXPECT_TRUE(tool.describeBars(bars));
    for (int id : bars) EXPECT_EQ(descriptionOfBar(project, id)->materialIcon, BarMaterialIcon::Generic);
    EXPECT_EQ(barDescriptionText(project, bars.front()), "Genérico");
    // the sections of the solver follow: A 25 cm2, Iyy 52.08 cm4
    const Bar* bar = project.structure.findBar(bars.front());
    const CrossSection* section = project.structure.findSection(bar->sectionId);
    ASSERT_NE(section, nullptr);
    EXPECT_NEAR(section->areaM2, 25.0e-4, 1e-12);
    EXPECT_NEAR(section->inertiaYM4, 52.08333e-8, 1e-12);
}

TEST_F(DescribeToolTest, BarsWithDifferentDescriptionsGetOneEntryEach) {
    edit = [](BarDescribeDialog& d) { d.materialButton(BarMaterialIcon::Generic)->click(); };
    ASSERT_TRUE(tool.describeBars({bars[0]}));
    edit = nullptr;
    EXPECT_TRUE(tool.describeBars(bars));  // bar 1 generic, bar 2 not described (defaults): two entries
    EXPECT_EQ(entryCount, 2);
    // untouched entries do not push anything new for the already described bar
    EXPECT_EQ(descriptionOfBar(project, bars[0])->materialIcon, BarMaterialIcon::Generic);
    EXPECT_EQ(descriptionOfBar(project, bars[1])->materialIcon, BarMaterialIcon::RolledSteel);
}

TEST_F(DescribeToolTest, RefusalOfTheDialogIsNotReachableWithTieType) {
    // Tirante is refused by the dialog itself; the tool never receives it as accepted.
    edit = [](BarDescribeDialog& d) { d.typeRadio(1)->click(); };
    accept = true;
    tool.setDialogRunner([this](QDialog& dialog) {
        auto* describe = dynamic_cast<BarDescribeDialog*>(&dialog);
        describe->typeRadio(1)->click();
        describe->accept();
        return static_cast<int>(dialog.result());
    });
    EXPECT_FALSE(tool.describeBars(bars));
    EXPECT_FALSE(isBarDescribed(project, bars.front()));
}

TEST_F(DescribeToolTest, DescribeMaterialGroups) {
    EXPECT_TRUE(tool.describeMaterial(bars));  // not described: only "Acero laminado"
    EXPECT_EQ(titles, (QStringList{"Acero laminado"}));
    edit = [](BarDescribeDialog& d) { d.materialButton(BarMaterialIcon::Concrete)->click(); };
    ASSERT_TRUE(tool.describeBars(bars));
    EXPECT_TRUE(tool.describeMaterial(bars));  // concrete alone: steel + concrete (measured)
    EXPECT_EQ(titles, (QStringList{"Acero laminado", "Hormigón"}));
    edit = [](BarDescribeDialog& d) { d.materialButton(BarMaterialIcon::Timber)->click(); };
    ASSERT_TRUE(tool.describeBars({bars[0]}));
    EXPECT_TRUE(tool.describeMaterial({bars[0]}));
    EXPECT_EQ(titles, (QStringList{"Acero laminado", "Madera"}));
    EXPECT_TRUE(tool.describeMaterial(bars));  // mixed selection: only steel (measured)
    EXPECT_EQ(titles, (QStringList{"Acero laminado"}));
    accept = false;
    EXPECT_FALSE(tool.describeMaterial(bars));
}
