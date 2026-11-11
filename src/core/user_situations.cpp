#include "core/user_situations.h"

#include <algorithm>
#include <cmath>

#include "core/combinations.h"

namespace cc::core {

bool hasCombinationFactors(SituationAction action) {
    switch (action) {
    case SituationAction::Imposed:
    case SituationAction::Wind:
    case SituationAction::Snow:
    case SituationAction::Temperature: return true;
    default: return false;
    }
}

namespace {
SituationRow variableDefaults() {
    SituationRow r;
    r.favourable = 0.0;  // measured: the favourable factor of the variable actions starts at 0.000
    return r;
}
}  // namespace

UserSituation UserSituation::withDefaults(std::size_t categories) {
    UserSituation s;
    s.wind = s.snow = s.temperature = variableDefaults();
    s.imposed.assign(categories, variableDefaults());
    return s;
}

std::vector<SituationRow*> UserSituation::rows() {
    std::vector<SituationRow*> out{&permanent, &prestress, &shrinkage};
    for (SituationRow& r : imposed) out.push_back(&r);
    out.insert(out.end(), {&wind, &snow, &temperature, &earthThrust});
    return out;
}

std::vector<const SituationRow*> UserSituation::rows() const {
    std::vector<const SituationRow*> out{&permanent, &prestress, &shrinkage};
    for (const SituationRow& r : imposed) out.push_back(&r);
    out.insert(out.end(), {&wind, &snow, &temperature, &earthThrust});
    return out;
}

void UserSituationSet::syncCategories() {
    for (UserSituation& s : situations) s.imposed.resize(categories.size(), variableDefaults());
}

void UserSituationSet::addCategory(const std::string& name) {
    categories.push_back(name);
    syncCategories();
}

void UserSituationSet::removeCategory(std::size_t index) {
    if (index >= categories.size()) return;
    categories.erase(categories.begin() + static_cast<std::ptrdiff_t>(index));
    for (UserSituation& s : situations)
        if (index < s.imposed.size()) s.imposed.erase(s.imposed.begin() + static_cast<std::ptrdiff_t>(index));
}

void UserSituationSet::remapCategories(const std::vector<std::string>& names, const std::vector<int>& origin) {
    for (UserSituation& s : situations) {
        std::vector<SituationRow> rows;
        for (std::size_t i = 0; i < names.size(); ++i) {
            const int from = i < origin.size() ? origin[i] : -1;
            rows.push_back(from >= 0 && static_cast<std::size_t>(from) < s.imposed.size() ? s.imposed[static_cast<std::size_t>(from)]
                                                                                         : variableDefaults());
        }
        s.imposed = std::move(rows);
    }
    categories = names;
}

SituationError validateSituation(const UserSituation& situation, CoefficientMode mode) {
    const auto rows = situation.rows();
    const auto inRange = [](double v) { return v >= kMinCoefficient && v <= kMaxCoefficient; };
    for (const SituationRow* r : rows)
        if (!inRange(r->favourable) || !inRange(r->unfavourable) || !inRange(r->principal) || !inRange(r->accompanying))
            return SituationError::OutOfRange;
    const bool seismic = situation.type == SituationType::Seismic;
    if (seismic) {  // "Sismo" row: fraction 0..1 in "Acompañamiento"
        const SituationRow& e = situation.seismic;
        const auto inSeismicRange = [](double v) { return std::fabs(v) <= kMaxSeismicCoefficient; };
        if (!inSeismicRange(e.favourable) || !inSeismicRange(e.unfavourable) || e.accompanying < kMinOrthogonalFraction ||
            e.accompanying > kMaxOrthogonalFraction)
            return SituationError::OutOfRange;
    }
    // Checks in the order of the original (section 12.4): first the favourable / unfavourable pairs, then principal / accompanying.
    for (const SituationRow* r : rows)
        if (r->favourable > r->unfavourable) return SituationError::FavourableAboveUnfavourable;
    if (seismic && situation.seismic.favourable > situation.seismic.unfavourable) return SituationError::FavourableAboveUnfavourable;
    if (mode == CoefficientMode::PartialAndCombination) {
        const SituationRow* variable[] = {&situation.wind, &situation.snow, &situation.temperature};
        for (const SituationRow* r : variable)
            if (r->principal < r->accompanying) return SituationError::PrincipalBelowAccompanying;
        for (const SituationRow& r : situation.imposed)
            if (r.principal < r.accompanying) return SituationError::PrincipalBelowAccompanying;
    }
    return SituationError::None;
}

SituationError validateSet(const UserSituationSet& set, const std::vector<std::string>& otherReferences, const SetChecks& checks) {
    if (set.reference.empty()) return SituationError::EmptyReference;
    if (std::find(otherReferences.begin(), otherReferences.end(), set.reference) != otherReferences.end())
        return SituationError::DuplicateReference;
    if (set.situations.empty()) return SituationError::NoSituations;
    if (checks.structure && !set.service) {
        const auto has = [&](SituationType t) {
            return std::any_of(set.situations.begin(), set.situations.end(), [t](const UserSituation& s) { return s.type == t; });
        };
        if (!has(SituationType::PersistentOrTransient)) return SituationError::NoPersistentSituation;
        if (checks.projectHasSeismic && !has(SituationType::Seismic)) return SituationError::NoSeismicSituation;
    }
    std::vector<std::string> names;
    for (const UserSituation& s : set.situations) {
        if (s.name.empty()) return SituationError::EmptyName;
        if (std::find(names.begin(), names.end(), s.name) != names.end()) return SituationError::DuplicateName;
        names.push_back(s.name);
        if (s.imposed.size() != set.categories.size()) return SituationError::OutOfRange;
        if (const SituationError e = validateSituation(s, set.mode); e != SituationError::None) return e;
    }
    return SituationError::None;
}

const char* situationErrorMessage(SituationError error) {
    switch (error) {
    case SituationError::FavourableAboveUnfavourable: return "No puede introducir un coeficiente favorable mayor que el desfavorable.";
    case SituationError::PrincipalBelowAccompanying: return "No puede introducir un coeficiente principal menor que el de acompañamiento.";
    case SituationError::EmptyName: return "'Nombre de la situación' no puede quedar en blanco.";   // #250426 with #153660 [V, research 14.4]
    case SituationError::DuplicateName: return "La referencia está repetida.";                       // #64970 [V, research 14.4]
    case SituationError::EmptyReference: return "Debe introducir una referencia";                    // #94341, Error [V]
    case SituationError::NoSituations: return "La lista no puede quedar vacía.";                     // #94354 [V]
    case SituationError::NoPersistentSituation: return "Se debe definir, al menos, una situación persistente o transitoria.";
    case SituationError::NoSeismicSituation: return "Se debe definir, al menos, una situación con sismo.";
    case SituationError::NoSetSelected: return "Debe introducir, al menos, una situación de proyecto.";
    default: return "";
    }
}

bool situationErrorIsError(SituationError error) {
    return error == SituationError::EmptyReference || error == SituationError::NoSetSelected;
}

std::string repeatedReferenceMessage(const std::string& reference) {
    return "La referencia '" + reference + "' aparece repetida en la caja de selección.\nDebe renombrarla o borrarla.";
}

const char* situationTypeCaption(SituationType type) {
    switch (type) {
    case SituationType::PersistentOrTransient: return "Situación persistente o transitoria";
    case SituationType::Accidental: return "Situación accidental";
    case SituationType::Seismic: return "Situación sísmica";
    case SituationType::PrestressInVacuum: return "Pretensado en vacío";
    case SituationType::Service: return "";  // E.L.S. sets have no type combo
    }
    return "";
}

}  // namespace cc::core

namespace cc::core {

UserSituationSet situationsFromNorm(bool eurocode, bool above, const std::string& reference) {
    const NormCode code = eurocode ? NormCode::En1990CaseB : NormCode::CteDbSe;
    UserSituationSet set;
    set.reference = reference;
    set.mode = CoefficientMode::PartialAndCombination;
    const std::vector<std::string> letters = eurocode ? std::vector<std::string>{"A", "B", "C", "D", "E", "F", "G", "H"}
                                                      : std::vector<std::string>{"A", "B", "C", "D", "E", "G1", "G2"};
    set.categories = letters;
    const SnowZone zone = above ? SnowZone::Above1000 : SnowZone::AtOrBelow1000;
    struct Kind {
        DesignSituation situation;
        SituationType type;
        const char* name;
    };
    const Kind kinds[] = {{DesignSituation::Persistent, SituationType::PersistentOrTransient, "Persistente o transitoria"},
                          {DesignSituation::Seismic, SituationType::Seismic, "Sísmica"},
                          {DesignSituation::Accidental, SituationType::Accidental, "Accidental"}};
    for (const Kind& kind : kinds) {
        UserSituation s = UserSituation::withDefaults(letters.size());
        s.name = kind.name;
        s.type = kind.type;
        const PartialFactors g = partialFactors(code, kind.situation);
        s.permanent.favourable = g.permFav;
        s.permanent.unfavourable = g.permUnfav;
        s.shrinkage.favourable = g.nonConstFav;
        s.shrinkage.unfavourable = g.nonConstUnfav;
        s.earthThrust.favourable = g.earthFav;
        s.earthThrust.unfavourable = g.earthUnfav;
        const auto setVariable = [&](SituationRow& row, const Psi& psi) {
            row.favourable = g.varFav;
            row.unfavourable = g.varUnfav;
            switch (kind.situation) {
            case DesignSituation::Persistent: row.principal = 1.0; row.accompanying = psi.psi0; break;
            case DesignSituation::Seismic: row.principal = row.accompanying = psi.psi2; break;
            default:  // accidental: CTE psi1 principal, EN 1990 psi2 principal
                row.principal = eurocode ? psi.psi2 : psi.psi1;
                row.accompanying = psi.psi2;
            }
        };
        for (std::size_t i = 0; i < letters.size(); ++i)
            if (const auto psi = psiLiveLoad(code, letters[i])) setVariable(s.imposed[i], *psi);
        setVariable(s.wind, psiWind(code));
        setVariable(s.snow, psiSnow(code, zone));
        setVariable(s.temperature, psiTemperature());
        set.situations.push_back(std::move(s));
    }
    return set;
}

}  // namespace cc::core
