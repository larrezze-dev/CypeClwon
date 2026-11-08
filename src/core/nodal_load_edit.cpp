#include "core/nodal_load_edit.h"

#include <cmath>
#include <memory>
#include <set>

namespace cc::core {

std::string nodalLoadErrorMessage(NodalLoadError error) {
    switch (error) {
        case NodalLoadError::None: return {};
        case NodalLoadError::NoNodes: return "No hay ningún nudo al que aplicar la carga.";
        case NodalLoadError::UnknownLoad: return "La carga no existe.";
        case NodalLoadError::UnknownLoadCase: return "La hipótesis no existe.";
        case NodalLoadError::InvalidValue: return nodalLoadRangeMessage();
        case NodalLoadError::InvalidDirection: return nodalLoadDirectionMessage();
        case NodalLoadError::InvalidScale: return "La escala debe ser un valor positivo.";
        case NodalLoadError::NothingToChange: return "La operación no modifica las cargas.";
    }
    return {};
}

namespace {

NodalLoadError push(Project& project, UndoStack& stack, NodalLoads after, const char* text) {
    auto cmd = std::make_unique<NodalLoadCommand>(project, std::move(after), text);
    if (!cmd->valid()) return NodalLoadError::NothingToChange;
    stack.push(std::move(cmd));
    return NodalLoadError::None;
}

NodalLoadError checkData(const Project& project, const NodalLoadData& data) {
    if (!nodalLoadValueInRange(data.magnitude)) return NodalLoadError::InvalidValue;
    if (!checkNodalLoadData(data).empty()) return NodalLoadError::InvalidDirection;
    if (!findLoadCase(project.loadCases, data.loadCaseId)) return NodalLoadError::UnknownLoadCase;
    return NodalLoadError::None;
}

void apply(NodalLoad& l, const NodalLoadData& data) {
    l.loadCaseId = data.loadCaseId;
    l.magnitude = roundLoadValue(data.magnitude);
    l.direction = data.direction;
    l.userVector = data.userVector;
}

}  // namespace

NodalLoadError addNodalLoads(Project& project, UndoStack& stack, const std::vector<int>& nodeIds, const NodalLoadData& data,
                             std::vector<int>* ids) {
    if (const NodalLoadError e = checkData(project, data); e != NodalLoadError::None) return e;
    NodalLoads after = project.nodalLoads;
    std::set<int> seen;
    std::vector<int> created;
    for (int nodeId : nodeIds) {
        if (!project.structure.findNode(nodeId) || !seen.insert(nodeId).second) continue;
        NodalLoad l;
        l.id = after.nextId++;
        l.nodeId = nodeId;
        apply(l, data);
        created.push_back(l.id);
        after.items.push_back(l);
    }
    if (created.empty()) return NodalLoadError::NoNodes;
    const NodalLoadError result = push(project, stack, std::move(after), "Nueva carga en nudos");
    if (result == NodalLoadError::None && ids) *ids = std::move(created);
    return result;
}

NodalLoadError editNodalLoads(Project& project, UndoStack& stack, const std::vector<int>& loadIds, const NodalLoadData& data) {
    if (const NodalLoadError e = checkData(project, data); e != NodalLoadError::None) return e;
    NodalLoads after = project.nodalLoads;
    const std::set<int> wanted(loadIds.begin(), loadIds.end());
    bool any = false;
    for (NodalLoad& l : after.items)
        if (wanted.count(l.id)) {
            apply(l, data);
            any = true;
        }
    if (!any) return NodalLoadError::UnknownLoad;
    return push(project, stack, std::move(after), "Editar cargas en nudos");
}

NodalLoadError changeNodalLoadCase(Project& project, UndoStack& stack, const std::vector<int>& loadIds, int loadCaseId) {
    if (!findLoadCase(project.loadCases, loadCaseId)) return NodalLoadError::UnknownLoadCase;
    NodalLoads after = project.nodalLoads;
    const std::set<int> wanted(loadIds.begin(), loadIds.end());
    bool any = false;
    for (NodalLoad& l : after.items)
        if (wanted.count(l.id)) {
            l.loadCaseId = loadCaseId;
            any = true;
        }
    if (!any) return NodalLoadError::UnknownLoad;
    return push(project, stack, std::move(after), "Cambiar hipótesis de cargas en nudos");
}

NodalLoadError deleteNodalLoads(Project& project, UndoStack& stack, const std::vector<int>& loadIds) {
    NodalLoads after = project.nodalLoads;
    const std::set<int> wanted(loadIds.begin(), loadIds.end());
    const std::size_t before = after.items.size();
    after.items.erase(std::remove_if(after.items.begin(), after.items.end(), [&](const NodalLoad& l) { return wanted.count(l.id) != 0; }),
                      after.items.end());
    if (after.items.size() == before) return NodalLoadError::UnknownLoad;
    return push(project, stack, std::move(after), "Borrar cargas en nudos");
}

NodalLoadError setLoadScales(Project& project, UndoStack& stack, const LoadScales& scales) {
    if (!(std::isfinite(scales.fixedSizeM) && scales.fixedSizeM > 0.0)) return NodalLoadError::InvalidScale;
    for (const auto& [id, v] : scales.perCase)
        if (!(std::isfinite(v) && v > 0.0)) return NodalLoadError::InvalidScale;
    for (double v : scales.perType)
        if (!(std::isfinite(v) && v >= kMinTypeScale && v <= kMaxTypeScale)) return NodalLoadError::InvalidScale;
    NodalLoads after = project.nodalLoads;
    after.scales = scales;
    return push(project, stack, std::move(after), "Escalas de cargas");
}

}  // namespace cc::core
