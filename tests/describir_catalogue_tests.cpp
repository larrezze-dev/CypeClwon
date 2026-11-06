#include <gtest/gtest.h>

#include <cmath>

#include "core/bar_description_edit.h"
#include "core/bar_edit.h"
#include "core/describir_catalogue.h"
#include "core/predefined_profiles.h"
#include "core/profile_library.h"
#include "core/project_json.h"
#include "core/project_settings.h"

// Catalogue of the "Describir" dialog transcribed from the user's manual captures (describir_barras_capturas_manuales.md): counts,
// catalogue shapes calculated from the REA tables, timber from the designations, tie series kept out of the library, TK-063 material values.

using namespace cc::core;

namespace {

const ProfileSeries* find(const std::string& reference) {
    for (const ProfileSeries& s : describirSeries())
        if (s.reference == reference) return &s;
    return nullptr;
}

std::size_t count(SectionFamily family) {
    std::size_t n = 0;
    for (const ProfileSeries& s : describirSeries())
        if (s.family == family) n += s.profiles.size();
    return n;
}

struct CatalogueFixture {
    Project project;
    UndoStack stack;
    std::vector<int> bars;
    CatalogueFixture() {
        EXPECT_TRUE(importSeries(project, stack, importablePredefinedSeries(), ImportPolicy::KeepExisting).empty());
        const int n1 = project.structure.addNode({0, 0, 0});
        const int n2 = project.structure.addNode({4, 0, 0});
        bars = addBar(project, stack, n1, n2);
        stack.clear();
    }
};

}  // namespace

TEST(DescribirCatalogue, CountsOfTheTranscription) {
    EXPECT_EQ(count(SectionFamily::BuiltUpSteel), 85u);
    EXPECT_EQ(count(SectionFamily::ColdFormedSteel), 14u + 36u);  // U (14) + C (36, REA table)
    EXPECT_EQ(count(SectionFamily::Aluminium), 11u);
    EXPECT_EQ(count(SectionFamily::Timber), 836u);
    // Series: IPC, U, C, (EP) I, Redondo, 3 of square, 32 of rectangle, RV
    EXPECT_EQ(describirSeries().size(), 1u + 2u + 1u + 1u + 3u + 32u + 1u);
    std::size_t tie = 0;
    for (ShapeKind k : tieShapes()) tie += tieSeries(k)->profiles.size();
    EXPECT_EQ(tie, 619u);
    EXPECT_EQ(tieSeries(ShapeKind::AngleEqual)->profiles.size(), 179u);
    EXPECT_EQ(tieSeries(ShapeKind::AngleUnequal)->profiles.size(), 31u);
    EXPECT_EQ(tieSeries(ShapeKind::FlatBar)->profiles.size(), 331u);
    EXPECT_EQ(tieSeries(ShapeKind::RoundBar)->profiles.size(), 70u);
    EXPECT_EQ(tieSeries(ShapeKind::SquareBar)->profiles.size(), 8u);
    EXPECT_EQ(tieSeries(ShapeKind::FlatBar)->reference, "Pletinas");
    EXPECT_EQ(tieSeries(ShapeKind::SquareBar)->reference, "Barras cuadradas");
    EXPECT_EQ(tieSeries(ShapeKind::Generic), nullptr);
}

TEST(DescribirCatalogue, EverySeriesIsValidAndInTheCaptureOrder) {
    ProfileLibrary library;
    library.series = importablePredefinedSeries();
    EXPECT_EQ(validateLibrary(library), "");
    for (ShapeKind k : tieShapes()) EXPECT_EQ(validateSeries(*tieSeries(k)), "");
    EXPECT_EQ(describirSeries().front().reference, "IPC");
    EXPECT_EQ(find("IPC")->profiles.front().reference, "IPC 533x203x62.6");
    EXPECT_EQ(find("IPC")->profiles.back().reference, "IPC 2032x508x601.4");
    EXPECT_EQ(find("U")->profiles.back().reference, "UF-140x6");
    EXPECT_EQ(find("(EP) I")->profiles[8].reference, "I-100x100x10");
    EXPECT_EQ(find("Laminada b60")->profiles.size(), 57u);
}

TEST(DescribirCatalogue, DesignationsGiveTheDimensions) {
    const Section& ipc = find("IPC")->profiles.front();
    EXPECT_EQ(ipc.kind, ShapeKind::BuiltUpDoubleT);
    EXPECT_NEAR(ipc.dims[0], 0.5334, 1e-12);  // REA table: 21 in (the designation rounds it to 533)
    EXPECT_NEAR(ipc.dims[1], 0.2032, 1e-12);
    EXPECT_NEAR(ipc.dims[2], 0.0079, 1e-12);  // tw
    EXPECT_NEAR(ipc.dims[3], 0.0095, 1e-12);  // tf
    const Section& uf = find("U")->profiles.front();
    EXPECT_EQ(uf.dims.size(), 4u);
    EXPECT_NEAR(uf.dims[1], 0.030, 1e-12);  // b = h / 2
    EXPECT_NEAR(uf.dims[2], 0.003, 1e-12);  // t
    EXPECT_NEAR(uf.dims[3], 0.003, 1e-12);  // inner radius
    ASSERT_TRUE(uf.tabulated.area.has_value());
    EXPECT_NEAR(*uf.tabulated.area, 3.30e-4, 1e-12);
    EXPECT_NEAR(*uf.tabulated.iy, 17.5e-8, 1e-15);
    EXPECT_FALSE(uf.tabulated.iz.has_value());
    const Section& cf = find("C")->profiles.front();
    EXPECT_EQ(cf.reference, "CF-60x2.0");
    EXPECT_EQ(cf.dims.size(), 5u);
    EXPECT_NEAR(cf.dims[2], 0.015, 1e-12);  // lip
    EXPECT_FALSE(cf.tabulated.iz.has_value());  // the original does not override Izz of the lipped C (REA: flag off)
    EXPECT_EQ(find("C")->profiles.size(), 36u);
    EXPECT_EQ(find("C")->profiles.back().reference, "CF-300x4.0");
    EXPECT_NEAR(find("(EP) I")->profiles[7].dims[1], 0.0465, 1e-12);  // I-82x46.5x3
    EXPECT_NEAR(find("(EP) I")->profiles[7].dims[2], 0.003, 1e-12);
    // timber: "S-NxB" is depth x width (series "Maciza hN"), the section stores b then h
    const Section& s = find("Maciza h120")->profiles.front();  // S-120x40
    EXPECT_EQ(s.reference, "S-120x40");
    EXPECT_NEAR(s.dims[0], 0.040, 1e-12);
    EXPECT_NEAR(s.dims[1], 0.120, 1e-12);
    const Section& gl = find("Laminada b260")->profiles.back();  // GL-1280x260
    EXPECT_NEAR(gl.dims[0], 0.260, 1e-12);
    EXPECT_NEAR(gl.dims[1], 1.280, 1e-12);
    const Section& rv = find("Rectangular, canto variable")->profiles.front();
    EXPECT_EQ(rv.reference, "RV-60");
    EXPECT_EQ(rv.kind, ShapeKind::TimberRectangularVariable);
    EXPECT_NEAR(rv.dims[0], 0.060, 1e-12);
    EXPECT_NEAR(rv.dims[1], 0.100, 1e-12);
    EXPECT_NEAR(rv.dims[2], 0.100, 1e-12);
    EXPECT_EQ(find("Redondo")->profiles[6].reference, "\xC3\x98" "200");
    // ties: fractions with a point
    bool found = false;
    for (const Section& p : tieSeries(ShapeKind::RoundBar)->profiles)
        if (p.reference == "R 22.25") {
            found = true;
            EXPECT_NEAR(p.dims[0], 0.02225, 1e-12);
        }
    EXPECT_TRUE(found);
}

TEST(DescribirCatalogue, EveryCatalogueProfileIsCalculable) {
    for (const char* series : {"IPC", "U", "C", "(EP) I"})
        for (const Section& p : find(series)->profiles) {
            EXPECT_TRUE(isCatalogueShape(p.kind));
            EXPECT_EQ(validateSection(p), "") << p.reference;
            const auto e = effectiveProperties(p);
            ASSERT_TRUE(e.has_value()) << p.reference;
            EXPECT_GT(e->area, 0.0);
            EXPECT_GT(e->iy, 0.0);
            EXPECT_GT(e->iz, 0.0);
            EXPECT_GT(e->it, 0.0);
            EXPECT_GT(e->wplY, 0.0);
            EXPECT_GT(e->avz, 0.0);
            EXPECT_GT(e->avy, 0.0);
            EXPECT_TRUE(catalogueMassPerMetre(p).has_value());
        }
    Section bad = find("IPC")->profiles.front();
    bad.dims[0] = 0;
    EXPECT_NE(validateSection(bad), "");
    bad = find("IPC")->profiles.front();
    bad.dims[3] = 0.3;  // flanges thicker than half the depth
    EXPECT_NE(validateSection(bad), "");
    bad = find("U")->profiles.front();
    bad.dims.pop_back();
    EXPECT_NE(validateSection(bad), "");
    bad = find("U")->profiles.front();
    bad.dims[3] = 0.05;  // radius that does not fit
    EXPECT_NE(validateSection(bad), "");
    bad = find("(EP) I")->profiles.front();
    bad.dims[2] = 0.08;  // web as thick as the flange is wide
    EXPECT_NE(validateSection(bad), "");
    EXPECT_EQ(shapesOfFamily(SectionFamily::BuiltUpSteel), (std::vector<ShapeKind>{ShapeKind::BuiltUpDoubleT}));
    EXPECT_EQ(shapesOfFamily(SectionFamily::ColdFormedSteel), (std::vector<ShapeKind>{ShapeKind::ColdFormedU, ShapeKind::ColdFormedC}));
    EXPECT_EQ(shapesOfFamily(SectionFamily::Aluminium), (std::vector<ShapeKind>{ShapeKind::AluminiumI}));
    EXPECT_FALSE(isConcreteOrTimber(ShapeKind::AluminiumI));
    EXPECT_TRUE(isConcreteOrTimber(ShapeKind::TimberRound));
}

TEST(DescribirCatalogue, TimberProfilesAreCalculable) {
    const auto p = effectiveProperties(find("Maciza h120")->profiles.front());  // 40 x 120
    ASSERT_TRUE(p.has_value());
    EXPECT_NEAR(p->area, 0.04 * 0.12, 1e-12);
    EXPECT_NEAR(p->iy, 0.04 * 0.12 * 0.12 * 0.12 / 12, 1e-12);
    const auto round = effectiveProperties(find("Redondo")->profiles.front());
    ASSERT_TRUE(round.has_value());
    EXPECT_NEAR(round->area, 3.14159265358979 * 0.1 * 0.1 / 4, 1e-9);
}

TEST(DescribirCatalogue, LibraryJsonRoundTrip) {
    ProfileLibrary library;
    library.series = describirSeries();
    const auto back = profileLibraryFromJson(toJson(library));
    ASSERT_TRUE(back.has_value());
    EXPECT_EQ(*back, library);
}

TEST(DescribirCatalogue, DescribingWithTheCatalogueSeriesCreatesSections) {
    struct Case {
        BarMaterialIcon icon;
        ShapeKind shape;
        const char* series;
        const char* profile;
        double areaCm2;
    };
    const Case cases[] = {{BarMaterialIcon::RolledSteelFamily2, ShapeKind::BuiltUpDoubleT, "IPC", "IPC 533x203x62.6", 79.25},
                          {BarMaterialIcon::ColdFormedSteel, ShapeKind::ColdFormedU, "U", "UF-60x3", 3.30},
                          {BarMaterialIcon::ColdFormedSteel, ShapeKind::ColdFormedC, "C", "CF-60x2.0", 3.12},
                          {BarMaterialIcon::ExtrudedAluminium, ShapeKind::AluminiumI, "(EP) I", "I-50x80x6", 11.88}};
    for (const Case& k : cases) {
        CatalogueFixture f;
        BarDescription d = defaultDescription(k.icon);
        d.shape = k.shape;
        d.seriesRef = k.series;
        d.profileRef = k.profile;
        EXPECT_EQ(describeBars(f.project, f.stack, f.bars, d), "") << k.profile;
        ASSERT_EQ(f.project.barDescriptions.items.size(), 1u) << k.profile;
        const CrossSection* cs = f.project.structure.findSection(f.project.barDescriptions.items.front().sectionId);
        ASSERT_NE(cs, nullptr) << k.profile;
        EXPECT_NEAR(cs->areaM2 * 1e4, k.areaCm2, 0.006) << k.profile;
    }
}

TEST(DescribirCatalogue, DescribingTimberFromTheLibraryAndTheVariableDepth) {
    CatalogueFixture f;
    f.project.profiles.timber.strengthClass = "C24";
    BarDescription d = defaultDescription(BarMaterialIcon::Timber);
    d.inlineSection.reset();
    d.shape = ShapeKind::TimberRectangularVariable;
    d.seriesRef = "Rectangular, canto variable";
    d.profileRef = "RV-100";
    d.depthStartM = 0.1;
    d.depthEndM = 0.2;
    ASSERT_EQ(describeBars(f.project, f.stack, f.bars, d), "");
    ASSERT_EQ(f.project.barDescriptions.items.size(), 1u);
    const BarDescription& stored = f.project.barDescriptions.items.front();
    EXPECT_NEAR(stored.depthEndM, 0.2, 1e-12);
    // the section of the bar is resolved with the depths of the description
    const auto section = resolveSection(stored, f.project.profileLibrary);
    ASSERT_TRUE(section.has_value());
    EXPECT_NEAR(section->dims[1], 0.1, 1e-12);
    EXPECT_NEAR(section->dims[2], 0.2, 1e-12);
    // the project JSON keeps the depths
    const std::string text = project_json::dumpSection(f.project, "bar_descriptions");
    Project other;
    project_json::loadSection(other, "bar_descriptions", text);
    ASSERT_EQ(other.barDescriptions.items.size(), 1u);
    EXPECT_NEAR(other.barDescriptions.items.front().depthEndM, 0.2, 1e-12);
    // the material comes from the strength class of the work (EN 338, not measured in the original)
    const Material* m = f.project.structure.findMaterial(stored.materialId);
    ASSERT_NE(m, nullptr);
    EXPECT_NEAR(m->elasticModulusMPa, 11000.0, 1e-9);
}

TEST(DescribirCatalogue, ConcreteValuesAreTheExactOnesOfTheOriginal) {
    // E (MPa) of the EC2 block of datmater.dll (REA), nu 0.2, alpha 1e-5, weight 25 kN/m3
    EXPECT_NEAR(concreteMaterialData("C25/30").elasticModulusMPa, 31476.0, 1e-9);
    EXPECT_NEAR(concreteMaterialData("C12/15").elasticModulusMPa, 27085.0, 1e-9);
    EXPECT_NEAR(concreteMaterialData("C50/60").elasticModulusMPa, 37278.0, 1e-9);
    EXPECT_NEAR(concreteMaterialData("Bc 15").elasticModulusMPa, 27287.0, 1e-9);
    EXPECT_NEAR(concreteMaterialData("Bc 25").elasticModulusMPa, 30121.0, 1e-9);
    EXPECT_NEAR(concreteMaterialData("Bc 60").elasticModulusMPa, 36283.0, 1e-9);
    for (const std::string& g : concreteGrades()) {
        const MaterialData m = concreteMaterialData(g);
        EXPECT_NEAR(m.poissonRatio, 0.2, 1e-12) << g;
        EXPECT_NEAR(m.thermalExpansion, 1e-5, 1e-15) << g;
        EXPECT_NEAR(m.densityKnM3, 24.525, 1e-12) << g;
        EXPECT_GT(m.elasticModulusMPa, 27000.0) << g;
    }
    EXPECT_NEAR(shearModulusMPa(concreteMaterialData("C25/30")), 13115.0, 0.5);  // G = E / (2 (1 + nu)); 13115 in the REA table
    EXPECT_NEAR(shearModulusMPa(concreteMaterialData("C12/15")), 11285.0, 0.5);
    EXPECT_NEAR(concreteMaterialData("unknown").elasticModulusMPa, 31476.0, 1e-9);  // default = C25/30
}

TEST(DescribirCatalogue, TimberClassesOfTheFourTypes) {
    const auto& types = timberMaterials();
    EXPECT_EQ(timberStrengthClassesFor(types[0]).size(), 12u);
    EXPECT_EQ(timberStrengthClassesFor(types[1]), (std::vector<std::string>{"D18", "D24", "D30", "D35", "D40", "D50", "D60", "D70"}));
    EXPECT_EQ(timberStrengthClassesFor(types[2]).front(), "GL20h");
    EXPECT_EQ(timberStrengthClassesFor(types[2]).back(), "GL36h");
    EXPECT_EQ(timberStrengthClassesFor(types[3]).front(), "GL20c");
    EXPECT_EQ(timberStrengthClassesFor(types[3]).back(), "GL36c");
    EXPECT_TRUE(timberStrengthClassesFor("nope").empty());
    // every class of every type has properties of its own type in both sets
    const char* keys[] = {"coniferas", "frondosas", "laminada_homogenea", "laminada_combinada"};
    std::size_t total = 0;
    for (std::size_t i = 0; i < 4; ++i)
        for (const std::string& c : timberStrengthClassesFor(types[i])) {
            for (TimberPropertySet set : {TimberPropertySet::En, TimberPropertySet::CteSeM}) {
                const auto p = timberClassProperties(c, set);
                ASSERT_TRUE(p.has_value()) << c;
                EXPECT_EQ(p->type, keys[i]) << c;
                EXPECT_GT(p->e0MeanMPa, 0.0);
                EXPECT_GT(p->gMeanMPa, 0.0);
                EXPECT_GT(p->densityKgM3, 0.0);
            }
            ++total;
        }
    EXPECT_EQ(total, 36u);
    // the two sets of ec5.dll: C27 and D35 differ, C24 is the same
    EXPECT_NEAR(timberClassProperties("C27", TimberPropertySet::En)->e0MeanMPa, 12000.0, 1e-9);
    EXPECT_NEAR(timberClassProperties("C27", TimberPropertySet::CteSeM)->e0MeanMPa, 11500.0, 1e-9);
    EXPECT_NEAR(timberClassProperties("C27", TimberPropertySet::CteSeM)->gMeanMPa, 720.0, 1e-9);
    EXPECT_NEAR(timberClassProperties("D35", TimberPropertySet::En)->gMeanMPa, 650.0, 1e-9);
    EXPECT_NEAR(timberClassProperties("D35", TimberPropertySet::CteSeM)->gMeanMPa, 750.0, 1e-9);
    EXPECT_NEAR(timberClassProperties("GL24h", TimberPropertySet::CteSeM)->e0MeanMPa, 11600.0, 1e-9);
    EXPECT_NEAR(timberClassProperties("GL24c", TimberPropertySet::CteSeM)->gMeanMPa, 590.0, 1e-9);
    EXPECT_NEAR(timberClassProperties("GL36c", TimberPropertySet::En)->densityKgM3, 520.0, 1e-9);
    EXPECT_FALSE(timberClassProperties("C24")->rare);
    EXPECT_TRUE(timberClassProperties("C14")->rare);
    EXPECT_FALSE(timberClassProperties("nope").has_value());
}

TEST(DescribirCatalogue, TimberMaterialValues) {
    const auto& types = timberMaterials();
    EXPECT_NEAR(timberMaterialData(types[0], "C24").elasticModulusMPa, 11000.0, 1e-9);
    EXPECT_NEAR(timberMaterialData(types[0], "C24").densityKnM3, 420.0 * 9.81 / 1000.0, 1e-9);
    EXPECT_NEAR(timberMaterialData(types[0], "C14").elasticModulusMPa, 7000.0, 1e-9);
    EXPECT_NEAR(timberMaterialData(types[0], "C27").elasticModulusMPa, 11500.0, 1e-9);  // default set = CTE DB SE-M
    EXPECT_NEAR(timberMaterialData(types[0], "C27", TimberPropertySet::En).elasticModulusMPa, 12000.0, 1e-9);
    EXPECT_NEAR(timberMaterialData(types[1], "D30").elasticModulusMPa, 12000.0, 1e-9);
    EXPECT_NEAR(timberMaterialData(types[2], "GL28h").elasticModulusMPa, 12600.0, 1e-9);
    EXPECT_NEAR(timberMaterialData(types[3], "GL32c").elasticModulusMPa, 13700.0, 1e-9);
    EXPECT_NEAR(timberMaterialData(types[0], "C24").poissonRatio, 11000.0 / 1380.0 - 1.0, 1e-9);  // nu = E/(2G)-1 uncapped (about 6.97)
    EXPECT_NEAR(timberMaterialData(types[0], "C24").thermalExpansion, 5e-6, 1e-15);
    EXPECT_NEAR(shearModulusMPa(timberMaterialData(types[0], "C24")), 690.0, 1e-9);
    EXPECT_NEAR(timberShearModulusMPa("C24"), 690.0, 1e-9);
    EXPECT_EQ(timberShearModulusMPa("nope"), 0.0);
    // a class of another type falls back to the default
    EXPECT_NEAR(timberMaterialData(types[1], "C24").elasticModulusMPa, defaultMaterialData(BarMaterialIcon::Timber).elasticModulusMPa, 1e-9);
}

TEST(DescribirCatalogue, TimberSettingsKeepTheFireCheck) {
    TimberSettings t;
    EXPECT_FALSE(t.fireResistance);
    t.fireResistance = true;
    Project project;
    project.profiles.timber = t;
    const std::string text = project_json::dumpSection(project, "profiles");
    Project other;
    project_json::loadSection(other, "profiles", text);
    EXPECT_TRUE(other.profiles.timber.fireResistance);
}

// Validation against the dialogs of the original (describir_info_perfiles.tsv) and the REA tables (7.2 / 7.6 of the capture notes).
TEST(DescribirCatalogue, DoubleTProfilesMatchTheOriginalExactly) {
    const auto ipc = effectiveProperties(*find("IPC")->findProfile("IPC 533x203x62.6"));
    ASSERT_TRUE(ipc.has_value());
    EXPECT_NEAR(ipc->area * 1e4, 79.2456, 1e-3);
    EXPECT_NEAR(ipc->iy * 1e8, 35455.70, 0.01);
    EXPECT_NEAR(ipc->iz * 1e8, 1330.56, 0.01);
    EXPECT_NEAR(ipc->it * 1e8, 20.07, 0.01);
    EXPECT_NEAR(ipc->iw * 1e12, 912999.76, 0.5);
    EXPECT_NEAR(ipc->wplY * 1e6, 1533.94, 0.01);
    EXPECT_NEAR(ipc->wplZ * 1e6, 204.15, 0.01);
    EXPECT_NEAR(ipc->avy * 1e4, 28.96, 0.01);
    EXPECT_NEAR(ipc->avz * 1e4, 36.57, 0.01);
    EXPECT_NEAR(*catalogueMassPerMetre(*find("IPC")->findProfile("IPC 533x203x62.6")), 62.21, 0.005);  // A x 0.785, not the nominal 62.6
    // control rows of the REA table (A and Iyy tabulated equal the formulas)
    const auto big = effectiveProperties(*find("IPC")->findProfile("IPC 2032x508x601.4"));
    ASSERT_TRUE(big.has_value());
    EXPECT_NEAR(big->area * 1e4, 761.29, 0.01);
    EXPECT_NEAR(big->iy * 1e8, 5827128.96, 1.0);
    // aluminium: the 11 profiles of the REA table (A, Iyy, Izz, It, Iw, Zyy, Zzz and weight)
    struct Alu { const char* name; double a, iy, iz, it, iw, zy, zz, kg; };
    const Alu alu[] = {{"I-50x80x6", 11.88, 49.50, 51.27, 1.43, 248.14, 23.29, 19.54, 3.21},
                       {"I-50x80x8", 15.52, 59.75, 68.41, 3.31, 301.70, 29.19, 26.14, 4.19},
                       {"I-82x46.5x3", 5.07, 54.53, 5.04, 0.15, 78.70, 15.35, 3.41, 1.37},
                       {"I-100x100x10", 28.00, 449.33, 167.33, 9.33, 3388.50, 106.00, 52.00, 7.56},
                       {"I-150x200x13", 68.12, 2653.84, 1735.60, 38.37, 81438.86, 406.17, 265.24, 18.39}};
    for (const Alu& r : alu) {
        const Section& s = *find("(EP) I")->findProfile(r.name);
        const auto p = effectiveProperties(s);
        ASSERT_TRUE(p.has_value()) << r.name;
        EXPECT_NEAR(p->area * 1e4, r.a, 0.006) << r.name;
        EXPECT_NEAR(p->iy * 1e8, r.iy, 0.006) << r.name;
        EXPECT_NEAR(p->iz * 1e8, r.iz, 0.006) << r.name;
        EXPECT_NEAR(p->it * 1e8, r.it, 0.006) << r.name;
        EXPECT_NEAR(p->iw * 1e12, r.iw, 0.006) << r.name;
        EXPECT_NEAR(p->wplY * 1e6, r.zy, 0.006) << r.name;
        EXPECT_NEAR(p->wplZ * 1e6, r.zz, 0.006) << r.name;
        EXPECT_NEAR(*catalogueMassPerMetre(s), r.kg, 0.006) << r.name;
    }
}

TEST(DescribirCatalogue, ColdFormedProfilesReproduceTheOriginalThinWalledModel) {
    // UF-60x3: every value of the original (i) (REA: seccion.dll calconf_Calcular_CANAL_SIMPLE, polyline of 5-point arcs) to 2 decimals.
    const Section& uf = *find("U")->findProfile("UF-60x3");
    const auto p = effectiveProperties(uf);
    ASSERT_TRUE(p.has_value());
    EXPECT_NEAR(p->area * 1e4, 3.30, 0.005);
    EXPECT_NEAR(p->iy * 1e8, 17.50, 0.005);
    EXPECT_NEAR(p->iz * 1e8, 2.82, 0.005);
    EXPECT_NEAR(p->it * 1e8, 0.10, 0.005);
    EXPECT_NEAR((p->yCg - 0.030 / 2) * 1000.0, -6.10, 0.015);
    EXPECT_NEAR(p->avy * 1e4, 1.42, 0.005);
    EXPECT_NEAR(p->avz * 1e4, 1.42, 0.005);
    EXPECT_NEAR(p->iw * 1e12, 14.78, 0.005);
    EXPECT_NEAR(*catalogueMassPerMetre(uf), 2.59, 0.006);
    // the model reproduces the tabulated A / Iyy of every UF
    for (const Section& s : find("U")->profiles) {
        const auto calc = calculatedProperties(s);
        ASSERT_TRUE(calc.has_value()) << s.reference;
        EXPECT_NEAR(calc->area, *s.tabulated.area, *s.tabulated.area * 0.005) << s.reference;
        EXPECT_NEAR(calc->iy, *s.tabulated.iy, *s.tabulated.iy * 0.01) << s.reference;
    }
    // CF: the REA depth h is the designation (perfil3.dll checks the name against "%.0fx%.1f" of h and t); A and Iyy are tabulated and shown,
    // Izz is NOT overridden (flag off) but the model agrees with the catalogue column
    for (const Section& s : find("C")->profiles) {
        const auto calc = calculatedProperties(s);
        ASSERT_TRUE(calc.has_value()) << s.reference;
        EXPECT_NEAR(s.dims[0] * 1000.0, std::stod(s.reference.substr(3, s.reference.find('x') - 3)), 1e-9) << s.reference;
        EXPECT_NEAR(calc->area, *s.tabulated.area, *s.tabulated.area * 0.005) << s.reference;
        EXPECT_NEAR(calc->iy, *s.tabulated.iy, *s.tabulated.iy * 0.03) << s.reference;
        const auto eff = effectiveProperties(s);
        EXPECT_NEAR(eff->area, *s.tabulated.area, 1e-15) << s.reference;
        EXPECT_NEAR(eff->iy, *s.tabulated.iy, 1e-15) << s.reference;
        EXPECT_FALSE(s.tabulated.iz.has_value()) << s.reference;
        EXPECT_NEAR(eff->iz, calc->iz, 1e-15) << s.reference;
        EXPECT_GT(eff->iw, 0.0) << s.reference;
    }
}
