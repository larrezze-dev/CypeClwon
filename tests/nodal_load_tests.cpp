#include <gtest/gtest.h>

#include <cmath>

#include "core/load_case_edit.h"
#include "core/nodal_load_edit.h"
#include "core/node_edit.h"
#include "core/node_support_edit.h"
#include "core/project_json.h"

using namespace cc::core;

namespace {

/// Project with three nodes 1, 2, 3 on a line and one additional case "CM 1" (id 1).
Project baseProject() {
    Project p;
    p.structure.addNode({0, 0, 0});
    p.structure.addNode({0, 0, 3});
    p.structure.addNode({4, 0, 3});
    return p;
}

int addCase(Project& p, UndoStack& stack, const std::string& ref = "CM 1") {
    NewLoadCase d;
    d.nature = LoadNature::DeadLoad;
    d.reference = ref;
    int id = 0;
    EXPECT_EQ(createLoadCase(p, stack, d, &id), LoadCaseError::None);
    return id;
}

NodalLoadData data(double kn, LoadDirection dir = LoadDirection::MinusZ, int loadCase = 0) {
    NodalLoadData d;
    d.magnitude = kn;
    d.direction = dir;
    d.loadCaseId = loadCase;
    return d;
}

}  // namespace

TEST(NodalLoad, ValueParsingAndMeasuredMessages) {
    EXPECT_EQ(nodalLoadRangeMessage(), "El valor de 'Valor' debe estar entre 0.01 y 49000.00.");
    EXPECT_EQ(nodalLoadNotNumberMessage(), "La cadena introducida no es un número.");
    for (const char* bad : {"0", "-7.256", "49000.01", "0.004", "0.00"}) {
        const LoadValueParse r = parseLoadValue(bad);
        EXPECT_FALSE(r.ok) << bad;
        EXPECT_EQ(r.message, nodalLoadRangeMessage()) << bad;
    }
    for (const char* bad : {"abc", "", "  ", "1e3", "inf", "nan", "1.2.3", "5 kN", "-", "."}) {
        const LoadValueParse r = parseLoadValue(bad);
        EXPECT_FALSE(r.ok) << bad;
        EXPECT_EQ(r.message, nodalLoadNotNumberMessage()) << bad;
    }
    EXPECT_TRUE(parseLoadValue("0.01").ok);
    EXPECT_TRUE(parseLoadValue("49000.00").ok);
    LoadValueParse r = parseLoadValue(" 7,256 ");  // shown as 7.26
    ASSERT_TRUE(r.ok);
    EXPECT_DOUBLE_EQ(r.value, 7.26);
    EXPECT_DOUBLE_EQ(parseLoadValue("9.81").value, 9.81);
    EXPECT_TRUE(nodalLoadValueInRange(0.006));  // rounds to 0.01
    EXPECT_FALSE(nodalLoadValueInRange(NAN));
}

TEST(NodalLoad, DirectionsAndForce) {
    NodalLoad l;
    l.magnitude = 5.0;
    const Point3 expected[6] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}, {-1, 0, 0}, {0, -1, 0}, {0, 0, -1}};
    for (int i = 0; i < 6; ++i) {
        l.direction = static_cast<LoadDirection>(i);
        EXPECT_EQ(nodalLoadDirection(l), expected[i]) << i;
    }
    EXPECT_EQ(nodalLoadForce(l), (Point3{0, 0, -5}));
    l.direction = LoadDirection::UserVector;
    l.userVector = {3, 0, 4};
    const Point3 f = nodalLoadForce(l);
    EXPECT_NEAR(f.x, 3.0, 1e-12);
    EXPECT_NEAR(f.z, 4.0, 1e-12);
    l.userVector = {0, 0, 0};
    EXPECT_EQ(nodalLoadDirection(l), (Point3{0, 0, 0}));
    EXPECT_EQ(NodalLoadData{}.direction, LoadDirection::MinusZ);
    EXPECT_EQ(NodalLoadData{}.userVector, (Point3{1, 0, 0}));
    EXPECT_DOUBLE_EQ(NodalLoadData{}.magnitude, 9.81);
}

TEST(NodalLoad, CheckDataRejectsRangeAndNullVector) {
    EXPECT_TRUE(checkNodalLoadData(data(9.81)).empty());
    EXPECT_EQ(checkNodalLoadData(data(0.0)), nodalLoadRangeMessage());
    EXPECT_EQ(checkNodalLoadData(data(49000.01)), nodalLoadRangeMessage());
    NodalLoadData d = data(1.0, LoadDirection::UserVector);
    d.userVector = {0, 0, 0};
    EXPECT_EQ(checkNodalLoadData(d), nodalLoadDirectionMessage());
    d.userVector = {NAN, 0, 1};
    EXPECT_EQ(checkNodalLoadData(d), nodalLoadDirectionMessage());
    d.userVector = {0, 0, -2};
    EXPECT_TRUE(checkNodalLoadData(d).empty());
    // A null vector is irrelevant when the direction is an axis.
    d = data(1.0, LoadDirection::PlusX);
    d.userVector = {0, 0, 0};
    EXPECT_TRUE(checkNodalLoadData(d).empty());
}

TEST(NodalLoad, AddToSeveralNodesNeverMergesAndUndoes) {
    Project p = baseProject();
    UndoStack stack;
    std::vector<int> ids;
    ASSERT_EQ(addNodalLoads(p, stack, {1, 2, 2, 99}, data(5.0, LoadDirection::PlusX), &ids), NodalLoadError::None);
    EXPECT_EQ(ids, (std::vector<int>{1, 2}));  // duplicate and unknown nodes dropped
    ASSERT_EQ(addNodalLoads(p, stack, {1}, data(5.0, LoadDirection::PlusX)), NodalLoadError::None);  // same node, case and value: no fusion
    EXPECT_EQ(p.nodalLoads.items.size(), 3u);
    EXPECT_EQ(nodalLoadsOfNode(p.nodalLoads, 1), (std::vector<int>{1, 3}));
    EXPECT_DOUBLE_EQ(findNodalLoad(p.nodalLoads, 1)->magnitude, 5.0);
    EXPECT_TRUE(hasNodalLoads(p.nodalLoads));

    stack.undo();
    EXPECT_EQ(p.nodalLoads.items.size(), 2u);
    stack.redo();
    EXPECT_EQ(p.nodalLoads.items.size(), 3u);
    stack.undo();
    stack.undo();
    EXPECT_TRUE(p.nodalLoads.items.empty());
    ASSERT_EQ(addNodalLoads(p, stack, {3}, data(1.0)), NodalLoadError::None);
    EXPECT_EQ(p.nodalLoads.items.back().id, 4);  // ids are not reused after undo
}

TEST(NodalLoad, AddChecksInOrderAndStoresRoundedValue) {
    Project p = baseProject();
    UndoStack stack;
    EXPECT_EQ(addNodalLoads(p, stack, {1}, data(0.0)), NodalLoadError::InvalidValue);
    NodalLoadData d = data(1.0, LoadDirection::UserVector);
    d.userVector = {0, 0, 0};
    EXPECT_EQ(addNodalLoads(p, stack, {1}, d), NodalLoadError::InvalidDirection);
    EXPECT_EQ(addNodalLoads(p, stack, {1}, data(1.0, LoadDirection::MinusZ, 42)), NodalLoadError::UnknownLoadCase);
    EXPECT_EQ(addNodalLoads(p, stack, {99}, data(1.0)), NodalLoadError::NoNodes);
    EXPECT_EQ(addNodalLoads(p, stack, {}, data(1.0)), NodalLoadError::NoNodes);
    EXPECT_EQ(stack.count(), 0u);
    ASSERT_EQ(addNodalLoads(p, stack, {1}, data(7.256)), NodalLoadError::None);
    EXPECT_DOUBLE_EQ(p.nodalLoads.items[0].magnitude, 7.26);
    EXPECT_EQ(nodalLoadErrorMessage(NodalLoadError::InvalidValue), nodalLoadRangeMessage());
}

TEST(NodalLoad, MultipleEditOverwritesEverySelectedLoad) {
    Project p = baseProject();
    UndoStack stack;
    const int cm = addCase(p, stack);
    ASSERT_EQ(addNodalLoads(p, stack, {1}, data(3.0, LoadDirection::MinusZ)), NodalLoadError::None);
    ASSERT_EQ(addNodalLoads(p, stack, {1}, data(8.0, LoadDirection::PlusY)), NodalLoadError::None);
    ASSERT_EQ(addNodalLoads(p, stack, {2}, data(1.0)), NodalLoadError::None);
    const std::vector<int> sel = nodalLoadsOfNode(p.nodalLoads, 1);
    NodalLoadData d = data(5.0, LoadDirection::PlusX, cm);
    ASSERT_EQ(editNodalLoads(p, stack, sel, d), NodalLoadError::None);
    for (int id : sel) {
        const NodalLoad* l = findNodalLoad(p.nodalLoads, id);
        EXPECT_DOUBLE_EQ(l->magnitude, 5.0);
        EXPECT_EQ(l->direction, LoadDirection::PlusX);
        EXPECT_EQ(l->loadCaseId, cm);
    }
    EXPECT_DOUBLE_EQ(findNodalLoad(p.nodalLoads, 3)->magnitude, 1.0);  // not selected: untouched
    EXPECT_EQ(editNodalLoads(p, stack, sel, d), NodalLoadError::NothingToChange);
    EXPECT_EQ(editNodalLoads(p, stack, {77}, d), NodalLoadError::UnknownLoad);
    EXPECT_EQ(editNodalLoads(p, stack, sel, data(0.0)), NodalLoadError::InvalidValue);
    stack.undo();
    EXPECT_DOUBLE_EQ(findNodalLoad(p.nodalLoads, 1)->magnitude, 3.0);
    EXPECT_EQ(findNodalLoad(p.nodalLoads, 2)->direction, LoadDirection::PlusY);
}

TEST(NodalLoad, ChangeCaseAndDelete) {
    Project p = baseProject();
    UndoStack stack;
    const int cm = addCase(p, stack);
    ASSERT_EQ(addNodalLoads(p, stack, {1, 2}, data(2.0)), NodalLoadError::None);
    ASSERT_EQ(changeNodalLoadCase(p, stack, {2}, cm), NodalLoadError::None);
    EXPECT_EQ(findNodalLoad(p.nodalLoads, 2)->loadCaseId, cm);
    EXPECT_EQ(findNodalLoad(p.nodalLoads, 1)->loadCaseId, 0);
    EXPECT_EQ(changeNodalLoadCase(p, stack, {2}, cm), NodalLoadError::NothingToChange);
    EXPECT_EQ(changeNodalLoadCase(p, stack, {2}, 55), NodalLoadError::UnknownLoadCase);
    EXPECT_EQ(changeNodalLoadCase(p, stack, {9}, cm), NodalLoadError::UnknownLoad);
    EXPECT_EQ(nodalLoadsOfCase(p.nodalLoads, cm), (std::vector<int>{2}));
    ASSERT_EQ(deleteNodalLoads(p, stack, {1, 2}), NodalLoadError::None);
    EXPECT_FALSE(hasNodalLoads(p.nodalLoads));
    EXPECT_EQ(deleteNodalLoads(p, stack, {1}), NodalLoadError::UnknownLoad);
    stack.undo();
    EXPECT_EQ(p.nodalLoads.items.size(), 2u);
}

TEST(NodalLoad, DrawLengthFollowsTheMeasuredScales) {
    NodalLoad load;
    load.magnitude = 5.0;
    LoadScales s;
    // Fixed mode: the value does not matter (0.5 m -> 50 px, 1.0 m -> 100 px in the original at 100 px/m).
    EXPECT_DOUBLE_EQ(nodalLoadDrawLengthM(s, load), 0.5);
    load.magnitude = 20.0;
    EXPECT_DOUBLE_EQ(nodalLoadDrawLengthM(s, load), 0.5);
    s.fixedSizeM = 1.0;
    EXPECT_DOUBLE_EQ(nodalLoadDrawLengthM(s, load), 1.0);
    // Proportional mode: 0.1 m per kN times the case scale times the "Puntual" scale (5 kN -> 0.5 m, 20 kN -> 2 m).
    s.mode = LoadScaleMode::Proportional;
    load.magnitude = 5.0;
    EXPECT_DOUBLE_EQ(nodalLoadDrawLengthM(s, load), 0.5);
    load.magnitude = 20.0;
    EXPECT_DOUBLE_EQ(nodalLoadDrawLengthM(s, load), 2.0);
    s.perCase[0] = 2.0;
    EXPECT_DOUBLE_EQ(nodalLoadDrawLengthM(s, load), 4.0);
    s.perCase.clear();
    s.perType[static_cast<std::size_t>(LoadScaleType::Punctual)] = 2.0;
    EXPECT_DOUBLE_EQ(nodalLoadDrawLengthM(s, load), 4.0);
    // Defaults of "Escalas de cargas por tipos": 1.000 everywhere, Retracción 100.000.
    const LoadScales d;
    for (int t = 0; t < kLoadScaleTypeCount - 1; ++t) EXPECT_DOUBLE_EQ(d.perType[static_cast<std::size_t>(t)], 1.0);
    EXPECT_DOUBLE_EQ(d.perType[static_cast<std::size_t>(LoadScaleType::Shrinkage)], 100.0);
}

TEST(NodalLoad, TypeScalesAreRangeChecked) {
    Project p = baseProject();
    UndoStack stack;
    for (double bad : {0.0, 0.0009, 1000.001, -1.0}) {
        LoadScales s;
        s.perType[1] = bad;
        EXPECT_EQ(setLoadScales(p, stack, s), NodalLoadError::InvalidScale) << bad;
    }
    LoadScales s;
    s.perType[1] = 1000.0;
    s.perType[2] = 0.001;
    EXPECT_EQ(setLoadScales(p, stack, s), NodalLoadError::None);
}

TEST(NodalLoad, ScalesAreUndoableAndValidated) {
    Project p = baseProject();
    UndoStack stack;
    EXPECT_EQ(p.nodalLoads.scales.mode, LoadScaleMode::Fixed);
    EXPECT_DOUBLE_EQ(p.nodalLoads.scales.fixedSizeM, 0.5);
    EXPECT_DOUBLE_EQ(loadScaleOfCase(p.nodalLoads.scales, 0), 1.0);
    LoadScales s;
    s.mode = LoadScaleMode::Proportional;
    s.perCase[0] = 2.5;
    ASSERT_EQ(setLoadScales(p, stack, s), NodalLoadError::None);
    EXPECT_DOUBLE_EQ(loadScaleOfCase(p.nodalLoads.scales, 0), 2.5);
    EXPECT_EQ(setLoadScales(p, stack, s), NodalLoadError::NothingToChange);
    s.fixedSizeM = 0.0;
    EXPECT_EQ(setLoadScales(p, stack, s), NodalLoadError::InvalidScale);
    s.fixedSizeM = 0.5;
    s.perCase[0] = -1.0;
    EXPECT_EQ(setLoadScales(p, stack, s), NodalLoadError::InvalidScale);
    stack.undo();
    EXPECT_EQ(p.nodalLoads.scales.mode, LoadScaleMode::Fixed);
}

TEST(NodalLoad, DeletingNodesPurgesLoadsAndUndoRestores) {
    Project p = baseProject();
    UndoStack stack;
    ASSERT_EQ(addNodalLoads(p, stack, {1, 2, 3}, data(2.0)), NodalLoadError::None);
    ASSERT_TRUE(deleteNodes(p, stack, {2}));
    EXPECT_TRUE(nodalLoadsOfNode(p.nodalLoads, 2).empty());
    EXPECT_EQ(p.nodalLoads.items.size(), 2u);
    stack.undo();
    EXPECT_EQ(p.nodalLoads.items.size(), 3u);
    EXPECT_EQ(nodalLoadsOfNode(p.nodalLoads, 2).size(), 1u);
    stack.redo();
    EXPECT_EQ(p.nodalLoads.items.size(), 2u);
}

TEST(NodalLoad, MergingNodesMovesLoadsToTheSurvivor) {
    Project p = baseProject();
    UndoStack stack;
    ASSERT_EQ(addNodalLoads(p, stack, {2}, data(2.0)), NodalLoadError::None);
    ASSERT_EQ(addNodalLoads(p, stack, {3}, data(4.0)), NodalLoadError::None);
    ASSERT_TRUE(moveNode(p, stack, 2, p.structure.findNode(3)->position));  // 2 merges into 3
    EXPECT_EQ(p.structure.findNode(2), nullptr);
    EXPECT_EQ(nodalLoadsOfNode(p.nodalLoads, 3).size(), 2u);  // both kept, not added up
    EXPECT_TRUE(nodalLoadsOfNode(p.nodalLoads, 2).empty());
    stack.undo();
    EXPECT_EQ(nodalLoadsOfNode(p.nodalLoads, 2).size(), 1u);
    EXPECT_EQ(nodalLoadsOfNode(p.nodalLoads, 3).size(), 1u);
    stack.redo();
    EXPECT_EQ(nodalLoadsOfNode(p.nodalLoads, 3).size(), 2u);
}

TEST(NodalLoad, DeletingALoadCaseDeletesItsLoadsAndPrescribedInOneCommand) {
    Project p = baseProject();
    UndoStack stack;
    const int cm = addCase(p, stack);
    const int cm2 = addCase(p, stack, "CM 2");
    ASSERT_EQ(addNodalLoads(p, stack, {1, 2}, data(2.0, LoadDirection::MinusZ, cm)), NodalLoadError::None);
    ASSERT_EQ(addNodalLoads(p, stack, {1}, data(3.0, LoadDirection::MinusZ, cm2)), NodalLoadError::None);
    LoadScales s;
    s.perCase[cm] = 2.0;
    s.perCase[cm2] = 3.0;
    ASSERT_EQ(setLoadScales(p, stack, s), NodalLoadError::None);
    ExternalSupport fixed;
    fixed.type = ExternalSupportType::Fixed;
    fixed.prescribed.push_back({cm, {0, 0, -0.01, 0, 0, 0}});
    fixed.prescribed.push_back({cm2, {0, 0, -0.02, 0, 0, 0}});
    ASSERT_EQ(assignExternalSupport(p, stack, {3}, fixed), 1);

    const LoadCaseDependents dep = loadCaseDependents(p, cm);
    EXPECT_EQ(dep.loads.size(), 2u);
    EXPECT_EQ(dep.prescribedNodes, (std::vector<int>{3}));
    EXPECT_FALSE(dep.empty());
    EXPECT_TRUE(loadCaseDependents(p, 0).empty());

    const std::size_t before = stack.count();
    ASSERT_EQ(deleteLoadCase(p, stack, cm), LoadCaseError::None);
    EXPECT_EQ(stack.count(), before + 1);  // one command
    EXPECT_TRUE(nodalLoadsOfCase(p.nodalLoads, cm).empty());
    EXPECT_EQ(nodalLoadsOfCase(p.nodalLoads, cm2).size(), 1u);
    EXPECT_EQ(p.nodalLoads.scales.perCase.count(cm), 0u);
    EXPECT_EQ(p.nodalLoads.scales.perCase.count(cm2), 1u);
    ASSERT_EQ(p.nodeSupports.external.at(3).prescribed.size(), 1u);
    EXPECT_EQ(p.nodeSupports.external.at(3).prescribed[0].loadCaseId, cm2);
    EXPECT_TRUE(project_json::validate(p).empty());

    stack.undo();
    EXPECT_EQ(p.nodalLoads.items.size(), 3u);
    EXPECT_NE(findLoadCase(p.loadCases, cm), nullptr);
    EXPECT_EQ(p.nodeSupports.external.at(3).prescribed.size(), 2u);
    EXPECT_EQ(p.nodalLoads.scales.perCase.count(cm), 1u);
    stack.redo();
    EXPECT_EQ(p.nodalLoads.items.size(), 1u);
    EXPECT_EQ(findLoadCase(p.loadCases, cm), nullptr);
}

TEST(NodalLoad, PrescribedValidationUsesTheLoadCases) {
    Project p = baseProject();
    UndoStack stack;
    ExternalSupport fixed;
    fixed.type = ExternalSupportType::Fixed;
    fixed.prescribed.push_back({0, {0, 0, -0.01, 0, 0, 0}});
    EXPECT_TRUE(checkExternalSupport(fixed, p.loadCases).empty());
    EXPECT_EQ(assignExternalSupport(p, stack, {1}, fixed), 1);

    fixed.prescribed.push_back({5, {0, 0, -0.01, 0, 0, 0}});  // case 5 does not exist
    auto issues = checkExternalSupport(fixed, p.loadCases);
    ASSERT_FALSE(issues.empty());
    EXPECT_TRUE(hasError(issues));
    EXPECT_EQ(assignExternalSupport(p, stack, {2}, fixed), 0);

    // All-zero is reported before the repeated case (measured order); both are errors.
    ExternalSupport s;
    s.type = ExternalSupportType::Fixed;
    s.prescribed = {{0, {}}, {0, {0, 0, 0.5, 0, 0, 0}}};
    issues = checkExternalSupport(s, p.loadCases);
    ASSERT_EQ(issues.size(), 2u);
    EXPECT_EQ(issues[0].messageId, 65466);
    EXPECT_EQ(issues[1].messageId, 65465);
    s.prescribed = {{0, {0, 0, 0.5, 0, 0, 0}}, {0, {0, 0, 0.7, 0, 0, 0}}};
    issues = checkExternalSupport(s, p.loadCases);
    ASSERT_EQ(issues.size(), 1u);
    EXPECT_EQ(issues[0].messageId, 65465);
}

TEST(NodalLoad, JsonRoundTripIsVersionedAndTolerant) {
    Project p = baseProject();
    UndoStack stack;
    const int cm = addCase(p, stack);
    ASSERT_EQ(addNodalLoads(p, stack, {1}, data(5.0, LoadDirection::PlusX)), NodalLoadError::None);
    NodalLoadData d = data(7.26, LoadDirection::UserVector, cm);
    d.userVector = {1, -2, 0.5};
    ASSERT_EQ(addNodalLoads(p, stack, {2, 3}, d), NodalLoadError::None);
    LoadScales s;
    s.mode = LoadScaleMode::Proportional;
    s.fixedSizeM = 0.75;
    s.perCase[cm] = 1.5;
    s.perType[static_cast<std::size_t>(LoadScaleType::Punctual)] = 2.0;
    ASSERT_EQ(setLoadScales(p, stack, s), NodalLoadError::None);
    ASSERT_EQ(deleteNodalLoads(p, stack, {3}), NodalLoadError::None);  // leaves a gap in the ids

    ASSERT_TRUE(project_json::isKnownSection("nodal_loads"));
    EXPECT_EQ(project_json::sectionTitle("nodal_loads"), "Cargas sobre nudos");
    const std::string text = project_json::dumpSection(p, "nodal_loads");
    EXPECT_NE(text.find("\"version\": 1"), std::string::npos);
    Project q = baseProject();
    q.loadCases = p.loadCases;
    project_json::loadSection(q, "nodal_loads", text);
    EXPECT_EQ(q.nodalLoads, p.nodalLoads);
    EXPECT_EQ(q.nodalLoads.nextId, p.nodalLoads.nextId);  // ids still not reused
    EXPECT_TRUE(project_json::validate(q).empty());

    // Old files without the section keep it empty; unknown keys and bad values are tolerated.
    Project old;
    project_json::loadSection(old, "nodal_loads", "{}");
    EXPECT_TRUE(old.nodalLoads.empty());
    EXPECT_EQ(old.nodalLoads.scales, LoadScales{});
    Project t = baseProject();
    project_json::loadSection(t, "nodal_loads",
                              R"({"version":9,"future":1,"items":[{"id":1,"node":1,"load_case":0,"value":7.256,"direction":42,"extra":true},
                                  {"id":0,"node":1},{"node":2}]})");
    ASSERT_EQ(t.nodalLoads.items.size(), 1u);
    EXPECT_DOUBLE_EQ(t.nodalLoads.items[0].magnitude, 7.26);
    EXPECT_EQ(t.nodalLoads.items[0].direction, LoadDirection::MinusZ);
    EXPECT_EQ(t.nodalLoads.nextId, 2);
    EXPECT_TRUE(project_json::validate(t).empty());
}

TEST(NodalLoad, JsonSkipsLoadsOfVanishedNodesAndValidationCatchesCorruption) {
    Project p = baseProject();
    UndoStack stack;
    ASSERT_EQ(addNodalLoads(p, stack, {1, 2}, data(2.0)), NodalLoadError::None);
    p.structure.removeNode(2);  // behind the commands' back
    const std::string text = project_json::dumpSection(p, "nodal_loads");
    Project q = baseProject();
    project_json::loadSection(q, "nodal_loads", text);
    EXPECT_EQ(q.nodalLoads.items.size(), 1u);

    EXPECT_TRUE(validateNodalLoads(q.nodalLoads, q.loadCases).empty());
    NodalLoads bad = q.nodalLoads;
    bad.items[0].loadCaseId = 9;
    EXPECT_FALSE(validateNodalLoads(bad, q.loadCases).empty());
    bad = q.nodalLoads;
    bad.items[0].magnitude = 0.0;
    EXPECT_FALSE(validateNodalLoads(bad, q.loadCases).empty());
    bad = q.nodalLoads;
    bad.items.push_back(bad.items[0]);
    EXPECT_FALSE(validateNodalLoads(bad, q.loadCases).empty());  // repeated id
    bad = q.nodalLoads;
    bad.items[0].direction = LoadDirection::UserVector;
    bad.items[0].userVector = {0, 0, 0};
    EXPECT_FALSE(validateNodalLoads(bad, q.loadCases).empty());
    bad = q.nodalLoads;
    bad.scales.fixedSizeM = -1.0;
    EXPECT_FALSE(validateNodalLoads(bad, q.loadCases).empty());
}
