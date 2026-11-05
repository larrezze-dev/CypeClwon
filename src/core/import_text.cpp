#include "core/import_text.h"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <map>
#include <set>
#include <sstream>

namespace cc::core {

namespace {

enum class TextSection { None, Nodes, Bars, Shells, Panels };

std::string trim(const std::string& s) {
    std::size_t a = 0, b = s.size();
    while (a < b && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
    while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) --b;
    return s.substr(a, b - a);
}

/// Upper-case ASCII; the accented letters of the keywords are mapped to their plain form (UTF-8 sequences A, E, N with tilde).
std::string keyword(const std::string& s) {
    std::string out;
    for (std::size_t i = 0; i < s.size(); ++i) {
        const unsigned char c = static_cast<unsigned char>(s[i]);
        if (c == 0xC3 && i + 1 < s.size()) {
            const unsigned char d = static_cast<unsigned char>(s[i + 1]);
            ++i;
            if (d == 0x81 || d == 0xA1) out += 'A';
            else if (d == 0x91 || d == 0xB1) out += 'N';
            else out += '?';
            continue;
        }
        out += static_cast<char>(std::toupper(c));
    }
    return out;
}

TextSection sectionOf(const std::string& line) {
    const std::string k = keyword(trim(line));
    if (k == "NUDOS" || k == "JOINTS") return TextSection::Nodes;
    if (k == "BARRAS" || k == "FRAMES") return TextSection::Bars;
    if (k == "LAMINAS" || k == "SHELLS") return TextSection::Shells;
    if (k == "PANOS" || k == "PANELS") return TextSection::Panels;
    return TextSection::None;
}

std::vector<std::string> split(const std::string& s) {
    std::vector<std::string> out;
    std::istringstream in(s);
    std::string tok;
    while (in >> tok) out.push_back(tok);
    return out;
}

/// Whole-token integer. False for anything else (including overflow).
bool parseInteger(const std::string& s, long long& out) {
    if (s.empty()) return false;
    std::size_t i = 0;
    if (s[0] == '+' || s[0] == '-') i = 1;
    if (i >= s.size()) return false;
    for (std::size_t k = i; k < s.size(); ++k)
        if (!std::isdigit(static_cast<unsigned char>(s[k]))) return false;
    if (s.size() - i > 15) {  // far beyond the maximum: still an integer, clamp
        out = s[0] == '-' ? -1000000000000000LL : 1000000000000000LL;
        return true;
    }
    out = std::stoll(s);
    return true;
}

bool parseNumber(std::string s, double& out) {
    std::replace(s.begin(), s.end(), ',', '.');
    if (s.empty()) return false;
    char* end = nullptr;
    errno = 0;
    out = std::strtod(s.c_str(), &end);
    return end == s.c_str() + s.size() && std::isfinite(out);
}

/// `X=1.5` -> 1.5 (the letter is case-insensitive).
bool parseCoordinate(const std::string& tok, char axis, double& out) {
    if (tok.size() < 3 || std::toupper(static_cast<unsigned char>(tok[0])) != axis || tok[1] != '=') return false;
    return parseNumber(tok.substr(2), out);
}

struct Line {
    long long number;  // 1-based line of the file
    std::string text;
};

}  // namespace

std::string importTextMessage(int id, long long a, long long b) {
    auto withNumbers = [&](const char* before, const char* middle, const char* after) {
        return std::string(before) + std::to_string(a) + middle + std::to_string(b) + after;
    };
    switch (id) {
        case 106854: return "Error: Numeración de nudo no positiva";
        case 106855: return "Error: Numeración de nudo mayor que 999999";
        case 106856: return "Error: Numeración de nudo ya existente";
        case 106857: return "Error: Coordenadas iguales a un nudo ya existente (nudo " + std::to_string(a) + ")";
        case 106859: return "Error: Sintaxis irreconocible";
        case 106860: return "Error: Numeración de usuario negativa";
        case 106861: return "Barra con nudo inicial y final iguales";
        case 106862: return "Error: numeración de barra no positiva";
        case 106863: return "Error: Numeración de barra mayor que 999999";
        case 106864: return "Error: Numeración de nudo en barra no positiva";
        case 106865: return "Error: Numeración de nudo en barra mayor que 999999";
        case 106866: return "Error: Numeración de barra ya existente";
        case 106867: return withNumbers("Error: Nudo inexistente en barra (barra: ", " nudo: ", ").");
        case 106868: return withNumbers("!! Error: Barra con conectividades ya existentes (", " = ", ").");
        case 106869: return "Error: Error de sintaxis en lectura de barra";
        case 106870: return "Error: Fichero vacío";
        case 106873: return "Error: No se encontraron todos los sectores de datos";
        case 106877: return "Error: No se ha leído ninguna barra ni ninguna lámina";
        case 106884: return "Error: Error de sintaxis en lectura de lámina";
        case 106885: return "Error: Numeración de lámina no positiva";
        case 106886: return "Error: Numeración de lámina mayor que 999999";
        case 106887: return "Error: Lámina con nudos repetidos";
        case 106888: return "Error: Numeración de lámina '" + std::to_string(a) + "' ya existente";
        case 106889: return withNumbers("Error: Nudo inexistente en lámina (lámina: ", ", nudo: ", ").");
        case 106890: return withNumbers("Error: Lámina con conectividades ya existentes (", " = ", ").");
        case 399536: return "Error: Sintaxis incorrecta en la lectura de paño de cargas";
        case 399537: return "Error: Numeración de paño de cargas no positiva";
        case 399538: return "Error: Numeración de paño de cargas mayor que 999999";
        case 399539: return "Error: Paño de carga con nudos repetidos";
        case 399540: return "Error: Numeración de paño de cargas '" + std::to_string(a) + "' ya existente";
        case 399541: return withNumbers("Error: Paño de cargas con conectividades ya existentes (", " = ", ").");
        default: return {};
    }
}

std::string importTotalsMessage(long long nodes, long long bars) {
    return "Total: " + std::to_string(nodes) + " nudos, " + std::to_string(bars) + " barras";
}

TextImportResult parseGeometryText(const std::string& text) {
    TextImportResult result;
    auto fail = [&](int id, long long line, long long a = 0, long long b = 0) { result.errors.push_back({id, importTextMessage(id, a, b), line}); };

    std::map<TextSection, std::vector<Line>> lines;
    std::set<TextSection> present;
    TextSection current = TextSection::None;
    {
        std::istringstream in(text);
        std::string raw;
        long long no = 0;
        while (std::getline(in, raw)) {
            ++no;
            if (no == 1 && raw.size() >= 3 && static_cast<unsigned char>(raw[0]) == 0xEF) raw.erase(0, 3);  // UTF-8 BOM
            const std::string t = trim(raw);
            if (t.empty()) continue;
            ++result.linesRead;
            const TextSection s = sectionOf(t);
            if (s != TextSection::None) {
                current = s;
                present.insert(s);
                continue;
            }
            if (current == TextSection::None) {
                fail(106859, no);
                continue;
            }
            lines[current].push_back({no, t});
        }
    }
    if (result.linesRead == 0) {
        fail(106870, 0);
        return result;
    }
    if (!present.count(TextSection::Nodes) || (!present.count(TextSection::Bars) && !present.count(TextSection::Shells))) fail(106873, 0);

    // --- nodes ---
    std::map<long long, Point3> nodePos;
    for (const Line& l : lines[TextSection::Nodes]) {
        const auto tok = split(l.text);
        long long n = 0;
        double x = 0, y = 0, z = 0;
        if (tok.size() != 4 || !parseInteger(tok[0], n) || !parseCoordinate(tok[1], 'X', x) || !parseCoordinate(tok[2], 'Y', y) ||
            !parseCoordinate(tok[3], 'Z', z)) {
            fail(106859, l.number);
            continue;
        }
        if (n < 0) { fail(106860, l.number); continue; }
        if (n == 0) { fail(106854, l.number); continue; }
        if (n > kMaxImportNumber) { fail(106855, l.number); continue; }
        if (nodePos.count(n)) { fail(106856, l.number); continue; }
        bool same = false;
        for (const auto& [other, p] : nodePos)
            if (p.x == x && p.y == y && p.z == z) {
                fail(106857, l.number, other);
                same = true;
                break;
            }
        if (same) continue;
        nodePos[n] = {x, y, z};
        result.geometry.nodes.push_back({n, {x, y, z}});
    }

    // --- bars ---
    std::set<long long> barNumbers;
    std::set<std::pair<long long, long long>> barPairs;
    std::map<std::pair<long long, long long>, long long> pairOwner;
    for (const Line& l : lines[TextSection::Bars]) {
        const auto tok = split(l.text);
        long long n = 0, a = 0, b = 0;
        if (tok.size() != 3 || !parseInteger(tok[0], n) || !parseInteger(tok[1], a) || !parseInteger(tok[2], b)) {
            fail(106869, l.number);
            continue;
        }
        if (n <= 0) { fail(106862, l.number); continue; }
        if (n > kMaxImportNumber) { fail(106863, l.number); continue; }
        if (a <= 0 || b <= 0) { fail(106864, l.number); continue; }
        if (a > kMaxImportNumber || b > kMaxImportNumber) { fail(106865, l.number); continue; }
        if (a == b) { fail(106861, l.number); continue; }
        if (barNumbers.count(n)) { fail(106866, l.number); continue; }
        if (!nodePos.count(a)) { fail(106867, l.number, n, a); continue; }
        if (!nodePos.count(b)) { fail(106867, l.number, n, b); continue; }
        const auto key = std::make_pair(std::min<long long>(a, b), std::max<long long>(a, b));
        if (const auto it = pairOwner.find(key); it != pairOwner.end()) { fail(106868, l.number, n, it->second); continue; }
        barNumbers.insert(n);
        pairOwner[key] = n;
        result.geometry.bars.push_back({n, a, b});
    }

    // --- shells and panels (same rules, different message ids) ---
    struct Ids { int syntax, nonPositive, tooBig, repeatedNodes, repeatedNumber, missingNode, connectivity; };
    auto polygons = [&](TextSection section, const Ids& ids, std::vector<ImportedPolygon>& out) {
        std::set<long long> numbers;
        std::map<std::vector<long long>, long long> owner;
        for (const Line& l : lines[section]) {
            const auto tok = split(l.text);
            long long n = 0;
            std::vector<long long> nodes;
            bool good = tok.size() >= 4 && parseInteger(tok[0], n);
            for (std::size_t i = 1; good && i < tok.size(); ++i) {
                long long v = 0;
                good = parseInteger(tok[i], v) && v > 0 && v <= kMaxImportNumber;
                nodes.push_back(static_cast<long long>(v));
            }
            if (!good) { fail(ids.syntax, l.number); continue; }
            if (n <= 0) { fail(ids.nonPositive, l.number); continue; }
            if (n > kMaxImportNumber) { fail(ids.tooBig, l.number); continue; }
            std::set<long long> distinct(nodes.begin(), nodes.end());
            if (distinct.size() != nodes.size()) { fail(ids.repeatedNodes, l.number); continue; }
            if (numbers.count(n)) { fail(ids.repeatedNumber, l.number, n); continue; }
            long long missing = 0;
            for (long long v : nodes)
                if (!nodePos.count(v)) { missing = v; break; }
            if (missing != 0) { fail(ids.missingNode, l.number, n, missing); continue; }
            const std::vector<long long> key(distinct.begin(), distinct.end());
            if (const auto it = owner.find(key); it != owner.end()) { fail(ids.connectivity, l.number, n, it->second); continue; }
            numbers.insert(n);
            owner[key] = n;
            out.push_back({n, nodes});
        }
    };
    polygons(TextSection::Shells, {106884, 106885, 106886, 106887, 106888, 106889, 106890}, result.geometry.shells);
    polygons(TextSection::Panels, {399536, 399537, 399538, 399539, 399540, 399536, 399541}, result.geometry.panels);

    if (result.errors.empty() && result.geometry.bars.empty() && result.geometry.shells.empty()) fail(106877, 0);
    std::stable_sort(result.errors.begin(), result.errors.end(), [](const ImportMessage& a, const ImportMessage& b) { return a.line < b.line; });
    return result;
}

}  // namespace cc::core
