#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>

#include "core/combinations.h"
#include "core/project.h"

using namespace cc::core;

namespace {

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
    EXPECT_EQ(r.combinations.front().name, "Comb. 1");
    EXPECT_EQ(r.combinations[1].name, "Comb. 2");
    EXPECT_EQ(r.combinations[2].name, "Comb. 1");  // service numbering restarts
}

TEST(Combinations, LiveLoadPersistentAndOrder) {
    const auto set = makeSet({LoadNature::LiveLoad});
    const auto r = generateCombinations(set, {});
    const auto p = of(r, DesignSituation::Persistent);
    ASSERT_EQ(p.size(), 4u);
    // Lexicographic ascending by coefficient vector.
    EXPECT_EQ(p[0]->coefficients, (std::vector<double>{0.8, 0.0}));
    EXPECT_EQ(p[1]->coefficients, (std::vector<double>{0.8, 1.5}));
    EXPECT_EQ(p[2]->coefficients, (std::vector<double>{1.35, 0.0}));
    EXPECT_EQ(p[3]->coefficients, (std::vector<double>{1.35, 1.5}));
}

TEST(Combinations, MainAndAccompanyingVariables) {
    const auto set = makeSet({LoadNature::LiveLoad, LoadNature::Wind, LoadNature::Snow});  // PP, Q, V, N
    const auto r = generateCombinations(set, {});
    ASSERT_TRUE(r.error.empty());
    EXPECT_TRUE(has(r, DesignSituation::Persistent, {1.35, 1.05, 1.5, 0.0}));    // wind main, Q psi0 0.7
    EXPECT_TRUE(has(r, DesignSituation::Persistent, {1.35, 1.5, 0.9, 0.0}));     // Q main, wind psi0 0.6
    EXPECT_TRUE(has(r, DesignSituation::Persistent, {1.35, 1.05, -1.5, 0.0}));   // wind sign alternation
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
