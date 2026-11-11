#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <set>

#include "core/bar_edit.h"
#include "core/bar_load_edit.h"
#include "core/geom3.h"
#include "core/load_case_edit.h"
#include "core/node_edit.h"
#include "core/piece_edit.h"
#include "core/project_json.h"
#include "core/tools_edit.h"

using namespace cc::core;

namespace {

/// Nodes 1 (0,0,0), 2 (4,0,0), 3 (4,0,3) and bars 1 (1-2, L = 4) and 2 (2-3, L = 3).
Project baseProject() {
    Project p;
    const int n1 = p.structure.addNode({0, 0, 0});
    const int n2 = p.structure.addNode({4, 0, 0});
    const int n3 = p.structure.addNode({4, 0, 3});
    p.structure.addBar(n1, n2);
    p.structure.addBar(n2, n3);
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

BarLoadData data(BarLoadType type, double v1 = 5.0, double v2 = 0.0) {
    BarLoadData d = barLoadDefaults();
    d.type = type;
    d.value1 = v1;
    d.value2 = v2;
    if (type == BarLoadType::UniformTemperature || type == BarLoadType::VariableTemperature) d.axes = BarLoadAxes::Local;
    return d;
}

void expectIssue(const BarLoadIssue& i, BarLoadIssueKind kind, const std::string& message) {
    EXPECT_EQ(i.kind, kind) << message;
    EXPECT_EQ(i.message, message);
}

}  // namespace

// ---------------------------------------------------------------- dialog fields and messages

TEST(BarLoad, TypeValuesAndButtonOrderMatchTheOriginal) {
    EXPECT_EQ(static_cast<int>(BarLoadType::Point), 0);
    EXPECT_EQ(static_cast<int>(BarLoadType::Uniform), 1);
    EXPECT_EQ(static_cast<int>(BarLoadType::Strip), 2);
    EXPECT_EQ(static_cast<int>(BarLoadType::TriangularLeft), 3);
    EXPECT_EQ(static_cast<int>(BarLoadType::TriangularRight), 4);
    EXPECT_EQ(static_cast<int>(BarLoadType::Trapezoidal), 5);
    EXPECT_EQ(static_cast<int>(BarLoadType::PointMoment), 6);
    EXPECT_EQ(static_cast<int>(BarLoadType::VariableTemperature), 7);
    EXPECT_EQ(static_cast<int>(BarLoadType::UniformTemperature), 8);
    // The uniform temperature (8) comes before the variable one (7) in the dialog.
    EXPECT_EQ(kBarLoadTypeButtonOrder[7], BarLoadType::UniformTemperature);
    EXPECT_EQ(kBarLoadTypeButtonOrder[8], BarLoadType::VariableTemperature);
    EXPECT_EQ(barLoadTypeName(BarLoadType::Trapezoidal), "Carga trapecial");
    EXPECT_FALSE(allowsGlobalAxes(BarLoadType::VariableTemperature));
    EXPECT_FALSE(allowsGlobalAxes(BarLoadType::UniformTemperature));
    EXPECT_TRUE(allowsGlobalAxes(BarLoadType::PointMoment));
    EXPECT_FALSE(hasDirection(BarLoadType::UniformTemperature));
    EXPECT_TRUE(hasDirection(BarLoadType::VariableTemperature));
}

TEST(BarLoad, DialogDefaultsAreMeasured) {
    const BarLoadData d = barLoadDefaults();
    EXPECT_EQ(d.type, BarLoadType::Uniform);
    EXPECT_EQ(d.axes, BarLoadAxes::Global);
    EXPECT_EQ(d.direction, LoadDirection::MinusZ);
    EXPECT_DOUBLE_EQ(d.value1, 9.81);
    EXPECT_TRUE(d.relative);
    EXPECT_DOUBLE_EQ(d.start, 0.0);
    EXPECT_DOUBLE_EQ(d.end, 1.0);
    EXPECT_EQ(d.userVector, (Point3{1, 0, 0}));
    EXPECT_EQ(d.temperatureDirection, TemperatureDirection::PlusZ);
    BarLoadSession session;
    EXPECT_TRUE(session.selfWeightActivate);
    EXPECT_EQ(session.last, barLoadDefaults());
}

TEST(BarLoad, FieldSpecsLabelsRangesAndDecimals) {
    BarLoadData d = barLoadDefaults();
    d.type = BarLoadType::Point;
    BarLoadFieldSpec s = barLoadFieldSpec(d, BarLoadField::Value1);
    EXPECT_EQ(s.label, "Valor");
    EXPECT_DOUBLE_EQ(s.min, 0.01);
    EXPECT_DOUBLE_EQ(s.max, 49000.0);
    EXPECT_EQ(s.decimals, 2);
    s = barLoadFieldSpec(d, BarLoadField::Start);
    EXPECT_EQ(s.label, "Distancia");
    EXPECT_DOUBLE_EQ(s.max, 1.0);
    d.relative = false;
    EXPECT_DOUBLE_EQ(barLoadFieldSpec(d, BarLoadField::Start).max, 1000.0);

    for (BarLoadType t : {BarLoadType::Uniform, BarLoadType::Strip, BarLoadType::PointMoment}) {
        d.type = t;
        s = barLoadFieldSpec(d, BarLoadField::Value1);
        EXPECT_EQ(s.label, "Valor");
        EXPECT_DOUBLE_EQ(s.min, 0.001);
        EXPECT_DOUBLE_EQ(s.max, 10000.0);
        EXPECT_EQ(s.decimals, 3);
    }
    d.type = BarLoadType::TriangularLeft;
    EXPECT_EQ(barLoadFieldSpec(d, BarLoadField::Value1).label, "Valor inicial");
    d.type = BarLoadType::Trapezoidal;
    EXPECT_EQ(barLoadFieldSpec(d, BarLoadField::Value1).label, "Valor inicial");
    EXPECT_EQ(barLoadFieldSpec(d, BarLoadField::Value2).label, "Valor final");
    EXPECT_EQ(barLoadFieldSpec(d, BarLoadField::Start).label, "Posición inicial");
    EXPECT_EQ(barLoadFieldSpec(d, BarLoadField::End).label, "Posición final");
    d.type = BarLoadType::UniformTemperature;
    s = barLoadFieldSpec(d, BarLoadField::Value1);
    EXPECT_EQ(s.label, "Valor");
    EXPECT_DOUBLE_EQ(s.min, -100.0);
    EXPECT_DOUBLE_EQ(s.max, 1000.0);
    EXPECT_EQ(s.decimals, 2);
    d.type = BarLoadType::VariableTemperature;
    EXPECT_EQ(barLoadFieldSpec(d, BarLoadField::Value1).label, "Valor 1");
    EXPECT_EQ(barLoadFieldSpec(d, BarLoadField::Value2).label, "Valor 2");
    EXPECT_EQ(barLoadFieldSpec(d, BarLoadField::TemperatureAngle).label, "Ángulo");
}

TEST(BarLoad, MeasuredRangeMessagesAreExact) {
    BarLoadData d = barLoadDefaults();
    EXPECT_EQ(barLoadRangeMessage(barLoadFieldSpec(d, BarLoadField::Value1)), "El valor de 'Valor' debe estar entre 0.001 y 10000.000.");
    d.type = BarLoadType::Point;
    EXPECT_EQ(barLoadRangeMessage(barLoadFieldSpec(d, BarLoadField::Value1)), "El valor de 'Valor' debe estar entre 0.01 y 49000.00.");
    // Relative position: the measured message has doubled spaces ("0.000  y 1.000 .").
    EXPECT_EQ(barLoadRangeMessage(barLoadFieldSpec(d, BarLoadField::Start)), "El valor de 'Distancia' debe estar entre 0.000  y 1.000 .");
    d.relative = false;
    EXPECT_EQ(barLoadRangeMessage(barLoadFieldSpec(d, BarLoadField::Start)), "El valor de 'Distancia' debe estar entre 0.000 y 1000.000.");
    EXPECT_EQ(barLoadRangeMessage(barLoadFieldSpec(d, BarLoadField::VectorX)), "El valor de 'X' debe estar entre -1000.000  y 1000.000 .");
    EXPECT_EQ(barLoadRangeMessage(barLoadFieldSpec(d, BarLoadField::AngleInPlane)),
              "El valor de 'Ángulo en el plano 'XZ' local' debe estar entre -180.000 y 180.000.");
    d.type = BarLoadType::VariableTemperature;
    EXPECT_EQ(barLoadRangeMessage(barLoadFieldSpec(d, BarLoadField::Value2)), "El valor de 'Valor 2' debe estar entre -100.00 y 1000.00.");
    EXPECT_EQ(barLoadZeroMessage(), "El valor introducido no puede ser cero.");
    EXPECT_EQ(barLoadBothZeroMessage(), "Al menos uno de los valores de la carga debe ser distinto de cero.");
    EXPECT_EQ(barLoadDirectionMessage(), "La dirección introducida no es correcta.");
    EXPECT_EQ(barLoadStartBeforeEndMessage(), "El valor de 'Posición inicial' debe ser inferior al de 'Posición final'.");
    EXPECT_EQ(barLoadsOutOfLengthMessage(), "Se han borrado cargas incompatibles con la longitud de la barra.");
    EXPECT_EQ(barLoadsMovedOverBarMessage(), "Al mover una barra sobre otra se pierden sus cargas.");
    EXPECT_EQ(barSelfWeightDisabledMessage(),
              "Se ha desactivado la generación automática de las cargas de peso propio para esta barra.");
}

TEST(BarLoad, ParsingFieldsUsesRoundingAndTheNotANumberMessage) {
    BarLoadData d = barLoadDefaults();
    BarLoadFieldParse r = parseBarLoadField(d, BarLoadField::Value1, "0.0005");  // rounds to 0.001 and is accepted (measured)
    ASSERT_TRUE(r.ok);
    EXPECT_DOUBLE_EQ(r.value, 0.001);
    for (const char* bad : {"0", "-1", "10000.001"}) {
        r = parseBarLoadField(d, BarLoadField::Value1, bad);
        EXPECT_FALSE(r.ok) << bad;
        EXPECT_EQ(r.message, "El valor de 'Valor' debe estar entre 0.001 y 10000.000.") << bad;
    }
    for (const char* bad : {"abc", "", "1e3", "inf", "1.2.3"}) {
        r = parseBarLoadField(d, BarLoadField::Value1, bad);
        EXPECT_FALSE(r.ok) << bad;
        EXPECT_EQ(r.message, "La cadena introducida no es un número.") << bad;
    }
    r = parseBarLoadField(d, BarLoadField::Value1, "9,8765");
    ASSERT_TRUE(r.ok);
    EXPECT_DOUBLE_EQ(r.value, 9.877);
    d.type = BarLoadType::Point;
    EXPECT_FALSE(parseBarLoadField(d, BarLoadField::Value1, "0").ok);
    EXPECT_FALSE(parseBarLoadField(d, BarLoadField::Value1, "49000.01").ok);
    EXPECT_TRUE(parseBarLoadField(d, BarLoadField::Value1, "49000").ok);
    r = parseBarLoadField(d, BarLoadField::Start, "1.5");
    EXPECT_FALSE(r.ok);
    EXPECT_EQ(r.message, "El valor de 'Distancia' debe estar entre 0.000  y 1.000 .");
    d.type = BarLoadType::UniformTemperature;
    EXPECT_TRUE(parseBarLoadField(d, BarLoadField::Value1, "-100").ok);
    EXPECT_TRUE(parseBarLoadField(d, BarLoadField::Value1, "0").ok);  // zero is in range: the business rule gives the error
    EXPECT_FALSE(parseBarLoadField(d, BarLoadField::Value1, "-101").ok);
    EXPECT_FALSE(parseBarLoadField(d, BarLoadField::Value1, "1001").ok);
}

TEST(BarLoad, CheckValuesPerTypeGivesMeasuredWarningsAndErrors) {
    BarLoadData d = data(BarLoadType::Uniform, 0.0);
    expectIssue(checkBarLoadData(d, 4.0), BarLoadIssueKind::Warning, "El valor de 'Valor' debe estar entre 0.001 y 10000.000.");
    d.value1 = -2.0;
    EXPECT_EQ(checkBarLoadData(d, 4.0).kind, BarLoadIssueKind::Warning);  // negative values give the range warning (the sign is the direction's)
    d.value1 = 0.0004;  // rounds to 0
    EXPECT_EQ(checkBarLoadData(d, 4.0).kind, BarLoadIssueKind::Warning);
    d.value1 = 0.0005;  // rounds to 0.001
    EXPECT_TRUE(checkBarLoadData(d, 4.0).ok());
    d.value1 = 10000.0;
    EXPECT_TRUE(checkBarLoadData(d, 4.0).ok());
    d.value1 = 10000.001;
    EXPECT_EQ(checkBarLoadData(d, 4.0).kind, BarLoadIssueKind::Warning);

    d = data(BarLoadType::Point, 0.0);
    expectIssue(checkBarLoadData(d, 4.0), BarLoadIssueKind::Warning, "El valor de 'Valor' debe estar entre 0.01 y 49000.00.");
    d.value1 = 49000.01;
    EXPECT_EQ(checkBarLoadData(d, 4.0).kind, BarLoadIssueKind::Warning);
    d.value1 = 49000;
    EXPECT_TRUE(checkBarLoadData(d, 4.0).ok());

    d = data(BarLoadType::TriangularRight, 0.0);
    expectIssue(checkBarLoadData(d, 4.0), BarLoadIssueKind::Warning, "El valor de 'Valor inicial' debe estar entre 0.001 y 10000.000.");
    d = data(BarLoadType::Trapezoidal, 0.0, 5.0);
    BarLoadIssue i = checkBarLoadData(d, 4.0);
    EXPECT_EQ(i.field, BarLoadField::Value1);
    expectIssue(i, BarLoadIssueKind::Warning, "El valor de 'Valor inicial' debe estar entre 0.001 y 10000.000.");
    d = data(BarLoadType::Trapezoidal, 5.0, -1.0);
    i = checkBarLoadData(d, 4.0);
    EXPECT_EQ(i.field, BarLoadField::Value2);
    expectIssue(i, BarLoadIssueKind::Warning, "El valor de 'Valor final' debe estar entre 0.001 y 10000.000.");
    d = data(BarLoadType::Trapezoidal, 5.0, 10.0);
    EXPECT_TRUE(checkBarLoadData(d, 4.0).ok());
    d = data(BarLoadType::PointMoment, 0.0);
    EXPECT_EQ(checkBarLoadData(d, 4.0).kind, BarLoadIssueKind::Warning);
}

TEST(BarLoad, TemperatureRulesAndErrorKinds) {
    BarLoadData d = data(BarLoadType::UniformTemperature, 0.0);
    expectIssue(checkBarLoadData(d, 4.0), BarLoadIssueKind::Error, "El valor introducido no puede ser cero.");
    d.value1 = 1001;
    expectIssue(checkBarLoadData(d, 4.0), BarLoadIssueKind::Warning, "El valor de 'Valor' debe estar entre -100.00 y 1000.00.");
    d.value1 = -101;
    EXPECT_EQ(checkBarLoadData(d, 4.0).kind, BarLoadIssueKind::Warning);
    d.value1 = -100;
    EXPECT_TRUE(checkBarLoadData(d, 4.0).ok());
    d.value1 = 0.004;  // rounds to 0.00
    EXPECT_EQ(checkBarLoadData(d, 4.0).kind, BarLoadIssueKind::Error);

    d = data(BarLoadType::VariableTemperature, 0.0, 0.0);
    expectIssue(checkBarLoadData(d, 4.0), BarLoadIssueKind::Error, "Al menos uno de los valores de la carga debe ser distinto de cero.");
    for (const auto& [a, b] : {std::pair{0.0, 10.0}, {10.0, 0.0}, {30.0, -30.0}, {-5.0, -7.0}}) {
        d = data(BarLoadType::VariableTemperature, a, b);
        EXPECT_TRUE(checkBarLoadData(d, 4.0).ok()) << a << "/" << b;  // different signs allowed
    }
    d = data(BarLoadType::VariableTemperature, 1001, 5);
    expectIssue(checkBarLoadData(d, 4.0), BarLoadIssueKind::Warning, "El valor de 'Valor 1' debe estar entre -100.00 y 1000.00.");
    d = data(BarLoadType::VariableTemperature, 5, -101);
    expectIssue(checkBarLoadData(d, 4.0), BarLoadIssueKind::Warning, "El valor de 'Valor 2' debe estar entre -100.00 y 1000.00.");
    d = data(BarLoadType::VariableTemperature, 5, 5);
    d.temperatureDirection = TemperatureDirection::Angle;
    d.temperatureAngle = 181;
    expectIssue(checkBarLoadData(d, 4.0), BarLoadIssueKind::Warning, "El valor de 'Ángulo' debe estar entre -180.000 y 180.000.");
    d.temperatureAngle = -180;
    EXPECT_TRUE(checkBarLoadData(d, 4.0).ok());
}

TEST(BarLoad, PositionsRelativeAbsoluteAndLength) {
    BarLoadData d = data(BarLoadType::Point, 20);
    d.start = 1.5;
    expectIssue(checkBarLoadData(d, 4.0), BarLoadIssueKind::Warning, "El valor de 'Distancia' debe estar entre 0.000  y 1.000 .");
    d.start = -0.1;
    EXPECT_EQ(checkBarLoadData(d, 4.0).kind, BarLoadIssueKind::Warning);
    d.start = 1.0;
    EXPECT_TRUE(checkBarLoadData(d, 4.0).ok());

    d.relative = false;
    d.start = 5.0;
    expectIssue(checkBarLoadData(d, 4.0), BarLoadIssueKind::Warning, "El valor de 'Distancia' debe estar entre 0.000 y 4.000.");
    d.start = 4.0001;  // measured: fails
    expectIssue(checkBarLoadData(d, 4.0), BarLoadIssueKind::Warning, "El valor de 'Distancia' debe estar entre 0.000 y 4.000.");
    d.start = 4.0;
    EXPECT_TRUE(checkBarLoadData(d, 4.0).ok());
    d.start = -1.0;
    expectIssue(checkBarLoadData(d, 4.0), BarLoadIssueKind::Warning, "El valor de 'Distancia' debe estar entre 0.000 y 1000.000.");
    d.start = 1001;
    expectIssue(checkBarLoadData(d, 0.0), BarLoadIssueKind::Warning, "El valor de 'Distancia' debe estar entre 0.000 y 1000.000.");
    d.start = 50.0;
    EXPECT_TRUE(checkBarLoadData(d, 0.0).ok());  // no length given: only the control range

    // Strip: start must be below end (Error), also with equal positions; absolute end beyond L.
    d = data(BarLoadType::Strip, 5);
    d.start = 0.6;
    d.end = 0.4;
    expectIssue(checkBarLoadData(d, 4.0), BarLoadIssueKind::Error, "El valor de 'Posición inicial' debe ser inferior al de 'Posición final'.");
    d.start = d.end = 0.5;
    EXPECT_EQ(checkBarLoadData(d, 4.0).kind, BarLoadIssueKind::Error);
    d.start = 0.4;
    d.end = 0.6;
    EXPECT_TRUE(checkBarLoadData(d, 4.0).ok());
    d.relative = false;
    d.start = 1.0;
    d.end = 4.5;
    expectIssue(checkBarLoadData(d, 4.0), BarLoadIssueKind::Warning, "El valor de 'Posición final' debe estar entre 0.000 y 4.000.");
    d.end = 4.0;
    EXPECT_TRUE(checkBarLoadData(d, 4.0).ok());
}

TEST(BarLoad, DirectionChecks) {
    BarLoadData d = data(BarLoadType::Uniform, 5);
    d.direction = LoadDirection::UserVector;
    d.userVector = {0, 0, 0};
    expectIssue(checkBarLoadData(d, 4.0), BarLoadIssueKind::Warning, "La dirección introducida no es correcta.");
    d.userVector = {0.0001, 0, 0};  // rounds to 0.000
    EXPECT_EQ(checkBarLoadData(d, 4.0).kind, BarLoadIssueKind::Warning);
    d.userVector = {1001, 0, 0};
    expectIssue(checkBarLoadData(d, 4.0), BarLoadIssueKind::Warning, "El valor de 'X' debe estar entre -1000.000  y 1000.000 .");
    EXPECT_EQ(checkBarLoadData(d, 4.0).field, BarLoadField::VectorX);
    d.userVector = {0, -1001, 0};
    EXPECT_EQ(checkBarLoadData(d, 4.0).field, BarLoadField::VectorY);
    d.userVector = {NAN, 0, 0};
    EXPECT_EQ(checkBarLoadData(d, 4.0).kind, BarLoadIssueKind::Warning);
    d.userVector = {1, -2, 0.5};
    EXPECT_TRUE(checkBarLoadData(d, 4.0).ok());

    d.axes = BarLoadAxes::Local;
    d.angleInPlaneXz = 181;
    expectIssue(checkBarLoadData(d, 4.0), BarLoadIssueKind::Warning, "El valor de 'Ángulo en el plano 'XZ' local' debe estar entre -180.000 y 180.000.");
    d.angleInPlaneXz = 0;
    d.angleToPlaneXz = -181;
    expectIssue(checkBarLoadData(d, 4.0), BarLoadIssueKind::Warning, "El valor de 'Ángulo con el plano 'XZ' local' debe estar entre -180.000 y 180.000.");
    d.angleToPlaneXz = 90;
    EXPECT_TRUE(checkBarLoadData(d, 4.0).ok());   // a null global vector is irrelevant for local axes
    d.userVector = {0, 0, 0};
    EXPECT_TRUE(checkBarLoadData(d, 4.0).ok());

    d.direction = static_cast<LoadDirection>(42);
    EXPECT_EQ(checkBarLoadData(d, 4.0).kind, BarLoadIssueKind::Warning);
    d = data(BarLoadType::Uniform, 5);
    d.type = static_cast<BarLoadType>(42);
    EXPECT_EQ(checkBarLoadData(d, 4.0).kind, BarLoadIssueKind::Error);
}

TEST(BarLoad, NormaliseDataToCanonicalForm) {
    BarLoadData d = data(BarLoadType::Uniform, 5.0004, 7);
    d.relative = false;
    d.start = 2;
    d.end = 3;
    d.temperatureDirection = TemperatureDirection::Angle;
    d.temperatureAngle = 33;
    d.userVector = {5, 5, 5};
    d.angleInPlaneXz = 10;
    BarLoadData n = normaliseBarLoadData(d);
    EXPECT_DOUBLE_EQ(n.value1, 5.0);
    EXPECT_DOUBLE_EQ(n.value2, 0.0);
    EXPECT_TRUE(n.relative);
    EXPECT_DOUBLE_EQ(n.start, 0.0);
    EXPECT_DOUBLE_EQ(n.end, 1.0);
    EXPECT_EQ(n.temperatureDirection, TemperatureDirection::PlusZ);
    EXPECT_DOUBLE_EQ(n.temperatureAngle, 0.0);
    EXPECT_EQ(n.userVector, (Point3{1, 0, 0}));  // preset direction: user fields reset
    EXPECT_DOUBLE_EQ(n.angleInPlaneXz, 0.0);
    EXPECT_EQ(normaliseBarLoadData(n), n);

    d = data(BarLoadType::Point, 12.345);
    d.start = 0.12345;
    n = normaliseBarLoadData(d);
    EXPECT_DOUBLE_EQ(n.value1, 12.35);   // point forces keep 2 decimals
    EXPECT_DOUBLE_EQ(n.start, 0.123);
    EXPECT_DOUBLE_EQ(n.end, 1.0);

    d = data(BarLoadType::UniformTemperature, 20.004);
    d.axes = BarLoadAxes::Global;
    d.direction = LoadDirection::PlusX;
    n = normaliseBarLoadData(d);
    EXPECT_EQ(n.axes, BarLoadAxes::Local);
    EXPECT_EQ(n.direction, LoadDirection::MinusZ);
    EXPECT_DOUBLE_EQ(n.value1, 20.0);

    d = data(BarLoadType::VariableTemperature, 30, -30);
    d.axes = BarLoadAxes::Global;
    d.temperatureDirection = TemperatureDirection::Angle;
    d.temperatureAngle = 33.3333;
    n = normaliseBarLoadData(d);
    EXPECT_EQ(n.axes, BarLoadAxes::Local);
    EXPECT_DOUBLE_EQ(n.temperatureAngle, 33.333);
    EXPECT_DOUBLE_EQ(n.value2, -30.0);

    d = data(BarLoadType::TriangularLeft, 5, 9);  // value2 is not used by triangulars
    EXPECT_DOUBLE_EQ(normaliseBarLoadData(d).value2, 0.0);

    d = data(BarLoadType::Uniform, 5);
    d.axes = BarLoadAxes::Local;
    d.direction = LoadDirection::UserVector;
    d.userVector = {3, 3, 3};
    d.angleInPlaneXz = 12.34567;
    n = normaliseBarLoadData(d);
    EXPECT_EQ(n.userVector, (Point3{1, 0, 0}));
    EXPECT_DOUBLE_EQ(n.angleInPlaneXz, 12.346);
    d.axes = BarLoadAxes::Global;
    n = normaliseBarLoadData(d);
    EXPECT_DOUBLE_EQ(n.angleInPlaneXz, 0.0);
    EXPECT_EQ(n.userVector, (Point3{3, 3, 3}));

    d = data(BarLoadType::PointMoment, 3);
    d.axes = BarLoadAxes::Global;
    EXPECT_EQ(normaliseBarLoadData(d).axes, BarLoadAxes::Global);  // moments take global axes
}

TEST(BarLoad, LinearTypeNormalisationFollowsTheConstructorRule) {
    // Null initial or final value: triangular, the other one is the maximum.
    LinearLoadShape s = normaliseLinearLoad(0, 4, 5, 0, 4);
    EXPECT_EQ(s.type, BarLoadType::TriangularLeft);
    EXPECT_DOUBLE_EQ(s.value1, 5.0);
    s = normaliseLinearLoad(0, 4, 0, 7, 4);
    EXPECT_EQ(s.type, BarLoadType::TriangularRight);
    EXPECT_DOUBLE_EQ(s.value1, 7.0);
    s = normaliseLinearLoad(0, 4, 5, 0.00005, 4);   // within 1e-4: null
    EXPECT_EQ(s.type, BarLoadType::TriangularLeft);
    // Equal values: uniform when the whole bar is covered, strip otherwise.
    s = normaliseLinearLoad(0, 4, 5, 5, 4);
    EXPECT_EQ(s.type, BarLoadType::Uniform);
    s = normaliseLinearLoad(0.0005, 3.9995, 5, 5.00005, 4);
    EXPECT_EQ(s.type, BarLoadType::Uniform);   // within the 1e-3 position tolerance and 1e-4 value tolerance
    s = normaliseLinearLoad(0.5, 4, 5, 5, 4);
    EXPECT_EQ(s.type, BarLoadType::Strip);
    s = normaliseLinearLoad(0, 3.5, 5, 5, 4);
    EXPECT_EQ(s.type, BarLoadType::Strip);
    EXPECT_DOUBLE_EQ(s.value1, 5.0);
    // Otherwise trapezoidal.
    s = normaliseLinearLoad(0, 4, 5, 10, 4);
    EXPECT_EQ(s.type, BarLoadType::Trapezoidal);
    EXPECT_DOUBLE_EQ(s.value1, 5.0);
    EXPECT_DOUBLE_EQ(s.value2, 10.0);
    s = normaliseLinearLoad(1, 2, 5, 5.01, 4);
    EXPECT_EQ(s.type, BarLoadType::Trapezoidal);
}

// ---------------------------------------------------------------- extents, directions, scales, drawing

TEST(BarLoad, ExtentPerType) {
    BarLoadData d = data(BarLoadType::Point, 8);
    d.start = 0.25;
    BarLoadExtent e = barLoadExtent(d, 4.0);
    EXPECT_DOUBLE_EQ(e.start, 1.0);
    EXPECT_DOUBLE_EQ(e.end, 1.0);
    d.relative = false;
    d.start = 3.0;
    EXPECT_DOUBLE_EQ(barLoadExtent(d, 4.0).start, 3.0);
    d.start = 9.0;
    EXPECT_DOUBLE_EQ(barLoadExtent(d, 4.0).start, 4.0);  // clamped to the bar

    e = barLoadExtent(data(BarLoadType::Uniform, 5), 4.0);
    EXPECT_DOUBLE_EQ(e.start, 0.0);
    EXPECT_DOUBLE_EQ(e.end, 4.0);
    EXPECT_DOUBLE_EQ(e.valueStart, 5.0);
    EXPECT_DOUBLE_EQ(e.valueEnd, 5.0);

    d = data(BarLoadType::Strip, 5);
    d.start = 0.25;
    d.end = 0.5;
    e = barLoadExtent(d, 4.0);
    EXPECT_DOUBLE_EQ(e.start, 1.0);
    EXPECT_DOUBLE_EQ(e.end, 2.0);
    d.type = BarLoadType::TriangularLeft;
    e = barLoadExtent(d, 4.0);
    EXPECT_DOUBLE_EQ(e.valueStart, 5.0);
    EXPECT_DOUBLE_EQ(e.valueEnd, 0.0);
    d.type = BarLoadType::TriangularRight;
    e = barLoadExtent(d, 4.0);
    EXPECT_DOUBLE_EQ(e.valueStart, 0.0);
    EXPECT_DOUBLE_EQ(e.valueEnd, 5.0);
    d = data(BarLoadType::Trapezoidal, 5, 10);
    e = barLoadExtent(d, 4.0);
    EXPECT_DOUBLE_EQ(e.valueStart, 5.0);
    EXPECT_DOUBLE_EQ(e.valueEnd, 10.0);
    e = barLoadExtent(data(BarLoadType::VariableTemperature, 30, -30), 4.0);
    EXPECT_DOUBLE_EQ(e.end, 4.0);
    EXPECT_DOUBLE_EQ(e.valueEnd, -30.0);
}

TEST(BarLoad, LocalAxesAndDirections) {
    Project p = baseProject();
    // Bar 1 runs along +X: local x = +X, z = +Z (up), y = z x x = +Y.
    auto f = barLocalAxes(p.structure, 1);
    ASSERT_TRUE(f.has_value());
    EXPECT_NEAR(f->length, 4.0, 1e-12);
    EXPECT_NEAR(f->x.x, 1.0, 1e-12);
    EXPECT_NEAR(f->y.y, 1.0, 1e-12);
    EXPECT_NEAR(f->z.z, 1.0, 1e-12);
    EXPECT_FALSE(barLocalAxes(p.structure, 99).has_value());

    BarLoadData d = data(BarLoadType::Uniform, 5);   // global -Z
    EXPECT_EQ(barLoadDirection(d, *f), (Point3{0, 0, -1}));
    d.axes = BarLoadAxes::Local;
    d.direction = LoadDirection::MinusZ;  // with the bar along X the local -Z is the global -Z (measured)
    const Point3 dz = barLoadDirection(d, *f);
    EXPECT_NEAR(dz.z, -1.0, 1e-12);
    d.direction = LoadDirection::PlusY;
    EXPECT_NEAR(barLoadDirection(d, *f).y, 1.0, 1e-12);
    d.direction = LoadDirection::MinusX;
    EXPECT_NEAR(barLoadDirection(d, *f).x, -1.0, 1e-12);
    // Local user angles: alpha in the XZ plane from +x towards +z, beta from that plane towards +y.
    d.direction = LoadDirection::UserVector;
    d.angleInPlaneXz = 90;
    d.angleToPlaneXz = 0;
    EXPECT_NEAR(barLoadDirection(d, *f).z, 1.0, 1e-12);
    d.angleInPlaneXz = 0;
    d.angleToPlaneXz = 90;
    EXPECT_NEAR(barLoadDirection(d, *f).y, 1.0, 1e-12);
    d.angleToPlaneXz = 45;
    const Point3 d45 = barLoadDirection(d, *f);
    EXPECT_NEAR(d45.x, std::sqrt(0.5), 1e-12);
    EXPECT_NEAR(d45.y, std::sqrt(0.5), 1e-12);

    // Global user vector is normalised; a null one gives no direction.
    d = data(BarLoadType::Uniform, 5);
    d.direction = LoadDirection::UserVector;
    d.userVector = {0, 3, 4};
    const Point3 u = barLoadDirection(d, *f);
    EXPECT_NEAR(u.y, 0.6, 1e-12);
    EXPECT_NEAR(u.z, 0.8, 1e-12);
    d.userVector = {0, 0, 0};
    EXPECT_EQ(barLoadDirection(d, *f), Point3{});

    // Temperatures: variable gives the variation direction (Y+, Z+, angle); uniform has none.
    d = data(BarLoadType::VariableTemperature, 30, -30);
    d.temperatureDirection = TemperatureDirection::PlusY;
    EXPECT_NEAR(barLoadDirection(d, *f).y, 1.0, 1e-12);
    d.temperatureDirection = TemperatureDirection::PlusZ;
    EXPECT_NEAR(barLoadDirection(d, *f).z, 1.0, 1e-12);
    d.temperatureDirection = TemperatureDirection::Angle;
    d.temperatureAngle = 90;
    EXPECT_NEAR(barLoadDirection(d, *f).y, 1.0, 1e-12);
    EXPECT_EQ(barLoadDirection(data(BarLoadType::UniformTemperature, 20), *f), Point3{});

    // Roll and x inversion.
    p.structure.findBar(1)->rollAngleDeg = 90;
    f = barLocalAxes(p.structure, 1);
    EXPECT_NEAR(f->z.y, -1.0, 1e-12);   // sign of the roll: not measured (documented)
    p.structure.findBar(1)->rollAngleDeg = 0;
    p.structure.findBar(1)->xAxisInverted = true;
    f = barLocalAxes(p.structure, 1);
    EXPECT_NEAR(f->x.x, -1.0, 1e-12);
    EXPECT_NEAR(f->z.z, 1.0, 1e-12);
    // Vertical bar 2 (up): documented provisional frame, orthonormal and right handed.
    f = barLocalAxes(p.structure, 2);
    ASSERT_TRUE(f.has_value());
    EXPECT_NEAR(f->x.z, 1.0, 1e-12);
    const Point3 c = geom::cross(f->x, f->y);
    EXPECT_NEAR(c.x, f->z.x, 1e-12);
    EXPECT_NEAR(c.y, f->z.y, 1e-12);
    EXPECT_NEAR(c.z, f->z.z, 1e-12);
}

TEST(BarLoad, DrawSizesFixedAndProportional) {
    LoadScales fixedScales;
    BarLoadData u = data(BarLoadType::Uniform, 5);
    // Fixed mode: every load reaches the maximum size whatever its value (0.5 m measured for 5 and 10).
    EXPECT_DOUBLE_EQ(barLoadDrawSizeM(fixedScales, u, 5, 5), 0.5);
    EXPECT_DOUBLE_EQ(barLoadDrawSizeM(fixedScales, u, 10, 10), 0.5);
    BarLoadData t = data(BarLoadType::Trapezoidal, 5, 10);
    EXPECT_DOUBLE_EQ(barLoadDrawSizeM(fixedScales, t, 5, 10), 0.25);   // other end of a wedge in proportion
    fixedScales.fixedSizeM = 1.0;
    EXPECT_DOUBLE_EQ(barLoadDrawSizeM(fixedScales, u, 5, 5), 1.0);

    LoadScales prop;
    prop.mode = LoadScaleMode::Proportional;
    EXPECT_DOUBLE_EQ(barLoadDrawSizeM(prop, u, 5, 5), 0.5);    // 0.1 m per kN/m
    EXPECT_DOUBLE_EQ(barLoadDrawSizeM(prop, u, 10, 10), 1.0);
    EXPECT_DOUBLE_EQ(barLoadDrawSizeM(prop, t, 5, 10), 0.5);   // trapezoidal 5 -> 10 is a wedge 0.5 -> 1.0
    EXPECT_DOUBLE_EQ(barLoadDrawSizeM(prop, t, 10, 10), 1.0);
    BarLoadData m = data(BarLoadType::PointMoment, 5);
    EXPECT_DOUBLE_EQ(barLoadDrawSizeM(prop, m, 5, 5), 0.5);
    // Case scale and type scale multiply; the "Uniforme" scale does not alter a trapezoidal load (measured).
    prop.perType[static_cast<std::size_t>(LoadScaleType::Uniform)] = 2.0;
    EXPECT_DOUBLE_EQ(barLoadDrawSizeM(prop, u, 5, 5), 1.0);
    EXPECT_DOUBLE_EQ(barLoadDrawSizeM(prop, t, 5, 10), 0.5);
    u.loadCaseId = 7;
    prop.perCase[7] = 1.5;
    EXPECT_DOUBLE_EQ(barLoadDrawSizeM(prop, u, 5, 5), 1.5);
    EXPECT_EQ(barLoadScaleType(BarLoadType::Point), LoadScaleType::Punctual);
    EXPECT_EQ(barLoadScaleType(BarLoadType::Strip), LoadScaleType::Strip);
    EXPECT_EQ(barLoadScaleType(BarLoadType::TriangularLeft), LoadScaleType::Triangular);
    EXPECT_EQ(barLoadScaleType(BarLoadType::TriangularRight), LoadScaleType::Triangular);
    EXPECT_EQ(barLoadScaleType(BarLoadType::Trapezoidal), LoadScaleType::Trapezoidal);
    EXPECT_EQ(barLoadScaleType(BarLoadType::PointMoment), LoadScaleType::PointMoment);
    EXPECT_EQ(barLoadScaleType(BarLoadType::UniformTemperature), LoadScaleType::Temperature);
    EXPECT_EQ(barLoadScaleType(BarLoadType::VariableTemperature), LoadScaleType::Temperature);
}

TEST(BarLoad, LabelsAreAbsoluteWithoutUnit) {
    EXPECT_EQ(barLoadValueLabel(20), "20");
    EXPECT_EQ(barLoadValueLabel(7), "7");
    EXPECT_EQ(barLoadValueLabel(9.81), "9.81");
    EXPECT_EQ(barLoadValueLabel(100), "100");
    EXPECT_EQ(barLoadValueLabel(0), "0");
    EXPECT_EQ(barLoadValueLabel(-30), "-30");
    EXPECT_EQ(barLoadValueLabel(0.001), "0.001");
    EXPECT_EQ(barLoadLabel(data(BarLoadType::Uniform, 5)), "5");
    EXPECT_EQ(barLoadLabel(data(BarLoadType::UniformTemperature, 20)), "20/20");
    EXPECT_EQ(barLoadLabel(data(BarLoadType::VariableTemperature, 30, -30)), "30/-30");
}

namespace {
BarFrame frameAlongX(double length = 4.0) {
    BarFrame f;
    f.origin = {0, 0, 0};
    f.x = {1, 0, 0};
    f.y = {0, 1, 0};
    f.z = {0, 0, 1};
    f.length = length;
    return f;
}
}  // namespace

TEST(BarLoad, DrawingUniformBandCoversTheBarAndProjectsInTheLoadDirection) {
    LoadScales scales;   // fixed 0.5 m
    const BarLoadDrawing g = barLoadDrawing(data(BarLoadType::Uniform, 5), frameAlongX(), scales);
    ASSERT_TRUE(g.valid);
    ASSERT_EQ(g.outline.size(), 4u);
    EXPECT_EQ(g.outline[0], (Point3{0, 0, 0}));
    EXPECT_EQ(g.outline[1], (Point3{4, 0, 0}));
    EXPECT_EQ(g.outline[2], (Point3{4, 0, -0.5}));   // default direction -Z
    EXPECT_EQ(g.outline[3], (Point3{0, 0, -0.5}));
    EXPECT_DOUBLE_EQ(g.sizeM, 0.5);
    ASSERT_EQ(g.labels.size(), 1u);
    EXPECT_EQ(g.labels[0].text, "5");
    EXPECT_NEAR(g.labels[0].anchor.x, 2.0, 1e-12);
    // Regular perpendicular lines from one end to the other.
    ASSERT_GE(g.ticks.size(), 2u);
    EXPECT_EQ(g.ticks.front().first, (Point3{0, 0, 0}));
    EXPECT_EQ(g.ticks.back().first, (Point3{4, 0, 0}));
    EXPECT_NEAR(g.ticks.back().second.z, -0.5, 1e-12);
    EXPECT_EQ(g.ticks.size(), 17u);   // 4 m / 0.25 m + 1
}

TEST(BarLoad, DrawingStripAndTriangularAndTrapezoidal) {
    LoadScales prop;
    prop.mode = LoadScaleMode::Proportional;
    BarLoadData d = data(BarLoadType::Strip, 10);
    d.start = 0.25;
    d.end = 0.5;
    BarLoadDrawing g = barLoadDrawing(d, frameAlongX(), prop);
    ASSERT_TRUE(g.valid);
    EXPECT_EQ(g.outline[0], (Point3{1, 0, 0}));
    EXPECT_EQ(g.outline[1], (Point3{2, 0, 0}));
    EXPECT_NEAR(g.outline[2].z, -1.0, 1e-12);
    ASSERT_EQ(g.labels.size(), 2u);   // labels at both ends of the strip
    EXPECT_EQ(g.labels[0].text, "10");
    EXPECT_NEAR(g.labels[0].anchor.x, 1.0, 1e-12);
    EXPECT_NEAR(g.labels[1].anchor.x, 2.0, 1e-12);

    d.type = BarLoadType::TriangularLeft;
    g = barLoadDrawing(d, frameAlongX(), prop);
    EXPECT_NEAR(g.outline[3].z, -1.0, 1e-12);   // maximum at the start
    EXPECT_NEAR(g.outline[2].z, 0.0, 1e-12);
    ASSERT_EQ(g.labels.size(), 1u);
    EXPECT_NEAR(g.labels[0].anchor.x, 1.0, 1e-12);
    d.type = BarLoadType::TriangularRight;
    g = barLoadDrawing(d, frameAlongX(), prop);
    EXPECT_NEAR(g.outline[3].z, 0.0, 1e-12);
    EXPECT_NEAR(g.outline[2].z, -1.0, 1e-12);
    EXPECT_NEAR(g.labels[0].anchor.x, 2.0, 1e-12);

    d = data(BarLoadType::Trapezoidal, 5, 10);   // wedge 0.5 -> 1.0 m (measured)
    d.start = 0;
    d.end = 1;
    g = barLoadDrawing(d, frameAlongX(), prop);
    EXPECT_NEAR(g.outline[3].z, -0.5, 1e-12);
    EXPECT_NEAR(g.outline[2].z, -1.0, 1e-12);
    ASSERT_EQ(g.labels.size(), 2u);
    EXPECT_EQ(g.labels[0].text, "5");
    EXPECT_EQ(g.labels[1].text, "10");
    EXPECT_NEAR(g.ticks[g.ticks.size() / 2].second.z, -0.75, 1e-9);   // linear height in between
}

TEST(BarLoad, DrawingPointArrowAndMomentArc) {
    LoadScales scales;
    BarLoadData d = data(BarLoadType::Point, 20);
    d.start = 0.5;
    BarLoadDrawing g = barLoadDrawing(d, frameAlongX(), scales);
    ASSERT_TRUE(g.valid);
    EXPECT_EQ(g.arrow.second, (Point3{2, 0, 0}));        // tip on the bar
    EXPECT_EQ(g.arrow.first, (Point3{2, 0, 0.5}));       // drawn from the side the load comes from (-Z load: above)
    ASSERT_EQ(g.labels.size(), 1u);
    EXPECT_EQ(g.labels[0].text, "20");

    LoadScales prop;
    prop.mode = LoadScaleMode::Proportional;
    g = barLoadDrawing(d, frameAlongX(), prop);
    EXPECT_NEAR(g.arrow.first.z, 2.0, 1e-12);            // 0.1 m per kN

    d = data(BarLoadType::PointMoment, 5);
    d.start = 0.5;
    d.direction = LoadDirection::PlusY;                  // moment about +Y
    g = barLoadDrawing(d, frameAlongX(), prop);
    ASSERT_TRUE(g.valid);
    EXPECT_DOUBLE_EQ(g.sizeM, 0.5);
    ASSERT_EQ(g.arc.size(), 17u);
    for (const Point3& q : g.arc) {   // radius 0.5 m around the point (2, 0, 0), in the plane perpendicular to the axis
        EXPECT_NEAR(std::hypot(q.x - 2.0, q.z), 0.5, 1e-12);
        EXPECT_NEAR(q.y, 0.0, 1e-12);
    }
    // The semicircle closes at both ends of a diameter.
    EXPECT_NEAR(geom::length(geom::sub(g.arc.front(), g.arc.back())), 1.0, 1e-12);
    EXPECT_EQ(g.labels[0].text, "5");

    d.direction = LoadDirection::UserVector;
    d.userVector = {0, 0, 0};
    EXPECT_FALSE(barLoadDrawing(d, frameAlongX(), prop).valid);
    EXPECT_FALSE(barLoadDrawing(data(BarLoadType::Uniform, 5), frameAlongX(0.0), prop).valid);
}

TEST(BarLoad, DrawingTemperatureHasConstantWidthAndT1T2Label) {
    LoadScales scales;
    BarLoadDrawing g = barLoadDrawing(data(BarLoadType::UniformTemperature, 20), frameAlongX(), scales);
    ASSERT_TRUE(g.valid);
    EXPECT_NEAR(g.outline[2].z, 0.5, 1e-12);   // towards local +Z
    EXPECT_NEAR(g.outline[3].z, 0.5, 1e-12);
    ASSERT_EQ(g.labels.size(), 1u);
    EXPECT_EQ(g.labels[0].text, "20/20");

    g = barLoadDrawing(data(BarLoadType::VariableTemperature, 30, -30), frameAlongX(), scales);
    ASSERT_TRUE(g.valid);
    EXPECT_NEAR(g.outline[2].z, 0.5, 1e-12);    // no wedge although T1 != T2
    EXPECT_EQ(g.labels[0].text, "30/-30");
    // Fixed mode: 20 and 30 draw the same width (0.5 m, measured).
    EXPECT_NEAR(barLoadDrawing(data(BarLoadType::UniformTemperature, 30), frameAlongX(), scales).outline[2].z, 0.5, 1e-12);
    LoadScales prop;
    prop.mode = LoadScaleMode::Proportional;
    g = barLoadDrawing(data(BarLoadType::VariableTemperature, 30, -30), frameAlongX(), prop);
    EXPECT_NEAR(g.outline[2].z, 30.0, 1e-12);   // measured: 1.0 m per degree (10 degC x 1.000 x 1.000 -> 10.1 m in the original)
    prop.perType[static_cast<std::size_t>(LoadScaleType::Temperature)] = 0.1;
    prop.perCase[0] = 3.0;
    EXPECT_NEAR(barLoadDrawing(data(BarLoadType::UniformTemperature, 10), frameAlongX(), prop).outline[2].z, 3.0, 1e-12);   // 10 x 1.0 x 0.1 x 3 (measured linear in both scales)
}

TEST(BarLoad, DrawingInGlobalAxesIgnoresTheBarOrientation) {
    // A vertical bar 3 m long loaded with a global -X uniform load: the band is offset along -X.
    Project p = baseProject();
    const auto f = barLocalAxes(p.structure, 2);
    ASSERT_TRUE(f.has_value());
    BarLoadData d = data(BarLoadType::Uniform, 5);
    d.direction = LoadDirection::MinusX;
    const BarLoadDrawing g = barLoadDrawing(d, *f, LoadScales{});
    ASSERT_TRUE(g.valid);
    EXPECT_EQ(g.outline[0], (Point3{4, 0, 0}));
    EXPECT_EQ(g.outline[1], (Point3{4, 0, 3}));
    EXPECT_NEAR(g.outline[2].x, 3.5, 1e-12);
}

// ---------------------------------------------------------------- commands

TEST(BarLoad, AddToSeveralBarsNeverMergesAndUndoes) {
    Project p = baseProject();
    UndoStack stack;
    std::vector<int> ids;
    BarLoadIssue issue;
    ASSERT_EQ(addBarLoads(p, stack, {1, 2, 2, 99}, data(BarLoadType::Uniform, 5), &ids, &issue), BarLoadError::None);
    EXPECT_TRUE(issue.ok());
    EXPECT_EQ(ids, (std::vector<int>{1, 2}));
    ASSERT_EQ(addBarLoads(p, stack, {1}, data(BarLoadType::Uniform, 5)), BarLoadError::None);   // a second identical load stays separate
    EXPECT_EQ(p.barLoads.items.size(), 3u);
    EXPECT_EQ(barLoadsOfBar(p.barLoads, 1), (std::vector<int>{1, 3}));
    EXPECT_EQ(barLoadsOfCase(p.barLoads, 0), (std::vector<int>{1, 2, 3}));
    EXPECT_TRUE(hasBarLoads(p.barLoads));
    ASSERT_NE(findBarLoad(p.barLoads, 2), nullptr);
    EXPECT_EQ(findBarLoad(p.barLoads, 2)->barId, 2);
    EXPECT_EQ(findBarLoad(p.barLoads, 42), nullptr);
    stack.undo();
    EXPECT_EQ(p.barLoads.items.size(), 2u);
    stack.undo();
    EXPECT_FALSE(hasBarLoads(p.barLoads));
    EXPECT_EQ(p.barLoads.nextId, 4);   // ids are never reused
    stack.redo();
    EXPECT_EQ(p.barLoads.items.size(), 2u);
    EXPECT_EQ(p.barLoads.items[0].id, 1);
}

TEST(BarLoad, AddChecksDataLoadCaseAndBarsAndRemembersTheLastValues) {
    Project p = baseProject();
    UndoStack stack;
    BarLoadIssue issue;
    BarLoadData bad = data(BarLoadType::Uniform, 0.0);
    EXPECT_EQ(addBarLoads(p, stack, {1}, bad, nullptr, &issue), BarLoadError::InvalidData);
    EXPECT_EQ(issue.kind, BarLoadIssueKind::Warning);
    EXPECT_EQ(issue.message, "El valor de 'Valor' debe estar entre 0.001 y 10000.000.");
    BarLoadData d = data(BarLoadType::Uniform, 5);
    d.loadCaseId = 77;
    EXPECT_EQ(addBarLoads(p, stack, {1}, d), BarLoadError::UnknownLoadCase);
    EXPECT_EQ(addBarLoads(p, stack, {99}, data(BarLoadType::Uniform, 5)), BarLoadError::NoBars);
    EXPECT_EQ(addBarLoads(p, stack, {}, data(BarLoadType::Uniform, 5)), BarLoadError::NoBars);
    EXPECT_FALSE(hasBarLoads(p.barLoads));
    EXPECT_EQ(stack.canUndo(), false);

    // Absolute position against the SHORTEST selected bar (bar 2 is 3 m long).
    d = data(BarLoadType::Point, 20);
    d.relative = false;
    d.start = 3.5;
    EXPECT_EQ(addBarLoads(p, stack, {1, 2}, d, nullptr, &issue), BarLoadError::InvalidData);
    EXPECT_EQ(issue.message, "El valor de 'Distancia' debe estar entre 0.000 y 3.000.");
    EXPECT_EQ(issue.field, BarLoadField::Start);
    EXPECT_EQ(addBarLoads(p, stack, {1}, d, nullptr, &issue), BarLoadError::None);

    BarLoadSession session;
    d = data(BarLoadType::Trapezoidal, 5, 10);
    ASSERT_EQ(addBarLoads(p, stack, {1}, d, nullptr, nullptr, &session), BarLoadError::None);
    EXPECT_EQ(session.last, d);   // type and values of the session are remembered (measured)
    EXPECT_EQ(addBarLoads(p, stack, {1}, data(BarLoadType::Uniform, 0), nullptr, nullptr, &session), BarLoadError::InvalidData);
    EXPECT_EQ(session.last, d);   // a refused dialog remembers nothing
}

TEST(BarLoad, StoredDataIsCanonicalAndRounded) {
    Project p = baseProject();
    UndoStack stack;
    BarLoadData d = data(BarLoadType::Point, 12.345);
    d.start = 0.12345;
    d.temperatureDirection = TemperatureDirection::Angle;   // irrelevant for a point load
    ASSERT_EQ(addBarLoads(p, stack, {1}, d), BarLoadError::None);
    const BarLoad& l = p.barLoads.items[0];
    EXPECT_DOUBLE_EQ(l.value1, 12.35);
    EXPECT_DOUBLE_EQ(l.start, 0.123);
    EXPECT_EQ(l.temperatureDirection, TemperatureDirection::PlusZ);
    EXPECT_EQ(barLoadToData(l), normaliseBarLoadData(barLoadToData(l)));
}

TEST(BarLoad, AllNineTypesCanBeCreatedAndValidated) {
    Project p = baseProject();
    UndoStack stack;
    for (BarLoadType t : kBarLoadTypeButtonOrder) {
        BarLoadData d = data(t, 5, hasTwoValues(t) ? 10 : 0);
        if (hasTwoPositions(t)) {
            d.start = 0.25;
            d.end = 0.75;
        }
        if (isPointLikeLoad(t)) d.start = 0.5;
        ASSERT_EQ(addBarLoads(p, stack, {1}, d), BarLoadError::None) << barLoadTypeName(t);
    }
    EXPECT_EQ(p.barLoads.items.size(), 9u);
    EXPECT_TRUE(project_json::validate(p).empty());
}

TEST(BarLoad, MultipleEditOverwritesTheWholeDefinitionOfEverySelectedLoad) {
    Project p = baseProject();
    UndoStack stack;
    const int cm = addCase(p, stack);
    BarLoadData point = data(BarLoadType::Point, 20);
    point.start = 0.5;
    ASSERT_EQ(addBarLoads(p, stack, {1}, point), BarLoadError::None);
    BarLoadData uniform = data(BarLoadType::Uniform, 3);
    uniform.axes = BarLoadAxes::Local;
    ASSERT_EQ(addBarLoads(p, stack, {1}, uniform), BarLoadError::None);
    ASSERT_EQ(addBarLoads(p, stack, {2}, uniform), BarLoadError::None);
    BarLoadData e = data(BarLoadType::Uniform, 7);
    e.loadCaseId = cm;
    // Measured: a point and a uniform edited with value 7 both became uniform 7 (type, values, axes, direction and case overwritten).
    ASSERT_EQ(editBarLoads(p, stack, {1, 2}, e), BarLoadError::None);
    for (int id : {1, 2}) {
        const BarLoad& l = *findBarLoad(p.barLoads, id);
        EXPECT_EQ(l.type, BarLoadType::Uniform);
        EXPECT_DOUBLE_EQ(l.value1, 7.0);
        EXPECT_EQ(l.axes, BarLoadAxes::Global);
        EXPECT_EQ(l.loadCaseId, cm);
    }
    EXPECT_DOUBLE_EQ(findBarLoad(p.barLoads, 3)->value1, 3.0);   // not selected: untouched
    EXPECT_EQ(findBarLoad(p.barLoads, 3)->loadCaseId, 0);
    EXPECT_EQ(editBarLoads(p, stack, {1, 2}, e), BarLoadError::NothingToChange);
    stack.undo();
    EXPECT_EQ(findBarLoad(p.barLoads, 1)->type, BarLoadType::Point);
    EXPECT_DOUBLE_EQ(findBarLoad(p.barLoads, 1)->value1, 20.0);
    // Errors.
    BarLoadIssue issue;
    EXPECT_EQ(editBarLoads(p, stack, {77}, e, &issue), BarLoadError::UnknownLoad);
    EXPECT_EQ(editBarLoads(p, stack, {1}, data(BarLoadType::Uniform, 0), &issue), BarLoadError::InvalidData);
    EXPECT_FALSE(issue.ok());
    e.loadCaseId = 99;
    EXPECT_EQ(editBarLoads(p, stack, {1}, e), BarLoadError::UnknownLoadCase);
    // Absolute position checked against the shortest SELECTED bar.
    BarLoadData abs = data(BarLoadType::Point, 20);
    abs.relative = false;
    abs.start = 3.5;
    EXPECT_EQ(editBarLoads(p, stack, {1, 3}, abs, &issue), BarLoadError::InvalidData);   // bar 2 is 3 m
    EXPECT_EQ(editBarLoads(p, stack, {1}, abs), BarLoadError::None);
}

TEST(BarLoad, EditRemembersLastValuesOnSuccessOnly) {
    Project p = baseProject();
    UndoStack stack;
    ASSERT_EQ(addBarLoads(p, stack, {1}, data(BarLoadType::Uniform, 5)), BarLoadError::None);
    BarLoadSession session;
    BarLoadData e = data(BarLoadType::Strip, 6);
    e.start = 0.2;
    e.end = 0.8;
    ASSERT_EQ(editBarLoads(p, stack, {1}, e, nullptr, &session), BarLoadError::None);
    EXPECT_EQ(session.last, e);
    e.end = 0.1;
    EXPECT_EQ(editBarLoads(p, stack, {1}, e, nullptr, &session), BarLoadError::InvalidData);
    EXPECT_EQ(session.last.end, 0.8);
}

TEST(BarLoad, ChangeCaseAndDelete) {
    Project p = baseProject();
    UndoStack stack;
    const int cm = addCase(p, stack);
    ASSERT_EQ(addBarLoads(p, stack, {1, 2}, data(BarLoadType::Uniform, 5)), BarLoadError::None);
    ASSERT_EQ(changeBarLoadCase(p, stack, {1}, cm), BarLoadError::None);
    EXPECT_EQ(findBarLoad(p.barLoads, 1)->loadCaseId, cm);
    EXPECT_EQ(findBarLoad(p.barLoads, 2)->loadCaseId, 0);
    EXPECT_EQ(barLoadsOfCase(p.barLoads, cm), (std::vector<int>{1}));
    EXPECT_EQ(changeBarLoadCase(p, stack, {1}, 99), BarLoadError::UnknownLoadCase);
    EXPECT_EQ(changeBarLoadCase(p, stack, {42}, cm), BarLoadError::UnknownLoad);
    EXPECT_EQ(changeBarLoadCase(p, stack, {1}, cm), BarLoadError::NothingToChange);
    stack.undo();
    EXPECT_EQ(findBarLoad(p.barLoads, 1)->loadCaseId, 0);

    ASSERT_EQ(deleteBarLoads(p, stack, {1}), BarLoadError::None);
    EXPECT_EQ(p.barLoads.items.size(), 1u);
    EXPECT_EQ(deleteBarLoads(p, stack, {1}), BarLoadError::UnknownLoad);
    stack.undo();
    EXPECT_EQ(p.barLoads.items.size(), 2u);
    ASSERT_EQ(deleteBarLoads(p, stack, {1, 2}), BarLoadError::None);
    EXPECT_FALSE(hasBarLoads(p.barLoads));   // Editar / Borrar / Hipótesis go grey again
}

TEST(BarLoad, SelfWeightActivateAndDeactivate) {
    Project p = baseProject();
    UndoStack stack;
    EXPECT_TRUE(barSelfWeightActive(p.barLoads, 1));
    EXPECT_EQ(setBarSelfWeight(p, stack, {1, 99}, true), BarLoadError::NothingToChange);   // already active
    ASSERT_EQ(setBarSelfWeight(p, stack, {1, 99}, false), BarLoadError::None);
    EXPECT_FALSE(barSelfWeightActive(p.barLoads, 1));
    EXPECT_TRUE(barSelfWeightActive(p.barLoads, 2));
    EXPECT_EQ(setBarSelfWeight(p, stack, {1}, false), BarLoadError::NothingToChange);
    EXPECT_EQ(setBarSelfWeight(p, stack, {99}, false), BarLoadError::NoBars);
    EXPECT_FALSE(hasBarLoads(p.barLoads));   // the flag is not a load: Editar / Borrar stay grey
    ASSERT_EQ(setBarSelfWeight(p, stack, {1}, true), BarLoadError::None);
    EXPECT_TRUE(barSelfWeightActive(p.barLoads, 1));
    stack.undo();
    EXPECT_FALSE(barSelfWeightActive(p.barLoads, 1));
    stack.undo();
    EXPECT_TRUE(barSelfWeightActive(p.barLoads, 1));
}

// ---------------------------------------------------------------- structure edits

TEST(BarLoad, DeletingABarDeletesItsLoadsSilentlyAndUndoRestores) {
    Project p = baseProject();
    UndoStack stack;
    ASSERT_EQ(addBarLoads(p, stack, {1, 2}, data(BarLoadType::Uniform, 5)), BarLoadError::None);
    ASSERT_EQ(setBarSelfWeight(p, stack, {1}, false), BarLoadError::None);
    ASSERT_TRUE(deleteBars(p, stack, {1}));
    EXPECT_EQ(p.barLoads.items.size(), 1u);
    EXPECT_EQ(p.barLoads.items[0].barId, 2);
    EXPECT_TRUE(barSelfWeightActive(p.barLoads, 1));   // flag of the deleted bar goes too
    EXPECT_TRUE(project_json::validate(p).empty());
    stack.undo();
    EXPECT_EQ(p.barLoads.items.size(), 2u);
    EXPECT_FALSE(barSelfWeightActive(p.barLoads, 1));
    stack.redo();
    EXPECT_EQ(p.barLoads.items.size(), 1u);
}

TEST(BarLoad, DeletingANodeDeletesLoadsOfItsBarsAndOfAMergedPair) {
    Project p = baseProject();
    UndoStack stack;
    ASSERT_EQ(addBarLoads(p, stack, {1, 2}, data(BarLoadType::Uniform, 5)), BarLoadError::None);
    ASSERT_TRUE(deleteNodes(p, stack, {3}));   // bar 2 goes
    ASSERT_EQ(p.barLoads.items.size(), 1u);
    EXPECT_EQ(p.barLoads.items[0].barId, 1);
    stack.undo();
    EXPECT_EQ(p.barLoads.items.size(), 2u);

    // Two aligned bars merged by deleting their node: the loads of both are lost (not measured, documented).
    Project q;
    UndoStack s2;
    q.structure.addNode({0, 0, 0});
    q.structure.addNode({2, 0, 0});
    q.structure.addNode({4, 0, 0});
    q.structure.addBar(1, 2);
    q.structure.addBar(2, 3);
    ASSERT_EQ(addBarLoads(q, s2, {1, 2}, data(BarLoadType::Uniform, 5)), BarLoadError::None);
    ASSERT_TRUE(deleteNodes(q, s2, {2}));
    ASSERT_EQ(q.structure.bars.size(), 1u);
    EXPECT_TRUE(q.barLoads.items.empty());
    s2.undo();
    EXPECT_EQ(q.barLoads.items.size(), 2u);
}

TEST(BarLoad, MovingANodeKeepsRelativeLoadsAndDeletesAbsoluteOnesThatNoLongerFit) {
    Project p = baseProject();
    UndoStack stack;
    BarLoadData rel = data(BarLoadType::Point, 10);
    rel.start = 0.75;
    BarLoadData absFits = data(BarLoadType::Point, 10);
    absFits.relative = false;
    absFits.start = 1.0;
    BarLoadData absLost = data(BarLoadType::Point, 10);
    absLost.relative = false;
    absLost.start = 3.5;
    BarLoadData strip = data(BarLoadType::Strip, 4);
    strip.relative = false;
    strip.start = 1.0;
    strip.end = 3.9;
    BarLoadData stripFits = strip;
    stripFits.end = 2.0;
    for (const BarLoadData* d : {&rel, &absFits, &absLost, &strip, &stripFits, &absLost})
        ASSERT_EQ(addBarLoads(p, stack, {1}, *d), BarLoadError::None);
    ASSERT_EQ(addBarLoads(p, stack, {1}, data(BarLoadType::Uniform, 5)), BarLoadError::None);
    ASSERT_EQ(p.barLoads.items.size(), 7u);
    ASSERT_TRUE(moveNode(p, stack, 2, {3, 0, 0}));   // bar 1 becomes 3 m long
    // Lost: absLost (3.5 > 3), absLost again and the long strip (3.9 > 3). Kept: relative, absFits, stripFits, uniform.
    ASSERT_EQ(p.barLoads.items.size(), 4u);
    EXPECT_EQ(p.barLoads.items[0].start, 0.75);
    EXPECT_EQ(p.barLoads.items[1].start, 1.0);
    EXPECT_EQ(p.barLoads.items[2].type, BarLoadType::Strip);
    EXPECT_EQ(p.barLoads.items[3].type, BarLoadType::Uniform);
    EXPECT_TRUE(project_json::validate(p).empty());
    stack.undo();
    EXPECT_EQ(p.barLoads.items.size(), 7u);
    stack.redo();
    EXPECT_EQ(p.barLoads.items.size(), 4u);
    // Moving it back to a longer bar keeps everything.
    ASSERT_TRUE(moveNode(p, stack, 2, {6, 0, 0}));
    EXPECT_EQ(p.barLoads.items.size(), 4u);
}

TEST(BarLoad, MovingANodeOntoAnotherNodeDropsTheCollapsedBarLoads) {
    Project p = baseProject();
    UndoStack stack;
    ASSERT_EQ(addBarLoads(p, stack, {1, 2}, data(BarLoadType::Uniform, 5)), BarLoadError::None);
    ASSERT_TRUE(moveNode(p, stack, 3, {4, 0, 0}));   // node 3 would merge into node 2: bar 2 collapses
    EXPECT_EQ(p.structure.findNode(3), nullptr);
    EXPECT_EQ(p.barLoads.items.size(), 1u);
    EXPECT_EQ(p.barLoads.items[0].barId, 1);
    stack.undo();
    EXPECT_EQ(p.barLoads.items.size(), 2u);
}

TEST(BarLoad, MovingABarEndReconcilesTheLoads) {
    Project p = baseProject();
    p.structure.addNode({2, 0, 2});   // node 4
    UndoStack stack;
    BarLoadData abs = data(BarLoadType::Point, 10);
    abs.relative = false;
    abs.start = 3.5;
    ASSERT_EQ(addBarLoads(p, stack, {1}, abs), BarLoadError::None);
    ASSERT_EQ(addBarLoads(p, stack, {1}, data(BarLoadType::Uniform, 5)), BarLoadError::None);
    // Bar 1 (node 1 -> node 2) gets its end moved to node 4 (2, 0, 2): length 2.83 m < 3.5.
    ASSERT_EQ(moveBarEnd(p, stack, 1, BarEnd::End, 4), PieceError::None);
    ASSERT_EQ(p.barLoads.items.size(), 1u);
    EXPECT_EQ(p.barLoads.items[0].type, BarLoadType::Uniform);
    stack.undo();
    EXPECT_EQ(p.barLoads.items.size(), 2u);
}

TEST(BarLoad, DividingABarSplitsItsLoadsBetweenTheStretches) {
    Project p;
    UndoStack stack;
    p.structure.addNode({0, 0, 0});
    p.structure.addNode({4, 0, 0});
    p.structure.addNode({1, 0, 0});   // cut node: stretches 0-1 (id 1, shortened) and 1-4 (new)
    ASSERT_NE(p.structure.addBar(1, 2), 0);
    BarLoadData uniform = data(BarLoadType::Uniform, 5);
    BarLoadData temp = data(BarLoadType::VariableTemperature, 30, -30);
    BarLoadData point = data(BarLoadType::Point, 12);
    point.relative = false;
    point.start = 2.5;   // lands in the second stretch at 1.5 m
    BarLoadData pointAtCut = data(BarLoadType::PointMoment, 3);
    pointAtCut.relative = false;
    pointAtCut.start = 1.0;   // exactly at the cut node: stays with the first stretch, at its end
    BarLoadData strip = data(BarLoadType::Strip, 6);
    strip.relative = false;
    strip.start = 0.5;
    strip.end = 3.0;   // 0.5..1 on the first stretch, 0..2 on the second
    BarLoadData tri = data(BarLoadType::TriangularLeft, 8);   // peak 8 at 0, 0 at 4 (whole bar, relative)
    for (const BarLoadData* d : {&uniform, &temp, &point, &pointAtCut, &strip, &tri}) {
        BarLoadData x = *d;
        if (hasTwoPositions(x.type) && x.type == BarLoadType::TriangularLeft) {
            x.start = 0.0;
            x.end = 1.0;
        }
        ASSERT_EQ(addBarLoads(p, stack, {1}, x), BarLoadError::None);
    }
    ASSERT_EQ(setBarSelfWeight(p, stack, {1}, false), BarLoadError::None);
    RepairReport report;
    ASSERT_TRUE(repairModel(p, stack, &report));
    ASSERT_EQ(report.splitBars, 1);
    ASSERT_EQ(p.structure.bars.size(), 2u);
    int tail = 0;
    for (const Bar& b : p.structure.bars)
        if (b.id != 1) tail = b.id;
    ASSERT_NE(tail, 0);
    EXPECT_DOUBLE_EQ(p.structure.barLength(1), 1.0);
    EXPECT_DOUBLE_EQ(p.structure.barLength(tail), 3.0);

    const auto loadsOf = [&](int bar) {
        std::vector<BarLoad> out;
        for (const BarLoad& l : p.barLoads.items)
            if (l.barId == bar) out.push_back(l);
        return out;
    };
    const std::vector<BarLoad> first = loadsOf(1), second = loadsOf(tail);
    const auto count = [](const std::vector<BarLoad>& v, BarLoadType t) {
        return std::count_if(v.begin(), v.end(), [&](const BarLoad& l) { return l.type == t; });
    };
    // Uniform and temperature loads are copied to every stretch.
    EXPECT_EQ(count(first, BarLoadType::Uniform), 1);
    EXPECT_EQ(count(second, BarLoadType::Uniform), 1);
    EXPECT_EQ(count(first, BarLoadType::VariableTemperature), 1);
    EXPECT_EQ(count(second, BarLoadType::VariableTemperature), 1);
    // Point load goes to the stretch that holds it; the load at the cut node stays at the end of the first stretch.
    EXPECT_EQ(count(first, BarLoadType::Point), 0);
    ASSERT_EQ(count(second, BarLoadType::Point), 1);
    for (const BarLoad& l : second)
        if (l.type == BarLoadType::Point) EXPECT_DOUBLE_EQ(l.start, 1.5);
    ASSERT_EQ(count(first, BarLoadType::PointMoment), 1);
    EXPECT_EQ(count(second, BarLoadType::PointMoment), 0);
    for (const BarLoad& l : first)
        if (l.type == BarLoadType::PointMoment) EXPECT_DOUBLE_EQ(l.start, 1.0);
    // Strip clipped to each stretch.
    ASSERT_EQ(count(first, BarLoadType::Strip), 1);
    ASSERT_EQ(count(second, BarLoadType::Strip), 1);
    for (const BarLoad& l : first)
        if (l.type == BarLoadType::Strip) {
            EXPECT_DOUBLE_EQ(l.start, 0.5);
            EXPECT_DOUBLE_EQ(l.end, 1.0);
        }
    for (const BarLoad& l : second)
        if (l.type == BarLoadType::Strip) {
            EXPECT_DOUBLE_EQ(l.start, 0.0);
            EXPECT_DOUBLE_EQ(l.end, 2.0);
        }
    // Triangular left (8 at x = 0, 0 at x = 4): value 6 at the cut node (x = 1): trapezoidal 8 -> 6 on the first stretch, 6 -> 0 triangular on the second.
    for (const BarLoad& l : first)
        if (l.type == BarLoadType::Trapezoidal) {
            EXPECT_DOUBLE_EQ(l.value1, 8.0);
            EXPECT_DOUBLE_EQ(l.value2, 6.0);
        }
    EXPECT_EQ(count(first, BarLoadType::Trapezoidal), 1);
    ASSERT_EQ(count(second, BarLoadType::TriangularLeft), 1);
    for (const BarLoad& l : second)
        if (l.type == BarLoadType::TriangularLeft) EXPECT_DOUBLE_EQ(l.value1, 6.0);
    // Self weight flag inherited by the new stretch; unique ids; valid after the edit.
    EXPECT_FALSE(barSelfWeightActive(p.barLoads, tail));
    EXPECT_TRUE(project_json::validate(p).empty());
    std::set<int> ids;
    for (const BarLoad& l : p.barLoads.items) EXPECT_TRUE(ids.insert(l.id).second);
    const int nextAfter = p.barLoads.nextId;
    EXPECT_EQ(findBarLoad(p.barLoads, 1) != nullptr, true);   // the original id survives on a stretch

    stack.undo();
    EXPECT_EQ(p.structure.bars.size(), 1u);
    EXPECT_EQ(p.barLoads.items.size(), 6u);
    EXPECT_EQ(p.barLoads.nextId, nextAfter);   // ids are not reused
    EXPECT_EQ(p.barLoads.selfWeightOff.count(tail), 0u);
    stack.redo();
    EXPECT_TRUE(project_json::validate(p).empty());
}

TEST(BarLoad, DeletingALoadCaseDeletesItsBarLoadsInOneCommand) {
    Project p = baseProject();
    UndoStack stack;
    const int cm = addCase(p, stack);
    BarLoadData d = data(BarLoadType::Uniform, 5);
    d.loadCaseId = cm;
    ASSERT_EQ(addBarLoads(p, stack, {1, 2}, d), BarLoadError::None);
    ASSERT_EQ(addBarLoads(p, stack, {1}, data(BarLoadType::Uniform, 5)), BarLoadError::None);   // self weight case 0
    const LoadCaseDependents dep = loadCaseDependents(p, cm);
    EXPECT_EQ(dep.barLoads, (std::vector<int>{1, 2}));
    EXPECT_FALSE(dep.empty());
    EXPECT_TRUE(loadCaseDependents(p, 0).barLoads.size() == 1u);
    ASSERT_EQ(deleteLoadCase(p, stack, cm), LoadCaseError::None);
    ASSERT_EQ(p.barLoads.items.size(), 1u);
    EXPECT_EQ(p.barLoads.items[0].loadCaseId, 0);
    EXPECT_TRUE(project_json::validate(p).empty());
    stack.undo();   // one command brings case and loads back
    EXPECT_EQ(p.barLoads.items.size(), 3u);
    EXPECT_NE(findLoadCase(p.loadCases, cm), nullptr);
    stack.redo();
    EXPECT_EQ(p.barLoads.items.size(), 1u);
}

// ---------------------------------------------------------------- persistence

TEST(BarLoad, JsonRoundTripIsVersionedAndTolerant) {
    Project p = baseProject();
    UndoStack stack;
    const int cm = addCase(p, stack);
    ASSERT_EQ(addBarLoads(p, stack, {1}, data(BarLoadType::Uniform, 5)), BarLoadError::None);
    BarLoadData d = data(BarLoadType::Trapezoidal, 5.5, 10.25);
    d.loadCaseId = cm;
    d.relative = false;
    d.start = 0.5;
    d.end = 2.5;
    d.axes = BarLoadAxes::Local;
    d.direction = LoadDirection::UserVector;
    d.angleInPlaneXz = 30;
    d.angleToPlaneXz = -15.5;
    ASSERT_EQ(addBarLoads(p, stack, {1, 2}, d), BarLoadError::None);
    BarLoadData g = data(BarLoadType::Strip, 4);
    g.direction = LoadDirection::UserVector;
    g.userVector = {1, -2, 0.5};
    g.start = 0.1;
    g.end = 0.9;
    ASSERT_EQ(addBarLoads(p, stack, {2}, g), BarLoadError::None);
    BarLoadData t = data(BarLoadType::VariableTemperature, 30, -30);
    t.temperatureDirection = TemperatureDirection::Angle;
    t.temperatureAngle = 33.5;
    ASSERT_EQ(addBarLoads(p, stack, {1}, t), BarLoadError::None);
    ASSERT_EQ(addBarLoads(p, stack, {2}, data(BarLoadType::UniformTemperature, 20)), BarLoadError::None);
    BarLoadData pm = data(BarLoadType::PointMoment, 7);
    pm.start = 0.5;
    ASSERT_EQ(addBarLoads(p, stack, {2}, pm), BarLoadError::None);
    ASSERT_EQ(deleteBarLoads(p, stack, {1}), BarLoadError::None);   // leaves a gap in the ids
    ASSERT_EQ(setBarSelfWeight(p, stack, {2}, false), BarLoadError::None);

    ASSERT_TRUE(project_json::isKnownSection("bar_loads"));
    EXPECT_EQ(project_json::sectionTitle("bar_loads"), "Cargas sobre barras");
    const std::string text = project_json::dumpSection(p, "bar_loads");
    EXPECT_NE(text.find("\"version\": 1"), std::string::npos);
    Project q = baseProject();
    q.loadCases = p.loadCases;
    project_json::loadSection(q, "bar_loads", text);
    EXPECT_EQ(q.barLoads, p.barLoads);
    EXPECT_EQ(q.barLoads.nextId, p.barLoads.nextId);   // ids still not reused
    EXPECT_FALSE(barSelfWeightActive(q.barLoads, 2));
    EXPECT_TRUE(project_json::validate(q).empty());

    // Old files without the section keep it empty; unknown keys and bad values are tolerated.
    Project old;
    project_json::loadSection(old, "bar_loads", "{}");
    EXPECT_TRUE(old.barLoads.empty());
    Project t2 = baseProject();
    project_json::loadSection(t2, "bar_loads",
                              R"({"version":9,"future":1,"items":[{"id":1,"bar":1,"load_case":0,"type":1,"value1":7.2564,"direction":42,"extra":true},
                                  {"id":2,"bar":1,"type":77},{"id":0,"bar":1},{"bar":2}],"self_weight_off":[2,"x",-1]})");
    ASSERT_EQ(t2.barLoads.items.size(), 1u);
    EXPECT_DOUBLE_EQ(t2.barLoads.items[0].value1, 7.256);
    EXPECT_EQ(t2.barLoads.items[0].direction, LoadDirection::MinusZ);
    EXPECT_EQ(t2.barLoads.nextId, 2);
    EXPECT_FALSE(barSelfWeightActive(t2.barLoads, 2));
    EXPECT_TRUE(project_json::validate(t2).empty());
}

TEST(BarLoad, JsonSkipsVanishedBarsAndValidationCatchesCorruption) {
    Project p = baseProject();
    UndoStack stack;
    ASSERT_EQ(addBarLoads(p, stack, {1, 2}, data(BarLoadType::Uniform, 5)), BarLoadError::None);
    ASSERT_EQ(setBarSelfWeight(p, stack, {2}, false), BarLoadError::None);
    p.structure.removeBar(2);   // behind the commands' back
    const std::string text = project_json::dumpSection(p, "bar_loads");
    EXPECT_EQ(text.find("\"bar\": 2"), std::string::npos);
    EXPECT_EQ(text.find("\"self_weight_off\": [\n    2"), std::string::npos);
    EXPECT_TRUE(project_json::validate(p).empty());   // loads of missing bars are tolerated

    Project q = baseProject();
    UndoStack s2;
    ASSERT_EQ(addBarLoads(q, s2, {1}, data(BarLoadType::Uniform, 5)), BarLoadError::None);
    ASSERT_TRUE(project_json::validate(q).empty());
    Project bad = q;
    bad.barLoads.items[0].value1 = 0.0;
    EXPECT_FALSE(project_json::validate(bad).empty());
    bad = q;
    bad.barLoads.items[0].loadCaseId = 99;
    EXPECT_FALSE(project_json::validate(bad).empty());
    bad = q;
    bad.barLoads.items.push_back(bad.barLoads.items[0]);   // repeated id
    EXPECT_FALSE(project_json::validate(bad).empty());
    bad = q;
    bad.barLoads.nextId = 1;   // id not below nextId
    EXPECT_FALSE(project_json::validate(bad).empty());
    bad = q;
    bad.barLoads.items[0].value1 = 5.0001;   // not rounded to the field decimals: not canonical
    EXPECT_FALSE(project_json::validate(bad).empty());
    bad = q;
    bad.barLoads.items[0].type = BarLoadType::Strip;
    bad.barLoads.items[0].start = 0.8;
    bad.barLoads.items[0].end = 0.2;
    EXPECT_FALSE(project_json::validate(bad).empty());
    bad = q;
    bad.barLoads.selfWeightOff.insert(0);
    EXPECT_FALSE(project_json::validate(bad).empty());
}

TEST(BarLoad, FilesWithoutBarLoadsMigrateCleanly) {
    // A project written before T-P7-03 has no "bar_loads" section: it loads with an empty set and saves an empty (versioned) section.
    Project p = baseProject();
    EXPECT_TRUE(p.barLoads.empty());
    EXPECT_EQ(p.barLoads.nextId, 1);
    const std::string text = project_json::dumpSection(p, "bar_loads");
    Project q = baseProject();
    project_json::loadSection(q, "bar_loads", text);
    EXPECT_TRUE(q.barLoads.empty());
    EXPECT_EQ(q.barLoads, p.barLoads);
    EXPECT_TRUE(project_json::validate(q).empty());
}

TEST(BarLoad, ReconcileIsANoOpWithoutLoadsAndReportsRemovals) {
    Project p = baseProject();
    StructuralModel after = p.structure;
    const BarLoads none;
    EXPECT_EQ(reconcileBarLoads(none, p.structure, after), none);

    UndoStack stack;
    BarLoadData abs = data(BarLoadType::Point, 10);
    abs.relative = false;
    abs.start = 3.8;
    ASSERT_EQ(addBarLoads(p, stack, {1}, abs), BarLoadError::None);
    after.findNode(2)->position = {3, 0, 0};
    BarLoadReconcileInfo info;
    const BarLoads r = reconcileBarLoads(p.barLoads, p.structure, after, &info);
    EXPECT_TRUE(r.items.empty());
    EXPECT_EQ(info.removedOutOfLength, 1);
    EXPECT_EQ(info.split, 0);
    // Same length: nothing is removed even for absolute loads beyond (they cannot exist, but the rule is about length changes).
    EXPECT_EQ(reconcileBarLoads(p.barLoads, p.structure, p.structure).items.size(), 1u);
}
