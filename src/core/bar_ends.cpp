#include "core/bar_ends.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <set>

namespace cc::core {

namespace {

std::string fixed(double v, int decimals) {
    char buf[64];
    std::snprintf(buf, sizeof buf, "%.*f", decimals, v);
    return buf;
}

std::string planeText(const EndPlaneFixity& p) {
    if (p.kind == EndFixityKind::Coefficient) return fixed(p.coefficient, 3);
    return fixed(p.stiffnessKnMPerRad, 2) + " kN·m/rad";
}

}  // namespace

bool isDefaultEnd(const BarEndFixity& end) { return end == BarEndFixity{}; }

BarEndsRecord barEndsOf(const BarEnds& ends, int barId) {
    const auto it = ends.bars.find(barId);
    return it == ends.bars.end() ? BarEndsRecord{} : it->second;
}

void setBarEnds(BarEnds& ends, int barId, const BarEndsRecord& record) {
    if (record == BarEndsRecord{}) ends.bars.erase(barId);
    else ends.bars[barId] = record;
}

BarEndsRecord swappedEnds(const BarEndsRecord& record) {
    BarEndsRecord out;
    out.ends[0] = record.ends[1];
    out.ends[1] = record.ends[0];
    return out;
}

std::string endFixityLabel(const BarEndFixity& end) {
    return "xy: " + planeText(end.planes[kPlaneXY]) + " xz: " + planeText(end.planes[kPlaneXZ]) + " yz: " + planeText(end.planes[kPlaneYZ]);
}

std::string endTooltipLine(const BarEndsRecord& record, int endIndex) {
    const int i = endIndex == kBarFinal ? kBarFinal : kBarOrigin;
    return std::string(i == kBarOrigin ? "Origen : " : "Extremo : ") + endFixityLabel(record.ends[static_cast<std::size_t>(i)]);
}

std::string endFixityRangeMessage() { return "El valor de '' debe estar entre 0.000 y 1.000 ."; }

std::string endFixityDefaultStiffnessQuestion() {
    return "El valor de la rigidez rotacional introducido coincide con el valor por defecto. Recuerde que debe asignar un valor conforme al "
           "grado de empotramiento que posee la unión en el extremo de la pieza. ¿Desea continuar?";
}

std::optional<EndFixityIssue> checkEndFixityEdit(const EndFixityEdit& edit) {
    for (int e = 0; e < 2; ++e)
        for (int pl = 0; pl < 3; ++pl) {
            if (!edit.assignPlane[static_cast<std::size_t>(pl)]) continue;
            const EndPlaneFixity& f = edit.value[static_cast<std::size_t>(e)][static_cast<std::size_t>(pl)];
            if (f.kind == EndFixityKind::Coefficient) {
                const double c = f.coefficient;
                if (!std::isfinite(c) || c < 0.0 || c > 1.0) return EndFixityIssue{0, endFixityRangeMessage()};
                // TODO(medir): the original offers a binary popup 0.000 / 1.000 for the torsion plane.
                if (pl == kPlaneYZ && c != 0.0 && c != 1.0) return EndFixityIssue{0, endFixityRangeMessage()};
            } else if (!std::isfinite(f.stiffnessKnMPerRad) || f.stiffnessKnMPerRad <= 0.0) {
                return EndFixityIssue{0, "El valor de la rigidez rotacional debe ser mayor que cero."};  // TODO(medir): text and range
            }
        }
    return std::nullopt;
}

bool endFixityEditAsksDefaultStiffness(const EndFixityEdit& edit) {
    for (int e = 0; e < 2; ++e)
        for (int pl = 0; pl < 3; ++pl) {
            if (!edit.assignPlane[static_cast<std::size_t>(pl)]) continue;
            const EndPlaneFixity& f = edit.value[static_cast<std::size_t>(e)][static_cast<std::size_t>(pl)];
            if (f.kind == EndFixityKind::Rotational && f.stiffnessKnMPerRad == kDefaultEndRotationalStiffness) return true;
        }
    return false;
}

void applyEndFixityEdit(BarEndsRecord& record, const EndFixityEdit& edit) {
    for (int e = 0; e < 2; ++e)
        for (int pl = 0; pl < 3; ++pl)
            if (edit.assignPlane[static_cast<std::size_t>(pl)])
                record.ends[static_cast<std::size_t>(e)].planes[static_cast<std::size_t>(pl)] = edit.value[static_cast<std::size_t>(e)][static_cast<std::size_t>(pl)];
}

std::string validateBarEnds(const BarEnds& ends, const StructuralModel& model) {
    for (const auto& [id, rec] : ends.bars) {
        (void)model;  // entries of bars that no longer exist are tolerated (never saved): deleting a bar leaves its data for undo
        if (rec == BarEndsRecord{}) return "extremos de barra: registro por defecto almacenado";
        for (const BarEndFixity& e : rec.ends)
            for (const EndPlaneFixity& p : e.planes) {
                if (!std::isfinite(p.coefficient) || p.coefficient < 0.0 || p.coefficient > 1.0) return "extremos de barra: coeficiente fuera de rango";
                if (!std::isfinite(p.stiffnessKnMPerRad) || p.stiffnessKnMPerRad <= 0.0) return "extremos de barra: rigidez no válida";
            }
    }
    return {};
}

BarEnds reconcileBarEnds(const BarEnds& ends, const StructuralModel& before, const StructuralModel& after) {
    BarEnds out = ends;
    for (const Bar& b : after.bars) {
        const Bar* old = before.findBar(b.id);
        if (!old || old->startNode != b.startNode || old->endNode == b.endNode) continue;
        const auto it = out.bars.find(b.id);
        if (it == out.bars.end()) continue;
        // Follow the new stretches that continue the shortened bar until the original end node is reached.
        int current = b.endNode;
        int last = 0;
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
            last = next;
            current = after.findBar(next)->endNode;
        }
        if (last == 0 || current != old->endNode) continue;  // not a division: leave the data as it was
        BarEndsRecord head = it->second, tail;
        tail.ends[kBarFinal] = head.ends[kBarFinal];
        head.ends[kBarFinal] = BarEndFixity{};
        setBarEnds(out, b.id, head);
        setBarEnds(out, last, tail);
    }
    for (auto it = out.bars.begin(); it != out.bars.end();) it = after.findBar(it->first) ? std::next(it) : out.bars.erase(it);
    return out;
}

}  // namespace cc::core
