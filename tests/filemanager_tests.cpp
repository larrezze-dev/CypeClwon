// Tests for the "Gestión archivos" dialog, run on Qt's offscreen platform.
#include <gtest/gtest.h>

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QToolButton>

#include "FileManagerDialog.h"

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

TEST(FileManagerDialog, OpenEmitsSelectedPathAndOtherButtonsAreDisabled) {
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
    for (const char* name : {"Copiar", "Borrar", "Buscar", "Comprimir", "Descomprimir", "Compartir", "Ejemplos"}) {
        EXPECT_FALSE(barButton(&dialog, name)->isEnabled()) << name;
    }
    QSignalSpy newSpy(&dialog, &FileManagerDialog::newRequested);
    barButton(&dialog, "Nuevo")->click();
    EXPECT_EQ(newSpy.count(), 1);
}

int main(int argc, char** argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
