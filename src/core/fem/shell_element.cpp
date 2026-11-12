#include "core/fem/shell_element.h"

#include <cmath>

namespace cc::fem::detail {

namespace {

struct GaussPoint {
    double r, s, w;  // w includes the 1/2 of the reference triangle
};
// 3-point rule (degree 2): membrane, bending, drilling-free terms.
constexpr GaussPoint kG3[3] = {{1.0 / 6, 1.0 / 6, 1.0 / 6}, {2.0 / 3, 1.0 / 6, 1.0 / 6}, {1.0 / 6, 2.0 / 3, 1.0 / 6}};
// 6-point rule (degree 4): assumed shear, drilling penalty, Winkler springs, pressure loads.
constexpr double kA6 = 0.445948490915965, kB6 = 0.091576213509771, kWa = 0.223381589678011 / 2, kWb = 0.109951743655322 / 2;
constexpr GaussPoint kG6[6] = {{kA6, kA6, kWa}, {1 - 2 * kA6, kA6, kWa}, {kA6, 1 - 2 * kA6, kWa},
                               {kB6, kB6, kWb}, {1 - 2 * kB6, kB6, kWb}, {kB6, 1 - 2 * kB6, kWb}};
constexpr double kNodeR[6] = {0, 1, 0, 0.5, 0.5, 0};
constexpr double kNodeS[6] = {0, 0, 1, 0, 0.5, 0.5};

struct Derivatives {
    std::array<double, 6> n{}, dr{}, ds{}, dx{}, dy{};
};

Derivatives shapeAt(const ShellElement& el, const Eigen::Matrix2d& jinv, double r, double s) {
    Derivatives d;
    const double t = 1 - r - s;
    d.n = tri6Shape(r, s);
    d.dr = {-(4 * t - 1), 4 * r - 1, 0, 4 * (t - r), 4 * s, -4 * s};
    d.ds = {-(4 * t - 1), 0, 4 * s - 1, -4 * r, 4 * r, 4 * (t - s)};
    for (int k = 0; k < 6; ++k) {
        d.dx[k] = jinv(0, 0) * d.dr[k] + jinv(0, 1) * d.ds[k];
        d.dy[k] = jinv(1, 0) * d.dr[k] + jinv(1, 1) * d.ds[k];
    }
    (void)el;
    return d;
}

// Local DOFs per node: u, v, w, thx, thy, thz. Rotations of the normal: beta_x = thy (u = z beta_x), beta_y = -thx (v = z beta_y).
constexpr int U = 0, Vd = 1, W = 2, TX = 3, TY = 4, TZ = 5;

Eigen::Matrix<double, 3, 36> membraneB(const Derivatives& d) {
    Eigen::Matrix<double, 3, 36> b = Eigen::Matrix<double, 3, 36>::Zero();
    for (int k = 0; k < 6; ++k) {
        b(0, 6 * k + U) = d.dx[k];
        b(1, 6 * k + Vd) = d.dy[k];
        b(2, 6 * k + U) = d.dy[k];
        b(2, 6 * k + Vd) = d.dx[k];
    }
    return b;
}

Eigen::Matrix<double, 3, 36> bendingB(const Derivatives& d) {
    Eigen::Matrix<double, 3, 36> b = Eigen::Matrix<double, 3, 36>::Zero();
    for (int k = 0; k < 6; ++k) {
        b(0, 6 * k + TY) = d.dx[k];   // kx = d beta_x / dx
        b(1, 6 * k + TX) = -d.dy[k];  // ky = d beta_y / dy
        b(2, 6 * k + TY) = d.dy[k];   // kxy = d beta_x / dy + d beta_y / dx
        b(2, 6 * k + TX) = -d.dx[k];
    }
    return b;
}

/// Covariant transverse shear strains (e_r, e_s) of the displacement field at (r, s): e_r = dw/dr + beta . dx/dr.
void covariantShear(const ShellElement& el, double r, double s, Eigen::Matrix<double, 1, 36>& er, Eigen::Matrix<double, 1, 36>& es) {
    const double xr = el.xy[1][0] - el.xy[0][0], yr = el.xy[1][1] - el.xy[0][1];
    const double xs = el.xy[2][0] - el.xy[0][0], ys = el.xy[2][1] - el.xy[0][1];
    const double t = 1 - r - s;
    const std::array<double, 6> n = tri6Shape(r, s);
    const std::array<double, 6> dr{-(4 * t - 1), 4 * r - 1, 0, 4 * (t - r), 4 * s, -4 * s};
    const std::array<double, 6> ds{-(4 * t - 1), 0, 4 * s - 1, -4 * r, 4 * r, 4 * (t - s)};
    er.setZero();
    es.setZero();
    for (int k = 0; k < 6; ++k) {
        er(6 * k + W) = dr[k];
        er(6 * k + TY) = n[k] * xr;
        er(6 * k + TX) = -n[k] * yr;
        es(6 * k + W) = ds[k];
        es(6 * k + TY) = n[k] * xs;
        es(6 * k + TX) = -n[k] * ys;
    }
}

// Assumed field (MITC6a type): e_r = a1 + b1 r + c1 s + s (d r + e s), e_s = a2 + b2 r + c2 s - r (d r + e s); unknowns (a1 b1 c1 a2 b2 c2 d e).
Eigen::Matrix<double, 1, 8> rowR(double r, double s) { return (Eigen::Matrix<double, 1, 8>() << 1, r, s, 0, 0, 0, s * r, s * s).finished(); }
Eigen::Matrix<double, 1, 8> rowS(double r, double s) { return (Eigen::Matrix<double, 1, 8>() << 0, 0, 0, 1, r, s, -r * r, -r * s).finished(); }

/// Cartesian assumed shear strains (gamma_x, gamma_y) at (r, s).
Eigen::Matrix<double, 2, 36> assumedShear(const ShellElement& el, const Eigen::Matrix2d& jinv, double r, double s) {
    const Eigen::Matrix<double, 1, 36> er = rowR(r, s) * el.shearTying, es = rowS(r, s) * el.shearTying;
    Eigen::Matrix<double, 2, 36> g;
    g.row(0) = jinv(0, 0) * er + jinv(0, 1) * es;
    g.row(1) = jinv(1, 0) * er + jinv(1, 1) * es;
    return g;
}

Eigen::Matrix2d jacobianInverse(const ShellElement& el, double& detJ) {
    Eigen::Matrix2d j;
    j << el.xy[1][0] - el.xy[0][0], el.xy[1][1] - el.xy[0][1], el.xy[2][0] - el.xy[0][0], el.xy[2][1] - el.xy[0][1];
    detJ = j.determinant();
    return j.inverse();
}

}  // namespace

std::array<double, 6> tri6Shape(double r, double s) {
    const double t = 1 - r - s;
    return {t * (2 * t - 1), r * (2 * r - 1), s * (2 * s - 1), 4 * t * r, 4 * r * s, 4 * s * t};
}

ShellElement buildShellElement(const FemShell& sh, const std::array<Vec3S, 6>& pos) {
    ShellElement el;
    for (int c = 0; c < 3; ++c) {
        el.r(0, c) = sh.ex[c];
        el.r(1, c) = sh.ey[c];
        el.r(2, c) = sh.ez[c];
    }
    for (int k = 0; k < 6; ++k) {
        const Eigen::Vector3d d(pos[k][0] - pos[0][0], pos[k][1] - pos[0][1], pos[k][2] - pos[0][2]);
        const Eigen::Vector3d l = el.r * d;
        el.xy[k] = {l.x(), l.y()};
    }
    double detJ = 0.0;
    const Eigen::Matrix2d jinv = jacobianInverse(el, detJ);
    el.area = 0.5 * detJ;
    if (!(detJ > 0.0) || !std::isfinite(detJ)) return el;

    // --- section with the stiffness factors (D' = S D S, S = diag(sqrt F) on the direct terms; shear / twist terms scaled directly) ---
    const auto& f = sh.factors;
    const double h = sh.thickness;
    Eigen::Matrix3d q = Eigen::Matrix3d::Zero();  // plane stress constitutive matrix (kN/m2)
    double g13 = 0.0, g23 = 0.0;
    if (sh.orthotropic) {
        const double nu21 = sh.e1 > 0 ? sh.nu12 * sh.e2 / sh.e1 : 0.0, den = 1.0 - sh.nu12 * nu21;
        q << sh.e1 / den, sh.nu12 * sh.e2 / den, 0, sh.nu12 * sh.e2 / den, sh.e2 / den, 0, 0, 0, sh.g12;
        g13 = sh.g13;
        g23 = sh.g23;
    } else {
        const double c = sh.e / (1.0 - sh.nu * sh.nu), g = sh.e / (2.0 * (1.0 + sh.nu));
        q << c, c * sh.nu, 0, c * sh.nu, c, 0, 0, 0, g;
        g13 = g23 = g;
    }
    const auto scaled = [&](double fx, double fxy, double fy, double thick) {
        Eigen::Matrix3d d = q * thick;
        d(0, 0) *= fx;
        d(1, 1) *= fy;
        d(0, 1) *= std::sqrt(fx * fy);
        d(1, 0) = d(0, 1);
        d(2, 2) *= fxy;
        return d;
    };
    // Stiffness: a factor 0 becomes kMinFactor (no mechanism warning by a factor [V, section 15.4]); forces: the nominal factors.
    std::array<double, 8> fk{};
    for (int i = 0; i < 8; ++i) fk[i] = f[i] > kMinFactor ? f[i] : kMinFactor;
    constexpr double kappa = 5.0 / 6.0;
    el.dm = scaled(fk[kFxx], fk[kFxy], fk[kFyy], h);
    el.db = scaled(fk[kMxx], fk[kMxy], fk[kMyy], h * h * h / 12.0);
    el.ds << kappa * g13 * h * fk[kVxz], 0, 0, kappa * g23 * h * fk[kVyz];
    el.dmForces = scaled(f[kFxx], f[kFxy], f[kFyy], h);
    el.dbForces = scaled(f[kMxx], f[kMxy], f[kMyy], h * h * h / 12.0);
    el.dsForces << kappa * g13 * h * f[kVxz], 0, 0, kappa * g23 * h * f[kVyz];
    const double drilling = 1e-3 * q(2, 2) * h * fk[kFxy];  // penalty on (thz - in-plane rotation)

    // --- assumed shear tying (two points per edge + the centroid) ---
    {
        const double a = 0.5 - 1.0 / (2.0 * std::sqrt(3.0)), b = 1.0 - a;
        Eigen::Matrix<double, 8, 8> m;
        Eigen::Matrix<double, 8, 36> rhs;
        Eigen::Matrix<double, 1, 36> er, es;
        int row = 0;
        for (double t : {a, b}) {
            covariantShear(el, t, 0, er, es);
            m.row(row) = rowR(t, 0);
            rhs.row(row++) = er;
        }
        for (double t : {a, b}) {
            covariantShear(el, 0, t, er, es);
            m.row(row) = rowS(0, t);
            rhs.row(row++) = es;
        }
        for (const auto& [r, s] : {std::pair{b, a}, std::pair{a, b}}) {  // hypotenuse: e_s - e_r
            covariantShear(el, r, s, er, es);
            m.row(row) = rowS(r, s) - rowR(r, s);
            rhs.row(row++) = es - er;
        }
        covariantShear(el, 1.0 / 3, 1.0 / 3, er, es);
        m.row(row) = rowR(1.0 / 3, 1.0 / 3);
        rhs.row(row++) = er;
        m.row(row) = rowS(1.0 / 3, 1.0 / 3);
        rhs.row(row++) = es;
        el.shearTying = m.fullPivLu().solve(rhs);
    }

    // --- stiffness ---
    Mat36 k = Mat36::Zero();
    for (const GaussPoint& gp : kG3) {
        const Derivatives d = shapeAt(el, jinv, gp.r, gp.s);
        const auto bm = membraneB(d);
        const auto bb = bendingB(d);
        k += (bm.transpose() * el.dm * bm + bb.transpose() * el.db * bb) * (gp.w * detJ);
    }
    for (const GaussPoint& gp : kG6) {
        const Derivatives d = shapeAt(el, jinv, gp.r, gp.s);
        const auto bs = assumedShear(el, jinv, gp.r, gp.s);
        k += bs.transpose() * el.ds * bs * (gp.w * detJ);
        Eigen::Matrix<double, 1, 36> rot = Eigen::Matrix<double, 1, 36>::Zero();  // thz - (dv/dx - du/dy) / 2
        Eigen::Matrix<double, 1, 36> nw = Eigen::Matrix<double, 1, 36>::Zero();
        for (int n = 0; n < 6; ++n) {
            rot(6 * n + TZ) = d.n[n];
            rot(6 * n + Vd) = -0.5 * d.dx[n];
            rot(6 * n + U) = 0.5 * d.dy[n];
            nw(6 * n + W) = d.n[n];
        }
        k += rot.transpose() * rot * (drilling * gp.w * detJ);
        if (sh.ballast > 0.0) k += nw.transpose() * nw * (sh.ballast * gp.w * detJ);
    }
    el.kLocal = 0.5 * (k + k.transpose());
    Mat36 t = Mat36::Zero();
    for (int b = 0; b < 12; ++b) t.block<3, 3>(3 * b, 3 * b) = el.r;
    el.kGlobal = t.transpose() * el.kLocal * t;
    el.valid = true;
    return el;
}

std::array<double, 2> initialStrain(const FemShell& sh, const FemShellLoad& load) {
    switch (load.kind) {
        case ShellLoadKind::Temperature: return {sh.alpha1 * load.value, sh.alpha2 * load.value};
        case ShellLoadKind::Shrinkage: return {-load.value, -load.value};
        case ShellLoadKind::Pressure: break;
    }
    return {0.0, 0.0};
}

Vec36 shellLoadVector(const ShellElement& el, const FemShell& sh, const FemShellLoad& load, const std::array<std::array<double, 2>, 6>& shellXY) {
    Vec36 fe = Vec36::Zero();
    if (!el.valid) return fe;
    double detJ = 0.0;
    const Eigen::Matrix2d jinv = jacobianInverse(el, detJ);
    if (load.kind == ShellLoadKind::Pressure) {
        Eigen::Vector3d dir(load.direction[0], load.direction[1], load.direction[2]);
        if (!load.global) dir = el.r.transpose() * dir;
        for (const GaussPoint& gp : kG6) {
            const std::array<double, 6> n = tri6Shape(gp.r, gp.s);
            double x = 0.0, y = 0.0;
            for (int k = 0; k < 6; ++k) {
                x += n[k] * shellXY[k][0];
                y += n[k] * shellXY[k][1];
            }
            const double p = load.p0 + load.px * x + load.py * y;
            for (int k = 0; k < 6; ++k) fe.segment<3>(6 * k) += dir * (n[k] * p * gp.w * detJ);
        }
        return fe;
    }
    const std::array<double, 2> e0 = initialStrain(sh, load);
    const Eigen::Vector3d eps0(e0[0], e0[1], 0.0);
    Vec36 local = Vec36::Zero();
    for (const GaussPoint& gp : kG3) {
        const Derivatives d = shapeAt(el, jinv, gp.r, gp.s);
        local += membraneB(d).transpose() * (el.dm * eps0) * (gp.w * detJ);
    }
    for (int k = 0; k < 6; ++k) {
        fe.segment<3>(6 * k) = el.r.transpose() * local.segment<3>(6 * k);
        fe.segment<3>(6 * k + 3) = el.r.transpose() * local.segment<3>(6 * k + 3);
    }
    return fe;
}

namespace {
template <std::size_t N>
std::array<ShellForces, N> forcesAt(const ShellElement& el, const Vec36& uGlobal, const std::array<double, 2>& strain0, const double* rr,
                                    const double* ss) {
    std::array<ShellForces, N> out{};
    if (!el.valid) return out;
    Vec36 u;
    for (int b = 0; b < 12; ++b) u.segment<3>(3 * b) = el.r * uGlobal.segment<3>(3 * b);
    double detJ = 0.0;
    const Eigen::Matrix2d jinv = jacobianInverse(el, detJ);
    const Eigen::Vector3d eps0(strain0[0], strain0[1], 0.0);
    for (std::size_t k = 0; k < N; ++k) {
        const Derivatives d = shapeAt(el, jinv, rr[k], ss[k]);
        const Eigen::Vector3d n = el.dmForces * (membraneB(d) * u - eps0);
        const Eigen::Vector3d m = el.dbForces * (bendingB(d) * u);
        const Eigen::Vector2d q = el.dsForces * (assumedShear(el, jinv, rr[k], ss[k]) * u);
        out[k] = {n(0), n(1), n(2), m(0), m(1), m(2), q(0), q(1)};
    }
    return out;
}
}  // namespace

std::array<ShellForces, 3> shellForcesAtGauss(const ShellElement& el, const Vec36& uGlobal, const std::array<double, 2>& strain0) {
    static constexpr double r[3] = {1.0 / 6, 2.0 / 3, 1.0 / 6}, s[3] = {1.0 / 6, 1.0 / 6, 2.0 / 3};
    return forcesAt<3>(el, uGlobal, strain0, r, s);
}

std::array<ShellForces, 6> shellForcesAtNodes(const ShellElement& el, const Vec36& uGlobal, const std::array<double, 2>& strain0) {
    return forcesAt<6>(el, uGlobal, strain0, kNodeR, kNodeS);
}

}  // namespace cc::fem::detail
