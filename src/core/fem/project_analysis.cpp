#include "core/fem/project_analysis.h"

#include <algorithm>

#include "core/fem/results.h"
#include "core/project.h"

namespace cc::fem {

std::string emptyWorkMessage() { return "La obra está vacía. Para poder calcular debe introducir, al menos, una barra o una lámina."; }
std::string mechanismReportMessage() { return "Esta estructura es un mecanismo. Revise los datos introducidos."; }
std::string calculatedWithoutChecksMessage() { return "La obra ha sido calculada sin realizar la comprobación de las barras."; }

namespace {

void add(ProjectAnalysis& a, ReportLevel level, const std::string& text) {
    for (const ReportEntry& e : a.report)
        if (e.level == level && e.text == text) return;  // one line per distinct message
    a.report.push_back({level, text});
}

/// The report line of a failed linear case / model.
std::string failureText(SolveStatus status, const std::string& message) {
    switch (status) {
        case SolveStatus::Mechanism: return mechanismReportMessage();
        case SolveStatus::ExcessiveDisplacements: return excessiveDisplacementsMessage();
        default: return message;
    }
}

}  // namespace

ProjectAnalysis analyseProject(const core::Project& project, const AnalysisOptions& options) {
    ProjectAnalysis a;
    a.nonlinearCombinations = project.nonlinearCombinations;
    if (project.structure.bars.empty() && project.structure.shells.empty()) {
        add(a, ReportLevel::Error, emptyWorkMessage());
        return a;
    }
    a.build = buildFrameModel(project);
    for (const std::string& e : a.build.errors) add(a, ReportLevel::Error, e);
    for (const std::string& u : a.build.unsupported) add(a, ReportLevel::Error, u);
    if (!a.build.errors.empty() || !a.build.unsupported.empty()) return a;
    for (const std::string& w : a.build.warnings) add(a, ReportLevel::Warning, w);

    // --- linear analysis of the load cases ---
    a.linear = solveLinear(a.build.model, a.build.cases, options.solver);
    bool ok = a.linear.status == SolveStatus::Ok && a.linear.cases.size() == a.build.cases.size();
    if (a.linear.status != SolveStatus::Ok) add(a, ReportLevel::Error, failureText(a.linear.status, a.linear.message));
    for (const LoadCaseResult& c : a.linear.cases)
        if (c.status != SolveStatus::Ok) {
            ok = false;
            add(a, ReportLevel::Error, failureText(c.status, c.message));
        }
    if (!ok) return a;
    a.calculated = true;

    // --- non-linear analysis (springs with «Sentido») ---
    a.hasNonlinearElements = hasUnilateralSprings(a.build.model);
    if (a.hasNonlinearElements && options.nonlinear) {
        core::NonlinearCombinations& set = a.nonlinearCombinations;
        const bool anyKind = std::any_of(set.generateKinds.begin(), set.generateKinds.end(), [](bool b) { return b; });
        if (!set.items.empty() && set.updateAfterLinear && anyKind) {
            const core::NonlinearGeneration g = core::generateNonlinearCombinations(project, set.generateKinds);
            if (g.error.empty()) {
                std::vector<core::NonlinearCombination> items = g.items;
                // Keep «Actúa» and «Escalones de carga» of the combinations that are generated again (same reference) [N].
                for (core::NonlinearCombination& n : items)
                    for (const core::NonlinearCombination& old : set.items)
                        if (old.reference == n.reference) {
                            n.acts = old.acts;
                            n.loadSteps = old.loadSteps;
                            break;
                        }
                if (items != set.items) {
                    set.items = std::move(items);
                    a.nonlinearCombinationsUpdated = true;
                }
            } else {
                add(a, ReportLevel::Error, g.error);
            }
        }
        if (set.items.empty()) {
            add(a, ReportLevel::Error, core::nonlinearAnalysisNotDoneMessage());
        } else if (const std::string err = core::checkNonlinearCombinations(set, project.loadCases); !err.empty()) {
            add(a, ReportLevel::Error, err);
        } else {
            for (std::size_t i = 0; i < set.items.size(); ++i) {
                const core::NonlinearCombination& nc = set.items[i];
                if (!nc.acts) continue;
                std::vector<double> coef(a.build.cases.size(), 0.0);
                for (std::size_t k = 0; k < a.build.cases.size(); ++k)
                    if (const auto it = nc.factors.find(a.build.cases[k].id); it != nc.factors.end()) coef[k] = it->second;
                a.nonlinearIndices.push_back(i);
                a.nonlinearInputs.push_back(
                    combineLoadCases(a.build.cases, coef, static_cast<int>(i + 1), core::nonlinearCombinationLabel(set, i)));
            }
            a.nonlinear = solveNonlinear(a.build.model, a.nonlinearInputs, options.solver);
            a.nonlinearCalculated = true;
            for (const LoadCaseResult& c : a.nonlinear.cases)
                if (c.status != SolveStatus::Ok) add(a, ReportLevel::Error, c.message);
        }
    }
    add(a, ReportLevel::Warning, calculatedWithoutChecksMessage());
    return a;
}

CombinationResults combinationResults(const ProjectAnalysis& analysis, std::span<const core::Combination> combinations) {
    CombinationResults out;
    std::vector<std::vector<double>> sets;
    std::vector<std::string> names;
    for (std::size_t k = 0; k < combinations.size(); ++k) {
        std::vector<double> coef(analysis.build.cases.size(), 0.0);
        for (std::size_t i = 0; i < coef.size() && i < combinations[k].coefficients.size(); ++i) coef[i] = combinations[k].coefficients[i];
        out.inputs.push_back(combineLoadCases(analysis.build.cases, coef, static_cast<int>(k + 1), combinations[k].name));
        sets.push_back(std::move(coef));
        names.push_back(combinations[k].name);
    }
    out.result = superposeResult(analysis.linear, sets, names);
    return out;
}

CombinationResults combinationResults(const ProjectAnalysis& analysis, const core::Project& project, core::StandardFamily family) {
    const core::CombinationResult r = core::generateCombinations(project, family);
    if (!r.error.empty()) {
        CombinationResults out;
        out.error = r.error;
        return out;
    }
    return combinationResults(analysis, r.combinations);
}

}  // namespace cc::fem
