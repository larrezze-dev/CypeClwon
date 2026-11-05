#include <gtest/gtest.h>

#include "core/bar_description_edit.h"
#include "core/bar_edit.h"
#include "core/piece_edit.h"
#include "core/predefined_profiles.h"
#include "core/profile_library.h"
#include "core/project_json.h"

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
