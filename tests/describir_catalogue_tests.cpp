#include <gtest/gtest.h>

#include "core/bar_description_edit.h"
#include "core/bar_edit.h"
#include "core/describir_catalogue.h"
#include "core/predefined_profiles.h"
#include "core/profile_library.h"
#include "core/project_json.h"
#include "core/project_settings.h"

// Catalogue of the "Describir" dialog transcribed from the user's manual captures (describir_barras_capturas_manuales.md): counts,
// shapes without calculable properties, timber from the designations, tie series kept out of the library, TK-063 material values.

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
    EXPECT_EQ(count(SectionFamily::ColdFormedSteel), 14u + 1u);  // U (14) + the single profile seen of C
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
    EXPECT_NEAR(ipc.dims[0], 0.533, 1e-12);
    EXPECT_NEAR(ipc.dims[1], 0.203, 1e-12);
    EXPECT_NEAR(ipc.dims[2], 62.6, 1e-12);  // mass per metre, kg/m: the only value that is not a length
    EXPECT_NEAR(find("U")->profiles.front().dims[1], 0.003, 1e-12);
    EXPECT_NEAR(find("(EP) I")->profiles[7].dims[1], 0.0465, 1e-12);  // I-82x46.5x3
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

TEST(DescribirCatalogue, ShapesWithoutGeometryHaveNoCalculatedProperties) {
    for (const char* series : {"IPC", "U", "C", "(EP) I"}) {
        const Section& p = find(series)->profiles.front();
        EXPECT_TRUE(isCatalogueOnly(p.kind));
        EXPECT_EQ(validateSection(p), "");
        EXPECT_FALSE(calculatedProperties(p).has_value());
        EXPECT_FALSE(effectiveProperties(p).has_value());
    }
    Section bad = find("IPC")->profiles.front();
    bad.dims[0] = 0;
    EXPECT_NE(validateSection(bad), "");
    Section tabulated = find("U")->profiles.front();
    tabulated.tabulated.area = 1e-3;
    EXPECT_NE(validateSection(tabulated), "");
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

TEST(DescribirCatalogue, DescribingWithAnIpcProfileIsRefusedUntilItsPropertiesAreCalculable) {
    CatalogueFixture f;
    BarDescription d = defaultDescription(BarMaterialIcon::RolledSteelFamily2);
    d.shape = ShapeKind::BuiltUpDoubleT;
    d.seriesRef = "IPC";
    d.profileRef = "IPC 533x203x62.6";
    EXPECT_EQ(describeBars(f.project, f.stack, f.bars, d),
              "Las propiedades mecánicas de los perfiles de esta serie aún no se pueden calcular.");
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

TEST(DescribirCatalogue, MaterialValuesOfTk063) {
    EXPECT_NEAR(concreteMaterialData("C25/30").elasticModulusMPa, 31000.0, 1e-9);  // EC2 table 3.1 (Ecm)
    EXPECT_NEAR(concreteMaterialData("C50/60").elasticModulusMPa, 37000.0, 1e-9);
    EXPECT_NEAR(concreteMaterialData("C25/30").poissonRatio, 0.2, 1e-12);
    EXPECT_NEAR(concreteMaterialData("Bc 25").elasticModulusMPa, defaultMaterialData(BarMaterialIcon::Concrete).elasticModulusMPa, 1e-9);
    const std::string conifers = timberMaterials().front();
    EXPECT_NEAR(timberMaterialData(conifers, "C24").elasticModulusMPa, 11000.0, 1e-9);  // EN 338
    EXPECT_NEAR(timberMaterialData(conifers, "C24").densityKnM3, 420.0 * 9.80665 / 1000.0, 1e-9);
    EXPECT_NEAR(timberMaterialData(conifers, "C14").elasticModulusMPa, 7000.0, 1e-9);
    EXPECT_NEAR(timberShearModulusMPa("C24"), 11000.0 / 16.0, 1e-9);
    EXPECT_EQ(timberShearModulusMPa("nope"), 0.0);
    EXPECT_NEAR(shearModulusMPa(concreteMaterialData("C25/30")), 31000.0 / 2.4, 1e-9);
    // other timber types: no class list yet
    EXPECT_TRUE(timberStrengthClassesFor(timberMaterials()[1]).empty());
    EXPECT_EQ(timberStrengthClassesFor(conifers).size(), 12u);
    EXPECT_NEAR(timberMaterialData(timberMaterials()[1], "D30").elasticModulusMPa,
                defaultMaterialData(BarMaterialIcon::Timber).elasticModulusMPa, 1e-9);
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
