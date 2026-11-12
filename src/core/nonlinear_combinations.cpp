#include "core/nonlinear_combinations.h"

#include <algorithm>
#include <cstdio>

#include "core/combinations.h"
#include "core/load_case.h"
#include "core/project.h"

namespace cc::core {

std::string nonlinearCombinationKindName(NonlinearCombinationKind kind) {
    switch (kind) {
        case NonlinearCombinationKind::Strength: return "Resistencia";
        case NonlinearCombinationKind::Cracking: return "Fisuración";
        case NonlinearCombinationKind::LongTermCracking: return "Fisuración bajo cargas de larga duración";
        case NonlinearCombinationKind::Displacements: return "Desplazamientos";
        case NonlinearCombinationKind::FoundationEquilibrium: return "Equilibrio de los elementos de cimentación";
        case NonlinearCombinationKind::GroundStresses: return "Tensiones en el terreno";
    }
    return {};
}

std::string nonlinearCombinationLabel(const NonlinearCombinations& set, std::size_t index) {
    const std::string ref = index < set.items.size() ? set.items[index].reference : std::string();
    return "[" + std::to_string(index + 1) + "] " + ref;
}

NonlinearGeneration generateNonlinearCombinations(const Project& project, const NonlinearKinds& kinds) {
    // Mapping of the kinds to the combinations of core::generateCombinations:
    //  - Resistencia: the ultimate limit state situations (persistent, seismic, accidental) with the code of the CONCRETE family
    //    (combinationOptionsFor). Measured with the default norms of a new work: "PP", "1.35·PP" [V s_10.png], i.e. a favourable factor 1.0
    //    for the self weight: the concrete family (Eurocódigo 2 -> EN 1990) gives it, the hot-rolled steel family (Código Estructural with the
    //    CTE radio -> CTE DB SE, favourable 0.8) would give "0.8·PP". Which family the original really reads is [N] (resultados_calculo.md 2.1).
    //  - Desplazamientos: the characteristic service combinations (the "Desplazamientos" family of core::generateCombinations) [V same work: adds
    //    "PP", already present, so the list stays "PP", "1.35·PP"].
    //  - Fisuración, Fisuración bajo cargas de larga duración, Equilibrio de los elementos de cimentación, Tensiones en el terreno: NOT generated
    //    ([N] situations of each kind not measured; the foundation kinds need the foundation module). They are returned in `unsupported`.
    NonlinearGeneration out;
    const CombinationOptions base = combinationOptionsFor(project, StandardFamily::Concrete);
    std::vector<std::vector<double>> seen;
    const auto add = [&](const Combination& c, NonlinearCombinationKind kind) {
        const auto k = static_cast<std::size_t>(kind);
        for (std::size_t i = 0; i < seen.size(); ++i)
            if (seen[i] == c.coefficients) {
                out.items[i].kinds[k] = true;
                return;
            }
        NonlinearCombination nc;
        nc.reference = c.name;
        for (std::size_t i = 0; i < c.coefficients.size() && i < project.loadCases.cases.size(); ++i)
            if (c.coefficients[i] != 0.0) nc.factors[project.loadCases.cases[i].id] = c.coefficients[i];
        nc.kinds[k] = true;
        seen.push_back(c.coefficients);
        out.items.push_back(std::move(nc));
    };
    for (int k = 0; k < kNonlinearCombinationKindCount; ++k) {
        if (!kinds[static_cast<std::size_t>(k)]) continue;
        const auto kind = static_cast<NonlinearCombinationKind>(k);
        CombinationOptions options = base;
        if (kind == NonlinearCombinationKind::Strength) {
            options.withUltimate = true;
            options.withService = false;
        } else if (kind == NonlinearCombinationKind::Displacements) {
            options.withUltimate = false;
            options.withService = true;
        } else {
            out.unsupported.push_back(kind);
            continue;
        }
        const CombinationResult r = generateCombinations(project.loadCases, options);
        if (!r.error.empty()) {
            out.error = r.error;
            return out;
        }
        for (const Combination& c : r.combinations) {
            if (kind == NonlinearCombinationKind::Displacements && c.situation != DesignSituation::ServiceCharacteristic) continue;
            add(c, kind);
        }
    }
    return out;
}

std::string nonlinearCaseNotActingMessage() {
    return "No es posible realizar el cálculo ya que hay combinaciones no lineales en las que intervienen hipótesis que no actúan. Elimine las "
           "combinaciones afectadas o borre las hipótesis que no actúan.";
}

std::string checkNonlinearCombinations(const NonlinearCombinations& set, const LoadCaseSet& cases) {
    for (const NonlinearCombination& c : set.items) {
        if (!c.acts) continue;
        for (const auto& [id, coef] : c.factors) {
            if (coef == 0.0) continue;
            const auto it = std::find_if(cases.cases.begin(), cases.cases.end(), [&](const LoadCase& lc) { return lc.id == id; });
            if (it != cases.cases.end() && !it->acts) return nonlinearCaseNotActingMessage();
        }
    }
    return {};
}

std::string nonlinearCombinationsOfCaseWillBeDeletedMessage() {
    return "Se van a borrar las combinaciones no lineales y de pandeo en las que interviene la hipótesis.";
}

bool nonlinearCombinationsUseCase(const NonlinearCombinations& set, int loadCaseId) {
    for (const NonlinearCombination& c : set.items) {
        const auto it = c.factors.find(loadCaseId);
        if (it != c.factors.end() && it->second != 0.0) return true;
    }
    return false;
}

void applyNonlinearCombinationsCaseDeleted(NonlinearCombinations& set, int loadCaseId) {
    std::erase_if(set.items, [&](const NonlinearCombination& c) {
        const auto it = c.factors.find(loadCaseId);
        return it != c.factors.end() && it->second != 0.0;
    });
    for (NonlinearCombination& c : set.items) c.factors.erase(loadCaseId);
}

std::string nonlinearAnalysisNotDoneMessage() { return "No se ha realizado el análisis de tensión/deformación no lineal"; }

std::string generatedCombinationsMessage(std::size_t count) {
    char buf[96];
    std::snprintf(buf, sizeof buf, "Se han generado '%zu' combinaciones.", count);
    return buf;
}

}  // namespace cc::core
