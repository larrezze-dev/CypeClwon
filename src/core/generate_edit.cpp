#include "core/generate_edit.h"

#include <algorithm>
#include <cmath>
#include <map>

#include "core/bar_edit.h"
#include "core/geom3.h"
#include "core/node_edit.h"
#include "core/shell_material.h"
#include "core/tools_edit.h"
#include "core/work_planes.h"

namespace cc::core {

using namespace geom;

std::string divisionsWarning(int segment) {
    return rangeWarning("Nº de divisiones del segmento " + std::to_string(segment), ValueRange{1.0, 1000.0}, 0);
}

std::string generateErrorMessage(GenerateError error) {
    switch (error) {
        case GenerateError::None: return {};
        case GenerateError::UnknownElement: return "Algún nudo o barra seleccionado no existe.";
        case GenerateError::LinesParallel: return "Las dos líneas de referencia deben ser distintas y no paralelas para definir un plano.";
        case GenerateError::PlaneNotOrthogonal: return "Sólo es posible generar planos ortogonales a los ejes globales.";
        case GenerateError::CountOutOfRange: return rangeWarning("Nº de planos a generar", ValueRange{1.0, 100.0}, 0);
        case GenerateError::SeparationOutOfRange:
            return rangeWarning("Separación entre planos", ValueRange{kMinPlaneSeparationM, kMaxPlaneSeparationM}, 3);
        case GenerateError::ZeroSeparation: return "La separación entre planos no puede ser cero.";
        case GenerateError::DivisionsOutOfRange: return "El número de divisiones debe estar entre 1 y 1000.";
        case GenerateError::DegenerateSegments:
            return "Los segmentos seleccionados no permiten generar la malla (longitud nula, alineados o contenidos en un mismo plano).";
        case GenerateError::DistanceOutOfRange:
            return rangeWarning("Distancia entre planos", ValueRange{-kMaxTetrahedraDistanceM, kMaxTetrahedraDistanceM}, 3) +
                   " La distancia no puede ser cero.";
        case GenerateError::TooManyElements: return "La generación crearía demasiados nudos.";
        case GenerateError::NothingGenerated: return "No se ha generado ningún elemento nuevo.";
    }
    return {};
}

namespace {

double coord(const Point3& p, int axis) { return axis == 0 ? p.x : axis == 1 ? p.y : p.z; }

Point3 axisVector(int axis, double length) { return {axis == 0 ? length : 0.0, axis == 1 ? length : 0.0, axis == 2 ? length : 0.0}; }

std::vector<std::vector<double>>& axisGroups(PlaneGroups& g, int axis) { return axis == 0 ? g.x : axis == 1 ? g.y : g.z; }
const std::vector<std::vector<double>>& axisGroups(const PlaneGroups& g, int axis) { return axis == 0 ? g.x : axis == 1 ? g.y : g.z; }

bool sameCoord(double a, double b) { return std::fabs(a - b) <= kSameCoordinate + 1e-9; }

int barBetween(const StructuralModel& m, int a, int b) {
    for (const Bar& bar : m.bars)
        if ((bar.startNode == a && bar.endNode == b) || (bar.startNode == b && bar.endNode == a)) return bar.id;
    return 0;
}

}  // namespace

// ---- Planos ---------------------------------------------------------------------------------------------------------------

std::optional<PlaneOfLines> planeOfBars(const StructuralModel& model, int barA, int barB, GenerateError* error) {
    const auto fail = [&](GenerateError e) -> std::optional<PlaneOfLines> {
        if (error) *error = e;
        return std::nullopt;
    };
    const Bar* a = model.findBar(barA);
    const Bar* b = model.findBar(barB);
    if (!a || !b) return fail(GenerateError::UnknownElement);
    const Node* a0 = model.findNode(a->startNode);
    const Node* a1 = model.findNode(a->endNode);
    const Node* b0 = model.findNode(b->startNode);
    const Node* b1 = model.findNode(b->endNode);
    if (!a0 || !a1 || !b0 || !b1) return fail(GenerateError::UnknownElement);
    const Point3 d1 = unit(sub(a1->position, a0->position)), d2 = unit(sub(b1->position, b0->position));
    const Point3 n = cross(d1, d2);
    if (length(n) < 1e-6) return fail(GenerateError::LinesParallel);
    const Point3 nu = unit(n);
    int axis = -1;
    for (int k = 0; k < 3; ++k)
        if (std::fabs(coord(nu, k)) > 1.0 - 1e-6) axis = k;
    if (axis < 0) return fail(GenerateError::PlaneNotOrthogonal);
    const double c = coord(a0->position, axis);
    // Both lines must lie on the same plane (the same coordinate along the axis).
    for (const Point3& p : {a1->position, b0->position, b1->position})
        if (std::fabs(coord(p, axis) - c) > kGenerateToleranceM) return fail(GenerateError::PlaneNotOrthogonal);
    return PlaneOfLines{axis, c};
}

PlaneContent elementsOnPlane(const StructuralModel& model, const PlaneOfLines& plane) {
    PlaneContent out;
    const auto onPlane = [&](int nodeId) {
        const Node* n = model.findNode(nodeId);
        return n && std::fabs(coord(n->position, plane.axis) - plane.coordinate) <= kGenerateToleranceM;
    };
    for (const Node& n : model.nodes)
        if (onPlane(n.id)) out.nodeIds.push_back(n.id);
    for (const Bar& b : model.bars)
        if (onPlane(b.startNode) && onPlane(b.endNode)) out.barIds.push_back(b.id);
    for (const Shell& s : model.shells)
        if (std::all_of(s.nodeIds.begin(), s.nodeIds.end(), onPlane)) out.shellIds.push_back(s.id);
    return out;
}

namespace {
GenerateError checkPlaneSpec(const PlaneSpec& spec) {
    if (spec.count < kMinPlaneCount || spec.count > kMaxPlaneCount) return GenerateError::CountOutOfRange;
    if (!std::isfinite(spec.separationM) || spec.separationM < kMinPlaneSeparationM || spec.separationM > kMaxPlaneSeparationM)
        return GenerateError::SeparationOutOfRange;
    if (spec.separationM == 0.0) return GenerateError::ZeroSeparation;
    return GenerateError::None;
}
}  // namespace

bool generationUngroups(const Project& project, const PlaneSpec& spec) {
    if (checkPlaneSpec(spec) != GenerateError::None) return false;
    const auto plane = planeOfBars(project.structure, spec.barA, spec.barB);
    if (!plane) return false;
    for (int k = 0; k <= spec.count; ++k) {
        const double c = plane->coordinate + k * spec.separationM;
        for (const auto& group : axisGroups(project.workPlanes.groups, plane->axis))
            for (double existing : group)
                if (sameCoord(existing, c)) return true;
    }
    return false;
}

GenerateError generatePlanes(Project& project, UndoStack& stack, const PlaneSpec& spec) {
    if (const GenerateError e = checkPlaneSpec(spec); e != GenerateError::None) return e;
    GenerateError error = GenerateError::None;
    const auto plane = planeOfBars(project.structure, spec.barA, spec.barB, &error);
    if (!plane) return error;
    const PlaneContent content = elementsOnPlane(project.structure, *plane);
    ToolSelection selection;
    selection.nodeIds = content.nodeIds;
    selection.barIds = content.barIds;
    selection.shellIds = content.shellIds;
    // Planes cannot be copied beyond the work area of the grid coordinates; the copy itself has no limit.
    const Point3 from{0, 0, 0};
    const Transform3 move = translation(from, axisVector(plane->axis, spec.separationM));

    stack.beginMacro("Generación de planos");
    const ToolError copied = transformElements(project, stack, selection, move, true, spec.count);
    if (copied != ToolError::None) {
        stack.endMacro();
        // An empty macro is discarded by the stack; nothing was pushed.
        return copied == ToolError::NothingCopied || copied == ToolError::NothingSelected ? GenerateError::NothingGenerated
                                                                                          : GenerateError::UnknownElement;
    }
    if (spec.group) {
        PlaneGroups groups = project.workPlanes.groups;
        auto& list = axisGroups(groups, plane->axis);
        std::vector<double> planes;
        for (int k = 0; k <= spec.count; ++k) planes.push_back(roundCoordinate(plane->coordinate + k * spec.separationM));
        // A group of any plane that takes part is dissolved first (#63970).
        list.erase(std::remove_if(list.begin(), list.end(),
                                  [&](const std::vector<double>& group) {
                                      for (double existing : group)
                                          for (double p : planes)
                                              if (sameCoord(existing, p)) return true;
                                      return false;
                                  }),
                   list.end());
        list.push_back(planes);
        stack.push(std::make_unique<SetPlaneGroupsCommand>(project, std::move(groups)));
    }
    stack.endMacro();
    return GenerateError::None;
}

// ---- shared mesh helpers ----------------------------------------------------------------------------------------------------

namespace {

struct Builder {
    StructuralModel& m;
    std::vector<int>& nodes;
    std::vector<int>& bars;

    int node(const Point3& p) {
        if (const auto id = findNodeAt(m, p)) return *id;
        const int id = m.addNode(p);
        if (id) nodes.push_back(id);
        return id;
    }
    /// Id of the new bar, 0 when a bar between the nodes exists already (or the pair is invalid).
    int bar(int a, int b) {
        if (a == b || barBetween(m, a, b) != 0) return 0;
        const int id = m.addBar(a, b);
        if (id) bars.push_back(id);
        return id;
    }
};

/// Groups single-bar pieces: one record per bar, all sharing the group id (the lowest piece id, like "Agrupar").
void groupBars(StructuralModel& m, const std::vector<std::pair<int, std::pair<int, int>>>& bars) {
    if (bars.size() < 2) return;
    std::vector<int> pieceIds;
    for (const auto& [barId, ends] : bars) {
        Piece p;
        p.id = m.nextPieceId;
        p.barIds = {barId};
        p.nodeIds = {ends.first, ends.second};
        if (!m.insertPiece(p)) continue;
        pieceIds.push_back(p.id);
    }
    if (pieceIds.size() < 2) return;
    const int group = *std::min_element(pieceIds.begin(), pieceIds.end());
    for (int id : pieceIds)
        for (Piece& p : m.pieces)
            if (p.id == id) p.groupId = group;
}

/// A line of the mesh: its nodes in order and the id of the bar created between each pair (0 = the bar existed already).
struct Line {
    std::vector<int> nodes;
    std::vector<int> bars;
};

/// "Crear pieza entre barras alineadas" / "Agrupar barras paralelas" for the lines of one direction.
void makePieces(StructuralModel& m, const std::vector<Line>& lines, bool piece, bool group) {
    if (!piece && !group) return;
    std::vector<int> pieceIds;
    std::vector<std::pair<int, std::pair<int, int>>> singles;
    for (const Line& line : lines) {
        const bool allNew = !line.bars.empty() && std::all_of(line.bars.begin(), line.bars.end(), [](int id) { return id != 0; });
        if (piece && allNew && line.bars.size() >= 2) {
            Piece p;
            p.id = m.nextPieceId;
            p.barIds = line.bars;
            p.nodeIds = line.nodes;
            if (m.insertPiece(p)) pieceIds.push_back(p.id);
        } else if (group) {
            for (std::size_t i = 0; i < line.bars.size(); ++i)
                if (line.bars[i] != 0) singles.push_back({line.bars[i], {line.nodes[i], line.nodes[i + 1]}});
        }
    }
    if (!group) return;
    for (const auto& [barId, ends] : singles) {
        Piece p;
        p.id = m.nextPieceId;
        p.barIds = {barId};
        p.nodeIds = {ends.first, ends.second};
        if (m.insertPiece(p)) pieceIds.push_back(p.id);
    }
    if (pieceIds.size() < 2) {
        if (pieceIds.size() == 1 && !piece) m.removePiece(pieceIds.front());  // a lone single-bar record needs no group
        return;
    }
    const int groupId = *std::min_element(pieceIds.begin(), pieceIds.end());
    for (Piece& p : m.pieces)
        if (std::find(pieceIds.begin(), pieceIds.end(), p.id) != pieceIds.end()) p.groupId = groupId;
}

}  // namespace

bool segmentIsIndependent(const StructuralModel& model, const std::vector<int>& soFar, int candidate) {
    if (soFar.size() < 2) return false;
    const Node* o = model.findNode(soFar[0]);
    const Node* e1 = model.findNode(soFar[1]);
    const Node* c = model.findNode(candidate);
    if (!o || !e1 || !c) return false;
    const Point3 s1 = sub(e1->position, o->position), sc = sub(c->position, o->position);
    if (length(s1) < kMinBarLengthM || length(sc) < kMinBarLengthM) return false;
    if (soFar.size() == 2) return length(cross(s1, sc)) / (length(s1) * length(sc)) > 1e-6;  // not collinear
    if (soFar.size() != 3) return false;
    const Node* e2 = model.findNode(soFar[2]);
    if (!e2) return false;
    const Point3 s2 = sub(e2->position, o->position);
    const Point3 n = cross(s1, s2);
    if (length(n) < 1e-9) return false;
    return std::fabs(dot(unit(n), sc)) / length(sc) > 1e-6;  // outside the plane of the first two
}

// ---- Nudos y barras ---------------------------------------------------------------------------------------------------------

GenerateNodeMeshCommand::GenerateNodeMeshCommand(Project& project, const NodeMeshSpec& spec) : SnapshotCommand(project, "Generación de nudos y barras") {
    const int segments = segmentCount(spec);
    if (segments < 1 || segments > 3) {
        m_error = GenerateError::UnknownElement;
        return;
    }
    std::vector<Point3> p;
    for (int id : spec.nodeIds) {
        const Node* n = work.findNode(id);
        if (!n) {
            m_error = GenerateError::UnknownElement;
            return;
        }
        p.push_back(n->position);
    }
    std::array<int, 3> n{1, 1, 1};
    std::size_t total = 1;
    for (int k = 0; k < segments; ++k) {
        n[k] = spec.divisions[k];
        if (n[k] < kMinDivisions || n[k] > kMaxDivisions) {
            m_error = GenerateError::DivisionsOutOfRange;
            return;
        }
        total *= static_cast<std::size_t>(n[k] + 1);
        if (total > kMaxGeneratedNodes) {
            m_error = GenerateError::TooManyElements;
            return;
        }
    }
    std::vector<Point3> s;  // segment vectors from the origin
    for (int k = 0; k < segments; ++k) s.push_back(sub(p[k + 1], p[0]));
    for (const Point3& v : s)
        if (length(v) < kMinBarLengthM) {
            m_error = GenerateError::DegenerateSegments;
            return;
        }
    if (segments >= 2 && length(cross(s[0], s[1])) / (length(s[0]) * length(s[1])) <= 1e-6) {
        m_error = GenerateError::DegenerateSegments;
        return;
    }
    if (segments == 3 && std::fabs(dot(unit(cross(s[0], s[1])), s[2])) / length(s[2]) <= 1e-6) {
        m_error = GenerateError::DegenerateSegments;
        return;
    }

    Builder b{work, m_nodes, m_bars};
    if (segments == 1) {
        if (n[0] == 1) {
            m_error = GenerateError::NothingGenerated;
            return;
        }
        std::vector<int> chain{spec.nodeIds[0]};
        for (int i = 1; i < n[0]; ++i) chain.push_back(b.node(add(p[0], scale(s[0], static_cast<double>(i) / n[0]))));
        chain.push_back(spec.nodeIds[1]);
        if (const int old = barBetween(work, spec.nodeIds[0], spec.nodeIds[1]); old != 0) {
            const Bar original = *work.findBar(old);
            const bool forward = original.startNode == spec.nodeIds[0];
            work.removeBar(old);
            for (std::size_t i = 0; i + 1 < chain.size(); ++i) {
                const int a = forward ? chain[i] : chain[chain.size() - 1 - i];
                const int c = forward ? chain[i + 1] : chain[chain.size() - 2 - i];
                const int id = work.addBar(a, c, original.materialId, original.sectionId);
                if (!id) continue;
                Bar* nb = work.findBar(id);
                nb->rollAngleDeg = original.rollAngleDeg;
                nb->xAxisInverted = original.xAxisInverted;
                nb->layerIds = original.layerIds;
                m_bars.push_back(id);
            }
        }
        if (m_nodes.empty() && m_bars.empty()) {
            m_error = GenerateError::NothingGenerated;
            return;
        }
        commit();
        return;
    }

    // 2D / 3D grid. node(i, j, k) is created in a fixed order so that the ids are deterministic.
    const int nk = segments == 3 ? n[2] : 0;
    const auto index = [&](int i, int j, int k) { return (static_cast<std::size_t>(k) * (n[1] + 1) + j) * (n[0] + 1) + i; };
    std::vector<int> grid(total, 0);
    for (int k = 0; k <= nk; ++k)
        for (int j = 0; j <= n[1]; ++j)
            for (int i = 0; i <= n[0]; ++i) {
                Point3 q = add(p[0], add(scale(s[0], static_cast<double>(i) / n[0]), scale(s[1], static_cast<double>(j) / n[1])));
                if (segments == 3) q = add(q, scale(s[2], static_cast<double>(k) / n[2]));
                grid[index(i, j, k)] = b.node(q);
            }
    if (!spec.onlyNodes) {
        for (int dir = 0; dir < segments; ++dir) {
            std::vector<Line> lines;
            const int limits[3] = {n[0], n[1], nk};
            const int o1 = (dir + 1) % 3, o2 = (dir + 2) % 3;  // the other two indices run over their ranges
            for (int u = 0; u <= limits[o1]; ++u)
                for (int v = 0; v <= limits[o2]; ++v) {
                    Line line;
                    for (int t = 0; t <= limits[dir]; ++t) {
                        int idx[3] = {0, 0, 0};
                        idx[dir] = t;
                        idx[o1] = u;
                        idx[o2] = v;
                        line.nodes.push_back(grid[index(idx[0], idx[1], idx[2])]);
                    }
                    for (std::size_t t = 0; t + 1 < line.nodes.size(); ++t) line.bars.push_back(b.bar(line.nodes[t], line.nodes[t + 1]));
                    lines.push_back(std::move(line));
                }
            makePieces(work, lines, spec.makePieces[dir], spec.groupParallel[dir]);
        }
    }
    if (m_nodes.empty() && m_bars.empty()) {
        m_error = GenerateError::NothingGenerated;
        return;
    }
    commit();
}

// ---- Malla de tetraedros ------------------------------------------------------------------------------------------------------

GenerateTetrahedraCommand::GenerateTetrahedraCommand(Project& project, const TetrahedraSpec& spec)
    : SnapshotCommand(project, "Generación de una malla de tetraedros") {
    const Node* o = work.findNode(spec.nodeIds[0]);
    const Node* e1 = work.findNode(spec.nodeIds[1]);
    const Node* e2 = work.findNode(spec.nodeIds[2]);
    if (!o || !e1 || !e2) {
        m_error = GenerateError::UnknownElement;
        return;
    }
    if (spec.divisions1 < kMinDivisions || spec.divisions1 > kMaxDivisions || spec.divisions2 < kMinDivisions || spec.divisions2 > kMaxDivisions) {
        m_error = GenerateError::DivisionsOutOfRange;
        return;
    }
    if (!std::isfinite(spec.distanceM) || spec.distanceM == 0.0 || std::fabs(spec.distanceM) > kMaxTetrahedraDistanceM) {
        m_error = GenerateError::DistanceOutOfRange;
        return;
    }
    const int n1 = spec.divisions1, n2 = spec.divisions2;
    if (static_cast<std::size_t>(n1 + 1) * (n2 + 1) + static_cast<std::size_t>(n1) * n2 > kMaxGeneratedNodes) {
        m_error = GenerateError::TooManyElements;
        return;
    }
    const Point3 s1 = sub(e1->position, o->position), s2 = sub(e2->position, o->position);
    const Point3 normal = cross(s1, s2);
    if (length(s1) < kMinBarLengthM || length(s2) < kMinBarLengthM || length(normal) / (length(s1) * length(s2)) <= 1e-6) {
        m_error = GenerateError::DegenerateSegments;
        return;
    }
    const Point3 lift = scale(unit(normal), spec.distanceM);  // "regla del tornillo"
    Builder b{work, m_nodes, m_bars};
    std::vector<int> lower(static_cast<std::size_t>(n1 + 1) * (n2 + 1)), upper(static_cast<std::size_t>(n1) * n2);
    const auto li = [&](int i, int j) { return static_cast<std::size_t>(j) * (n1 + 1) + i; };
    const auto ui = [&](int i, int j) { return static_cast<std::size_t>(j) * n1 + i; };
    for (int j = 0; j <= n2; ++j)
        for (int i = 0; i <= n1; ++i)
            lower[li(i, j)] = b.node(add(o->position, add(scale(s1, static_cast<double>(i) / n1), scale(s2, static_cast<double>(j) / n2))));
    for (int j = 0; j < n2; ++j)
        for (int i = 0; i < n1; ++i)
            upper[ui(i, j)] = b.node(add(add(o->position, lift), add(scale(s1, (i + 0.5) / n1), scale(s2, (j + 0.5) / n2))));

    std::vector<std::pair<int, std::pair<int, int>>> lowerBars, upperBars, diagonals;
    const auto record = [&](std::vector<std::pair<int, std::pair<int, int>>>& list, int a, int c) {
        if (const int id = b.bar(a, c)) list.push_back({id, {a, c}});
    };
    for (int j = 0; j <= n2; ++j)
        for (int i = 0; i <= n1; ++i) {
            if (i < n1) record(lowerBars, lower[li(i, j)], lower[li(i + 1, j)]);
            if (j < n2) record(lowerBars, lower[li(i, j)], lower[li(i, j + 1)]);
        }
    for (int j = 0; j < n2; ++j)
        for (int i = 0; i < n1; ++i) {
            if (i + 1 < n1) record(upperBars, upper[ui(i, j)], upper[ui(i + 1, j)]);
            if (j + 1 < n2) record(upperBars, upper[ui(i, j)], upper[ui(i, j + 1)]);
            for (int corner = 0; corner < 4; ++corner) record(diagonals, upper[ui(i, j)], lower[li(i + (corner & 1), j + (corner >> 1))]);
        }
    if (spec.groupLowerBars) groupBars(work, lowerBars);
    if (spec.groupUpperBars) groupBars(work, upperBars);
    if (spec.groupDiagonals) groupBars(work, diagonals);
    if (m_nodes.empty() && m_bars.empty()) {
        m_error = GenerateError::NothingGenerated;
        return;
    }
    commit();
}

GenerateError generateNodeMesh(Project& project, UndoStack& stack, const NodeMeshSpec& spec) {
    auto cmd = std::make_unique<GenerateNodeMeshCommand>(project, spec);
    if (!cmd->valid()) return cmd->error();
    stack.push(std::move(cmd));
    return GenerateError::None;
}

GenerateError generateTetrahedra(Project& project, UndoStack& stack, const TetrahedraSpec& spec) {
    auto cmd = std::make_unique<GenerateTetrahedraCommand>(project, spec);
    if (!cmd->valid()) return cmd->error();
    stack.push(std::move(cmd));
    return GenerateError::None;
}

}  // namespace cc::core
