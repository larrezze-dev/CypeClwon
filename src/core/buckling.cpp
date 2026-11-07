#include "core/buckling.h"

#include <cmath>
#include <cstdio>
#include <set>

namespace cc::core {

namespace {

std::string fixed3(double v) {
    char buf[64];
    std::snprintf(buf, sizeof buf, "%.3f", v);
    return buf;
}

bool inRange(double v, double lo, double hi) { return std::isfinite(v) && v >= lo && v <= hi; }

/// Measured (pandeo.md 9.6): a value ABOVE the maximum names 0.000 as the lower bound ("entre 0.000 y 1000.000." for 1000.1), a value
/// below the minimum names the real one ("entre 0.001 y 1000.000." for 0 and 0.0009).
BucklingIssue rangeIssue(const std::string& field, double lo, double hi, bool spaced = true, double value = 0.0) {
    const double shownLo = std::isfinite(value) && value > hi ? 0.0 : lo;
    return {0, bucklingRangeMessage(field, shownLo, hi, spaced)};
}

std::string fixed2(double v) {
    char buf[64];
    std::snprintf(buf, sizeof buf, "%.2f", v);
    return buf;
}

/// Lc / L of a flange as drawn: "L", "2L", "0.5L"; a user spacing as "3.000 m".
std::string lateralLengthText(const LateralBuckling& f) {
    const auto factorText = [](double factor) {
        if (std::fabs(factor - 1.0) < 1e-9) return std::string("L");
        // Measured: 0.5 -> "0.5L", 0.75 -> "0.75L", 2 -> "2L", 1.2345 -> "1.24L" (two decimals, trailing zeros trimmed).
        // The edit fields keep three decimals (measured: a typed 1.2345 is drawn as 1.24L), then two are shown.
        char buf[64];
        std::snprintf(buf, sizeof buf, "%.2f", std::round(factor * 1000.0) / 1000.0);
        std::string text = buf;
        while (!text.empty() && text.back() == '0') text.pop_back();
        if (!text.empty() && text.back() == '.') text.pop_back();
        return text + "L";
    };
    switch (f.kind) {
        case LateralKind::None: return "L";  // only for a flange never assigned (measured "Sup.: L, 1.00"); an assigned "No comprueba" draws no line
        case LateralKind::HalfLength: return factorText(0.5);
        case LateralKind::Length: return factorText(1.0);
        case LateralKind::DoubleLength: return factorText(2.0);
        case LateralKind::UserCoeff: return factorText(f.coeff);
        case LateralKind::UserSpacing: return fixed3(f.spacingM) + " m";
    }
    return "L";
}

}  // namespace

std::optional<double> bucklingBetaOfKind(BucklingKind kind) {
    switch (kind) {
        case BucklingKind::None: return 0.0;
        case BucklingKind::FixedFixed: return 0.5;
        case BucklingKind::PinnedFixed: return 0.7;
        case BucklingKind::PinnedPinned: return 1.0;
        case BucklingKind::Cantilever: return 2.0;
        default: return std::nullopt;
    }
}

BucklingKind bucklingKindFromBeta(double beta) {
    if (std::fabs(beta) < kBucklingKindTolerance) return BucklingKind::None;
    for (BucklingKind k : {BucklingKind::FixedFixed, BucklingKind::PinnedFixed, BucklingKind::PinnedPinned, BucklingKind::Cantilever})
        if (std::fabs(beta - *bucklingBetaOfKind(k)) < kBucklingKindTolerance) return k;
    return BucklingKind::UserBeta;
}

BucklingPlane bucklingPlaneFromBeta(double beta, bool reviewed) {
    BucklingPlane p;
    p.kind = bucklingKindFromBeta(beta);
    p.beta = beta;
    p.reviewed = reviewed;
    return p;
}

double effectiveBeta(const BucklingPlane& plane, double barLengthM) {
    if (plane.kind == BucklingKind::None) return 0.0;
    if (plane.kind == BucklingKind::UserLength) return barLengthM > 0.0 ? plane.lengthM / barLengthM : 0.0;
    return plane.beta;
}

double lateralLengthFactor(const LateralBuckling& f, double barLengthM) {
    switch (f.kind) {
        case LateralKind::None: return 0.0;
        case LateralKind::HalfLength: return 0.5;
        case LateralKind::Length: return 1.0;
        case LateralKind::DoubleLength: return 2.0;
        case LateralKind::UserCoeff: return f.coeff;
        case LateralKind::UserSpacing: return barLengthM > 0.0 ? f.spacingM / barLengthM : 0.0;
    }
    return 0.0;
}

bool isDefaultBuckling(const BarBuckling& b) { return b == BarBuckling{}; }

BarBuckling bucklingOf(const BucklingData& data, int barId) {
    const auto it = data.bars.find(barId);
    return it == data.bars.end() ? BarBuckling{} : it->second;
}

void setBuckling(BucklingData& data, int barId, const BarBuckling& record) {
    if (isDefaultBuckling(record)) data.bars.erase(barId);
    else data.bars[barId] = record;
}

std::vector<int> unreviewedBucklingBars(const BucklingData& data, const StructuralModel& model) {
    std::vector<int> out;
    for (const Bar& b : model.bars) {
        const BarBuckling r = bucklingOf(data, b.id);
        if (!r.xy.reviewed || !r.xz.reviewed) out.push_back(b.id);
    }
    return out;
}

std::string bucklingRangeMessage(const std::string& field, double lo, double hi, bool spaced) {
    const std::string gap = spaced ? " " : "";
    return "El valor de '" + field + "' debe estar entre " + fixed3(lo) + gap + " y " + fixed3(hi) + gap + ".";
}

namespace {
/// "1.00" (beta) or "3.00 m" (user buckling length Lk: measured "xz: 1.00 m / 1.00" in the label; the info panel is assumed to use the same text).
std::string planeValueText(const BucklingPlane& p, double barLengthM) {
    return p.kind == BucklingKind::UserLength ? fixed2(p.lengthM) + " m" : fixed2(effectiveBeta(p, barLengthM));
}

bool flangeDrawn(const LateralBuckling& f) { return !(f.kind == LateralKind::None && f.reviewed); }
}  // namespace

std::vector<std::string> bucklingLabelLines(const BarBuckling& b, double barLengthM) {
    return {"xy: " + planeValueText(b.xy, barLengthM) + " / " + fixed2(b.xy.momentCoeff),
            "xz: " + planeValueText(b.xz, barLengthM) + " / " + fixed2(b.xz.momentCoeff)};
}

std::vector<std::string> lateralLabelLines(const BarBuckling& b, double) {
    // Measured (pandeo.md 9.6): a flange explicitly assigned "No comprueba" draws no line; a flange never assigned draws "L".
    std::vector<std::string> out;
    if (flangeDrawn(b.top)) out.push_back("Sup.: " + lateralLengthText(b.top) + ", " + fixed2(b.top.momentCoeff));
    if (flangeDrawn(b.bottom)) out.push_back("Inf.: " + lateralLengthText(b.bottom) + ", " + fixed2(b.bottom.momentCoeff));
    out.push_back("C1: " + fixed2(b.c1));
    return out;
}

std::vector<std::string> lateralInfoLines(const BarBuckling& b) {
    // Measured: the block only exists for a bar with lateral data (a never assigned bar shows no "Pandeo lateral" block).
    if (!b.top.reviewed && !b.bottom.reviewed) return {};
    std::vector<std::string> out{"Pandeo lateral"};
    if (flangeDrawn(b.top)) out.push_back("Sup. : " + lateralLengthText(b.top) + ", " + fixed2(b.top.momentCoeff));
    if (flangeDrawn(b.bottom)) out.push_back("Inf. : " + lateralLengthText(b.bottom) + ", " + fixed2(b.bottom.momentCoeff));
    out.push_back("C\u2081 : " + fixed2(b.c1));
    return out;
}

std::vector<std::string> bucklingInfoLines(const BarBuckling& b, double barLengthM) {
    return {"Pandeo",
            "xy : " + planeValueText(b.xy, barLengthM) + " / " + fixed2(b.xy.momentCoeff),
            "xz : " + planeValueText(b.xz, barLengthM) + " / " + fixed2(b.xz.momentCoeff),
            "Análisis de pandeo",
            b.considerInAnalysis ? "Se considera en el análisis" : "No se considera en el análisis"};
}

std::optional<BucklingIssue> checkBucklingEdit(const BucklingEdit& edit) {
    for (int pl = 0; pl < 2; ++pl) {
        if (!(pl == 0 ? edit.assignXY : edit.assignXZ)) continue;
        const BucklingPlaneEdit& e = pl == 0 ? edit.xy : edit.xz;
        switch (e.kind) {
            case BucklingKind::UserBeta:
                if (!inRange(e.beta, kBucklingBetaMin, kBucklingBetaMax)) return rangeIssue("Coeficiente de pandeo", kBucklingBetaMin, kBucklingBetaMax);
                break;
            case BucklingKind::UserLength:
                if (!inRange(e.lengthM, kBucklingLengthMin, kBucklingLengthMax)) return rangeIssue("Longitud", kBucklingLengthMin, kBucklingLengthMax, false, e.lengthM);
                break;
            case BucklingKind::Computed:
                if (!inRange(e.beta, 0.0, kBucklingBetaMax)) return rangeIssue("Coeficiente de pandeo", 0.0, kBucklingBetaMax);
                break;
            default: break;
        }
        if ((pl == 0 ? edit.assignMomentXY : edit.assignMomentXZ) && !inRange(e.momentCoeff, kMomentCoeffMin, kMomentCoeffMax))
            return rangeIssue("Coeficiente de momento equivalente Cm", kMomentCoeffMin, kMomentCoeffMax);
    }
    return std::nullopt;
}

std::optional<BucklingIssue> checkLateralEdit(const LateralEdit& edit) {
    for (int fl = 0; fl < 2; ++fl) {
        if (!(fl == 0 ? edit.assignTop : edit.assignBottom)) continue;
        const LateralBuckling& e = fl == 0 ? edit.top : edit.bottom;
        if (e.kind == LateralKind::UserCoeff && !inRange(e.coeff, kLateralCoeffMin, kLateralCoeffMax))
            return rangeIssue("Coeficiente de pandeo lateral", kLateralCoeffMin, kLateralCoeffMax);
        if (e.kind == LateralKind::UserSpacing && !inRange(e.spacingM, kBucklingLengthMin, kBucklingLengthMax))
            return rangeIssue("Separación entre arriostramientos", kBucklingLengthMin, kBucklingLengthMax, false, e.spacingM);
        if (e.kind != LateralKind::None && !inRange(e.momentCoeff, kMomentCoeffMin, kMomentCoeffMax))
            return rangeIssue("Coeficiente de momento equivalente Cm", kMomentCoeffMin, kMomentCoeffMax);
    }
    if (edit.assignC1 && !inRange(edit.c1, kMomentCoeffMin, kMomentCoeffMax))
        return rangeIssue("", kMomentCoeffMin, kMomentCoeffMax);  // empty name: defect of the original
    return std::nullopt;
}

BucklingPlane applyBucklingPlaneEdit(const BucklingPlane& current, const BucklingPlaneEdit& edit) {
    BucklingPlane p = current;
    p.reviewed = true;
    switch (edit.kind) {
        case BucklingKind::UserBeta:
            // Measured (pandeo.md 9.6): a typed value keeps the "Otros valores" option, even 0 / 0.5 / 1 / 2 (the radio is persisted).
            p.kind = BucklingKind::UserBeta;
            p.beta = edit.beta;
            break;
        case BucklingKind::UserLength:
            p.kind = BucklingKind::UserLength;
            p.lengthM = edit.lengthM;
            break;
        case BucklingKind::Computed:
            p.kind = BucklingKind::Computed;
            p.beta = edit.beta;
            p.translational = edit.translational;
            break;
        default:
            p.kind = edit.kind;
            p.beta = *bucklingBetaOfKind(edit.kind);
            break;
    }
    return p;
}

void applyBucklingEdit(BarBuckling& record, const BucklingEdit& edit) {
    if (edit.assignXY) record.xy = applyBucklingPlaneEdit(record.xy, edit.xy);
    if (edit.assignXZ) record.xz = applyBucklingPlaneEdit(record.xz, edit.xz);
    if (edit.assignMomentXY) record.xy.momentCoeff = edit.xy.momentCoeff;
    if (edit.assignMomentXZ) record.xz.momentCoeff = edit.xz.momentCoeff;
    if (edit.assignCriteria) {
        record.considerInAnalysis = edit.considerInAnalysis;
        record.updateXY = edit.updateXY;
        record.updateXZ = edit.updateXZ;
    }
}

void applyLateralEdit(BarBuckling& record, const LateralEdit& edit) {
    if (edit.assignTop) {
        record.top = edit.top;
        record.top.reviewed = true;
    }
    if (edit.assignBottom) {
        record.bottom = edit.bottom;
        record.bottom.reviewed = true;
    }
    if (edit.assignC1) record.c1 = edit.c1;
}

std::string validateBuckling(const BucklingData& data, const StructuralModel& model) {
    (void)model;  // entries of bars that no longer exist are tolerated (never saved): deleting a bar leaves its data for undo
    for (const auto& [id, r] : data.bars) {
        (void)id;
        if (isDefaultBuckling(r)) return "pandeo: registro por defecto almacenado";
        for (const BucklingPlane* p : {&r.xy, &r.xz}) {
            if (static_cast<int>(p->kind) < 0 || static_cast<int>(p->kind) > static_cast<int>(BucklingKind::Computed)) return "pandeo: tipo no válido";
            if (!std::isfinite(p->beta) || p->beta < 0.0 || p->beta > kBucklingBetaMax) return "pandeo: coeficiente fuera de rango";
            if (!inRange(p->lengthM, kBucklingLengthMin, kBucklingLengthMax)) return "pandeo: longitud fuera de rango";
            if (!inRange(p->momentCoeff, kMomentCoeffMin, kMomentCoeffMax)) return "pandeo: coeficiente de momento fuera de rango";
        }
        if (!inRange(r.c1, kMomentCoeffMin, kMomentCoeffMax)) return "pandeo lateral: C1 fuera de rango";
        for (const LateralBuckling* f : {&r.top, &r.bottom}) {
            if (static_cast<int>(f->kind) < 0 || static_cast<int>(f->kind) > static_cast<int>(LateralKind::UserSpacing)) return "pandeo lateral: tipo no válido";
            if (!inRange(f->coeff, kLateralCoeffMin, kLateralCoeffMax)) return "pandeo lateral: coeficiente fuera de rango";
            if (!inRange(f->spacingM, kBucklingLengthMin, kBucklingLengthMax)) return "pandeo lateral: separación fuera de rango";
            if (!inRange(f->momentCoeff, kMomentCoeffMin, kMomentCoeffMax)) return "pandeo lateral: coeficiente de momentos no válido";
        }
    }
    return {};
}

BucklingData reconcileBuckling(const BucklingData& data, const StructuralModel& before, const StructuralModel& after) {
    BucklingData out = data;
    for (const Bar& b : after.bars) {
        const Bar* old = before.findBar(b.id);
        if (!old || old->startNode != b.startNode || old->endNode == b.endNode) continue;
        const auto it = data.bars.find(b.id);
        if (it == data.bars.end()) continue;
        std::vector<int> chain;
        int current = b.endNode;
        std::set<int> seen;
        while (current != old->endNode) {
            int next = 0;
            for (const Bar& c : after.bars)
                if (!before.findBar(c.id) && c.startNode == current && !seen.count(c.id)) {
                    next = c.id;
                    break;
                }
            if (next == 0) break;
            seen.insert(next);
            chain.push_back(next);
            current = after.findBar(next)->endNode;
        }
        if (chain.empty() || current != old->endNode) continue;  // not a division: leave the data as it was
        for (int id : chain) out.bars[id] = it->second;
    }
    for (auto it = out.bars.begin(); it != out.bars.end();) it = after.findBar(it->first) ? std::next(it) : out.bars.erase(it);
    return out;
}

}  // namespace cc::core
