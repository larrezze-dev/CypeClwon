#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <set>
#include <string>
#include <tuple>

#include "core/combinations.h"
#include "core/project.h"

using namespace cc::core;

#define MD "\xC2\xB7"  // U+00B7 middle dot of the original's combination names

namespace {

/// Reference work of the original (research 12.1/12.2): PP, Q 1, five wind cases, N(EI) and optionally S 1, S 2, A 1.
LoadCaseSet referenceWork(bool seismic, bool accidental, int seismicCount = 1) {
    LoadCaseSet set;
    auto add = [&](LoadNature n, const char* ref) {
        LoadCase c;
        c.id = set.nextId++;
        c.nature = n;
        c.reference = ref;
        set.cases.push_back(c);
    };
    add(LoadNature::LiveLoad, "Q 1");
    for (const char* v : {"V(0\xC2\xB0) H1", "V(0\xC2\xB0) H2", "V(90\xC2\xB0) H1", "V(180\xC2\xB0) H1", "V(270\xC2\xB0) H1"})
        add(LoadNature::Wind, v);
    add(LoadNature::Snow, "N(EI)");
    if (seismic) {
        add(LoadNature::Seismic, "S 1");
        if (seismicCount > 1) add(LoadNature::Seismic, "S 2");
    }
    if (accidental) add(LoadNature::Accidental, "A 1");
    return set;
}

CombinationOptions referenceOptions(bool ec) {
    CombinationOptions o;
    o.code = ec ? NormCode::En1990CaseB : NormCode::CteDbSe;
    o.snow = ec ? SnowZone::Above1000 : SnowZone::AtOrBelow1000;
    o.defaultUseCategory = ec ? "H" : "G2";  // psi0 = 0
    return o;
}

std::vector<std::string> namesOf(const CombinationResult& r, DesignSituation s) {
    std::vector<std::string> v;
    for (const auto& c : r.combinations)
        if (c.situation == s) v.push_back(c.name);
    return v;
}

LoadCaseSet makeSet(std::initializer_list<LoadNature> natures) {
    LoadCaseSet set;
    for (LoadNature n : natures) {
        LoadCase c;
        c.id = set.nextId++;
        c.nature = n;
        c.reference = loadNaturePrefix(n) + std::to_string(c.id);
        set.cases.push_back(c);
    }
    return set;
}

std::vector<const Combination*> of(const CombinationResult& r, DesignSituation s) {
    std::vector<const Combination*> v;
    for (const auto& c : r.combinations)
        if (c.situation == s) v.push_back(&c);
    return v;
}

bool has(const CombinationResult& r, DesignSituation s, std::vector<double> coefs) {
    for (const auto* c : of(r, s)) {
        if (c->coefficients.size() != coefs.size()) continue;
        bool same = true;
        for (size_t i = 0; i < coefs.size(); ++i) same = same && std::fabs(c->coefficients[i] - coefs[i]) < 1e-9;
        if (same) return true;
    }
    return false;
}

}  // namespace

TEST(CombinationTables, CteGamma) {
    const auto p = partialFactors(NormCode::CteDbSe, DesignSituation::Persistent);
    EXPECT_DOUBLE_EQ(p.permFav, 0.8);
    EXPECT_DOUBLE_EQ(p.permUnfav, 1.35);
    EXPECT_DOUBLE_EQ(p.varFav, 0.0);
    EXPECT_DOUBLE_EQ(p.varUnfav, 1.5);
    EXPECT_DOUBLE_EQ(p.earthFav, 0.7);
    EXPECT_DOUBLE_EQ(p.earthUnfav, 1.35);
    for (auto s : {DesignSituation::Seismic, DesignSituation::Accidental, DesignSituation::ServiceCharacteristic}) {
        const auto q = partialFactors(NormCode::CteDbSe, s);
        EXPECT_DOUBLE_EQ(q.permFav, 1.0);
        EXPECT_DOUBLE_EQ(q.permUnfav, 1.0);
        EXPECT_DOUBLE_EQ(q.varUnfav, 1.0);
        EXPECT_DOUBLE_EQ(q.varFav, 0.0);
    }
}

TEST(CombinationTables, En1990AndEheGamma) {
    const auto b = partialFactors(NormCode::En1990CaseB, DesignSituation::Persistent);
    EXPECT_EQ(std::make_tuple(b.permFav, b.permUnfav, b.varFav, b.varUnfav), std::make_tuple(1.0, 1.35, 0.0, 1.5));
    const auto c = partialFactors(NormCode::En1990CaseC, DesignSituation::Persistent);
    EXPECT_EQ(std::make_tuple(c.permFav, c.permUnfav, c.varFav, c.varUnfav), std::make_tuple(1.0, 1.0, 0.0, 1.3));
    const auto e = partialFactors(NormCode::Ehe08, DesignSituation::Persistent);
    EXPECT_EQ(std::make_tuple(e.permFav, e.permUnfav, e.varUnfav), std::make_tuple(1.0, 1.35, 1.5));
    EXPECT_EQ(std::make_tuple(e.nonConstFav, e.nonConstUnfav), std::make_tuple(1.0, 1.5));
}

TEST(CombinationTables, CtePsi) {
    EXPECT_EQ(*psiLiveLoad(NormCode::CteDbSe, "A"), (Psi{0.7, 0.5, 0.3}));
    EXPECT_EQ(*psiLiveLoad(NormCode::CteDbSe, "B"), (Psi{0.7, 0.5, 0.3}));
    for (const char* c : {"C", "D", "E"}) EXPECT_EQ(*psiLiveLoad(NormCode::CteDbSe, c), (Psi{0.7, 0.7, 0.6}));
    for (const char* c : {"G1", "G2"}) EXPECT_EQ(*psiLiveLoad(NormCode::CteDbSe, c), (Psi{0, 0, 0}));
    EXPECT_FALSE(psiLiveLoad(NormCode::CteDbSe, "H").has_value());
    EXPECT_EQ(psiWind(NormCode::CteDbSe), (Psi{0.6, 0.5, 0.0}));
    EXPECT_EQ(psiTemperature(), (Psi{0.6, 0.5, 0.0}));
    EXPECT_EQ(psiSnow(NormCode::CteDbSe, SnowZone::Above1000), (Psi{0.7, 0.5, 0.2}));
    EXPECT_EQ(psiSnow(NormCode::CteDbSe, SnowZone::AtOrBelow1000), (Psi{0.5, 0.2, 0.0}));
}

TEST(CombinationTables, En1990Psi) {
    EXPECT_EQ(*psiLiveLoad(NormCode::En1990CaseB, "E"), (Psi{1.0, 0.9, 0.8}));
    EXPECT_EQ(*psiLiveLoad(NormCode::En1990CaseB, "H"), (Psi{0, 0, 0}));
    EXPECT_EQ(*psiLiveLoad(NormCode::En1990CaseB, "F"), (Psi{0.7, 0.7, 0.6}));
    EXPECT_EQ(*psiLiveLoad(NormCode::En1990CaseB, "G"), (Psi{0.7, 0.5, 0.3}));
    EXPECT_FALSE(psiLiveLoad(NormCode::En1990CaseB, "G1").has_value());
    EXPECT_EQ(psiWind(NormCode::En1990CaseB), (Psi{0.6, 0.2, 0.0}));
    EXPECT_EQ(psiWind(NormCode::En1990CaseB, 8), (Psi{0.5, 0.2, 0.0}));
    EXPECT_EQ(psiLiveLoad(NormCode::En1990CaseB, "H", 9)->psi0, 0.7);  // Malaysia / Singapore
    EXPECT_EQ(psiLiveLoad(NormCode::En1990CaseB, "E", 9)->psi0, 1.0);
    EXPECT_EQ(psiSnow(NormCode::En1990CaseB, SnowZone::Nordic), (Psi{0.7, 0.5, 0.2}));
    EXPECT_EQ(psiSnow(NormCode::En1990CaseB, SnowZone::AtOrBelow1000), (Psi{0.5, 0.2, 0.0}));
}

TEST(Combinations, EffectiveCoefficient) {
    EXPECT_DOUBLE_EQ(effectiveVariableCoefficient(1.5, 0.3, 1.5, 0.7, 0.0), 1.05);
    EXPECT_DOUBLE_EQ(effectiveVariableCoefficient(1.5, 0.3, 1.5, 0.7, 1.0), 0.45);
    EXPECT_DOUBLE_EQ(effectiveVariableCoefficient(1.5, 0.3, 1.5, 0.7, 0.5), 0.75);
}

TEST(Combinations, SelfWeightOnly) {
    const auto set = makeSet({});
    const auto r = generateCombinations(set, {});
    ASSERT_TRUE(r.error.empty());
    EXPECT_EQ(of(r, DesignSituation::Persistent).size(), 2u);  // favourable / unfavourable
    EXPECT_TRUE(has(r, DesignSituation::Persistent, {0.8}));
    EXPECT_TRUE(has(r, DesignSituation::Persistent, {1.35}));
    EXPECT_EQ(of(r, DesignSituation::Seismic).size(), 0u);
    EXPECT_EQ(of(r, DesignSituation::Accidental).size(), 0u);
    EXPECT_EQ(of(r, DesignSituation::ServiceCharacteristic).size(), 1u);
    EXPECT_EQ(of(r, DesignSituation::ServiceFrequent).size(), 1u);
    EXPECT_EQ(of(r, DesignSituation::ServiceQuasiPermanent).size(), 1u);
    EXPECT_EQ(r.combinations[0].name, "0.8" MD "PP");  // formula names of the original, favourable first
    EXPECT_EQ(r.combinations[1].name, "1.35" MD "PP");
    EXPECT_EQ(r.combinations[2].name, "PP");
}

TEST(Combinations, LiveLoadPersistentAndOrder) {
    const auto set = makeSet({LoadNature::LiveLoad});
    const auto r = generateCombinations(set, {});
    const auto p = of(r, DesignSituation::Persistent);
    ASSERT_EQ(p.size(), 4u);
    // Index order (Q more significant than PP; absent < principal; favourable < unfavourable), not by coefficient value.
    EXPECT_EQ(p[0]->coefficients, (std::vector<double>{0.8, 0.0}));
    EXPECT_EQ(p[1]->coefficients, (std::vector<double>{1.35, 0.0}));
    EXPECT_EQ(p[2]->coefficients, (std::vector<double>{0.8, 1.5}));
    EXPECT_EQ(p[3]->coefficients, (std::vector<double>{1.35, 1.5}));
}

TEST(Combinations, MainAndAccompanyingVariables) {
    const auto set = makeSet({LoadNature::LiveLoad, LoadNature::Wind, LoadNature::Snow});  // PP, Q, V, N
    const auto r = generateCombinations(set, {});
    ASSERT_TRUE(r.error.empty());
    EXPECT_TRUE(has(r, DesignSituation::Persistent, {1.35, 1.05, 1.5, 0.0}));    // wind main, Q psi0 0.7
    EXPECT_TRUE(has(r, DesignSituation::Persistent, {1.35, 1.5, 0.9, 0.0}));     // Q main, wind psi0 0.6
    EXPECT_FALSE(has(r, DesignSituation::Persistent, {1.35, 1.05, -1.5, 0.0}));  // wind has no negative sign
    EXPECT_TRUE(has(r, DesignSituation::Persistent, {1.35, 1.5, 0.0, 0.75}));    // snow psi0 0.5 (<= 1000 m)
    EXPECT_TRUE(has(r, DesignSituation::Persistent, {0.8, 0.0, 1.5, 0.0}));      // favourable permanent
    // No duplicates in any situation.
    for (auto s : {DesignSituation::Persistent, DesignSituation::ServiceCharacteristic}) {
        auto v = of(r, s);
        for (size_t i = 0; i < v.size(); ++i)
            for (size_t j = i + 1; j < v.size(); ++j) EXPECT_NE(v[i]->coefficients, v[j]->coefficients);
    }
    CombinationOptions high;
    high.snow = SnowZone::Above1000;
    EXPECT_TRUE(has(generateCombinations(set, high), DesignSituation::Persistent, {1.35, 1.5, 0.0, 1.05}));  // psi0 0.7
}

TEST(Combinations, SeismicAndAccidental) {
    const auto set = makeSet({LoadNature::LiveLoad, LoadNature::Seismic, LoadNature::Accidental});  // PP, Q, S, A
    const auto r = generateCombinations(set, {});
    ASSERT_TRUE(r.error.empty());
    const auto s = of(r, DesignSituation::Seismic);
    EXPECT_TRUE(has(r, DesignSituation::Seismic, {1, 0.3, 1, 0}));
    EXPECT_TRUE(has(r, DesignSituation::Seismic, {1, 0.3, -1, 0}));
    EXPECT_TRUE(has(r, DesignSituation::Seismic, {1, 0, 1, 0}));
    EXPECT_EQ(s.size(), 4u);
    EXPECT_TRUE(has(r, DesignSituation::Accidental, {1, 0.5, 0, 1}));  // psi1 main
    EXPECT_TRUE(has(r, DesignSituation::Accidental, {1, 0.3, 0, 1}));  // psi2 only
    EXPECT_TRUE(has(r, DesignSituation::Accidental, {1, 0, 0, 1}));
    EXPECT_EQ(of(r, DesignSituation::Accidental).size(), 3u);
    CombinationOptions off;
    off.withSeismic = off.withAccidental = false;
    const auto r2 = generateCombinations(set, off);
    EXPECT_TRUE(of(r2, DesignSituation::Seismic).empty());
    EXPECT_TRUE(of(r2, DesignSituation::Accidental).empty());
}

TEST(Combinations, ServiceSituations) {
    const auto set = makeSet({LoadNature::LiveLoad});
    const auto r = generateCombinations(set, {});
    EXPECT_TRUE(has(r, DesignSituation::ServiceCharacteristic, {1, 1}));
    EXPECT_TRUE(has(r, DesignSituation::ServiceCharacteristic, {1, 0}));
    EXPECT_TRUE(has(r, DesignSituation::ServiceFrequent, {1, 0.5}));
    EXPECT_TRUE(has(r, DesignSituation::ServiceQuasiPermanent, {1, 0.3}));
    EXPECT_EQ(of(r, DesignSituation::ServiceQuasiPermanent).size(), 2u);
    for (const auto* c : of(r, DesignSituation::ServiceFrequent)) EXPECT_EQ(c->limitState, CombinationLimitState::Service);
}

TEST(Combinations, NonConcomitantG1) {
    const auto set = makeSet({LoadNature::LiveLoad});
    CombinationOptions o;
    o.defaultUseCategory = "G1";
    const auto r = generateCombinations(set, o);
    ASSERT_TRUE(r.error.empty());
    // G1 acts only in its own "(G1)" situation, alone with the permanent load.
    EXPECT_TRUE(has(r, DesignSituation::Persistent, {1.35, 1.5}));
    EXPECT_TRUE(has(r, DesignSituation::Persistent, {0.8, 1.5}));
    EXPECT_TRUE(has(r, DesignSituation::Persistent, {1.35, 0.0}));
    EXPECT_EQ(of(r, DesignSituation::Persistent).size(), 4u);
}

TEST(Combinations, En1990Cases) {
    const auto set = makeSet({LoadNature::LiveLoad, LoadNature::Wind});
    CombinationOptions o;
    o.code = NormCode::En1990CaseB;
    o.defaultUseCategory = "E";
    auto r = generateCombinations(set, o);
    EXPECT_TRUE(has(r, DesignSituation::Persistent, {1.35, 1.5, 0.9}));   // Q main, wind psi0 0.6
    EXPECT_TRUE(has(r, DesignSituation::Persistent, {1.0, 1.5, 0.9}));    // favourable permanent 1.0
    EXPECT_TRUE(has(r, DesignSituation::Persistent, {1.35, 1.5, 1.5}));   // E: psi0 1.0 -> 1.5 as accompanying
    o.code = NormCode::En1990CaseC;
    r = generateCombinations(set, o);
    EXPECT_TRUE(has(r, DesignSituation::Persistent, {1.0, 1.3, 0.78}));   // gamma 1.3, wind psi0 0.6
    o.defaultUseCategory = "G1";  // not an EN 1990 category
    r = generateCombinations(set, o);
    EXPECT_FALSE(r.error.empty());
    EXPECT_TRUE(r.combinations.empty());
}

TEST(Combinations, EarthPressureAndEhe) {
    const auto set = makeSet({LoadNature::EarthPressure});
    auto r = generateCombinations(set, {});
    EXPECT_TRUE(has(r, DesignSituation::Persistent, {0.8, 0.7}));
    EXPECT_TRUE(has(r, DesignSituation::Persistent, {1.35, 1.35}));
    EXPECT_TRUE(has(r, DesignSituation::Persistent, {1.35, 0.7}));
    EXPECT_EQ(of(r, DesignSituation::Persistent).size(), 4u);
    CombinationOptions e;
    e.code = NormCode::Ehe08;
    r = generateCombinations(set, e);
    EXPECT_TRUE(has(r, DesignSituation::Persistent, {1.0, 1.0}));
    EXPECT_TRUE(has(r, DesignSituation::Persistent, {1.35, 1.5}));
    EXPECT_EQ(of(r, DesignSituation::Persistent).size(), 4u);
    // Accidental action: +/- under EHE-08.
    const auto acc = makeSet({LoadNature::Accidental});
    r = generateCombinations(acc, e);
    EXPECT_TRUE(has(r, DesignSituation::Accidental, {1, 1}));
    EXPECT_TRUE(has(r, DesignSituation::Accidental, {1, -1}));
}

TEST(Combinations, CasesThatDoNotActAreSkipped) {
    auto set = makeSet({LoadNature::LiveLoad, LoadNature::Wind});
    set.cases[2].acts = false;
    const auto r = generateCombinations(set, {});
    for (const auto& c : r.combinations) EXPECT_DOUBLE_EQ(c.coefficients[2], 0.0);
    EXPECT_EQ(of(r, DesignSituation::Persistent).size(), 4u);
}

TEST(Combinations, UseCategoryPerCaseAndCap) {
    auto set = makeSet({LoadNature::LiveLoad, LoadNature::LiveLoad});  // PP, Q1, Q2
    CombinationOptions o;
    o.caseUseCategory[set.cases[1].id] = "A";
    o.caseUseCategory[set.cases[2].id] = "G2";
    auto r = generateCombinations(set, o);
    EXPECT_TRUE(has(r, DesignSituation::Persistent, {1.35, 1.5, 0.0}));  // G2 psi0 = 0 as accompanying: same as absent
    EXPECT_TRUE(has(r, DesignSituation::Persistent, {1.35, 0.0, 1.5}));  // G2 principal factor 1
    EXPECT_TRUE(has(r, DesignSituation::Persistent, {1.35, 1.05, 1.5}));  // Q1 accompanying psi0 0.7 with G2 main
    o.maxCombinations = 3;
    EXPECT_FALSE(generateCombinations(set, o).error.empty());
}

TEST(Combinations, LongTermFractionMixesPsi2) {
    const auto set = makeSet({LoadNature::LiveLoad});
    CombinationOptions o;
    o.longTermFraction = 1.0;
    EXPECT_TRUE(has(generateCombinations(set, o), DesignSituation::Persistent, {1.35, 1.5 * 0.3}));
}

TEST(Combinations, FromProject) {
    Project p;
    p.loadCases = makeSet({LoadNature::Snow});
    auto o = combinationOptionsFor(p, StandardFamily::Concrete);  // Eurocódigo 2 (UE)
    EXPECT_EQ(o.code, NormCode::En1990CaseB);
    EXPECT_EQ(o.snow, SnowZone::AtOrBelow1000);
    EXPECT_EQ(combinationOptionsFor(p, StandardFamily::ColdFormedSteel).code, NormCode::CteDbSe);
    EXPECT_EQ(combinationOptionsFor(p, StandardFamily::HotRolledSteel).code, NormCode::CteDbSe);  // radio CTE
    p.limitStates.hotRolledCode = CombinationCode::EC;
    EXPECT_EQ(combinationOptionsFor(p, StandardFamily::HotRolledSteel).code, NormCode::En1990CaseB);
    p.standards.concrete = "EHE-08";
    EXPECT_EQ(combinationOptionsFor(p, StandardFamily::Concrete).code, NormCode::Ehe08);
    p.limitStates.timberSnow = "Altitud superior a 1000 m";
    EXPECT_EQ(combinationOptionsFor(p, StandardFamily::Timber).snow, SnowZone::Above1000);
    EXPECT_FALSE(combinationCodeFromStandard("Algo desconocido").has_value());
    EXPECT_EQ(combinationCodeFromStandard("Código Estructural (España)"), NormCode::Ehe08);
    const auto r = generateCombinations(p, StandardFamily::Timber);
    EXPECT_TRUE(has(r, DesignSituation::Persistent, {1.35, 1.5}));
}

// ---- T-P7-06: adjustments to what was measured in the original (research 12.1 - 12.5) ----

TEST(CombinationsOriginal, FormulaNames) {
    const auto set = referenceWork(true, true);  // PP, Q1, V0H1, V0H2, V90H1, V180H1, V270H1, N, S1, A1
    EXPECT_EQ(combinationFormulaName(set, {1.35, 1.5, 0, 0, 0, 0, 0.9, 1.05, 0, 0}),
              "1.35" MD "PP+1.5" MD "Q1+0.9" MD "V(270\xC2\xB0)H1+1.05" MD "N(EI)");
    EXPECT_EQ(combinationFormulaName(set, {0.8, 0, 0, 0, 0, 0, 0, 1.5, 0, 0}), "0.8" MD "PP+1.5" MD "N(EI)");
    EXPECT_EQ(combinationFormulaName(set, {1, 0, 0, 0, 0, 0, 0, 0, -1, 0}), "PP-S1");
    EXPECT_EQ(combinationFormulaName(set, {1, 0, 0, 0, 0, 0, 0, 0, 1, 0}), "PP+S1");
    EXPECT_EQ(combinationFormulaName(set, {1, 0, 0.5, 0, 0, 0, 0, 0, 0, 1}), "PP+0.5" MD "V(0\xC2\xB0)H1+A1");
    EXPECT_EQ(combinationFormulaName(set, {1, 0, 0, 0, 0, 0, 0, 0.2, 0, 1}), "PP+0.2" MD "N(EI)+A1");
    EXPECT_EQ(combinationFormulaName(set, {1, 0, 0, 0, 0, 0, 0, 0, 0, 0}), "PP");
}

TEST(CombinationsOriginal, PersistentCount58EcAndCte) {
    for (const bool ec : {true, false}) {
        const auto set = referenceWork(false, false);
        auto o = referenceOptions(ec);
        o.withSeismic = o.withAccidental = o.withService = false;
        const auto r = generateCombinations(set, o);
        ASSERT_TRUE(r.error.empty());
        const auto names = namesOf(r, DesignSituation::Persistent);
        EXPECT_EQ(names.size(), 58u) << (ec ? "EC" : "CTE");
        EXPECT_EQ(r.combinations.size(), 58u);
        // 24 without snow + 12 with snow as the main action + 22 with snow accompanying.
        const std::string snowMain = "1.5" MD "N(EI)", snowAcc = std::string(ec ? "1.05" : "0.75") + MD "N(EI)";
        int none = 0, main = 0, acc = 0;
        for (const auto& n : names) {
            if (n.find("N(EI)") == std::string::npos) ++none;
            else if (n.find(snowMain) != std::string::npos) ++main;
            else if (n.find(snowAcc) != std::string::npos) ++acc;
        }
        EXPECT_EQ(none, 24);
        EXPECT_EQ(main, 12);
        EXPECT_EQ(acc, 22);
        // Order of the original: PP favourable first, then Q, then each wind (without / with Q), then snow.
        const std::string fav = ec ? "PP" : "0.8" MD "PP";
        EXPECT_EQ(names[0], fav);
        EXPECT_EQ(names[1], "1.35" MD "PP");
        EXPECT_EQ(names[2], fav + "+1.5" MD "Q1");
        EXPECT_EQ(names[3], "1.35" MD "PP+1.5" MD "Q1");
        EXPECT_EQ(names[4], fav + "+1.5" MD "V(0\xC2\xB0)H1");
        EXPECT_EQ(names[5], "1.35" MD "PP+1.5" MD "V(0\xC2\xB0)H1");
        EXPECT_EQ(names[6], fav + "+1.5" MD "Q1+0.9" MD "V(0\xC2\xB0)H1");
        EXPECT_EQ(names[8], fav + "+1.5" MD "V(0\xC2\xB0)H2");
        EXPECT_EQ(names[24], fav + "+1.5" MD "N(EI)");  // first snow-main combination
        EXPECT_EQ(names.back(), std::string("1.35" MD "PP+1.5" MD "Q1+0.9" MD "V(270\xC2\xB0)H1+") + (ec ? "1.05" : "0.75") + MD "N(EI)");
        // No negative sign for the wind and no duplicates.
        for (const auto& c : r.combinations)
            for (size_t i = 2; i < 7; ++i) EXPECT_GE(c.coefficients[i], 0.0);
        std::set<std::string> uniq(names.begin(), names.end());
        EXPECT_EQ(uniq.size(), names.size());
    }
}

TEST(CombinationsOriginal, SeismicAndAccidentalCte67) {
    const auto set = referenceWork(true, true);
    auto o = referenceOptions(false);
    o.withService = false;
    const auto r = generateCombinations(set, o);
    ASSERT_TRUE(r.error.empty());
    EXPECT_EQ(r.combinations.size(), 67u);  // 58 + 2 (S1) + 7 (A1, psi1 principal)
    EXPECT_EQ(namesOf(r, DesignSituation::Seismic), (std::vector<std::string>{"PP-S1", "PP+S1"}));
    const auto acc = namesOf(r, DesignSituation::Accidental);
    ASSERT_EQ(acc.size(), 7u);
    EXPECT_EQ(acc[0], "PP+A1");
    EXPECT_EQ(acc[1], "PP+0.5" MD "V(0\xC2\xB0)H1+A1");
    EXPECT_EQ(acc[5], "PP+0.5" MD "V(270\xC2\xB0)H1+A1");
    EXPECT_EQ(acc[6], "PP+0.2" MD "N(EI)+A1");
}

TEST(CombinationsOriginal, SeismicAndAccidentalEcUsePsi2) {
    const auto set = referenceWork(true, true, 2);
    auto o = referenceOptions(true);
    o.snow = SnowZone::AtOrBelow1000;  // snow psi2 = 0 (TODO: with H > 1000 m psi2 = 0.2 would add a snow term; not measured)
    o.withService = false;
    const auto r = generateCombinations(set, o);
    ASSERT_TRUE(r.error.empty());
    EXPECT_EQ(namesOf(r, DesignSituation::Seismic), (std::vector<std::string>{"PP-S1", "PP+S1", "PP-S2", "PP+S2"}));
    EXPECT_EQ(namesOf(r, DesignSituation::Accidental), (std::vector<std::string>{"PP+A1"}));  // psi2 principal: wind / snow psi2 = 0
    // Principal factor of a live load in the accidental situation: EC psi2, CTE psi1.
    const auto q = makeSet({LoadNature::LiveLoad, LoadNature::Accidental});
    CombinationOptions e;
    e.code = NormCode::En1990CaseB;  // category A: psi1 0.5, psi2 0.3
    EXPECT_TRUE(has(generateCombinations(q, e), DesignSituation::Accidental, {1, 0.3, 1}));
    EXPECT_FALSE(has(generateCombinations(q, e), DesignSituation::Accidental, {1, 0.5, 1}));
}

TEST(CombinationsOriginal, DisplacementsAre24) {
    for (const bool ec : {true, false}) {
        const auto set = referenceWork(false, false);
        auto o = referenceOptions(ec);
        o.withUltimate = false;
        const auto r = generateCombinations(set, o);
        ASSERT_TRUE(r.error.empty());
        const auto d = namesOf(r, DesignSituation::ServiceCharacteristic);
        ASSERT_EQ(d.size(), 24u);
        EXPECT_EQ(d[0], "PP");
        EXPECT_EQ(d[1], "PP+Q1");
        EXPECT_EQ(d[2], "PP+V(0\xC2\xB0)H1");
        EXPECT_EQ(d[3], "PP+Q1+V(0\xC2\xB0)H1");
        EXPECT_EQ(d[12], "PP+N(EI)");
        EXPECT_EQ(d[13], "PP+Q1+N(EI)");
        EXPECT_EQ(d[14], "PP+V(0\xC2\xB0)H1+N(EI)");
        EXPECT_EQ(d.back(), "PP+Q1+V(270\xC2\xB0)H1+N(EI)");
    }
    // With two seismic cases: the +/- variants follow (Q 1 only; wind and snow do not appear with the earthquake).
    const auto set = referenceWork(true, false, 2);
    auto o = referenceOptions(false);
    o.withUltimate = false;
    const auto d = namesOf(generateCombinations(set, o), DesignSituation::ServiceCharacteristic);
    ASSERT_EQ(d.size(), 32u);
    const std::vector<std::string> tail(d.begin() + 24, d.end());
    EXPECT_EQ(tail, (std::vector<std::string>{"PP-S1", "PP+Q1-S1", "PP+S1", "PP+Q1+S1", "PP-S2", "PP+Q1-S2", "PP+S2", "PP+Q1+S2"}));
}

TEST(CombinationsOriginal, ShrinkageAndEarthPressureGamma) {
    const auto cte = partialFactors(NormCode::CteDbSe, DesignSituation::Persistent);
    EXPECT_EQ(std::make_tuple(cte.nonConstFav, cte.nonConstUnfav), std::make_tuple(1.0, 1.5));  // Retracción 1.000 / 1.500
    const auto ecb = partialFactors(NormCode::En1990CaseB, DesignSituation::Persistent);
    EXPECT_EQ(std::make_tuple(ecb.earthFav, ecb.earthUnfav), std::make_tuple(1.0, 1.35));  // Empujes del terreno
    for (auto s : {DesignSituation::Seismic, DesignSituation::Accidental}) {
        const auto p = partialFactors(NormCode::CteDbSe, s);
        EXPECT_EQ(std::make_tuple(p.nonConstFav, p.nonConstUnfav), std::make_tuple(1.0, 1.0));  // 1 / 1
    }
    const auto set = makeSet({LoadNature::Shrinkage});
    const auto r = generateCombinations(set, {});  // CTE
    EXPECT_TRUE(has(r, DesignSituation::Persistent, {0.8, 1.0}));
    EXPECT_TRUE(has(r, DesignSituation::Persistent, {1.35, 1.5}));
}

TEST(CombinationsOriginal, HotRolledRadioChangesTheSet) {
    Project p;
    p.loadCases = referenceWork(true, true);
    p.limitStates.hotRolledSnow = "Resto de los Estados miembro del CEN, H > 1000 m";
    auto cteOpt = combinationOptionsFor(p, StandardFamily::HotRolledSteel);
    p.limitStates.hotRolledCode = CombinationCode::EC;
    auto ecOpt = combinationOptionsFor(p, StandardFamily::HotRolledSteel);
    EXPECT_EQ(ecOpt.snow, SnowZone::Above1000);
    cteOpt.snow = SnowZone::AtOrBelow1000;
    cteOpt.caseUseCategory[1] = "G2";
    ecOpt.caseUseCategory[1] = "H";
    cteOpt.withService = ecOpt.withService = false;
    const auto cte = generateCombinations(p.loadCases, cteOpt);
    const auto ec = generateCombinations(p.loadCases, ecOpt);
    ASSERT_TRUE(cte.error.empty() && ec.error.empty());
    EXPECT_EQ(cte.combinations.front().name, "0.8" MD "PP");  // CTE: PP favourable 0.8
    EXPECT_EQ(ec.combinations.front().name, "PP");             // EC: PP favourable 1.0
    EXPECT_EQ(namesOf(cte, DesignSituation::Accidental).size(), 7u);  // psi1 principal
    EXPECT_EQ(namesOf(ec, DesignSituation::Persistent).size(), 58u);
    EXPECT_EQ(namesOf(cte, DesignSituation::Persistent).size(), 58u);
    EXPECT_EQ(snowZoneFromText("Finlandia, Islandia, Noruega, Suecia"), SnowZone::Nordic);
    EXPECT_EQ(snowZoneFromText("Resto de los Estados miembro del CEN, H <= 1000 m"), SnowZone::AtOrBelow1000);
}
