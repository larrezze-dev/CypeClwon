#include "core/shell_load.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "core/geom3.h"
#include "core/shell_edit.h"
#include "core/shell_properties.h"

namespace cc::core {

namespace {

std::string fixedText(double v, int decimals) {
    char buf[64];
    std::snprintf(buf, sizeof buf, "%.*f", decimals, v);
    return buf;
}

bool validType(ShellLoadType t) { return static_cast<int>(t) >= 0 && static_cast<int>(t) < kShellLoadTypeCount; }

ShellLoadIssue issue(ShellLoadIssueKind kind, std::string message, std::optional<ShellLoadField> field = std::nullopt) {
    return ShellLoadIssue{kind, std::move(message), field};
}

bool inRange(const ShellLoadFieldSpec& s, double v) {
    if (!std::isfinite(v)) return false;
    const double r = roundToDecimals(v, s.decimals);
    return r >= s.min - 1e-12 && r <= s.max + 1e-12;
}

bool nullVector(const Point3& v) { return !(std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z)) || geom::length(v) < 1e-12; }

}  // namespace

// --- type properties ---

std::string shellLoadTypeName(ShellLoadType type) {
    switch (type) {
        case ShellLoadType::Uniform: return "Uniforme";
        case ShellLoadType::VariableX: return "Variable en eje x local";
        case ShellLoadType::VariableY: return "Variable en eje y local";
        case ShellLoadType::UniformTemperature: return "Incremento de temperatura uniforme";
        case ShellLoadType::Shrinkage: return "Retracción";
    }
    return {};
}
bool isSurfaceLoad(ShellLoadType t) { return t == ShellLoadType::Uniform || t == ShellLoadType::VariableX || t == ShellLoadType::VariableY; }
bool isVariableShellLoad(ShellLoadType t) { return t == ShellLoadType::VariableX || t == ShellLoadType::VariableY; }
bool hasShellAxesAndSense(ShellLoadType t) { return isSurfaceLoad(t); }

ShellLoadData shellLoadDefaults(int loadCaseId, ShellLoadType type) {
    ShellLoadData d;
    d.loadCaseId = loadCaseId;
    d.type = type;
    switch (type) {
        case ShellLoadType::Uniform: d.value1 = kShellLoadDefaultSurface; break;
        case ShellLoadType::VariableX:
        case ShellLoadType::VariableY:
            d.value1 = d.value2 = kShellLoadDefaultSurface;
            break;
        case ShellLoadType::UniformTemperature: d.value1 = kShellLoadDefaultTemperature; break;
        case ShellLoadType::Shrinkage: d.value1 = kShellLoadDefaultShrinkage; break;
    }
    return d;
}

ShellLoadData shellLoadToData(const ShellLoad& load) { return static_cast<const ShellLoadData&>(load); }

// --- queries ---

const ShellLoad* findShellLoad(const ShellLoads& loads, int id) {
    for (const ShellLoad& l : loads.items)
        if (l.id == id) return &l;
    return nullptr;
}
std::vector<int> shellLoadsOfShell(const ShellLoads& loads, int shellId) {
    std::vector<int> out;
    for (const ShellLoad& l : loads.items)
        if (l.shellId == shellId) out.push_back(l.id);
    return out;
}
std::vector<int> shellLoadsOfCase(const ShellLoads& loads, int loadCaseId) {
    std::vector<int> out;
    for (const ShellLoad& l : loads.items)
        if (l.loadCaseId == loadCaseId) out.push_back(l.id);
    return out;
}

// --- fields ---

ShellLoadFieldSpec shellLoadFieldSpec(const ShellLoadData& data, ShellLoadField field) {
    const bool second = field == ShellLoadField::Value2 && isVariableShellLoad(data.type);
    switch (data.type) {
        case ShellLoadType::Uniform: return {"Valor", kMinShellSurfaceLoad, kMaxShellSurfaceLoad, 3, "kN/m²", false};
        case ShellLoadType::VariableX:
        case ShellLoadType::VariableY:
            return {second ? "Valor final" : "Valor inicial", kMinShellSurfaceLoad, kMaxShellSurfaceLoad, 3, "kN/m²", false};
        case ShellLoadType::UniformTemperature: return {"Valor", kMinShellTemperature, kMaxShellTemperature, 2, "", false};
        case ShellLoadType::Shrinkage: return {"Valor", kMinShellShrinkage, kMaxShellShrinkage, 4, "", true};
    }
    return {};
}

std::string shellLoadRangeMessage(const ShellLoadFieldSpec& spec) {
    const std::string lo = fixedText(spec.min, spec.decimals), hi = fixedText(spec.max, spec.decimals);
    if (spec.blankUnitSpacing) return "El valor de '" + spec.label + "' debe estar entre " + lo + "  y " + hi + " .";
    if (!spec.unit.empty())
        return "El valor de '" + spec.label + "' debe estar entre " + lo + " " + spec.unit + " y " + hi + " " + spec.unit + ".";
    return "El valor de '" + spec.label + "' debe estar entre " + lo + " y " + hi + ".";
}
std::string shellLoadNotNumberMessage() { return nodalLoadNotNumberMessage(); }
std::string shellLoadZeroMessage() { return "El valor introducido no puede ser cero."; }
std::string shellLoadDirectionMessage() { return "La dirección introducida no es correcta."; }

ShellLoadFieldParse parseShellLoadField(const ShellLoadData& data, ShellLoadField field, const std::string& text) {
    ShellLoadFieldParse out;
    const ShellLoadFieldSpec spec = shellLoadFieldSpec(data, field);
    double v = 0.0;
    if (!parsePlainDecimal(text, v)) {
        out.message = shellLoadNotNumberMessage();
        return out;
    }
    if (!inRange(spec, v)) {
        out.message = shellLoadRangeMessage(spec);
        return out;
    }
    out.ok = true;
    out.value = roundToDecimals(v, spec.decimals);
    return out;
}

// --- checks ---

ShellLoadIssue checkShellLoadData(const ShellLoadData& data) {
    const ShellLoadType t = data.type;
    if (!validType(t)) return issue(ShellLoadIssueKind::Error, "El tipo de carga no es válido.");
    const auto warn = [&](ShellLoadField f, const ShellLoadFieldSpec& s) { return issue(ShellLoadIssueKind::Warning, shellLoadRangeMessage(s), f); };

    const ShellLoadFieldSpec v1 = shellLoadFieldSpec(data, ShellLoadField::Value1);
    if (!inRange(v1, data.value1)) return warn(ShellLoadField::Value1, v1);
    if (isVariableShellLoad(t)) {
        const ShellLoadFieldSpec v2 = shellLoadFieldSpec(data, ShellLoadField::Value2);
        if (!inRange(v2, data.value2)) return warn(ShellLoadField::Value2, v2);
    }
    const bool zero1 = roundToDecimals(data.value1, v1.decimals) == 0.0;
    if (isVariableShellLoad(t)) {
        if (zero1 && roundToDecimals(data.value2, v1.decimals) == 0.0) return issue(ShellLoadIssueKind::Error, shellLoadZeroMessage(), ShellLoadField::Value1);
    } else if (zero1) {
        return issue(ShellLoadIssueKind::Error, shellLoadZeroMessage(), ShellLoadField::Value1);
    }
    if (hasShellAxesAndSense(t)) {
        const int a = static_cast<int>(data.axes), s = static_cast<int>(data.sense);
        if (a < 0 || a > 1 || s < 0 || s > 1) return issue(ShellLoadIssueKind::Warning, shellLoadDirectionMessage());
    }
    return {};
}

ShellLoadData normaliseShellLoadData(ShellLoadData d) {
    const ShellLoadData defaults;
    d.value1 = roundToDecimals(d.value1, shellLoadFieldSpec(d, ShellLoadField::Value1).decimals);
    d.value2 = isVariableShellLoad(d.type) ? roundToDecimals(d.value2, shellLoadFieldSpec(d, ShellLoadField::Value2).decimals) : 0.0;
    if (!hasShellAxesAndSense(d.type)) {
        d.axes = defaults.axes;
        d.sense = defaults.sense;
    }
    return d;
}

// --- self weight ---

double shellSpecificWeightKnM3(const ShellMaterial& m) {
    switch (m.kind) {
        case ShellMaterialKind::Concrete: return kShellConcreteWeightKnM3;
        case ShellMaterialKind::RolledSteel:
        case ShellMaterialKind::ColdFormedSteel: return kShellSteelWeightKnM3;
        case ShellMaterialKind::Aluminium: return kShellAluminiumWeightKnM3;
        case ShellMaterialKind::GenericIsotropic: return m.isoDensityKnM3;
        case ShellMaterialKind::GenericOrthotropic: return m.orthoDensityKnM3;
    }
    return 0.0;
}

double shellSelfWeightKnM2(const Shell& shell) { return shell.thicknessM * shellSpecificWeightKnM3(shell.material); }

std::optional<ShellLoadData> shellSelfWeightLoad(const Shell& shell) {
    const double w = shellSelfWeightKnM2(shell);
    if (!(w > 0.0) || !std::isfinite(w)) return std::nullopt;
    ShellLoadData d;
    d.loadCaseId = 0;
    d.type = ShellLoadType::Uniform;
    d.axes = ShellLoadAxes::Global;
    d.sense = ShellLoadSense::MinusZ;
    d.value1 = w;
    return d;
}

// --- geometry ---

std::optional<ShellLoadFrame> shellLoadFrame(const StructuralModel& model, int shellId) {
    const Shell* shell = model.findShell(shellId);
    if (!shell) return std::nullopt;
    const auto axes = shellLocalAxes(model, *shell);
    if (!axes) return std::nullopt;
    ShellLoadFrame f;
    f.corners = shellDrawnCorners(model, *shell);
    if (f.corners.size() < 3) return std::nullopt;
    f.origin = f.corners.front();
    f.x = axes->x;
    f.y = axes->y;
    f.z = axes->z;
    return f;
}

Point3 shellLoadDirection(const ShellLoadData& d, const ShellLoadFrame& f) {
    if (!hasShellAxesAndSense(d.type)) return f.z;
    const double sign = d.sense == ShellLoadSense::MinusZ ? -1.0 : 1.0;
    const Point3 axis = d.axes == ShellLoadAxes::Global ? Point3{0, 0, 1} : f.z;
    return geom::scale(axis, sign);
}

std::vector<double> shellLoadCornerValues(const ShellLoadData& d, const ShellLoadFrame& f) {
    std::vector<double> out(f.corners.size(), std::fabs(d.value1));
    if (!isVariableShellLoad(d.type)) return out;
    const Point3& axis = d.type == ShellLoadType::VariableX ? f.x : f.y;
    std::vector<double> t;
    for (const Point3& c : f.corners) t.push_back(geom::dot(geom::sub(c, f.origin), axis));
    const auto [lo, hi] = std::minmax_element(t.begin(), t.end());
    const double span = *hi - *lo;
    if (span < 1e-9) return out;
    for (std::size_t i = 0; i < out.size(); ++i) out[i] = std::fabs(d.value1 + (d.value2 - d.value1) * (t[i] - *lo) / span);
    return out;
}

LoadScaleType shellLoadScaleType(ShellLoadType type) {
    switch (type) {
        case ShellLoadType::Uniform:
        case ShellLoadType::VariableX:
        case ShellLoadType::VariableY: return LoadScaleType::Surface;
        case ShellLoadType::UniformTemperature: return LoadScaleType::Temperature;
        case ShellLoadType::Shrinkage: return LoadScaleType::Shrinkage;
    }
    return LoadScaleType::Surface;
}

double shellLoadDrawSizeM(const LoadScales& scales, const ShellLoadData& d, double value, double maxValue) {
    return loadDrawSizeM(scales, shellLoadScaleType(d.type), d.loadCaseId, value, maxValue);
}

double shellLoadMaxSizeM(const LoadScales& scales, const ShellLoadData& d) {
    const double m = isVariableShellLoad(d.type) ? std::max(std::fabs(d.value1), std::fabs(d.value2)) : std::fabs(d.value1);
    return shellLoadDrawSizeM(scales, d, m, m);
}

std::vector<double> shellLoadStackOffsets(const std::vector<ShellLoadData>& loads, const LoadScales& scales) {
    std::vector<double> out(loads.size(), 0.0);
    const auto side = [](const ShellLoadData& d) { return hasShellAxesAndSense(d.type) ? d.sense : ShellLoadSense::PlusZ; };
    for (std::size_t i = 0; i < loads.size(); ++i)
        for (std::size_t j = 0; j < i; ++j)
            if (side(loads[j]) == side(loads[i])) out[i] += shellLoadMaxSizeM(scales, loads[j]);
    return out;
}

std::string shellLoadValueLabel(double value, int decimals) {
    std::string s = fixedText(std::fabs(value) < 0.5 * std::pow(10.0, -decimals) ? 0.0 : std::fabs(value), decimals);
    if (s.find('.') != std::string::npos) {
        while (!s.empty() && s.back() == '0') s.pop_back();
        if (!s.empty() && s.back() == '.') s.pop_back();
    }
    return s.empty() ? "0" : s;
}

std::string shellLoadLabelText(const ShellLoadData& d, double value) {
    switch (d.type) {
        case ShellLoadType::Uniform:
        case ShellLoadType::VariableX:
        case ShellLoadType::VariableY: return shellLoadValueLabel(value, 3) + " kN/m²";
        case ShellLoadType::UniformTemperature: return shellLoadValueLabel(value, 2) + "° C";
        case ShellLoadType::Shrinkage: return shellLoadValueLabel(value, 4);
    }
    return {};
}

ShellLoadDrawing shellLoadDrawing(const ShellLoadData& d, const ShellLoadFrame& f, const LoadScales& scales, double stackOffsetM) {
    using namespace geom;
    ShellLoadDrawing out;
    out.type = d.type;
    const std::size_t n = f.corners.size();
    if (n < 3) return out;
    out.direction = shellLoadDirection(d, f);
    if (nullVector(out.direction)) return out;
    const std::vector<double> values = shellLoadCornerValues(d, f);
    const double maxV = *std::max_element(values.begin(), values.end());
    std::vector<double> heights(n);
    for (std::size_t i = 0; i < n; ++i) {
        heights[i] = shellLoadDrawSizeM(scales, d, values[i], maxV);
        out.sizeM = std::max(out.sizeM, heights[i]);
    }
    // The prism lies on the side the load comes from: opposite to the direction in which it pushes.
    const Point3 away = scale(out.direction, -1.0);
    for (std::size_t i = 0; i < n; ++i) {
        const Point3 b = add(f.corners[i], scale(away, stackOffsetM));
        out.base.push_back(b);
        out.top.push_back(add(b, scale(away, heights[i])));
    }
    for (std::size_t i = 0; i < n; ++i) {
        const std::size_t j = (i + 1) % n;
        const double len = length(sub(out.base[j], out.base[i]));
        const int steps = std::max(1, static_cast<int>(std::ceil(len / kShellLoadTickSpacingM - 1e-9)));
        for (int k = 0; k < steps; ++k) {
            const double t = static_cast<double>(k) / steps;
            const Point3 b = add(out.base[i], scale(sub(out.base[j], out.base[i]), t));
            const Point3 e = add(out.top[i], scale(sub(out.top[j], out.top[i]), t));
            out.ticks.push_back({b, e});
        }
    }
    const auto mid = [&](std::size_t i) { return scale(add(out.base[i], out.top[i]), 0.5); };
    if (isVariableShellLoad(d.type)) {
        for (std::size_t i = 0; i < n; ++i) out.labels.push_back({mid(i), shellLoadLabelText(d, values[i])});
    } else {
        out.labels.push_back({mid(0), shellLoadLabelText(d, values[0])});
    }
    out.valid = true;
    return out;
}

// --- edits on the data ---

void applyShellLoadsShellsDeleted(ShellLoads& loads, const std::vector<int>& shellIds) {
    const std::set<int> gone(shellIds.begin(), shellIds.end());
    loads.items.erase(std::remove_if(loads.items.begin(), loads.items.end(), [&](const ShellLoad& l) { return gone.count(l.shellId) != 0; }),
                      loads.items.end());
    for (int id : gone) loads.selfWeightOff.erase(id);
}

void applyShellLoadsCaseDeleted(ShellLoads& loads, int loadCaseId) {
    loads.items.erase(std::remove_if(loads.items.begin(), loads.items.end(), [&](const ShellLoad& l) { return l.loadCaseId == loadCaseId; }),
                      loads.items.end());
}

ShellLoads reconcileShellLoads(const ShellLoads& loads, const StructuralModel& before, const StructuralModel& after, ShellLoadReconcileInfo* info) {
    ShellLoads out = loads;
    if (loads.empty()) return out;
    ShellLoadReconcileInfo local;
    ShellLoadReconcileInfo& stats = info ? *info : local;

    out.items.clear();
    for (const ShellLoad& l : loads.items) {
        if (after.findShell(l.shellId)) out.items.push_back(l);
        else ++stats.removed;
    }
    // New shells born from a division: copy the loads of the shell that contained them.
    for (const Shell& s : after.shells) {
        if (before.findShell(s.id)) continue;
        Point3 c;
        std::size_t count = 0;
        for (int id : s.nodeIds)
            if (const Node* node = after.findNode(id)) {
                c = geom::add(c, node->position);
                ++count;
            }
        if (count == 0) continue;
        c = geom::scale(c, 1.0 / static_cast<double>(count));
        const Shell* parent = nullptr;
        for (const Shell& cand : before.shells) {
            const auto frame = shellFrame(before, cand);
            if (!frame || std::fabs(frame->heightOf(c)) > kShellCoplanarityToleranceM) continue;
            if (pointInPolygon2D(frame->toLocal(c), shellContour2D(before, cand), 1e-6)) {
                parent = &cand;
                break;
            }
        }
        if (!parent) continue;
        for (const ShellLoad& l : loads.items)
            if (l.shellId == parent->id) {
                ShellLoad copy = l;
                copy.id = out.nextId++;
                copy.shellId = s.id;
                out.items.push_back(copy);
                ++stats.copied;
            }
        if (loads.selfWeightOff.count(parent->id)) out.selfWeightOff.insert(s.id);
    }
    for (auto it = out.selfWeightOff.begin(); it != out.selfWeightOff.end();) it = after.findShell(*it) ? std::next(it) : out.selfWeightOff.erase(it);
    return out;
}

std::string validateShellLoads(const ShellLoads& loads, const LoadCaseSet& cases) {
    std::set<int> ids;
    for (const ShellLoad& l : loads.items) {
        const std::string name = "Carga de lámina " + std::to_string(l.id);
        if (l.id < 1 || l.id >= loads.nextId) return "Carga de lámina con identificador no válido.";
        if (!ids.insert(l.id).second) return "Carga de lámina con identificador repetido.";
        if (l.shellId < 1) return name + " con lámina no válida.";
        if (!findLoadCase(cases, l.loadCaseId)) return name + " en una hipótesis inexistente.";
        const ShellLoadData data = shellLoadToData(l);
        if (!validType(l.type) || normaliseShellLoadData(data) != data) return name + " con datos no canónicos.";
        if (!checkShellLoadData(data).ok()) return name + " con datos no válidos.";
    }
    for (int id : loads.selfWeightOff)
        if (id < 1) return "Peso propio desactivado en una lámina no válida.";
    return {};
}

}  // namespace cc::core
