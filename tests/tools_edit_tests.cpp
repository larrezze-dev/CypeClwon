#include <gtest/gtest.h>

#include <cmath>

#include "core/bar_edit.h"
#include "core/geom3.h"
#include "core/piece_edit.h"
#include "core/shell_edit.h"
#include "core/tools_edit.h"

using namespace cc::core;

namespace {

int node(Project& p, double x, double y = 0, double z = 0) { return p.structure.addNode({x, y, z}); }

void expectNear(const Point3& a, const Point3& b, double tol = 1e-9) {
    EXPECT_NEAR(a.x, b.x, tol);
    EXPECT_NEAR(a.y, b.y, tol);
    EXPECT_NEAR(a.z, b.z, tol);
}

/// Two bars 0-1 and 1-2 along x with a material and a section, plus a square shell above them.
struct Frame {
    Project p;
    UndoStack stack;
    int n[3] = {}, b[2] = {};
    int sectionId = 0;
    Frame() {
        n[0] = node(p, 0);
        n[1] = node(p, 2);
        n[2] = node(p, 4);
        CrossSection sec;
        sec.areaM2 = 0.01;
        sec.name = "S";
        sectionId = p.structure.addSection(sec);
        b[0] = p.structure.addBar(n[0], n[1], 0, sectionId);
        b[1] = p.structure.addBar(n[1], n[2], 0, sectionId);
    }
};

}  // namespace

TEST(ToolsEdit, TransformFactories) {
    expectNear(translation({1, 1, 1}, {3, 0, 1}).apply({0, 0, 0}), {2, -1, 0});
    // Right-hand rotation of 90 degrees about the z axis through (1,0,0): (2,0,0) -> (1,1,0).
    expectNear(rotationInXY({1, 0, 0}, 90).apply({2, 0, 0}), {1, 1, 0});
    const auto axis = rotationAboutAxis({0, 0, 0}, {1, 0, 0}, 90);
    ASSERT_TRUE(axis.has_value());
    expectNear(axis->apply({0, 1, 0}), {0, 0, 1});
    EXPECT_FALSE(axis->reflects());
    EXPECT_FALSE(rotationAboutAxis({1, 1, 1}, {1, 1, 1}, 10).has_value());
    const auto mirror = reflectionAcrossPlane({0, 0, 0}, {1, 0, 0}, {0, 1, 0});  // plane z = 0
    ASSERT_TRUE(mirror.has_value());
    expectNear(mirror->apply({1, 2, 3}), {1, 2, -3});
    EXPECT_TRUE(mirror->reflects());
    EXPECT_FALSE(reflectionAcrossPlane({0, 0, 0}, {1, 0, 0}, {2, 0, 0}).has_value());
    const auto vertical = reflectionVertical({1, 0, 0}, {1, 5, 0});  // plane x = 1
    ASSERT_TRUE(vertical.has_value());
    expectNear(vertical->apply({3, 2, 7}), {-1, 2, 7});
    EXPECT_FALSE(reflectionVertical({0, 0, 0}, {0, 0, 5}).has_value());
}

TEST(ToolsEdit, MoveSelectionUndoRedo) {
    Frame f;
    ToolSelection sel;
    sel.barIds = {f.b[1]};
    ASSERT_EQ(transformElements(f.p, f.stack, sel, translation({0, 0, 0}, {0, 0, 3}), false), ToolError::None);
    expectNear(f.p.structure.findNode(f.n[1])->position, {2, 0, 3});  // shared node: the other bar follows by reference
    expectNear(f.p.structure.findNode(f.n[2])->position, {4, 0, 3});
    expectNear(f.p.structure.findNode(f.n[0])->position, {0, 0, 0});
    EXPECT_TRUE(f.p.structure.validate().empty());
    f.stack.undo();
    expectNear(f.p.structure.findNode(f.n[2])->position, {4, 0, 0});
    f.stack.redo();
    expectNear(f.p.structure.findNode(f.n[2])->position, {4, 0, 3});
    EXPECT_EQ(transformElements(f.p, f.stack, {}, translation({0, 0, 0}, {1, 0, 0}), false), ToolError::NothingSelected);
    ToolSelection bad;
    bad.nodeIds = {99};
    EXPECT_EQ(transformElements(f.p, f.stack, bad, translation({0, 0, 0}, {1, 0, 0}), false), ToolError::UnknownElement);
    EXPECT_EQ(transformElements(f.p, f.stack, sel, translation({0, 0, 0}, {1, 0, 0}), true, 0), ToolError::InvalidTransform);
    EXPECT_EQ(toolErrorMessage(ToolError::NothingSelected), "No hay elementos seleccionados.");
}

TEST(ToolsEdit, MoveRefusesCollapsingABar) {
    Frame f;
    ToolSelection sel;
    sel.nodeIds = {f.n[2]};
    // Moving node 2 onto node 1 would make bar 2 of zero length.
    EXPECT_EQ(transformElements(f.p, f.stack, sel, translation({4, 0, 0}, {2, 0, 0}), false), ToolError::InvalidResult);
    EXPECT_EQ(f.stack.count(), 0u);
}

TEST(ToolsEdit, RotateAboutVerticalAxis) {
    Frame f;
    ToolSelection sel;
    sel.barIds = {f.b[0], f.b[1]};
    ASSERT_EQ(transformElements(f.p, f.stack, sel, rotationInXY({0, 0, 0}, 90), false), ToolError::None);
    expectNear(f.p.structure.findNode(f.n[2])->position, {0, 4, 0}, 1e-9);
    EXPECT_NEAR(f.p.structure.barLength(f.b[0]), 2.0, 1e-9);
}

TEST(ToolsEdit, CopyBarsKeepsDescriptionPropertiesAndSharesNodes) {
    Frame f;
    f.p.structure.findBar(f.b[0])->rollAngleDeg = 30;
    ToolSelection sel;
    sel.barIds = {f.b[0], f.b[1]};
    ASSERT_EQ(transformElements(f.p, f.stack, sel, translation({0, 0, 0}, {0, 3, 0}), true), ToolError::None);
    ASSERT_EQ(f.p.structure.bars.size(), 4u);
    EXPECT_EQ(f.p.structure.nodes.size(), 6u);
    const Bar& copy = f.p.structure.bars[2];
    EXPECT_EQ(copy.sectionId, f.sectionId);  // the description / section is shared, not duplicated
    EXPECT_EQ(f.p.structure.sections.size(), 1u);
    EXPECT_DOUBLE_EQ(copy.rollAngleDeg, 30.0);
    // The copied chain is connected through the shared middle node.
    EXPECT_EQ(f.p.structure.bars[2].endNode, f.p.structure.bars[3].startNode);
    EXPECT_TRUE(f.p.structure.validate().empty());
    // Copying onto itself finds everything already there.
    ToolSelection all;
    all.barIds = {f.b[0]};
    EXPECT_EQ(transformElements(f.p, f.stack, all, translation({0, 0, 0}, {0, 0, 0}), true), ToolError::NothingCopied);
    f.stack.undo();
    EXPECT_EQ(f.p.structure.bars.size(), 2u);
    EXPECT_EQ(f.p.structure.nodes.size(), 3u);
}

TEST(ToolsEdit, CopyArrayAndPieces) {
    Frame f;
    int pieceId = 0;
    ASSERT_EQ(createPiece(f.p, f.stack, f.n[0], f.n[2], &pieceId), PieceError::None);
    ASSERT_EQ(setPieceType(f.p, f.stack, {f.b[0]}, ElementType::Beam), PieceError::None);
    ToolSelection sel;
    sel.barIds = {f.b[0], f.b[1]};
    // Three copies one metre apart in y: 1 original + 3.
    ASSERT_EQ(transformElements(f.p, f.stack, sel, translation({0, 0, 0}, {0, 1, 0}), true, 3), ToolError::None);
    EXPECT_EQ(f.p.structure.bars.size(), 8u);
    EXPECT_EQ(f.p.structure.nodes.size(), 12u);
    ASSERT_EQ(f.p.structure.pieces.size(), 4u);
    for (const Piece& p : f.p.structure.pieces) {
        EXPECT_EQ(p.barIds.size(), 2u);
        EXPECT_EQ(p.type, ElementType::Beam);
    }
    EXPECT_TRUE(f.p.structure.validate().empty());
}

TEST(ToolsEdit, MirrorCopyReversesShellContourAndMoveInPlace) {
    Project p;
    UndoStack stack;
    const int a = node(p, 1, 0, 0), b = node(p, 3, 0, 0), c = node(p, 3, 2, 0), d = node(p, 1, 2, 0);
    int sid = 0;
    ASSERT_EQ(addShell(p, stack, {a, b, c, d}, 0.3, 0, &sid), ShellError::None);
    ASSERT_EQ(setShellXEdge(p, stack, sid, a, b), ShellError::None);
    ToolSelection sel;
    sel.shellIds = {sid};
    const auto mirror = reflectionAcrossPlane({0, 0, 0}, {0, 1, 0}, {0, 0, 1});  // plane x = 0
    ASSERT_TRUE(mirror.has_value());
    ASSERT_EQ(transformElements(p, stack, sel, *mirror, true), ToolError::None);
    ASSERT_EQ(p.structure.shells.size(), 2u);
    const Shell& copy = p.structure.shells[1];
    EXPECT_DOUBLE_EQ(copy.thicknessM, 0.3);
    std::vector<Point3> pts;
    for (int n : copy.nodeIds) pts.push_back(p.structure.findNode(n)->position);
    EXPECT_GT(geom::newell(pts).z, 0);  // normal still points up (contour order reversed)
    // The x edge follows its nodes: it still runs along the copy of edge a-b (the nodes at x = -1 and -3).
    const auto axes = shellLocalAxes(p.structure, copy);
    ASSERT_TRUE(axes.has_value());
    EXPECT_NEAR(std::fabs(axes->x.x), 1.0, 1e-9);

    // Mirror in place of the original shell: contour reversed, valid, same plane side.
    ToolSelection orig;
    orig.shellIds = {sid};
    const auto flip = reflectionVertical({0, 1, 0}, {5, 1, 0});  // plane y = 1
    ASSERT_TRUE(flip.has_value());
    ASSERT_EQ(transformElements(p, stack, orig, *flip, false), ToolError::None);
    EXPECT_NEAR(p.structure.findNode(c)->position.y, 0.0, 1e-9);
    pts.clear();
    for (int n : p.structure.findShell(sid)->nodeIds) pts.push_back(p.structure.findNode(n)->position);
    EXPECT_GT(geom::newell(pts).z, 0);
    EXPECT_TRUE(p.structure.validate().empty());
}

TEST(ToolsEdit, MoveInPlaceSplitsPieceThatBreaksAlignment) {
    Frame f;
    ASSERT_EQ(createPiece(f.p, f.stack, f.n[0], f.n[2]), PieceError::None);
    ToolSelection sel;
    sel.nodeIds = {f.n[1]};
    ASSERT_EQ(transformElements(f.p, f.stack, sel, translation({0, 0, 0}, {0, 0, 1}), false), ToolError::None);
    EXPECT_EQ(f.p.structure.pieces.size(), 0u);  // two generic one-bar pieces need no record
    EXPECT_TRUE(f.p.structure.validate().empty());
}

TEST(ToolsEdit, RepairMergesNodesSplitsBarsAndShellEdges) {
    Project p;
    UndoStack stack;
    const int n1 = node(p, 0, 0), n2 = node(p, 4, 0), n3 = node(p, 8, 0), n2b = node(p, 4.0004, 0), mid = node(p, 2, 0), up = node(p, 0, 5);
    const int bA = p.structure.addBar(n1, n2), bB = p.structure.addBar(n2b, n3);
    p.structure.addBar(n1, up);
    RepairReport report;
    ASSERT_TRUE(repairModel(p, stack, &report));
    EXPECT_EQ(report.mergedNodes, 1);
    EXPECT_EQ(report.splitBars, 1);       // bar A divided by the node at x = 2
    EXPECT_TRUE(report.modified());
    bool nodesClose = false, nodeOnBar = false, unneeded = false;
    for (const RepairIssue& i : report.issues) {
        nodesClose = nodesClose || i.kind == RepairIssue::Kind::NodesTooClose;
        nodeOnBar = nodeOnBar || i.kind == RepairIssue::Kind::NodeOnBar;
        unneeded = unneeded || i.kind == RepairIssue::Kind::UnneededNode;
    }
    EXPECT_TRUE(nodesClose);
    EXPECT_TRUE(nodeOnBar);
    EXPECT_TRUE(unneeded);  // reported before repair: the node at x = 2 had no bar yet
    EXPECT_EQ(p.structure.findNode(n2b), nullptr);          // merged into the lowest id
    EXPECT_EQ(p.structure.findBar(bB)->startNode, n2);
    EXPECT_EQ(p.structure.bars.size(), 4u);                 // 3 original + 1 stretch
    EXPECT_EQ(p.structure.findBar(bA)->endNode, mid);
    EXPECT_EQ(p.structure.pieces.size(), 1u);               // the two stretches of the divided bar form one piece
    EXPECT_TRUE(p.structure.validate().empty());
    stack.undo();
    EXPECT_EQ(p.structure.bars.size(), 3u);
    EXPECT_NE(p.structure.findNode(n2b), nullptr);

    // A second run on a clean model changes nothing and pushes nothing.
    stack.redo();
    const std::size_t count = stack.count();
    RepairReport again;
    EXPECT_FALSE(repairModel(p, stack, &again));
    EXPECT_EQ(stack.count(), count);
    EXPECT_FALSE(again.modified());
    EXPECT_EQ(repairResultMessage(true), "El modelo ha sido modificado, por favor, revíselo antes de continuar.");
    EXPECT_EQ(repairResultMessage(false), "No se han realizado modificaciones en el modelo.");
    EXPECT_NE(repairConfirmationMessage().find("¿Desea continuar?"), std::string::npos);
}

TEST(ToolsEdit, RepairRemovesRepeatedBarsAndCollapsedOnes) {
    Project p;
    UndoStack stack;
    const int a = node(p, 0, 0), b = node(p, 4, 0), b2 = node(p, 4.0002, 0), c = node(p, 8, 0);
    const int x = p.structure.addBar(a, b);
    const int y = p.structure.addBar(a, b2);   // repeats x once b2 merges into b
    const int z = p.structure.addBar(b, b2);   // collapses
    const int w = p.structure.addBar(b2, c);
    RepairReport report;
    ASSERT_TRUE(repairModel(p, stack, &report));
    EXPECT_EQ(report.mergedNodes, 1);
    EXPECT_EQ(report.removedBars, 2);
    EXPECT_NE(p.structure.findBar(x), nullptr);
    EXPECT_EQ(p.structure.findBar(y), nullptr);
    EXPECT_EQ(p.structure.findBar(z), nullptr);
    EXPECT_EQ(p.structure.findBar(w)->startNode, b);
    EXPECT_TRUE(p.structure.validate().empty());
}

TEST(ToolsEdit, RepairInsertsNodeIntoShellEdgeAndSplitsLongContour) {
    Project p;
    UndoStack stack;
    const int a = node(p, 0, 0), b = node(p, 4, 0), c = node(p, 4, 4), d = node(p, 0, 4);
    const int e = node(p, 2, 0);   // on edge a-b
    const int f = node(p, 4, 2);   // on edge b-c
    const int sid = p.structure.addShell({a, b, c, d}, 0.2);
    RepairReport report;
    ASSERT_TRUE(repairModel(p, stack, &report));
    EXPECT_EQ(report.splitShellEdges, 2);
    bool edgeIssue = false;
    for (const RepairIssue& i : report.issues) edgeIssue = edgeIssue || i.kind == RepairIssue::Kind::NodeOnShellEdge;
    EXPECT_TRUE(edgeIssue);
    EXPECT_GE(p.structure.shells.size(), 2u);   // 6 nodes -> several shells of at most 4
    double area = 0;
    for (const Shell& s : p.structure.shells) {
        EXPECT_LE(s.nodeIds.size(), 4u);
        std::vector<Point3> pts;
        for (int n : s.nodeIds) pts.push_back(p.structure.findNode(n)->position);
        area += geom::newell(pts).z / 2;
    }
    EXPECT_NEAR(area, 16.0, 1e-9);
    EXPECT_NE(p.structure.findShell(sid), nullptr);
    (void)e; (void)f;
    EXPECT_TRUE(p.structure.validate().empty());
}

TEST(ToolsEdit, AnalyseReportsOriginalMessages) {
    Project p;
    const int a = node(p, 0, 0), b = node(p, 0.0005, 0), c = node(p, 5, 0), lonely = node(p, 9, 9);
    const int mid = node(p, 2, 0);
    (void)b; (void)mid;
    p.structure.addBar(a, c);
    const auto issues = analyseGeometry(p.structure);
    bool close = false, onBar = false, unneeded = false;
    for (const RepairIssue& i : issues) {
        if (i.kind == RepairIssue::Kind::NodesTooClose) {
            close = true;
            EXPECT_EQ(i.message, "Los nudos 1 y 2 están demasiado próximos entre sí (distancia: 0,0005 m).");
        }
        if (i.kind == RepairIssue::Kind::NodeOnBar) {
            onBar = true;
            EXPECT_EQ(i.message, "El nudo '5' está demasiado próximo a la barra '1' (distancia: 0 m).");
        }
        if (i.kind == RepairIssue::Kind::UnneededNode && i.ids.front() == lonely) {
            unneeded = true;
            EXPECT_EQ(i.message, "El nudo no es necesario ya que no tiene cargas ni vinculación exterior.");
        }
    }
    EXPECT_TRUE(close && onBar && unneeded);
}

// Measured in the original (herramientas.md section 11.1).
TEST(ToolsEdit, RepairLeavesNodesAt5cmAndCrossingBarsAlone) {
    Project p;
    UndoStack stack;
    const int a = node(p, 0, 0), b = node(p, 4, 0), c = node(p, 4.05, 0), d = node(p, 8, 0);
    p.structure.addBar(a, b);
    p.structure.addBar(c, d);
    // Two bars that cross without a common node.
    const int e = node(p, 0, 5), f = node(p, 4, 5), g = node(p, 2, 4), h = node(p, 2, 6);
    p.structure.addBar(e, f);
    p.structure.addBar(g, h);
    const std::size_t nodes = p.structure.nodes.size(), bars = p.structure.bars.size();
    RepairReport report;
    EXPECT_FALSE(repairModel(p, stack, &report));
    EXPECT_FALSE(report.modified());
    EXPECT_EQ(p.structure.nodes.size(), nodes);
    EXPECT_EQ(p.structure.bars.size(), bars) << "no crossing node, no division";
    EXPECT_EQ(stack.count(), 0u);
}

TEST(ToolsEdit, RepairFusesNodesAtTheToleranceAndDropsTheDegenerateBarThenUndoes) {
    Project p;
    UndoStack stack;
    const int n1 = node(p, 0, 0), n2 = node(p, 4, 0), n3 = node(p, 4, 3);
    const int n5 = node(p, 8, 0), n6 = node(p, 8.0004, 0);  // 0.4 mm apart
    p.structure.addBar(n1, n2);
    p.structure.addBar(n2, n3);
    p.structure.addBar(n6, n5);  // the degenerate bar joining them
    ASSERT_EQ(p.structure.nodes.size(), 5u);
    ASSERT_TRUE(repairModel(p, stack));
    EXPECT_EQ(p.structure.nodes.size(), 4u);
    EXPECT_EQ(p.structure.bars.size(), 2u);
    stack.undo();
    EXPECT_EQ(p.structure.nodes.size(), 5u);
    EXPECT_EQ(p.structure.bars.size(), 3u);
    // Within the tolerance (1 mm) merges, a little more does not.
    Project q;
    UndoStack qs;
    node(q, 0);
    node(q, 0.0009);
    node(q, 1);
    node(q, 1.0011);
    q.structure.addBar(1, 3);
    q.structure.addBar(2, 4);
    RepairReport report;
    ASSERT_TRUE(repairModel(q, qs, &report));
    EXPECT_EQ(report.mergedNodes, 1);
    EXPECT_EQ(q.structure.nodes.size(), 3u);
}
