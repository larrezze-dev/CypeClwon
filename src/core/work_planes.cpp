#include "core/work_planes.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace cc::core {

using json = nlohmann::json;

namespace {

constexpr double kSameValue = 1e-9;  // tolerance for "same coordinate" / "same elevation" (the original's value is not known)

/// Replaces the successive "%s" of `pattern` by `args`.
std::string format(std::string pattern, std::initializer_list<std::string> args) {
    for (const std::string& arg : args) {
        const std::size_t at = pattern.find("%s");
        if (at == std::string::npos) break;
        pattern.replace(at, 2, arg);
    }
    return pattern;
}

bool inRange(double coordinate) { return std::isfinite(coordinate) && std::fabs(coordinate) <= kMaxGridCoordinate; }

bool sameValue(double a, double b) { return std::fabs(a - b) <= kSameValue; }

// Measured in the original: 3.000 and 3.001 are the same coordinate, 3.002 is not (the small term absorbs binary rounding).
bool sameCoordinate(double a, double b) { return std::fabs(a - b) <= kSameCoordinate + 1e-9; }

bool validGroups(const std::vector<std::vector<double>>& groups) {
    std::vector<double> seen;
    for (const auto& group : groups)
        for (double coordinate : group) {
            if (!inRange(coordinate)) return false;
            // A plane cannot be in two groups (nor twice in the same one).
            for (double other : seen)
                if (sameValue(other, coordinate)) return false;
            seen.push_back(coordinate);
        }
    return true;
}

const char* positionName(LabelPosition p) { return p == LabelPosition::End ? "end" : "origin"; }

}  // namespace

bool WorkPlanes::isValid() const {
    if (!validateGridLines(gridX).empty() || !validateGridLines(gridY).empty()) return false;
    if (!levels.empty()) {
        if (!validateLevels(levels).empty()) return false;
        for (const Level& level : levels)
            if (!std::isfinite(level.elevation)) return false;
    }
    return validGroups(groups.x) && validGroups(groups.y) && validGroups(groups.z);
}

double roundCoordinate(double value) { return std::round(value * 1000.0) / 1000.0; }

GridLine newGridLine() { return GridLine{}; }

Level newLevel() { return Level{"Nuevo nivel", 0.0}; }

std::string nextReference(const std::string& label) {
    if (label.empty()) return "1";
    std::string out = label;
    const auto isDigit = [](char c) { return c >= '0' && c <= '9'; };
    const auto isUpper = [](char c) { return c >= 'A' && c <= 'Z'; };
    const auto isLower = [](char c) { return c >= 'a' && c <= 'z'; };
    if (isDigit(out.back())) {
        int i = static_cast<int>(out.size()) - 1;
        while (i >= 0 && isDigit(out[i])) {
            if (out[i] == '9') {
                out[i] = '0';
                --i;
            } else {
                ++out[i];
                return out;
            }
        }
        out.insert(static_cast<std::size_t>(i + 1), "1");  // every digit of the run carried (9 -> 10)
        return out;
    }
    if (isUpper(out.back()) || isLower(out.back())) {
        const bool upper = isUpper(out.back());
        const char first = upper ? 'A' : 'a';
        const char last = upper ? 'Z' : 'z';
        int i = static_cast<int>(out.size()) - 1;
        while (i >= 0 && (upper ? isUpper(out[i]) : isLower(out[i]))) {
            if (out[i] == last) {
                out[i] = first;
                --i;
            } else {
                ++out[i];
                return out;
            }
        }
        out.insert(static_cast<std::size_t>(i + 1), 1, first);  // Z -> AA, ZZ -> AAA
        return out;
    }
    return "1";
}

std::vector<GridLine> generateGridAxis(const GridAxisSpec& spec) {
    if (spec.count < kMinGridLines || spec.count > kMaxGridLines)
        throw std::invalid_argument("El número de líneas debe estar entre 2 y 1000.");
    if (!std::isfinite(spec.spacing)) throw std::invalid_argument("La separación entre líneas no es válida.");
    if (!inRange(spec.spacing * (spec.count - 1)))
        throw std::invalid_argument("Las coordenadas de las líneas deben estar entre -50000 y 50000 m.");
    std::vector<GridLine> lines;
    lines.reserve(static_cast<std::size_t>(spec.count));
    std::string label = spec.firstLabel;
    for (int i = 0; i < spec.count; ++i) {
        if (i > 0) label = nextReference(label);
        lines.push_back(GridLine{label, roundCoordinate(i * spec.spacing), spec.position});
    }
    return lines;
}

void generateGrid(WorkPlanes& planes, const GridSpec& spec) {
    std::vector<GridLine> x = generateGridAxis(spec.x);
    std::vector<GridLine> y = generateGridAxis(spec.y);  // both are built before anything is replaced
    planes.gridX = std::move(x);
    planes.gridY = std::move(y);
}

std::vector<Level> generateLevels(const std::string& prefix, int count, double startElevation, double spacing) {
    if (count < kMinLevels || !std::isfinite(startElevation) || !std::isfinite(spacing))
        throw std::invalid_argument("Debe definir al menos dos niveles.");
    std::vector<Level> levels;
    levels.reserve(static_cast<std::size_t>(count));
    for (int i = 0; i < count; ++i)
        levels.push_back(Level{prefix + " " + std::to_string(i), startElevation + i * spacing});
    return levels;
}

std::string validateGridLines(const std::vector<GridLine>& lines) {
    for (std::size_t i = 0; i < lines.size(); ++i)
        for (std::size_t j = i + 1; j < lines.size(); ++j)
            if (!lines[i].label.empty() && lines[i].label == lines[j].label)
                return format("La etiqueta '%s' está repetida.", {lines[i].label});
    for (std::size_t i = 0; i < lines.size(); ++i)
        for (std::size_t j = i + 1; j < lines.size(); ++j)
            if (sameCoordinate(lines[i].coordinate, lines[j].coordinate))
                return format("Las líneas '%s' y '%s' están situadas en la misma coordenada.",
                              {lines[i].label, lines[j].label});
    for (const GridLine& line : lines)
        if (!inRange(line.coordinate)) return "La coordenada de la línea '" + line.label + "' debe estar entre -50000 y 50000 m.";
    return "";
}

std::string validateLevels(const std::vector<Level>& levels) {
    if (levels.size() < static_cast<std::size_t>(kMinLevels)) return "Debe definir al menos dos niveles.";
    for (std::size_t i = 0; i < levels.size(); ++i)
        for (std::size_t j = i + 1; j < levels.size(); ++j)
            if (levels[i].name == levels[j].name) return format("La referencia '%s' está repetida.", {levels[i].name});
    for (std::size_t i = 0; i < levels.size(); ++i)
        for (std::size_t j = i + 1; j < levels.size(); ++j)
            if (sameValue(levels[i].elevation, levels[j].elevation))
                return format("Los niveles '%s' y '%s' tienen asignada la misma cota.", {levels[i].name, levels[j].name});
    return "";
}

std::vector<Level> sortedLevels(const std::vector<Level>& levels) {
    std::vector<Level> out = levels;
    std::stable_sort(out.begin(), out.end(), [](const Level& a, const Level& b) { return a.elevation < b.elevation; });
    return out;
}

std::vector<GridLine> sortedGridLines(const std::vector<GridLine>& lines) {
    std::vector<GridLine> out = lines;
    std::stable_sort(out.begin(), out.end(),
                     [](const GridLine& a, const GridLine& b) { return a.coordinate < b.coordinate; });
    return out;
}

// --- JSON ---------------------------------------------------------------------------------------------------------

namespace {

json lineJson(const GridLine& line) {
    return json{{"label", line.label}, {"coordinate", line.coordinate}, {"labelPosition", positionName(line.labelPosition)}};
}

json linesJson(const std::vector<GridLine>& lines) {
    json a = json::array();
    for (const GridLine& line : lines) a.push_back(lineJson(line));
    return a;
}

bool readLines(const json& j, const char* key, std::vector<GridLine>& out) {
    if (!j.contains(key)) return true;
    const json& a = j.at(key);
    if (!a.is_array()) return false;
    std::vector<GridLine> lines;
    for (const json& item : a) {
        if (!item.is_object()) return false;
        GridLine line;
        if (item.contains("label")) line.label = item.at("label").get<std::string>();
        if (item.contains("coordinate")) line.coordinate = item.at("coordinate").get<double>();
        if (item.contains("labelPosition")) {
            const std::string name = item.at("labelPosition").get<std::string>();
            if (name == "origin") line.labelPosition = LabelPosition::Origin;
            else if (name == "end") line.labelPosition = LabelPosition::End;
            else return false;
        }
        lines.push_back(std::move(line));
    }
    out = std::move(lines);
    return true;
}

bool readGroups(const json& j, const char* key, std::vector<std::vector<double>>& out) {
    if (!j.contains(key)) return true;
    const json& a = j.at(key);
    if (!a.is_array()) return false;
    std::vector<std::vector<double>> groups;
    for (const json& group : a) {
        if (!group.is_array()) return false;
        std::vector<double> coordinates;
        for (const json& c : group) coordinates.push_back(c.get<double>());
        groups.push_back(std::move(coordinates));
    }
    out = std::move(groups);
    return true;
}

}  // namespace

std::string toJson(const WorkPlanes& planes) {
    json j;
    j["adjustElements"] = planes.adjustElements;
    j["gridX"] = linesJson(planes.gridX);
    j["gridY"] = linesJson(planes.gridY);
    json levels = json::array();
    for (const Level& level : planes.levels) levels.push_back(json{{"name", level.name}, {"elevation", level.elevation}});
    j["levels"] = levels;
    j["groups"] = json{{"x", planes.groups.x}, {"y", planes.groups.y}, {"z", planes.groups.z}};
    if (!planes.visible) j["visible"] = false;  // absent = visible
    if (!(planes.references == ReferenceOptions{})) {
        const ReferenceOptions& r = planes.references;
        j["references"] = json{{"showGrid", r.showGrid},         {"referenceLines", r.referenceLines},
                               {"onlyWithNode", r.onlyWithNode}, {"parallelX", r.parallelX},
                               {"parallelY", r.parallelY},       {"parallelZ", r.parallelZ},
                               {"colourByDirection", r.colourByDirection}};
    }
    return j.dump();
}

std::optional<WorkPlanes> workPlanesFromJson(const std::string& jsonText) {
    try {
        const json j = json::parse(jsonText);
        if (!j.is_object()) return std::nullopt;
        WorkPlanes planes;
        if (j.contains("adjustElements")) planes.adjustElements = j.at("adjustElements").get<bool>();
        if (!readLines(j, "gridX", planes.gridX) || !readLines(j, "gridY", planes.gridY)) return std::nullopt;
        if (j.contains("levels")) {
            const json& a = j.at("levels");
            if (!a.is_array()) return std::nullopt;
            for (const json& item : a) {
                if (!item.is_object()) return std::nullopt;
                Level level;
                if (item.contains("name")) level.name = item.at("name").get<std::string>();
                if (item.contains("elevation")) level.elevation = item.at("elevation").get<double>();
                planes.levels.push_back(std::move(level));
            }
        }
        if (j.contains("groups")) {
            const json& g = j.at("groups");
            if (!g.is_object()) return std::nullopt;
            if (!readGroups(g, "x", planes.groups.x) || !readGroups(g, "y", planes.groups.y) ||
                !readGroups(g, "z", planes.groups.z))
                return std::nullopt;
        }
        if (j.contains("visible")) planes.visible = j.at("visible").get<bool>();
        if (j.contains("references")) {
            const json& r = j.at("references");
            if (!r.is_object()) return std::nullopt;
            const auto read = [&r](const char* key, bool& out) {
                if (r.contains(key)) out = r.at(key).get<bool>();
            };
            ReferenceOptions& o = planes.references;
            read("showGrid", o.showGrid);
            read("referenceLines", o.referenceLines);
            read("onlyWithNode", o.onlyWithNode);
            read("parallelX", o.parallelX);
            read("parallelY", o.parallelY);
            read("parallelZ", o.parallelZ);
            read("colourByDirection", o.colourByDirection);
        }
        return planes;
    } catch (const json::exception&) {
        return std::nullopt;
    }
}

}  // namespace cc::core
