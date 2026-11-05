#include <gtest/gtest.h>

#include <cmath>
#include <cstdio>
#include <map>
#include <string>
#include <vector>

#include "core/predefined_profiles.h"
#include "core/section_properties.h"

using namespace cc::core;

namespace {

struct RefRow {
    const char* series;
    const char* reference;
    std::vector<double> v;
};

/// Published values of the ArcelorMittal catalogue (see tests/profile_reference_data.inc for the column layout). I-type rows:
/// [0]=A cm2, [1]=Iy cm4, [2]=Wel,y cm3, [3]=Wpl,y cm3, [4]=iy, [5]=Avz cm2, [6]=Iz cm4, [7]=Wel,z, [8]=Wpl,z, [9]=iz,
/// [10]=ss, [11]=It cm4, [12]=Iw 1e9 mm6.  Angles: [0]=A cm2, [1]=Iy cm4, [2]=Wel cm3, [3]=i cm.
const std::vector<RefRow>& reference() {
    static const std::vector<RefRow> rows = {
#include "profile_reference_data.inc"
    };
    return rows;
}

constexpr double kCm2 = 1e-4, kCm3 = 1e-6, kCm4 = 1e-8, kIw = 1e-9;

double rel(double computed, double published) { return std::fabs(computed - published) / std::fabs(published); }

Section find(const std::string& series, const std::string& reference) {
    for (const ProfileSeries& s : predefinedSeries())
        if (s.reference == series)
            if (const Section* p = s.findProfile(reference)) return *p;
    ADD_FAILURE() << "profile not found: " << reference;
    return {};
}

Section mm(ShapeKind kind, std::vector<double> dimsMm) {
    Section s;
    s.reference = "x";
    s.kind = kind;
    for (double v : dimsMm) s.dims.push_back(v / 1000.0);
    return s;
}

/// Worst relative deviation of one property over a series; the test fails when it exceeds `tolerance`.
struct Worst {
    double value = 0;
    std::string where;
    void note(double deviation, const std::string& ref) {
        if (deviation > value) {
            value = deviation;
            where = ref;
        }
    }
};

}  // namespace

TEST(SectionProperties, ShapeTablesAreConsistent) {
    EXPECT_EQ(allShapes().size(), 21u);
    for (ShapeKind k : allShapes()) {
        EXPECT_EQ(shapeFromKey(shapeKey(k)), k);
        EXPECT_FALSE(dimensionKeys(k).empty());
        EXPECT_FALSE(std::string(shapeNameEs(k)).empty());
    }
    for (SectionFamily f : {SectionFamily::HotRolledSteel, SectionFamily::BuiltUpSteel, SectionFamily::ColdFormedSteel,
                            SectionFamily::Aluminium, SectionFamily::Timber, SectionFamily::Generic, SectionFamily::Concrete})
        EXPECT_EQ(familyFromKey(familyKey(f)), f);
    EXPECT_FALSE(shapeFromKey("nope").has_value());
}

TEST(SectionProperties, Ipe200AgainstPublishedValues) {
    const auto p = effectiveProperties(find("IPE", "IPE 200"));
    ASSERT_TRUE(p.has_value());
    EXPECT_NEAR(p->area, 28.5 * kCm2, 0.01 * 28.5 * kCm2);
    EXPECT_NEAR(p->iy, 1943 * kCm4, 0.005 * 1943 * kCm4);
    EXPECT_NEAR(p->iz, 142 * kCm4, 0.01 * 142 * kCm4);
    EXPECT_NEAR(p->welY, 194 * kCm3, 0.01 * 194 * kCm3);
    EXPECT_NEAR(p->wplY, 221 * kCm3, 0.01 * 221 * kCm3);
    EXPECT_NEAR(p->wplZ, 44.6 * kCm3, 0.01 * 44.6 * kCm3);
    EXPECT_NEAR(p->it, 6.98 * kCm4, 0.01 * 6.98 * kCm4);
    EXPECT_NEAR(p->iw, 13.0 * kIw, 0.02 * 13.0 * kIw);
    EXPECT_NEAR(p->avz, 14.0 * kCm2, 0.01 * 14.0 * kCm2);
    EXPECT_NEAR(p->yCg, 0.05, 1e-9);
    EXPECT_NEAR(p->zCg, 0.1, 1e-9);
}

TEST(SectionProperties, ParallelFlangeSeriesMatchPublishedTables) {
    std::map<std::string, Worst> area, iy, iz, welY, wplY, wplZ, it, iw, avz;
    int rows = 0;
    for (const RefRow& r : reference()) {
        const std::string s = r.series;
        if (s != "IPE" && s != "HEA" && s != "HEB" && s != "HEM") continue;
        const auto p = effectiveProperties(find(s, r.reference));
        ASSERT_TRUE(p.has_value()) << r.reference;
        ++rows;
        area[s].note(rel(p->area, r.v[0] * kCm2), r.reference);
        iy[s].note(rel(p->iy, r.v[1] * kCm4), r.reference);
        welY[s].note(rel(p->welY, r.v[2] * kCm3), r.reference);
        wplY[s].note(rel(p->wplY, r.v[3] * kCm3), r.reference);
        avz[s].note(rel(p->avz, r.v[5] * kCm2), r.reference);
        iz[s].note(rel(p->iz, r.v[6] * kCm4), r.reference);
        wplZ[s].note(rel(p->wplZ, r.v[8] * kCm3), r.reference);
        it[s].note(rel(p->it, r.v[11] * kCm4), r.reference);
        iw[s].note(rel(p->iw, r.v[12] * kIw), r.reference);
    }
    EXPECT_GE(rows, 70);
    for (const auto& [s, w] : area) {
        EXPECT_LT(w.value, 0.012) << s << " area " << w.where;
        EXPECT_LT(iy[s].value, 0.012) << s << " Iy " << iy[s].where;
        EXPECT_LT(iz[s].value, 0.02) << s << " Iz " << iz[s].where;
        EXPECT_LT(welY[s].value, 0.012) << s << " Wel,y " << welY[s].where;
        EXPECT_LT(wplY[s].value, 0.015) << s << " Wpl,y " << wplY[s].where;
        EXPECT_LT(wplZ[s].value, 0.02) << s << " Wpl,z " << wplZ[s].where;
        EXPECT_LT(avz[s].value, 0.03) << s << " Avz " << avz[s].where;
        EXPECT_LT(it[s].value, 0.05) << s << " It " << it[s].where;
        EXPECT_LT(iw[s].value, 0.05) << s << " Iw " << iw[s].where;
    }
}

TEST(SectionProperties, SlopedFlangeSeriesMatchPublishedTables) {
    std::map<std::string, Worst> area, iy, iz, avz, it, iw;
    for (const RefRow& r : reference()) {
        if (std::string(r.series) != "IPN" && std::string(r.series) != "UPN") continue;
        // UPN 320, 350, 380 and 400 do not follow the 8 % flange slope of the smaller channels (their published inertias fit an
        // almost parallel flange, see perfiles_datos.md): compared with a wider tolerance, ticket TK-052.
        const bool wide = r.reference == std::string("UPN 320") || r.reference == std::string("UPN 350") || r.reference == std::string("UPN 380") ||
                          r.reference == std::string("UPN 400");
        const std::string s = wide ? "UPN>=320" : r.series;
        const auto p = effectiveProperties(find(r.series, r.reference));
        ASSERT_TRUE(p.has_value()) << r.reference;
        area[s].note(rel(p->area, r.v[0] * kCm2), r.reference);
        iy[s].note(rel(p->iy, r.v[1] * kCm4), r.reference);
        iz[s].note(rel(p->iz, r.v[6] * kCm4), r.reference);
        avz[s].note(rel(p->avz, r.v[5] * kCm2), r.reference);
        it[s].note(rel(p->it, r.v[11] * kCm4), r.reference);
        iw[s].note(rel(p->iw, r.v[12] * kIw), r.reference);
    }
    for (const auto& [s, w] : area) {
        const double k = s == "UPN>=320" ? 3.0 : 1.0;  // wider tolerance for the four large channels
        EXPECT_LT(w.value, 0.015 * k) << s << " area " << w.where;
        EXPECT_LT(iy[s].value, 0.02 * k) << s << " Iy " << iy[s].where;
        EXPECT_LT(iz[s].value, 0.03 * k) << s << " Iz " << iz[s].where;
        EXPECT_LT(avz[s].value, 0.08 * k) << s << " Avz " << avz[s].where;
        EXPECT_LT(it[s].value, 0.20 * k) << s << " It " << it[s].where;  // sum of rectangles / junction formula: see perfiles_datos.md
        EXPECT_LT(iw[s].value, 0.25 * k) << s << " Iw " << iw[s].where;
    }
}

TEST(SectionProperties, EqualAnglesMatchPublishedTables) {
    Worst area, iy, wel;
    int rows = 0;
    for (const RefRow& r : reference()) {
        if (std::string(r.series) != "L") continue;
        const auto p = effectiveProperties(find("L", r.reference));
        ASSERT_TRUE(p.has_value()) << r.reference;
        ++rows;
        area.note(rel(p->area, r.v[0] * kCm2), r.reference);
        iy.note(rel(p->iy, r.v[1] * kCm4), r.reference);
        wel.note(rel(p->welY, r.v[2] * kCm3), r.reference);
        EXPECT_NEAR(p->iy, p->iz, 1e-3 * p->iy) << r.reference;  // equal legs: symmetric about the bisector
    }
    EXPECT_GE(rows, 40);
    EXPECT_LT(area.value, 0.012) << area.where;
    EXPECT_LT(iy.value, 0.02) << iy.where;
    EXPECT_LT(wel.value, 0.02) << wel.where;
}

TEST(SectionProperties, SharpEqualAngleMatchesHandFormulas) {
    const auto p = calculatedProperties(mm(ShapeKind::AngleEqual, {100, 10, 0, 0}));
    ASSERT_TRUE(p.has_value());
    EXPECT_NEAR(p->area, 1900e-6, 1e-12);
    const double a = 100, t = 10, ys = (a * a + a * t - t * t) / (2 * (2 * a - t));  // 28.68 mm
    EXPECT_NEAR(p->yCg * 1000, ys, 1e-6);
    EXPECT_NEAR(p->zCg * 1000, ys, 1e-6);
    // Iy about the centroid = t a^3/3 + (a - t) t^3 /3 - A ys^2 (axis through the base moved to the centroid)
    const double expected = (t * a * a * a + (a - t) * t * t * t) / 3 - 1900 * ys * ys;
    EXPECT_NEAR(p->iy * 1e12, expected, expected * 1e-9);
    EXPECT_NEAR(p->it, (100 + 100 - 10) * 1e-3 * std::pow(10e-3, 3) / 3, 1e-15);
}

TEST(SectionProperties, SharpTeeMatchesHandFormulas) {
    const auto p = calculatedProperties(mm(ShapeKind::Tee, {100, 120, 10, 12, 0}));
    ASSERT_TRUE(p.has_value());
    const double A = 120.0 * 12 + 88.0 * 10;
    EXPECT_NEAR(p->area * 1e6, A, 1e-6);
    const double zc = (120.0 * 12 * 94 + 88.0 * 10 * 44) / A;  // from the stem end
    EXPECT_NEAR(p->zCg * 1000, zc, 1e-6);
    const double iy = 120.0 * std::pow(12, 3) / 12 + 120.0 * 12 * std::pow(94 - zc, 2) + 10.0 * std::pow(88, 3) / 12 +
                      880.0 * std::pow(44 - zc, 2);
    EXPECT_NEAR(p->iy * 1e12, iy, iy * 1e-9);
    EXPECT_NEAR(p->iz * 1e12, 12.0 * std::pow(120, 3) / 12 + 88.0 * std::pow(10, 3) / 12, 1e-3);
}

TEST(SectionProperties, SolidShapesUseClosedForms) {
    const auto round = calculatedProperties(mm(ShapeKind::RoundBar, {100}));
    ASSERT_TRUE(round.has_value());
    EXPECT_NEAR(round->area, std::acos(-1.0) * 0.01 / 4, 1e-12);
    EXPECT_NEAR(round->iy, std::acos(-1.0) * 1e-4 / 64, 1e-15);
    EXPECT_NEAR(round->it, 2 * round->iy, 1e-15);
    EXPECT_NEAR(round->wplY, 1e-3 / 6, 1e-12);
    EXPECT_NEAR(round->avy, 0.9 * round->area, 1e-12);

    const auto square = calculatedProperties(mm(ShapeKind::SquareBar, {80}));
    ASSERT_TRUE(square.has_value());
    EXPECT_NEAR(square->iy, std::pow(0.08, 4) / 12, 1e-15);
    EXPECT_NEAR(square->it, 0.1408 * std::pow(0.08, 4), 0.002 * std::pow(0.08, 4));  // exact value 0.1406 a^4
    EXPECT_NEAR(square->wplY, std::pow(0.08, 3) / 4, 1e-12);

    const auto flat = calculatedProperties(mm(ShapeKind::FlatBar, {200, 10}));  // depth 200, width 10
    ASSERT_TRUE(flat.has_value());
    EXPECT_NEAR(flat->iy, 0.01 * std::pow(0.2, 3) / 12, 1e-15);
    EXPECT_NEAR(flat->iz, 0.2 * std::pow(0.01, 3) / 12, 1e-18);
    EXPECT_NEAR(flat->wplY, 0.01 * 0.04 / 4, 1e-12);
    EXPECT_NEAR(flat->it, 0.2 * std::pow(0.01, 3) / 3 * (1 - 0.63 * 0.05), 0.02 * 0.2 * std::pow(0.01, 3) / 3);
    EXPECT_NEAR(flat->avz, 5.0 / 6.0 * 0.002, 1e-12);
}

TEST(SectionProperties, TabulatedValuesReplaceTheCalculatedOnes) {
    Section s = find("IPE", "IPE 200");
    const auto calculated = effectiveProperties(s);
    ASSERT_TRUE(calculated.has_value());
    s.tabulated.area = 30e-4;
    s.tabulated.iy = 2000e-8;
    s.tabulated.it = 7.5e-8;
    const auto p = effectiveProperties(s);
    ASSERT_TRUE(p.has_value());
    EXPECT_DOUBLE_EQ(p->area, 30e-4);
    EXPECT_DOUBLE_EQ(p->iy, 2000e-8);
    EXPECT_DOUBLE_EQ(p->it, 7.5e-8);
    EXPECT_DOUBLE_EQ(p->iz, calculated->iz);          // not tabulated: calculated
    EXPECT_NEAR(p->welY, 2000e-8 / 0.1, 1e-15);       // Wel follows the tabulated inertia
    EXPECT_NEAR(calculatedProperties(s)->iy, calculated->iy, 1e-15);  // pure geometry ignores the table
    EXPECT_TRUE(s.tabulated.any());
}

TEST(SectionProperties, GenericSectionNeedsTabulatedValues) {
    Section g;
    g.reference = "G1";
    g.kind = ShapeKind::Generic;
    g.dims = {0.1, 0.1, 0.15, 0.25};
    EXPECT_EQ(validateSection(g), "Debe introducir el área y las inercias de la sección genérica.");
    EXPECT_FALSE(effectiveProperties(g).has_value());
    g.tabulated.area = 0.01;
    g.tabulated.iy = 1e-4;
    g.tabulated.iz = 5e-5;
    g.tabulated.avz = 0.02;  // larger than the area
    EXPECT_EQ(validateSection(g),
              "Los valores de las áreas de cortante no pueden ser mayores que el valor del área de la sección.");
    g.tabulated.avz = 0.005;
    ASSERT_EQ(validateSection(g), "");
    const auto p = effectiveProperties(g);
    ASSERT_TRUE(p.has_value());
    EXPECT_NEAR(p->welY, 1e-4 / 0.25, 1e-15);  // I / larger extreme distance
    EXPECT_NEAR(p->welZ, 5e-5 / 0.1, 1e-15);
    EXPECT_DOUBLE_EQ(p->avz, 0.005);
    EXPECT_FALSE(calculatedProperties(g).has_value());  // no geometry to compute from
}

TEST(SectionProperties, ValidationMessages) {
    Section s = mm(ShapeKind::IParallel, {200, 100, 5.6, 8.5, 12});
    EXPECT_EQ(validateSection(s), "");
    s.dims[3] = 0.1;  // flanges thicker than the half depth
    EXPECT_EQ(validateSection(s), "El canto del perfil no es compatible con el espesor de las alas.");
    s = mm(ShapeKind::IParallel, {200, 5, 5.6, 8.5, 12});
    EXPECT_EQ(validateSection(s), "La anchura del perfil no es compatible con el espesor del alma.");
    s = mm(ShapeKind::IParallel, {200, 100, 5.6, 8.5, 60});
    EXPECT_EQ(validateSection(s), "La anchura del perfil no es compatible con el espesor del alma y los radios de acuerdo de las alas.");
    s = mm(ShapeKind::IParallel, {200, 100, 5.6, 8.5});
    EXPECT_EQ(validateSection(s), "El número de dimensiones no corresponde a la forma del perfil.");
    s = mm(ShapeKind::IParallel, {200, 100, -1, 8.5, 12});
    EXPECT_EQ(validateSection(s), "Las dimensiones del perfil deben ser mayores que cero.");
    s = mm(ShapeKind::IParallel, {200, 100, 5.6, 8.5, 12});
    s.dims[0] = std::nan("");
    EXPECT_FALSE(validateSection(s).empty());
    // inclined flange whose tip thickness would be negative
    s = mm(ShapeKind::ISloped, {200, 90, 7.5, 3, 7.5, 4.5, 14});
    s.dims[6] = 14;  // slope is a percentage, not a length
    EXPECT_EQ(validateSection(s), "El espesor del ala en su extremo no es positivo; revise la pendiente de las alas.");
    s = mm(ShapeKind::AngleEqual, {50, 60, 0, 0});
    EXPECT_FALSE(validateSection(s).empty());
    EXPECT_FALSE(calculatedProperties(s).has_value());
}

TEST(SectionProperties, PredefinedSeriesHaveTheDocumentedContent) {
    const auto& all = predefinedSeries();
    ASSERT_EQ(all.size(), 7u);
    const std::vector<std::pair<std::string, std::size_t>> expected = {{"IPE", 18}, {"HEA", 24}, {"HEB", 24}, {"HEM", 24},
                                                                       {"IPN", 19}, {"UPN", 18}};
    for (std::size_t i = 0; i < expected.size(); ++i) {
        EXPECT_EQ(all[i].reference, expected[i].first);
        EXPECT_EQ(all[i].profiles.size(), expected[i].second) << all[i].reference;
    }
    EXPECT_GE(all[6].profiles.size(), 90u);
    for (const ProfileSeries& s : all)
        for (const Section& p : s.profiles) {
            EXPECT_EQ(validateSection(p), "") << p.reference;
            EXPECT_TRUE(calculatedProperties(p).has_value()) << p.reference;
        }
    // the dimensions of IPE 80 are in metres
    EXPECT_NEAR(all[0].profiles.front().dims[0], 0.08, 1e-12);
}
