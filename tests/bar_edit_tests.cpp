#include <gtest/gtest.h>

#include "core/bar_edit.h"

using namespace cc::core;

namespace {

int node(Project& p, double x, double y, double z) { return p.structure.addNode({x, y, z}); }

}  // namespace

TEST(BarEdit, AddBarUndoRedoAndDefaults) {
    Project p;
    UndoStack stack;
    const int a = node(p, 0, 0, 0), b = node(p, 4, 0, 0);
    const auto ids = addBar(p, stack, a, b);
    ASSERT_EQ(ids.size(), 1u);
    const Bar* bar = p.structure.findBar(ids[0]);
    ASSERT_NE(bar, nullptr);
    EXPECT_EQ(bar->materialId, 0);
    EXPECT_EQ(bar->sectionId, 0);
    EXPECT_EQ(bar->startNode, a);
    EXPECT_EQ(bar->endNode, b);
    EXPECT_EQ(bar->layerIds, std::vector<int>{kDefaultLayerId});
    stack.undo();
    EXPECT_TRUE(p.structure.bars.empty());
    stack.redo();
    ASSERT_EQ(p.structure.bars.size(), 1u);
    EXPECT_EQ(p.structure.bars[0].id, ids[0]);
    stack.undo();
    stack.redo();
    EXPECT_EQ(p.structure.bars.size(), 1u);
    EXPECT_TRUE(p.structure.validate().empty());
}

TEST(BarEdit, AddBarUsesActiveLayer) {
    Project p;
    UndoStack stack;
    const int layer = p.structure.addLayer("Cubierta");
    p.structure.setActiveLayer(layer);
    const int a = node(p, 0, 0, 0), b = node(p, 0, 3, 0);
    const auto ids = addBar(p, stack, a, b);
    ASSERT_EQ(ids.size(), 1u);
    EXPECT_EQ(p.structure.findBar(ids[0])->layerIds, std::vector<int>{layer});
}

TEST(BarEdit, AddBarInvalidCases) {
    Project p;
    UndoStack stack;
    const int a = node(p, 0, 0, 0), b = node(p, 4, 0, 0), c = node(p, 4, 0, 1e-9);
    EXPECT_TRUE(addBar(p, stack, a, a).empty());
    EXPECT_TRUE(addBar(p, stack, a, 99).empty());
    EXPECT_TRUE(addBar(p, stack, 99, a).empty());
    EXPECT_TRUE(addBar(p, stack, b, c).empty());  // shorter than kMinBarLengthM
    EXPECT_FALSE(addBar(p, stack, a, b).empty());
    EXPECT_TRUE(addBar(p, stack, a, b).empty());  // duplicate
    EXPECT_TRUE(addBar(p, stack, b, a).empty());  // duplicate in the opposite direction
    EXPECT_EQ(p.structure.bars.size(), 1u);
    EXPECT_FALSE(AddBarCommand(p, a, a).valid());
}

TEST(BarEdit, IdsAreNotReusedAfterUndo) {
    Project p;
    UndoStack stack;
    const int a = node(p, 0, 0, 0), b = node(p, 4, 0, 0), c = node(p, 8, 0, 0);
    const auto first = addBar(p, stack, a, b);
    stack.undo();
    const auto second = addBar(p, stack, b, c);
    ASSERT_EQ(first.size(), 1u);
    ASSERT_EQ(second.size(), 1u);
    EXPECT_NE(first[0], second[0]);
}

TEST(BarEdit, AddBarSplitsAtIntermediateNodes) {
    Project p;
    UndoStack stack;
    const int a = node(p, 0, 0, 0), b = node(p, 10, 0, 0);
    const int m2 = node(p, 6, 0, 0), m1 = node(p, 3, 0, 0);  // created out of order
    node(p, 5, 1, 0);                                         // off the line
    const auto ids = addBar(p, stack, a, b, true);
    ASSERT_EQ(ids.size(), 3u);
    EXPECT_EQ(p.structure.findBar(ids[0])->startNode, a);
    EXPECT_EQ(p.structure.findBar(ids[0])->endNode, m1);
    EXPECT_EQ(p.structure.findBar(ids[1])->startNode, m1);
    EXPECT_EQ(p.structure.findBar(ids[1])->endNode, m2);
    EXPECT_EQ(p.structure.findBar(ids[2])->endNode, b);
    stack.undo();  // single command
    EXPECT_TRUE(p.structure.bars.empty());
    stack.redo();
    EXPECT_EQ(p.structure.bars.size(), 3u);
    EXPECT_TRUE(p.structure.validate().empty());
}

TEST(BarEdit, AddBarWithoutCutOptionStaysWhole) {
    Project p;
    UndoStack stack;
    const int a = node(p, 0, 0, 0), b = node(p, 10, 0, 0);
    node(p, 5, 0, 0);
    EXPECT_EQ(addBar(p, stack, a, b, false).size(), 1u);
    EXPECT_EQ(p.structure.bars.size(), 1u);
}

TEST(BarEdit, SplitSkipsExistingStretchesAndRejectsWhenNothingNew) {
    Project p;
    UndoStack stack;
    const int a = node(p, 0, 0, 0), m = node(p, 5, 0, 0), b = node(p, 10, 0, 0);
    ASSERT_FALSE(addBar(p, stack, a, m, false).empty());
    const auto ids = addBar(p, stack, a, b, true);
    ASSERT_EQ(ids.size(), 1u);  // only m-b is new
    EXPECT_EQ(p.structure.findBar(ids[0])->startNode, m);
    EXPECT_TRUE(addBar(p, stack, a, b, true).empty());  // a-b is not a bar, but both stretches exist: nothing new
    EXPECT_EQ(p.structure.bars.size(), 2u);
}

TEST(BarEdit, SplitIgnoresNodesBeyondTheEnds) {
    Project p;
    UndoStack stack;
    const int a = node(p, 0, 0, 0), b = node(p, 4, 0, 0);
    node(p, 8, 0, 0);
    node(p, -2, 0, 0);
    EXPECT_EQ(addBar(p, stack, a, b).size(), 1u);
}

TEST(BarEdit, BeamChainCreatesOneBarPerPair) {
    Project p;
    UndoStack stack;
    const int a = node(p, 0, 0, 3), b = node(p, 4, 0, 3), c = node(p, 8, 0, 3), d = node(p, 8, 5, 3);
    const auto ids = addBeamChain(p, stack, {a, b, c, d});
    ASSERT_EQ(ids.size(), 3u);
    EXPECT_EQ(p.structure.findBar(ids[2])->startNode, c);
    EXPECT_EQ(p.structure.findBar(ids[2])->endNode, d);
    stack.undo();
    EXPECT_TRUE(p.structure.bars.empty());
    stack.redo();
    EXPECT_EQ(p.structure.bars.size(), 3u);
}

TEST(BarEdit, BeamChainSkipsDuplicatesAndRepeatedNodes) {
    Project p;
    UndoStack stack;
    const int a = node(p, 0, 0, 0), b = node(p, 4, 0, 0), c = node(p, 8, 0, 0);
    ASSERT_FALSE(addBar(p, stack, a, b, false).empty());
    const auto ids = addBeamChain(p, stack, {a, b, b, c, b, a}, false);
    ASSERT_EQ(ids.size(), 1u);  // a-b exists, b-b skipped, b-c new, c-b / b-a duplicates
    EXPECT_EQ(p.structure.bars.size(), 2u);
}

TEST(BarEdit, BeamChainInvalidCases) {
    Project p;
    UndoStack stack;
    const int a = node(p, 0, 0, 0), b = node(p, 4, 0, 0);
    EXPECT_TRUE(addBeamChain(p, stack, {}).empty());
    EXPECT_TRUE(addBeamChain(p, stack, {a}).empty());
    EXPECT_TRUE(addBeamChain(p, stack, {a, 77}).empty());
    EXPECT_TRUE(addBeamChain(p, stack, {a, a}).empty());
    ASSERT_FALSE(addBeamChain(p, stack, {a, b}).empty());
    EXPECT_TRUE(addBeamChain(p, stack, {b, a}).empty());  // everything skipped
    EXPECT_EQ(p.structure.bars.size(), 1u);
}

TEST(BarEdit, BeamChainSplitsAtCutNodes) {
    Project p;
    UndoStack stack;
    const int a = node(p, 0, 0, 0), b = node(p, 10, 0, 0), c = node(p, 10, 5, 0);
    node(p, 5, 0, 0);
    EXPECT_EQ(addBeamChain(p, stack, {a, b, c}, true).size(), 3u);
    stack.undo();
    EXPECT_EQ(addBeamChain(p, stack, {a, b, c}, false).size(), 2u);
}

TEST(BarEdit, ColumnCreatesNodesAndAscendingBars) {
    Project p;
    UndoStack stack;
    const auto ids = addColumn(p, stack, 2, 3, {6, 0, 3});  // unsorted levels
    ASSERT_EQ(ids.size(), 2u);
    EXPECT_EQ(p.structure.nodes.size(), 3u);
    for (int id : ids) {
        const Bar* bar = p.structure.findBar(id);
        EXPECT_LT(p.structure.findNode(bar->startNode)->position.z, p.structure.findNode(bar->endNode)->position.z);
        EXPECT_DOUBLE_EQ(p.structure.findNode(bar->startNode)->position.x, 2);
        EXPECT_DOUBLE_EQ(p.structure.findNode(bar->endNode)->position.y, 3);
    }
    EXPECT_DOUBLE_EQ(p.structure.barLength(ids[0]), 3.0);
    stack.undo();
    EXPECT_TRUE(p.structure.bars.empty());
    EXPECT_TRUE(p.structure.nodes.empty());
    stack.redo();
    EXPECT_EQ(p.structure.nodes.size(), 3u);
    EXPECT_EQ(p.structure.bars.size(), 2u);
    EXPECT_TRUE(p.structure.validate().empty());
}

TEST(BarEdit, ColumnReusesExistingNodes) {
    Project p;
    UndoStack stack;
    const int base = node(p, 1, 1, 0);
    const auto ids = addColumn(p, stack, 1, 1, {0, 3});
    ASSERT_EQ(ids.size(), 1u);
    EXPECT_EQ(p.structure.nodes.size(), 2u);
    EXPECT_EQ(p.structure.findBar(ids[0])->startNode, base);
    stack.undo();
    EXPECT_NE(p.structure.findNode(base), nullptr);  // the reused node survives the undo
    EXPECT_EQ(p.structure.nodes.size(), 1u);
}

TEST(BarEdit, ColumnNeedsTwoDistinctLevels) {
    Project p;
    UndoStack stack;
    AddColumnCommand none(p, 0, 0, {});
    EXPECT_FALSE(none.valid());
    EXPECT_EQ(none.error(),
              "Para poder introducir pilares con esta opción debe definir previamente al menos 2 niveles en la obra.");
    EXPECT_TRUE(addColumn(p, stack, 0, 0, {3}).empty());
    EXPECT_TRUE(addColumn(p, stack, 0, 0, {3, 3}).empty());
    EXPECT_TRUE(addColumn(p, stack, NAN, 0, {0, 3}).empty());
    EXPECT_TRUE(addColumn(p, stack, 0, 0, {0, INFINITY}).empty());
    EXPECT_TRUE(p.structure.nodes.empty());
    EXPECT_FALSE(stack.canUndo());
}

TEST(BarEdit, ColumnSkipsExistingBarsAndIdsNeverReused) {
    Project p;
    UndoStack stack;
    ASSERT_EQ(addColumn(p, stack, 0, 0, {0, 3}).size(), 1u);
    EXPECT_TRUE(addColumn(p, stack, 0, 0, {0, 3}).empty());  // nothing new
    const auto ids = addColumn(p, stack, 0, 0, {0, 3, 6});
    ASSERT_EQ(ids.size(), 1u);  // only 3-6 is new
    EXPECT_EQ(p.structure.nodes.size(), 3u);
    stack.undo();
    const auto again = addColumn(p, stack, 0, 0, {0, 3, 6});
    ASSERT_EQ(again.size(), 1u);
    EXPECT_NE(again[0], ids[0]);
}

TEST(BarEdit, DeleteBarsKeepsNodesAndUndoes) {
    Project p;
    UndoStack stack;
    const int a = node(p, 0, 0, 0), b = node(p, 4, 0, 0), c = node(p, 8, 0, 0);
    const auto ids = addBeamChain(p, stack, {a, b, c}, false);
    ASSERT_EQ(ids.size(), 2u);
    EXPECT_TRUE(deleteBars(p, stack, {ids[0], ids[0], 999}));
    EXPECT_EQ(p.structure.bars.size(), 1u);
    EXPECT_EQ(p.structure.nodes.size(), 3u);
    stack.undo();
    ASSERT_EQ(p.structure.bars.size(), 2u);
    EXPECT_EQ(p.structure.bars[0].id, ids[0]);
    stack.redo();
    EXPECT_EQ(p.structure.bars.size(), 1u);
    EXPECT_FALSE(deleteBars(p, stack, {}));
    EXPECT_FALSE(deleteBars(p, stack, {999}));
    EXPECT_TRUE(deleteBars(p, stack, {ids[1]}));
    EXPECT_TRUE(p.structure.bars.empty());
    EXPECT_TRUE(p.structure.validate().empty());
}

TEST(BarEdit, DeletedBarCanBeDrawnAgainWithNewId) {
    Project p;
    UndoStack stack;
    const int a = node(p, 0, 0, 0), b = node(p, 4, 0, 0);
    const auto first = addBar(p, stack, a, b);
    ASSERT_TRUE(deleteBars(p, stack, first));
    const auto second = addBar(p, stack, a, b);
    ASSERT_EQ(second.size(), 1u);
    EXPECT_NE(first[0], second[0]);
}
