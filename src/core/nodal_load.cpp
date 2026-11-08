#include "core/nodal_load.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <set>

namespace cc::core {

double nodalLoadDrawLengthM(const LoadScales& scales, const NodalLoad& load) {
    if (scales.mode == LoadScaleMode::Fixed) return scales.fixedSizeM;
    return kLoadMetresPerKn * load.magnitude * loadScaleOfCase(scales, load.loadCaseId) *
           scales.perType[static_cast<std::size_t>(LoadScaleType::Punctual)];
}

double loadScaleOfCase(const LoadScales& scales, int loadCaseId) {
    const auto it = scales.perCase.find(loadCaseId);
    return it != scales.perCase.end() ? it->second : kDefaultLoadCaseScale;
}

const NodalLoad* findNodalLoad(const NodalLoads& loads, int id) {
    for (const NodalLoad& l : loads.items)
        if (l.id == id) return &l;
    return nullptr;
}

std::vector<int> nodalLoadsOfNode(const NodalLoads& loads, int nodeId) {
    std::vector<int> out;
    for (const NodalLoad& l : loads.items)
        if (l.nodeId == nodeId) out.push_back(l.id);
    return out;
}

std::vector<int> nodalLoadsOfCase(const NodalLoads& loads, int loadCaseId) {
    std::vector<int> out;
    for (const NodalLoad& l : loads.items)
        if (l.loadCaseId == loadCaseId) out.push_back(l.id);
    return out;
}

Point3 nodalLoadDirection(const NodalLoad& load) {
    switch (load.direction) {
        case LoadDirection::PlusX: return {1, 0, 0};
        case LoadDirection::PlusY: return {0, 1, 0};
        case LoadDirection::PlusZ: return {0, 0, 1};
        case LoadDirection::MinusX: return {-1, 0, 0};
        case LoadDirection::MinusY: return {0, -1, 0};
        case LoadDirection::MinusZ: return {0, 0, -1};
        case LoadDirection::UserVector: {
            const Point3& v = load.userVector;
            const double n = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
            if (!(n > 1e-12) || !std::isfinite(n)) return {0, 0, 0};
            return {v.x / n, v.y / n, v.z / n};
        }
    }
    return {0, 0, 0};
}

Point3 nodalLoadForce(const NodalLoad& load) {
    const Point3 d = nodalLoadDirection(load);
    return {d.x * load.magnitude, d.y * load.magnitude, d.z * load.magnitude};
}

std::string nodalLoadRangeMessage() { return "El valor de 'Valor' debe estar entre 0.01 y 49000.00."; }
std::string nodalLoadNotNumberMessage() { return "La cadena introducida no es un número."; }
std::string nodalLoadDirectionMessage() { return "La dirección introducida no es correcta."; }

double roundLoadValue(double value) { return std::round(value * 100.0) / 100.0; }

bool nodalLoadValueInRange(double value) {
    if (!std::isfinite(value)) return false;
    const double r = roundLoadValue(value);
    return r >= kMinNodalLoadKn && r <= kMaxNodalLoadKn;
}

LoadValueParse parseLoadValue(const std::string& text) {
    LoadValueParse out;
    std::string s = text;
    const auto blank = [](char c) { return std::isspace(static_cast<unsigned char>(c)) != 0; };
    while (!s.empty() && blank(s.front())) s.erase(s.begin());
    while (!s.empty() && blank(s.back())) s.pop_back();
    std::replace(s.begin(), s.end(), ',', '.');
    // Only plain decimal notation: digits, one point, optional leading sign (no hex, "inf", "nan" or exponent).
    bool digit = false, point = false, valid = !s.empty();
    for (std::size_t i = 0; i < s.size() && valid; ++i) {
        const char c = s[i];
        if (c >= '0' && c <= '9') {
            digit = true;
        } else if (c == '.' && !point) {
            point = true;
        } else if (!((c == '-' || c == '+') && i == 0)) {
            valid = false;
        }
    }
    if (!valid || !digit) {
        out.message = nodalLoadNotNumberMessage();
        return out;
    }
    const double v = std::strtod(s.c_str(), nullptr);
    if (!nodalLoadValueInRange(v)) {
        out.message = nodalLoadRangeMessage();
        return out;
    }
    out.ok = true;
    out.value = roundLoadValue(v);
    return out;
}

namespace {
bool finite3(const Point3& v) { return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z); }
bool nullVector(const Point3& v) { return !finite3(v) || std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z) < 1e-12; }
bool validDirection(LoadDirection d) { return static_cast<int>(d) >= 0 && static_cast<int>(d) < kLoadDirectionCount; }
}  // namespace

std::string checkNodalLoadData(const NodalLoadData& data) {
    if (!nodalLoadValueInRange(data.magnitude)) return nodalLoadRangeMessage();
    if (!validDirection(data.direction)) return nodalLoadDirectionMessage();
    if (data.direction == LoadDirection::UserVector && nullVector(data.userVector)) return nodalLoadDirectionMessage();
    return {};
}

void applyNodalLoadsNodesDeleted(NodalLoads& loads, const std::vector<int>& nodeIds) {
    const std::set<int> gone(nodeIds.begin(), nodeIds.end());
    loads.items.erase(std::remove_if(loads.items.begin(), loads.items.end(), [&](const NodalLoad& l) { return gone.count(l.nodeId) != 0; }),
                      loads.items.end());
}

void applyNodalLoadsNodesMerged(NodalLoads& loads, int from, int into) {
    for (NodalLoad& l : loads.items)
        if (l.nodeId == from) l.nodeId = into;
}

void applyNodalLoadsCaseDeleted(NodalLoads& loads, int loadCaseId) {
    loads.items.erase(std::remove_if(loads.items.begin(), loads.items.end(), [&](const NodalLoad& l) { return l.loadCaseId == loadCaseId; }),
                      loads.items.end());
    loads.scales.perCase.erase(loadCaseId);
}

std::string validateNodalLoads(const NodalLoads& loads, const LoadCaseSet& cases) {
    std::set<int> ids;
    for (const NodalLoad& l : loads.items) {
        if (l.id < 1 || l.id >= loads.nextId) return "Carga de nudo con identificador no válido.";
        if (!ids.insert(l.id).second) return "Carga de nudo con identificador repetido.";
        if (l.nodeId < 1) return "Carga de nudo con nudo no válido.";
        if (!findLoadCase(cases, l.loadCaseId)) return "Carga de nudo " + std::to_string(l.id) + " en una hipótesis inexistente.";
        if (!nodalLoadValueInRange(l.magnitude) || l.magnitude != roundLoadValue(l.magnitude))
            return "Carga de nudo " + std::to_string(l.id) + " con valor no válido.";
        if (!validDirection(l.direction) || (l.direction == LoadDirection::UserVector && nullVector(l.userVector)))
            return "Carga de nudo " + std::to_string(l.id) + " con dirección no válida.";
    }
    const LoadScales& s = loads.scales;
    if (!(std::isfinite(s.fixedSizeM) && s.fixedSizeM > 0.0)) return "Escala de cargas no válida.";
    for (const auto& [id, v] : s.perCase)
        if (!(std::isfinite(v) && v > 0.0)) return "Escala de cargas no válida.";
    for (double v : s.perType)
        if (!(std::isfinite(v) && v >= kMinTypeScale && v <= kMaxTypeScale)) return "Escala de cargas no válida.";
    return {};
}

}  // namespace cc::core
