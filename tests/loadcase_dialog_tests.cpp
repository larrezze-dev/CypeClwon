// Tests of the load-case dialogs (T-P7-01 GUI), run on Qt's offscreen platform. Modal windows opened from a dialog are not driven here: the
// public operations of the dialogs (createCase, deleteRow, addCategory...) are called directly and the windows the original opens are built alone.
#include <gtest/gtest.h>

#include <QApplication>
#include <QCheckBox>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QRadioButton>
#include <QTableWidget>
#include <QTimer>
#include <QToolButton>

#include "LoadCaseDialogs.h"
#include "UserMessages.h"
#include "core/general_data.h"
#include "core/shell_load_edit.h"

using namespace cc::app;
using namespace cc::core;

namespace {
void ensureApp() {
    static int argc = 1;
    static char name[] = "loadcase_dialog_tests";
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

LoadCaseSet withOneCase(LoadNature nature, const char* ref, const char* description) {
    Project p;
    UndoStack s;
    NewLoadCase n;
    n.nature = nature;
    n.reference = ref;
    n.description = description;
    EXPECT_EQ(createLoadCase(p, s, n), LoadCaseError::None);
    return p.loadCases;
}
}  // namespace

TEST(AdditionalLoadsDialog, ShowsTheTenRowsWithCounters) {
    ensureApp();
    AdditionalLoadsDialog dialog{LoadCaseSet{}};
    EXPECT_EQ(dialog.size(), QSize(280, 443));
    EXPECT_EQ(dialog.categoriesLabel()->text(), "No hay categorías de uso definidas");
    EXPECT_EQ(dialog.counter(LoadNature::SelfWeight)->text(), "---");
    EXPECT_EQ(dialog.editButton(LoadNature::SelfWeight), nullptr);  // "Peso propio" has no button
    for (int n = 1; n < kLoadNatureCount; ++n) {
        EXPECT_EQ(dialog.counter(static_cast<LoadNature>(n))->text(), "0");
        EXPECT_NE(dialog.editButton(static_cast<LoadNature>(n)), nullptr);
    }
}

TEST(AdditionalLoadsDialog, CounterFollowsTheSet) {
    ensureApp();
    AdditionalLoadsDialog dialog{withOneCase(LoadNature::Wind, "V 1", "")};
    EXPECT_EQ(dialog.counter(LoadNature::Wind)->text(), "1");
    EXPECT_EQ(dialog.counter(LoadNature::Snow)->text(), "0");
    EXPECT_EQ(dialog.value().cases.size(), 2u);
}

TEST(LoadNatureDialog, EmptyTableHasFiveRowsAndTitleIsTheNature) {
    ensureApp();
    Project project;
    UndoStack stack;
    LoadNatureDialog dialog(LoadNature::DeadLoad, project, stack);
    EXPECT_EQ(dialog.windowTitle(), "Cargas muertas");
    EXPECT_EQ(dialog.table()->rowCount(), 5);
    EXPECT_EQ(dialog.table()->columnCount(), 4);
    EXPECT_EQ(dialog.table()->horizontalHeaderItem(0)->text(), "Actúa");
    EXPECT_EQ(dialog.table()->horizontalHeaderItem(1)->text(), "Hipótesis adicionales");
    EXPECT_EQ(dialog.table()->horizontalHeaderItem(2)->text(), "Editar");
    EXPECT_EQ(dialog.table()->horizontalHeaderItem(3)->text(), "Borrar");
    EXPECT_EQ(dialog.secondOrderCheck(), nullptr);
}

TEST(LoadNatureDialog, RowShowsReferenceAndDescriptionOnTwoLines) {
    ensureApp();
    Project project;
    project.loadCases = withOneCase(LoadNature::DeadLoad, "CM 1", "Prueba uno");
    UndoStack stack;
    LoadNatureDialog dialog(LoadNature::DeadLoad, project, stack);
    EXPECT_EQ(dialog.table()->item(0, 1)->text().section('\n', 0, 0), "CM 1");
    EXPECT_EQ(dialog.table()->item(0, 1)->text().section('\n', 1).trimmed(), "Prueba uno");
    EXPECT_FALSE(dialog.table()->item(0, 0)->icon().isNull());
    EXPECT_GE(dialog.caseIdOfRow(0), 1);
    EXPECT_EQ(dialog.caseIdOfRow(1), -1);
}

TEST(LoadNatureDialog, ActsDeleteAndUndoThroughTheCore) {
    ensureApp();
    Project project;
    project.loadCases = withOneCase(LoadNature::Snow, "N 1", "");
    UndoStack stack;
    LoadNatureDialog dialog(LoadNature::Snow, project, stack);
    dialog.toggleRow(0);
    EXPECT_FALSE(project.loadCases.cases[1].acts);
    stack.undo();
    EXPECT_TRUE(project.loadCases.cases[1].acts);
    dialog.deleteRow(0);  // no loads: immediate, no question
    EXPECT_EQ(project.loadCases.cases.size(), 1u);
    EXPECT_EQ(dialog.caseIdOfRow(0), -1);
    stack.undo();
    EXPECT_EQ(project.loadCases.cases.size(), 2u);
}

TEST(LoadNatureDialog, DeletingAHypothesisWithShellLoadsAsksTheMeasuredQuestion) {
    ensureApp();
    EXPECT_EQ(deleteLoadCaseQuestion(QStringLiteral("CM 1")),
              QStringLiteral("Se van a borrar todas las cargas definidas en la hipótesis.: 'CM 1'. ¿Desea continuar?"));
    Project project;
    project.structure.addNode({0, 0, 0});
    project.structure.addNode({1, 0, 0});
    project.structure.addNode({1, 1, 0});
    ASSERT_GT(project.structure.addShell({1, 2, 3}, 0.1), 0);
    project.loadCases = withOneCase(LoadNature::DeadLoad, "CM 1", "");
    UndoStack stack;
    const int cm = project.loadCases.cases[1].id;
    ASSERT_EQ(addShellLoads(project, stack, {project.structure.shells.front().id}, shellLoadDefaults(cm)), ShellLoadError::None);
    EXPECT_EQ(loadCaseDependents(project, cm).shellLoads.size(), 1u) << "the loads of shells count as dependents";
    std::vector<QString> asked;
    bool answer = false;
    messages::handler() = [&](QWidget*, messages::Kind kind, const QString& text) {
        EXPECT_EQ(kind, messages::Kind::Question);
        asked.push_back(text);
        return answer;
    };
    LoadNatureDialog dialog(LoadNature::DeadLoad, project, stack);
    dialog.deleteRow(0);  // No: nothing is destroyed
    ASSERT_EQ(asked.size(), 1u);
    EXPECT_EQ(asked[0], QStringLiteral("Se van a borrar todas las cargas definidas en la hipótesis.: 'CM 1'. ¿Desea continuar?"));
    EXPECT_EQ(project.loadCases.cases.size(), 2u);
    EXPECT_EQ(project.shellLoads.items.size(), 1u);
    answer = true;  // Sí
    dialog.deleteRow(0);
    EXPECT_EQ(project.loadCases.cases.size(), 1u);
    EXPECT_TRUE(project.shellLoads.items.empty());
    messages::handler() = nullptr;
}

TEST(LoadNatureDialog, CancelRestoresTheInitialCases) {
    ensureApp();
    Project project;
    project.loadCases = withOneCase(LoadNature::Snow, "N 1", "");
    UndoStack stack;
    LoadNatureDialog dialog(LoadNature::Snow, project, stack);
    dialog.deleteRow(0);
    EXPECT_EQ(project.loadCases.cases.size(), 1u);
    dialog.reject();
    EXPECT_EQ(project.loadCases.cases.size(), 2u);
}

TEST(LoadNatureDialog, CancelRestoresTheLoadsAndTheSecondDeletionAsksAgain) {
    ensureApp();
    Project project;
    project.structure.addNode({0, 0, 0});
    project.structure.addNode({1, 0, 0});
    project.structure.addNode({1, 1, 0});
    ASSERT_GT(project.structure.addShell({1, 2, 3}, 0.1), 0);
    project.loadCases = withOneCase(LoadNature::DeadLoad, "CM 1", "");
    UndoStack stack;
    const int cm = project.loadCases.cases[1].id;
    ASSERT_EQ(addShellLoads(project, stack, {project.structure.shells.front().id}, shellLoadDefaults(cm)), ShellLoadError::None);
    const auto before = LoadCaseState::capture(project);
    int asked = 0;
    messages::handler() = [&](QWidget*, messages::Kind, const QString&) {
        ++asked;
        return true;
    };
    {
        LoadNatureDialog dialog(LoadNature::DeadLoad, project, stack);
        dialog.deleteRow(0);  // Sí
        EXPECT_TRUE(project.shellLoads.items.empty());
        dialog.reject();  // Cancelar: hypotheses AND loads come back
    }
    EXPECT_EQ(asked, 1);
    EXPECT_EQ(project.loadCases.cases.size(), 2u);
    EXPECT_EQ(project.shellLoads.items.size(), 1u);
    EXPECT_EQ(loadCaseDependents(project, cm).shellLoads.size(), 1u);
    {
        LoadNatureDialog dialog(LoadNature::DeadLoad, project, stack);
        dialog.deleteRow(0);
    }
    EXPECT_EQ(asked, 2) << "the second deletion asks again";
    before.restoreTo(project);  // the snapshot alone brings everything back
    EXPECT_EQ(project.loadCases.cases.size(), 2u);
    EXPECT_EQ(project.shellLoads.items.size(), 1u);
    messages::handler() = nullptr;
}

TEST(LoadNatureDialog, SeismicHasTypeColumnAndSecondOrderCheck) {
    ensureApp();
    Project project;
    UndoStack stack;
    LoadNatureDialog dialog(LoadNature::Seismic, project, stack);
    EXPECT_EQ(dialog.table()->columnCount(), 5);
    EXPECT_EQ(dialog.table()->horizontalHeaderItem(2)->text(), "Tipo");
    ASSERT_NE(dialog.secondOrderCheck(), nullptr);
    EXPECT_EQ(dialog.secondOrderCheck()->text(), "Considerar efectos de segundo orden");
    EXPECT_FALSE(dialog.secondOrderCheck()->isChecked());
    dialog.secondOrderCheck()->setChecked(true);
    EXPECT_TRUE(project.loadCases.seismicSecondOrder);
    stack.undo();
    EXPECT_FALSE(project.loadCases.seismicSecondOrder);
}

TEST(LoadCaseDataDialog, NewProposesTheReferenceAndHasNoArrangementsYet) {
    ensureApp();
    LoadCaseDataDialog dialog(LoadNature::Wind, "V 1");
    EXPECT_EQ(dialog.size(), QSize(415, 189));
    EXPECT_EQ(dialog.referenceEdit()->text(), "V 1");
    EXPECT_EQ(dialog.descriptionEdit()->text(), "");
    ASSERT_NE(dialog.arrangementsCheck(), nullptr);
    EXPECT_FALSE(dialog.arrangementsCheck()->isChecked());
    EXPECT_FALSE(dialog.withArrangements());
    EXPECT_EQ(dialog.compat(), ArrangementCompat::Simultaneous);
}

TEST(LoadCaseDataDialog, ArrangementsGrowTheWindowAndFollowTheCoreRules) {
    ensureApp();
    LoadCaseDataDialog dialog(LoadNature::Wind, "V 1");
    dialog.arrangementsCheck()->setChecked(true);
    EXPECT_GT(dialog.height(), 189);
    EXPECT_EQ(dialog.arrangements(), (std::vector<std::string>{"1", "2"}));
    EXPECT_TRUE(dialog.compatRadio(ArrangementCompat::Simultaneous)->isChecked());
    dialog.addArrangement();
    dialog.addArrangement();
    EXPECT_EQ(dialog.arrangements(), (std::vector<std::string>{"1", "2", "3", "4"}));
    EXPECT_EQ(dialog.arrangementsTable()->selectionModel()->selectedRows().first().row(), 3);  // the new one stays selected
    dialog.deleteSelectedArrangement();
    EXPECT_EQ(dialog.arrangements().size(), 3u);
    dialog.compatRadio(ArrangementCompat::Incompatible)->setChecked(true);
    const NewLoadCase value = dialog.value();
    EXPECT_TRUE(value.withArrangements);
    EXPECT_EQ(value.compat, ArrangementCompat::Incompatible);
}

TEST(LoadCaseDataDialog, SeismicOffersTheStaticTypeInsteadOfArrangements) {
    ensureApp();
    LoadCaseDataDialog dialog(LoadNature::Seismic, "S 1");
    EXPECT_EQ(dialog.arrangementsCheck(), nullptr);
    ASSERT_NE(dialog.seismicRadio(SeismicType::Simple), nullptr);
    EXPECT_TRUE(dialog.seismicRadio(SeismicType::Simple)->isChecked());
    dialog.seismicRadio(SeismicType::XY)->setChecked(true);
    EXPECT_EQ(dialog.value().seismic, SeismicType::XY);
}

TEST(LoadCaseDataDialog, EditShowsOnlyReferenceAndDescription) {
    ensureApp();
    const LoadCaseSet set = withOneCase(LoadNature::Wind, "V 1", "Norte");
    LoadCaseDataDialog dialog(set.cases[1]);
    EXPECT_EQ(dialog.windowTitle(), "Editar hipótesis adicional");
    EXPECT_EQ(dialog.referenceEdit()->text(), "V 1");
    EXPECT_EQ(dialog.descriptionEdit()->text(), "Norte");
    EXPECT_EQ(dialog.arrangementsCheck(), nullptr);
}

TEST(LoadCaseDataDialog, BlankReferenceKeepsTheDialogOpenWithTheNotice) {
    ensureApp();
    Notices notices;
    LoadCaseDataDialog dialog(LoadNature::Wind, "V 1");
    dialog.referenceEdit()->clear();
    dialog.accept();
    EXPECT_NE(dialog.result(), QDialog::Accepted);
    ASSERT_EQ(notices.texts.size(), 1u);
    EXPECT_EQ(notices.texts[0], "'Referencia' no puede quedar en blanco.");
    dialog.referenceEdit()->setText("V 1");
    dialog.accept();
    EXPECT_EQ(dialog.result(), QDialog::Accepted);
}

TEST(LoadCaseDataDialog, EnterActsAsTab) {
    ensureApp();
    LoadCaseDataDialog dialog(LoadNature::Wind, "V 1");
    dialog.show();
    dialog.referenceEdit()->setFocus();
    QKeyEvent press(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
    QApplication::sendEvent(dialog.referenceEdit(), &press);
    EXPECT_EQ(dialog.focusWidget(), dialog.descriptionEdit());
    QApplication::sendEvent(dialog.descriptionEdit(), &press);
    EXPECT_EQ(dialog.focusWidget(), dialog.acceptButton());
    EXPECT_NE(dialog.result(), QDialog::Accepted);  // Intro did not accept the dialog
}

TEST(UseCategories, AddCopyDeleteAndCancelThroughTheCore) {
    ensureApp();
    Project project;
    UndoStack stack;
    UseCategoriesDialog dialog(project, stack);
    EXPECT_EQ(dialog.table()->rowCount(), 0);
    dialog.addCategory({"A. Domésticos y residenciales", "A. Zonas residenciales"});
    ASSERT_EQ(dialog.table()->rowCount(), 1);
    EXPECT_EQ(dialog.table()->item(0, 0)->text(), "A");
    EXPECT_EQ(dialog.table()->item(0, 1)->text(), "A. Domésticos y residenciales");
    dialog.copySelected();
    EXPECT_EQ(dialog.table()->rowCount(), 2);
    EXPECT_EQ(dialog.table()->item(1, 0)->text(), "B");
    dialog.deleteSelected();
    EXPECT_EQ(dialog.table()->rowCount(), 1);
    stack.undo();
    EXPECT_EQ(project.loadCases.useCategories.size(), 2u);
    dialog.reject();
    EXPECT_TRUE(project.loadCases.useCategories.empty());  // Cancelar: nothing stays
}

TEST(UseCategories, CategoryDialogHasOneComboPerNormGroup) {
    ensureApp();
    UseCategoryDialog dialog;
    EXPECT_EQ(dialog.windowTitle(), "Categoría");
    EXPECT_EQ(dialog.choices()[0], "A. Domésticos y residenciales");
    EXPECT_EQ(dialog.choices()[1], "A. Zonas residenciales");
}

TEST(UseCategories, ChoicesCommandChangesTheDescription) {
    Project project;
    UndoStack stack;
    ASSERT_EQ(addUseCategory(project, stack, {"x", "y"}), LoadCaseError::None);
    ASSERT_EQ(setUseCategoryChoices(project, stack, 0, {"z", "w"}), LoadCaseError::None);
    EXPECT_EQ(project.loadCases.useCategories[0].description, "z");
    EXPECT_EQ(project.loadCases.useCategories[0].choices[1], "w");
    EXPECT_EQ(setUseCategoryChoices(project, stack, 5, {"z", "w"}), LoadCaseError::UnknownCategory);
    stack.undo();
    EXPECT_EQ(project.loadCases.useCategories[0].description, "x");
}

TEST(AdditionalLoadsDialog, LiveLoadWithoutCategoriesShowsTheNoticeAndOpensCategories) {
    ensureApp();
    Notices notices;
    AdditionalLoadsDialog dialog{LoadCaseSet{}};
    // The nested "Categorías de uso" window is closed at once by a timer, as if the user defined nothing.
    QTimer::singleShot(0, [] {
        for (QWidget* w : QApplication::topLevelWidgets())
            if (auto* d = qobject_cast<UseCategoriesDialog*>(w)) d->reject();
    });
    dialog.openNature(LoadNature::LiveLoad);
    ASSERT_GE(notices.texts.size(), 1u);
    EXPECT_EQ(notices.texts[0],
              "Para poder definir las hipótesis de sobrecarga de uso, primero se deben definir las categorías de uso de la obra.");
    EXPECT_EQ(dialog.counter(LoadNature::LiveLoad)->text(), "0");
}

TEST(GeneralDataLoadCases, TravelWithTheGeneralData) {
    GeneralData data;
    data.loadCases = withOneCase(LoadNature::Snow, "N 1", "");
    Project project;
    applyGeneralData(project, data);
    EXPECT_EQ(project.loadCases.cases.size(), 2u);
    EXPECT_TRUE(sameGeneralData(generalDataOf(project), data));
    EXPECT_FALSE(sameGeneralData(GeneralData{}, data));
}

int main(int argc, char** argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
