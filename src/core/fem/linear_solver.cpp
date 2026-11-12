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

}  // namespace

std::string mechanismWarningZone(int dof, double x1, double y1, double x2, double y2, double z) {
    std::string s = mechanismWarning(dof, {0, 0, 0});
    const std::size_t a = s.find("\nEl error se encuentra");
    const std::size_t b = s.find("\nAlguna de las barras");
    char zone[200];
    std::snprintf(zone, sizeof zone, "\nEl error se encuentra en la zona de coordenadas:\n (%.6f,%.6f) a (%.6f,%.6f), en la cota %.6f", x1, y1, x2, y2, z);
    return s.substr(0, a) + zone + s.substr(b);
}

LinearResult solveLinear(const FrameModel& model, std::span<const LoadCaseInput> cases, const SolverOptions& options) {
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
    for (int i = 0; i < nNodes; ++i)
        for (int k = 0; k < 6; ++k) {
            const double s = model.nodes[i].spring[k];
            if (s != 0.0) {
                if (!std::isfinite(s) || s < 0.0) return failAll(SolveStatus::Error, formatInt("El muelle del nudo %d no es válido.", model.nodes[i].id));
                triplets.emplace_back(6 * i + k, 6 * i + k, s);
            }
        }
    Eigen::SparseMatrix<double> kFull(nDof, nDof);
    kFull.setFromTriplets(triplets.begin(), triplets.end());
    const Eigen::VectorXd diag = kFull.diagonal();
    const double maxDiag = diag.cwiseAbs().maxCoeff();

    // --- 4. constrained (supports and edge bonds), orphan (no stiffness) and free DOFs ---
    std::vector<char> fixed(nDof, 0), orphan(nDof, 0);
    for (int i = 0; i < nAll; ++i)
        for (int k = 0; k < 6; ++k) fixed[6 * i + k] = ((i < nNodes && model.nodes[i].fixed[k]) || mesh.edgeFixed[i][k]) ? 1 : 0;
    for (int d = 0; d < nDof; ++d) {
        if (!fixed[d] && std::fabs(diag(d)) <= options.orphanTolerance * maxDiag) {
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
            if (!fixed[d] && !orphan[d] && !nullFixed[d]) {
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
        const Eigen::VectorXd ku = kFull * u;
        for (int i = 0; i < nAll; ++i) {
            bool supported = false;
            Dof6 r{};
            for (int k = 0; k < 6; ++k)
                if (fixed[6 * i + k]) {
                    supported = true;
                    r[k] = ku(6 * i + k) - f(6 * i + k);
                }
            if (!supported) continue;
            if (i < nNodes) cr.reactions[model.nodes[i].id] = r;
            else cr.meshReactions[i] = r;
        }
        cr.barForces.resize(model.bars.size());
        cr.barDisplacements.resize(model.bars.size());
        for (std::size_t b = 0; b < model.bars.size(); ++b) {
            BarEndForces& bf = cr.barForces[b];
            BarEndDisplacements& bd = cr.barDisplacements[b];
            bf.bar = bd.bar = model.bars[b].id;
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

}  // namespace cc::fem
