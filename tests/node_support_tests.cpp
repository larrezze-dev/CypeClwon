#include <gtest/gtest.h>

#include <cmath>

#include "core/node_edit.h"
#include "core/node_support_edit.h"
#include "core/project_json.h"

using namespace cc::core;

namespace {

using Dofs = std::array<bool, 6>;

Dofs dofs(const ExternalSupport& s) { return restraintOf(s).fixed; }
ExternalSupport of(ExternalSupportType t) {
    ExternalSupport s;
    s.type = t;
    return s;
}

Project twoNodes() {
    Project p;
    p.structure.addNode({0, 0, 0});
    p.structure.addNode({0, 0, 3});
    return p;
}

}  // namespace

TEST(NodeSupport, TwentyTypesWithOriginalIndices) {
    EXPECT_EQ(allExternalSupportTypes().size(), 20u);
    EXPECT_EQ(supportTypeIndex(ExternalSupportType::Free), 0);
    EXPECT_EQ(supportTypeIndex(ExternalSupportType::Pinned), 1);
    EXPECT_EQ(supportTypeIndex(ExternalSupportType::Generic), 2);
    EXPECT_EQ(supportTypeIndex(ExternalSupportType::Fixed), 3);
    EXPECT_EQ(supportTypeIndex(ExternalSupportType::LineUserFixed), 7);
    EXPECT_EQ(supportTypeIndex(ExternalSupportType::PlaneUserFixed), 11);
    EXPECT_EQ(supportTypeIndex(ExternalSupportType::LineXPinned), 12);
    EXPECT_EQ(supportTypeIndex(ExternalSupportType::PlaneUserPinned), 19);
}

TEST(NodeSupport, BasicTypes) {
    EXPECT_EQ(dofs(of(ExternalSupportType::Free)), (Dofs{}));
    EXPECT_EQ(dofs(of(ExternalSupportType::Pinned)), (Dofs{true, true, true, false, false, false}));
    EXPECT_EQ(dofs(of(ExternalSupportType::Fixed)), (Dofs{true, true, true, true, true, true}));
}

TEST(NodeSupport, LineAndPlaneAxisTypes) {
    // Line along X: Dx free, Dy and Dz blocked. Fixed family blocks the rotations, pinned family leaves them free.
    EXPECT_EQ(dofs(of(ExternalSupportType::LineXFixed)), (Dofs{false, true, true, true, true, true}));
    EXPECT_EQ(dofs(of(ExternalSupportType::LineYFixed)), (Dofs{true, false, true, true, true, true}));
    EXPECT_EQ(dofs(of(ExternalSupportType::LineZPinned)), (Dofs{true, true, false, false, false, false}));
    // Plane XY: Dx, Dy free, Dz blocked.
    EXPECT_EQ(dofs(of(ExternalSupportType::PlaneXYFixed)), (Dofs{false, false, true, true, true, true}));
    EXPECT_EQ(dofs(of(ExternalSupportType::PlaneXZPinned)), (Dofs{false, true, false, false, false, false}));
    EXPECT_EQ(dofs(of(ExternalSupportType::PlaneYZPinned)), (Dofs{true, false, false, false, false, false}));
}

TEST(NodeSupport, EveryTypeIsConsistentWithItsFamily) {
    for (ExternalSupportType t : allExternalSupportTypes()) {
        const SupportTypeInfo info = supportTypeInfo(t);
        const Dofs d = dofs(of(t));
        if (info.kind == SupportKind::Generic || info.kind == SupportKind::Free) continue;
        // Rotations are blocked together or not at all, following the family.
        for (int i = 3; i < 6; ++i) EXPECT_EQ(d[i], info.rotationsFixed) << supportTypeIndex(t);
    }
}

TEST(NodeSupport, UserVectorParallelToAxisReducesToAxisType) {
    ExternalSupport s = of(ExternalSupportType::LineUserFixed);
    s.userVector = {0, -2, 0};
    EXPECT_EQ(restraintOf(s).oblique, Restraint::Oblique::None);
    EXPECT_EQ(dofs(s), dofs(of(ExternalSupportType::LineYFixed)));
    s.type = ExternalSupportType::PlaneUserPinned;
    s.userVector = {0, 0, 5};
    EXPECT_EQ(dofs(s), dofs(of(ExternalSupportType::PlaneXYPinned)));
}

TEST(NodeSupport, SkewUserVectorIsOblique) {
    ExternalSupport s = of(ExternalSupportType::LineUserPinned);
    s.userVector = {1, 1, 0};
    const Restraint r = restraintOf(s);
    EXPECT_EQ(r.oblique, Restraint::Oblique::LineFree);
    EXPECT_NEAR(r.direction.x, std::sqrt(0.5), 1e-12);
    EXPECT_FALSE(r.fixed[0] || r.fixed[1] || r.fixed[2]);
    EXPECT_TRUE(r.any());
    s.type = ExternalSupportType::PlaneUserFixed;
    EXPECT_EQ(restraintOf(s).oblique, Restraint::Oblique::PlaneFree);
    EXPECT_TRUE(restraintOf(s).fixed[5]);  // fixed family blocks rotations
}

TEST(NodeSupport, ElasticCheckboxTurnsBlockedDofsIntoSprings) {
    ExternalSupport s = of(ExternalSupportType::Pinned);
    s.elasticDofs = {true, false, true, true, true, true};  // Dy stays "Fijo"; the rotations are not blocked, so they cannot be springs
    const Restraint r = restraintOf(s);
    EXPECT_EQ(r.elastic, (Dofs{true, false, true, false, false, false}));
    EXPECT_DOUBLE_EQ(kDefaultElasticConstant, 100000.0);
    for (SupportSense sense : s.senses) EXPECT_EQ(sense, SupportSense::Both);
}

TEST(NodeSupport, GenericIndependent) {
    ExternalSupport s = of(ExternalSupportType::Generic);
    s.generic.translations = {DofMode::Fixed, DofMode::Elastic, DofMode::Free};
    s.generic.rotations = {DofMode::Free, DofMode::Free, DofMode::Fixed};
    const Restraint r = restraintOf(s);
    EXPECT_EQ(r.fixed, (Dofs{true, true, false, false, false, true}));
    EXPECT_EQ(r.elastic, (Dofs{false, true, false, false, false, false}));
}

TEST(NodeSupport, GenericOnLineAndPlane) {
    ExternalSupport s = of(ExternalSupportType::Generic);
    s.generic.mode = GenericDisplacementMode::OnLine;
    s.generic.vector = {1, 0, 0};
    EXPECT_EQ(dofs(s), (Dofs{false, true, true, false, false, false}));
    s.generic.mode = GenericDisplacementMode::OnPlane;
    s.generic.vector = {1, 0, 0};
    EXPECT_EQ(dofs(s), (Dofs{true, false, false, false, false, false}));
    s.generic.vector = {1, 2, 3};
    EXPECT_EQ(restraintOf(s).oblique, Restraint::Oblique::PlaneFree);
}

TEST(NodeSupport, GenericWithEverythingFreeIsFreeSilently) {
    // Measured: the original closes the dialog without a message and the node stays free (#65006 was not reproduced).
    ExternalSupport s = of(ExternalSupportType::Generic);
    EXPECT_TRUE(checkExternalSupport(s).empty());
    EXPECT_EQ(supportMessage(65006), "Debe coaccionar, al menos, un grado de libertad del nudo.");
    Project p = twoNodes();
    UndoStack stack;
    ASSERT_EQ(assignExternalSupport(p, stack, {1}, of(ExternalSupportType::Fixed)), 1);
    EXPECT_EQ(assignExternalSupport(p, stack, {1}, s), 1);  // turns a restrained node into a free one
    EXPECT_TRUE(p.nodeSupports.empty());
    s.generic.rotations[1] = DofMode::Fixed;
    EXPECT_EQ(assignExternalSupport(p, stack, {2}, s), 1);
    EXPECT_EQ(p.nodeSupports.external.size(), 1u);
}

TEST(NodeSupport, ValidationUserVectorAndSpringConstant) {
    ExternalSupport s = of(ExternalSupportType::LineUserFixed);
    s.userVector = {0, 0, 0};
    auto vectorIssues = checkExternalSupport(s);
    ASSERT_EQ(vectorIssues.size(), 1u);
    EXPECT_EQ(vectorIssues[0].text, "La dirección introducida no es correcta.");
    s.userVector = {1, 1, 1};
    EXPECT_TRUE(checkExternalSupport(s).empty());
    ExternalSupport e = of(ExternalSupportType::Fixed);
    e.elasticDofs = {false, false, true, false, false, false};
    // Measured range of K: 1 .. 10000000.
    for (double bad : {0.0, -5.0, 20000000.0}) {
        e.elasticConstants[2] = bad;
        const auto issues = checkExternalSupport(e);
        ASSERT_EQ(issues.size(), 1u) << bad;
        EXPECT_EQ(issues[0].text, "El valor de 'K' debe estar entre 1 y 10000000.");
    }
    for (double good : {1.0, 5e5, 10000000.0}) {
        e.elasticConstants[2] = good;
        EXPECT_FALSE(hasError(checkExternalSupport(e))) << good;
    }
    e.elasticConstants[2] = -1.0;  // a spring that is not used (Dz is "Fijo" again) is not checked
    e.elasticDofs[2] = false;
    EXPECT_FALSE(hasError(checkExternalSupport(e)));
}

TEST(NodeSupport, DefaultElasticConstantQuestion65010) {
    ExternalSupport s = of(ExternalSupportType::Pinned);
    EXPECT_FALSE(elasticConstantIsDefault(s));  // not elastic
    s.elasticDofs = {true, true, true, false, false, false};
    EXPECT_TRUE(elasticConstantIsDefault(s));
    for (int i = 0; i < 3; ++i) s.elasticConstants[i] = 5000;  // measured: another K does not ask
    EXPECT_FALSE(elasticConstantIsDefault(s));
    const std::string q = elasticDefaultQuestion();  // the original writes the literal "K"
    EXPECT_NE(q.find("(K) coincide con el valor por defecto"), std::string::npos);
    EXPECT_EQ(q.find("%s"), std::string::npos);
    EXPECT_NE(q.find("¿Desea continuar?"), std::string::npos);
}

TEST(NodeSupport, PrescribedValidation) {
    ExternalSupport s = of(ExternalSupportType::PlaneXYPinned);  // only Dz blocked
    s.prescribed.push_back({7, {0, 0, -0.01, 0, 0, 0}});
    EXPECT_TRUE(checkExternalSupport(s).empty());

    s.prescribed.push_back({7, {0, 0, -0.02, 0, 0, 0}});
    auto issues = checkExternalSupport(s);
    ASSERT_EQ(issues.size(), 1u);
    EXPECT_EQ(issues[0].messageId, 65465);
    EXPECT_EQ(issues[0].severity, IssueSeverity::Error);

    s.prescribed = {{1, {}}};
    issues = checkExternalSupport(s);
    ASSERT_EQ(issues.size(), 1u);
    EXPECT_EQ(issues[0].messageId, 65466);

    s.prescribed = {{1, {0.5, 0, -0.01, 0, 0, 0}}};  // Dx is not blocked
    issues = checkExternalSupport(s);
    ASSERT_EQ(issues.size(), 1u);
    EXPECT_EQ(issues[0].messageId, 65467);
    EXPECT_EQ(issues[0].severity, IssueSeverity::Warning);
    EXPECT_FALSE(hasError(issues));
    const auto eff = effectivePrescribed(s);
    EXPECT_DOUBLE_EQ(eff[0].values[0], 0.0);
    EXPECT_DOUBLE_EQ(eff[0].values[2], -0.01);
}

TEST(NodeSupport, InternalLinkValidation) {
    InternalLink partial;
    partial.type = InternalLinkType::PartialFixity;
    EXPECT_DOUBLE_EQ(partial.partialCoefficient, 0.0);  // measured default 0.00
    EXPECT_TRUE(checkInternalLink(partial).empty());
    EXPECT_TRUE(checkInternalLink({InternalLinkType::PartialFixity, kMaxPartialCoefficient, {}, {}}).empty());
    for (double bad : {-1.0, 20000000.0}) {
        partial.partialCoefficient = bad;
        const auto issues = checkInternalLink(partial);
        ASSERT_EQ(issues.size(), 1u) << bad;
        EXPECT_EQ(issues[0].text, "El valor de 'Coef.Emp.Parc.' debe estar entre 0.00 y 10000000.00.");
    }

    InternalLink g;
    g.type = InternalLinkType::Generic;
    EXPECT_TRUE(hasError(checkInternalLink(g)));  // no elements
    g.rows = {{false, 1.0}, {true, 0.0}};
    g.elementRow = {0, 0, 1};
    EXPECT_TRUE(checkInternalLink(g, 3).empty());
    EXPECT_TRUE(hasError(checkInternalLink(g, 4)));
    g.elementRow[2] = 2;
    EXPECT_TRUE(hasError(checkInternalLink(g)));

    EXPECT_TRUE(checkGenericInternalSelection({3, 3, 3}).empty());
    const auto mixed = checkGenericInternalSelection({3, 2});
    ASSERT_EQ(mixed.size(), 1u);
    EXPECT_EQ(mixed[0].messageId, 65473);
    EXPECT_EQ(mixed[0].text.substr(0, 31), "Ha seleccionado nudos con disti");
}

TEST(NodeSupport, AssignRemoveUndoRedoAndQueries) {
    Project p = twoNodes();
    UndoStack stack;
    EXPECT_FALSE(isSupported(p.structure, p.nodeSupports, 1));
    EXPECT_EQ(assignExternalSupport(p, stack, {1, 2, 99}, of(ExternalSupportType::Fixed)), 2);
    EXPECT_TRUE(isSupported(p.structure, p.nodeSupports, 1));
    EXPECT_EQ(constrainedDofs(p.structure, p.nodeSupports, 2), (Dofs{true, true, true, true, true, true}));
    EXPECT_FALSE(isSupported(p.structure, p.nodeSupports, 99));
    stack.undo();
    EXPECT_TRUE(p.nodeSupports.empty());
    stack.redo();
    EXPECT_EQ(p.nodeSupports.external.size(), 2u);

    // Overwrite one node, then undo restores the previous value.
    ASSERT_EQ(assignExternalSupport(p, stack, {1}, of(ExternalSupportType::Pinned)), 1);
    EXPECT_EQ(p.nodeSupports.external.at(1).type, ExternalSupportType::Pinned);
    stack.undo();
    EXPECT_EQ(p.nodeSupports.external.at(1).type, ExternalSupportType::Fixed);

    // Free is the same as removing; unknown nodes alone do nothing.
    EXPECT_EQ(assignExternalSupport(p, stack, {1}, of(ExternalSupportType::Free)), 1);
    EXPECT_EQ(p.nodeSupports.external.count(1), 0u);
    EXPECT_EQ(removeExternalSupport(p, stack, {42}), 0);
    EXPECT_EQ(removeExternalSupport(p, stack, {2}), 1);
    EXPECT_TRUE(p.nodeSupports.external.empty());
    stack.undo();
    EXPECT_EQ(p.nodeSupports.external.count(2), 1u);
}

TEST(NodeSupport, InvalidAssignmentPushesNothing) {
    Project p = twoNodes();
    UndoStack stack;
    ExternalSupport zero = of(ExternalSupportType::LineUserFixed);
    zero.userVector = {0, 0, 0};
    EXPECT_EQ(assignExternalSupport(p, stack, {1}, zero), 0);
    EXPECT_EQ(stack.count(), 0u);
    EXPECT_TRUE(p.nodeSupports.empty());
}

TEST(NodeSupport, InternalLinkAssignment) {
    Project p;
    const int a = p.structure.addNode({0, 0, 0});
    const int b = p.structure.addNode({0, 0, 3});
    const int c = p.structure.addNode({3, 0, 3});
    ASSERT_NE(p.structure.addBar(a, b), 0);
    ASSERT_NE(p.structure.addBar(b, c), 0);
    EXPECT_EQ(elementCountAtNode(p.structure, b), 2);
    EXPECT_EQ(internalLinkOf(p.nodeSupports, b).type, InternalLinkType::Fixed);
    UndoStack stack;

    InternalLink pinned;
    pinned.type = InternalLinkType::Pinned;
    EXPECT_EQ(assignInternalLink(p, stack, {a, b, c}, pinned), 3);
    EXPECT_EQ(internalLinkOf(p.nodeSupports, b).type, InternalLinkType::Pinned);

    // Generic with two elements applies only to the node with two elements.
    InternalLink g;
    g.type = InternalLinkType::Generic;
    g.rows = {{false, 1.0}, {false, 1.0}};
    g.elementRow = {0, 1};
    EXPECT_EQ(assignInternalLink(p, stack, {a, b, c}, g), 1);
    EXPECT_EQ(internalLinkOf(p.nodeSupports, b).type, InternalLinkType::Generic);
    EXPECT_EQ(internalLinkOf(p.nodeSupports, a).type, InternalLinkType::Pinned);  // skipped: unchanged
    stack.undo();
    EXPECT_EQ(internalLinkOf(p.nodeSupports, b).type, InternalLinkType::Pinned);

    // Restoring the default removes the entries.
    EXPECT_EQ(removeInternalLink(p, stack, {a, b, c}), 3);
    EXPECT_TRUE(p.nodeSupports.internal.empty());
    stack.undo();
    EXPECT_EQ(p.nodeSupports.internal.size(), 3u);
}

TEST(NodeSupport, DeleteNodeDropsAndUndoRestoresSupport) {
    Project p = twoNodes();
    UndoStack stack;
    ASSERT_EQ(assignExternalSupport(p, stack, {1}, of(ExternalSupportType::Fixed)), 1);
    InternalLink pinned;
    pinned.type = InternalLinkType::Pinned;
    ASSERT_EQ(assignInternalLink(p, stack, {1}, pinned), 1);
    ASSERT_TRUE(deleteNodes(p, stack, {1}));
    EXPECT_TRUE(p.nodeSupports.empty());
    stack.undo();
    EXPECT_EQ(p.nodeSupports.external.count(1), 1u);
    EXPECT_EQ(p.nodeSupports.internal.count(1), 1u);
    stack.redo();
    EXPECT_TRUE(p.nodeSupports.empty());
}

TEST(NodeSupport, MergeNodeDropsAndUndoRestoresSupport) {
    Project p = twoNodes();
    UndoStack stack;
    ASSERT_EQ(assignExternalSupport(p, stack, {2}, of(ExternalSupportType::Pinned)), 1);
    ASSERT_TRUE(moveNode(p, stack, 2, {0, 0, 0}));  // merges node 2 into node 1
    EXPECT_EQ(p.structure.findNode(2), nullptr);
    EXPECT_EQ(p.nodeSupports.external.count(2), 0u);
    stack.undo();
    EXPECT_EQ(p.nodeSupports.external.count(2), 1u);
}

TEST(NodeSupport, JsonRoundTripAndBackwardCompatibility) {
    Project p = twoNodes();
    ExternalSupport s = of(ExternalSupportType::Generic);
    s.generic.mode = GenericDisplacementMode::OnPlane;
    s.generic.vector = {1, 2, 3};
    s.generic.rotations[2] = DofMode::Elastic;
    s.elasticConstants[5] = 123.5;
    s.senses[5] = SupportSense::Negative;
    s.senses[2] = SupportSense::Positive;
    s.prescribed.push_back({3, {0, 0, 0.004, 0, 0, 0}});
    p.nodeSupports.external[1] = s;
    ExternalSupport pinned = of(ExternalSupportType::LineUserPinned);
    pinned.userVector = {1, 0, 1};
    pinned.elasticDofs = {true, false, true, false, false, false};
    p.nodeSupports.external[2] = pinned;
    InternalLink g;
    g.type = InternalLinkType::Generic;
    g.rows = {{true, 0.25}, {false, 1.0}};
    g.elementRow = {1, 0};
    p.nodeSupports.internal[2] = g;

    const std::string text = project_json::dumpSection(p, "node_supports");
    Project q = twoNodes();
    project_json::loadSection(q, "node_supports", text);
    EXPECT_EQ(q.nodeSupports, p.nodeSupports);
    EXPECT_TRUE(project_json::isKnownSection("node_supports"));
    EXPECT_TRUE(project_json::validate(q).empty());

    // A file without the section (older versions) leaves it empty; an empty section loads as empty.
    Project old = twoNodes();
    project_json::loadSection(old, "node_supports", "{}");
    EXPECT_TRUE(old.nodeSupports.empty());
    // Missing keys inside an entry take defaults.
    project_json::loadSection(old, "node_supports", R"({"external":[{"node":1,"support":{"type":"fixed"}}]})");
    EXPECT_EQ(old.nodeSupports.external.at(1).type, ExternalSupportType::Fixed);
    EXPECT_DOUBLE_EQ(old.nodeSupports.external.at(1).elasticConstants[0], 100000.0);
    // Unknown enum names are rejected, never silently replaced.
    EXPECT_THROW(project_json::loadSection(old, "node_supports", R"({"external":[{"node":1,"support":{"type":"bogus"}}]})"),
                 std::exception);
}

TEST(NodeSupport, OrphanEntriesAreNotSaved) {
    Project p = twoNodes();
    p.nodeSupports.external[77] = of(ExternalSupportType::Fixed);  // node 77 does not exist
    Project q = twoNodes();
    project_json::loadSection(q, "node_supports", project_json::dumpSection(p, "node_supports"));
    EXPECT_TRUE(q.nodeSupports.empty());
}
