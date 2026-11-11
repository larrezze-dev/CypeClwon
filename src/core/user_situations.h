#pragma once
#include <string>
#include <vector>

namespace cc::core {

/// Project situations defined by the user (T-P7-06, GUI part). Measured in the original: docs/research/ui/combinaciones_normas.md
/// section 12.4 ("Crear - [Situaciones]", "Coeficientes de combinación", "Categorías de uso", "Situación de proyecto definida por el usuario").

/// Type combo of "Situación de proyecto definida por el usuario" (the same four entries for "Desplazamientos").
/// `Service` = situation of an E.L.S. set (cracking "Cuasipermanente"): no type combo and no combination columns in the original [V, research 14.4].
enum class SituationType { PersistentOrTransient, Accidental, Seismic, PrestressInVacuum, Service };

/// Radio of "Coeficientes de combinación": partial factors only, or partial factors and combination factors (psi).
enum class CoefficientMode { PartialFactors, PartialAndCombination };

/// Range of every coefficient cell: tooltip "Debe estar entre 0.000 y 10.000 ." (measured).
constexpr double kMinCoefficient = 0.0;
constexpr double kMaxCoefficient = 10.0;
/// "Sismo" row of a seismic situation [V, research 14.1]: the orthogonal fraction (cell "Acompañamiento") is written as a fraction 0..1
/// (0.3 accepted and shown 0.300; 30 and 2 refused). The range of the favourable / unfavourable cells (-1.000 / 1.000) is not measured [N].
constexpr double kMinOrthogonalFraction = 0.0;
constexpr double kMaxOrthogonalFraction = 1.0;
constexpr double kMaxSeismicCoefficient = 10.0;  // [N] |value| limit of the Sismo favourable / unfavourable cells

/// One row of the coefficient grid. `principal` / `accompanying` are the combination factors; they are only used by the variable
/// actions of a set in `PartialAndCombination` mode ("-" cells of the original are disabled).
struct SituationRow {
    double favourable = 1.0;
    double unfavourable = 1.0;
    double principal = 1.0;
    double accompanying = 1.0;
    bool operator==(const SituationRow&) const = default;
};

/// Kinds of row, in the order of the original grid: Carga permanente, Pretensado, Retracción, Sobrecarga (Uso n) one per use category,
/// Viento, Nieve, Temperatura, Empujes del terreno.
enum class SituationAction { Permanent, Prestress, Shrinkage, Imposed, Wind, Snow, Temperature, EarthThrust };

/// Whether the cells principal / accompanying of the action are enabled (variable actions only; the permanent ones and the earth thrusts
/// show "-").
bool hasCombinationFactors(SituationAction action);

struct UserSituation {
    std::string name;
    SituationType type = SituationType::PersistentOrTransient;
    SituationRow permanent, prestress, shrinkage, wind, snow, temperature, earthThrust;
    std::vector<SituationRow> imposed;  ///< one per use category of the set
    /// "Sismo (E)" row, used only when `type == Seismic` (not part of rows()): favourable -1.000, unfavourable 1.000, principal 1.000 (fixed, no
    /// cell) and `accompanying` = fraction of the seismic effects of the orthogonal direction, default 0.000 [V, research 14.1].
    SituationRow seismic{-1.0, 1.0, 1.0, 0.0};

    /// Defaults of the original: everything 1.000 except the favourable factor of the variable actions (0.000).
    static UserSituation withDefaults(std::size_t categories);
    /// Rows in grid order (imposed expanded); the pointer of each is valid while the situation lives.
    std::vector<SituationRow*> rows();
    std::vector<const SituationRow*> rows() const;
    bool operator==(const UserSituation&) const = default;
};

/// "Crear - [Situaciones]": one reference plus its list of situations and the use categories they are defined with.
struct UserSituationSet {
    std::string reference;
    CoefficientMode mode = CoefficientMode::PartialFactors;
    std::vector<std::string> categories;  ///< "Categoría de uso" cells ("Uso 1" ... in the grid)
    std::vector<UserSituation> situations;
    /// True for the sets of the E.L.S. (cracking) library; the E.L.U. and E.L.S. libraries are separate, so an E.L.U. and an E.L.S. set
    /// may share a reference [V, research 14.3/14.4]. Such sets use `SituationType::Service` and `CoefficientMode::PartialFactors`.
    bool service = false;
    bool operator==(const UserSituationSet&) const = default;

    /// Grows / shrinks every situation to the categories of the set (new rows get the defaults).
    void syncCategories();
    void addCategory(const std::string& name);
    void removeCategory(std::size_t index);
    /// Replaces the category list: entry i of `names` takes the row of the old category `origin[i]` (-1 = new, defaults; a repeated
    /// origin = a copy). Used by the "Categorías de uso" dialog, whose rows can be added, copied and deleted.
    void remapCategories(const std::vector<std::string>& names, const std::vector<int>& origin);
};

/// Set built from the situations of a design code (arrow "Crear a partir de las situaciones de la norma", #156485): persistent or
/// transient, seismic and accidental, with the gamma / psi tables of combinations.cpp. `code` 0 = CTE DB-SE, 1 = EN 1990 case B.
UserSituationSet situationsFromNorm(bool eurocode, bool aboveThousandMetres, const std::string& reference);

/// Why a situation (or a set) cannot be accepted. Order = order of the checks of the original (measured, 12.4): favourable > unfavourable
/// first, then principal < accompanying.
enum class SituationError {
    None,
    OutOfRange,                ///< some coefficient outside 0.000 .. 10.000 (the cell itself refuses it)
    FavourableAboveUnfavourable,  ///< #153667 "No puede introducir un coeficiente favorable mayor que el desfavorable."
    PrincipalBelowAccompanying,   ///< #153668 "No puede introducir un coeficiente principal menor que el de acompañamiento."
    EmptyName,                    ///< #153581 [check not measured]
    DuplicateName,                ///< #153580 [check not measured]
    EmptyReference,               ///< a set without reference cannot be referred to by the limit states [N]
    DuplicateReference,
    NoSituations,                 ///< #94354 "La lista no puede quedar vacía." (empty list of "Crear - [Situaciones]")
    NoPersistentSituation,        ///< #153643 "Se debe definir, al menos, una situación persistente o transitoria."
    NoSeismicSituation,           ///< #153641 "Se debe definir, al menos, una situación con sismo." (work with seismic cases)
    NoSetSelected,                ///< #156481 Error "Debe introducir, al menos, una situación de proyecto." (accepting a limit state with the user radio and an empty combo)
};

/// Validates one situation. `mode` decides whether the combination factors take part in the check.
SituationError validateSituation(const UserSituation& situation, CoefficientMode mode);
/// Context of the checks of a whole set.
struct SetChecks {
    bool projectHasSeismic = false;  ///< the work has seismic cases: a seismic situation is then required
    bool structure = true;           ///< false: skip the persistent / seismic requirements (loading a stored work)
};
/// Validates the set in the order of the original [V, research 14.1/14.4]: empty reference, (optional) repeated reference among
/// `otherReferences`, empty list, no persistent situation, no seismic situation (work with seismic cases), then every situation
/// (name, repeated name, coefficients). The persistent / seismic checks do not apply to an E.L.S. set (`service`). The original does NOT
/// detect repeated references when creating the set (only when accepting the limit state, see LimitStatesSettings::repeatedReference),
/// so callers should leave `otherReferences` empty there.
SituationError validateSet(const UserSituationSet& set, const std::vector<std::string>& otherReferences = {},
                           const SetChecks& checks = {});
/// Message of the original (es-ES) for an error; empty for None / OutOfRange.
const char* situationErrorMessage(SituationError error);
/// True when the original shows the message as an Error (red icon) instead of an Aviso: empty reference (#94341) and #156481.
bool situationErrorIsError(SituationError error);
/// Error #94676 text, accepting a limit state with a repeated set reference.
std::string repeatedReferenceMessage(const std::string& reference);

const char* situationTypeCaption(SituationType type);  ///< es-ES text of the type combo

}  // namespace cc::core
