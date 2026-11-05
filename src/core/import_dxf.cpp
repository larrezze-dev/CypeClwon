#include "core/import_dxf.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <map>
#include <sstream>

namespace cc::core {

namespace {

struct Entity {
    std::string type;
    std::string layer = "0";
    std::vector<std::pair<int, std::string>> codes;
};

std::string trim(const std::string& s) {
    std::size_t a = 0, b = s.size();
    while (a < b && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
    while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) --b;
    return s.substr(a, b - a);
}

std::string upper(std::string s) {
    for (char& c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return s;
}

double number(const std::string& s) {
    return std::strtod(trim(s).c_str(), nullptr);
}

/// Splits the ENTITIES section into entities; `lines` receives the number of lines read.
std::vector<Entity> readEntities(const std::string& content, int& lines, bool& malformed) {
    std::vector<std::string> raw;
    {
        std::istringstream in(content);
        std::string l;
        while (std::getline(in, l)) raw.push_back(l);
    }
    lines = static_cast<int>(raw.size());
    std::vector<Entity> out;
    bool inEntities = false, expectName = false;
    std::string sectionName;
    Entity* current = nullptr;
    for (std::size_t i = 0; i + 1 < raw.size(); i += 2) {
        const std::string codeText = trim(raw[i]);
        char* end = nullptr;
        const long long code = std::strtol(codeText.c_str(), &end, 10);
        if (codeText.empty() || *end != '\0') {
            malformed = true;
            return out;
        }
        const std::string value = trim(raw[i + 1]);
        if (code == 0) {
            const std::string v = upper(value);
            current = nullptr;
            if (v == "SECTION") {
                expectName = true;
                inEntities = false;
            } else if (v == "ENDSEC") {
                inEntities = false;
            } else if (v == "EOF") {
                break;
            } else if (inEntities) {
                out.push_back({v, "0", {}});
                current = &out.back();
            }
            continue;
        }
        if (expectName && code == 2) {
            inEntities = upper(value) == "ENTITIES";
            expectName = false;
            continue;
        }
        if (current) {
            if (code == 8) current->layer = value;
            current->codes.push_back({static_cast<int>(code), value});
        }
    }
    return out;
}

struct Builder {
    const DxfOptions& options;
    DxfReadResult& result;
    std::map<std::tuple<long long, long long, long long>, long long> nodeByKey;
    std::map<std::pair<long long, long long>, long long> barByPair;

    long long node(const Point3& p) {
        const double q = kDxfNodeToleranceM;
        const auto key = std::make_tuple(std::llround(p.x / q), std::llround(p.y / q), std::llround(p.z / q));
        if (const auto it = nodeByKey.find(key); it != nodeByKey.end()) return it->second;
        const long long n = static_cast<long long>(result.geometry.nodes.size()) + 1;
        result.geometry.nodes.push_back({n, p});
        nodeByKey[key] = n;
        return n;
    }

    void bar(const Point3& a, const Point3& b) {
        if (distance(a, b) > kDxfMaxBarLengthM) {
            ++result.skippedLongBars;
            return;
        }
        if (distance(a, b) < kDxfNodeToleranceM) return;  // a point, not a bar
        const long long na = node(a), nb = node(b);
        if (na == nb) return;
        const auto key = std::make_pair(std::min(na, nb), std::max(na, nb));
        if (barByPair.count(key)) {
            ++result.skippedRepeatedBars;
            return;
        }
        const long long n = static_cast<long long>(result.geometry.bars.size()) + 1;
        barByPair[key] = n;
        result.geometry.bars.push_back({n, na, nb});
    }
};

Point3 point(const Entity& e, int codeX) {
    Point3 p;
    for (const auto& [code, value] : e.codes) {
        if (code == codeX) p.x = number(value);
        else if (code == codeX + 10) p.y = number(value);
        else if (code == codeX + 20) p.z = number(value);
    }
    return p;
}

bool wanted(const DxfOptions& o, const std::string& layer) { return o.layers.empty() || o.layers.count(layer) != 0; }

bool supported(const std::string& type) { return type == "LINE" || type == "LWPOLYLINE" || type == "POLYLINE" || type == "3DFACE"; }

std::vector<std::string> layersOf(const std::vector<Entity>& entities) {
    std::vector<std::string> layers;
    for (const Entity& e : entities)
        if (supported(e.type) && std::find(layers.begin(), layers.end(), e.layer) == layers.end()) layers.push_back(e.layer);
    return layers;
}

}  // namespace

std::vector<std::string> dxfLayers(const std::string& content) {
    int lines = 0;
    bool malformed = false;
    return layersOf(readEntities(content, lines, malformed));
}

DxfReadResult readDxf(const std::string& content, const DxfOptions& options) {
    DxfReadResult result;
    if (content.rfind("AutoCAD Binary DXF", 0) == 0 || content.rfind("AC10", 0) == 0) {
        result.errors.push_back({0, "Formato no soportado: solo se pueden leer ficheros DXF en formato ASCII.", 0});
        return result;
    }
    bool malformed = false;
    const std::vector<Entity> entities = readEntities(content, result.linesRead, malformed);
    if (malformed) {
        result.errors.push_back({60948, "No se encontraron entidades en el fichero. El fichero es erróneo.", 0});
        return result;
    }
    result.layers = layersOf(entities);
    if (result.layers.empty()) {
        result.errors.push_back({60948, "No se encontraron entidades en el fichero. El fichero es erróneo.", 0});
        return result;
    }
    if (!options.layers.empty()) {
        bool any = false;
        for (const std::string& l : result.layers) any = any || options.layers.count(l) != 0;
        if (!any) {
            result.errors.push_back({64993, "Las capas seleccionadas no tienen elementos asociados.", 0});
            return result;
        }
    }

    Builder b{options, result, {}, {}};
    for (std::size_t i = 0; i < entities.size(); ++i) {
        const Entity& e = entities[i];
        if (!supported(e.type) || !wanted(options, e.layer)) continue;
        if (e.type == "LINE") {
            b.bar(point(e, 10), point(e, 11));
        } else if (e.type == "3DFACE") {
            std::vector<Point3> corners{point(e, 10), point(e, 11), point(e, 12), point(e, 13)};
            if (distance(corners[3], corners[2]) < kDxfNodeToleranceM) corners.pop_back();  // triangle
            ImportedPolygon shell;
            shell.number = static_cast<long long>(result.geometry.shells.size()) + 1;
            for (const Point3& p : corners) shell.nodeNumbers.push_back(b.node(p));
            result.geometry.shells.push_back(shell);
        } else {
            std::vector<Point3> vertices;
            bool closed = false;
            if (e.type == "LWPOLYLINE") {
                double elevation = 0;
                for (const auto& [code, value] : e.codes) {
                    if (code == 38) elevation = number(value);
                    else if (code == 70) closed = (std::lround(number(value)) & 1) != 0;
                }
                for (const auto& [code, value] : e.codes) {
                    if (code == 10) vertices.push_back({number(value), 0, elevation});
                    else if (code == 20 && !vertices.empty()) vertices.back().y = number(value);
                }
            } else {  // POLYLINE followed by VERTEX entities up to SEQEND
                for (const auto& [code, value] : e.codes)
                    if (code == 70) closed = (std::lround(number(value)) & 1) != 0;
                std::size_t j = i + 1;
                for (; j < entities.size() && entities[j].type != "SEQEND"; ++j)
                    if (entities[j].type == "VERTEX") vertices.push_back(point(entities[j], 10));
                i = j;  // the vertices belong to the polyline, whatever their own layer
            }
            const std::size_t count = vertices.size();
            if (count < 2) continue;
            for (std::size_t k = 0; k + 1 < count; ++k) b.bar(vertices[k], vertices[k + 1]);
            if (closed && count > 2) b.bar(vertices[count - 1], vertices[0]);
            if (closed && options.polygonsAsPanels && count >= 3) {
                ImportedPolygon panel;
                panel.number = static_cast<long long>(result.geometry.panels.size()) + 1;
                for (const Point3& p : vertices) panel.nodeNumbers.push_back(b.node(p));
                result.geometry.panels.push_back(panel);
            }
        }
    }
    if (result.skippedLongBars > 0)
        result.warnings.push_back({65115,
                                   "Algunas barras no se han podido importar. Revise la geometría introducida en el fichero DXF/DWG. Puede que haya barras de "
                                   "longitud excesiva (más de mil metros), barras solapadas entre sí, etc.",
                                   0});
    if (result.skippedRepeatedBars > 0)
        result.warnings.push_back({64502, "No es posible importar la barra debido a que ya existe otra entre las mismas coordenadas.", 0});
    if (result.geometry.bars.empty() && result.geometry.shells.empty() && result.geometry.panels.empty())
        result.errors.push_back({60948, "No se encontraron entidades en el fichero. El fichero es erróneo.", 0});
    return result;
}

DxfReadResult readDxfFile(const std::filesystem::path& path, const DxfOptions& options) {
    DxfReadResult result;
    std::string ext = path.extension().string();
    for (char& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    std::error_code ec;
    if (!std::filesystem::exists(path, ec)) {
        result.errors.push_back({71697, "No existe el fichero " + path.string(), 0});
        return result;
    }
    if (ext == ".dwg") {
        result.errors.push_back({0, "Formato no soportado: los ficheros DWG (binarios) no se pueden leer todavía; guarde el dibujo como DXF.", 0});
        return result;
    }
    std::ifstream in(path, std::ios::binary);
    std::ostringstream buf;
    buf << in.rdbuf();
    return readDxf(buf.str(), options);
}

}  // namespace cc::core
