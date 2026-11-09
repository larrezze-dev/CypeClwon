#pragma once
#include <map>
#include <string>

#include "core/bar_description.h"
#include "core/categories.h"
#include "core/continuous_model.h"
#include "core/display_options.h"
#include "core/limit_states.h"
#include "core/bar_ends.h"
#include "core/bar_load.h"
#include "core/buckling.h"
#include "core/deflection_group.h"
#include "core/load_case.h"
#include "core/node_diaphragm.h"
#include "core/nodal_load.h"
#include "core/node_ligature.h"
#include "core/node_support.h"
#include "core/options_settings.h"
#include "core/profile_series.h"
#include "core/shell_load.h"
#include "core/shell_properties.h"
#include "core/project_settings.h"
#include "core/standards.h"
#include "core/structure_model.h"
#include "core/units.h"
#include "core/work_planes.h"

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
    DisplayOptions display;        ///< "Opciones de visualización"; stored with the project like the original
    CategorySettings categories;   ///< "Categorías" panel state
    WorkPlanes workPlanes;         ///< "Planos": grid lines, levels and plane groups (section work_planes)
    ProfileLibrary profileLibrary;  ///< "Biblioteca de perfiles" of the work (section profile_library); empty in a new work
    BarDescriptions barDescriptions;  ///< "Describir" of bars (section bar_descriptions); empty in older files
    ContinuousElements continuous;  ///< continuous pillars / beams, references, slab depths, deflection limits (section continuous_elements); empty in older files
    NodeSupports nodeSupports;  ///< external restraints and internal links of nodes (section node_supports); empty in older files
    NodeLigatures ligatures;  ///< node ties "Ligaduras" in X, Y, Z (section node_ties); empty in older files
    Diaphragms diaphragms;  ///< rigid diaphragms (section diaphragms); empty in older files
    BarEnds barEnds;  ///< articulations and partial fixity at the ends of bars (section bar_ends); empty in older files
    BucklingData buckling;  ///< member buckling coefficients and lateral buckling per bar (section buckling); empty in older files
    DeflectionData deflection;  ///< deflection groups that differ from "one secant group per bar" (section deflection_groups); empty in older files
    LoadCaseSet loadCases;  ///< load cases "Hipótesis de carga": self weight plus additional cases and use categories (section load_cases); only the self weight in older files
    BarLoads barLoads;  ///< bar loads and the bars without automatic self weight (section bar_loads); empty in older files
    ShellLoads shellLoads;  ///< shell loads and the shells without automatic self weight (section shell_loads); empty in older files
    NodalLoads nodalLoads;  ///< nodal loads and their drawing scales (section nodal_loads); empty in older files
    ShellProperties shellProps;  ///< shell properties: discretisation, stiffness factors, integration bands, edge bonds, buckling, modal (section shell_properties); empty in older files
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
