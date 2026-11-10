#include "core/combinations.h"

#include <algorithm>
#include <cmath>
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

enum class PermKind { Plain, Earth, NonConstant };

struct PermEntry {
    int idx;
    PermKind kind;
};

/// One alternative of a variable slot: a case with its sign and psi.
struct VarOption {
    int idx;
    double sign;
    Psi psi;
    bool nonConcomitant;
};
struct VarSlot {
    std::vector<VarOption> options;
};

enum class Role { PrincipalRequired, PrincipalOptional, NoPrincipal };

struct SituationSpec {
    DesignSituation situation;
    Role role;
    bool g1Mode = false;
};

double principalFactor(DesignSituation s, const Psi& psi) {
    switch (s) {
    case DesignSituation::Persistent: return 1.0;
    case DesignSituation::ServiceCharacteristic: return 1.0;
    case DesignSituation::Accidental:
    case DesignSituation::ServiceFrequent: return psi.psi1;
    default: return psi.psi2;
    }
}
double accompanyingFactor(DesignSituation s, const Psi& psi) {
    switch (s) {
    case DesignSituation::Persistent:
    case DesignSituation::ServiceCharacteristic: return psi.psi0;
    default: return psi.psi2;
    }
}

struct Generator {
    const LoadCaseSet& set;
    const CombinationOptions& opt;
    std::vector<PermEntry> perms;
    std::vector<VarSlot> slots;       // live load, temperature, snow, wind
    std::vector<VarOption> seismic;   // alternatives with sign
    std::vector<VarOption> accidental;
    std::string error;

    bool build() {
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
                slots.push_back({{{idx, 1.0, *psi, isNonConcomitantCategory(cat) && !isEn(opt.code)}}});
                break;
            }
            case LoadNature::Temperature: slots.push_back({{{idx, 1.0, psiTemperature(), false}}}); break;
            case LoadNature::Snow: slots.push_back({{{idx, 1.0, psiSnow(opt.code, opt.snow), false}}}); break;
            case LoadNature::Wind: {
                if (slots.empty() || slots.back().options.empty() || !windSlotOpen) {
                    slots.push_back({});
                    windSlot = slots.size() - 1;
                    windSlotOpen = true;
                }
                const Psi psi = psiWind(opt.code, opt.nationalAnnex);
                slots[windSlot].options.push_back({idx, 1.0, psi, false});
                slots[windSlot].options.push_back({idx, -1.0, psi, false});
                break;
            }
            case LoadNature::Seismic:
                seismic.push_back({idx, 1.0, {}, false});
                seismic.push_back({idx, -1.0, {}, false});
                break;
            case LoadNature::Accidental:
                accidental.push_back({idx, 1.0, {}, false});
                if (opt.code == NormCode::Ehe08) accidental.push_back({idx, -1.0, {}, false});  // EHE-08: +/- accidental action [V]
                break;
            }
        }
        return true;
    }

    bool windSlotOpen = false;
    size_t windSlot = 0;

    long long emitted = 0;
    std::set<std::vector<long long>> seen;
    std::vector<std::vector<double>> out;

    void emit(const std::vector<double>& v) {
        if (error.size()) return;
        if (!seen.insert(keyOf(v)).second) return;
        if (static_cast<long long>(seen.size()) > opt.maxCombinations) {
            error = "Too many combinations (limit " + std::to_string(opt.maxCombinations) + ").";
            return;
        }
        out.push_back(v);
    }

    void principalLoop(const SituationSpec& spec, const PartialFactors& g, const std::vector<double>& base,
                       const std::vector<const VarOption*>& chosen) {
        auto coefOf = [&](const VarOption& o, bool principal) {
            const double factor = principal ? principalFactor(spec.situation, o.psi) : accompanyingFactor(spec.situation, o.psi);
            const double gamma = g.varUnfav;
            if (opt.longTermFraction > 0.0)
                return o.sign * effectiveVariableCoefficient(gamma, o.psi.psi2, gamma, factor, opt.longTermFraction);
            return o.sign * gamma * factor;
        };
        auto build = [&](int principal) {
            std::vector<double> v = base;
            for (size_t i = 0; i < chosen.size(); ++i)
                v[static_cast<size_t>(chosen[i]->idx)] = coefOf(*chosen[i], static_cast<int>(i) == principal);
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

    void slotLoop(const SituationSpec& spec, const PartialFactors& g, const std::vector<double>& base, size_t slot,
                  std::vector<const VarOption*>& chosen) {
        if (error.size()) return;
        if (slot == slots.size()) {
            principalLoop(spec, g, base, chosen);
            return;
        }
        slotLoop(spec, g, base, slot + 1, chosen);  // slot not acting
        for (const VarOption& o : slots[slot].options) {
            if (spec.g1Mode != o.nonConcomitant) continue;  // G1 acts only alone, in its own situation
            chosen.push_back(&o);
            slotLoop(spec, g, base, slot + 1, chosen);
            chosen.pop_back();
        }
    }

    void run(const SituationSpec& spec) {
        const PartialFactors g = partialFactors(opt.code, spec.situation);
        const bool seismicSit = spec.situation == DesignSituation::Seismic;
        const bool accSit = spec.situation == DesignSituation::Accidental;
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
            std::vector<double> base(n, 0.0);
            for (size_t i = 0; i < perms.size(); ++i) {
                double f, u;
                gammas(perms[i].kind, f, u);
                bool unfavourable = true;
                const auto it = std::find(varying.begin(), varying.end(), i);
                if (it != varying.end()) unfavourable = (mask >> (it - varying.begin())) & 1ULL;
                base[static_cast<size_t>(perms[i].idx)] = unfavourable ? u : f;
            }
            if (seismicSit || accSit) {
                for (const VarOption& a : seismicSit ? seismic : accidental) {
                    std::vector<double> b = base;
                    b[static_cast<size_t>(a.idx)] = a.sign * 1.0;  // gamma_A = gamma_E = 1.0 [V]
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

}  // namespace

PartialFactors partialFactors(NormCode code, DesignSituation s) {
    PartialFactors p;  // 1 / 1, variable 0 / 1: accidental, seismic and service situations
    if (s != DesignSituation::Persistent) return p;
    switch (code) {
    case NormCode::CteDbSe:  // CTE DB-SE 4.2.2 [V]
        p.permFav = 0.8; p.permUnfav = 1.35; p.varUnfav = 1.5;
        p.earthFav = 0.7; p.earthUnfav = 1.35;
        p.nonConstFav = 0.8; p.nonConstUnfav = 1.35;
        break;
    case NormCode::En1990CaseB:  // EN 1990 Table A1.2(B) [V]
        p.permFav = 1.0; p.permUnfav = 1.35; p.varUnfav = 1.5;
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
        if (options.withSeismic) blocks.push_back({{DesignSituation::Seismic, Role::NoPrincipal, false}, CombinationLimitState::Ultimate});
        if (options.withAccidental)
            blocks.push_back({{DesignSituation::Accidental, Role::PrincipalOptional, false}, CombinationLimitState::Ultimate});
    }
    if (options.withService) {
        blocks.push_back({{DesignSituation::ServiceCharacteristic, Role::PrincipalRequired, false}, CombinationLimitState::Service});
        blocks.push_back({{DesignSituation::ServiceFrequent, Role::PrincipalRequired, false}, CombinationLimitState::Service});
        blocks.push_back({{DesignSituation::ServiceQuasiPermanent, Role::NoPrincipal, false}, CombinationLimitState::Service});
    }

    int ultimateCount = 0, serviceCount = 0;
    std::set<std::pair<int, std::vector<long long>>> seenPerSituation;  // the "(G1)" block shares the persistent situation
    for (const Block& b : blocks) {
        gen.out.clear();
        gen.seen.clear();
        gen.run(b.spec);
        if (!gen.error.empty()) {
            result.error = gen.error;
            result.combinations.clear();
            return result;
        }
        std::vector<std::vector<double>> sorted = gen.out;
        std::sort(sorted.begin(), sorted.end(), [](const auto& a, const auto& c) { return keyOf(a) < keyOf(c); });
        for (auto& v : sorted) {
            if (!seenPerSituation.insert({static_cast<int>(b.spec.situation), keyOf(v)}).second) continue;
            Combination c;
            c.situation = b.spec.situation;
            c.limitState = b.ls;
            c.coefficients = std::move(v);
            c.name.clear();
            result.combinations.push_back(std::move(c));
        }
    }
    // Per situation, lexicographic order across the persistent ordinary and "(G1)" blocks.
    std::stable_sort(result.combinations.begin(), result.combinations.end(), [](const Combination& a, const Combination& b) {
        if (a.situation != b.situation) return static_cast<int>(a.situation) < static_cast<int>(b.situation);
        return keyOf(a.coefficients) < keyOf(b.coefficients);
    });
    for (Combination& c : result.combinations) {
        const int n = c.limitState == CombinationLimitState::Ultimate ? ++ultimateCount : ++serviceCount;
        c.name = "Comb. " + std::to_string(n);
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
    return text.find("superior a 1000") != std::string::npos ? SnowZone::Above1000 : SnowZone::AtOrBelow1000;
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
