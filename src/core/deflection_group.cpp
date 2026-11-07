#include "core/deflection_group.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <set>

namespace cc::core {

namespace {

constexpr double kAlignmentToleranceM = 1e-6;  // same as kPieceAlignmentToleranceM (piece_edit.h)

Point3 minus(const Point3& a, const Point3& b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
double dotp(const Point3& a, const Point3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }

struct Chain {
    std::vector<int> bars;
    int node1 = 0, node2 = 0;
};

/// Orders `barIds` as a straight chain of consecutive bars; nullopt when they do not form one (missing bar, branch, gap, bend, fold-back).
std::optional<Chain> orderChain(const StructuralModel& model, const std::vector<int>& barIds, int preferredNode1) {
    if (barIds.empty()) return std::nullopt;
    std::map<int, std::vector<int>> at;  // node -> bars of the set that touch it
    for (int id : barIds) {
        const Bar* b = model.findBar(id);
        if (!b) return std::nullopt;
        at[b->startNode].push_back(id);
        at[b->endNode].push_back(id);
    }
    std::vector<int> ends;
    for (const auto& [node, list] : at) {
        if (list.size() == 1) ends.push_back(node);
        else if (list.size() != 2) return std::nullopt;
    }
    if (ends.size() != 2) return std::nullopt;
    Chain chain;
    chain.node1 = std::find(ends.begin(), ends.end(), preferredNode1) != ends.end() ? preferredNode1 : ends.front();
    std::set<int> used;
    int current = chain.node1;
    while (used.size() < barIds.size()) {
        int next = 0;
        for (int id : at[current])
            if (!used.count(id)) next = id;
        if (next == 0) return std::nullopt;
        used.insert(next);
        chain.bars.push_back(next);
        const Bar& b = *model.findBar(next);
        current = b.startNode == current ? b.endNode : b.startNode;
    }
    chain.node2 = current;
    if (chain.node2 == chain.node1) return std::nullopt;
    // Straight and monotonic along the line node1 -> node2.
    const Point3 p0 = model.findNode(chain.node1)->position;
    const Point3 d = minus(model.findNode(chain.node2)->position, p0);
    const double len = std::sqrt(dotp(d, d));
    if (len <= 0.0) return std::nullopt;
    double previous = 0.0;
    int node = chain.node1;
    for (int id : chain.bars) {
        const Bar& b = *model.findBar(id);
        node = b.startNode == node ? b.endNode : b.startNode;
        const Point3 v = minus(model.findNode(node)->position, p0);
        const Point3 c{v.y * d.z - v.z * d.y, v.z * d.x - v.x * d.z, v.x * d.y - v.y * d.x};
        const double along = dotp(v, d) / len;
        if (std::sqrt(dotp(c, c)) / len > kAlignmentToleranceM) return std::nullopt;
        if (along <= previous + kAlignmentToleranceM) return std::nullopt;
        previous = along;
    }
    return chain;
}

int otherNode(const Bar& b, int node) { return b.startNode == node ? b.endNode : b.startNode; }

}  // namespace

bool isPresetDeflectionDivisor(double divisor) { return divisor == 250.0 || divisor == 300.0 || divisor == 400.0 || divisor == 500.0; }

bool isDefaultDeflectionGroup(const DeflectionBarGroup& g) {
    return g.bars.size() == 1 && g.xy == DeflectionType::Secant && g.xz == DeflectionType::Secant && g.limits == DeflectionLimits{};
}

int barXOrigin(const Bar& bar) { return bar.xAxisInverted ? bar.endNode : bar.startNode; }

DeflectionData normaliseDeflection(const DeflectionData& data, const StructuralModel& model) {
    DeflectionData out;
    std::set<int> taken;
    for (const DeflectionBarGroup& g : data.groups) {
        DeflectionBarGroup n = g;
        n.bars.clear();
        for (int id : g.bars)
            if (model.findBar(id) && !taken.count(id) && std::find(n.bars.begin(), n.bars.end(), id) == n.bars.end()) n.bars.push_back(id);
        if (n.bars.empty()) continue;
        if (n.bars.size() == 1) {
            n.node1 = barXOrigin(*model.findBar(n.bars.front()));
        } else {
            const auto chain = orderChain(model, n.bars, g.node1);
            if (!chain) continue;  // dissolved: the bars become implicit groups
            n.bars = chain->bars;
            n.node1 = chain->node1;
        }
        if (isDefaultDeflectionGroup(n)) continue;
        for (int id : n.bars) taken.insert(id);
        out.groups.push_back(std::move(n));
    }
    return out;
}

std::vector<DeflectionGroupInfo> deflectionGroups(const DeflectionData& data, const StructuralModel& model) {
    const DeflectionData clean = normaliseDeflection(data, model);
    std::vector<DeflectionGroupInfo> list;
    std::set<int> covered;
    for (const DeflectionBarGroup& g : clean.groups) {
        DeflectionGroupInfo info;
        info.bars = g.bars;
        info.node1 = g.node1;
        if (g.bars.size() == 1) {
            info.node2 = otherNode(*model.findBar(g.bars.front()), g.node1);
        } else {
            info.node2 = orderChain(model, g.bars, g.node1)->node2;
        }
        info.xy = g.xy;
        info.xz = g.xz;
        info.limits = g.limits;
        for (int id : g.bars) covered.insert(id);
        list.push_back(std::move(info));
    }
    for (const Bar& b : model.bars) {
        if (covered.count(b.id)) continue;
        DeflectionGroupInfo info;
        info.bars = {b.id};
        info.node1 = barXOrigin(b);
        info.node2 = otherNode(b, info.node1);
        list.push_back(std::move(info));
    }
    std::sort(list.begin(), list.end(), [](const DeflectionGroupInfo& a, const DeflectionGroupInfo& b) {
        return *std::min_element(a.bars.begin(), a.bars.end()) < *std::min_element(b.bars.begin(), b.bars.end());
    });
    for (std::size_t i = 0; i < list.size(); ++i) list[i].number = static_cast<int>(i) + 1;
    return list;
}

int deflectionGroupIndexOfBar(const std::vector<DeflectionGroupInfo>& list, int barId) {
    for (std::size_t i = 0; i < list.size(); ++i)
        if (std::find(list[i].bars.begin(), list[i].bars.end(), barId) != list[i].bars.end()) return static_cast<int>(i);
    return -1;
}

std::string deflectionTypeLabel(DeflectionType type) {
    switch (type) {
        case DeflectionType::Secant: return "1-2";
        case DeflectionType::TangentToNode1: return "1";
        case DeflectionType::TangentToNode2: return "2";
    }
    return "1-2";
}

std::string deflectionGroupLabel(const DeflectionGroupInfo& g) {
    return "G" + std::to_string(g.number) + "(" + deflectionTypeLabel(g.xy) + " xy, " + deflectionTypeLabel(g.xz) + " xz)";
}

std::string deflectionTypeName(DeflectionType type) {
    switch (type) {
        case DeflectionType::Secant: return "Secante";
        case DeflectionType::TangentToNode1: return "Tangente a nudo 1";
        case DeflectionType::TangentToNode2: return "Tangente a nudo 2";
    }
    return "Secante";
}

std::string deflectionLimitName(LimitSlot slot) {
    static const char* const names[kLimitSlotCount] = {
        "Flecha máxima absoluta xy", "Flecha máxima absoluta xz", "Flecha activa absoluta xy", "Flecha activa absoluta xz",
        "Flecha máxima relativa xy", "Flecha máxima relativa xz", "Flecha activa relativa xy", "Flecha activa relativa xz"};
    return names[static_cast<int>(slot)];
}

bool isAbsoluteLimitSlot(LimitSlot slot) { return static_cast<int>(slot) < static_cast<int>(LimitSlot::MaxRelXY); }

std::optional<DeflectionIssue> checkDeflectionLimits(const DeflectionLimits& limits) {
    for (int i = 0; i < kLimitSlotCount; ++i) {
        const auto slot = static_cast<LimitSlot>(i);
        const DeflectionLimitItem& it = limits.item(slot);
        if (!it.enabled) continue;
        if (isAbsoluteLimitSlot(slot)) {
            if (!std::isfinite(it.absoluteMm) || it.absoluteMm <= kMinAbsoluteDeflectionLimit)
                return DeflectionIssue{0, "El valor de '" + deflectionLimitName(slot) + "' debe ser mayor que 0.00001."};
        } else if (!std::isfinite(it.relativeDivisor) || it.relativeDivisor <= kMinDeflectionDivisor) {
            return DeflectionIssue{0, "El valor de '" + deflectionLimitName(slot) + "' debe ser mayor que 0.0001."};
        }
    }
    return std::nullopt;
}

std::string validateDeflection(const DeflectionData& data) {
    std::set<int> seen;
    for (const DeflectionBarGroup& g : data.groups) {
        if (g.bars.empty()) return "grupos de flecha: grupo sin barras";
        if (isDefaultDeflectionGroup(g)) return "grupos de flecha: grupo por defecto almacenado";
        for (DeflectionType t : {g.xy, g.xz})
            if (static_cast<int>(t) < 0 || static_cast<int>(t) > static_cast<int>(DeflectionType::TangentToNode2))
                return "grupos de flecha: tipo no válido";
        if (checkDeflectionLimits(g.limits)) return "grupos de flecha: límite no válido";
        for (int id : g.bars)
            if (!seen.insert(id).second) return "grupos de flecha: barra en dos grupos";
    }
    return {};
}

DeflectionData reconcileDeflection(const DeflectionData& data, const StructuralModel& before, const StructuralModel& after) {
    DeflectionData out = data;
    for (const Bar& b : after.bars) {
        const Bar* old = before.findBar(b.id);
        if (!old) continue;
        // A shortened bar: exactly one end changed.
        int newEnd = 0, target = 0;
        if (old->startNode == b.startNode && old->endNode != b.endNode) {
            newEnd = b.endNode;
            target = old->endNode;
        } else if (old->endNode == b.endNode && old->startNode != b.startNode) {
            newEnd = b.startNode;
            target = old->startNode;
        } else {
            continue;
        }
        DeflectionBarGroup* group = nullptr;
        for (DeflectionBarGroup& g : out.groups)
            if (std::find(g.bars.begin(), g.bars.end(), b.id) != g.bars.end()) group = &g;
        if (!group) continue;
        std::vector<int> chain;
        std::set<int> seen;
        int current = newEnd;
        while (current != target) {
            int next = 0;
            for (const Bar& c : after.bars)
                if (!before.findBar(c.id) && !seen.count(c.id) && (c.startNode == current || c.endNode == current)) {
                    next = c.id;
                    break;
                }
            if (next == 0) break;
            seen.insert(next);
            chain.push_back(next);
            current = otherNode(*after.findBar(next), current);
        }
        if (chain.empty() || current != target) continue;  // not a division: leave the data as it was
        group->bars.insert(group->bars.end(), chain.begin(), chain.end());
    }
    return normaliseDeflection(out, after);
}

DeflectionData invertDeflectionGroups(const DeflectionData& data, const std::vector<int>& inverted, const StructuralModel& after) {
    DeflectionData out = data;
    for (DeflectionBarGroup& g : out.groups) {
        const bool all = std::all_of(g.bars.begin(), g.bars.end(), [&](int id) { return std::find(inverted.begin(), inverted.end(), id) != inverted.end(); });
        if (!all) continue;
        int node2 = 0;
        if (g.bars.size() == 1) {
            const Bar* b = after.findBar(g.bars.front());
            if (!b) continue;
            node2 = barXOrigin(*b);  // after the inversion the x origin of the bar is the old node 2
        } else if (const auto chain = orderChain(after, g.bars, g.node1)) {
            node2 = chain->node2;
        } else {
            continue;
        }
        auto swapType = [](DeflectionType t) {
            return t == DeflectionType::TangentToNode1 ? DeflectionType::TangentToNode2
                   : t == DeflectionType::TangentToNode2 ? DeflectionType::TangentToNode1 : t;
        };
        g.xy = swapType(g.xy);
        g.xz = swapType(g.xz);
        std::reverse(g.bars.begin(), g.bars.end());
        g.node1 = node2;
    }
    return normaliseDeflection(out, after);
}

}  // namespace cc::core
