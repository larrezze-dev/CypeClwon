#include "core/shell_properties_edit.h"

#include <algorithm>
#include <memory>

namespace cc::core {

namespace {

void push(Project& project, UndoStack& stack, ShellProperties after, const char* text) {
    auto cmd = std::make_unique<ShellPropertiesCommand>(project, std::move(after), text);
    if (!cmd->valid()) return;
    stack.push(std::move(cmd));
}

template <typename Apply>
void editShells(Project& project, UndoStack& stack, const std::vector<int>& shellIds, const char* text, Apply apply) {
    const std::vector<int> ids = existingShells(project, shellIds);
    if (ids.empty()) return;
    ShellProperties after = project.shellProps;
    for (int id : ids) {
        ShellRecord rec = shellRecordOf(after, id);
        apply(rec);
        setShellRecord(after, id, rec);
    }
    push(project, stack, std::move(after), text);
}

}  // namespace

std::vector<int> existingShells(const Project& project, const std::vector<int>& ids) {
    std::vector<int> out;
    for (int id : ids)
        if (project.structure.findShell(id) && std::find(out.begin(), out.end(), id) == out.end()) out.push_back(id);
    return out;
}

std::vector<ShellEdgeKey> existingShellEdges(const Project& project, const std::vector<ShellEdgeKey>& keys) {
    std::vector<ShellEdgeKey> out;
    for (const ShellEdgeKey& raw : keys) {
        const ShellEdgeKey key = makeEdgeKey(raw.shellId, raw.nodeA, raw.nodeB);
        const std::vector<ShellEdgeKey> edges = shellEdges(project.structure, key.shellId);
        if (std::find(edges.begin(), edges.end(), key) != edges.end() && std::find(out.begin(), out.end(), key) == out.end()) out.push_back(key);
    }
    return out;
}

std::optional<ShellPropIssue> assignShellDiscretisation(Project& project, UndoStack& stack, const std::vector<int>& shellIds, double maxSideM) {
    if (auto issue = checkTriangleSize(maxSideM)) return issue;
    editShells(project, stack, shellIds, "Discretización de láminas", [&](ShellRecord& r) { r.maxTriangleSideM = maxSideM; });
    return std::nullopt;
}

std::optional<ShellPropIssue> assignShellStiffness(Project& project, UndoStack& stack, const std::vector<int>& shellIds, const ShellStiffnessFactors& factors) {
    if (auto issue = checkStiffnessFactors(factors)) return issue;
    editShells(project, stack, shellIds, "Factores de rigidez de láminas", [&](ShellRecord& r) { r.stiffness = factors; });
    return std::nullopt;
}

std::optional<ShellPropIssue> assignShellBuckling(Project& project, UndoStack& stack, const std::vector<int>& shellIds, bool considered) {
    editShells(project, stack, shellIds, "Pandeo de láminas", [&](ShellRecord& r) { r.bucklingConsidered = considered; });
    return std::nullopt;
}

std::optional<ShellPropIssue> assignShellModal(Project& project, UndoStack& stack, const std::vector<int>& shellIds, bool considered) {
    editShells(project, stack, shellIds, "Análisis modal de láminas", [&](ShellRecord& r) { r.modalConsidered = considered; });
    return std::nullopt;
}

std::optional<ShellPropIssue> assignEdgeInterior(Project& project, UndoStack& stack, const std::vector<ShellEdgeKey>& edges, const EdgeInteriorBond& bond) {
    if (auto issue = checkEdgeInterior(bond)) return issue;
    const std::vector<ShellEdgeKey> keys = existingShellEdges(project, edges);
    if (keys.empty()) return std::nullopt;
    ShellProperties after = project.shellProps;
    for (const ShellEdgeKey& k : keys) setEdgeInterior(after, k, bond);
    push(project, stack, std::move(after), "Vinculación interior de aristas de láminas");
    return std::nullopt;
}

std::optional<ShellPropIssue> assignEdgeExterior(Project& project, UndoStack& stack, const std::vector<ShellEdgeKey>& edges, const ExternalSupport& support) {
    for (const SupportIssue& s : checkExternalSupport(support))
        if (s.severity == IssueSeverity::Error) return ShellPropIssue{s.messageId, s.text};
    const std::vector<ShellEdgeKey> keys = existingShellEdges(project, edges);
    if (keys.empty()) return std::nullopt;
    ShellProperties after = project.shellProps;
    for (const ShellEdgeKey& k : keys) setEdgeExterior(after, k, support);
    push(project, stack, std::move(after), "Vinculación exterior de aristas de láminas");
    return std::nullopt;
}

std::optional<ShellPropIssue> assignShellBands(Project& project, UndoStack& stack, const std::vector<int>& shellIds,
                                               const std::vector<ShellIntegrationBand>& bands, int* dropped) {
    if (dropped) *dropped = 0;
    const std::vector<int> ids = existingShells(project, shellIds);
    if (ids.empty()) return std::nullopt;
    std::vector<ShellIntegrationBand> normalised;
    for (const ShellIntegrationBand& b : bands) normalised.push_back(normaliseBand(b));

    // The edit is made on the geometry of the first shell: its bands must all be valid.
    const std::vector<Point2> firstContour = shellContour2D(project.structure, *project.structure.findShell(ids.front()));
    if (firstContour.empty()) return ShellPropIssue{0, "La lámina no tiene una geometría válida."};
    for (const ShellIntegrationBand& b : normalised)
        if (auto issue = checkBand(firstContour, b, false)) return issue;

    ShellProperties after = project.shellProps;
    int lost = 0;
    for (int id : ids) {
        ShellRecord rec = shellRecordOf(after, id);
        const std::vector<Point2> contour = shellContour2D(project.structure, *project.structure.findShell(id));
        rec.bands.clear();
        for (const ShellIntegrationBand& b : normalised) {
            if (id != ids.front() && (contour.empty() || checkBand(contour, b, false))) {
                ++lost;
                continue;
            }
            rec.bands.push_back(b);
        }
        setShellRecord(after, id, rec);
    }
    if (dropped) *dropped = lost;
    push(project, stack, std::move(after), "Bandas de integración");
    return std::nullopt;
}

std::optional<ShellPropIssue> assignBandForceOptions(Project& project, UndoStack& stack, const BandForceOptions& options) {
    ShellProperties after = project.shellProps;
    after.bandForce = options;
    push(project, stack, std::move(after), "Integración de esfuerzos en bandas");
    return std::nullopt;
}

}  // namespace cc::core
