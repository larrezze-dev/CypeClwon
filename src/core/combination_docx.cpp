#include <miniz.h>

#include <sstream>

#include "core/combination_export.h"

namespace cc::core {

namespace {

std::string xml(const std::string& s) {
    std::string o;
    for (char c : s) {
        if (c == '&') o += "&amp;";
        else if (c == '<') o += "&lt;";
        else if (c == '>') o += "&gt;";
        else o += c;
    }
    return o;
}

std::string paragraph(const std::string& text, bool bold = false, bool pageBreakBefore = false) {
    std::string p = "<w:p><w:pPr>";
    if (pageBreakBefore) p += "<w:pageBreakBefore/>";
    p += "</w:pPr><w:r>";
    if (bold) p += "<w:rPr><w:b/></w:rPr>";
    p += "<w:t xml:space=\"preserve\">" + xml(text) + "</w:t></w:r></w:p>";
    return p;
}

std::string cell(const std::string& text, bool bold = false) {
    return "<w:tc><w:tcPr><w:tcW w:w=\"1800\" w:type=\"dxa\"/></w:tcPr>" + paragraph(text, bold) + "</w:tc>";
}

std::string row(const std::vector<std::string>& cells, bool bold = false) {
    std::string r = "<w:tr>";
    for (const auto& c : cells) r += cell(c, bold);
    return r + "</w:tr>";
}

std::string documentXml(const CombinationReport& report) {
    std::ostringstream d;
    d << "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
         "<w:document xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\"><w:body>";
    const auto& f = formulae();
    bool firstSection = true;
    for (const auto& sec : report.sections) {
        bool first = true;
        for (const auto& t : sec.titles) {
            d << paragraph(t, true, first && !firstSection);
            first = false;
        }
        firstSection = false;
        d << paragraph(introText());
        if (sec.layout == SectionLayout::Displacements) {
            d << paragraph("Situaciones persistentes o transitorias") << paragraph(f.displacementsPersistent)
              << paragraph("Situaciones sísmicas") << paragraph(f.displacementsSeismic);
        } else if (sec.layout == SectionLayout::Ultimate) {
            d << paragraph("Situaciones persistentes o transitorias") << paragraph(f.persistent)
              << paragraph("Situaciones sísmicas") << paragraph(f.seismic) << paragraph("Situaciones accidentales")
              << paragraph(f.accidental);
        } else {
            d << paragraph(f.persistent);
        }
        d << paragraph("Donde:");
        for (const auto& item : legend(sec.layout)) {
            const auto p = prettySymbol(item.symbol);
            d << paragraph(p.base + p.subscript + "  " + item.description);
        }
        for (const auto& t : sec.tables) {
            d << paragraph(t.title, true) << "<w:tbl><w:tblPr><w:tblBorders>";
            for (const char* side : {"top", "left", "bottom", "right", "insideH", "insideV"})
                d << "<w:" << side << " w:val=\"single\" w:sz=\"4\" w:space=\"0\" w:color=\"000000\"/>";
            d << "</w:tblBorders></w:tblPr>";
            if (sec.layout == SectionLayout::Displacements) {
                d << row({"", "Favorable (γ)", "Desfavorable (γ)"}, true);
                for (const auto& r : t.rows)
                    d << row({r.label, formatCoefficient(r.favourable, sec.layout), formatCoefficient(r.unfavourable, sec.layout)});
                d << "</w:tbl>" << paragraph("");
                continue;
            }
            d << row({"", "Favorable (γ)", "Desfavorable (γ)", "Principal (ψp)", "Acompañamiento (ψa)"}, true);
            for (const auto& r : t.rows)
                d << row({r.label, formatCoefficient(r.favourable), formatCoefficient(r.unfavourable),
                          formatCoefficient(r.principal), formatCoefficient(r.accompanying)});
            d << "</w:tbl>" << paragraph("");
        }
    }
    d << "</w:body></w:document>";
    return d.str();
}

}  // namespace

std::string exportDocx(const CombinationReport& report) {
    mz_zip_archive zip{};
    if (!mz_zip_writer_init_heap(&zip, 0, 64 * 1024)) return {};
    const auto add = [&zip](const char* name, const std::string& data) {
        return mz_zip_writer_add_mem(&zip, name, data.data(), data.size(), MZ_DEFAULT_LEVEL);
    };
    bool ok = add("[Content_Types].xml",
                  "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
                  "<Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\">"
                  "<Default Extension=\"rels\" ContentType=\"application/vnd.openxmlformats-package.relationships+xml\"/>"
                  "<Default Extension=\"xml\" ContentType=\"application/xml\"/>"
                  "<Override PartName=\"/word/document.xml\" "
                  "ContentType=\"application/vnd.openxmlformats-officedocument.wordprocessingml.document.main+xml\"/>"
                  "</Types>");
    ok = ok && add("_rels/.rels",
                   "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
                   "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">"
                   "<Relationship Id=\"rId1\" "
                   "Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument\" "
                   "Target=\"word/document.xml\"/></Relationships>");
    ok = ok && add("word/document.xml", documentXml(report));
    void* buffer = nullptr;
    std::size_t size = 0;
    std::string out;
    if (ok && mz_zip_writer_finalize_heap_archive(&zip, &buffer, &size)) out.assign(static_cast<const char*>(buffer), size);
    if (buffer) mz_free(buffer);
    mz_zip_writer_end(&zip);
    return out;
}

}  // namespace cc::core
