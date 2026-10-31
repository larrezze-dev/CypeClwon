#pragma once
#include <map>
#include <string>

#include "core/limit_states.h"
#include "core/options_settings.h"
#include "core/project_settings.h"
#include "core/standards.h"
#include "core/structure_model.h"
#include "core/units.h"

namespace cc::core {

/// Everything a project (.ed3) stores today. Each member is saved as its own section of the file, so the model of
/// nodes, members and shells (T-P3-01) will add new sections without touching the existing ones.
struct Project {
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
    DimensioningUnits units;  ///< "Unidades de acotación"; cm / cm / m when a file has no such section
    StructuralModel structure;  ///< nodes, materials, cross sections, bars and shells (sections nodes ... shells)
};

/// Header of a project file, readable without loading the project.
struct ProjectManifest {
    int formatVersion = 0;       ///< version of the file layout (see kProjectFormatVersion)
    std::string appVersion;      ///< application version that last saved the file
    std::string description;     ///< "Descripción" of "Gestión archivos"
    std::string createdAt;       ///< ISO 8601, UTC ("2026-10-31T12:00:00Z")
    std::string modifiedAt;
};

/// A project together with the data of its file that the application does not interpret.
struct ProjectDocument {
    ProjectManifest manifest;
    Project project;
    /// Sections written by a newer build (or by a module that is not loaded): kept verbatim, name -> JSON text, and
    /// written back on save so that opening and saving never destroys data.
    std::map<std::string, std::string> unknownSections;
};

}  // namespace cc::core
