#include "core/shell_edit.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <set>

#include "core/bar_edit.h"
#include "core/geom3.h"
#include "core/node_edit.h"

namespace cc::core {

namespace {

using namespace geom;

struct P2 {
    double a = 0, b = 0;
};

int dominantAxis(const Point3& n) {
    const double ax = std::fabs(n.x), ay = std::fabs(n.y), az = std::fabs(n.z);
    if (ax >= ay && ax >= az) return 0;
    return ay >= az ? 1 : 2;
}

/// Orientation-preserving projection that drops the dominant axis of the normal.
P2 to2(const Point3& p, int axis) {
    if (axis == 0) return {p.y, p.z};
    if (axis == 1) return {p.z, p.x};
    return {p.x, p.y};
}

double cross2(const P2& o, const P2& a, const P2& b) { return (a.a - o.a) * (b.b - o.b) - (a.b - o.b) * (b.a - o.a); }

bool segmentsCross(const P2& p1, const P2& p2, const P2& p3, const P2& p4) {
    const double d1 = cross2(p3, p4, p1), d2 = cross2(p3, p4, p2), d3 = cross2(p1, p2, p3), d4 = cross2(p1, p2, p4);
    return d1 * d2 < 0 && d3 * d4 < 0;
}

std::vector<Point3> positions(const StructuralModel& m, const std::vector<int>& ids) {
    std::vector<Point3> out;
    for (int id : ids)
        if (const Node* n = m.findNode(id)) out.push_back(n->position);
    return out;
}

bool sortedUnique(std::vector<int>& v) {
    std::sort(v.begin(), v.end());
    const bool dup = std::adjacent_find(v.begin(), v.end()) != v.end();
    v.erase(std::unique(v.begin(), v.end()), v.end());
    return !dup;
}

/// Node at `p` (within kShellDivideToleranceM) or a new one.
int nodeAt(StructuralModel& m, const Point3& p) {
    if (auto id = findNodeAt(m, p, kShellDivideToleranceM)) return *id;
    return m.addNode(p);
}

/// Keeps the "Definir eje x" edge when it is still a consecutive pair of the new contour.
void adoptXEdge(Shell& s, int a, int b) {
    s.xEdgeStart = s.xEdgeEnd = 0;
    if (a == 0 || b == 0) return;
    const std::size_t n = s.nodeIds.size();
    for (std::size_t i = 0; i < n; ++i)
        if (s.nodeIds[i] == a && s.nodeIds[(i + 1) % n] == b) {
            s.xEdgeStart = a;
            s.xEdgeEnd = b;
        }
}

}  // namespace

std::string shellErrorMessage(ShellError error) {
    switch (error) {
        case ShellError::None: return {};
        case ShellError::InvalidContour: return "Los puntos introducidos no permiten definir correctamente la lámina.";
        case ShellError::NotCoplanar: return "Las láminas seleccionadas no están contenidas en el mismo plano.";
        case ShellError::NotClosedRegion:
            return "No es posible unir las láminas seleccionadas porque no delimitan una región cerrada sin huecos.";
        case ShellError::TooFewShells:
            return "Debe seleccionar, al menos, dos láminas situadas en el mismo plano para poder realizar la unión.";
        case ShellError::UnknownElement: return "El nudo o la lámina indicados no existen.";
        case ShellError::NothingToDivide: return "El segmento no atraviesa ninguna de las láminas seleccionadas.";
        case ShellError::CannotDivide: return "No es posible dividir la lámina con ese segmento.";
        case ShellError::ContourTooComplex: return "El contorno de la lámina resultante tiene más de cuatro nudos.";
        case ShellError::InvalidMove: return "El desplazamiento deja una barra o una lámina con una geometría no válida.";
        case ShellError::InvalidEdge: return "Los nudos indicados no forman una arista de la lámina.";
        case ShellError::InvalidThickness: return rangeWarning("Espesor", kShellThicknessMm, 2);
        case ShellError::InvalidBallast: return rangeWarning("Módulo de balasto en Z local", kShellBallastKnM3, 2);
        case ShellError::InvalidMaterial: return "El material de la lámina no es válido.";
        case ShellError::InvalidOffset: return rangeWarning("Z", kShellOffsetMm, 0);
        case ShellError::NothingToChange: return "No hay nada que modificar.";
    }
    return {};
}

bool pointsCoplanar(const std::vector<Point3>& pts, double tol) {
    if (pts.size() <= 3) return true;
    const Point3& p0 = pts[0];
    std::size_t far = 0;
    double best = 0;
    for (std::size_t i = 1; i < pts.size(); ++i) {
        const double d = length(sub(pts[i], p0));
        if (d > best) best = d, far = i;
    }
    if (best < 1e-12) return true;
    const Point3 e = sub(pts[far], p0);
    Point3 normal;
    double bestCross = 0;
    for (const Point3& p : pts) {
        const Point3 c = cross(e, sub(p, p0));
        if (length(c) > bestCross) bestCross = length(c), normal = c;
    }
    if (bestCross < 1e-12) return true;  // collinear
    normal = unit(normal);
    for (const Point3& p : pts)
        if (std::fabs(dot(normal, sub(p, p0))) > tol) return false;
    return true;
}

ShellError checkContour(const StructuralModel& m, const std::vector<int>& ids) {
    if (ids.size() != 3 && ids.size() != 4) return ShellError::InvalidContour;
    for (std::size_t i = 0; i < ids.size(); ++i) {
        if (!m.findNode(ids[i])) return ShellError::InvalidContour;
        for (std::size_t j = 0; j < i; ++j)
            if (ids[i] == ids[j]) return ShellError::InvalidContour;
    }
    const std::vector<Point3> pts = positions(m, ids);
    const Point3 n = newell(pts);
    if (length(n) < 1e-9) return ShellError::InvalidContour;
    if (!pointsCoplanar(pts)) return ShellError::NotCoplanar;
    if (pts.size() == 4) {
        const int axis = dominantAxis(n);
        P2 q[4];
        for (int i = 0; i < 4; ++i) q[i] = to2(pts[static_cast<std::size_t>(i)], axis);
        if (segmentsCross(q[0], q[1], q[2], q[3]) || segmentsCross(q[1], q[2], q[3], q[0])) return ShellError::InvalidContour;
    }
    return ShellError::None;
}

std::vector<std::vector<int>> decomposeContour(const StructuralModel& m, const std::vector<int>& ids) {
    if (ids.size() <= 4) return {ids};
    std::vector<int> r = ids;
    std::vector<std::vector<int>> out;
    const Point3 normal = newell(positions(m, ids));
    if (length(normal) < 1e-9) return {};
    const int axis = dominantAxis(normal);
    auto pt = [&](int id) { return to2(m.findNode(id)->position, axis); };
    // Orientation sign of the polygon in the projected plane.
    double area = 0;
    for (std::size_t i = 0; i < r.size(); ++i) {
        const P2 a = pt(r[i]), b = pt(r[(i + 1) % r.size()]);
        area += a.a * b.b - b.a * a.b;
    }
    const double sign = area >= 0 ? 1.0 : -1.0;
    while (r.size() > 4) {
        bool found = false;
        const std::size_t n = r.size();
        for (std::size_t i = 0; i < n && !found; ++i) {
            const int prev = r[(i + n - 1) % n], cur = r[i], next = r[(i + 1) % n];
            const P2 a = pt(prev), b = pt(cur), c = pt(next);
            const double turn = cross2(a, b, c) * sign;
            if (turn <= 1e-12) continue;  // reflex or flat corner
            bool inside = false;
            for (std::size_t k = 0; k < n && !inside; ++k) {
                if (r[k] == prev || r[k] == cur || r[k] == next) continue;
                const P2 p = pt(r[k]);
                const double s1 = cross2(a, b, p) * sign, s2 = cross2(b, c, p) * sign, s3 = cross2(c, a, p) * sign;
                inside = s1 >= -1e-12 && s2 >= -1e-12 && s3 >= -1e-12;
            }
            if (inside) continue;
            out.push_back({prev, cur, next});
            r.erase(r.begin() + static_cast<std::ptrdiff_t>(i));
            found = true;
        }
        if (!found) return {};
    }
    out.push_back(r);
    return out;
}

std::optional<ShellAxes> shellLocalAxes(const StructuralModel& m, const Shell& s) {
    const std::vector<Point3> pts = positions(m, s.nodeIds);
    if (pts.size() != s.nodeIds.size() || pts.size() < 3) return std::nullopt;
    const Point3 n = unit(newell(pts));
    if (length(n) == 0) return std::nullopt;
    ShellAxes ax;
    ax.z = s.zAxisInverted ? scale(n, -1.0) : n;
    Point3 x;
    const std::size_t count = s.nodeIds.size();
    for (std::size_t i = 0; i < count && length(x) == 0; ++i) {
        const int a = s.nodeIds[i], b = s.nodeIds[(i + 1) % count];
        if (s.xEdgeStart == a && s.xEdgeEnd == b) x = unit(sub(pts[(i + 1) % count], pts[i]));
        else if (s.xEdgeStart == b && s.xEdgeEnd == a) x = unit(sub(pts[i], pts[(i + 1) % count]));
    }
    if (s.xEdgeStart == 0 && s.xEdgeEnd == 0) x = Point3{};
    if (length(x) == 0) {
        const Point3 gx{1, 0, 0};
        x = unit(sub(gx, scale(n, dot(gx, n))));
        if (length(x) == 0) {
            const Point3 gy{0, 1, 0};
            x = unit(sub(gy, scale(n, dot(gy, n))));
        }
    }
    ax.x = x;
    ax.y = cross(ax.z, ax.x);
    return ax;
}

std::vector<Point3> shellDrawnCorners(const StructuralModel& m, const Shell& s) {
    std::vector<Point3> pts = positions(m, s.nodeIds);
    if (pts.size() != s.nodeIds.size()) return {};
    const double offset = s.layout.centralPlaneOffsetM;
    if (offset == 0.0) return pts;
    const std::optional<ShellAxes> axes = shellLocalAxes(m, s);
    if (!axes) return pts;
    for (Point3& p : pts) p = add(p, scale(axes->z, offset));
    return pts;
}

// ---------------------------------------------------------------------------------------------------------------------

namespace {
/// Range checks shared by the commands; the thickness is in metres.
ShellError checkSection(const ShellSection& section) {
    // A little slack: mm -> m -> mm conversions must not reject the bounds. Also refuses NaN.
    const double mm = section.thicknessM * 1000.0;
    if (!(mm > kShellThicknessMm.min - 1e-9 && mm < kShellThicknessMm.max + 1e-9)) return ShellError::InvalidThickness;
    if (section.ballastEnabled && !kShellBallastKnM3.contains(section.ballastKnM3)) return ShellError::InvalidBallast;
    return ShellError::None;
}

ShellError checkLayout(const ShellLayout& layout) {
    const double mm = layout.centralPlaneOffsetM * 1000.0;
    return mm >= kShellOffsetMm.min - 1e-9 && mm <= kShellOffsetMm.max + 1e-9 ? ShellError::None : ShellError::InvalidOffset;
}

void applySection(Shell& s, const ShellSection& section) {
    s.thicknessM = section.thicknessM;
    s.ballastEnabled = section.ballastEnabled;
    if (section.ballastEnabled) {
        s.ballastKnM3 = section.ballastKnM3;
        s.ballastBehaviour = section.ballastBehaviour;
    }
}
}  // namespace

AddShellCommand::AddShellCommand(Project& project, const std::vector<int>& nodeIds, double thicknessM, int materialId,
                                 const ShellSection* section, const ShellMaterial* material, const ShellLayout* layout)
    : SnapshotCommand(project, "Nueva lámina") {
    if (section) thicknessM = section->thicknessM;
    if (section) {
        if (const ShellError e = checkSection(*section); e != ShellError::None) {
            m_error = e;
            return;
        }
    }
    if (material) {
        m_message = validateShellMaterial(*material);
        if (!m_message.empty()) {
            m_error = ShellError::InvalidMaterial;
            return;
        }
    }
    if (layout) {
        if (const ShellError e = checkLayout(*layout); e != ShellError::None) {
            m_error = e;
            return;
        }
    }
    if (checkContour(work, nodeIds) != ShellError::None || !std::isfinite(thicknessM) || thicknessM <= 0 ||
        (materialId != 0 && !work.findMaterial(materialId))) {
        m_error = ShellError::InvalidContour;
        return;
    }
    std::vector<int> key = nodeIds;
    std::sort(key.begin(), key.end());
    for (const Shell& s : work.shells) {
        std::vector<int> other = s.nodeIds;
        std::sort(other.begin(), other.end());
        if (other == key) {  // a shell with the same corner nodes already exists
            m_error = ShellError::InvalidContour;
            return;
        }
    }
    m_shellId = work.addShell(nodeIds, thicknessM, materialId);
    if (m_shellId == 0) {
        m_error = ShellError::InvalidContour;
        return;
    }
    Shell* created = work.findShell(m_shellId);
    if (section) applySection(*created, *section);
    if (material) created->material = *material;
    if (layout) created->layout = *layout;
    commit();
}

DeleteShellsCommand::DeleteShellsCommand(Project& project, const std::vector<int>& shellIds)
    : SnapshotCommand(project, "Borrar láminas") {
    if (shellIds.empty()) {
        m_error = ShellError::UnknownElement;
        return;
    }
    for (int id : shellIds)
        if (!work.removeShell(id)) {
            m_error = ShellError::UnknownElement;
            return;
        }
    commit();
}

DivideShellsCommand::DivideShellsCommand(Project& project, const std::vector<int>& shellIds, const Point3& p, const Point3& q)
    : SnapshotCommand(project, "Dividir láminas") {
    std::vector<int> ids = shellIds;
    sortedUnique(ids);
    if (ids.empty()) {
        m_error = ShellError::UnknownElement;
        return;
    }
    const Point3 d = sub(q, p);
    if (length(d) < kShellDivideToleranceM) {
        m_error = ShellError::NothingToDivide;
        return;
    }
    struct Ref {
        int node = 0;  // 0 = new point at pos
        Point3 pos;
    };
    bool any = false;
    for (int shellId : ids) {
        const Shell* found = work.findShell(shellId);
        if (!found) {
            m_error = ShellError::UnknownElement;
            return;
        }
        const Shell old = *found;
        const std::vector<Point3> pts = positions(work, old.nodeIds);
        const Point3 normal = unit(newell(pts));
        const Point3 mRaw = cross(d, normal);
        if (length(normal) == 0 || length(mRaw) < 1e-9 * length(d)) continue;  // segment parallel to the normal
        const Point3 m = unit(mRaw);

        const std::size_t n = pts.size();
        std::vector<int> cls(n);
        std::vector<double> s(n);
        int pos = 0, neg = 0;
        for (std::size_t i = 0; i < n; ++i) {
            s[i] = dot(m, sub(pts[i], p));
            cls[i] = s[i] > kShellDivideToleranceM ? 1 : (s[i] < -kShellDivideToleranceM ? -1 : 0);
            pos += cls[i] > 0;
            neg += cls[i] < 0;
        }
        if (pos == 0 || neg == 0) continue;  // the line does not cross the shell
        std::vector<Ref> partA, partB;
        std::vector<Point3> cuts;
        for (std::size_t i = 0; i < n; ++i) {
            const std::size_t j = (i + 1) % n;
            const Ref here{old.nodeIds[i], pts[i]};
            if (cls[i] >= 0) partA.push_back(here);
            if (cls[i] <= 0) partB.push_back(here);
            if (cls[i] == 0) cuts.push_back(pts[i]);
            if (cls[i] * cls[j] == -1) {
                const double t = s[i] / (s[i] - s[j]);
                const Point3 x = add(pts[i], scale(sub(pts[j], pts[i]), t));
                partA.push_back({0, x});
                partB.push_back({0, x});
                cuts.push_back(x);
            }
        }
        if (cuts.size() != 2 || partA.size() < 3 || partB.size() < 3) {
            m_error = ShellError::CannotDivide;
            return;
        }
        // The segment must penetrate the shell: its parameter interval overlaps the chord of the cut.
        const double dd = dot(d, d);
        const double u1 = dot(d, sub(cuts[0], p)) / dd, u2 = dot(d, sub(cuts[1], p)) / dd;
        const double overlap = std::min(1.0, std::max(u1, u2)) - std::max(0.0, std::min(u1, u2));
        if (overlap <= 1e-9) continue;

        // The part that holds the first corner keeps the shell id.
        std::vector<Ref>* first = cls[0] < 0 ? &partB : &partA;
        std::vector<Ref>* second = first == &partA ? &partB : &partA;
        std::vector<std::vector<int>> contours;
        for (std::vector<Ref>* part : {first, second}) {
            std::vector<int> nodes;
            for (const Ref& r : *part) nodes.push_back(r.node != 0 ? r.node : nodeAt(work, r.pos));
            const auto pieces = decomposeContour(work, nodes);
            if (pieces.empty()) {
                m_error = ShellError::CannotDivide;
                return;
            }
            for (const auto& c : pieces) {
                if (checkContour(work, c) != ShellError::None) {
                    m_error = ShellError::CannotDivide;
                    return;
                }
                contours.push_back(c);
            }
        }
        Shell* keep = work.findShell(shellId);
        keep->nodeIds = contours[0];
        adoptXEdge(*keep, old.xEdgeStart, old.xEdgeEnd);
        for (std::size_t k = 1; k < contours.size(); ++k) {
            Shell extra = old;
            extra.id = work.nextShellId;
            extra.nodeIds = contours[k];
            adoptXEdge(extra, old.xEdgeStart, old.xEdgeEnd);
            work.insertShell(extra);
            m_created.push_back(extra.id);
        }
        any = true;
    }
    if (!any) {
        m_error = ShellError::NothingToDivide;
        return;
    }
    commit();
}

JoinShellsCommand::JoinShellsCommand(Project& project, const std::vector<int>& shellIds) : SnapshotCommand(project, "Unir láminas") {
    std::vector<int> ids = shellIds;
    sortedUnique(ids);
    for (int id : ids)
        if (!work.findShell(id)) {
            m_error = ShellError::UnknownElement;
            return;
        }
    if (ids.size() < 2) {
        m_error = ShellError::TooFewShells;
        return;
    }
    std::vector<Point3> all;
    std::set<int> nodeSet;
    for (int id : ids)
        for (int n : work.findShell(id)->nodeIds)
            if (nodeSet.insert(n).second) all.push_back(work.findNode(n)->position);
    if (!pointsCoplanar(all)) {
        m_error = ShellError::NotCoplanar;
        return;
    }
    // Boundary = edges used by exactly one selected shell; it must be a single simple cycle (one polygon, no holes).
    std::map<std::pair<int, int>, int> edgeCount;
    for (int id : ids) {
        const auto& nodes = work.findShell(id)->nodeIds;
        for (std::size_t i = 0; i < nodes.size(); ++i) {
            const int a = nodes[i], b = nodes[(i + 1) % nodes.size()];
            ++edgeCount[{std::min(a, b), std::max(a, b)}];
        }
    }
    std::map<int, std::vector<int>> adjacent;
    std::size_t boundaryEdges = 0;
    for (const auto& [edge, count] : edgeCount) {
        if (count > 2) {
            m_error = ShellError::NotClosedRegion;
            return;
        }
        if (count == 1) {
            adjacent[edge.first].push_back(edge.second);
            adjacent[edge.second].push_back(edge.first);
            ++boundaryEdges;
        }
    }
    if (boundaryEdges < 3) {
        m_error = ShellError::NotClosedRegion;
        return;
    }
    for (const auto& [node, list] : adjacent)
        if (list.size() != 2) {
            m_error = ShellError::NotClosedRegion;
            return;
        }
    std::vector<int> cycle;
    {
        const int start = adjacent.begin()->first;
        int prev = 0, cur = start;
        do {
            cycle.push_back(cur);
            const auto& nb = adjacent[cur];
            const int next = nb[0] != prev ? nb[0] : nb[1];
            prev = cur;
            cur = next;
        } while (cur != start && cycle.size() <= boundaryEdges);
    }
    if (cycle.size() != boundaryEdges) {  // several loops
        m_error = ShellError::NotClosedRegion;
        return;
    }
    const Shell first = *work.findShell(ids.front());
    // Orientation of the first shell's normal.
    if (dot(newell(positions(work, cycle)), newell(positions(work, first.nodeIds))) < 0) std::reverse(cycle.begin(), cycle.end());

    // Drop corner nodes that sit on a straight stretch and belong to nothing else.
    auto usedElsewhere = [&](int node) {
        if (!work.barsUsingNode(node).empty()) return true;
        for (int sid : work.shellsUsingNode(node))
            if (!std::binary_search(ids.begin(), ids.end(), sid)) return true;
        return false;
    };
    bool changed = true;
    while (changed && cycle.size() > 3) {
        changed = false;
        for (std::size_t i = 0; i < cycle.size(); ++i) {
            const std::size_t n = cycle.size();
            const Point3 a = work.findNode(cycle[(i + n - 1) % n])->position, b = work.findNode(cycle[i])->position,
                         c = work.findNode(cycle[(i + 1) % n])->position;
            const Point3 ac = sub(c, a);
            if (length(ac) < 1e-12) continue;
            const double dist = length(cross(sub(b, a), ac)) / length(ac);
            if (dist <= kShellDivideToleranceM && dot(sub(b, a), sub(c, b)) > 0 && !usedElsewhere(cycle[i])) {
                cycle.erase(cycle.begin() + static_cast<std::ptrdiff_t>(i));
                changed = true;
                break;
            }
        }
    }
    if (cycle.size() > 4) {
        m_error = ShellError::ContourTooComplex;
        return;
    }
    if (checkContour(work, cycle) != ShellError::None) {
        m_error = ShellError::NotClosedRegion;
        return;
    }
    for (std::size_t i = 1; i < ids.size(); ++i) work.removeShell(ids[i]);
    Shell* result = work.findShell(ids.front());
    result->nodeIds = cycle;
    adoptXEdge(*result, first.xEdgeStart, first.xEdgeEnd);
    m_result = result->id;
    commit();
}

InvertShellZCommand::InvertShellZCommand(Project& project, const std::vector<int>& shellIds)
    : SnapshotCommand(project, "Invertir eje z") {
    std::vector<int> ids = shellIds;
    sortedUnique(ids);
    if (ids.empty()) {
        m_error = ShellError::NothingToChange;
        return;
    }
    for (int id : ids) {
        Shell* s = work.findShell(id);
        if (!s) {
            m_error = ShellError::UnknownElement;
            return;
        }
        s->zAxisInverted = !s->zAxisInverted;
    }
    commit();
}

SetShellSectionCommand::SetShellSectionCommand(Project& project, const std::vector<int>& shellIds, const ShellSection& section)
    : SnapshotCommand(project, "Sección de la lámina") {
    std::vector<int> ids = shellIds;
    sortedUnique(ids);
    if (ids.empty()) {
        m_error = ShellError::NothingToChange;
        return;
    }
    if (const ShellError e = checkSection(section); e != ShellError::None) {  // also refuses NaN
        m_error = e;
        return;
    }
    for (int id : ids) {
        Shell* s = work.findShell(id);
        if (!s) {
            m_error = ShellError::UnknownElement;
            return;
        }
        applySection(*s, section);
    }
    commit();
}

SetShellXEdgeCommand::SetShellXEdgeCommand(Project& project, int shellId, int a, int b) : SnapshotCommand(project, "Definir eje x") {
    Shell* s = work.findShell(shellId);
    if (!s) {
        m_error = ShellError::UnknownElement;
        return;
    }
    const std::size_t n = s->nodeIds.size();
    for (std::size_t i = 0; i < n; ++i) {
        const int u = s->nodeIds[i], v = s->nodeIds[(i + 1) % n];
        if ((u == a && v == b) || (u == b && v == a)) {
            s->xEdgeStart = u;
            s->xEdgeEnd = v;
            commit();
            return;
        }
    }
    m_error = ShellError::InvalidEdge;
}

MoveShellNodesCommand::MoveShellNodesCommand(Project& project, int shellId, const std::vector<int>& nodeIds, const Point3& delta)
    : SnapshotCommand(project, "Mover láminas") {
    const Shell* shell = work.findShell(shellId);
    if (!shell || nodeIds.empty()) {
        m_error = ShellError::UnknownElement;
        return;
    }
    if (!std::isfinite(delta.x) || !std::isfinite(delta.y) || !std::isfinite(delta.z)) {
        m_error = ShellError::InvalidMove;
        return;
    }
    std::vector<int> ids = nodeIds;
    sortedUnique(ids);
    for (int id : ids)
        if (std::find(shell->nodeIds.begin(), shell->nodeIds.end(), id) == shell->nodeIds.end()) {
            m_error = ShellError::UnknownElement;
            return;
        }
    for (int id : ids) work.findNode(id)->position = add(work.findNode(id)->position, delta);
    std::set<int> bars, shells;
    for (int id : ids) {
        for (int b : work.barsUsingNode(id)) bars.insert(b);
        for (int s : work.shellsUsingNode(id)) shells.insert(s);
    }
    for (int b : bars)
        if (work.barLength(b) < kMinBarLengthM) {
            m_error = ShellError::InvalidMove;
            return;
        }
    for (int s : shells) {
        const ShellError e = checkContour(work, work.findShell(s)->nodeIds);
        if (e != ShellError::None) {
            m_error = e == ShellError::NotCoplanar ? ShellError::NotCoplanar : ShellError::InvalidMove;
            return;
        }
    }
    commit();
}

// --- wrappers --------------------------------------------------------------------------------------------------------

namespace {
template <typename Cmd, typename... Args>
ShellError run(Project& project, UndoStack& stack, Cmd*& out, Args&&... args) {
    auto cmd = std::make_unique<Cmd>(project, std::forward<Args>(args)...);
    out = cmd.get();
    const ShellError e = cmd->error();
    if (e == ShellError::None) stack.push(std::move(cmd));
    return e;
}
}  // namespace

SetShellMaterialCommand::SetShellMaterialCommand(Project& project, const std::vector<int>& shellIds, const ShellMaterial& material)
    : SnapshotCommand(project, "Material de la lámina") {
    std::vector<int> ids = shellIds;
    sortedUnique(ids);
    if (ids.empty()) {
        m_error = ShellError::NothingToChange;
        return;
    }
    m_message = validateShellMaterial(material);
    if (!m_message.empty()) {
        m_error = ShellError::InvalidMaterial;
        return;
    }
    for (int id : ids) {
        Shell* s = work.findShell(id);
        if (!s) {
            m_error = ShellError::UnknownElement;
            return;
        }
        s->material = material;
    }
    commit();
}

SetShellLayoutCommand::SetShellLayoutCommand(Project& project, const std::vector<int>& shellIds, const ShellLayout& layout)
    : SnapshotCommand(project, "Disposición de la lámina") {
    std::vector<int> ids = shellIds;
    sortedUnique(ids);
    if (ids.empty()) {
        m_error = ShellError::NothingToChange;
        return;
    }
    if (const ShellError e = checkLayout(layout); e != ShellError::None) {
        m_error = e;
        return;
    }
    for (int id : ids) {
        Shell* s = work.findShell(id);
        if (!s) {
            m_error = ShellError::UnknownElement;
            return;
        }
        s->layout = layout;
    }
    commit();
}

ShellError setShellMaterial(Project& project, UndoStack& stack, const std::vector<int>& shellIds, const ShellMaterial& material,
                            std::string* message) {
    auto cmd = std::make_unique<SetShellMaterialCommand>(project, shellIds, material);
    if (!cmd->valid()) {
        if (message) *message = cmd->message();
        return cmd->error();
    }
    stack.push(std::move(cmd));
    return ShellError::None;
}

ShellError setShellLayout(Project& project, UndoStack& stack, const std::vector<int>& shellIds, const ShellLayout& layout) {
    auto cmd = std::make_unique<SetShellLayoutCommand>(project, shellIds, layout);
    if (!cmd->valid()) return cmd->error();
    stack.push(std::move(cmd));
    return ShellError::None;
}

ShellError addShell(Project& project, UndoStack& stack, const std::vector<int>& nodeIds, double thicknessM, int materialId, int* shellId,
                    const ShellSection* section, const ShellMaterial* material, const ShellLayout* layout) {
    AddShellCommand* c = nullptr;
    int id = 0;
    {
        auto cmd = std::make_unique<AddShellCommand>(project, nodeIds, thicknessM, materialId, section, material, layout);
        if (!cmd->valid()) return cmd->error();
        id = cmd->shellId();
        c = cmd.get();
        stack.push(std::move(cmd));
    }
    (void)c;
    if (shellId) *shellId = id;
    return ShellError::None;
}

ShellError deleteShells(Project& project, UndoStack& stack, const std::vector<int>& shellIds) {
    DeleteShellsCommand* c = nullptr;
    return run(project, stack, c, shellIds);
}

ShellError divideShells(Project& project, UndoStack& stack, const std::vector<int>& shellIds, const Point3& p, const Point3& q,
                        std::vector<int>* createdIds) {
    auto cmd = std::make_unique<DivideShellsCommand>(project, shellIds, p, q);
    if (!cmd->valid()) return cmd->error();
    if (createdIds) *createdIds = cmd->createdShellIds();
    stack.push(std::move(cmd));
    return ShellError::None;
}

ShellError joinShells(Project& project, UndoStack& stack, const std::vector<int>& shellIds, int* resultId) {
    auto cmd = std::make_unique<JoinShellsCommand>(project, shellIds);
    if (!cmd->valid()) return cmd->error();
    if (resultId) *resultId = cmd->resultShellId();
    stack.push(std::move(cmd));
    return ShellError::None;
}

ShellError invertShellZ(Project& project, UndoStack& stack, const std::vector<int>& shellIds) {
    InvertShellZCommand* c = nullptr;
    return run(project, stack, c, shellIds);
}

ShellError setShellSection(Project& project, UndoStack& stack, const std::vector<int>& shellIds, const ShellSection& section) {
    SetShellSectionCommand* c = nullptr;
    return run(project, stack, c, shellIds, section);
}

ShellError setShellXEdge(Project& project, UndoStack& stack, int shellId, int nodeA, int nodeB) {
    SetShellXEdgeCommand* c = nullptr;
    return run(project, stack, c, shellId, nodeA, nodeB);
}

ShellError moveShellNodes(Project& project, UndoStack& stack, int shellId, const std::vector<int>& nodeIds, const Point3& delta) {
    MoveShellNodesCommand* c = nullptr;
    return run(project, stack, c, shellId, nodeIds, delta);
}

}  // namespace cc::core
