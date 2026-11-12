#pragma once
#include <array>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "core/fem/frame_model.h"

namespace cc::fem {

/// Mesh of the shells of a model (see fem/shell_model.h for the rules). Used by solveLinear(); exposed for the GUI and the tests.
ShellMesh meshShells(const FrameModel& model);

/// Method of "Integración de esfuerzos en bandas" [V, section 13.1]: internal stresses smoothed (default), internal stresses without smoothing,
/// nodal forces (equilibrium of the part of the band on one side of the cut; much coarser with a coarse mesh, as in the original).
enum class BandMethod { SmoothedStresses, UnsmoothedStresses, NodalForces };

/// An integration band of one shell: centre line (x1, y1)-(x2, y2) in the local 2D system of the shell (metres from its first contour node)
/// and width (m).
struct BandSpec {
    int shell = 0;   ///< FemShell::id
    double x1 = 0, y1 = 0, x2 = 0, y2 = 0;
    double width = 0.15;
};

/// Section forces of a band at a point, like a bar [V, section 13.1]: total resultants N, Vy, Vz (kN), Mt, My, Mz (kN.m) in the axes of the band
/// (x along the centre line from (x1, y1), y in the plane, z = shell z), in the sign convention of the bars (applySignConvention): with a load
/// along +z and Mx > 0 in the plate, a band along x has My = -Mx * width and Vz = +Qx * width (measured).
struct BandPoint {
    double s = 0.0;                  ///< distance from (x1, y1), m
    double leftWidth = 0.0, rightWidth = 0.0;  ///< half widths actually integrated (clipped by the contour of the shell)
    std::array<double, 6> forces{};  ///< N, Vy, Vz, Mt, My, Mz
};

/// Forces of a band at the distance `s` (m) along its centre line. A point on the contour is moved inside by min(1 % of the length, 0.01 m)
/// [V, cefbandint]. nullopt when the shell or the case has no results or the point is outside the shell.
std::optional<BandPoint> bandForcesAt(const FrameModel& model, const LinearResult& result, std::size_t caseIndex, const BandSpec& band,
                                      double s, BandMethod method = BandMethod::SmoothedStresses);
/// `points` equally spaced points from one end of the band to the other ("Número de puntos de esfuerzo", default 10).
std::vector<BandPoint> integrateBand(const FrameModel& model, const LinearResult& result, std::size_t caseIndex, const BandSpec& band,
                                     int points = 10, BandMethod method = BandMethod::SmoothedStresses);

/// Smoothed shell forces at the corner mesh nodes of a shell (mesh node index -> forces): plain arithmetic mean of the values of the triangles
/// of the shell that share the node [V, section 16.2]. Mid-side nodes are not smoothed and not listed.
std::map<int, ShellForces> smoothedShellForces(const FrameModel& model, const LinearResult& result, std::size_t caseIndex, int shellId);

/// Shell forces at a local 2D point of a shell: linear interpolation between the three corners of the triangle that contains the point, of the
/// smoothed nodal values or (smoothed = false) of the values of that triangle at its corners [N interpolation, section 16.3]. nullopt outside the
/// shell.
std::optional<ShellForces> shellForcesAt(const FrameModel& model, const LinearResult& result, std::size_t caseIndex, int shellId, double x,
                                         double y, bool smoothed = true);

/// Displacements (global Dx..Gz) at a local 2D point of a shell: quadratic interpolation of the six nodes of the triangle that contains the point
/// (deformed shape of the shells; the interpolation is the clone's [N]). nullopt outside the shell or without results.
std::optional<Dof6> shellDisplacementAt(const FrameModel& model, const LinearResult& result, std::size_t caseIndex, int shellId, double x,
                                        double y);

}  // namespace cc::fem
