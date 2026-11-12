#include "core/fem/results.h"

#include <algorithm>
#include <cmath>

#include "core/fem/linear_solver.h"

namespace cc::fem {

namespace {

const FemNode* findNode(const FrameModel& model, int id, std::size_t* index = nullptr) {
    for (std::size_t i = 0; i < model.nodes.size(); ++i)
        if (model.nodes[i].id == id) {
            if (index) *index = i;
            return &model.nodes[i];
        }
    return nullptr;
}

bool hasBarResults(const LoadCaseResult& result, std::size_t barIndex) {
    return result.status == SolveStatus::Ok && barIndex < result.barForces.size() && barIndex < result.barDisplacements.size();
}

/// The piece of a split bar that contains x (the earlier one at a boundary), or nullptr for a bar in one piece.
const BarSegmentResult* pieceAt(const LoadCaseResult& result, std::size_t barIndex, double x, bool& last) {
    last = true;
    if (barIndex >= result.barSegments.size() || result.barSegments[barIndex].empty()) return nullptr;
    const std::vector<BarSegmentResult>& pieces = result.barSegments[barIndex];
    for (std::size_t k = 0; k < pieces.size(); ++k)
        if (x <= pieces[k].x1 + 1e-12 || k + 1 == pieces.size()) {
            last = k + 1 == pieces.size();
            return &pieces[k];
        }
    return nullptr;
}

void addScaled(Dof6& acc, const Dof6& v, double c) {
    for (int k = 0; k < 6; ++k) acc[k] += c * v[k];
}

}  // namespace

double barLength(const FrameModel& model, std::size_t barIndex) {
    if (barIndex >= model.bars.size()) return 0.0;
    const FemNode* a = findNode(model, model.bars[barIndex].nodeI);
    const FemNode* b = findNode(model, model.bars[barIndex].nodeJ);
    if (!a || !b) return 0.0;
    const double dx = b->position[0] - a->position[0], dy = b->position[1] - a->position[1], dz = b->position[2] - a->position[2];
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

std::vector<FemBarLoad> loadsOfBar(const LoadCaseInput& input, int barId) {
    std::vector<FemBarLoad> out;
    for (const FemBarLoad& l : input.bars)
        if (l.bar == barId) out.push_back(l);
    return out;
}

std::optional<Dof6> nodeDisplacement(const FrameModel& model, const LoadCaseResult& result, int nodeId) {
    std::size_t index = 0;
    if (result.status != SolveStatus::Ok || !findNode(model, nodeId, &index) || index >= result.displacements.size()) return std::nullopt;
    return result.displacements[index];
}

Dof6 barForcesAt(const FrameModel& model, const LoadCaseResult& result, const LoadCaseInput& input, std::size_t barIndex, double x) {
    if (barIndex >= model.bars.size() || !hasBarResults(result, barIndex)) return {};
    const FemBar& bar = model.bars[barIndex];
    const double length = barLength(model, barIndex);
    x = std::clamp(x, 0.0, length);
    const std::vector<FemBarLoad> loads = loadsOfBar(input, bar.id);
    bool last = true;
    if (const BarSegmentResult* p = pieceAt(result, barIndex, x, last)) return pieceForcesAt(bar, length, p->x0, p->x1, last, p->endForces, loads, x);
    return pieceForcesAt(bar, length, 0.0, length, true, result.barForces[barIndex].end, loads, x);
}

Dof6 barDisplacementAt(const FrameModel& model, const LoadCaseResult& result, const LoadCaseInput& input, std::size_t barIndex, double x,
                       const SolverOptions& options) {
    if (barIndex >= model.bars.size() || !hasBarResults(result, barIndex)) return {};
    const FemBar& bar = model.bars[barIndex];
    const double length = barLength(model, barIndex);
    x = std::clamp(x, 0.0, length);
    const std::vector<FemBarLoad> loads = loadsOfBar(input, bar.id);
    bool last = true;
    if (const BarSegmentResult* p = pieceAt(result, barIndex, x, last))
        return pieceDisplacementAt(bar, length, p->x0, p->x1, last, p->originDisplacement, p->endForces, loads, x, options);
    return pieceDisplacementAt(bar, length, 0.0, length, true, result.barDisplacements[barIndex].origin, result.barForces[barIndex].end, loads, x,
                               options);
}

std::vector<BarLawPoint> barLaw(const FrameModel& model, const LoadCaseResult& result, const LoadCaseInput& input, std::size_t barIndex, int divisions,
                                const SolverOptions& options) {
    std::vector<BarLawPoint> out;
    if (barIndex >= model.bars.size() || !hasBarResults(result, barIndex)) return out;
    const double length = barLength(model, barIndex);
    if (length <= 0.0) return out;
    divisions = std::max(1, divisions);
    std::vector<double> xs;
    for (int i = 0; i <= divisions; ++i) xs.push_back(length * i / divisions);
    constexpr double kJump = 1e-9;
    // Piece boundaries of a split bar: the shell acts there with a concentrated force, so both sides are sampled.
    if (barIndex < result.barSegments.size())
        for (const BarSegmentResult& p : result.barSegments[barIndex]) {
            xs.push_back(p.x1);
            if (p.x1 < length - kJump) xs.push_back(p.x1 + kJump);
        }
    for (const FemBarLoad& l : loadsOfBar(input, model.bars[barIndex].id)) {
        if (l.kind != BarLoadKind::PointForce && l.kind != BarLoadKind::PointMoment) continue;
        if (l.x1 > kJump) xs.push_back(l.x1 - kJump);
        if (l.x1 < length - kJump) xs.push_back(l.x1 + kJump);
    }
    std::sort(xs.begin(), xs.end());
    xs.erase(std::unique(xs.begin(), xs.end(), [](double a, double b) { return std::fabs(a - b) < 1e-12; }), xs.end());
    for (double x : xs) {
        x = std::clamp(x, 0.0, length);
        out.push_back({x, barForcesAt(model, result, input, barIndex, x), barDisplacementAt(model, result, input, barIndex, x, options)});
    }
    return out;
}

LoadCaseResult superposeCases(std::span<const LoadCaseResult> cases, std::span<const double> coefficients, int id, const std::string& name) {
    LoadCaseResult out;
    out.id = id;
    out.name = name;
    const LoadCaseResult* shape = nullptr;
    for (std::size_t i = 0; i < cases.size(); ++i) {
        const double c = i < coefficients.size() ? coefficients[i] : 0.0;
        if (c == 0.0) continue;
        if (cases[i].status != SolveStatus::Ok) {
            out.status = cases[i].status;
            out.message = cases[i].message;
            return out;
        }
        if (!shape) shape = &cases[i];
    }
    if (!shape) {
        // No case acts: a zero result with the shape of any solved case.
        for (const LoadCaseResult& lc : cases)
            if (lc.status == SolveStatus::Ok) { shape = &lc; break; }
        if (!shape) return out;
    }
    out.displacements.assign(shape->displacements.size(), Dof6{});
    out.barForces = shape->barForces;
    out.barDisplacements = shape->barDisplacements;
    for (BarEndForces& f : out.barForces) f.origin = f.end = Dof6{};
    for (BarEndDisplacements& d : out.barDisplacements) d.origin = d.end = Dof6{};
    out.barSegments = shape->barSegments;
    for (auto& pieces : out.barSegments)
        for (BarSegmentResult& p : pieces) p.originForces = p.endForces = p.originDisplacement = Dof6{};
    out.meshDisplacements.assign(shape->meshDisplacements.size(), Dof6{});
    out.triangleForces.assign(shape->triangleForces.size(), TriangleForces{});
    for (std::size_t i = 0; i < cases.size(); ++i) {
        const double c = i < coefficients.size() ? coefficients[i] : 0.0;
        if (c == 0.0) continue;
        const LoadCaseResult& lc = cases[i];
        for (std::size_t n = 0; n < out.displacements.size() && n < lc.displacements.size(); ++n) addScaled(out.displacements[n], lc.displacements[n], c);
        for (const auto& [node, r] : lc.reactions) addScaled(out.reactions[node], r, c);
        for (std::size_t b = 0; b < out.barForces.size() && b < lc.barForces.size(); ++b) {
            addScaled(out.barForces[b].origin, lc.barForces[b].origin, c);
            addScaled(out.barForces[b].end, lc.barForces[b].end, c);
        }
        for (std::size_t b = 0; b < out.barDisplacements.size() && b < lc.barDisplacements.size(); ++b) {
            addScaled(out.barDisplacements[b].origin, lc.barDisplacements[b].origin, c);
            addScaled(out.barDisplacements[b].end, lc.barDisplacements[b].end, c);
        }
        for (std::size_t b = 0; b < out.barSegments.size() && b < lc.barSegments.size(); ++b)
            for (std::size_t k = 0; k < out.barSegments[b].size() && k < lc.barSegments[b].size(); ++k) {
                addScaled(out.barSegments[b][k].originForces, lc.barSegments[b][k].originForces, c);
                addScaled(out.barSegments[b][k].endForces, lc.barSegments[b][k].endForces, c);
                addScaled(out.barSegments[b][k].originDisplacement, lc.barSegments[b][k].originDisplacement, c);
            }
        for (std::size_t n = 0; n < out.meshDisplacements.size() && n < lc.meshDisplacements.size(); ++n)
            addScaled(out.meshDisplacements[n], lc.meshDisplacements[n], c);
        for (const auto& [node, r] : lc.meshReactions) addScaled(out.meshReactions[node], r, c);
        for (std::size_t e = 0; e < out.triangleForces.size() && e < lc.triangleForces.size(); ++e) {
            TriangleForces& acc = out.triangleForces[e];
            const TriangleForces& t = lc.triangleForces[e];
            for (int p = 0; p < 6; ++p)
                for (int q = 0; q < 8; ++q) acc.atNodes[p][q] += c * t.atNodes[p][q];
            for (int p = 0; p < 3; ++p)
                for (int q = 0; q < 8; ++q) acc.atGauss[p][q] += c * t.atGauss[p][q];
            for (int p = 0; p < 36; ++p) acc.nodalForces[p] += c * t.nodalForces[p];
        }
    }
    // Supported nodes keep their entry even when every contribution was zero.
    for (const auto& [node, r] : shape->reactions) out.reactions.try_emplace(node, Dof6{});
    for (const auto& [node, r] : shape->meshReactions) out.meshReactions.try_emplace(node, Dof6{});
    return out;
}

LinearResult superposeResult(const LinearResult& base, std::span<const std::vector<double>> coefficientSets, std::span<const std::string> names) {
    LinearResult out;
    out.status = base.status;
    out.message = base.message;
    out.mechanismNode = base.mechanismNode;
    out.mechanismDof = base.mechanismDof;
    out.lockedDofs = base.lockedDofs;
    out.nullModeDofs = base.nullModeDofs;
    out.mesh = base.mesh;
    for (std::size_t k = 0; k < coefficientSets.size(); ++k) {
        const std::string name = k < names.size() ? names[k] : std::string();
        out.cases.push_back(superposeCases(base.cases, coefficientSets[k], static_cast<int>(k + 1), name));
    }
    return out;
}

}  // namespace cc::fem
