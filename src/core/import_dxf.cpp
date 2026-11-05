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

/// Splits the ENTITIES section into entities; `lines` receives the number of lines read. When `layerColors` is given the colours of
/// the LAYER records of the TABLES section are stored in it.
std::vector<Entity> readEntities(const std::string& content, int& lines, bool& malformed, std::map<std::string, int>* layerColors = nullptr) {
    std::vector<std::string> raw;
    {
        std::istringstream in(content);
        std::string l;
        while (std::getline(in, l)) raw.push_back(l);
    }
    lines = static_cast<int>(raw.size());
    std::vector<Entity> out;
    bool inEntities = false, inTables = false, expectName = false, inLayerRecord = false;
    std::string layerName;
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
            inLayerRecord = false;
            if (v == "SECTION") {
                expectName = true;
                inEntities = false;
                inTables = false;
            } else if (v == "ENDSEC") {
                inEntities = false;
                inTables = false;
            } else if (v == "LAYER" && inTables) {
                inLayerRecord = true;
                layerName.clear();
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
            inTables = upper(value) == "TABLES";
            expectName = false;
            continue;
        }
        if (inLayerRecord && layerColors) {
            if (code == 2) {
                layerName = value;
                layerColors->try_emplace(layerName, 7);
            } else if (code == 62 && !layerName.empty()) {
                (*layerColors)[layerName] = std::abs(static_cast<int>(std::lround(number(value))));
            }
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
        if (distance(a, b) <= 1e-5) return;  // [REA] FUN_18009e0c0: a segment is added only when longer than 1e-5
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

/// Removes consecutive coincident corners (including last = first); fewer than 3 left -> empty ([REA] FUN_18009e030, 1e-5).
std::vector<Point3> cleanPolygon(const std::vector<Point3>& in) {
    std::vector<Point3> out;
    for (const Point3& p : in)
        if (out.empty() || distance(out.back(), p) > 1e-5) out.push_back(p);
    while (out.size() > 1 && distance(out.back(), out.front()) <= 1e-5) out.pop_back();
    if (out.size() < 3) out.clear();
    return out;
}

Point3 point(const Entity& e, int codeX) {
    Point3 p;
    for (const auto& [code, value] : e.codes) {
        if (code == codeX) p.x = number(value);
        else if (code == codeX + 10) p.y = number(value);
        else if (code == codeX + 20) p.z = number(value);
    }
    return p;
}

int intCode(const Entity& e, int code, int fallback = 0) {
    for (const auto& [c, v] : e.codes)
        if (c == code) return static_cast<int>(std::lround(number(v)));
    return fallback;
}

bool wanted(const DxfOptions& o, const std::string& layer) { return o.layers.empty() || o.layers.count(layer) != 0; }

bool supported(const std::string& type) { return type == "LINE" || type == "LWPOLYLINE" || type == "POLYLINE" || type == "3DFACE"; }

/// What a POLYLINE entity holds: its vertices and, for a mesh, the polygons as lists of vertex indices (0-based).
struct PolylineData {
    std::vector<Point3> vertices;
    std::vector<std::vector<std::size_t>> faces;  ///< non-empty for polyface and polygon meshes
    bool closed = false;
    bool mesh = false;
};

/// Reads the POLYLINE at entities[i] and its VERTEX records; `i` is left on the SEQEND.
PolylineData readPolyline(const std::vector<Entity>& entities, std::size_t& i) {
    const Entity& head = entities[i];
    PolylineData d;
    const int flags = intCode(head, 70);
    d.closed = (flags & 1) != 0;
    std::vector<const Entity*> records;
    std::size_t j = i + 1;
    for (; j < entities.size() && entities[j].type != "SEQEND"; ++j)
        if (entities[j].type == "VERTEX") records.push_back(&entities[j]);
    i = j;  // the vertices belong to the polyline, whatever their own layer
    if ((flags & 64) != 0) {  // polyface mesh: position records (flag 64), then face records (flag 128 without 64)
        d.mesh = true;
        for (const Entity* r : records) {
            const int f = intCode(*r, 70);
            if ((f & 64) != 0) {
                d.vertices.push_back(point(*r, 10));
            } else if ((f & 128) != 0) {
                std::vector<std::size_t> face;
                for (int code = 71; code <= 74; ++code) {
                    const int idx = std::abs(intCode(*r, code));  // a negative index only marks an invisible edge
                    if (idx >= 1 && static_cast<std::size_t>(idx) <= d.vertices.size()) face.push_back(static_cast<std::size_t>(idx - 1));
                }
                d.faces.push_back(face);
            }
        }
    } else if ((flags & 16) != 0) {  // polygon mesh M x N (closure flags 1 and 32)
        d.mesh = true;
        for (const Entity* r : records) d.vertices.push_back(point(*r, 10));
        const int m = intCode(head, 71), n = intCode(head, 72);
        if (m >= 2 && n >= 2 && d.vertices.size() >= static_cast<std::size_t>(m) * static_cast<std::size_t>(n)) {
            const int cm = (flags & 1) != 0 ? m : m - 1, cn = (flags & 32) != 0 ? n : n - 1;
            const auto at = [&](int a, int b) { return static_cast<std::size_t>((a % m) * n + (b % n)); };
            for (int a = 0; a < cm; ++a)
                for (int b = 0; b < cn; ++b) d.faces.push_back({at(a, b), at(a + 1, b), at(a + 1, b + 1), at(a, b + 1)});
        }
    } else {
        for (const Entity* r : records) d.vertices.push_back(point(*r, 10));
    }
    return d;
}

std::vector<DxfLayerInfo> layerInfoOf(const std::vector<Entity>& entities, const std::map<std::string, int>& colours) {
    std::vector<DxfLayerInfo> layers;
    const auto find = [&](const std::string& name) -> DxfLayerInfo& {
        for (DxfLayerInfo& l : layers)
            if (l.name == name) return l;
        const auto it = colours.find(name);
        layers.push_back({name, it == colours.end() || it->second <= 0 || it->second > 255 ? 7 : it->second, 0, 0});
        return layers.back();
    };
    for (std::size_t i = 0; i < entities.size(); ++i) {
        const Entity& e = entities[i];
        if (!supported(e.type)) continue;
        DxfLayerInfo& l = find(e.layer);
        if (e.type == "LINE") {
            ++l.segments;
        } else if (e.type == "3DFACE") {
            ++l.polygons;
        } else if (e.type == "LWPOLYLINE") {
            int count = 0;
            for (const auto& [code, v] : e.codes) count += code == 10 ? 1 : 0;
            l.segments += std::max(0, count - 1);
        } else {
            const std::string layer = e.layer;
            const PolylineData d = readPolyline(entities, i);
            DxfLayerInfo& owner = find(layer);  // `find` may have moved the vector: look the layer up again
            if (d.mesh) owner.polygons += static_cast<int>(d.faces.size());
            else owner.segments += std::max(0, static_cast<int>(d.vertices.size()) - 1);
        }
    }
    return layers;
}

std::vector<std::string> layersOf(const std::vector<Entity>& entities) {
    std::vector<std::string> names;
    for (const DxfLayerInfo& l : layerInfoOf(entities, {})) names.push_back(l.name);
    return names;
}

}  // namespace

std::vector<std::string> dxfLayers(const std::string& content) {
    int lines = 0;
    bool malformed = false;
    return layersOf(readEntities(content, lines, malformed));
}

std::vector<DxfLayerInfo> dxfLayerInfo(const std::string& content) {
    int lines = 0;
    bool malformed = false;
    std::map<std::string, int> colours;
    const std::vector<Entity> entities = readEntities(content, lines, malformed, &colours);
    return malformed ? std::vector<DxfLayerInfo>{} : layerInfoOf(entities, colours);
}

unsigned aciToRgb(int index) {
    static const unsigned fixed[] = {0x000000, 0xFF0000, 0xFFFF00, 0x00FF00, 0x00FFFF, 0x0000FF, 0xFF00FF, 0xFFFFFF, 0x414141, 0x808080};
    if (index < 0 || index > 255) index = 7;
    if (index < 10) return fixed[index];
    if (index >= 250) {
        const unsigned g = static_cast<unsigned>(51 + (index - 250) * 41);
        return (g << 16) | (g << 8) | g;
    }
    const int n = index - 10;
    const double hue = (n / 10) * 15.0;  // 24 hues, 15 degrees apart
    static const double sat[] = {1.0, 0.5, 1.0, 0.5, 1.0, 0.5, 1.0, 0.5, 1.0, 0.5};
    static const double val[] = {1.0, 1.0, 0.65, 0.65, 0.5, 0.5, 0.3, 0.3, 0.15, 0.15};
    const double s = sat[n % 10], v = val[n % 10];
    const double c = v * s, x = c * (1.0 - std::fabs(std::fmod(hue / 60.0, 2.0) - 1.0)), m = v - c;
    double r = 0, g = 0, b = 0;
    switch (static_cast<int>(hue / 60.0) % 6) {
        case 0: r = c; g = x; break;
        case 1: r = x; g = c; break;
        case 2: g = c; b = x; break;
        case 3: g = x; b = c; break;
        case 4: r = x; b = c; break;
        default: r = c; b = x; break;
    }
    const auto byte = [&](double u) { return static_cast<unsigned>(std::lround((u + m) * 255.0)); };
    return (byte(r) << 16) | (byte(g) << 8) | byte(b);
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
    const auto emitChain = [&](const std::vector<Point3>& vertices, bool closed) {
        const std::size_t count = vertices.size();
        if (count < 2) return;
        for (std::size_t k = 0; k + 1 < count; ++k) b.bar(vertices[k], vertices[k + 1]);
        if (closed && count > 2) b.bar(vertices[count - 1], vertices[0]);
    };
    const auto emitPolygon = [&](const std::vector<Point3>& corners, const std::string& layer) {
        const std::vector<Point3> clean = cleanPolygon(corners);
        if (clean.empty()) return;
        auto& target = options.panelLayers.count(layer) != 0 ? result.geometry.panels : result.geometry.shells;
        ImportedPolygon polygon;
        polygon.number = static_cast<long long>(target.size()) + 1;
        for (const Point3& p : clean) polygon.nodeNumbers.push_back(b.node(p));
        target.push_back(polygon);
    };
    for (std::size_t i = 0; i < entities.size(); ++i) {
        const Entity& e = entities[i];
        if (!supported(e.type)) continue;
        const bool take = wanted(options, e.layer);
        if (e.type == "POLYLINE") {
            const PolylineData d = readPolyline(entities, i);  // always consumed up to its SEQEND
            if (!take) continue;
            if (d.mesh) {
                for (const auto& face : d.faces) {
                    std::vector<Point3> corners;
                    for (std::size_t k : face) corners.push_back(d.vertices[k]);
                    emitPolygon(corners, e.layer);
                }
            } else {
                emitChain(d.vertices, d.closed);
            }
            continue;
        }
        if (!take) continue;
        if (e.type == "LINE") {
            b.bar(point(e, 10), point(e, 11));
        } else if (e.type == "3DFACE") {
            emitPolygon({point(e, 10), point(e, 11), point(e, 12), point(e, 13)}, e.layer);
        } else {  // LWPOLYLINE
            std::vector<Point3> vertices;
            bool closed = false;
            double elevation = 0;
            for (const auto& [code, value] : e.codes) {
                if (code == 38) elevation = number(value);
                else if (code == 70) closed = (std::lround(number(value)) & 1) != 0;
            }
            for (const auto& [code, value] : e.codes) {
                if (code == 10) vertices.push_back({number(value), 0, elevation});
                else if (code == 20 && !vertices.empty()) vertices.back().y = number(value);
            }
            emitChain(vertices, closed);
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
