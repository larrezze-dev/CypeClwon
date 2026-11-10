#include <gtest/gtest.h>

#include <cmath>
#include <set>

#include "core/geom3.h"
#include "core/load_case_edit.h"
#include "core/node_edit.h"
#include "core/panel_edit.h"
#include "core/project_json.h"
#include "core/shell_edit.h"

using namespace cc::core;

namespace {

std::vector<Point3> square(double z = 0.0) { return {{0, 0, z}, {4, 0, z}, {4, 4, z}, {0, 4, z}}; }
/// Inclined quad: z = 0 along y = 0 and z = 2 along y = 4.
std::vector<Point3> inclined() { return {{0, 0, 0}, {4, 0, 0}, {4, 4, 2}, {0, 4, 2}}; }

/// Nodes 1..4 on the corners of the 4 x 4 square and bars 1 (N1-N2, along X, y = 0), 2 (N2-N3, along Y, x = 4), 3 (N3-N4, y = 4), 4 (N4-N1, x = 0).
Project baseProject() {
    Project p;
    const int n1 = p.structure.addNode({0, 0, 0});
    const int n2 = p.structure.addNode({4, 0, 0});
    const int n3 = p.structure.addNode({4, 4, 0});
    const int n4 = p.structure.addNode({0, 4, 0});
    p.structure.addBar(n1, n2);
    p.structure.addBar(n2, n3);
    p.structure.addBar(n3, n4);
    p.structure.addBar(n4, n1);
    return p;
}

int addCase(Project& p, UndoStack& stack, LoadNature nature = LoadNature::DeadLoad, const std::string& ref = "CM 1") {
    NewLoadCase d;
    d.nature = nature;
    d.reference = ref;
    int id = 0;
    EXPECT_EQ(createLoadCase(p, stack, d, &id), LoadCaseError::None);
    return id;
}

SurfaceLoadData uniform(double v, int caseId = 0) {
    SurfaceLoadData d = surfaceLoadDefaults(caseId);
    d.value = v;
    return d;
}
SurfaceLoadData variable(double lower, double upper, int caseId = 0) {
    SurfaceLoadData d = surfaceLoadDefaults(caseId, SurfaceLoadType::Variable);
    d.lower = lower;
    d.upper = upper;
    return d;
}

int newPanel(Project& p, UndoStack& s, const std::vector<Point3>& v = square(), const std::vector<SurfaceLoadData>& loads = {}) {
    int id = 0;
    EXPECT_EQ(addPanel(p, s, v, 0, PanelDirectionMode::Parallel, loads, &id), PanelError::None);
    return id;
}

const PanelBarLoad* loadOnBar(const std::vector<PanelBarLoad>& loads, int barId) {
    for (const PanelBarLoad& l : loads)
        if (l.barId == barId) return &l;
    return nullptr;
}

}  // namespace

// ---------------------------------------------------------------- fields

TEST(Panel, DefaultsRangeAndMessagesAreTheMeasuredOnes) {
    const SurfaceLoadData d = surfaceLoadDefaults(3);
    EXPECT_EQ(d.loadCaseId, 3);
    EXPECT_EQ(d.type, SurfaceLoadType::Uniform);
    EXPECT_DOUBLE_EQ(d.value, 0.0);   // measured 0.000
    EXPECT_EQ(surfaceLoadRangeMessage(surfaceLoadFieldSpec(SurfaceLoadField::Value)), "El valor de 'Valor' debe estar entre -10000.000 kN/m² y 10000.000 kN/m².");
    EXPECT_EQ(surfaceLoadFieldSpec(SurfaceLoadField::Lower).label, "Valor en la cota inferior");
    EXPECT_EQ(surfaceLoadFieldSpec(SurfaceLoadField::Upper).label, "Valor en la cota superior");
    EXPECT_EQ(surfaceLoadZeroMessage(), "No puede introducir cargas de valor nulo.");
    EXPECT_EQ(panelContourMessage(false, false), "Los puntos introducidos no permiten definir correctamente un paño.");
    EXPECT_EQ(panelContourMessage(false, true), "Los puntos indicados no permiten definir correctamente un paño.");
    EXPECT_EQ(panelContourMessage(true, false), "Los puntos introducidos no permiten definir correctamente el contorno de una carga superficial.");
    EXPECT_EQ(panelContourMessage(true, true), "Los puntos indicados no permiten definir correctamente el contorno de una carga superficial.");
    EXPECT_EQ(surfaceLoadSeveralPanelsMessage(), "La carga superficial se solapa con varios paños.");
    EXPECT_EQ(surfaceLoadNotContainedMessage(), "La superficie de la carga debe estar contenida completamente en un paño.");
    EXPECT_EQ(panelWarningNotAssignedMessage(), "No se han podido asignar totalmente las cargas superficiales del paño.");
    EXPECT_EQ(panelWarningNoBarsMessage(true), "Se han encontrado paños sin barras ni láminas asociadas.");
    EXPECT_EQ(panelWarningNotCoveredByShellsMessage(), "El paño no está cubierto completamente por láminas.");
}

TEST(Panel, ParsingAndChecks) {
    EXPECT_TRUE(parseSurfaceLoadField(SurfaceLoadField::Value, "-5,2504").ok);
    EXPECT_DOUBLE_EQ(parseSurfaceLoadField(SurfaceLoadField::Value, "-5,2504").value, -5.25);
    EXPECT_EQ(parseSurfaceLoadField(SurfaceLoadField::Value, "abc").message, "La cadena introducida no es un número.");
    EXPECT_EQ(parseSurfaceLoadField(SurfaceLoadField::Value, "20000").message, "El valor de 'Valor' debe estar entre -10000.000 kN/m² y 10000.000 kN/m².");
    EXPECT_TRUE(parseSurfaceLoadField(SurfaceLoadField::Upper, "10000").ok);

    SurfaceLoadIssue i = checkSurfaceLoadData(uniform(0.0));
    EXPECT_EQ(i.kind, SurfaceLoadIssueKind::Error);
    EXPECT_EQ(i.message, "No puede introducir cargas de valor nulo.");
    EXPECT_EQ(checkSurfaceLoadData(uniform(0.0004)).kind, SurfaceLoadIssueKind::Error);   // rounds to 0.000
    i = checkSurfaceLoadData(uniform(20000.0));
    EXPECT_EQ(i.kind, SurfaceLoadIssueKind::Warning);
    EXPECT_EQ(i.field, SurfaceLoadField::Value);
    EXPECT_TRUE(checkSurfaceLoadData(uniform(-5.0)).ok());                  // negative accepted
    EXPECT_EQ(checkSurfaceLoadData(variable(0.0, 0.0)).kind, SurfaceLoadIssueKind::Error);
    EXPECT_TRUE(checkSurfaceLoadData(variable(0.0, 6.0)).ok());             // one null value is accepted (measured 0 / 6)
    EXPECT_TRUE(checkSurfaceLoadData(variable(6.0, -4.0)).ok());            // mixed signs accepted (measured)
    EXPECT_EQ(checkSurfaceLoadData(variable(1.0, 20000.0)).field, SurfaceLoadField::Upper);

    const SurfaceLoadData n = normaliseSurfaceLoadData({0, SurfaceLoadType::Uniform, 1.23456, 7.0, 8.0});
    EXPECT_DOUBLE_EQ(n.value, 1.235);
    EXPECT_DOUBLE_EQ(n.lower, 0.0);
    EXPECT_DOUBLE_EQ(n.upper, 0.0);
    const SurfaceLoadData v = normaliseSurfaceLoadData({0, SurfaceLoadType::Variable, 9.0, 1.00049, 2.0});
    EXPECT_DOUBLE_EQ(v.value, 0.0);
    EXPECT_DOUBLE_EQ(v.lower, 1.0);
}

// ---------------------------------------------------------------- geometry

TEST(Panel, ContourValidation) {
    EXPECT_EQ(validatePanelContour(square()), ContourError::None);
    EXPECT_EQ(validatePanelContour(inclined()), ContourError::None);
    EXPECT_EQ(validatePanelContour({{0, 0, 0}, {1, 0, 0}}), ContourError::TooFewPoints);
    EXPECT_EQ(validatePanelContour({{0, 0, 0}, {1, 0, 0}, {1, 0, 0}, {0, 1, 0}}), ContourError::CoincidentPoints);
    EXPECT_EQ(validatePanelContour({{0, 0, 0}, {1, 0, 0}, {2, 0, 0}}), ContourError::Degenerate);
    EXPECT_EQ(validatePanelContour({{0, 0, 0}, {4, 0, 0}, {4, 4, 1}, {0, 4, 0}}), ContourError::NotPlanar);   // #65133
    EXPECT_EQ(validatePanelContour({{0, 0, 0}, {4, 0, 0}, {0, 3, 0}, {5, 3, 0}}), ContourError::SelfIntersecting);   // bow tie
    EXPECT_EQ(validatePanelContour({{0, 0, 0}, {4, 0, 0}, {4, 4, 0}, {0, 4, 0}, {2, 0, 0}}), ContourError::SelfIntersecting);  // a vertex on an edge
    EXPECT_EQ(validatePanelContour({{0, 0, 0}, {4, 0, 0}, {2, 0, 0}, {2, 3, 0}}), ContourError::SelfIntersecting);   // folded edge (spike)
    // A concave polygon is valid.
    EXPECT_EQ(validatePanelContour({{0, 0, 0}, {4, 0, 0}, {4, 4, 0}, {2, 1, 0}, {0, 4, 0}}), ContourError::None);
}

TEST(Panel, FrameHorizontalAndAreas) {
    const auto f = panelFrame(square());
    ASSERT_TRUE(f.has_value());
    EXPECT_NEAR(f->w.z, 1.0, 1e-12);
    EXPECT_TRUE(isHorizontalPlane(*f));
    EXPECT_TRUE(isHorizontalPolygon(square(3.0)));
    EXPECT_FALSE(isHorizontalPolygon(inclined()));
    EXPECT_NEAR(polygonArea(square()), 16.0, 1e-12);
    EXPECT_NEAR(polygonArea(inclined()), 4.0 * std::sqrt(20.0), 1e-9);
    const Point3 c = polygonCentroid(square());
    EXPECT_NEAR(c.x, 2.0, 1e-12);
    EXPECT_NEAR(c.y, 2.0, 1e-12);
    EXPECT_TRUE(pointInsidePolygon(square(), {2, 2, 0}));
    EXPECT_TRUE(pointInsidePolygon(square(), {4, 2, 0}));     // on the boundary
    EXPECT_FALSE(pointInsidePolygon(square(), {2, 2, 1}));    // out of the plane
    EXPECT_FALSE(pointInsidePolygon(square(), {5, 2, 0}));
    EXPECT_FALSE(panelFrame({{0, 0, 0}, {1, 0, 0}, {2, 0, 0}}).has_value());
}

TEST(Panel, DirectionFromSideAndArrow) {
    const Point3 par = panelDirectionFromSide(square(), 0, PanelDirectionMode::Parallel);
    EXPECT_NEAR(par.x, 1.0, 1e-12);
    const Point3 perp = panelDirectionFromSide(square(), 0, PanelDirectionMode::Perpendicular);
    EXPECT_NEAR(perp.y, 1.0, 1e-12);
    const Point3 side1 = panelDirectionFromSide(square(), 1, PanelDirectionMode::Parallel);
    EXPECT_NEAR(side1.y, 1.0, 1e-12);
    const Point3 slope = panelDirectionFromSide(inclined(), 1, PanelDirectionMode::Parallel);   // the sloped edge, in the plane
    EXPECT_NEAR(slope.y, 2.0 / std::sqrt(5.0), 1e-9);
    EXPECT_NEAR(slope.z, 1.0 / std::sqrt(5.0), 1e-9);
    EXPECT_NEAR(geom::length(panelDirectionFromSide(square(), 9, PanelDirectionMode::Parallel)), 0.0, 1e-12);
    EXPECT_EQ(panelSideLines(square()).size(), 4u);

    Panel panel;
    panel.vertices = square();
    panel.direction = par;
    const PanelArrow arrow = panelArrow(panel);
    ASSERT_TRUE(arrow.valid);
    EXPECT_NEAR((arrow.from.x + arrow.to.x) / 2.0, 2.0, 1e-12);   // centred
    EXPECT_NEAR(arrow.to.y, arrow.from.y, 1e-12);
    EXPECT_GT(arrow.to.x, arrow.from.x);
    // A direction out of the plane is projected; one along the normal falls back to the first side.
    EXPECT_NEAR(panelDirectionInPlane(square(), {1, 0, 5}).x, 1.0, 1e-12);
    EXPECT_NEAR(panelDirectionInPlane(square(), {0, 0, 1}).x, 1.0, 1e-12);
}

TEST(Panel, PositiveSenseTexts) {
    EXPECT_EQ(panelPositiveSenseText(square(), false), "Vertical hacia abajo");
    EXPECT_EQ(panelPositiveSenseText(square(), true), "Vertical hacia arriba");        // wind on a horizontal panel (measured)
    EXPECT_EQ(panelPositiveSenseText(inclined(), false), "Vertical hacia abajo");
    EXPECT_EQ(panelPositiveSenseText(inclined(), true), "(0, -0.447, 0.894)");         // measured
    // Clockwise order: the wind vector still points up.
    const std::vector<Point3> clockwise{{0, 0, 0}, {0, 4, 0}, {4, 4, 0}, {4, 0, 0}};
    EXPECT_EQ(panelPositiveSenseText(clockwise, true), "Vertical hacia arriba");
    // A load pushes along the positive vector times its sign.
    EXPECT_NEAR(panelLoadActionDirection(square(), false, 5.0).z, -1.0, 1e-12);
    EXPECT_NEAR(panelLoadActionDirection(square(), false, -5.0).z, 1.0, 1e-12);
    EXPECT_NEAR(panelLoadActionDirection(square(), true, 5.0).z, 1.0, 1e-12);
    EXPECT_NEAR(geom::length(panelLoadActionDirection(square(), true, 0.0)), 0.0, 1e-12);

    UndoStack stack;
    Project p = baseProject();
    const int cm = addCase(p, stack);
    const int v = addCase(p, stack, LoadNature::Wind, "V 1");
    EXPECT_FALSE(isWindCase(p.loadCases, 0));
    EXPECT_FALSE(isWindCase(p.loadCases, cm));
    EXPECT_TRUE(isWindCase(p.loadCases, v));
}

TEST(Panel, ColourSlotFollowsThePositionOfTheHypothesis) {
    UndoStack stack;
    Project p = baseProject();
    const int a = addCase(p, stack, LoadNature::Wind, "V 1");   // first additional: cyan whatever its nature (measured)
    const int b = addCase(p, stack, LoadNature::DeadLoad, "CM 1");
    const int c = addCase(p, stack, LoadNature::Snow, "N 1");
    EXPECT_EQ(panelLoadColourSlot(p.loadCases, 0), 0);
    EXPECT_EQ(panelLoadColourSlot(p.loadCases, a), 1);
    EXPECT_EQ(panelLoadColourSlot(p.loadCases, b), 2);
    EXPECT_EQ(panelLoadColourSlot(p.loadCases, c), 1);   // third: assumed to cycle
    EXPECT_EQ(panelLoadSlotRgb(0), 0xFFFF00u);
    EXPECT_EQ(panelLoadSlotRgb(1), 0x00BFFFu);
    EXPECT_EQ(panelLoadSlotRgb(2), 0x00FF00u);
}

TEST(Panel, DrawingPrismsHeightsAndStacking) {
    const LoadScales scales;   // fixed 0.5 m
    PanelLoadView view{uniform(10.0), panelPositiveDirection(square(), false)};
    PanelLoadDrawing d = panelLoadDrawing(view, square(), scales);
    ASSERT_TRUE(d.valid);
    EXPECT_NEAR(d.sizeM, 0.5, 1e-12);
    EXPECT_NEAR(d.top[0].z, 0.5, 1e-12);   // a positive load of a normal hypothesis stands ABOVE the plane (measured)
    EXPECT_EQ(d.ticks.size(), 80u);        // 4 edges of 4 m every 0.2 m
    ASSERT_EQ(d.labels.size(), 1u);
    EXPECT_EQ(d.labels[0].text, "10 kN/m²");

    PanelLoadView wind{uniform(3.0), panelPositiveDirection(square(), true)};
    d = panelLoadDrawing(wind, square(), scales);
    EXPECT_NEAR(d.top[0].z, -0.5, 1e-12);  // positive wind points up: the prism hangs BELOW (measured)
    wind.data.value = -3.0;
    d = panelLoadDrawing(wind, square(), scales);
    EXPECT_NEAR(d.top[0].z, 0.5, 1e-12);

    // Variable: heights proportional (measured 2 / 6 -> 1/3 .. 1), a label per corner, mixed signs cross the plane.
    PanelLoadView var{variable(2.0, 6.0), panelPositiveDirection(inclined(), true)};
    d = panelLoadDrawing(var, inclined(), scales);
    ASSERT_TRUE(d.valid);
    EXPECT_EQ(d.labels.size(), 4u);
    EXPECT_NEAR(geom::length(geom::sub(d.top[0], d.base[0])), 0.5 / 3.0, 1e-9);   // lowest corner
    EXPECT_NEAR(geom::length(geom::sub(d.top[2], d.base[2])), 0.5, 1e-9);         // highest corner
    PanelLoadView mixed{variable(6.0, -4.0), panelPositiveDirection(inclined(), true)};
    d = panelLoadDrawing(mixed, inclined(), scales);
    const double s0 = geom::dot(geom::sub(d.top[0], d.base[0]), panelPositiveDirection(inclined(), true));
    const double s2 = geom::dot(geom::sub(d.top[2], d.base[2]), panelPositiveDirection(inclined(), true));
    EXPECT_LT(s0 * s2, 0.0);   // opposite sides of the plane
    // Proportional mode: 0.1 m per unit.
    LoadScales proportional;
    proportional.mode = LoadScaleMode::Proportional;
    EXPECT_NEAR(panelLoadDrawing(view, square(), proportional).sizeM, 1.0, 1e-12);
    EXPECT_NEAR(panelLoadMaxSizeM(proportional, uniform(5.0)), 0.5, 1e-12);

    // Two equal loads: the second goes beyond the first (double height, measured); opposite sides do not stack.
    const std::vector<PanelLoadView> two{view, view, PanelLoadView{uniform(-4.0), view.positiveDirection}};
    const std::vector<double> offsets = panelLoadStackOffsets(two, scales);
    EXPECT_NEAR(offsets[0], 0.0, 1e-12);
    EXPECT_NEAR(offsets[1], 0.5, 1e-12);
    EXPECT_NEAR(offsets[2], 0.0, 1e-12);
    d = panelLoadDrawing(two[1], square(), scales, offsets[1]);
    EXPECT_NEAR(d.base[0].z, 0.5, 1e-12);
    EXPECT_NEAR(d.top[0].z, 1.0, 1e-12);
    EXPECT_FALSE(panelLoadDrawing(PanelLoadView{uniform(0.0), view.positiveDirection}, square(), scales).valid);
}

TEST(Panel, ContainmentAndValueAt) {
    Panels panels;
    Panel a;
    a.id = 1;
    a.vertices = square();
    panels.items.push_back(a);
    Panel b = a;
    b.id = 2;
    panels.items.push_back(b);
    const std::vector<Point3> inside{{1, 1, 0}, {3, 1, 0}, {3, 3, 0}, {1, 3, 0}};
    EXPECT_EQ(panelsContaining(panels, inside), (std::vector<int>{1, 2}));
    EXPECT_TRUE(panelsContaining(panels, {{1, 1, 0}, {5, 1, 0}, {5, 3, 0}, {1, 3, 0}}).empty());
    EXPECT_TRUE(panelsContaining(panels, {{1, 1, 1}, {3, 1, 1}, {3, 3, 1}, {1, 3, 1}}).empty());
    EXPECT_TRUE(panelContainsPolygon(square(), square()));
    const SurfaceLoadData v = variable(2.0, 6.0);
    EXPECT_NEAR(surfaceLoadValueAt(v, inclined(), {0, 0, 0}), 2.0, 1e-12);
    EXPECT_NEAR(surfaceLoadValueAt(v, inclined(), {0, 4, 2}), 6.0, 1e-12);
    EXPECT_NEAR(surfaceLoadValueAt(v, inclined(), {0, 2, 1}), 4.0, 1e-12);
    EXPECT_NEAR(surfaceLoadValueAt(v, square(), {1, 1, 0}), 2.0, 1e-12);
    EXPECT_NEAR(surfaceLoadValueAt(uniform(7.0), square(), {1, 1, 0}), 7.0, 1e-12);
}

// ---------------------------------------------------------------- panel commands

TEST(Panel, AddUndoRedoAndIdsAreNeverReused) {
    Project p = baseProject();
    UndoStack stack;
    int id = 0;
    ASSERT_EQ(addPanel(p, stack, square(), 0, PanelDirectionMode::Parallel, {}, &id), PanelError::None);
    EXPECT_EQ(id, 1);
    ASSERT_NE(findPanel(p.panels, 1), nullptr);
    EXPECT_NEAR(findPanel(p.panels, 1)->direction.x, 1.0, 1e-12);
    EXPECT_TRUE(hasPanels(p.panels));
    EXPECT_TRUE(project_json::validate(p).empty());
    stack.undo();
    EXPECT_TRUE(p.panels.items.empty());
    stack.redo();
    EXPECT_EQ(p.panels.items.size(), 1u);
    stack.undo();
    ASSERT_EQ(addPanel(p, stack, square(), 1, PanelDirectionMode::Perpendicular, {}, &id), PanelError::None);
    EXPECT_EQ(id, 2);   // never reused
    EXPECT_NEAR(findPanel(p.panels, 2)->direction.x, -1.0, 1e-12);   // perpendicular to side 1 (along Y): along -X
    ASSERT_EQ(addPanelWithDirection(p, stack, square(), {0, 1, 7}, {}, &id), PanelError::None);
    EXPECT_NEAR(findPanel(p.panels, 3)->direction.y, 1.0, 1e-12);
}

TEST(Panel, AddRefusesInvalidContoursSidesAndRows) {
    Project p = baseProject();
    UndoStack stack;
    ContourError why = ContourError::None;
    EXPECT_EQ(addPanel(p, stack, {{0, 0, 0}, {4, 0, 0}, {4, 4, 1}, {0, 4, 0}}, 0, PanelDirectionMode::Parallel, {}, nullptr, &why), PanelError::InvalidContour);
    EXPECT_EQ(why, ContourError::NotPlanar);
    EXPECT_EQ(addPanel(p, stack, {{0, 0, 0}, {1, 0, 0}}, 0, PanelDirectionMode::Parallel, {}, nullptr, &why), PanelError::InvalidContour);
    EXPECT_EQ(why, ContourError::TooFewPoints);
    EXPECT_EQ(addPanel(p, stack, square(), 7), PanelError::InvalidSide);
    SurfaceLoadIssue issue;
    EXPECT_EQ(addPanel(p, stack, square(), 0, PanelDirectionMode::Parallel, {uniform(0.0)}, nullptr, nullptr, &issue), PanelError::InvalidData);
    EXPECT_EQ(issue.message, "No puede introducir cargas de valor nulo.");
    EXPECT_EQ(addPanel(p, stack, square(), 0, PanelDirectionMode::Parallel, {uniform(5.0, 99)}), PanelError::UnknownLoadCase);
    EXPECT_EQ(addPanel(p, stack, square(), 0, PanelDirectionMode::Parallel, {variable(1.0, 2.0)}, nullptr, nullptr, &issue), PanelError::VariableOnHorizontal);
    EXPECT_EQ(issue.message, surfaceLoadVariableOnHorizontalMessage());
    EXPECT_EQ(addPanel(p, stack, inclined(), 0, PanelDirectionMode::Parallel, {variable(1.0, 2.0), uniform(3.0)}), PanelError::None);
    EXPECT_EQ(p.panels.items.size(), 1u);   // only the last one was created
    EXPECT_FALSE(stack.canUndo() && stack.undoText().empty());
}

TEST(Panel, SetLoadsEditsEverySelectedPanelAndChecksTheRows) {
    Project p = baseProject();
    UndoStack stack;
    const int cm = addCase(p, stack);
    const int a = newPanel(p, stack), b = newPanel(p, stack, inclined());
    SurfaceLoadIssue issue;
    EXPECT_EQ(setPanelLoads(p, stack, {99}, {uniform(5.0)}), PanelError::NoPanels);
    EXPECT_EQ(setPanelLoads(p, stack, {a, b}, {uniform(0.0)}, &issue), PanelError::InvalidData);
    EXPECT_EQ(issue.kind, SurfaceLoadIssueKind::Error);
    EXPECT_EQ(setPanelLoads(p, stack, {a, b}, {uniform(20000.0)}, &issue), PanelError::InvalidData);
    EXPECT_EQ(issue.kind, SurfaceLoadIssueKind::Warning);
    EXPECT_EQ(setPanelLoads(p, stack, {a, b}, {variable(1.0, 2.0)}), PanelError::VariableOnHorizontal);   // one of them is horizontal
    EXPECT_EQ(setPanelLoads(p, stack, {a}, {uniform(1.0, 42)}), PanelError::UnknownLoadCase);
    ASSERT_EQ(setPanelLoads(p, stack, {a, b}, {uniform(10.0), uniform(-5.0, cm)}), PanelError::None);   // equalises both panels
    EXPECT_EQ(findPanel(p.panels, a)->loads, findPanel(p.panels, b)->loads);
    EXPECT_EQ(findPanel(p.panels, a)->loads.size(), 2u);
    EXPECT_EQ(setPanelLoads(p, stack, {a, b}, {uniform(10.0), uniform(-5.0, cm)}), PanelError::NothingToChange);
    ASSERT_EQ(setPanelLoads(p, stack, {b}, {uniform(3.0), variable(0.0, 6.0)}), PanelError::None);   // 0 / 6 accepted
    EXPECT_EQ(findPanel(p.panels, b)->loads.size(), 2u);
    EXPECT_EQ(findPanel(p.panels, a)->loads.size(), 2u);
    EXPECT_TRUE(project_json::validate(p).empty());
    ASSERT_EQ(setPanelLoads(p, stack, {a}, {}), PanelError::None);   // an empty list clears
    EXPECT_TRUE(findPanel(p.panels, a)->loads.empty());
    stack.undo();
    EXPECT_EQ(findPanel(p.panels, a)->loads.size(), 2u);
}

TEST(Panel, DeleteTakesTheContainedSurfaceLoads) {
    Project p = baseProject();
    UndoStack stack;
    const int a = newPanel(p, stack);
    int load = 0;
    const std::vector<Point3> inside{{1, 1, 0}, {3, 1, 0}, {3, 3, 0}, {1, 3, 0}};
    ASSERT_EQ(addSurfaceLoad(p, stack, inside, uniform(5.0), false, &load), PanelError::None);
    const std::vector<Point3> far{{10, 10, 0}, {12, 10, 0}, {12, 12, 0}, {10, 12, 0}};
    int orphan = 0;
    ASSERT_EQ(addSurfaceLoad(p, stack, far, uniform(2.0), false, &orphan), PanelError::None);   // no panel: stays
    EXPECT_EQ(deletePanels(p, stack, {77}), PanelError::NoPanels);
    std::vector<int> removed;
    ASSERT_EQ(deletePanels(p, stack, {a}, &removed), PanelError::None);   // no question
    EXPECT_EQ(removed, (std::vector<int>{load}));
    EXPECT_TRUE(p.panels.items.empty());
    ASSERT_EQ(p.surfaceLoads.items.size(), 1u);
    EXPECT_EQ(p.surfaceLoads.items[0].id, orphan);
    EXPECT_EQ(p.structure.bars.size(), 4u);   // the contour bars stay
    stack.undo();
    EXPECT_EQ(p.surfaceLoads.items.size(), 2u);
    EXPECT_EQ(p.panels.items.size(), 1u);
    stack.redo();
    EXPECT_EQ(p.surfaceLoads.items.size(), 1u);
}

TEST(Panel, DeletingOneOfTwoOverlappingPanelsKeepsTheLoadsThatStillHaveAPanel) {
    Project p = baseProject();
    UndoStack stack;
    const int a = newPanel(p, stack);
    const int b = newPanel(p, stack);   // identical, overlapped (no warning when drawing: measured)
    int load = 0;
    ASSERT_EQ(addSurfaceLoad(p, stack, square(), uniform(5.0), false, &load), PanelError::None);
    ASSERT_EQ(deletePanels(p, stack, {a}), PanelError::None);
    EXPECT_EQ(p.surfaceLoads.items.size(), 1u);   // still contained in panel b (assumption, not isolated in the original)
    ASSERT_EQ(deletePanels(p, stack, {b}), PanelError::None);
    EXPECT_TRUE(p.surfaceLoads.items.empty());
}

TEST(Panel, MoveVerticesKeepsThePlaneOrRefuses) {
    Project p = baseProject();
    UndoStack stack;
    const int a = newPanel(p, stack, square(), {uniform(5.0)});
    ContourError why = ContourError::None;
    EXPECT_EQ(movePanelVertices(p, stack, 99, {0}, {1, 0, 0}), PanelError::NoPanels);
    EXPECT_EQ(movePanelVertices(p, stack, a, {9}, {1, 0, 0}), PanelError::InvalidVertex);
    EXPECT_EQ(movePanelVertices(p, stack, a, {}, {1, 0, 0}), PanelError::NothingToChange);
    EXPECT_EQ(movePanelVertices(p, stack, a, {2}, {0, 0, 1}, &why), PanelError::InvalidContour);   // non-planar: #65133
    EXPECT_EQ(why, ContourError::NotPlanar);
    EXPECT_EQ(findPanel(p.panels, a)->vertices, square());   // unchanged
    ASSERT_EQ(movePanelVertices(p, stack, a, {1, 2}, {1, 0, 0}), PanelError::None);
    EXPECT_EQ(findPanel(p.panels, a)->vertices[1], (Point3{5, 0, 0}));
    EXPECT_EQ(findPanel(p.panels, a)->vertices[2], (Point3{5, 4, 0}));
    EXPECT_EQ(findPanel(p.panels, a)->loads.size(), 1u);
    // Tilting the whole panel (all vertices) keeps it valid and the direction stays in the plane.
    ASSERT_EQ(movePanelVertices(p, stack, a, {2, 3}, {0, 0, 2}), PanelError::None);
    const Panel& moved = *findPanel(p.panels, a);
    const auto f = panelFrame(moved.vertices);
    ASSERT_TRUE(f.has_value());
    EXPECT_NEAR(geom::dot(moved.direction, f->w), 0.0, 1e-9);
    EXPECT_NEAR(geom::length(moved.direction), 1.0, 1e-9);
    EXPECT_TRUE(project_json::validate(p).empty());
    stack.undo();
    EXPECT_EQ(findPanel(p.panels, a)->vertices[2], (Point3{5, 4, 0}));
    // A panel flattened to horizontal drops its variable rows.
    const int b = newPanel(p, stack, inclined(), {variable(1.0, 2.0), uniform(3.0)});
    ASSERT_EQ(movePanelVertices(p, stack, b, {2, 3}, {0, 0, -2}), PanelError::None);
    ASSERT_EQ(findPanel(p.panels, b)->loads.size(), 1u);
    EXPECT_EQ(findPanel(p.panels, b)->loads[0].type, SurfaceLoadType::Uniform);
}

TEST(Panel, DivideGivesTwoPanelsWithTheSameDirectionAndACopyOfTheLoads) {
    Project p = baseProject();
    UndoStack stack;
    const int a = newPanel(p, stack, square(), {uniform(10.0)});
    std::vector<int> created;
    EXPECT_EQ(dividePanels(p, stack, {5, 0, 0}, {5, 4, 0}, &created), PanelError::NothingToDivide);   // the line misses the panel
    EXPECT_EQ(dividePanels(p, stack, {0, 0, 0}, {4, 0, 0}, &created), PanelError::NothingToDivide);   // along an edge
    ASSERT_EQ(dividePanels(p, stack, {2, -1, 0}, {2, 5, 0}, &created), PanelError::None);   // two points, the line crosses the whole panel
    ASSERT_EQ(created.size(), 1u);
    ASSERT_EQ(p.panels.items.size(), 2u);
    const Panel& first = *findPanel(p.panels, a);
    const Panel& second = *findPanel(p.panels, created[0]);
    EXPECT_NEAR(polygonArea(first.vertices), 8.0, 1e-9);
    EXPECT_NEAR(polygonArea(second.vertices), 8.0, 1e-9);
    EXPECT_NEAR(first.direction.x, 1.0, 1e-9);
    EXPECT_NEAR(second.direction.x, 1.0, 1e-9);   // same direction (measured)
    EXPECT_EQ(first.loads, second.loads);          // loads copied to both (measured)
    ASSERT_EQ(first.loads.size(), 1u);
    EXPECT_DOUBLE_EQ(second.loads[0].value, 10.0);
    EXPECT_EQ(validatePanelContour(first.vertices), ContourError::None);
    EXPECT_EQ(validatePanelContour(second.vertices), ContourError::None);
    EXPECT_TRUE(project_json::validate(p).empty());
    EXPECT_NEAR(geom::length(first.vertices[0]), 0.0, 1e-12);   // the piece with the first vertex keeps the id
    stack.undo();
    EXPECT_EQ(p.panels.items.size(), 1u);
    EXPECT_NEAR(polygonArea(findPanel(p.panels, a)->vertices), 16.0, 1e-9);
    stack.redo();
    // A diagonal cut through two vertices gives two triangles.
    Project q = baseProject();
    UndoStack s2;
    newPanel(q, s2);
    ASSERT_EQ(dividePanels(q, s2, {0, 0, 0}, {4, 4, 0}), PanelError::None);
    ASSERT_EQ(q.panels.items.size(), 2u);
    EXPECT_NEAR(polygonArea(q.panels.items[0].vertices), 8.0, 1e-9);
    // A concave panel cut at four points cannot be divided.
    Project c = baseProject();
    UndoStack s3;
    int id = 0;
    ASSERT_EQ(addPanel(c, s3, {{0, 0, 0}, {6, 0, 0}, {6, 4, 0}, {4, 4, 0}, {4, 1, 0}, {2, 1, 0}, {2, 4, 0}, {0, 4, 0}}, 0, PanelDirectionMode::Parallel, {}, &id), PanelError::None);
    EXPECT_EQ(dividePanels(c, s3, {-1, 2, 0}, {7, 2, 0}), PanelError::CannotDivide);
    EXPECT_EQ(c.panels.items.size(), 1u);
}

TEST(Panel, DivideCutsEveryCrossedPanelAndInclinedPanelsInTheirPlane) {
    Project p = baseProject();
    UndoStack stack;
    newPanel(p, stack);
    newPanel(p, stack, inclined());
    std::vector<int> created;
    ASSERT_EQ(dividePanels(p, stack, {2, -1, 0}, {2, 5, 0}, &created), PanelError::None);   // vertical line x = 2: crosses both
    EXPECT_EQ(created.size(), 2u);
    EXPECT_EQ(p.panels.items.size(), 4u);
    for (const Panel& panel : p.panels.items) EXPECT_EQ(validatePanelContour(panel.vertices), ContourError::None);
    EXPECT_NEAR(polygonArea(p.panels.items[1].vertices) + polygonArea(p.panels.items[3].vertices), 4.0 * std::sqrt(20.0), 1e-9);
}

TEST(Panel, SetDirectionFollowsTheSelectedSide) {
    Project p = baseProject();
    UndoStack stack;
    const int a = newPanel(p, stack), b = newPanel(p, stack);
    EXPECT_EQ(setPanelDirection(p, stack, {99}, 0, PanelDirectionMode::Parallel), PanelError::NoPanels);
    EXPECT_EQ(setPanelDirection(p, stack, {a}, 9, PanelDirectionMode::Parallel), PanelError::InvalidSide);
    EXPECT_EQ(setPanelDirection(p, stack, {a}, 0, PanelDirectionMode::Parallel), PanelError::NothingToChange);   // already along X
    ASSERT_EQ(setPanelDirection(p, stack, {a, b}, 1, PanelDirectionMode::Parallel), PanelError::None);
    EXPECT_NEAR(findPanel(p.panels, a)->direction.y, 1.0, 1e-12);
    EXPECT_NEAR(findPanel(p.panels, b)->direction.y, 1.0, 1e-12);
    ASSERT_EQ(setPanelDirection(p, stack, {a}, 1, PanelDirectionMode::Perpendicular), PanelError::None);
    EXPECT_NEAR(findPanel(p.panels, a)->direction.x, -1.0, 1e-12);
    stack.undo();
    EXPECT_NEAR(findPanel(p.panels, a)->direction.y, 1.0, 1e-12);
}

// ---------------------------------------------------------------- surface loads

TEST(SurfaceLoad, AddWithAndWithoutAssociatedPanel) {
    Project p = baseProject();
    UndoStack stack;
    const std::vector<Point3> tri{{0, 0, 0}, {4, 0, 0}, {4, 4, 0}};
    EXPECT_FALSE(surfaceLoadOverlapsPanel(p.panels, tri));   // the original then asks #63986
    int load = 0, panel = -1;
    ContourError why = ContourError::None;
    EXPECT_EQ(addSurfaceLoad(p, stack, {{0, 0, 0}, {4, 0, 0}, {8, 0, 0}}, uniform(5.0), false, &load, &panel, nullptr, &why), PanelError::InvalidContour);
    EXPECT_EQ(why, ContourError::Degenerate);
    SurfaceLoadIssue issue;
    EXPECT_EQ(addSurfaceLoad(p, stack, tri, uniform(0.0), false, &load, &panel, &issue), PanelError::InvalidData);
    EXPECT_EQ(issue.message, "No puede introducir cargas de valor nulo.");
    EXPECT_EQ(addSurfaceLoad(p, stack, tri, uniform(5.0, 9), false), PanelError::UnknownLoadCase);
    EXPECT_EQ(addSurfaceLoad(p, stack, tri, variable(1.0, 2.0), false, nullptr, nullptr, &issue), PanelError::VariableOnHorizontal);
    ASSERT_EQ(addSurfaceLoad(p, stack, tri, uniform(5.0), false, &load, &panel), PanelError::None);   // "No": the load without panel
    EXPECT_EQ(load, 1);
    EXPECT_EQ(panel, 0);
    EXPECT_TRUE(p.panels.items.empty());
    // "Sí": the panel of identical shape with the direction of the first two points, without loads.
    ASSERT_EQ(addSurfaceLoad(p, stack, {{0, 0, 0}, {4, 0, 0}, {4, 4, 0}, {0, 4, 0}}, uniform(5.0), true, &load, &panel), PanelError::None);
    EXPECT_EQ(panel, 1);
    ASSERT_EQ(p.panels.items.size(), 1u);
    EXPECT_EQ(p.panels.items[0].vertices, square());
    EXPECT_NEAR(p.panels.items[0].direction.x, 1.0, 1e-12);
    EXPECT_TRUE(p.panels.items[0].loads.empty());
    EXPECT_TRUE(surfaceLoadOverlapsPanel(p.panels, tri));
    EXPECT_TRUE(surfaceLoadOverlapsPanel(p.panels, {{3, 3, 0}, {6, 3, 0}, {6, 6, 0}}));   // partial overlap counts
    EXPECT_FALSE(surfaceLoadOverlapsPanel(p.panels, {{10, 10, 0}, {12, 10, 0}, {12, 12, 0}}));
    ASSERT_TRUE(hasSurfaceLoads(p.surfaceLoads));
    stack.undo();
    EXPECT_TRUE(p.panels.items.empty());
    EXPECT_EQ(p.surfaceLoads.items.size(), 1u);
    EXPECT_TRUE(project_json::validate(p).empty());
}

TEST(SurfaceLoad, EditCopyChangeCaseDelete) {
    Project p = baseProject();
    UndoStack stack;
    const int cm = addCase(p, stack);
    int a = 0, b = 0, c = 0;
    ASSERT_EQ(addSurfaceLoad(p, stack, square(), uniform(8.0), false, &a), PanelError::None);
    ASSERT_EQ(copySurfaceLoad(p, stack, a, uniform(8.0), &b), PanelError::None);   // two identical loads are allowed (measured)
    EXPECT_EQ(p.surfaceLoads.items.size(), 2u);
    EXPECT_EQ(findSurfaceLoad(p.surfaceLoads, b)->contour, square());
    EXPECT_EQ(copySurfaceLoad(p, stack, 99, uniform(1.0)), PanelError::UnknownLoad);
    SurfaceLoadIssue issue;
    EXPECT_EQ(copySurfaceLoad(p, stack, a, uniform(0.0), nullptr, &issue), PanelError::InvalidData);
    ASSERT_EQ(copySurfaceLoad(p, stack, a, uniform(-3.0, cm), &c), PanelError::None);
    EXPECT_EQ(findSurfaceLoad(p.surfaceLoads, c)->data.loadCaseId, cm);

    EXPECT_EQ(editSurfaceLoads(p, stack, {99}, uniform(1.0)), PanelError::UnknownLoad);
    EXPECT_EQ(editSurfaceLoads(p, stack, {a}, uniform(0.0), &issue), PanelError::InvalidData);
    EXPECT_EQ(editSurfaceLoads(p, stack, {a}, variable(1.0, 2.0), &issue), PanelError::VariableOnHorizontal);
    EXPECT_EQ(editSurfaceLoads(p, stack, {a}, uniform(8.0)), PanelError::NothingToChange);
    ASSERT_EQ(editSurfaceLoads(p, stack, {a, b}, uniform(12.0, cm)), PanelError::None);   // every selected takes the whole definition
    EXPECT_EQ(findSurfaceLoad(p.surfaceLoads, a)->data, uniform(12.0, cm));
    EXPECT_EQ(findSurfaceLoad(p.surfaceLoads, b)->data, uniform(12.0, cm));

    // "Hipótesis": the value is kept.
    EXPECT_EQ(changeSurfaceLoadCase(p, stack, {a}, 55), PanelError::UnknownLoadCase);
    EXPECT_EQ(changeSurfaceLoadCase(p, stack, {99}, 0), PanelError::UnknownLoad);
    ASSERT_EQ(changeSurfaceLoadCase(p, stack, {a}, 0), PanelError::None);
    EXPECT_EQ(findSurfaceLoad(p.surfaceLoads, a)->data.loadCaseId, 0);
    EXPECT_DOUBLE_EQ(findSurfaceLoad(p.surfaceLoads, a)->data.value, 12.0);
    EXPECT_EQ(changeSurfaceLoadCase(p, stack, {a}, 0), PanelError::NothingToChange);

    // Of two stacked loads only the selected one goes.
    ASSERT_EQ(deleteSurfaceLoads(p, stack, {b}), PanelError::None);
    EXPECT_EQ(p.surfaceLoads.items.size(), 2u);
    EXPECT_NE(findSurfaceLoad(p.surfaceLoads, a), nullptr);
    EXPECT_EQ(deleteSurfaceLoads(p, stack, {b}), PanelError::UnknownLoad);
    stack.undo();
    EXPECT_EQ(p.surfaceLoads.items.size(), 3u);
    EXPECT_EQ(surfaceLoadsOfCase(p.surfaceLoads, cm), (std::vector<int>{b, c}));
    EXPECT_TRUE(project_json::validate(p).empty());
}

TEST(SurfaceLoad, MoveVerticesWithOrWithoutTheOtherHypotheses) {
    Project p = baseProject();
    UndoStack stack;
    const int cm = addCase(p, stack);
    int a = 0, b = 0, c = 0;
    ASSERT_EQ(addSurfaceLoad(p, stack, square(), uniform(5.0), false, &a), PanelError::None);
    ASSERT_EQ(addSurfaceLoad(p, stack, square(), uniform(6.0, cm), false, &b), PanelError::None);   // same zone, another hypothesis
    ASSERT_EQ(addSurfaceLoad(p, stack, square(), uniform(7.0), false, &c), PanelError::None);       // same zone, same hypothesis
    ContourError why = ContourError::None;
    EXPECT_EQ(moveSurfaceLoadVertices(p, stack, 99, {0}, {1, 0, 0}), PanelError::UnknownLoad);
    EXPECT_EQ(moveSurfaceLoadVertices(p, stack, a, {9}, {1, 0, 0}), PanelError::InvalidVertex);
    EXPECT_EQ(moveSurfaceLoadVertices(p, stack, a, {2}, {0, 0, 1}, true, nullptr, &why), PanelError::InvalidContour);   // #65134
    EXPECT_EQ(why, ContourError::NotPlanar);
    EXPECT_EQ(findSurfaceLoad(p.surfaceLoads, a)->contour, square());
    std::vector<int> moved;
    ASSERT_EQ(moveSurfaceLoadVertices(p, stack, a, {1, 2}, {1, 0, 0}, true, &moved), PanelError::None);   // box checked by default
    EXPECT_EQ(moved, (std::vector<int>{a, b}));
    EXPECT_EQ(findSurfaceLoad(p.surfaceLoads, a)->contour[1], (Point3{5, 0, 0}));
    EXPECT_EQ(findSurfaceLoad(p.surfaceLoads, b)->contour[2], (Point3{5, 4, 0}));
    EXPECT_EQ(findSurfaceLoad(p.surfaceLoads, c)->contour, square());   // same hypothesis: not dragged
    stack.undo();
    ASSERT_EQ(moveSurfaceLoadVertices(p, stack, a, {1, 2}, {1, 0, 0}, false, &moved), PanelError::None);
    EXPECT_EQ(moved, (std::vector<int>{a}));
    EXPECT_EQ(findSurfaceLoad(p.surfaceLoads, b)->contour, square());
}

TEST(SurfaceLoad, AssignTogglesEachBarAndSkipsTies) {
    Project p = baseProject();
    UndoStack stack;
    EXPECT_EQ(toggleSurfaceBarAssignment(p, stack, {99}), PanelError::NoBars);
    // Bar 4 is a tie ("tirante"): through its description.
    p.structure.findBar(4)->sectionId = 7;
    BarDescription d;
    d.id = 1;
    d.sectionId = 7;
    d.tieSelected = true;
    p.barDescriptions.items.push_back(d);
    EXPECT_TRUE(barIsTie(p, 4));
    EXPECT_FALSE(barIsTie(p, 1));
    EXPECT_EQ(toggleSurfaceBarAssignment(p, stack, {4}), PanelError::NoBars);
    ASSERT_EQ(toggleSurfaceBarAssignment(p, stack, {1, 2, 3, 4, 1}), PanelError::None);   // first RMB: dashed
    EXPECT_EQ(p.surfaceLoads.unassignedBars, (std::set<int>{1, 2, 3}));
    EXPECT_FALSE(surfaceBarAssigned(p.surfaceLoads, 2));
    EXPECT_TRUE(surfaceBarAssigned(p.surfaceLoads, 4));
    ASSERT_EQ(toggleSurfaceBarAssignment(p, stack, {1, 2, 3}), PanelError::None);          // second RMB: back to continuous
    EXPECT_TRUE(p.surfaceLoads.unassignedBars.empty());
    ASSERT_EQ(toggleSurfaceBarAssignment(p, stack, {2}), PanelError::None);
    ASSERT_EQ(toggleSurfaceBarAssignment(p, stack, {2, 3}), PanelError::None);   // mixed states flip one by one
    EXPECT_EQ(p.surfaceLoads.unassignedBars, (std::set<int>{3}));
    EXPECT_EQ(setSurfaceBarAssignment(p, stack, {3}, false), PanelError::NothingToChange);
    ASSERT_EQ(setSurfaceBarAssignment(p, stack, {3}, true), PanelError::None);
    EXPECT_TRUE(p.surfaceLoads.unassignedBars.empty());
    stack.undo();
    EXPECT_EQ(p.surfaceLoads.unassignedBars, (std::set<int>{3}));
}

// ---------------------------------------------------------------- cascades

TEST(PanelCascade, DeletingALoadCaseDeletesItsSurfaceLoadsAndRowsButNotThePanel) {
    Project p = baseProject();
    UndoStack stack;
    const int cm = addCase(p, stack);
    const int v = addCase(p, stack, LoadNature::Wind, "V 1");
    const int a = newPanel(p, stack, square(), {uniform(10.0), uniform(4.0, v), uniform(5.0, cm)});
    int s1 = 0, s2 = 0;
    ASSERT_EQ(addSurfaceLoad(p, stack, square(), uniform(3.0, v), false, &s1), PanelError::None);
    ASSERT_EQ(addSurfaceLoad(p, stack, square(), uniform(2.0), false, &s2), PanelError::None);
    const LoadCaseDependents dep = loadCaseDependents(p, v);
    EXPECT_EQ(dep.panels, (std::vector<int>{a}));
    EXPECT_EQ(dep.surfaceLoads, (std::vector<int>{s1}));
    EXPECT_FALSE(dep.empty());
    EXPECT_TRUE(loadCaseDependents(p, 0).panels == std::vector<int>{a});
    ASSERT_EQ(deleteLoadCase(p, stack, v), LoadCaseError::None);
    ASSERT_NE(findPanel(p.panels, a), nullptr);   // the panel stays (measured)
    EXPECT_EQ(findPanel(p.panels, a)->loads.size(), 2u);
    ASSERT_EQ(p.surfaceLoads.items.size(), 1u);   // the surface load of that hypothesis goes (measured)
    EXPECT_EQ(p.surfaceLoads.items[0].id, s2);
    EXPECT_TRUE(project_json::validate(p).empty());
    stack.undo();
    EXPECT_EQ(findPanel(p.panels, a)->loads.size(), 3u);
    EXPECT_EQ(p.surfaceLoads.items.size(), 2u);
    stack.redo();
    EXPECT_EQ(p.surfaceLoads.items.size(), 1u);
    EXPECT_EQ(loadCaseDependents(p, cm).panels, (std::vector<int>{a}));
    EXPECT_TRUE(loadCaseDependents(p, 42).empty());
}

TEST(PanelCascade, DeletingANodeDeletesThePanelsOnItAndTheirSurfaceLoads) {
    Project p = baseProject();
    UndoStack stack;
    const int a = newPanel(p, stack);
    int load = 0;
    ASSERT_EQ(addSurfaceLoad(p, stack, {{1, 1, 0}, {3, 1, 0}, {3, 3, 0}, {1, 3, 0}}, uniform(5.0), false, &load), PanelError::None);
    const int other = newPanel(p, stack, {{10, 10, 0}, {12, 10, 0}, {12, 12, 0}});   // not on any node
    ASSERT_TRUE(deleteNodes(p, stack, {3}));   // N3 = (4, 4, 0): a vertex of the first panel
    EXPECT_EQ(findPanel(p.panels, a), nullptr);
    EXPECT_NE(findPanel(p.panels, other), nullptr);
    EXPECT_TRUE(p.surfaceLoads.items.empty());
    stack.undo();
    EXPECT_NE(findPanel(p.panels, a), nullptr);
    EXPECT_EQ(p.surfaceLoads.items.size(), 1u);
    stack.redo();
    EXPECT_EQ(findPanel(p.panels, a), nullptr);
}

TEST(PanelCascade, MovingANodeDragsTheVerticesOnIt) {
    Project p = baseProject();
    UndoStack stack;
    const int a = newPanel(p, stack, square(), {uniform(5.0)});
    int load = 0;
    ASSERT_EQ(addSurfaceLoad(p, stack, square(), uniform(2.0), false, &load), PanelError::None);
    ASSERT_TRUE(moveNode(p, stack, 3, {5, 4, 0}));
    EXPECT_EQ(findPanel(p.panels, a)->vertices[2], (Point3{5, 4, 0}));
    EXPECT_EQ(findSurfaceLoad(p.surfaceLoads, load)->contour[2], (Point3{5, 4, 0}));
    EXPECT_NEAR(geom::length(findPanel(p.panels, a)->direction), 1.0, 1e-12);
    stack.undo();
    EXPECT_EQ(findPanel(p.panels, a)->vertices, square());
    EXPECT_EQ(findSurfaceLoad(p.surfaceLoads, load)->contour, square());
    // Moving a node out of the plane tilts the polygon: it follows (no check, like the shells).
    stack.redo();
    ASSERT_TRUE(moveNode(p, stack, 3, {5, 4, 1}));
    EXPECT_EQ(findPanel(p.panels, a)->vertices[2], (Point3{5, 4, 1}));
}

TEST(PanelCascade, MergingNodesDragsTheVertexAndFusesCollapsedOnes) {
    Project p = baseProject();
    UndoStack stack;
    const int a = newPanel(p, stack);
    ASSERT_TRUE(moveNode(p, stack, 3, {0, 4, 0}));   // N3 lands on N4: merge
    EXPECT_EQ(p.structure.findNode(3), nullptr);
    const Panel* panel = findPanel(p.panels, a);
    ASSERT_NE(panel, nullptr);
    EXPECT_EQ(panel->vertices.size(), 3u);   // the two coincident vertices fused
    EXPECT_EQ(validatePanelContour(panel->vertices), ContourError::None);
    stack.undo();
    EXPECT_EQ(findPanel(p.panels, a)->vertices, square());
}

TEST(PanelCascade, StructureEditsThroughSnapshotCommandsFollowToo) {
    Project p = baseProject();
    p.structure.addShell({1, 2, 3, 4}, 0.1);
    UndoStack stack;
    const int a = newPanel(p, stack);
    ASSERT_EQ(moveShellNodes(p, stack, 1, {3}, {1, 0, 0}), ShellError::None);
    ASSERT_NE(findPanel(p.panels, a), nullptr);
    EXPECT_EQ(findPanel(p.panels, a)->vertices[2], (Point3{5, 4, 0}));
    stack.undo();
    EXPECT_EQ(findPanel(p.panels, a)->vertices[2], (Point3{4, 4, 0}));
}

TEST(PanelCascade, ReconcileKeepsTheBarsSwitchedOffThroughDivisionsAndPrunesVanishedOnes) {
    Project p = baseProject();
    p.surfaceLoads.unassignedBars = {1, 2};
    StructuralModel after = p.structure;
    const int mid = after.addNode({2, 0, 0});
    const int piece = after.addBar(mid, 2);   // the second half of bar 1 (N1-N2)
    ASSERT_GT(piece, 0);
    after.removeBar(2);
    Panels panels;
    PanelReconcileInfo info;
    reconcilePanels(panels, p.surfaceLoads, p.structure, after, {}, &info);
    EXPECT_TRUE(p.surfaceLoads.unassignedBars.count(1));
    EXPECT_TRUE(p.surfaceLoads.unassignedBars.count(piece));   // lies on the bar that was off
    EXPECT_FALSE(p.surfaceLoads.unassignedBars.count(2));      // vanished
    EXPECT_EQ(info.panelsRemoved, 0);
}

// ---------------------------------------------------------------- JSON

TEST(PanelJson, RoundTripOldFilesAndTolerance) {
    Project p = baseProject();
    UndoStack stack;
    const int cm = addCase(p, stack);
    const int a = newPanel(p, stack, inclined(), {uniform(-5.0, cm), variable(0.0, 6.0)});
    ASSERT_EQ(setPanelDirection(p, stack, {a}, 1, PanelDirectionMode::Perpendicular), PanelError::None);
    int s = 0;
    ASSERT_EQ(addSurfaceLoad(p, stack, inclined(), variable(6.0, -4.0, cm), false, &s), PanelError::None);
    ASSERT_EQ(addSurfaceLoad(p, stack, square(), uniform(1.5), false), PanelError::None);
    ASSERT_EQ(setSurfaceBarAssignment(p, stack, {2, 3}, false), PanelError::None);
    ASSERT_TRUE(project_json::isKnownSection("panels"));
    ASSERT_TRUE(project_json::isKnownSection("surface_loads"));
    EXPECT_EQ(project_json::sectionTitle("panels"), "Paños");
    EXPECT_EQ(project_json::sectionTitle("surface_loads"), "Cargas superficiales");
    const std::string pt = project_json::dumpSection(p, "panels"), st = project_json::dumpSection(p, "surface_loads");
    EXPECT_NE(pt.find("\"version\": 1"), std::string::npos);
    Project q = baseProject();
    q.loadCases = p.loadCases;
    project_json::loadSection(q, "panels", pt);
    project_json::loadSection(q, "surface_loads", st);
    EXPECT_EQ(q.panels, p.panels);
    EXPECT_EQ(q.surfaceLoads, p.surfaceLoads);
    EXPECT_EQ(q.panels.nextId, p.panels.nextId);
    EXPECT_EQ(q.surfaceLoads.nextId, p.surfaceLoads.nextId);
    EXPECT_TRUE(project_json::validate(q).empty());

    Project old;   // files without the sections keep opening
    project_json::loadSection(old, "panels", "{}");
    project_json::loadSection(old, "surface_loads", "{}");
    EXPECT_TRUE(old.panels.empty());
    EXPECT_TRUE(old.surfaceLoads.empty());
    EXPECT_TRUE(project_json::validate(old).empty());

    Project t = baseProject();
    project_json::loadSection(t, "surface_loads",
                              R"({"version":9,"future":1,"items":[{"id":1,"contour":[[0,0,0],[4,0,0],[4,4,0]],"load_case":0,"type":0,"value":7.2564,"extra":1},
                                  {"id":2,"contour":[[0,0,0],[1,0,0],[1,1,0]],"type":77},{"id":0,"contour":[]},{"contour":[]}],"unassigned_bars":[1,"x",-1,99]})");
    ASSERT_EQ(t.surfaceLoads.items.size(), 1u);
    EXPECT_DOUBLE_EQ(t.surfaceLoads.items[0].data.value, 7.256);
    EXPECT_EQ(t.surfaceLoads.nextId, 2);
    EXPECT_TRUE(project_json::validate(t).empty());
    EXPECT_EQ(t.surfaceLoads.unassignedBars, (std::set<int>{1, 99}));
    // A vanished bar is not saved.
    const std::string saved = project_json::dumpSection(t, "surface_loads");
    Project u = baseProject();
    project_json::loadSection(u, "surface_loads", saved);
    EXPECT_EQ(u.surfaceLoads.unassignedBars, (std::set<int>{1}));
}

TEST(PanelJson, ValidationCatchesCorruption) {
    Project p = baseProject();
    UndoStack stack;
    const int a = newPanel(p, stack, square(), {uniform(5.0)});
    int s = 0;
    ASSERT_EQ(addSurfaceLoad(p, stack, square(), uniform(5.0), false, &s), PanelError::None);
    EXPECT_TRUE(validatePanels(p.panels, p.loadCases).empty());
    EXPECT_TRUE(validateSurfaceLoads(p.surfaceLoads, p.loadCases).empty());
    Panels bad = p.panels;
    bad.items[0].loads[0].loadCaseId = 99;
    EXPECT_FALSE(validatePanels(bad, p.loadCases).empty());
    bad = p.panels;
    bad.items[0].loads[0].value = 0.0;
    EXPECT_FALSE(validatePanels(bad, p.loadCases).empty());
    bad = p.panels;
    bad.items[0].loads[0].value = 5.00049;   // not canonical
    EXPECT_FALSE(validatePanels(bad, p.loadCases).empty());
    bad = p.panels;
    bad.items[0].loads.push_back(variable(1.0, 2.0));   // variable on a horizontal panel
    EXPECT_FALSE(validatePanels(bad, p.loadCases).empty());
    bad = p.panels;
    bad.items[0].direction = {0, 0, 1};   // not in the plane
    EXPECT_FALSE(validatePanels(bad, p.loadCases).empty());
    bad = p.panels;
    bad.items[0].direction = {2, 0, 0};   // not unit
    EXPECT_FALSE(validatePanels(bad, p.loadCases).empty());
    bad = p.panels;
    bad.items[0].vertices[2] = {4, 4, 1};
    EXPECT_FALSE(validatePanels(bad, p.loadCases).empty());
    bad = p.panels;
    bad.items.push_back(bad.items[0]);
    EXPECT_FALSE(validatePanels(bad, p.loadCases).empty());
    bad = p.panels;
    bad.nextId = a;
    EXPECT_FALSE(validatePanels(bad, p.loadCases).empty());
    SurfaceLoads badLoads = p.surfaceLoads;
    badLoads.items[0].contour.pop_back();
    badLoads.items[0].contour.pop_back();
    EXPECT_FALSE(validateSurfaceLoads(badLoads, p.loadCases).empty());
    badLoads = p.surfaceLoads;
    badLoads.items[0].data.loadCaseId = 99;
    EXPECT_FALSE(validateSurfaceLoads(badLoads, p.loadCases).empty());
    badLoads = p.surfaceLoads;
    badLoads.unassignedBars.insert(-3);
    EXPECT_FALSE(validateSurfaceLoads(badLoads, p.loadCases).empty());
}

// ---------------------------------------------------------------- distribution onto bars

TEST(PanelDistribution, MeasuredSquareOnFourBars) {
    // Measured (cargas_panos.md 10.2): 4 x 4 m panel on four bars, 10 kN/m2, arrow along X -> only the bars perpendicular to the arrow, 20 kN/m each.
    Project p = baseProject();
    UndoStack stack;
    const int a = newPanel(p, stack, square(), {uniform(10.0)});
    const PanelDistribution r = distributePanels(p);
    EXPECT_TRUE(r.warnings.empty());
    ASSERT_EQ(r.loads.size(), 2u);
    const PanelBarLoad* b2 = loadOnBar(r.loads, 2);
    const PanelBarLoad* b4 = loadOnBar(r.loads, 4);
    ASSERT_NE(b2, nullptr);
    ASSERT_NE(b4, nullptr);
    EXPECT_NEAR(b2->q, 20.0, 1e-9);
    EXPECT_NEAR(b4->q, 20.0, 1e-9);
    EXPECT_NEAR(b2->s1, 0.0, 1e-9);
    EXPECT_NEAR(b2->s2, 4.0, 1e-9);
    EXPECT_NEAR(b4->s2, 4.0, 1e-9);
    EXPECT_EQ(b2->panelId, a);
    EXPECT_EQ(b2->surfaceLoadId, 0);
    EXPECT_NEAR(b2->direction.z, -1.0, 1e-12);   // positive: down
    EXPECT_EQ(loadOnBar(r.loads, 1), nullptr);
    EXPECT_EQ(loadOnBar(r.loads, 3), nullptr);

    // Arrow along Y (the second side): the other two bars.
    ASSERT_EQ(setPanelDirection(p, stack, {a}, 1, PanelDirectionMode::Parallel), PanelError::None);
    const PanelDistribution r2 = distributePanels(p);
    ASSERT_EQ(r2.loads.size(), 2u);
    EXPECT_NE(loadOnBar(r2.loads, 1), nullptr);
    EXPECT_NE(loadOnBar(r2.loads, 3), nullptr);
    EXPECT_NEAR(loadOnBar(r2.loads, 1)->q, 20.0, 1e-9);
    // "Perpendicular al lado seleccionado" of side 0 is the same as parallel to side 1.
    ASSERT_EQ(setPanelDirection(p, stack, {a}, 0, PanelDirectionMode::Parallel), PanelError::None);
    ASSERT_EQ(setPanelDirection(p, stack, {a}, 0, PanelDirectionMode::Perpendicular), PanelError::None);
    const PanelDistribution r3 = distributePanels(p);
    EXPECT_EQ(r3.loads.size(), 2u);
    EXPECT_NE(loadOnBar(r3.loads, 1), nullptr);
}

TEST(PanelDistribution, AsymmetricFractionFollowsTheInverseDistance) {
    // An intermediate bar at x = 1: the force (160 kN at x = 2) goes to the two nearest bars, d_other / (d_own + d_other): 2/3 and 1/3.
    Project p = baseProject();
    const int n5 = p.structure.addNode({1, 0, 0});
    const int n6 = p.structure.addNode({1, 4, 0});
    const int mid = p.structure.addBar(n5, n6);
    UndoStack stack;
    newPanel(p, stack, square(), {uniform(10.0)});
    const PanelDistribution r = distributePanels(p);
    const PanelBarLoad* m = loadOnBar(r.loads, mid);
    const PanelBarLoad* far = loadOnBar(r.loads, 2);
    ASSERT_NE(m, nullptr);
    ASSERT_NE(far, nullptr);
    EXPECT_NEAR(m->q * 4.0, 160.0 * 2.0 / 3.0, 1e-9);
    EXPECT_NEAR(far->q * 4.0, 160.0 / 3.0, 1e-9);
    EXPECT_EQ(loadOnBar(r.loads, 4), nullptr);   // only the two nearest bars take load
    double total = 0.0;
    for (const PanelBarLoad& l : r.loads) total += l.q * (l.s2 - l.s1);
    EXPECT_NEAR(total, 160.0, 1e-9);   // equilibrium
}

TEST(PanelDistribution, OneSidedAndUnassignedForces) {
    // Only the bar at x = 0 (bar 4): it takes everything (fraction 1).
    Project p = baseProject();
    UndoStack stack;
    newPanel(p, stack, square(), {uniform(10.0)});
    ASSERT_EQ(setSurfaceBarAssignment(p, stack, {2}, false), PanelError::None);
    PanelDistribution r = distributePanels(p);
    ASSERT_EQ(r.loads.size(), 1u);
    EXPECT_EQ(r.loads[0].barId, 4);
    EXPECT_NEAR(r.loads[0].q, 40.0, 1e-9);
    // No bar perpendicular to the arrow left: the force is not assigned (#64202).
    ASSERT_EQ(setSurfaceBarAssignment(p, stack, {4}, false), PanelError::None);
    r = distributePanels(p);
    EXPECT_TRUE(r.loads.empty());
    ASSERT_EQ(r.warnings.size(), 1u);
    EXPECT_EQ(r.warnings[0].kind, PanelWarning::Kind::NotAssigned);
    EXPECT_EQ(r.warnings[0].message, "No se han podido asignar totalmente las cargas superficiales del paño.");
    EXPECT_NEAR(r.warnings[0].where.x, 2.0, 1e-9);
    EXPECT_NEAR(r.warnings[0].where.y, 2.0, 1e-9);
}

TEST(PanelDistribution, InclinedPanelAndVariableLoad) {
    Project p;
    const int n1 = p.structure.addNode({0, 0, 0});
    const int n2 = p.structure.addNode({4, 0, 0});
    const int n3 = p.structure.addNode({4, 4, 2});
    const int n4 = p.structure.addNode({0, 4, 2});
    p.structure.addBar(n1, n2);
    const int right = p.structure.addBar(n2, n3);
    p.structure.addBar(n3, n4);
    const int left = p.structure.addBar(n4, n1);
    UndoStack stack;
    const double slope = std::sqrt(20.0);
    int a = 0;
    ASSERT_EQ(addPanel(p, stack, inclined(), 0, PanelDirectionMode::Parallel, {uniform(10.0)}, &a), PanelError::None);
    PanelDistribution r = distributePanels(p);
    ASSERT_EQ(r.loads.size(), 2u);
    EXPECT_NEAR(loadOnBar(r.loads, right)->q, 20.0, 1e-9);   // 10 x 4 x slope / 2 / slope (value per m2 of panel)
    EXPECT_NEAR(loadOnBar(r.loads, right)->s2, slope, 1e-9);
    EXPECT_NEAR(loadOnBar(r.loads, left)->q, 20.0, 1e-9);
    ASSERT_EQ(setPanelLoads(p, stack, {a}, {variable(0.0, 6.0)}), PanelError::None);   // mean height gives 3 kN/m2
    r = distributePanels(p);
    ASSERT_EQ(r.loads.size(), 2u);
    EXPECT_NEAR(loadOnBar(r.loads, right)->q, 6.0, 1e-9);
}

TEST(PanelDistribution, WindActsAlongTheNormalAndMixedSignsSplit) {
    Project p = baseProject();
    UndoStack stack;
    const int v = addCase(p, stack, LoadNature::Wind, "V 1");
    ASSERT_EQ(addPanel(p, stack, square(), 0, PanelDirectionMode::Parallel, {uniform(10.0, v), uniform(-2.0, v)}), PanelError::None);
    const PanelDistribution r = distributePanels(p);
    ASSERT_EQ(r.loads.size(), 4u);   // two rows, two bars each
    int up = 0, down = 0;
    for (const PanelBarLoad& l : r.loads) {
        EXPECT_EQ(l.loadCaseId, v);
        (l.direction.z > 0 ? up : down)++;
    }
    EXPECT_EQ(up, 2);     // +10 along the positive (upward) vector
    EXPECT_EQ(down, 2);   // -2 flips it

    // A variable load that changes sign inside the panel: total force of each part is kept.
    DistributionInput in;
    in.polygon = inclined();
    in.direction = panelDirectionFromSide(inclined(), 1);   // along the slope: the value changes along the arrow
    in.load = variable(-2.0, 6.0);
    in.positiveDirection = panelPositiveDirection(inclined(), true);
    const double slope = std::sqrt(20.0);
    in.bars = {{1, {0, 0, 0}, {4, 0, 0}}, {2, {4, 4, 2}, {0, 4, 2}}};
    const DistributionResult res = distributeSurfaceLoad(in);
    ASSERT_TRUE(res.valid);
    EXPECT_FALSE(res.unassigned());
    double positive = 0.0, negative = 0.0;
    for (const PanelBarLoad& l : res.loads) (geom::dot(l.direction, in.positiveDirection) > 0 ? positive : negative) += l.q * (l.s2 - l.s1);
    // v(t) = -2 + 8 t along the slope (zero at a quarter): positive force 1/2 x 6 x 0.75 L x 4, negative 1/2 x 2 x 0.25 L x 4
    EXPECT_NEAR(positive, 0.5 * 6.0 * (0.75 * slope) * 4.0, 1e-6);
    EXPECT_NEAR(negative, 0.5 * 2.0 * (0.25 * slope) * 4.0, 1e-6);
}

TEST(PanelDistribution, SurfaceLoadsGoToTheBarsOfTheirPanel) {
    Project p = baseProject();
    UndoStack stack;
    const int a = newPanel(p, stack, square(), {uniform(10.0)});
    int s = 0;
    // A load on the inner 2 x 2 m: 10 kN/m2 x 4 m2 = 40 kN, half to each bar perpendicular to the arrow, 10 kN/m over the central 2 m.
    ASSERT_EQ(addSurfaceLoad(p, stack, {{1, 1, 0}, {3, 1, 0}, {3, 3, 0}, {1, 3, 0}}, uniform(10.0), false, &s), PanelError::None);
    const PanelDistribution r = distributePanels(p);
    EXPECT_TRUE(r.warnings.empty());
    std::vector<PanelBarLoad> own, surface;
    for (const PanelBarLoad& l : r.loads) (l.surfaceLoadId == 0 ? own : surface).push_back(l);
    EXPECT_EQ(own.size(), 2u);
    ASSERT_EQ(surface.size(), 2u);
    for (const PanelBarLoad& l : surface) {
        EXPECT_EQ(l.surfaceLoadId, s);
        EXPECT_EQ(l.panelId, a);
        EXPECT_NEAR(l.q, 10.0, 1e-9);
        EXPECT_NEAR(l.s1, 1.0, 1e-9);
        EXPECT_NEAR(l.s2, 3.0, 1e-9);
    }
    // A surface load outside every panel is not distributed.
    ASSERT_EQ(addSurfaceLoad(p, stack, {{10, 10, 0}, {12, 10, 0}, {12, 12, 0}}, uniform(10.0)), PanelError::None);
    EXPECT_EQ(distributePanels(p).loads.size(), r.loads.size());
    // Bars switched off with "Asignar" take nothing.
    ASSERT_EQ(setSurfaceBarAssignment(p, stack, {4}, false), PanelError::None);
    for (const PanelBarLoad& l : distributePanels(p).loads) EXPECT_NE(l.barId, 4);
}

TEST(PanelDistribution, TiesNeverCarryLoadAndMergeKeepsStretchesApart) {
    Project p = baseProject();
    p.structure.findBar(4)->sectionId = 7;
    BarDescription d;
    d.id = 1;
    d.sectionId = 7;
    d.tieSelected = true;
    p.barDescriptions.items.push_back(d);
    UndoStack stack;
    newPanel(p, stack, square(), {uniform(10.0)});
    const PanelDistribution r = distributePanels(p);
    ASSERT_EQ(r.loads.size(), 1u);   // the tie (bar 4) is skipped: bar 2 takes everything
    EXPECT_EQ(r.loads[0].barId, 2);
    EXPECT_NEAR(r.loads[0].q, 40.0, 1e-9);
}

TEST(PanelDistribution, PartialStripsAndCompaction) {
    // Bar 2 is shorter than the panel side: the stretch is limited to the bar and uncompacted strips are merged when equal.
    DistributionInput in;
    in.polygon = square();
    in.direction = {1, 0, 0};
    in.load = uniform(10.0);
    in.positiveDirection = {0, 0, -1};
    in.bars = {{1, {0, 0, 0}, {0, 4, 0}}, {2, {4, 1, 0}, {4, 3, 0}}, {3, {2, 0, 0}, {2, 4, 0}}};   // x = 0, x = 4 (short) and x = 2
    in.compact = false;
    const DistributionResult raw = distributeSurfaceLoad(in);
    in.compact = true;
    const DistributionResult compact = distributeSurfaceLoad(in);
    EXPECT_GT(raw.loads.size(), compact.loads.size());
    double a = 0.0, b = 0.0;
    for (const PanelBarLoad& l : raw.loads) a += l.q * (l.s2 - l.s1);
    for (const PanelBarLoad& l : compact.loads) b += l.q * (l.s2 - l.s1);
    EXPECT_NEAR(a, b, 1e-9);
    EXPECT_NEAR(b, 160.0, 1e-6);   // all the force is assigned
    EXPECT_FALSE(compact.unassigned());
}

TEST(PanelDistribution, WarningsNoBarsOverlapAndShells) {
    Project p;
    UndoStack stack;
    newPanel(p, stack);   // no bars at all
    PanelDistribution r = distributePanels(p);
    ASSERT_EQ(r.warnings.size(), 1u);
    EXPECT_EQ(r.warnings[0].kind, PanelWarning::Kind::NoBars);
    EXPECT_EQ(r.warnings[0].message, "Se han encontrado paños sin ninguna barra asociada.");   // no shells in the project
    // Overlapped panels: #64204.
    newPanel(p, stack, {{2, 2, 0}, {6, 2, 0}, {6, 6, 0}, {2, 6, 0}});
    r = distributePanels(p);
    int overlap = 0;
    for (const PanelWarning& w : r.warnings)
        if (w.kind == PanelWarning::Kind::Overlap) {
            ++overlap;
            EXPECT_EQ(w.message, "Existen paños solapados.");
        }
    EXPECT_EQ(overlap, 1);
    EXPECT_TRUE(panelsOverlap(square(), {{1, 1, 0}, {3, 1, 0}, {3, 3, 0}}));            // contained
    EXPECT_TRUE(panelsOverlap(square(), square()));                                      // identical
    EXPECT_FALSE(panelsOverlap(square(), {{4, 0, 0}, {8, 0, 0}, {8, 4, 0}, {4, 4, 0}}));   // sharing an edge only
    EXPECT_FALSE(panelsOverlap(square(), {{0, 0, 1}, {4, 0, 1}, {4, 4, 1}, {0, 4, 1}}));   // another plane

    // With shells in the project: covered completely -> no warning; partly -> #64206; not at all -> #64205.
    Project q;
    const int n1 = q.structure.addNode({0, 0, 0}), n2 = q.structure.addNode({4, 0, 0}), n3 = q.structure.addNode({4, 4, 0}), n4 = q.structure.addNode({0, 4, 0});
    q.structure.addShell({n1, n2, n3, n4}, 0.1);
    UndoStack s2;
    newPanel(q, s2);
    EXPECT_TRUE(distributePanels(q).warnings.empty());
    newPanel(q, s2, {{2, 2, 0}, {6, 2, 0}, {6, 6, 0}, {2, 6, 0}});   // a quarter on the shell
    PanelDistribution r2 = distributePanels(q);
    bool partial = false;
    for (const PanelWarning& w : r2.warnings)
        if (w.kind == PanelWarning::Kind::NotCoveredByShells) {
            partial = true;
            EXPECT_EQ(w.message, "El paño no está cubierto completamente por láminas.");
        }
    EXPECT_TRUE(partial);
    newPanel(q, s2, {{20, 20, 0}, {24, 20, 0}, {24, 24, 0}, {20, 24, 0}});
    bool none = false;
    for (const PanelWarning& w : distributePanels(q).warnings)
        if (w.kind == PanelWarning::Kind::NoBars) {
            none = true;
            EXPECT_EQ(w.message, "Se han encontrado paños sin barras ni láminas asociadas.");
        }
    EXPECT_TRUE(none);
}

TEST(PanelDistribution, BarsOfPanelAndInvalidInputs) {
    const std::vector<DistributionBar> bars{{1, {0, 0, 0}, {4, 0, 0}}, {2, {0, 0, 1}, {4, 0, 1}}, {3, {10, 0, 0}, {12, 0, 0}}, {4, {2, -2, 0}, {2, 2, 0}}};
    const std::vector<DistributionBar> mine = barsOfPanel(square(), bars);
    ASSERT_EQ(mine.size(), 2u);   // on the edge and crossing it; the raised and the faraway ones are out
    EXPECT_EQ(mine[0].id, 1);
    EXPECT_EQ(mine[1].id, 4);
    DistributionInput in;
    in.polygon = {{0, 0, 0}, {1, 0, 0}, {2, 0, 0}};   // no plane
    in.load = uniform(1.0);
    EXPECT_FALSE(distributeSurfaceLoad(in).valid);
    in.polygon = square();
    in.direction = {0, 0, 1};   // along the normal: no direction in the plane
    EXPECT_FALSE(distributeSurfaceLoad(in).valid);
    in.direction = {1, 0, 0};
    in.positiveDirection = {0, 0, -1};
    const DistributionResult none = distributeSurfaceLoad(in);   // no bars
    EXPECT_TRUE(none.valid);
    EXPECT_TRUE(none.unassigned());
    EXPECT_TRUE(none.loads.empty());
}
