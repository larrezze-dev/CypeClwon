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
///  4b. Rule [R] of TK-108 refined (solver_medidas_original.md 10.3): every null mode of the stiffness (null pivot) is fixed to 0
///     (LinearResult::nullModeDofs); a load case that does not excite it calculates without a warning, a case whose loads excite it is a
///     mechanism with the warning of the original (mechanismWarning / mechanismWarningZone) naming the DOF of largest amplitude of the mode.
///  4c. Shells (T-P8-02): meshed by meshShells(), bars along shell edges split at the mesh nodes, 6-node shell triangles assembled with the bars.
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

/// «Se han obtenido desplazamientos demasiado grandes. Puede consultar los mensajes de aviso correspondientes mediante la opción
/// 'Mostrar/Oculta incidencias' del menú 'Cálculo'.» Text shown by the original (measured, solver_medidas_original.md 9.2; the resource id was not
/// located [N]). Raised when a rotation component of a node of a bar exceeds SolverOptions::maxNodeRotation (pi/2 [V]); which component or norm is checked is [N], and nodes reached only by shells are not checked [N, elemento_lamina_bandas.md 17.2].
std::string excessiveDisplacementsMessage();

/// «Esta estructura es un mecanismo. Revise los datos introducidos. / Giro alrededor del eje Y / El error se encuentra en el punto de
/// coordenadas: / (x = 0.000000, y = 0.000000, z = 0.000000) / Alguna de las barras de la estructura no está vinculada correctamente en sus
/// extremos, es decir, no es estable.» (lines separated by a newline; measured in the original; the DOF names are frontlib.dll ids 153556..153561).
/// `dof` 0..5 = Dx, Dy, Dz, Gx, Gy, Gz. This is what the user sees for a null pivot (not the #214001 text of mechanismMessage()).
std::string mechanismWarning(int dof, const Vec3& position);
/// Variant of mechanismWarning() measured for a node of a vertical bar [V, solver_medidas_original.md 10.3]: «... / El error se encuentra en la
/// zona de coordenadas: (x1,y1) a (x2,y2), en la cota z / ...» (plan coordinates of the ends of the bar and the level of the node).
std::string mechanismWarningZone(int dof, double x1, double y1, double x2, double y2, double z);

/// THE sign convention of section forces, MEASURED in the original (TK-107, docs/research/solver_medidas_original.md section 3.2). Input: the
/// canonical internal resultant at a cut, i.e. the force and moment exerted on the part [0, x] of the bar by the part [x, L], in local axes
/// (N, Vy, Vz, T, My, Mz). N, Vy, Vz, T and Mz are kept (N > 0 is tension) and ONLY My is inverted: a simply supported beam loaded
/// downwards along -z has My > 0 at midspan, a cantilever loaded along -z has My < 0 at the clamp (dMy/dx = -Vz). Every section force the
/// solver returns goes through this function, so changing the convention means changing only its body.
Dof6 applySignConvention(const Dof6& canonical);
/// Inverse of applySignConvention().
Dof6 removeSignConvention(const Dof6& convention);

/// Section forces at `x` metres from nodeI (convention of applySignConvention), from the end forces of a solved bar and the loads of the
/// same bar and case (global loads are rotated with the bar axes). Thermal loads do not change the statics. For diagrams (T-P8-06).
Dof6 sectionForcesAt(const FemBar& bar, double length, const BarEndForces& forces, std::span<const FemBarLoad> barLoads, double x);

}  // namespace cc::fem
