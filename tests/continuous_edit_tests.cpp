#include <gtest/gtest.h>

#include <filesystem>

#include "core/continuous_edit.h"
#include "core/piece_edit.h"
#include "core/project_file.h"
#include "core/project_json.h"

using namespace cc::core;
namespace fs = std::filesystem;

namespace {

/// Vertical stack: nodes at z = 0, 3, 6, 9 (x = y = 0), bars b[0..2]. Pieces: A = b0 + b1, B = b2, both pillars.
struct Tower {
    Project p;
    UndoStack stack;
    int n[4] = {};
    int b[3] = {};
    int pieceA = 0, pieceB = 0;
    Tower() {
        for (int i = 0; i < 4; ++i) n[i] = p.structure.addNode({0, 0, 3.0 * i});
        for (int i = 0; i < 3; ++i) b[i] = p.structure.addBar(n[i], n[i + 1]);
        EXPECT_EQ(createPiece(p, stack, n[0], n[2], &pieceA), PieceError::None);
        EXPECT_EQ(setPieceType(p, stack, {b[0]}, ElementType::Pillar), PieceError::None);
        EXPECT_EQ(setPieceType(p, stack, {b[2]}, ElementType::Pillar), PieceError::None);
        pieceB = p.structure.pieceOfBar(b[2])->id;
        stack.clear();
    }
};

/// Horizontal row: nodes at x = 0, 4, 8, 12 (z = 3), one beam piece per bar.
struct Row {
    Project p;
    UndoStack stack;
    int n[4] = {};
    int b[3] = {};
    int piece[3] = {};
    Row() {
        const int steel = p.structure.addMaterial(Material{0, "S275", MaterialKind::Steel});
        for (int i = 0; i < 4; ++i) n[i] = p.structure.addNode({4.0 * i, 0, 3});
        for (int i = 0; i < 3; ++i) {
            b[i] = p.structure.addBar(n[i], n[i + 1], steel);
            EXPECT_EQ(setPieceType(p, stack, {b[i]}, ElementType::Beam), PieceError::None);
            piece[i] = p.structure.pieceOfBar(b[i])->id;
        }
        stack.clear();
    }
};

}  // namespace

// --- Crear pilar continuo ---------------------------------------------------------------------------------------------

TEST(ContinuousColumn, CreatesFromWholePiecesWithUndoRedo) {
    Tower t;
    int id = 0;
    ASSERT_EQ(createContinuousColumn(t.p, t.stack, t.n[0], t.n[3], &id), ContinuousError::None);
    ASSERT_NE(id, 0);
    const ContinuousColumn* c = t.p.continuous.findColumn(id);
    ASSERT_NE(c, nullptr);
    EXPECT_EQ(c->pieceIds, (std::vector<int>{t.pieceA, t.pieceB}));
    EXPECT_TRUE(validateContinuous(t.p).empty());
    t.stack.undo();
    EXPECT_TRUE(t.p.continuous.empty());
    t.stack.redo();
    EXPECT_EQ(t.p.continuous.columns.size(), 1u);
    t.stack.undo();
    EXPECT_GT(t.p.continuous.nextColumnId, id) << "ids are never reused";
}

TEST(ContinuousColumn, RefusesDuplicateAndSinglePiece) {
    Tower t;
    ASSERT_EQ(createContinuousColumn(t.p, t.stack, t.n[0], t.n[3]), ContinuousError::None);
    EXPECT_EQ(createContinuousColumn(t.p, t.stack, t.n[0], t.n[3]), ContinuousError::AlreadyExists);
    EXPECT_EQ(continuousErrorMessage(ContinuousError::AlreadyExists), "Ya existe un pilar con las mismas piezas.");
    Tower u;  // one pillar piece is already a pillar
    EXPECT_EQ(createContinuousColumn(u.p, u.stack, u.n[2], u.n[3]), ContinuousError::AlreadyExists);
    EXPECT_EQ(u.stack.count(), 0u);
}

TEST(ContinuousColumn, RefusesPartialPiecesAndNonPillars) {
    Tower t;  // nodes 0 and 1 cut piece A (bars 0-1)
    EXPECT_EQ(createContinuousColumn(t.p, t.stack, t.n[0], t.n[1]), ContinuousError::IncompletePieces);
    EXPECT_EQ(continuousErrorMessage(ContinuousError::IncompletePieces), "Para crear el pilar debe seleccionar piezas completas.");
    ASSERT_EQ(setPieceType(t.p, t.stack, {t.b[2]}, ElementType::Generic), PieceError::None);
    EXPECT_EQ(createContinuousColumn(t.p, t.stack, t.n[0], t.n[3]), ContinuousError::NotPillar);
    EXPECT_EQ(continuousErrorMessage(ContinuousError::NotPillar), "Alguna de las piezas entre los nudos seleccionados no es de tipo 'pilar'.");
}

TEST(ContinuousColumn, RefusesBadGeometry) {
    Tower t;
    EXPECT_EQ(createContinuousColumn(t.p, t.stack, t.n[0], 999), ContinuousError::UnknownElement);
    EXPECT_EQ(createContinuousColumn(t.p, t.stack, t.n[0], t.n[0]), ContinuousError::UnknownElement);
    // Selected from top to bottom: the end is below the start.
    EXPECT_EQ(createContinuousColumn(t.p, t.stack, t.n[3], t.n[0]), ContinuousError::NotAscending);
    // Local x pointing down: swap the nodes of the top bar.
    ASSERT_EQ(invertBarAxis(t.p, t.stack, t.b[2]), PieceError::None);
    EXPECT_EQ(createContinuousColumn(t.p, t.stack, t.n[0], t.n[3]), ContinuousError::NotAscending);

    // Inclined and disconnected.
    Project q;
    UndoStack stack;
    const int a = q.structure.addNode({0, 0, 0}), b = q.structure.addNode({1, 0, 3}), c = q.structure.addNode({0, 0, 6});
    q.structure.addBar(a, b);
    q.structure.addBar(b, c);
    EXPECT_EQ(createContinuousColumn(q, stack, a, c), ContinuousError::NotAlignedConnected);
    const int d = q.structure.addNode({1, 0, 9});
    const int bar = q.structure.addBar(b, d);
    ASSERT_EQ(setPieceType(q, stack, {bar}, ElementType::Pillar), PieceError::None);
    EXPECT_EQ(createContinuousColumn(q, stack, b, d), ContinuousError::AlreadyExists) << "a single vertical pillar piece";
    const int e = q.structure.addNode({2, 0, 9});
    q.structure.addBar(b, e);
    EXPECT_EQ(createContinuousColumn(q, stack, b, e), ContinuousError::NotVertical);
}

// --- Crear viga continua ----------------------------------------------------------------------------------------------

TEST(ContinuousBeam, CreatesInDirectrixOrderWhateverTheSelectionOrder) {
    Row r;
    int id = 0;
    ASSERT_EQ(createContinuousBeam(r.p, r.stack, {r.b[2], r.b[0], r.b[1]}, &id), ContinuousError::None);
    EXPECT_EQ(r.p.continuous.findBeam(id)->pieceIds, (std::vector<int>{r.piece[0], r.piece[1], r.piece[2]}));
    EXPECT_TRUE(validateContinuous(r.p).empty());
    EXPECT_EQ(createContinuousBeam(r.p, r.stack, {r.b[0], r.b[1], r.b[2]}), ContinuousError::AlreadyExists);
    r.stack.undo();
    EXPECT_TRUE(r.p.continuous.empty());
    r.stack.redo();
    EXPECT_EQ(r.p.continuous.beams.size(), 1u);
}

TEST(ContinuousBeam, RefusesWrongSelections) {
    Row r;
    EXPECT_EQ(createContinuousBeam(r.p, r.stack, {}), ContinuousError::UnknownElement);
    EXPECT_EQ(createContinuousBeam(r.p, r.stack, {r.b[0]}), ContinuousError::TooFewPieces);
    EXPECT_EQ(createContinuousBeam(r.p, r.stack, {r.b[0], r.b[2]}), ContinuousError::NotConnected);
    ASSERT_EQ(setPieceType(r.p, r.stack, {r.b[1]}, ElementType::Pillar), PieceError::None);
    EXPECT_EQ(createContinuousBeam(r.p, r.stack, {r.b[0], r.b[1]}), ContinuousError::NotBeam);
    EXPECT_EQ(continuousErrorMessage(ContinuousError::NotBeam),
              "Alguna de las piezas seleccionadas no está definida como elemento estructural de tipo 'viga'.");
}

TEST(ContinuousBeam, RefusesMixedMaterialRollAndSlope) {
    {
        Row r;
        const int conc = r.p.structure.addMaterial(Material{0, "C25", MaterialKind::Concrete});
        r.p.structure.findBar(r.b[1])->materialId = conc;
        EXPECT_EQ(createContinuousBeam(r.p, r.stack, {r.b[0], r.b[1]}), ContinuousError::MixedMaterial);
    }
    {
        Row r;
        r.p.structure.findBar(r.b[1])->rollAngleDeg = 90.0;
        EXPECT_EQ(createContinuousBeam(r.p, r.stack, {r.b[0], r.b[1]}), ContinuousError::PlaneNotVertical);
        r.p.structure.findBar(r.b[1])->rollAngleDeg = 180.0;
        EXPECT_EQ(createContinuousBeam(r.p, r.stack, {r.b[0], r.b[1]}), ContinuousError::None);
    }
    {
        Row r;
        r.p.structure.findNode(r.n[2])->position.z = 3 + 40.0;  // second bar rises 40 m over 4 m: 84 degrees
        std::string detail;
        EXPECT_EQ(createContinuousBeam(r.p, r.stack, {r.b[0], r.b[1]}, nullptr, &detail), ContinuousError::TooSteep);
        EXPECT_EQ(detail, "80°");
        EXPECT_NE(continuousErrorMessage(ContinuousError::TooSteep, detail).find(detail), std::string::npos);
    }
}

// --- References and numbering -----------------------------------------------------------------------------------------

TEST(ContinuousReferences, EditPillarAndBeamReferences) {
    Tower t;
    int col = 0;
    ASSERT_EQ(createContinuousColumn(t.p, t.stack, t.n[0], t.n[3], &col), ContinuousError::None);
    const ReferenceTarget target{ReferenceTarget::Kind::ContinuousColumn, col};
    EXPECT_EQ(elementReference(t.p, target, kDefaultPillarPrefix), "P1");  // automatic
    ASSERT_EQ(setReference(t.p, t.stack, target, "Pilar A"), ContinuousError::None);
    EXPECT_EQ(elementReference(t.p, target, kDefaultPillarPrefix), "Pilar A");
    EXPECT_EQ(setReference(t.p, t.stack, target, "Pilar A"), ContinuousError::NothingToChange);
    t.stack.undo();
    EXPECT_EQ(elementReference(t.p, target, kDefaultPillarPrefix), "P1");
    t.stack.redo();
    EXPECT_EQ(setReference(t.p, t.stack, target, ""), ContinuousError::EmptyReference);  // #64971
    EXPECT_EQ(continuousErrorMessage(ContinuousError::EmptyReference), "La referencia no puede quedar vacía.");
    EXPECT_EQ(setReference(t.p, t.stack, target, std::string(31, 'a')), ContinuousError::ReferenceTooLong);
    EXPECT_EQ(setReference(t.p, t.stack, target, std::string(30, 'a')), ContinuousError::None);
    // A piece inside a continuous column is edited through the column.
    EXPECT_EQ(setReference(t.p, t.stack, {ReferenceTarget::Kind::Piece, t.pieceA}, "x"), ContinuousError::PartOfContinuous);
    EXPECT_EQ(setReference(t.p, t.stack, {ReferenceTarget::Kind::ContinuousColumn, 99}, "x"), ContinuousError::UnknownElement);

    Row r;
    const ReferenceTarget lone{ReferenceTarget::Kind::Piece, r.piece[1]};
    ASSERT_EQ(setReference(r.p, r.stack, lone, "V-7"), ContinuousError::None);
    EXPECT_EQ(elementReference(r.p, lone, kDefaultBeamPrefix), "V-7");
    EXPECT_EQ(elementReference(r.p, {ReferenceTarget::Kind::Piece, r.piece[0]}, kDefaultBeamPrefix), "V1");
    int cb = 0;
    ASSERT_EQ(createContinuousBeam(r.p, r.stack, {r.b[0], r.b[1]}, &cb), ContinuousError::None);
    const ReferenceTarget beam{ReferenceTarget::Kind::ContinuousBeam, cb};
    ASSERT_EQ(setReference(r.p, r.stack, beam, "Pórtico 2"), ContinuousError::None);
    EXPECT_EQ(elementReference(r.p, beam, kDefaultBeamPrefix), "Pórtico 2");
    // Generic pieces take no reference.
    Project q;
    UndoStack s;
    EXPECT_EQ(setReference(q, s, {ReferenceTarget::Kind::Piece, 1}, "x"), ContinuousError::UnknownElement);
}

TEST(ContinuousReferences, NumberPillarsFromAnInitialReference) {
    Tower t;
    int col = 0;
    ASSERT_EQ(createContinuousColumn(t.p, t.stack, t.n[0], t.n[3], &col), ContinuousError::None);
    // A second, lone pillar at x = 5.
    const int a = t.p.structure.addNode({5, 0, 0}), b = t.p.structure.addNode({5, 0, 3});
    const int bar = t.p.structure.addBar(a, b);
    ASSERT_EQ(setPieceType(t.p, t.stack, {bar}, ElementType::Pillar), PieceError::None);
    const int lonePiece = t.p.structure.pieceOfBar(bar)->id;
    int count = 0;
    ASSERT_EQ(numberPillars(t.p, t.stack, "Pilar 08", &count), ContinuousError::None);
    EXPECT_EQ(count, 2);
    EXPECT_EQ(elementReference(t.p, {ReferenceTarget::Kind::ContinuousColumn, col}, "P"), "Pilar 08");
    EXPECT_EQ(elementReference(t.p, {ReferenceTarget::Kind::Piece, lonePiece}, "P"), "Pilar 09");
    EXPECT_EQ(numberPillars(t.p, t.stack, "Pilar 08"), ContinuousError::NothingToChange);
    EXPECT_EQ(numberPillars(t.p, t.stack, "Pilar"), ContinuousError::InvalidReference);
    EXPECT_EQ(numberPillars(t.p, t.stack, ""), ContinuousError::EmptyReference);
    EXPECT_EQ(numberPillars(t.p, t.stack, std::string(29, 'a') + "9"), ContinuousError::ReferenceTooLong) << "29 + 10 would be 31";
    t.stack.undo();
    EXPECT_TRUE(t.p.continuous.pieceReferences.empty());
    EXPECT_TRUE(t.p.continuous.findColumn(col)->reference.empty());
    Project none;
    UndoStack s;
    EXPECT_EQ(numberPillars(none, s, "P1"), ContinuousError::NoElements);
}

// --- Slab depth ---------------------------------------------------------------------------------------------------------

TEST(ContinuousSlabDepth, SetResetAndRefusals) {
    Tower t;
    ASSERT_EQ(setSlabDepth(t.p, t.stack, t.n[1], 0.30), ContinuousError::None);
    EXPECT_EQ(slabDepthAt(t.p, t.n[1]), 0.30);
    ASSERT_EQ(setSlabDepth(t.p, t.stack, t.n[1], 0.35), ContinuousError::None);
    EXPECT_EQ(slabDepthAt(t.p, t.n[1]), 0.35);
    EXPECT_EQ(setSlabDepth(t.p, t.stack, t.n[1], 0.35), ContinuousError::NothingToChange);
    EXPECT_EQ(setSlabDepth(t.p, t.stack, t.n[1], -1.0), ContinuousError::InvalidValue);
    EXPECT_EQ(setSlabDepth(t.p, t.stack, 999, 0.3), ContinuousError::UnknownElement);
    ASSERT_EQ(setSlabDepth(t.p, t.stack, t.n[1], std::nullopt), ContinuousError::None);
    EXPECT_FALSE(slabDepthAt(t.p, t.n[1]).has_value());
    t.stack.undo();
    EXPECT_EQ(slabDepthAt(t.p, t.n[1]), 0.35);
    t.stack.undo();
    t.stack.undo();
    EXPECT_FALSE(slabDepthAt(t.p, t.n[1]).has_value());
    Row r;  // beam nodes are not pillar nodes
    EXPECT_EQ(setSlabDepth(r.p, r.stack, r.n[1], 0.3), ContinuousError::NotPillarNode);
}

// --- Deflection groups --------------------------------------------------------------------------------------------------

TEST(ContinuousDeflection, LimitOfBeamGroupsUndoRedo) {
    Row r;
    int cb = 0;
    ASSERT_EQ(createContinuousBeam(r.p, r.stack, {r.b[0], r.b[1]}, &cb), ContinuousError::None);
    const std::vector<int> bars = beamBarIds(r.p, {ReferenceTarget::Kind::ContinuousBeam, cb});
    EXPECT_EQ(bars, (std::vector<int>{r.b[0], r.b[1]}));
    EXPECT_FALSE(deflectionLimitOfBar(r.p, r.b[0]).has_value());
    ASSERT_EQ(setDeflectionLimit(r.p, r.stack, bars, {300.0, 0.0}), ContinuousError::None);
    ASSERT_TRUE(deflectionLimitOfBar(r.p, r.b[1]).has_value());
    EXPECT_EQ(deflectionLimitOfBar(r.p, r.b[1])->relative, 300.0);
    EXPECT_EQ(setDeflectionLimit(r.p, r.stack, bars, {300.0, 0.0}), ContinuousError::NothingToChange);
    ASSERT_EQ(setDeflectionLimit(r.p, r.stack, bars, {400.0, 15.0}), ContinuousError::None);
    EXPECT_EQ(r.p.continuous.deflectionGroups.size(), 1u) << "the group is edited, not duplicated";
    EXPECT_EQ(deflectionLimitOfBar(r.p, r.b[0])->absoluteMm, 15.0);
    // A group that partly overlaps another is refused.
    EXPECT_EQ(setDeflectionLimit(r.p, r.stack, {r.b[1], r.b[2]}, {250.0, 0.0}), ContinuousError::GroupConflict);
    EXPECT_EQ(setDeflectionLimit(r.p, r.stack, {r.b[0]}, {250.0, 0.0}), ContinuousError::GroupConflict);
    EXPECT_EQ(setDeflectionLimit(r.p, r.stack, {r.b[2]}, {-1.0, 0.0}), ContinuousError::InvalidValue);
    EXPECT_EQ(setDeflectionLimit(r.p, r.stack, {999}, {250.0, 0.0}), ContinuousError::UnknownElement);
    r.stack.undo();
    EXPECT_EQ(deflectionLimitOfBar(r.p, r.b[0])->relative, 300.0);
    r.stack.redo();
    // Clearing the limit removes the explicit group.
    ASSERT_EQ(setDeflectionLimit(r.p, r.stack, bars, {}), ContinuousError::None);
    EXPECT_TRUE(r.p.continuous.deflectionGroups.empty());
    EXPECT_EQ(setDeflectionLimit(r.p, r.stack, bars, {}), ContinuousError::NothingToChange);
    // A lone beam piece and non-beam bars.
    ASSERT_EQ(setDeflectionLimit(r.p, r.stack, beamBarIds(r.p, {ReferenceTarget::Kind::Piece, r.piece[2]}), {0.0, 20.0}), ContinuousError::None);
    ASSERT_EQ(setPieceType(r.p, r.stack, {r.b[2]}, ElementType::Generic), PieceError::None);
    EXPECT_EQ(setDeflectionLimit(r.p, r.stack, {r.b[2]}, {100.0, 0.0}), ContinuousError::NotBeamBars);
}

// --- File round trip and compatibility ----------------------------------------------------------------------------------

namespace {

Project fullProject() {
    Tower t;
    int col = 0;
    EXPECT_EQ(createContinuousColumn(t.p, t.stack, t.n[0], t.n[3], &col), ContinuousError::None);
    EXPECT_EQ(setSlabDepth(t.p, t.stack, t.n[1], 0.32), ContinuousError::None);
    EXPECT_EQ(setReference(t.p, t.stack, {ReferenceTarget::Kind::ContinuousColumn, col}, "Pilar 4"), ContinuousError::None);
    // Beams beside the tower, in the same project.
    const int steel = t.p.structure.addMaterial(Material{0, "S275", MaterialKind::Steel});
    int node[4], bar[3], piece[3];
    for (int i = 0; i < 4; ++i) node[i] = t.p.structure.addNode({10.0 + 4.0 * i, 0, 3});
    for (int i = 0; i < 3; ++i) {
        bar[i] = t.p.structure.addBar(node[i], node[i + 1], steel);
        EXPECT_EQ(setPieceType(t.p, t.stack, {bar[i]}, ElementType::Beam), PieceError::None);
        piece[i] = t.p.structure.pieceOfBar(bar[i])->id;
    }
    int cb = 0;
    EXPECT_EQ(createContinuousBeam(t.p, t.stack, {bar[0], bar[1]}, &cb), ContinuousError::None);
    EXPECT_EQ(setReference(t.p, t.stack, {ReferenceTarget::Kind::ContinuousBeam, cb}, "Pórtico 3"), ContinuousError::None);
    EXPECT_EQ(setReference(t.p, t.stack, {ReferenceTarget::Kind::Piece, piece[2]}, "V-9"), ContinuousError::None);
    EXPECT_EQ(setDeflectionLimit(t.p, t.stack, {bar[0], bar[1]}, {300.0, 12.5}), ContinuousError::None);
    return t.p;
}

}  // namespace

TEST(ContinuousFile, SectionRoundTripAndFileRoundTrip) {
    const Project original = fullProject();
    ASSERT_FALSE(original.continuous.empty());
    EXPECT_TRUE(project_json::isKnownSection("continuous_elements"));
    const std::string text = project_json::dumpSection(original, "continuous_elements");
    Project loaded;
    project_json::loadSection(loaded, "continuous_elements", text);
    EXPECT_EQ(loaded.continuous, original.continuous);

    const fs::path dir = fs::temp_directory_path() / "cc_continuous_tests";
    fs::remove_all(dir);
    fs::create_directories(dir);
    ProjectDocument doc = makeDocument(original, "continuos");
    ASSERT_TRUE(saveProjectFile(dir / "c.ed3", doc).ok);
    LoadResult back = loadProjectFile(dir / "c.ed3");
    ASSERT_TRUE(back.ok) << back.error;
    EXPECT_EQ(back.document.project.continuous, original.continuous);
    EXPECT_TRUE(back.document.unknownSections.empty());
    EXPECT_TRUE(validateContinuous(back.document.project).empty());
    EXPECT_TRUE(project_json::validate(back.document.project).empty());
    fs::remove_all(dir);
}

TEST(ContinuousFile, OldFilesAndPartialSectionsLoadWithDefaults) {
    Project p;
    EXPECT_TRUE(p.continuous.empty());  // a file without the section keeps the default (empty)
    project_json::loadSection(p, "continuous_elements", R"({"slabDepths":[{"nodeId":2,"depthM":0.25}],"futureField":1})");
    ASSERT_EQ(p.continuous.slabDepths.size(), 1u);
    EXPECT_EQ(p.continuous.slabDepths[0].depthM, 0.25);
    EXPECT_TRUE(p.continuous.columns.empty());
    EXPECT_EQ(p.continuous.nextColumnId, 1);
    EXPECT_THROW(project_json::loadSection(p, "continuous_elements", "{not json"), std::exception);
    // An inconsistent section is reported by validate().
    Project bad;
    bad.continuous.slabDepths.push_back({1, -2.0});
    EXPECT_FALSE(project_json::validate(bad).empty());
}

TEST(ContinuousReferences, PillarsAreNumberedByYThenX) {
    Project p;
    UndoStack stack;
    // (x, y): A (5, 0), B (0, 2), C (3, 0), D (9, 1)
    const double pos[4][2] = {{5, 0}, {0, 2}, {3, 0}, {9, 1}};
    int piece[4];
    for (int i = 0; i < 4; ++i) {
        const int a = p.structure.addNode({pos[i][0], pos[i][1], 0}), b = p.structure.addNode({pos[i][0], pos[i][1], 3});
        const int bar = p.structure.addBar(a, b);
        ASSERT_EQ(setPieceType(p, stack, {bar}, ElementType::Pillar), PieceError::None);
        piece[i] = p.structure.pieceOfBar(bar)->id;
    }
    ASSERT_EQ(numberPillars(p, stack, "P1"), ContinuousError::None);
    auto ref = [&](int i) { return elementReference(p, {ReferenceTarget::Kind::Piece, piece[i]}, "P"); };
    EXPECT_EQ(ref(2), "P1");  // y = 0, x = 3
    EXPECT_EQ(ref(0), "P2");  // y = 0, x = 5
    EXPECT_EQ(ref(3), "P3");  // y = 1
    EXPECT_EQ(ref(1), "P4");  // y = 2
}
