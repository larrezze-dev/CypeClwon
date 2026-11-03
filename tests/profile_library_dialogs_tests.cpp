// Obra > Biblioteca de perfiles (T-P5-05, GUI part). Never shown on screen (offscreen).
#include <gtest/gtest.h>

#include <QApplication>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QToolButton>
#include <filesystem>

#include "ProfileLibraryDialogs.h"
#include "core/predefined_profiles.h"
#include "core/profile_library.h"
#include "core/project_file.h"

using namespace cc::app;
using namespace cc::core;

namespace {
void ensureApp() {
    static int argc = 1;
    static char name[] = "profile_library_dialogs_tests";
    static char* argv[] = {name, nullptr};
    static QApplication instance(argc, argv);
}

struct Sink {
    QStringList warnings;
    bool keepExisting = true;
    int asked = 0;
    QString openPath, savePath;
    Sink() {
        ensureApp();
        profileMessageSink().warn = [this](QWidget*, const QString&, const QString& text) { warnings << text; };
        profileMessageSink().ask = [this](QWidget*, const QString&, const QString&, const QString&, const QString&) {
            ++asked;
            return keepExisting;
        };
        profileFileSink().openFile = [this](QWidget*) { return openPath; };
        profileFileSink().saveFile = [this](QWidget*) { return savePath; };
    }
};

Section ipe(const std::string& ref, double h) {
    Section s;
    s.reference = ref;
    s.kind = ShapeKind::IParallel;
    s.dims = {h / 1000.0, 0.1, 0.006, 0.009, 0.01};
    return s;
}
ProfileSeries series(const std::string& ref) {
    ProfileSeries s;
    s.reference = ref;
    s.description = "Prueba";
    s.profiles = {ipe(ref + " 200", 200), ipe(ref + " 240", 240)};
    return s;
}
}  // namespace

TEST(ProfileLibraryDialog, HasTheDocumentedButtonsAndTitle) {
    Sink sink;
    ProfileLibraryDialog dialog{ProfileLibrary{}};
    EXPECT_EQ(dialog.windowTitle(), "Gestión de la biblioteca de perfiles");
    EXPECT_EQ(dialog.button("predefined")->text(), "Importación de series de perfiles predefinidas");
    EXPECT_EQ(dialog.button("catalogues")->text(), "Importar catálogos de fabricante");
    EXPECT_EQ(dialog.button("initial")->text(), "Importación inicial de series predefinidas de perfiles");
    EXPECT_EQ(dialog.button("export")->text(), "Exportar series de perfiles");
    EXPECT_EQ(dialog.size().width(), 328);
    EXPECT_EQ(dialog.acceptButton()->text(), "Aceptar");
}

TEST(ProfileLibraryDialog, CataloguesNeedTheNetwork) {
    Sink sink;
    ProfileLibraryDialog dialog{ProfileLibrary{}};
    dialog.openManufacturerCatalogues();
    ASSERT_EQ(sink.warnings.size(), 1);
    EXPECT_TRUE(sink.warnings[0].contains("Internet"));
}

TEST(PredefinedImportDialog, ListsTheOriginalRowsAndEnablesOnlyWhatTheCoreOffers) {
    Sink sink;
    LibraryScratch scratch{ProfileLibrary{}};
    PredefinedImportDialog dialog(scratch);
    EXPECT_EQ(dialog.size(), QSize(312, 555));
    EXPECT_EQ(dialog.table()->rowCount(), 23);
    EXPECT_EQ(dialog.table()->horizontalHeaderItem(1)->text(), "Nombre Biblioteca");
    int enabled = 0;
    for (int r = 0; r < dialog.table()->rowCount(); ++r) {
        EXPECT_EQ(dialog.table()->item(r, 0)->checkState(), Qt::Unchecked);
        if (dialog.table()->item(r, 0)->flags() & Qt::ItemIsUserCheckable) {
            ++enabled;
            EXPECT_EQ(dialog.table()->item(r, 1)->text(), "Gama de perfiles europeos");
        }
    }
    EXPECT_EQ(enabled, 1);
}

TEST(PredefinedImportDialog, ImportsAsOneUndoableStepAndAsksOnConflict) {
    Sink sink;
    LibraryScratch scratch{ProfileLibrary{}};
    PredefinedImportDialog dialog(scratch);
    EXPECT_FALSE(dialog.importChecked());  // nothing checked: #33818
    ASSERT_EQ(sink.warnings.size(), 1);
    EXPECT_EQ(sink.warnings[0], "Debe seleccionar, al menos, una serie de perfiles.");
    dialog.table()->item(1, 0)->setCheckState(Qt::Checked);
    EXPECT_TRUE(dialog.importChecked());
    EXPECT_EQ(scratch.library().series.size(), predefinedSeries().size());
    EXPECT_EQ(scratch.undo.count(), 1u);  // one macro
    scratch.undo.undo();
    EXPECT_TRUE(scratch.library().series.empty());
    scratch.undo.redo();
    // Importing again: every series exists; keep them all.
    EXPECT_TRUE(dialog.importChecked());
    EXPECT_EQ(sink.asked, static_cast<int>(predefinedSeries().size()));
    EXPECT_EQ(scratch.library().series.size(), predefinedSeries().size());
}

TEST(SeriesEditDialog, EmptySeriesIsRefusedWithTheOriginalMessage) {
    Sink sink;
    SeriesEditDialog dialog(ProfileSeries{}, true);
    EXPECT_EQ(dialog.windowTitle(), "Nueva serie");
    EXPECT_EQ(dialog.size(), QSize(330, 421));
    dialog.referenceEdit()->setText("MIA");
    dialog.accept();
    EXPECT_EQ(dialog.result(), QDialog::Rejected);
    ASSERT_EQ(sink.warnings.size(), 1);
    EXPECT_EQ(sink.warnings[0], "Debe introducir, al menos, un perfil en la serie.");
}

TEST(SeriesEditDialog, ProfileOperationsAndAccept) {
    Sink sink;
    SeriesEditDialog dialog(ProfileSeries{}, true);
    dialog.referenceEdit()->setText("MIA");
    dialog.descriptionEdit()->setPlainText("Mi serie");
    dialog.addProfile(ipe("A", 100));
    dialog.addProfile(ipe("B", 120));
    dialog.duplicateProfile(0);
    ASSERT_EQ(dialog.table()->rowCount(), 3);
    EXPECT_EQ(dialog.table()->item(1, 0)->text(), "A (copia)");
    dialog.moveProfile(1, +1);
    EXPECT_EQ(dialog.table()->item(2, 0)->text(), "A (copia)");
    dialog.moveProfile(0, -1);  // at the top: nothing happens
    EXPECT_EQ(dialog.table()->item(0, 0)->text(), "A");
    dialog.removeProfile(2);
    EXPECT_EQ(dialog.table()->rowCount(), 2);
    EXPECT_FALSE(dialog.shapeCombo()->isEnabled());  // shape fixed once there are profiles
    dialog.accept();
    EXPECT_EQ(dialog.result(), QDialog::Accepted);
    EXPECT_EQ(dialog.value().reference, "MIA");
    EXPECT_EQ(dialog.value().description, "Mi serie");
    EXPECT_EQ(dialog.value().profiles.size(), 2u);
    EXPECT_TRUE(sink.warnings.isEmpty());
}

TEST(SeriesEditDialog, RepeatedProfileIsRefused) {
    Sink sink;
    SeriesEditDialog dialog(ProfileSeries{}, true);
    dialog.referenceEdit()->setText("MIA");
    dialog.addProfile(ipe("A", 100));
    dialog.addProfile(ipe("A", 120));
    dialog.accept();
    EXPECT_EQ(dialog.result(), QDialog::Rejected);
    ASSERT_EQ(sink.warnings.size(), 1);
    EXPECT_TRUE(sink.warnings[0].contains("repetido"));
}

TEST(ProfileEditDialog, ShowsCalculatedPropertiesAndValidates) {
    Sink sink;
    ProfileEditDialog dialog(ipe("IPE 200", 200), false);
    EXPECT_TRUE(dialog.propertiesLabel()->text().contains("A = "));
    EXPECT_TRUE(dialog.propertiesLabel()->text().contains("Iy = "));
    const QString before = dialog.propertiesLabel()->text();
    dialog.dimensionSpin(0)->setValue(300.0);  // h in mm
    EXPECT_NE(dialog.propertiesLabel()->text(), before);
    EXPECT_NEAR(dialog.value().dims[0], 0.3, 1e-9);
    dialog.referenceEdit()->setText("");
    dialog.accept();
    EXPECT_EQ(dialog.result(), QDialog::Rejected);
    ASSERT_EQ(sink.warnings.size(), 1);
    EXPECT_EQ(sink.warnings[0], "Debe introducir una referencia para el perfil.");
    dialog.referenceEdit()->setText("IPE 300");
    dialog.dimensionSpin(1)->setValue(0.0);  // zero flange width is invalid
    dialog.accept();
    EXPECT_EQ(dialog.result(), QDialog::Rejected);
    EXPECT_EQ(sink.warnings.size(), 2);
}

TEST(SeriesManagerDialog, ManagesSeriesOnTheScratchAndCancelRestores) {
    Sink sink;
    LibraryScratch scratch{ProfileLibrary{}};
    SeriesManagerDialog dialog(scratch);
    EXPECT_EQ(dialog.size(), QSize(530, 520));
    EXPECT_EQ(dialog.table()->horizontalHeaderItem(0)->text(), "Referencia");
    EXPECT_EQ(dialog.table()->horizontalHeaderItem(1)->text(), "Catálogo");
    EXPECT_EQ(dialog.table()->horizontalHeaderItem(2)->text(), "Descripción");
    EXPECT_TRUE(dialog.newSeries(series("A")));
    EXPECT_TRUE(dialog.newSeries(series("B")));
    EXPECT_FALSE(dialog.newSeries(series("A")));  // repeated: refused and reported
    EXPECT_EQ(sink.warnings.size(), 1);
    EXPECT_TRUE(dialog.duplicateSeries(0));
    ASSERT_EQ(dialog.table()->rowCount(), 3);
    EXPECT_EQ(dialog.table()->item(2, 0)->text(), "A (copia)");
    EXPECT_TRUE(dialog.moveSeries(2, -1));
    EXPECT_EQ(dialog.table()->item(1, 0)->text(), "A (copia)");
    EXPECT_FALSE(dialog.moveSeries(0, -1));
    EXPECT_TRUE(dialog.removeSeries(1));
    EXPECT_EQ(dialog.table()->rowCount(), 2);
    ProfileSeries edited = scratch.library().series[0];
    edited.description = "Otra";
    EXPECT_TRUE(dialog.editSeries(0, edited));
    EXPECT_EQ(dialog.table()->item(0, 2)->text(), "Otra");
    dialog.reject();
    EXPECT_TRUE(scratch.library().series.empty());
}

TEST(SeriesManagerDialog, ExportThenImportThroughAFile) {
    Sink sink;
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const QString path = dir.filePath("series.json");
    LibraryScratch source{ProfileLibrary{}};
    SeriesManagerDialog first(source);
    first.newSeries(series("A"));
    first.newSeries(series("B"));
    first.table()->clearSelection();
    EXPECT_TRUE(first.exportFile(path));  // nothing selected: every series
    LibraryScratch target{ProfileLibrary{}};
    SeriesManagerDialog second(target);
    EXPECT_TRUE(second.importFile(path));
    EXPECT_EQ(target.library().series.size(), 2u);
    EXPECT_EQ(target.undo.count(), 1u);
    // Overwrite question (#138928) and conflict question (#33822).
    sink.keepExisting = false;  // "No": do not overwrite
    EXPECT_FALSE(first.exportFile(path));
    sink.keepExisting = true;
    EXPECT_TRUE(second.importFile(path));
    EXPECT_EQ(sink.asked, 3);  // overwrite + two existing series
    EXPECT_EQ(target.library().series.size(), 2u);
    EXPECT_FALSE(second.importFile(dir.filePath("missing.json")));
    EXPECT_TRUE(sink.warnings.last().contains("no existe"));
}

TEST(ProfileLibraryDialog, LibrarySurvivesSaveAndOpen) {
    Sink sink;
    ProfileLibrary lib;
    lib.series.push_back(series("MIA"));
    ProfileLibraryDialog dialog(lib);
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    Project project;
    project.profileLibrary = dialog.library();
    ProjectDocument doc = makeDocument(project, "test");
    const std::filesystem::path path = std::filesystem::path(dir.filePath("obra.ed3").toStdString());
    ASSERT_TRUE(saveProjectFile(path, doc).ok);
    const LoadResult loaded = loadProjectFile(path);
    ASSERT_TRUE(loaded.ok) << loaded.error;
    EXPECT_TRUE(loaded.document.project.profileLibrary == lib);
}


// Visual comparison aid: CC_SNAPSHOT_DIR=<dir> writes PNGs of the dialogs (offscreen) to compare with orig_biblioteca_perfiles_*.
TEST(ProfileLibraryDialog, SnapshotsForVisualComparison) {
    const QByteArray dir = qgetenv("CC_SNAPSHOT_DIR");
    if (dir.isEmpty()) GTEST_SKIP() << "CC_SNAPSHOT_DIR not set";
    Sink sink;
    LibraryScratch scratch{ProfileLibrary{}};
    for (const ProfileSeries& s : predefinedSeries()) scratch.project.profileLibrary.series.push_back(s);
    const QString base = QString::fromLocal8Bit(dir);
    ProfileLibraryDialog a{ProfileLibrary{}};
    a.grab().save(base + "/clone_biblioteca_perfiles_01.png");
    PredefinedImportDialog b(scratch);
    b.grab().save(base + "/clone_biblioteca_perfiles_02.png");
    SeriesManagerDialog c(scratch);
    c.grab().save(base + "/clone_series_perfiles.png");
    SeriesEditDialog d(ProfileSeries{}, true);
    d.grab().save(base + "/clone_series_nueva.png");
    ProfileEditDialog e(scratch.library().series[0].profiles[0], false);
    e.grab().save(base + "/clone_perfil_editar.png");
}

int main(int argc, char** argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
