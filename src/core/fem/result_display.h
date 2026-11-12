#pragma once
#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "core/fem/project_analysis.h"

namespace cc::fem {

/// Presentation of the results in the panels «Propiedades - Desplazamientos / Reacciones / Esfuerzos» (T-P8-06, GUI part). Qt-free so it can be
/// tested without a screen. Sources: docs/research/resultados_calculo.md section 1.1, docs/research/ui/resultados_paneles.md and the
/// screenshots s_lin.png / s_nl2.png.

/// Entries of the first combo of «Comb. seleccionada»: «Hipótesis simple» (#64399) and «Combinaciones no lineales» (#377012) [V]. The families
/// of the linear combinations the original also lists there were not captured [N] and are not offered.
enum class ResultFamily { LoadCases, Nonlinear };
std::string resultFamilyName(ResultFamily family);
/// Families that have results: the load cases once the linear analysis succeeded, the non-linear combinations once some was solved.
std::vector<ResultFamily> availableFamilies(const ProjectAnalysis& analysis);
/// Second combo: the load case labels («Peso propio», ...) or the labels «[n] referencia» of the solved non-linear combinations [V].
std::vector<std::string> resultItemNames(const ProjectAnalysis& analysis, ResultFamily family);

/// One presentable case: its result, the loads of the case (needed between the ends of a bar) and the linear result that holds it with its index
/// (shell queries).
struct ResultRef {
    const LoadCaseResult* result = nullptr;
    const LoadCaseInput* input = nullptr;
    const LinearResult* linear = nullptr;
    std::size_t caseIndex = 0;
};
/// nullopt when the item does not exist or failed (a failed non-linear combination has no values).
std::optional<ResultRef> resultOf(const ProjectAnalysis& analysis, ResultFamily family, std::size_t index);

/// Fixed-point text with a point as decimal separator, never "-0.000" (a value that rounds to zero is written without sign) [V s_lin.png].
std::string formatFixed(double value, int decimals);

/// Mask of the check boxes Dx, Dy, Dz, Gx, Gy, Gz (or Rx..Mz, N..Mz): all on by default [V s_lin.png].
using ComponentMask = std::array<bool, 6>;
inline constexpr ComponentMask kAllComponents{true, true, true, true, true, true};

/// Balloon of a node in «Desplazamientos»: "Dx: 0.000 mm" ... "Gz: 0.000 mRad" (m and rad in, three decimals) [V s_lin.png]. Each line is
/// split into the bold label ("Dx:") and the value text (" 0.000 mm").
struct BalloonLine {
    std::string label;
    std::string value;
};
std::vector<BalloonLine> displacementLines(const Dof6& d, const ComponentMask& shown = kAllComponents);
/// Balloon of a node in «Reacciones»: "Rx: 0.000 kN" (forces, three decimals) and "Mx: 0.00 kN·m" (moments, two decimals; resolution read in
/// the original: 0.001 kN and 0.01 kN·m, solver_medidas_original.md section 1).
std::vector<BalloonLine> reactionLines(const Dof6& r, const ComponentMask& shown = kAllComponents);
/// Balloon of «Consultar valores»: "X: 2.934 m" then N, Vy, Vz (kN, three decimals) and Mt, My, Mz (kN·m, two decimals) [V section 1 / 3.2].
std::vector<BalloonLine> forceLines(double x, const Dof6& f, const ComponentMask& shown = kAllComponents);

/// Index in FrameModel::nodes / bars of an id (project ids are kept by buildFrameModel; nodes without bars are left out, #64010).
std::optional<std::size_t> nodeIndexOf(const FrameModel& model, int nodeId);
std::optional<std::size_t> barIndexOf(const FrameModel& model, int barId);
/// Point of the bar axis at `x` metres from nodeI.
Vec3 barPointAt(const FrameModel& model, std::size_t barIndex, double x);
/// Diagonal of the bounding box of the nodes (0 without nodes).
double modelDiagonal(const FrameModel& model);

/// Drawing scale of a result whose largest absolute value is `maxValue` so that it is drawn `fraction` of `modelSize` long. 0 when there is
/// nothing to draw. The defaults of the original for the deformed shape of bars and for the laws are [N] (TK-104): the clone draws the largest
/// value at kDefaultResultFraction of the model diagonal.
inline constexpr double kDefaultResultFraction = 0.1;
/// Scales of the panel «Esfuerzos» (rows with a scale field, defaults 0.100 for the laws and 100.000 for the deformed shape [V,
/// resultados_paneles.md]). Measured roughly: with 0.100 a law is drawn ~0.0099 m per kN·m, i.e. drawn length (m) = value x scale x 0.1 [≈V];
/// the deformed shape is drawn as position + scale x displacement (m) [N: the unit of the field is not shown].
inline constexpr double kLawMetresPerUnit = 0.1;
inline double lawDrawLength(double value, double scale) { return value * scale * kLawMetresPerUnit; }
double automaticScale(double maxValue, double modelSize, double fraction = kDefaultResultFraction);

/// Largest translation of the bars and nodes of a case (sampled along every bar at `divisions` intervals), m.
double maxTranslation(const FrameModel& model, const ResultRef& ref, int divisions = 20, const SolverOptions& options = {});
/// Deformed axis of a bar: `divisions` + 1 points, position + scale * translation (global).
std::vector<Vec3> deformedBar(const FrameModel& model, const ResultRef& ref, std::size_t barIndex, double scale, int divisions = 20,
                              const SolverOptions& options = {});

/// Laws of «Esfuerzos» (component 0..5 = N, Vy, Vz, Mt, My, Mz). The direction a law is drawn in: local z for N, Vz, Mt and My, local y for Vy
/// and Mz (the clone's choice; the drawing convention of the original is [N] until captured).
Vec3 lawDirection(const FrameModel& model, std::size_t barIndex, int component);
/// Largest absolute value of a component over the given bars (barLaw with `divisions`).
double maxLawValue(const FrameModel& model, const ResultRef& ref, const std::vector<std::size_t>& barIndices, int component, int divisions = 20,
                   const SolverOptions& options = {});

}  // namespace cc::fem
