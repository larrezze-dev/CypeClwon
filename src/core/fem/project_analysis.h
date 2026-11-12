#pragma once
#include <span>
#include <string>
#include <vector>

#include "core/combinations.h"
#include "core/fem/frame_builder.h"
#include "core/fem/linear_solver.h"
#include "core/nonlinear_combinations.h"

namespace cc::fem {

/// Calculation of a project («Cálculo > Calcular», T-P8-06, core part). Source: docs/research/resultados_calculo.md.
///
/// Flow (what the original does, [V] unless marked):
///  1. A work without bars and shells: #64197 and nothing is calculated.
///  2. buildFrameModel(); its `errors` («Existen barras sin describir.», ranges of the shear areas, ...) stop the calculation; so do the data the
///     clone does not model yet (`unsupported`, clone-specific texts) because presenting results without them would be wrong.
///  3. Linear analysis of every load case («hipótesis»): solveLinear(). The springs with «Sentido» are bilateral here [V 10.1].
///     A mechanism («Esta estructura es un mecanismo. Revise los datos introducidos.» in the report, the detailed warning in LinearResult::message)
///     or «desplazamientos demasiado grandes» disables every result (the result buttons of the original become inactive) [V].
///  4. Non-linear analysis, only when the model has springs with «Sentido» (hasUnilateralSprings) and AnalysisOptions::nonlinear: without
///     non-linear combinations the report says #378613 (red icon) [V s_5.png]; with them, «Actualizar las combinaciones no lineales tras el análisis
///     lineal» regenerates them from the last kinds chosen ([N]: what the update keeps of manual edits), #381789 stops it when an acting
///     combination uses a load case that does not act, and every ACTING combination is solved by solveNonlinear() as «[n] reference». A failed
///     combination gives its message (#214001 «No es posible calcular el caso de carga '[1] PP'. La estructura es un mecanismo...», red icon [V])
///     and the linear results stay available ([N]).
///  5. «La obra ha sido calculada sin realizar la comprobación de las barras.» (bar checks are T-P9) [V s_5.png, warning icon].

enum class ReportLevel { Info, Warning, Error };

/// One line of the «Informe final de cálculo» under «Cálculo de la estructura» [V s_5.png].
struct ReportEntry {
    ReportLevel level = ReportLevel::Info;
    std::string text;
};

struct AnalysisOptions {
    SolverOptions solver;
    bool nonlinear = true;  ///< «Análisis no lineal de tensión / deformación» (offered only with unilateral springs; checked by default [V])
};

struct ProjectAnalysis {
    FrameBuild build;
    LinearResult linear;                       ///< one case per load case, build.cases order (= Project::loadCases order)
    bool calculated = false;                   ///< the linear results can be presented
    bool hasNonlinearElements = false;         ///< springs with «Sentido»: the original enables the non-linear analysis
    core::NonlinearCombinations nonlinearCombinations;  ///< the set used (regenerated when «Actualizar...» is on); store it back in the project
    bool nonlinearCombinationsUpdated = false; ///< the set above differs from the project's (regenerated)
    std::vector<std::size_t> nonlinearIndices; ///< index in nonlinearCombinations.items of every solved (acting) combination
    std::vector<LoadCaseInput> nonlinearInputs;  ///< combined cases named «[n] reference», same order as `nonlinear.cases`
    LinearResult nonlinear;                    ///< results of the acting non-linear combinations
    bool nonlinearCalculated = false;          ///< the non-linear analysis was run (some combination may still have failed)
    std::vector<ReportEntry> report;
};

/// «La obra está vacía. Para poder calcular debe introducir, al menos, una barra o una lámina.» (#64197, ed3dlib.dll).
std::string emptyWorkMessage();
/// «Esta estructura es un mecanismo. Revise los datos introducidos.» (the line of the final report for a mechanism [V tk108_B1.png]).
std::string mechanismReportMessage();
/// «La obra ha sido calculada sin realizar la comprobación de las barras.» [V s_5.png].
std::string calculatedWithoutChecksMessage();

ProjectAnalysis analyseProject(const core::Project& project, const AnalysisOptions& options = {});

/// The linear combinations of a family of the project (core::generateCombinations) as results: `inputs` (combined loads, for the bar queries) and
/// `result` (superposition, with the mesh: shell queries by index). The names are the formula names of the combinations. Seismic XY cases: the
/// orthogonal (Y) coefficients are not modelled (the builder has no seismic loads yet) and are ignored [N].
struct CombinationResults {
    std::vector<LoadCaseInput> inputs;
    LinearResult result;
    std::string error;  ///< error of the combination generator
};
CombinationResults combinationResults(const ProjectAnalysis& analysis, std::span<const core::Combination> combinations);
CombinationResults combinationResults(const ProjectAnalysis& analysis, const core::Project& project, core::StandardFamily family);

}  // namespace cc::fem
