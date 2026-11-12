#pragma once
#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "core/fem/frame_model.h"

namespace cc::fem {

/// Results layer (T-P8-06): queries on a solved case for the result panels of the original (Cálculo > Desplazamientos / Reacciones / Esfuerzos,
/// deformed shape). Source: docs/research/resultados_calculo.md. Every query takes the model, one LoadCaseResult and the LoadCaseInput of the same
/// case (its bar loads are needed between the ends of a bar). Units: m, rad, kN, kN.m.
///
/// Conventions (measured, docs/research/solver_medidas_original.md section 3):
///  - Displacements Dx, Dy, Dz, Gx, Gy, Gz: global axes, right-hand rule.
///  - Reactions Rx, Ry, Rz, Mx, My, Mz: forces of the supports on the structure, global axes (LoadCaseResult::reactions).
///  - Bar forces N, Vy, Vz, Mt, My, Mz: local axes of the bar at the point, convention of applySignConvention() (TK-107: N > 0 tension, only My
///    opposite to the canonical resultant; simply supported beam loaded downwards: My > 0 at midspan).

/// Length of the bar FrameModel::bars[barIndex] (0 when a node is missing).
double barLength(const FrameModel& model, std::size_t barIndex);
/// Bar loads of `input` acting on the bar with that id.
std::vector<FemBarLoad> loadsOfBar(const LoadCaseInput& input, int barId);
/// Displacements of a node (global), nullopt when the node is not in the model or the case has no results.
std::optional<Dof6> nodeDisplacement(const FrameModel& model, const LoadCaseResult& result, int nodeId);

/// Section forces at `x` metres from nodeI of the bar (convention above). A bar split along a shell edge is evaluated in the piece that contains x
/// (LoadCaseResult::barSegments); at a piece boundary the piece before it is used. Zero when the case has no results.
Dof6 barForcesAt(const FrameModel& model, const LoadCaseResult& result, const LoadCaseInput& input, std::size_t barIndex, double x);

/// Deformed shape: global displacements and rotations at `x` metres from nodeI of the bar (pieceDisplacementAt: the exact field of the element,
/// including shear deformation and temperature, starting from the displacement of the bar END at nodeI, so released / partially fixed ends are
/// honoured). Zero when the case has no results.
Dof6 barDisplacementAt(const FrameModel& model, const LoadCaseResult& result, const LoadCaseInput& input, std::size_t barIndex, double x,
                       const SolverOptions& options = {});

/// One point of the laws of a bar.
struct BarLawPoint {
    double x = 0.0;      ///< metres from nodeI
    Dof6 forces{};       ///< N, Vy, Vz, Mt, My, Mz
    Dof6 displacement{}; ///< global Dx..Gz
};
/// `divisions` + 1 equally spaced points from nodeI to nodeJ, plus the piece boundaries of a split bar and the positions of the point loads of the
/// case (each taken just before and just after the load, 1e-9 m apart, so a jump of the law is drawn vertically). How many points the original
/// samples for its laws is [N] (resultados_calculo.md section 4).
std::vector<BarLawPoint> barLaw(const FrameModel& model, const LoadCaseResult& result, const LoadCaseInput& input, std::size_t barIndex,
                                int divisions = 20, const SolverOptions& options = {});

/// Linear superposition: sum of coefficients[i] x cases[i] for displacements, reactions, bar end forces and displacements, bar pieces, mesh
/// displacements and reactions, and shell forces (every quantity of a linear case is linear in the loads). When a case with a non-zero coefficient
/// failed (status not Ok) the result carries its status and message and no values: a combination cannot be presented then.
LoadCaseResult superposeCases(std::span<const LoadCaseResult> cases, std::span<const double> coefficients, int id, const std::string& name);

/// The combinations `coefficientSets` (one coefficient per case of `base`, in its order) of a linear result as a new LinearResult with the same
/// mesh and model data (so the shell queries of fem/shell_analysis.h work on it with the index of the combination).
LinearResult superposeResult(const LinearResult& base, std::span<const std::vector<double>> coefficientSets, std::span<const std::string> names);

}  // namespace cc::fem
