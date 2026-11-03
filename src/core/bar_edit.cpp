#include "core/bar_edit.h"

#include <algorithm>
#include <cmath>
#include <set>
#include <utility>

namespace cc::core {

namespace {

constexpr double kMaxCoordinateM = 1.0e6;  // mirrors the coordinate limit of structure_model.cpp

std::pair<int, int> unordered(int a, int b) { return a < b ? std::make_pair(a, b) : std::make_pair(b, a); }

bool pairExists(const StructuralModel& model, int a, int b) {
    for (const Bar& bar : model.bars)
        if ((bar.startNode == a && bar.endNode == b) || (bar.startNode == b && bar.endNode == a)) return true;
    return false;
}

/// Ids of the nodes lying strictly inside the segment a-b (further than kMinBarLengthM from both ends and within
/// kCutNodeToleranceM of the line), ordered from a to b.
std::vector<int> nodesOnSegment(const StructuralModel& model, int a, int b) {
    const Point3 pa = model.findNode(a)->position;
    const Point3 pb = model.findNode(b)->position;
    const Point3 ab{pb.x - pa.x, pb.y - pa.y, pb.z - pa.z};
    const double len2 = ab.x * ab.x + ab.y * ab.y + ab.z * ab.z;
    const double len = std::sqrt(len2);
    std::vector<std::pair<double, int>> found;
    for (const Node& n : model.nodes) {
        if (n.id == a || n.id == b) continue;
        const Point3 an{n.position.x - pa.x, n.position.y - pa.y, n.position.z - pa.z};
        const double along = (an.x * ab.x + an.y * ab.y + an.z * ab.z) / len;  // metres from a
        if (along < kMinBarLengthM || along > len - kMinBarLengthM) continue;
        const Point3 c{an.y * ab.z - an.z * ab.y, an.z * ab.x - an.x * ab.z, an.x * ab.y - an.y * ab.x};
        const double perpendicular = std::sqrt(c.x * c.x + c.y * c.y + c.z * c.z) / len;
        if (perpendicular <= kCutNodeToleranceM) found.emplace_back(along, n.id);
    }
    std::sort(found.begin(), found.end());
    std::vector<int> ids;
    for (const auto& f : found) ids.push_back(f.second);
    return ids;
}

/// Plans the bars of a polyline of nodes. `strict`: a pair that already has a bar invalidates the plan (single bar);
/// otherwise such pairs are skipped. Returns false when the plan is invalid.
bool planBars(const Project& project, const std::vector<int>& nodeIds, bool cut, bool strict, std::vector<Bar>& out) {
    const StructuralModel& model = project.structure;
    if (nodeIds.size() < 2) return false;
    for (int id : nodeIds)
        if (!model.findNode(id)) return false;

    std::set<std::pair<int, int>> pending;
    int nextId = model.nextBarId;
    const int layer = model.activeLayerId();
    for (std::size_t i = 0; i + 1 < nodeIds.size(); ++i) {
        const int a = nodeIds[i];
        const int b = nodeIds[i + 1];
        if (a == b) {
            if (strict) return false;
            continue;
        }
        if (distance(model.findNode(a)->position, model.findNode(b)->position) < kMinBarLengthM) return false;
        if (strict && pairExists(model, a, b)) return false;

        std::vector<int> stops{a};
        if (cut) {
            const std::vector<int> mid = nodesOnSegment(model, a, b);
            stops.insert(stops.end(), mid.begin(), mid.end());
        }
        stops.push_back(b);
        for (std::size_t k = 0; k + 1 < stops.size(); ++k) {
            const auto key = unordered(stops[k], stops[k + 1]);
            if (pairExists(model, key.first, key.second) || pending.count(key)) continue;
            pending.insert(key);
            Bar bar;
            bar.id = nextId++;
            bar.startNode = stops[k];
            bar.endNode = stops[k + 1];
            bar.layerIds = {layer};
            out.push_back(bar);
        }
    }
    return !out.empty();
}

void insertBars(StructuralModel& model, const std::vector<Bar>& bars) {
    for (const Bar& b : bars) model.insertBar(b);
}

void removeBars(StructuralModel& model, const std::vector<Bar>& bars) {
    for (const Bar& b : bars) model.removeBar(b.id);
}

std::vector<int> idsOf(const std::vector<Bar>& bars) {
    std::vector<int> ids;
    for (const Bar& b : bars) ids.push_back(b.id);
    return ids;
}

bool validCoordinate(double v) { return std::isfinite(v) && std::fabs(v) <= kMaxCoordinateM; }

}  // namespace

// --- AddBarCommand ----------------------------------------------------------------------------------------------

AddBarCommand::AddBarCommand(Project& project, int startNode, int endNode, bool generateCutNodes) : m_project(project) {
    m_valid = planBars(project, {startNode, endNode}, generateCutNodes, true, m_bars);
    if (!m_valid) m_bars.clear();
}

std::vector<int> AddBarCommand::barIds() const { return idsOf(m_bars); }

void AddBarCommand::redo() {
    if (m_valid) insertBars(m_project.structure, m_bars);
}

void AddBarCommand::undo() {
    if (m_valid) removeBars(m_project.structure, m_bars);
}

// --- AddBeamChainCommand ----------------------------------------------------------------------------------------

AddBeamChainCommand::AddBeamChainCommand(Project& project, const std::vector<int>& nodeIds, bool generateCutNodes)
    : m_project(project) {
    m_valid = planBars(project, nodeIds, generateCutNodes, false, m_bars);
    if (!m_valid) m_bars.clear();
}

std::vector<int> AddBeamChainCommand::barIds() const { return idsOf(m_bars); }

void AddBeamChainCommand::redo() {
    if (m_valid) insertBars(m_project.structure, m_bars);
}

void AddBeamChainCommand::undo() {
    if (m_valid) removeBars(m_project.structure, m_bars);
}

// --- AddColumnCommand -------------------------------------------------------------------------------------------

AddColumnCommand::AddColumnCommand(Project& project, double x, double y, std::vector<double> levelsZ)
    : m_project(project) {
    const StructuralModel& model = project.structure;
    constexpr const char* kNeedTwoLevels =
        "Para poder introducir pilares con esta opción debe definir previamente al menos 2 niveles en la obra.";
    if (!validCoordinate(x) || !validCoordinate(y)) return;
    for (double z : levelsZ)
        if (!validCoordinate(z)) return;
    std::sort(levelsZ.begin(), levelsZ.end());
    std::vector<double> levels;
    for (double z : levelsZ)
        if (levels.empty() || z - levels.back() >= kMinBarLengthM) levels.push_back(z);
    if (levels.size() < 2) {
        m_error = kNeedTwoLevels;
        return;
    }

    std::vector<int> ids;
    int nextNodeId = model.nextNodeId;
    for (double z : levels) {
        const Point3 p{x, y, z};
        int found = 0;
        double best = kMinBarLengthM;
        for (const Node& n : model.nodes) {
            const double d = distance(n.position, p);
            if (d <= best) {
                best = d;
                found = n.id;
            }
        }
        if (found == 0) {
            found = nextNodeId++;
            m_nodes.push_back(Node{found, p});
        }
        ids.push_back(found);
    }

    int nextBarId = model.nextBarId;
    const int layer = model.activeLayerId();
    for (std::size_t i = 0; i + 1 < ids.size(); ++i) {
        if (pairExists(model, ids[i], ids[i + 1])) continue;
        Bar bar;
        bar.id = nextBarId++;
        bar.startNode = ids[i];  // lower level first: local x axis ascending
        bar.endNode = ids[i + 1];
        bar.layerIds = {layer};
        m_bars.push_back(bar);
    }
    m_valid = !m_bars.empty();
    if (!m_valid) m_nodes.clear();
}

std::vector<int> AddColumnCommand::barIds() const { return idsOf(m_bars); }

std::vector<int> AddColumnCommand::createdNodeIds() const {
    std::vector<int> ids;
    for (const Node& n : m_nodes) ids.push_back(n.id);
    return ids;
}

void AddColumnCommand::redo() {
    if (!m_valid) return;
    for (const Node& n : m_nodes) m_project.structure.insertNode(n);
    insertBars(m_project.structure, m_bars);
}

void AddColumnCommand::undo() {
    if (!m_valid) return;
    removeBars(m_project.structure, m_bars);
    for (const Node& n : m_nodes) m_project.structure.removeNode(n.id);
}

// --- DeleteBarsCommand ------------------------------------------------------------------------------------------

DeleteBarsCommand::DeleteBarsCommand(Project& project, std::vector<int> barIds) : m_project(project) {
    std::sort(barIds.begin(), barIds.end());
    barIds.erase(std::unique(barIds.begin(), barIds.end()), barIds.end());
    for (int id : barIds)
        if (const Bar* b = project.structure.findBar(id)) m_bars.push_back(*b);
}

void DeleteBarsCommand::redo() { removeBars(m_project.structure, m_bars); }

void DeleteBarsCommand::undo() { insertBars(m_project.structure, m_bars); }

// --- wrappers ---------------------------------------------------------------------------------------------------

std::vector<int> addBar(Project& project, UndoStack& stack, int startNode, int endNode, bool generateCutNodes) {
    auto cmd = std::make_unique<AddBarCommand>(project, startNode, endNode, generateCutNodes);
    if (!cmd->valid()) return {};
    std::vector<int> ids = cmd->barIds();
    stack.push(std::move(cmd));
    return ids;
}

std::vector<int> addBeamChain(Project& project, UndoStack& stack, const std::vector<int>& nodeIds, bool generateCutNodes) {
    auto cmd = std::make_unique<AddBeamChainCommand>(project, nodeIds, generateCutNodes);
    if (!cmd->valid()) return {};
    std::vector<int> ids = cmd->barIds();
    stack.push(std::move(cmd));
    return ids;
}

std::vector<int> addColumn(Project& project, UndoStack& stack, double x, double y, const std::vector<double>& levelsZ) {
    auto cmd = std::make_unique<AddColumnCommand>(project, x, y, levelsZ);
    if (!cmd->valid()) return {};
    std::vector<int> ids = cmd->barIds();
    stack.push(std::move(cmd));
    return ids;
}

bool deleteBars(Project& project, UndoStack& stack, const std::vector<int>& barIds) {
    auto cmd = std::make_unique<DeleteBarsCommand>(project, barIds);
    if (!cmd->valid()) return false;
    stack.push(std::move(cmd));
    return true;
}

}  // namespace cc::core
