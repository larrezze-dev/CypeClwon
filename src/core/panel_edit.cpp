#include "core/panel_edit.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <set>

#include "core/geom3.h"

namespace cc::core {

std::string panelErrorMessage(PanelError error) {
    switch (error) {
        case PanelError::None: return {};
        case PanelError::NoPanels: return "No hay ningún paño seleccionado.";
        case PanelError::UnknownLoad: return "La carga superficial no existe.";
        case PanelError::UnknownLoadCase: return "La hipótesis no existe.";
        case PanelError::InvalidData: return "Los datos de la carga no son válidos.";
        case PanelError::InvalidContour: return "Los puntos indicados no permiten definir correctamente el contorno.";
        case PanelError::InvalidSide: return "La recta indicada no es un lado del paño.";
        case PanelError::InvalidVertex: return "El vértice indicado no existe.";
        case PanelError::VariableOnHorizontal: return surfaceLoadVariableOnHorizontalMessage();
        case PanelError::NothingToDivide: return "La recta no corta ningún paño.";
        case PanelError::CannotDivide: return "No es posible dividir el paño con la recta indicada.";
        case PanelError::NoBars: return "No hay ninguna barra a la que asignar cargas superficiales.";
        case PanelError::NothingToChange: return "La operación no modifica los paños ni las cargas superficiales.";
    }
    return {};
}

namespace {

using geom::add;
using geom::cross;
using geom::dot;
using geom::length;
using geom::scale;
using geom::sub;

PanelError push(Project& project, UndoStack& stack, Panels panels, SurfaceLoads loads, const char* text) {
    auto cmd = std::make_unique<PanelCommand>(project, std::move(panels), std::move(loads), text);
    if (!cmd->valid()) return PanelError::NothingToChange;
    stack.push(std::move(cmd));
    return PanelError::None;
}

/// Checks one dialog / table row: data, load case, and (when `horizontal`) variable rows.
PanelError checkRow(const Project& project, const SurfaceLoadData& data, bool horizontal, SurfaceLoadIssue* issue) {
    const SurfaceLoadIssue found = checkSurfaceLoadData(data);
    if (issue) *issue = found;
    if (!found.ok()) return PanelError::InvalidData;
    if (!findLoadCase(project.loadCases, data.loadCaseId)) return PanelError::UnknownLoadCase;
    if (horizontal && data.type == SurfaceLoadType::Variable) {
        if (issue) *issue = {SurfaceLoadIssueKind::Warning, surfaceLoadVariableOnHorizontalMessage(), std::nullopt};
        return PanelError::VariableOnHorizontal;
    }
    return PanelError::None;
}

PanelError checkRows(const Project& project, const std::vector<SurfaceLoadData>& rows, bool horizontal, SurfaceLoadIssue* issue) {
    for (const SurfaceLoadData& row : rows)
        if (const PanelError e = checkRow(project, row, horizontal, issue); e != PanelError::None) return e;
    return PanelError::None;
}

std::vector<SurfaceLoadData> normalised(const std::vector<SurfaceLoadData>& rows) {
    std::vector<SurfaceLoadData> out;
    out.reserve(rows.size());
    for (const SurfaceLoadData& r : rows) out.push_back(normaliseSurfaceLoadData(r));
    return out;
}

}  // namespace

// --- panels ---

PanelError addPanelWithDirection(Project& project, UndoStack& stack, const std::vector<Point3>& vertices, const Point3& direction,
                                 const std::vector<SurfaceLoadData>& loads, int* panelId, ContourError* why, SurfaceLoadIssue* issue) {
    if (issue) *issue = {};
    const ContourError contour = validatePanelContour(vertices);
    if (why) *why = contour;
    if (contour != ContourError::None) return PanelError::InvalidContour;
    const auto frame = panelFrame(vertices);
    if (!frame) return PanelError::InvalidContour;
    if (const PanelError e = checkRows(project, loads, isHorizontalPlane(*frame), issue); e != PanelError::None) return e;
    Panels after = project.panels;
    Panel p;
    p.id = after.nextId++;
    p.vertices = vertices;
    p.direction = panelDirectionInPlane(vertices, direction);
    p.loads = normalised(loads);
    const int id = p.id;
    after.items.push_back(std::move(p));
    const PanelError result = push(project, stack, std::move(after), project.surfaceLoads, "Nuevo paño");
    if (result == PanelError::None && panelId) *panelId = id;
    return result;
}

PanelError addPanel(Project& project, UndoStack& stack, const std::vector<Point3>& vertices, int side, PanelDirectionMode mode,
                    const std::vector<SurfaceLoadData>& loads, int* panelId, ContourError* why, SurfaceLoadIssue* issue) {
    if (why) *why = ContourError::None;
    const ContourError contour = validatePanelContour(vertices);
    if (contour != ContourError::None) {
        if (why) *why = contour;
        if (issue) *issue = {};
        return PanelError::InvalidContour;
    }
    const Point3 direction = panelDirectionFromSide(vertices, side, mode);
    if (length(direction) < 0.5) return PanelError::InvalidSide;
    return addPanelWithDirection(project, stack, vertices, direction, loads, panelId, why, issue);
}

PanelError setPanelLoads(Project& project, UndoStack& stack, const std::vector<int>& panelIds, const std::vector<SurfaceLoadData>& rows, SurfaceLoadIssue* issue) {
    if (issue) *issue = {};
    const std::set<int> wanted(panelIds.begin(), panelIds.end());
    bool any = false, anyHorizontal = false;
    for (const Panel& p : project.panels.items)
        if (wanted.count(p.id)) {
            any = true;
            anyHorizontal = anyHorizontal || isHorizontalPolygon(p.vertices);
        }
    if (!any) return PanelError::NoPanels;
    if (const PanelError e = checkRows(project, rows, anyHorizontal, issue); e != PanelError::None) return e;
    Panels after = project.panels;
    const std::vector<SurfaceLoadData> canonical = normalised(rows);
    for (Panel& p : after.items)
        if (wanted.count(p.id)) p.loads = canonical;
    return push(project, stack, std::move(after), project.surfaceLoads, "Editar cargas del paño");
}

PanelError deletePanels(Project& project, UndoStack& stack, const std::vector<int>& panelIds, std::vector<int>* removedSurfaceLoads) {
    Panels panels = project.panels;
    SurfaceLoads loads = project.surfaceLoads;
    std::vector<int> existing;
    for (int id : panelIds)
        if (findPanel(panels, id)) existing.push_back(id);
    if (existing.empty()) return PanelError::NoPanels;
    std::vector<int> removed = applyPanelsDeleted(panels, loads, existing);
    const PanelError result = push(project, stack, std::move(panels), std::move(loads), "Borrar paños");
    if (result == PanelError::None && removedSurfaceLoads) *removedSurfaceLoads = std::move(removed);
    return result;
}

PanelError movePanelVertices(Project& project, UndoStack& stack, int panelId, const std::vector<int>& vertexIndices, const Point3& delta, ContourError* why) {
    if (why) *why = ContourError::None;
    Panels after = project.panels;
    Panel* panel = findPanel(after, panelId);
    if (!panel) return PanelError::NoPanels;
    for (int i : vertexIndices)
        if (i < 0 || static_cast<std::size_t>(i) >= panel->vertices.size()) return PanelError::InvalidVertex;
    if (vertexIndices.empty() || length(delta) < 1e-12) return PanelError::NothingToChange;
    for (int i : std::set<int>(vertexIndices.begin(), vertexIndices.end())) panel->vertices[static_cast<std::size_t>(i)] = add(panel->vertices[static_cast<std::size_t>(i)], delta);
    const ContourError contour = validatePanelContour(panel->vertices);
    if (why) *why = contour;
    if (contour != ContourError::None) return PanelError::InvalidContour;
    panel->direction = panelDirectionInPlane(panel->vertices, panel->direction);
    // A panel that becomes horizontal cannot keep variable rows: they stay (the dialog hides them); validatePanels would refuse the file, so drop them.
    if (isHorizontalPolygon(panel->vertices))
        panel->loads.erase(std::remove_if(panel->loads.begin(), panel->loads.end(), [](const SurfaceLoadData& d) { return d.type == SurfaceLoadType::Variable; }),
                           panel->loads.end());
    return push(project, stack, std::move(after), project.surfaceLoads, "Mover paño");
}

namespace {

enum class SplitResult { NotCrossed, Cannot, Done };

/// Splits a simple polygon (local 2D) by the line through `p` with unit direction `d`. Pieces: `first` holds vertex 0.
SplitResult splitByLine(const std::vector<Point2>& poly, const Point2& p, const Point2& d, std::vector<Point2>& first, std::vector<Point2>& second) {
    const std::size_t n = poly.size();
    constexpr double kEps = 1e-6;
    std::vector<double> dist(n);
    std::vector<int> sign(n);
    bool plus = false, minus = false;
    for (std::size_t i = 0; i < n; ++i) {
        dist[i] = d.x * (poly[i].y - p.y) - d.y * (poly[i].x - p.x);
        sign[i] = dist[i] > kEps ? 1 : (dist[i] < -kEps ? -1 : 0);
        plus = plus || sign[i] > 0;
        minus = minus || sign[i] < 0;
    }
    if (!plus || !minus) return SplitResult::NotCrossed;
    std::vector<Point2> a, b, cuts;
    for (std::size_t i = 0; i < n; ++i) {
        const std::size_t j = (i + 1) % n;
        const Point2& cur = poly[i];
        if (sign[i] > 0) a.push_back(cur);
        else if (sign[i] < 0) b.push_back(cur);
        else {
            a.push_back(cur);
            b.push_back(cur);
            cuts.push_back(cur);
        }
        if (sign[i] * sign[j] < 0) {
            const double t = dist[i] / (dist[i] - dist[j]);
            const Point2 x{cur.x + (poly[j].x - cur.x) * t, cur.y + (poly[j].y - cur.y) * t};
            a.push_back(x);
            b.push_back(x);
            cuts.push_back(x);
        }
    }
    if (cuts.size() != 2 || a.size() < 3 || b.size() < 3) return SplitResult::Cannot;
    const bool firstIsA = sign[0] >= 0;
    first = firstIsA ? a : b;
    second = firstIsA ? b : a;
    return SplitResult::Done;
}

}  // namespace

PanelError dividePanels(Project& project, UndoStack& stack, const Point3& p, const Point3& q, std::vector<int>* createdIds) {
    Panels after = project.panels;
    std::vector<int> created;
    bool crossed = false;
    const std::size_t count = after.items.size();
    for (std::size_t k = 0; k < count; ++k) {
        const Panel original = after.items[k];  // copy: the vector grows
        const auto frame = panelFrame(original.vertices);
        if (!frame) continue;
        const Point2 lp = frame->toLocal(p), lq = frame->toLocal(q);
        Point2 d{lq.x - lp.x, lq.y - lp.y};
        const double dl = std::hypot(d.x, d.y);
        if (dl < 1e-9) continue;  // the line is perpendicular to this panel: it projects to a point
        d = {d.x / dl, d.y / dl};
        std::vector<Point2> first, second;
        const SplitResult r = splitByLine(polygonLocal(*frame, original.vertices), lp, d, first, second);
        if (r == SplitResult::NotCrossed) continue;
        if (r == SplitResult::Cannot) return PanelError::CannotDivide;
        const auto world = [&](const std::vector<Point2>& pts) {
            std::vector<Point3> out;
            for (const Point2& l : pts) out.push_back(frame->toWorld(l));
            return out;
        };
        const std::vector<Point3> a = world(first), b = world(second);
        if (validatePanelContour(a) != ContourError::None || validatePanelContour(b) != ContourError::None) return PanelError::CannotDivide;
        crossed = true;
        after.items[k].vertices = a;
        after.items[k].direction = panelDirectionInPlane(a, original.direction);
        Panel piece = original;
        piece.id = after.nextId++;
        piece.vertices = b;
        piece.direction = panelDirectionInPlane(b, original.direction);
        created.push_back(piece.id);
        after.items.push_back(std::move(piece));
    }
    if (!crossed) return PanelError::NothingToDivide;
    const PanelError result = push(project, stack, std::move(after), project.surfaceLoads, "Dividir paño");
    if (result == PanelError::None && createdIds) *createdIds = std::move(created);
    return result;
}

PanelError setPanelDirection(Project& project, UndoStack& stack, const std::vector<int>& panelIds, int side, PanelDirectionMode mode) {
    const std::set<int> wanted(panelIds.begin(), panelIds.end());
    Panels after = project.panels;
    bool any = false, valid = false;
    for (Panel& p : after.items) {
        if (!wanted.count(p.id)) continue;
        any = true;
        const Point3 d = panelDirectionFromSide(p.vertices, side, mode);
        if (length(d) < 0.5) continue;
        valid = true;
        p.direction = d;
    }
    if (!any) return PanelError::NoPanels;
    if (!valid) return PanelError::InvalidSide;
    return push(project, stack, std::move(after), project.surfaceLoads, "Dirección de reparto del paño");
}

// --- surface loads ---

bool surfaceLoadOverlapsPanel(const Panels& panels, const std::vector<Point3>& contour) {
    for (const Panel& p : panels.items)
        if (panelContainsPolygon(p.vertices, contour) || panelsOverlap(p.vertices, contour)) return true;
    return false;
}

PanelError addSurfaceLoad(Project& project, UndoStack& stack, const std::vector<Point3>& contour, const SurfaceLoadData& data, bool createPanel, int* loadId,
                          int* panelId, SurfaceLoadIssue* issue, ContourError* why) {
    if (issue) *issue = {};
    const ContourError c = validatePanelContour(contour);
    if (why) *why = c;
    if (c != ContourError::None) return PanelError::InvalidContour;
    if (const PanelError e = checkRow(project, data, isHorizontalPolygon(contour), issue); e != PanelError::None) return e;
    SurfaceLoads loads = project.surfaceLoads;
    SurfaceLoad l;
    l.id = loads.nextId++;
    l.contour = contour;
    l.data = normaliseSurfaceLoadData(data);
    const int newLoad = l.id;
    loads.items.push_back(std::move(l));
    Panels panels = project.panels;
    int newPanel = 0;
    if (createPanel) {
        Panel p;
        p.id = panels.nextId++;
        p.vertices = contour;
        p.direction = panelDirectionFromSide(contour, 0, PanelDirectionMode::Parallel);  // the line of the first two points
        newPanel = p.id;
        panels.items.push_back(std::move(p));
    }
    const PanelError result = push(project, stack, std::move(panels), std::move(loads), "Nueva carga superficial");
    if (result == PanelError::None) {
        if (loadId) *loadId = newLoad;
        if (panelId) *panelId = newPanel;
    }
    return result;
}

PanelError editSurfaceLoads(Project& project, UndoStack& stack, const std::vector<int>& loadIds, const SurfaceLoadData& data, SurfaceLoadIssue* issue) {
    if (issue) *issue = {};
    const std::set<int> wanted(loadIds.begin(), loadIds.end());
    SurfaceLoads after = project.surfaceLoads;
    const SurfaceLoadData canonical = normaliseSurfaceLoadData(data);
    bool any = false;
    for (SurfaceLoad& l : after.items)
        if (wanted.count(l.id)) {
            if (const PanelError e = checkRow(project, data, isHorizontalPolygon(l.contour), issue); e != PanelError::None) return e;
            l.data = canonical;
            any = true;
        }
    if (!any) return PanelError::UnknownLoad;
    return push(project, stack, project.panels, std::move(after), "Editar cargas superficiales");
}

PanelError changeSurfaceLoadCase(Project& project, UndoStack& stack, const std::vector<int>& loadIds, int loadCaseId) {
    if (!findLoadCase(project.loadCases, loadCaseId)) return PanelError::UnknownLoadCase;
    const std::set<int> wanted(loadIds.begin(), loadIds.end());
    SurfaceLoads after = project.surfaceLoads;
    bool any = false;
    for (SurfaceLoad& l : after.items)
        if (wanted.count(l.id)) {
            l.data.loadCaseId = loadCaseId;
            any = true;
        }
    if (!any) return PanelError::UnknownLoad;
    return push(project, stack, project.panels, std::move(after), "Cambiar hipótesis de cargas superficiales");
}

PanelError deleteSurfaceLoads(Project& project, UndoStack& stack, const std::vector<int>& loadIds) {
    const std::set<int> wanted(loadIds.begin(), loadIds.end());
    SurfaceLoads after = project.surfaceLoads;
    const std::size_t before = after.items.size();
    after.items.erase(std::remove_if(after.items.begin(), after.items.end(), [&](const SurfaceLoad& l) { return wanted.count(l.id) != 0; }), after.items.end());
    if (after.items.size() == before) return PanelError::UnknownLoad;
    return push(project, stack, project.panels, std::move(after), "Borrar cargas superficiales");
}

PanelError copySurfaceLoad(Project& project, UndoStack& stack, int sourceId, const SurfaceLoadData& data, int* newId, SurfaceLoadIssue* issue) {
    if (issue) *issue = {};
    const SurfaceLoad* source = findSurfaceLoad(project.surfaceLoads, sourceId);
    if (!source) return PanelError::UnknownLoad;
    if (const PanelError e = checkRow(project, data, isHorizontalPolygon(source->contour), issue); e != PanelError::None) return e;
    SurfaceLoads after = project.surfaceLoads;
    SurfaceLoad l;
    l.id = after.nextId++;
    l.contour = source->contour;
    l.data = normaliseSurfaceLoadData(data);
    const int id = l.id;
    after.items.push_back(std::move(l));
    const PanelError result = push(project, stack, project.panels, std::move(after), "Copiar carga superficial");
    if (result == PanelError::None && newId) *newId = id;
    return result;
}

PanelError moveSurfaceLoadVertices(Project& project, UndoStack& stack, int loadId, const std::vector<int>& vertexIndices, const Point3& delta, bool alsoOtherCases,
                                   std::vector<int>* movedIds, ContourError* why) {
    if (why) *why = ContourError::None;
    const SurfaceLoad* source = findSurfaceLoad(project.surfaceLoads, loadId);
    if (!source) return PanelError::UnknownLoad;
    for (int i : vertexIndices)
        if (i < 0 || static_cast<std::size_t>(i) >= source->contour.size()) return PanelError::InvalidVertex;
    if (vertexIndices.empty() || length(delta) < 1e-12) return PanelError::NothingToChange;
    // Positions (before the move) of the vertices that move.
    std::vector<Point3> movingPoints;
    for (int i : std::set<int>(vertexIndices.begin(), vertexIndices.end())) movingPoints.push_back(source->contour[static_cast<std::size_t>(i)]);
    const auto sameZone = [&](const SurfaceLoad& other) {
        if (other.contour.size() != source->contour.size()) return false;
        for (const Point3& p : source->contour) {
            const bool found = std::any_of(other.contour.begin(), other.contour.end(), [&](const Point3& o) { return length(sub(o, p)) <= kPanelPointToleranceM; });
            if (!found) return false;
        }
        return true;
    };
    SurfaceLoads after = project.surfaceLoads;
    std::vector<int> moved;
    for (SurfaceLoad& l : after.items) {
        const bool selected = l.id == loadId;
        const bool follows = alsoOtherCases && !selected && l.data.loadCaseId != source->data.loadCaseId && sameZone(l);
        if (!selected && !follows) continue;
        for (Point3& p : l.contour)
            if (std::any_of(movingPoints.begin(), movingPoints.end(), [&](const Point3& m) { return length(sub(m, p)) <= kPanelPointToleranceM; })) p = add(p, delta);
        const ContourError c = validatePanelContour(l.contour);
        if (c != ContourError::None) {
            if (why) *why = c;
            return PanelError::InvalidContour;
        }
        moved.push_back(l.id);
    }
    const PanelError result = push(project, stack, project.panels, std::move(after), "Mover carga superficial");
    if (result == PanelError::None && movedIds) *movedIds = std::move(moved);
    return result;
}

namespace {

PanelError assignBars(Project& project, UndoStack& stack, const std::vector<int>& barIds, const bool* forced) {
    SurfaceLoads after = project.surfaceLoads;
    bool any = false;
    std::set<int> seen;
    for (int id : barIds) {
        if (!project.structure.findBar(id) || barIsTie(project, id) || !seen.insert(id).second) continue;
        any = true;
        const bool active = forced ? *forced : after.unassignedBars.count(id) != 0;  // toggling an inactive bar activates it
        if (active) after.unassignedBars.erase(id);
        else after.unassignedBars.insert(id);
    }
    if (!any) return PanelError::NoBars;
    return push(project, stack, project.panels, std::move(after), "Asignar cargas superficiales a barras");
}

}  // namespace

PanelError toggleSurfaceBarAssignment(Project& project, UndoStack& stack, const std::vector<int>& barIds) { return assignBars(project, stack, barIds, nullptr); }

PanelError setSurfaceBarAssignment(Project& project, UndoStack& stack, const std::vector<int>& barIds, bool active) { return assignBars(project, stack, barIds, &active); }

}  // namespace cc::core
