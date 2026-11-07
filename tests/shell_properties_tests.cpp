#include <gtest/gtest.h>

#include <cmath>

#include "core/node_edit.h"
#include "core/project_json.h"
#include "core/shell_edit.h"
#include "core/shell_properties_edit.h"

using namespace cc::core;

namespace {

/// Square 4 x 4 (nodes 1..4 counter-clockwise, local 2D system = global XY) with one shell.
struct Plate {
    Project p;
    UndoStack stack;
    int n[4] = {};
    int shell = 0;
    Plate() {
        n[0] = p.structure.addNode({0, 0, 0});
        n[1] = p.structure.addNode({4, 0, 0});
        n[2] = p.structure.addNode({4, 4, 0});
        n[3] = p.structure.addNode({0, 4, 0});
        EXPECT_EQ(addShell(p, stack, {n[0], n[1], n[2], n[3]}, 0.1, 0, &shell), ShellError::None);
        stack.clear();
    }
    std::vector<Point2> contour() const { return shellContour2D(p.structure, *p.structure.findShell(shell)); }
};

std::vector<Point2> square() { return {{0, 0}, {4, 0}, {4, 4}, {0, 4}}; }

}  // namespace

TEST(ShellProps, DefaultsAndLabels) {
    const ShellRecord r;
    EXPECT_DOUBLE_EQ(r.maxTriangleSideM, 1.0);
    for (int i = 0; i < kStiffnessFactorCount; ++i) EXPECT_DOUBLE_EQ(r.stiffness.value(i), 1.0);
    EXPECT_FALSE(r.bucklingConsidered);
    EXPECT_TRUE(r.modalConsidered);
    EXPECT_TRUE(r.bands.empty());
    EXPECT_EQ(stiffnessFieldLabel(0), "Membrana Fxx");
    EXPECT_EQ(stiffnessFieldLabel(1), "Membrana Fxy");
    EXPECT_EQ(stiffnessFieldLabel(3), "Flexión Mxx");
    EXPECT_EQ(stiffnessFieldLabel(7), "Cortante Vyz");
    EXPECT_TRUE(ShellProperties{}.empty());
    EXPECT_TRUE(isDefaultEdgeInterior(EdgeInteriorBond{}));
    EXPECT_EQ(EdgeInteriorBond{}.kind, EdgeInteriorKind::Fixed);
    EXPECT_DOUBLE_EQ(EdgeInteriorBond{}.k[3], 100000.0);
    EXPECT_EQ(BandForceOptions{}.method, BandForceMethod::InternalStresses);
    EXPECT_TRUE(BandForceOptions{}.smoothedStresses);
    // Dialog order: Fxx, Fxy, Fyy.
    ShellStiffnessFactors f;
    f.set(2, 2.5);
    EXPECT_DOUBLE_EQ(f.fyy, 2.5);
    EXPECT_DOUBLE_EQ(f.fxy, 1.0);
}

TEST(ShellProps, InfoLines) {
    ShellRecord r;
    const auto stiff = shellInfoLines(r, ShellInfoPart::Stiffness);
    ASSERT_EQ(stiff.size(), 8u);
    EXPECT_EQ(stiff[0], "Membrana Fxx : 1");
    EXPECT_EQ(stiff[1], "Membrana Fyy : 1");
    EXPECT_EQ(stiff[2], "Membrana Fxy : 1");
    EXPECT_EQ(stiff[4], "Flexión Myy : 1");
    EXPECT_EQ(shellInfoLines(r, ShellInfoPart::Discretisation)[0], "Tamaño máximo de los lados de un triángulo : 1.00 m");
    EXPECT_EQ(shellInfoLines(r, ShellInfoPart::Buckling), (std::vector<std::string>{"Análisis de pandeo", "No se considera en el análisis"}));
    EXPECT_EQ(shellInfoLines(r, ShellInfoPart::Modal), (std::vector<std::string>{"Análisis modal", "Se considera en el análisis"}));
    EXPECT_EQ(edgeInteriorLine(EdgeInteriorBond{}), "Vinc. interior : Empotrada");
    EXPECT_EQ(edgeInteriorKindName(EdgeInteriorKind::Generic), "Genérica");
}

TEST(ShellProps, TriangleSizeValidation) {
    EXPECT_FALSE(checkTriangleSize(1.0));
    EXPECT_FALSE(checkTriangleSize(0.01));
    EXPECT_FALSE(checkTriangleSize(100.0));
    for (double bad : {0.0, 0.001, 100000.0, -1.0, std::nan("")}) {
        const auto issue = checkTriangleSize(bad);
        ASSERT_TRUE(issue.has_value());
        EXPECT_EQ(issue->text, "El valor de 'Tamaño máximo de los lados de un triángulo' debe estar entre 0.01 m y 100.00 m.");
    }
}

TEST(ShellProps, StiffnessValidationMessagesAreLiteral) {
    ShellStiffnessFactors f;
    EXPECT_FALSE(checkStiffnessFactors(f));
    f.fxx = 0.0;
    f.vyz = 100.0;
    EXPECT_FALSE(checkStiffnessFactors(f));
    f.fxx = 100.5;
    auto issue = checkStiffnessFactors(f);
    ASSERT_TRUE(issue.has_value());
    EXPECT_EQ(issue->text, "El valor de 'Membrana Fxx' debe estar entre 0.000  y 100.000 .");
    f.fxx = 1.0;
    f.myy = -1.0;  // dialog order: Myy is the 6th field
    f.vxz = 1000.0;
    issue = checkStiffnessFactors(f);
    ASSERT_TRUE(issue.has_value());
    EXPECT_EQ(issue->text, "El valor de 'Flexión Myy' debe estar entre 0.000  y 100.000 .");
}

TEST(ShellProps, BandWidthValidation) {
    EXPECT_FALSE(checkBandWidth(0.15));
    EXPECT_FALSE(checkBandWidth(0.01));
    EXPECT_FALSE(checkBandWidth(100.0));
    EXPECT_EQ(checkBandWidth(0.005)->text, "El valor de 'Anchura' debe estar entre 1.0 y 10000.0.");
    EXPECT_TRUE(checkBandWidth(101.0).has_value());
}

TEST(ShellProps, EdgeInteriorNormalisationAndValidation) {
    EdgeInteriorBond b;
    b.kind = EdgeInteriorKind::Pinned;
    b.dof[0] = DofMode::Fixed;  // ignored for a non-generic kind
    b.k[1] = 5.0;
    EXPECT_EQ(normaliseEdgeInterior(b).dof[0], DofMode::Free);
    EXPECT_DOUBLE_EQ(normaliseEdgeInterior(b).k[1], 100000.0);
    EdgeInteriorBond g;
    g.kind = EdgeInteriorKind::Generic;
    g.dof[2] = DofMode::Elastic;
    g.k[2] = 0.0;
    EXPECT_TRUE(checkEdgeInterior(g).has_value());
    g.k[2] = 250.0;
    g.k[4] = 7.0;  // not elastic -> reset
    EXPECT_FALSE(checkEdgeInterior(g));
    EXPECT_DOUBLE_EQ(normaliseEdgeInterior(g).k[4], 100000.0);
    EXPECT_DOUBLE_EQ(normaliseEdgeInterior(g).k[2], 250.0);
}

TEST(ShellProps, EdgeKeyIsNormalisedAndListedInContourOrder) {
    Plate pl;
    EXPECT_EQ(makeEdgeKey(3, 5, 2), (ShellEdgeKey{3, 2, 5}));
    const auto edges = shellEdges(pl.p.structure, pl.shell);
    ASSERT_EQ(edges.size(), 4u);
    EXPECT_EQ(edges[0], makeEdgeKey(pl.shell, pl.n[0], pl.n[1]));
    EXPECT_EQ(edges[3], makeEdgeKey(pl.shell, pl.n[3], pl.n[0]));
    EXPECT_TRUE(shellEdges(pl.p.structure, 99).empty());
}

TEST(ShellProps, LocalFrameOfTheShell) {
    Plate pl;
    const auto frame = shellFrame(pl.p.structure, *pl.p.structure.findShell(pl.shell));
    ASSERT_TRUE(frame.has_value());
    const Point2 c = frame->toLocal({4, 4, 0});
    EXPECT_NEAR(c.x, 4.0, 1e-12);
    EXPECT_NEAR(c.y, 4.0, 1e-12);
    const Point3 w = frame->toWorld({1.5, 2.5});
    EXPECT_NEAR(w.x, 1.5, 1e-12);
    EXPECT_NEAR(w.y, 2.5, 1e-12);
    EXPECT_EQ(pl.contour().size(), 4u);
}

TEST(ShellBands, NewBandChecks) {
    const auto c = square();
    auto r = newBand(c, {}, {3, 1}, {1, 1}, 0.15);
    ASSERT_TRUE(r.ok());
    ASSERT_EQ(r.bands.size(), 1u);
    EXPECT_DOUBLE_EQ(r.bands[0].x1, 1.0);  // ends stored ordered
    EXPECT_DOUBLE_EQ(r.bands[0].x2, 3.0);
    // Outside / sticking out because of the width (#64190).
    r = newBand(c, {}, {1, 1}, {5, 1}, 0.15);
    ASSERT_FALSE(r.ok());
    EXPECT_EQ(r.issue->messageId, 64190);
    EXPECT_EQ(r.issue->text, "La banda de integración no está completamente contenida en la lámina.");
    EXPECT_FALSE(newBand(c, {}, {1, 0.05}, {3, 0.05}, 0.15).ok());
    EXPECT_TRUE(newBand(c, {}, {1, 0.075}, {3, 0.075}, 0.15).ok());  // the edge of the rectangle touches the contour
    // Too short (#64191), bad width.
    r = newBand(c, {}, {1, 1}, {1.1, 1}, 0.15);
    ASSERT_FALSE(r.ok());
    EXPECT_EQ(r.issue->messageId, 64191);
    EXPECT_EQ(r.issue->text, "La longitud de la banda de integración es inferior a la mínima permitida (0.16 m).");
    EXPECT_EQ(newBand(c, {}, {1, 1}, {3, 1}, 0.0).issue->text, bandWidthRangeMessage());
}

TEST(ShellBands, ForcePointsDefaultRangeAndEdit) {
    const auto c = square();
    auto r = newBand(c, {}, {1, 1}, {3, 1}, 0.15);
    ASSERT_TRUE(r.ok());
    EXPECT_EQ(r.bands[0].forcePoints, 10) << "default measured";
    EXPECT_FALSE(checkBandForcePoints(3).has_value());
    EXPECT_FALSE(checkBandForcePoints(1000).has_value());
    for (int bad : {0, 1, 2, 1001}) {
        const auto issue = checkBandForcePoints(bad);
        ASSERT_TRUE(issue.has_value()) << bad;
        EXPECT_EQ(issue->text, "El valor de '' debe estar entre 3  y 1000 .") << "empty quotes: the original does not interpolate the label";
    }
    auto e = editBands(c, r.bands, {0}, 0.3, 25);
    ASSERT_TRUE(e.ok());
    EXPECT_EQ(e.bands[0].forcePoints, 25);
    EXPECT_DOUBLE_EQ(e.bands[0].widthM, 0.3);
    EXPECT_FALSE(editBands(c, r.bands, {0}, 0.3, 1).ok());
    EXPECT_EQ(editBands(c, e.bands, {0}, 0.2).bands[0].forcePoints, 25) << "no value given: the points are kept";
    // Divide / join keep them.
    EXPECT_EQ(divideBands(c, e.bands, {0}, 2).bands[1].forcePoints, 25);
}

TEST(ShellBands, MinimumLengthIsProvisionalAndOnlyNewAndGenerateEnforceIt) {
    const auto c = square();
    EXPECT_DOUBLE_EQ(kBandMinLengthM, 0.16);
    EXPECT_FALSE(newBand(c, {}, {1, 1}, {1.15, 1}, 0.15).ok());
    EXPECT_TRUE(newBand(c, {}, {1, 1}, {1.18, 1}, 0.15).ok());
    // "Mover extremo" keeps a 6 cm band without a warning; "Incidencias" flags it (checkBand with the length check).
    auto r = newBand(c, {}, {1, 1}, {3, 1}, 0.15);
    auto m = moveBandEnd(c, r.bands, 0, 1, {1.06, 1});
    ASSERT_TRUE(m.ok());
    EXPECT_NEAR(m.bands[0].length(), 0.06, 1e-9);
    ASSERT_TRUE(checkBand(c, m.bands[0]).has_value());
    EXPECT_EQ(checkBand(c, m.bands[0])->messageId, 64191);
    EXPECT_FALSE(checkBand(c, m.bands[0], false).has_value());
}

TEST(ShellBands, ConcaveContourRejectsBandCrossingTheNotch) {
    // L shape: the notch occupies x > 2, y > 2.
    const std::vector<Point2> l{{0, 0}, {4, 0}, {4, 2}, {2, 2}, {2, 4}, {0, 4}};
    EXPECT_TRUE(newBand(l, {}, {0.5, 3}, {1.5, 3}, 0.15).ok());
    EXPECT_FALSE(newBand(l, {}, {0.5, 3}, {3.5, 3}, 0.15).ok());
    EXPECT_FALSE(newBand(l, {}, {1, 1}, {3, 3}, 0.15).ok());  // the diagonal cuts the notch corner
    EXPECT_TRUE(newBand(l, {}, {0.5, 1}, {3.5, 1}, 0.15).ok());
}

TEST(ShellBands, EditDeleteMove) {
    const auto c = square();
    auto r = newBand(c, {}, {1, 1}, {3, 1}, 0.15);
    r = newBand(c, r.bands, {1, 2}, {3, 2}, 0.15);
    r = newBand(c, r.bands, {1, 3}, {3, 3}, 0.15);
    ASSERT_EQ(r.bands.size(), 3u);

    auto e = editBands(c, r.bands, {0, 2, 7}, 0.3);
    ASSERT_TRUE(e.ok());
    EXPECT_DOUBLE_EQ(e.bands[0].widthM, 0.3);
    EXPECT_DOUBLE_EQ(e.bands[1].widthM, 0.15);
    EXPECT_DOUBLE_EQ(e.bands[2].widthM, 0.3);
    EXPECT_FALSE(editBands(c, r.bands, {0}, 5.0).ok());   // 5 m wide does not fit
    EXPECT_FALSE(editBands(c, r.bands, {9}, 0.3).ok());   // nothing selected
    EXPECT_FALSE(editBands(c, r.bands, {0}, 0.0).ok());

    auto d = deleteBands(r.bands, {1});
    ASSERT_TRUE(d.ok());
    ASSERT_EQ(d.bands.size(), 2u);
    EXPECT_DOUBLE_EQ(d.bands[1].y1, 3.0);
    EXPECT_FALSE(deleteBands(r.bands, {}).ok());

    auto m = moveBandEnd(c, r.bands, 0, 1, {3.5, 1.5});
    ASSERT_TRUE(m.ok());
    EXPECT_DOUBLE_EQ(m.bands[0].x2, 3.5);
    EXPECT_DOUBLE_EQ(m.bands[0].y2, 1.5);
    EXPECT_DOUBLE_EQ(m.bands[0].x1, 1.0);
    m = moveBandEnd(c, r.bands, 0, 0, {5, 1});
    ASSERT_FALSE(m.ok());
    EXPECT_EQ(m.issue->messageId, 64190);
    // Moving the first end beyond the second reorders the ends.
    m = moveBandEnd(c, r.bands, 0, 0, {3.5, 1});
    ASSERT_TRUE(m.ok());
    EXPECT_DOUBLE_EQ(m.bands[0].x1, 3.0);
    EXPECT_DOUBLE_EQ(m.bands[0].x2, 3.5);
}

TEST(ShellBands, JoinMergesParallelBandsIntoOneCentredBetweenThem) {
    const auto c = square();
    // Measured (docs 11.1): two horizontal bands 0.61 m apart, lengths 3.05 and 2.44 m, width 0.15 m -> one band centred between them,
    // length = union of the projections (3.05 m), width = separation + width = 0.76 m.
    auto r = newBand(c, {}, {0.5, 1.0}, {3.55, 1.0}, 0.15);
    r = newBand(c, r.bands, {0.8, 1.61}, {3.24, 1.61}, 0.15);
    auto j = joinBands(c, r.bands, {0, 1});
    ASSERT_TRUE(j.ok());
    ASSERT_EQ(j.bands.size(), 1u);
    EXPECT_NEAR(j.bands[0].x1, 0.5, 1e-9);
    EXPECT_NEAR(j.bands[0].x2, 3.55, 1e-9);
    EXPECT_NEAR(j.bands[0].y1, 1.305, 1e-9);
    EXPECT_NEAR(j.bands[0].y2, 1.305, 1e-9);
    EXPECT_NEAR(j.bands[0].widthM, 0.76, 1e-9);
    // The same with vertical bands and a different drawing order.
    r = newBand(c, {}, {1.61, 0.8}, {1.61, 3.24}, 0.15);
    r = newBand(c, r.bands, {1.0, 0.5}, {1.0, 3.55}, 0.15);
    j = joinBands(c, r.bands, {0, 1});
    ASSERT_TRUE(j.ok());
    ASSERT_EQ(j.bands.size(), 1u);
    EXPECT_NEAR(j.bands[0].x1, 1.305, 1e-9);
    EXPECT_NEAR(j.bands[0].widthM, 0.76, 1e-9);
    EXPECT_NEAR(j.bands[0].length(), 3.05, 1e-9);
}

TEST(ShellBands, JoinNeedsParallelBands) {
    const auto c = square();
    auto r = newBand(c, {}, {0.5, 1}, {1.5, 1}, 0.15);
    r = newBand(c, r.bands, {2, 1}, {3.5, 1}, 0.2);
    r = newBand(c, r.bands, {1, 2}, {1, 3}, 0.15);
    ASSERT_EQ(r.bands.size(), 3u);
    auto j = joinBands(c, r.bands, {0, 1});
    ASSERT_TRUE(j.ok());
    ASSERT_EQ(j.bands.size(), 2u);
    EXPECT_DOUBLE_EQ(j.bands[0].x1, 0.5);
    EXPECT_DOUBLE_EQ(j.bands[0].x2, 3.5);
    EXPECT_DOUBLE_EQ(j.bands[0].widthM, 0.2) << "collinear bands: no separation, the widest width";
    j = joinBands(c, r.bands, {0, 2});
    ASSERT_FALSE(j.ok());
    EXPECT_EQ(j.issue->messageId, 63999);
    EXPECT_EQ(j.issue->text, "Las bandas de integración seleccionadas deben ser paralelas.");
    EXPECT_FALSE(joinBands(c, r.bands, {0}).ok());
}

TEST(ShellBands, Divide) {
    const auto c = square();
    auto r = newBand(c, {}, {0.5, 1}, {3.5, 1}, 0.15);
    r = newBand(c, r.bands, {1, 3}, {2, 3}, 0.1);
    auto d = divideBands(c, r.bands, {0}, 3);
    ASSERT_TRUE(d.ok());
    ASSERT_EQ(d.bands.size(), 4u);
    EXPECT_DOUBLE_EQ(d.bands[0].x1, 0.5);
    EXPECT_NEAR(d.bands[0].x2, 1.5, 1e-12);
    EXPECT_NEAR(d.bands[1].x1, 1.5, 1e-12);
    EXPECT_DOUBLE_EQ(d.bands[2].x2, 3.5);
    EXPECT_DOUBLE_EQ(d.bands[3].widthM, 0.1);
    EXPECT_FALSE(divideBands(c, r.bands, {0}, 1).ok());
    EXPECT_FALSE(divideBands(c, r.bands, {}, 2).ok());
    // Measured (docs 11.2): range 2..10 and the literal warning with the double space.
    for (int bad : {0, 1, 11, 500}) {
        const auto e = divideBands(c, r.bands, {0}, bad);
        ASSERT_FALSE(e.ok()) << bad;
        EXPECT_EQ(e.issue->text, "El valor de 'Número de divisiones' debe estar entre 2  y 10 .");
    }
    EXPECT_TRUE(divideBands(c, r.bands, {0}, 10).ok());
    EXPECT_EQ(kBandDefaultDivisions, 2);
}

TEST(ShellBands, GenerateInZoneStacksBandsWithStepEqualToWidth) {
    const auto c = square();
    // Zone 3.0 x 3.18: 3.18 / 0.15 = 21.2 -> 21 contiguous bands.
    const std::array<Point2, 4> zone{{{0.5, 0.5}, {3.5, 0.5}, {3.5, 3.68}, {0.5, 3.68}}};
    auto g = generateBands(c, {}, zone, 0.15);
    ASSERT_TRUE(g.ok());
    ASSERT_EQ(g.bands.size(), 21u);
    for (std::size_t i = 0; i < g.bands.size(); ++i) {
        EXPECT_NEAR(g.bands[i].x1, 0.5, 1e-9);
        EXPECT_NEAR(g.bands[i].x2, 3.5, 1e-9);
        EXPECT_NEAR(g.bands[i].y1, 0.5 + (static_cast<double>(i) + 0.5) * 0.15, 1e-9);
        EXPECT_NEAR(g.bands[i].y1, g.bands[i].y2, 1e-9);
        EXPECT_DOUBLE_EQ(g.bands[i].widthM, 0.15);
    }
    EXPECT_FALSE(checkBand(c, g.bands.back()).has_value());
    // The existing bands are kept.
    const auto first = newBand(c, {}, {1, 1}, {2, 1}, 0.15);
    EXPECT_EQ(generateBands(c, first.bands, zone, 0.15).bands.size(), 22u);
}

TEST(ShellBands, GenerateClipsToTheShellAndReportsErrors) {
    const auto c = square();
    // The zone sticks out of the shell on the right: bands end at the contour.
    const std::array<Point2, 4> wide{{{0.5, 1}, {6, 1}, {6, 2}, {0.5, 2}}};
    auto g = generateBands(c, {}, wide, 0.5);
    ASSERT_TRUE(g.ok());
    ASSERT_EQ(g.bands.size(), 2u);
    EXPECT_NEAR(g.bands[0].x2, 4.0, 1e-9);
    // Bow tie.
    const std::array<Point2, 4> bow{{{0, 0}, {1, 1}, {1, 0}, {0, 1}}};
    auto e = generateBands(c, {}, bow, 0.15);
    ASSERT_FALSE(e.ok());
    EXPECT_EQ(e.issue->messageId, 65158);
    EXPECT_EQ(e.issue->text, "Los puntos seleccionados no definen un cuadrilátero");
    const std::array<Point2, 4> degenerate{{{0, 0}, {1, 0}, {2, 0}, {3, 0}}};
    EXPECT_EQ(generateBands(c, {}, degenerate, 0.15).issue->messageId, 65158);
    // Zone not wider than the band.
    const std::array<Point2, 4> thin{{{1, 1}, {3, 1}, {3, 1.1}, {1, 1.1}}};
    e = generateBands(c, {}, thin, 0.15);
    ASSERT_FALSE(e.ok());
    EXPECT_EQ(e.issue->messageId, 65159);
    EXPECT_EQ(e.issue->text, "El ancho de la zona de generación debe ser mayor que el ancho de la banda de integración.");
    // Zone outside the shell.
    const std::array<Point2, 4> outside{{{5, 5}, {7, 5}, {7, 7}, {5, 7}}};
    e = generateBands(c, {}, outside, 0.15);
    ASSERT_FALSE(e.ok());
    EXPECT_EQ(e.issue->messageId, 64476);
    EXPECT_EQ(e.issue->text, "No es posible generar bandas de integración en la zona seleccionada.");
}

TEST(ShellPropsEdit, DiscretisationUndoRedoAndErrors) {
    Plate pl;
    EXPECT_FALSE(assignShellDiscretisation(pl.p, pl.stack, {pl.shell}, 0.5));
    EXPECT_DOUBLE_EQ(shellRecordOf(pl.p.shellProps, pl.shell).maxTriangleSideM, 0.5);
    pl.stack.undo();
    EXPECT_TRUE(pl.p.shellProps.empty());
    pl.stack.redo();
    EXPECT_FALSE(pl.p.shellProps.empty());
    const auto issue = assignShellDiscretisation(pl.p, pl.stack, {pl.shell}, 0.001);
    ASSERT_TRUE(issue.has_value());
    EXPECT_DOUBLE_EQ(shellRecordOf(pl.p.shellProps, pl.shell).maxTriangleSideM, 0.5);
    // Back to the default erases the record.
    EXPECT_FALSE(assignShellDiscretisation(pl.p, pl.stack, {pl.shell}, 1.0));
    EXPECT_TRUE(pl.p.shellProps.empty());
    // No change / unknown shell push nothing.
    const bool canUndo = pl.stack.canUndo();
    EXPECT_FALSE(assignShellDiscretisation(pl.p, pl.stack, {pl.shell}, 1.0));
    EXPECT_FALSE(assignShellDiscretisation(pl.p, pl.stack, {42}, 2.0));
    EXPECT_EQ(pl.stack.canUndo(), canUndo);
    EXPECT_TRUE(pl.p.shellProps.empty());
}

TEST(ShellPropsEdit, StiffnessBucklingModal) {
    Plate pl;
    ShellStiffnessFactors f;
    f.mxy = 0.0;
    f.vyz = 12.3456;
    EXPECT_FALSE(assignShellStiffness(pl.p, pl.stack, {pl.shell}, f));
    EXPECT_DOUBLE_EQ(shellRecordOf(pl.p.shellProps, pl.shell).stiffness.mxy, 0.0);
    f.fyy = 101.0;
    ASSERT_TRUE(assignShellStiffness(pl.p, pl.stack, {pl.shell}, f).has_value());
    EXPECT_DOUBLE_EQ(shellRecordOf(pl.p.shellProps, pl.shell).stiffness.fyy, 1.0);
    EXPECT_FALSE(assignShellBuckling(pl.p, pl.stack, {pl.shell}, true));
    EXPECT_FALSE(assignShellModal(pl.p, pl.stack, {pl.shell}, false));
    const ShellRecord r = shellRecordOf(pl.p.shellProps, pl.shell);
    EXPECT_TRUE(r.bucklingConsidered);
    EXPECT_FALSE(r.modalConsidered);
    pl.stack.undo();
    EXPECT_TRUE(shellRecordOf(pl.p.shellProps, pl.shell).modalConsidered);
    EXPECT_EQ(shellInfoLines(r, ShellInfoPart::Buckling)[1], "Se considera en el análisis");
}

TEST(ShellPropsEdit, EdgeBonds) {
    Plate pl;
    const ShellEdgeKey bottom = makeEdgeKey(pl.shell, pl.n[1], pl.n[0]);  // reversed on purpose
    EdgeInteriorBond pinned;
    pinned.kind = EdgeInteriorKind::Pinned;
    EXPECT_FALSE(assignEdgeInterior(pl.p, pl.stack, {bottom}, pinned));
    EXPECT_EQ(edgeInteriorOf(pl.p.shellProps, bottom).kind, EdgeInteriorKind::Pinned);
    EXPECT_EQ(edgeInteriorOf(pl.p.shellProps, makeEdgeKey(pl.shell, pl.n[1], pl.n[2])).kind, EdgeInteriorKind::Fixed);
    // A pair that is not an edge of the shell (a diagonal) is ignored.
    const bool beforeDiagonal = pl.stack.canUndo();
    EXPECT_FALSE(assignEdgeInterior(pl.p, pl.stack, {makeEdgeKey(pl.shell, pl.n[0], pl.n[2])}, pinned));
    EXPECT_EQ(pl.p.shellProps.interior.size(), 1u);
    EXPECT_EQ(pl.stack.canUndo(), beforeDiagonal);
    // "Empotrada" again erases it.
    EXPECT_FALSE(assignEdgeInterior(pl.p, pl.stack, {bottom}, EdgeInteriorBond{}));
    EXPECT_TRUE(pl.p.shellProps.interior.empty());
    EdgeInteriorBond bad;
    bad.kind = EdgeInteriorKind::Generic;
    bad.dof[0] = DofMode::Elastic;
    bad.k[0] = -3;
    EXPECT_TRUE(assignEdgeInterior(pl.p, pl.stack, {bottom}, bad).has_value());

    // Exterior: reuses the node supports.
    ExternalSupport pin;
    pin.type = ExternalSupportType::Pinned;
    EXPECT_FALSE(assignEdgeExterior(pl.p, pl.stack, {bottom, makeEdgeKey(pl.shell, pl.n[2], pl.n[3])}, pin));
    EXPECT_EQ(pl.p.shellProps.exterior.size(), 2u);
    EXPECT_EQ(edgeExteriorOf(pl.p.shellProps, bottom).type, ExternalSupportType::Pinned);
    EXPECT_EQ(edgeExteriorOf(pl.p.shellProps, makeEdgeKey(pl.shell, pl.n[1], pl.n[2])).type, ExternalSupportType::Free);
    ExternalSupport elastic;
    elastic.type = ExternalSupportType::Generic;
    elastic.generic.translations[2] = DofMode::Elastic;
    elastic.elasticConstants[2] = 0.0;  // outside 1..10000000
    const auto issue = assignEdgeExterior(pl.p, pl.stack, {bottom}, elastic);
    ASSERT_TRUE(issue.has_value());
    EXPECT_EQ(issue->text, "El valor de 'K' debe estar entre 1 y 10000000.");
    EXPECT_EQ(pl.p.shellProps.exterior.size(), 2u);
    EXPECT_FALSE(assignEdgeExterior(pl.p, pl.stack, {bottom}, ExternalSupport{}));  // "Libre"
    EXPECT_EQ(pl.p.shellProps.exterior.size(), 1u);
    pl.stack.undo();
    EXPECT_EQ(pl.p.shellProps.exterior.size(), 2u);
}

TEST(ShellPropsEdit, BandsCopyToTheOtherShellsOnTheGeometryOfTheFirst) {
    Plate pl;
    // A second shell next to the first: 2 x 2 at x = 6..8 (bands y up to 3.9 fit only in the big one).
    const int a = pl.p.structure.addNode({6, 0, 0}), b = pl.p.structure.addNode({8, 0, 0}), c = pl.p.structure.addNode({8, 2, 0}),
              d = pl.p.structure.addNode({6, 2, 0});
    int small = 0;
    ASSERT_EQ(addShell(pl.p, pl.stack, {a, b, c, d}, 0.1, 0, &small), ShellError::None);
    pl.stack.clear();

    const auto b1 = newBand(pl.contour(), {}, {0.5, 1}, {1.5, 1}, 0.15);
    auto both = newBand(pl.contour(), b1.bands, {0.5, 3.5}, {1.5, 3.5}, 0.15);  // outside the small shell (it is only 2 m high)
    int dropped = -1;
    EXPECT_FALSE(assignShellBands(pl.p, pl.stack, {pl.shell, small}, both.bands, &dropped));
    EXPECT_EQ(dropped, 1);
    EXPECT_EQ(shellRecordOf(pl.p.shellProps, pl.shell).bands.size(), 2u);
    EXPECT_EQ(shellRecordOf(pl.p.shellProps, small).bands.size(), 1u);
    // A band invalid for the first shell is an error and nothing changes.
    auto bad = both.bands;
    bad[0].x2 = 9.0;
    const auto issue = assignShellBands(pl.p, pl.stack, {pl.shell, small}, bad);
    ASSERT_TRUE(issue.has_value());
    EXPECT_EQ(issue->messageId, 64190);
    EXPECT_EQ(shellRecordOf(pl.p.shellProps, pl.shell).bands.size(), 2u);
    pl.stack.undo();
    EXPECT_TRUE(pl.p.shellProps.empty());
    pl.stack.redo();
    // Deleting all of them leaves the default record, which is erased.
    EXPECT_FALSE(assignShellBands(pl.p, pl.stack, {pl.shell, small}, {}));
    EXPECT_TRUE(pl.p.shellProps.empty());
}

TEST(ShellPropsEdit, BandForceOptions) {
    Plate pl;
    EXPECT_FALSE(assignBandForceOptions(pl.p, pl.stack, {BandForceMethod::NodalForces, false}));
    EXPECT_EQ(pl.p.shellProps.bandForce.method, BandForceMethod::NodalForces);
    EXPECT_FALSE(pl.p.shellProps.bandForce.smoothedStresses);
    pl.stack.undo();
    EXPECT_EQ(pl.p.shellProps.bandForce, BandForceOptions{});
    EXPECT_TRUE(pl.p.shellProps.empty());
}

TEST(ShellPropsJson, RoundTripAndOldFiles) {
    Plate pl;
    ShellStiffnessFactors f;
    f.fxy = 0.25;
    f.vxz = 3.0;
    ASSERT_FALSE(assignShellStiffness(pl.p, pl.stack, {pl.shell}, f));
    ASSERT_FALSE(assignShellDiscretisation(pl.p, pl.stack, {pl.shell}, 0.35));
    ASSERT_FALSE(assignShellBuckling(pl.p, pl.stack, {pl.shell}, true));
    ASSERT_FALSE(assignShellModal(pl.p, pl.stack, {pl.shell}, false));
    auto bands = newBand(pl.contour(), {}, {3, 1}, {1, 1.5}, 0.2);
    bands.bands[0].forcePoints = 37;
    ASSERT_FALSE(assignShellBands(pl.p, pl.stack, {pl.shell}, bands.bands));
    EdgeInteriorBond g;
    g.kind = EdgeInteriorKind::Generic;
    g.dof = {DofMode::Fixed, DofMode::Elastic, DofMode::Free, DofMode::Free, DofMode::Fixed, DofMode::Free};
    g.k[1] = 4321.0;
    ASSERT_FALSE(assignEdgeInterior(pl.p, pl.stack, {makeEdgeKey(pl.shell, pl.n[0], pl.n[1])}, g));
    ExternalSupport pin;
    pin.type = ExternalSupportType::Pinned;
    ASSERT_FALSE(assignEdgeExterior(pl.p, pl.stack, {makeEdgeKey(pl.shell, pl.n[2], pl.n[3])}, pin));
    ASSERT_FALSE(assignBandForceOptions(pl.p, pl.stack, {BandForceMethod::NodalForces, false}));
    ASSERT_TRUE(project_json::validate(pl.p).empty());

    ASSERT_TRUE(project_json::isKnownSection("shell_properties"));
    EXPECT_EQ(project_json::sectionTitle("shell_properties"), "Propiedades de láminas");
    const std::string text = project_json::dumpSection(pl.p, "shell_properties");
    Project q = pl.p;
    q.shellProps = {};
    project_json::loadSection(q, "shell_properties", text);
    EXPECT_EQ(q.shellProps, pl.p.shellProps);
    EXPECT_DOUBLE_EQ(shellRecordOf(q.shellProps, pl.shell).stiffness.fxy, 0.25);
    EXPECT_EQ(shellRecordOf(q.shellProps, pl.shell).bands.size(), 1u);
    EXPECT_DOUBLE_EQ(shellRecordOf(q.shellProps, pl.shell).bands[0].x1, 1.0);  // stored ordered
    EXPECT_EQ(shellRecordOf(q.shellProps, pl.shell).bands[0].forcePoints, 37);

    // Old file: no section / empty section = defaults.
    Project old = pl.p;
    project_json::loadSection(old, "shell_properties", "{}");
    EXPECT_TRUE(old.shellProps.empty());
    // Keys missing inside a record take the defaults.
    project_json::loadSection(old, "shell_properties", "{\"shells\":[{\"shell\":1,\"triangle\":2.5}]}");
    EXPECT_DOUBLE_EQ(shellRecordOf(old.shellProps, 1).maxTriangleSideM, 2.5);
    EXPECT_TRUE(shellRecordOf(old.shellProps, 1).modalConsidered);
    EXPECT_DOUBLE_EQ(shellRecordOf(old.shellProps, 1).stiffness.fxx, 1.0);
    // A band of an old file (no "force_points") loads with the default 10.
    project_json::loadSection(old, "shell_properties", "{\"shells\":[{\"shell\":1,\"bands\":[{\"x1\":1,\"y1\":1,\"x2\":3,\"y2\":1,\"width\":0.15}]}]}");
    ASSERT_EQ(shellRecordOf(old.shellProps, 1).bands.size(), 1u);
    EXPECT_EQ(shellRecordOf(old.shellProps, 1).bands[0].forcePoints, 10);
}

TEST(ShellPropsJson, EntriesOfDeletedShellsAreNotSaved) {
    Plate pl;
    ASSERT_FALSE(assignShellDiscretisation(pl.p, pl.stack, {pl.shell}, 0.5));
    ASSERT_EQ(deleteShells(pl.p, pl.stack, {pl.shell}), ShellError::None);
    EXPECT_TRUE(pl.p.shellProps.shells.empty());  // reconciled by the shell command
    Project q = pl.p;
    q.shellProps.shells[77] = ShellRecord{.maxTriangleSideM = 2.0};
    EXPECT_EQ(project_json::dumpSection(q, "shell_properties").find("\"shell\": 77"), std::string::npos);
}

TEST(ShellPropsStructure, DeleteShellDropsTheDataAndUndoRestoresIt) {
    Plate pl;
    ASSERT_FALSE(assignShellDiscretisation(pl.p, pl.stack, {pl.shell}, 0.5));
    EdgeInteriorBond pinned;
    pinned.kind = EdgeInteriorKind::Pinned;
    ASSERT_FALSE(assignEdgeInterior(pl.p, pl.stack, {makeEdgeKey(pl.shell, pl.n[0], pl.n[1])}, pinned));
    const ShellProperties kept = pl.p.shellProps;
    ASSERT_EQ(deleteShells(pl.p, pl.stack, {pl.shell}), ShellError::None);
    EXPECT_TRUE(pl.p.shellProps.shells.empty());
    EXPECT_TRUE(pl.p.shellProps.interior.empty());
    pl.stack.undo();
    EXPECT_EQ(pl.p.shellProps, kept);
    pl.stack.redo();
    EXPECT_TRUE(pl.p.shellProps.empty());
}

TEST(ShellPropsStructure, DeleteNodesRemovesTheShellData) {
    Plate pl;
    ASSERT_FALSE(assignShellDiscretisation(pl.p, pl.stack, {pl.shell}, 0.5));
    const ShellProperties kept = pl.p.shellProps;
    ASSERT_TRUE(deleteNodes(pl.p, pl.stack, {pl.n[0]}));
    EXPECT_TRUE(pl.p.structure.shells.empty());
    EXPECT_TRUE(pl.p.shellProps.empty());
    pl.stack.undo();
    EXPECT_EQ(pl.p.shellProps, kept);
}

TEST(ShellPropsStructure, DividedShellsInheritTheRecordBandsAndEdgeBonds) {
    Plate pl;
    ShellStiffnessFactors f;
    f.mxx = 2.0;
    ASSERT_FALSE(assignShellStiffness(pl.p, pl.stack, {pl.shell}, f));
    ASSERT_FALSE(assignShellDiscretisation(pl.p, pl.stack, {pl.shell}, 0.4));
    auto bands = newBand(pl.contour(), {}, {0.5, 1}, {1.5, 1}, 0.15);
    bands = newBand(pl.contour(), bands.bands, {1, 2.5}, {3, 2.5}, 0.15);  // straddles the cut
    ASSERT_FALSE(assignShellBands(pl.p, pl.stack, {pl.shell}, bands.bands));
    ExternalSupport pin;
    pin.type = ExternalSupportType::Pinned;
    ASSERT_FALSE(assignEdgeExterior(pl.p, pl.stack, {makeEdgeKey(pl.shell, pl.n[0], pl.n[1])}, pin));
    const ShellProperties before = pl.p.shellProps;

    ASSERT_EQ(divideShells(pl.p, pl.stack, {pl.shell}, {2, -1, 0}, {2, 5, 0}), ShellError::None);
    ASSERT_EQ(pl.p.structure.shells.size(), 2u);
    int totalBands = 0;
    for (const Shell& s : pl.p.structure.shells) {
        const ShellRecord r = shellRecordOf(pl.p.shellProps, s.id);
        EXPECT_DOUBLE_EQ(r.maxTriangleSideM, 0.4);
        EXPECT_DOUBLE_EQ(r.stiffness.mxx, 2.0);
        totalBands += static_cast<int>(r.bands.size());
        for (const ShellIntegrationBand& b : r.bands) EXPECT_NEAR(b.y1, 1.0, 1e-9);  // the straddling band is gone
        // The bottom edge (y = 0) was cut at x = 2: each half inherits the exterior bond.
        int pinned = 0;
        for (const ShellEdgeKey& e : shellEdges(pl.p.structure, s.id)) {
            const Node *na = pl.p.structure.findNode(e.nodeA), *nb = pl.p.structure.findNode(e.nodeB);
            if (na->position.y == 0.0 && nb->position.y == 0.0 && edgeExteriorOf(pl.p.shellProps, e).type == ExternalSupportType::Pinned) ++pinned;
        }
        EXPECT_EQ(pinned, 1);
    }
    EXPECT_EQ(totalBands, 1);
    pl.stack.undo();
    EXPECT_EQ(pl.p.shellProps, before);
    EXPECT_EQ(pl.p.structure.shells.size(), 1u);
}

TEST(ShellPropsStructure, JoinedShellKeepsItsRecordAndGathersTheBands) {
    Plate pl;
    ASSERT_EQ(divideShells(pl.p, pl.stack, {pl.shell}, {2, -1, 0}, {2, 5, 0}), ShellError::None);
    ASSERT_EQ(pl.p.structure.shells.size(), 2u);
    const int s1 = pl.p.structure.shells[0].id, s2 = pl.p.structure.shells[1].id;
    ASSERT_FALSE(assignShellDiscretisation(pl.p, pl.stack, {s1}, 0.3));
    const auto c1 = shellContour2D(pl.p.structure, *pl.p.structure.findShell(s1));
    const auto c2 = shellContour2D(pl.p.structure, *pl.p.structure.findShell(s2));
    // One band in each half (whichever is left / right).
    // Local systems differ per shell: a short vertical band through the centre of each contour.
    auto band = [&](const std::vector<Point2>& contour) {
        Point2 c;
        for (const Point2& q : contour) c = {c.x + q.x / 4.0, c.y + q.y / 4.0};
        return newBand(contour, {}, {c.x, c.y - 0.5}, {c.x, c.y + 0.5}, 0.15);
    };
    ASSERT_TRUE(band(c1).ok());
    ASSERT_FALSE(assignShellBands(pl.p, pl.stack, {s1}, band(c1).bands));
    ASSERT_TRUE(band(c2).ok());
    ASSERT_FALSE(assignShellBands(pl.p, pl.stack, {s2}, band(c2).bands));

    int result = 0;
    ASSERT_EQ(joinShells(pl.p, pl.stack, {s1, s2}, &result), ShellError::None);
    EXPECT_EQ(result, std::min(s1, s2));
    const ShellRecord r = shellRecordOf(pl.p.shellProps, result);
    EXPECT_EQ(r.bands.size(), 2u);
    EXPECT_EQ(pl.p.shellProps.shells.size(), 1u);
    if (result == s1) EXPECT_DOUBLE_EQ(r.maxTriangleSideM, 0.3);
    pl.stack.undo();
    EXPECT_EQ(shellRecordOf(pl.p.shellProps, s1).bands.size(), 1u);
}

TEST(ShellPropsStructure, MovingACornerDropsTheBandsThatNoLongerFitAndKeepsTheOthers) {
    Plate pl;
    auto bands = newBand(pl.contour(), {}, {1, 0.5}, {3, 0.5}, 0.15);
    bands = newBand(pl.contour(), bands.bands, {1, 2.5}, {3, 2.5}, 0.15);
    ASSERT_FALSE(assignShellBands(pl.p, pl.stack, {pl.shell}, bands.bands));
    const ShellProperties before = pl.p.shellProps;
    // Corner (4,4) -> (4,2): the top edge now runs from (4,2) to (0,4); the band at y = 2.5 sticks out.
    ASSERT_TRUE(moveNode(pl.p, pl.stack, pl.n[2], {4, 2, 0}));
    const ShellRecord r = shellRecordOf(pl.p.shellProps, pl.shell);
    ASSERT_EQ(r.bands.size(), 1u);
    EXPECT_NEAR(r.bands[0].y1, 0.5, 1e-9);
    pl.stack.undo();
    EXPECT_EQ(pl.p.shellProps, before);
}

TEST(ShellPropsStructure, MovingAnUnrelatedNodeLeavesTheDataUntouched) {
    Plate pl;
    const int free = pl.p.structure.addNode({10, 10, 10});
    auto bands = newBand(pl.contour(), {}, {1, 1}, {3, 1}, 0.15);
    ASSERT_FALSE(assignShellBands(pl.p, pl.stack, {pl.shell}, bands.bands));
    ASSERT_FALSE(assignShellDiscretisation(pl.p, pl.stack, {pl.shell}, 0.7));
    const ShellProperties before = pl.p.shellProps;
    ASSERT_TRUE(moveNode(pl.p, pl.stack, free, {11, 10, 10}));
    EXPECT_EQ(pl.p.shellProps, before);
}
