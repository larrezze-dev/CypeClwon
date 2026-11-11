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
    double prestressFav = 1.0, prestressUnfav = 1.0;  ///< "Pretensado (PST)"; EHE-08 / Código Estructural quasi-permanent (cracking) 0.9 / 1.1 [V, research 14.2]
    /// Temperature row when it differs from the other variable actions (EHE-08 quasi-permanent cracking: 1.000 / 1.000 with psi 0, so the
    /// factor never reaches a coefficient) [V, research 14.2]; negative = same as the other variable actions (varFav / varUnfav).
    double temperatureFav = -1.0, temperatureUnfav = -1.0;
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
    /// Fraction (0..1) of the seismic effects of the orthogonal direction of an XY static seismic case: the "Acompañamiento" cell of the
    /// "Sismo" row of the seismic situation [V, research 14.1]. 0 = default (4 combinations measured); > 0 expands according to REA 7.1
    /// form 2 of FUN_1800035e0 [N: name and order not measured].
    double seismicOrthogonalFraction = 0.0;
    int maxCombinations = 20000;  ///< safety cap; above it the generation reports an error
};

struct Combination {
    DesignSituation situation = DesignSituation::Persistent;
    CombinationLimitState limitState = CombinationLimitState::Ultimate;
    std::vector<double> coefficients;  ///< one per load case, in the order of LoadCaseSet::cases (X component for an XY seismic case)
    /// Y component of the XY seismic cases (same indexing as `coefficients`, zero elsewhere); empty when the set has no XY seismic case.
    /// The names are `S1(X)` / `S1(Y)` [V, research 14.1].
    std::vector<double> orthogonalCoefficients;
    /// Formula name of the original (research 12.1), e.g. "1.35·PP+1.5·Q1+0.9·V(270°)H1+1.05·N(EI)", "PP-S1" (see combinationFormulaName).
    std::string name;
    /// Coefficient-set index per load case (-1 = does not act; permanent 0 = favourable / 1 = unfavourable; variable 0 = principal /
    /// 1 = accompanying; seismic 0 = "-" / 1 = "+"). The list is ordered by it, comparing from the last case to the first.
    std::vector<int> indices;
};

struct CombinationResult {
    std::vector<Combination> combinations;
    std::string error;  ///< empty when generation succeeded
};

/// Name of a combination in the original's format (measured, research 12.1): terms joined by "+", each "coef·REF" (U+00B7, coefficient
/// 1 omitted, minimum decimals), "PP" for the self weight, references without spaces, order PP, CM, Q, T, R, V, N, H, S, A (only
/// PP, Q, V, N, S, A measured), zero coefficients omitted, negative terms as "-S1".
/// `orthogonal` = Combination::orthogonalCoefficients: an XY seismic case is written `S1(X)` / `S1(Y)` (axis in brackets, X first).
std::string combinationFormulaName(const LoadCaseSet& set, const std::vector<double>& coefficients,
                                   const std::vector<double>& orthogonal = {});

/// Generates the combinations of every requested situation, in the order of the original: per situation (Persistent, Seismic,
/// Accidental, ELS characteristic = "Desplazamientos", frequent, quasi-permanent), then by coefficient-set index vector
/// (significance N > V > Q > PP, absent < principal < accompanying, favourable < unfavourable, seismic "-" < "+"), deduplicated by
/// coefficient vector. Changes of T-P7-06 for the frontend: `name` is the formula (no "Comb. n", no numbering), `indices` is new, wind
/// cases form one group of alternatives with sign + only, ELS characteristic = displacements (every variable 1.0, no principal, every
/// subset, + seismic +/- variants without wind / snow), accidental EC uses psi2 as principal (CTE psi1).
/// Self weight, dead loads and shrinkage are permanent; earth pressure is permanent with its own gamma; live loads, temperature
/// and snow are independent variables; seismic cases are alternatives with sign +/- [V section 7.1 form 2];
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
