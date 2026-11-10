#pragma once
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "core/load_case.h"
#include "core/standards.h"

namespace cc::core {

struct Project;

/// Combinations of actions by design code (T-P7-06, core part, no GUI). Source: docs/research/ui/combinaciones_normas.md
/// (REA of sithipcmb/ctesebas/eurocod/ehe/gestcomp). Tags below follow that document: [V] value read in the binaries,
/// [N] deduced / assumed (not verified).

/// Code whose gamma / psi tables are applied. EC = EN 1990 with the choice of case B or C for persistent situations.
enum class NormCode { CteDbSe, En1990CaseB, En1990CaseC, Ehe08 };

/// "Altitud superior a 1000 m" / "inferior o igual a 1000 m" (CTE and EN 1990) / "Finland, Iceland, Norway, Sweden" (EN 1990 only).
enum class SnowZone { Above1000, AtOrBelow1000, Nordic };

enum class DesignSituation { Persistent, Seismic, Accidental, ServiceCharacteristic, ServiceFrequent, ServiceQuasiPermanent };
enum class CombinationLimitState { Ultimate, Service };

/// Partial safety factors of one situation (favourable / unfavourable). Non-constant permanent = EHE-08 art. 12.1 (applied here
/// to earth pressure and shrinkage under EHE-08 [N]); earth = CTE DB-SE 4.2.2 earth pressure (persistent only, else permanent).
struct PartialFactors {
    double permFav = 1.0, permUnfav = 1.0;
    double varFav = 0.0, varUnfav = 1.0;
    double earthFav = 1.0, earthUnfav = 1.0;
    double nonConstFav = 1.0, nonConstUnfav = 1.0;
    bool operator==(const PartialFactors&) const = default;
};

struct Psi {
    double psi0 = 0.0, psi1 = 0.0, psi2 = 0.0;
    bool operator==(const Psi&) const = default;
};

/// Gamma factors of the code for a situation (service situations use 1.0 throughout, EN 1990 Table A1.4 / CTE).
PartialFactors partialFactors(NormCode code, DesignSituation situation);
/// Psi of a variable action. `useCategory` is the norm letter ("A".."E","G1","G2" for CTE; "A".."H" for EN 1990); returns nullopt
/// for an unknown letter. `annex` 8 / 9 = EN 1990 national annexes with the Malaysia / Singapore values [N: country mapping].
std::optional<Psi> psiLiveLoad(NormCode code, const std::string& useCategory, int annex = 0);
Psi psiWind(NormCode code, int annex = 0);
Psi psiTemperature();
Psi psiSnow(NormCode code, SnowZone zone);
/// CTE "G1" (roof for maintenance only, not concomitant): main factor 0 in the ordinary situations, 1 in the extra "(G1)" one.
bool isNonConcomitantCategory(const std::string& useCategory);

/// Section 7.1: effective coefficient of a variable, g_l*p_l*f + g_c*p_c*(1 - f), f = long-term fraction in [0, 1].
double effectiveVariableCoefficient(double gammaLong, double psiLong, double gammaShort, double psiShort, double longTermFraction);

struct CombinationOptions {
    NormCode code = NormCode::CteDbSe;
    SnowZone snow = SnowZone::AtOrBelow1000;
    int nationalAnnex = 0;  ///< EN 1990 annex index (section 4.5); only 8 / 9 change the values
    /// Norm letter of a live-load case without an entry in `caseUseCategory` (the model does not link cases to categories yet).
    std::string defaultUseCategory = "A";
    std::map<int, std::string> caseUseCategory;  ///< load-case id -> norm letter of its use category
    double longTermFraction = 0.0;               ///< 0 = instantaneous. [N] long pair = (gamma, psi2)
    bool withSeismic = true, withAccidental = true;
    bool withUltimate = true, withService = true;
    int maxCombinations = 20000;  ///< safety cap; above it the generation reports an error
};

struct Combination {
    DesignSituation situation = DesignSituation::Persistent;
    CombinationLimitState limitState = CombinationLimitState::Ultimate;
    std::vector<double> coefficients;  ///< one per load case, in the order of LoadCaseSet::cases
    std::string name;                  ///< "Comb. n" (ASSUMPTION: the original's text was not found), numbered per limit state
};

struct CombinationResult {
    std::vector<Combination> combinations;
    std::string error;  ///< empty when generation succeeded
};

/// Generates the combinations of every requested situation: ordered per situation (Persistent, Seismic, Accidental, ELS
/// characteristic, frequent, quasi-permanent), lexicographically by coefficient vector, deduplicated by vector.
/// Self weight, dead loads (and shrinkage [N]) are permanent; earth pressure is permanent with its own gamma; live loads, temperature
/// and snow are independent variables; wind cases (and seismic cases) are alternatives with sign +/- [V section 7.1 form 2];
/// accidental cases act only in the accidental situation. Cases that do not act (`acts == false`) are skipped; load arrangements
/// are not expanded yet (ticket).
CombinationResult generateCombinations(const LoadCaseSet& set, const CombinationOptions& options);

/// Code recognised from a standard name of "Selección de normas" ("Eurocódigo 2 (UE)" -> case B, "EHE-08", "Código Estructural
/// (España)" -> Ehe08, "CTE DB SE-A (España)" -> CteDbSe); nullopt otherwise.
std::optional<NormCode> combinationCodeFromStandard(const std::string& standardName);
/// Snow zone from the combo text ("Altitud superior a 1000 m" -> Above1000, anything else -> AtOrBelow1000).
SnowZone snowZoneFromText(const std::string& text);

/// Options for the limit states of a norm family of the project: code from the chosen standard (hot-rolled steel honours the
/// CTE / EC radio of "Estados límite"), snow from the settings of that family. Unrecognised standards fall back to CTE.
CombinationOptions combinationOptionsFor(const Project& project, StandardFamily family);

/// Convenience: generateCombinations(project.loadCases, combinationOptionsFor(project, family)).
CombinationResult generateCombinations(const Project& project, StandardFamily family);

}  // namespace cc::core
