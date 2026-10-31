#pragma once
#include <string>

#include "core/project.h"
#include "core/undo_stack.h"

namespace cc::core {

/// "Datos generales" of a project (Obra > Datos generales): everything the "Obra vacía" wizard sets, i.e. every
/// member of Project except the structural model and the display units.
struct GeneralData {
    StandardsSelection standards;
    ProfileSettings profiles;
    ReinforcedConcreteSettings concrete;
    ActionsSettings actions;
    EnvironmentSettings environment;
    GroundSettings ground;
    LimitStatesSettings limitStates;
    PileSettings piles;
    MicropileSettings micropiles;
    OptionsSettings options;
};

GeneralData generalDataOf(const Project& project);
/// Replaces the general data of `project`; the structural model and the units are left untouched.
void applyGeneralData(Project& project, const GeneralData& data);
/// True when both describe the same settings (compared through their saved form, so it needs no operator== per type).
bool sameGeneralData(const GeneralData& a, const GeneralData& b);

/// Undoable edit of the general data of a project. The project must outlive the command (the owner clears the undo
/// stack whenever the project is replaced).
class EditGeneralDataCommand : public UndoCommand {
public:
    EditGeneralDataCommand(Project& project, GeneralData newData);
    void redo() override;
    void undo() override;
    std::string text() const override { return "Datos generales"; }

private:
    Project& m_project;
    GeneralData m_before;
    GeneralData m_after;
};

}  // namespace cc::core
