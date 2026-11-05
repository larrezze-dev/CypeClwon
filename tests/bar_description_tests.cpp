#include <gtest/gtest.h>

#include "core/bar_description_edit.h"
#include "core/bar_edit.h"
#include "core/piece_edit.h"
#include "core/predefined_profiles.h"
#include "core/profile_library.h"
#include "core/project_json.h"
#include "core/tie_document.h"

#include <nlohmann/json.hpp>

// T-P5-07 (core): description of bars ("Describir"): arrangements per shape, description + undo/redo, JSON round trip, use of a
// profile by bars and the generic section. Behaviour source: docs/research/ui/dialogs_biblioteca_perfiles.md section 2.

using namespace cc::core;

namespace {

struct Fixture {
    Project project;
    UndoStack stack;
    int n1, n2, n3;
    std::vector<int> bars;

    Fixture() {
        const auto ipe = predefinedSeries();
        std::vector<ProfileSeries> wanted;
        for (const ProfileSeries& s : ipe)
            if (s.reference == "IPE" || s.reference == "HEA") wanted.push_back(s);
        EXPECT_TRUE(importSeries(project, stack, wanted, ImportPolicy::KeepExisting).empty());
        stack.clear();
        n1 = project.structure.addNode({0, 0, 0});
        n2 = project.structure.addNode({4, 0, 0});
        n3 = project.structure.addNode({8, 0, 3});
        bars = addBar(project, stack, n1, n2);
        const auto second = addBar(project, stack, n2, n3);
        bars.insert(bars.end(), second.begin(), second.end());
        stack.clear();
    }
};

BarDescription ipe(const std::string& profile = "IPE 80", const std::string& series = "IPE") {
    BarDescription d = defaultDescription(BarMaterialIcon::RolledSteel);
    d.seriesRef = series;
    d.profileRef = profile;
    return d;
}

}  // namespace

TEST(BarDescription, ArrangementsPerShape) {
    const auto steel = arrangementsFor(BarMaterialIcon::RolledSteel, ShapeKind::IParallel);
    ASSERT_EQ(steel.size(), 9u);
    EXPECT_EQ(steel.front(), Arrangement::Simple);
    EXPECT_EQ(steel.back(), Arrangement::Boyd);
    EXPECT_EQ(arrangementsFor(BarMaterialIcon::RolledSteel, ShapeKind::Channel), std::vector<Arrangement>{Arrangement::Simple});
    EXPECT_EQ(arrangementsFor(BarMaterialIcon::RolledSteelFamily2, ShapeKind::IParallel),
              (std::vector<Arrangement>{Arrangement::Simple, Arrangement::WithConcreteSlab}));
    const auto cold = arrangementsFor(BarMaterialIcon::ColdFormedSteel, ShapeKind::IParallel);
    ASSERT_EQ(cold.size(), 7u);
    EXPECT_EQ(cold[1], Arrangement::DoubleBoxWelded);
    EXPECT_EQ(cold[6], Arrangement::DoubleIGeneric);
    EXPECT_EQ(arrangementsFor(BarMaterialIcon::Timber, ShapeKind::TimberRectangular), std::vector<Arrangement>{Arrangement::Simple});
    EXPECT_TRUE(stiffenersAllowed(BarMaterialIcon::RolledSteelFamily2, Arrangement::Simple));
    EXPECT_FALSE(stiffenersAllowed(BarMaterialIcon::RolledSteel, Arrangement::Simple));
    EXPECT_EQ(shapesFor(BarMaterialIcon::RolledSteel).size(), 8u);
    EXPECT_EQ(shapesFor(BarMaterialIcon::Timber).size(), 4u);
    EXPECT_EQ(shapesFor(BarMaterialIcon::Concrete).size(), 3u);
    EXPECT_EQ(allMaterialIcons().size(), 8u);

    const WebStiffeners s;
    EXPECT_DOUBLE_EQ(s.spacingM, 1.0);
    EXPECT_DOUBLE_EQ(s.thicknessM, 0.005);
    EXPECT_FALSE(s.present);
}

TEST(BarDescription, ValidationOfArrangementAndStiffeners) {
    Fixture f;
    BarDescription d = ipe();
    d.arrangement = Arrangement::DoubleIWelded;  // cold-formed only
    EXPECT_FALSE(validateDescription(d, f.project.profileLibrary).empty());
    d.arrangement = Arrangement::Boyd;
    EXPECT_TRUE(validateDescription(d, f.project.profileLibrary).empty());
    d.stiffeners.present = true;  // only family 2
    EXPECT_FALSE(validateDescription(d, f.project.profileLibrary).empty());
    d = ipe("IPE 999");
    EXPECT_FALSE(validateDescription(d, f.project.profileLibrary).empty());
    d = ipe("IPE 80", "NOPE");
    EXPECT_FALSE(validateDescription(d, f.project.profileLibrary).empty());
    d = ipe("HE 100 A", "HEA");
    d.materialIcon = BarMaterialIcon::ColdFormedSteel;  // series family does not match the icon
    EXPECT_FALSE(validateDescription(d, f.project.profileLibrary).empty());
}

TEST(BarDescription, DescribeUndoRedoAndSharing) {
    Fixture f;
    EXPECT_EQ(undescribedBars(f.project).size(), 2u);
    EXPECT_EQ(barDescriptionText(f.project, f.bars[0]), kNotDescribedText);

    int id = 0;
    ASSERT_EQ(describeBars(f.project, f.stack, {f.bars[0]}, ipe(), false, &id), "");
    EXPECT_EQ(f.stack.count(), 1u);
    const Bar* b = f.project.structure.findBar(f.bars[0]);
    ASSERT_NE(b, nullptr);
    EXPECT_NE(b->sectionId, 0);
    EXPECT_NE(b->materialId, 0);
    const CrossSection* cs = f.project.structure.findSection(b->sectionId);
    ASSERT_NE(cs, nullptr);
    const Section* profile = f.project.profileLibrary.find("IPE")->findProfile("IPE 80");
    const auto props = effectiveProperties(*profile);
    ASSERT_TRUE(props.has_value());
    EXPECT_DOUBLE_EQ(cs->areaM2, props->area);
    EXPECT_DOUBLE_EQ(cs->inertiaYM4, props->iy);
    EXPECT_GT(cs->areaM2, 0.0);
    const Material* m = f.project.structure.findMaterial(b->materialId);
    ASSERT_NE(m, nullptr);
    EXPECT_EQ(m->kind, MaterialKind::Steel);
    EXPECT_EQ(m->name, f.project.profiles.hotRolledSteel);
    EXPECT_EQ(barDescriptionText(f.project, f.bars[0]), "IPE 80 (S275 (UNE-EN 10025-2))");
    EXPECT_EQ(undescribedBars(f.project), std::vector<int>{f.bars[1]});
    ASSERT_NE(descriptionOfBar(f.project, f.bars[0]), nullptr);
    EXPECT_EQ(descriptionOfBar(f.project, f.bars[0])->id, id);

    // The second bar with the same description shares description, section and material.
    ASSERT_EQ(describeBars(f.project, f.stack, {f.bars[1]}, ipe(), false), "");
    EXPECT_EQ(f.project.barDescriptions.items.size(), 1u);
    EXPECT_EQ(f.project.structure.sections.size(), 1u);
    EXPECT_EQ(f.project.structure.materials.size(), 1u);
    EXPECT_EQ(f.project.structure.findBar(f.bars[1])->sectionId, b->sectionId);

    // Re-describing one of them leaves the other with the old description.
    ASSERT_EQ(describeBars(f.project, f.stack, {f.bars[0]}, ipe("HE 100 A", "HEA"), false), "");
    EXPECT_EQ(f.project.barDescriptions.items.size(), 2u);
    EXPECT_EQ(barDescriptionText(f.project, f.bars[0]), "HE 100 A (S275 (UNE-EN 10025-2))");
    EXPECT_EQ(barDescriptionText(f.project, f.bars[1]), "IPE 80 (S275 (UNE-EN 10025-2))");

    f.stack.undo();
    EXPECT_EQ(f.project.barDescriptions.items.size(), 1u);
    EXPECT_EQ(barDescriptionText(f.project, f.bars[0]), "IPE 80 (S275 (UNE-EN 10025-2))");
    f.stack.undo();
    f.stack.undo();
    EXPECT_TRUE(f.project.barDescriptions.items.empty());
    EXPECT_TRUE(f.project.structure.sections.empty());
    EXPECT_TRUE(f.project.structure.materials.empty());
    EXPECT_EQ(undescribedBars(f.project).size(), 2u);
    f.stack.redo();
    f.stack.redo();
    f.stack.redo();
    EXPECT_EQ(barDescriptionText(f.project, f.bars[0]), "HE 100 A (S275 (UNE-EN 10025-2))");
    EXPECT_EQ(project_json::validate(f.project), "");
}

TEST(BarDescription, ReplacedDescriptionIsPruned) {
    Fixture f;
    ASSERT_EQ(describeBars(f.project, f.stack, {f.bars[0]}, ipe(), false), "");
    ASSERT_EQ(describeBars(f.project, f.stack, {f.bars[0]}, ipe("IPE 100"), false), "");
    EXPECT_EQ(f.project.barDescriptions.items.size(), 1u);
    EXPECT_EQ(f.project.structure.sections.size(), 1u);
    EXPECT_EQ(f.project.structure.materials.size(), 1u);  // the identical material is reused
    EXPECT_EQ(barDescriptionText(f.project, f.bars[0]), "IPE 100 (S275 (UNE-EN 10025-2))");
}

TEST(BarDescription, RefusalsPushNothing) {
    Fixture f;
    EXPECT_FALSE(describeBars(f.project, f.stack, {}, ipe()).empty());
    EXPECT_FALSE(describeBars(f.project, f.stack, {999}, ipe()).empty());
    EXPECT_FALSE(describeBars(f.project, f.stack, {f.bars[0]}, ipe("IPE 999")).empty());
    EXPECT_EQ(f.stack.count(), 0u);
    EXPECT_TRUE(f.project.barDescriptions.items.empty());
}

TEST(BarDescription, ElementTypePropagatesToThePiece) {
    Fixture f;
    EXPECT_EQ(f.project.structure.pieceOfBar(f.bars[0]), nullptr);
    BarDescription d = ipe();
    d.elementType = ElementType::Beam;
    ASSERT_EQ(describeBars(f.project, f.stack, {f.bars[0]}, d, false), "");
    EXPECT_EQ(f.stack.count(), 1u);  // one undo step for the description and the mark
    const Piece* piece = f.project.structure.pieceOfBar(f.bars[0]);
    ASSERT_NE(piece, nullptr);
    EXPECT_EQ(piece->type, ElementType::Beam);
    f.stack.undo();
    EXPECT_EQ(f.project.structure.pieceOfBar(f.bars[0]), nullptr);
    EXPECT_EQ(undescribedBars(f.project).size(), 2u);
    f.stack.redo();
    EXPECT_EQ(f.project.structure.pieceOfBar(f.bars[0])->type, ElementType::Beam);
}

TEST(BarDescription, WholePieceIsDescribedTogether) {
    Fixture f;
    // Two aligned bars form one piece.
    Project p;
    UndoStack stack;
    const auto series = predefinedSeries();
    std::vector<ProfileSeries> ipeOnly;
    for (const ProfileSeries& s : series)
        if (s.reference == "IPE") ipeOnly.push_back(s);
    ASSERT_TRUE(importSeries(p, stack, ipeOnly, ImportPolicy::KeepExisting).empty());
    const int a = p.structure.addNode({0, 0, 0}), b = p.structure.addNode({2, 0, 0}), c = p.structure.addNode({4, 0, 0});
    const auto bars = addBeamChain(p, stack, {a, b, c});
    ASSERT_EQ(bars.size(), 2u);
    ASSERT_EQ(createPiece(p, stack, a, c), PieceError::None);
    ASSERT_EQ(describeBars(p, stack, {bars[0]}, ipe()), "");
    EXPECT_TRUE(undescribedBars(p).empty());
    EXPECT_EQ(barsOfPieces(p, {bars[1]}).size(), 2u);
}

TEST(BarDescription, JsonRoundTripAndOldFiles) {
    Fixture f;
    ASSERT_EQ(describeBars(f.project, f.stack, {f.bars[0]}, ipe(), false), "");
    BarDescription generic = defaultDescription(BarMaterialIcon::Generic);
    generic.behaviour = BehaviourKind::TensionOnly;
    ASSERT_EQ(describeBars(f.project, f.stack, {f.bars[1]}, generic, false), "");

    const std::string text = project_json::dumpSection(f.project, "bar_descriptions");
    Project loaded;
    project_json::loadSection(loaded, "bar_descriptions", text);
    EXPECT_EQ(loaded.barDescriptions, f.project.barDescriptions);
    ASSERT_EQ(loaded.barDescriptions.items.size(), 2u);
    EXPECT_EQ(loaded.barDescriptions.items[1].behaviour, BehaviourKind::TensionOnly);

    // A concrete description keeps its inline section.
    BarDescription concrete = defaultDescription(BarMaterialIcon::Concrete);
    Project q;
    q.barDescriptions.items = {concrete};
    q.barDescriptions.items[0].id = 1;
    q.barDescriptions.next = 2;
    Project q2;
    project_json::loadSection(q2, "bar_descriptions", project_json::dumpSection(q, "bar_descriptions"));
    EXPECT_EQ(q2.barDescriptions, q.barDescriptions);
    ASSERT_TRUE(q2.barDescriptions.items[0].inlineSection.has_value());

    // Old file: no section; the project is the default one and valid.
    Project old;
    EXPECT_TRUE(old.barDescriptions.items.empty());
    EXPECT_EQ(project_json::validate(old), "");
    EXPECT_TRUE(project_json::isKnownSection("bar_descriptions"));

    // A description whose section does not exist is rejected by validation.
    Project broken = f.project;
    broken.structure.sections.clear();
    EXPECT_NE(project_json::validate(broken), "");
    // Unknown enumeration name -> error.
    std::string bad = text;
    const auto at = bad.find("\"rolledSteel\"");
    ASSERT_NE(at, std::string::npos);
    bad.replace(at, 13, "\"nonsense\"");
    Project target;
    EXPECT_THROW(project_json::loadSection(target, "bar_descriptions", bad), std::exception);
}

TEST(BarDescription, UseOfProfilesAndSeries) {
    Fixture f;
    ASSERT_EQ(describeBars(f.project, f.stack, {f.bars[0]}, ipe(), false), "");
    ASSERT_EQ(describeBars(f.project, f.stack, {f.bars[1]}, ipe("HE 100 A", "HEA"), false), "");
    EXPECT_EQ(barsUsingProfile(f.project, "IPE", "IPE 80"), std::vector<int>{f.bars[0]});
    EXPECT_TRUE(barsUsingProfile(f.project, "IPE", "IPE 100").empty());
    EXPECT_EQ(barsUsingSeries(f.project, "HEA"), std::vector<int>{f.bars[1]});
    EXPECT_TRUE(barsUsingSeries(f.project, "UPN").empty());
}

TEST(BarDescription, RefreshAfterEditingTheProfile) {
    Fixture f;
    ASSERT_EQ(describeBars(f.project, f.stack, {f.bars[0]}, ipe(), false), "");
    const double before = f.project.structure.findSection(f.project.structure.findBar(f.bars[0])->sectionId)->areaM2;

    std::vector<int> unresolved;
    EXPECT_EQ(refreshDescriptions(f.project, f.stack, &unresolved), "");
    EXPECT_EQ(f.stack.count(), 1u);  // nothing changed: nothing pushed

    Section edited = *f.project.profileLibrary.find("IPE")->findProfile("IPE 80");
    edited.dims[2] *= 2.0;  // tw
    edited.tabulated = {};  // the tabulated values belong to the nominal dimensions (the edit dialog drops them as well)
    ASSERT_EQ(editProfile(f.project, f.stack, "IPE", "IPE 80", edited), "");
    EXPECT_EQ(refreshDescriptions(f.project, f.stack, &unresolved), "");
    EXPECT_TRUE(unresolved.empty());
    const double after = f.project.structure.findSection(f.project.structure.findBar(f.bars[0])->sectionId)->areaM2;
    EXPECT_GT(after, before);
    f.stack.undo();
    EXPECT_DOUBLE_EQ(f.project.structure.findSection(f.project.structure.findBar(f.bars[0])->sectionId)->areaM2, before);
}

TEST(BarDescription, GenericSection) {
    Fixture f;
    BarDescription g = defaultDescription(BarMaterialIcon::Generic);
    EXPECT_NEAR(g.generic.areaM2, 25.0e-4, 1e-12);
    EXPECT_NEAR(g.generic.shearAreaYM2, 20.83e-4, 1e-6);
    EXPECT_NEAR(g.generic.inertiaYM4, 52.08e-8, 1e-10);
    EXPECT_NEAR(g.generic.torsionM4, 87.50e-8, 1e-12);
    EXPECT_NEAR(g.generic.widthLeftM, 0.025, 1e-12);
    EXPECT_DOUBLE_EQ(g.material.elasticModulusMPa, 206010.0);
    EXPECT_DOUBLE_EQ(g.material.densityKnM3, 77.01);
    EXPECT_DOUBLE_EQ(g.material.thermalExpansion, 0.000012);
    EXPECT_EQ(arrangementsFor(g.materialIcon, g.shape), std::vector<Arrangement>{Arrangement::Simple});

    g.generic.areaM2 = 0.01;
    g.generic.shearAreaYM2 = 0.008;
    g.generic.shearAreaZM2 = 0.008;
    g.material.elasticModulusMPa = 70000.0;
    ASSERT_EQ(describeBars(f.project, f.stack, {f.bars[0]}, g, false), "");
    const Bar* bar = f.project.structure.findBar(f.bars[0]);
    const CrossSection* cs = f.project.structure.findSection(bar->sectionId);
    EXPECT_DOUBLE_EQ(cs->areaM2, 0.01);
    EXPECT_DOUBLE_EQ(cs->shearAreaYM2, 0.008);
    EXPECT_DOUBLE_EQ(cs->torsionM4, 87.5e-8);
    const Material* m = f.project.structure.findMaterial(bar->materialId);
    EXPECT_EQ(m->kind, MaterialKind::Generic);
    EXPECT_DOUBLE_EQ(m->elasticModulusMPa, 70000.0);
    EXPECT_EQ(barDescriptionText(f.project, f.bars[0]), "Genérico");

    BarDescription bad = g;
    bad.generic.shearAreaYM2 = 1.0;  // greater than the area
    EXPECT_FALSE(validateDescription(bad, f.project.profileLibrary).empty());
    bad = g;
    bad.generic.inertiaYM4 = 0.0;
    EXPECT_FALSE(validateDescription(bad, f.project.profileLibrary).empty());
    bad = g;
    bad.material.poissonRatio = 0.7;
    EXPECT_FALSE(validateDescription(bad, f.project.profileLibrary).empty());
}

TEST(BarDescription, ConcreteAndTimberInlineSections) {
    Fixture f;
    BarDescription c = defaultDescription(BarMaterialIcon::Concrete);
    ASSERT_EQ(describeBars(f.project, f.stack, {f.bars[0]}, c, false), "");
    const CrossSection* cs = f.project.structure.findSection(f.project.structure.findBar(f.bars[0])->sectionId);
    EXPECT_NEAR(cs->areaM2, 0.09, 1e-12);
    EXPECT_EQ(cs->kind, MaterialKind::Concrete);
    EXPECT_EQ(barDescriptionText(f.project, f.bars[0]), "30x30");

    BarDescription t = defaultDescription(BarMaterialIcon::Timber);
    ASSERT_EQ(describeBars(f.project, f.stack, {f.bars[1]}, t, false), "");
    EXPECT_EQ(f.project.structure.findMaterial(f.project.structure.findBar(f.bars[1])->materialId)->kind, MaterialKind::Timber);

    BarDescription wrong = c;
    wrong.inlineSection->kind = ShapeKind::TimberRectangular;
    EXPECT_FALSE(validateDescription(wrong, f.project.profileLibrary).empty());
    wrong = c;
    wrong.inlineSection->dims = {0.0, 0.3};
    EXPECT_FALSE(validateDescription(wrong, f.project.profileLibrary).empty());
}

// Labels of the bar in the 3D window and in "Perfiles utilizados" (measured in the original: IPE 80, Gen.(19.6 kg/m), 30 cm x 30 cm).
TEST(BarDescription, BarLabelText) {
    EXPECT_EQ(barLabelText(defaultDescription(BarMaterialIcon::RolledSteel)), "IPE 80");
    EXPECT_EQ(barLabelText(defaultDescription(BarMaterialIcon::Generic)), "Gen.(19.6 kg/m)");  // 25 cm2 x 77.01 kN/m3 / g
    EXPECT_EQ(barLabelText(defaultDescription(BarMaterialIcon::Concrete)), "30 cm x 30 cm");
    BarDescription round = defaultDescription(BarMaterialIcon::Timber);
    Section s;
    s.reference = "Ø100";
    s.kind = ShapeKind::TimberRound;
    s.dims = {0.10};
    round.inlineSection = s;
    EXPECT_EQ(barLabelText(round), "Ø100");
}

// --- Pilar / Viga / Tirante registers (TK-070 / TK-076; describir_barras.md section 19) ---------------------------------------------

namespace {
BarDescription pillarDescription() {
    BarDescription d = defaultDescription(BarMaterialIcon::RolledSteel);
    d.elementType = ElementType::Pillar;
    return d;
}
BarDescription beamDescription() {
    BarDescription d = defaultDescription(BarMaterialIcon::RolledSteel);
    d.elementType = ElementType::Beam;
    return d;
}
}  // namespace

TEST(BarElements, DefaultsAndRangesOfTheOriginal) {
    const PillarData p;
    EXPECT_EQ(p.material, PillarMaterial::Concrete);
    EXPECT_EQ(p.concreteKind, PillarConcreteKind::Rectangular);
    EXPECT_NEAR(p.widthXM, 0.30, 1e-12);
    EXPECT_NEAR(p.diameterM, 0.30, 1e-12);
    const BeamData b;
    EXPECT_EQ(b.family, BeamFamily::Hung);  // "Vigas descolgadas" by default
    EXPECT_EQ(b.type, 0);
    EXPECT_NEAR(b.widthM, 0.30, 1e-12);
    EXPECT_NEAR(b.flangeLeftM, 0.15, 1e-12);
    EXPECT_EQ(validatePillarData(p), "");
    EXPECT_EQ(validateBeamData(b), "");
    PillarData small = p;
    small.widthXM = 0.05;
    EXPECT_EQ(validatePillarData(small), "El valor de 'Dimensión X' debe estar entre 10 y 1000.");
    small = p;
    small.concreteKind = PillarConcreteKind::Circular;
    small.diameterM = 10.5;
    EXPECT_EQ(validatePillarData(small), "El valor de 'Diámetro' debe estar entre 10 y 1000.");
    BeamData thin;
    thin.widthM = 0.04;
    EXPECT_EQ(validateBeamData(thin), "El valor de 'Anchura' debe estar entre 5 y 500.");
}

TEST(BarElements, BeamFamilyTypesFieldsAndBelowSlabCheck) {
    EXPECT_EQ(beamTypeCount(BeamFamily::Flat), 4);
    EXPECT_EQ(beamTypeCount(BeamFamily::Hung), 8);
    EXPECT_EQ(beamTypeCount(BeamFamily::Lattice), 5);
    EXPECT_EQ(beamTypeCount(BeamFamily::Prestressed), 4);
    EXPECT_EQ(beamTypeCount(BeamFamily::Steel), 0);
    EXPECT_EQ(beamTypeCount(BeamFamily::Timber), 0);
    // «Viga bajo forjado» (#56456): only hung type 0, lattice type 4, steel and timber (REA, FUN_18005a5d0 / FUN_180059d40)
    for (int type = 0; type < 8; ++type) EXPECT_EQ(beamBelowSlabOffered(BeamFamily::Hung, type), type == 0);
    for (int type = 0; type < 5; ++type) EXPECT_EQ(beamBelowSlabOffered(BeamFamily::Lattice, type), type == 4);
    EXPECT_FALSE(beamBelowSlabOffered(BeamFamily::Flat, 0));
    EXPECT_FALSE(beamBelowSlabOffered(BeamFamily::Prestressed, 0));
    EXPECT_TRUE(beamBelowSlabOffered(BeamFamily::Steel, 0));
    EXPECT_TRUE(beamBelowSlabOffered(BeamFamily::Timber, 0));
    BeamData b;
    b.belowSlab = true;
    EXPECT_TRUE(beamIsBelowSlab(b));
    b.type = 1;  // a type without the box reads false whatever is stored (svseccion_es_viga_bajo_forjado)
    EXPECT_FALSE(beamIsBelowSlab(b));
    // fields: the rectangular hung beam shows Anchura and Canto only (measured)
    EXPECT_EQ(beamFields(BeamFamily::Hung, 0), (std::vector<BeamField>{BeamField::Width, BeamField::Depth}));
    EXPECT_EQ(beamFields(BeamFamily::Hung, 7), (std::vector<BeamField>{BeamField::Width, BeamField::DepthStart, BeamField::DepthEnd}));
    EXPECT_TRUE(beamFields(BeamFamily::Steel, 0).empty());
}

TEST(BarElements, BeamValidationsOnAccepting) {
    BeamData b;
    b.type = 3;  // hung 'T'
    b.depthM = 0.30;
    b.flangeDepthM = 0.30;  // equal fails too (#157403: depth - flange depth < 0.0001)
    EXPECT_EQ(validateBeamData(b), "El canto del ala (30.0 cm) no puede ser mayor que el canto del alma (30.0 cm).");
    b.flangeDepthM = 0.15;
    EXPECT_EQ(validateBeamData(b), "");
    b.type = 6;  // inverted 'T'
    b.flangeDepthM = 0.40;
    EXPECT_NE(validateBeamData(b), "");
    b.type = 4;  // no check for the other types
    EXPECT_EQ(validateBeamData(b), "");
    b.type = 7;  // variable depth: initial and final must differ (#404312)
    b.depthStartM = b.depthEndM = 0.30;
    EXPECT_EQ(validateBeamData(b), "Los valores 'Canto inicial' y 'Canto final' deben ser diferentes.");
    b.depthEndM = 0.40;
    EXPECT_EQ(validateBeamData(b), "");
}

TEST(BarElements, MixedPillarProfileMustFitInTheConcrete) {
    PillarData p;
    p.material = PillarMaterial::Mixed;
    p.widthXM = p.widthYM = 0.30;
    EXPECT_EQ(validatePillarData(p, 0.20, 0.10), "");
    EXPECT_EQ(validatePillarData(p, 0.40, 0.10), "El perfil metálico no está contenido dentro de la sección de hormigón.");
    p.mixedKind = MixedPillarKind::CircularEmbeddedProfile;
    p.diameterM = 0.30;
    EXPECT_EQ(validatePillarData(p, 0.25, 0.25), "El perfil metálico no está contenido dentro de la sección de hormigón.");  // diagonal 0.354
}

TEST(BarElements, ProjectionOntoTheIconModel) {
    // Pilar, concrete: a rectangular concrete section, labelled as the original labels it
    BarDescription pillar = pillarDescription();
    pillar.pillar.widthXM = 0.40;
    pillar.pillar.widthYM = 0.25;
    const auto projected = projectedDescription(pillar);
    ASSERT_TRUE(projected.has_value());
    EXPECT_EQ(projected->materialIcon, BarMaterialIcon::Concrete);
    EXPECT_EQ(barLabelText(pillar), "40 cm x 25 cm");
    pillar.pillar.concreteKind = PillarConcreteKind::Circular;
    EXPECT_EQ(projectedDescription(pillar)->inlineSection->kind, ShapeKind::ConcreteCircular);
    // steel pillar: the profile of the register
    pillar.pillar.material = PillarMaterial::Steel;
    EXPECT_EQ(projectedDescription(pillar)->materialIcon, BarMaterialIcon::RolledSteel);
    EXPECT_EQ(barLabelText(pillar), "IPE 80");
    // mixed: not resolvable yet
    pillar.pillar.material = PillarMaterial::Mixed;
    EXPECT_FALSE(isElementResolvable(pillar));
    EXPECT_EQ(validateDescription(pillar, ProfileLibrary{}), kNotAvailableText);

    BarDescription beam = beamDescription();
    beam.beam.type = 0;  // hung rectangular
    beam.beam.widthM = 0.25;
    beam.beam.depthM = 0.50;
    ASSERT_TRUE(isElementResolvable(beam));
    EXPECT_EQ(barLabelText(beam), "25 cm x 50 cm");
    beam.beam.type = 7;
    beam.beam.depthStartM = 0.5;
    beam.beam.depthEndM = 0.3;
    EXPECT_EQ(projectedDescription(beam)->inlineSection->kind, ShapeKind::ConcreteRectangularVariable);
    beam.beam.type = 3;  // 'T' beams need a shape the clone does not have
    EXPECT_FALSE(isElementResolvable(beam));
    beam.beam.family = BeamFamily::Lattice;
    beam.beam.type = 0;
    EXPECT_FALSE(isElementResolvable(beam));
    // tie: no module
    BarDescription tie = defaultDescription();
    tie.tieSelected = true;
    EXPECT_FALSE(isElementResolvable(tie));
    EXPECT_EQ(validateDescription(tie, ProfileLibrary{}), kNotAvailableText);
}

TEST(BarElements, EveryRegisterIsKeptWhenTheTypeChanges) {
    BarDescription d = defaultDescription(BarMaterialIcon::Generic);
    d.pillar.widthXM = 0.5;
    d.beam.widthM = 0.4;
    d.tie.shape = 0;
    d.elementType = ElementType::Pillar;
    d.elementType = ElementType::Beam;
    d.elementType = ElementType::Generic;
    EXPECT_NEAR(d.pillar.widthXM, 0.5, 1e-12);
    EXPECT_NEAR(d.beam.widthM, 0.4, 1e-12);
    EXPECT_EQ(d.materialIcon, BarMaterialIcon::Generic);
    // the solver sees only the active register: generic
    EXPECT_EQ(projectedDescription(d)->materialIcon, BarMaterialIcon::Generic);
}

TEST(BarElements, DescribingPillarsAndBeamsGeneratesTheirSections) {
    Fixture f;
    BarDescription pillar = pillarDescription();
    pillar.pillar.widthXM = 0.40;
    pillar.pillar.widthYM = 0.30;
    ASSERT_EQ(describeBars(f.project, f.stack, {f.bars[0]}, pillar, false), "");
    const Bar* bar = f.project.structure.findBar(f.bars[0]);
    const CrossSection* cs = f.project.structure.findSection(bar->sectionId);
    ASSERT_NE(cs, nullptr);
    EXPECT_NEAR(cs->areaM2, 0.12, 1e-12);
    EXPECT_EQ(cs->kind, MaterialKind::Concrete);
    EXPECT_EQ(f.project.structure.findMaterial(bar->materialId)->kind, MaterialKind::Concrete);
    EXPECT_EQ(descriptionOfBar(f.project, f.bars[0])->elementType, ElementType::Pillar);
    // not resolvable registers are refused and nothing is pushed
    BarDescription tie = defaultDescription();
    tie.tieSelected = true;
    const std::size_t before = f.stack.count();
    EXPECT_EQ(describeBars(f.project, f.stack, {f.bars[1]}, tie, false), kNotAvailableText);
    EXPECT_EQ(f.stack.count(), before);
    // steel beam: the library profile
    BarDescription beam = beamDescription();
    beam.beam.family = BeamFamily::Steel;
    beam.beam.seriesRef = "HEA";
    ASSERT_NE(f.project.profileLibrary.find("HEA"), nullptr);
    beam.beam.profileRef = f.project.profileLibrary.find("HEA")->profiles.front().reference;
    beam.beam.shape = ShapeKind::IParallel;
    ASSERT_EQ(describeBars(f.project, f.stack, {f.bars[1]}, beam, false), "");
    EXPECT_EQ(barDescriptionText(f.project, f.bars[1]), beam.beam.profileRef + " (" + f.project.profiles.hotRolledSteel + ")");
    beam.beam.profileRef = "HEA 999";
    EXPECT_NE(describeBars(f.project, f.stack, {f.bars[1]}, beam, false), "");  // the profile must exist in the library
}

TEST(BarElements, JsonRoundTripKeepsAllRegisters) {
    Fixture f;
    BarDescription stored = pillarDescription();
    stored.pillar.mixedKind = MixedPillarKind::CircularTube;  // kept although the concrete register is the active one
    stored.pillar.widthXM = 0.45;
    stored.beam.family = BeamFamily::Lattice;
    stored.beam.type = 4;
    stored.beam.belowSlab = true;
    stored.beam.depthStartM = 0.55;
    stored.tie.shape = 3;
    stored.tie.seriesRef = "R";
    stored.tie.profileRef = "R 10";
    ASSERT_EQ(describeBars(f.project, f.stack, {f.bars[0]}, stored, false), "");
    const std::string text = project_json::dumpSection(f.project, "bar_descriptions");
    Project loaded;
    project_json::loadSection(loaded, "bar_descriptions", text);
    ASSERT_EQ(loaded.barDescriptions.items.size(), 1u);
    const BarDescription& back = loaded.barDescriptions.items.front();
    EXPECT_EQ(back.pillar, stored.pillar);
    EXPECT_EQ(back.beam, stored.beam);
    EXPECT_EQ(back.tie, stored.tie);
    EXPECT_EQ(back.elementType, ElementType::Pillar);
    EXPECT_FALSE(back.tieSelected);
    // files saved before the registers existed load with the defaults
    Fixture g;
    ASSERT_EQ(describeBars(g.project, g.stack, {g.bars[0]}, defaultDescription(BarMaterialIcon::Generic), false), "");
    nlohmann::json stripped = nlohmann::json::parse(project_json::dumpSection(g.project, "bar_descriptions"));
    for (auto& item : stripped["items"])
        for (const char* key : {"pillar", "beam", "tie", "tieSelected"}) item.erase(key);
    Project q;
    project_json::loadSection(q, "bar_descriptions", stripped.dump());
    EXPECT_EQ(q.barDescriptions.items.front().pillar, PillarData{});
    EXPECT_EQ(q.barDescriptions.items.front().beam, BeamData{});
    EXPECT_EQ(q.barDescriptions.items.front().tie, TieData{});
}

TEST(BarElements, TieDocumentFollowsTheOriginalOrder) {
    const auto& blocks = tieDocument();
    ASSERT_FALSE(blocks.empty());
    EXPECT_EQ(blocks.front().kind, TieBlockKind::Title);
    EXPECT_EQ(blocks.front().text, "CÁLCULO DE TIRANTES EN CYPE 3D");
    int figures = 0, items = 0;
    for (const TieBlock& b : blocks) {
        if (b.kind == TieBlockKind::FigurePlaceholder) ++figures;
        if (b.kind == TieBlockKind::ListItem) ++items;
        EXPECT_EQ(b.text.find("%s"), std::string::npos) << b.id;  // every placeholder is filled
    }
    EXPECT_EQ(items, 7);  // 3 limitations + 4 steps
    EXPECT_GE(figures, 1);
    for (const TieBlock& b : blocks)
        if (b.id == 64096) EXPECT_NE(b.text.find("20 %"), std::string::npos);  // #64096 carries the 20 % limit
}
