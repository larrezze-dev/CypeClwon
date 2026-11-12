#include <gtest/gtest.h>

#include <cmath>
#include <set>

#include "core/geom3.h"
#include "core/load_case_edit.h"
#include "core/node_edit.h"
#include "core/nodal_load_edit.h"
#include "core/project_json.h"
#include "core/shell_edit.h"
#include "core/shell_load_edit.h"

using namespace cc::core;

namespace {

/// Square 4 x 4 in the xy plane (nodes 1..4 counter-clockwise, local axes = global ones) with shell 1 of 100 mm of concrete.
Project baseProject() {
    Project p;
    const int n1 = p.structure.addNode({0, 0, 0});
    const int n2 = p.structure.addNode({4, 0, 0});
    const int n3 = p.structure.addNode({4, 4, 0});
    const int n4 = p.structure.addNode({0, 4, 0});
    p.structure.addShell({n1, n2, n3, n4}, 0.1);
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

ShellLoadData data(ShellLoadType type, double v1 = 5.0, double v2 = 0.0) {
    ShellLoadData d = shellLoadDefaults(0, type);
    d.value1 = v1;
    d.value2 = v2;
    return d;
}

void expectIssue(const ShellLoadIssue& i, ShellLoadIssueKind kind, const std::string& message) {
    EXPECT_EQ(i.kind, kind) << message;
    EXPECT_EQ(i.message, message);
}

ShellLoadFrame frameOf(const Project& p, int shellId = 1) {
    const auto f = shellLoadFrame(p.structure, shellId);
    EXPECT_TRUE(f.has_value());
    return f.value_or(ShellLoadFrame{});
}

}  // namespace

// ---------------------------------------------------------------- types, defaults, fields

TEST(ShellLoad, TypeValuesAndButtonOrderMatchTheOriginal) {
    EXPECT_EQ(static_cast<int>(ShellLoadType::Uniform), 0);
    EXPECT_EQ(static_cast<int>(ShellLoadType::VariableX), 1);
    EXPECT_EQ(static_cast<int>(ShellLoadType::VariableY), 2);
    EXPECT_EQ(static_cast<int>(ShellLoadType::UniformTemperature), 3);
    EXPECT_EQ(static_cast<int>(ShellLoadType::Shrinkage), 4);
    for (int i = 0; i < kShellLoadTypeCount; ++i) EXPECT_EQ(static_cast<int>(kShellLoadTypeButtonOrder[i]), i);   // UI order = enum order
    EXPECT_EQ(shellLoadTypeName(ShellLoadType::VariableX), "Variable en eje x local");
    EXPECT_EQ(shellLoadTypeName(ShellLoadType::VariableY), "Variable en eje y local");
    EXPECT_EQ(shellLoadTypeName(ShellLoadType::Shrinkage), "Retracción");
    EXPECT_TRUE(isSurfaceLoad(ShellLoadType::VariableY));
    EXPECT_FALSE(isSurfaceLoad(ShellLoadType::UniformTemperature));
    EXPECT_TRUE(isVariableShellLoad(ShellLoadType::VariableX));
    EXPECT_FALSE(isVariableShellLoad(ShellLoadType::Uniform));
}

TEST(ShellLoad, DialogDefaultsAreMeasured) {
    const ShellLoadData u = shellLoadDefaults();
    EXPECT_EQ(u.type, ShellLoadType::Uniform);
    EXPECT_EQ(u.axes, ShellLoadAxes::Local);    // "Ejes locales de la lámina" by default
    EXPECT_EQ(u.sense, ShellLoadSense::PlusZ);  // button 1
    EXPECT_DOUBLE_EQ(u.value1, 9.81);
    const ShellLoadData vx = shellLoadDefaults(3, ShellLoadType::VariableX);
    EXPECT_EQ(vx.loadCaseId, 3);
    EXPECT_DOUBLE_EQ(vx.value1, 9.81);
    EXPECT_DOUBLE_EQ(vx.value2, 9.81);
    EXPECT_DOUBLE_EQ(shellLoadDefaults(0, ShellLoadType::UniformTemperature).value1, 10.0);
    EXPECT_DOUBLE_EQ(shellLoadDefaults(0, ShellLoadType::Shrinkage).value1, 0.0001);
    EXPECT_TRUE(checkShellLoadData(shellLoadDefaults(0, ShellLoadType::Shrinkage)).ok());
}

TEST(ShellLoad, FieldSpecsLabelsRangesAndDecimals) {
    ShellLoadFieldSpec s = shellLoadFieldSpec(data(ShellLoadType::Uniform), ShellLoadField::Value1);
    EXPECT_EQ(s.label, "Valor");
    EXPECT_DOUBLE_EQ(s.min, 0.0);
    EXPECT_DOUBLE_EQ(s.max, 100000.0);
    EXPECT_EQ(s.decimals, 3);
    EXPECT_EQ(s.unit, "kN/m²");
    s = shellLoadFieldSpec(data(ShellLoadType::VariableY), ShellLoadField::Value1);
    EXPECT_EQ(s.label, "Valor inicial");
    s = shellLoadFieldSpec(data(ShellLoadType::VariableY), ShellLoadField::Value2);
    EXPECT_EQ(s.label, "Valor final");
    s = shellLoadFieldSpec(data(ShellLoadType::UniformTemperature), ShellLoadField::Value1);
    EXPECT_EQ(s.decimals, 2);
    EXPECT_DOUBLE_EQ(s.min, -100.0);
    EXPECT_DOUBLE_EQ(s.max, 1000.0);
    EXPECT_TRUE(s.unit.empty());
    s = shellLoadFieldSpec(data(ShellLoadType::Shrinkage), ShellLoadField::Value1);
    EXPECT_EQ(s.decimals, 4);
    EXPECT_DOUBLE_EQ(s.min, -1000.0);
    EXPECT_TRUE(s.blankUnitSpacing);
}

TEST(ShellLoad, MeasuredRangeMessagesAreExact) {
    EXPECT_EQ(shellLoadRangeMessage(shellLoadFieldSpec(data(ShellLoadType::Uniform), ShellLoadField::Value1)),
              "El valor de 'Valor' debe estar entre 0.000 kN/m² y 100000.000 kN/m².");
    EXPECT_EQ(shellLoadRangeMessage(shellLoadFieldSpec(data(ShellLoadType::VariableX), ShellLoadField::Value1)),
              "El valor de 'Valor inicial' debe estar entre 0.000 kN/m² y 100000.000 kN/m².");
    EXPECT_EQ(shellLoadRangeMessage(shellLoadFieldSpec(data(ShellLoadType::VariableX), ShellLoadField::Value2)),
              "El valor de 'Valor final' debe estar entre 0.000 kN/m² y 100000.000 kN/m².");
    EXPECT_EQ(shellLoadRangeMessage(shellLoadFieldSpec(data(ShellLoadType::UniformTemperature), ShellLoadField::Value1)),
              "El valor de 'Valor' debe estar entre -100.00 y 1000.00.");
    // Doubled spaces before «y» and before the full stop (unit-less control).
    EXPECT_EQ(shellLoadRangeMessage(shellLoadFieldSpec(data(ShellLoadType::Shrinkage), ShellLoadField::Value1)),
              "El valor de 'Valor' debe estar entre -1000.0000  y 1000.0000 .");
    EXPECT_EQ(shellLoadNotNumberMessage(), "La cadena introducida no es un número.");
    EXPECT_EQ(shellLoadZeroMessage(), "El valor introducido no puede ser cero.");
}

TEST(ShellLoad, ParsingFieldsUsesRoundingAndTheNotANumberMessage) {
    const ShellLoadData u = data(ShellLoadType::Uniform);
    ShellLoadFieldParse r = parseShellLoadField(u, ShellLoadField::Value1, "9,8101");
    ASSERT_TRUE(r.ok);
    EXPECT_DOUBLE_EQ(r.value, 9.81);
    EXPECT_EQ(parseShellLoadField(u, ShellLoadField::Value1, "abc").message, "La cadena introducida no es un número.");
    EXPECT_FALSE(parseShellLoadField(u, ShellLoadField::Value1, "-1").ok);
    EXPECT_EQ(parseShellLoadField(u, ShellLoadField::Value1, "100000.001").message,
              "El valor de 'Valor' debe estar entre 0.000 kN/m² y 100000.000 kN/m².");
    EXPECT_TRUE(parseShellLoadField(u, ShellLoadField::Value1, "100000").ok);
    // Zero parses (the null rule is of Aceptar, not of the field); 0.0004 rounds to 0.000.
    r = parseShellLoadField(u, ShellLoadField::Value1, "0.0004");
    ASSERT_TRUE(r.ok);
    EXPECT_DOUBLE_EQ(r.value, 0.0);
    const ShellLoadData temp = data(ShellLoadType::UniformTemperature);
    EXPECT_FALSE(parseShellLoadField(temp, ShellLoadField::Value1, "1000.01").ok);
    EXPECT_FALSE(parseShellLoadField(temp, ShellLoadField::Value1, "-100.01").ok);
    EXPECT_TRUE(parseShellLoadField(temp, ShellLoadField::Value1, "-100").ok);
    const ShellLoadData shr = data(ShellLoadType::Shrinkage);
    EXPECT_FALSE(parseShellLoadField(shr, ShellLoadField::Value1, "1000.0001").ok);
    EXPECT_FALSE(parseShellLoadField(shr, ShellLoadField::Value1, "-1000.0001").ok);
    EXPECT_DOUBLE_EQ(parseShellLoadField(shr, ShellLoadField::Value1, "0.00049").value, 0.0005);
}

// ---------------------------------------------------------------- checks

TEST(ShellLoad, CheckGivesTheMeasuredWarningsAndErrors) {
    const std::string range = "El valor de 'Valor' debe estar entre 0.000 kN/m² y 100000.000 kN/m².";
    const std::string zero = "El valor introducido no puede ser cero.";
    EXPECT_TRUE(checkShellLoadData(data(ShellLoadType::Uniform, 5)).ok());
    EXPECT_TRUE(checkShellLoadData(data(ShellLoadType::Uniform, 100000)).ok());
    expectIssue(checkShellLoadData(data(ShellLoadType::Uniform, -1)), ShellLoadIssueKind::Warning, range);   // negative = range Aviso
    expectIssue(checkShellLoadData(data(ShellLoadType::Uniform, 100000.001)), ShellLoadIssueKind::Warning, range);
    expectIssue(checkShellLoadData(data(ShellLoadType::Uniform, 0)), ShellLoadIssueKind::Error, zero);        // zero = Error #65323
    expectIssue(checkShellLoadData(data(ShellLoadType::Uniform, 0.0004)), ShellLoadIssueKind::Error, zero);   // rounds to 0.000
    EXPECT_EQ(checkShellLoadData(data(ShellLoadType::Uniform, 0)).field, ShellLoadField::Value1);
    EXPECT_EQ(checkShellLoadData(data(ShellLoadType::Uniform, -1)).field, ShellLoadField::Value1);

    // Variable: initial first, final second; both null is the Error; ONE null value is accepted.
    expectIssue(checkShellLoadData(data(ShellLoadType::VariableX, -1, 5)), ShellLoadIssueKind::Warning,
                "El valor de 'Valor inicial' debe estar entre 0.000 kN/m² y 100000.000 kN/m².");
    const ShellLoadIssue fin = checkShellLoadData(data(ShellLoadType::VariableY, 5, 100000.001));
    expectIssue(fin, ShellLoadIssueKind::Warning, "El valor de 'Valor final' debe estar entre 0.000 kN/m² y 100000.000 kN/m².");
    EXPECT_EQ(fin.field, ShellLoadField::Value2);
    expectIssue(checkShellLoadData(data(ShellLoadType::VariableX, -3, -3)), ShellLoadIssueKind::Warning,
                "El valor de 'Valor inicial' debe estar entre 0.000 kN/m² y 100000.000 kN/m².");   // range before the null rule
    expectIssue(checkShellLoadData(data(ShellLoadType::VariableX, 0, 0)), ShellLoadIssueKind::Error, zero);
    EXPECT_TRUE(checkShellLoadData(data(ShellLoadType::VariableX, 0, 5)).ok());
    EXPECT_TRUE(checkShellLoadData(data(ShellLoadType::VariableY, 9.81, 0)).ok());

    // Temperature: -100..1000, zero is the Error, negative allowed.
    const std::string trange = "El valor de 'Valor' debe estar entre -100.00 y 1000.00.";
    expectIssue(checkShellLoadData(data(ShellLoadType::UniformTemperature, 1000.01)), ShellLoadIssueKind::Warning, trange);
    expectIssue(checkShellLoadData(data(ShellLoadType::UniformTemperature, -100.01)), ShellLoadIssueKind::Warning, trange);
    expectIssue(checkShellLoadData(data(ShellLoadType::UniformTemperature, 0)), ShellLoadIssueKind::Error, zero);
    EXPECT_TRUE(checkShellLoadData(data(ShellLoadType::UniformTemperature, -30)).ok());
    EXPECT_TRUE(checkShellLoadData(data(ShellLoadType::UniformTemperature, 1000)).ok());

    // Shrinkage: -1000..1000, 4 decimals, zero is the Error.
    const std::string srange = "El valor de 'Valor' debe estar entre -1000.0000  y 1000.0000 .";
    expectIssue(checkShellLoadData(data(ShellLoadType::Shrinkage, 1000.0001)), ShellLoadIssueKind::Warning, srange);
    expectIssue(checkShellLoadData(data(ShellLoadType::Shrinkage, -1000.0001)), ShellLoadIssueKind::Warning, srange);
    expectIssue(checkShellLoadData(data(ShellLoadType::Shrinkage, 0)), ShellLoadIssueKind::Error, zero);
    expectIssue(checkShellLoadData(data(ShellLoadType::Shrinkage, 0.00004)), ShellLoadIssueKind::Error, zero);
    EXPECT_TRUE(checkShellLoadData(data(ShellLoadType::Shrinkage, -0.0005)).ok());

    // Corrupt enum values.
    ShellLoadData bad = data(ShellLoadType::Uniform, 5);
    bad.type = static_cast<ShellLoadType>(9);
    EXPECT_FALSE(checkShellLoadData(bad).ok());
    bad = data(ShellLoadType::Uniform, 5);
    bad.sense = static_cast<ShellLoadSense>(7);
    expectIssue(checkShellLoadData(bad), ShellLoadIssueKind::Warning, "La dirección introducida no es correcta.");
    EXPECT_FALSE(checkShellLoadData(data(ShellLoadType::Uniform, std::nan(""))).ok());
}

TEST(ShellLoad, NormaliseToCanonicalForm) {
    ShellLoadData u = data(ShellLoadType::Uniform, 5.00049, 7.0);
    u.axes = ShellLoadAxes::Global;
    u.sense = ShellLoadSense::MinusZ;
    ShellLoadData n = normaliseShellLoadData(u);
    EXPECT_DOUBLE_EQ(n.value1, 5.0);
    EXPECT_DOUBLE_EQ(n.value2, 0.0);   // unused
    EXPECT_EQ(n.axes, ShellLoadAxes::Global);
    EXPECT_EQ(n.sense, ShellLoadSense::MinusZ);

    ShellLoadData v = data(ShellLoadType::VariableX, 5.1234, 15.9996);
    n = normaliseShellLoadData(v);
    EXPECT_DOUBLE_EQ(n.value1, 5.123);
    EXPECT_DOUBLE_EQ(n.value2, 16.0);

    ShellLoadData t = data(ShellLoadType::UniformTemperature, 20.004, 3.0);
    t.axes = ShellLoadAxes::Global;
    t.sense = ShellLoadSense::MinusZ;
    n = normaliseShellLoadData(t);
    EXPECT_DOUBLE_EQ(n.value1, 20.0);
    EXPECT_DOUBLE_EQ(n.value2, 0.0);
    EXPECT_EQ(n.axes, ShellLoadAxes::Local);   // neither axes nor sense
    EXPECT_EQ(n.sense, ShellLoadSense::PlusZ);
    n = normaliseShellLoadData(data(ShellLoadType::Shrinkage, 0.00049));
    EXPECT_DOUBLE_EQ(n.value1, 0.0005);
    EXPECT_EQ(normaliseShellLoadData(n), n);   // idempotent
}

// ---------------------------------------------------------------- self weight

TEST(ShellLoad, SelfWeightIsThicknessTimesSpecificWeight) {
    Project p = baseProject();
    const Shell& s = *p.structure.findShell(1);
    EXPECT_DOUBLE_EQ(shellSpecificWeightKnM3(s.material), 2.5 * 9.81);
    EXPECT_NEAR(shellSelfWeightKnM2(s), 0.1 * 2.5 * 9.81, 1e-12);   // 100 mm of C25/30 (measured)
    const auto load = shellSelfWeightLoad(s);
    ASSERT_TRUE(load.has_value());
    EXPECT_EQ(load->type, ShellLoadType::Uniform);
    EXPECT_EQ(load->sense, ShellLoadSense::MinusZ);
    EXPECT_EQ(load->axes, ShellLoadAxes::Global);
    EXPECT_EQ(load->loadCaseId, 0);
    EXPECT_NEAR(load->value1, 0.1 * 2.5 * 9.81, 1e-12);

    Shell thick = s;
    thick.thicknessM = 0.25;
    EXPECT_NEAR(shellSelfWeightKnM2(thick), 0.25 * 2.5 * 9.81, 1e-12);
    thick.material.kind = ShellMaterialKind::GenericIsotropic;
    thick.material.isoDensityKnM3 = 10.0;
    EXPECT_NEAR(shellSelfWeightKnM2(thick), 2.5, 1e-12);
    thick.material.kind = ShellMaterialKind::GenericOrthotropic;
    EXPECT_NEAR(shellSelfWeightKnM2(thick), 0.25 * thick.material.orthoDensityKnM3, 1e-12);
    thick.material.kind = ShellMaterialKind::RolledSteel;
    EXPECT_NEAR(shellSelfWeightKnM2(thick), 0.25 * 77.01, 1e-12);
    thick.thicknessM = 0.0;
    EXPECT_FALSE(shellSelfWeightLoad(thick).has_value());
}

TEST(ShellLoad, SelfWeightActivateAndDeactivate) {
    Project p = baseProject();
    UndoStack stack;
    EXPECT_TRUE(shellSelfWeightActive(p.shellLoads, 1));
    EXPECT_EQ(setShellSelfWeight(p, stack, {1}, true), ShellLoadError::NothingToChange);   // already active
    EXPECT_EQ(setShellSelfWeight(p, stack, {99}, false), ShellLoadError::NoShells);
    EXPECT_EQ(setShellSelfWeight(p, stack, {}, false), ShellLoadError::NoShells);
    ASSERT_EQ(setShellSelfWeight(p, stack, {1, 99}, false), ShellLoadError::None);
    EXPECT_FALSE(shellSelfWeightActive(p.shellLoads, 1));
    EXPECT_FALSE(p.shellLoads.empty());
    EXPECT_FALSE(hasShellLoads(p.shellLoads));   // the flag is not a load: Editar / Borrar stay disabled
    EXPECT_EQ(setShellSelfWeight(p, stack, {1}, false), ShellLoadError::NothingToChange);
    EXPECT_TRUE(project_json::validate(p).empty());
    stack.undo();
    EXPECT_TRUE(shellSelfWeightActive(p.shellLoads, 1));
    stack.redo();
    EXPECT_FALSE(shellSelfWeightActive(p.shellLoads, 1));
    ASSERT_EQ(setShellSelfWeight(p, stack, {1}, true), ShellLoadError::None);
    EXPECT_TRUE(p.shellLoads.empty());
}

// ---------------------------------------------------------------- operations

TEST(ShellLoad, AddNeverMergesAndUndoes) {
    Project p = baseProject();
    UndoStack stack;
    std::vector<int> ids;
    ASSERT_EQ(addShellLoads(p, stack, {1}, data(ShellLoadType::Uniform, 5), &ids), ShellLoadError::None);
    EXPECT_EQ(ids, (std::vector<int>{1}));
    ASSERT_EQ(addShellLoads(p, stack, {1, 1}, data(ShellLoadType::Uniform, 5), &ids), ShellLoadError::None);   // same data again: second load
    EXPECT_EQ(ids, (std::vector<int>{2}));                                                                     // duplicates dropped
    EXPECT_EQ(shellLoadsOfShell(p.shellLoads, 1), (std::vector<int>{1, 2}));
    ASSERT_NE(findShellLoad(p.shellLoads, 2), nullptr);
    EXPECT_EQ(findShellLoad(p.shellLoads, 9), nullptr);
    EXPECT_TRUE(hasShellLoads(p.shellLoads));
    EXPECT_TRUE(project_json::validate(p).empty());
    stack.undo();
    EXPECT_EQ(p.shellLoads.items.size(), 1u);
    stack.undo();
    EXPECT_FALSE(hasShellLoads(p.shellLoads));
    stack.redo();
    stack.redo();
    EXPECT_EQ(p.shellLoads.items.size(), 2u);
    EXPECT_EQ(p.shellLoads.nextId, 3);
}

TEST(ShellLoad, AddChecksDataLoadCaseAndShellsAndRemembersTheLastValues) {
    Project p = baseProject();
    UndoStack stack;
    ShellLoadSession session;
    EXPECT_DOUBLE_EQ(session.last.value1, 9.81);
    EXPECT_TRUE(session.selfWeightActivate);
    ShellLoadIssue issue;
    EXPECT_EQ(addShellLoads(p, stack, {1}, data(ShellLoadType::Uniform, 0), nullptr, &issue, &session), ShellLoadError::InvalidData);
    EXPECT_EQ(issue.message, "El valor introducido no puede ser cero.");
    EXPECT_DOUBLE_EQ(session.last.value1, 9.81);   // not remembered on failure
    ShellLoadData d = data(ShellLoadType::Uniform, 5);
    d.loadCaseId = 42;
    EXPECT_EQ(addShellLoads(p, stack, {1}, d, nullptr, &issue), ShellLoadError::UnknownLoadCase);
    EXPECT_EQ(addShellLoads(p, stack, {77}, data(ShellLoadType::Uniform, 5)), ShellLoadError::NoShells);
    EXPECT_EQ(addShellLoads(p, stack, {}, data(ShellLoadType::Uniform, 5)), ShellLoadError::NoShells);
    EXPECT_FALSE(shellLoadErrorMessage(ShellLoadError::NoShells).empty());
    EXPECT_TRUE(shellLoadErrorMessage(ShellLoadError::None).empty());
    EXPECT_TRUE(p.shellLoads.items.empty());
    ASSERT_EQ(addShellLoads(p, stack, {1}, data(ShellLoadType::VariableX, 4, 8), nullptr, &issue, &session), ShellLoadError::None);
    EXPECT_TRUE(issue.ok());
    EXPECT_EQ(session.last.type, ShellLoadType::VariableX);
    EXPECT_DOUBLE_EQ(session.last.value2, 8.0);
}

TEST(ShellLoad, StoredDataIsCanonicalAndAllFiveTypesCanBeCreated) {
    Project p = baseProject();
    UndoStack stack;
    for (ShellLoadType t : kShellLoadTypeButtonOrder) ASSERT_EQ(addShellLoads(p, stack, {1}, shellLoadDefaults(0, t)), ShellLoadError::None);
    ASSERT_EQ(p.shellLoads.items.size(), 5u);
    for (const ShellLoad& l : p.shellLoads.items) EXPECT_EQ(normaliseShellLoadData(shellLoadToData(l)), shellLoadToData(l));
    EXPECT_DOUBLE_EQ(p.shellLoads.items[0].value2, 0.0);
    EXPECT_DOUBLE_EQ(p.shellLoads.items[1].value2, 9.81);
    EXPECT_DOUBLE_EQ(p.shellLoads.items[3].value1, 10.0);
    EXPECT_DOUBLE_EQ(p.shellLoads.items[4].value1, 0.0001);
    EXPECT_TRUE(project_json::validate(p).empty());
}

TEST(ShellLoad, EditOverwritesTheWholeDefinitionIncludingTheCase) {
    Project p = baseProject();
    UndoStack stack;
    const int cm = addCase(p, stack);
    ASSERT_EQ(addShellLoads(p, stack, {1}, data(ShellLoadType::Uniform, 5)), ShellLoadError::None);
    ASSERT_EQ(addShellLoads(p, stack, {1}, data(ShellLoadType::UniformTemperature, 20)), ShellLoadError::None);
    ShellLoadData d = data(ShellLoadType::VariableY, 3, 6);
    d.loadCaseId = cm;
    d.sense = ShellLoadSense::MinusZ;
    d.axes = ShellLoadAxes::Global;
    ShellLoadSession session;
    ASSERT_EQ(editShellLoads(p, stack, {1, 2, 99}, d, nullptr, &session), ShellLoadError::None);
    for (const ShellLoad& l : p.shellLoads.items) {
        EXPECT_EQ(l.type, ShellLoadType::VariableY);
        EXPECT_EQ(l.loadCaseId, cm);
        EXPECT_EQ(l.sense, ShellLoadSense::MinusZ);
        EXPECT_EQ(l.axes, ShellLoadAxes::Global);
        EXPECT_DOUBLE_EQ(l.value2, 6.0);
    }
    EXPECT_EQ(session.last.type, ShellLoadType::VariableY);
    EXPECT_EQ(editShellLoads(p, stack, {1, 2}, d), ShellLoadError::NothingToChange);
    EXPECT_EQ(editShellLoads(p, stack, {99}, d), ShellLoadError::UnknownLoad);
    ShellLoadIssue issue;
    EXPECT_EQ(editShellLoads(p, stack, {1}, data(ShellLoadType::Uniform, 0), &issue, &session), ShellLoadError::InvalidData);
    EXPECT_EQ(session.last.type, ShellLoadType::VariableY);   // failure does not touch the session
    ShellLoadData unknownCase = d;
    unknownCase.loadCaseId = 77;
    EXPECT_EQ(editShellLoads(p, stack, {1}, unknownCase), ShellLoadError::UnknownLoadCase);
    // Editing one load of the two leaves the other alone.
    ASSERT_EQ(editShellLoads(p, stack, {2}, data(ShellLoadType::Shrinkage, 0.0005)), ShellLoadError::None);
    EXPECT_EQ(findShellLoad(p.shellLoads, 1)->type, ShellLoadType::VariableY);
    EXPECT_EQ(findShellLoad(p.shellLoads, 2)->type, ShellLoadType::Shrinkage);
    stack.undo();
    EXPECT_EQ(findShellLoad(p.shellLoads, 2)->type, ShellLoadType::VariableY);
    stack.undo();
    EXPECT_EQ(findShellLoad(p.shellLoads, 1)->type, ShellLoadType::Uniform);
}

TEST(ShellLoad, ChangeCaseAndDelete) {
    Project p = baseProject();
    UndoStack stack;
    const int cm = addCase(p, stack);
    ASSERT_EQ(addShellLoads(p, stack, {1}, data(ShellLoadType::Uniform, 5)), ShellLoadError::None);
    ASSERT_EQ(addShellLoads(p, stack, {1}, data(ShellLoadType::Uniform, 6)), ShellLoadError::None);
    EXPECT_EQ(changeShellLoadCase(p, stack, {1}, 77), ShellLoadError::UnknownLoadCase);
    EXPECT_EQ(changeShellLoadCase(p, stack, {9}, cm), ShellLoadError::UnknownLoad);
    ASSERT_EQ(changeShellLoadCase(p, stack, {1}, cm), ShellLoadError::None);
    EXPECT_EQ(shellLoadsOfCase(p.shellLoads, cm), (std::vector<int>{1}));
    EXPECT_EQ(shellLoadsOfCase(p.shellLoads, 0), (std::vector<int>{2}));
    EXPECT_EQ(changeShellLoadCase(p, stack, {1}, cm), ShellLoadError::NothingToChange);
    stack.undo();
    EXPECT_EQ(findShellLoad(p.shellLoads, 1)->loadCaseId, 0);
    stack.redo();

    EXPECT_EQ(deleteShellLoads(p, stack, {9}), ShellLoadError::UnknownLoad);
    ASSERT_EQ(deleteShellLoads(p, stack, {2}), ShellLoadError::None);   // no confirmation
    EXPECT_EQ(p.shellLoads.items.size(), 1u);
    ASSERT_EQ(deleteShellLoads(p, stack, {1}), ShellLoadError::None);
    EXPECT_FALSE(hasShellLoads(p.shellLoads));   // Editar / Borrar / Hipótesis disabled again
    stack.undo();
    EXPECT_EQ(p.shellLoads.items.size(), 1u);
    ASSERT_EQ(addShellLoads(p, stack, {1}, data(ShellLoadType::Uniform, 7)), ShellLoadError::None);
    EXPECT_EQ(p.shellLoads.items.back().id, 3);   // ids never reused
}

// ---------------------------------------------------------------- structure cascades

TEST(ShellLoad, DeletingAShellDeletesItsLoadsSilentlyAndUndoRestores) {
    Project p = baseProject();
    UndoStack stack;
    ASSERT_EQ(addShellLoads(p, stack, {1}, data(ShellLoadType::Uniform, 5)), ShellLoadError::None);
    ASSERT_EQ(setShellSelfWeight(p, stack, {1}, false), ShellLoadError::None);
    ASSERT_EQ(deleteShells(p, stack, {1}), ShellError::None);
    EXPECT_TRUE(p.shellLoads.items.empty());
    EXPECT_TRUE(p.shellLoads.selfWeightOff.empty());
    EXPECT_TRUE(project_json::validate(p).empty());
    stack.undo();
    EXPECT_EQ(p.shellLoads.items.size(), 1u);
    EXPECT_FALSE(shellSelfWeightActive(p.shellLoads, 1));
    stack.redo();
    EXPECT_TRUE(p.shellLoads.items.empty());
}

TEST(ShellLoad, DeletingANodeOfAShellDeletesItsLoads) {
    Project p = baseProject();
    UndoStack stack;
    ASSERT_EQ(addShellLoads(p, stack, {1}, data(ShellLoadType::Uniform, 5)), ShellLoadError::None);
    ASSERT_TRUE(deleteNodes(p, stack, {1}));
    EXPECT_TRUE(p.structure.shells.empty());
    EXPECT_TRUE(p.shellLoads.items.empty());
    stack.undo();
    EXPECT_EQ(p.shellLoads.items.size(), 1u);
    EXPECT_EQ(p.structure.shells.size(), 1u);
    stack.redo();
    EXPECT_TRUE(p.shellLoads.items.empty());
}

TEST(ShellLoad, MovingANodeKeepsTheLoads) {
    Project p = baseProject();
    UndoStack stack;
    ASSERT_EQ(addShellLoads(p, stack, {1}, data(ShellLoadType::Uniform, 5)), ShellLoadError::None);
    ASSERT_TRUE(moveNode(p, stack, 3, {5, 5, 0}));
    EXPECT_EQ(p.shellLoads.items.size(), 1u);
}

TEST(ShellLoad, DividingAShellCopiesItsLoadsToThePieces) {
    Project p = baseProject();
    UndoStack stack;
    const int cm = addCase(p, stack);
    ASSERT_EQ(addShellLoads(p, stack, {1}, data(ShellLoadType::Uniform, 5)), ShellLoadError::None);
    ShellLoadData d = data(ShellLoadType::VariableX, 2, 4);
    d.loadCaseId = cm;
    ASSERT_EQ(addShellLoads(p, stack, {1}, d), ShellLoadError::None);
    ASSERT_EQ(setShellSelfWeight(p, stack, {1}, false), ShellLoadError::None);
    std::vector<int> created;
    ASSERT_EQ(divideShells(p, stack, {1}, {2, 1, 0}, {2, 2, 0}, &created), ShellError::None);
    ASSERT_EQ(created.size(), 1u);
    const int piece = created[0];
    EXPECT_EQ(p.shellLoads.items.size(), 4u);
    EXPECT_EQ(shellLoadsOfShell(p.shellLoads, 1).size(), 2u);
    ASSERT_EQ(shellLoadsOfShell(p.shellLoads, piece).size(), 2u);
    const ShellLoad* copy = findShellLoad(p.shellLoads, shellLoadsOfShell(p.shellLoads, piece)[1]);
    ASSERT_NE(copy, nullptr);
    EXPECT_EQ(copy->type, ShellLoadType::VariableX);
    EXPECT_EQ(copy->loadCaseId, cm);
    EXPECT_DOUBLE_EQ(copy->value2, 4.0);
    EXPECT_FALSE(shellSelfWeightActive(p.shellLoads, piece));   // the flag follows the pieces
    EXPECT_GE(copy->id, 3);                                     // new ids
    EXPECT_TRUE(project_json::validate(p).empty());
    stack.undo();
    EXPECT_EQ(p.shellLoads.items.size(), 2u);
    EXPECT_TRUE(shellSelfWeightActive(p.shellLoads, piece));
    stack.redo();
    EXPECT_EQ(p.shellLoads.items.size(), 4u);
}

TEST(ShellLoad, JoiningShellsKeepsTheLoadsOfTheSurvivorOnly) {
    Project p;
    const int a = p.structure.addNode({0, 0, 0}), b = p.structure.addNode({4, 0, 0}), c = p.structure.addNode({4, 4, 0}), d = p.structure.addNode({0, 4, 0});
    const int e = p.structure.addNode({8, 0, 0}), f = p.structure.addNode({8, 4, 0});
    p.structure.addShell({a, b, c, d}, 0.1);
    p.structure.addShell({b, e, f, c}, 0.1);
    UndoStack stack;
    ASSERT_EQ(addShellLoads(p, stack, {1, 2}, data(ShellLoadType::Uniform, 5)), ShellLoadError::None);
    int result = 0;
    ASSERT_EQ(joinShells(p, stack, {1, 2}, &result), ShellError::None);
    EXPECT_EQ(result, 1);
    ASSERT_EQ(p.shellLoads.items.size(), 1u);   // the absorbed shell's load is lost (assumption)
    EXPECT_EQ(p.shellLoads.items[0].shellId, 1);
    stack.undo();
    EXPECT_EQ(p.shellLoads.items.size(), 2u);
}

TEST(ShellLoad, ReconcileIsANoOpWithoutLoadsAndReportsTheCounts) {
    Project p = baseProject();
    ShellLoads empty;
    EXPECT_EQ(reconcileShellLoads(empty, p.structure, p.structure), empty);
    UndoStack stack;
    ASSERT_EQ(addShellLoads(p, stack, {1}, data(ShellLoadType::Uniform, 5)), ShellLoadError::None);
    StructuralModel after = p.structure;
    after.removeShell(1);
    ShellLoadReconcileInfo info;
    const ShellLoads out = reconcileShellLoads(p.shellLoads, p.structure, after, &info);
    EXPECT_TRUE(out.items.empty());
    EXPECT_EQ(info.removed, 1);
    EXPECT_EQ(info.copied, 0);
    EXPECT_EQ(out.nextId, p.shellLoads.nextId);
}

TEST(ShellLoad, DeletingALoadCaseDeletesItsShellLoadsInOneCommand) {
    Project p = baseProject();
    UndoStack stack;
    const int cm = addCase(p, stack);
    ShellLoadData d = data(ShellLoadType::Uniform, 5);
    d.loadCaseId = cm;
    ASSERT_EQ(addShellLoads(p, stack, {1}, d), ShellLoadError::None);
    ASSERT_EQ(addShellLoads(p, stack, {1}, data(ShellLoadType::Uniform, 6)), ShellLoadError::None);   // case 0
    const LoadCaseDependents dep = loadCaseDependents(p, cm);
    EXPECT_EQ(dep.shellLoads, (std::vector<int>{1}));
    EXPECT_FALSE(dep.empty());
    EXPECT_EQ(loadCaseDependents(p, 0).shellLoads.size(), 1u);
    const std::size_t before = stack.count();
    ASSERT_EQ(deleteLoadCase(p, stack, cm), LoadCaseError::None);
    EXPECT_EQ(stack.count(), before + 1);
    ASSERT_EQ(p.shellLoads.items.size(), 1u);
    EXPECT_EQ(p.shellLoads.items[0].loadCaseId, 0);
    EXPECT_TRUE(project_json::validate(p).empty());
    stack.undo();
    EXPECT_EQ(p.shellLoads.items.size(), 2u);
    EXPECT_NE(findLoadCase(p.loadCases, cm), nullptr);
    stack.redo();
    EXPECT_EQ(p.shellLoads.items.size(), 1u);
}

// ---------------------------------------------------------------- persistence

TEST(ShellLoad, JsonRoundTripIsVersionedAndTolerant) {
    Project p = baseProject();
    UndoStack stack;
    const int cm = addCase(p, stack);
    ShellLoadData d = data(ShellLoadType::VariableX, 5.5, 15);
    d.loadCaseId = cm;
    d.axes = ShellLoadAxes::Global;
    d.sense = ShellLoadSense::MinusZ;
    ASSERT_EQ(addShellLoads(p, stack, {1}, d), ShellLoadError::None);
    ASSERT_EQ(addShellLoads(p, stack, {1}, data(ShellLoadType::Shrinkage, 0.0005)), ShellLoadError::None);
    ASSERT_EQ(setShellSelfWeight(p, stack, {1}, false), ShellLoadError::None);
    ASSERT_TRUE(project_json::isKnownSection("shell_loads"));
    EXPECT_EQ(project_json::sectionTitle("shell_loads"), "Cargas en láminas");
    const std::string text = project_json::dumpSection(p, "shell_loads");
    EXPECT_NE(text.find("\"version\": 1"), std::string::npos);
    Project q = baseProject();
    q.loadCases = p.loadCases;
    project_json::loadSection(q, "shell_loads", text);
    EXPECT_EQ(q.shellLoads, p.shellLoads);
    EXPECT_EQ(q.shellLoads.nextId, p.shellLoads.nextId);
    EXPECT_FALSE(shellSelfWeightActive(q.shellLoads, 1));
    EXPECT_TRUE(project_json::validate(q).empty());

    Project old;
    project_json::loadSection(old, "shell_loads", "{}");
    EXPECT_TRUE(old.shellLoads.empty());
    Project t2 = baseProject();
    project_json::loadSection(t2, "shell_loads",
                              R"({"version":9,"future":1,"items":[{"id":1,"shell":1,"load_case":0,"type":0,"value1":7.2564,"axes":0,"sense":1,"extra":true},
                                  {"id":2,"shell":1,"type":77},{"id":0,"shell":1},{"shell":2}],"self_weight_off":[1,"x",-1]})");
    ASSERT_EQ(t2.shellLoads.items.size(), 1u);
    EXPECT_DOUBLE_EQ(t2.shellLoads.items[0].value1, 7.256);
    EXPECT_EQ(t2.shellLoads.items[0].axes, ShellLoadAxes::Global);
    EXPECT_EQ(t2.shellLoads.items[0].sense, ShellLoadSense::MinusZ);
    EXPECT_EQ(t2.shellLoads.nextId, 2);
    EXPECT_FALSE(shellSelfWeightActive(t2.shellLoads, 1));
    EXPECT_TRUE(project_json::validate(t2).empty());
}

TEST(ShellLoad, JsonSkipsVanishedShellsAndValidationCatchesCorruption) {
    Project p = baseProject();
    UndoStack stack;
    ASSERT_EQ(addShellLoads(p, stack, {1}, data(ShellLoadType::Uniform, 5)), ShellLoadError::None);
    ASSERT_EQ(setShellSelfWeight(p, stack, {1}, false), ShellLoadError::None);
    p.structure.removeShell(1);   // behind the commands' back
    const std::string text = project_json::dumpSection(p, "shell_loads");
    Project q = baseProject();
    project_json::loadSection(q, "shell_loads", text);
    EXPECT_TRUE(q.shellLoads.items.empty());
    EXPECT_TRUE(q.shellLoads.selfWeightOff.empty());

    Project c = baseProject();
    UndoStack s2;
    ASSERT_EQ(addShellLoads(c, s2, {1}, data(ShellLoadType::Uniform, 5)), ShellLoadError::None);
    EXPECT_TRUE(validateShellLoads(c.shellLoads, c.loadCases).empty());
    ShellLoads bad = c.shellLoads;
    bad.items[0].loadCaseId = 99;
    EXPECT_FALSE(validateShellLoads(bad, c.loadCases).empty());
    bad = c.shellLoads;
    bad.items[0].value1 = 5.00049;   // not canonical
    EXPECT_FALSE(validateShellLoads(bad, c.loadCases).empty());
    bad = c.shellLoads;
    bad.items[0].value1 = 0.0;
    EXPECT_FALSE(validateShellLoads(bad, c.loadCases).empty());
    bad = c.shellLoads;
    bad.items.push_back(bad.items[0]);   // repeated id
    EXPECT_FALSE(validateShellLoads(bad, c.loadCases).empty());
    bad = c.shellLoads;
    bad.nextId = 1;
    EXPECT_FALSE(validateShellLoads(bad, c.loadCases).empty());
}

TEST(ShellLoad, FilesWithoutShellLoadsMigrateCleanly) {
    Project p = baseProject();
    EXPECT_TRUE(p.shellLoads.empty());
    const std::string text = project_json::dumpSection(p, "shell_loads");
    Project q = baseProject();
    project_json::loadSection(q, "shell_loads", text);
    EXPECT_TRUE(q.shellLoads.empty());
    EXPECT_TRUE(project_json::validate(q).empty());
}

// ---------------------------------------------------------------- geometry

TEST(ShellLoad, FrameAndDirections) {
    Project p = baseProject();
    ShellLoadFrame f = frameOf(p);
    ASSERT_EQ(f.corners.size(), 4u);
    EXPECT_NEAR(f.z.z, 1.0, 1e-12);
    EXPECT_NEAR(f.x.x, 1.0, 1e-12);
    EXPECT_FALSE(shellLoadFrame(p.structure, 99).has_value());

    ShellLoadData d = data(ShellLoadType::Uniform, 5);
    Point3 dir = shellLoadDirection(d, f);
    EXPECT_NEAR(dir.z, 1.0, 1e-12);   // button 1 pushes towards +Z local
    d.sense = ShellLoadSense::MinusZ;
    EXPECT_NEAR(shellLoadDirection(d, f).z, -1.0, 1e-12);

    // Inverted z: local +Z is global -Z, global axes do not follow it.
    p.structure.findShell(1)->zAxisInverted = true;
    f = frameOf(p);
    d.sense = ShellLoadSense::PlusZ;
    EXPECT_NEAR(shellLoadDirection(d, f).z, -1.0, 1e-12);
    d.axes = ShellLoadAxes::Global;
    EXPECT_NEAR(shellLoadDirection(d, f).z, 1.0, 1e-12);
    // Temperature and shrinkage have no direction: the local +z.
    EXPECT_NEAR(shellLoadDirection(data(ShellLoadType::UniformTemperature, 20), f).z, -1.0, 1e-12);
}

TEST(ShellLoad, CornerValuesPerType) {
    Project p = baseProject();
    const ShellLoadFrame f = frameOf(p);   // N1 (0,0) N2 (4,0) N3 (4,4) N4 (0,4)
    EXPECT_EQ(shellLoadCornerValues(data(ShellLoadType::Uniform, 5), f), (std::vector<double>{5, 5, 5, 5}));
    EXPECT_EQ(shellLoadCornerValues(data(ShellLoadType::UniformTemperature, -20), f), (std::vector<double>{20, 20, 20, 20}));
    // Initial value on the smaller coordinate, final on the larger (measured 5 -> 15).
    const std::vector<double> vx = shellLoadCornerValues(data(ShellLoadType::VariableX, 5, 15), f);
    EXPECT_EQ(vx, (std::vector<double>{5, 15, 15, 5}));
    const std::vector<double> vy = shellLoadCornerValues(data(ShellLoadType::VariableY, 5, 15), f);
    EXPECT_EQ(vy, (std::vector<double>{5, 5, 15, 15}));
    // Decreasing: initial 15 at the smaller coordinate.
    EXPECT_EQ(shellLoadCornerValues(data(ShellLoadType::VariableX, 15, 5), f), (std::vector<double>{15, 5, 5, 15}));
    // A triangle gives the linear value at its corners.
    Project t;
    const int a = t.structure.addNode({0, 0, 0}), b = t.structure.addNode({4, 0, 0}), c = t.structure.addNode({0, 4, 0});
    t.structure.addShell({a, b, c}, 0.1);
    const std::vector<double> tri = shellLoadCornerValues(data(ShellLoadType::VariableX, 2, 6), frameOf(t));
    EXPECT_EQ(tri, (std::vector<double>{2, 6, 2}));
}

TEST(ShellLoad, DrawSizesFixedAndProportional) {
    LoadScales fixed;   // 0.5 m maximum
    const ShellLoadData u = data(ShellLoadType::Uniform, 5);
    EXPECT_DOUBLE_EQ(shellLoadDrawSizeM(fixed, u, 5, 5), 0.5);
    EXPECT_DOUBLE_EQ(shellLoadMaxSizeM(fixed, data(ShellLoadType::Uniform, 77)), 0.5);   // whatever the value
    EXPECT_NEAR(shellLoadDrawSizeM(fixed, u, 5, 15), 0.5 / 3.0, 1e-12);                  // wedge 5 -> 15: 1/3 .. 1
    EXPECT_DOUBLE_EQ(shellLoadMaxSizeM(fixed, data(ShellLoadType::VariableX, 5, 15)), 0.5);

    LoadScales prop;
    prop.mode = LoadScaleMode::Proportional;
    EXPECT_NEAR(shellLoadDrawSizeM(prop, u, 5, 5), 0.5, 1e-12);       // 0.1 m per kN/m2
    EXPECT_NEAR(shellLoadDrawSizeM(prop, u, 2.5, 2.5), 0.25, 1e-12);  // self weight of 100 mm of concrete
    prop.perCase[0] = 2.0;
    EXPECT_NEAR(shellLoadDrawSizeM(prop, u, 5, 5), 1.0, 1e-12);
    prop.perType[static_cast<std::size_t>(LoadScaleType::Surface)] = 3.0;   // "Superficial" applies to uniform and variable
    EXPECT_NEAR(shellLoadDrawSizeM(prop, u, 5, 5), 3.0, 1e-12);
    EXPECT_NEAR(shellLoadDrawSizeM(prop, data(ShellLoadType::VariableY, 5, 15), 15, 15), 9.0, 1e-12);
    EXPECT_NEAR(shellLoadDrawSizeM(prop, data(ShellLoadType::UniformTemperature, 20), 20, 20), 4.0, 1e-12);   // Temperature 1.0 x case 2.0 x 0.1 x 20
    EXPECT_NEAR(shellLoadDrawSizeM(prop, data(ShellLoadType::Shrinkage, 0.0005), 0.0005, 0.0005), 0.1 * 0.0005 * 2.0 * 100.0, 1e-12);
    EXPECT_EQ(shellLoadScaleType(ShellLoadType::VariableX), LoadScaleType::Surface);
    EXPECT_EQ(shellLoadScaleType(ShellLoadType::UniformTemperature), LoadScaleType::Temperature);
    EXPECT_EQ(shellLoadScaleType(ShellLoadType::Shrinkage), LoadScaleType::Shrinkage);
}

TEST(ShellLoad, LabelsFollowTheMeasuredForms) {
    EXPECT_EQ(shellLoadValueLabel(5.0), "5");
    EXPECT_EQ(shellLoadValueLabel(9.81), "9.81");
    EXPECT_EQ(shellLoadValueLabel(-7.5), "7.5");
    EXPECT_EQ(shellLoadValueLabel(0.0001, 4), "0.0001");
    EXPECT_EQ(shellLoadValueLabel(0.0, 3), "0");
    EXPECT_EQ(shellLoadLabelText(data(ShellLoadType::Uniform, 5), 5), "5 kN/m²");
    EXPECT_EQ(shellLoadLabelText(data(ShellLoadType::VariableX, 5, 15), 15), "15 kN/m²");
    EXPECT_EQ(shellLoadLabelText(data(ShellLoadType::UniformTemperature, 20), 20), "20° C");
    EXPECT_EQ(shellLoadLabelText(data(ShellLoadType::UniformTemperature, 12.5), 12.5), "12.5° C");
    EXPECT_EQ(shellLoadLabelText(data(ShellLoadType::Shrinkage, 0.0005), 0.0005), "0.0005");
}

TEST(ShellLoad, DrawingUniformPrismLiesBelowTheShellForSenseOneAndAboveForSenseTwo) {
    Project p = baseProject();
    const ShellLoadFrame f = frameOf(p);
    const LoadScales scales;
    ShellLoadDrawing dr = shellLoadDrawing(data(ShellLoadType::Uniform, 5), f, scales);
    ASSERT_TRUE(dr.valid);
    ASSERT_EQ(dr.base.size(), 4u);
    ASSERT_EQ(dr.top.size(), 4u);
    for (std::size_t i = 0; i < 4; ++i) {
        EXPECT_EQ(dr.base[i], f.corners[i]);
        EXPECT_NEAR(dr.top[i].z, -0.5, 1e-12);   // button 1 (+Z): the load comes from below, the prism hangs under the shell
        EXPECT_NEAR(dr.top[i].x, f.corners[i].x, 1e-12);
    }
    EXPECT_DOUBLE_EQ(dr.sizeM, 0.5);
    EXPECT_NEAR(dr.direction.z, 1.0, 1e-12);
    // 4 edges of 4 m at 0.5 m: 8 ticks each.
    EXPECT_EQ(dr.ticks.size(), 32u);
    for (const auto& [b, t] : dr.ticks) {
        EXPECT_NEAR(b.z, 0.0, 1e-12);
        EXPECT_NEAR(t.z, -0.5, 1e-12);
    }
    ASSERT_EQ(dr.labels.size(), 1u);
    EXPECT_EQ(dr.labels[0].text, "5 kN/m²");
    EXPECT_NEAR(dr.labels[0].anchor.z, -0.25, 1e-12);

    ShellLoadData d = data(ShellLoadType::Uniform, 5);
    d.sense = ShellLoadSense::MinusZ;
    dr = shellLoadDrawing(d, f, scales);
    for (const Point3& t : dr.top) EXPECT_NEAR(t.z, 0.5, 1e-12);   // button 2: above the shell
    d.axes = ShellLoadAxes::Global;   // same on a horizontal shell
    dr = shellLoadDrawing(d, f, scales);
    for (const Point3& t : dr.top) EXPECT_NEAR(t.z, 0.5, 1e-12);
}

TEST(ShellLoad, DrawingWedgesTemperatureAndShrinkage) {
    Project p = baseProject();
    const ShellLoadFrame f = frameOf(p);
    const LoadScales scales;
    ShellLoadDrawing dr = shellLoadDrawing(data(ShellLoadType::VariableX, 5, 15), f, scales);
    ASSERT_TRUE(dr.valid);
    EXPECT_NEAR(dr.top[0].z, -0.5 / 3.0, 1e-12);   // N1 x = 0: 1/3 of the maximum (measured 19 px ~ 1/3)
    EXPECT_NEAR(dr.top[1].z, -0.5, 1e-12);         // N2 x = 4: the maximum
    EXPECT_NEAR(dr.top[2].z, -0.5, 1e-12);
    EXPECT_NEAR(dr.top[3].z, -0.5 / 3.0, 1e-12);
    EXPECT_DOUBLE_EQ(dr.sizeM, 0.5);
    ASSERT_EQ(dr.labels.size(), 4u);
    EXPECT_EQ(dr.labels[0].text, "5 kN/m²");
    EXPECT_EQ(dr.labels[1].text, "15 kN/m²");

    dr = shellLoadDrawing(data(ShellLoadType::VariableY, 5, 15), f, scales);
    EXPECT_NEAR(dr.top[0].z, -0.5 / 3.0, 1e-12);   // y = 0
    EXPECT_NEAR(dr.top[1].z, -0.5 / 3.0, 1e-12);
    EXPECT_NEAR(dr.top[2].z, -0.5, 1e-12);         // y = 4
    EXPECT_NEAR(dr.top[3].z, -0.5, 1e-12);

    ShellLoadData wedge = data(ShellLoadType::VariableX, 5, 15);
    wedge.sense = ShellLoadSense::MinusZ;
    dr = shellLoadDrawing(wedge, f, scales);
    EXPECT_NEAR(dr.top[1].z, 0.5, 1e-12);   // button 2: the wedge on the shell

    dr = shellLoadDrawing(data(ShellLoadType::UniformTemperature, 20), f, scales);
    ASSERT_TRUE(dr.valid);
    for (const Point3& t : dr.top) EXPECT_NEAR(t.z, -0.5, 1e-12);   // constant height
    ASSERT_EQ(dr.labels.size(), 1u);
    EXPECT_EQ(dr.labels[0].text, "20° C");
    EXPECT_EQ(dr.labels[0].anchor.x, 0.0);   // first corner

    dr = shellLoadDrawing(data(ShellLoadType::Shrinkage, 0.0005), f, scales);
    for (const Point3& t : dr.top) EXPECT_NEAR(t.z, -0.5, 1e-12);
    EXPECT_EQ(dr.labels[0].text, "0.0005");
}

TEST(ShellLoad, DrawingGlobalAxesOnATiltedShellPushesVertically) {
    Project p;
    const int a = p.structure.addNode({0, 0, 0}), b = p.structure.addNode({4, 0, 0}), c = p.structure.addNode({4, 0, 3}), d = p.structure.addNode({0, 0, 3});
    p.structure.addShell({a, b, c, d}, 0.1);   // vertical wall in the xz plane
    const ShellLoadFrame f = frameOf(p);
    ShellLoadData g = data(ShellLoadType::Uniform, 5);
    g.axes = ShellLoadAxes::Global;
    const ShellLoadDrawing dg = shellLoadDrawing(g, f, LoadScales{});
    ASSERT_TRUE(dg.valid);
    for (std::size_t i = 0; i < 4; ++i) EXPECT_NEAR(dg.top[i].z, f.corners[i].z - 0.5, 1e-12);   // prism hangs vertically
    const ShellLoadDrawing dl = shellLoadDrawing(data(ShellLoadType::Uniform, 5), f, LoadScales{});
    for (std::size_t i = 0; i < 4; ++i) {
        EXPECT_NEAR(dl.top[i].z, f.corners[i].z, 1e-12);   // local: perpendicular to the wall
        EXPECT_NEAR(std::fabs(dl.top[i].y - f.corners[i].y), 0.5, 1e-12);
    }
}

TEST(ShellLoad, StackedLoadsArePlacedBeyondThePreviousOnesOnTheSameSide) {
    Project p = baseProject();
    const ShellLoadFrame f = frameOf(p);
    const LoadScales scales;
    const std::vector<ShellLoadData> loads = {data(ShellLoadType::Uniform, 5), data(ShellLoadType::VariableX, 5, 15), data(ShellLoadType::Uniform, 3),
                                              data(ShellLoadType::UniformTemperature, 20)};
    ShellLoadData up = data(ShellLoadType::Uniform, 4);
    up.sense = ShellLoadSense::MinusZ;
    std::vector<ShellLoadData> mixed = loads;
    mixed.insert(mixed.begin() + 1, up);
    const std::vector<double> offsets = shellLoadStackOffsets(loads, scales);
    ASSERT_EQ(offsets.size(), 4u);
    EXPECT_DOUBLE_EQ(offsets[0], 0.0);
    EXPECT_DOUBLE_EQ(offsets[1], 0.5);
    EXPECT_DOUBLE_EQ(offsets[2], 1.0);
    EXPECT_DOUBLE_EQ(offsets[3], 1.5);   // temperature shares the +Z side
    const std::vector<double> m = shellLoadStackOffsets(mixed, scales);
    EXPECT_DOUBLE_EQ(m[1], 0.0);   // the -Z load is alone on its side
    EXPECT_DOUBLE_EQ(m[2], 0.5);   // the second +Z load follows the first one
    EXPECT_TRUE(shellLoadStackOffsets({}, scales).empty());

    const ShellLoadDrawing second = shellLoadDrawing(loads[1], f, scales, offsets[1]);
    EXPECT_NEAR(second.base[0].z, -0.5, 1e-12);   // starts where the first one ends (below it)
    EXPECT_NEAR(second.top[1].z, -1.0, 1e-12);
}

TEST(ShellLoad, DrawingRejectsDegenerateInput) {
    const LoadScales scales;
    EXPECT_FALSE(shellLoadDrawing(data(ShellLoadType::Uniform, 5), ShellLoadFrame{}, scales).valid);
    Project p = baseProject();
    ShellLoadFrame f = frameOf(p);
    f.z = {};
    EXPECT_FALSE(shellLoadDrawing(data(ShellLoadType::Uniform, 5), f, scales).valid);
}

TEST(ShellLoad, SelfWeightDrawingIsAPrismOnTheShellInProportionalMode) {
    Project p = baseProject();
    const ShellLoadFrame f = frameOf(p);
    LoadScales prop;
    prop.mode = LoadScaleMode::Proportional;
    const auto load = shellSelfWeightLoad(*p.structure.findShell(1));
    ASSERT_TRUE(load.has_value());
    const ShellLoadDrawing dr = shellLoadDrawing(*load, f, prop);
    ASSERT_TRUE(dr.valid);
    for (const Point3& t : dr.top) EXPECT_NEAR(t.z, 0.1 * 2.4525, 1e-9);   // 0.1 m x 2.5 kN/m2, above the shell (acts towards -Z)
    EXPECT_EQ(dr.labels[0].text, "2.453 kN/m²");
}
