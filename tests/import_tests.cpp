#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <fstream>

#include "core/import_dxf.h"
#include "core/import_geometry.h"
#include "core/import_text.h"

using namespace cc::core;

namespace {

// The literal example of the help of the original (importar.md section 1).
const char* kExample =
    "NUDOS\n1 X=0 Y=0 Z=0\n2 X=0 Y=0 Z=3\n3 X=5 Y=0 Z=5\n4 X=10 Y=0 Z=3\n5 X=10 Y=0 Z=0\n\n"
    "BARRAS\n1 1 2\n2 2 3\n3 3 4\n4 4 5\n";

bool hasError(const TextImportResult& r, int id) {
    return std::any_of(r.errors.begin(), r.errors.end(), [id](const ImportMessage& m) { return m.id == id; });
}

}  // namespace

TEST(ImportText, ParsesTheExampleOfTheHelp) {
    const TextImportResult r = parseGeometryText(kExample);
    ASSERT_TRUE(r.ok()) << (r.errors.empty() ? "" : r.errors[0].text);
    ASSERT_EQ(r.geometry.nodes.size(), 5u);
    ASSERT_EQ(r.geometry.bars.size(), 4u);
    EXPECT_DOUBLE_EQ(r.geometry.nodes[2].position.z, 5.0);
    EXPECT_EQ(r.geometry.bars[3].startNumber, 4);
    EXPECT_EQ(r.geometry.bars[3].endNumber, 5);
    EXPECT_EQ(r.linesRead, 12);  // blank line included
    EXPECT_EQ(importTotalsMessage(5, 4), "Total: 5 nudos, 4 barras");
}

TEST(ImportText, EnglishKeywordsShellsPanelsAndDecimalComma) {
    const TextImportResult r = parseGeometryText(
        "joints\r\n1 x=0 y=0 z=0\r\n2 X=1,5 Y=0 Z=0\r\n3 X=1.5 Y=2 Z=0\r\n4 X=0 Y=2 Z=0\r\n"
        "Shells\r\n7 1 2 3 4\r\n"
        "PANELS\r\n2 1 2 3\r\n"
        "FRAMES\r\n1 1 2\r\n");
    ASSERT_TRUE(r.ok()) << (r.errors.empty() ? "" : r.errors[0].text);
    EXPECT_DOUBLE_EQ(r.geometry.nodes[1].position.x, 1.5);
    ASSERT_EQ(r.geometry.shells.size(), 1u);
    EXPECT_EQ(r.geometry.shells[0].number, 7);
    EXPECT_EQ(r.geometry.shells[0].nodeNumbers, (std::vector<long long>{1, 2, 3, 4}));
    ASSERT_EQ(r.geometry.panels.size(), 1u);
    // Spanish keywords with the eñe, as UTF-8.
    const TextImportResult es = parseGeometryText("NUDOS\n1 X=0 Y=0 Z=0\n2 X=1 Y=0 Z=0\n3 X=1 Y=1 Z=0\nL\xC3\x81MINAS\n1 1 2 3\nPA\xC3\x91OS\n1 1 2 3\n");
    EXPECT_TRUE(es.ok()) << (es.errors.empty() ? "" : es.errors[0].text);
    EXPECT_EQ(es.geometry.panels.size(), 1u);
}

TEST(ImportText, NodeErrors) {
    auto run = [](const std::string& nodes) { return parseGeometryText("NUDOS\n" + nodes + "BARRAS\n1 1 2\n"); };
    EXPECT_TRUE(hasError(run("0 X=0 Y=0 Z=0\n1 X=0 Y=0 Z=0\n2 X=1 Y=0 Z=0\n"), 106854));
    EXPECT_TRUE(hasError(run("-4 X=0 Y=0 Z=0\n1 X=0 Y=0 Z=0\n2 X=1 Y=0 Z=0\n"), 106860));
    EXPECT_TRUE(hasError(run("1000000 X=0 Y=0 Z=0\n1 X=0 Y=0 Z=0\n2 X=1 Y=0 Z=0\n"), 106855));
    EXPECT_TRUE(hasError(run("1 X=0 Y=0 Z=0\n1 X=5 Y=0 Z=0\n2 X=1 Y=0 Z=0\n"), 106856));
    const TextImportResult same = run("1 X=0 Y=0 Z=0\n2 X=0 Y=0 Z=0\n3 X=1 Y=0 Z=0\n");
    EXPECT_TRUE(hasError(same, 106857));
    EXPECT_EQ(same.errors[0].text, "Error: Coordenadas iguales a un nudo ya existente (nudo 1)");
    EXPECT_TRUE(hasError(run("1 X=0 Y=0\n1 X=0 Y=0 Z=0\n2 X=1 Y=0 Z=0\n"), 106859));
    EXPECT_TRUE(hasError(run("uno X=0 Y=0 Z=0\n1 X=0 Y=0 Z=0\n2 X=1 Y=0 Z=0\n"), 106859));
    // 999999 is the largest number allowed.
    EXPECT_TRUE(parseGeometryText("NUDOS\n999999 X=0 Y=0 Z=0\n1 X=1 Y=0 Z=0\nBARRAS\n1 1 999999\n").ok());
}

TEST(ImportText, BarErrors) {
    auto run = [](const std::string& bars) {
        return parseGeometryText("NUDOS\n1 X=0 Y=0 Z=0\n2 X=1 Y=0 Z=0\n3 X=2 Y=0 Z=0\nBARRAS\n" + bars);
    };
    EXPECT_TRUE(hasError(run("0 1 2\n"), 106862));
    EXPECT_TRUE(hasError(run("1000000 1 2\n"), 106863));
    EXPECT_TRUE(hasError(run("1 0 2\n"), 106864));
    EXPECT_TRUE(hasError(run("1 1 1000000\n"), 106865));
    EXPECT_TRUE(hasError(run("1 2 2\n"), 106861));
    EXPECT_TRUE(hasError(run("1 1 2\n1 2 3\n"), 106866));
    const TextImportResult missing = run("1 1 9\n");
    EXPECT_TRUE(hasError(missing, 106867));
    EXPECT_EQ(missing.errors[0].text, "Error: Nudo inexistente en barra (barra: 1 nudo: 9).");
    const TextImportResult dup = run("1 1 2\n2 2 1\n");
    EXPECT_TRUE(hasError(dup, 106868));
    EXPECT_EQ(dup.errors[0].text, "!! Error: Barra con conectividades ya existentes (2 = 1).");
    EXPECT_TRUE(hasError(run("1 1\n"), 106869));
    EXPECT_TRUE(hasError(run("1 1 2 3\n"), 106869));
    EXPECT_EQ(dup.errors[0].line, 7);
}

TEST(ImportText, ShellAndPanelErrors) {
    auto run = [](const std::string& body) {
        return parseGeometryText("NUDOS\n1 X=0 Y=0 Z=0\n2 X=1 Y=0 Z=0\n3 X=1 Y=1 Z=0\n4 X=0 Y=1 Z=0\n" + body);
    };
    EXPECT_TRUE(hasError(run("LAMINAS\n1 1 2\n"), 106884));
    EXPECT_TRUE(hasError(run("LAMINAS\n0 1 2 3\n"), 106885));
    EXPECT_TRUE(hasError(run("LAMINAS\n1000000 1 2 3\n"), 106886));
    EXPECT_TRUE(hasError(run("LAMINAS\n1 1 2 2\n"), 106887));
    EXPECT_TRUE(hasError(run("LAMINAS\n1 1 2 3\n1 2 3 4\n"), 106888));
    const TextImportResult missing = run("LAMINAS\n3 1 2 9\n");
    EXPECT_TRUE(hasError(missing, 106889));
    EXPECT_EQ(missing.errors[0].text, "Error: Nudo inexistente en lámina (lámina: 3, nudo: 9).");
    EXPECT_TRUE(hasError(run("LAMINAS\n1 1 2 3\n2 3 2 1\n"), 106890));
    EXPECT_TRUE(hasError(run("PANELS\n0 1 2 3\n"), 399537));
    EXPECT_TRUE(hasError(run("PANELS\n1 1 2 2\n"), 399539));
    EXPECT_TRUE(hasError(run("PANELS\n1 1 2 3\n2 1 2 3\n"), 399541));
}

TEST(ImportText, GlobalErrors) {
    EXPECT_TRUE(hasError(parseGeometryText(""), 106870));
    EXPECT_TRUE(hasError(parseGeometryText("\n   \n"), 106870));
    // Fewer than 3 lines, whatever they hold: "Fichero vacío".
    EXPECT_TRUE(hasError(parseGeometryText("NUDOS\n1 X=0 Y=0 Z=0\n"), 106870));
    EXPECT_TRUE(hasError(parseGeometryText("1 X=0 Y=0 Z=0\n"), 106870));
    EXPECT_TRUE(hasError(parseGeometryText("NUDOS\n1 X=0 Y=0 Z=0\n2 X=1 Y=0 Z=0\n"), 106873));  // no BARRAS / LAMINAS / PAÑOS
    EXPECT_TRUE(hasError(parseGeometryText("BARRAS\n1 1 2\nNUDOS\n1 X=0 Y=0 Z=0\n2 X=1 Y=0 Z=0\n"), 106859));  // NUDOS must come first
    EXPECT_TRUE(hasError(parseGeometryText("1 X=0 Y=0 Z=0\nNUDOS\n1 X=0 Y=0 Z=0\n2 X=1 Y=0 Z=0\nBARRAS\n1 1 2\n"), 106859));
    // There is no way back to NUDOS.
    EXPECT_TRUE(hasError(parseGeometryText("NUDOS\n1 X=0 Y=0 Z=0\n2 X=1 Y=0 Z=0\nBARRAS\n1 1 2\nNUDOS\n3 X=2 Y=0 Z=0\n"), 106859));
    EXPECT_TRUE(hasError(parseGeometryText("NUDOS\n1 X=0 Y=0 Z=0\nBARRAS\n"), 106877));
    EXPECT_EQ(importTextMessage(106870), "Error: Fichero vacío");
    EXPECT_EQ(importTextMessage(1), "");
}

TEST(ImportGeometry, ImportsIntoProjectUndoRedoAndReusesNodes) {
    Project p;
    UndoStack stack;
    const int existing = p.structure.addNode({0, 0, 0});
    const TextImportResult r = parseGeometryText(kExample);
    ASSERT_TRUE(r.ok());
    ImportSummary summary;
    ASSERT_TRUE(importGeometry(p, stack, r.geometry, &summary));
    EXPECT_EQ(summary.nodesCreated, 4);  // node 1 of the file coincides with the existing one
    EXPECT_EQ(summary.barsCreated, 4);
    EXPECT_EQ(p.structure.bars[0].startNode, existing);
    EXPECT_EQ(p.structure.bars[0].materialId, 0);  // not described yet
    EXPECT_TRUE(p.structure.validate().empty());
    // Importing the same file again adds nothing and pushes nothing.
    EXPECT_FALSE(importGeometry(p, stack, r.geometry, &summary));
    EXPECT_EQ(summary.skippedBars, 4);
    EXPECT_EQ(stack.count(), 1u);
    stack.undo();
    EXPECT_EQ(p.structure.nodes.size(), 1u);
    EXPECT_TRUE(p.structure.bars.empty());
    stack.redo();
    EXPECT_EQ(p.structure.bars.size(), 4u);
}

TEST(ImportGeometry, ShellsAndPanels) {
    Project p;
    UndoStack stack;
    const TextImportResult r = parseGeometryText(
        "NUDOS\n1 X=0 Y=0 Z=0\n2 X=2 Y=0 Z=0\n3 X=2 Y=2 Z=0\n4 X=0 Y=2 Z=0\n5 X=1 Y=3 Z=0\n6 X=0 Y=0 Z=1\n"
        "LAMINAS\n1 1 2 3 4\n2 3 4 5\n3 1 2 3 6\nPANELS\n1 1 2 3\n");
    ASSERT_TRUE(r.ok()) << (r.errors.empty() ? "" : r.errors[0].text);
    ImportSummary summary;
    ASSERT_TRUE(importGeometry(p, stack, r.geometry, &summary));
    EXPECT_EQ(summary.shellsCreated, 2);   // the third one (non-coplanar) is skipped
    EXPECT_EQ(summary.skippedShells, 1);
    EXPECT_EQ(summary.ignoredPanels, 1);
    EXPECT_TRUE(p.structure.validate().empty());
}

// --- DXF ---

namespace {

std::string dxf(const std::string& entities) { return "0\nSECTION\n2\nENTITIES\n" + entities + "0\nENDSEC\n0\nEOF\n"; }

std::string line(const std::string& layer, double x1, double y1, double z1, double x2, double y2, double z2) {
    auto s = [](double v) { return std::to_string(v); };
    return "0\nLINE\n8\n" + layer + "\n10\n" + s(x1) + "\n20\n" + s(y1) + "\n30\n" + s(z1) + "\n11\n" + s(x2) + "\n21\n" + s(y2) + "\n31\n" +
           s(z2) + "\n";
}

}  // namespace

TEST(ImportDxf, LinesBecomeBarsSharingNodes) {
    const DxfReadResult r = readDxf(dxf(line("A", 0, 0, 0, 5, 0, 0) + line("A", 5, 0, 0, 5, 3, 0) + line("B", 5, 3, 0, 0, 0, 0)));
    ASSERT_TRUE(r.ok());
    EXPECT_EQ(r.geometry.nodes.size(), 3u);
    EXPECT_EQ(r.geometry.bars.size(), 3u);
    EXPECT_EQ(r.layers, (std::vector<std::string>{"A", "B"}));
    EXPECT_EQ(dxfLayers(dxf(line("A", 0, 0, 0, 1, 0, 0))), (std::vector<std::string>{"A"}));
}

TEST(ImportDxf, LayerFilterRepeatedAndExcessiveBars) {
    DxfOptions only;
    only.layers = {"B"};
    const std::string content = dxf(line("A", 0, 0, 0, 5, 0, 0) + line("B", 0, 0, 0, 0, 5, 0) + line("B", 0, 5, 0, 0, 0, 0) +
                                    line("B", 0, 0, 0, 2000, 0, 0));
    const DxfReadResult r = readDxf(content, only);
    ASSERT_TRUE(r.ok());
    EXPECT_EQ(r.geometry.bars.size(), 1u);
    EXPECT_EQ(r.skippedRepeatedBars, 1);   // #64502
    EXPECT_EQ(r.skippedLongBars, 1);       // #65115
    ASSERT_EQ(r.warnings.size(), 2u);
    EXPECT_EQ(r.warnings[0].id, 65115);
    EXPECT_EQ(r.warnings[1].id, 64502);
    DxfOptions none;
    none.layers = {"Z"};
    const DxfReadResult empty = readDxf(content, none);
    ASSERT_FALSE(empty.ok());
    EXPECT_EQ(empty.errors[0].id, 64993);
}

TEST(ImportDxf, PolylinesFacesAndPanels) {
    const std::string lw = "0\nLWPOLYLINE\n8\nP\n90\n4\n70\n1\n38\n2.0\n10\n0\n20\n0\n10\n3\n20\n0\n10\n3\n20\n3\n10\n0\n20\n3\n";
    const std::string poly =
        "0\nPOLYLINE\n8\nQ\n70\n8\n0\nVERTEX\n8\nQ\n10\n0\n20\n0\n30\n0\n0\nVERTEX\n8\nQ\n10\n1\n20\n0\n30\n1\n0\nVERTEX\n8\nQ\n10\n2\n20\n0\n30\n2\n0\nSEQEND\n8\nQ\n";
    const std::string face = "0\n3DFACE\n8\nF\n10\n0\n20\n0\n30\n0\n11\n1\n21\n0\n31\n0\n12\n1\n22\n1\n32\n0\n13\n1\n23\n1\n33\n0\n";
    DxfOptions panels;
    panels.panelLayers = {"F"};
    const DxfReadResult r = readDxf(dxf(lw + poly + face), panels);
    ASSERT_TRUE(r.ok());
    EXPECT_EQ(r.geometry.bars.size(), 4u + 2u);        // closed square + two segments
    // The 3DFACE sits on layer F, which is marked "paños de carga": a triangle panel (4th corner equal to the 3rd); no shells.
    ASSERT_EQ(r.geometry.panels.size(), 1u);
    EXPECT_EQ(r.geometry.panels[0].nodeNumbers.size(), 3u);
    EXPECT_TRUE(r.geometry.shells.empty());
    const DxfReadResult asShell = readDxf(dxf(face));
    ASSERT_EQ(asShell.geometry.shells.size(), 1u);
    EXPECT_TRUE(asShell.geometry.panels.empty());
    EXPECT_DOUBLE_EQ(r.geometry.nodes[0].position.z, 2.0);   // elevation of the LWPOLYLINE
    const DxfReadResult noPanels = readDxf(dxf(lw));
    EXPECT_TRUE(noPanels.geometry.panels.empty());
    EXPECT_EQ(noPanels.geometry.bars.size(), 4u);  // closed polylines give bars only, even with the panels option

    Project p;
    UndoStack stack;
    ImportSummary summary;
    ASSERT_TRUE(importGeometry(p, stack, asShell.geometry, &summary));
    EXPECT_EQ(summary.shellsCreated, 1);
    EXPECT_TRUE(p.structure.validate().empty());
}

TEST(ImportDxf, UnsupportedAndBrokenInput) {
    EXPECT_FALSE(readDxf("AC1015\x00\x01 binary").ok());
    EXPECT_FALSE(readDxf("AutoCAD Binary DXF\r\n").ok());
    const DxfReadResult none = readDxf(dxf(""));
    ASSERT_FALSE(none.ok());
    EXPECT_EQ(none.errors[0].id, 60948);
    EXPECT_FALSE(readDxf("not a dxf at all\n").ok());

    const auto dir = std::filesystem::temp_directory_path();
    const auto dwg = dir / "cc_import_test.dwg";
    { std::ofstream(dwg, std::ios::binary) << "AC1027"; }
    const DxfReadResult d = readDxfFile(dwg);
    ASSERT_FALSE(d.ok());
    EXPECT_EQ(d.errors[0].text.rfind("Formato no soportado", 0), 0u);
    std::filesystem::remove(dwg);
    const DxfReadResult missing = readDxfFile(dir / "cc_does_not_exist.dxf");
    ASSERT_FALSE(missing.ok());
    EXPECT_EQ(missing.errors[0].id, 71697);
    const auto ok = dir / "cc_import_test.dxf";
    { std::ofstream(ok, std::ios::binary) << dxf(line("A", 0, 0, 0, 1, 0, 0)); }
    EXPECT_TRUE(readDxfFile(ok).ok());
    std::filesystem::remove(ok);
}

TEST(ImportText, CommentsLatin1AndOnlyPanels) {
    // Everything from the first ':' is a comment, a line that starts with ':' is ignored.
    const TextImportResult r = parseGeometryText(
        ": header\nNUDOS : nodes\n1 X=0 Y=0 Z=0 : origin\n2 X=1 Y=0 Z=0\n3 X=1 Y=1 Z=0\nPA\xD1OS\n1 1 2 3\n");
    // Only panels: the sectors are complete but there is no bar nor shell (#106877).
    EXPECT_TRUE(hasError(r, 106877));
    EXPECT_FALSE(hasError(r, 106873));
    EXPECT_EQ(r.geometry.panels.size(), 1u);  // the Latin-1 enye was read as a keyword
    EXPECT_EQ(r.linesRead, 7);
    // A comment may hide a syntax error.
    EXPECT_TRUE(parseGeometryText("NUDOS\n1 X=0 Y=0 Z=0 : fine\n2 X=1 Y=0 Z=0\nBARRAS\n1 1 2 :9 9\n").ok());
}

TEST(ImportDxf, LayerInfoColoursAndPalette) {
    const std::string content =
        "0\nSECTION\n2\nTABLES\n0\nTABLE\n2\nLAYER\n0\nLAYER\n2\nA\n62\n1\n0\nLAYER\n2\nB\n62\n-5\n0\nLAYER\n2\nUNUSED\n62\n3\n0\nENDTAB\n0\nENDSEC\n" +
        dxf(line("A", 0, 0, 0, 1, 0, 0) + line("B", 0, 0, 0, 0, 1, 0) +
            "0\n3DFACE\n8\nB\n10\n0\n20\n0\n30\n0\n11\n1\n21\n0\n31\n0\n12\n1\n22\n1\n32\n0\n13\n0\n23\n1\n33\n0\n");
    const auto layers = dxfLayerInfo(content);
    ASSERT_EQ(layers.size(), 2u);
    EXPECT_EQ(layers[0].name, "A");
    EXPECT_EQ(layers[0].colorIndex, 1);
    EXPECT_EQ(layers[0].segments, 1);
    EXPECT_EQ(layers[1].colorIndex, 5);  // a negative colour means "layer off": its magnitude is the colour
    EXPECT_EQ(layers[1].polygons, 1);
    EXPECT_EQ(layers[1].segments, 1);
    EXPECT_EQ(aciToRgb(1), 0xFF0000u);
    EXPECT_EQ(aciToRgb(5), 0x0000FFu);
    EXPECT_EQ(aciToRgb(10), 0xFF0000u);
    EXPECT_EQ(aciToRgb(250), 0x333333u);
    EXPECT_EQ(aciToRgb(999), 0xFFFFFFu);  // out of range -> 7
}

TEST(ImportDxf, PolyfaceAndPolygonMeshes) {
    const auto vertex = [](double x, double y, double z, int flags) {
        return "0\nVERTEX\n8\nM\n10\n" + std::to_string(x) + "\n20\n" + std::to_string(y) + "\n30\n" + std::to_string(z) + "\n70\n" +
               std::to_string(flags) + "\n";
    };
    const auto face = [](int a, int b, int c, int d) {
        return "0\nVERTEX\n8\nM\n10\n0\n20\n0\n30\n0\n70\n128\n71\n" + std::to_string(a) + "\n72\n" + std::to_string(b) + "\n73\n" +
               std::to_string(c) + "\n74\n" + std::to_string(d) + "\n";
    };
    // Polyface mesh: 4 corners, one quad and one triangle (an invisible edge is a negative index).
    const std::string polyface = "0\nPOLYLINE\n8\nM\n70\n64\n71\n4\n72\n2\n" + vertex(0, 0, 0, 192) + vertex(1, 0, 0, 192) +
                                 vertex(1, 1, 0, 192) + vertex(0, 1, 0, 192) + face(1, 2, 3, 4) + face(1, -2, 3, 0) + "0\nSEQEND\n8\nM\n";
    DxfReadResult r = readDxf(dxf(polyface));
    ASSERT_TRUE(r.ok());
    ASSERT_EQ(r.geometry.shells.size(), 2u);
    EXPECT_EQ(r.geometry.shells[0].nodeNumbers.size(), 4u);
    EXPECT_EQ(r.geometry.shells[1].nodeNumbers.size(), 3u);
    EXPECT_TRUE(r.geometry.bars.empty());
    DxfOptions panels;
    panels.panelLayers = {"M"};
    r = readDxf(dxf(polyface), panels);
    EXPECT_EQ(r.geometry.panels.size(), 2u);
    EXPECT_TRUE(r.geometry.shells.empty());

    // Polygon mesh 3 x 2 -> two cells.
    const std::string grid = "0\nPOLYLINE\n8\nM\n70\n16\n71\n3\n72\n2\n" + vertex(0, 0, 0, 64) + vertex(0, 1, 0, 64) + vertex(1, 0, 0, 64) +
                             vertex(1, 1, 0, 64) + vertex(2, 0, 0, 64) + vertex(2, 1, 0, 64) + "0\nSEQEND\n8\nM\n";
    r = readDxf(dxf(grid));
    ASSERT_TRUE(r.ok());
    EXPECT_EQ(r.geometry.shells.size(), 2u);
    const auto info = dxfLayerInfo(dxf(grid));
    ASSERT_EQ(info.size(), 1u);
    EXPECT_EQ(info[0].polygons, 2);
}
