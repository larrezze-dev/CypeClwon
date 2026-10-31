#include "core/general_data.h"

#include <utility>

#include "core/project_json.h"

namespace cc::core {

GeneralData generalDataOf(const Project& p) {
    return GeneralData{p.standards, p.profiles, p.concrete, p.actions,   p.environment,
                       p.ground,    p.limitStates, p.piles, p.micropiles, p.options};
}

void applyGeneralData(Project& p, const GeneralData& d) {
    p.standards = d.standards;
    p.profiles = d.profiles;
    p.concrete = d.concrete;
    p.actions = d.actions;
    p.environment = d.environment;
    p.ground = d.ground;
    p.limitStates = d.limitStates;
    p.piles = d.piles;
    p.micropiles = d.micropiles;
    p.options = d.options;
}

bool sameGeneralData(const GeneralData& a, const GeneralData& b) {
    Project pa, pb;
    applyGeneralData(pa, a);
    applyGeneralData(pb, b);
    for (const char* name : {"standards", "profiles", "concrete", "actions", "environment", "ground", "limit_states", "piles",
                             "micropiles", "options"})
        if (project_json::dumpSection(pa, name) != project_json::dumpSection(pb, name)) return false;
    return true;
}

EditGeneralDataCommand::EditGeneralDataCommand(Project& project, GeneralData newData)
    : m_project(project), m_before(generalDataOf(project)), m_after(std::move(newData)) {}

void EditGeneralDataCommand::redo() { applyGeneralData(m_project, m_after); }
void EditGeneralDataCommand::undo() { applyGeneralData(m_project, m_before); }

}  // namespace cc::core
