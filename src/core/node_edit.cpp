#include "core/node_edit.h"

#include <algorithm>
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
    for (int barId : model.barsUsingNode(nodeId)) {
        const Bar* bar = model.findBar(barId);
        const Node* far = model.findNode(otherEnd(*bar, nodeId));
        if (!far || distance(far->position, newPosition) < kMinBarLengthM) return;
    }
    m_valid = true;
}

void MoveNodeCommand::redo() {
    if (Node* n = m_project.structure.findNode(m_id)) n->position = m_after;
}

void MoveNodeCommand::undo() {
    if (Node* n = m_project.structure.findNode(m_id)) n->position = m_before;
}

// --- DeleteNodesCommand -----------------------------------------------------------------------------------------

DeleteNodesCommand::DeleteNodesCommand(Project& project, std::vector<int> nodeIds) : m_project(project) {
    const StructuralModel& model = project.structure;
    std::sort(nodeIds.begin(), nodeIds.end());
    nodeIds.erase(std::unique(nodeIds.begin(), nodeIds.end()), nodeIds.end());
    nodeIds.erase(std::remove_if(nodeIds.begin(), nodeIds.end(), [&](int id) { return !model.findNode(id); }),
                  nodeIds.end());
    auto deleted = [&](int id) { return std::binary_search(nodeIds.begin(), nodeIds.end(), id); };

    std::vector<int> barIds, shellIds;
    int nextBarId = model.nextBarId;
    for (int id : nodeIds) {
        m_nodes.push_back(*model.findNode(id));
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
        m_merged.push_back(merged);
    }
    std::sort(barIds.begin(), barIds.end());
    barIds.erase(std::unique(barIds.begin(), barIds.end()), barIds.end());
    std::sort(shellIds.begin(), shellIds.end());
    shellIds.erase(std::unique(shellIds.begin(), shellIds.end()), shellIds.end());
    for (int id : barIds) m_bars.push_back(*model.findBar(id));
    for (int id : shellIds) m_shells.push_back(*model.findShell(id));
}

void DeleteNodesCommand::redo() {
    StructuralModel& model = m_project.structure;
    for (const Shell& s : m_shells) model.removeShell(s.id);
    for (const Bar& b : m_bars) model.removeBar(b.id);
    for (const Node& n : m_nodes) model.removeNode(n.id);
    for (const Bar& b : m_merged) model.insertBar(b);
}

void DeleteNodesCommand::undo() {
    StructuralModel& model = m_project.structure;
    for (const Bar& b : m_merged) model.removeBar(b.id);
    for (const Node& n : m_nodes) model.insertNode(n);
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
