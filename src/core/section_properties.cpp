#include "core/section_properties.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <utility>

namespace cc::core {

namespace {

constexpr double kPi = std::numbers::pi;

struct ShapeInfo {
    ShapeKind kind;
    const char* key;
    const char* nameEs;
    std::vector<std::string> dims;
};

const std::vector<ShapeInfo>& shapeTable() {
    static const std::vector<ShapeInfo> table = {
        {ShapeKind::IParallel, "iParallel", "Doble T de alas paralelas", {"h", "b", "tw", "tf", "r"}},
        {ShapeKind::ISloped, "iSloped", "Doble T de alas inclinadas", {"h", "b", "tw", "tf", "r1", "r2", "slope"}},
        {ShapeKind::Channel, "channel", "U", {"h", "b", "tw", "tf", "r1", "r2", "slope"}},
        {ShapeKind::Tee, "tee", "T", {"h", "b", "tw", "tf", "r"}},
        {ShapeKind::AngleEqual, "angleEqual", "L de lados iguales", {"h", "t", "r1", "r2"}},
        {ShapeKind::AngleUnequal, "angleUnequal", "L de lados desiguales", {"h", "b", "t", "r1", "r2"}},
        {ShapeKind::FlatBar, "flat", "Plano", {"h", "b"}},
        {ShapeKind::RoundBar, "round", "Redondo", {"d"}},
        {ShapeKind::SquareBar, "square", "Cuadrado", {"a"}},
        {ShapeKind::Generic, "generic", "Genérico", {"yNeg", "yPos", "zNeg", "zPos"}},
        {ShapeKind::ConcreteCircular, "concreteCircular", "Circular", {"d"}},
        {ShapeKind::ConcreteRectangular, "concreteRectangular", "Rectangular", {"b", "h"}},
        {ShapeKind::ConcreteRectangularVariable, "concreteRectangularVariable", "Rectangular de canto variable", {"b", "h0", "h1"}},  // #65245
        {ShapeKind::TimberRound, "timberRound", "Redondo", {"d"}},
        {ShapeKind::TimberSquare, "timberSquare", "Cuadrado", {"a"}},
        {ShapeKind::TimberRectangular, "timberRectangular", "Rectangular", {"b", "h"}},
        {ShapeKind::TimberRectangularVariable, "timberRectangularVariable", "Rectangular de canto variable", {"b", "h0", "h1"}},
        {ShapeKind::BuiltUpDoubleT, "builtUpDoubleT", "Doble T simétrica armada", {"h", "b", "tw", "tf"}},
        {ShapeKind::ColdFormedU, "coldFormedU", "U conformado simple", {"h", "b", "t", "r"}},
        {ShapeKind::ColdFormedC, "coldFormedC", "U conformado rigidizado", {"h", "b", "c", "t", "r"}},
        {ShapeKind::AluminiumI, "aluminiumI", "Doble T de aluminio", {"h", "b", "tw", "tf"}},
    };
    return table;
}

const ShapeInfo& info(ShapeKind kind) {
    for (const ShapeInfo& s : shapeTable())
        if (s.kind == kind) return s;
    return shapeTable().front();
}

struct FamilyInfo {
    SectionFamily family;
    const char* key;
    const char* nameEs;
};

const FamilyInfo kFamilies[] = {
    {SectionFamily::HotRolledSteel, "hotRolledSteel", "Acero laminado"},
    {SectionFamily::BuiltUpSteel, "builtUpSteel", "Acero armado"},
    {SectionFamily::ColdFormedSteel, "coldFormedSteel", "Acero conformado"},
    {SectionFamily::Aluminium, "aluminium", "Aluminio extruido"},
    {SectionFamily::Timber, "timber", "Madera"},
    {SectionFamily::Generic, "generic", "Genérico"},
    {SectionFamily::Concrete, "concrete", "Hormigón"},  // #27639
};

// ---------------------------------------------------------------------------------------------------------------
// Contour integration. A section contour is a closed polygon in the (y, z) plane; rounded corners (root fillets, toe
// radii) are tangent arcs approximated by kArcSegments chords, which makes the area and inertia error of an arc
// about 1e-4 of its own contribution (negligible next to the rounding of the published tables).
// ---------------------------------------------------------------------------------------------------------------

struct P2 {
    double y, z;
};
using Poly = std::vector<P2>;

constexpr int kArcSegments = 32;

Poly roundCorners(const Poly& pts, const std::vector<double>& radii) {
    Poly out;
    const std::size_t n = pts.size();
    for (std::size_t i = 0; i < n; ++i) {
        const P2 p = pts[i];
        const double r = radii[i];
        if (r <= 0) {
            out.push_back(p);
            continue;
        }
        const P2 a = pts[(i + n - 1) % n], b = pts[(i + 1) % n];
        double uy = a.y - p.y, uz = a.z - p.z, vy = b.y - p.y, vz = b.z - p.z;
        const double lu = std::hypot(uy, uz), lv = std::hypot(vy, vz);
        uy /= lu; uz /= lu; vy /= lv; vz /= lv;
        const double phi = std::acos(std::clamp(uy * vy + uz * vz, -1.0, 1.0));  // angle between the two edges
        const double tangent = r / std::tan(phi / 2);
        const P2 t1{p.y + uy * tangent, p.z + uz * tangent};
        const P2 t2{p.y + vy * tangent, p.z + vz * tangent};
        double by = uy + vy, bz = uz + vz;
        const double lb = std::hypot(by, bz);
        by /= lb; bz /= lb;
        const double d = r / std::sin(phi / 2);
        const P2 c{p.y + by * d, p.z + bz * d};
        const double a1 = std::atan2(t1.z - c.z, t1.y - c.y), a2 = std::atan2(t2.z - c.z, t2.y - c.y);
        double da = a2 - a1;
        while (da > kPi) da -= 2 * kPi;
        while (da < -kPi) da += 2 * kPi;
        for (int k = 0; k <= kArcSegments; ++k) {
            const double ang = a1 + da * k / kArcSegments;
            out.push_back({c.y + r * std::cos(ang), c.z + r * std::sin(ang)});
        }
    }
    return out;
}

struct Moments {
    double area = 0, cy = 0, cz = 0;  ///< area (positive) and centroid
    double iy = 0, iz = 0;            ///< about the centroidal y / z axes
};

Moments momentsOf(const Poly& poly) {
    double a2 = 0, sy = 0, sz = 0, iyO = 0, izO = 0;  // about the origin, twice the area
    const std::size_t n = poly.size();
    for (std::size_t i = 0; i < n; ++i) {
        const P2 p = poly[i], q = poly[(i + 1) % n];
        const double cr = p.y * q.z - q.y * p.z;
        a2 += cr;
        sy += (p.y + q.y) * cr;
        sz += (p.z + q.z) * cr;
        iyO += (p.z * p.z + p.z * q.z + q.z * q.z) * cr;
        izO += (p.y * p.y + p.y * q.y + q.y * q.y) * cr;
    }
    Moments m;
    const double sign = a2 < 0 ? -1.0 : 1.0;
    m.area = std::fabs(a2) / 2;
    if (m.area <= 0) return m;
    m.cy = sy / (3 * a2);
    m.cz = sz / (3 * a2);
    m.iy = sign * iyO / 12 - m.area * m.cz * m.cz;
    m.iz = sign * izO / 12 - m.area * m.cy * m.cy;
    return m;
}

/// Part of the polygon with coordinate `axis` (0 = y, 1 = z) on the side `keepAbove` of `c` (Sutherland-Hodgman).
Poly clip(const Poly& poly, int axis, double c, bool keepAbove) {
    const auto coord = [axis](const P2& p) { return axis == 0 ? p.y : p.z; };
    const auto inside = [&](const P2& p) { return keepAbove ? coord(p) >= c : coord(p) <= c; };
    Poly out;
    const std::size_t n = poly.size();
    for (std::size_t i = 0; i < n; ++i) {
        const P2 p = poly[i], q = poly[(i + 1) % n];
        const bool pin = inside(p), qin = inside(q);
        if (pin) out.push_back(p);
        if (pin != qin) {
            const double t = (c - coord(p)) / (coord(q) - coord(p));
            out.push_back({p.y + t * (q.y - p.y), p.z + t * (q.z - p.z)});
        }
    }
    return out;
}

/// Plastic modulus about the axis parallel to y (axis = 1: neutral axis z = const) or to z (axis = 0): the neutral axis
/// divides the area in two equal parts and W = sum of area * distance of each half's centroid to the axis.
double plasticModulus(const Poly& poly, const Moments& whole, int axis) {
    double lo = poly.front().y, hi = lo;  // range of the coordinate
    for (const P2& p : poly) {
        const double v = axis == 0 ? p.y : p.z;
        lo = std::min(lo, v);
        hi = std::max(hi, v);
    }
    const double half = whole.area / 2;
    for (int it = 0; it < 100; ++it) {
        const double mid = (lo + hi) / 2;
        const Moments upper = momentsOf(clip(poly, axis, mid, true));
        if (upper.area > half)
            lo = mid;
        else
            hi = mid;
    }
    const double c = (lo + hi) / 2;
    const Moments up = momentsOf(clip(poly, axis, c, true)), down = momentsOf(clip(poly, axis, c, false));
    const auto dist = [&](const Moments& m) { return std::fabs((axis == 0 ? m.cy : m.cz) - c); };
    return up.area * dist(up) + down.area * dist(down);
}

// ---------------------------------------------------------------------------------------------------------------
// Shape geometry
// ---------------------------------------------------------------------------------------------------------------

/// Inclined flanges: thickness `tf` is given at `refFromTip` metres from the flange tip and grows towards the web by
/// `slope` (fraction). Returns the thickness at the tip.
double tipThickness(double tf, double slope, double refFromTip) { return tf - slope * refFromTip; }

/// Contour of the polygonal shapes; empty for the closed-form ones.
Poly contour(const Section& s) {
    const std::vector<double>& d = s.dims;
    switch (s.kind) {
    case ShapeKind::IParallel:
    case ShapeKind::ISloped: {
        const double h = d[0], b = d[1], tw = d[2], tf = d[3];
        const bool sloped = s.kind == ShapeKind::ISloped;
        const double r1 = d[4], r2 = sloped ? d[5] : 0.0, slope = sloped ? d[6] / 100.0 : 0.0;
        const double tip = tipThickness(tf, slope, b / 4), root = tip + slope * (b - tw) / 2;
        const double y0 = b / 2 - tw / 2, y1 = b / 2 + tw / 2;
        const Poly pts = {{0, 0},    {b, 0},      {b, tip},      {y1, root},     {y1, h - root}, {b, h - tip},
                          {b, h},    {0, h},      {0, h - tip},  {y0, h - root}, {y0, root},     {0, tip}};
        return roundCorners(pts, {0, 0, r2, r1, r1, r2, 0, 0, r2, r1, r1, r2});
    }
    case ShapeKind::Channel: {
        const double h = d[0], b = d[1], tw = d[2], tf = d[3], r1 = d[4], r2 = d[5], slope = d[6] / 100.0;
        const double tip = tipThickness(tf, slope, b / 2), root = tip + slope * (b - tw);
        const Poly pts = {{0, 0}, {b, 0}, {b, tip}, {tw, root}, {tw, h - root}, {b, h - tip}, {b, h}, {0, h}};
        return roundCorners(pts, {0, 0, r2, r1, r1, r2, 0, 0});
    }
    case ShapeKind::Tee: {
        const double h = d[0], b = d[1], tw = d[2], tf = d[3], r = d[4];
        const double y0 = b / 2 - tw / 2, y1 = b / 2 + tw / 2;
        const Poly pts = {{y0, 0}, {y1, 0}, {y1, h - tf}, {b, h - tf}, {b, h}, {0, h}, {0, h - tf}, {y0, h - tf}};
        return roundCorners(pts, {0, 0, r, 0, 0, 0, 0, r});
    }
    case ShapeKind::ColdFormedU: {
        // Web outer face at y = 0, flanges towards +y, z measured from the bottom. Outer corners radius r + t, inner corners r, tips sharp.
        const double h = d[0], b = d[1], t = d[2], r = d[3];
        const Poly pts = {{0, 0}, {b, 0}, {b, t}, {t, t}, {t, h - t}, {b, h - t}, {b, h}, {0, h}};
        return roundCorners(pts, {r + t, 0, 0, r, r, 0, 0, r + t});
    }
    case ShapeKind::ColdFormedC: {
        const double h = d[0], b = d[1], c = d[2], t = d[3], r = d[4];
        const Poly pts = {{0, 0}, {b, 0}, {b, c}, {b - t, c}, {b - t, t}, {t, t}, {t, h - t}, {b - t, h - t}, {b - t, h - c}, {b, h - c}, {b, h}, {0, h}};
        return roundCorners(pts, {r + t, r + t, 0, 0, r, r, r, r, 0, 0, r + t, r + t});
    }
    case ShapeKind::AngleEqual:
    case ShapeKind::AngleUnequal: {
        const bool eq = s.kind == ShapeKind::AngleEqual;
        const double h = d[0], b = eq ? d[0] : d[1], t = eq ? d[1] : d[2], r1 = eq ? d[2] : d[3], r2 = eq ? d[3] : d[4];
        const Poly pts = {{0, 0}, {b, 0}, {b, t}, {t, t}, {t, h}, {0, h}};
        return roundCorners(pts, {0, 0, r2, r1, r2, 0});
    }
    default:
        return {};
    }
}

/// Torsion constant of a rectangle b x t (b >= t), Saint-Venant series truncated as in the classical tables.
double rectangleTorsion(double b, double t) {
    const double l = std::max(b, t), s = std::min(b, t);
    return l * s * s * s * (1.0 / 3.0 - 0.21 * (s / l) * (1.0 - std::pow(s, 4) / (12.0 * std::pow(l, 4))));
}

/// Torsion constant of a solid rectangle as the original computes it (seccion.dll FUN_18001ef90, called by
/// paramsec_inercia_torsion_rectangulo): It = beta(l/s) * l * s^3 with beta piecewise linear through the classical table
/// (l/s: 1 -> 0.140, 1.25 -> 0.171, 1.5 -> 0.196, 2 -> 0.229, 3 -> 0.263, 4 -> 0.281, 6 -> 0.299, 10 -> 0.333) and constant 0.333
/// beyond 10. `l` is the longer side. Exactly the original's table, NOT Saint-Venant's series.
double rectangleTorsionOriginal(double l, double s) {
    static const double kRatio[] = {1.0, 1.25, 1.5, 2.0, 3.0, 4.0, 6.0, 10.0};
    static const double kBeta[] = {0.14, 0.171, 0.196, 0.229, 0.263, 0.281, 0.299, 0.333};
    const double r = l / s;
    double beta = kBeta[7];
    if (r <= kRatio[7]) {
        int i = 0;
        while (i < 6 && r > kRatio[i + 1]) ++i;
        beta = kBeta[i] + (r - kRatio[i]) * (kBeta[i + 1] - kBeta[i]) / (kRatio[i + 1] - kRatio[i]);
    }
    return beta * l * s * s * s;
}

/// Contribution of the web-flange junction of an I / T section (Bach-Roark inscribed circle): alpha * D^4.
double junctionTorsion(double tw, double tf, double r) {
    const double D = (std::pow(r + tw / 2, 2) + std::pow(r + tf, 2) - r * r) / (2 * r + tf);
    const double alpha = (tw / tf) * (0.1 * r / tf + 0.145);
    return alpha * std::pow(D, 4);
}

/// Smallest dimension (m) accepted by the original's rectangle / circle routines.
constexpr double kMinDimension = 1e-5;

bool positive(double v) { return std::isfinite(v) && v > 0; }
bool nonNegative(double v) { return std::isfinite(v) && v >= 0; }

}  // namespace

// --- names -----------------------------------------------------------------------------------------------------------

const char* shapeKey(ShapeKind kind) { return info(kind).key; }
const char* shapeNameEs(ShapeKind kind) { return info(kind).nameEs; }
const char* familyKey(SectionFamily family) {
    for (const FamilyInfo& f : kFamilies)
        if (f.family == family) return f.key;
    return kFamilies[0].key;
}
const char* familyNameEs(SectionFamily family) {
    for (const FamilyInfo& f : kFamilies)
        if (f.family == family) return f.nameEs;
    return kFamilies[0].nameEs;
}
std::optional<ShapeKind> shapeFromKey(const std::string& key) {
    for (const ShapeInfo& s : shapeTable())
        if (key == s.key) return s.kind;
    return std::nullopt;
}
std::optional<SectionFamily> familyFromKey(const std::string& key) {
    for (const FamilyInfo& f : kFamilies)
        if (key == f.key) return f.family;
    return std::nullopt;
}
const std::vector<ShapeKind>& allShapes() {
    static const std::vector<ShapeKind> all = [] {
        std::vector<ShapeKind> v;
        for (const ShapeInfo& s : shapeTable()) v.push_back(s.kind);
        return v;
    }();
    return all;
}
const std::vector<std::string>& dimensionKeys(ShapeKind kind) { return info(kind).dims; }

std::optional<SectionFamily> shapeFamily(ShapeKind kind) {
    switch (kind) {
    case ShapeKind::ConcreteCircular:
    case ShapeKind::ConcreteRectangular:
    case ShapeKind::ConcreteRectangularVariable:
        return SectionFamily::Concrete;
    case ShapeKind::TimberRound:
    case ShapeKind::TimberSquare:
    case ShapeKind::TimberRectangular:
    case ShapeKind::TimberRectangularVariable:
        return SectionFamily::Timber;
    case ShapeKind::BuiltUpDoubleT: return SectionFamily::BuiltUpSteel;
    case ShapeKind::ColdFormedU:
    case ShapeKind::ColdFormedC: return SectionFamily::ColdFormedSteel;
    case ShapeKind::AluminiumI: return SectionFamily::Aluminium;
    default:
        return std::nullopt;
    }
}

std::vector<ShapeKind> shapesOfFamily(SectionFamily family) {
    std::vector<ShapeKind> out;
    for (ShapeKind k : allShapes()) {
        if (k == ShapeKind::Generic) continue;
        const auto f = shapeFamily(k);
        if (f ? *f == family : family == SectionFamily::HotRolledSteel) out.push_back(k);
    }
    return out;
}

bool isConcreteOrTimber(ShapeKind kind) {
    const auto f = shapeFamily(kind);
    return f && (*f == SectionFamily::Concrete || *f == SectionFamily::Timber);
}

bool isCatalogueShape(ShapeKind kind) {
    return kind == ShapeKind::BuiltUpDoubleT || kind == ShapeKind::ColdFormedU || kind == ShapeKind::ColdFormedC ||
           kind == ShapeKind::AluminiumI;
}

bool isVariableDepth(ShapeKind kind) {
    return kind == ShapeKind::ConcreteRectangularVariable || kind == ShapeKind::TimberRectangularVariable;
}

bool SectionOverrides::any() const {
    return area || iy || iz || it || iw || wplY || wplZ || avy || avz;
}

// --- validation -------------------------------------------------------------------------------------------------------

std::string validateSection(const Section& s) {
    const std::vector<double>& d = s.dims;
    if (d.size() != dimensionKeys(s.kind).size()) return "El número de dimensiones no corresponde a la forma del perfil.";
    for (double v : d)
        if (!std::isfinite(v)) return "Las dimensiones del perfil deben ser números válidos.";
    for (const std::optional<double>* v : {&s.tabulated.area, &s.tabulated.iy, &s.tabulated.iz, &s.tabulated.it,
                                           &s.tabulated.iw, &s.tabulated.wplY, &s.tabulated.wplZ, &s.tabulated.avy,
                                           &s.tabulated.avz})
        if (*v && !nonNegative(**v)) return "Los valores tabulados del perfil no pueden ser negativos.";
    const char* kPositive = "Las dimensiones del perfil deben ser mayores que cero.";
    if (isCatalogueShape(s.kind)) {
        const bool cold = s.kind == ShapeKind::ColdFormedU || s.kind == ShapeKind::ColdFormedC;
        for (std::size_t i = 0; i < d.size(); ++i) {
            const bool radius = cold && i + 1 == d.size();  // the inner radius of the cold-formed shapes may be zero
            if (radius ? !(d[i] >= 0) : !(d[i] > kMinDimension)) return kPositive;
        }
        if (!cold) {  // double T: h, b, tw, tf
            if (d[0] <= 2 * d[3]) return "El canto del perfil no es compatible con el espesor de las alas.";
            if (d[1] <= d[2]) return "La anchura del perfil no es compatible con el espesor del alma.";
            return {};
        }
        // cold-formed: U = h, b, t, r; C = h, b, c, t, r. The contour of the section needs the corner arcs to fit in the straight parts.
        const bool lipped = s.kind == ShapeKind::ColdFormedC;
        const double h = d[0], b = d[1], c = lipped ? d[2] : 0.0, t = d[lipped ? 3 : 2], r = d[lipped ? 4 : 3];
        const double ro = r + t;
        if (h < 2 * ro) return "El canto del perfil no es compatible con el espesor y el radio de acuerdo.";
        if (b < (lipped ? 2 * ro : ro)) return "La anchura del perfil no es compatible con el espesor y el radio de acuerdo.";
        if (lipped && c < ro) return "La longitud del rigidizador no es compatible con el espesor y el radio de acuerdo.";
        return {};
    }
    if (isConcreteOrTimber(s.kind)) {
        // Concrete and timber sections are solid, computed from their dimensions only. The minimum 1e-5 m is the limit the
        // original asserts on the rectangle / circle routines ("ancho > .00001", "canto > .00001"; seccion.dll paramsec_*).
        if (s.tabulated.any()) return "Las secciones de hormigón y madera no admiten valores tabulados.";
        for (double v : d)
            if (!(v > kMinDimension)) return kPositive;
        return {};
    }
    switch (s.kind) {
    case ShapeKind::IParallel:
    case ShapeKind::Tee: {
        const bool tee = s.kind == ShapeKind::Tee;
        const double h = d[0], b = d[1], tw = d[2], tf = d[3], r = d[4];
        if (!positive(h) || !positive(b) || !positive(tw) || !positive(tf) || !nonNegative(r)) return kPositive;
        if (tee) {
            if (h <= tf) return "El canto del perfil no es compatible con el espesor del ala.";
        } else if (h <= 2 * tf) {
            return "El canto del perfil no es compatible con el espesor de las alas.";  // #131956
        }
        if (b <= tw) return "La anchura del perfil no es compatible con el espesor del alma.";  // #131957
        if (h < (tee ? tf : 2 * tf) + (tee ? 1 : 2) * r) return "El canto del perfil no es compatible con el espesor y el radio de acuerdo.";  // #131949
        if (b < tw + 2 * r) return "La anchura del perfil no es compatible con el espesor del alma y los radios de acuerdo de las alas.";  // #131955
        return {};
    }
    case ShapeKind::ISloped:
    case ShapeKind::Channel: {
        const bool channel = s.kind == ShapeKind::Channel;
        const double h = d[0], b = d[1], tw = d[2], tf = d[3], r1 = d[4], r2 = d[5], slope = d[6] / 100.0;
        if (!positive(h) || !positive(b) || !positive(tw) || !positive(tf) || !nonNegative(r1) || !nonNegative(r2) ||
            !nonNegative(d[6]))
            return kPositive;
        const double tip = tipThickness(tf, slope, channel ? b / 2 : b / 4);
        if (tip <= 0) return "El espesor del ala en su extremo no es positivo; revise la pendiente de las alas.";
        const double web = channel ? b - tw : (b - tw) / 2;  // length of the flange measured from the web
        const double root = tip + slope * web;
        if (!channel && b <= tw) return "La anchura del perfil no es compatible con el espesor del alma.";
        if (channel && b <= tw) return "La anchura del perfil no es compatible con el espesor del alma.";
        if (h <= 2 * root) return "El canto del perfil no es compatible con el espesor de las alas.";
        if (r1 > web || r1 > (h - 2 * root) / 2) return "El canto del perfil no es compatible con el espesor y el radio de acuerdo.";
        if (r2 > tip) return "La dimensión del ala no es compatible con el espesor y el radio de acuerdo.";
        return {};
    }
    case ShapeKind::AngleEqual:
    case ShapeKind::AngleUnequal: {
        const bool eq = s.kind == ShapeKind::AngleEqual;
        const double h = d[0], b = eq ? d[0] : d[1], t = eq ? d[1] : d[2], r1 = eq ? d[2] : d[3], r2 = eq ? d[3] : d[4];
        if (!positive(h) || !positive(b) || !positive(t) || !nonNegative(r1) || !nonNegative(r2)) return kPositive;
        if (t >= std::min(h, b)) return "El espesor debe ser menor que las dimensiones de los lados.";
        if (r1 > std::min(h, b) - t || r2 > t) return "La dimensión del ala no es compatible con el espesor y el radio de acuerdo.";
        return {};
    }
    case ShapeKind::FlatBar:
        return positive(d[0]) && positive(d[1]) ? std::string() : std::string(kPositive);
    case ShapeKind::RoundBar:
    case ShapeKind::SquareBar:
        return positive(d[0]) ? std::string() : std::string(kPositive);
    case ShapeKind::Generic: {
        for (double v : d)
            if (!nonNegative(v)) return "Las distancias al contorno envolvente no pueden ser negativas.";
        if (!positive(d[0] + d[1]) || !positive(d[2] + d[3])) return "El contorno envolvente de la sección debe tener dimensiones.";
        const SectionOverrides& t = s.tabulated;
        if (!t.area || !t.iy || !t.iz || *t.area <= 0 || *t.iy <= 0 || *t.iz <= 0)
            return "Debe introducir el área y las inercias de la sección genérica.";
        if (t.avy && t.avz && *t.avy > *t.area) return "Los valores de las áreas de cortante no pueden ser mayores que el valor del área de la sección.";  // #65239
        if (t.avz && *t.avz > *t.area) return "Los valores de las áreas de cortante no pueden ser mayores que el valor del área de la sección.";
        return {};
    }
    }
    return "Forma de perfil desconocida.";
}

// --- properties -------------------------------------------------------------------------------------------------------

namespace {

/// Closed-form shapes.
SectionProperties solid(const Section& s) {
    SectionProperties p;
    const std::vector<double>& d = s.dims;
    switch (s.kind) {
    case ShapeKind::ConcreteCircular:
    case ShapeKind::TimberRound:
    case ShapeKind::RoundBar: {
        const double D = d[0];
        p.area = kPi * D * D / 4;
        p.iy = p.iz = kPi * std::pow(D, 4) / 64;
        p.it = kPi * std::pow(D, 4) / 32;  // original: 2 * flexural inertia (paramsec_inercia_torsion_redondo)
        p.wplY = p.wplZ = D * D * D / 6;
        p.avy = p.avz = 0.9 * p.area;  // original: 0.9 * A (paramsec_area_cortante_redondo)
        p.yNeg = p.yPos = p.zNeg = p.zPos = D / 2;
        break;
    }
    case ShapeKind::ConcreteRectangular:
    case ShapeKind::TimberRectangular:
    case ShapeKind::TimberSquare:
    case ShapeKind::SquareBar:
    case ShapeKind::FlatBar: {
        const bool concreteOrTimber = isConcreteOrTimber(s.kind);
        // dimension order: FlatBar {h, b}; concrete / timber rectangle {b, h}; squares {a}
        const bool square = s.kind == ShapeKind::SquareBar || s.kind == ShapeKind::TimberSquare;
        const double h = concreteOrTimber && !square ? d[1] : d[0];
        const double b = square ? d[0] : (concreteOrTimber ? d[0] : d[1]);
        p.area = h * b;
        p.iy = b * h * h * h / 12;
        p.iz = h * b * b * b / 12;
        p.it = concreteOrTimber ? rectangleTorsionOriginal(std::max(b, h), std::min(b, h)) : rectangleTorsion(b, h);
        p.iw = std::pow(b * h, 3) / 144;  // thin rectangle: negligible but non-zero
        if (square || concreteOrTimber) p.iw = 0;
        p.wplY = b * h * h / 4;
        p.wplZ = h * b * b / 4;
        p.avy = p.avz = 5.0 / 6.0 * p.area;  // original: 5 A / 6 (paramsec_area_cortante_rectangulo), same in both directions
        p.yNeg = p.yPos = b / 2;
        p.zNeg = p.zPos = h / 2;
        break;
    }
    default:
        break;
    }
    p.yCg = p.yPos;
    p.zCg = p.zPos;
    return p;
}


// ---- Thin-walled open sections of the original (seccion.dll calconf_Calcular_CANAL_SIMPLE / _RIGIDIZADO) -----------------------------------
// The original builds the CENTRE LINE of the cold-formed channel as a polyline (creaconf_Puntos_CANAL_*): straight parts plus corner arcs of
// radius r + t/2, each arc discretised in 5 points INCLUDING its end points (arr_AnyadirArcoPunto2D_numPuntosArco(..., 5)), with the origin
// at the centre of the bounding box (web outer face at y = -b/2, flange tips at y = +b/2). Then (calconf_Calcular):
//  A = sum(L t); centroid of the polyline; Iy, Iz = integrals of z^2 / y^2 over each segment (the local t^3 term is not added);
//  It = sum(L) t^3 / 3 (open section, FUN_180006430); Avy = 0.83333 t sum|dy|, Avz = 0.83333 t sum|dz| (FUN_180005ff0);
//  Iw = integral of the normalised sectorial coordinate squared about the shear centre (FUN_180009a90, FUN_180006800).
// Verified against the (i) of UF-60x3 (A 3.30, Iyy 17.50, Izz 2.82, It 0.10, Avy = Avz 1.42, Iw 14.78, y = -6.1): all exact to 2 decimals.
struct ThinWall {
    double area = 0, yc = 0, zc = 0, iy = 0, iz = 0, it = 0, avy = 0, avz = 0, iw = 0;
};

void appendArc(Poly& pts, P2 centre, P2 from, P2 to, int count) {
    const double a0 = std::atan2(from.z - centre.z, from.y - centre.y);
    double da = std::atan2(to.z - centre.z, to.y - centre.y) - a0;
    while (da > kPi) da -= 2 * kPi;
    while (da < -kPi) da += 2 * kPi;
    const double radius = std::hypot(from.y - centre.y, from.z - centre.z);
    for (int i = 0; i < count; ++i) {
        const double a = a0 + da * i / (count - 1);
        pts.push_back({centre.y + radius * std::cos(a), centre.z + radius * std::sin(a)});
    }
}

/// Centre line of the simple channel (UF): h = outer web depth, b = outer flange width, t, r = inner corner radius. Order of the original:
/// upper flange tip, upper arc, web, lower arc, lower flange tip.
Poly channelCentreLine(double h, double b, double t, double r) {
    const double rr = r + t / 2, d1 = (t - b) / 2, d2 = (t - h) / 2, d3 = (h - t) / 2;
    Poly pts{{b / 2, d3}};
    appendArc(pts, {d1 + rr, d3 - rr}, {d1 + rr, d3}, {d1, d3 - rr}, 5);
    appendArc(pts, {d1 + rr, d2 + rr}, {d1, d2 + rr}, {d1 + rr, d2}, 5);
    pts.push_back({b / 2, d2});
    return pts;
}

/// Centre line of the lipped channel (CF): c = lip depth measured from the outer face (scsegcon_segmentos_canal_rigidizado_posicion_C).
Poly lippedChannelCentreLine(double h, double b, double c, double t, double r) {
    const double rr = r + t / 2, yR = (b - t) / 2, zb = -(h - t) / 2, zt = -zb;
    Poly pts{{yR, h / 2 - c}};
    appendArc(pts, {yR - rr, zt - rr}, {yR, zt - rr}, {yR - rr, zt}, 5);
    appendArc(pts, {-yR + rr, zt - rr}, {-yR + rr, zt}, {-yR, zt - rr}, 5);
    appendArc(pts, {-yR + rr, zb + rr}, {-yR, zb + rr}, {-yR + rr, zb}, 5);
    appendArc(pts, {yR - rr, zb + rr}, {yR - rr, zb}, {yR, zb + rr}, 5);
    pts.push_back({yR, c - h / 2});
    return pts;
}

ThinWall thinWallOpen(const Poly& pts, double t) {
    ThinWall w;
    const std::size_t n = pts.size() - 1;
    std::vector<double> len(n);
    double total = 0, sy = 0, sz = 0;
    for (std::size_t i = 0; i < n; ++i) {
        len[i] = std::hypot(pts[i + 1].y - pts[i].y, pts[i + 1].z - pts[i].z);
        total += len[i];
        sy += len[i] * (pts[i].y + pts[i + 1].y) / 2;
        sz += len[i] * (pts[i].z + pts[i + 1].z) / 2;
        w.avy += std::fabs(pts[i + 1].y - pts[i].y);
        w.avz += std::fabs(pts[i + 1].z - pts[i].z);
    }
    w.area = total * t;
    w.yc = sy / total;
    w.zc = sz / total;
    w.it = total * t * t * t / 3;
    w.avy *= 0.83333 * t;
    w.avz *= 0.83333 * t;
    for (std::size_t i = 0; i < n; ++i) {
        const double y0 = pts[i].y - w.yc, y1 = pts[i + 1].y - w.yc, z0 = pts[i].z - w.zc, z1 = pts[i + 1].z - w.zc;
        w.iy += t * len[i] * (z0 * z0 + z0 * z1 + z1 * z1) / 3;
        w.iz += t * len[i] * (y0 * y0 + y0 * y1 + y1 * y1) / 3;
    }
    // Sectorial coordinate about a pole; the shear centre is the pole for which the sectorial products with y and z vanish (they are affine
    // in the pole, so two probes plus the origin solve it exactly).
    struct Products {
        double iyw, izw;
    };
    const auto sectorial = [&](double py, double pz, std::vector<double>& om, double& mean) {
        om.assign(n + 1, 0.0);
        for (std::size_t i = 0; i < n; ++i)
            om[i + 1] = om[i] + (pts[i].y - py) * (pts[i + 1].z - pts[i].z) - (pts[i].z - pz) * (pts[i + 1].y - pts[i].y);
        mean = 0;
        for (std::size_t i = 0; i < n; ++i) mean += len[i] * t * (om[i] + om[i + 1]) / 2;
        mean /= w.area;
    };
    const auto products = [&](double py, double pz) {
        std::vector<double> om;
        double mean;
        sectorial(py, pz, om, mean);
        Products r{0, 0};
        for (std::size_t i = 0; i < n; ++i) {
            const double w0 = om[i] - mean, w1 = om[i + 1] - mean;
            const double y0 = pts[i].y - w.yc, y1 = pts[i + 1].y - w.yc, z0 = pts[i].z - w.zc, z1 = pts[i + 1].z - w.zc;
            r.iyw += t * len[i] * (2 * y0 * w0 + y0 * w1 + y1 * w0 + 2 * y1 * w1) / 6;
            r.izw += t * len[i] * (2 * z0 * w0 + z0 * w1 + z1 * w0 + 2 * z1 * w1) / 6;
        }
        return r;
    };
    const Products f0 = products(w.yc, w.zc), fy = products(w.yc + 1, w.zc), fz = products(w.yc, w.zc + 1);
    const double a = fy.iyw - f0.iyw, bb = fz.iyw - f0.iyw, c = fy.izw - f0.izw, d = fz.izw - f0.izw;
    const double det = a * d - bb * c;
    if (std::fabs(det) > 0) {
        const double dy = (-f0.iyw * d + bb * f0.izw) / det, dz = (-a * f0.izw + c * f0.iyw) / det;
        std::vector<double> om;
        double mean;
        sectorial(w.yc + dy, w.zc + dz, om, mean);
        for (std::size_t i = 0; i < n; ++i) {
            const double w0 = om[i] - mean, w1 = om[i + 1] - mean;
            w.iw += t * len[i] * (w0 * w0 + w0 * w1 + w1 * w1) / 3;
        }
    }
    return w;
}

}  // namespace

std::optional<SectionProperties> calculatedProperties(const Section& s) {
    if (!validateSection(s).empty() || s.kind == ShapeKind::Generic) return std::nullopt;
    if (s.kind == ShapeKind::BuiltUpDoubleT || s.kind == ShapeKind::AluminiumI) {
        // Doble T with sharp corners: the formulas of docs/research/ui/describir_barras_capturas_manuales.md 7.2 (exact to 2 decimals
        // against the original's dialogs: IPC 533x203x62.6 and I-50x80x6). The shear areas follow the coefficients DEDUCED from that
        // one IPC profile: Avy = 0.75 (2 b tf), Avz = 0.9 (h - 2 tf) tw.
        Section equivalent;
        equivalent.kind = ShapeKind::IParallel;
        equivalent.dims = {s.dims[0], s.dims[1], s.dims[2], s.dims[3], 0.0};
        auto p = calculatedProperties(equivalent);
        if (!p) return std::nullopt;
        p->avy = 1.5 * s.dims[1] * s.dims[3];
        p->avz = 0.9 * (s.dims[0] - 2 * s.dims[3]) * s.dims[2];
        return p;
    }
    if (isVariableDepth(s.kind)) return calculatedProperties(*sectionAt(s, 0.0));
    switch (s.kind) {
    case ShapeKind::RoundBar:
    case ShapeKind::SquareBar:
    case ShapeKind::FlatBar:
    case ShapeKind::ConcreteCircular:
    case ShapeKind::ConcreteRectangular:
    case ShapeKind::TimberRound:
    case ShapeKind::TimberSquare:
    case ShapeKind::TimberRectangular:
        return solid(s);
    default:
        break;
    }

    const Poly poly = contour(s);
    const Moments m = momentsOf(poly);
    if (!(m.area > 0)) return std::nullopt;
    SectionProperties p;
    p.area = m.area;
    p.iy = m.iy;
    p.iz = m.iz;
    double ymin = poly.front().y, ymax = ymin, zmin = poly.front().z, zmax = zmin;
    for (const P2& q : poly) {
        ymin = std::min(ymin, q.y); ymax = std::max(ymax, q.y);
        zmin = std::min(zmin, q.z); zmax = std::max(zmax, q.z);
    }
    p.yCg = m.cy - ymin;
    p.zCg = m.cz - zmin;
    p.yNeg = m.cy - ymin; p.yPos = ymax - m.cy;
    p.zNeg = m.cz - zmin; p.zPos = zmax - m.cz;
    p.wplY = plasticModulus(poly, m, 1);
    p.wplZ = plasticModulus(poly, m, 0);

    const std::vector<double>& d = s.dims;
    switch (s.kind) {
    case ShapeKind::IParallel:
    case ShapeKind::ISloped: {
        const double h = d[0], b = d[1], tw = d[2], tf = d[3], r = d[4];
        const bool sloped = s.kind == ShapeKind::ISloped;
        const double hw = h - 2 * tf;
        if (r > 0 || sloped)
            p.it = 2.0 / 3.0 * (b - 0.63 * tf) * std::pow(tf, 3) + hw * std::pow(tw, 3) / 3 + 2 * junctionTorsion(tw, tf, r);
        else
            p.it = 2.0 / 3.0 * b * std::pow(tf, 3) + hw * std::pow(tw, 3) / 3;
        p.iw = p.iz * std::pow(h - tf, 2) / 4;
        p.avz = std::max(p.area - 2 * b * tf + (tw + 2 * r) * tf, hw * tw);  // EN 1993-1-1 6.2.6(3)(a)
        p.avy = p.area - hw * tw;
        break;
    }
    case ShapeKind::Channel: {
        const double h = d[0], b = d[1], tw = d[2], tf = d[3], r1 = d[4];
        const double hw = h - 2 * tf;
        p.it = (hw * std::pow(tw, 3) + 2 * (b - tw / 2) * std::pow(tf, 3)) / 3;  // sum of rectangles (lower bound)
        const double bf = b - tw / 2, hc = h - tf;
        p.iw = tf * std::pow(bf, 3) * hc * hc / 12 * (3 * bf * tf + 2 * hc * tw) / (6 * bf * tf + hc * tw);
        p.avz = std::max(p.area - 2 * b * tf + (tw + r1) * tf, hw * tw);  // EN 1993-1-1 6.2.6(3)(b)
        p.avy = p.area - hw * tw;
        break;
    }
    case ShapeKind::ColdFormedU:
    case ShapeKind::ColdFormedC: {
        // Exact against the original: its own thin-walled polyline model (see thinWallOpen). The contour above only supplies the plastic moduli.
        const bool lipped = s.kind == ShapeKind::ColdFormedC;
        const double h = d[0], b = d[1], t = d[lipped ? 3 : 2], r = d[lipped ? 4 : 3];
        const ThinWall w = thinWallOpen(lipped ? lippedChannelCentreLine(h, b, d[2], t, r) : channelCentreLine(h, b, t, r), t);
        p.area = w.area;
        p.iy = w.iy;
        p.iz = w.iz;
        p.it = w.it;
        p.avy = w.avy;
        p.avz = w.avz;
        p.iw = w.iw;
        p.yCg = w.yc + b / 2;  // measured from the outer face of the web
        p.yNeg = p.yCg;
        p.yPos = b - p.yCg;
        break;
    }
    case ShapeKind::Tee: {
        const double h = d[0], b = d[1], tw = d[2], tf = d[3], r = d[4];
        p.it = (b * std::pow(tf, 3) + (h - tf / 2) * std::pow(tw, 3)) / 3 + (r > 0 ? junctionTorsion(tw, tf, r) : 0.0);
        p.iw = std::pow(tf * b, 3) / 144 + std::pow((h - tf / 2) * tw, 3) / 36;
        p.avz = 0.9 * (p.area - b * tf);  // EN 1993-1-1 6.2.6(3)(c)
        p.avy = p.area - (h - tf) * tw;
        break;
    }
    case ShapeKind::AngleEqual:
    case ShapeKind::AngleUnequal: {
        const bool eq = s.kind == ShapeKind::AngleEqual;
        const double h = d[0], b = eq ? d[0] : d[1], t = eq ? d[1] : d[2];
        p.it = (h + b - t) * std::pow(t, 3) / 3;  // two thin legs; fillets ignored
        p.iw = std::pow(t, 3) / 36 * (std::pow(h - t / 2, 3) + std::pow(b - t / 2, 3));
        p.avz = p.area - (b - t) * t;  // the leg parallel to z, corner included
        p.avy = p.area - (h - t) * t;
        break;
    }
    default:
        break;
    }
    return p;
}

Section defaultConcreteSection() {
    Section s;
    s.reference = "30x30";
    s.kind = ShapeKind::ConcreteRectangular;
    s.dims = {0.30, 0.30};
    return s;
}

std::optional<Section> sectionAt(const Section& s, double t) {
    if (!(t >= 0.0 && t <= 1.0) || !validateSection(s).empty()) return std::nullopt;
    if (!isVariableDepth(s.kind)) return s;
    Section out;
    out.reference = s.reference;
    out.kind = s.kind == ShapeKind::ConcreteRectangularVariable ? ShapeKind::ConcreteRectangular : ShapeKind::TimberRectangular;
    out.dims = {s.dims[0], s.dims[1] + (s.dims[2] - s.dims[1]) * t};
    return out;
}

std::optional<SectionProperties> propertiesAt(const Section& s, double t) {
    const auto at = sectionAt(s, t);
    if (!at) return std::nullopt;
    return effectiveProperties(*at);
}

std::optional<SectionProperties> effectiveProperties(const Section& s) {
    if (!validateSection(s).empty()) return std::nullopt;
    if (isVariableDepth(s.kind)) return propertiesAt(s, 0.0);
    SectionProperties p;
    if (s.kind == ShapeKind::Generic) {
        const SectionOverrides& t = s.tabulated;
        p.area = *t.area;
        p.iy = *t.iy;
        p.iz = *t.iz;
        p.yNeg = s.dims[0]; p.yPos = s.dims[1]; p.zNeg = s.dims[2]; p.zPos = s.dims[3];
        p.yCg = p.yNeg;
        p.zCg = p.zNeg;
    } else {
        const auto calc = calculatedProperties(s);
        if (!calc) return std::nullopt;
        p = *calc;
    }
    const SectionOverrides& t = s.tabulated;
    const auto pick = [](const std::optional<double>& tabulated, double& value) {
        if (tabulated) value = *tabulated;
    };
    pick(t.area, p.area);
    pick(t.iy, p.iy);
    pick(t.iz, p.iz);
    pick(t.it, p.it);
    pick(t.iw, p.iw);
    pick(t.wplY, p.wplY);
    pick(t.wplZ, p.wplZ);
    pick(t.avy, p.avy);
    pick(t.avz, p.avz);
    const double zMax = std::max(p.zNeg, p.zPos), yMax = std::max(p.yNeg, p.yPos);
    p.welY = zMax > 0 ? p.iy / zMax : 0.0;
    p.welZ = yMax > 0 ? p.iz / yMax : 0.0;
    return p;
}

std::optional<double> catalogueMassPerMetre(const Section& s) {
    if (!isCatalogueShape(s.kind)) return std::nullopt;
    const auto p = effectiveProperties(s);
    if (!p) return std::nullopt;
    const double density = s.kind == ShapeKind::AluminiumI ? 2700.0 : 7850.0;
    return p->area * density;
}

}  // namespace cc::core
