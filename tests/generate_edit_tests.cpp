// Geometria > Herramientas > Generar (T-P5-10, core): planes, node meshes and the tetrahedra mesh.
#include <gtest/gtest.h>

#include <cmath>

#include "core/generate_edit.h"

using namespace cc::core;

namespace {

/// Portal frame in the xz plane (y = 0): N1 (0,0,0), N2 (0,0,3), N3 (4,0,3), N4 (4,0,0); bars B1 N1-N2, B2 N2-N3, B3 N3-N4.
struct Portal {
    Project p;
    UndoStack stack;
    Portal() {
        auto& m = p.structure;
        m.addNode({0, 0, 0});
        m.addNode({0, 0, 3});
        m.addNode({4, 0, 3});
        m.addNode({4, 0, 0});
        m.addBar(1, 2);
        m.addBar(2, 3);
        m.addBar(3, 4);
    }
};

/// Origin and three segments: N1 (0,0,0), N2 (4,0,0), N3 (0,3,0), N4 (0,0,2).
struct Frame {
    Project p;
    UndoStack stack;
    Frame() {
        auto& m = p.structure;
        m.addNode({0, 0, 0});
        m.addNode({4, 0, 0});
        m.addNode({0, 3, 0});
        m.addNode({0, 0, 2});
    }
};

bool hasNodeAt(const StructuralModel& m, double x, double y, double z) {
    for (const Node& n : m.nodes)
        if (std::fabs(n.position.x - x) < 1e-9 && std::fabs(n.position.y - y) < 1e-9 && std::fabs(n.position.z - z) < 1e-9) return true;
    return false;
}

}  // namespace

TEST(GeneratePlanes, PlaneOfTwoBarsIsPerpendicularToAGlobalAxis) {
    Portal f;
    GenerateError e = GenerateError::None;
    const auto plane = planeOfBars(f.p.structure, 1, 2, &e);  // column and lintel: the plane y = 0
    ASSERT_TRUE(plane);
    EXPECT_EQ(plane->axis, 1);
    EXPECT_NEAR(plane->coordinate, 0.0, 1e-12);
    EXPECT_FALSE(planeOfBars(f.p.structure, 1, 3, &e));  // two parallel columns
    EXPECT_EQ(e, GenerateError::LinesParallel);
    const int n5 = f.p.structure.addNode({1, 1, 0});
    const int n6 = f.p.structure.addNode({2, 2, 1});
    f.p.structure.addBar(1, n5);  // the plane of this bar and the first column is oblique
    f.p.structure.addBar(n5, n6);
    EXPECT_FALSE(planeOfBars(f.p.structure, 4, 5, &e));
    EXPECT_EQ(e, GenerateError::PlaneNotOrthogonal);
    EXPECT_FALSE(planeOfBars(f.p.structure, 1, 99, &e));
    EXPECT_EQ(e, GenerateError::UnknownElement);
}

TEST(GeneratePlanes, TwoPlanesOneMetreApartCopyThePlaneTwice) {
    Portal f;
    ASSERT_EQ(generatePlanes(f.p, f.stack, PlaneSpec{1, 2, 2, 1.0, false}), GenerateError::None);
    const StructuralModel& m = f.p.structure;
    EXPECT_EQ(m.nodes.size(), 12u);
    EXPECT_EQ(m.bars.size(), 9u);
    EXPECT_TRUE(hasNodeAt(m, 0, 1, 0));
    EXPECT_TRUE(hasNodeAt(m, 4, 2, 3));
    EXPECT_TRUE(m.validate().empty());
    EXPECT_EQ(f.stack.count(), 1u) << "one undo step";
    f.stack.undo();
    EXPECT_EQ(f.p.structure.nodes.size(), 4u);
    f.stack.redo();
    EXPECT_EQ(f.p.structure.nodes.size(), 12u);
}

TEST(GeneratePlanes, NegativeSeparationGoesTheOtherWay) {
    Portal f;
    ASSERT_EQ(generatePlanes(f.p, f.stack, PlaneSpec{1, 2, 1, -2.5, false}), GenerateError::None);
    EXPECT_TRUE(hasNodeAt(f.p.structure, 0, -2.5, 3));
}

TEST(GeneratePlanes, OnlyTheElementsOfThePlaneAreCopied) {
    Portal f;
    const int off = f.p.structure.addNode({0, 5, 0});  // not on the plane y = 0
    f.p.structure.addBar(1, off);
    ASSERT_EQ(generatePlanes(f.p, f.stack, PlaneSpec{1, 2, 1, 1.0, false}), GenerateError::None);
    EXPECT_EQ(f.p.structure.nodes.size(), 4u + 1u + 4u);
    EXPECT_FALSE(hasNodeAt(f.p.structure, 0, 6, 0));
}

TEST(GeneratePlanes, RangesAndZeroSeparationAreRefused) {
    Portal f;
    EXPECT_EQ(generatePlanes(f.p, f.stack, PlaneSpec{1, 2, 0, 1.0, false}), GenerateError::CountOutOfRange);
    EXPECT_EQ(generatePlanes(f.p, f.stack, PlaneSpec{1, 2, 101, 1.0, false}), GenerateError::CountOutOfRange);
    EXPECT_EQ(generatePlanes(f.p, f.stack, PlaneSpec{1, 2, 1, 10000.5, false}), GenerateError::SeparationOutOfRange);
    EXPECT_EQ(generatePlanes(f.p, f.stack, PlaneSpec{1, 2, 1, -10001.0, false}), GenerateError::SeparationOutOfRange);
    // The original hangs / crashes here: the clone refuses it with a clear message.
    EXPECT_EQ(generatePlanes(f.p, f.stack, PlaneSpec{1, 2, 1, 0.0, false}), GenerateError::ZeroSeparation);
    EXPECT_NE(generateErrorMessage(GenerateError::ZeroSeparation).find("cero"), std::string::npos);
    EXPECT_EQ(generateErrorMessage(GenerateError::CountOutOfRange), "El valor de 'Nº de planos a generar' debe estar entre 1 y 100.");
    EXPECT_EQ(f.stack.count(), 0u);
    EXPECT_EQ(f.p.structure.nodes.size(), 4u);
    EXPECT_EQ(generatePlanes(f.p, f.stack, PlaneSpec{1, 1, 1, 1.0, false}), GenerateError::LinesParallel);
}

TEST(GeneratePlanes, GroupingMakesOneGroupOfAllThePlanesAndUndoRestoresIt) {
    Portal f;
    ASSERT_EQ(generatePlanes(f.p, f.stack, PlaneSpec{1, 2, 2, 1.0, true}), GenerateError::None);
    ASSERT_EQ(f.p.workPlanes.groups.y.size(), 1u);
    EXPECT_EQ(f.p.workPlanes.groups.y[0], (std::vector<double>{0.0, 1.0, 2.0}));
    EXPECT_TRUE(f.p.workPlanes.isValid());
    EXPECT_EQ(f.stack.count(), 1u) << "copy and grouping are one undo step";
    f.stack.undo();
    EXPECT_TRUE(f.p.workPlanes.groups.y.empty());
    EXPECT_EQ(f.p.structure.nodes.size(), 4u);
}

TEST(GeneratePlanes, AGroupOfAnInvolvedPlaneIsDissolved) {
    Portal f;
    f.p.workPlanes.groups.y = {{0.0, 7.0}};
    PlaneSpec spec{1, 2, 1, 1.0, true};
    EXPECT_TRUE(generationUngroups(f.p, spec));
    ASSERT_EQ(generatePlanes(f.p, f.stack, spec), GenerateError::None);
    ASSERT_EQ(f.p.workPlanes.groups.y.size(), 1u);
    EXPECT_EQ(f.p.workPlanes.groups.y[0], (std::vector<double>{0.0, 1.0}));
    EXPECT_TRUE(f.p.workPlanes.isValid());
}

TEST(GenerateNodeMesh, OneSegmentDividesTheBarIntoEqualParts) {
    Portal f;
    NodeMeshSpec spec;
    spec.nodeIds = {2, 3};  // the lintel, 4 m
    spec.divisions = {3, 1, 1};
    ASSERT_EQ(generateNodeMesh(f.p, f.stack, spec), GenerateError::None);
    const StructuralModel& m = f.p.structure;
    EXPECT_EQ(m.nodes.size(), 6u) << "two intermediate nodes";
    EXPECT_EQ(m.bars.size(), 5u) << "the lintel became three bars";
    EXPECT_TRUE(hasNodeAt(m, 4.0 / 3.0, 0, 3));
    EXPECT_TRUE(hasNodeAt(m, 8.0 / 3.0, 0, 3));
    EXPECT_EQ(m.barLength(4) > 0.0 ? 1 : 0, 1);
    EXPECT_TRUE(m.validate().empty());
    f.stack.undo();
    EXPECT_EQ(f.p.structure.bars.size(), 3u);
    EXPECT_EQ(f.p.structure.nodes.size(), 4u);
}

TEST(GenerateNodeMesh, OneSegmentKeepsTheSectionOfTheDividedBarAndWorksWithoutABar) {
    Portal f;
    const int mat = f.p.structure.addMaterial(Material{});
    f.p.structure.findBar(2)->materialId = mat;
    f.p.structure.findBar(2)->rollAngleDeg = 30.0;
    NodeMeshSpec spec;
    spec.nodeIds = {2, 3};
    spec.divisions = {2, 1, 1};
    ASSERT_EQ(generateNodeMesh(f.p, f.stack, spec), GenerateError::None);
    int withMaterial = 0;
    for (const Bar& b : f.p.structure.bars)
        if (b.materialId == mat && b.rollAngleDeg == 30.0) ++withMaterial;
    EXPECT_EQ(withMaterial, 2);
    // Nodes 1 and 4 have no bar between them: only the nodes are created.
    NodeMeshSpec free;
    free.nodeIds = {1, 4};
    free.divisions = {4, 1, 1};
    const std::size_t bars = f.p.structure.bars.size();
    ASSERT_EQ(generateNodeMesh(f.p, f.stack, free), GenerateError::None);
    EXPECT_EQ(f.p.structure.bars.size(), bars);
    EXPECT_TRUE(hasNodeAt(f.p.structure, 1.0, 0, 0));
    // One division changes nothing.
    NodeMeshSpec none;
    none.nodeIds = {1, 4};
    EXPECT_EQ(generateNodeMesh(f.p, f.stack, none), GenerateError::NothingGenerated);
}

TEST(GenerateNodeMesh, TwoSegmentsMakeAGridOfNodesAndBars) {
    Frame f;
    NodeMeshSpec spec;
    spec.nodeIds = {1, 2, 3};
    spec.divisions = {2, 3, 1};
    ASSERT_EQ(generateNodeMesh(f.p, f.stack, spec), GenerateError::None);
    const StructuralModel& m = f.p.structure;
    EXPECT_EQ(m.nodes.size(), 4u + 12u - 3u) << "12 grid nodes, three of them were already there";
    // 2 divisions along 1 -> 2 bars x 4 lines; 3 divisions along 2 -> 3 bars x 3 lines.
    EXPECT_EQ(m.bars.size(), 8u + 9u);
    EXPECT_TRUE(hasNodeAt(m, 2, 1, 0));
    EXPECT_TRUE(hasNodeAt(m, 4, 3, 0));
    EXPECT_TRUE(m.validate().empty());
}

TEST(GenerateNodeMesh, OnlyNodesCreatesNoBars) {
    Frame f;
    NodeMeshSpec spec;
    spec.nodeIds = {1, 2, 3};
    spec.divisions = {2, 2, 1};
    spec.onlyNodes = true;
    ASSERT_EQ(generateNodeMesh(f.p, f.stack, spec), GenerateError::None);
    EXPECT_TRUE(f.p.structure.bars.empty());
    EXPECT_EQ(f.p.structure.nodes.size(), 9u + 1u) << "the fourth node of the frame is not part of the grid";
}

TEST(GenerateNodeMesh, ThreeSegmentsMakeASpatialGrid) {
    Frame f;
    NodeMeshSpec spec;
    spec.nodeIds = {1, 2, 3, 4};
    spec.divisions = {2, 2, 2};
    ASSERT_EQ(generateNodeMesh(f.p, f.stack, spec), GenerateError::None);
    EXPECT_EQ(f.p.structure.nodes.size(), 27u);
    EXPECT_EQ(f.p.structure.bars.size(), 3u * 2u * 9u);
    EXPECT_TRUE(f.p.structure.validate().empty());
}

TEST(GenerateNodeMesh, PiecesAndGroupsFollowTheOptions) {
    Frame f;
    NodeMeshSpec spec;
    spec.nodeIds = {1, 2, 3};
    spec.divisions = {2, 1, 1};
    spec.makePieces = {true, false, false};
    spec.groupParallel = {true, false, false};
    ASSERT_EQ(generateNodeMesh(f.p, f.stack, spec), GenerateError::None);
    const StructuralModel& m = f.p.structure;
    // Two lines along segment 1, each one piece of two aligned bars, both in one group.
    ASSERT_EQ(m.pieces.size(), 2u);
    for (const Piece& piece : m.pieces) {
        EXPECT_EQ(piece.barIds.size(), 2u);
        EXPECT_EQ(piece.groupId, m.pieces.front().id);
    }
    EXPECT_TRUE(m.validate().empty());
    // Grouping without pieces: one record per bar.
    Frame g;
    NodeMeshSpec onlyGroup;
    onlyGroup.nodeIds = {1, 2, 3};
    onlyGroup.divisions = {1, 1, 1};
    onlyGroup.groupParallel = {true, false, false};
    ASSERT_EQ(generateNodeMesh(g.p, g.stack, onlyGroup), GenerateError::None);
    EXPECT_EQ(g.p.structure.pieces.size(), 2u);
    EXPECT_EQ(g.p.structure.pieces[0].groupId, g.p.structure.pieces[1].groupId);
}

TEST(GenerateNodeMesh, RefusesWhatCannotBeMeshed) {
    Frame f;
    NodeMeshSpec spec;
    spec.nodeIds = {1, 2, 3};
    spec.divisions = {0, 1, 1};
    EXPECT_EQ(generateNodeMesh(f.p, f.stack, spec), GenerateError::DivisionsOutOfRange);
    spec.divisions = {1001, 1, 1};
    EXPECT_EQ(generateNodeMesh(f.p, f.stack, spec), GenerateError::DivisionsOutOfRange);
    EXPECT_EQ(divisionsWarning(2), "El valor de 'Nº de divisiones del segmento 2' debe estar entre 1 y 1000.");
    spec.divisions = {1000, 1000, 1};
    EXPECT_EQ(generateNodeMesh(f.p, f.stack, spec), GenerateError::TooManyElements);
    // Collinear second segment, coplanar third one.
    const int far = f.p.structure.addNode({8, 0, 0});
    NodeMeshSpec collinear;
    collinear.nodeIds = {1, 2, far};
    EXPECT_EQ(generateNodeMesh(f.p, f.stack, collinear), GenerateError::DegenerateSegments);
    const int flat = f.p.structure.addNode({4, 3, 0});
    NodeMeshSpec coplanar;
    coplanar.nodeIds = {1, 2, 3, flat};
    EXPECT_EQ(generateNodeMesh(f.p, f.stack, coplanar), GenerateError::DegenerateSegments);
    NodeMeshSpec unknown;
    unknown.nodeIds = {1, 99};
    EXPECT_EQ(generateNodeMesh(f.p, f.stack, unknown), GenerateError::UnknownElement);
    EXPECT_EQ(f.stack.count(), 0u);
}

TEST(GenerateNodeMesh, IndependenceOfThePicks) {
    Frame f;
    const StructuralModel& m = f.p.structure;
    EXPECT_TRUE(segmentIsIndependent(m, {1, 2}, 3));
    const int far = f.p.structure.addNode({8, 0, 0});
    EXPECT_FALSE(segmentIsIndependent(m, {1, 2}, far)) << "collinear with segment 1";
    EXPECT_TRUE(segmentIsIndependent(m, {1, 2, 3}, 4));
    const int flat = f.p.structure.addNode({4, 3, 0});
    EXPECT_FALSE(segmentIsIndependent(m, {1, 2, 3}, flat)) << "coplanar with the first two: the original keeps the 2D dialog";
    EXPECT_FALSE(segmentIsIndependent(m, {1}, 2));
}

TEST(GenerateTetrahedra, TheMinimalMeshIsAPyramidOnTheBase) {
    Frame f;
    TetrahedraSpec spec;
    spec.nodeIds = {1, 2, 3};
    ASSERT_EQ(generateTetrahedra(f.p, f.stack, spec), GenerateError::None);
    const StructuralModel& m = f.p.structure;
    // Lower plane 2 x 2 nodes (3 already existed), one upper node above the cell centre, a distance of 1 m along s1 x s2 = +z.
    EXPECT_EQ(m.nodes.size(), 6u) << "the four nodes of the frame, plus the fourth corner and the upper node";
    EXPECT_TRUE(hasNodeAt(m, 2.0, 1.5, 1.0));
    EXPECT_TRUE(hasNodeAt(m, 4.0, 3.0, 0.0));
    EXPECT_EQ(m.bars.size(), 4u + 0u + 4u);
    EXPECT_EQ(m.pieces.size(), 4u + 4u) << "lower and diagonal bars are grouped (default on): one record per bar";
    EXPECT_TRUE(m.validate().empty());
}

TEST(GenerateTetrahedra, ScrewRuleAndNegativeDistance) {
    Frame f;
    TetrahedraSpec spec;
    spec.nodeIds = {1, 3, 2};  // s1 x s2 = (0,3,0) x (4,0,0) = -z
    spec.distanceM = 2.0;
    ASSERT_EQ(generateTetrahedra(f.p, f.stack, spec), GenerateError::None);
    EXPECT_TRUE(hasNodeAt(f.p.structure, 2.0, 1.5, -2.0));
    // Measured range 0.000 - 1000.000: a negative distance is refused, the sense comes from the order of the segments.
    Frame g;
    spec.nodeIds = {1, 2, 3};
    spec.distanceM = -2.0;
    EXPECT_EQ(generateTetrahedra(g.p, g.stack, spec), GenerateError::DistanceOutOfRange);
    spec.distanceM = 1000.0;
    EXPECT_EQ(generateTetrahedra(g.p, g.stack, spec), GenerateError::None);
    Frame h;
    spec.distanceM = 1000.001;
    EXPECT_EQ(generateTetrahedra(h.p, h.stack, spec), GenerateError::DistanceOutOfRange);
    EXPECT_EQ(generateErrorMessage(GenerateError::DistanceOutOfRange).rfind("El valor de 'Distancia entre planos' debe estar entre 0.000 y 1000.000.", 0), 0u);
}

// Measured in the original (herramientas.md section 11.2): segments of 4 m and 3 m drawn as bars, 2 x 2 divisions, default checkboxes.
TEST(GenerateNodeMesh, TheBarsOfTheSegmentsAreDividedLikeTheOriginal) {
    Frame f;
    f.p.structure.addBar(1, 2);  // segment 1
    f.p.structure.addBar(1, 3);  // segment 2
    NodeMeshSpec spec;
    spec.nodeIds = {1, 2, 3};
    spec.divisions = {2, 2, 1};
    ASSERT_EQ(generateNodeMesh(f.p, f.stack, spec), GenerateError::None);
    EXPECT_EQ(f.p.structure.nodes.size(), 9u + 1u) << "3 x 3 grid plus the unrelated fourth node of the fixture";
    EXPECT_EQ(f.p.structure.bars.size(), 12u) << "every line of the grid, the original bars divided";
    EXPECT_TRUE(f.p.structure.validate().empty());
    f.stack.undo();
    EXPECT_EQ(f.p.structure.bars.size(), 2u);
    EXPECT_EQ(f.p.structure.nodes.size(), 4u);
}

TEST(GenerateNodeMesh, OnlyNodesDoesNotDivideTheExistingBars) {
    Frame f;
    f.p.structure.addBar(1, 2);
    f.p.structure.addBar(1, 3);
    NodeMeshSpec spec;
    spec.nodeIds = {1, 2, 3};
    spec.divisions = {2, 2, 1};
    spec.onlyNodes = true;
    ASSERT_EQ(generateNodeMesh(f.p, f.stack, spec), GenerateError::None);
    EXPECT_EQ(f.p.structure.nodes.size(), 10u);
    EXPECT_EQ(f.p.structure.bars.size(), 2u) << "no new bar and the originals keep their length";
}

TEST(GenerateNodeMesh, DividedPiecesStayPiecesAndTheGridPositionsFollowTheFormula) {
    Frame f;
    const int b1 = f.p.structure.addBar(1, 2);
    f.p.structure.addBar(1, 3);
    Piece piece;
    piece.id = f.p.structure.nextPieceId;
    piece.barIds = {b1};
    piece.nodeIds = {1, 2};
    piece.type = ElementType::Beam;
    ASSERT_TRUE(f.p.structure.insertPiece(piece));
    NodeMeshSpec spec;
    spec.nodeIds = {1, 2, 3};
    spec.divisions = {2, 2, 1};
    ASSERT_EQ(generateNodeMesh(f.p, f.stack, spec), GenerateError::None);
    const Piece* kept = f.p.structure.findPiece(piece.id);
    ASSERT_NE(kept, nullptr);
    EXPECT_EQ(kept->barIds.size(), 2u) << "the original bar became two stretches of the same piece";
    EXPECT_EQ(kept->type, ElementType::Beam);
    // point(i, j) = P0 + i * s2 * U2 + j * s1 * U1 with s = L / n.
    for (int i = 0; i <= 2; ++i)
        for (int j = 0; j <= 2; ++j) EXPECT_TRUE(hasNodeAt(f.p.structure, j * 2.0, i * 1.5, 0.0));
}

// Measured: 2 x 2 divisions, 1 m, the three groupings on: 13 nodes and 32 bars (upper layer at the cell centres, 4 diagonals per cell).
TEST(GenerateTetrahedra, MeasuredCountsOf2x2Divisions) {
    Frame f;
    TetrahedraSpec spec;
    spec.nodeIds = {1, 2, 3};
    spec.divisions1 = spec.divisions2 = 2;
    ASSERT_EQ(generateTetrahedra(f.p, f.stack, spec), GenerateError::None);
    EXPECT_EQ(f.p.structure.nodes.size(), 13u + 1u) << "13 mesh nodes plus the unrelated fourth node of the fixture";
    EXPECT_EQ(f.p.structure.bars.size(), 32u);
    EXPECT_TRUE(hasNodeAt(f.p.structure, 1.0, 0.75, 1.0)) << "upper layer in the centre of the first cell, +1 m";
}

TEST(GenerateTetrahedra, DivisionsAndGroupsAndErrors) {
    Frame f;
    TetrahedraSpec spec;
    spec.nodeIds = {1, 2, 3};
    spec.divisions1 = 2;
    spec.divisions2 = 2;
    spec.groupLowerBars = spec.groupUpperBars = spec.groupDiagonals = false;
    ASSERT_EQ(generateTetrahedra(f.p, f.stack, spec), GenerateError::None);
    const StructuralModel& m = f.p.structure;
    EXPECT_EQ(m.bars.size(), 12u + 4u + 16u) << "lower grid, upper grid and four diagonals per cell";
    EXPECT_TRUE(m.pieces.empty());
    EXPECT_TRUE(m.validate().empty());
    f.stack.undo();
    EXPECT_TRUE(f.p.structure.bars.empty());

    spec.distanceM = 0.0;
    EXPECT_EQ(generateTetrahedra(f.p, f.stack, spec), GenerateError::DistanceOutOfRange);
    spec.distanceM = 1.0;
    spec.divisions1 = 0;
    EXPECT_EQ(generateTetrahedra(f.p, f.stack, spec), GenerateError::DivisionsOutOfRange);
    spec.divisions1 = 1;
    spec.nodeIds = {1, 2, 2};
    EXPECT_EQ(generateTetrahedra(f.p, f.stack, spec), GenerateError::DegenerateSegments);
    spec.nodeIds = {1, 2, 99};
    EXPECT_EQ(generateTetrahedra(f.p, f.stack, spec), GenerateError::UnknownElement);
}
