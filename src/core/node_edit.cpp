#include "core/node_edit.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace cc::core {

namespace {

constexpr double kMaxCoordinateM = 1.0e6;  // mirrors the coordinate limit of structure_model.cpp
constexpr double kMinBarLengthM = 1e-6;    // mirrors the minimum bar length of structure_model.cpp

bool validPosition(const Point3& p) {
    return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z) && std::fabs(p.x) <= kMaxCoordinateM &&
           std::fabs(p.y) <= kMaxCoordinateM && std::fabs(p.z) <= kMaxCoordinateM;
}

Point3 sub(const Point3& a, const Point3& b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
double dot(const Point3& a, const Point3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
Point3 cross(const Point3& a, const Point3& b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

int otherEnd(const Bar& bar, int nodeId) { return bar.startNode == nodeId ? bar.endNode : bar.startNode; }

/// True when `n` lies on the segment a-b (strictly between the ends, within kCollinearToleranceM of the line).
bool lieOnSegment(const Point3& a, const Point3& n, const Point3& b) {
    const Point3 ab = sub(b, a);
    const double len2 = dot(ab, ab);
    if (len2 <= 0.0) return false;
    const double t = dot(sub(n, a), ab) / len2;
    if (t <= 0.0 || t >= 1.0) return false;
    const Point3 c = cross(sub(n, a), ab);
    return std::sqrt(dot(c, c) / len2) <= kCollinearToleranceM;
}

}  // namespace

// --- AddNodeCommand ---------------------------------------------------------------------------------------------

AddNodeCommand::AddNodeCommand(Project& project, const Point3& position)
    : m_project(project), m_node{project.structure.nextNodeId, position}, m_valid(validPosition(position)) {}

void AddNodeCommand::redo() {
    if (m_valid) m_project.structure.insertNode(m_node);
}

void AddNodeCommand::undo() { m_project.structure.removeNode(m_node.id); }

// --- MoveNodeCommand --------------------------------------------------------------------------------------------

MoveNodeCommand::MoveNodeCommand(Project& project, int nodeId, const Point3& newPosition)
    : m_project(project), m_id(nodeId), m_after(newPosition), m_valid(false) {
    const StructuralModel& model = project.structure;
    const Node* node = model.findNode(nodeId);
    if (!node || !validPosition(newPosition)) return;
    m_before = node->position;
    m_node = *node;
    m_ligaturesBefore = project.ligatures;
    m_diaphragmsBefore = project.diaphragms;
    m_deflectionBefore = project.deflection;
    m_deflectionAfter = project.deflection;
    m_shellPropsBefore = project.shellProps;
    m_shellPropsAfter = project.shellProps;
    if (auto it = project.nodeSupports.external.find(nodeId); it != project.nodeSupports.external.end()) m_externalBefore = it->second;
    if (auto it = project.nodeSupports.internal.find(nodeId); it != project.nodeSupports.internal.end()) m_internalBefore = it->second;

    // Another node already lies at the destination: the two merge (nudos.md section 9.2).
    for (const Node& other : model.nodes)
        if (other.id != nodeId && distance(other.position, newPosition) <= kCoincidentNodeToleranceM) {
            m_mergeTarget = other.id;
            break;
        }

    std::vector<int> barIds = model.barsUsingNode(nodeId);
    std::sort(barIds.begin(), barIds.end());
    if (m_mergeTarget == 0) {
        for (int barId : barIds) {
            const Bar* bar = model.findBar(barId);
            const Node* far = model.findNode(otherEnd(*bar, nodeId));
            if (!far || distance(far->position, newPosition) < kMinBarLengthM) return;
        }
        m_valid = true;
        if (!project.deflection.empty() || !project.shellProps.empty()) {
            StructuralModel work = model;
            work.findNode(nodeId)->position = newPosition;
            if (!project.deflection.empty()) m_deflectionAfter = normaliseDeflection(project.deflection, work);
            m_shellPropsAfter = reconcileShellProperties(project.shellProps, model, work);
        }
        return;
    }

    for (int barId : barIds) {
        const Bar& bar = *model.findBar(barId);
        m_oldBars.push_back(bar);
        if (otherEnd(bar, nodeId) == m_mergeTarget) continue;  // the bar between the two nodes has length zero: it disappears
        Bar moved = bar;
        (moved.startNode == nodeId ? moved.startNode : moved.endNode) = m_mergeTarget;
        m_newBars.push_back(moved);
    }
    std::vector<int> shellIds = model.shellsUsingNode(nodeId);
    std::sort(shellIds.begin(), shellIds.end());
    for (int shellId : shellIds) {
        const Shell& shell = *model.findShell(shellId);
        if (std::find(shell.nodeIds.begin(), shell.nodeIds.end(), m_mergeTarget) != shell.nodeIds.end()) return;  // would collapse
        m_oldShells.push_back(shell);
        Shell moved = shell;
        std::replace(moved.nodeIds.begin(), moved.nodeIds.end(), nodeId, m_mergeTarget);
        m_newShells.push_back(moved);
    }
    m_valid = true;
    if (!project.deflection.empty() || !project.shellProps.empty()) {
        StructuralModel work = model;
        for (const Shell& s : m_oldShells) work.removeShell(s.id);
        for (const Bar& b : m_oldBars) work.removeBar(b.id);
        work.removeNode(nodeId);
        for (const Bar& b : m_newBars) work.insertBar(b);
        if (!project.deflection.empty()) m_deflectionAfter = normaliseDeflection(project.deflection, work);
        for (const Shell& s : m_newShells) work.insertShell(s);
        m_shellPropsAfter = reconcileShellProperties(project.shellProps, model, work);
    }
}

void MoveNodeCommand::redo() {
    if (!m_valid) return;
    StructuralModel& model = m_project.structure;
    if (m_mergeTarget == 0) {
        if (Node* n = model.findNode(m_id)) n->position = m_after;
        m_project.deflection = m_deflectionAfter;
        m_project.shellProps = m_shellPropsAfter;
        return;
    }
    for (const Shell& s : m_oldShells) model.removeShell(s.id);
    for (const Bar& b : m_oldBars) model.removeBar(b.id);
    model.removeNode(m_id);
    m_project.nodeSupports.external.erase(m_id);
    m_project.nodeSupports.internal.erase(m_id);
    applyLigatureNodesDeleted(m_project.ligatures, {m_id});
    applyDiaphragmNodesDeleted(m_project.diaphragms, {m_id});
    for (const Bar& b : m_newBars) model.insertBar(b);
    for (const Shell& s : m_newShells) model.insertShell(s);
    m_project.deflection = m_deflectionAfter;
    m_project.shellProps = m_shellPropsAfter;
}

void MoveNodeCommand::undo() {
    if (!m_valid) return;
    StructuralModel& model = m_project.structure;
    if (m_mergeTarget == 0) {
        if (Node* n = model.findNode(m_id)) n->position = m_before;
        m_project.deflection = m_deflectionBefore;
        m_project.shellProps = m_shellPropsBefore;
        return;
    }
    for (const Shell& s : m_newShells) model.removeShell(s.id);
    for (const Bar& b : m_newBars) model.removeBar(b.id);
    model.insertNode(m_node);
    if (m_externalBefore) m_project.nodeSupports.external[m_id] = *m_externalBefore;
    if (m_internalBefore) m_project.nodeSupports.internal[m_id] = *m_internalBefore;
    m_project.ligatures = m_ligaturesBefore;
    m_project.diaphragms = m_diaphragmsBefore;
    for (const Bar& b : m_oldBars) model.insertBar(b);
    for (const Shell& s : m_oldShells) model.insertShell(s);
    m_project.deflection = m_deflectionBefore;
    m_project.shellProps = m_shellPropsBefore;
}

// --- DeleteNodesCommand -----------------------------------------------------------------------------------------

DeleteNodesCommand::DeleteNodesCommand(Project& project, std::vector<int> nodeIds) : m_project(project) {
    const StructuralModel& model = project.structure;
    std::sort(nodeIds.begin(), nodeIds.end());
    nodeIds.erase(std::unique(nodeIds.begin(), nodeIds.end()), nodeIds.end());
    nodeIds.erase(std::remove_if(nodeIds.begin(), nodeIds.end(), [&](int id) { return !model.findNode(id); }),
                  nodeIds.end());
    m_ligaturesBefore = project.ligatures;
    m_diaphragmsBefore = project.diaphragms;
    m_barEndsBefore = project.barEnds;
    m_barEndsAfter = project.barEnds;
    m_bucklingBefore = project.buckling;
    m_bucklingAfter = project.buckling;
    m_deflectionBefore = project.deflection;
    m_deflectionAfter = project.deflection;
    m_shellPropsBefore = project.shellProps;
    m_shellPropsAfter = project.shellProps;
    auto deleted = [&](int id) { return std::binary_search(nodeIds.begin(), nodeIds.end(), id); };

    std::vector<int> barIds, shellIds;
    std::vector<std::array<int, 3>> mergeInfo;  // first, second, merged bar ids
    int nextBarId = model.nextBarId;
    for (int id : nodeIds) {
        m_nodes.push_back(*model.findNode(id));
        if (auto it = project.nodeSupports.external.find(id); it != project.nodeSupports.external.end()) m_externalBefore[id] = it->second;
        if (auto it = project.nodeSupports.internal.find(id); it != project.nodeSupports.internal.end()) m_internalBefore[id] = it->second;
        std::vector<int> used = model.barsUsingNode(id);
        std::sort(used.begin(), used.end());
        const std::vector<int> shellsUsed = model.shellsUsingNode(id);
        barIds.insert(barIds.end(), used.begin(), used.end());
        shellIds.insert(shellIds.end(), shellsUsed.begin(), shellsUsed.end());

        // Exception of the original: an intermediate node between exactly two aligned bars.
        if (used.size() != 2 || !shellsUsed.empty()) continue;
        const Bar& first = *model.findBar(used[0]);  // lower id
        const Bar& second = *model.findBar(used[1]);
        const int farFirst = otherEnd(first, id);
        const int farSecond = otherEnd(second, id);
        if (farFirst == farSecond || deleted(farFirst) || deleted(farSecond)) continue;
        if (first.materialId != second.materialId || first.sectionId != second.sectionId) continue;
        if (!lieOnSegment(model.findNode(farFirst)->position, model.findNode(id)->position,
                          model.findNode(farSecond)->position))
            continue;
        Bar merged = first;
        merged.id = nextBarId++;
        // Keep the direction of the first bar: it ran towards `farFirst` when it started at the deleted node.
        const bool towardsFirstFar = first.startNode == id;
        merged.startNode = towardsFirstFar ? farSecond : farFirst;
        merged.endNode = towardsFirstFar ? farFirst : farSecond;
        // The new bar runs between the far nodes: each of its ends takes the data of the end of the old bar that sits at that node.
        auto endDataAt = [&](const Bar& old, int node) { return barEndsOf(project.barEnds, old.id).ends[old.startNode == node ? kBarOrigin : kBarFinal]; };
        BarEndsRecord rec;
        for (int e = 0; e < 2; ++e) {
            const int node = e == kBarOrigin ? merged.startNode : merged.endNode;
            rec.ends[static_cast<std::size_t>(e)] = endDataAt(node == farFirst ? first : second, node);
        }
        setBarEnds(m_barEndsAfter, merged.id, rec);
        setBuckling(m_bucklingAfter, merged.id, bucklingOf(project.buckling, first.id));
        m_merged.push_back(merged);
        mergeInfo.push_back({first.id, second.id, merged.id});
    }
    std::sort(barIds.begin(), barIds.end());
    barIds.erase(std::unique(barIds.begin(), barIds.end()), barIds.end());
    std::sort(shellIds.begin(), shellIds.end());
    shellIds.erase(std::unique(shellIds.begin(), shellIds.end()), shellIds.end());
    for (int id : barIds) m_bars.push_back(*model.findBar(id));
    for (int id : shellIds) m_shells.push_back(*model.findShell(id));

    if (!project.deflection.empty()) {
        // The merged bar takes the place of the two bars in their explicit group (the group of the first one wins).
        DeflectionData work = project.deflection;
        auto groupOf = [&](int bar) -> DeflectionBarGroup* {
            for (DeflectionBarGroup& g : work.groups)
                if (std::find(g.bars.begin(), g.bars.end(), bar) != g.bars.end()) return &g;
            return nullptr;
        };
        for (const auto& [first, second, mergedId] : mergeInfo) {
            DeflectionBarGroup* gFirst = groupOf(first);
            DeflectionBarGroup* gSecond = groupOf(second);
            DeflectionBarGroup* target = gFirst ? gFirst : gSecond;
            for (DeflectionBarGroup* g : {gFirst, gSecond})
                if (g) g->bars.erase(std::remove_if(g->bars.begin(), g->bars.end(), [&](int id) { return id == first || id == second; }), g->bars.end());
            if (target) target->bars.push_back(mergedId);
        }
        StructuralModel after = model;
        for (int id : shellIds) after.removeShell(id);
        for (int id : barIds) after.removeBar(id);
        for (int id : nodeIds) after.removeNode(id);
        for (const Bar& b : m_merged) after.insertBar(b);
        m_deflectionAfter = normaliseDeflection(work, after);
    }
    if (!project.shellProps.empty()) {
        StructuralModel after = model;
        for (int id : shellIds) after.removeShell(id);
        for (int id : barIds) after.removeBar(id);
        for (int id : nodeIds) after.removeNode(id);
        m_shellPropsAfter = reconcileShellProperties(project.shellProps, model, after);
    }
}

void DeleteNodesCommand::redo() {
    StructuralModel& model = m_project.structure;
    for (const Shell& s : m_shells) model.removeShell(s.id);
    for (const Bar& b : m_bars) model.removeBar(b.id);
    for (const Node& n : m_nodes) {
        model.removeNode(n.id);
        m_project.nodeSupports.external.erase(n.id);
        m_project.nodeSupports.internal.erase(n.id);
    }
    std::vector<int> deletedIds;
    for (const Node& n : m_nodes) deletedIds.push_back(n.id);
    applyLigatureNodesDeleted(m_project.ligatures, deletedIds);
    applyDiaphragmNodesDeleted(m_project.diaphragms, deletedIds);
    for (const Bar& b : m_merged) model.insertBar(b);
    m_project.barEnds = m_barEndsAfter;
    m_project.buckling = m_bucklingAfter;
    m_project.deflection = m_deflectionAfter;
    m_project.shellProps = m_shellPropsAfter;
}

void DeleteNodesCommand::undo() {
    StructuralModel& model = m_project.structure;
    for (const Bar& b : m_merged) model.removeBar(b.id);
    for (const Node& n : m_nodes) model.insertNode(n);
    for (const auto& [id, support] : m_externalBefore) m_project.nodeSupports.external[id] = support;
    for (const auto& [id, link] : m_internalBefore) m_project.nodeSupports.internal[id] = link;
    m_project.ligatures = m_ligaturesBefore;
    m_project.diaphragms = m_diaphragmsBefore;
    m_project.barEnds = m_barEndsBefore;
    m_project.buckling = m_bucklingBefore;
    m_project.deflection = m_deflectionBefore;
    m_project.shellProps = m_shellPropsBefore;
    for (const Bar& b : m_bars) model.insertBar(b);
    for (const Shell& s : m_shells) model.insertShell(s);
}

std::optional<int> findNodeAt(const StructuralModel& model, const Point3& p, double toleranceM) {
    std::optional<int> best;
    double bestDist = toleranceM;
    for (const Node& n : model.nodes) {
        const Point3 d = sub(n.position, p);
        const double dist = std::sqrt(dot(d, d));
        if (best ? dist < bestDist : dist <= toleranceM) {
            bestDist = dist;
            best = n.id;
        }
    }
    return best;
}

// --- wrappers ---------------------------------------------------------------------------------------------------

int addNode(Project& project, UndoStack& stack, const Point3& position) {
    auto cmd = std::make_unique<AddNodeCommand>(project, position);
    if (!cmd->valid()) return 0;
    const int id = cmd->id();
    stack.push(std::move(cmd));
    return id;
}

int addNodeIfFree(Project& project, UndoStack& stack, const Point3& position) {
    if (!validPosition(position) || findNodeAt(project.structure, position)) return 0;
    return addNode(project, stack, position);
}

bool moveNode(Project& project, UndoStack& stack, int nodeId, const Point3& newPosition) {
    auto cmd = std::make_unique<MoveNodeCommand>(project, nodeId, newPosition);
    if (!cmd->valid()) return false;
    stack.push(std::move(cmd));
    return true;
}

bool deleteNodes(Project& project, UndoStack& stack, const std::vector<int>& nodeIds) {
    auto cmd = std::make_unique<DeleteNodesCommand>(project, nodeIds);
    if (!cmd->valid()) return false;
    stack.push(std::move(cmd));
    return true;
}

}  // namespace cc::core
