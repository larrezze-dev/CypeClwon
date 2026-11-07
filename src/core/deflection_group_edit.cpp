#include "core/deflection_group_edit.h"

#include <algorithm>
#include <memory>

#include "core/piece_edit.h"  // findAlignedChain

namespace cc::core {

std::string deflectionErrorMessage(DeflectionError error) {
    switch (error) {
        case DeflectionError::None: return {};
        case DeflectionError::NoAlignedGroup:
            return "No se ha encontrado un grupo de barras alineadas que conecte los dos nudos seleccionados.";
        case DeflectionError::EquivalentExists: return "Ya existe un grupo de flecha equivalente.";
        case DeflectionError::DifferentArrangement: return "Las barras seleccionadas no tienen la misma disposición (giro y enrase).";
        case DeflectionError::UnknownElement: return "El elemento seleccionado no existe.";
        case DeflectionError::NothingToChange: return "La operación no modifica ningún grupo de flecha.";
    }
    return {};
}

std::vector<int> existingDeflectionBars(const Project& project, const std::vector<int>& ids) {
    std::vector<int> out;
    for (int id : ids)
        if (project.structure.findBar(id) && std::find(out.begin(), out.end(), id) == out.end()) out.push_back(id);
    return out;
}

DeflectionError checkCreateDeflectionGroup(const Project& project, int node1, int node2, std::vector<int>* chainOut) {
    const StructuralModel& model = project.structure;
    if (!model.findNode(node1) || !model.findNode(node2)) return DeflectionError::UnknownElement;
    const std::vector<int> chain = findAlignedChain(model, node1, node2);
    if (chain.empty()) return DeflectionError::NoAlignedGroup;
    if (chainOut) *chainOut = chain;

    std::vector<int> sorted = chain;
    std::sort(sorted.begin(), sorted.end());
    for (const DeflectionGroupInfo& g : deflectionGroups(project.deflection, model)) {
        std::vector<int> bars = g.bars;
        std::sort(bars.begin(), bars.end());
        if (bars == sorted) return DeflectionError::EquivalentExists;
    }

    // Same arrangement: equal roll angle and equal direction of the local x axis along the chain ("enrase" is not modelled).
    int current = node1;
    bool first = true;
    double roll = 0.0;
    bool direction = false;
    for (int barId : chain) {
        const Bar& b = *model.findBar(barId);
        const bool forward = b.startNode == current;
        const bool along = forward != b.xAxisInverted;
        if (first) {
            roll = b.rollAngleDeg;
            direction = along;
            first = false;
        } else if (b.rollAngleDeg != roll || along != direction) {
            return DeflectionError::DifferentArrangement;
        }
        current = forward ? b.endNode : b.startNode;
    }
    return DeflectionError::None;
}

DeflectionError createDeflectionGroup(Project& project, UndoStack& stack, int node1, int node2, DeflectionType xy, DeflectionType xz, int* number) {
    std::vector<int> chain;
    if (const DeflectionError error = checkCreateDeflectionGroup(project, node1, node2, &chain); error != DeflectionError::None) return error;
    const StructuralModel& model = project.structure;

    DeflectionData after = normaliseDeflection(project.deflection, model);
    // A new group prevails over every group that shares a bar with it (the ones left over regenerate as one-bar secant groups).
    after.groups.erase(std::remove_if(after.groups.begin(), after.groups.end(),
                                      [&](const DeflectionBarGroup& g) {
                                          return std::any_of(g.bars.begin(), g.bars.end(), [&](int id) { return std::find(chain.begin(), chain.end(), id) != chain.end(); });
                                      }),
                       after.groups.end());
    DeflectionBarGroup group;
    group.bars = chain;
    group.node1 = node1;
    group.xy = xy;
    group.xz = xz;
    after.groups.push_back(std::move(group));
    after = normaliseDeflection(after, model);

    auto cmd = std::make_unique<DeflectionCommand>(project, std::move(after), "Crear grupo de flecha");
    if (!cmd->valid()) return DeflectionError::NothingToChange;
    stack.push(std::move(cmd));
    if (number) {
        const auto list = deflectionGroups(project.deflection, model);
        const int index = deflectionGroupIndexOfBar(list, chain.front());
        *number = index < 0 ? 0 : list[static_cast<std::size_t>(index)].number;
    }
    return DeflectionError::None;
}

namespace {

/// Applies `modify` to every group (implicit ones included) that contains one of the given bars and pushes the resulting data.
template <typename Modify>
DeflectionError pushGroupEdit(Project& project, UndoStack& stack, const std::vector<int>& barIds, const char* text, Modify modify) {
    const std::vector<int> ids = existingDeflectionBars(project, barIds);
    if (ids.empty()) return DeflectionError::UnknownElement;
    const StructuralModel& model = project.structure;
    DeflectionData all;  // every group explicit; normalise strips the default ones
    for (const DeflectionGroupInfo& g : deflectionGroups(project.deflection, model)) {
        DeflectionBarGroup d;
        d.bars = g.bars;
        d.node1 = g.node1;
        d.xy = g.xy;
        d.xz = g.xz;
        d.limits = g.limits;
        const bool selected = std::any_of(g.bars.begin(), g.bars.end(), [&](int id) { return std::find(ids.begin(), ids.end(), id) != ids.end(); });
        if (selected) modify(d);
        all.groups.push_back(std::move(d));
    }
    auto cmd = std::make_unique<DeflectionCommand>(project, normaliseDeflection(all, model), text);
    if (!cmd->valid()) return DeflectionError::NothingToChange;
    stack.push(std::move(cmd));
    return DeflectionError::None;
}

}  // namespace

DeflectionError editDeflectionTypes(Project& project, UndoStack& stack, const std::vector<int>& barIds, DeflectionType xy, DeflectionType xz) {
    return pushGroupEdit(project, stack, barIds, "Editar grupo de flecha", [&](DeflectionBarGroup& g) {
        g.xy = xy;
        g.xz = xz;
    });
}

std::optional<DeflectionIssue> setDeflectionLimits(Project& project, UndoStack& stack, const std::vector<int>& barIds, const DeflectionLimits& limits) {
    if (auto issue = checkDeflectionLimits(limits)) return issue;
    pushGroupEdit(project, stack, barIds, "Flecha límite", [&](DeflectionBarGroup& g) { g.limits = limits; });
    return std::nullopt;
}

}  // namespace cc::core
