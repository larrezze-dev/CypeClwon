// "Asignar referencia" / "Buscar" (T-P5-10, core) and the new shell data: material, layout and reference (T-P5-09).
#include <gtest/gtest.h>

#include <nlohmann/json.hpp>

#include "core/project_json.h"
#include "core/reference_edit.h"
#include "core/shell_edit.h"
#include "core/shell_material.h"

using namespace cc::core;

namespace {
struct Fixture {
    Project p;
    UndoStack stack;
    int shell = 0;
    Fixture() {
        auto& m = p.structure;
        for (int i = 0; i < 4; ++i) m.addNode({double(i % 2) * 4, double(i / 2) * 4, 0});
        EXPECT_EQ(addShell(p, stack, {1, 2, 4, 3}, kDefaultShellThicknessM, 0, &shell), ShellError::None);
        stack.clear();
    }
};
}  // namespace

TEST(ReferenceEdit, AutomaticReferencesFollowTheMeasuredNumbering) {
    Fixture f;
    EXPECT_EQ(elementReference(f.p.structure, ReferenceKind::Node, 3), "N3");
    EXPECT_EQ(elementReference(f.p.structure, ReferenceKind::Shell, f.shell), "L1");  // the first shell is L1 in the original
    EXPECT_EQ(elementReference(f.p.structure, ReferenceKind::Node, 99), "");
}

TEST(ReferenceEdit, AssignAndUndoAndClearWhenEqualToTheAutomaticOne) {
    Fixture f;
    ASSERT_EQ(assignReferences(f.p, f.stack, ReferenceKind::Node, {1}, {"P1"}), ReferenceError::None);
    EXPECT_EQ(elementReference(f.p.structure, ReferenceKind::Node, 1), "P1");
    f.stack.undo();
    EXPECT_EQ(elementReference(f.p.structure, ReferenceKind::Node, 1), "N1");
    f.stack.redo();
    ASSERT_EQ(assignReferences(f.p, f.stack, ReferenceKind::Node, {1}, {"N1"}), ReferenceError::None);
    EXPECT_TRUE(f.p.structure.findNode(1)->reference.empty());
    EXPECT_EQ(assignReferences(f.p, f.stack, ReferenceKind::Node, {1}, {"N1"}), ReferenceError::NothingToChange);
    EXPECT_EQ(assignReferences(f.p, f.stack, ReferenceKind::Node, {1}, {"  "}), ReferenceError::EmptyReference);
    EXPECT_EQ(assignReferences(f.p, f.stack, ReferenceKind::Node, {99}, {"X"}), ReferenceError::UnknownElement);
    ASSERT_EQ(assignReferences(f.p, f.stack, ReferenceKind::Shell, {f.shell}, {"Losa 1"}), ReferenceError::None);
    EXPECT_EQ(f.p.structure.findShell(f.shell)->reference, "Losa 1");
}

TEST(ReferenceEdit, MultipleAssignsConsecutiveReferences) {
    EXPECT_EQ(consecutiveReferences("N1", 3), (std::vector<std::string>{"N1", "N2", "N3"}));
    EXPECT_EQ(consecutiveReferences("A", 3), (std::vector<std::string>{"A", "B", "C"}));
    Fixture f;
    ASSERT_EQ(assignReferences(f.p, f.stack, ReferenceKind::Node, {1, 2, 3}, consecutiveReferences("P7", 3)), ReferenceError::None);
    EXPECT_EQ(elementReference(f.p.structure, ReferenceKind::Node, 3), "P9");
    EXPECT_EQ(f.stack.count(), 1u);
}

TEST(ReferenceEdit, FindByReferenceContainsOrWholeWord) {
    Fixture f;
    assignReferences(f.p, f.stack, ReferenceKind::Node, {1, 2}, {"Pilar A", "Pilar AB"});
    EXPECT_EQ(findByReference(f.p.structure, ReferenceKind::Node, "pilar", false), (std::vector<int>{1, 2}));
    EXPECT_EQ(findByReference(f.p.structure, ReferenceKind::Node, "pilar a", true), (std::vector<int>{1}));
    EXPECT_EQ(findByReference(f.p.structure, ReferenceKind::Node, "N3", true), (std::vector<int>{3}));
    EXPECT_EQ(findByReference(f.p.structure, ReferenceKind::Node, "N", false), (std::vector<int>{3, 4}));
    EXPECT_TRUE(findByReference(f.p.structure, ReferenceKind::Node, "", false).empty());
    EXPECT_EQ(findByReference(f.p.structure, ReferenceKind::Shell, "l1", true), (std::vector<int>{f.shell}));
}

TEST(ReferenceEdit, ReferencesAreSavedAndOldFilesLoad) {
    Fixture f;
    assignReferences(f.p, f.stack, ReferenceKind::Node, {2}, {"Base"});
    Project back;
    project_json::loadSection(back, "nodes", project_json::dumpSection(f.p, "nodes"));
    EXPECT_EQ(back.structure.findNode(2)->reference, "Base");
    nlohmann::json old = nlohmann::json::parse(project_json::dumpSection(f.p, "nodes"));
    for (auto& n : old["items"]) n.erase("reference");
    Project legacy;
    project_json::loadSection(legacy, "nodes", old.dump());
    EXPECT_TRUE(legacy.structure.findNode(2)->reference.empty());
}

TEST(ShellMaterial, DefaultsAndRangesAreTheMeasuredOnes) {
    const ShellMaterial m;
    EXPECT_EQ(m.kind, ShellMaterialKind::Concrete);
    EXPECT_TRUE(m.useGeneralData);
    EXPECT_DOUBLE_EQ(m.isoElasticModulusMPa, 206010.0);
    EXPECT_DOUBLE_EQ(m.isoPoissonRatio, 0.30);
    EXPECT_DOUBLE_EQ(m.isoThermalExpansion, 0.000012);
    EXPECT_DOUBLE_EQ(m.isoDensityKnM3, 24.53);
    EXPECT_DOUBLE_EQ(m.orthoElasticModulusXMPa, 1098.72);
    EXPECT_DOUBLE_EQ(m.orthoElasticModulusYMPa, 878.98);
    EXPECT_DOUBLE_EQ(m.orthoShearModulusYZMPa, 39.73);
    EXPECT_DOUBLE_EQ(m.orthoDensityKnM3, 14.72);
    EXPECT_DOUBLE_EQ(kDefaultShellThicknessM, 0.1);
    EXPECT_DOUBLE_EQ(kDefaultShellBallastKnM3, 29430.0);
    EXPECT_EQ(rangeWarning("Espesor", kShellThicknessMm, 2), "El valor de 'Espesor' debe estar entre 0.10 y 10000.00.");
    EXPECT_EQ(rangeWarning("Z", kShellOffsetMm, 0), "El valor de 'Z' debe estar entre -5000 y 5000.");
    EXPECT_EQ(rangeWarning("Coeficiente de dilatación", kThermalExpansion, 6),
              "El valor de 'Coeficiente de dilatación' debe estar entre 0.000001 y 100.000000.");
}

TEST(ShellMaterial, ValidationUsesTheWarningsOfTheOriginal) {
    ShellMaterial m;
    EXPECT_TRUE(validateShellMaterial(m).empty());
    m.kind = ShellMaterialKind::GenericIsotropic;
    EXPECT_TRUE(validateShellMaterial(m).empty());
    m.isoPoissonRatio = 0.5;
    EXPECT_EQ(validateShellMaterial(m), "El valor de 'Módulo de Poisson' debe estar entre 0.01 y 0.49.");
    m.isoPoissonRatio = 0.3;
    m.isoDensityKnM3 = 0.0;
    EXPECT_EQ(validateShellMaterial(m), "El valor de 'Peso específico' debe estar entre 0.01 y 100000.00.");
    m.kind = ShellMaterialKind::GenericOrthotropic;  // the isotropic values are not checked for the orthotropic kind
    EXPECT_TRUE(validateShellMaterial(m).empty());
    m.orthoPoissonRatioXY = 20000.0;
    EXPECT_EQ(validateShellMaterial(m), "El valor de 'Módulo de Poisson (nu,xy)' debe estar entre -10000.00 y 10000.00.");
    m.orthoPoissonRatioXY = -0.5;  // not restrictive
    EXPECT_TRUE(validateShellMaterial(m).empty());
    m.orthoElasticModulusYMPa = 0.05;
    EXPECT_NE(validateShellMaterial(m).find("Módulo de elasticidad (Ey)"), std::string::npos);
}

TEST(ShellMaterial, NamesComeFromTheGeneralDataOrTheChoice) {
    Project p;
    ShellMaterial m;
    EXPECT_EQ(shellMaterialName(m, p.profiles), "C25/30");  // measured: the ficha shows C25/30
    m.useGeneralData = false;
    EXPECT_EQ(shellMaterialName(m, p.profiles), "C12/15");
    m.grade = "C30/37";
    EXPECT_EQ(shellMaterialName(m, p.profiles), "C30/37");
    m.kind = ShellMaterialKind::RolledSteel;
    m.useGeneralData = true;
    EXPECT_EQ(shellMaterialName(m, p.profiles), "S275 (UNE-EN 10025-2)");
    m.kind = ShellMaterialKind::ColdFormedSteel;
    EXPECT_EQ(shellMaterialName(m, p.profiles), "S235");
    m.kind = ShellMaterialKind::Aluminium;
    EXPECT_EQ(shellMaterialName(m, p.profiles), "EN AW-5083 - F");
    m.useGeneralData = false;
    m.alloy = "EN AW-6061";
    m.temper = "H112";
    EXPECT_EQ(shellMaterialName(m, p.profiles), "EN AW-6061 - H112");
    EXPECT_FALSE(isIsotropic(ShellMaterial{ShellMaterialKind::GenericOrthotropic}));
}

TEST(ShellEditNew, MaterialIsStoredPerShellUndoableAndSaved) {
    Fixture f;
    ShellMaterial m;
    m.kind = ShellMaterialKind::Aluminium;
    m.useGeneralData = false;
    m.alloy = "EN AW-6060";
    const std::size_t before = f.stack.count();
    ASSERT_EQ(setShellMaterial(f.p, f.stack, {f.shell}, m), ShellError::None);
    EXPECT_EQ(f.stack.count(), before + 1);
    EXPECT_EQ(f.p.structure.findShell(f.shell)->material.alloy, "EN AW-6060");
    f.stack.undo();
    EXPECT_EQ(f.p.structure.findShell(f.shell)->material.kind, ShellMaterialKind::Concrete);
    f.stack.redo();
    std::string message;
    ShellMaterial bad;
    bad.kind = ShellMaterialKind::GenericIsotropic;
    bad.isoElasticModulusMPa = 0.0;
    EXPECT_EQ(setShellMaterial(f.p, f.stack, {f.shell}, bad, &message), ShellError::InvalidMaterial);
    EXPECT_EQ(message, "El valor de 'Módulo de elasticidad' debe estar entre 0.10 y 10000000.00.");
    EXPECT_EQ(setShellMaterial(f.p, f.stack, {}, m), ShellError::NothingToChange);
    EXPECT_EQ(setShellMaterial(f.p, f.stack, {99}, m), ShellError::UnknownElement);
    // Round trip and an old file without the key (concrete from the general data).
    Project back;
    project_json::loadSection(back, "nodes", project_json::dumpSection(f.p, "nodes"));
    project_json::loadSection(back, "shells", project_json::dumpSection(f.p, "shells"));
    EXPECT_EQ(back.structure.findShell(f.shell)->material, m);
    nlohmann::json old = nlohmann::json::parse(project_json::dumpSection(f.p, "shells"));
    for (auto& s : old["items"]) {
        s.erase("material");
        s.erase("layout");
        s.erase("reference");
    }
    Project legacy;
    project_json::loadSection(legacy, "nodes", project_json::dumpSection(f.p, "nodes"));
    project_json::loadSection(legacy, "shells", old.dump());
    EXPECT_EQ(legacy.structure.findShell(f.shell)->material, ShellMaterial{});
    EXPECT_EQ(legacy.structure.findShell(f.shell)->layout, ShellLayout{});
}

TEST(ShellEditNew, LayoutIsRangeCheckedUndoableAndSaved) {
    Fixture f;
    EXPECT_TRUE(f.p.structure.findShell(f.shell)->layout.considerEccentricity) << "measured: on by default";
    EXPECT_DOUBLE_EQ(f.p.structure.findShell(f.shell)->layout.centralPlaneOffsetM, 0.0);
    ASSERT_EQ(setShellLayout(f.p, f.stack, {f.shell}, ShellLayout{false, 0.05}), ShellError::None);
    EXPECT_FALSE(f.p.structure.findShell(f.shell)->layout.considerEccentricity);
    EXPECT_DOUBLE_EQ(f.p.structure.findShell(f.shell)->layout.centralPlaneOffsetM, 0.05);
    ASSERT_EQ(setShellLayout(f.p, f.stack, {f.shell}, ShellLayout{true, -5.0}), ShellError::None) << "-5000 mm is the lower bound";
    ASSERT_EQ(setShellLayout(f.p, f.stack, {f.shell}, ShellLayout{true, 5.0}), ShellError::None);
    EXPECT_EQ(setShellLayout(f.p, f.stack, {f.shell}, ShellLayout{true, 5.001}), ShellError::InvalidOffset);
    EXPECT_EQ(shellErrorMessage(ShellError::InvalidOffset), "El valor de 'Z' debe estar entre -5000 y 5000.");
    EXPECT_EQ(setShellLayout(f.p, f.stack, {}, ShellLayout{}), ShellError::NothingToChange);
    f.stack.undo();
    f.stack.undo();
    EXPECT_DOUBLE_EQ(f.p.structure.findShell(f.shell)->layout.centralPlaneOffsetM, 0.05);
    Project back;
    project_json::loadSection(back, "nodes", project_json::dumpSection(f.p, "nodes"));
    project_json::loadSection(back, "shells", project_json::dumpSection(f.p, "shells"));
    EXPECT_EQ(back.structure.findShell(f.shell)->layout, (ShellLayout{false, 0.05}));
}

TEST(ShellEditNew, NewShellTakesSectionMaterialAndLayoutOfTheDialog) {
    Fixture f;
    ShellSection section{0.25, true, 5000.0, BallastBehaviour::CompressionOnly};
    ShellMaterial material;
    material.kind = ShellMaterialKind::ColdFormedSteel;
    material.useGeneralData = false;
    material.grade = "S355";
    int id = 0;
    const int n5 = f.p.structure.addNode({8, 0, 0});
    const int n6 = f.p.structure.addNode({8, 4, 0});
    ASSERT_EQ(addShell(f.p, f.stack, {2, n5, n6, 4}, 0.0, 0, &id, &section, &material, nullptr), ShellError::None);
    const Shell* s = f.p.structure.findShell(id);
    EXPECT_DOUBLE_EQ(s->thicknessM, 0.25);
    EXPECT_TRUE(s->ballastEnabled);
    EXPECT_EQ(s->ballastBehaviour, BallastBehaviour::CompressionOnly);
    EXPECT_EQ(s->material.grade, "S355");
    // Out of range values are refused before anything is created.
    ShellSection thick{20.0, false, 0.0, BallastBehaviour::Linear};
    const std::size_t count = f.stack.count();
    EXPECT_EQ(addShell(f.p, f.stack, {1, 2, 4, 3}, 0.0, 0, nullptr, &thick, nullptr, nullptr), ShellError::InvalidThickness);
    EXPECT_EQ(f.stack.count(), count);
}

TEST(ShellEditNew, SectionRangesAreTheMeasuredOnes) {
    Fixture f;
    EXPECT_EQ(setShellSection(f.p, f.stack, {f.shell}, ShellSection{0.0001, false, 0.0, BallastBehaviour::Linear}), ShellError::None) << "0.10 mm";
    EXPECT_EQ(setShellSection(f.p, f.stack, {f.shell}, ShellSection{10.0, false, 0.0, BallastBehaviour::Linear}), ShellError::None) << "10000 mm";
    EXPECT_EQ(setShellSection(f.p, f.stack, {f.shell}, ShellSection{0.00009, false, 0.0, BallastBehaviour::Linear}), ShellError::InvalidThickness);
    EXPECT_EQ(setShellSection(f.p, f.stack, {f.shell}, ShellSection{10.01, false, 0.0, BallastBehaviour::Linear}), ShellError::InvalidThickness);
    EXPECT_EQ(setShellSection(f.p, f.stack, {f.shell}, ShellSection{0.1, true, 0.005, BallastBehaviour::Linear}), ShellError::InvalidBallast);
    EXPECT_EQ(setShellSection(f.p, f.stack, {f.shell}, ShellSection{0.1, true, 1000001.0, BallastBehaviour::Linear}), ShellError::InvalidBallast);
    EXPECT_EQ(setShellSection(f.p, f.stack, {f.shell}, ShellSection{0.1, true, 1000000.0, BallastBehaviour::Linear}), ShellError::None);
    EXPECT_EQ(shellErrorMessage(ShellError::InvalidBallast), "El valor de 'Módulo de balasto en Z local' debe estar entre 0.01 y 1000000.00.");
}
