#include <gtest/gtest.h>

#include "core/bar_edit.h"
#include "core/piece_edit.h"
#include "core/project_json.h"

using namespace cc::core;

namespace {

int node(Project& p, double x, double y = 0, double z = 0) { return p.structure.addNode({x, y, z}); }

/// Nodes at x = 0, 2, 4, 6 on the x axis and bars 0-1, 1-2, 2-3 (ids returned).
struct Row {
    Project p;
    UndoStack stack;
    int n[4] = {};
    int b[3] = {};
    Row() {
        for (int i = 0; i < 4; ++i) n[i] = node(p, 2.0 * i);
        for (int i = 0; i < 3; ++i) b[i] = p.structure.addBar(n[i], n[i + 1]);
    }
};

}  // namespace

TEST(PieceEdit, FindsAlignedChainInEitherDirectionOfBars) {
    Row r;
    EXPECT_EQ(findAlignedChain(r.p.structure, r.n[0], r.n[3]), (std::vector<int>{r.b[0], r.b[1], r.b[2]}));
    EXPECT_EQ(findAlignedChain(r.p.structure, r.n[1], r.n[3]), (std::vector<int>{r.b[1], r.b[2]}));
    EXPECT_EQ(findAlignedChain(r.p.structure, r.n[3], r.n[0]), (std::vector<int>{r.b[2], r.b[1], r.b[0]}));
    EXPECT_TRUE(findAlignedChain(r.p.structure, r.n[0], r.n[0]).empty());
    EXPECT_TRUE(findAlignedChain(r.p.structure, r.n[0], 999).empty());
}

TEST(PieceEdit, CreatePieceFromThreeBarsUndoRedo) {
    Row r;
    int id = 0;
    ASSERT_EQ(createPiece(r.p, r.stack, r.n[0], r.n[3], &id), PieceError::None);
    ASSERT_NE(id, 0);
    const Piece* piece = r.p.structure.findPiece(id);
    ASSERT_NE(piece, nullptr);
    EXPECT_EQ(piece->barIds, (std::vector<int>{r.b[0], r.b[1], r.b[2]}));
    EXPECT_EQ(piece->nodeIds, (std::vector<int>{r.n[0], r.n[1], r.n[2], r.n[3]}));
    EXPECT_EQ(piece->type, ElementType::Generic);
    EXPECT_EQ(r.p.structure.pieceOfBar(r.b[1])->id, id);
    EXPECT_TRUE(r.p.structure.validate().empty());
    r.stack.undo();
    EXPECT_TRUE(r.p.structure.pieces.empty());
    r.stack.redo();
    ASSERT_EQ(r.p.structure.pieces.size(), 1u);
    EXPECT_EQ(r.p.structure.pieces[0].id, id);
}

TEST(PieceEdit, CreatePieceFromNodesInReverseOrderStartsAtFirstNode) {
    Row r;
    int id = 0;
    ASSERT_EQ(createPiece(r.p, r.stack, r.n[3], r.n[1], &id), PieceError::None);
    const Piece* piece = r.p.structure.findPiece(id);
    EXPECT_EQ(piece->nodeIds, (std::vector<int>{r.n[3], r.n[2], r.n[1]}));
    EXPECT_EQ(piece->barIds, (std::vector<int>{r.b[2], r.b[1]}));
}

TEST(PieceEdit, ErrorEquivalentPieceAlreadyExists) {
    Row r;
    ASSERT_EQ(createPiece(r.p, r.stack, r.n[0], r.n[2]), PieceError::None);
    const std::size_t depth = r.stack.count();
    EXPECT_EQ(createPiece(r.p, r.stack, r.n[0], r.n[2]), PieceError::EquivalentExists);
    EXPECT_EQ(r.stack.count(), depth);  // nothing pushed
    // a lone bar is already an implicit piece
    EXPECT_EQ(createPiece(r.p, r.stack, r.n[2], r.n[3]), PieceError::EquivalentExists);
    EXPECT_EQ(pieceErrorMessage(PieceError::EquivalentExists), "Ya existe una pieza equivalente.");
}

TEST(PieceEdit, ErrorNoAlignedGroup) {
    Row r;
    const int off = node(r.p, 6, 3, 0);
    r.p.structure.addBar(r.n[3], off);  // bar leaves the line
    UndoStack& s = r.stack;
    EXPECT_EQ(createPiece(r.p, s, r.n[0], off), PieceError::NoAlignedGroup);  // chain bends at n3
    const int lone = node(r.p, 20);
    EXPECT_EQ(createPiece(r.p, s, r.n[0], lone), PieceError::NoAlignedGroup);  // no bars reach it
    EXPECT_EQ(createPiece(r.p, s, r.n[0], r.n[0]), PieceError::NoAlignedGroup);
    EXPECT_EQ(pieceErrorMessage(PieceError::NoAlignedGroup),
              "No se ha encontrado un grupo de barras alineadas que conecte los dos nudos seleccionados.");
    EXPECT_EQ(s.count(), 0u);
}

TEST(PieceEdit, ErrorPartialOverlapWithExistingPiece) {
    Row r;
    ASSERT_EQ(createPiece(r.p, r.stack, r.n[0], r.n[2]), PieceError::None);
    EXPECT_EQ(createPiece(r.p, r.stack, r.n[1], r.n[3]), PieceError::NoAlignedGroup);  // provisional rule
}

TEST(PieceEdit, ErrorDifferentArrangement) {
    Row r;
    r.p.structure.findBar(r.b[1])->rollAngleDeg = 90.0;
    EXPECT_EQ(createPiece(r.p, r.stack, r.n[0], r.n[2]), PieceError::DifferentArrangement);
    EXPECT_EQ(pieceErrorMessage(PieceError::DifferentArrangement),
              "Las barras seleccionadas no tienen la misma disposición (giro y enrase).");
    r.p.structure.findBar(r.b[1])->rollAngleDeg = 0.0;
    r.p.structure.findBar(r.b[2])->xAxisInverted = true;
    EXPECT_EQ(createPiece(r.p, r.stack, r.n[1], r.n[3]), PieceError::DifferentArrangement);
    EXPECT_EQ(createPiece(r.p, r.stack, r.n[0], r.n[2]), PieceError::None);
}

TEST(PieceEdit, BarStoredBackwardsDiffersInDirectionUnlessInverted) {
    Row r;
    Bar* second = r.p.structure.findBar(r.b[1]);
    std::swap(second->startNode, second->endNode);  // stored n2 -> n1
    EXPECT_EQ(createPiece(r.p, r.stack, r.n[0], r.n[2]), PieceError::DifferentArrangement);
    second->xAxisInverted = true;  // flag compensates, so both run the same way along the piece
    EXPECT_EQ(createPiece(r.p, r.stack, r.n[0], r.n[2]), PieceError::None);
}

TEST(PieceEdit, CreatePieceAbsorbsInnerPiecesAndKeepsMark) {
    Row r;
    ASSERT_EQ(setPieceType(r.p, r.stack, {r.b[0]}, ElementType::Beam), PieceError::None);
    ASSERT_EQ(createPiece(r.p, r.stack, r.n[1], r.n[3]), PieceError::None);
    int id = 0;
    ASSERT_EQ(createPiece(r.p, r.stack, r.n[0], r.n[3], &id), PieceError::None);
    EXPECT_EQ(r.p.structure.pieces.size(), 1u);
    EXPECT_EQ(r.p.structure.findPiece(id)->type, ElementType::Beam);
    EXPECT_TRUE(r.p.structure.validate().empty());
    r.stack.undo();
    EXPECT_EQ(r.p.structure.pieces.size(), 2u);
}

TEST(PieceEdit, DeletingABarSplitsItsPieceAndUndoRestores) {
    Row r;
    ASSERT_EQ(createPiece(r.p, r.stack, r.n[0], r.n[3]), PieceError::None);
    const std::vector<Piece> before = r.p.structure.pieces;
    ASSERT_TRUE(deleteBars(r.p, r.stack, {r.b[1]}));
    EXPECT_TRUE(r.p.structure.validate().empty());
    EXPECT_EQ(r.p.structure.pieces.size(), 0u);  // two lone bars: implicit pieces
    r.stack.undo();
    EXPECT_EQ(r.p.structure.pieces, before);
    ASSERT_TRUE(deleteBars(r.p, r.stack, {r.b[2]}));
    ASSERT_EQ(r.p.structure.pieces.size(), 1u);
    EXPECT_EQ(r.p.structure.pieces[0].barIds, (std::vector<int>{r.b[0], r.b[1]}));
    EXPECT_EQ(r.p.structure.pieces[0].nodeIds, (std::vector<int>{r.n[0], r.n[1], r.n[2]}));
}

TEST(PieceEdit, ElementTypeMarksFromColumnBeamAndSplitBar) {
    Project p;
    UndoStack stack;
    const auto column = addColumn(p, stack, 0, 0, {0, 3, 6});
    ASSERT_EQ(column.size(), 2u);
    for (int id : column) {
        ASSERT_NE(p.structure.pieceOfBar(id), nullptr);
        EXPECT_EQ(p.structure.pieceOfBar(id)->type, ElementType::Pillar);
    }
    const int a = node(p, 1, 0, 3), b = node(p, 5, 0, 3);
    const auto beam = addBeamChain(p, stack, {a, b});
    ASSERT_EQ(beam.size(), 1u);
    EXPECT_EQ(p.structure.pieceOfBar(beam[0])->type, ElementType::Beam);
    EXPECT_TRUE(p.structure.validate().empty());
    stack.undo();
    stack.undo();
    EXPECT_TRUE(p.structure.pieces.empty());
    stack.redo();
    EXPECT_EQ(p.structure.pieces.size(), 2u);
}

TEST(PieceEdit, BarSplitByCutNodesFormsOnePiece) {
    Project p;
    UndoStack stack;
    const int a = node(p, 0), m = node(p, 3), b = node(p, 6);
    (void)m;
    const auto ids = addBar(p, stack, a, b);
    ASSERT_EQ(ids.size(), 2u);
    ASSERT_EQ(p.structure.pieces.size(), 1u);
    EXPECT_EQ(p.structure.pieces[0].barIds, ids);
    EXPECT_EQ(p.structure.pieces[0].type, ElementType::Generic);
    EXPECT_TRUE(p.structure.validate().empty());
    stack.undo();
    EXPECT_TRUE(p.structure.pieces.empty());
}

TEST(PieceEdit, SetPieceTypeAndNothingToChange) {
    Row r;
    ASSERT_EQ(setPieceType(r.p, r.stack, {r.b[0]}, ElementType::Pillar), PieceError::None);
    EXPECT_EQ(r.p.structure.pieceOfBar(r.b[0])->type, ElementType::Pillar);
    EXPECT_EQ(setPieceType(r.p, r.stack, {r.b[0]}, ElementType::Pillar), PieceError::NothingToChange);
    EXPECT_EQ(setPieceType(r.p, r.stack, {999}, ElementType::Beam), PieceError::UnknownElement);
    ASSERT_EQ(setPieceType(r.p, r.stack, {r.b[0]}, ElementType::Generic), PieceError::None);
    EXPECT_TRUE(r.p.structure.pieces.empty());  // back to the implicit piece
    r.stack.undo();
    EXPECT_EQ(r.p.structure.pieceOfBar(r.b[0])->type, ElementType::Pillar);
}

TEST(PieceEdit, GroupSharesProfileAndMaterialOfFirstPiece) {
    Row r;
    Material m1;
    m1.name = "A";
    Material m2;
    m2.name = "B";
    CrossSection s1;
    s1.name = "S1";
    s1.areaM2 = 0.01;
    CrossSection s2 = s1;
    s2.name = "S2";
    const int mat1 = r.p.structure.addMaterial(m1), mat2 = r.p.structure.addMaterial(m2);
    const int sec1 = r.p.structure.addSection(s1), sec2 = r.p.structure.addSection(s2);
    r.p.structure.findBar(r.b[0])->materialId = mat1;
    r.p.structure.findBar(r.b[0])->sectionId = sec1;
    r.p.structure.findBar(r.b[2])->materialId = mat2;
    r.p.structure.findBar(r.b[2])->sectionId = sec2;
    int group = 0;
    ASSERT_EQ(groupPieces(r.p, r.stack, {r.b[2], r.b[0]}, &group), PieceError::None);  // first selected: bar 2
    ASSERT_NE(group, 0);
    EXPECT_EQ(r.p.structure.findBar(r.b[0])->materialId, mat2);  // provisional: first piece prevails
    EXPECT_EQ(r.p.structure.findBar(r.b[0])->sectionId, sec2);
    EXPECT_EQ(r.p.structure.pieceOfBar(r.b[0])->groupId, group);
    EXPECT_EQ(r.p.structure.pieceOfBar(r.b[2])->groupId, group);
    EXPECT_EQ(r.p.structure.pieceOfBar(r.b[1]), nullptr);
    EXPECT_TRUE(r.p.structure.validate().empty());
    r.stack.undo();
    EXPECT_EQ(r.p.structure.findBar(r.b[0])->materialId, mat1);
    EXPECT_EQ(r.p.structure.findBar(r.b[0])->sectionId, sec1);
    EXPECT_TRUE(r.p.structure.pieces.empty());
    r.stack.redo();
    EXPECT_EQ(r.p.structure.findBar(r.b[0])->sectionId, sec2);
}

TEST(PieceEdit, GroupErrors) {
    Row r;
    EXPECT_EQ(groupPieces(r.p, r.stack, {r.b[0]}), PieceError::TooFewPieces);
    EXPECT_EQ(groupPieces(r.p, r.stack, {r.b[0], r.b[0]}), PieceError::TooFewPieces);
    EXPECT_EQ(groupPieces(r.p, r.stack, {r.b[0], 999}), PieceError::UnknownElement);
    ASSERT_EQ(createPiece(r.p, r.stack, r.n[0], r.n[2]), PieceError::None);
    EXPECT_EQ(groupPieces(r.p, r.stack, {r.b[0], r.b[1]}), PieceError::TooFewPieces);  // same piece
    ASSERT_EQ(groupPieces(r.p, r.stack, {r.b[0], r.b[2]}), PieceError::None);
    EXPECT_EQ(groupPieces(r.p, r.stack, {r.b[0], r.b[2]}), PieceError::NothingToChange);
}

TEST(PieceEdit, GroupingDragsTheWholeGroupIn) {
    Project p;
    UndoStack stack;
    int bars[4] = {};
    for (int i = 0; i < 4; ++i) bars[i] = p.structure.addBar(node(p, 0, i * 3.0), node(p, 4, i * 3.0));
    ASSERT_EQ(groupPieces(p, stack, {bars[0], bars[1]}), PieceError::None);
    int group = 0;
    ASSERT_EQ(groupPieces(p, stack, {bars[1], bars[2]}, &group), PieceError::None);
    for (int i = 0; i < 3; ++i) EXPECT_EQ(p.structure.pieceOfBar(bars[i])->groupId, group);
    EXPECT_EQ(p.structure.pieceOfBar(bars[3]), nullptr);
}

TEST(PieceEdit, UngroupLeavesRestAndDissolvesSingleton) {
    Project p;
    UndoStack stack;
    int bars[3] = {};
    for (int i = 0; i < 3; ++i) bars[i] = p.structure.addBar(node(p, 0, i * 3.0), node(p, 4, i * 3.0));
    ASSERT_EQ(groupPieces(p, stack, {bars[0], bars[1], bars[2]}), PieceError::None);
    ASSERT_EQ(ungroupPieces(p, stack, {bars[0]}), PieceError::None);
    EXPECT_EQ(p.structure.pieceOfBar(bars[0]), nullptr);  // independent again
    EXPECT_NE(p.structure.pieceOfBar(bars[1]), nullptr);
    ASSERT_EQ(ungroupPieces(p, stack, {bars[1]}), PieceError::None);
    EXPECT_TRUE(p.structure.pieces.empty());  // the remaining piece alone is no group
    stack.undo();
    EXPECT_NE(p.structure.pieceOfBar(bars[2]), nullptr);
    EXPECT_EQ(ungroupPieces(p, stack, {999}), PieceError::UnknownElement);
    EXPECT_EQ(ungroupPieces(p, stack, {}), PieceError::UnknownElement);
}

TEST(PieceEdit, UngroupNothingToChange) {
    Row r;
    EXPECT_EQ(ungroupPieces(r.p, r.stack, {r.b[0]}), PieceError::NothingToChange);
}

TEST(PieceEdit, MoveBarEnd) {
    Row r;
    const int spare = node(r.p, 5, 1, 0);
    ASSERT_EQ(moveBarEnd(r.p, r.stack, r.b[2], BarEnd::End, spare), PieceError::None);
    EXPECT_EQ(r.p.structure.findBar(r.b[2])->endNode, spare);
    EXPECT_EQ(r.p.structure.findBar(r.b[2])->startNode, r.n[2]);
    EXPECT_TRUE(r.p.structure.validate().empty());
    r.stack.undo();
    EXPECT_EQ(r.p.structure.findBar(r.b[2])->endNode, r.n[3]);
    r.stack.redo();
    EXPECT_EQ(r.p.structure.findBar(r.b[2])->endNode, spare);
}

TEST(PieceEdit, MoveBarEndErrors) {
    Row r;
    const int spare = node(r.p, 5, 1, 0);
    EXPECT_EQ(moveBarEnd(r.p, r.stack, 999, BarEnd::End, spare), PieceError::UnknownElement);
    EXPECT_EQ(moveBarEnd(r.p, r.stack, r.b[0], BarEnd::End, 999), PieceError::UnknownElement);
    EXPECT_EQ(moveBarEnd(r.p, r.stack, r.b[0], BarEnd::End, r.n[1]), PieceError::InvalidMove);  // same node
    EXPECT_EQ(moveBarEnd(r.p, r.stack, r.b[0], BarEnd::End, r.n[0]), PieceError::InvalidMove);  // other end
    EXPECT_EQ(moveBarEnd(r.p, r.stack, r.b[0], BarEnd::Start, r.n[2]), PieceError::InvalidMove);  // bar 2-1 exists
    const int twin = node(r.p, 2, 0, 0);  // coincident with n1 -> zero length from n1? start moved onto n1's twin
    (void)twin;
    const int onFixed = node(r.p, 0, 0, 0);  // coincident with the fixed end
    EXPECT_EQ(moveBarEnd(r.p, r.stack, r.b[0], BarEnd::End, onFixed), PieceError::InvalidMove);  // too short
    EXPECT_EQ(r.stack.count(), 0u);
}

TEST(PieceEdit, MoveBarEndSplitsMultiBarPiece) {
    Row r;
    ASSERT_EQ(createPiece(r.p, r.stack, r.n[0], r.n[3]), PieceError::None);
    ASSERT_EQ(setPieceType(r.p, r.stack, {r.b[0]}, ElementType::Beam), PieceError::None);
    const int spare = node(r.p, 3, 2, 0);
    ASSERT_EQ(moveBarEnd(r.p, r.stack, r.b[1], BarEnd::End, spare), PieceError::None);
    EXPECT_TRUE(r.p.structure.validate().empty());
    for (int b : r.b) {
        ASSERT_NE(r.p.structure.pieceOfBar(b), nullptr);  // beam mark kept: records remain
        EXPECT_EQ(r.p.structure.pieceOfBar(b)->barIds.size(), 1u);
        EXPECT_EQ(r.p.structure.pieceOfBar(b)->type, ElementType::Beam);
    }
    r.stack.undo();
    EXPECT_EQ(r.p.structure.pieces.size(), 1u);
    EXPECT_EQ(r.p.structure.pieces[0].barIds.size(), 3u);
}

TEST(PieceEdit, InvertAxisSwapsNodesAndUndoes) {
    Row r;
    ASSERT_EQ(invertBarAxis(r.p, r.stack, r.b[1]), PieceError::None);
    EXPECT_EQ(r.p.structure.findBar(r.b[1])->startNode, r.n[2]);
    EXPECT_EQ(r.p.structure.findBar(r.b[1])->endNode, r.n[1]);
    EXPECT_TRUE(r.p.structure.validate().empty());
    ASSERT_EQ(invertBarAxis(r.p, r.stack, r.b[1]), PieceError::None);  // twice = original
    EXPECT_EQ(r.p.structure.findBar(r.b[1])->startNode, r.n[1]);
    r.stack.undo();
    EXPECT_EQ(r.p.structure.findBar(r.b[1])->startNode, r.n[2]);
    EXPECT_EQ(invertBarAxis(r.p, r.stack, 999), PieceError::UnknownElement);
}

TEST(PieceEdit, InvertAxisOfMultiBarPieceInvertsWholePiece) {
    Row r;
    int id = 0;
    ASSERT_EQ(createPiece(r.p, r.stack, r.n[0], r.n[3], &id), PieceError::None);
    ASSERT_EQ(invertBarAxis(r.p, r.stack, r.b[1]), PieceError::None);
    const Piece* piece = r.p.structure.findPiece(id);
    EXPECT_EQ(piece->nodeIds, (std::vector<int>{r.n[3], r.n[2], r.n[1], r.n[0]}));
    EXPECT_EQ(piece->barIds, (std::vector<int>{r.b[2], r.b[1], r.b[0]}));
    for (int b : r.b) EXPECT_GT(r.p.structure.findBar(b)->startNode, r.p.structure.findBar(b)->endNode);
    EXPECT_TRUE(r.p.structure.validate().empty());
    r.stack.undo();
    EXPECT_EQ(r.p.structure.findPiece(id)->nodeIds.front(), r.n[0]);
    EXPECT_LT(r.p.structure.findBar(r.b[0])->startNode, r.p.structure.findBar(r.b[0])->endNode);
}

TEST(PieceEdit, PersistsPiecesInJsonAndMissingSectionIsEmpty) {
    Row r;
    ASSERT_EQ(createPiece(r.p, r.stack, r.n[0], r.n[2]), PieceError::None);
    ASSERT_EQ(setPieceType(r.p, r.stack, {r.b[2]}, ElementType::Pillar), PieceError::None);
    ASSERT_EQ(groupPieces(r.p, r.stack, {r.b[0], r.b[2]}), PieceError::None);
    const std::string text = project_json::dumpSection(r.p, "pieces");
    Project loaded = r.p;
    loaded.structure.pieces.clear();
    loaded.structure.nextPieceId = 1;
    project_json::loadSection(loaded, "pieces", text);
    EXPECT_EQ(loaded.structure.pieces, r.p.structure.pieces);
    EXPECT_EQ(loaded.structure.nextPieceId, r.p.structure.nextPieceId);
    EXPECT_TRUE(loaded.structure.validate().empty());
    EXPECT_TRUE(project_json::isKnownSection("pieces"));
    EXPECT_TRUE(Project{}.structure.pieces.empty());  // a file without the section loads as no pieces
}

TEST(PieceEdit, ValidateRejectsInconsistentPieces) {
    Row r;
    ASSERT_EQ(createPiece(r.p, r.stack, r.n[0], r.n[2]), PieceError::None);
    Project bad = r.p;
    bad.structure.pieces[0].barIds[0] = 999;
    EXPECT_FALSE(bad.structure.validate().empty());
    bad = r.p;
    bad.structure.pieces[0].nodeIds[1] = r.n[3];
    EXPECT_FALSE(bad.structure.validate().empty());
    bad = r.p;
    Piece twin = bad.structure.pieces[0];
    twin.id = bad.structure.nextPieceId;
    bad.structure.insertPiece(twin);
    EXPECT_FALSE(bad.structure.validate().empty());  // a bar in two pieces
}
