#include "core/shell_load_edit.h"

#include <memory>
#include <set>

namespace cc::core {

std::string shellLoadErrorMessage(ShellLoadError error) {
    switch (error) {
        case ShellLoadError::None: return {};
        case ShellLoadError::NoShells: return "No hay ninguna lámina a la que aplicar la carga.";
        case ShellLoadError::UnknownLoad: return "La carga no existe.";
        case ShellLoadError::UnknownLoadCase: return "La hipótesis no existe.";
        case ShellLoadError::InvalidData: return "Los datos de la carga no son válidos.";
        case ShellLoadError::NothingToChange: return "La operación no modifica las cargas.";
    }
    return {};
}

namespace {

ShellLoadError push(Project& project, UndoStack& stack, ShellLoads after, const char* text) {
    auto cmd = std::make_unique<ShellLoadCommand>(project, std::move(after), text);
    if (!cmd->valid()) return ShellLoadError::NothingToChange;
    stack.push(std::move(cmd));
    return ShellLoadError::None;
}

ShellLoadError check(const Project& project, const ShellLoadData& data, ShellLoadIssue* issue) {
    const ShellLoadIssue found = checkShellLoadData(data);
    if (issue) *issue = found;
    if (!found.ok()) return ShellLoadError::InvalidData;
    if (!findLoadCase(project.loadCases, data.loadCaseId)) return ShellLoadError::UnknownLoadCase;
    return ShellLoadError::None;
}

}  // namespace

ShellLoadError addShellLoads(Project& project, UndoStack& stack, const std::vector<int>& shellIds, const ShellLoadData& data,
                             std::vector<int>* ids, ShellLoadIssue* issue, ShellLoadSession* session) {
    if (issue) *issue = {};
    if (const ShellLoadError e = check(project, data, issue); e != ShellLoadError::None) return e;
    std::vector<int> shells;
    std::set<int> seen;
    for (int id : shellIds)
        if (project.structure.findShell(id) && seen.insert(id).second) shells.push_back(id);
    if (shells.empty()) return ShellLoadError::NoShells;
    const ShellLoadData canonical = normaliseShellLoadData(data);
    ShellLoads after = project.shellLoads;
    std::vector<int> created;
    for (int shellId : shells) {
        ShellLoad l;
        l.id = after.nextId++;
        l.shellId = shellId;
        static_cast<ShellLoadData&>(l) = canonical;
        created.push_back(l.id);
        after.items.push_back(l);
    }
    const ShellLoadError result = push(project, stack, std::move(after), "Nueva carga en láminas");
    if (result == ShellLoadError::None) {
        if (ids) *ids = std::move(created);
        if (session) session->remember(data);
    }
    return result;
}

ShellLoadError editShellLoads(Project& project, UndoStack& stack, const std::vector<int>& loadIds, const ShellLoadData& data, ShellLoadIssue* issue,
                              ShellLoadSession* session) {
    if (issue) *issue = {};
    if (const ShellLoadError e = check(project, data, issue); e != ShellLoadError::None) return e;
    const std::set<int> wanted(loadIds.begin(), loadIds.end());
    const ShellLoadData canonical = normaliseShellLoadData(data);
    ShellLoads after = project.shellLoads;
    bool any = false;
    for (ShellLoad& l : after.items)
        if (wanted.count(l.id)) {
            static_cast<ShellLoadData&>(l) = canonical;
            any = true;
        }
    if (!any) return ShellLoadError::UnknownLoad;
    const ShellLoadError result = push(project, stack, std::move(after), "Editar cargas en láminas");
    if (result == ShellLoadError::None && session) session->remember(data);
    return result;
}

ShellLoadError changeShellLoadCase(Project& project, UndoStack& stack, const std::vector<int>& loadIds, int loadCaseId) {
    if (!findLoadCase(project.loadCases, loadCaseId)) return ShellLoadError::UnknownLoadCase;
    ShellLoads after = project.shellLoads;
    const std::set<int> wanted(loadIds.begin(), loadIds.end());
    bool any = false;
    for (ShellLoad& l : after.items)
        if (wanted.count(l.id)) {
            l.loadCaseId = loadCaseId;
            any = true;
        }
    if (!any) return ShellLoadError::UnknownLoad;
    return push(project, stack, std::move(after), "Cambiar hipótesis de cargas en láminas");
}

ShellLoadError deleteShellLoads(Project& project, UndoStack& stack, const std::vector<int>& loadIds) {
    ShellLoads after = project.shellLoads;
    const std::set<int> wanted(loadIds.begin(), loadIds.end());
    const std::size_t before = after.items.size();
    after.items.erase(std::remove_if(after.items.begin(), after.items.end(), [&](const ShellLoad& l) { return wanted.count(l.id) != 0; }),
                      after.items.end());
    if (after.items.size() == before) return ShellLoadError::UnknownLoad;
    return push(project, stack, std::move(after), "Borrar cargas en láminas");
}

ShellLoadError setShellSelfWeight(Project& project, UndoStack& stack, const std::vector<int>& shellIds, bool active) {
    ShellLoads after = project.shellLoads;
    bool any = false;
    for (int id : shellIds) {
        if (!project.structure.findShell(id)) continue;
        any = true;
        if (active) after.selfWeightOff.erase(id);
        else after.selfWeightOff.insert(id);
    }
    if (!any) return ShellLoadError::NoShells;
    return push(project, stack, std::move(after), active ? "Activar peso propio de láminas" : "Desactivar peso propio de láminas");
}

}  // namespace cc::core
