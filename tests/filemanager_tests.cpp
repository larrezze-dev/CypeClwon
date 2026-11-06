// Tests for the "Gestión archivos" dialog, run on Qt's offscreen platform.
#include <gtest/gtest.h>

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QToolButton>

#include <QCheckBox>
#include <QComboBox>
#include <QFileInfo>
#include <QTimer>
#include <QTreeWidget>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>

#include "FileManagerDialog.h"
#include "FileOpsDialogs.h"
#include "core/project_file.h"

using namespace cc::app;

namespace {
void ensureApp() {
    static int argc = 1;
    static char name[] = "filemanager_tests";
    static char* argv[] = {name, nullptr};
    static QApplication instance(argc, argv);
}

QToolButton* barButton(QWidget* parent, const QString& text) {
    for (QToolButton* b : parent->findChildren<QToolButton*>()) {
        if (b->text() == text) return b;
    }
    return nullptr;
}

void touch(const QString& path) {
    QFile f(path);
    ASSERT_TRUE(f.open(QIODevice::WriteOnly));
    f.write("x");
}
}  // namespace

TEST(FileManagerDialog, ListsOnlyProjectsOfTheFolder) {
    ensureApp();
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    touch(dir.filePath("beta.ed3"));
    touch(dir.filePath("alpha.ed3"));
    touch(dir.filePath("notes.txt"));
    FileManagerDialog dialog(dir.path());
    EXPECT_EQ(dialog.size(), QSize(676, 525));
    ASSERT_EQ(dialog.table()->rowCount(), 2);
    EXPECT_EQ(dialog.table()->item(0, 0)->text(), "alpha");
    EXPECT_EQ(dialog.table()->item(1, 0)->text(), "beta");
    EXPECT_EQ(dialog.table()->columnCount(), 4);
    EXPECT_EQ(dialog.table()->horizontalHeaderItem(0)->text(), "Obra");
    EXPECT_EQ(dialog.table()->horizontalHeaderItem(3)->text(), "Fecha");
    EXPECT_EQ(dialog.table()->item(0, 3)->text().size(), 10);  // dd/MM/yyyy
}

TEST(FileManagerDialog, ShowsDescriptionAndVersionFromTheManifest) {
    ensureApp();
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    cc::core::ProjectDocument doc = cc::core::makeDocument(cc::core::Project{}, "Nave de prueba");
    ASSERT_TRUE(cc::core::saveProjectFile(std::filesystem::path(dir.filePath("real.ed3").toStdWString()), doc).ok);
    touch(dir.filePath("zz_foreign.ed3"));  // not one of our files: listed, with empty columns
    FileManagerDialog dialog(dir.path());
    ASSERT_EQ(dialog.table()->rowCount(), 2);
    EXPECT_EQ(dialog.table()->item(0, 1)->text(), "Nave de prueba");
    EXPECT_EQ(dialog.table()->item(0, 2)->text(), QString::fromStdString(doc.manifest.appVersion));
    EXPECT_FALSE(dialog.table()->item(0, 2)->text().isEmpty());
    EXPECT_TRUE(dialog.table()->item(1, 1)->text().isEmpty());
    EXPECT_TRUE(dialog.table()->item(1, 2)->text().isEmpty());
}

TEST(FileManagerDialog, OpenEmitsSelectedPathAndPendingButtonsAreDisabled) {
    ensureApp();
    QTemporaryDir dir;
    touch(dir.filePath("one.ed3"));
    FileManagerDialog dialog(dir.path());
    QSignalSpy spy(&dialog, &FileManagerDialog::openRequested);
    barButton(&dialog, "Abrir")->click();  // nothing selected yet
    EXPECT_EQ(spy.count(), 0);
    dialog.table()->selectRow(0);
    barButton(&dialog, "Abrir")->click();
    ASSERT_EQ(spy.count(), 1);
    EXPECT_TRUE(spy.at(0).at(0).toString().endsWith("one.ed3"));
    for (const char* name : {"Buscar", "Compartir", "Ejemplos"}) {  // Buscar needs more than one project
        EXPECT_FALSE(barButton(&dialog, name)->isEnabled()) << name;
    }
    for (const char* name : {"Copiar", "Borrar", "Comprimir", "Descomprimir"}) {
        EXPECT_TRUE(barButton(&dialog, name)->isEnabled()) << name;
    }
    QSignalSpy newSpy(&dialog, &FileManagerDialog::newRequested);
    barButton(&dialog, "Nuevo")->click();
    EXPECT_EQ(newSpy.count(), 1);
}

TEST(FileOpsDialogs, DeleteConfirmationDefaultsToIncludingBackups) {
    ensureApp();
    DeleteProjectDialog dialog;
    EXPECT_TRUE(dialog.includeBackups());
    EXPECT_EQ(dialog.acceptButton()->text(), "Sí");
    EXPECT_EQ(dialog.cancelButton()->text(), "No");
}

TEST(FileOpsDialogs, CompressProposesCypNextToTheProjectAndReportsKBytes) {
    ensureApp();
    QTemporaryDir dir;
    QFile f(dir.filePath("nuevo2.ed3"));
    ASSERT_TRUE(f.open(QIODevice::WriteOnly));
    f.write(QByteArray(2048, 'x'));
    f.close();
    QFile bak(dir.filePath("nuevo2.ed$"));
    ASSERT_TRUE(bak.open(QIODevice::WriteOnly));
    bak.write(QByteArray(1024, 'x'));
    bak.close();
    CompressDialog dialog(dir.filePath("nuevo2.ed3"));
    EXPECT_TRUE(dialog.archivePath().endsWith("/nuevo2.cyp"));
    EXPECT_FALSE(dialog.includeBackups());
    const auto labels = dialog.findChildren<QLabel*>();
    auto sizeText = [&] {
        for (QLabel* l : labels) if (l->text().contains("KBytes")) return l->text();
        return QString();
    };
    EXPECT_TRUE(sizeText().contains("2.00"));
    dialog.findChild<QCheckBox*>()->setChecked(true);
    EXPECT_TRUE(sizeText().contains("3.00"));
    EXPECT_EQ(CompressDialog::kilobytesText(824135), "804.82");  // figure shown by the original for nuevo2
}

TEST(FileOpsDialogs, RecentFilesDialogChoosesTheSelectedEntry) {
    ensureApp();
    RecentFilesDialog dialog({"C:/a/one.ed3", "C:/a/two.ed3"});
    ASSERT_EQ(dialog.list()->count(), 2);
    dialog.list()->setCurrentRow(1);
    dialog.acceptButton()->click();
    EXPECT_EQ(dialog.chosenFile(), "C:/a/two.ed3");
}

TEST(FileManagerDialog, BuscarNeedsMoreThanOneProject) {
    ensureApp();
    QTemporaryDir dir;
    touch(dir.filePath("alpha.ed3"));
    FileManagerDialog single(dir.path());
    EXPECT_FALSE(barButton(&single, "Buscar")->isEnabled());
    cc::core::ProjectDocument doc = cc::core::makeDocument(cc::core::Project{}, "Cimentación profunda");
    ASSERT_TRUE(cc::core::saveProjectFile(std::filesystem::path(dir.filePath("beta.ed3").toStdWString()), doc).ok);
    FileManagerDialog dialog(dir.path());
    EXPECT_TRUE(barButton(&dialog, "Buscar")->isEnabled());
    EXPECT_FALSE(barButton(&dialog, "Compartir")->isEnabled());
    EXPECT_FALSE(barButton(&dialog, "Compartir")->toolTip().isEmpty());
}

TEST(ThesaurusDialog, WordsAreFileNamesAndDescriptionWordsInLowerCaseSorted) {
    ensureApp();
    // Measured in the original (dialogs_gestion_archivos.md, 2026-11-06).
    const QList<SearchEntry> entries = {{"alfa_puente", "Puente metalico de prueba", "C:/o/alfa_puente.ed3"},
                                        {"beta_cubierta", "Cubierta de madera - prueba piloto", "C:/o/beta_cubierta.ed3"}};
    EXPECT_EQ(ProjectThesaurusDialog::thesaurusWords(entries),
              (QStringList{"-", "alfa_puente", "beta_cubierta", "cubierta", "de", "madera", "metalico", "piloto", "prueba", "puente"}));
    ProjectThesaurusDialog dialog(entries);
    EXPECT_EQ(dialog.windowTitle(), "Localización de obra por tesauro");
    EXPECT_TRUE(dialog.combo()->isEditable());
    EXPECT_EQ(dialog.combo()->currentText(), "");
    EXPECT_EQ(dialog.wordList()->count(), 10);
    EXPECT_EQ(dialog.projectList()->headerItem()->text(0), "Obra");
    EXPECT_EQ(dialog.projectList()->headerItem()->text(1), "Descripción");
    EXPECT_EQ(dialog.previewPanel()->findChildren<QWidget*>().size(), 0);  // no preview data: empty panel
}

TEST(ThesaurusDialog, WordFiltersProjectsAndTypingIgnoresCase) {
    ensureApp();
    const QList<SearchEntry> entries = {{"alfa_puente", "Puente metalico de prueba", "C:/o/alfa_puente.ed3"},
                                        {"beta_cubierta", "Cubierta de madera - prueba piloto", "C:/o/beta_cubierta.ed3"}};
    ProjectThesaurusDialog dialog(entries);
    dialog.wordList()->setCurrentRow(dialog.wordList()->row(dialog.wordList()->findItems("metalico", Qt::MatchExactly).first()));
    ASSERT_EQ(dialog.projectList()->topLevelItemCount(), 1);
    EXPECT_EQ(dialog.projectList()->topLevelItem(0)->text(0), "alfa_puente");
    EXPECT_EQ(dialog.combo()->currentText(), "metalico");  // the combo follows the chosen word
    dialog.combo()->setEditText("Prueba");  // typed with a capital: selects "prueba"
    EXPECT_EQ(dialog.wordList()->currentItem()->text(), "prueba");
    EXPECT_EQ(dialog.projectList()->topLevelItemCount(), 2);
    dialog.combo()->setEditText("-");
    ASSERT_EQ(dialog.projectList()->topLevelItemCount(), 1);
    EXPECT_EQ(dialog.projectList()->topLevelItem(0)->text(0), "beta_cubierta");
    dialog.combo()->setEditText("zzz");
    EXPECT_EQ(dialog.projectList()->topLevelItemCount(), 0);
    EXPECT_FALSE(dialog.acceptButton()->isEnabled());
}

TEST(ThesaurusDialog, AcceptChoosesTheProjectOfTheSelectedRow) {
    ensureApp();
    const QList<SearchEntry> entries = {{"alfa_puente", "Puente metalico de prueba", "C:/o/alfa_puente.ed3"},
                                        {"beta_cubierta", "Cubierta de madera - prueba piloto", "C:/o/beta_cubierta.ed3"}};
    ProjectThesaurusDialog dialog(entries);
    dialog.combo()->setEditText("prueba");
    dialog.projectList()->setCurrentItem(dialog.projectList()->topLevelItem(1));
    dialog.acceptButton()->click();
    EXPECT_EQ(dialog.result(), QDialog::Accepted);
    EXPECT_EQ(dialog.chosenIndex(), 1);
}

TEST(FileManagerDialog, ThesaurusAcceptOpensTheProject) {
    ensureApp();
    QTemporaryDir dir;
    touch(dir.filePath("alpha.ed3"));
    cc::core::ProjectDocument doc = cc::core::makeDocument(cc::core::Project{}, "Cimentación profunda");
    ASSERT_TRUE(cc::core::saveProjectFile(std::filesystem::path(dir.filePath("beta.ed3").toStdWString()), doc).ok);
    FileManagerDialog dialog(dir.path());
    QString opened;
    QObject::connect(&dialog, &FileManagerDialog::openRequested, [&](const QString& path) { opened = path; });
    QTimer::singleShot(50, [] {
        for (QWidget* w : QApplication::topLevelWidgets())
            if (auto* d = qobject_cast<ProjectThesaurusDialog*>(w)) {
                d->combo()->setEditText("cimentación");
                d->acceptButton()->click();
                return;
            }
    });
    QTimer::singleShot(5000, [] {  // failsafe: never hang the suite
        for (QWidget* w : QApplication::topLevelWidgets())
            if (auto* d = qobject_cast<ProjectThesaurusDialog*>(w)) d->reject();
    });
    barButton(&dialog, "Buscar")->click();
    EXPECT_EQ(QFileInfo(opened).fileName(), "beta.ed3");
}

TEST(FileManagerDialog, CompressionMessagesUseTheOriginalTexts) {
    ensureApp();
    const QString archive = QDir::toNativeSeparators("C:/obras/nuevo2.cyp");
    EXPECT_EQ(FileManagerDialog::compressedMessage("C:/obras/nuevo2.cyp"),
              "Se ha comprimido la obra en el fichero '" + archive + "'.");
    EXPECT_EQ(FileManagerDialog::decompressConfirmationMessage("C:/obras"),
              "¿Desea descomprimir la obra en '" + QDir::toNativeSeparators("C:/obras") + "'?");
    const QString error = FileManagerDialog::decompressErrorMessage("C:/obras/nuevo2.cyp", "fichero dañado");
    EXPECT_TRUE(error.startsWith("No se ha podido descomprimir el fichero '" + archive + "' por el siguiente motivo:"));
    EXPECT_TRUE(error.endsWith("fichero dañado"));
}

int main(int argc, char** argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
