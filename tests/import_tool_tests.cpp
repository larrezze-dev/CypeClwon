// Geometria > Importar (T-P5-11, GUI part): ImportTool, the entity selection dialog, the insertion dialog and the reading log.
// Never shown on screen (offscreen). Uses the sample files of docs/sample_project.
#include <gtest/gtest.h>

#include <QAction>
#include <QApplication>
#include <QDialog>
#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QTableWidget>
#include <filesystem>
#include <fstream>

#include "ImportDialogs.h"
#include "ImportTool.h"

using cc::app::DxfSelectionDialog;
using cc::app::ImportInsertionDialog;
using cc::app::ImportLogDialog;
using cc::app::ImportTool;
using namespace cc::core;

namespace {
QApplication* app() {
    static int argc = 1;
    static char name[] = "import_tool_tests";
    static char* argv[] = {name, nullptr};
    static QApplication instance(argc, argv);
    return &instance;
}

QString sample(const char* file) { return QString::fromStdString((std::filesystem::path(CC_SOURCE_DIR) / "docs" / "sample_project" / file).string()); }

struct AppGuard {
    AppGuard() { app(); }  // the QApplication must exist before the QActions of the tool
};

class ImportToolTest : public AppGuard, public ::testing::Test {
protected:
    void SetUp() override {
        app();
        tool.setProject(&project, &stack);
        tool.setDialogRunner([this](QDialog& d) {
            ++dialogs;
            if (auto* sel = dynamic_cast<DxfSelectionDialog*>(&d)) {
                lastSelection = sel->selectedLayers();
                if (onSelection) onSelection(*sel);
                return answer ? 1 : 0;
            }
            if (auto* where = dynamic_cast<ImportInsertionDialog*>(&d)) {
                if (onInsertion) onInsertion(*where);
                return answer ? 1 : 0;
            }
            if (auto* log = dynamic_cast<ImportLogDialog*>(&d)) {
                lastLog = log->logText();
                lastLogSuccess = log->success();
                return answer ? 1 : 0;
            }
            return 0;
        });
        QObject::connect(&tool, &ImportTool::messageRequested, [this](const QString& title, const QString& text, bool error) {
            messages.push_back({title, text, error});
        });
    }
    struct Msg { QString title, text; bool error; };
    Project project;
    UndoStack stack;
    ImportTool tool;
    int dialogs = 0;
    bool answer = true;
    std::set<std::string> lastSelection;
    QString lastLog;
    bool lastLogSuccess = false;
    std::function<void(DxfSelectionDialog&)> onSelection;
    std::function<void(ImportInsertionDialog&)> onInsertion;
    std::vector<Msg> messages;
};
}  // namespace

TEST_F(ImportToolTest, ActionsAreDisabledWithoutProject) {
    ImportTool bare;
    EXPECT_FALSE(bare.dxfAction()->isEnabled());
    EXPECT_FALSE(bare.textAction()->isEnabled());
    EXPECT_EQ(bare.dxfAction()->text(), "DXF o DWG");
    EXPECT_EQ(bare.textAction()->text(), "Fichero de texto");
    EXPECT_TRUE(tool.dxfAction()->isEnabled());
    EXPECT_FALSE(bare.importDxf());  // nothing happens without a work
}

TEST_F(ImportToolTest, TextFileImportsAfterTheYesAnswer) {
    ASSERT_TRUE(tool.importTextFile(sample("import_sample.txt")));
    EXPECT_TRUE(lastLogSuccess);
    EXPECT_TRUE(lastLog.contains("-> Lectura datos fichero texto"));
    EXPECT_TRUE(lastLog.contains("Cargando fichero"));
    EXPECT_TRUE(lastLog.contains("Leídas 13 líneas."));
    EXPECT_TRUE(lastLog.contains("-> Lectura sector nudos"));
    EXPECT_TRUE(lastLog.contains("-> Lectura sector barras"));
    EXPECT_TRUE(lastLog.contains("Total: 5 nudos, 4 barras"));
    EXPECT_EQ(project.structure.nodes.size(), 5u);
    EXPECT_EQ(project.structure.bars.size(), 4u);
    EXPECT_EQ(stack.count(), 1u);
    stack.undo();
    EXPECT_TRUE(project.structure.nodes.empty());
}

TEST_F(ImportToolTest, TextFileAnsweredNoImportsNothing) {
    answer = false;
    EXPECT_FALSE(tool.importTextFile(sample("import_sample.txt")));
    EXPECT_TRUE(project.structure.nodes.empty());
    EXPECT_EQ(stack.count(), 0u);
}

TEST_F(ImportToolTest, TextFileErrorsShowTheLogWithTheLine) {
    const QString bad = QDir::temp().filePath("cc_import_bad.txt");
    {
        QFile f(bad);
        ASSERT_TRUE(f.open(QIODevice::WriteOnly));
        f.write("NUDOS\n1 X=0 Y=0 Z=0\n2 X=1 Y=0\nBARRAS\n1 1 2\n");
    }
    EXPECT_FALSE(tool.importTextFile(bad));
    EXPECT_FALSE(lastLogSuccess);
    EXPECT_TRUE(lastLog.contains("!! Error: Sintaxis irreconocible"));
    EXPECT_TRUE(lastLog.contains("Línea 3"));
    EXPECT_TRUE(project.structure.nodes.empty());
    QFile::remove(bad);
    // A file with fewer than 3 lines.
    const QString tiny = QDir::temp().filePath("cc_import_tiny.txt");
    {
        QFile f(tiny);
        ASSERT_TRUE(f.open(QIODevice::WriteOnly));
        f.write("NUDOS\n");
    }
    EXPECT_FALSE(tool.importTextFile(tiny));
    EXPECT_TRUE(lastLog.contains("Fichero vacío"));
    QFile::remove(tiny);
    // A missing file.
    EXPECT_FALSE(tool.importTextFile(QDir::temp().filePath("cc_does_not_exist.txt")));
    ASSERT_FALSE(messages.empty());
    EXPECT_TRUE(messages.back().error);
}

TEST_F(ImportToolTest, TextImportUsesTheFileChooser) {
    int chosen = 0;
    tool.setFileChooser([&](ImportTool::Kind kind) {
        ++chosen;
        return kind == ImportTool::Kind::Text ? sample("import_sample.txt") : QString();
    });
    EXPECT_TRUE(tool.importText());
    EXPECT_EQ(chosen, 1);
    EXPECT_FALSE(tool.importDxf());  // chooser answers "cancelled"
    EXPECT_EQ(stack.count(), 1u);
}

TEST_F(ImportToolTest, DxfSelectionListsLayersWithColours) {
    onSelection = [](DxfSelectionDialog& d) {
        EXPECT_EQ(d.layerCount(), 2);
        EXPECT_EQ(d.table()->horizontalHeaderItem(1)->text(), "Capa");
        EXPECT_EQ(d.table()->horizontalHeaderItem(2)->text(), "Color");
        EXPECT_EQ(d.table()->horizontalHeaderItem(3)->text(), "Importar polígonos como\npaños de carga");
        EXPECT_EQ(d.table()->item(0, 1)->text(), "Barras");
        EXPECT_EQ(d.table()->item(0, 2)->background().color(), QColor(0xFF, 0x00, 0x00));  // ACI 1
        EXPECT_EQ(d.table()->item(1, 2)->background().color(), QColor(0x00, 0x00, 0xFF));  // ACI 5
        EXPECT_TRUE(d.panelLayers().empty());                                               // default false
        EXPECT_GT(d.previewStrokes(), 0);
    };
    ASSERT_TRUE(tool.importDxfFile(sample("import_sample.dxf")));
    EXPECT_EQ(dialogs, 1);  // empty work: no "Importar" dialog
    EXPECT_EQ(project.structure.nodes.size(), 10u);
    EXPECT_EQ(project.structure.bars.size(), 11u);
    EXPECT_EQ(project.structure.shells.size(), 2u);
    EXPECT_EQ(stack.count(), 1u);
}

TEST_F(ImportToolTest, DxfLayerSelectionFiltersAndPanelsAreIgnored) {
    onSelection = [](DxfSelectionDialog& d) {
        d.setLayerChecked(0, false);   // no bars
        d.setPanelChecked(1, true);    // roof faces as load panels
        EXPECT_EQ(d.previewStrokes(), 8);
    };
    EXPECT_FALSE(tool.importDxfFile(sample("import_sample.dxf")));  // only panels: the model has none, nothing is added
    EXPECT_TRUE(project.structure.nodes.empty());
    ASSERT_FALSE(messages.empty());
    EXPECT_NE(messages.back().text.indexOf("paños de carga"), -1);
    EXPECT_EQ(stack.count(), 0u);
}

TEST_F(ImportToolTest, DxfSelectionNeedsOneLayer) {
    DxfSelectionDialog dialog("", {{"A", 1, 1, 0}});
    dialog.setLayerChecked(0, false);
    dialog.accept();
    EXPECT_EQ(dialog.result(), QDialog::Rejected);  // still open
    EXPECT_EQ(dialog.errorText(), "Es necesario haber seleccionado, al menos, una capa.");
    dialog.setLayerChecked(0, true);
    dialog.accept();
    EXPECT_EQ(dialog.result(), QDialog::Accepted);
    EXPECT_TRUE(dialog.errorText().isEmpty());
}

TEST_F(ImportToolTest, CancellingTheSelectionImportsNothing) {
    answer = false;
    EXPECT_FALSE(tool.importDxfFile(sample("import_sample.dxf")));
    EXPECT_TRUE(project.structure.nodes.empty());
}

TEST_F(ImportToolTest, DwgGivesTheUnsupportedFormatError) {
    const QString dwg = QDir::temp().filePath("cc_import_test.dwg");
    {
        QFile f(dwg);
        ASSERT_TRUE(f.open(QIODevice::WriteOnly));
        f.write("AC1027");
    }
    EXPECT_FALSE(tool.importDxfFile(dwg));
    ASSERT_EQ(messages.size(), 1u);
    EXPECT_TRUE(messages[0].error);
    EXPECT_TRUE(messages[0].text.startsWith("Formato no soportado"));
    EXPECT_EQ(dialogs, 0);
    QFile::remove(dwg);
}

TEST_F(ImportToolTest, WorkWithDataAsksWhereAndCanTranslate) {
    project.structure.addNode({0, 0, 0});
    onInsertion = [](ImportInsertionDialog& d) {
        EXPECT_FALSE(d.useReferencePoint());  // default: position of the file
        d.setUseReferencePoint(true);
        d.setReferencePoint({100, 0, 0});
    };
    ASSERT_TRUE(tool.importDxfFile(sample("import_sample.dxf")));
    EXPECT_EQ(dialogs, 2);  // selection + "Importar"
    bool shifted = false;
    for (const auto& n : project.structure.nodes) shifted = shifted || n.position.x == 100.0;
    EXPECT_TRUE(shifted);
    EXPECT_EQ(project.structure.nodes.size(), 1u + 10u);
    // Undo removes everything of the import in one step.
    stack.undo();
    EXPECT_EQ(project.structure.nodes.size(), 1u);
}

TEST_F(ImportToolTest, WorkWithDataInTheFilePosition) {
    project.structure.addNode({0, 0, 0});
    ASSERT_TRUE(tool.importDxfFile(sample("import_sample.dxf")));
    EXPECT_EQ(project.structure.nodes.size(), 10u);  // the node at the origin is reused
}

TEST_F(ImportToolTest, GroupedPlanesAreUngroupedAfterTheConfirmation) {
    project.workPlanes.groups.z = {{0.0, 3.0}};
    QString asked;
    tool.setConfirmer([&](const QString& q) {
        asked = q;
        return false;
    });
    EXPECT_FALSE(tool.importDxfFile(sample("import_sample.dxf")));
    EXPECT_TRUE(asked.contains("Se van a desagrupar todos los planos que intervienen en la generación."));
    EXPECT_TRUE(asked.contains("¿Desea continuar?"));
    EXPECT_EQ(dialogs, 0);  // No cancels before the selection dialog
    EXPECT_FALSE(project.workPlanes.groups.z.empty());
    tool.setConfirmer([](const QString&) { return true; });
    ASSERT_TRUE(tool.importDxfFile(sample("import_sample.dxf")));
    EXPECT_TRUE(project.workPlanes.groups.z.empty());
    EXPECT_EQ(stack.count(), 1u);  // one macro: ungroup + import
    stack.undo();
    EXPECT_FALSE(project.workPlanes.groups.z.empty());
    EXPECT_TRUE(project.structure.nodes.empty());
}

TEST(ImportDialogs, InsertionDialogDefaults) {
    app();
    ImportInsertionDialog d;
    EXPECT_FALSE(d.useReferencePoint());
    d.setUseReferencePoint(true);
    d.setReferencePoint({1, 2, 3});
    EXPECT_TRUE(d.useReferencePoint());
    EXPECT_DOUBLE_EQ(d.referencePoint().y, 2.0);
}

int main(int argc, char** argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
