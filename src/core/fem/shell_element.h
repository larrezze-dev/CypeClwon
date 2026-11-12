#pragma once
// Internal header of the solver (not part of the public API): the 6-node shell triangle of T-P8-02 (see fem/shell_model.h).
#include <array>

#include <Eigen/Dense>

#include "core/fem/shell_model.h"

namespace cc::fem::detail {

using Mat36 = Eigen::Matrix<double, 36, 36>;
using Vec36 = Eigen::Matrix<double, 36, 1>;

/// Stiffness factor that replaces a factor 0 in the stiffness [V measured, section 15.4: with every out-of-plane factor 0 the original
/// calculates without a message and the displacement grows about 1e9 times].
constexpr double kMinFactor = 1e-9;

struct ShellElement {
    Eigen::Matrix3d r = Eigen::Matrix3d::Identity();  ///< rows: ex, ey, ez of the shell (global -> local)
    std::array<std::array<double, 2>, 6> xy{};       ///< local coordinates of the nodes from node 0
    double area = 0.0;
    Eigen::Matrix3d dm = Eigen::Matrix3d::Zero(), db = Eigen::Matrix3d::Zero();
    Eigen::Matrix2d ds = Eigen::Matrix2d::Zero();
    /// Section of the stiffness: a factor 0 is replaced by kMinFactor (dm, db, ds above). The forces are recovered with the nominal
    /// factors (a factor 0 gives exactly 0, as the original lists [V, section 15.4]).
    Eigen::Matrix3d dmForces = Eigen::Matrix3d::Zero(), dbForces = Eigen::Matrix3d::Zero();
    Eigen::Matrix2d dsForces = Eigen::Matrix2d::Zero();
    Eigen::Matrix<double, 8, 36> shearTying = Eigen::Matrix<double, 8, 36>::Zero();  ///< assumed covariant shear strain coefficients
    Mat36 kLocal = Mat36::Zero();
    Mat36 kGlobal = Mat36::Zero();
    bool valid = false;
};

/// Element of shell `sh` on the mesh nodes at `pos` (corners counter-clockwise about ez, then mid-sides 01, 12, 20).
ShellElement buildShellElement(const FemShell& sh, const std::array<Vec3S, 6>& pos);

/// Equivalent nodal forces (global DOFs) of a load on the element. `shellXY` = local 2D coordinates of the six nodes in the system of the shell
/// (from its first contour node), used by the affine pressure field.
Vec36 shellLoadVector(const ShellElement& el, const FemShell& sh, const FemShellLoad& load, const std::array<std::array<double, 2>, 6>& shellXY);
/// Initial membrane strain (exx, eyy) of a thermal / shrinkage load ({0, 0} for a pressure).
std::array<double, 2> initialStrain(const FemShell& sh, const FemShellLoad& load);

/// Shell forces at the six nodes from the global displacements of the element and the initial membrane strain of the case.
std::array<ShellForces, 6> shellForcesAtNodes(const ShellElement& el, const Vec36& uGlobal, const std::array<double, 2>& strain0);

/// Shell forces at the three Gauss points (1/6, 1/6), (2/3, 1/6), (1/6, 2/3).
std::array<ShellForces, 3> shellForcesAtGauss(const ShellElement& el, const Vec36& uGlobal, const std::array<double, 2>& strain0);

/// Shape functions of the 6-node triangle at (r, s).
std::array<double, 6> tri6Shape(double r, double s);

}  // namespace cc::fem::detail
