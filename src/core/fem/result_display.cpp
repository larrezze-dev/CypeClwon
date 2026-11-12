#include "core/fem/result_display.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "core/fem/results.h"

namespace cc::fem {

std::string resultFamilyName(ResultFamily family) {
    switch (family) {
        case ResultFamily::LoadCases: return "Hipótesis simple";
        case ResultFamily::Nonlinear: return "Combinaciones no lineales";
    }
    return {};
}

std::vector<ResultFamily> availableFamilies(const ProjectAnalysis& analysis) {
    std::vector<ResultFamily> out;
    if (!analysis.calculated) return out;
    out.push_back(ResultFamily::LoadCases);
    if (analysis.nonlinearCalculated && !analysis.nonlinear.cases.empty()) out.push_back(ResultFamily::Nonlinear);
    return out;
}

std::vector<std::string> resultItemNames(const ProjectAnalysis& analysis, ResultFamily family) {
    std::vector<std::string> out;
    if (!analysis.calculated) return out;
    if (family == ResultFamily::LoadCases) {
        for (const LoadCaseInput& c : analysis.build.cases) out.push_back(c.name);
    } else {
        for (const LoadCaseInput& c : analysis.nonlinearInputs) out.push_back(c.name);
    }
    return out;
}

std::optional<ResultRef> resultOf(const ProjectAnalysis& analysis, ResultFamily family, std::size_t index) {
    if (!analysis.calculated) return std::nullopt;
    const LinearResult& linear = family == ResultFamily::LoadCases ? analysis.linear : analysis.nonlinear;
    const std::vector<LoadCaseInput>& inputs = family == ResultFamily::LoadCases ? analysis.build.cases : analysis.nonlinearInputs;
    if (family == ResultFamily::Nonlinear && !analysis.nonlinearCalculated) return std::nullopt;
    if (index >= linear.cases.size() || index >= inputs.size()) return std::nullopt;
    if (linear.cases[index].status != SolveStatus::Ok) return std::nullopt;
    return ResultRef{&linear.cases[index], &inputs[index], &linear, index};
}

std::string formatFixed(double value, int decimals) {
    if (!std::isfinite(value)) return "-";
    decimals = std::clamp(decimals, 0, 9);
    char buffer[64];
    std::snprintf(buffer, sizeof buffer, "%.*f", decimals, value);
    std::string text = buffer;
    // A value that rounds to zero is written without a sign.
    if (!text.empty() && text.front() == '-' && text.find_first_not_of("-0.") == std::string::npos) text.erase(0, 1);
    return text;
}

namespace {

std::vector<BalloonLine> lines(const Dof6& v, const ComponentMask& shown, const std::array<const char*, 6>& names,
                               const std::array<double, 6>& factor, const std::array<int, 6>& decimals, const std::array<const char*, 6>& units) {
    std::vector<BalloonLine> out;
    for (std::size_t k = 0; k < 6; ++k) {
        if (!shown[k]) continue;
        out.push_back({std::string(names[k]) + ":", " " + formatFixed(v[k] * factor[k], decimals[k]) + " " + units[k]});
    }
    return out;
}

}  // namespace

std::vector<BalloonLine> displacementLines(const Dof6& d, const ComponentMask& shown) {
    return lines(d, shown, {"Dx", "Dy", "Dz", "Gx", "Gy", "Gz"}, {1000, 1000, 1000, 1000, 1000, 1000}, {3, 3, 3, 3, 3, 3},
                 {"mm", "mm", "mm", "mRad", "mRad", "mRad"});
}

std::vector<BalloonLine> reactionLines(const Dof6& r, const ComponentMask& shown) {
    return lines(r, shown, {"Rx", "Ry", "Rz", "Mx", "My", "Mz"}, {1, 1, 1, 1, 1, 1}, {3, 3, 3, 2, 2, 2},
                 {"kN", "kN", "kN", "kN·m", "kN·m", "kN·m"});
}

std::vector<BalloonLine> forceLines(double x, const Dof6& f, const ComponentMask& shown) {
    std::vector<BalloonLine> out{{"X:", " " + formatFixed(x, 3) + " m"}};
    for (BalloonLine& l : lines(f, shown, {"N", "Vy", "Vz", "Mt", "My", "Mz"}, {1, 1, 1, 1, 1, 1}, {3, 3, 3, 2, 2, 2},
                                {"kN", "kN", "kN", "kN·m", "kN·m", "kN·m"}))
        out.push_back(std::move(l));
    return out;
}

std::optional<std::size_t> nodeIndexOf(const FrameModel& model, int nodeId) {
    for (std::size_t i = 0; i < model.nodes.size(); ++i)
        if (model.nodes[i].id == nodeId) return i;
    return std::nullopt;
}

std::optional<std::size_t> barIndexOf(const FrameModel& model, int barId) {
    for (std::size_t i = 0; i < model.bars.size(); ++i)
        if (model.bars[i].id == barId) return i;
    return std::nullopt;
}

Vec3 barPointAt(const FrameModel& model, std::size_t barIndex, double x) {
    if (barIndex >= model.bars.size()) return {};
    const FemBar& bar = model.bars[barIndex];
    const auto a = nodeIndexOf(model, bar.nodeI), b = nodeIndexOf(model, bar.nodeJ);
    if (!a || !b) return {};
    const Vec3& p = model.nodes[*a].position;
    const Vec3& q = model.nodes[*b].position;
    const double length = barLength(model, barIndex);
    const double t = length > 0.0 ? x / length : 0.0;
    return {p[0] + t * (q[0] - p[0]), p[1] + t * (q[1] - p[1]), p[2] + t * (q[2] - p[2])};
}

double modelDiagonal(const FrameModel& model) {
    if (model.nodes.empty()) return 0.0;
    Vec3 lo = model.nodes.front().position, hi = lo;
    for (const FemNode& n : model.nodes)
        for (std::size_t k = 0; k < 3; ++k) {
            lo[k] = std::min(lo[k], n.position[k]);
            hi[k] = std::max(hi[k], n.position[k]);
        }
    return std::sqrt((hi[0] - lo[0]) * (hi[0] - lo[0]) + (hi[1] - lo[1]) * (hi[1] - lo[1]) + (hi[2] - lo[2]) * (hi[2] - lo[2]));
}

double automaticScale(double maxValue, double modelSize, double fraction) {
    if (!(maxValue > 1e-15) || !(modelSize > 0.0) || !std::isfinite(maxValue)) return 0.0;
    return fraction * modelSize / maxValue;
}

double maxTranslation(const FrameModel& model, const ResultRef& ref, int divisions, const SolverOptions& options) {
    if (!ref.result || !ref.input) return 0.0;
    double best = 0.0;
    const auto norm = [](const Dof6& d) { return std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]); };
    for (const Dof6& d : ref.result->displacements) best = std::max(best, norm(d));
    divisions = std::max(1, divisions);
    for (std::size_t b = 0; b < model.bars.size(); ++b) {
        const double length = barLength(model, b);
        for (int i = 0; i <= divisions; ++i)
            best = std::max(best, norm(barDisplacementAt(model, *ref.result, *ref.input, b, length * i / divisions, options)));
    }
    return best;
}

std::vector<Vec3> deformedBar(const FrameModel& model, const ResultRef& ref, std::size_t barIndex, double scale, int divisions,
                              const SolverOptions& options) {
    std::vector<Vec3> out;
    if (!ref.result || !ref.input || barIndex >= model.bars.size()) return out;
    divisions = std::max(1, divisions);
    const double length = barLength(model, barIndex);
    for (int i = 0; i <= divisions; ++i) {
        const double x = length * i / divisions;
        const Vec3 p = barPointAt(model, barIndex, x);
        const Dof6 d = barDisplacementAt(model, *ref.result, *ref.input, barIndex, x, options);
        out.push_back({p[0] + scale * d[0], p[1] + scale * d[1], p[2] + scale * d[2]});
    }
    return out;
}

Vec3 lawDirection(const FrameModel& model, std::size_t barIndex, int component) {
    if (barIndex >= model.bars.size()) return {0, 0, 1};
    const FemBar& bar = model.bars[barIndex];
    // [V resultados_medidas_original.md B]: N, Vy and Vz are drawn towards the positive local axis, My and Mz towards the negative one.
    const Vec3 axis = (component == 1 || component == 5) ? bar.ey : bar.ez;
    if (component == 4 || component == 5) return Vec3{-axis[0], -axis[1], -axis[2]};
    return axis;
}

double maxLawValue(const FrameModel& model, const ResultRef& ref, const std::vector<std::size_t>& barIndices, int component, int divisions,
                   const SolverOptions& options) {
    if (!ref.result || !ref.input || component < 0 || component > 5) return 0.0;
    double best = 0.0;
    for (std::size_t b : barIndices) {
        if (b >= model.bars.size()) continue;
        for (const BarLawPoint& p : barLaw(model, *ref.result, *ref.input, b, divisions, options))
            best = std::max(best, std::abs(p.forces[static_cast<std::size_t>(component)]));
    }
    return best;
}


std::vector<DeflectionPoint> barDeflection(const FrameModel& model, const ResultRef& ref, std::size_t barIndex, int divisions, const SolverOptions& options) {
    std::vector<DeflectionPoint> out;
    if (!ref.result || !ref.input || barIndex >= model.bars.size()) return out;
    divisions = std::max(1, divisions);
    const FemBar& bar = model.bars[barIndex];
    const double length = barLength(model, barIndex);
    const Dof6 d0 = barDisplacementAt(model, *ref.result, *ref.input, barIndex, 0.0, options);
    const Dof6 d1 = barDisplacementAt(model, *ref.result, *ref.input, barIndex, length, options);
    for (int i = 0; i <= divisions; ++i) {
        const double x = length * i / divisions;
        const double t = length > 0.0 ? x / length : 0.0;
        const Dof6 d = barDisplacementAt(model, *ref.result, *ref.input, barIndex, x, options);
        Vec3 rel;
        for (std::size_t k = 0; k < 3; ++k) rel[k] = d[k] - (d0[k] + t * (d1[k] - d0[k]));
        DeflectionPoint p;
        p.x = x;
        p.fxy = rel[0] * bar.ey[0] + rel[1] * bar.ey[1] + rel[2] * bar.ey[2];
        p.fxz = rel[0] * bar.ez[0] + rel[1] * bar.ez[1] + rel[2] * bar.ez[2];
        p.f = std::sqrt(p.fxy * p.fxy + p.fxz * p.fxz);
        out.push_back(p);
    }
    return out;
}

std::size_t largestAbsIndex(const std::vector<double>& values) {
    std::size_t best = 0;
    for (std::size_t i = 1; i < values.size(); ++i)
        if (std::abs(values[i]) > std::abs(values[best]) + 1e-12) best = i;
    return best;
}

std::array<std::string, 2> extremeLabel(const std::string& law, bool isMaximum, double value, int decimals, const std::string& unit, double x) {
    return {law + (isMaximum ? " máx.: " : " mín.: ") + formatFixed(value, decimals) + " " + unit, "X: " + formatFixed(x, 3) + " m"};
}

}  // namespace cc::fem
