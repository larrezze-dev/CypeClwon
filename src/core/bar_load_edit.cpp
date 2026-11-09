#include "core/bar_load_edit.h"

#include <cmath>
#include <memory>
#include <set>

namespace cc::core {

std::string barLoadErrorMessage(BarLoadError error) {
    switch (error) {
        case BarLoadError::None: return {};
        case BarLoadError::NoBars: return "No hay ninguna barra a la que aplicar la carga.";
        case BarLoadError::UnknownLoad: return "La carga no existe.";
        case BarLoadError::UnknownLoadCase: return "La hipótesis no existe.";
        case BarLoadError::InvalidData: return "Los datos de la carga no son válidos.";
        case BarLoadError::NothingToChange: return "La operación no modifica las cargas.";
    }
    return {};
}

double shortestBarLength(const StructuralModel& model, const std::vector<int>& barIds) {
    double best = 0.0;
    for (int id : barIds) {
        if (!model.findBar(id)) continue;
        const double l = model.barLength(id);
        if (best == 0.0 || l < best) best = l;
    }
    return best;
}

namespace {

BarLoadError push(Project& project, UndoStack& stack, BarLoads after, const char* text) {
    auto cmd = std::make_unique<BarLoadCommand>(project, std::move(after), text);
    if (!cmd->valid()) return BarLoadError::NothingToChange;
    stack.push(std::move(cmd));
    return BarLoadError::None;
}

BarLoadError check(const Project& project, const BarLoadData& data, double lengthM, BarLoadIssue* issue) {
    const BarLoadIssue found = checkBarLoadData(data, lengthM);
    if (issue) *issue = found;
    if (!found.ok()) return BarLoadError::InvalidData;
    if (!findLoadCase(project.loadCases, data.loadCaseId)) return BarLoadError::UnknownLoadCase;
    return BarLoadError::None;
}

void apply(BarLoad& l, const BarLoadData& canonical) { static_cast<BarLoadData&>(l) = canonical; }

}  // namespace

BarLoadError addBarLoads(Project& project, UndoStack& stack, const std::vector<int>& barIds, const BarLoadData& data, std::vector<int>* ids,
                         BarLoadIssue* issue, BarLoadSession* session) {
    if (issue) *issue = {};
    std::vector<int> bars;
    std::set<int> seen;
    for (int id : barIds)
        if (project.structure.findBar(id) && seen.insert(id).second) bars.push_back(id);
    if (const BarLoadError e = check(project, data, shortestBarLength(project.structure, bars), issue); e != BarLoadError::None) return e;
    if (bars.empty()) return BarLoadError::NoBars;
    const BarLoadData canonical = normaliseBarLoadData(data);
    BarLoads after = project.barLoads;
    std::vector<int> created;
    for (int barId : bars) {
        BarLoad l;
        l.id = after.nextId++;
        l.barId = barId;
        apply(l, canonical);
        created.push_back(l.id);
        after.items.push_back(l);
    }
    const BarLoadError result = push(project, stack, std::move(after), "Nueva carga en barras");
    if (result == BarLoadError::None) {
        if (ids) *ids = std::move(created);
        if (session) session->remember(data);
    }
    return result;
}

BarLoadError editBarLoads(Project& project, UndoStack& stack, const std::vector<int>& loadIds, const BarLoadData& data, BarLoadIssue* issue,
                          BarLoadSession* session) {
    if (issue) *issue = {};
    const std::set<int> wanted(loadIds.begin(), loadIds.end());
    std::vector<int> bars;
    for (const BarLoad& l : project.barLoads.items)
        if (wanted.count(l.id)) bars.push_back(l.barId);
    if (const BarLoadError e = check(project, data, shortestBarLength(project.structure, bars), issue); e != BarLoadError::None) return e;
    if (bars.empty()) return BarLoadError::UnknownLoad;
    const BarLoadData canonical = normaliseBarLoadData(data);
    BarLoads after = project.barLoads;
    for (BarLoad& l : after.items)
        if (wanted.count(l.id)) apply(l, canonical);
    const BarLoadError result = push(project, stack, std::move(after), "Editar cargas en barras");
    if (result == BarLoadError::None && session) session->remember(data);
    return result;
}

BarLoadError changeBarLoadCase(Project& project, UndoStack& stack, const std::vector<int>& loadIds, int loadCaseId) {
    if (!findLoadCase(project.loadCases, loadCaseId)) return BarLoadError::UnknownLoadCase;
    BarLoads after = project.barLoads;
    const std::set<int> wanted(loadIds.begin(), loadIds.end());
    bool any = false;
    for (BarLoad& l : after.items)
        if (wanted.count(l.id)) {
            l.loadCaseId = loadCaseId;
            any = true;
        }
    if (!any) return BarLoadError::UnknownLoad;
    return push(project, stack, std::move(after), "Cambiar hipótesis de cargas en barras");
}

BarLoadError deleteBarLoads(Project& project, UndoStack& stack, const std::vector<int>& loadIds) {
    BarLoads after = project.barLoads;
    const std::set<int> wanted(loadIds.begin(), loadIds.end());
    const std::size_t before = after.items.size();
    after.items.erase(std::remove_if(after.items.begin(), after.items.end(), [&](const BarLoad& l) { return wanted.count(l.id) != 0; }),
                      after.items.end());
    if (after.items.size() == before) return BarLoadError::UnknownLoad;
    return push(project, stack, std::move(after), "Borrar cargas en barras");
}

BarLoadError setBarSelfWeight(Project& project, UndoStack& stack, const std::vector<int>& barIds, bool active) {
    BarLoads after = project.barLoads;
    bool any = false;
    for (int id : barIds) {
        if (!project.structure.findBar(id)) continue;
        any = true;
        if (active) after.selfWeightOff.erase(id);
        else after.selfWeightOff.insert(id);
    }
    if (!any) return BarLoadError::NoBars;
    return push(project, stack, std::move(after), active ? "Activar peso propio de barras" : "Desactivar peso propio de barras");
}

}  // namespace cc::core
