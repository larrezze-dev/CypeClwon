// Tests of the user situations and of the layout of "Estados límite" (T-P7-06, GUI part: model, validations, persistence).
#include <gtest/gtest.h>

#include "core/combination_export.h"
#include "core/combination_report.h"
#include "core/combinations.h"
#include "core/limit_states.h"
#include "core/project.h"
#include "core/project_json.h"
#include "core/user_situations.h"

using namespace cc::core;

namespace {
UserSituationSet sampleSet() {
    UserSituationSet set;
    set.reference = "Mi conjunto";
    set.mode = CoefficientMode::PartialAndCombination;
    set.categories = {"A", "B"};
    UserSituation s = UserSituation::withDefaults(2);
    s.name = "Nombre1";
    s.type = SituationType::Seismic;
    s.wind.favourable = 0.2;
    s.wind.unfavourable = 1.5;
    s.wind.principal = 1.0;
    s.wind.accompanying = 0.6;
    set.situations.push_back(s);
    UserSituation persistent = UserSituation::withDefaults(2);
    persistent.name = "Persistente";
    set.situations.push_back(persistent);
    return set;
}
}  // namespace

TEST(UserSituations, DefaultsAreTheMeasuredOnes) {
    const UserSituation s = UserSituation::withDefaults(3);
    EXPECT_EQ(s.imposed.size(), 3u);
    // Everything 1.000 except the favourable factor of the variable actions.
    EXPECT_DOUBLE_EQ(s.permanent.favourable, 1.0);
    EXPECT_DOUBLE_EQ(s.prestress.unfavourable, 1.0);
    EXPECT_DOUBLE_EQ(s.shrinkage.favourable, 1.0);
    EXPECT_DOUBLE_EQ(s.earthThrust.favourable, 1.0);
    for (const SituationRow* r : {&s.wind, &s.snow, &s.temperature, &s.imposed[0], &s.imposed[2]}) {
        EXPECT_DOUBLE_EQ(r->favourable, 0.0);
        EXPECT_DOUBLE_EQ(r->unfavourable, 1.0);
        EXPECT_DOUBLE_EQ(r->principal, 1.0);
        EXPECT_DOUBLE_EQ(r->accompanying, 1.0);
    }
    EXPECT_EQ(s.type, SituationType::PersistentOrTransient);
}

TEST(UserSituations, GridRowsFollowTheOriginalOrder) {
    UserSituation s = UserSituation::withDefaults(2);
    const auto rows = s.rows();
    ASSERT_EQ(rows.size(), 9u);  // permanent, prestress, shrinkage, 2 imposed, wind, snow, temperature, earth
    EXPECT_EQ(rows[0], &s.permanent);
    EXPECT_EQ(rows[3], &s.imposed[0]);
    EXPECT_EQ(rows[4], &s.imposed[1]);
    EXPECT_EQ(rows[5], &s.wind);
    EXPECT_EQ(rows[8], &s.earthThrust);
    EXPECT_TRUE(hasCombinationFactors(SituationAction::Wind));
    EXPECT_TRUE(hasCombinationFactors(SituationAction::Imposed));
    EXPECT_FALSE(hasCombinationFactors(SituationAction::Permanent));
    EXPECT_FALSE(hasCombinationFactors(SituationAction::EarthThrust));
}

TEST(UserSituations, ValidationOrderMatchesTheOriginal) {
    UserSituation s = UserSituation::withDefaults(1);
    s.name = "x";
    EXPECT_EQ(validateSituation(s, CoefficientMode::PartialAndCombination), SituationError::None);
    // Both errors present: the favourable / unfavourable one is reported first (measured).
    s.permanent.favourable = 2.0;
    s.permanent.unfavourable = 1.0;
    s.wind.principal = 0.5;
    s.wind.accompanying = 0.9;
    EXPECT_EQ(validateSituation(s, CoefficientMode::PartialAndCombination), SituationError::FavourableAboveUnfavourable);
    s.permanent.favourable = 1.0;
    EXPECT_EQ(validateSituation(s, CoefficientMode::PartialAndCombination), SituationError::PrincipalBelowAccompanying);
    // The combination factors only count in "mayoración y combinación".
    EXPECT_EQ(validateSituation(s, CoefficientMode::PartialFactors), SituationError::None);
    EXPECT_STREQ(situationErrorMessage(SituationError::FavourableAboveUnfavourable), "No puede introducir un coeficiente favorable mayor que el desfavorable.");
    EXPECT_STREQ(situationErrorMessage(SituationError::PrincipalBelowAccompanying), "No puede introducir un coeficiente principal menor que el de acompañamiento.");
}

TEST(UserSituations, RangeIsZeroToTen) {
    UserSituation s = UserSituation::withDefaults(0);
    s.permanent.unfavourable = 10.0;
    EXPECT_EQ(validateSituation(s, CoefficientMode::PartialFactors), SituationError::None);
    s.permanent.unfavourable = 10.001;
    EXPECT_EQ(validateSituation(s, CoefficientMode::PartialFactors), SituationError::OutOfRange);
    s.permanent.unfavourable = 1.0;
    s.snow.favourable = -0.001;
    EXPECT_EQ(validateSituation(s, CoefficientMode::PartialFactors), SituationError::OutOfRange);
}

TEST(UserSituations, SetValidation) {
    UserSituationSet set = sampleSet();
    EXPECT_EQ(validateSet(set), SituationError::None);
    EXPECT_EQ(validateSet(set, {"Mi conjunto"}), SituationError::DuplicateReference);
    UserSituationSet blank = set;
    blank.reference.clear();
    EXPECT_EQ(validateSet(blank), SituationError::EmptyReference);
    UserSituationSet none = set;
    none.situations.clear();
    EXPECT_EQ(validateSet(none), SituationError::NoSituations);  // #94354
    UserSituationSet dup = set;
    dup.situations.push_back(dup.situations.front());
    EXPECT_EQ(validateSet(dup), SituationError::DuplicateName);  // #153580
    dup.situations.back().name.clear();
    EXPECT_EQ(validateSet(dup), SituationError::EmptyName);      // #153581
}

TEST(UserSituations, CategoriesGrowShrinkAndRemap) {
    UserSituationSet set = sampleSet();
    set.situations[0].imposed[1].unfavourable = 3.0;
    set.addCategory("C");
    ASSERT_EQ(set.situations[0].imposed.size(), 3u);
    EXPECT_DOUBLE_EQ(set.situations[0].imposed[2].favourable, 0.0);
    set.removeCategory(0);
    ASSERT_EQ(set.categories.size(), 2u);
    EXPECT_DOUBLE_EQ(set.situations[0].imposed[0].unfavourable, 3.0);  // former "B"
    // Remap: a copy of "B" followed by a new one.
    set.remapCategories({"B", "B bis", "nueva"}, {0, 0, -1});
    ASSERT_EQ(set.situations[0].imposed.size(), 3u);
    EXPECT_DOUBLE_EQ(set.situations[0].imposed[1].unfavourable, 3.0);
    EXPECT_DOUBLE_EQ(set.situations[0].imposed[2].unfavourable, 1.0);
}

TEST(UserSituations, FromNormUsesTheCodeTables) {
    const UserSituationSet cte = situationsFromNorm(false, false, "N");
    ASSERT_EQ(cte.situations.size(), 3u);
    EXPECT_EQ(cte.categories.size(), 7u);  // A..E, G1, G2
    const UserSituation& persistent = cte.situations[0];
    EXPECT_DOUBLE_EQ(persistent.permanent.favourable, 0.8);
    EXPECT_DOUBLE_EQ(persistent.permanent.unfavourable, 1.35);
    EXPECT_DOUBLE_EQ(persistent.wind.unfavourable, 1.5);
    EXPECT_DOUBLE_EQ(persistent.wind.principal, 1.0);
    EXPECT_DOUBLE_EQ(persistent.wind.accompanying, 0.6);
    EXPECT_DOUBLE_EQ(persistent.snow.accompanying, 0.5);  // CTE, altitude <= 1000 m
    EXPECT_EQ(cte.situations[1].type, SituationType::Seismic);
    EXPECT_DOUBLE_EQ(cte.situations[1].snow.principal, psiSnow(NormCode::CteDbSe, SnowZone::AtOrBelow1000).psi2);
    // Accidental: CTE principal psi1, EN 1990 principal psi2 (measured, section 12.3).
    EXPECT_DOUBLE_EQ(cte.situations[2].wind.principal, 0.5);
    const UserSituationSet ec = situationsFromNorm(true, true, "E");
    EXPECT_EQ(ec.categories.size(), 8u);  // A..H
    EXPECT_DOUBLE_EQ(ec.situations[0].snow.accompanying, 0.7);
    EXPECT_DOUBLE_EQ(ec.situations[2].wind.principal, 0.0);
    EXPECT_EQ(validateSet(cte), SituationError::None);
    EXPECT_EQ(validateSet(ec), SituationError::None);
}

TEST(UserSituations, TypeCaptionsAreTheFourOfTheCombo) {
    EXPECT_STREQ(situationTypeCaption(SituationType::PersistentOrTransient), "Situación persistente o transitoria");
    EXPECT_STREQ(situationTypeCaption(SituationType::Accidental), "Situación accidental");
    EXPECT_STREQ(situationTypeCaption(SituationType::Seismic), "Situación sísmica");
    EXPECT_STREQ(situationTypeCaption(SituationType::PrestressInVacuum), "Pretensado en vacío");
}

// ----------------------------------------------------------------------------- persistence

TEST(LimitStatesPersistence, SetsAndAssignmentsRoundTrip) {
    Project p;
    p.limitStates.userSets.push_back(sampleSet());
    p.limitStates.setUserSetFor(LimitStateKind::Aluminium, "Mi conjunto");
    p.limitStates.hotRolledCode = CombinationCode::Mixed;
    p.limitStates.codeChosen = true;
    p.limitStates.hotRolledSnow = "Altitud superior a 1000 m";
    p.standards.foundation = "Criterio del Código Estructural";
    ASSERT_EQ(project_json::validate(p), "");
    Project q;
    for (const char* section : {"standards", "limit_states"}) project_json::loadSection(q, section, project_json::dumpSection(p, section));
    EXPECT_EQ(q.limitStates.userSets, p.limitStates.userSets);
    EXPECT_EQ(q.limitStates.userSetFor(LimitStateKind::Aluminium), "Mi conjunto");
    EXPECT_EQ(q.limitStates.userSetFor(LimitStateKind::Concrete), "");
    EXPECT_EQ(q.limitStates.hotRolledCode, CombinationCode::Mixed);
    EXPECT_TRUE(q.limitStates.codeChosen);
    EXPECT_EQ(q.standards.foundation, "Criterio del Código Estructural");
}

TEST(LimitStatesPersistence, OldFilesWithoutNewFieldsStillOpen) {
    Project q;
    project_json::loadSection(q, "limit_states", R"({"groundSnow":"Altitud superior a 1000 m"})");
    EXPECT_TRUE(q.limitStates.userSets.empty());
    EXPECT_EQ(q.limitStates.userSetFor(LimitStateKind::Ground), "");
    EXPECT_EQ(q.limitStates.groundSnow, "Altitud superior a 1000 m");
    EXPECT_EQ(project_json::validate(q), "");
}

TEST(LimitStatesPersistence, RejectsAssignmentToMissingSetAndBadSets) {
    Project p;
    p.limitStates.setUserSetFor(LimitStateKind::Concrete, "no existe");
    EXPECT_FALSE(p.limitStates.isValid());
    Project q;
    q.limitStates.userSets.push_back(sampleSet());
    q.limitStates.userSets.back().situations.front().permanent.favourable = 5.0;  // above the unfavourable one
    EXPECT_FALSE(q.limitStates.isValid());
}

TEST(LimitStatesPersistence, SetInUseBlocksDeletion) {
    LimitStatesSettings s;
    s.userSets.push_back(sampleSet());
    EXPECT_FALSE(s.setInUse("Mi conjunto"));
    s.setUserSetFor(LimitStateKind::Timber, "Mi conjunto");
    EXPECT_TRUE(s.setInUse("Mi conjunto"));
    EXPECT_EQ(s.rows(StandardsSelection{})[4].value, "Mi conjunto");  // the button shows the set
}

// ----------------------------------------------------------------------------- layout of "Estados límite"

TEST(LimitStateLayout, DefaultStandardsGiveTheMeasuredBlocks) {
    const LimitStateLayout l = buildLimitStateLayout(StandardsSelection{});
    ASSERT_EQ(l.blocks.size(), 3u);
    EXPECT_EQ(l.blocks[0].cls, NormClass::Eurocode);
    EXPECT_EQ(l.blocks[0].kinds, (std::vector<LimitStateKind>{LimitStateKind::Concrete, LimitStateKind::Foundation, LimitStateKind::Aluminium}));
    EXPECT_TRUE(l.blocks[0].hasGround);
    EXPECT_EQ(l.blocks[1].cls, NormClass::Cte);
    EXPECT_EQ(l.blocks[1].kinds, (std::vector<LimitStateKind>{LimitStateKind::ColdFormed, LimitStateKind::Timber}));
    EXPECT_EQ(l.blocks[2].cls, NormClass::StructuralCode);
    EXPECT_TRUE(l.blocks[2].hasRadios);
    EXPECT_FALSE(l.ec3Snow);
    EXPECT_FALSE(l.groundStandalone);
}

TEST(LimitStateLayout, AllEurocodesOneBlockAndThreeSnowZones) {
    StandardsSelection s;
    s.concrete = "Eurocódigo 2 (UE)";
    s.coldFormedSteel = "Eurocódigos 3 y 4 (UE)";
    s.hotRolledSteel = "Eurocódigos 3 y 4 (UE)";
    s.timber = "Eurocódigo 5 (UE)";
    const LimitStateLayout l = buildLimitStateLayout(s);
    ASSERT_EQ(l.blocks.size(), 1u);
    EXPECT_EQ(l.blocks[0].kinds.size(), 6u);
    EXPECT_TRUE(l.blocks[0].hasGround);
    EXPECT_TRUE(l.ec3Snow);
    EXPECT_EQ(LimitStatesSettings::snowChoicesEc().size(), 3u);
}

TEST(LimitStateLayout, CteButtonSet) {
    const StandardsSelection s = StandardsSelection::cteSet();
    const LimitStateLayout l = buildLimitStateLayout(s);
    ASSERT_EQ(l.blocks.size(), 3u);
    EXPECT_EQ(l.blocks[0].cls, NormClass::StructuralCode);  // concrete: radios CTE | EC | Hormigón: EC, Acero: CTE
    EXPECT_EQ(l.blocks[0].kinds, (std::vector<LimitStateKind>{LimitStateKind::Concrete, LimitStateKind::Foundation}));
    EXPECT_EQ(l.blocks[1].kinds, (std::vector<LimitStateKind>{LimitStateKind::ColdFormed, LimitStateKind::HotRolled, LimitStateKind::Timber}));
    EXPECT_EQ(l.blocks[2].kinds, (std::vector<LimitStateKind>{LimitStateKind::Aluminium}));
    EXPECT_TRUE(l.groundStandalone);  // "Acciones características"
    // The third radio is marked by default with a structural-code concrete (measured).
    EXPECT_EQ(LimitStatesSettings{}.effectiveCode(s), CombinationCode::Mixed);
}

TEST(LimitStateLayout, StructuralCodeButtonSetGroupsConcreteAndHotRolled) {
    const StandardsSelection s = StandardsSelection::structuralCodeSet();
    const LimitStateLayout l = buildLimitStateLayout(s);
    ASSERT_GE(l.blocks.size(), 2u);
    EXPECT_EQ(l.blocks[0].kinds, (std::vector<LimitStateKind>{LimitStateKind::Concrete, LimitStateKind::Foundation, LimitStateKind::HotRolled}));
}

TEST(LimitStateLayout, ButtonSetsMatchTheMeasuredOnes) {
    const StandardsSelection cte = StandardsSelection::cteSet();
    EXPECT_EQ(cte.concrete, "Código Estructural (España)");
    EXPECT_EQ(cte.coldFormedSteel, "CTE DB SE-A (España)");
    EXPECT_EQ(cte.hotRolledSteel, "CTE DB SE-A (España)");
    EXPECT_EQ(cte.timber, "CTE DB SE-M (España)");
    EXPECT_EQ(cte.aluminium, "Eurocódigo 9");
    EXPECT_EQ(cte.foundation, "Criterio del CTE DB-SE-C");
    const StandardsSelection ce = StandardsSelection::structuralCodeSet();
    EXPECT_EQ(ce.coldFormedSteel, "Eurocódigos 3 y 4 (España)");
    EXPECT_EQ(ce.hotRolledSteel, "Código Estructural (España)");
    EXPECT_EQ(ce.timber, "CTE DB SE-M (España)");
    EXPECT_EQ(ce.foundation, "Criterio del Código Estructural");
    EXPECT_TRUE(cte.isValid());
    EXPECT_TRUE(ce.isValid());
}

TEST(LimitStateLayout, SnowListsAreNormalisedWhenTheListChanges) {
    EXPECT_EQ(LimitStatesSettings::normalisedSnow("Altitud superior a 1000 m", true), "Resto de los Estados miembro del CEN, H > 1000 m");
    EXPECT_EQ(LimitStatesSettings::normalisedSnow("Altitud inferior o igual a 1000 m", true), "Resto de los Estados miembro del CEN, H <= 1000 m");
    EXPECT_EQ(LimitStatesSettings::normalisedSnow("Resto de los Estados miembro del CEN, H <= 1000 m", false), "Altitud inferior o igual a 1000 m");
    EXPECT_EQ(LimitStatesSettings::normalisedSnow("Finlandia, Islandia, Noruega, Suecia", false), "Altitud superior a 1000 m");
    EXPECT_EQ(LimitStatesSettings::normalisedSnow("Altitud superior a 1000 m", false), "Altitud superior a 1000 m");
}

TEST(LimitStateLayout, RowsFollowTheBlocks) {
    const StandardsSelection ce = StandardsSelection::structuralCodeSet();
    LimitStatesSettings s;
    s.hotRolledSnow = "Altitud superior a 1000 m";
    s.hotRolledCode = CombinationCode::EC;
    s.codeChosen = true;
    const auto rows = s.rows(ce);
    EXPECT_EQ(rows[0].value, "EC - Altitud superior a 1000 m");   // concrete follows the radio
    EXPECT_EQ(rows[3].value, "EC - Altitud superior a 1000 m");   // hot-rolled steel, Código Estructural
    EXPECT_EQ(rows[6].value, "Acciones características");         // ground stresses: concrete is not a Eurocode
    EXPECT_EQ(rows[7].value, "Acciones características");
    s.hotRolledCode = CombinationCode::Mixed;
    EXPECT_EQ(s.rows(ce)[0].value, "EC - Altitud superior a 1000 m");  // Hormigón: EC
    EXPECT_EQ(s.rows(ce)[3].value, "CTE - Altitud superior a 1000 m"); // Acero: CTE
}

TEST(LimitStateLayout, CombinationOptionsReadTheBlockSnowAndRadio) {
    Project p;
    p.standards = StandardsSelection::structuralCodeSet();
    p.limitStates.hotRolledSnow = "Altitud superior a 1000 m";
    p.limitStates.hotRolledCode = CombinationCode::EC;
    p.limitStates.codeChosen = true;
    const CombinationOptions steel = combinationOptionsFor(p, StandardFamily::HotRolledSteel);
    EXPECT_EQ(steel.code, NormCode::En1990CaseB);
    EXPECT_EQ(steel.snow, SnowZone::Above1000);
    p.limitStates.hotRolledCode = CombinationCode::Mixed;
    EXPECT_EQ(combinationOptionsFor(p, StandardFamily::HotRolledSteel).code, NormCode::CteDbSe);
    // Eurocode block: the 3-entry EN 1990 list.
    Project q;
    q.standards.concrete = "Eurocódigo 2 (UE)";
    q.standards.coldFormedSteel = "Eurocódigos 3 y 4 (UE)";
    q.standards.hotRolledSteel = "Eurocódigos 3 y 4 (UE)";
    q.standards.timber = "Eurocódigo 5 (UE)";
    q.limitStates.groundSnow = "Resto de los Estados miembro del CEN, H > 1000 m";
    EXPECT_EQ(combinationOptionsFor(q, StandardFamily::Concrete).snow, SnowZone::Above1000);
    q.limitStates.groundSnow = "Finlandia, Islandia, Noruega, Suecia";
    EXPECT_EQ(combinationOptionsFor(q, StandardFamily::Aluminium).snow, SnowZone::Nordic);
}

TEST(UserSituations, ListingOfASetHasOneTablePerSituation) {
    UserSituationSet set = sampleSet();
    const CombinationReport both = buildUserSetReport(set);
    ASSERT_EQ(both.sections.size(), 1u);
    EXPECT_EQ(both.sections[0].layout, SectionLayout::Ultimate);
    ASSERT_EQ(both.sections[0].tables.size(), 2u);  // seismic + persistent situations of sampleSet
    const CoefficientTable& t = both.sections[0].tables[0];
    EXPECT_EQ(t.rows.size(), 9u);  // permanent, prestress, shrinkage, 2 imposed, wind, snow, temperature, earth
    EXPECT_EQ(t.rows[0].label, "Carga permanente (G)");
    EXPECT_FALSE(t.rows[0].principal.present);  // "-" in the permanent rows
    EXPECT_TRUE(t.rows[5].principal.present);   // wind
    EXPECT_DOUBLE_EQ(t.rows[5].accompanying.value, 0.6);
    set.mode = CoefficientMode::PartialFactors;
    EXPECT_EQ(buildUserSetReport(set).sections[0].layout, SectionLayout::Displacements);
    EXPECT_FALSE(exportText(both, 120).empty());  // the viewer / exporters accept it
}


// ----------------------------------------------------------------------------- T-P7-06 session 2 (research 14)

TEST(UserSituations, SeismicRowDefaultsAndFractionRange) {
    UserSituation s = UserSituation::withDefaults(0);
    EXPECT_EQ(s.seismic, (SituationRow{-1.0, 1.0, 1.0, 0.0}));  // Sismo (E): -1.000 | 1.000 | principal 1.000 | acompañamiento 0.000
    s.type = SituationType::Seismic;
    s.seismic.accompanying = 0.3;  // fraction 0..1: 0.3 accepted
    EXPECT_EQ(validateSituation(s, CoefficientMode::PartialAndCombination), SituationError::None);
    s.seismic.accompanying = 30.0;
    EXPECT_EQ(validateSituation(s, CoefficientMode::PartialFactors), SituationError::OutOfRange);
    s.seismic.accompanying = 2.0;
    EXPECT_EQ(validateSituation(s, CoefficientMode::PartialFactors), SituationError::OutOfRange);
    s.seismic.accompanying = 1.0;
    EXPECT_EQ(validateSituation(s, CoefficientMode::PartialFactors), SituationError::None);
    // The row only counts in a seismic situation.
    s.type = SituationType::PersistentOrTransient;
    s.seismic.accompanying = 30.0;
    EXPECT_EQ(validateSituation(s, CoefficientMode::PartialFactors), SituationError::None);
    // The seismic row is not part of the grid rows of the other actions.
    EXPECT_EQ(UserSituation::withDefaults(2).rows().size(), 9u);
}

TEST(UserSituations, SetChecksFollowTheMeasuredOrder) {
    UserSituationSet set = sampleSet();  // situations[0] seismic, situations[1] persistent
    SetChecks work;
    work.projectHasSeismic = true;
    EXPECT_EQ(validateSet(set, {}, work), SituationError::None);
    // reference empty -> empty list -> no persistent -> no seismic.
    UserSituationSet bad = set;
    bad.reference.clear();
    bad.situations.clear();
    EXPECT_EQ(validateSet(bad, {}, work), SituationError::EmptyReference);
    bad.reference = "R";
    EXPECT_EQ(validateSet(bad, {}, work), SituationError::NoSituations);
    bad.situations = {set.situations[0]};  // only the seismic one
    EXPECT_EQ(validateSet(bad, {}, work), SituationError::NoPersistentSituation);
    bad.situations = {set.situations[1]};  // only the persistent one
    EXPECT_EQ(validateSet(bad, {}, work), SituationError::NoSeismicSituation);
    EXPECT_EQ(validateSet(bad), SituationError::None);  // a work without seismic cases does not need it
    SetChecks lenient;
    lenient.structure = false;
    bad.situations = {set.situations[0]};
    EXPECT_EQ(validateSet(bad, {}, lenient), SituationError::None);
    EXPECT_STREQ(situationErrorMessage(SituationError::NoSeismicSituation), "Se debe definir, al menos, una situación con sismo.");
    EXPECT_STREQ(situationErrorMessage(SituationError::NoPersistentSituation),
                 "Se debe definir, al menos, una situación persistente o transitoria.");
}

TEST(UserSituations, MessagesAndSeverityOfResearch14) {
    EXPECT_STREQ(situationErrorMessage(SituationError::EmptyReference), "Debe introducir una referencia");
    EXPECT_STREQ(situationErrorMessage(SituationError::NoSituations), "La lista no puede quedar vacía.");
    EXPECT_STREQ(situationErrorMessage(SituationError::EmptyName), "'Nombre de la situación' no puede quedar en blanco.");
    EXPECT_STREQ(situationErrorMessage(SituationError::DuplicateName), "La referencia está repetida.");
    EXPECT_STREQ(situationErrorMessage(SituationError::NoSetSelected), "Debe introducir, al menos, una situación de proyecto.");
    EXPECT_TRUE(situationErrorIsError(SituationError::EmptyReference));
    EXPECT_TRUE(situationErrorIsError(SituationError::NoSetSelected));  // #156481 is an Error, not an Aviso
    EXPECT_FALSE(situationErrorIsError(SituationError::NoSituations));
    EXPECT_EQ(repeatedReferenceMessage("CA"), "La referencia 'CA' aparece repetida en la caja de selección.\nDebe renombrarla o borrarla.");
}

TEST(LimitStates, ServiceSetsAreASeparateLibraryAndRepeatedReferencesOnlyFailAtAccept) {
    LimitStatesSettings s;
    UserSituationSet elu = sampleSet();
    elu.reference = "CA";
    UserSituationSet els;  // E.L.S. set: Service situations, partial factors only, no type combo
    els.reference = "CA";  // an E.L.U. and an E.L.S. set may share a reference
    els.service = true;
    UserSituation q = UserSituation::withDefaults(0);
    q.name = "Cuasipermanente";
    q.type = SituationType::Service;
    els.situations.push_back(q);
    s.userSets = {elu, els};
    EXPECT_EQ(s.repeatedReference(false), "");
    EXPECT_EQ(s.repeatedReference(true), "");
    ASSERT_NE(s.findSet("CA", true), nullptr);
    EXPECT_TRUE(s.findSet("CA", true)->service);
    EXPECT_FALSE(s.findSet("CA", false)->service);
    EXPECT_EQ(validateSet(els), SituationError::None);  // no persistent requirement in the E.L.S. library
    EXPECT_STREQ(situationTypeCaption(SituationType::Service), "");
    // Rows: only concrete and foundation have the second row.
    EXPECT_TRUE(LimitStatesSettings::hasServiceRow(LimitStateKind::Concrete));
    EXPECT_TRUE(LimitStatesSettings::hasServiceRow(LimitStateKind::Foundation));
    EXPECT_FALSE(LimitStatesSettings::hasServiceRow(LimitStateKind::Timber));
    s.setUserSetFor(LimitStateKind::Concrete, "CA");
    s.setUserServiceSetFor(LimitStateKind::Concrete, "CA");
    EXPECT_TRUE(s.setInUse("CA", false));
    EXPECT_TRUE(s.setInUse("CA", true));
    EXPECT_TRUE(s.isValid());
    // A second "CA" in the E.L.U. library: still a legal stored work, but reported when accepting the dialog (#94676).
    s.userSets.push_back(elu);
    EXPECT_TRUE(s.isValid());
    EXPECT_EQ(s.repeatedReference(false), "CA");
    EXPECT_EQ(s.repeatedReference(true), "");
    // An E.L.S. assignment of a kind without that row is invalid.
    s.setUserServiceSetFor(LimitStateKind::Timber, "CA");
    EXPECT_FALSE(s.isValid());
}

TEST(LimitStatesPersistence, ServiceSetsSeismicRowRoundTripAndOldFilesOpen) {
    Project p;
    UserSituationSet set = sampleSet();
    set.situations[0].seismic.accompanying = 0.3;
    UserSituationSet els;
    els.reference = "F";
    els.service = true;
    UserSituation q = UserSituation::withDefaults(0);
    q.name = "C";
    q.type = SituationType::Service;
    els.situations.push_back(q);
    p.limitStates.userSets = {set, els};
    p.limitStates.setUserServiceSetFor(LimitStateKind::Foundation, "F");
    ASSERT_EQ(project_json::validate(p), "");
    Project r;
    project_json::loadSection(r, "limit_states", project_json::dumpSection(p, "limit_states"));
    EXPECT_EQ(r.limitStates.userSets, p.limitStates.userSets);
    EXPECT_DOUBLE_EQ(r.limitStates.userSets[0].situations[0].seismic.accompanying, 0.3);
    EXPECT_EQ(r.limitStates.userServiceSetFor(LimitStateKind::Foundation), "F");
    // A file written before these fields (no seismic row, no service flag, no ELS assignments) opens with the defaults.
    Project q2;
    project_json::loadSection(
        q2, "limit_states",
        R"({"userSets":[{"reference":"Old","mode":"partialFactors","categories":[],"situations":[{"name":"P","type":"persistent"}]}]})");
    ASSERT_EQ(q2.limitStates.userSets.size(), 1u);
    EXPECT_FALSE(q2.limitStates.userSets[0].service);
    EXPECT_EQ(q2.limitStates.userSets[0].situations[0].seismic, (SituationRow{-1.0, 1.0, 1.0, 0.0}));
    EXPECT_TRUE(q2.limitStates.isValid());
}

TEST(LimitStates, DefaultSnowOfAnAllEurocodeWorkIsHLessOrEqual1000) {
    // Measured in the original (research 14.5): all-EC norms -> "Resto de los Estados miembro del CEN, H <= 1000 m".
    const std::string& ec = LimitStatesSettings::snowChoicesEc().back();
    EXPECT_EQ(ec, "Resto de los Estados miembro del CEN, H <= 1000 m");
    EXPECT_EQ(LimitStatesSettings::normalisedSnow(LimitStatesSettings::defaultSnow(), true), ec);
    StandardsSelection s;
    s.concrete = "Eurocódigo 2 (UE)";
    s.coldFormedSteel = s.hotRolledSteel = "Eurocódigos 3 y 4 (UE)";
    s.timber = "Eurocódigo 5 (UE)";
    EXPECT_EQ(LimitStatesSettings{}.effective(LimitStateKind::Concrete, s).snow, ec);
    EXPECT_EQ(StandardsSelection::foundationCriteria().size(), 2u);  // "Cimentación" combo: 2 entries
}
