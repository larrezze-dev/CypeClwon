#include "core/combination_export.h"

#include <cstdint>
#include <sstream>

namespace cc::core {

namespace {

constexpr int kColumnWidths[5] = {27, 21, 24, 18, 23};

/// Centre like the original: the odd character goes to the right.
std::string centre(const std::string& s, int width) {
    const int chars = static_cast<int>(utf8Length(s));
    const int pad = width > chars ? width - chars : 0;
    return std::string(pad / 2, ' ') + s + std::string(pad - pad / 2, ' ');
}

std::string dashes(int n) { return std::string(n, '-'); }

std::string separator() {
    std::string s = " |";
    for (int w : kColumnWidths) s += dashes(w) + "|";
    return s + " ";
}

std::string spaceRow() {
    std::string s = " |";
    for (int w : kColumnWidths) s += std::string(w, ' ') + "|";
    return s + " ";
}

std::vector<std::string> wrap(const std::string& text, int width) {
    std::vector<std::string> lines;
    std::string line = " ";
    std::istringstream in(text);
    std::string word;
    bool first = true;
    while (in >> word) {
        if (!first && utf8Length(line + " " + word + " ") > static_cast<std::size_t>(width)) {
            lines.push_back(line + " ");
            line = " ";
            first = true;
        }
        line += (first ? "" : " ") + word;
        first = false;
    }
    lines.push_back(line + " ");
    return lines;
}

void blanks(std::vector<std::string>& out, int n) {
    for (int i = 0; i < n; ++i) out.emplace_back();
}

void displacementTableLines(std::vector<std::string>& out, const CoefficientTable& t) {
    const auto separator2 = [] { return " |" + dashes(42) + "|" + dashes(35) + "|" + dashes(38) + "| "; };
    out.push_back(" |" + dashes(117) + "| ");
    out.push_back(" |" + centre(t.title, 117) + "| ");
    out.push_back(" |" + dashes(42) + "|" + dashes(74) + "| ");
    out.push_back(" |" + std::string(42, ' ') + "|" + centre("Coeficientes parciales de seguridad (g)", 74) + "| ");
    out.push_back(" |" + std::string(42, ' ') + "|" + dashes(35) + "|" + dashes(38) + "| ");
    out.push_back(" |" + std::string(42, ' ') + "|" + centre("Favorable", 35) + "|" + centre("Desfavorable", 38) + "| ");
    out.push_back(separator2());
    bool firstRow = true;
    for (const auto& r : t.rows) {
        if (!firstRow) out.push_back(" |" + std::string(42, ' ') + "|" + std::string(35, ' ') + "|" + std::string(38, ' ') + "| ");
        firstRow = false;
        std::string label = r.label;
        const std::size_t len = utf8Length(label);
        if (len < 42) label += std::string(42 - len, ' ');
        out.push_back(" |" + label + "|" + centre(formatCoefficient(r.favourable, SectionLayout::Displacements), 35) + "|" +
                      centre(formatCoefficient(r.unfavourable, SectionLayout::Displacements), 38) + "| ");
    }
    out.push_back(separator2());
}

void tableLines(std::vector<std::string>& out, const CoefficientTable& t) {
    out.push_back(" |" + dashes(117) + "| ");
    out.push_back(" |" + centre(t.title, 117) + "| ");
    out.push_back(" |" + dashes(27) + "|" + dashes(46) + "|" + dashes(42) + "| ");
    out.push_back(" |" + std::string(27, ' ') + "|" + centre("Coeficientes parciales de seguridad (g)", 46) + "|" +
                  centre("Coeficientes de combinación (y)", 42) + "| ");
    out.push_back(" |" + std::string(27, ' ') + "|" + dashes(21) + "|" + dashes(24) + "|" + dashes(18) + "|" + dashes(23) +
                  "| ");
    out.push_back(" |" + std::string(27, ' ') + "|" + centre("Favorable", 21) + "|" + centre("Desfavorable", 24) + "|" +
                  centre("Principal (yp)", 18) + "|" + centre("Acompañamiento (ya)", 23) + "| ");
    out.push_back(separator());
    bool firstRow = true;
    for (const auto& r : t.rows) {
        if (!firstRow) out.push_back(spaceRow());
        firstRow = false;
        std::string label = r.label;
        const std::size_t len = utf8Length(label);
        if (len < 27) label += std::string(27 - len, ' ');
        out.push_back(" |" + label + "|" + centre(formatCoefficient(r.favourable), 21) + "|" +
                      centre(formatCoefficient(r.unfavourable), 24) + "|" + centre(formatCoefficient(r.principal), 18) +
                      "|" + centre(formatCoefficient(r.accompanying), 23) + "| ");
    }
    out.push_back(separator());
}

void legendLines(std::vector<std::string>& out, SectionLayout layout) {
    out.push_back("  -Donde: ");
    blanks(out, 9);
    for (const auto& item : legend(layout)) {
        std::string sym = item.symbol;
        sym.resize(4, ' ');
        out.push_back("    " + sym + " " + item.description + " ");
        out.emplace_back();
    }
    out.push_back("     ");
    blanks(out, 4);
}

}  // namespace

std::size_t utf8Length(const std::string& s) {
    std::size_t n = 0;
    for (unsigned char c : s) n += (c & 0xC0) != 0x80;
    return n;
}

std::string exportText(const CombinationReport& report, int pageWidth) {
    std::vector<std::string> out;
    out.emplace_back();
    for (std::size_t si = 0; si < report.sections.size(); ++si) {
        const auto& sec = report.sections[si];
        for (const auto& t : sec.titles) out.push_back(t + " ");
        out.emplace_back();
        for (const auto& l : wrap(introText(), pageWidth)) out.push_back(l);
        blanks(out, 2);
        if (sec.layout != SectionLayout::Service) {
            const bool disp = sec.layout == SectionLayout::Displacements;
            for (const char* name : {"persistentes o transitorias", "sísmicas", "accidentales"}) {
                if (disp && std::string(name) == "accidentales") break;
                out.push_back(std::string("  -Situaciones ") + name + " ");
                blanks(out, 4);
                out.push_back("   - ");
                blanks(out, 4);
            }
        } else {
            out.push_back("  - ");
            out.emplace_back();
        }
        legendLines(out, sec.layout);
        for (std::size_t ti = 0; ti < sec.tables.size(); ++ti) {
            if (sec.layout == SectionLayout::Displacements) displacementTableLines(out, sec.tables[ti]);
            else tableLines(out, sec.tables[ti]);
            out.push_back("   ");
            if (ti + 1 < sec.tables.size()) blanks(out, 3);
            else if (si + 1 < report.sections.size()) blanks(out, 2);
        }
    }
    std::string text;
    for (std::size_t i = 0; i < out.size(); ++i) {
        text += out[i];
        if (i + 1 < out.size()) text += "\r\n";
    }
    return text;
}

std::string utf8ToUtf16Le(const std::string& utf8, bool withBom) {
    std::string out;
    const auto put = [&out](std::uint32_t unit) {
        out.push_back(static_cast<char>(unit & 0xFF));
        out.push_back(static_cast<char>((unit >> 8) & 0xFF));
    };
    if (withBom) put(0xFEFF);
    for (std::size_t i = 0; i < utf8.size();) {
        const unsigned char c = static_cast<unsigned char>(utf8[i]);
        std::uint32_t cp;
        std::size_t len;
        if (c < 0x80) { cp = c; len = 1; }
        else if (c < 0xE0) { cp = c & 0x1F; len = 2; }
        else if (c < 0xF0) { cp = c & 0x0F; len = 3; }
        else { cp = c & 0x07; len = 4; }
        for (std::size_t k = 1; k < len && i + k < utf8.size(); ++k)
            cp = (cp << 6) | (static_cast<unsigned char>(utf8[i + k]) & 0x3F);
        i += len;
        if (cp >= 0x10000) {
            cp -= 0x10000;
            put(0xD800 + (cp >> 10));
            put(0xDC00 + (cp & 0x3FF));
        } else {
            put(cp);
        }
    }
    return out;
}

std::string exportTextUtf16(const CombinationReport& report, int pageWidth) {
    return utf8ToUtf16Le(exportText(report, pageWidth), true);
}

namespace {

std::string htmlEscape(const std::string& s) {
    std::string o;
    for (char c : s) {
        if (c == '&') o += "&amp;";
        else if (c == '<') o += "&lt;";
        else if (c == '>') o += "&gt;";
        else o += c;
    }
    return o;
}

std::string htmlSymbol(const std::string& ascii) {
    const auto p = prettySymbol(ascii);
    return p.subscript.empty() ? p.base : p.base + "<sub>" + htmlEscape(p.subscript) + "</sub>";
}

std::string rtfEscape(const std::string& utf8) {
    std::string o;
    for (std::size_t i = 0; i < utf8.size();) {
        const unsigned char c = static_cast<unsigned char>(utf8[i]);
        if (c < 0x80) {
            if (c == '\\' || c == '{' || c == '}') o += '\\';
            o += static_cast<char>(c);
            ++i;
            continue;
        }
        const std::size_t len = c < 0xE0 ? 2 : c < 0xF0 ? 3 : 4;
        const std::string u16 = utf8ToUtf16Le(utf8.substr(i, len), false);
        i += len;
        for (std::size_t k = 0; k + 1 < u16.size(); k += 2) {
            const int unit = static_cast<unsigned char>(u16[k]) | (static_cast<unsigned char>(u16[k + 1]) << 8);
            o += "\\u" + std::to_string(unit > 32767 ? unit - 65536 : unit) + "?";
        }
    }
    return o;
}

}  // namespace

std::string exportHtml(const CombinationReport& report) {
    std::ostringstream h;
    h << "<!DOCTYPE html>\n<html lang=\"es\">\n<head>\n<meta charset=\"utf-8\">\n<title>" << htmlEscape(report.documentName)
      << "</title>\n<style>body{font-family:Arial,sans-serif;font-size:10pt}table{border-collapse:collapse;margin:8px 0 "
         "20px}td,th{border:1px solid #000;padding:2px 6px;text-align:center}td:first-child{text-align:left}"
         "</style>\n</head>\n<body>\n";
    const auto& f = formulae();
    for (const auto& sec : report.sections) {
        for (const auto& t : sec.titles) h << "<h3>" << htmlEscape(t) << "</h3>\n";
        h << "<p>" << htmlEscape(introText()) << "</p>\n<ul>\n";
        if (sec.layout == SectionLayout::Displacements) {
            h << "<li>Situaciones persistentes o transitorias<br>" << htmlEscape(f.displacementsPersistent) << "</li>\n"
              << "<li>Situaciones sísmicas<br>" << htmlEscape(f.displacementsSeismic) << "</li>\n";
        } else if (sec.layout == SectionLayout::Ultimate) {
            h << "<li>Situaciones persistentes o transitorias<br>" << htmlEscape(f.persistent) << "</li>\n"
              << "<li>Situaciones sísmicas<br>" << htmlEscape(f.seismic) << "</li>\n"
              << "<li>Situaciones accidentales<br>" << htmlEscape(f.accidental) << "</li>\n";
        } else {
            h << "<li>" << htmlEscape(f.persistent) << "</li>\n";
        }
        h << "</ul>\n<p>Donde:</p>\n<table style=\"border:0\">\n";
        for (const auto& item : legend(sec.layout))
            h << "<tr><td style=\"border:0\">" << htmlSymbol(item.symbol) << "</td><td style=\"border:0;text-align:left\">"
              << htmlEscape(item.description) << "</td></tr>\n";
        h << "</table>\n";
        for (const auto& t : sec.tables) {
            if (sec.layout == SectionLayout::Displacements) {
                h << "<table>\n<tr><th colspan=\"3\">" << htmlEscape(t.title) << "</th></tr>\n"
                  << "<tr><th rowspan=\"2\"></th><th colspan=\"2\">Coeficientes parciales de seguridad (γ)</th></tr>\n"
                  << "<tr><th>Favorable</th><th>Desfavorable</th></tr>\n";
                for (const auto& r : t.rows)
                    h << "<tr><td>" << htmlEscape(r.label) << "</td><td>" << formatCoefficient(r.favourable, sec.layout)
                      << "</td><td>" << formatCoefficient(r.unfavourable, sec.layout) << "</td></tr>\n";
                h << "</table>\n";
                continue;
            }
            h << "<table>\n<tr><th colspan=\"5\">" << htmlEscape(t.title) << "</th></tr>\n"
              << "<tr><th rowspan=\"2\"></th><th colspan=\"2\">Coeficientes parciales de seguridad (γ)</th>"
                 "<th colspan=\"2\">Coeficientes de combinación (ψ)</th></tr>\n"
              << "<tr><th>Favorable</th><th>Desfavorable</th><th>Principal (ψ<sub>p</sub>)</th>"
                 "<th>Acompañamiento (ψ<sub>a</sub>)</th></tr>\n";
            for (const auto& r : t.rows)
                h << "<tr><td>" << htmlEscape(r.label) << "</td><td>" << formatCoefficient(r.favourable) << "</td><td>"
                  << formatCoefficient(r.unfavourable) << "</td><td>" << formatCoefficient(r.principal) << "</td><td>"
                  << formatCoefficient(r.accompanying) << "</td></tr>\n";
            h << "</table>\n";
        }
    }
    h << "</body>\n</html>\n";
    return h.str();
}

std::string exportRtf(const CombinationReport& report) {
    std::ostringstream r;
    r << "{\\rtf1\\ansi\\ansicpg1252\\deff0{\\fonttbl{\\f0\\fswiss Arial;}}\\fs20\n";
    const auto& f = formulae();
    const auto para = [&r](const std::string& s, bool bold = false) {
        r << "\\pard" << (bold ? "\\b " : " ") << rtfEscape(s) << (bold ? "\\b0" : "") << "\\par\n";
    };
    bool firstSection = true;
    for (const auto& sec : report.sections) {
        if (!firstSection) r << "\\page\n";
        firstSection = false;
        for (const auto& t : sec.titles) para(t, true);
        para(introText());
        if (sec.layout == SectionLayout::Displacements) {
            para("Situaciones persistentes o transitorias: " + std::string(f.displacementsPersistent));
            para("Situaciones sísmicas: " + std::string(f.displacementsSeismic));
        } else if (sec.layout == SectionLayout::Ultimate) {
            para("Situaciones persistentes o transitorias: " + std::string(f.persistent));
            para("Situaciones sísmicas: " + std::string(f.seismic));
            para("Situaciones accidentales: " + std::string(f.accidental));
        } else {
            para(f.persistent);
        }
        para("Donde:");
        for (const auto& item : legend(sec.layout)) {
            const auto p = prettySymbol(item.symbol);
            para(p.base + p.subscript + "  " + item.description);
        }
        for (const auto& t : sec.tables) {
            para(t.title, true);
            if (sec.layout == SectionLayout::Displacements) {
                para("Concepto | Favorable | Desfavorable");
                for (const auto& row : t.rows)
                    para(row.label + " | " + formatCoefficient(row.favourable, sec.layout) + " | " +
                         formatCoefficient(row.unfavourable, sec.layout));
                continue;
            }
            para("Concepto | Favorable | Desfavorable | Principal (ψp) | Acompañamiento (ψa)");
            for (const auto& row : t.rows)
                para(row.label + " | " + formatCoefficient(row.favourable) + " | " + formatCoefficient(row.unfavourable) +
                     " | " + formatCoefficient(row.principal) + " | " + formatCoefficient(row.accompanying));
        }
    }
    r << "}\n";
    return r.str();
}

}  // namespace cc::core
