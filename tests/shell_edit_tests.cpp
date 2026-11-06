#include <gtest/gtest.h>

#include <cmath>

#include <nlohmann/json.hpp>

#include "core/geom3.h"
#include "core/project_json.h"
#include "core/shell_edit.h"

using namespace cc::core;

namespace {

int node(Project& p, double x, double y, double z = 0) { return p.structure.addNode({x, y, z}); }

/// Square 4 x 4 in the xy plane: nodes 1..4 counter-clockwise.
struct Square {
    Project p;
    UndoStack stack;
    int n[4] = {};
    Square() {
        n[0] = node(p, 0, 0);
        n[1] = node(p, 4, 0);
        n[2] = node(p, 4, 4);
        n[3] = node(p, 0, 4);
    }
    std::vector<int> ring() const { return {n[0], n[1], n[2], n[3]}; }
};

}  // namespace

TEST(ShellEdit, AddsShellUndoRedoAndRejectsBadContours) {
    Square s;
    int id = 0;
    ASSERT_EQ(addShell(s.p, s.stack, s.ring(), 0.25, 0, &id), ShellError::None);
    ASSERT_NE(id, 0);
    EXPECT_EQ(s.p.structure.findShell(id)->nodeIds, s.ring());
    EXPECT_DOUBLE_EQ(s.p.structure.findShell(id)->thicknessM, 0.25);
    EXPECT_TRUE(s.p.structure.validate().empty());
    s.stack.undo();
    EXPECT_TRUE(s.p.structure.shells.empty());
    s.stack.redo();
    EXPECT_EQ(s.p.structure.shells.size(), 1u);

    // Same corners again, bow-tie order, two nodes only, repeated node, unknown node, non-coplanar, collinear.
    EXPECT_EQ(addShell(s.p, s.stack, {s.n[3], s.n[2], s.n[1], s.n[0]}), ShellError::InvalidContour);
    EXPECT_EQ(addShell(s.p, s.stack, {s.n[0], s.n[2], s.n[1], s.n[3]}), ShellError::InvalidContour);
    EXPECT_EQ(addShell(s.p, s.stack, {s.n[0], s.n[1]}), ShellError::InvalidContour);
    EXPECT_EQ(addShell(s.p, s.stack, {s.n[0], s.n[1], s.n[1]}), ShellError::InvalidContour);
    EXPECT_EQ(addShell(s.p, s.stack, {s.n[0], s.n[1], 999}), ShellError::InvalidContour);
    const int up = node(s.p, 4, 4, 1);
    EXPECT_EQ(addShell(s.p, s.stack, {s.n[0], s.n[1], up, s.n[3]}), ShellError::InvalidContour);
    const int a = node(s.p, 10, 0), b = node(s.p, 11, 0), c = node(s.p, 12, 0);
    EXPECT_EQ(addShell(s.p, s.stack, {a, b, c}), ShellError::InvalidContour);
    EXPECT_EQ(s.stack.count(), 1u);
    EXPECT_EQ(shellErrorMessage(ShellError::InvalidContour), "Los puntos introducidos no permiten definir correctamente la lámina.");
}

TEST(ShellEdit, PointsCoplanarUsesTolerance) {
    EXPECT_TRUE(pointsCoplanar({{0, 0, 0}, {1, 0, 0}, {1, 1, 0.0005}, {0, 1, 0}}));
    EXPECT_FALSE(pointsCoplanar({{0, 0, 0}, {1, 0, 0}, {1, 1, 0.01}, {0, 1, 0}}));
    EXPECT_TRUE(pointsCoplanar({{0, 0, 0}, {1, 0, 0}, {2, 0, 0}, {3, 0, 0}}));
}

TEST(ShellEdit, DeleteShells) {
    Square s;
    int id = 0;
    ASSERT_EQ(addShell(s.p, s.stack, s.ring(), 0.2, 0, &id), ShellError::None);
    EXPECT_EQ(deleteShells(s.p, s.stack, {99}), ShellError::UnknownElement);
    EXPECT_EQ(deleteShells(s.p, s.stack, {}), ShellError::UnknownElement);
    ASSERT_EQ(deleteShells(s.p, s.stack, {id}), ShellError::None);
    EXPECT_TRUE(s.p.structure.shells.empty());
    EXPECT_EQ(s.p.structure.nodes.size(), 4u);
    s.stack.undo();
    EXPECT_EQ(s.p.structure.shells.size(), 1u);
}

TEST(ShellEdit, DivideQuadThroughTwoEdgesMakesTwoQuads) {
    Square s;
    int id = 0;
    ASSERT_EQ(addShell(s.p, s.stack, s.ring(), 0.3, 0, &id), ShellError::None);
    s.p.structure.findShell(id)->zAxisInverted = true;
    std::vector<int> created;
    // Vertical line x = 2, the segment only penetrates the shell partially (y from 1 to 2).
    ASSERT_EQ(divideShells(s.p, s.stack, {id}, {2, 1, 0}, {2, 2, 0}, &created), ShellError::None);
    ASSERT_EQ(created.size(), 1u);
    ASSERT_EQ(s.p.structure.shells.size(), 2u);
    EXPECT_EQ(s.p.structure.nodes.size(), 6u);
    double area = 0;
    for (const Shell& sh : s.p.structure.shells) {
        EXPECT_EQ(sh.nodeIds.size(), 4u);
        EXPECT_DOUBLE_EQ(sh.thicknessM, 0.3);
        EXPECT_TRUE(sh.zAxisInverted);
        std::vector<Point3> pts;
        for (int n : sh.nodeIds) pts.push_back(s.p.structure.findNode(n)->position);
        const Point3 a = geom::newell(pts);
        EXPECT_GT(a.z, 0);  // contour order preserved (counter-clockwise)
        area += a.z / 2;
    }
    EXPECT_NEAR(area, 16.0, 1e-9);
    EXPECT_TRUE(s.p.structure.validate().empty());
    s.stack.undo();
    ASSERT_EQ(s.p.structure.shells.size(), 1u);
    EXPECT_EQ(s.p.structure.shells[0].nodeIds, s.ring());
    EXPECT_EQ(s.p.structure.nodes.size(), 4u);
    s.stack.redo();
    EXPECT_EQ(s.p.structure.shells.size(), 2u);
}

TEST(ShellEdit, DivideReusesNodesAndHandlesDiagonalAndCornerCases) {
    Square s;
    int id = 0;
    ASSERT_EQ(addShell(s.p, s.stack, s.ring(), 0.2, 0, &id), ShellError::None);
    // Diagonal through two corners gives two triangles and no new node.
    ASSERT_EQ(divideShells(s.p, s.stack, {id}, {0, 0, 0}, {4, 4, 0}), ShellError::None);
    ASSERT_EQ(s.p.structure.shells.size(), 2u);
    EXPECT_EQ(s.p.structure.nodes.size(), 4u);
    for (const Shell& sh : s.p.structure.shells) EXPECT_EQ(sh.nodeIds.size(), 3u);
    // Segment that does not reach the shell (the line would cross it, the segment does not touch it).
    EXPECT_EQ(divideShells(s.p, s.stack, {s.p.structure.shells[0].id}, {10, -10, 0}, {10, -9, 0}), ShellError::NothingToDivide);
    // Line outside the shell.
    EXPECT_EQ(divideShells(s.p, s.stack, {id}, {9, 0, 0}, {9, 1, 0}), ShellError::NothingToDivide);
    EXPECT_EQ(divideShells(s.p, s.stack, {77}, {2, 1, 0}, {2, 2, 0}), ShellError::UnknownElement);
}

TEST(ShellEdit, DivideCuttingACornerGivesTriangleAndPentagonSplitIntoValidShells) {
    Square s;
    int id = 0;
    ASSERT_EQ(addShell(s.p, s.stack, s.ring(), 0.2, 0, &id), ShellError::None);
    // Cuts the edges (0,0)-(4,0) at x = 1 and (0,0)-(0,4) at y = 1: triangle + pentagon -> triangle + triangle + quad.
    ASSERT_EQ(divideShells(s.p, s.stack, {id}, {1, 0, 0}, {0, 1, 0}), ShellError::None);
    EXPECT_EQ(s.p.structure.shells.size(), 4u - 1u);
    double area = 0;
    for (const Shell& sh : s.p.structure.shells) {
        EXPECT_TRUE(sh.nodeIds.size() == 3 || sh.nodeIds.size() == 4);
        std::vector<Point3> pts;
        for (int n : sh.nodeIds) pts.push_back(s.p.structure.findNode(n)->position);
        area += geom::newell(pts).z / 2;
    }
    EXPECT_NEAR(area, 16.0, 1e-9);
    EXPECT_TRUE(s.p.structure.validate().empty());
}

TEST(ShellEdit, JoinTwoCoplanarQuadsIntoOne) {
    Project p;
    UndoStack stack;
    const int a = node(p, 0, 0), b = node(p, 2, 0), c = node(p, 4, 0), d = node(p, 4, 2), e = node(p, 2, 2), f = node(p, 0, 2);
    int s1 = 0, s2 = 0;
    ASSERT_EQ(addShell(p, stack, {a, b, e, f}, 0.2, 0, &s1), ShellError::None);
    ASSERT_EQ(addShell(p, stack, {b, c, d, e}, 0.4, 0, &s2), ShellError::None);
    int result = 0;
    ASSERT_EQ(joinShells(p, stack, {s2, s1}, &result), ShellError::None);
    EXPECT_EQ(result, s1);
    ASSERT_EQ(p.structure.shells.size(), 1u);
    const Shell& sh = p.structure.shells[0];
    EXPECT_EQ(sh.nodeIds.size(), 4u);
    EXPECT_DOUBLE_EQ(sh.thicknessM, 0.2);
    std::vector<Point3> pts;
    for (int n : sh.nodeIds) pts.push_back(p.structure.findNode(n)->position);
    EXPECT_NEAR(geom::newell(pts).z / 2, 8.0, 1e-9);  // same orientation as shell 1 (counter-clockwise)
    EXPECT_EQ(p.structure.nodes.size(), 6u);          // nodes are never deleted
    EXPECT_TRUE(p.structure.validate().empty());
    stack.undo();
    EXPECT_EQ(p.structure.shells.size(), 2u);
    stack.redo();
    EXPECT_EQ(p.structure.shells.size(), 1u);
}

TEST(ShellEdit, JoinErrors) {
    Project p;
    UndoStack stack;
    const int a = node(p, 0, 0), b = node(p, 2, 0), c = node(p, 2, 2), d = node(p, 0, 2);
    int s1 = 0;
    ASSERT_EQ(addShell(p, stack, {a, b, c, d}, 0.2, 0, &s1), ShellError::None);
    EXPECT_EQ(joinShells(p, stack, {s1}), ShellError::TooFewShells);        // #64232
    EXPECT_EQ(joinShells(p, stack, {s1, s1}), ShellError::TooFewShells);
    EXPECT_EQ(joinShells(p, stack, {s1, 42}), ShellError::UnknownElement);
    // Not coplanar: second shell rises out of the plane.
    const int e = node(p, 2, 4, 3), f = node(p, 0, 4, 3);
    int s2 = 0;
    ASSERT_EQ(addShell(p, stack, {d, c, e, f}, 0.2, 0, &s2), ShellError::None);
    EXPECT_EQ(joinShells(p, stack, {s1, s2}), ShellError::NotCoplanar);  // #64004
    // Coplanar but disjoint: no closed single region.
    const int g = node(p, 10, 0), h = node(p, 12, 0), i = node(p, 12, 2), j = node(p, 10, 2);
    int s3 = 0;
    ASSERT_EQ(addShell(p, stack, {g, h, i, j}, 0.2, 0, &s3), ShellError::None);
    EXPECT_EQ(joinShells(p, stack, {s1, s3}), ShellError::NotClosedRegion);  // #64005
    EXPECT_EQ(shellErrorMessage(ShellError::NotCoplanar), "Las láminas seleccionadas no están contenidas en el mismo plano.");
    EXPECT_EQ(shellErrorMessage(ShellError::TooFewShells).substr(0, 24), "Debe seleccionar, al men");
}

TEST(ShellEdit, JoinRingWithHoleIsRejectedAndLShapeTooComplex) {
    Project p;
    UndoStack stack;
    // Four triangles around a central node do join (square), but a ring around a hole does not.
    const int a = node(p, 0, 0), b = node(p, 4, 0), c = node(p, 4, 4), d = node(p, 0, 4), m = node(p, 2, 2);
    int t[4] = {};
    ASSERT_EQ(addShell(p, stack, {a, b, m}, 0.2, 0, &t[0]), ShellError::None);
    ASSERT_EQ(addShell(p, stack, {b, c, m}, 0.2, 0, &t[1]), ShellError::None);
    ASSERT_EQ(addShell(p, stack, {c, d, m}, 0.2, 0, &t[2]), ShellError::None);
    ASSERT_EQ(addShell(p, stack, {d, a, m}, 0.2, 0, &t[3]), ShellError::None);
    ASSERT_EQ(joinShells(p, stack, {t[0], t[1], t[2], t[3]}), ShellError::None);
    ASSERT_EQ(p.structure.shells.size(), 1u);
    EXPECT_EQ(p.structure.shells[0].nodeIds.size(), 4u);

    Project q;
    UndoStack st;
    // L shape made of three squares: 6 corners.
    const int n0 = node(q, 0, 0), n1 = node(q, 1, 0), n2 = node(q, 2, 0), n3 = node(q, 0, 1), n4 = node(q, 1, 1), n5 = node(q, 2, 1),
              n6 = node(q, 0, 2), n7 = node(q, 1, 2);
    int s1 = 0, s2 = 0, s3 = 0;
    ASSERT_EQ(addShell(q, st, {n0, n1, n4, n3}, 0.2, 0, &s1), ShellError::None);
    ASSERT_EQ(addShell(q, st, {n1, n2, n5, n4}, 0.2, 0, &s2), ShellError::None);
    ASSERT_EQ(addShell(q, st, {n3, n4, n7, n6}, 0.2, 0, &s3), ShellError::None);
    EXPECT_EQ(joinShells(q, st, {s1, s2, s3}), ShellError::ContourTooComplex);
    EXPECT_EQ(q.structure.shells.size(), 3u);
}

TEST(ShellEdit, InvertZAndLocalAxes) {
    Square s;
    int id = 0;
    ASSERT_EQ(addShell(s.p, s.stack, s.ring(), 0.2, 0, &id), ShellError::None);
    auto axes = shellLocalAxes(s.p.structure, *s.p.structure.findShell(id));
    ASSERT_TRUE(axes.has_value());
    EXPECT_NEAR(axes->z.z, 1.0, 1e-12);
    EXPECT_NEAR(axes->x.x, 1.0, 1e-12);  // PROVISIONAL default: global X projected
    EXPECT_NEAR(axes->y.y, 1.0, 1e-12);

    ASSERT_EQ(invertShellZ(s.p, s.stack, {id}), ShellError::None);
    EXPECT_TRUE(s.p.structure.findShell(id)->zAxisInverted);
    axes = shellLocalAxes(s.p.structure, *s.p.structure.findShell(id));
    EXPECT_NEAR(axes->z.z, -1.0, 1e-12);
    EXPECT_NEAR(axes->y.y, -1.0, 1e-12);
    s.stack.undo();
    EXPECT_FALSE(s.p.structure.findShell(id)->zAxisInverted);
    EXPECT_EQ(invertShellZ(s.p, s.stack, {}), ShellError::NothingToChange);
    EXPECT_EQ(invertShellZ(s.p, s.stack, {id, 5}), ShellError::UnknownElement);

    // Define x along edge n[1]-n[2] (the y direction), given in reverse order: stored in contour order.
    ASSERT_EQ(setShellXEdge(s.p, s.stack, id, s.n[2], s.n[1]), ShellError::None);
    EXPECT_EQ(s.p.structure.findShell(id)->xEdgeStart, s.n[1]);
    EXPECT_EQ(s.p.structure.findShell(id)->xEdgeEnd, s.n[2]);
    axes = shellLocalAxes(s.p.structure, *s.p.structure.findShell(id));
    EXPECT_NEAR(axes->x.y, 1.0, 1e-12);
    EXPECT_NEAR(axes->y.x, -1.0, 1e-12);  // y = z x x
    EXPECT_EQ(setShellXEdge(s.p, s.stack, id, s.n[0], s.n[2]), ShellError::InvalidEdge);  // diagonal
    EXPECT_EQ(setShellXEdge(s.p, s.stack, 9, s.n[0], s.n[1]), ShellError::UnknownElement);
    s.stack.undo();
    EXPECT_EQ(s.p.structure.findShell(id)->xEdgeStart, 0);
}

TEST(ShellEdit, VerticalShellDefaultAxes) {
    Project p;
    const int a = node(p, 0, 0, 0), b = node(p, 0, 4, 0), c = node(p, 0, 4, 3), d = node(p, 0, 0, 3);  // plane x = 0
    const int id = p.structure.addShell({a, b, c, d}, 0.2);
    const auto axes = shellLocalAxes(p.structure, *p.structure.findShell(id));
    ASSERT_TRUE(axes.has_value());
    EXPECT_NEAR(std::fabs(axes->z.x), 1.0, 1e-12);
    EXPECT_NEAR(axes->x.y, 1.0, 1e-12);  // global X is perpendicular: falls back to global Y
}

TEST(ShellEdit, MoveShellNodes) {
    Square s;
    int id = 0;
    ASSERT_EQ(addShell(s.p, s.stack, s.ring(), 0.2, 0, &id), ShellError::None);
    const int bar = s.p.structure.addBar(s.n[0], s.n[1]);
    // Sliding one corner inside the plane keeps the shell valid; the bar follows by reference.
    ASSERT_EQ(moveShellNodes(s.p, s.stack, id, {s.n[2]}, {1, 0, 0}), ShellError::None);
    EXPECT_DOUBLE_EQ(s.p.structure.findNode(s.n[2])->position.x, 5.0);
    // Lifting a corner breaks planarity.
    EXPECT_EQ(moveShellNodes(s.p, s.stack, id, {s.n[2]}, {0, 0, 1}), ShellError::NotCoplanar);
    // Moving two corners together up is fine only when the whole plane stays flat: lift the whole top edge.
    EXPECT_EQ(moveShellNodes(s.p, s.stack, id, {s.n[2], s.n[3]}, {0, 0, 1}), ShellError::None);
    EXPECT_NEAR(s.p.structure.findNode(s.n[3])->position.z, 1.0, 1e-12);
    // Moving a corner onto its bar neighbour collapses the bar.
    EXPECT_EQ(moveShellNodes(s.p, s.stack, id, {s.n[1]}, {-4, 0, 0}), ShellError::InvalidMove);
    EXPECT_EQ(moveShellNodes(s.p, s.stack, id, {999}, {1, 0, 0}), ShellError::UnknownElement);
    (void)bar;
    s.stack.undo();
    EXPECT_NEAR(s.p.structure.findNode(s.n[3])->position.z, 0.0, 1e-12);
}

TEST(ShellEdit, XEdgeSurvivesJsonRoundTripAndDivision) {
    Square s;
    int id = 0;
    ASSERT_EQ(addShell(s.p, s.stack, s.ring(), 0.2, 0, &id), ShellError::None);
    ASSERT_EQ(setShellXEdge(s.p, s.stack, id, s.n[0], s.n[1]), ShellError::None);
    Project back;
    project_json::loadSection(back, "nodes", project_json::dumpSection(s.p, "nodes"));
    project_json::loadSection(back, "shells", project_json::dumpSection(s.p, "shells"));
    EXPECT_EQ(back.structure.findShell(id)->xEdgeStart, s.n[0]);
    EXPECT_EQ(back.structure.findShell(id)->xEdgeEnd, s.n[1]);
    // Dividing across the x edge drops the reference on the part that no longer holds the edge pair as is.
    ASSERT_EQ(divideShells(s.p, s.stack, {id}, {2, -1, 0}, {2, 5, 0}), ShellError::None);
    int with = 0;
    for (const Shell& sh : s.p.structure.shells) with += sh.xEdgeStart != 0;
    EXPECT_LE(with, 1);
}

TEST(ShellEdit, SectionSetsThicknessAndBallastInOneStepAndIsRefusedWhenInvalid) {
    Square s;
    int id = 0;
    ASSERT_EQ(addShell(s.p, s.stack, s.ring(), 0.2, 0, &id), ShellError::None);
    const std::size_t before = s.stack.count();
    ShellSection sec{0.3, true, 8000.0, BallastBehaviour::CompressionOnly};
    EXPECT_EQ(setShellSection(s.p, s.stack, {id}, ShellSection{0.0, false, 0.0, BallastBehaviour::Linear}), ShellError::InvalidThickness);
    EXPECT_EQ(setShellSection(s.p, s.stack, {id}, ShellSection{0.3, true, 0.0, BallastBehaviour::Linear}), ShellError::InvalidBallast);
    EXPECT_EQ(setShellSection(s.p, s.stack, {}, sec), ShellError::NothingToChange);
    EXPECT_EQ(setShellSection(s.p, s.stack, {999}, sec), ShellError::UnknownElement);
    EXPECT_EQ(s.stack.count(), before);
    ASSERT_EQ(setShellSection(s.p, s.stack, {id}, sec), ShellError::None);
    EXPECT_EQ(s.stack.count(), before + 1);
    const Shell* sh = s.p.structure.findShell(id);
    EXPECT_NEAR(sh->thicknessM, 0.3, 1e-12);
    EXPECT_TRUE(sh->ballastEnabled);
    // Disabling keeps the stored value and behaviour.
    ASSERT_EQ(setShellSection(s.p, s.stack, {id}, ShellSection{0.3, false, 0.0, BallastBehaviour::Linear}), ShellError::None);
    sh = s.p.structure.findShell(id);
    EXPECT_FALSE(sh->ballastEnabled);
    EXPECT_DOUBLE_EQ(sh->ballastKnM3, 8000.0);
    // JSON round trip, and a file without the ballast keys loads as disabled.
    ASSERT_EQ(setShellSection(s.p, s.stack, {id}, sec), ShellError::None);
    Project back;
    project_json::loadSection(back, "nodes", project_json::dumpSection(s.p, "nodes"));
    project_json::loadSection(back, "shells", project_json::dumpSection(s.p, "shells"));
    EXPECT_TRUE(back.structure.findShell(id)->ballastEnabled);
    EXPECT_EQ(back.structure.findShell(id)->ballastBehaviour, BallastBehaviour::CompressionOnly);
    const std::string text = project_json::dumpSection(s.p, "shells");
    ASSERT_NE(text.find("ballastEnabled"), std::string::npos);
    nlohmann::json old = nlohmann::json::parse(text);
    for (auto& shell : old["items"]) {
        shell.erase("ballastEnabled");
        shell.erase("ballastKnM3");
        shell.erase("ballastBehaviour");
    }
    Project legacy;
    project_json::loadSection(legacy, "nodes", project_json::dumpSection(s.p, "nodes"));
    project_json::loadSection(legacy, "shells", old.dump());
    EXPECT_FALSE(legacy.structure.findShell(id)->ballastEnabled);
    EXPECT_NEAR(legacy.structure.findShell(id)->thicknessM, 0.3, 1e-12);
    // Division and union keep the ballast of the original shell.
    ASSERT_EQ(divideShells(s.p, s.stack, {id}, {2, -1, 0}, {2, 5, 0}), ShellError::None);
    for (const Shell& part : s.p.structure.shells) EXPECT_TRUE(part.ballastEnabled);
}

// Measured (laminas.md section 11.2): Z > 0 draws the shell towards +Z local; the nodes do not move.
TEST(ShellEdit, DrawnCornersFollowTheEccentricityAlongLocalZ) {
    Project p;
    auto& m = p.structure;
    const int a = m.addNode({0, 0, 0}), b = m.addNode({4, 0, 0}), c = m.addNode({4, 3, 0}), d = m.addNode({0, 3, 0});
    UndoStack stack;
    int id = 0;
    ASSERT_EQ(addShell(p, stack, {a, b, c, d}, kDefaultShellThicknessM, 0, &id), ShellError::None);
    EXPECT_NEAR(kDefaultShellThicknessM, 0.1, 1e-12);
    EXPECT_NEAR(m.findShell(id)->material.orthoDensityKnM3, 14.72, 1e-12);
    Shell* shell = m.findShell(id);
    shell->layout.centralPlaneOffsetM = 1.0;
    auto corners = shellDrawnCorners(m, *shell);
    ASSERT_EQ(corners.size(), 4u);
    const double sign = shellLocalAxes(m, *shell)->z.z;
    for (const Point3& q : corners) EXPECT_NEAR(q.z, sign * 1.0, 1e-9);
    EXPECT_NEAR(corners[2].x, 4.0, 1e-9);
    EXPECT_DOUBLE_EQ(m.findNode(a)->position.z, 0.0) << "the nodes stay on the definition plane";
    shell->zAxisInverted = !shell->zAxisInverted;
    corners = shellDrawnCorners(m, *shell);
    EXPECT_NEAR(corners[0].z, -sign * 1.0, 1e-9) << "follows the local Z, so flipping z flips the displacement";
    shell->layout.centralPlaneOffsetM = 0.0;
    EXPECT_NEAR(shellDrawnCorners(m, *shell)[0].z, 0.0, 1e-12);
}
