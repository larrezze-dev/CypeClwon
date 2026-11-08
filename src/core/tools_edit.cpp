#include "core/tools_edit.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <map>
#include <set>

#include "core/bar_edit.h"
#include "core/geom3.h"
#include "core/node_edit.h"
#include "core/piece_edit.h"
#include "core/shell_edit.h"

namespace cc::core {

using namespace geom;

// --- transforms ------------------------------------------------------------------------------------------------------

Point3 Transform3::apply(const Point3& p) const {
    return {r[0] * p.x + r[1] * p.y + r[2] * p.z + t.x, r[3] * p.x + r[4] * p.y + r[5] * p.z + t.y,
            r[6] * p.x + r[7] * p.y + r[8] * p.z + t.z};
}

bool Transform3::reflects() const {
    const double det = r[0] * (r[4] * r[8] - r[5] * r[7]) - r[1] * (r[3] * r[8] - r[5] * r[6]) + r[2] * (r[3] * r[7] - r[4] * r[6]);
    return det < 0;
}

Transform3 Transform3::after(const Transform3& f) const {
    Transform3 out;
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j) {
            double v = 0;
            for (int k = 0; k < 3; ++k) v += r[i * 3 + k] * f.r[k * 3 + j];
            out.r[i * 3 + j] = v;
        }
    const Point3 ft = f.t;
    out.t = {r[0] * ft.x + r[1] * ft.y + r[2] * ft.z + t.x, r[3] * ft.x + r[4] * ft.y + r[5] * ft.z + t.y,
             r[6] * ft.x + r[7] * ft.y + r[8] * ft.z + t.z};
    return out;
}

Transform3 translation(const Point3& from, const Point3& to) {
    Transform3 m;
    m.t = sub(to, from);
    return m;
}

std::optional<Transform3> rotationAboutAxis(const Point3& a, const Point3& b, double angleDeg) {
    const Point3 k = unit(sub(b, a));
    if (length(k) == 0 || !std::isfinite(angleDeg)) return std::nullopt;
    const double th = angleDeg * 3.14159265358979323846 / 180.0;
    const double c = std::cos(th), s = std::sin(th), v = 1 - c;
    Transform3 m;
    // Rodrigues: R = c I + s [k]x + (1 - c) k k^T
    const double R[9] = {c + k.x * k.x * v,       k.x * k.y * v - k.z * s, k.x * k.z * v + k.y * s,
                         k.y * k.x * v + k.z * s, c + k.y * k.y * v,       k.y * k.z * v - k.x * s,
                         k.z * k.x * v - k.y * s, k.z * k.y * v + k.x * s, c + k.z * k.z * v};
    for (int i = 0; i < 9; ++i) m.r[i] = R[i];
    const Point3 rot = {R[0] * a.x + R[1] * a.y + R[2] * a.z, R[3] * a.x + R[4] * a.y + R[5] * a.z, R[6] * a.x + R[7] * a.y + R[8] * a.z};
    m.t = sub(a, rot);  // p' = R (p - a) + a
    return m;
}

Transform3 rotationInXY(const Point3& origin, double angleDeg) {
    return *rotationAboutAxis(origin, {origin.x, origin.y, origin.z + 1.0}, angleDeg);
}

std::optional<Transform3> reflectionAcrossPlane(const Point3& a, const Point3& b, const Point3& c) {
    const Point3 n = unit(cross(sub(b, a), sub(c, a)));
    if (length(n) == 0) return std::nullopt;
    Transform3 m;
    const double R[9] = {1 - 2 * n.x * n.x, -2 * n.x * n.y,     -2 * n.x * n.z,     -2 * n.y * n.x,     1 - 2 * n.y * n.y,
                         -2 * n.y * n.z,    -2 * n.z * n.x,     -2 * n.z * n.y,     1 - 2 * n.z * n.z};
    for (int i = 0; i < 9; ++i) m.r[i] = R[i];
    m.t = scale(n, 2 * dot(n, a));
    return m;
}

std::optional<Transform3> reflectionVertical(const Point3& a, const Point3& b) {
    return reflectionAcrossPlane(a, b, {a.x, a.y, a.z + 1.0});
}

std::string toolErrorMessage(ToolError error) {
    switch (error) {
        case ToolError::None: return {};
        case ToolError::NothingSelected: return "No hay elementos seleccionados.";
        case ToolError::UnknownElement: return "Algún elemento seleccionado no existe.";
        case ToolError::InvalidTransform: return "La transformación indicada no es válida.";
        case ToolError::InvalidResult: return "La operación deja una barra o una lámina con una geometría no válida.";
        case ToolError::NothingCopied: return "No se ha creado ningún elemento nuevo.";
    }
    return {};
}

namespace {

bool finite(const Point3& p) { return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z); }

std::vector<int> uniqueSorted(std::vector<int> v) {
    std::sort(v.begin(), v.end());
    v.erase(std::unique(v.begin(), v.end()), v.end());
    return v;
}

int barBetween(const StructuralModel& m, int a, int b) {
    for (const Bar& bar : m.bars)
        if ((bar.startNode == a && bar.endNode == b) || (bar.startNode == b && bar.endNode == a)) return bar.id;
    return 0;
}

/// Splits the pieces whose chain is no longer straight into one-bar pieces (type and group kept; the first keeps the id).
void splitBrokenPieces(StructuralModel& m) {
    std::vector<Piece> pieces = m.pieces;
    bool changed = false;
    for (const Piece& p : pieces) {
        if (p.barIds.size() < 2) continue;
        const Point3 s = m.findNode(p.nodeIds.front())->position, e = m.findNode(p.nodeIds.back())->position;
        const Point3 d = sub(e, s);
        bool straight = length(d) > 0;
        for (std::size_t i = 1; straight && i + 1 < p.nodeIds.size(); ++i) {
            const Point3 v = sub(m.findNode(p.nodeIds[i])->position, s);
            straight = length(cross(v, d)) / length(d) <= kPieceAlignmentToleranceM && dot(v, d) > 0;
        }
        if (straight) continue;
        changed = true;
        m.removePiece(p.id);
        for (std::size_t i = 0; i < p.barIds.size(); ++i) {
            Piece part;
            part.id = i == 0 ? p.id : m.nextPieceId;
            part.barIds = {p.barIds[i]};
            part.nodeIds = {p.nodeIds[i], p.nodeIds[i + 1]};
            part.type = p.type;
            part.groupId = p.groupId;
            if (part.type == ElementType::Generic && part.groupId == 0) continue;  // an implicit one-bar piece needs no record
            m.insertPiece(part);
        }
    }
    (void)changed;
}

bool contourReversedXEdge(Shell& s) {
    std::reverse(s.nodeIds.begin(), s.nodeIds.end());
    std::swap(s.xEdgeStart, s.xEdgeEnd);
    return true;
}

}  // namespace

// --- Mover / Copiar --------------------------------------------------------------------------------------------------

TransformElementsCommand::TransformElementsCommand(Project& project, const ToolSelection& sel, const Transform3& tf, bool copy, int copies)
    : SnapshotCommand(project, copy ? "Copiar elementos" : "Mover elementos") {
    const Point3 probe = tf.apply({1, 2, 3});
    if (!finite(probe) || !finite(tf.t) || copies < 1 || (!copy && copies != 1)) {
        m_error = ToolError::InvalidTransform;
        return;
    }
    if (sel.nodeIds.empty() && sel.barIds.empty() && sel.shellIds.empty()) {
        m_error = ToolError::NothingSelected;
        return;
    }
    const std::vector<int> barIds = uniqueSorted(sel.barIds), shellIds = uniqueSorted(sel.shellIds);
    std::set<int> closure(sel.nodeIds.begin(), sel.nodeIds.end());
    for (int id : sel.nodeIds)
        if (!work.findNode(id)) {
            m_error = ToolError::UnknownElement;
            return;
        }
    for (int id : barIds) {
        const Bar* b = work.findBar(id);
        if (!b) {
            m_error = ToolError::UnknownElement;
            return;
        }
        closure.insert(b->startNode);
        closure.insert(b->endNode);
    }
    for (int id : shellIds) {
        const Shell* s = work.findShell(id);
        if (!s) {
            m_error = ToolError::UnknownElement;
            return;
        }
        closure.insert(s->nodeIds.begin(), s->nodeIds.end());
    }

    if (!copy) {
        const StructuralModel before = work;
        for (int id : closure) work.findNode(id)->position = tf.apply(before.findNode(id)->position);
        std::set<int> touchedBars, touchedShells;
        for (int id : closure) {
            for (int b : work.barsUsingNode(id)) touchedBars.insert(b);
            for (int s : work.shellsUsingNode(id)) touchedShells.insert(s);
        }
        for (int b : touchedBars)
            if (work.barLength(b) < kMinBarLengthM) {
                m_error = ToolError::InvalidResult;
                return;
            }
        for (int sid : touchedShells) {
            Shell* s = work.findShell(sid);
            const bool whole = std::all_of(s->nodeIds.begin(), s->nodeIds.end(), [&](int n) { return closure.count(n) != 0; });
            if (whole && tf.reflects()) contourReversedXEdge(*s);
            if (checkContour(work, s->nodeIds) != ShellError::None) {
                m_error = ToolError::InvalidResult;
                return;
            }
        }
        splitBrokenPieces(work);
        commit();
        return;
    }

    // --- copy ---
    const std::vector<int> originalNodes(closure.begin(), closure.end());
    const std::vector<Piece> originalPieces = work.pieces;
    Transform3 current;  // identity
    for (int k = 1; k <= copies; ++k) {
        current = tf.after(current);
        std::map<int, int> nodeMap;
        for (int id : originalNodes) {
            const Point3 p = current.apply(work.findNode(id)->position);
            if (const auto existing = findNodeAt(work, p)) {
                nodeMap[id] = *existing;
            } else {
                nodeMap[id] = work.addNode(p);
                m_nodes.push_back(nodeMap[id]);
            }
        }
        std::map<int, int> barMap;
        for (int id : barIds) {
            Bar copyBar = *work.findBar(id);
            const int a = nodeMap[copyBar.startNode], b = nodeMap[copyBar.endNode];
            if (a == b || barBetween(work, a, b) != 0) continue;
            copyBar.id = work.nextBarId;
            copyBar.startNode = a;
            copyBar.endNode = b;
            if (!work.insertBar(copyBar)) continue;
            barMap[id] = copyBar.id;
            m_bars.push_back(copyBar.id);
        }
        for (int id : shellIds) {
            Shell copyShell = *work.findShell(id);
            std::vector<int> nodes;
            for (int n : copyShell.nodeIds) nodes.push_back(nodeMap[n]);
            const int xa = copyShell.xEdgeStart != 0 ? nodeMap[copyShell.xEdgeStart] : 0;
            const int xb = copyShell.xEdgeEnd != 0 ? nodeMap[copyShell.xEdgeEnd] : 0;
            if (tf.reflects()) {
                std::reverse(nodes.begin(), nodes.end());
                copyShell.xEdgeStart = xb;
                copyShell.xEdgeEnd = xa;
            } else {
                copyShell.xEdgeStart = xa;
                copyShell.xEdgeEnd = xb;
            }
            std::vector<int> key = nodes;
            std::sort(key.begin(), key.end());
            bool repeated = false;
            for (const Shell& other : work.shells) {
                std::vector<int> o = other.nodeIds;
                std::sort(o.begin(), o.end());
                repeated = repeated || o == key;
            }
            if (repeated || checkContour(work, nodes) != ShellError::None) continue;
            copyShell.id = work.nextShellId;
            copyShell.nodeIds = nodes;
            if (!work.insertShell(copyShell)) continue;
            m_shells.push_back(copyShell.id);
        }
        // Pieces whose bars were all copied.
        std::map<int, std::vector<int>> groupMembers;  // old group -> new piece ids
        for (const Piece& p : originalPieces) {
            bool all = true;
            for (int b : p.barIds) all = all && barMap.count(b) != 0;
            if (!all) continue;
            Piece np;
            np.id = work.nextPieceId;
            np.type = p.type;
            for (int b : p.barIds) np.barIds.push_back(barMap[b]);
            for (int n : p.nodeIds) np.nodeIds.push_back(nodeMap[n]);
            if (!work.insertPiece(np)) continue;
            if (p.groupId != 0) groupMembers[p.groupId].push_back(np.id);
        }
        for (const auto& [oldGroup, members] : groupMembers) {
            if (members.size() < 2) continue;
            for (int id : members)
                for (Piece& p : work.pieces)
                    if (p.id == id) p.groupId = members.front();
        }
    }
    if (m_nodes.empty() && m_bars.empty() && m_shells.empty()) {
        m_error = ToolError::NothingCopied;
        return;
    }
    commit();
}

ToolError transformElements(Project& project, UndoStack& stack, const ToolSelection& selection, const Transform3& transform, bool copy,
                            int copies) {
    auto cmd = std::make_unique<TransformElementsCommand>(project, selection, transform, copy, copies);
    if (!cmd->valid()) return cmd->error();
    stack.push(std::move(cmd));
    return ToolError::None;
}

// --- Reparar ---------------------------------------------------------------------------------------------------------

std::string repairConfirmationMessage() {
    return "El programa analizará a continuación la geometría del modelo e intentará corregirla, si fuera necesario. ¿Desea continuar?";
}

std::string repairResultMessage(bool modified) {
    return modified ? "El modelo ha sido modificado, por favor, revíselo antes de continuar."
                    : "No se han realizado modificaciones en el modelo.";
}

namespace {

std::string formatDistance(double metres) {
    char buf[32];
    std::snprintf(buf, sizeof buf, "%.4g", metres);
    std::string s = buf;
    std::replace(s.begin(), s.end(), '.', ',');
    return s + " m";
}

/// Distance from p to segment a-b; `along` receives the parameter (0..1 inside the segment) of the foot point.
double segmentDistance(const Point3& p, const Point3& a, const Point3& b, double* along = nullptr) {
    const Point3 d = sub(b, a);
    const double l2 = dot(d, d);
    double u = l2 > 0 ? dot(sub(p, a), d) / l2 : 0;
    if (along) *along = u;
    u = std::clamp(u, 0.0, 1.0);
    return length(sub(p, add(a, scale(d, u))));
}

std::string text(int id) { return std::to_string(id); }

std::string replaceNext(std::string s, const std::string& value) {
    const auto at = s.find("%s");
    if (at != std::string::npos) s.replace(at, 2, value);
    return s;
}

std::string fill(std::string pattern, const std::vector<std::string>& values) {
    for (const std::string& v : values) pattern = replaceNext(pattern, v);
    return pattern;
}

bool pieceConsistent(const StructuralModel& m, const Piece& p) {
    if (p.barIds.empty() || p.nodeIds.size() != p.barIds.size() + 1) return false;
    for (std::size_t i = 0; i < p.barIds.size(); ++i) {
        const Bar* b = m.findBar(p.barIds[i]);
        if (!b) return false;
        const bool fwd = b->startNode == p.nodeIds[i] && b->endNode == p.nodeIds[i + 1];
        const bool bwd = b->endNode == p.nodeIds[i] && b->startNode == p.nodeIds[i + 1];
        if (!fwd && !bwd) return false;
    }
    return true;
}

}  // namespace

std::vector<RepairIssue> analyseGeometry(const StructuralModel& m, double tol) {
    std::vector<RepairIssue> out;
    // #399480 nodes too close.
    for (std::size_t i = 0; i < m.nodes.size(); ++i)
        for (std::size_t j = i + 1; j < m.nodes.size(); ++j) {
            const double d = distance(m.nodes[i].position, m.nodes[j].position);
            if (d <= tol)
                out.push_back({RepairIssue::Kind::NodesTooClose,
                               fill("Los nudos %s y %s están demasiado próximos entre sí (distancia: %s).",
                                    {text(m.nodes[i].id), text(m.nodes[j].id), formatDistance(d)}),
                               {m.nodes[i].id, m.nodes[j].id}});
        }
    // #400347 node on a bar, #400346 overlapped bars.
    for (const Bar& b : m.bars) {
        const Point3 s = m.findNode(b.startNode)->position, e = m.findNode(b.endNode)->position;
        const double len = distance(s, e);
        for (const Node& n : m.nodes) {
            if (n.id == b.startNode || n.id == b.endNode) continue;
            double u = 0;
            const double d = segmentDistance(n.position, s, e, &u);
            if (d <= tol && u * len > tol && (1 - u) * len > tol)
                out.push_back({RepairIssue::Kind::NodeOnBar,
                               fill("El nudo '%s' está demasiado próximo a la barra '%s' (distancia: %s).",
                                    {text(n.id), text(b.id), formatDistance(d)}),
                               {n.id, b.id}});
        }
    }
    for (std::size_t i = 0; i < m.bars.size(); ++i)
        for (std::size_t j = i + 1; j < m.bars.size(); ++j) {
            const Bar &a = m.bars[i], &b = m.bars[j];
            const Point3 as = m.findNode(a.startNode)->position, ae = m.findNode(a.endNode)->position;
            const Point3 bs = m.findNode(b.startNode)->position, be = m.findNode(b.endNode)->position;
            const double la = distance(as, ae);
            if (segmentDistance(bs, as, ae) > tol || segmentDistance(be, as, ae) > tol) {
                // b may still contain a: the overlap is symmetric, so test the other direction too.
                if (segmentDistance(as, bs, be) > tol || segmentDistance(ae, bs, be) > tol) continue;
            }
            const Point3 d = unit(sub(ae, as));
            const double t0 = dot(sub(bs, as), d), t1 = dot(sub(be, as), d);
            const double overlap = std::min(la, std::max(t0, t1)) - std::max(0.0, std::min(t0, t1));
            if (overlap > tol)
                out.push_back({RepairIssue::Kind::BarsOverlap, fill("Las barras '%s' y '%s' están solapadas.", {text(a.id), text(b.id)}),
                               {a.id, b.id}});
        }
    // #400344 node on a shell edge.
    for (const Shell& sh : m.shells)
        for (std::size_t k = 0; k < sh.nodeIds.size(); ++k) {
            const Point3 a = m.findNode(sh.nodeIds[k])->position, b = m.findNode(sh.nodeIds[(k + 1) % sh.nodeIds.size()])->position;
            const double len = distance(a, b);
            for (const Node& n : m.nodes) {
                if (std::find(sh.nodeIds.begin(), sh.nodeIds.end(), n.id) != sh.nodeIds.end()) continue;
                double u = 0;
                const double d = segmentDistance(n.position, a, b, &u);
                if (d <= tol && u * len > tol && (1 - u) * len > tol)
                    out.push_back({RepairIssue::Kind::NodeOnShellEdge,
                                   fill("El nudo '%s' está demasiado próximo a la arista (distancia: %s).", {text(n.id), formatDistance(d)}),
                                   {n.id, sh.id}});
            }
        }
    // #400345 nodes without bars or shells (the model has no loads or restraints yet).
    for (const Node& n : m.nodes)
        if (m.barsUsingNode(n.id).empty() && m.shellsUsingNode(n.id).empty())
            out.push_back({RepairIssue::Kind::UnneededNode, "El nudo no es necesario ya que no tiene cargas ni vinculación exterior.", {n.id}});
    return out;
}

RepairModelCommand::RepairModelCommand(Project& project, double tol) : SnapshotCommand(project, "Reparar") {
    m_report.issues = analyseGeometry(work, tol);

    // 1. Merge coincident nodes (single linkage, lowest id wins).
    {
        std::map<int, int> parent;
        for (const Node& n : work.nodes) parent[n.id] = n.id;
        auto root = [&](int x) {
            while (parent[x] != x) x = parent[x];
            return x;
        };
        for (std::size_t i = 0; i < work.nodes.size(); ++i)
            for (std::size_t j = i + 1; j < work.nodes.size(); ++j)
                if (distance(work.nodes[i].position, work.nodes[j].position) <= tol) {
                    const int a = root(work.nodes[i].id), b = root(work.nodes[j].id);
                    if (a != b) parent[std::max(a, b)] = std::min(a, b);
                }
        std::map<int, int> remap;
        for (const Node& n : work.nodes)
            if (root(n.id) != n.id) remap[n.id] = root(n.id);
        nodeRemap = remap;
        if (!remap.empty()) {
            auto mapped = [&](int id) {
                const auto it = remap.find(id);
                return it == remap.end() ? id : it->second;
            };
            for (Bar& b : work.bars) {
                b.startNode = mapped(b.startNode);
                b.endNode = mapped(b.endNode);
            }
            for (Piece& p : work.pieces)
                for (int& n : p.nodeIds) n = mapped(n);
            std::vector<int> collapsed;
            for (const Bar& b : work.bars)
                if (b.startNode == b.endNode) collapsed.push_back(b.id);
            for (int id : collapsed) {
                work.removeBar(id);
                ++m_report.removedBars;
            }
            std::vector<int> deadShells;
            for (Shell& s : work.shells) {
                std::vector<int> nodes;
                for (int n : s.nodeIds) {
                    const int m = mapped(n);
                    if (nodes.empty() || nodes.back() != m) nodes.push_back(m);
                }
                if (nodes.size() > 1 && nodes.front() == nodes.back()) nodes.pop_back();
                s.xEdgeStart = mapped(s.xEdgeStart);
                s.xEdgeEnd = mapped(s.xEdgeEnd);
                if (nodes.size() != s.nodeIds.size() || nodes != s.nodeIds) {
                    s.nodeIds = nodes;
                }
                std::set<int> distinct(nodes.begin(), nodes.end());
                if (distinct.size() != nodes.size() || nodes.size() < 3) deadShells.push_back(s.id);
            }
            for (int id : deadShells) {
                work.removeShell(id);
                ++m_report.removedShells;
            }
            for (const auto& [gone, keep] : remap) {
                (void)keep;
                auto it = std::find_if(work.nodes.begin(), work.nodes.end(), [&](const Node& n) { return n.id == gone; });
                if (it != work.nodes.end()) work.nodes.erase(it);
                ++m_report.mergedNodes;
            }
            // Shells that no longer hold a valid contour after the merge are removed too.
            std::vector<int> bad;
            for (const Shell& s : work.shells)
                if (checkContour(work, s.nodeIds) != ShellError::None) bad.push_back(s.id);
            for (int id : bad) {
                work.removeShell(id);
                ++m_report.removedShells;
            }
            work.pieces.erase(std::remove_if(work.pieces.begin(), work.pieces.end(), [&](const Piece& p) { return !pieceConsistent(work, p); }),
                              work.pieces.end());
        }
    }

    // 2. Divide bars at the nodes lying on them.
    {
        const std::vector<Bar> bars = work.bars;
        for (const Bar& original : bars) {
            const Bar* current = work.findBar(original.id);
            if (!current) continue;
            const Point3 s = work.findNode(current->startNode)->position, e = work.findNode(current->endNode)->position;
            const double len = distance(s, e);
            std::vector<std::pair<double, int>> cuts;
            for (const Node& n : work.nodes) {
                if (n.id == current->startNode || n.id == current->endNode) continue;
                double u = 0;
                if (segmentDistance(n.position, s, e, &u) <= tol && u * len > tol && (1 - u) * len > tol) cuts.push_back({u, n.id});
            }
            if (cuts.empty()) continue;
            std::sort(cuts.begin(), cuts.end());
            std::vector<int> chain{current->startNode};
            for (const auto& c : cuts) chain.push_back(c.second);
            chain.push_back(current->endNode);
            // Stretches S0 (keeps the id) .. Sk.
            Bar proto = *current;
            std::vector<int> stretchIds{proto.id};
            work.findBar(proto.id)->endNode = chain[1];
            for (std::size_t i = 1; i + 1 < chain.size(); ++i) {
                Bar nb = proto;
                nb.id = work.nextBarId;
                nb.startNode = chain[i];
                nb.endNode = chain[i + 1];
                work.insertBar(nb);
                stretchIds.push_back(nb.id);
                ++m_report.splitBars;
            }
            // Pieces.
            if (const Piece* found = work.pieceOfBar(proto.id)) {
                Piece piece = *found;
                const std::size_t at = static_cast<std::size_t>(std::find(piece.barIds.begin(), piece.barIds.end(), proto.id) - piece.barIds.begin());
                const bool forward = piece.nodeIds[at] == proto.startNode;
                std::vector<int> bs = stretchIds, ns = chain;
                if (!forward) {
                    std::reverse(bs.begin(), bs.end());
                    std::reverse(ns.begin(), ns.end());
                }
                piece.barIds.erase(piece.barIds.begin() + static_cast<std::ptrdiff_t>(at));
                piece.barIds.insert(piece.barIds.begin() + static_cast<std::ptrdiff_t>(at), bs.begin(), bs.end());
                piece.nodeIds.erase(piece.nodeIds.begin() + static_cast<std::ptrdiff_t>(at), piece.nodeIds.begin() + static_cast<std::ptrdiff_t>(at) + 2);
                piece.nodeIds.insert(piece.nodeIds.begin() + static_cast<std::ptrdiff_t>(at), ns.begin(), ns.end());
                work.removePiece(piece.id);
                work.insertPiece(piece);
            } else {
                Piece piece;
                piece.id = work.nextPieceId;
                piece.barIds = stretchIds;
                piece.nodeIds = chain;
                work.insertPiece(piece);
            }
        }
    }

    // 3. Repeated bars (same node pair): keep the lowest id.
    {
        std::map<std::pair<int, int>, int> seen;
        std::vector<int> repeated;
        for (const Bar& b : work.bars) {
            const auto key = std::make_pair(std::min(b.startNode, b.endNode), std::max(b.startNode, b.endNode));
            if (!seen.emplace(key, b.id).second) repeated.push_back(b.id);
        }
        for (int id : repeated) {
            work.removeBar(id);
            ++m_report.removedBars;
        }
    }

    // 4. Nodes lying on shell edges go into the contour.
    {
        const std::vector<int> shellIds = [&] {
            std::vector<int> ids;
            for (const Shell& s : work.shells) ids.push_back(s.id);
            return ids;
        }();
        for (int sid : shellIds) {
            const Shell old = *work.findShell(sid);
            std::vector<int> nodes;
            int inserted = 0;
            Shell updated = old;
            for (std::size_t k = 0; k < old.nodeIds.size(); ++k) {
                const int a = old.nodeIds[k], b = old.nodeIds[(k + 1) % old.nodeIds.size()];
                nodes.push_back(a);
                const Point3 pa = work.findNode(a)->position, pb = work.findNode(b)->position;
                const double len = distance(pa, pb);
                std::vector<std::pair<double, int>> cuts;
                for (const Node& n : work.nodes) {
                    if (std::find(old.nodeIds.begin(), old.nodeIds.end(), n.id) != old.nodeIds.end()) continue;
                    double u = 0;
                    if (segmentDistance(n.position, pa, pb, &u) <= tol && u * len > tol && (1 - u) * len > tol) cuts.push_back({u, n.id});
                }
                std::sort(cuts.begin(), cuts.end());
                for (const auto& c : cuts) {
                    nodes.push_back(c.second);
                    ++inserted;
                }
                if (!cuts.empty() && old.xEdgeStart == a && old.xEdgeEnd == b) updated.xEdgeEnd = cuts.front().second;
            }
            if (inserted == 0) continue;
            const auto parts = decomposeContour(work, nodes);
            bool ok = !parts.empty();
            for (const auto& part : parts) ok = ok && checkContour(work, part) == ShellError::None;
            if (!ok) continue;
            std::vector<int> resultIds{sid};
            Shell* keep = work.findShell(sid);
            *keep = updated;
            keep->nodeIds = parts[0];
            for (std::size_t p = 1; p < parts.size(); ++p) {
                Shell extra = updated;
                extra.id = work.nextShellId;
                extra.nodeIds = parts[p];
                work.insertShell(extra);
                resultIds.push_back(extra.id);
            }
            // The x edge survives only where it is still a consecutive pair.
            for (int rid : resultIds) {
                Shell& s = *work.findShell(rid);
                bool found = false;
                for (std::size_t i = 0; i < s.nodeIds.size(); ++i)
                    found = found || (s.nodeIds[i] == s.xEdgeStart && s.nodeIds[(i + 1) % s.nodeIds.size()] == s.xEdgeEnd);
                if (!found) s.xEdgeStart = s.xEdgeEnd = 0;
            }
            m_report.splitShellEdges += inserted;
        }
    }

    if (m_report.modified()) {
        work.pieces.erase(std::remove_if(work.pieces.begin(), work.pieces.end(), [&](const Piece& p) { return !pieceConsistent(work, p); }),
                          work.pieces.end());
        commit();
    }
}

bool repairModel(Project& project, UndoStack& stack, RepairReport* report, double toleranceM) {
    auto cmd = std::make_unique<RepairModelCommand>(project, toleranceM);
    if (report) *report = cmd->report();
    if (!cmd->committed()) return false;
    stack.push(std::move(cmd));
    return true;
}

}  // namespace cc::core
