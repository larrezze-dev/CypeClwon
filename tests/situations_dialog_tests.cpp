// Tests for the "Situaciones" viewer, run on Qt's offscreen platform.
#include <gtest/gtest.h>

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QPushButton>
#include <QTemporaryDir>

#include "SituationsDialog.h"
#include "core/combination_export.h"

using namespace cc::app;
using namespace cc::core;

namespace {
void ensureApp() {
    static int argc = 1;
    static char name[] = "situations_dialog_tests";
    static char* argv[] = {name, nullptr};
    static QApplication instance(argc, argv);
}
}  // namespace

TEST(SituationsDialog, FixedSizeAndPageNavigationIsClamped) {
    ensureApp();
    SituationsDialog dialog(buildGroundReport(false));
    EXPECT_EQ(dialog.size(), QSize(778, 268));
    EXPECT_EQ(dialog.pageCount(), 5);
    dialog.goToPage(99);
    EXPECT_EQ(dialog.currentPage(), 5);
    dialog.goToPage(-3);
    EXPECT_EQ(dialog.currentPage(), 1);
}

TEST(SituationsDialog, ShareIsDisabled) {
    ensureApp();
    SituationsDialog dialog(buildGroundReport(false));
    bool found = false;
    for (QPushButton* b : dialog.findChildren<QPushButton*>())
        if (b->text() == "Compartir") {
            found = true;
            EXPECT_FALSE(b->isEnabled());
        }
    EXPECT_TRUE(found);
}

TEST(SituationsDialog, ExportsEveryFormat) {
    ensureApp();
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const auto report = buildColdFormedTimberReport(false);
    SituationsDialog dialog(report);
    for (ExportFormat f : {ExportFormat::Pdf, ExportFormat::Docx, ExportFormat::Text, ExportFormat::Html, ExportFormat::Rtf}) {
        const QString path = dir.filePath("sub/Situaciones." + SituationsDialog::extensionOf(f));
        ASSERT_TRUE(dialog.exportTo(f, path)) << path.toStdString();
        EXPECT_GT(QFile(path).size(), 100) << path.toStdString();
    }
    QFile pdf(dir.filePath("sub/Situaciones.pdf"));
    ASSERT_TRUE(pdf.open(QIODevice::ReadOnly));
    EXPECT_EQ(pdf.read(5), QByteArray("%PDF-"));
}

TEST(SituationsDialog, TextExportHonoursTheGoldenBytes) {
    ensureApp();
    QTemporaryDir dir;
    SituationsDialog dialog(buildGroundReport(true));
    const QString path = dir.filePath("s.txt");
    ASSERT_TRUE(dialog.exportTo(ExportFormat::Text, path, 120));
    QFile f(path);
    ASSERT_TRUE(f.open(QIODevice::ReadOnly));
    const QByteArray bytes = f.readAll();
    EXPECT_EQ(bytes.left(2), QByteArray("\xFF\xFE", 2));  // UTF-16 LE byte-order mark
    EXPECT_EQ(bytes.toStdString(), exportTextUtf16(buildGroundReport(true)));
}

TEST(SituationsDialog, RejectsUnwritablePath) {
    ensureApp();
    SituationsDialog dialog(buildGroundReport(false));
    QTemporaryDir dir;
    QFile blocker(dir.filePath("file"));
    ASSERT_TRUE(blocker.open(QIODevice::WriteOnly));
    blocker.close();
    // "file" is a regular file, so it cannot act as a directory.
    EXPECT_FALSE(dialog.exportTo(ExportFormat::Html, dir.filePath("file/out.html")));
}

int main(int argc, char** argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
