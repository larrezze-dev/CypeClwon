#include <gtest/gtest.h>

#include "core/load_case_edit.h"
#include "core/project_json.h"

using namespace cc::core;

namespace {

NewLoadCase make(LoadNature n, const std::string& ref) {
    NewLoadCase d;
    d.nature = n;
    d.reference = ref;
    return d;
}

const std::array<std::string, kUseCategoryNormGroups> kChoices{"A. Domésticos y residenciales", "A. Zonas residenciales"};

}  // namespace

TEST(LoadCase, NewSetHasOnlySelfWeight) {
    const Project p;
    ASSERT_EQ(p.loadCases.cases.size(), 1u);
    EXPECT_EQ(p.loadCases.cases[0].id, 0);
    EXPECT_EQ(p.loadCases.cases[0].reference, "Peso propio");
    EXPECT_TRUE(p.loadCases.cases[0].acts);
    EXPECT_TRUE(validateLoadCases(p.loadCases).empty());
}

TEST(LoadCase, NatureNamesAndPrefixes) {
    EXPECT_EQ(loadNatureName(LoadNature::EarthPressure), "Empujes del terreno");
    EXPECT_EQ(loadNatureName(LoadNature::Shrinkage), "Retracción");
    const char* prefixes[] = {"", "CM", "Q", "T", "R", "V", "S", "N", "H", "A"};
    for (int i = 0; i < kLoadNatureCount; ++i) EXPECT_EQ(loadNaturePrefix(static_cast<LoadNature>(i)), prefixes[i]);
    EXPECT_FALSE(loadNatureAcceptsAdditional(LoadNature::SelfWeight));
}

TEST(LoadCase, ProposedReferenceFollowsCounterAndIsNotReused) {
    Project p;
    UndoStack stack;
    EXPECT_EQ(proposedLoadReference(p.loadCases, LoadNature::DeadLoad), "CM 1");
    EXPECT_EQ(proposedLoadReference(p.loadCases, LoadNature::Wind), "V 1");
    int id = -1;
    ASSERT_EQ(createLoadCase(p, stack, make(LoadNature::DeadLoad, "CM 1"), &id), LoadCaseError::None);
    EXPECT_EQ(id, 1);
    EXPECT_EQ(proposedLoadReference(p.loadCases, LoadNature::DeadLoad), "CM 2");
    EXPECT_EQ(proposedLoadReference(p.loadCases, LoadNature::Wind), "V 1");
    ASSERT_EQ(deleteLoadCase(p, stack, id), LoadCaseError::None);
    EXPECT_EQ(proposedLoadReference(p.loadCases, LoadNature::DeadLoad), "CM 1");  // measured: the lowest free number is proposed again
    ASSERT_EQ(createLoadCase(p, stack, make(LoadNature::DeadLoad, "CM 1"), &id), LoadCaseError::None);
    EXPECT_EQ(id, 2);  // ids are stable and not reused
    int second = 0;
    ASSERT_EQ(createLoadCase(p, stack, make(LoadNature::DeadLoad, "CM 2"), &second), LoadCaseError::None);
    ASSERT_EQ(deleteLoadCase(p, stack, id), LoadCaseError::None);
    EXPECT_EQ(proposedLoadReference(p.loadCases, LoadNature::DeadLoad), "CM 1");  // measured: CM 2 left after deleting CM 1 -> "CM 1"
}

TEST(LoadCase, ListOrderPutsAccidentalBeforeEarthPressure) {
    Project p;
    UndoStack stack;
    for (LoadNature n : {LoadNature::EarthPressure, LoadNature::Accidental, LoadNature::Snow, LoadNature::Seismic, LoadNature::Temperature})
        ASSERT_EQ(createLoadCase(p, stack, make(n, proposedLoadReference(p.loadCases, n)), nullptr), LoadCaseError::None);
    std::string order;
    for (const LoadCase* c : loadCasesInListOrder(p.loadCases)) order += (c->id == 0 ? std::string("PP") : c->reference) + ";";
    EXPECT_EQ(order, "PP;T 1;S 1;N 1;A 1;H 1;") << "measured: Peso propio first, then T, S, N, A, H";
}

TEST(LoadCase, BlankReferenceIsRefused) {
    Project p;
    UndoStack stack;
    EXPECT_EQ(createLoadCase(p, stack, make(LoadNature::Snow, ""), nullptr), LoadCaseError::BlankReference);
    EXPECT_EQ(loadCaseErrorMessage(LoadCaseError::BlankReference), "'Referencia' no puede quedar en blanco.");
    EXPECT_EQ(p.loadCases.cases.size(), 1u);
    EXPECT_FALSE(stack.canUndo());
}

TEST(LoadCase, SelfWeightIsProtected) {
    Project p;
    UndoStack stack;
    EXPECT_EQ(createLoadCase(p, stack, make(LoadNature::SelfWeight, "X"), nullptr), LoadCaseError::ProtectedCase);
    EXPECT_EQ(editLoadCase(p, stack, 0, "X", ""), LoadCaseError::ProtectedCase);
    EXPECT_EQ(toggleLoadCaseActs(p, stack, 0), LoadCaseError::ProtectedCase);
    EXPECT_EQ(deleteLoadCase(p, stack, 0), LoadCaseError::ProtectedCase);
    EXPECT_EQ(deleteLoadCase(p, stack, 77), LoadCaseError::UnknownCase);
    EXPECT_FALSE(stack.canUndo());
}

TEST(LoadCase, LiveLoadNeedsUseCategories) {
    Project p;
    UndoStack stack;
    EXPECT_EQ(createLoadCase(p, stack, make(LoadNature::LiveLoad, "Q 1"), nullptr), LoadCaseError::NeedsUseCategories);
    EXPECT_EQ(loadCaseErrorMessage(LoadCaseError::NeedsUseCategories),
              "Para poder definir las hipótesis de sobrecarga de uso, primero se deben definir las categorías de uso de la obra.");
    ASSERT_EQ(addUseCategory(p, stack, kChoices), LoadCaseError::None);
    EXPECT_EQ(createLoadCase(p, stack, make(LoadNature::LiveLoad, "Q 1"), nullptr), LoadCaseError::None);
    EXPECT_EQ(loadCasesOfNature(p.loadCases, LoadNature::LiveLoad).size(), 1u);
}

TEST(LoadCase, ArrangementsDefaultsAndEditing) {
    Project p;
    UndoStack stack;
    NewLoadCase d = make(LoadNature::Wind, "V 1");
    d.withArrangements = true;
    int id = 0;
    ASSERT_EQ(createLoadCase(p, stack, d, &id), LoadCaseError::None);
    const LoadCase* c = findLoadCase(p.loadCases, id);
    ASSERT_TRUE(c);
    EXPECT_EQ(c->arrangements, (std::vector<std::string>{"1", "2"}));
    EXPECT_EQ(c->compat, ArrangementCompat::Simultaneous);
    std::string name;
    ASSERT_EQ(addLoadArrangement(p, stack, id, &name), LoadCaseError::None);
    EXPECT_EQ(name, "3");
    ASSERT_EQ(deleteLoadArrangement(p, stack, id, 0), LoadCaseError::None);
    ASSERT_EQ(addLoadArrangement(p, stack, id, &name), LoadCaseError::None);
    EXPECT_EQ(name, "4");
    EXPECT_EQ(findLoadCase(p.loadCases, id)->arrangements, (std::vector<std::string>{"2", "3", "4"}));
    EXPECT_EQ(deleteLoadArrangement(p, stack, id, 9), LoadCaseError::InvalidArrangement);
    // A case without arrangements has nothing to add to.
    int plain = 0;
    ASSERT_EQ(createLoadCase(p, stack, make(LoadNature::Snow, "N 1"), &plain), LoadCaseError::None);
    EXPECT_EQ(addLoadArrangement(p, stack, plain, nullptr), LoadCaseError::InvalidArrangement);
    EXPECT_FALSE(findLoadCase(p.loadCases, plain)->hasArrangements());
    // Incompatible chosen at creation.
    NewLoadCase e = make(LoadNature::Wind, "V 2");
    e.withArrangements = true;
    e.compat = ArrangementCompat::Incompatible;
    ASSERT_EQ(createLoadCase(p, stack, e, &id), LoadCaseError::None);
    EXPECT_EQ(findLoadCase(p.loadCases, id)->compat, ArrangementCompat::Incompatible);
}

TEST(LoadCase, SeismicTypeAndNoArrangements) {
    Project p;
    UndoStack stack;
    NewLoadCase d = make(LoadNature::Seismic, "S 1");
    d.withArrangements = true;
    EXPECT_EQ(createLoadCase(p, stack, d, nullptr), LoadCaseError::InvalidArrangement);
    d.withArrangements = false;
    d.seismic = SeismicType::XY;
    int id = 0;
    ASSERT_EQ(createLoadCase(p, stack, d, &id), LoadCaseError::None);
    EXPECT_EQ(findLoadCase(p.loadCases, id)->seismic, SeismicType::XY);
    ASSERT_EQ(setSeismicSecondOrder(p, stack, true), LoadCaseError::None);
    EXPECT_TRUE(p.loadCases.seismicSecondOrder);
    EXPECT_EQ(setSeismicSecondOrder(p, stack, true), LoadCaseError::NothingToChange);
}

TEST(LoadCase, EditAndToggleActs) {
    Project p;
    UndoStack stack;
    int id = 0;
    ASSERT_EQ(createLoadCase(p, stack, make(LoadNature::Temperature, "T 1"), &id), LoadCaseError::None);
    EXPECT_EQ(editLoadCase(p, stack, id, "", "x"), LoadCaseError::BlankReference);
    EXPECT_EQ(editLoadCase(p, stack, id, "T 1", ""), LoadCaseError::NothingToChange);
    ASSERT_EQ(editLoadCase(p, stack, id, "Verano", "Variación +20 ºC"), LoadCaseError::None);
    EXPECT_EQ(findLoadCase(p.loadCases, id)->reference, "Verano");
    EXPECT_EQ(findLoadCase(p.loadCases, id)->description, "Variación +20 ºC");
    EXPECT_EQ(loadCaseLabel(*findLoadCase(p.loadCases, id)), "Verano");
    EXPECT_TRUE(findLoadCase(p.loadCases, id)->acts);
    ASSERT_EQ(toggleLoadCaseActs(p, stack, id), LoadCaseError::None);
    EXPECT_FALSE(findLoadCase(p.loadCases, id)->acts);
    ASSERT_EQ(toggleLoadCaseActs(p, stack, id), LoadCaseError::None);
    EXPECT_TRUE(findLoadCase(p.loadCases, id)->acts);
    EXPECT_EQ(editLoadCase(p, stack, 42, "a", "b"), LoadCaseError::UnknownCase);
}

TEST(LoadCase, DeleteHookHasNoDependentsYet) {
    Project p;
    UndoStack stack;
    int id = 0;
    ASSERT_EQ(createLoadCase(p, stack, make(LoadNature::Accidental, "A 1"), &id), LoadCaseError::None);
    EXPECT_TRUE(loadCaseDependents(p, id).empty());
    ASSERT_EQ(deleteLoadCase(p, stack, id), LoadCaseError::None);
    EXPECT_EQ(findLoadCase(p.loadCases, id), nullptr);
}

TEST(LoadCase, UseCategoriesLettersCopyDelete) {
    Project p;
    UndoStack stack;
    ASSERT_EQ(addUseCategory(p, stack, kChoices), LoadCaseError::None);
    ASSERT_EQ(addUseCategory(p, stack, {"B. Zonas administrativas", "B. Zonas administrativas"}), LoadCaseError::None);
    ASSERT_EQ(p.loadCases.useCategories.size(), 2u);
    EXPECT_EQ(p.loadCases.useCategories[0].ref, "A");
    EXPECT_EQ(p.loadCases.useCategories[0].description, "A. Domésticos y residenciales");
    EXPECT_EQ(p.loadCases.useCategories[1].ref, "B");
    ASSERT_EQ(copyUseCategory(p, stack, 0), LoadCaseError::None);
    EXPECT_EQ(p.loadCases.useCategories[2].ref, "C");
    EXPECT_EQ(p.loadCases.useCategories[2].choices, p.loadCases.useCategories[0].choices);
    ASSERT_EQ(deleteUseCategory(p, stack, 0), LoadCaseError::None);
    EXPECT_EQ(nextUseCategoryRef(p.loadCases), "A");  // first unused letter
    EXPECT_EQ(deleteUseCategory(p, stack, 9), LoadCaseError::UnknownCategory);
    EXPECT_EQ(copyUseCategory(p, stack, 9), LoadCaseError::UnknownCategory);
}

TEST(LoadCase, UndoRedoRestoresExactly) {
    Project p;
    UndoStack stack;
    const LoadCaseSet initial = p.loadCases;
    int id = 0;
    ASSERT_EQ(addUseCategory(p, stack, kChoices), LoadCaseError::None);
    ASSERT_EQ(createLoadCase(p, stack, make(LoadNature::LiveLoad, "Q 1"), &id), LoadCaseError::None);
    ASSERT_EQ(toggleLoadCaseActs(p, stack, id), LoadCaseError::None);
    ASSERT_EQ(deleteLoadCase(p, stack, id), LoadCaseError::None);
    const LoadCaseSet end = p.loadCases;
    stack.undo();
    EXPECT_NE(findLoadCase(p.loadCases, id), nullptr);
    EXPECT_FALSE(findLoadCase(p.loadCases, id)->acts);
    stack.undo();
    EXPECT_TRUE(findLoadCase(p.loadCases, id)->acts);
    stack.undo();
    EXPECT_EQ(findLoadCase(p.loadCases, id), nullptr);
    EXPECT_EQ(p.loadCases.created[static_cast<std::size_t>(LoadNature::LiveLoad)], 0);
    stack.undo();
    EXPECT_EQ(p.loadCases, initial);
    while (stack.canRedo()) stack.redo();
    EXPECT_EQ(p.loadCases, end);
}

TEST(LoadCase, JsonRoundTrip) {
    Project p;
    UndoStack stack;
    int id = 0;
    ASSERT_EQ(addUseCategory(p, stack, kChoices), LoadCaseError::None);
    ASSERT_EQ(createLoadCase(p, stack, make(LoadNature::LiveLoad, "Q 1"), &id), LoadCaseError::None);
    NewLoadCase w = make(LoadNature::Wind, "V 1");
    w.description = "Dirección X";
    w.withArrangements = true;
    w.compat = ArrangementCompat::Incompatible;
    ASSERT_EQ(createLoadCase(p, stack, w, &id), LoadCaseError::None);
    ASSERT_EQ(toggleLoadCaseActs(p, stack, id), LoadCaseError::None);
    NewLoadCase s = make(LoadNature::Seismic, "S 1");
    s.seismic = SeismicType::XY;
    ASSERT_EQ(createLoadCase(p, stack, s, &id), LoadCaseError::None);
    ASSERT_EQ(setSeismicSecondOrder(p, stack, true), LoadCaseError::None);
    ASSERT_EQ(createLoadCase(p, stack, make(LoadNature::Snow, "N 1"), &id), LoadCaseError::None);
    ASSERT_EQ(deleteLoadCase(p, stack, id), LoadCaseError::None);  // counter of Snow stays at 1
    ASSERT_TRUE(project_json::validate(p).empty());
    ASSERT_TRUE(project_json::isKnownSection("load_cases"));
    EXPECT_EQ(project_json::sectionTitle("load_cases"), "Hipótesis de carga");
    const std::string text = project_json::dumpSection(p, "load_cases");
    EXPECT_NE(text.find("\"version\": 1"), std::string::npos);
    Project q;
    project_json::loadSection(q, "load_cases", text);
    EXPECT_EQ(q.loadCases, p.loadCases);
    EXPECT_EQ(proposedLoadReference(q.loadCases, LoadNature::Snow), "N 1");  // measured: the lowest free number (N 1 was deleted)
}

TEST(LoadCase, OldFilesWithoutSectionKeepOnlySelfWeight) {
    Project q;
    project_json::loadSection(q, "load_cases", "{}");
    EXPECT_EQ(q.loadCases.cases.size(), 1u);
    EXPECT_EQ(q.loadCases.cases[0].id, 0);
    EXPECT_TRUE(q.loadCases.useCategories.empty());
    EXPECT_TRUE(validateLoadCases(q.loadCases).empty());
}

TEST(LoadCase, ValidateDetectsCorruptData) {
    LoadCaseSet s;
    LoadCase c;
    c.id = 1;
    c.nature = LoadNature::Wind;
    c.reference = "V 1";
    s.cases.push_back(c);
    s.nextId = 2;
    s.created[static_cast<std::size_t>(LoadNature::Wind)] = 1;
    EXPECT_TRUE(validateLoadCases(s).empty());
    LoadCaseSet bad = s;
    bad.cases[1].reference.clear();
    EXPECT_FALSE(validateLoadCases(bad).empty());
    bad = s;
    bad.cases[1].id = 0;
    EXPECT_FALSE(validateLoadCases(bad).empty());
    bad = s;
    bad.created[static_cast<std::size_t>(LoadNature::Wind)] = 0;
    EXPECT_FALSE(validateLoadCases(bad).empty());
    bad = s;
    bad.cases[1].nature = LoadNature::SelfWeight;
    EXPECT_FALSE(validateLoadCases(bad).empty());
}

TEST(LoadCase, ColourPaletteHasPeriodEightFromSelfWeight) {
    const unsigned expected[8] = {0xFFFF00, 0x00BFFF, 0x00FF00, 0xFF00FF, 0xCC6600, 0x0000FF, 0x800080, 0x808080};
    for (int i = 0; i < 8; ++i) EXPECT_EQ(loadCaseColour(i), expected[i]) << i;
    EXPECT_EQ(loadCaseColour(8), 0xFFFF00u);   // measured: the ninth (index 8) is yellow again
    EXPECT_EQ(loadCaseColour(9), 0x00BFFFu);
    EXPECT_EQ(loadCaseColour(17), 0x00BFFFu);
    EXPECT_EQ(loadCaseColour(-3), 0xFFFF00u);

    Project p;
    UndoStack stack;
    std::vector<int> ids;
    for (int i = 1; i <= 8; ++i) {
        int id = 0;
        ASSERT_EQ(createLoadCase(p, stack, make(LoadNature::Wind, "V " + std::to_string(i)), &id), LoadCaseError::None);
        ids.push_back(id);
    }
    EXPECT_EQ(loadCaseColourOf(p.loadCases, 0), 0xFFFF00u);
    for (int i = 0; i < 8; ++i) {
        EXPECT_EQ(loadCaseCreationIndex(p.loadCases, ids[static_cast<std::size_t>(i)]), i + 1);
        EXPECT_EQ(loadCaseColourOf(p.loadCases, ids[static_cast<std::size_t>(i)]), expected[(i + 1) % 8]);
    }
    EXPECT_EQ(loadCaseColourOf(p.loadCases, ids.back()), 0xFFFF00u);   // V 8 = index 8
    EXPECT_EQ(loadCaseColourOf(p.loadCases, 9999), 0xFFFF00u);
}

TEST(LoadCase, ViewStateDefaultsPersistsAndFallsBackOnDelete) {
    Project p;
    UndoStack stack;
    EXPECT_EQ(p.loadCases.viewedCaseId, 0);
    EXPECT_FALSE(p.loadCases.viewAllCases);
    int id = 0;
    ASSERT_EQ(createLoadCase(p, stack, make(LoadNature::Wind, "V 1"), &id), LoadCaseError::None);
    EXPECT_EQ(setLoadView(p, 99, true), LoadCaseError::UnknownCase);
    ASSERT_EQ(setLoadView(p, id, true), LoadCaseError::None);
    const std::string text = project_json::dumpSection(p, "load_cases");
    Project q;
    project_json::loadSection(q, "load_cases", text);
    EXPECT_EQ(q.loadCases.viewedCaseId, id);
    EXPECT_TRUE(q.loadCases.viewAllCases);
    EXPECT_TRUE(validateLoadCases(q.loadCases).empty());
    // tolerant: an unknown viewed case in the file becomes Peso propio; old files keep the defaults
    Project r;
    project_json::loadSection(r, "load_cases", "{\"viewedCaseId\": 42}");
    EXPECT_EQ(r.loadCases.viewedCaseId, 0);
    // deleting the viewed case falls back to Peso propio (undoable together with the deletion)
    ASSERT_EQ(deleteLoadCase(p, stack, id), LoadCaseError::None);
    EXPECT_EQ(p.loadCases.viewedCaseId, 0);
    EXPECT_TRUE(p.loadCases.viewAllCases);
    EXPECT_TRUE(validateLoadCases(p.loadCases).empty());
    stack.undo();
    EXPECT_EQ(p.loadCases.viewedCaseId, id);
}
