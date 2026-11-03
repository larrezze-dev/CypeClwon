#include <gtest/gtest.h>

#include "core/node_edit.h"

using namespace cc::core;

namespace {

/// Project with a material and a section registered.
Project baseProject() {
    Project p;
    Material m;
    m.name = "S275";
    p.structure.addMaterial(m);
    CrossSection s;
    s.name = "IPE 200";
    s.areaM2 = 1e-3;
    p.structure.addSection(s);
    return p;
}

}  // namespace

TEST(NodeEdit, AddNodeUndoRedoNeverReusesIds) {
    Project p;
    UndoStack stack;
    const int a = addNode(p, stack, {1, 2, 3});
    EXPECT_EQ(a, 1);
    ASSERT_NE(p.structure.findNode(a), nullptr);
    stack.undo();
    EXPECT_TRUE(p.structure.nodes.empty());
    stack.redo();
    ASSERT_NE(p.structure.findNode(a), nullptr);
    EXPECT_EQ(p.structure.findNode(a)->position, (Point3{1, 2, 3}));
    stack.undo();
    EXPECT_EQ(addNode(p, stack, {0, 0, 0}), 2);  // id 1 is not reused after undo + new edit
    EXPECT_EQ(addNode(p, stack, {NAN, 0, 0}), 0);
    EXPECT_EQ(p.structure.nodes.size(), 1u);
}

TEST(NodeEdit, MoveNodeFollowsBarsAndUndoes) {
    Project p = baseProject();
    UndoStack stack;
    const int a = p.structure.addNode({0, 0, 0});
    const int b = p.structure.addNode({0, 0, 3});
    const int bar = p.structure.addBar(a, b, 1, 1);
    ASSERT_NE(bar, 0);
    EXPECT_TRUE(moveNode(p, stack, b, {4, 0, 3}));
    EXPECT_DOUBLE_EQ(p.structure.barLength(bar), 5.0);
    stack.undo();
    EXPECT_DOUBLE_EQ(p.structure.barLength(bar), 3.0);
    stack.redo();
    EXPECT_DOUBLE_EQ(p.structure.barLength(bar), 5.0);
    EXPECT_FALSE(moveNode(p, stack, b, {NAN, 0, 3}));
    EXPECT_FALSE(moveNode(p, stack, 99, {1, 1, 1}));
    EXPECT_EQ(stack.count(), 1u);
}

TEST(NodeEdit, MoveOntoAnotherNodeMergesThemInOneUndoableCommand) {
    // N1 (0,0,0) - N2 (0,0,3) - N3 (4,0,3): moving N2 onto N3 (as measured in the original).
    Project p = baseProject();
    UndoStack stack;
    const int n1 = p.structure.addNode({0, 0, 0});
    const int n2 = p.structure.addNode({0, 0, 3});
    const int n3 = p.structure.addNode({4, 0, 3});
    const int b12 = p.structure.addBar(n1, n2, 1, 1);
    const int b23 = p.structure.addBar(n2, n3, 1, 1);
    const StructuralModel before = p.structure;
    ASSERT_TRUE(moveNode(p, stack, n2, {4, 0, 3}));
    EXPECT_EQ(stack.count(), 1u);
    EXPECT_EQ(p.structure.findNode(n2), nullptr) << "the moved node disappears";
    EXPECT_EQ(p.structure.findBar(b23), nullptr) << "the bar of length zero disappears";
    const Bar* joined = p.structure.findBar(b12);
    ASSERT_NE(joined, nullptr) << "the other bar is attached to the destination node";
    EXPECT_EQ(joined->startNode, n1);
    EXPECT_EQ(joined->endNode, n3);
    EXPECT_DOUBLE_EQ(p.structure.barLength(b12), 5.0);
    stack.undo();
    EXPECT_EQ(p.structure.nodes.size(), before.nodes.size());
    EXPECT_EQ(p.structure.bars, before.bars);
    EXPECT_EQ(p.structure.findNode(n2)->position, (Point3{0, 0, 3}));
    stack.redo();
    EXPECT_EQ(p.structure.findNode(n2), nullptr);
    EXPECT_EQ(p.structure.findBar(b12)->endNode, n3);
}

TEST(NodeEdit, MergeCarriesShellsAndRefusesToCollapseOne) {
    Project p = baseProject();
    UndoStack stack;
    const int a = p.structure.addNode({0, 0, 0});
    const int b = p.structure.addNode({1, 0, 0});
    const int c = p.structure.addNode({0, 1, 0});
    const int d = p.structure.addNode({1, 1, 0});
    const int shell = p.structure.addShell({a, b, c}, 0.1, 1);
    ASSERT_NE(shell, 0);
    EXPECT_FALSE(moveNode(p, stack, a, {1, 0, 0}));  // a and b are corners of the same shell
    EXPECT_EQ(stack.count(), 0u);
    ASSERT_TRUE(moveNode(p, stack, c, {1, 1, 0}));  // c onto d: the shell follows
    EXPECT_EQ(p.structure.findShell(shell)->nodeIds, (std::vector<int>{a, b, d}));
    stack.undo();
    EXPECT_EQ(p.structure.findShell(shell)->nodeIds, (std::vector<int>{a, b, c}));
    ASSERT_NE(p.structure.findNode(c), nullptr);
}

TEST(NodeEdit, DeleteNodeCascadesBarsAndShells) {
    Project p = baseProject();
    UndoStack stack;
    const int a = p.structure.addNode({0, 0, 0});
    const int b = p.structure.addNode({1, 0, 0});
    const int c = p.structure.addNode({0, 1, 0});
    const int d = p.structure.addNode({0, 0, 5});
    p.structure.addBar(a, b, 1, 1);
    p.structure.addBar(a, c, 1, 1);
    p.structure.addBar(a, d, 1, 1);  // a has three bars: no merge
    p.structure.addShell({a, b, c}, 0.1, 1);
    const StructuralModel before = p.structure;
    ASSERT_TRUE(deleteNodes(p, stack, {a}));
    EXPECT_EQ(p.structure.nodes.size(), 3u);
    EXPECT_TRUE(p.structure.bars.empty());
    EXPECT_TRUE(p.structure.shells.empty());
    EXPECT_EQ(p.structure.validate(), "");
    stack.undo();
    EXPECT_EQ(p.structure.nodes.size(), before.nodes.size());
    EXPECT_TRUE(p.structure.bars == before.bars);
    EXPECT_EQ(p.structure.shells.size(), 1u);
    EXPECT_EQ(p.structure.validate(), "");
    stack.redo();
    EXPECT_TRUE(p.structure.bars.empty());
    EXPECT_FALSE(deleteNodes(p, stack, {99}));
}

TEST(NodeEdit, DeleteIntermediateNodeMergesAlignedBars) {
    Project p = baseProject();
    UndoStack stack;
    const int a = p.structure.addNode({0, 0, 0});
    const int n = p.structure.addNode({0, 0, 2});
    const int b = p.structure.addNode({0, 0, 5});
    const int b1 = p.structure.addBar(a, n, 1, 1);
    const int b2 = p.structure.addBar(n, b, 1, 1);
    p.structure.findBar(b1)->rollAngleDeg = 30.0;
    p.structure.findBar(b1)->layerIds = {1};
    p.structure.nextLayerId = 5;
    p.structure.layers.push_back(Layer{4, "L4", 0xff0000, false, true, false, false});
    p.structure.findBar(b1)->layerIds = {1, 4};
    ASSERT_NE(b2, 0);

    ASSERT_TRUE(deleteNodes(p, stack, {n}));
    ASSERT_EQ(p.structure.bars.size(), 1u);
    const Bar& m = p.structure.bars[0];
    EXPECT_EQ(m.id, b2 + 1);  // new id, old ids not reused
    EXPECT_EQ(m.startNode, a);
    EXPECT_EQ(m.endNode, b);
    EXPECT_EQ(m.materialId, 1);
    EXPECT_EQ(m.sectionId, 1);
    EXPECT_DOUBLE_EQ(m.rollAngleDeg, 30.0);
    EXPECT_EQ(m.layerIds, (std::vector<int>{1, 4}));
    EXPECT_EQ(p.structure.validate(), "");

    stack.undo();
    EXPECT_EQ(p.structure.bars.size(), 2u);
    EXPECT_NE(p.structure.findNode(n), nullptr);
    EXPECT_EQ(p.structure.validate(), "");
    stack.redo();
    ASSERT_EQ(p.structure.bars.size(), 1u);
    EXPECT_EQ(p.structure.bars[0].id, b2 + 1);
}

TEST(NodeEdit, NoMergeWhenNotAlignedOrDifferentSection) {
    Project p = baseProject();
    CrossSection s2;
    s2.name = "IPE 300";
    s2.areaM2 = 2e-3;
    p.structure.addSection(s2);
    UndoStack stack;
    const int a = p.structure.addNode({0, 0, 0});
    const int n = p.structure.addNode({1, 0, 0});
    const int b = p.structure.addNode({1, 1, 0});  // L shape
    p.structure.addBar(a, n, 1, 1);
    p.structure.addBar(n, b, 1, 1);
    ASSERT_TRUE(deleteNodes(p, stack, {n}));
    EXPECT_TRUE(p.structure.bars.empty());
    stack.undo();

    const int c = p.structure.addNode({2, 0, 0});  // aligned with a-n but another section
    p.structure.addBar(n, c, 1, 2);
    p.structure.removeBar(2);
    ASSERT_TRUE(deleteNodes(p, stack, {n}));
    EXPECT_TRUE(p.structure.bars.empty());
}

TEST(NodeEdit, MergedBarKeepsDirectionOfFirstBar) {
    Project p = baseProject();
    UndoStack stack;
    const int a = p.structure.addNode({0, 0, 0});
    const int n = p.structure.addNode({2, 0, 0});
    const int b = p.structure.addNode({5, 0, 0});
    p.structure.addBar(n, a, 1, 1);  // first bar runs n -> a
    p.structure.addBar(n, b, 1, 1);
    ASSERT_TRUE(deleteNodes(p, stack, {n}));
    ASSERT_EQ(p.structure.bars.size(), 1u);
    EXPECT_EQ(p.structure.bars[0].startNode, b);
    EXPECT_EQ(p.structure.bars[0].endNode, a);
}

TEST(NodeEdit, FindNodeAtToleranceAndNearest) {
    Project p;
    const int a = p.structure.addNode({0, 0, 0});
    const int b = p.structure.addNode({1, 0, 0});
    EXPECT_EQ(findNodeAt(p.structure, {0, 0, 0}), a);
    EXPECT_EQ(findNodeAt(p.structure, {5e-7, 0, 0}), a);
    EXPECT_FALSE(findNodeAt(p.structure, {2e-6, 0, 0}).has_value());
    EXPECT_FALSE(findNodeAt(p.structure, {3, 3, 3}).has_value());
    EXPECT_EQ(findNodeAt(p.structure, {0.6, 0, 0}, 0.5), b);  // nearest within a wide tolerance
    EXPECT_EQ(findNodeAt(p.structure, {0.5, 0, 0}, 0.5), a);  // tie: first (lower id) wins
}

TEST(NodeEdit, AddNodeIfFreeSkipsDuplicatesWithoutTouchingUndo) {
    Project p;
    UndoStack stack;
    const int a = addNodeIfFree(p, stack, {1, 1, 1});
    EXPECT_EQ(a, 1);
    const auto count = stack.count();
    stack.setClean();
    EXPECT_EQ(addNodeIfFree(p, stack, {1, 1, 1}), 0);
    EXPECT_EQ(addNodeIfFree(p, stack, {1, 1, 1 + 5e-7}), 0);
    EXPECT_EQ(addNodeIfFree(p, stack, {NAN, 0, 0}), 0);
    EXPECT_EQ(addNodeIfFree(p, stack, {2e6, 0, 0}), 0);
    EXPECT_EQ(stack.count(), count);
    EXPECT_TRUE(stack.isClean());
    EXPECT_EQ(p.structure.nodes.size(), 1u);
    EXPECT_EQ(addNodeIfFree(p, stack, {2, 1, 1}), 2);
    EXPECT_EQ(stack.count(), count + 1);
    EXPECT_FALSE(stack.isClean());
}
