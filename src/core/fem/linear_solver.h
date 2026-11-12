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
///  4d. Relations (T-P8-03): ligatures, diaphragms and oblique supports become linear relations eliminated exactly by transformation
///      (K_r = T' K T, f_r = T' f, u = T u_r); springs to the ground are linear (their "Sentido" is ignored here, as in the original's linear
///      analysis [V]: see solveNonlinear). Diaphragm loads go to the master node with Mz = (x - xm) Fy - (y - ym) Fx.
///  4e. Shell edge bonds (T-P8-03, TK-110): elastic edges = springs per metre integrated over the tributary length of every mesh node of the
///      edge (ShellMesh::edgeSprings, with their "Sentido" in solveNonlinear); oblique edges = the same exact relation as oblique nodes on every
///      mesh node of the edge; interior bonds and partially fixed corners = copies of the mesh nodes joined by relations / two-node springs
///      (ShellMesh::relations / links). Diaphragms with shells tie every mesh node of those shells.
///  5. Null pivot -> SolveStatus::Mechanism (#214001), negative pivot -> SolveStatus::Unstable (#380376), a loaded DOF without stiffness ->
///     Mechanism for that case. Results: nodal displacements, reactions, bar end forces.
LinearResult solveLinear(const FrameModel& model, std::span<const LoadCaseInput> cases, const SolverOptions& options = {});

/// Combined load case: sum of coefficients[i] x cases[i] (nodal, bar, shell and diaphragm loads, temperatures and prescribed displacements;
/// scaling the prescribed displacements is [N]). `coefficients` has one value per case (missing = 0).
LoadCaseInput combineLoadCases(std::span<const LoadCaseInput> cases, std::span<const double> coefficients, int id, const std::string& name);

/// True when some spring to the ground has a "Sentido" other than Both: the original then enables «Análisis no lineal» and generates one
/// non-linear combination per combination [V, solver_diafragmas_ligaduras_apoyos.md 10.1].
bool hasUnilateralSprings(const FrameModel& model);

/// Non-linear combinations (T-P8-03): every input case is one combined case (see combineLoadCases; the original names them "[1] PP",
/// "[2] 1.35·PP"). Each one is solved on its own (no superposition) with the springs that have a "Sentido" acting only while the
/// displacement of their DOF has that sign [V 10.1]: active-set iteration starting with every spring active, re-solving (and refactorising)
/// whenever the set changes. A final set that leaves a mechanism gives mechanismMessage(name) [V]; a set that does not settle within
/// `maxIterations` (or repeats) gives SolveStatus::NotConverged with nonConvergenceMessage(name) (#328573). Without unilateral springs it
/// equals solveLinear.
LinearResult solveNonlinear(const FrameModel& model, std::span<const LoadCaseInput> combinedCases, const SolverOptions& options = {},
                            int maxIterations = 50);

/// «Con las condiciones impuestas, no se alcanza la convergencia para el caso de carga '%s'. Revise las condiciones del modelo.» (#328573,
/// cyopensees.dll).
std::string nonConvergenceMessage(const std::string& caseName);

/// «No es posible calcular el caso de carga '%s'. La estructura es un mecanismo. Revise la definición de cargas y las condiciones de
/// vinculación del modelo.» (#214001, cyopensees.dll).
std::string mechanismMessage(const std::string& caseName);
/// «No es posible calcular el caso de carga '%s', ya que la estructura es inestable. Revise la definición de cargas y las condiciones de
/// contorno introducidas.» (#380376, cyopensees.dll).
std::string unstableMessage(const std::string& caseName);
/// «Se ha producido un error calculando el caso de carga '%s'. Revise las condiciones del modelo.» (#214000, cyopensees.dll).
std::string caseErrorMessage(const std::string& caseName);
/// «Se ha encontrado un nudo que no está conectado a ninguna barra ni a ninguna lámina.» (#64010, ed3dlib.dll): solveLinear fails with it when
/// interior edge bonds «Libre» leave a shell corner without any element (ShellMesh::orphanModelNodes) [V 11.4].
std::string detachedNodeMessage();

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

/// Section forces at `x` metres from nodeI inside the piece [x0, x1] of a bar of length `barLength` (a whole bar: 0, L, last = true), from the
/// section forces at x1 (convention of applySignConvention) and the loads of the bar: loads at exactly x1 belong to the next piece unless `last`.
/// fem/results.h uses it for bars split along shell edges (LoadCaseResult::barSegments).
Dof6 pieceForcesAt(const FemBar& bar, double barLength, double x0, double x1, bool last, const Dof6& endForces,
                   std::span<const FemBarLoad> barLoads, double x);

/// Deformed shape: global displacements (Dx, Dy, Dz, Gx, Gy, Gz; m, rad) at `x` metres from nodeI inside the piece [x0, x1] of a bar. EXACT for
/// the element of the solver (not a cubic interpolation): the displacement of the piece at x0 (element side, so a released / partially fixed
/// end rotates with the bar) plus the integral of the strains of the section forces (N / EA, V / G Av when shear deformation is considered,
/// T / G J, M / E I) and of the temperature loads, by the unit load method on the piece clamped at x0. At x1 it closes on the displacement of the
/// other end.
Dof6 pieceDisplacementAt(const FemBar& bar, double barLength, double x0, double x1, bool last, const Dof6& originDisplacement,
                         const Dof6& endForces, std::span<const FemBarLoad> barLoads, double x, const SolverOptions& options = {});

}  // namespace cc::fem
