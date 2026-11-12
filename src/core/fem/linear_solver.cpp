#include "core/fem/linear_solver.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <unordered_map>

#include <Eigen/Dense>
#include <Eigen/Sparse>

#include "core/fem/shell_analysis.h"
#include "core/fem/shell_element.h"

namespace cc::fem {

namespace {

using Mat12 = Eigen::Matrix<double, 12, 12>;
using Vec12 = Eigen::Matrix<double, 12, 1>;
using Mat3 = Eigen::Matrix3d;
using V3 = Eigen::Vector3d;
using V6 = Eigen::Matrix<double, 6, 1>;

V3 toEigen(const Vec3& v) { return {v[0], v[1], v[2]}; }

/// e_x x v (local axes).
V3 crossX(const V3& v) { return {0.0, -v.z(), v.y()}; }

std::string format(const char* pattern, const std::string& name) {
    const int n = std::snprintf(nullptr, 0, pattern, name.c_str());
    std::string s(static_cast<std::size_t>(n > 0 ? n : 0), '\0');
    std::snprintf(s.data(), s.size() + 1, pattern, name.c_str());
    return s;
}

std::string formatInt(const char* pattern, int value) {
    char buf[256];
    std::snprintf(buf, sizeof buf, pattern, value);
    return buf;
}

/// A bar load in local axes, positions in metres from nodeI clamped to [0, L].
struct LocalLoad {
    BarLoadKind kind = BarLoadKind::Distributed;
    double x1 = 0.0, x2 = 0.0;
    V3 w1 = V3::Zero(), w2 = V3::Zero();
    double uniformTemperature = 0.0, gradientY = 0.0, gradientZ = 0.0;
};

Mat3 rotation(const FemBar& bar) {
    Mat3 r;
    r.row(0) = toEigen(bar.ex).transpose();
    r.row(1) = toEigen(bar.ey).transpose();
    r.row(2) = toEigen(bar.ez).transpose();
    return r;
}

LocalLoad toLocal(const FemBarLoad& load, const Mat3& r, double length) {
    LocalLoad l;
    l.kind = load.kind;
    l.x1 = std::clamp(load.x1, 0.0, length);
    l.x2 = std::clamp(load.x2, 0.0, length);
    l.w1 = load.global ? V3(r * toEigen(load.w1)) : toEigen(load.w1);
    l.w2 = load.global ? V3(r * toEigen(load.w2)) : toEigen(load.w2);
    l.uniformTemperature = load.uniformTemperature;
    l.gradientY = load.gradientY;
    l.gradientZ = load.gradientZ;
    if (l.kind == BarLoadKind::Distributed && l.x2 < l.x1) {
        std::swap(l.x1, l.x2);
        std::swap(l.w1, l.w2);
    }
    return l;
}

/// Resultant (force, moment about the point x of the axis) of the loads acting beyond x. `inclusive` also takes point loads at exactly x.
void addLoadsBeyond(const std::vector<LocalLoad>& loads, double x, bool inclusive, V3& force, V3& moment) {
    for (const LocalLoad& l : loads) {
        switch (l.kind) {
            case BarLoadKind::PointForce:
                if (l.x1 > x || (inclusive && l.x1 >= x)) {
                    force += l.w1;
                    moment += (l.x1 - x) * crossX(l.w1);
                }
                break;
            case BarLoadKind::PointMoment:
                if (l.x1 > x || (inclusive && l.x1 >= x)) moment += l.w1;
                break;
            case BarLoadKind::Distributed: {
                const double span = l.x2 - l.x1;
                if (span <= 0.0) break;
                const double p = std::max(x, l.x1), q = l.x2;
                if (q <= p) break;
                const auto w = [&](double s) -> V3 { return l.w1 + (l.w2 - l.w1) * ((s - l.x1) / span); };
                const double m = 0.5 * (p + q);
                force += 0.5 * (q - p) * (w(p) + w(q));
                // Simpson is exact for (s - x) w(s), a polynomial of degree 2.
                const V3 first = (q - p) / 6.0 * ((p - x) * w(p) + 4.0 * (m - x) * w(m) + (q - x) * w(q));
                moment += crossX(first);
                break;
            }
            case BarLoadKind::Temperature: break;
        }
    }
}

/// Canonical internal resultant at x due to a unit generalised force at the free end J of a cantilever clamped at I.
V6 unitResultant(int k, double length, double x) {
    V6 s = V6::Zero();
    switch (k) {
        case 0: s(0) = 1.0; break;
        case 1: s(1) = 1.0; s(5) = length - x; break;
        case 2: s(2) = 1.0; s(4) = -(length - x); break;
        default: s(k) = 1.0; break;
    }
    return s;
}

constexpr double kGaussX[4] = {-0.8611363115940526, -0.3399810435848563, 0.3399810435848563, 0.8611363115940526};
constexpr double kGaussW[4] = {0.3478548451374538, 0.6521451548625461, 0.6521451548625461, 0.3478548451374538};

struct ElementData {
    double length = 0.0;
    bool shearY = false, shearZ = false;  // Timoshenko in plane xy (Avy) / xz (Avz)
    Mat12 ke = Mat12::Zero();             // local, element side (unreleased)
    Mat12 t = Mat12::Zero();              // global -> local
    std::array<int, 12> index{};          // element DOF -> augmented DOF (>= 12: internal)
    int internalCount = 0;
    Eigen::MatrixXd kai, kiiPinv;
    Mat12 kc = Mat12::Zero();             // condensed, local
    Mat12 kg = Mat12::Zero();             // condensed, global
    V6 flexibility = V6::Zero();          // 1/EA, 1/GAvy, 1/GAvz, 1/GJ, 1/EIy, 1/EIz (shear terms 0 when not considered)
    double alpha = 0.0, depthY = 0.0, depthZ = 0.0;

    Vec12 condenseLoad(const Vec12& fe) const {
        if (internalCount == 0) return fe;
        Vec12 fa = Vec12::Zero();
        Eigen::VectorXd fi = Eigen::VectorXd::Zero(internalCount);
        for (int p = 0; p < 12; ++p) {
            if (index[p] < 12) fa(index[p]) += fe(p);
            else fi(index[p] - 12) += fe(p);
        }
        return fa - kai * (kiiPinv * fi);
    }

    Vec12 elementDisplacements(const Vec12& ua, const Vec12& fe) const {
        if (internalCount == 0) return ua;
        Eigen::VectorXd fi = Eigen::VectorXd::Zero(internalCount);
        for (int p = 0; p < 12; ++p)
            if (index[p] >= 12) fi(index[p] - 12) += fe(p);
        const Eigen::VectorXd ui = kiiPinv * (fi - kai.transpose() * ua);
        Vec12 ue;
        for (int p = 0; p < 12; ++p) ue(p) = index[p] < 12 ? ua(p) : ui(index[p] - 12);
        return ue;
    }
};

Mat12 localStiffness(const FemBar& b, double length, bool shearY, bool shearZ) {
    const double l = length, e = b.e, g = b.g;
    const SectionData& s = b.section;
    const double phiY = shearY ? 12.0 * e * s.iz / (g * s.avy * l * l) : 0.0;  // plane xy
    const double phiZ = shearZ ? 12.0 * e * s.iy / (g * s.avz * l * l) : 0.0;  // plane xz
    Mat12 k = Mat12::Zero();
    const double ea = e * s.area / l, gj = g * s.torsion / l;
    k(0, 0) = ea; k(0, 6) = -ea; k(6, 6) = ea;
    k(3, 3) = gj; k(3, 9) = -gj; k(9, 9) = gj;
    const double a = e * s.iz / ((1.0 + phiY) * l * l * l);
    k(1, 1) = 12 * a; k(1, 5) = 6 * a * l; k(1, 7) = -12 * a; k(1, 11) = 6 * a * l;
    k(5, 5) = (4 + phiY) * a * l * l; k(5, 7) = -6 * a * l; k(5, 11) = (2 - phiY) * a * l * l;
    k(7, 7) = 12 * a; k(7, 11) = -6 * a * l; k(11, 11) = (4 + phiY) * a * l * l;
    const double c = e * s.iy / ((1.0 + phiZ) * l * l * l);
    k(2, 2) = 12 * c; k(2, 4) = -6 * c * l; k(2, 8) = -12 * c; k(2, 10) = -6 * c * l;
    k(4, 4) = (4 + phiZ) * c * l * l; k(4, 8) = 6 * c * l; k(4, 10) = (2 - phiZ) * c * l * l;
    k(8, 8) = 12 * c; k(8, 10) = 6 * c * l; k(10, 10) = (4 + phiZ) * c * l * l;
    for (int i = 0; i < 12; ++i)
        for (int j = 0; j < i; ++j) k(i, j) = k(j, i);
    return k;
}

ElementData buildElement(const FemBar& b, double length, const SolverOptions& options) {
    ElementData d;
    d.length = length;
    d.shearY = options.shearDeformation && b.section.avy > 0.0;
    d.shearZ = options.shearDeformation && b.section.avz > 0.0;
    d.ke = localStiffness(b, length, d.shearY, d.shearZ);
    const Mat3 r = rotation(b);
    for (int i = 0; i < 4; ++i) d.t.block<3, 3>(3 * i, 3 * i) = r;
    const SectionData& s = b.section;
    d.flexibility << 1.0 / (b.e * s.area), d.shearY ? 1.0 / (b.g * s.avy) : 0.0, d.shearZ ? 1.0 / (b.g * s.avz) : 0.0,
        1.0 / (b.g * s.torsion), 1.0 / (b.e * s.iy), 1.0 / (b.e * s.iz);
    d.alpha = b.alpha;
    d.depthY = s.depthY;
    d.depthZ = s.depthZ;

    // Released / partially fixed rotations: one internal DOF each, joined to the node DOF by a spring (0 = free).
    std::vector<std::pair<int, double>> internal;  // element DOF, spring stiffness
    for (int end = 0; end < 2; ++end)
        for (int k = 0; k < 3; ++k) {
            const EndRelease& rel = b.releases[end][k];
            if (rel.kind == EndRelease::Kind::Rigid) continue;
            internal.emplace_back(6 * end + 3 + k, rel.kind == EndRelease::Kind::Spring ? rel.stiffness : 0.0);
        }
    for (int p = 0; p < 12; ++p) d.index[p] = p;
    d.internalCount = static_cast<int>(internal.size());
    if (d.internalCount == 0) {
        d.kc = d.ke;
    } else {
        const int n = 12 + d.internalCount;
        for (int j = 0; j < d.internalCount; ++j) d.index[internal[j].first] = 12 + j;
        Eigen::MatrixXd kaug = Eigen::MatrixXd::Zero(n, n);
        for (int p = 0; p < 12; ++p)
            for (int q = 0; q < 12; ++q) kaug(d.index[p], d.index[q]) += d.ke(p, q);
        for (int j = 0; j < d.internalCount; ++j) {
            const int a = internal[j].first, i = 12 + j;
            const double ks = internal[j].second;
            kaug(a, a) += ks;
            kaug(a, i) -= ks;
            kaug(i, a) -= ks;
            kaug(i, i) += ks;
        }
        d.kai = kaug.topRightCorner(12, d.internalCount);
        const Eigen::MatrixXd kii = kaug.bottomRightCorner(d.internalCount, d.internalCount);
        Eigen::CompleteOrthogonalDecomposition<Eigen::MatrixXd> cod(kii);
        cod.setThreshold(1e-10);  // both ends of the torsion released: singular block, the twist of the bar has no stiffness
        d.kiiPinv = cod.pseudoInverse();
        d.kc = kaug.topLeftCorner(12, 12) - d.kai * d.kiiPinv * d.kai.transpose();
        d.kc = 0.5 * (d.kc + d.kc.transpose()).eval();
    }
    d.kg = d.t.transpose() * d.kc * d.t;
    return d;
}

/// Exact fixed-end reactions (forces of the clamps on the element at I and J, local axes) by the flexibility method: tip displacement of
/// the cantilever clamped at I (unit load method, Gauss over the load breakpoints, shear flexibility and initial strains included), J reactions
/// that cancel it through the clamped-clamped J stiffness block, I reactions by statics.
Vec12 fixedEndReactions(const ElementData& d, const std::vector<LocalLoad>& loads) {
    const double length = d.length;
    std::vector<double> breaks{0.0, length};
    V6 strain0 = V6::Zero();
    for (const LocalLoad& l : loads) {
        if (l.kind == BarLoadKind::Temperature) {
            strain0(0) += d.alpha * l.uniformTemperature;
            if (d.depthZ > 0.0) strain0(4) += d.alpha * l.gradientZ / d.depthZ;   // hotter +z face: convex towards +z
            if (d.depthY > 0.0) strain0(5) -= d.alpha * l.gradientY / d.depthY;   // hotter +y face: convex towards +y
            continue;
        }
        breaks.push_back(l.x1);
        if (l.kind == BarLoadKind::Distributed) breaks.push_back(l.x2);
    }
    std::sort(breaks.begin(), breaks.end());
    breaks.erase(std::unique(breaks.begin(), breaks.end(), [](double a, double b) { return std::fabs(a - b) < 1e-12; }), breaks.end());

    V6 delta = V6::Zero();
    for (std::size_t s = 0; s + 1 < breaks.size(); ++s) {
        const double a = breaks[s], b = breaks[s + 1];
        if (b - a <= 0.0) continue;
        for (int gp = 0; gp < 4; ++gp) {
            const double x = 0.5 * (a + b) + 0.5 * (b - a) * kGaussX[gp];
            const double w = 0.5 * (b - a) * kGaussW[gp];
            V3 f = V3::Zero(), m = V3::Zero();
            addLoadsBeyond(loads, x, false, f, m);
            V6 sReal;
            sReal << f, m;
            const V6 strain = d.flexibility.cwiseProduct(sReal) + strain0;
            for (int k = 0; k < 6; ++k) delta(k) += w * strain.dot(unitResultant(k, length, x));
        }
    }
    const Eigen::Matrix<double, 6, 6> kjj = d.ke.block<6, 6>(6, 6);
    const V6 rj = -(kjj * delta);
    V3 f0 = V3::Zero(), m0 = V3::Zero();
    addLoadsBeyond(loads, 0.0, true, f0, m0);
    const V3 rjF = rj.head<3>(), rjM = rj.tail<3>();
    Vec12 r;
    r.segment<3>(0) = -(f0 + rjF);
    r.segment<3>(3) = -(m0 + length * crossX(rjF) + rjM);
    r.segment<6>(6) = rj;
    return r;
}

std::string checkBar(const FemBar& b, double length) {
    const SectionData& s = b.section;
    const auto bad = [](double v) { return !std::isfinite(v) || v <= 0.0; };
    if (length < 1e-9) return formatInt("La barra %d tiene longitud nula.", b.id);
    if (bad(b.e) || bad(b.g) || !std::isfinite(b.alpha)) return formatInt("La barra %d no tiene un material válido.", b.id);
    if (bad(s.area) || bad(s.iy) || bad(s.iz) || bad(s.torsion) || !std::isfinite(s.avy) || !std::isfinite(s.avz) || s.avy < 0 || s.avz < 0)
        return formatInt("La barra %d no tiene una sección válida.", b.id);
    const V3 ex = toEigen(b.ex), ey = toEigen(b.ey), ez = toEigen(b.ez);
    const double tol = 1e-5;  // the original checks |v.v - 1| < 1e-5 for the three local vectors
    if (std::fabs(ex.squaredNorm() - 1) > tol || std::fabs(ey.squaredNorm() - 1) > tol || std::fabs(ez.squaredNorm() - 1) > tol ||
        std::fabs(ex.dot(ey)) > tol || std::fabs(ex.dot(ez)) > tol || std::fabs(ey.dot(ez)) > tol || (ex.cross(ey) - ez).norm() > 1e-4)
        return formatInt("Los ejes locales de la barra %d no son válidos.", b.id);
    for (const auto& end : b.releases)
        for (const EndRelease& r : end)
            if (r.kind == EndRelease::Kind::Spring && bad(r.stiffness))
                return formatInt("La rigidez al giro de un extremo de la barra %d no es válida.", b.id);
    return {};
}

}  // namespace

std::string mechanismMessage(const std::string& caseName) {
    return format("No es posible calcular el caso de carga '%s'. La estructura es un mecanismo. Revise la definición de cargas y las "
                  "condiciones de vinculación del modelo.",
                  caseName);
}

std::string unstableMessage(const std::string& caseName) {
    return format("No es posible calcular el caso de carga '%s', ya que la estructura es inestable. Revise la definición de cargas y las "
                  "condiciones de contorno introducidas.",
                  caseName);
}

std::string excessiveDisplacementsMessage() {
    return "Se han obtenido desplazamientos demasiado grandes. Puede consultar los mensajes de aviso correspondientes mediante la opción "
           "'Mostrar/Oculta incidencias' del menú 'Cálculo'.";
}

std::string caseErrorMessage(const std::string& caseName) {
    return format("Se ha producido un error calculando el caso de carga '%s'. Revise las condiciones del modelo.", caseName);
}

// Measured in the original (docs/research/solver_medidas_original.md section 3.2): only My is the opposite of the canonical resultant.
Dof6 applySignConvention(const Dof6& canonical) {
    Dof6 r = canonical;
    r[4] = -r[4];
    return r;
}
Dof6 removeSignConvention(const Dof6& convention) { return applySignConvention(convention); }  // an involution

std::string mechanismWarning(int dof, const Vec3& position) {
    static const char* const kTexts[6] = {"Desplazamiento según el eje X", "Desplazamiento según el eje Y", "Desplazamiento según el eje Z",
                                          "Giro alrededor del eje X", "Giro alrededor del eje Y", "Giro alrededor del eje Z"};
    char coords[160];
    std::snprintf(coords, sizeof coords, " (x = %.6f, y = %.6f, z = %.6f)", position[0], position[1], position[2]);
    return std::string("Esta estructura es un mecanismo. Revise los datos introducidos.\n") + kTexts[std::clamp(dof, 0, 5)] +
           "\nEl error se encuentra en el punto de coordenadas:\n" + coords +
           "\nAlguna de las barras de la estructura no está vinculada correctamente en sus extremos, es decir, no es estable.";
}

Dof6 sectionForcesAt(const FemBar& bar, double length, const BarEndForces& forces, std::span<const FemBarLoad> barLoads, double x) {
    const Mat3 r = rotation(bar);
    std::vector<LocalLoad> loads;
    loads.reserve(barLoads.size());
    for (const FemBarLoad& l : barLoads) loads.push_back(toLocal(l, r, length));
    x = std::clamp(x, 0.0, length);
    const Dof6 cj = removeSignConvention(forces.end);
    const V3 fj(cj[0], cj[1], cj[2]), mj(cj[3], cj[4], cj[5]);
    V3 f = fj, m = mj + (length - x) * crossX(fj);
    addLoadsBeyond(loads, x, false, f, m);
    return applySignConvention({f.x(), f.y(), f.z(), m.x(), m.y(), m.z()});
}

namespace {

/// Free DOF (global numbering) with the largest amplitude in the null mode of the matrix: inverse iteration on the matrix regularised with
/// 1e-6 K_ii. The original names the DOF of its null pivot, which depends on its elimination order [N]; the largest amplitude reproduces the
/// measured L-shaped case (N3, Dz) and is deterministic. -1 if the factorisation fails.
int locateMechanismDof(const Eigen::SparseMatrix<double>& kff, const std::vector<int>& freeDofs, const Eigen::VectorXd& diag) {
    const int n = static_cast<int>(freeDofs.size());
    if (n == 0) return -1;
    Eigen::SparseMatrix<double> reg = kff;
    for (int i = 0; i < n; ++i) reg.coeffRef(i, i) += 1e-6 * std::max(std::fabs(diag(freeDofs[i])), 1e-300);
    Eigen::SimplicialLDLT<Eigen::SparseMatrix<double>> f(reg);
    if (f.info() != Eigen::Success) return -1;
    Eigen::VectorXd x(n);
    for (int i = 0; i < n; ++i) x(i) = 1.0 + 0.1 * std::sin(1.0 + i);
    for (int it = 0; it < 3; ++it) {
        x = f.solve(x);
        if (f.info() != Eigen::Success || !x.allFinite()) return -1;
        const double m = x.cwiseAbs().maxCoeff();
        if (m <= 0.0) return -1;
        x /= m;
    }
    int best = 0;
    double bestAmp = -1.0;
    for (int i = 0; i < n; ++i)
        if (std::fabs(x(i)) > bestAmp * (1.0 + 1e-9)) { bestAmp = std::fabs(x(i)); best = i; }
    return freeDofs[best];
}

/// Part of a bar between two mesh nodes (a bar along a shell edge is split at the mesh nodes of the edge; other bars are one segment).
struct Segment {
    int bar = 0;          ///< index into FrameModel::bars
    double x0 = 0.0;      ///< distance of the segment origin from nodeI of the bar
    ElementData el;
    std::array<int, 2> nodes{};  ///< mesh node indices
};

/// The loads of a bar restricted to the segment [a, b] and measured from a.
std::vector<LocalLoad> segmentLoads(const std::vector<LocalLoad>& loads, double a, double b, bool last) {
    std::vector<LocalLoad> out;
    for (const LocalLoad& l : loads) {
        switch (l.kind) {
            case BarLoadKind::Temperature: out.push_back(l); break;
            case BarLoadKind::PointForce:
            case BarLoadKind::PointMoment:
                if (l.x1 >= a - 1e-12 && (l.x1 < b - 1e-12 || (last && l.x1 <= b + 1e-12))) {
                    LocalLoad s = l;
                    s.x1 = std::clamp(l.x1 - a, 0.0, b - a);
                    out.push_back(s);
                }
                break;
            case BarLoadKind::Distributed: {
                const double p = std::max(l.x1, a), q = std::min(l.x2, b);
                if (q - p <= 1e-12 || l.x2 - l.x1 <= 0.0) break;
                const auto w = [&](double x) -> V3 { return l.w1 + (l.w2 - l.w1) * ((x - l.x1) / (l.x2 - l.x1)); };
                LocalLoad s = l;
                s.x1 = p - a;
                s.x2 = q - a;
                s.w1 = w(p);
                s.w2 = w(q);
                out.push_back(s);
                break;
            }
        }
    }
    return out;
}

/// A null mode of the stiffness found by a null pivot: the DOF fixed to remove it and the mode itself (full DOF vector).
struct NullMode {
    int dof = -1;
    Eigen::VectorXd v;
};

/// Loads of a piece [x0, x1] of a bar, in local axes and measured from x0.
std::vector<LocalLoad> pieceLoads(const FemBar& bar, double barLength, std::span<const FemBarLoad> barLoads, double x0, double x1, bool last) {
    const Mat3 r = rotation(bar);
    std::vector<LocalLoad> loads;
    loads.reserve(barLoads.size());
    for (const FemBarLoad& l : barLoads) loads.push_back(toLocal(l, r, barLength));
    return segmentLoads(loads, x0, x1, last);
}

/// Canonical resultant at s (metres from the origin of the piece) from the canonical forces at its end (length len) and its loads.
V6 pieceCanonicalAt(const std::vector<LocalLoad>& loads, double len, const Dof6& endConvention, double s) {
    const Dof6 cj = removeSignConvention(endConvention);
    const V3 fj(cj[0], cj[1], cj[2]), mj(cj[3], cj[4], cj[5]);
    V3 f = fj, m = mj + (len - s) * crossX(fj);
    addLoadsBeyond(loads, s, false, f, m);
    V6 out;
    out << f, m;
    return out;
}

}  // namespace

Dof6 pieceForcesAt(const FemBar& bar, double barLength, double x0, double x1, bool last, const Dof6& endForces,
                   std::span<const FemBarLoad> barLoads, double x) {
    const std::vector<LocalLoad> loads = pieceLoads(bar, barLength, barLoads, x0, x1, last);
    const double len = x1 - x0, s = std::clamp(x - x0, 0.0, len);
    const V6 c = pieceCanonicalAt(loads, len, endForces, s);
    return applySignConvention({c(0), c(1), c(2), c(3), c(4), c(5)});
}

Dof6 pieceDisplacementAt(const FemBar& bar, double barLength, double x0, double x1, bool last, const Dof6& originDisplacement,
                         const Dof6& endForces, std::span<const FemBarLoad> barLoads, double x, const SolverOptions& options) {
    const std::vector<LocalLoad> loads = pieceLoads(bar, barLength, barLoads, x0, x1, last);
    const double len = x1 - x0, s = std::clamp(x - x0, 0.0, len);
    const SectionData& sec = bar.section;
    const bool shearY = options.shearDeformation && sec.avy > 0.0, shearZ = options.shearDeformation && sec.avz > 0.0;
    V6 flex;
    flex << 1.0 / (bar.e * sec.area), shearY ? 1.0 / (bar.g * sec.avy) : 0.0, shearZ ? 1.0 / (bar.g * sec.avz) : 0.0, 1.0 / (bar.g * sec.torsion),
        1.0 / (bar.e * sec.iy), 1.0 / (bar.e * sec.iz);
    // Initial strains of the temperature loads, exactly as in fixedEndReactions().
    V6 strain0 = V6::Zero();
    std::vector<double> breaks{0.0, s};
    for (const LocalLoad& l : loads) {
        if (l.kind == BarLoadKind::Temperature) {
            strain0(0) += bar.alpha * l.uniformTemperature;
            if (sec.depthZ > 0.0) strain0(4) += bar.alpha * l.gradientZ / sec.depthZ;
            if (sec.depthY > 0.0) strain0(5) -= bar.alpha * l.gradientY / sec.depthY;
            continue;
        }
        if (l.x1 > 0.0 && l.x1 < s) breaks.push_back(l.x1);
        if (l.kind == BarLoadKind::Distributed && l.x2 > 0.0 && l.x2 < s) breaks.push_back(l.x2);
    }
    std::sort(breaks.begin(), breaks.end());
    // Unit load method on the piece clamped at its origin: displacement at s relative to the rigid motion of the origin.
    V6 delta = V6::Zero();
    for (std::size_t k = 0; k + 1 < breaks.size(); ++k) {
        const double a = breaks[k], b = breaks[k + 1];
        if (b - a <= 1e-14) continue;
        for (int gp = 0; gp < 4; ++gp) {
            const double t = 0.5 * (a + b) + 0.5 * (b - a) * kGaussX[gp];
            const double w = 0.5 * (b - a) * kGaussW[gp];
            const V6 strain = flex.cwiseProduct(pieceCanonicalAt(loads, len, endForces, t)) + strain0;
            for (int q = 0; q < 6; ++q) delta(q) += w * strain.dot(unitResultant(q, s, t));
        }
    }
    const Mat3 r = rotation(bar);
    const V3 u0 = r * V3(originDisplacement[0], originDisplacement[1], originDisplacement[2]);
    const V3 th0 = r * V3(originDisplacement[3], originDisplacement[4], originDisplacement[5]);
    const V3 uLocal = u0 + th0.cross(V3(s, 0.0, 0.0)) + delta.head<3>();
    const V3 thLocal = th0 + delta.tail<3>();
    const V3 ug = r.transpose() * uLocal, thg = r.transpose() * thLocal;
    return {ug.x(), ug.y(), ug.z(), thg.x(), thg.y(), thg.z()};
}

namespace detail {

/// Homogeneous linear relation sum(coef * u_dof) = 0; `preferred` = the DOF to eliminate when possible.
struct DofRelation {
    int preferred = -1;
    std::vector<std::pair<int, double>> terms;
};

struct DofElimination {
    std::vector<char> slave;          ///< eliminated DOFs
    Eigen::SparseMatrix<double> t;    ///< nDof x nDof: u = T u_r (identity on the kept DOFs, slave rows = their expressions)
    std::string error;
};

/// Gaussian elimination of the relations, one at a time: every relation is rewritten in terms of the DOFs kept so far, one of its DOFs (the
/// preferred one when it is not blocked and its coefficient is not small, otherwise the free DOF of largest coefficient) becomes a slave
/// and is substituted in the expressions of the previous slaves. Blocked DOFs (supports) may be masters (their prescribed values enter
/// through u_r) but never slaves; a relation left with blocked DOFs only, or with nothing, is redundant and dropped [N: the original
/// forbids those combinations in the GUI].
DofElimination eliminateRelations(const std::vector<DofRelation>& rows, const std::vector<char>& fixed, int nDof) {
    DofElimination out;
    out.slave.assign(nDof, 0);
    std::vector<std::vector<std::pair<int, double>>> expr(nDof);
    std::vector<int> slaves;
    for (const DofRelation& row : rows) {
        std::map<int, double> acc;
        for (const auto& [d, c] : row.terms) {
            if (d < 0 || d >= nDof || !std::isfinite(c)) { out.error = "Relación entre grados de libertad no válida."; return out; }
            if (out.slave[d])
                for (const auto& [m, cm] : expr[d]) acc[m] += c * cm;
            else acc[d] += c;
        }
        double big = 0.0;
        for (const auto& [d, c] : acc) big = std::max(big, std::fabs(c));
        int s = -1;
        double bigFree = 0.0;
        for (const auto& [d, c] : acc)
            if (!fixed[d] && std::fabs(c) > 1e-12 * big && std::fabs(c) > bigFree) { bigFree = std::fabs(c); s = d; }
        if (s < 0) continue;  // redundant or between blocked DOFs only
        if (const auto it = acc.find(row.preferred); it != acc.end() && !fixed[row.preferred] && std::fabs(it->second) >= 0.1 * bigFree)
            s = row.preferred;
        const double cs = acc[s];
        std::vector<std::pair<int, double>> e;
        for (const auto& [d, c] : acc)
            if (d != s && std::fabs(c) > 1e-12 * big) e.emplace_back(d, -c / cs);
        for (int other : slaves) {
            auto& ex = expr[other];
            const auto hit = std::find_if(ex.begin(), ex.end(), [&](const auto& p) { return p.first == s; });
            if (hit == ex.end()) continue;
            const double k = hit->second;
            ex.erase(hit);
            for (const auto& [m, cm] : e) {
                const auto same = std::find_if(ex.begin(), ex.end(), [&](const auto& p) { return p.first == m; });
                if (same != ex.end()) same->second += k * cm;
                else ex.emplace_back(m, k * cm);
            }
        }
        expr[s] = std::move(e);
        out.slave[s] = 1;
        slaves.push_back(s);
    }
    std::vector<Eigen::Triplet<double>> tr;
    tr.reserve(nDof + 3 * slaves.size());
    for (int d = 0; d < nDof; ++d) {
        if (!out.slave[d]) tr.emplace_back(d, d, 1.0);
        else
            for (const auto& [m, c] : expr[d])
                if (c != 0.0) tr.emplace_back(d, m, c);
    }
    out.t = Eigen::SparseMatrix<double>(nDof, nDof);
    out.t.setFromTriplets(tr.begin(), tr.end());
    return out;
}

}  // namespace detail

std::string mechanismWarningZone(int dof, double x1, double y1, double x2, double y2, double z) {
    std::string s = mechanismWarning(dof, {0, 0, 0});
    const std::size_t a = s.find("\nEl error se encuentra");
    const std::size_t b = s.find("\nAlguna de las barras");
    char zone[200];
    std::snprintf(zone, sizeof zone, "\nEl error se encuentra en la zona de coordenadas:\n (%.6f,%.6f) a (%.6f,%.6f), en la cota %.6f", x1, y1, x2, y2, z);
    return s.substr(0, a) + zone + s.substr(b);
}

std::string detachedNodeMessage() { return "Se ha encontrado un nudo que no está conectado a ninguna barra ni a ninguna lámina."; }

namespace {

/// A spring to the ground of the analysis: the springs of the FemNodes first (model order, DOF order), then those of the elastic edge bonds
/// (ShellMesh::edgeSprings order). The order is deterministic, so solveNonlinear switches them off by index.
struct GroundSpring {
    int node = 0;  ///< mesh node index
    int dof = 0;
    double k = 0.0;
    SpringSense sense = SpringSense::Both;
};

std::vector<GroundSpring> groundSprings(const FrameModel& model, const ShellMesh& mesh) {
    std::vector<GroundSpring> out;
    for (std::size_t i = 0; i < model.nodes.size(); ++i)
        for (int k = 0; k < 6; ++k)
            if (model.nodes[i].spring[k] != 0.0) out.push_back({static_cast<int>(i), k, model.nodes[i].spring[k], model.nodes[i].springSense[k]});
    for (const ShellMesh::GroundSpring& s : mesh.edgeSprings) out.push_back({s.node, s.dof, s.k, s.sense});
    return out;
}

/// solveLinear with some springs switched off (`inactive[i]` != 0 for groundSprings()[i]); nullptr = every spring acts.
LinearResult solveLinearImpl(const FrameModel& model, std::span<const LoadCaseInput> cases, const SolverOptions& options,
                             const std::vector<char>* inactive);

}  // namespace

LinearResult solveLinear(const FrameModel& model, std::span<const LoadCaseInput> cases, const SolverOptions& options) {
    return solveLinearImpl(model, cases, options, nullptr);
}

namespace {

LinearResult solveLinearImpl(const FrameModel& model, std::span<const LoadCaseInput> cases, const SolverOptions& options,
                             const std::vector<char>* inactive) {
    LinearResult result;
    result.cases.resize(cases.size());
    for (std::size_t c = 0; c < cases.size(); ++c) {
        result.cases[c].id = cases[c].id;
        result.cases[c].name = cases[c].name;
    }
    const auto failAll = [&](SolveStatus status, const std::string& modelMessage) {
        result.status = status;
        result.message = modelMessage;
        for (LoadCaseResult& cr : result.cases) {
            cr.status = status;
            cr.message = status == SolveStatus::Mechanism ? (modelMessage.empty() ? mechanismMessage(cr.name) : modelMessage)
                         : status == SolveStatus::Unstable ? unstableMessage(cr.name)
                                                            : caseErrorMessage(cr.name);
        }
        return result;
    };

    // --- 1. nodes, shell mesh and DOFs ---
    const int nNodes = static_cast<int>(model.nodes.size());
    std::unordered_map<int, int> nodeIndex;
    for (int i = 0; i < nNodes; ++i) {
        if (!nodeIndex.emplace(model.nodes[i].id, i).second) return failAll(SolveStatus::Error, formatInt("Nudo %d repetido.", model.nodes[i].id));
    }
    std::unordered_map<int, int> shellIndex;
    for (std::size_t s = 0; s < model.shells.size(); ++s) {
        const FemShell& sh = model.shells[s];
        if (!shellIndex.emplace(sh.id, static_cast<int>(s)).second) return failAll(SolveStatus::Error, formatInt("Lámina %d repetida.", sh.id));
        const auto bad = [](double v) { return !std::isfinite(v) || v <= 0.0; };
        const bool material = sh.orthotropic ? !(bad(sh.e1) || bad(sh.e2) || bad(sh.g12) || bad(sh.g13) || bad(sh.g23))
                                             : !(bad(sh.e) || !std::isfinite(sh.nu) || sh.nu <= -1.0 || sh.nu >= 0.5);
        bool factors = true;
        for (double f : sh.factors) factors = factors && std::isfinite(f) && f >= 0.0;
        if (bad(sh.thickness) || !material || !factors || !std::isfinite(sh.ballast) || sh.ballast < 0.0)
            return failAll(SolveStatus::Error, formatInt("La lámina %d no tiene una sección válida.", sh.id));
    }
    ShellMesh mesh = meshShells(model);
    const int nAll = static_cast<int>(mesh.positions.size());
    const int nDof = 6 * nAll;
    if (nDof == 0) return failAll(SolveStatus::Error, "No hay nudos que calcular.");
    const bool hasShells = !model.shells.empty();
    if (!mesh.orphanModelNodes.empty()) return failAll(SolveStatus::Error, detachedNodeMessage());
    const std::vector<GroundSpring> springs = groundSprings(model, mesh);

    // --- 2. bar elements (segments) ---
    std::unordered_map<int, int> barIndex;
    std::vector<Segment> segments;
    std::vector<std::vector<int>> barSegments(model.bars.size());
    std::vector<double> barLength(model.bars.size(), 0.0);
    for (std::size_t b = 0; b < model.bars.size(); ++b) {
        const FemBar& bar = model.bars[b];
        const auto ni = nodeIndex.find(bar.nodeI), nj = nodeIndex.find(bar.nodeJ);
        if (ni == nodeIndex.end() || nj == nodeIndex.end() || bar.nodeI == bar.nodeJ)
            return failAll(SolveStatus::Error, formatInt("La barra %d no tiene nudos válidos.", bar.id));
        if (!barIndex.emplace(bar.id, static_cast<int>(b)).second) return failAll(SolveStatus::Error, formatInt("Barra %d repetida.", bar.id));
        const Vec3 &pi = model.nodes[ni->second].position, &pj = model.nodes[nj->second].position;
        const double length = std::sqrt((pj[0] - pi[0]) * (pj[0] - pi[0]) + (pj[1] - pi[1]) * (pj[1] - pi[1]) + (pj[2] - pi[2]) * (pj[2] - pi[2]));
        if (const std::string err = checkBar(bar, length); !err.empty()) return failAll(SolveStatus::Error, err);
        barLength[b] = length;
        std::vector<std::pair<int, double>> chain{{ni->second, 0.0}, {nj->second, length}};
        if (const auto it = mesh.barSplits.find(static_cast<int>(b)); it != mesh.barSplits.end()) chain = it->second;
        for (std::size_t k = 0; k + 1 < chain.size(); ++k) {
            FemBar part = bar;
            for (int e = 0; e < 2; ++e) {
                const bool outer = (e == 0 && k == 0) || (e == 1 && k + 2 == chain.size());
                if (!outer) part.releases[e] = {};
            }
            Segment seg;
            seg.bar = static_cast<int>(b);
            seg.x0 = chain[k].second;
            seg.el = buildElement(part, chain[k + 1].second - chain[k].second, options);
            seg.nodes = {chain[k].first, chain[k + 1].first};
            barSegments[b].push_back(static_cast<int>(segments.size()));
            segments.push_back(std::move(seg));
        }
    }

    // --- 2b. shell elements ---
    std::vector<detail::ShellElement> shellElements;
    shellElements.reserve(mesh.triangles.size());
    for (const ShellMesh::Triangle& t : mesh.triangles) {
        std::array<Vec3S, 6> pos;
        for (int k = 0; k < 6; ++k) pos[k] = mesh.positions[t.nodes[k]];
        shellElements.push_back(detail::buildShellElement(model.shells[t.shell], pos));
        if (!shellElements.back().valid) return failAll(SolveStatus::Error, formatInt("La lámina %d tiene elementos degenerados.", model.shells[t.shell].id));
    }
    // Local 2D coordinates of the nodes of every triangle in the system of its shell (pressure fields, bands).
    const auto shellXY = [&](const ShellMesh::Triangle& t) {
        const FemShell& sh = model.shells[t.shell];
        const Vec3S& o = mesh.positions[nodeIndex.at(sh.nodes.front())];
        std::array<std::array<double, 2>, 6> xy{};
        for (int k = 0; k < 6; ++k) {
            const Vec3S& p = mesh.positions[t.nodes[k]];
            const double d[3] = {p[0] - o[0], p[1] - o[1], p[2] - o[2]};
            xy[k] = {d[0] * sh.ex[0] + d[1] * sh.ex[1] + d[2] * sh.ex[2], d[0] * sh.ey[0] + d[1] * sh.ey[1] + d[2] * sh.ey[2]};
        }
        return xy;
    };

    // --- 3. global stiffness (all DOFs) ---
    std::vector<Eigen::Triplet<double>> triplets;
    triplets.reserve(segments.size() * 144 + shellElements.size() * 1296 + nDof);
    for (const Segment& seg : segments) {
        int dof[12];
        for (int k = 0; k < 6; ++k) {
            dof[k] = 6 * seg.nodes[0] + k;
            dof[6 + k] = 6 * seg.nodes[1] + k;
        }
        const Mat12& kg = seg.el.kg;
        for (int p = 0; p < 12; ++p)
            for (int q = 0; q < 12; ++q)
                if (kg(p, q) != 0.0) triplets.emplace_back(dof[p], dof[q], kg(p, q));
    }
    for (std::size_t e = 0; e < shellElements.size(); ++e) {
        const auto& kg = shellElements[e].kGlobal;
        const auto& nodes = mesh.triangles[e].nodes;
        for (int p = 0; p < 36; ++p)
            for (int q = 0; q < 36; ++q)
                if (kg(p, q) != 0.0) triplets.emplace_back(6 * nodes[p / 6] + p % 6, 6 * nodes[q / 6] + q % 6, kg(p, q));
    }
    std::vector<double> springK(nDof, 0.0);  // active springs to the ground per DOF (reactions)
    for (std::size_t s = 0; s < springs.size(); ++s) {
        const GroundSpring& g = springs[s];
        if (!std::isfinite(g.k) || g.k < 0.0)
            return failAll(SolveStatus::Error, g.node < nNodes ? formatInt("El muelle del nudo %d no es válido.", model.nodes[g.node].id)
                                                               : std::string("El muelle de una arista de lámina no es válido."));
        if (inactive && s < inactive->size() && (*inactive)[s]) continue;
        triplets.emplace_back(6 * g.node + g.dof, 6 * g.node + g.dof, g.k);
        springK[6 * g.node + g.dof] += g.k;
    }
    // Two-node springs of the interior edge bonds: k a a' on the block (translations or rotations) of both nodes, with opposite signs.
    for (const ShellMesh::Link& l : mesh.links) {
        if (!std::isfinite(l.k) || l.k < 0.0) return failAll(SolveStatus::Error, "El muelle de una vinculación interior de lámina no es válido.");
        const int off = l.rotation ? 3 : 0;
        for (int p = 0; p < 3; ++p)
            for (int q = 0; q < 3; ++q) {
                const double v = l.k * l.axis[p] * l.axis[q];
                if (v == 0.0) continue;
                triplets.emplace_back(6 * l.a + off + p, 6 * l.a + off + q, v);
                triplets.emplace_back(6 * l.b + off + p, 6 * l.b + off + q, v);
                triplets.emplace_back(6 * l.a + off + p, 6 * l.b + off + q, -v);
                triplets.emplace_back(6 * l.b + off + p, 6 * l.a + off + q, -v);
            }
    }
    Eigen::SparseMatrix<double> kOrig(nDof, nDof);
    kOrig.setFromTriplets(triplets.begin(), triplets.end());

    std::vector<char> fixed(nDof, 0), orphan(nDof, 0);
    for (int i = 0; i < nAll; ++i)
        for (int k = 0; k < 6; ++k) fixed[6 * i + k] = ((i < nNodes && model.nodes[i].fixed[k]) || mesh.edgeFixed[i][k]) ? 1 : 0;

    // --- 3b. relations between DOFs (ligatures, diaphragms, oblique supports), eliminated by transformation: u = T u_r ---
    // TODO(medir): the error texts of this block are the clone's, not the original's (e.g. the original reports diaphragm nodes at different
    // levels with #375642, text unknown); the GUI forbids most of these situations before the calculation.
    std::vector<char> slave(nDof, 0);
    Eigen::SparseMatrix<double> tMat;
    bool anyOblique = false;
    std::unordered_map<int, int> diaphragmMaster;  // FemDiaphragm::id -> node index of its master
    {
        std::vector<detail::DofRelation> rows;
        const auto badTie = [&](int a, int b) {
            char buf[160];
            std::snprintf(buf, sizeof buf, "La ligadura entre los nudos %d y %d no es válida.", a, b);
            return failAll(SolveStatus::Error, buf);
        };
        for (const FemEqualDof& e : model.equalDofs) {
            const auto im = nodeIndex.find(e.master), is = nodeIndex.find(e.slave);
            if (im == nodeIndex.end() || is == nodeIndex.end() || e.master == e.slave) return badTie(e.master, e.slave);
            for (int k = 0; k < 6; ++k)
                if (e.dofs[k]) {
                    const int m = 6 * im->second + k, s = 6 * is->second + k;
                    rows.push_back({s, {{s, 1.0}, {m, -1.0}}});
                }
        }
        for (const FemDiaphragm& dg : model.diaphragms) {
            std::vector<int> idx;
            for (int id : dg.nodes) {
                const auto it = nodeIndex.find(id);
                if (it == nodeIndex.end()) return failAll(SolveStatus::Error, formatInt("El diafragma %d contiene nudos que no existen.", dg.id));
                if (std::find(idx.begin(), idx.end(), it->second) == idx.end()) idx.push_back(it->second);
            }
            // Shells of the diaphragm: every mesh node of their triangles (copies included) [N].
            for (int sid : dg.shells) {
                const auto it = shellIndex.find(sid);
                if (it == shellIndex.end()) return failAll(SolveStatus::Error, formatInt("El diafragma %d contiene láminas que no existen.", dg.id));
                std::vector<int> nodes;
                for (const ShellMesh::Triangle& t : mesh.triangles)
                    if (t.shell == it->second) nodes.insert(nodes.end(), t.nodes.begin(), t.nodes.end());
                std::sort(nodes.begin(), nodes.end());
                nodes.erase(std::unique(nodes.begin(), nodes.end()), nodes.end());
                for (int n : nodes)
                    if (std::find(idx.begin(), idx.end(), n) == idx.end()) idx.push_back(n);
            }
            if (idx.empty()) continue;
            const int m = idx.front();
            if (!diaphragmMaster.emplace(dg.id, m).second) return failAll(SolveStatus::Error, formatInt("Diafragma %d repetido.", dg.id));
            const Vec3S& pm = mesh.positions[m];
            for (std::size_t j = 1; j < idx.size(); ++j) {
                const int s = idx[j];
                const Vec3S& ps = mesh.positions[s];
                // Assertion of the original: a diaphragm is horizontal, |z_m - z_s| < 1e-3 [REA 2].
                if (std::fabs(ps[2] - pm[2]) >= 1e-3)
                    return failAll(SolveStatus::Error, formatInt("Los nudos del diafragma %d no están a la misma cota.", dg.id));
                const double dx = ps[0] - pm[0], dy = ps[1] - pm[1];
                rows.push_back({6 * s + 0, {{6 * s + 0, 1.0}, {6 * m + 0, -1.0}, {6 * m + 5, dy}}});
                rows.push_back({6 * s + 1, {{6 * s + 1, 1.0}, {6 * m + 1, -1.0}, {6 * m + 5, -dx}}});
                rows.push_back({6 * s + 5, {{6 * s + 5, 1.0}, {6 * m + 5, -1.0}}});
            }
        }
        for (int i = 0; i < nNodes; ++i) {
            const FemNode& n = model.nodes[i];
            if (n.oblique == ObliqueSupport::None) continue;
            const V3 v = toEigen(n.obliqueDirection);
            if (!v.allFinite() || v.norm() < 1e-12 || fixed[6 * i] || fixed[6 * i + 1] || fixed[6 * i + 2])
                return failAll(SolveStatus::Error, formatInt("El apoyo oblicuo del nudo %d no es válido.", n.id));
            anyOblique = true;
            int km = 0;
            for (int k = 1; k < 3; ++k)
                if (std::fabs(v(k)) > std::fabs(v(km))) km = k;
            if (n.oblique == ObliqueSupport::Plane) {
                rows.push_back({6 * i + km, {{6 * i, v(0)}, {6 * i + 1, v(1)}, {6 * i + 2, v(2)}}});  // n . u = 0
            } else {
                for (int k = 0; k < 3; ++k)  // u parallel to d: d_km u_k - d_k u_km = 0
                    if (k != km) rows.push_back({6 * i + k, {{6 * i + k, v(km)}, {6 * i + km, -v(k)}}});
            }
        }
        // Oblique edge bonds [V 11.3]: the same relations on every mesh node of the edge. A node that already has an oblique support of its
        // own keeps it; blocked translations are absorbed by the elimination (a relation between blocked DOFs only is dropped).
        for (int i = 0; i < nAll && i < static_cast<int>(mesh.oblique.size()); ++i) {
            if (mesh.oblique[i] == ObliqueSupport::None || (i < nNodes && model.nodes[i].oblique != ObliqueSupport::None)) continue;
            const V3 v(mesh.obliqueDirection[i][0], mesh.obliqueDirection[i][1], mesh.obliqueDirection[i][2]);
            if (!v.allFinite() || v.norm() < 1e-12) return failAll(SolveStatus::Error, "El apoyo oblicuo de una arista de lámina no es válido.");
            anyOblique = true;
            int km = 0;
            for (int k = 1; k < 3; ++k)
                if (std::fabs(v(k)) > std::fabs(v(km))) km = k;
            if (mesh.oblique[i] == ObliqueSupport::Plane) {
                rows.push_back({6 * i + km, {{6 * i, v(0)}, {6 * i + 1, v(1)}, {6 * i + 2, v(2)}}});
            } else {
                for (int k = 0; k < 3; ++k)
                    if (k != km) rows.push_back({6 * i + k, {{6 * i + k, v(km)}, {6 * i + km, -v(k)}}});
            }
        }
        // Interior edge bonds and partially fixed shell corners: copies tied to their original nodes.
        for (const ShellMesh::Relation& r : mesh.relations) rows.push_back({r.preferred, r.terms});
        if (!rows.empty()) {
            const detail::DofElimination elim = detail::eliminateRelations(rows, fixed, nDof);
            if (!elim.error.empty()) return failAll(SolveStatus::Error, elim.error);
            slave = elim.slave;
            tMat = elim.t;
        }
    }
    const bool hasRelations = tMat.rows() > 0;
    Eigen::SparseMatrix<double> kFull = hasRelations ? Eigen::SparseMatrix<double>(tMat.transpose() * kOrig * tMat) : kOrig;
    if (hasRelations) kFull.prune(0.0);
    const Eigen::VectorXd diag = kFull.diagonal();
    const double maxDiag = diag.cwiseAbs().maxCoeff();

    // --- 4. constrained (supports and edge bonds), eliminated (slaves of relations), orphan (no stiffness) and free DOFs ---
    for (int d = 0; d < nDof; ++d) {
        if (!fixed[d] && !slave[d] && std::fabs(diag(d)) <= options.orphanTolerance * maxDiag) {
            orphan[d] = 1;
            if (d / 6 < nNodes) result.lockedDofs[model.nodes[d / 6].id][d % 6] = true;
        }
    }
    std::vector<char> nullFixed(nDof, 0);
    std::vector<int> freeIndex(nDof, -1);
    std::vector<int> freeDofs;
    Eigen::SparseMatrix<double> kff, kfc;
    const auto partition = [&]() {
        std::fill(freeIndex.begin(), freeIndex.end(), -1);
        freeDofs.clear();
        for (int d = 0; d < nDof; ++d)
            if (!fixed[d] && !slave[d] && !orphan[d] && !nullFixed[d]) {
                freeIndex[d] = static_cast<int>(freeDofs.size());
                freeDofs.push_back(d);
            }
        const int nf = static_cast<int>(freeDofs.size());
        std::vector<Eigen::Triplet<double>> tff, tfc;
        for (int col = 0; col < kFull.outerSize(); ++col)
            for (Eigen::SparseMatrix<double>::InnerIterator it(kFull, col); it; ++it) {
                const int row = static_cast<int>(it.row());
                if (freeIndex[row] < 0) continue;
                if (freeIndex[col] >= 0) tff.emplace_back(freeIndex[row], freeIndex[col], it.value());
                else tfc.emplace_back(freeIndex[row], col, it.value());
            }
        kff = Eigen::SparseMatrix<double>(nf, nf);
        kfc = Eigen::SparseMatrix<double>(nf, nDof);
        kff.setFromTriplets(tff.begin(), tff.end());
        kfc.setFromTriplets(tfc.begin(), tfc.end());
    };

    const auto nameMechanism = [&](int dof) {
        const int node = dof / 6;
        result.mechanismNode = node < nNodes ? model.nodes[node].id : -1;
        result.mechanismDof = dof % 6;
        // A node of a vertical bar is located by the zone of the bar (plan coordinates of its ends and the level of the node) [V, 10.3].
        if (node < nNodes)
            for (const Segment& seg : segments) {
                const FemBar& bar = model.bars[seg.bar];
                if (std::fabs(bar.ex[2]) < 1.0 - 1e-9 || (seg.nodes[0] != node && seg.nodes[1] != node)) continue;
                const Vec3S &a = mesh.positions[seg.nodes[0]], &b = mesh.positions[seg.nodes[1]];
                return mechanismWarningZone(dof % 6, a[0], a[1], b[0], b[1], mesh.positions[node][2]);
            }
        return mechanismWarning(dof % 6, mesh.positions[node]);
    };
    const auto mechanismFailure = [&]() {
        const int dof = locateMechanismDof(kff, freeDofs, diag);
        if (dof < 0) return failAll(SolveStatus::Mechanism, {});
        return failAll(SolveStatus::Mechanism, nameMechanism(dof));
    };

    // --- 5. factorisation and null pivots ---
    // A null pivot is a null mode of the stiffness. Rule [R] (solver_medidas_original.md 10.3): a mode that the loads of a case do not excite
    // is fixed to 0 without a warning; a case whose loads excite it is a mechanism (warning of the original naming a DOF of the mode).
    // The stiffness is positive semi-definite, so every null pivot of one factorisation is fixed at once and the matrix factorised again.
    Eigen::SimplicialLDLT<Eigen::SparseMatrix<double>> ldlt;
    for (int pass = 0;; ++pass) {
        partition();
        const int nf = static_cast<int>(freeDofs.size());
        if (nf == 0) break;
        // Detection on the matrix shifted by 1e-3 x the pivot tolerance (positive definite): a null mode gives a pivot of about the shift instead of
        // an exact zero, which would stop the factorisation (e.g. the twist of identical collinear bars).
        Eigen::SparseMatrix<double> shifted = kff;
        for (int i = 0; i < nf; ++i) shifted.coeffRef(i, i) += 1e-3 * options.pivotTolerance * std::fabs(diag(freeDofs[i]));
        ldlt.compute(shifted);
        if (ldlt.info() != Eigen::Success) return mechanismFailure();
        const Eigen::VectorXd d = ldlt.vectorD();
        Eigen::VectorXd kd(nf);
        for (int i = 0; i < nf; ++i) kd(i) = diag(freeDofs[i]);
        const Eigen::VectorXd kdPermuted = ldlt.permutationP() * kd;
        std::vector<int> original(nf);
        for (int j = 0; j < nf; ++j) original[ldlt.permutationP().indices()(j)] = j;
        std::vector<int> nullPivots;
        for (int i = 0; i < nf; ++i) {
            if (!std::isfinite(d(i))) return mechanismFailure();
            if (d(i) < -options.pivotTolerance * std::fabs(kdPermuted(i))) return failAll(SolveStatus::Unstable, {});
            if (d(i) <= options.pivotTolerance * std::fabs(kdPermuted(i))) nullPivots.push_back(freeDofs[original[i]]);
        }
        if (nullPivots.empty()) {
            ldlt.compute(kff);  // the factorisation used for the solution is the unshifted one
            if (ldlt.info() != Eigen::Success) return mechanismFailure();
            break;
        }
        if (pass >= 20) return mechanismFailure();
        for (int dof : nullPivots) nullFixed[dof] = 1;
    }
    const int nFree = static_cast<int>(freeDofs.size());
    std::vector<NullMode> nullModes;
    for (int d = 0; d < nDof; ++d) {
        if (!nullFixed[d]) continue;
        NullMode m;
        m.dof = d;
        m.v = Eigen::VectorXd::Zero(nDof);
        m.v(d) = 1.0;
        if (nFree > 0) {
            Eigen::VectorXd rhs = Eigen::VectorXd::Zero(nFree);
            for (Eigen::SparseMatrix<double>::InnerIterator it(kFull, d); it; ++it)
                if (freeIndex[it.row()] >= 0) rhs(freeIndex[it.row()]) = -it.value();
            const Eigen::VectorXd x = ldlt.solve(rhs);
            for (int i = 0; i < nFree; ++i) m.v(freeDofs[i]) = x(i);
        }
        // The mode must be exact (K v = 0 also on the fixed rows); otherwise the pivot was only small: a mechanism.
        const Eigen::VectorXd kv = kFull * m.v;
        double residual = 0.0, scale = 0.0;
        for (int r = 0; r < nDof; ++r)
            if (!fixed[r]) residual = std::max(residual, std::fabs(kv(r)));
        for (int c = 0; c < kFull.outerSize(); ++c)
            for (Eigen::SparseMatrix<double>::InnerIterator it(kFull, c); it; ++it) scale = std::max(scale, std::fabs(it.value() * m.v(c)));
        if (!(residual <= 1e-6 * scale)) return mechanismFailure();
        if (d / 6 < nNodes) result.nullModeDofs[model.nodes[d / 6].id][d % 6] = true;
        nullModes.push_back(std::move(m));
    }

    if (hasShells) result.mesh = mesh;

    // --- 6. load cases ---
    for (std::size_t c = 0; c < cases.size(); ++c) {
        const LoadCaseInput& lc = cases[c];
        LoadCaseResult& cr = result.cases[c];
        const auto failCase = [&](SolveStatus status) {
            cr.status = status;
            cr.message = status == SolveStatus::Mechanism ? mechanismMessage(cr.name) : caseErrorMessage(cr.name);
            if (result.status == SolveStatus::Ok) result.status = status;
        };
        Eigen::VectorXd f = Eigen::VectorXd::Zero(nDof);
        bool bad = false;
        for (const FemNodalLoad& nl : lc.nodal) {
            const auto it = nodeIndex.find(nl.node);
            if (it == nodeIndex.end()) { bad = true; break; }
            for (int k = 0; k < 6; ++k) f(6 * it->second + k) += nl.values[k];
        }
        // Bar loads -> equivalent nodal loads (local, unreleased) of every segment, kept for the end force recovery.
        std::vector<std::vector<LocalLoad>> local(model.bars.size());
        for (const FemBarLoad& bl : lc.bars) {
            const auto it = barIndex.find(bl.bar);
            if (it == barIndex.end()) { bad = true; break; }
            const int b = it->second;
            local[b].push_back(toLocal(bl, rotation(model.bars[b]), barLength[b]));
        }
        std::vector<Vec12> fe(segments.size(), Vec12::Zero());
        if (!bad) {
            for (std::size_t b = 0; b < model.bars.size(); ++b) {
                if (local[b].empty()) continue;
                for (std::size_t k = 0; k < barSegments[b].size(); ++k) {
                    const int si = barSegments[b][k];
                    const Segment& seg = segments[si];
                    const std::vector<LocalLoad> loads =
                        barSegments[b].size() == 1 ? local[b] : segmentLoads(local[b], seg.x0, seg.x0 + seg.el.length, k + 1 == barSegments[b].size());
                    if (loads.empty()) continue;
                    fe[si] = -fixedEndReactions(seg.el, loads);
                    const Vec12 fg = seg.el.t.transpose() * seg.el.condenseLoad(fe[si]);
                    for (int q = 0; q < 6; ++q) {
                        f(6 * seg.nodes[0] + q) += fg(q);
                        f(6 * seg.nodes[1] + q) += fg(6 + q);
                    }
                }
            }
        }
        // Shell loads.
        std::vector<detail::Vec36> shellFe(mesh.triangles.size(), detail::Vec36::Zero());
        std::vector<std::array<double, 2>> strain0(mesh.triangles.size(), {0.0, 0.0});
        for (const FemShellLoad& sl : lc.shellLoads) {
            if (bad) break;
            const auto it = shellIndex.find(sl.shell);
            if (it == shellIndex.end()) { bad = true; break; }
            const FemShell& sh = model.shells[it->second];
            const std::array<double, 2> e0 = detail::initialStrain(sh, sl);
            for (std::size_t e = 0; e < mesh.triangles.size(); ++e) {
                if (mesh.triangles[e].shell != it->second) continue;
                const detail::Vec36 v = detail::shellLoadVector(shellElements[e], sh, sl, shellXY(mesh.triangles[e]));
                shellFe[e] += v;
                strain0[e][0] += e0[0];
                strain0[e][1] += e0[1];
                for (int p = 0; p < 36; ++p) f(6 * mesh.triangles[e].nodes[p / 6] + p % 6) += v(p);
            }
        }
        // Diaphragm loads: Fx, Fy at (x, y) -> the master with Mz = (x - xm) Fy - (y - ym) Fx [V 10.2].
        for (const FemDiaphragmLoad& dl : lc.diaphragmLoads) {
            if (bad) break;
            const auto it = diaphragmMaster.find(dl.diaphragm);
            if (it == diaphragmMaster.end()) { bad = true; break; }
            const int m = it->second;
            const Vec3S& pm = mesh.positions[m];
            f(6 * m + 0) += dl.fx;
            f(6 * m + 1) += dl.fy;
            f(6 * m + 5) += (dl.x - pm[0]) * dl.fy - (dl.y - pm[1]) * dl.fx;
        }
        // Loads in the reduced numbering (slaves carry nothing: their loads go to their masters).
        const Eigen::VectorXd fNodal = f;
        if (hasRelations) f = tMat.transpose() * fNodal;
        Eigen::VectorXd u = Eigen::VectorXd::Zero(nDof);
        for (const auto& [nodeId, values] : lc.prescribed) {
            const auto it = nodeIndex.find(nodeId);
            if (it == nodeIndex.end()) { bad = true; break; }
            for (int k = 0; k < 6; ++k)
                if (fixed[6 * it->second + k]) u(6 * it->second + k) = values[k];
        }
        if (bad || !f.allFinite() || !u.allFinite()) { failCase(SolveStatus::Error); continue; }
        // A loaded DOF without stiffness cannot be equilibrated.
        const double fScale = std::max(1.0, f.cwiseAbs().maxCoeff());
        bool orphanLoaded = false;
        for (int d = 0; d < nDof; ++d)
            if (orphan[d] && std::fabs(f(d)) > 1e-9 * fScale) orphanLoaded = true;
        if (orphanLoaded) { failCase(SolveStatus::Mechanism); continue; }
        // A load with a component along a null mode is a mechanism (warning of the original naming the largest DOF of the mode).
        if (!nullModes.empty()) {
            const Eigen::VectorXd g = f - kFull * u;
            double gNorm = 0.0;
            for (int d = 0; d < nDof; ++d)
                if (!fixed[d]) gNorm += g(d) * g(d);
            gNorm = std::sqrt(gNorm);
            const NullMode* excited = nullptr;
            for (const NullMode& m : nullModes) {
                double work = 0.0;
                for (int d = 0; d < nDof; ++d)
                    if (!fixed[d]) work += m.v(d) * g(d);
                if (std::fabs(work) > options.nullModeTolerance * gNorm * m.v.norm()) { excited = &m; break; }
            }
            if (excited) {
                int best = excited->dof;
                for (int d = 0; d < nDof; ++d)
                    if (!fixed[d] && std::fabs(excited->v(d)) > std::fabs(excited->v(best)) * (1.0 + 1e-9)) best = d;
                const int savedNode = result.mechanismNode, savedDof = result.mechanismDof;
                const std::string warning = nameMechanism(best);
                cr.status = SolveStatus::Mechanism;
                cr.message = warning;
                if (result.status == SolveStatus::Ok) {
                    result.status = SolveStatus::Mechanism;
                    result.message = warning;
                } else {
                    result.mechanismNode = savedNode;
                    result.mechanismDof = savedDof;
                }
                continue;
            }
        }

        if (nFree > 0) {
            Eigen::VectorXd rhs(nFree);
            for (int i = 0; i < nFree; ++i) rhs(i) = f(freeDofs[i]);
            rhs -= kfc * u;
            const Eigen::VectorXd uf = ldlt.solve(rhs);
            if (ldlt.info() != Eigen::Success || !uf.allFinite()) { failCase(SolveStatus::Mechanism); continue; }
            for (int i = 0; i < nFree; ++i) u(freeDofs[i]) = uf(i);
        }
        const Eigen::VectorXd ur = u;  // reduced numbering (slaves 0)
        if (hasRelations) u = tMat * ur;

        if (options.maxNodeRotation > 0.0) {
            bool excessive = false;
            // Nodes of the bars only: nodes reached only by shells are not checked (a plate with every out-of-plane stiffness factor 0
            // reaches 9.6e8 mm without any warning in the original [V, elemento_lamina_bandas.md 15.4]; the exact rule is [N]).
            std::vector<char> barNode(nAll, 0);
            for (const Segment& seg : segments) barNode[seg.nodes[0]] = barNode[seg.nodes[1]] = 1;
            for (int i = 0; i < nAll && !excessive; ++i)
                for (int k = 3; k < 6 && barNode[i]; ++k)
                    if (std::fabs(u(6 * i + k)) > options.maxNodeRotation) { excessive = true; break; }
            if (excessive) {
                cr.status = SolveStatus::ExcessiveDisplacements;
                cr.message = excessiveDisplacementsMessage();
                if (result.status == SolveStatus::Ok) { result.status = cr.status; result.message = cr.message; }
                continue;
            }
        }
        cr.displacements.assign(nNodes, Dof6{});
        for (int i = 0; i < nNodes; ++i)
            for (int k = 0; k < 6; ++k) cr.displacements[i][k] = u(6 * i + k);
        // Blocked DOFs: residual of the reduced system (includes the forces transmitted by the relations to a blocked master). Oblique
        // supports: residual of the unreduced system on the translations of the node (the force of the support, along its normal / across
        // its line). Springs: -K u.
        const Eigen::VectorXd ku = kFull * ur;
        Eigen::VectorXd kuNodal;
        if (anyOblique) kuNodal = kOrig * u;
        for (int i = 0; i < nAll; ++i) {
            bool supported = false;
            Dof6 r{};
            for (int k = 0; k < 6; ++k)
                if (fixed[6 * i + k]) {
                    supported = true;
                    r[k] = ku(6 * i + k) - f(6 * i + k);
                }
            if ((i < nNodes && model.nodes[i].oblique != ObliqueSupport::None) ||
                (i < static_cast<int>(mesh.oblique.size()) && mesh.oblique[i] != ObliqueSupport::None)) {
                supported = true;
                for (int k = 0; k < 3; ++k)
                    if (!fixed[6 * i + k]) r[k] = kuNodal(6 * i + k) - fNodal(6 * i + k);
            }
            for (int k = 0; k < 6; ++k)
                if (springK[6 * i + k] > 0.0 && !fixed[6 * i + k]) {
                    supported = true;
                    r[k] = -springK[6 * i + k] * u(6 * i + k);
                }
            if (!supported) continue;
            if (i < nNodes) cr.reactions[model.nodes[i].id] = r;
            else cr.meshReactions[i] = r;
        }
        cr.barForces.resize(model.bars.size());
        cr.barDisplacements.resize(model.bars.size());
        cr.barSegments.assign(model.bars.size(), {});
        for (std::size_t b = 0; b < model.bars.size(); ++b) {
            BarEndForces& bf = cr.barForces[b];
            BarEndDisplacements& bd = cr.barDisplacements[b];
            bf.bar = bd.bar = model.bars[b].id;
            if (barSegments[b].size() > 1) {
                for (const int si : barSegments[b]) {
                    const Segment& seg = segments[si];
                    Vec12 ug;
                    for (int k = 0; k < 6; ++k) {
                        ug(k) = u(6 * seg.nodes[0] + k);
                        ug(6 + k) = u(6 * seg.nodes[1] + k);
                    }
                    const Vec12 ue = seg.el.elementDisplacements(seg.el.t * ug, fe[si]);
                    const Vec12 s = seg.el.ke * ue - fe[si];
                    const Vec12 ueGlobal = seg.el.t.transpose() * ue;
                    BarSegmentResult piece;
                    piece.x0 = seg.x0;
                    piece.x1 = seg.x0 + seg.el.length;
                    piece.originForces = applySignConvention({-s(0), -s(1), -s(2), -s(3), -s(4), -s(5)});
                    piece.endForces = applySignConvention({s(6), s(7), s(8), s(9), s(10), s(11)});
                    for (int k = 0; k < 6; ++k) piece.originDisplacement[k] = ueGlobal(k);
                    cr.barSegments[b].push_back(piece);
                }
            }
            for (int which = 0; which < 2; ++which) {
                const int si = which == 0 ? barSegments[b].front() : barSegments[b].back();
                const Segment& seg = segments[si];
                Vec12 ug;
                for (int k = 0; k < 6; ++k) {
                    ug(k) = u(6 * seg.nodes[0] + k);
                    ug(6 + k) = u(6 * seg.nodes[1] + k);
                }
                const Vec12 ue = seg.el.elementDisplacements(seg.el.t * ug, fe[si]);
                const Vec12 s = seg.el.ke * ue - fe[si];  // forces of the nodes on the element, local
                const Vec12 ueGlobal = seg.el.t.transpose() * ue;
                if (which == 0) {
                    bf.origin = applySignConvention({-s(0), -s(1), -s(2), -s(3), -s(4), -s(5)});
                    for (int k = 0; k < 6; ++k) bd.origin[k] = ueGlobal(k);
                } else {
                    bf.end = applySignConvention({s(6), s(7), s(8), s(9), s(10), s(11)});
                    for (int k = 0; k < 6; ++k) bd.end[k] = ueGlobal(6 + k);
                }
            }
        }
        if (hasShells) {
            cr.meshDisplacements.assign(nAll, Dof6{});
            for (int i = 0; i < nAll; ++i)
                for (int k = 0; k < 6; ++k) cr.meshDisplacements[i][k] = u(6 * i + k);
            cr.triangleForces.resize(mesh.triangles.size());
            for (std::size_t e = 0; e < mesh.triangles.size(); ++e) {
                detail::Vec36 ue;
                for (int p = 0; p < 36; ++p) ue(p) = u(6 * mesh.triangles[e].nodes[p / 6] + p % 6);
                cr.triangleForces[e].atNodes = detail::shellForcesAtNodes(shellElements[e], ue, strain0[e]);
                cr.triangleForces[e].atGauss = detail::shellForcesAtGauss(shellElements[e], ue, strain0[e]);
                const detail::Vec36 nodal = shellElements[e].kGlobal * ue - shellFe[e];
                for (int p = 0; p < 36; ++p) cr.triangleForces[e].nodalForces[p] = nodal(p);
            }
        }
    }
    return result;
}

}  // namespace

std::string nonConvergenceMessage(const std::string& caseName) {
    return format("Con las condiciones impuestas, no se alcanza la convergencia para el caso de carga '%s'. Revise las condiciones del modelo.",
                  caseName);
}

LoadCaseInput combineLoadCases(std::span<const LoadCaseInput> cases, std::span<const double> coefficients, int id, const std::string& name) {
    LoadCaseInput out;
    out.id = id;
    out.name = name;
    const auto scale3 = [](Vec3 v, double c) { return Vec3{c * v[0], c * v[1], c * v[2]}; };
    for (std::size_t i = 0; i < cases.size(); ++i) {
        const double c = i < coefficients.size() ? coefficients[i] : 0.0;
        if (c == 0.0) continue;
        const LoadCaseInput& lc = cases[i];
        for (FemNodalLoad nl : lc.nodal) {
            for (double& v : nl.values) v *= c;
            out.nodal.push_back(nl);
        }
        for (FemBarLoad bl : lc.bars) {
            bl.w1 = scale3(bl.w1, c);
            bl.w2 = scale3(bl.w2, c);
            bl.uniformTemperature *= c;
            bl.gradientY *= c;
            bl.gradientZ *= c;
            out.bars.push_back(bl);
        }
        for (FemShellLoad sl : lc.shellLoads) {
            sl.p0 *= c;
            sl.px *= c;
            sl.py *= c;
            sl.value *= c;
            out.shellLoads.push_back(sl);
        }
        for (FemDiaphragmLoad dl : lc.diaphragmLoads) {
            dl.fx *= c;
            dl.fy *= c;
            out.diaphragmLoads.push_back(dl);
        }
        for (const auto& [node, values] : lc.prescribed) {
            Dof6& acc = out.prescribed[node];
            for (int k = 0; k < 6; ++k) acc[k] += c * values[k];
        }
    }
    return out;
}

bool hasUnilateralSprings(const FrameModel& model) {
    for (const FemNode& n : model.nodes)
        for (int k = 0; k < 6; ++k)
            if (n.spring[k] > 0.0 && n.springSense[k] != SpringSense::Both) return true;
    for (const FemShell& sh : model.shells)
        for (std::size_t e = 0; e < sh.edgeSpring.size() && e < sh.edgeSpringSense.size(); ++e)
            for (int k = 0; k < 6; ++k)
                if (sh.edgeSpring[e][k] > 0.0 && sh.edgeSpringSense[e][k] != SpringSense::Both) return true;
    return false;
}

LinearResult solveNonlinear(const FrameModel& model, std::span<const LoadCaseInput> combinedCases, const SolverOptions& options, int maxIterations) {
    if (!hasUnilateralSprings(model)) return solveLinear(model, combinedCases, options);
    // Unilateral springs: those of the FemNodes and of the elastic shell edges (indices into groundSprings(), whose order is deterministic).
    const std::vector<GroundSpring> all = groundSprings(model, meshShells(model));
    struct Spring { std::size_t index; int node; int dof; double sign; };
    std::vector<Spring> springs;
    for (std::size_t i = 0; i < all.size(); ++i)
        if (all[i].k > 0.0 && all[i].sense != SpringSense::Both)
            springs.push_back({i, all[i].node, all[i].dof, all[i].sense == SpringSense::Positive ? 1.0 : -1.0});
    const bool hasShells = !model.shells.empty();
    LinearResult result;
    result.cases.resize(combinedCases.size());
    const auto worsen = [&](const LoadCaseResult& cr) {
        if (result.status == SolveStatus::Ok && cr.status != SolveStatus::Ok) {
            result.status = cr.status;
            result.message = cr.message;
        }
    };
    for (std::size_t c = 0; c < combinedCases.size(); ++c) {
        const LoadCaseInput& lc = combinedCases[c];
        std::vector<char> active(springs.size(), 1);
        std::vector<std::vector<char>> seen;
        LoadCaseResult& out = result.cases[c];
        bool done = false;
        for (int it = 0; it < std::max(1, maxIterations) && !done; ++it) {
            seen.push_back(active);
            std::vector<char> inactive(all.size(), 0);
            for (std::size_t s = 0; s < springs.size(); ++s)
                if (!active[s]) inactive[springs[s].index] = 1;
            LinearResult r = solveLinearImpl(model, std::span<const LoadCaseInput>(&lc, 1), options, &inactive);
            for (const auto& [id, mask] : r.lockedDofs)
                for (int k = 0; k < 6; ++k) result.lockedDofs[id][k] = result.lockedDofs[id][k] || mask[k];
            if (result.mesh.positions.empty()) result.mesh = std::move(r.mesh);
            LoadCaseResult& cr = r.cases.front();
            if (cr.status != SolveStatus::Ok) {
                out = std::move(cr);
                if (out.status == SolveStatus::Mechanism) out.message = mechanismMessage(lc.name);  // text measured for "[1] PP" [V 10.1]
                done = true;
                break;
            }
            // A spring acts while the displacement has the sign of its sense; a dead band of 1e-12 m (rad) avoids flip-flop at u = 0.
            std::vector<char> next(springs.size(), 0);
            for (std::size_t s = 0; s < springs.size(); ++s) {
                const double u = hasShells ? cr.meshDisplacements[springs[s].node][springs[s].dof] : cr.displacements[springs[s].node][springs[s].dof];
                const double v = springs[s].sign * u;
                next[s] = active[s] ? (v > -1e-12) : (v > 1e-12);
            }
            if (next == active) {
                out = std::move(cr);
                done = true;
                break;
            }
            if (std::find(seen.begin(), seen.end(), next) != seen.end()) break;  // the set cycles
            active = std::move(next);
        }
        if (!done) {
            out = LoadCaseResult{};
            out.status = SolveStatus::NotConverged;
            out.message = nonConvergenceMessage(lc.name);
        }
        out.id = lc.id;
        out.name = lc.name;
        worsen(out);
    }
    return result;
}

}  // namespace cc::fem
