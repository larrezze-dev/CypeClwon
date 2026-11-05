#include "core/import_geometry.h"

#include <algorithm>
#include <map>

#include "core/bar_edit.h"
#include "core/node_edit.h"
#include "core/shell_edit.h"

namespace cc::core {

ImportGeometryCommand::ImportGeometryCommand(Project& project, const ImportedGeometry& g) : SnapshotCommand(project, "Importar geometría") {
    std::map<long long, int> ids;  // file number -> model id
    for (const ImportedNode& n : g.nodes) {
        if (auto existing = findNodeAt(work, n.position)) {
            ids[n.number] = *existing;
        } else if (int id = work.addNode(n.position)) {
            ids[n.number] = id;
            ++m_summary.nodesCreated;
        }
    }
    auto resolve = [&](long long number) {
        const auto it = ids.find(number);
        return it == ids.end() ? 0 : it->second;
    };
    for (const ImportedBar& b : g.bars) {
        const int a = resolve(b.startNumber), e = resolve(b.endNumber);
        bool repeated = false;
        for (const Bar& other : work.bars)
            repeated = repeated || (other.startNode == a && other.endNode == e) || (other.startNode == e && other.endNode == a);
        if (a == 0 || e == 0 || a == e || repeated || work.addBar(a, e) == 0) {
            ++m_summary.skippedBars;
            continue;
        }
        ++m_summary.barsCreated;
    }
    for (const ImportedPolygon& p : g.shells) {
        std::vector<int> nodes;
        bool known = true;
        for (long long number : p.nodeNumbers) {
            const int id = resolve(number);
            known = known && id != 0;
            nodes.push_back(id);
        }
        const auto parts = known ? decomposeContour(work, nodes) : std::vector<std::vector<int>>{};
        bool any = false;
        for (const auto& part : parts) {
            std::vector<int> key = part;
            std::sort(key.begin(), key.end());
            bool repeated = false;
            for (const Shell& s : work.shells) {
                std::vector<int> other = s.nodeIds;
                std::sort(other.begin(), other.end());
                repeated = repeated || other == key;
            }
            if (repeated || checkContour(work, part) != ShellError::None || work.addShell(part, kImportedShellThicknessM) == 0) continue;
            ++m_summary.shellsCreated;
            any = true;
        }
        if (!any) ++m_summary.skippedShells;
    }
    m_summary.ignoredPanels = static_cast<int>(g.panels.size());
    if (m_summary.nodesCreated + m_summary.barsCreated + m_summary.shellsCreated > 0) commit();
}

bool importGeometry(Project& project, UndoStack& stack, const ImportedGeometry& geometry, ImportSummary* summary) {
    auto cmd = std::make_unique<ImportGeometryCommand>(project, geometry);
    if (summary) *summary = cmd->summary();
    if (!cmd->committed()) return false;
    stack.push(std::move(cmd));
    return true;
}

}  // namespace cc::core
