#include "core/combinations.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <map>
#include <set>

#include "core/project.h"

namespace cc::core {

namespace {

bool isEn(NormCode c) { return c == NormCode::En1990CaseB || c == NormCode::En1990CaseC; }

std::vector<long long> keyOf(const std::vector<double>& v) {
    std::vector<long long> k(v.size());
    for (size_t i = 0; i < v.size(); ++i) k[i] = std::llround(v[i] * 1e6);
    return k;
}

/// Ordering of the original (T-P7-06, research section 12.2): vectors of coefficient-set indices compared from the LAST
/// hypothesis (most significant: N > V > Q > PP) to the first, ascending. -1 (does not act) < 0 < 1 ...
bool indexLess(const std::vector<int>& a, const std::vector<int>& b) {
    for (size_t i = a.size(); i-- > 0;)
        if (a[i] != b[i]) return a[i] < b[i];
    return false;
}

enum class PermKind { Plain, Earth, NonConstant };

struct PermEntry {
    int idx;
    PermKind kind;
};

/// One alternative of a variable slot: a case with its sign and psi. `signIndex` is the coefficient-set index of an action with
/// sign (seismic: 0 = "-", 1 = "+").
struct VarOption {
    int idx;
    double sign;
    Psi psi;
    bool nonConcomitant;
    LoadNature nature = LoadNature::LiveLoad;
    int signIndex = 0;
};
struct VarSlot {
    std::vector<VarOption> options;
};

enum class Role { PrincipalRequired, PrincipalOptional, NoPrincipal };
enum class ActionGroup { None, Seismic, Accidental };

struct SituationSpec {
    DesignSituation situation;
    Role role;
    bool g1Mode = false;
    ActionGroup group = ActionGroup::None;
    bool skipWindAndSnow = false;  ///< seismic displacements: wind and snow do not appear (measured in the reference work)
};

/// Combination coefficients plus the coefficient-set index of every case (-1 = does not act).
struct Item {
    std::vector<double> c;
    std::vector<int> k;
};

double principalFactor(NormCode code, DesignSituation s, const Psi& psi) {
    switch (s) {
    case DesignSituation::Persistent: return 1.0;
    case DesignSituation::ServiceCharacteristic: return 1.0;
    case DesignSituation::Accidental: return isEn(code) ? psi.psi2 : psi.psi1;  // EC: psi2, CTE: psi1 [V, research 12.3]
    case DesignSituation::ServiceFrequent: return psi.psi1;
    default: return psi.psi2;
    }
}
double accompanyingFactor(DesignSituation s, const Psi& psi) {
    switch (s) {
    case DesignSituation::Persistent: return psi.psi0;
    case DesignSituation::ServiceCharacteristic: return 1.0;  // "Acciones características" (displacements): no psi [V]
    default: return psi.psi2;
    }
}

struct Generator {
    const LoadCaseSet& set;
    const CombinationOptions& opt;
    std::vector<PermEntry> perms;
    std::vector<VarSlot> slots;      // live load, temperature, snow, wind (one slot for all the wind cases)
    std::vector<VarOption> seismic;  // alternatives with sign
    std::vector<VarOption> accidental;
    std::string error;

    bool build() {
        bool windSlotOpen = false;
        size_t windSlot = 0;
        for (size_t i = 0; i < set.cases.size(); ++i) {
            const LoadCase& c = set.cases[i];
            if (!c.acts) continue;
            const int idx = static_cast<int>(i);
            switch (c.nature) {
            case LoadNature::SelfWeight:
            case LoadNature::DeadLoad: perms.push_back({idx, PermKind::Plain}); break;
            case LoadNature::EarthPressure: perms.push_back({idx, PermKind::Earth}); break;
            case LoadNature::Shrinkage: perms.push_back({idx, PermKind::NonConstant}); break;
            case LoadNature::LiveLoad: {
                std::string cat = opt.defaultUseCategory;
                if (const auto it = opt.caseUseCategory.find(c.id); it != opt.caseUseCategory.end()) cat = it->second;
                const auto psi = psiLiveLoad(opt.code, cat, opt.nationalAnnex);
                if (!psi) {
                    error = "Unknown use category '" + cat + "' for the selected code.";
                    return false;
                }
                slots.push_back({{{idx, 1.0, *psi, isNonConcomitantCategory(cat) && !isEn(opt.code), LoadNature::LiveLoad}}});
                break;
            }
            case LoadNature::Temperature:
                slots.push_back({{{idx, 1.0, psiTemperature(), false, LoadNature::Temperature}}});
                break;
            case LoadNature::Snow:
                slots.push_back({{{idx, 1.0, psiSnow(opt.code, opt.snow), false, LoadNature::Snow}}});
                break;
            case LoadNature::Wind: {  // one group of alternatives, sign + only [V, research 12.2]
                if (!windSlotOpen) {
                    slots.push_back({});
                    windSlot = slots.size() - 1;
                    windSlotOpen = true;
                }
                slots[windSlot].options.push_back({idx, 1.0, psiWind(opt.code, opt.nationalAnnex), false, LoadNature::Wind});
                break;
            }
            case LoadNature::Seismic:
                seismic.push_back({idx, -1.0, {}, false, LoadNature::Seismic, 0});
                seismic.push_back({idx, 1.0, {}, false, LoadNature::Seismic, 1});
                break;
            case LoadNature::Accidental:
                if (opt.code == NormCode::Ehe08) {  // EHE-08: +/- accidental action [V]
                    accidental.push_back({idx, -1.0, {}, false, LoadNature::Accidental, 0});
                    accidental.push_back({idx, 1.0, {}, false, LoadNature::Accidental, 1});
                } else {
                    accidental.push_back({idx, 1.0, {}, false, LoadNature::Accidental, 0});
                }
                break;
            }
        }
        return true;
    }

    std::map<std::vector<long long>, size_t> seen;
    std::vector<Item> out;

    /// Deduplicates by coefficient vector keeping the entry with the smallest index vector (what the original's sort + filter does).
    void emit(const Item& it) {
        if (error.size()) return;
        const auto key = keyOf(it.c);
        if (const auto f = seen.find(key); f != seen.end()) {
            if (indexLess(it.k, out[f->second].k)) out[f->second] = it;
            return;
        }
        seen.emplace(key, out.size());
        out.push_back(it);
        if (static_cast<long long>(seen.size()) > opt.maxCombinations)
            error = "Too many combinations (limit " + std::to_string(opt.maxCombinations) + ").";
    }

    void principalLoop(const SituationSpec& spec, const PartialFactors& g, const Item& base,
                       const std::vector<const VarOption*>& chosen) {
        auto coefOf = [&](const VarOption& o, bool principal) {
            const double factor =
                principal ? principalFactor(opt.code, spec.situation, o.psi) : accompanyingFactor(spec.situation, o.psi);
            const double gamma = g.varUnfav;
            if (opt.longTermFraction > 0.0)
                return o.sign * effectiveVariableCoefficient(gamma, o.psi.psi2, gamma, factor, opt.longTermFraction);
            return o.sign * gamma * factor;
        };
        auto build = [&](int principal) {
            Item v = base;
            for (size_t i = 0; i < chosen.size(); ++i) {
                const size_t at = static_cast<size_t>(chosen[i]->idx);
                const bool isPrincipal = static_cast<int>(i) == principal;
                v.c[at] = coefOf(*chosen[i], isPrincipal);
                v.k[at] = (spec.role == Role::NoPrincipal || isPrincipal) ? 0 : 1;  // principal < accompanying
            }
            emit(v);
        };
        if (chosen.empty()) {
            emit(base);
            return;
        }
        if (spec.role == Role::NoPrincipal) {
            build(-1);
            return;
        }
        if (spec.role == Role::PrincipalOptional) build(-1);
        for (size_t i = 0; i < chosen.size(); ++i) build(static_cast<int>(i));
    }

    void slotLoop(const SituationSpec& spec, const PartialFactors& g, const Item& base, size_t slot,
                  std::vector<const VarOption*>& chosen) {
        if (error.size()) return;
        if (slot == slots.size()) {
            principalLoop(spec, g, base, chosen);
            return;
        }
        slotLoop(spec, g, base, slot + 1, chosen);  // slot not acting
        for (const VarOption& o : slots[slot].options) {
            if (spec.g1Mode != o.nonConcomitant) continue;  // G1 acts only alone, in its own situation
            if (spec.skipWindAndSnow && (o.nature == LoadNature::Wind || o.nature == LoadNature::Snow)) continue;
            chosen.push_back(&o);
            slotLoop(spec, g, base, slot + 1, chosen);
            chosen.pop_back();
        }
    }

    void run(const SituationSpec& spec) {
        const PartialFactors g = partialFactors(opt.code, spec.situation);
        const bool seismicSit = spec.group == ActionGroup::Seismic;
        const bool accSit = spec.group == ActionGroup::Accidental;
        if (seismicSit && seismic.empty()) return;
        if (accSit && accidental.empty()) return;
        if (spec.g1Mode && std::none_of(slots.begin(), slots.end(), [](const VarSlot& s) {
                return std::any_of(s.options.begin(), s.options.end(), [](const VarOption& o) { return o.nonConcomitant; });
            }))
            return;

        auto gammas = [&](PermKind k, double& fav, double& unfav) {
            switch (k) {
            case PermKind::Plain: fav = g.permFav; unfav = g.permUnfav; break;
            case PermKind::Earth: fav = g.earthFav; unfav = g.earthUnfav; break;
            case PermKind::NonConstant: fav = g.nonConstFav; unfav = g.nonConstUnfav; break;
            }
        };
        std::vector<size_t> varying;
        for (size_t i = 0; i < perms.size(); ++i) {
            double f, u;
            gammas(perms[i].kind, f, u);
            if (std::fabs(f - u) > 1e-12) varying.push_back(i);
        }
        if (varying.size() > 16) {
            error = "Too many permanent load cases to expand favourable / unfavourable.";
            return;
        }
        const size_t n = set.cases.size();
        for (unsigned long long mask = 0; mask < (1ULL << varying.size()); ++mask) {
            Item base{std::vector<double>(n, 0.0), std::vector<int>(n, -1)};
            for (size_t i = 0; i < perms.size(); ++i) {
                double f, u;
                gammas(perms[i].kind, f, u);
                bool unfavourable = true;
                const auto it = std::find(varying.begin(), varying.end(), i);
                if (it != varying.end()) unfavourable = (mask >> (it - varying.begin())) & 1ULL;
                const size_t at = static_cast<size_t>(perms[i].idx);
                base.c[at] = unfavourable ? u : f;
                base.k[at] = (it != varying.end() && unfavourable) ? 1 : 0;  // favourable set first
            }
            if (seismicSit || accSit) {
                for (const VarOption& a : seismicSit ? seismic : accidental) {
                    Item b = base;
                    b.c[static_cast<size_t>(a.idx)] = a.sign * 1.0;  // gamma_A = gamma_E = 1.0 [V]
                    b.k[static_cast<size_t>(a.idx)] = a.signIndex;
                    std::vector<const VarOption*> chosen;
                    slotLoop(spec, g, b, 0, chosen);
                }
            } else {
                std::vector<const VarOption*> chosen;
                slotLoop(spec, g, base, 0, chosen);
            }
        }
    }
};

/// Rank of a nature in the name of a combination: PP, Q, V, N, S, A measured; the others are placed by analogy [N].
int nameRank(LoadNature n) {
    switch (n) {
    case LoadNature::SelfWeight: return 0;
    case LoadNature::DeadLoad: return 1;
    case LoadNature::LiveLoad: return 2;
    case LoadNature::Temperature: return 3;
    case LoadNature::Shrinkage: return 4;
    case LoadNature::Wind: return 5;
    case LoadNature::Snow: return 6;
    case LoadNature::EarthPressure: return 7;
    case LoadNature::Seismic: return 8;
    case LoadNature::Accidental: return 9;
    }
    return 10;
}

std::string formatCoefficient(double v) {
    char buf[32];
    std::snprintf(buf, sizeof buf, "%.6g", v);
    return buf;
}

}  // namespace

std::string combinationFormulaName(const LoadCaseSet& set, const std::vector<double>& coefficients) {
    std::vector<size_t> order;
    for (size_t i = 0; i < set.cases.size() && i < coefficients.size(); ++i)
        if (std::fabs(coefficients[i]) > 1e-12) order.push_back(i);
    std::stable_sort(order.begin(), order.end(),
                     [&](size_t a, size_t b) { return nameRank(set.cases[a].nature) < nameRank(set.cases[b].nature); });
    std::string name;
    for (size_t i : order) {
        const double c = coefficients[i];
        const LoadCase& lc = set.cases[i];
        std::string ref = lc.nature == LoadNature::SelfWeight ? "PP" : lc.reference;
        ref.erase(std::remove(ref.begin(), ref.end(), ' '), ref.end());
        if (c < 0.0) name += '-';
        else if (!name.empty()) name += '+';
        const double m = std::fabs(c);
        if (std::fabs(m - 1.0) > 1e-9) name += formatCoefficient(m) + "\xC2\xB7";  // U+00B7 middle dot
        name += ref;
    }
    return name;
}

PartialFactors partialFactors(NormCode code, DesignSituation s) {
    PartialFactors p;  // 1 / 1, variable 0 / 1: accidental, seismic and service situations
    if (s != DesignSituation::Persistent) return p;
    switch (code) {
    case NormCode::CteDbSe:  // CTE DB-SE 4.2.2 [V]
        p.permFav = 0.8; p.permUnfav = 1.35; p.varUnfav = 1.5;
        p.earthFav = 0.7; p.earthUnfav = 1.35;
        p.nonConstFav = 1.0; p.nonConstUnfav = 1.5;  // shrinkage "Retracción" 1.000 / 1.500 [V, measured in the original]
        break;
    case NormCode::En1990CaseB:  // EN 1990 Table A1.2(B) [V]
        p.permFav = 1.0; p.permUnfav = 1.35; p.varUnfav = 1.5;
        p.earthFav = 1.0; p.earthUnfav = 1.35;  // measured in the original
        break;
    case NormCode::En1990CaseC:  // Table A1.2(C) [V]
        p.permFav = 1.0; p.permUnfav = 1.0; p.varUnfav = 1.3;
        break;
    case NormCode::Ehe08:  // EHE-08 art. 12.1 [V]
        p.permFav = 1.0; p.permUnfav = 1.35; p.varUnfav = 1.5;
        p.nonConstFav = 1.0; p.nonConstUnfav = 1.5;
        p.earthFav = 1.0; p.earthUnfav = 1.5;  // [N] earth pressure as non-constant permanent
        break;
    }
    return p;
}

std::optional<Psi> psiLiveLoad(NormCode code, const std::string& cat, int annex) {
    if (!isEn(code)) {  // CTE DB-SE (EHE-08 uses the CTE values [N]); [V] ctesebas
        if (cat == "A" || cat == "B") return Psi{0.7, 0.5, 0.3};
        if (cat == "C" || cat == "D" || cat == "E") return Psi{0.7, 0.7, 0.6};
        if (cat == "G1" || cat == "G2") return Psi{0.0, 0.0, 0.0};
        return std::nullopt;
    }
    std::optional<Psi> p;  // EN 1990 Table A1.1 [V]
    if (cat == "A" || cat == "B") p = Psi{0.7, 0.5, 0.3};
    else if (cat == "C" || cat == "D" || cat == "F") p = Psi{0.7, 0.7, 0.6};
    else if (cat == "E") p = Psi{1.0, 0.9, 0.8};
    else if (cat == "G") p = Psi{0.7, 0.5, 0.3};
    else if (cat == "H") p = Psi{0.0, 0.0, 0.0};
    if (p && (annex == 8 || annex == 9)) p->psi0 = cat == "E" ? 1.0 : 0.7;  // Malaysia / Singapore variant [V]
    return p;
}

Psi psiWind(NormCode code, int annex) {
    if (!isEn(code)) return {0.6, 0.5, 0.0};
    return {(annex == 8 || annex == 9) ? 0.5 : 0.6, 0.2, 0.0};
}

Psi psiTemperature() { return {0.6, 0.5, 0.0}; }

Psi psiSnow(NormCode, SnowZone zone) {
    if (zone == SnowZone::AtOrBelow1000) return {0.5, 0.2, 0.0};
    return {0.7, 0.5, 0.2};  // > 1000 m, and the EN 1990 Nordic countries (same values)
}

bool isNonConcomitantCategory(const std::string& useCategory) { return useCategory == "G1"; }

double effectiveVariableCoefficient(double gl, double pl, double gc, double pc, double f) {
    return gl * pl * f + gc * pc * (1.0 - f);
}

CombinationResult generateCombinations(const LoadCaseSet& set, const CombinationOptions& options) {
    CombinationResult result;
    Generator gen{set, options, {}, {}, {}, {}, {}};
    if (!gen.build()) {
        result.error = gen.error;
        return result;
    }

    struct Block {
        SituationSpec spec;
        CombinationLimitState ls;
    };
    std::vector<Block> blocks;
    if (options.withUltimate) {
        blocks.push_back({{DesignSituation::Persistent, Role::PrincipalRequired, false}, CombinationLimitState::Ultimate});
        blocks.push_back({{DesignSituation::Persistent, Role::PrincipalRequired, true}, CombinationLimitState::Ultimate});
        if (options.withSeismic)
            blocks.push_back({{DesignSituation::Seismic, Role::NoPrincipal, false, ActionGroup::Seismic}, CombinationLimitState::Ultimate});
        if (options.withAccidental)
            blocks.push_back({{DesignSituation::Accidental, Role::PrincipalOptional, false, ActionGroup::Accidental},
                              CombinationLimitState::Ultimate});
    }
    if (options.withService) {
        // "Desplazamientos / Acciones características": every variable at 1.0, no principal, every subset (+ seismic +/- variants).
        blocks.push_back({{DesignSituation::ServiceCharacteristic, Role::NoPrincipal, false}, CombinationLimitState::Service});
        if (options.withSeismic)
            blocks.push_back({{DesignSituation::ServiceCharacteristic, Role::NoPrincipal, false, ActionGroup::Seismic, true},
                              CombinationLimitState::Service});
        blocks.push_back({{DesignSituation::ServiceFrequent, Role::PrincipalRequired, false}, CombinationLimitState::Service});
        blocks.push_back({{DesignSituation::ServiceQuasiPermanent, Role::NoPrincipal, false}, CombinationLimitState::Service});
    }

    struct Entry {
        Combination comb;
        std::vector<int> k;
    };
    std::vector<Entry> entries;
    for (const Block& b : blocks) {
        gen.out.clear();
        gen.seen.clear();
        gen.run(b.spec);
        if (!gen.error.empty()) {
            result.error = gen.error;
            return result;
        }
        for (Item& it : gen.out) {
            Entry e;
            e.comb.situation = b.spec.situation;
            e.comb.limitState = b.ls;
            e.comb.coefficients = std::move(it.c);
            e.k = std::move(it.k);
            entries.push_back(std::move(e));
        }
    }
    // Per situation, by coefficient-set index vector (the "(G1)" block merges into the persistent situation); then deduplicate by
    // coefficient vector keeping the first.
    std::stable_sort(entries.begin(), entries.end(), [](const Entry& a, const Entry& b) {
        if (a.comb.situation != b.comb.situation) return static_cast<int>(a.comb.situation) < static_cast<int>(b.comb.situation);
        return indexLess(a.k, b.k);
    });
    std::set<std::pair<int, std::vector<long long>>> seenPerSituation;
    for (Entry& e : entries) {
        if (!seenPerSituation.insert({static_cast<int>(e.comb.situation), keyOf(e.comb.coefficients)}).second) continue;
        e.comb.indices = std::move(e.k);
        e.comb.name = combinationFormulaName(set, e.comb.coefficients);
        result.combinations.push_back(std::move(e.comb));
    }
    return result;
}

std::optional<NormCode> combinationCodeFromStandard(const std::string& name) {
    if (name.find("Eurocódigo") != std::string::npos) return NormCode::En1990CaseB;
    if (name.find("EHE") != std::string::npos || name.find("Código Estructural") != std::string::npos) return NormCode::Ehe08;
    if (name.find("CTE") != std::string::npos) return NormCode::CteDbSe;
    return std::nullopt;
}

SnowZone snowZoneFromText(const std::string& text) {
    if (text.find("Finlandia") != std::string::npos) return SnowZone::Nordic;  // EC list: "Finlandia, Islandia, Noruega, Suecia"
    if (text.find("superior a 1000") != std::string::npos || text.find("H > 1000") != std::string::npos) return SnowZone::Above1000;
    return SnowZone::AtOrBelow1000;
}

CombinationOptions combinationOptionsFor(const Project& project, StandardFamily family) {
    CombinationOptions o;
    std::string snow = LimitStatesSettings::defaultSnow();
    switch (family) {
    case StandardFamily::Concrete:
    case StandardFamily::Aluminium: snow = project.limitStates.groundSnow; break;
    case StandardFamily::ColdFormedSteel: snow = project.limitStates.coldFormedSnow; break;
    case StandardFamily::Timber: snow = project.limitStates.timberSnow; break;
    case StandardFamily::HotRolledSteel: snow = project.limitStates.hotRolledSnow; break;
    }
    o.snow = snowZoneFromText(snow);
    o.code = combinationCodeFromStandard(project.standards.get(family)).value_or(NormCode::CteDbSe);
    if (family == StandardFamily::HotRolledSteel)  // radio CTE / EC of "Estados límite" (section 9 of the research)
        o.code = project.limitStates.hotRolledCode == CombinationCode::EC ? NormCode::En1990CaseB : NormCode::CteDbSe;
    return o;
}

CombinationResult generateCombinations(const Project& project, StandardFamily family) {
    return generateCombinations(project.loadCases, combinationOptionsFor(project, family));
}

}  // namespace cc::core
