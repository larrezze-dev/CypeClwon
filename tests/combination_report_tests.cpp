#include <gtest/gtest.h>

#include <fstream>
#include <iterator>

#include "core/combination_export.h"

using namespace cc::core;

#ifndef CC_SOURCE_DIR
#error CC_SOURCE_DIR must be defined by the build
#endif

namespace {

/// Raw bytes of a file captured from the original (UTF-16 LE with BOM, CRLF).
std::string golden(const char* name) {
    std::ifstream f(std::string(CC_SOURCE_DIR) + "/docs/research/original_export/" + name, std::ios::binary);
    return {std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()};
}

}  // namespace

TEST(CombinationReport, TextExportMatchesOriginalForGroundReportLowSnow) {
    const std::string expected = golden("s1.txt");
    ASSERT_FALSE(expected.empty());
    EXPECT_EQ(exportTextUtf16(buildGroundReport(false)), expected);
}

TEST(CombinationReport, TextExportMatchesOriginalForGroundReportHighSnow) {
    const std::string expected = golden("s1hi.txt");
    ASSERT_FALSE(expected.empty());
    EXPECT_EQ(exportTextUtf16(buildGroundReport(true)), expected);
}

TEST(CombinationReport, TextExportMatchesOriginalForColdFormedTimberReport) {
    const std::string expected = golden("s2.txt");
    ASSERT_FALSE(expected.empty());
    EXPECT_EQ(exportTextUtf16(buildColdFormedTimberReport(false)), expected);
}

TEST(CombinationReport, SnowRowsDependOnAltitude) {
    const auto lo = buildGroundReport(false);
    const auto hi = buildGroundReport(true);
    const auto snow = [](const CombinationReport& r) {
        for (const auto& row : r.sections[0].tables[0].rows)
            if (row.label == "Nieve (Q)") return row.accompanying.value;
        return -1.0;
    };
    EXPECT_DOUBLE_EQ(snow(lo), 0.5);
    EXPECT_DOUBLE_EQ(snow(hi), 0.7);
}

TEST(CombinationReport, RichFormatsContainTheTables) {
    const auto report = buildColdFormedTimberReport(false);
    const std::string html = exportHtml(report);
    EXPECT_NE(html.find("Acero conformado"), std::string::npos);
    EXPECT_NE(html.find("Accidental de incendio"), std::string::npos);
    const std::string rtf = exportRtf(report);
    EXPECT_EQ(rtf.rfind("{\\rtf1", 0), 0u);
    EXPECT_NE(rtf.find("\\u968"), std::string::npos);  // psi
}

TEST(CombinationReport, DocxIsAZipWithTheDocumentPart) {
    const std::string docx = exportDocx(buildGroundReport(false));
    ASSERT_GT(docx.size(), 4u);
    EXPECT_EQ(docx.substr(0, 4), std::string("PK", 4));
    EXPECT_NE(docx.find("word/document.xml"), std::string::npos);
    EXPECT_NE(docx.find("[Content_Types].xml"), std::string::npos);
}
