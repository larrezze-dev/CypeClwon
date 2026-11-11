#include "core/limit_states.h"

#include <algorithm>

namespace cc::core {

const char* limitStateCaption(LimitStateKind kind) {
    switch (kind) {
    case LimitStateKind::Concrete: return "Hormigón";
    case LimitStateKind::Foundation: return "Cimentación";
    case LimitStateKind::ColdFormed: return "Acero conformado";
    case LimitStateKind::HotRolled: return "Acero laminado";
    case LimitStateKind::Timber: return "Madera";
    case LimitStateKind::Aluminium: return "Aluminio";
    case LimitStateKind::Ground: return "Tensiones sobre el terreno";
    case LimitStateKind::Displacements: return "Desplazamientos";
    }
    return "";
}

NormClass normClassOf(const std::string& name) {
    const auto starts = [&name](const char* prefix) { return name.rfind(prefix, 0) == 0; };
    if (starts("Eurocód")) return NormClass::Eurocode;
    if (starts("CTE")) return NormClass::Cte;
    if (starts("Código Estructural") || starts("EHE")) return NormClass::StructuralCode;  // EHE-08 grouped with the structural code [N]
    return NormClass::Other;
}

namespace {

NormClass classOfKind(LimitStateKind kind, const StandardsSelection& s) {
    switch (kind) {
    case LimitStateKind::Concrete:
    case LimitStateKind::Foundation: return normClassOf(s.concrete);
    case LimitStateKind::ColdFormed: return normClassOf(s.coldFormedSteel);
    case LimitStateKind::HotRolled: return normClassOf(s.hotRolledSteel);
    case LimitStateKind::Timber: return normClassOf(s.timber);
    case LimitStateKind::Aluminium: return normClassOf(s.aluminium);
    default: return NormClass::Other;
    }
}

}  // namespace

LimitStateLayout buildLimitStateLayout(const StandardsSelection& standards) {
    LimitStateLayout layout;
    const LimitStateKind order[] = {LimitStateKind::Concrete,  LimitStateKind::Foundation, LimitStateKind::ColdFormed,
                                    LimitStateKind::HotRolled, LimitStateKind::Timber,     LimitStateKind::Aluminium};
    // Blocks by norm class, ordered by their first member; "Other" has no combos but keeps its heading lines in its own block.
    std::vector<NormClass> sequence;
    for (const LimitStateKind k : order) {
        const NormClass c = classOfKind(k, standards);
        if (std::find(sequence.begin(), sequence.end(), c) == sequence.end()) sequence.push_back(c);
    }
    const bool concreteIsEc = normClassOf(standards.concrete) == NormClass::Eurocode;
    layout.ec3Snow = normClassOf(standards.concrete) == NormClass::Eurocode && normClassOf(standards.coldFormedSteel) == NormClass::Eurocode &&
                     normClassOf(standards.hotRolledSteel) == NormClass::Eurocode && normClassOf(standards.timber) == NormClass::Eurocode &&
                     normClassOf(standards.aluminium) == NormClass::Eurocode;
    for (const NormClass c : sequence) {
        LimitStateBlock block;
        block.cls = c;
        for (const LimitStateKind k : order)
            if (classOfKind(k, standards) == c) block.kinds.push_back(k);
        block.hasSnow = c != NormClass::Other;
        block.hasRadios = c == NormClass::StructuralCode;
        block.hasGround = c == NormClass::Eurocode && concreteIsEc;
        layout.blocks.push_back(block);
    }
    layout.groundStandalone = !concreteIsEc;
    return layout;
}

const std::string& LimitStatesSettings::defaultSnow() {
    static const std::string v = "Altitud inferior o igual a 1000 m";
    return v;
}

const std::vector<std::string>& LimitStatesSettings::snowChoices() {
    static const std::vector<std::string> v = {"Altitud superior a 1000 m", defaultSnow()};  // as listed by the original
    return v;
}

const std::vector<std::string>& LimitStatesSettings::snowChoicesEc() {
    static const std::vector<std::string> v = {"Finlandia, Islandia, Noruega, Suecia", "Resto de los Estados miembro del CEN, H > 1000 m",
                                               "Resto de los Estados miembro del CEN, H <= 1000 m"};
    return v;
}

std::string LimitStatesSettings::normalisedSnow(const std::string& value, bool ec3) {
    const auto& list = ec3 ? snowChoicesEc() : snowChoices();
    if (std::find(list.begin(), list.end(), value) != list.end()) return value;
    // Equivalent entry of the other list: zone preserved.
    const bool above = value == snowChoices()[0] || value == snowChoicesEc()[0] || value == snowChoicesEc()[1];
    if (ec3) return above ? snowChoicesEc()[1] : snowChoicesEc()[2];
    return above ? snowChoices()[0] : snowChoices()[1];
}

std::string LimitStatesSettings::userSetFor(LimitStateKind kind) const {
    const auto i = static_cast<std::size_t>(kind);
    return i < userSetOfKind.size() ? userSetOfKind[i] : std::string{};
}

void LimitStatesSettings::setUserSetFor(LimitStateKind kind, const std::string& reference) {
    if (userSetOfKind.size() < static_cast<std::size_t>(kLimitStateKindCount)) userSetOfKind.resize(kLimitStateKindCount);
    userSetOfKind[static_cast<std::size_t>(kind)] = reference;
}

bool LimitStatesSettings::hasServiceRow(LimitStateKind kind) {
    return kind == LimitStateKind::Concrete || kind == LimitStateKind::Foundation;
}

std::string LimitStatesSettings::userServiceSetFor(LimitStateKind kind) const {
    const auto i = static_cast<std::size_t>(kind);
    return i < userServiceSetOfKind.size() ? userServiceSetOfKind[i] : std::string{};
}

void LimitStatesSettings::setUserServiceSetFor(LimitStateKind kind, const std::string& reference) {
    if (userServiceSetOfKind.size() < static_cast<std::size_t>(kLimitStateKindCount)) userServiceSetOfKind.resize(kLimitStateKindCount);
    userServiceSetOfKind[static_cast<std::size_t>(kind)] = reference;
}

const UserSituationSet* LimitStatesSettings::findSet(const std::string& reference, bool service) const {
    for (const UserSituationSet& s : userSets)
        if (s.service == service && s.reference == reference) return &s;
    return nullptr;
}

bool LimitStatesSettings::setInUse(const std::string& reference, bool service) const {
    const auto& used = service ? userServiceSetOfKind : userSetOfKind;
    return !reference.empty() && std::find(used.begin(), used.end(), reference) != used.end();
}

std::string LimitStatesSettings::repeatedReference(bool service) const {
    std::vector<std::string> seen;
    for (const UserSituationSet& s : userSets) {
        if (s.service != service) continue;
        if (std::find(seen.begin(), seen.end(), s.reference) != seen.end()) return s.reference;
        seen.push_back(s.reference);
    }
    return {};
}

CombinationCode LimitStatesSettings::effectiveCode(const StandardsSelection& standards) const {
    if (!codeChosen && normClassOf(standards.concrete) == NormClass::StructuralCode) return CombinationCode::Mixed;
    return hotRolledCode;
}

LimitStatesSettings::Effective LimitStatesSettings::effective(LimitStateKind kind, const StandardsSelection& standards) const {
    Effective e;
    if (kind == LimitStateKind::Displacements) {
        e.characteristic = true;
        return e;
    }
    const LimitStateLayout layout = buildLimitStateLayout(standards);
    if (kind == LimitStateKind::Ground) {
        if (layout.groundStandalone) {
            e.characteristic = true;
            return e;
        }
        e.code = "EC";
        e.snow = normalisedSnow(groundSnow, layout.ec3Snow);
        return e;
    }
    switch (classOfKind(kind, standards)) {
    case NormClass::Eurocode:
        e.code = "EC";
        e.snow = normalisedSnow(groundSnow, layout.ec3Snow);
        break;
    case NormClass::Cte:
        e.code = "CTE";
        e.snow = normalisedSnow(kind == LimitStateKind::Timber ? timberSnow : coldFormedSnow, false);
        break;
    case NormClass::StructuralCode: {
        const CombinationCode code = effectiveCode(standards);
        const bool concreteKind = kind == LimitStateKind::Concrete || kind == LimitStateKind::Foundation;
        e.code = code == CombinationCode::CTE ? "CTE" : code == CombinationCode::EC ? "EC" : (concreteKind ? "EC" : "CTE");
        e.snow = normalisedSnow(hotRolledSnow, false);
        break;
    }
    case NormClass::Other: break;
    }
    return e;
}

std::vector<LimitStatesSettings::Row> LimitStatesSettings::rows() const { return rows(StandardsSelection{}); }

std::vector<LimitStatesSettings::Row> LimitStatesSettings::rows(const StandardsSelection& standards) const {
    std::vector<Row> out;
    for (int i = 0; i < kLimitStateKindCount; ++i) {
        const auto kind = static_cast<LimitStateKind>(i);
        Row row{limitStateCaption(kind), {}};
        if (const std::string ref = userSetFor(kind); !ref.empty()) {
            row.value = ref;
        } else {
            const Effective e = effective(kind, standards);
            row.value = e.characteristic ? "Acciones características" : e.code.empty() ? std::string{} : e.code + " - " + e.snow;
        }
        out.push_back(row);
    }
    return out;
}

bool LimitStatesSettings::isValid() const {
    const auto known = [](const std::string& s) {
        const auto& a = snowChoices();
        const auto& b = snowChoicesEc();
        return std::find(a.begin(), a.end(), s) != a.end() || std::find(b.begin(), b.end(), s) != b.end();
    };
    if (!(known(groundSnow) && known(coldFormedSnow) && known(timberSnow) && known(hotRolledSnow))) return false;
    if (static_cast<int>(hotRolledCode) < 0 || static_cast<int>(hotRolledCode) > 2) return false;
    // Repeated references are legal in a stored work (only the accept of the dialog refuses them); neither are the persistent / seismic
    // requirements re-checked, so that works saved before they existed keep opening.
    SetChecks lenient;
    lenient.structure = false;
    for (const UserSituationSet& s : userSets)
        if (validateSet(s, {}, lenient) != SituationError::None) return false;
    if (userSetOfKind.size() > static_cast<std::size_t>(kLimitStateKindCount)) return false;
    if (userServiceSetOfKind.size() > static_cast<std::size_t>(kLimitStateKindCount)) return false;
    for (const std::string& ref : userSetOfKind)
        if (!ref.empty() && !findSet(ref)) return false;
    for (std::size_t i = 0; i < userServiceSetOfKind.size(); ++i)
        if (!userServiceSetOfKind[i].empty() &&
            (!hasServiceRow(static_cast<LimitStateKind>(i)) || !findSet(userServiceSetOfKind[i], true)))
            return false;
    return true;
}

}  // namespace cc::core
