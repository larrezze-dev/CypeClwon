#include "core/load_case_edit.h"

#include <algorithm>
#include <memory>

namespace cc::core {

std::string loadCaseErrorMessage(LoadCaseError error) {
    switch (error) {
        case LoadCaseError::None: return {};
        case LoadCaseError::BlankReference: return "'Referencia' no puede quedar en blanco.";
        case LoadCaseError::NeedsUseCategories:  // #39206
            return "Para poder definir las hipótesis de sobrecarga de uso, primero se deben definir las categorías de uso de la obra.";
        case LoadCaseError::UnknownCase: return "La hipótesis no existe.";
        case LoadCaseError::ProtectedCase: return "La hipótesis de peso propio no se puede editar ni borrar.";
        case LoadCaseError::InvalidArrangement: return "La disposición de cargas indicada no es válida.";
        case LoadCaseError::UnknownCategory: return "La categoría de uso no existe.";
        case LoadCaseError::NothingToChange: return "La operación no modifica las hipótesis.";
    }
    return {};
}

namespace {

LoadCaseError push(Project& project, UndoStack& stack, LoadCaseSet after, const char* text) {
    auto cmd = std::make_unique<LoadCaseCommand>(project, std::move(after), text);
    if (!cmd->valid()) return LoadCaseError::NothingToChange;
    stack.push(std::move(cmd));
    return LoadCaseError::None;
}

LoadCase* mutableCase(LoadCaseSet& set, int id) {
    for (LoadCase& c : set.cases)
        if (c.id == id) return &c;
    return nullptr;
}

}  // namespace

LoadCaseError createLoadCase(Project& project, UndoStack& stack, const NewLoadCase& data, int* id) {
    if (!loadNatureAcceptsAdditional(data.nature)) return LoadCaseError::ProtectedCase;
    if (data.withArrangements && data.nature == LoadNature::Seismic) return LoadCaseError::InvalidArrangement;
    if (data.reference.empty()) return LoadCaseError::BlankReference;
    if (data.nature == LoadNature::LiveLoad && project.loadCases.useCategories.empty()) return LoadCaseError::NeedsUseCategories;

    LoadCaseSet after = project.loadCases;
    LoadCase c;
    c.id = after.nextId++;
    c.nature = data.nature;
    c.reference = data.reference;
    c.description = data.description;
    if (data.withArrangements) {
        for (int i = 1; i <= kDefaultArrangementCount; ++i) c.arrangements.push_back(std::to_string(i));
        c.compat = data.compat;
    }
    if (data.nature == LoadNature::Seismic) c.seismic = data.seismic;
    ++after.created[static_cast<std::size_t>(data.nature)];
    const int newId = c.id;
    after.cases.push_back(std::move(c));
    const LoadCaseError result = push(project, stack, std::move(after), "Nueva hipótesis adicional");
    if (result == LoadCaseError::None && id) *id = newId;
    return result;
}

LoadCaseError editLoadCase(Project& project, UndoStack& stack, int id, const std::string& reference, const std::string& description) {
    if (id == 0) return LoadCaseError::ProtectedCase;
    LoadCaseSet after = project.loadCases;
    LoadCase* c = mutableCase(after, id);
    if (!c) return LoadCaseError::UnknownCase;
    if (reference.empty()) return LoadCaseError::BlankReference;
    c->reference = reference;
    c->description = description;
    return push(project, stack, std::move(after), "Editar hipótesis adicional");
}

LoadCaseError toggleLoadCaseActs(Project& project, UndoStack& stack, int id) {
    if (id == 0) return LoadCaseError::ProtectedCase;
    LoadCaseSet after = project.loadCases;
    LoadCase* c = mutableCase(after, id);
    if (!c) return LoadCaseError::UnknownCase;
    c->acts = !c->acts;
    return push(project, stack, std::move(after), "Actúa / No actúa");
}

LoadCaseDependents loadCaseDependents(const Project& project, int id) {
    LoadCaseDependents out;
    out.loads = nodalLoadsOfCase(project.nodalLoads, id);
    for (const auto& [nodeId, support] : project.nodeSupports.external)
        for (const PrescribedDisplacement& p : support.prescribed)
            if (p.loadCaseId == id) {
                out.prescribedNodes.push_back(nodeId);
                break;
            }
    // TODO(T-P7-03..05, T-P7-06): bar / surface loads, non-linear combinations and buckling combinations do not exist yet.
    return out;
}

DeleteLoadCaseCommand::DeleteLoadCaseCommand(Project& project, LoadCaseSet casesAfter, int loadCaseId)
    : m_project(project), m_caseId(loadCaseId), m_casesBefore(project.loadCases), m_casesAfter(std::move(casesAfter)),
      m_loadsBefore(project.nodalLoads), m_loadsAfter(project.nodalLoads), m_supportsBefore(project.nodeSupports),
      m_supportsAfter(project.nodeSupports) {
    applyNodalLoadsCaseDeleted(m_loadsAfter, loadCaseId);
    for (auto& [nodeId, support] : m_supportsAfter.external)
        support.prescribed.erase(std::remove_if(support.prescribed.begin(), support.prescribed.end(),
                                                [&](const PrescribedDisplacement& p) { return p.loadCaseId == loadCaseId; }),
                                 support.prescribed.end());
}

void DeleteLoadCaseCommand::redo() {
    m_project.loadCases = m_casesAfter;
    const int next = std::max(m_project.nodalLoads.nextId, m_loadsAfter.nextId);
    m_project.nodalLoads = m_loadsAfter;
    m_project.nodalLoads.nextId = next;
    m_project.nodeSupports = m_supportsAfter;
}

void DeleteLoadCaseCommand::undo() {
    m_project.loadCases = m_casesBefore;
    const int next = std::max(m_project.nodalLoads.nextId, m_loadsBefore.nextId);
    m_project.nodalLoads = m_loadsBefore;
    m_project.nodalLoads.nextId = next;
    m_project.nodeSupports = m_supportsBefore;
}

LoadCaseError deleteLoadCase(Project& project, UndoStack& stack, int id) {
    if (id == 0) return LoadCaseError::ProtectedCase;
    LoadCaseSet after = project.loadCases;
    const auto it = std::find_if(after.cases.begin(), after.cases.end(), [&](const LoadCase& c) { return c.id == id; });
    if (it == after.cases.end()) return LoadCaseError::UnknownCase;
    after.cases.erase(it);  // the per-nature counter is kept: the numbers are not reused (PROVISIONAL)
    // Nodal loads and prescribed sets of the case go with it (same command); TODO(T-P7-03..06): bar / surface loads and combinations.
    stack.push(std::make_unique<DeleteLoadCaseCommand>(project, std::move(after), id));
    return LoadCaseError::None;
}

LoadCaseError addLoadArrangement(Project& project, UndoStack& stack, int id, std::string* name) {
    LoadCaseSet after = project.loadCases;
    LoadCase* c = mutableCase(after, id);
    if (!c) return LoadCaseError::UnknownCase;
    if (!c->hasArrangements()) return LoadCaseError::InvalidArrangement;
    // Next number after the highest numeric name ("3" after "1", "2"); names are always numbers.
    int highest = 0;
    for (const std::string& n : c->arrangements) {
        try {
            highest = std::max(highest, std::stoi(n));
        } catch (...) {
        }
    }
    const std::string added = std::to_string(highest + 1);
    c->arrangements.push_back(added);
    const LoadCaseError result = push(project, stack, std::move(after), "Añadir disposición");
    if (result == LoadCaseError::None && name) *name = added;
    return result;
}

LoadCaseError deleteLoadArrangement(Project& project, UndoStack& stack, int id, std::size_t index) {
    LoadCaseSet after = project.loadCases;
    LoadCase* c = mutableCase(after, id);
    if (!c) return LoadCaseError::UnknownCase;
    if (!c->hasArrangements() || index >= c->arrangements.size()) return LoadCaseError::InvalidArrangement;
    // An empty list would turn the case into one without arrangements; the original's minimum (#39208, not verified) is not enforced
    // beyond keeping one entry.
    if (c->arrangements.size() == 1) return LoadCaseError::InvalidArrangement;
    c->arrangements.erase(c->arrangements.begin() + static_cast<std::ptrdiff_t>(index));
    return push(project, stack, std::move(after), "Borrar disposición");
}

LoadCaseError setSeismicSecondOrder(Project& project, UndoStack& stack, bool value) {
    LoadCaseSet after = project.loadCases;
    after.seismicSecondOrder = value;
    return push(project, stack, std::move(after), "Efectos de segundo orden del sismo");
}

LoadCaseError addUseCategory(Project& project, UndoStack& stack, const std::array<std::string, kUseCategoryNormGroups>& choices) {
    LoadCaseSet after = project.loadCases;
    UseCategory u;
    u.ref = nextUseCategoryRef(after);
    u.choices = choices;
    u.description = choices[0];
    after.useCategories.push_back(std::move(u));
    return push(project, stack, std::move(after), "Nueva categoría de uso");
}

LoadCaseError setUseCategoryChoices(Project& project, UndoStack& stack, std::size_t index,
                                    const std::array<std::string, kUseCategoryNormGroups>& choices) {
    LoadCaseSet after = project.loadCases;
    if (index >= after.useCategories.size()) return LoadCaseError::UnknownCategory;
    after.useCategories[index].choices = choices;
    after.useCategories[index].description = choices[0];
    return push(project, stack, std::move(after), "Editar categoría de uso");
}

LoadCaseError copyUseCategory(Project& project, UndoStack& stack, std::size_t index) {
    LoadCaseSet after = project.loadCases;
    if (index >= after.useCategories.size()) return LoadCaseError::UnknownCategory;
    UseCategory u = after.useCategories[index];
    u.ref = nextUseCategoryRef(after);
    after.useCategories.push_back(std::move(u));
    return push(project, stack, std::move(after), "Copiar categoría de uso");
}

LoadCaseError deleteUseCategory(Project& project, UndoStack& stack, std::size_t index) {
    LoadCaseSet after = project.loadCases;
    if (index >= after.useCategories.size()) return LoadCaseError::UnknownCategory;
    after.useCategories.erase(after.useCategories.begin() + static_cast<std::ptrdiff_t>(index));
    return push(project, stack, std::move(after), "Borrar categoría de uso");
}

}  // namespace cc::core
