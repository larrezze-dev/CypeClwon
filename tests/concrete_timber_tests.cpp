#include <gtest/gtest.h>

#include <cmath>
#include <numbers>

#include "core/profile_library.h"
#include "core/project_json.h"
#include "core/section_properties.h"

// T-P5-06 (core): concrete and timber sections. Expected values are textbook formulas (b h^3 / 12, pi d^4 / 64 ...) and the
// original's own rules recovered with REA (seccion.dll paramsec_*): It of a rectangle from a beta table, Av = 5/6 A (rectangle)
// and 0.9 A (circle). See docs/research/ui/perfiles_hormigon_madera.md.

using namespace cc::core;

namespace {

constexpr double kPi = std::numbers::pi;

Section make(ShapeKind kind, std::vector<double> dims, std::string ref = "S") {
    Section s;
    s.reference = std::move(ref);
    s.kind = kind;
    s.dims = std::move(dims);
    return s;
}

SectionProperties props(const Section& s) {
    const auto p = effectiveProperties(s);
    EXPECT_TRUE(p.has_value()) << validateSection(s);
    return p.value_or(SectionProperties{});
}

double near(double v) { return std::max(1e-12, std::fabs(v) * 1e-9); }

}  // namespace

TEST(ConcreteTimber, TablesAndFamilies) {
    EXPECT_EQ(allShapes().size(), 21u);
    for (ShapeKind k : allShapes()) {
        EXPECT_EQ(shapeFromKey(shapeKey(k)), k);
        EXPECT_FALSE(dimensionKeys(k).empty());
    }
    EXPECT_EQ(familyFromKey(familyKey(SectionFamily::Concrete)), SectionFamily::Concrete);
    EXPECT_STREQ(familyNameEs(SectionFamily::Concrete), "Hormigón");
    EXPECT_EQ(shapesOfFamily(SectionFamily::Concrete),
              (std::vector<ShapeKind>{ShapeKind::ConcreteCircular, ShapeKind::ConcreteRectangular, ShapeKind::ConcreteRectangularVariable}));
    EXPECT_EQ(shapesOfFamily(SectionFamily::Timber),
              (std::vector<ShapeKind>{ShapeKind::TimberRound, ShapeKind::TimberSquare, ShapeKind::TimberRectangular,
                                      ShapeKind::TimberRectangularVariable}));
    EXPECT_EQ(shapesOfFamily(SectionFamily::HotRolledSteel).size(), 9u);  // the 9 legacy shapes (Generic excluded)
    EXPECT_FALSE(shapeFamily(ShapeKind::IParallel).has_value());
    EXPECT_EQ(dimensionKeys(ShapeKind::ConcreteRectangularVariable), (std::vector<std::string>{"b", "h0", "h1"}));
    EXPECT_STREQ(shapeNameEs(ShapeKind::ConcreteRectangularVariable), "Rectangular de canto variable");  // #65245
}

TEST(ConcreteTimber, DefaultIsRectangular30x30) {
    const Section d = defaultConcreteSection();
    ASSERT_EQ(d.kind, ShapeKind::ConcreteRectangular);
    EXPECT_EQ(d.dims, (std::vector<double>{0.30, 0.30}));
    EXPECT_EQ(validateSection(d), "");
    const SectionProperties p = props(d);
    EXPECT_NEAR(p.area, 0.09, near(0.09));
    EXPECT_NEAR(p.iy, 0.3 * std::pow(0.3, 3) / 12, near(6.75e-4));
    EXPECT_NEAR(p.iz, p.iy, near(p.iy));
    EXPECT_NEAR(p.welY, std::pow(0.3, 3) / 6, near(4.5e-3));
    EXPECT_NEAR(p.welZ, std::pow(0.3, 3) / 6, near(4.5e-3));
    EXPECT_NEAR(p.wplY, std::pow(0.3, 3) / 4, near(6.75e-3));
    EXPECT_NEAR(p.it, 0.14 * std::pow(0.3, 4), near(1.134e-3));  // beta(1) = 0.14
    EXPECT_NEAR(p.avy, 5.0 / 6.0 * 0.09, near(0.075));
    EXPECT_NEAR(p.avz, 5.0 / 6.0 * 0.09, near(0.075));
    EXPECT_EQ(p.iw, 0.0);
    EXPECT_NEAR(p.yNeg, 0.15, 1e-12);
    EXPECT_NEAR(p.zPos, 0.15, 1e-12);
}

TEST(ConcreteTimber, RectangleAgainstTextbookFormulas) {
    for (ShapeKind kind : {ShapeKind::ConcreteRectangular, ShapeKind::TimberRectangular}) {
        const double b = 0.20, h = 0.40;
        const SectionProperties p = props(make(kind, {b, h}));
        EXPECT_NEAR(p.area, b * h, near(b * h));
        EXPECT_NEAR(p.iy, b * h * h * h / 12, near(p.iy));  // about y: strong axis, h is the depth along z
        EXPECT_NEAR(p.iz, h * b * b * b / 12, near(p.iz));
        EXPECT_NEAR(p.welY, b * h * h / 6, near(p.welY));
        EXPECT_NEAR(p.welZ, h * b * b / 6, near(p.welZ));
        EXPECT_NEAR(p.wplY, b * h * h / 4, near(p.wplY));
        EXPECT_NEAR(p.wplZ, h * b * b / 4, near(p.wplZ));
        EXPECT_NEAR(p.it, 0.229 * h * b * b * b, near(p.it));  // l/s = 2 -> beta = 0.229
        EXPECT_NEAR(p.avy, 5.0 / 6.0 * b * h, near(p.avy));
        EXPECT_NEAR(p.avz, 5.0 / 6.0 * b * h, near(p.avz));
        EXPECT_NEAR(p.yNeg, b / 2, 1e-12);
        EXPECT_NEAR(p.zNeg, h / 2, 1e-12);
    }
    // a wide, shallow beam: b longer than h flips the long side of the torsion formula
    const SectionProperties w = props(make(ShapeKind::ConcreteRectangular, {0.40, 0.20}));
    EXPECT_NEAR(w.it, 0.229 * 0.40 * std::pow(0.20, 3), near(w.it));
    EXPECT_NEAR(w.iy, 0.40 * std::pow(0.20, 3) / 12, near(w.iy));
}

TEST(ConcreteTimber, TorsionBetaTableOfTheOriginal) {
    // l/s on the nodes of the table (seccion.dll FUN_18001ef90) and between them (linear), unit short side
    const struct { double ratio, beta; } nodes[] = {{1.0, 0.14}, {1.25, 0.171}, {1.5, 0.196}, {2.0, 0.229},
                                                    {3.0, 0.263}, {4.0, 0.281}, {6.0, 0.299}, {10.0, 0.333}};
    for (const auto& n : nodes) {
        const SectionProperties p = props(make(ShapeKind::TimberRectangular, {n.ratio * 0.1, 0.1}));
        EXPECT_NEAR(p.it, n.beta * n.ratio * 0.1 * std::pow(0.1, 3), near(p.it)) << n.ratio;
    }
    const SectionProperties mid = props(make(ShapeKind::TimberRectangular, {0.25, 0.1}));  // l/s = 2.5 -> 0.246
    EXPECT_NEAR(mid.it, 0.246 * 0.25 * std::pow(0.1, 3), near(mid.it));
    const SectionProperties thin = props(make(ShapeKind::TimberRectangular, {1.5, 0.1}));  // l/s = 15 -> 0.333 (constant)
    EXPECT_NEAR(thin.it, 0.333 * 1.5 * std::pow(0.1, 3), near(thin.it));
    // consistency with the 5 x 5 cm generic section shown by the original (It = 87.50 cm4, Iyy = 52.08, A = 25, Av = 20.83)
    const SectionProperties g = props(make(ShapeKind::TimberSquare, {0.05}));
    EXPECT_NEAR(g.it, 87.50e-8, 1e-12);
    EXPECT_NEAR(g.iy, 52.08e-8, 0.01e-8);
    EXPECT_NEAR(g.area, 25.0e-4, 1e-12);
    EXPECT_NEAR(g.avy, 20.83e-4, 0.01e-4);
}

TEST(ConcreteTimber, CircleAgainstTextbookFormulas) {
    for (ShapeKind kind : {ShapeKind::ConcreteCircular, ShapeKind::TimberRound}) {
        const double d = 0.40;
        const SectionProperties p = props(make(kind, {d}));
        EXPECT_NEAR(p.area, kPi * d * d / 4, near(p.area));
        EXPECT_NEAR(p.iy, kPi * std::pow(d, 4) / 64, near(p.iy));
        EXPECT_NEAR(p.iz, p.iy, near(p.iy));
        EXPECT_NEAR(p.it, kPi * std::pow(d, 4) / 32, near(p.it));
        EXPECT_NEAR(p.welY, kPi * std::pow(d, 3) / 32, near(p.welY));
        EXPECT_NEAR(p.welZ, p.welY, near(p.welY));
        EXPECT_NEAR(p.wplY, std::pow(d, 3) / 6, near(p.wplY));
        EXPECT_NEAR(p.avy, 0.9 * p.area, near(p.area));
        EXPECT_NEAR(p.avz, 0.9 * p.area, near(p.area));
        EXPECT_NEAR(p.yPos, d / 2, 1e-12);
    }
}

TEST(ConcreteTimber, SquareEqualsRectangle) {
    const SectionProperties sq = props(make(ShapeKind::TimberSquare, {0.12}));
    const SectionProperties re = props(make(ShapeKind::TimberRectangular, {0.12, 0.12}));
    EXPECT_EQ(sq.area, re.area);
    EXPECT_EQ(sq.iy, re.iy);
    EXPECT_EQ(sq.it, re.it);
    EXPECT_EQ(sq.avz, re.avz);
}

TEST(ConcreteTimber, VariableDepthIsEvaluatedAlongThePiece) {
    for (ShapeKind kind : {ShapeKind::ConcreteRectangularVariable, ShapeKind::TimberRectangularVariable}) {
        const Section s = make(kind, {0.30, 0.30, 0.50});
        ASSERT_EQ(validateSection(s), "");
        const Section constant = make(shapeFamily(kind) == SectionFamily::Concrete ? ShapeKind::ConcreteRectangular : ShapeKind::TimberRectangular,
                                      {0.30, 0.40});
        const auto start = propertiesAt(s, 0.0), mid = propertiesAt(s, 0.5), end = propertiesAt(s, 1.0);
        ASSERT_TRUE(start && mid && end);
        EXPECT_NEAR(start->iy, 0.30 * std::pow(0.30, 3) / 12, near(start->iy));
        EXPECT_NEAR(end->iy, 0.30 * std::pow(0.50, 3) / 12, near(end->iy));
        EXPECT_NEAR(mid->area, 0.30 * 0.40, near(mid->area));
        EXPECT_EQ(mid->iy, props(constant).iy);
        EXPECT_NEAR(end->zPos, 0.25, 1e-12);
        // without a position the initial end is used
        EXPECT_EQ(effectiveProperties(s)->iy, start->iy);
        EXPECT_EQ(calculatedProperties(s)->iy, start->iy);
        // the equivalent constant section
        const auto at = sectionAt(s, 0.25);
        ASSERT_TRUE(at.has_value());
        EXPECT_FALSE(isVariableDepth(at->kind));
        EXPECT_NEAR(at->dims[1], 0.35, 1e-12);
        // position outside the piece
        EXPECT_FALSE(propertiesAt(s, -0.01).has_value());
        EXPECT_FALSE(propertiesAt(s, 1.01).has_value());
        EXPECT_FALSE(propertiesAt(s, std::nan("")).has_value());
    }
    // a decreasing depth works too
    const auto shrinking = propertiesAt(make(ShapeKind::TimberRectangularVariable, {0.1, 0.4, 0.2}), 1.0);
    ASSERT_TRUE(shrinking.has_value());
    EXPECT_NEAR(shrinking->area, 0.1 * 0.2, 1e-12);
    // constant sections pass through sectionAt unchanged
    const Section c = make(ShapeKind::TimberRound, {0.2});
    EXPECT_EQ(sectionAt(c, 0.3), c);
}

TEST(ConcreteTimber, Validation) {
    EXPECT_EQ(validateSection(make(ShapeKind::ConcreteRectangular, {0.3, 0.3})), "");
    const std::string positive = "Las dimensiones del perfil deben ser mayores que cero.";
    EXPECT_EQ(validateSection(make(ShapeKind::ConcreteRectangular, {0.0, 0.3})), positive);
    EXPECT_EQ(validateSection(make(ShapeKind::ConcreteRectangular, {0.3, -0.3})), positive);
    EXPECT_EQ(validateSection(make(ShapeKind::ConcreteRectangular, {0.3, 5e-6})), positive);  // below the original's 1e-5 limit
    EXPECT_EQ(validateSection(make(ShapeKind::ConcreteCircular, {0.0})), positive);
    EXPECT_EQ(validateSection(make(ShapeKind::TimberRectangularVariable, {0.1, 0.3, 0.0})), positive);
    EXPECT_NE(validateSection(make(ShapeKind::TimberSquare, {std::nan("")})), "");
    EXPECT_NE(validateSection(make(ShapeKind::TimberRound, {std::numeric_limits<double>::infinity()})), "");
    EXPECT_EQ(validateSection(make(ShapeKind::ConcreteRectangular, {0.3})), "El número de dimensiones no corresponde a la forma del perfil.");
    Section tab = make(ShapeKind::TimberRound, {0.2});
    tab.tabulated.area = 0.03;
    EXPECT_EQ(validateSection(tab), "Las secciones de hormigón y madera no admiten valores tabulados.");
    EXPECT_FALSE(effectiveProperties(make(ShapeKind::ConcreteRectangular, {0.0, 0.3})).has_value());
    EXPECT_FALSE(propertiesAt(make(ShapeKind::ConcreteRectangularVariable, {0.3, 0.3, -1.0}), 0.5).has_value());
}

namespace {

ProfileSeries concreteSeries(const std::string& ref) {
    ProfileSeries s;
    s.reference = ref;
    s.description = "Hormigón rectangular";
    s.family = SectionFamily::Concrete;
    s.kind = ShapeKind::ConcreteRectangular;
    s.profiles = {defaultConcreteSection(), make(ShapeKind::ConcreteRectangular, {0.25, 0.50}, "25x50")};
    return s;
}

ProfileSeries timberSeries(const std::string& ref) {
    ProfileSeries s;
    s.reference = ref;
    s.family = SectionFamily::Timber;
    s.kind = ShapeKind::TimberRectangularVariable;
    s.profiles = {make(ShapeKind::TimberRectangularVariable, {0.10, 0.20, 0.40}, "10x20-40")};
    return s;
}

}  // namespace

TEST(ConcreteTimber, SeriesFamilyIsChecked) {
    EXPECT_EQ(validateSeries(concreteSeries("HA")), "");
    ProfileSeries wrong = concreteSeries("HA");
    wrong.family = SectionFamily::HotRolledSteel;
    EXPECT_EQ(validateSeries(wrong), "La forma 'Rectangular' solo puede pertenecer a una serie de hormigón.");
    ProfileSeries mixed = timberSeries("MD");
    mixed.profiles.push_back(make(ShapeKind::TimberRectangular, {0.1, 0.2}, "x"));
    EXPECT_NE(validateSeries(mixed), "");  // profile shape differs from the series
    ProfileSeries bad = concreteSeries("HA");
    bad.profiles.push_back(make(ShapeKind::ConcreteRectangular, {0.0, 0.2}, "zero"));
    EXPECT_EQ(validateSeries(bad), "Perfil 'zero': Las dimensiones del perfil deben ser mayores que cero.");
}

TEST(ConcreteTimber, LibraryUndoAndProjectJson) {
    Project project;
    UndoStack stack;
    ASSERT_EQ(addSeries(project, stack, concreteSeries("HA")), "");
    ASSERT_EQ(addSeries(project, stack, timberSeries("MD")), "");
    EXPECT_EQ(addProfile(project, stack, "HA", make(ShapeKind::ConcreteRectangular, {0.4, 0.4}, "40x40")), "");
    EXPECT_NE(addProfile(project, stack, "HA", make(ShapeKind::ConcreteCircular, {0.4}, "d40")), "");  // wrong shape for the series
    EXPECT_EQ(editProfile(project, stack, "HA", "40x40", make(ShapeKind::ConcreteRectangular, {0.45, 0.45}, "40x40")), "");
    ASSERT_EQ(project.profileLibrary.find("HA")->profiles.size(), 3u);
    EXPECT_EQ(project_json::validate(project), "");

    Project loaded;
    project_json::loadSection(loaded, "profile_library", project_json::dumpSection(project, "profile_library"));
    EXPECT_EQ(loaded.profileLibrary, project.profileLibrary);
    const Section* md = loaded.profileLibrary.find("MD")->findProfile("10x20-40");
    ASSERT_NE(md, nullptr);
    EXPECT_EQ(md->kind, ShapeKind::TimberRectangularVariable);
    EXPECT_EQ(loaded.profileLibrary.find("HA")->family, SectionFamily::Concrete);
    EXPECT_NEAR(propertiesAt(*md, 1.0)->area, 0.10 * 0.40, 1e-12);

    stack.undo();  // edit
    EXPECT_NEAR(project.profileLibrary.find("HA")->findProfile("40x40")->dims[0], 0.4, 1e-12);
    stack.undo();  // add profile
    stack.undo();  // add timber series
    EXPECT_EQ(project.profileLibrary.find("MD"), nullptr);
    stack.undo();  // add concrete series
    EXPECT_TRUE(project.profileLibrary.series.empty());
    stack.redo();
    EXPECT_NE(project.profileLibrary.find("HA"), nullptr);
}
