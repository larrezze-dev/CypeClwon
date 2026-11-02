// Tests for the "Planos" dialogs (Rejilla, Niveles, Visibilidad), run on Qt's offscreen platform.
#include <gtest/gtest.h>

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QPushButton>
#include <QRadioButton>
#include <QToolButton>
#include <QTableWidget>

#include "WorkPlanesDialogs.h"

using namespace cc::app;
using namespace cc::core;

namespace {
void ensureApp() {
    static int argc = 1;
    static char name[] = "work_planes_dialog_tests";
    static char* argv[] = {name, nullptr};
    static QApplication instance(argc, argv);
}

/// Prompts that record what they were shown and answer as told, instead of opening a modal box.
struct Recorder {
    QStringList warnings;
    QStringList questions;
    bool answer = true;
    PlanesPrompts prompts() {
        PlanesPrompts p;
        p.warn = [this](QWidget*, const QString& m) { warnings << m; };
        p.askNoDefault = [this](QWidget*, const QString& m) {
            questions << m;
            return answer;
        };
        return p;
    }
};
}  // namespace

TEST(GridDialog, SizeTitlesAndDefaults) {
    ensureApp();
    GridDialog dialog{WorkPlanes{}};
    EXPECT_EQ(dialog.size(), QSize(678, 621));
    EXPECT_EQ(dialog.findChildren<QTableWidget*>().size(), 2);
    for (QCheckBox* box : dialog.findChildren<QCheckBox*>()) EXPECT_TRUE(box->isChecked());
    EXPECT_TRUE(dialog.value().adjustElements);
    EXPECT_TRUE(dialog.value().gridX.empty());
}

TEST(GridDialog, GenerationFillsBothTablesWithTheOriginalDefaults) {
    ensureApp();
    GridDialog dialog{WorkPlanes{}};
    Recorder rec;
    dialog.prompts = rec.prompts();
    EXPECT_TRUE(dialog.applyGeneration(GridSpec{}, [] { return true; }));
    EXPECT_TRUE(rec.questions.isEmpty());  // nothing to overwrite: no question
    const WorkPlanes v = dialog.value();
    ASSERT_EQ(v.gridX.size(), 4u);
    EXPECT_EQ(v.gridX[0].label, "A");
    EXPECT_EQ(v.gridX[3].label, "D");
    EXPECT_DOUBLE_EQ(v.gridX[3].coordinate, 9.0);
    EXPECT_EQ(v.gridY[2].label, "3");
    const auto tables = dialog.findChildren<QTableWidget*>();
    EXPECT_EQ(tables[0]->rowCount(), 4);
    EXPECT_EQ(tables[0]->item(1, 1)->text(), "3.000");  // three decimals
}

TEST(GridDialog, RegenerationAsksAndNoKeepsTheLines) {
    ensureApp();
    WorkPlanes initial;
    generateGrid(initial, GridSpec{});
    GridDialog dialog{initial};
    GridSpec other;
    other.x.count = 2;
    int asked = 0;
    EXPECT_FALSE(dialog.applyGeneration(other, [&] { ++asked; return false; }));
    EXPECT_EQ(asked, 1);
    EXPECT_EQ(dialog.value().gridX.size(), 4u);
    EXPECT_TRUE(dialog.applyGeneration(other, [&] { ++asked; return true; }));
    EXPECT_EQ(dialog.value().gridX.size(), 2u);
}

TEST(GridDialog, InvalidGenerationShowsTheMessageAndChangesNothing) {
    ensureApp();
    GridDialog dialog{WorkPlanes{}};
    Recorder rec;
    dialog.prompts = rec.prompts();
    GridSpec bad;
    bad.x.count = 1;
    EXPECT_FALSE(dialog.applyGeneration(bad, [] { return true; }));
    EXPECT_EQ(rec.warnings.size(), 1);
    EXPECT_TRUE(dialog.value().gridX.empty());
}

TEST(GridDialog, AcceptValidatesRepeatedLabelsAndSameCoordinate) {
    ensureApp();
    GridDialog dialog{WorkPlanes{}};
    Recorder rec;
    dialog.prompts = rec.prompts();
    dialog.addLine(0, GridLine{"A", 0.0, LabelPosition::Origin});
    dialog.addLine(0, GridLine{"A", 3.0, LabelPosition::Origin});
    dialog.accept();
    EXPECT_NE(dialog.result(), QDialog::Accepted);  // not closed
    ASSERT_EQ(rec.warnings.size(), 1);
    EXPECT_TRUE(rec.warnings[0].contains("repetida"));
    dialog.deleteLine(0, 1);
    dialog.addLine(0, GridLine{"B", 0.0, LabelPosition::End});
    dialog.accept();
    EXPECT_EQ(rec.warnings.size(), 2);
    EXPECT_TRUE(rec.warnings[1].contains("misma coordenada"));
    dialog.deleteLine(0, 1);
    dialog.addLine(0, GridLine{"B", 3.0, LabelPosition::End});
    dialog.accept();
    EXPECT_EQ(dialog.result(), QDialog::Accepted);
    EXPECT_EQ(rec.warnings.size(), 2);
}

TEST(GridDialog, DuplicateAndDeleteAndEditCells) {
    ensureApp();
    WorkPlanes initial;
    initial.gridY = {GridLine{"1", 0.0, LabelPosition::Origin}};
    GridDialog dialog{initial};
    dialog.duplicateLine(1, 0);
    ASSERT_EQ(dialog.value().gridY.size(), 2u);
    EXPECT_EQ(dialog.value().gridY[1].label, "2");
    auto* table = dialog.findChildren<QTableWidget*>()[1];
    table->item(1, 1)->setText("4,5");  // comma accepted as decimal separator
    EXPECT_DOUBLE_EQ(dialog.value().gridY[1].coordinate, 4.5);
    table->item(1, 1)->setText("abc");  // unreadable: discarded
    EXPECT_DOUBLE_EQ(dialog.value().gridY[1].coordinate, 4.5);
    EXPECT_EQ(table->item(1, 1)->text(), "4.500");
    table->item(1, 1)->setText("60000");  // beyond +-50000 m
    EXPECT_DOUBLE_EQ(dialog.value().gridY[1].coordinate, 4.5);
    qobject_cast<QComboBox*>(table->cellWidget(1, 2))->setCurrentIndex(1);
    EXPECT_EQ(dialog.value().gridY[1].labelPosition, LabelPosition::End);
    dialog.deleteLine(1, 0);
    EXPECT_EQ(dialog.value().gridY.size(), 1u);
}

TEST(GridGenerationDialog, DefaultsAndRoundTrip) {
    ensureApp();
    GridGenerationDialog dialog{GridSpec{}};
    EXPECT_EQ(dialog.size(), QSize(487, 227));
    const GridSpec v = dialog.value();
    EXPECT_EQ(v.x.count, 4);
    EXPECT_DOUBLE_EQ(v.x.spacing, 3.0);
    EXPECT_EQ(v.x.firstLabel, "A");
    EXPECT_EQ(v.y.firstLabel, "1");
    EXPECT_EQ(v.y.position, LabelPosition::Origin);
}

TEST(LevelsDialog, GenerationAndValidation) {
    ensureApp();
    LevelsDialog dialog{WorkPlanes{}};
    EXPECT_EQ(dialog.size(), QSize(280, 420));
    Recorder rec;
    dialog.prompts = rec.prompts();
    EXPECT_TRUE(dialog.applyGeneration("Nivel", 3, 0.0, 3.0, [] { return true; }));
    ASSERT_EQ(dialog.value().levels.size(), 3u);
    EXPECT_EQ(dialog.value().levels[2].name, "Nivel 2");
    EXPECT_DOUBLE_EQ(dialog.value().levels[2].elevation, 6.0);
    // Existing levels: the question decides.
    EXPECT_FALSE(dialog.applyGeneration("N", 2, 0.0, 1.0, [] { return false; }));
    EXPECT_EQ(dialog.value().levels.size(), 3u);
    // One level only: "Debe definir al menos dos niveles."
    dialog.deleteLevel(0);
    dialog.deleteLevel(0);
    dialog.accept();
    ASSERT_EQ(rec.warnings.size(), 1);
    EXPECT_TRUE(rec.warnings[0].contains("al menos dos niveles"));
    EXPECT_NE(dialog.result(), QDialog::Accepted);
}

TEST(LevelsDialog, NoLevelsIsAcceptableAndNewLevelsStaySortedByElevation) {
    ensureApp();
    LevelsDialog empty{WorkPlanes{}};
    empty.accept();
    EXPECT_EQ(empty.result(), QDialog::Accepted);

    LevelsDialog dialog{WorkPlanes{}};
    dialog.addLevel(Level{"Alto", 6.0});
    dialog.addLevel(Level{"Bajo", 0.0});
    ASSERT_EQ(dialog.value().levels.size(), 2u);
    EXPECT_EQ(dialog.value().levels[0].name, "Bajo");
    auto* table = dialog.findChildren<QTableWidget*>()[0];
    EXPECT_EQ(table->item(1, 1)->text(), "6.000");
    table->item(0, 1)->setText("9");  // the list is re-sorted
    EXPECT_EQ(dialog.value().levels[1].name, "Bajo");
    dialog.accept();
    EXPECT_EQ(dialog.result(), QDialog::Accepted);
}

TEST(NewDialogs, DefaultsOfTheOriginal) {
    ensureApp();
    NewLevelDialog level;
    EXPECT_EQ(level.value().name, "Nuevo nivel");
    EXPECT_DOUBLE_EQ(level.value().elevation, 0.0);
    GenerateLevelsDialog generate;
    EXPECT_EQ(generate.prefix(), "Nivel");
    EXPECT_EQ(generate.count(), 2);
    EXPECT_DOUBLE_EQ(generate.start(), 0.0);
    EXPECT_DOUBLE_EQ(generate.spacing(), 3.0);
    NewGridLineDialog line;
    EXPECT_EQ(line.value().label, "");
    EXPECT_EQ(line.value().labelPosition, LabelPosition::Origin);
}

TEST(PlanesVisibilityDialog, DefaultsToOcultar) {
    ensureApp();
    PlanesVisibilityDialog dialog;
    EXPECT_EQ(dialog.size(), QSize(243, 135));
    EXPECT_FALSE(dialog.showPlanes());
    dialog.setShowPlanes(true);
    EXPECT_TRUE(dialog.showPlanes());
}

int main(int argc, char** argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

TEST(GridDialog, TypedCoordinatesAreRoundedToThreeDecimals) {
    ensureApp();
    WorkPlanes planes;
    planes.gridX = {{"A", 0.0, LabelPosition::Origin}};
    GridDialog dialog{planes};
    QTableWidget* table = dialog.findChildren<QTableWidget*>().at(0);
    table->item(0, 1)->setText("3.0004");
    EXPECT_DOUBLE_EQ(dialog.value().gridX[0].coordinate, 3.0);
    table->item(0, 1)->setText("3,0009");
    EXPECT_DOUBLE_EQ(dialog.value().gridX[0].coordinate, 3.001);
    EXPECT_EQ(table->item(0, 1)->text(), "3.001");
    NewGridLineDialog line;
    EXPECT_DOUBLE_EQ(line.value().coordinate, 0.0);
}

TEST(GridDialog, AcceptUsesTheMeasuredMessagesAndTolerance) {
    ensureApp();
    Recorder rec;
    GridDialog dialog{WorkPlanes{}};
    dialog.prompts = rec.prompts();
    dialog.addLine(0, GridLine{"A", 3.0, LabelPosition::Origin});
    dialog.addLine(0, GridLine{"E", 3.001, LabelPosition::Origin});
    dialog.accept();
    ASSERT_EQ(rec.warnings.size(), 1);
    EXPECT_EQ(rec.warnings[0], "Las líneas 'A' y 'E' están situadas en la misma coordenada.");
    EXPECT_NE(dialog.result(), QDialog::Accepted);
    dialog.deleteLine(0, 1);
    dialog.addLine(0, GridLine{"E", 3.002, LabelPosition::Origin});
    dialog.accept();
    EXPECT_EQ(rec.warnings.size(), 1) << "3.002 is accepted";
    EXPECT_EQ(dialog.result(), QDialog::Accepted);
}

TEST(ReferencesDialog, DefaultsDependencyAndValue) {
    ensureApp();
    ReferencesDialog dialog{ReferenceOptions{}};
    EXPECT_EQ(dialog.size(), QSize(246, 262));
    const auto boxes = dialog.findChildren<QCheckBox*>();
    ASSERT_EQ(boxes.size(), 7);
    const auto box = [&](const QString& text) {
        for (QCheckBox* b : boxes)
            if (b->text() == text) return b;
        ADD_FAILURE() << "missing box " << text.toStdString();
        return boxes.first();
    };
    EXPECT_TRUE(box("Mostrar la rejilla")->isChecked());
    EXPECT_TRUE(box("Líneas de referencia")->isChecked());
    EXPECT_FALSE(box("Sólo las que tienen nudo")->isChecked());
    EXPECT_TRUE(box("Paralelas a X")->isChecked());
    EXPECT_TRUE(box("Paralelas a Y")->isChecked());
    EXPECT_TRUE(box("Paralelas a Z")->isChecked());
    EXPECT_FALSE(box("Dibujar con color según dirección")->isChecked());
    EXPECT_EQ(dialog.value(), ReferenceOptions{});
    box("Dibujar con color según dirección")->setChecked(true);
    box("Líneas de referencia")->setChecked(false);  // greys the five boxes below
    for (const char* name : {"Sólo las que tienen nudo", "Paralelas a X", "Paralelas a Y", "Paralelas a Z", "Dibujar con color según dirección"})
        EXPECT_FALSE(box(name)->isEnabled()) << name;
    EXPECT_TRUE(box("Mostrar la rejilla")->isEnabled());
    box("Líneas de referencia")->setChecked(true);
    EXPECT_TRUE(box("Paralelas a X")->isEnabled());
    const ReferenceOptions v = dialog.value();
    EXPECT_TRUE(v.colourByDirection);
    EXPECT_TRUE(v.referenceLines);
}

TEST(DimensionsBar, FourButtonsWithTheTextsOfTheOriginal) {
    ensureApp();
    DimensionsBar bar;
    EXPECT_EQ(bar.findChildren<QToolButton*>().size(), 5);  // four commands and the close button
    using C = DimensionsBar::Command;
    EXPECT_EQ(DimensionsBar::title(C::Add), "Añadir");
    EXPECT_EQ(DimensionsBar::title(C::Delete), "Borrar");
    EXPECT_EQ(DimensionsBar::title(C::Show), "Mostrar");
    EXPECT_EQ(DimensionsBar::title(C::Hide), "Ocultar");
    EXPECT_TRUE(DimensionsBar::tooltipBody(C::Delete).isEmpty());
    EXPECT_EQ(DimensionsBar::tooltipBody(C::Hide), "Selección de cotas para ocultarlas en planos.");
    EXPECT_EQ(DimensionsBar::statusText(C::Add),
              "Cotas - Añadir | - LMB: Seleccione las dos líneas de referencia o planos entre los que se define la cota que desea introducir. - RMB: Comenzar otra vez.");
    EXPECT_EQ(DimensionsBar::statusText(C::Delete), "Cotas - Borrar | - LMB: Seleccione un grupo de elementos. - RMB: Actuar sobre los elementos seleccionados.");
    EXPECT_EQ(bar.button(C::Show)->statusTip(), DimensionsBar::statusText(C::Show));
    int chosen = -1;
    QObject::connect(&bar, &DimensionsBar::commandChosen, [&](DimensionsBar::Command c) { chosen = static_cast<int>(c); });
    bar.button(C::Hide)->click();
    EXPECT_EQ(chosen, static_cast<int>(C::Hide));
}
