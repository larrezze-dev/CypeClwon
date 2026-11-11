#pragma once
#include <array>
#include <span>
#include <string>

#include "core/fem/frame_model.h"

namespace cc::fem {

/// Linear static solver of 3D frames (T-P8-01). See fem/frame_model.h for units and conventions.
///
/// Algorithm (docs/research/solver_lineal_portico.md section 11.2):
///  1. 6 DOFs per node in FrameModel::nodes order.
///  2. Local 12x12 stiffness (Euler-Bernoulli or Timoshenko per plane); released / partially fixed rotations are extra internal DOFs joined to
///     the node by a spring (0 = free) and statically condensed (exact; the original uses relations of stiffness 1e-3 instead).
///  3. Bar loads -> exact fixed-end reactions by the flexibility method (cantilever integrated with Gauss over the load breakpoints,
///     shear flexibility included), condensed like the stiffness. Temperature as initial strains.
///  4. Global sparse assembly, DOFs without stiffness locked (LinearResult::lockedDofs), supports eliminated, prescribed displacements moved
///     to the right-hand side, one sparse LDLT factorisation for every load case.
///  5. Null pivot -> SolveStatus::Mechanism (#214001), negative pivot -> SolveStatus::Unstable (#380376), a loaded DOF without stiffness ->
///     Mechanism for that case. Results: nodal displacements, reactions, bar end forces.
LinearResult solveLinear(const FrameModel& model, std::span<const LoadCaseInput> cases, const SolverOptions& options = {});

/// «No es posible calcular el caso de carga '%s'. La estructura es un mecanismo. Revise la definición de cargas y las condiciones de
/// vinculación del modelo.» (#214001, cyopensees.dll).
std::string mechanismMessage(const std::string& caseName);
/// «No es posible calcular el caso de carga '%s', ya que la estructura es inestable. Revise la definición de cargas y las condiciones de
/// contorno introducidas.» (#380376, cyopensees.dll).
std::string unstableMessage(const std::string& caseName);
/// «Se ha producido un error calculando el caso de carga '%s'. Revise las condiciones del modelo.» (#214000, cyopensees.dll).
std::string caseErrorMessage(const std::string& caseName);

/// THE sign convention of section forces (the original's convention is not verified: TK-107). Input: the canonical internal resultant at a
/// cut, i.e. the force and moment exerted on the part [0, x] of the bar by the part [x, L], in local axes (N, Vy, Vz, T, My, Mz). With the
/// identity chosen here: N > 0 is tension; Vy, Vz, T, My, Mz are the components along the local axes of the action of the far part
/// (a simply supported beam loaded downwards along -z has My < 0 at midspan). Every section force the solver returns goes through this
/// function, so changing the convention means changing only its body.
Dof6 applySignConvention(const Dof6& canonical);
/// Inverse of applySignConvention().
Dof6 removeSignConvention(const Dof6& convention);

/// Section forces at `x` metres from nodeI (convention of applySignConvention), from the end forces of a solved bar and the loads of the
/// same bar and case (global loads are rotated with the bar axes). Thermal loads do not change the statics. For diagrams (T-P8-06).
Dof6 sectionForcesAt(const FemBar& bar, double length, const BarEndForces& forces, std::span<const FemBarLoad> barLoads, double x);

}  // namespace cc::fem
