#pragma once
#include <string>
#include <vector>

#include "core/panel.h"
#include "core/project.h"

namespace cc::core {

/// Reparto ("distribución") of the loads of the panels onto the bars, as a pure function of the geometry. Source: docs/research/ui/cargas_panos.md section 9.2
/// (REA of panyos3d.dll p3dcalsup.c, algorithm per load) and section 10.2 (measured: 4 x 4 m panel on four bars, 10 kN/m2 -> 20 kN/m on the bars
/// perpendicular to the arrow).
///
/// Algorithm (per load, in the plane of the panel). d = direction of distribution, t = d turned 90 degrees. The discretisation line goes through the
/// centroid of the LOADED polygon along t; the cut points on it are the projections of the vertices of the loaded polygon, of the intersections of every
/// bar with it and of the ends of the bars that are not parallel to d. Each pair of consecutive cut points is a strip of width = difference; its centre
/// line (along d) is clipped to the polygon into segments. Each segment gives a force = area of the trapezoid (value x width along the segment) applied at
/// its centroid; the force goes to the nearest bar ahead and behind along d with the fraction d_other / (d_own + d_other) (one side only: all of it; none:
/// not assigned, warning #64202), and on every bar it becomes a UNIFORM linear load q = |F x fraction| / (s2 - s1) over the stretch [s1, s2] of the bar
/// that the strip covers (measured from the start of the bar); |q| <= 1e-4 is dropped. Adjacent stretches with the same q are merged.
///
/// ASSUMPTIONS (not measured, see the notes of the research file): the load of a segment is split at a sign change of a variable load (the REA asserts
/// the same sign); the value of the load is per m2 of the panel (not of its horizontal projection); a bar is associated with the panel when it lies in its
/// plane (0.001 m) and overlaps its polygon; bars parallel to d never receive load; a surface load is distributed with the direction and the bars of the
/// panel that contains it, over its own polygon only.

struct DistributionBar {
    int id = 0;
    Point3 a, b;  ///< start / end of the bar
};

/// A uniform linear load on a bar, over [s1, s2] metres from its start. The magnitude is positive; `direction` is the unit vector in which it acts.
struct PanelBarLoad {
    int panelId = 0;
    int surfaceLoadId = 0;  ///< 0 = a load of the panel itself
    int barId = 0;
    int loadCaseId = 0;
    double s1 = 0.0, s2 = 0.0;
    double q = 0.0;  ///< kN/m
    Point3 direction;
    bool operator==(const PanelBarLoad&) const = default;
};

struct DistributionInput {
    std::vector<Point3> polygon;  ///< loaded region (the panel itself, or a surface load)
    std::vector<Point3> support;  ///< polygon of the panel whose bars carry the load; empty = `polygon`
    Point3 direction;             ///< direction of distribution (unit, in the plane)
    std::vector<DistributionBar> bars;
    SurfaceLoadData load;
    Point3 positiveDirection;  ///< unit vector of a positive value (panelPositiveDirection)
    int panelId = 0, surfaceLoadId = 0;
    bool compact = true;       ///< merge adjacent stretches with equal q
};

struct DistributionResult {
    bool valid = false;                 ///< the polygon has a plane and the direction is usable
    std::vector<PanelBarLoad> loads;
    std::vector<Point3> unassignedPoints;  ///< points of application of forces that no bar could take (#64202)
    bool unassigned() const { return !unassignedPoints.empty(); }
};
DistributionResult distributeSurfaceLoad(const DistributionInput& input);

/// Bars associated with a panel: in its plane (0.001 m) and overlapping its polygon.
std::vector<DistributionBar> barsOfPanel(const std::vector<Point3>& polygon, const std::vector<DistributionBar>& bars);
/// True when the two polygons lie in one plane and share some area.
bool panelsOverlap(const std::vector<Point3>& a, const std::vector<Point3>& b);

/// A tie bar ("tirante") takes no loads. Looks the description up through the cross section of the bar.
bool barIsTie(const Project& project, int barId);

struct PanelWarning {
    enum class Kind { NoBars, Overlap, NotCoveredByShells, NotAssigned };
    Kind kind = Kind::NoBars;
    int panelId = 0;
    int otherPanelId = 0;  ///< Overlap
    Point3 where;          ///< NotAssigned: the point of application
    std::string message;   ///< #64203 / #64205 / #64204 / #64206 / #64202
};
struct PanelDistribution {
    std::vector<PanelBarLoad> loads;
    std::vector<PanelWarning> warnings;
};
/// All the panels of the project: their own rows and the surface loads they contain, on the bars that "Asignar" left active and that are not ties.
/// Warnings: #64204 overlapped panels, #64203 / #64205 panel without bars (with shells in the project: none when the shells cover it completely, #64206
/// when partially, #64205 when not at all; the transfer to shells is NOT implemented), #64202 force not assigned (with its point).
PanelDistribution distributePanels(const Project& project);

}  // namespace cc::core
