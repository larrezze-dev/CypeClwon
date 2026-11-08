#pragma once
#include <array>
#include <optional>
#include <string>
#include <vector>

namespace cc::core {

/// Load cases ("Hipótesis de carga", T-P7-01, core part). Behaviour source: docs/research/ui/hipotesis_carga.md (REA of combs3.dll,
/// catuso.dll, ed3dlib.dll and measurements in the original).
///
/// Model. The set always holds "Peso propio" as case 0 (automatic, id 0, neither editable nor deletable) followed by the additional cases
/// the user created, in creation order. Every case has a stable id (never reused) so that loads (T-P7-02..05) and combinations can refer to it.
/// Per nature there is a counter of the cases created (never decremented when a case is deleted: PROVISIONAL, the original's behaviour after
/// a deletion was not verified); the proposed reference is "<prefix> <counter + 1>" ("CM 1", "Q 1", ...).
///
/// NOT VERIFIED (TODO(medir)): duplicate references (message #375648 exists, its use for load cases is unconfirmed, so duplicates are allowed
/// here); maximum length of reference / description; minimum number of arrangements (#39208 suggests 2, not tested, not enforced); which
/// letter the original gives to a new use category after deleting one (here: the first unused letter A..Z, then AA..).

/// Order = rows of "Hipótesis adicionales".
enum class LoadNature {
    SelfWeight = 0,  ///< "Peso propio" (automatic only)
    DeadLoad,        ///< "Cargas muertas" (CM)
    LiveLoad,        ///< "Sobrecarga de uso" (Q); needs use categories
    Temperature,     ///< "Temperatura" (T)
    Shrinkage,       ///< "Retracción" (R)
    Wind,            ///< "Viento" (V)
    Seismic,         ///< "Sismo" (S)
    Snow,            ///< "Nieve" (N)
    EarthPressure,   ///< "Empujes del terreno" (H)
    Accidental,      ///< "Accidental" (A)
};
inline constexpr int kLoadNatureCount = 10;

/// Radios of "Con distintas disposiciones de cargas". Default Simultaneous.
enum class ArrangementCompat { Compatible = 0, Incompatible, Simultaneous };

/// Radios of "Tipo de sismo estático". Default Simple.
enum class SeismicType { Simple = 0, XY };

/// Number of norm groups that choose a use category (concrete / foundation / aluminium / ground stresses; rolled / cold-formed steel / timber).
inline constexpr int kUseCategoryNormGroups = 2;
/// Number of arrangements a new case "with different load arrangements" starts with ("1" and "2"), measured.
inline constexpr int kDefaultArrangementCount = 2;

struct LoadCase {
    int id = 0;  ///< stable; 0 = "Peso propio"
    LoadNature nature = LoadNature::SelfWeight;
    std::string reference;    ///< short name ("CM 1")
    std::string description;  ///< optional text
    bool acts = true;         ///< column "Actúa": true = taken into account, false = "No actúa"
    std::vector<std::string> arrangements;  ///< names "1", "2", ... ; empty = case without arrangements
    ArrangementCompat compat = ArrangementCompat::Simultaneous;  ///< meaningful only with arrangements
    SeismicType seismic = SeismicType::Simple;                   ///< meaningful only for Seismic
    bool hasArrangements() const { return !arrangements.empty(); }
    bool operator==(const LoadCase&) const = default;
};

/// A use category of the work: letter "Ref." plus one chosen entry per norm group (text as shown in the combos).
struct UseCategory {
    std::string ref;          ///< "A", "B", ...
    std::string description;  ///< "A. Domésticos y residenciales" (the choice of the first group)
    std::array<std::string, kUseCategoryNormGroups> choices{};
    bool operator==(const UseCategory&) const = default;
};

struct LoadCaseSet {
    std::vector<LoadCase> cases;  ///< cases[0] is always "Peso propio"
    std::array<int, kLoadNatureCount> created{};  ///< cases created per nature (the next proposal is created + 1)
    std::vector<UseCategory> useCategories;
    bool seismicSecondOrder = false;  ///< "Considerar efectos de segundo orden" (#46224)
    int nextId = 1;
    LoadCaseSet();
    bool operator==(const LoadCaseSet&) const = default;
};

/// "Peso propio", "Cargas muertas", ...
std::string loadNatureName(LoadNature nature);
/// Prefix of the proposed reference: "CM", "Q", "T", "R", "V", "S", "N", "H", "A"; empty for SelfWeight (not seen in the original).
std::string loadNaturePrefix(LoadNature nature);
/// True when the nature takes additional cases (every one but SelfWeight).
bool loadNatureAcceptsAdditional(LoadNature nature);

/// Proposed reference for the next case of the nature: "CM 1" for the first, "CM 2" after one was created, ...
std::string proposedLoadReference(const LoadCaseSet& set, LoadNature nature);

const LoadCase* findLoadCase(const LoadCaseSet& set, int id);
/// Additional cases of a nature (the "Adicionales" counter is the size of this list).
std::vector<const LoadCase*> loadCasesOfNature(const LoadCaseSet& set, LoadNature nature);
/// Text of the combo "Hipótesis": the reference ("Peso propio" for case 0). TODO(medir): whether the original appends the description.
std::string loadCaseLabel(const LoadCase& c);

/// Letter proposed for a new use category: first unused "A".."Z", then "AA"...
std::string nextUseCategoryRef(const LoadCaseSet& set);

/// Objects that depend on a load case and are removed with it (nodal loads: T-P7-02; hooks for T-P7-03..06; see deleteLoadCase).
struct LoadCaseDependents {
    std::vector<int> loads;                  ///< ids of the loads defined in the case (nodal so far; bar, surface... later)
    std::vector<int> prescribedNodes;        ///< nodes whose prescribed displacements include a set for the case (removed with it; not measured)
    std::vector<int> nonlinearCombinations;  ///< non-linear combinations that use it
    std::vector<int> bucklingCombinations;   ///< buckling combinations that use it
    bool empty() const { return loads.empty() && prescribedNodes.empty() && nonlinearCombinations.empty() && bucklingCombinations.empty(); }
};

/// Internal consistency of the stored set (case 0 is the self weight, unique ids below nextId, non-blank references, extra cases of a nature
/// that takes additional ones, arrangement data only where it applies, counters not below the number of cases, categories with a ref).
/// Empty when valid.
std::string validateLoadCases(const LoadCaseSet& set);

}  // namespace cc::core
