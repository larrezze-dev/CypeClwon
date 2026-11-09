#include "core/bar_load.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <map>

#include "core/geom3.h"

namespace cc::core {

namespace {

constexpr double kPi = 3.14159265358979323846;

std::string fixed(double v, int decimals) {
    char buf[64];
    std::snprintf(buf, sizeof buf, "%.*f", decimals, v);
    return buf;
}

bool finite3(const Point3& v) { return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z); }
bool nullVector(const Point3& v) { return !finite3(v) || geom::length(v) < 1e-12; }
bool validDirection(LoadDirection d) { return static_cast<int>(d) >= 0 && static_cast<int>(d) < kLoadDirectionCount; }
bool validType(BarLoadType t) { return static_cast<int>(t) >= 0 && static_cast<int>(t) < kBarLoadTypeCount; }

BarLoadIssue issue(BarLoadIssueKind kind, std::string message, std::optional<BarLoadField> field = std::nullopt) {
    return BarLoadIssue{kind, std::move(message), field};
}

}  // namespace

// --- type properties ---

std::string barLoadTypeName(BarLoadType type) {
    switch (type) {
        case BarLoadType::Point: return "Carga puntual";
        case BarLoadType::Uniform: return "Carga uniforme";
        case BarLoadType::Strip: return "Carga en faja";
        case BarLoadType::TriangularLeft: return "Carga triangular izquierda";
        case BarLoadType::TriangularRight: return "Carga triangular derecha";
        case BarLoadType::Trapezoidal: return "Carga trapecial";
        case BarLoadType::PointMoment: return "Momento puntual";
        case BarLoadType::VariableTemperature: return "Incremento de temperatura variable";
        case BarLoadType::UniformTemperature: return "Incremento de temperatura uniforme";
    }
    return {};
}
bool isForceLoad(BarLoadType t) { return t == BarLoadType::Point; }
bool isMomentLoad(BarLoadType t) { return t == BarLoadType::PointMoment; }
bool isTemperatureLoad(BarLoadType t) { return t == BarLoadType::VariableTemperature || t == BarLoadType::UniformTemperature; }
bool isPointLikeLoad(BarLoadType t) { return t == BarLoadType::Point || t == BarLoadType::PointMoment; }
bool hasTwoPositions(BarLoadType t) {
    return t == BarLoadType::Strip || t == BarLoadType::TriangularLeft || t == BarLoadType::TriangularRight || t == BarLoadType::Trapezoidal;
}
bool hasDirection(BarLoadType t) { return t != BarLoadType::UniformTemperature; }
bool allowsGlobalAxes(BarLoadType t) { return static_cast<int>(t) >= 0 && static_cast<int>(t) <= 6; }
bool hasTwoValues(BarLoadType t) { return t == BarLoadType::Trapezoidal || t == BarLoadType::VariableTemperature; }

BarLoadData barLoadDefaults(int loadCaseId) {
    BarLoadData d;
    d.loadCaseId = loadCaseId;
    return d;
}

BarLoadData barLoadToData(const BarLoad& load) { return static_cast<const BarLoadData&>(load); }

// --- queries ---

const BarLoad* findBarLoad(const BarLoads& loads, int id) {
    for (const BarLoad& l : loads.items)
        if (l.id == id) return &l;
    return nullptr;
}
std::vector<int> barLoadsOfBar(const BarLoads& loads, int barId) {
    std::vector<int> out;
    for (const BarLoad& l : loads.items)
        if (l.barId == barId) out.push_back(l.id);
    return out;
}
std::vector<int> barLoadsOfCase(const BarLoads& loads, int loadCaseId) {
    std::vector<int> out;
    for (const BarLoad& l : loads.items)
        if (l.loadCaseId == loadCaseId) out.push_back(l.id);
    return out;
}

// --- fields ---

double roundToDecimals(double value, int decimals) {
    const double f = std::pow(10.0, decimals);
    return std::round(value * f) / f;
}

BarLoadFieldSpec barLoadFieldSpec(const BarLoadData& data, BarLoadField field) {
    BarLoadFieldSpec s;
    const BarLoadType t = data.type;
    switch (field) {
        case BarLoadField::Value1:
        case BarLoadField::Value2: {
            const bool second = field == BarLoadField::Value2;
            if (t == BarLoadType::Point) {
                s = {"Valor", kMinBarLoadForceKn, kMaxBarLoadForceKn, 2, false};
            } else if (isTemperatureLoad(t)) {
                s = {t == BarLoadType::VariableTemperature ? (second ? "Valor 2" : "Valor 1") : "Valor", kMinBarTemperature, kMaxBarTemperature, 2, false};
            } else {
                const bool initial = t == BarLoadType::TriangularLeft || t == BarLoadType::TriangularRight || t == BarLoadType::Trapezoidal;
                s = {initial ? (second ? "Valor final" : "Valor inicial") : "Valor", kMinBarLoadLinear, kMaxBarLoadLinear, 3, false};
            }
            return s;
        }
        case BarLoadField::Start:
        case BarLoadField::End: {
            const bool first = field == BarLoadField::Start;
            s.label = isPointLikeLoad(t) ? "Distancia" : (first ? "Posición inicial" : "Posición final");
            s.min = 0.0;
            s.max = data.relative ? 1.0 : kMaxBarLoadAbsolutePosition;
            s.decimals = 3;
            s.blankUnitSpacing = data.relative;
            return s;
        }
        case BarLoadField::VectorX: return {"X", -kMaxBarLoadVector, kMaxBarLoadVector, 3, true};
        case BarLoadField::VectorY: return {"Y", -kMaxBarLoadVector, kMaxBarLoadVector, 3, true};
        case BarLoadField::VectorZ: return {"Z", -kMaxBarLoadVector, kMaxBarLoadVector, 3, true};
        case BarLoadField::AngleInPlane: return {"Ángulo en el plano 'XZ' local", -kMaxBarLoadAngle, kMaxBarLoadAngle, 3, false};
        case BarLoadField::AngleToPlane: return {"Ángulo con el plano 'XZ' local", -kMaxBarLoadAngle, kMaxBarLoadAngle, 3, false};
        case BarLoadField::TemperatureAngle: return {"Ángulo", -kMaxBarLoadAngle, kMaxBarLoadAngle, 3, false};
    }
    return s;
}

std::string barLoadRangeMessage(const BarLoadFieldSpec& spec) {
    const std::string lo = fixed(spec.min, spec.decimals), hi = fixed(spec.max, spec.decimals);
    if (spec.blankUnitSpacing) return "El valor de '" + spec.label + "' debe estar entre " + lo + "  y " + hi + " .";
    return "El valor de '" + spec.label + "' debe estar entre " + lo + " y " + hi + ".";
}
std::string barLoadNotNumberMessage() { return nodalLoadNotNumberMessage(); }

std::string barLoadZeroMessage() { return "El valor introducido no puede ser cero."; }
std::string barLoadBothZeroMessage() { return "Al menos uno de los valores de la carga debe ser distinto de cero."; }
std::string barLoadDirectionMessage() { return "La dirección introducida no es correcta."; }
std::string barLoadStartBeforeEndMessage() { return "El valor de 'Posición inicial' debe ser inferior al de 'Posición final'."; }
std::string barLoadsOutOfLengthMessage() { return "Se han borrado cargas incompatibles con la longitud de la barra."; }
std::string barLoadsMovedOverBarMessage() { return "Al mover una barra sobre otra se pierden sus cargas."; }
std::string barSelfWeightDisabledMessage() {
    return "Se ha desactivado la generación automática de las cargas de peso propio para esta barra.";
}

namespace {
bool inRange(const BarLoadFieldSpec& s, double v) {
    if (!std::isfinite(v)) return false;
    const double r = roundToDecimals(v, s.decimals);
    return r >= s.min - 1e-12 && r <= s.max + 1e-12;
}
}  // namespace

BarLoadFieldParse parseBarLoadField(const BarLoadData& data, BarLoadField field, const std::string& text) {
    BarLoadFieldParse out;
    const BarLoadFieldSpec spec = barLoadFieldSpec(data, field);
    double v = 0.0;
    if (!parsePlainDecimal(text, v)) {
        out.message = barLoadNotNumberMessage();
        return out;
    }
    if (!inRange(spec, v)) {
        out.message = barLoadRangeMessage(spec);
        return out;
    }
    out.ok = true;
    out.value = roundToDecimals(v, spec.decimals);
    return out;
}

// --- checks ---

BarLoadIssue checkBarLoadData(const BarLoadData& data, double lengthM) {
    const BarLoadType t = data.type;
    if (!validType(t)) return issue(BarLoadIssueKind::Error, "El tipo de carga no es válido.");
    const auto warn = [&](BarLoadField f, const BarLoadFieldSpec& s) { return issue(BarLoadIssueKind::Warning, barLoadRangeMessage(s), f); };

    // 1. Values (tab order: Valor(es) first).
    const BarLoadFieldSpec v1 = barLoadFieldSpec(data, BarLoadField::Value1);
    if (!inRange(v1, data.value1)) return warn(BarLoadField::Value1, v1);
    if (hasTwoValues(t)) {
        const BarLoadFieldSpec v2 = barLoadFieldSpec(data, BarLoadField::Value2);
        if (!inRange(v2, data.value2)) return warn(BarLoadField::Value2, v2);
    }
    if (t == BarLoadType::UniformTemperature && roundToDecimals(data.value1, 2) == 0.0)
        return issue(BarLoadIssueKind::Error, barLoadZeroMessage(), BarLoadField::Value1);
    if (t == BarLoadType::VariableTemperature && roundToDecimals(data.value1, 2) == 0.0 && roundToDecimals(data.value2, 2) == 0.0)
        return issue(BarLoadIssueKind::Error, barLoadBothZeroMessage(), BarLoadField::Value1);

    // 2. Positions.
    const bool one = isPointLikeLoad(t), two = hasTwoPositions(t);
    if (one || two) {
        const BarLoadFieldSpec sp = barLoadFieldSpec(data, BarLoadField::Start);
        if (!inRange(sp, data.start)) return warn(BarLoadField::Start, sp);
        BarLoadFieldSpec ep;
        if (two) {
            ep = barLoadFieldSpec(data, BarLoadField::End);
            if (!inRange(ep, data.end)) return warn(BarLoadField::End, ep);
        }
        if (!data.relative && std::isfinite(lengthM) && lengthM > 0.0) {
            // Checked on Aceptar on the unrounded value (measured: 4.0001 fails with L = 4, 4.000 passes).
            BarLoadFieldSpec lim = sp;
            lim.max = lengthM;
            if (data.start > lengthM + 1e-9) return warn(BarLoadField::Start, lim);
            if (two) {
                lim = ep;
                lim.max = lengthM;
                if (data.end > lengthM + 1e-9) return warn(BarLoadField::End, lim);
            }
        }
        if (two && !(roundToDecimals(data.start, 3) < roundToDecimals(data.end, 3)))
            return issue(BarLoadIssueKind::Error, barLoadStartBeforeEndMessage(), BarLoadField::Start);
    }

    // 3. Axes and direction.
    if (!hasDirection(t)) return {};
    if (t == BarLoadType::VariableTemperature) {
        const int td = static_cast<int>(data.temperatureDirection);
        if (td < 0 || td > 2) return issue(BarLoadIssueKind::Warning, barLoadDirectionMessage());
        if (data.temperatureDirection == TemperatureDirection::Angle) {
            const BarLoadFieldSpec a = barLoadFieldSpec(data, BarLoadField::TemperatureAngle);
            if (!inRange(a, data.temperatureAngle)) return warn(BarLoadField::TemperatureAngle, a);
        }
        return {};
    }
    if (!validDirection(data.direction)) return issue(BarLoadIssueKind::Warning, barLoadDirectionMessage());
    if (data.direction == LoadDirection::UserVector) {
        const bool global = data.axes == BarLoadAxes::Global && allowsGlobalAxes(t);
        if (global) {
            const std::pair<BarLoadField, double> comps[] = {
                {BarLoadField::VectorX, data.userVector.x}, {BarLoadField::VectorY, data.userVector.y}, {BarLoadField::VectorZ, data.userVector.z}};
            for (const auto& [f, v] : comps) {
                const BarLoadFieldSpec s = barLoadFieldSpec(data, f);
                if (!inRange(s, v)) return warn(f, s);
            }
            const Point3 r{roundToDecimals(data.userVector.x, 3), roundToDecimals(data.userVector.y, 3), roundToDecimals(data.userVector.z, 3)};
            if (nullVector(r)) return issue(BarLoadIssueKind::Warning, barLoadDirectionMessage(), BarLoadField::VectorX);
        } else {
            const BarLoadFieldSpec a = barLoadFieldSpec(data, BarLoadField::AngleInPlane), b = barLoadFieldSpec(data, BarLoadField::AngleToPlane);
            if (!inRange(a, data.angleInPlaneXz)) return warn(BarLoadField::AngleInPlane, a);
            if (!inRange(b, data.angleToPlaneXz)) return warn(BarLoadField::AngleToPlane, b);
        }
    }
    return {};
}

BarLoadData normaliseBarLoadData(BarLoadData d) {
    const BarLoadData defaults;
    const BarLoadType t = d.type;
    if (!allowsGlobalAxes(t)) d.axes = BarLoadAxes::Local;
    d.value1 = roundToDecimals(d.value1, barLoadFieldSpec(d, BarLoadField::Value1).decimals);
    d.value2 = hasTwoValues(t) ? roundToDecimals(d.value2, barLoadFieldSpec(d, BarLoadField::Value2).decimals) : 0.0;
    if (isPointLikeLoad(t) || hasTwoPositions(t)) {
        d.start = roundToDecimals(d.start, 3);
        d.end = hasTwoPositions(t) ? roundToDecimals(d.end, 3) : 1.0;
    } else {
        d.relative = true;
        d.start = 0.0;
        d.end = 1.0;
    }
    if (t == BarLoadType::VariableTemperature) {
        d.temperatureAngle = d.temperatureDirection == TemperatureDirection::Angle ? roundToDecimals(d.temperatureAngle, 3) : 0.0;
    } else {
        d.temperatureDirection = defaults.temperatureDirection;
        d.temperatureAngle = 0.0;
    }
    if (!hasDirection(t) || t == BarLoadType::VariableTemperature) {
        d.direction = defaults.direction;
        d.userVector = defaults.userVector;
        d.angleInPlaneXz = d.angleToPlaneXz = 0.0;
    } else if (d.direction != LoadDirection::UserVector) {
        d.userVector = defaults.userVector;
        d.angleInPlaneXz = d.angleToPlaneXz = 0.0;
    } else if (d.axes == BarLoadAxes::Global) {
        d.userVector = {roundToDecimals(d.userVector.x, 3), roundToDecimals(d.userVector.y, 3), roundToDecimals(d.userVector.z, 3)};
        d.angleInPlaneXz = d.angleToPlaneXz = 0.0;
    } else {
        d.userVector = defaults.userVector;
        d.angleInPlaneXz = roundToDecimals(d.angleInPlaneXz, 3);
        d.angleToPlaneXz = roundToDecimals(d.angleToPlaneXz, 3);
    }
    if (t == BarLoadType::UniformTemperature) d.axes = BarLoadAxes::Local;
    return d;
}

LinearLoadShape normaliseLinearLoad(double startM, double endM, double value1, double value2, double lengthM) {
    LinearLoadShape out;
    const double a = std::fabs(value1), b = std::fabs(value2);
    const bool null1 = a <= kBarLoadValueTolerance, null2 = b <= kBarLoadValueTolerance;
    if (null1 && null2) {
        out.type = BarLoadType::Uniform;
        return out;
    }
    if (null1 || null2) {
        out.type = null2 ? BarLoadType::TriangularLeft : BarLoadType::TriangularRight;
        out.value1 = null2 ? a : b;
        return out;
    }
    if (std::fabs(a - b) <= kBarLoadValueTolerance) {
        const bool whole = startM <= kBarLoadPositionToleranceM && endM + kBarLoadPositionToleranceM >= lengthM;
        out.type = whole ? BarLoadType::Uniform : BarLoadType::Strip;
        out.value1 = a;
        return out;
    }
    out.type = BarLoadType::Trapezoidal;
    out.value1 = a;
    out.value2 = b;
    return out;
}

// --- geometry ---

std::optional<BarFrame> barLocalAxes(const StructuralModel& model, int barId) {
    const Bar* bar = model.findBar(barId);
    if (!bar) return std::nullopt;
    const Node* a = model.findNode(bar->startNode);
    const Node* b = model.findNode(bar->endNode);
    if (!a || !b) return std::nullopt;
    BarFrame f;
    f.origin = a->position;
    const Point3 axis = geom::sub(b->position, a->position);
    f.length = geom::length(axis);
    if (f.length < 1e-12) return std::nullopt;
    f.x = geom::unit(axis);
    if (bar->xAxisInverted) f.x = geom::scale(f.x, -1.0);
    Point3 z0;
    if (std::fabs(f.x.z) > 1.0 - 1e-9) {
        z0 = {1, 0, 0};
    } else {
        z0 = geom::unit(geom::sub(Point3{0, 0, 1}, geom::scale(f.x, f.x.z)));
    }
    const Point3 y0 = geom::cross(z0, f.x);
    const double r = bar->rollAngleDeg * kPi / 180.0;
    f.y = geom::add(geom::scale(y0, std::cos(r)), geom::scale(z0, std::sin(r)));
    f.z = geom::sub(geom::scale(z0, std::cos(r)), geom::scale(y0, std::sin(r)));
    return f;
}

Point3 barLoadDirection(const BarLoadData& d, const BarFrame& f) {
    using namespace geom;
    if (!hasDirection(d.type)) return {};
    if (d.type == BarLoadType::VariableTemperature) {
        switch (d.temperatureDirection) {
            case TemperatureDirection::PlusY: return f.y;
            case TemperatureDirection::PlusZ: return f.z;
            case TemperatureDirection::Angle: {
                const double a = d.temperatureAngle * kPi / 180.0;
                return unit(add(scale(f.z, std::cos(a)), scale(f.y, std::sin(a))));
            }
        }
        return {};
    }
    const bool global = d.axes == BarLoadAxes::Global && allowsGlobalAxes(d.type);
    const Point3 gx{1, 0, 0}, gy{0, 1, 0}, gz{0, 0, 1};
    const Point3 &ex = global ? gx : f.x, &ey = global ? gy : f.y, &ez = global ? gz : f.z;
    switch (d.direction) {
        case LoadDirection::PlusX: return ex;
        case LoadDirection::PlusY: return ey;
        case LoadDirection::PlusZ: return ez;
        case LoadDirection::MinusX: return scale(ex, -1.0);
        case LoadDirection::MinusY: return scale(ey, -1.0);
        case LoadDirection::MinusZ: return scale(ez, -1.0);
        case LoadDirection::UserVector: {
            if (global) return nullVector(d.userVector) ? Point3{} : unit(d.userVector);
            const double al = d.angleInPlaneXz * kPi / 180.0, be = d.angleToPlaneXz * kPi / 180.0;
            const Point3 inPlane = add(scale(f.x, std::cos(al)), scale(f.z, std::sin(al)));
            return unit(add(scale(inPlane, std::cos(be)), scale(f.y, std::sin(be))));
        }
    }
    return {};
}

BarLoadExtent barLoadExtent(const BarLoadData& d, double L) {
    BarLoadExtent e;
    const auto pos = [&](double v) { return std::clamp(d.relative ? v * L : v, 0.0, L); };
    switch (d.type) {
        case BarLoadType::Point:
        case BarLoadType::PointMoment:
            e.start = e.end = pos(d.start);
            e.valueStart = e.valueEnd = d.value1;
            break;
        case BarLoadType::Uniform:
        case BarLoadType::UniformTemperature:
            e.end = L;
            e.valueStart = e.valueEnd = d.value1;
            break;
        case BarLoadType::VariableTemperature:
            e.end = L;
            e.valueStart = d.value1;
            e.valueEnd = d.value2;
            break;
        case BarLoadType::Strip:
            e.start = pos(d.start);
            e.end = pos(d.end);
            e.valueStart = e.valueEnd = d.value1;
            break;
        case BarLoadType::TriangularLeft:
            e.start = pos(d.start);
            e.end = pos(d.end);
            e.valueStart = d.value1;
            break;
        case BarLoadType::TriangularRight:
            e.start = pos(d.start);
            e.end = pos(d.end);
            e.valueEnd = d.value1;
            break;
        case BarLoadType::Trapezoidal:
            e.start = pos(d.start);
            e.end = pos(d.end);
            e.valueStart = d.value1;
            e.valueEnd = d.value2;
            break;
    }
    return e;
}

LoadScaleType barLoadScaleType(BarLoadType type) {
    switch (type) {
        case BarLoadType::Point: return LoadScaleType::Punctual;
        case BarLoadType::Uniform: return LoadScaleType::Uniform;
        case BarLoadType::Strip: return LoadScaleType::Strip;
        case BarLoadType::TriangularLeft:
        case BarLoadType::TriangularRight: return LoadScaleType::Triangular;
        case BarLoadType::Trapezoidal: return LoadScaleType::Trapezoidal;
        case BarLoadType::PointMoment: return LoadScaleType::PointMoment;
        case BarLoadType::VariableTemperature:
        case BarLoadType::UniformTemperature: return LoadScaleType::Temperature;
    }
    return LoadScaleType::Uniform;
}

double barLoadDrawSizeM(const LoadScales& scales, const BarLoadData& d, double value, double maxValue) {
    const double v = std::fabs(value);
    if (scales.mode == LoadScaleMode::Fixed) {
        const double m = std::fabs(maxValue);
        return m > 0.0 ? scales.fixedSizeM * v / m : scales.fixedSizeM;
    }
    return kLoadMetresPerKn * v * loadScaleOfCase(scales, d.loadCaseId) * scales.perType[static_cast<std::size_t>(barLoadScaleType(d.type))];
}

std::string barLoadValueLabel(double value) {
    std::string s = fixed(std::fabs(value) < 0.0005 ? 0.0 : value, 3);
    while (!s.empty() && s.back() == '0') s.pop_back();
    if (!s.empty() && s.back() == '.') s.pop_back();
    return s.empty() || s == "-" ? "0" : s;
}

std::string barLoadLabel(const BarLoadData& d) {
    if (d.type == BarLoadType::UniformTemperature) return barLoadValueLabel(d.value1) + "/" + barLoadValueLabel(d.value1);
    if (d.type == BarLoadType::VariableTemperature) return barLoadValueLabel(d.value1) + "/" + barLoadValueLabel(d.value2);
    return barLoadValueLabel(std::fabs(d.value1));
}

namespace {
Point3 lerp(const Point3& a, const Point3& b, double t) { return geom::add(a, geom::scale(geom::sub(b, a), t)); }
Point3 offset(const Point3& p, const Point3& dir, double h) { return geom::add(p, geom::scale(dir, h)); }
}  // namespace

BarLoadDrawing barLoadDrawing(const BarLoadData& d, const BarFrame& f, const LoadScales& scales) {
    using namespace geom;
    BarLoadDrawing out;
    out.type = d.type;
    if (!(f.length > 0.0)) return out;
    const BarLoadExtent e = barLoadExtent(d, f.length);
    const auto at = [&](double x) { return add(f.origin, scale(f.x, x)); };

    if (isPointLikeLoad(d.type)) {
        const Point3 dir = barLoadDirection(d, f);
        if (nullVector(dir)) return out;
        const Point3 p = at(e.start);
        out.sizeM = barLoadDrawSizeM(scales, d, d.value1, d.value1);
        if (d.type == BarLoadType::Point) {
            out.arrow = {offset(p, dir, -out.sizeM), p};   // nodal convention: tip on the bar, drawn from the side the load comes from
            out.labels.push_back({lerp(out.arrow.first, p, 0.5), barLoadLabel(d)});
        } else {
            Point3 n1 = unit(cross(dir, f.x));
            if (nullVector(n1)) n1 = unit(cross(dir, f.y));
            const Point3 n2 = cross(dir, n1);
            constexpr int kSegments = 16;
            for (int i = 0; i <= kSegments; ++i) {
                const double a = kPi * i / kSegments;
                out.arc.push_back(add(p, scale(add(scale(n1, std::cos(a)), scale(n2, std::sin(a))), out.sizeM)));
            }
            out.labels.push_back({out.arc[kSegments / 2], barLoadLabel(d)});
        }
        out.valid = true;
        return out;
    }

    Point3 dir = isTemperatureLoad(d.type) ? (d.type == BarLoadType::UniformTemperature ? f.z : barLoadDirection(d, f)) : barLoadDirection(d, f);
    if (nullVector(dir)) return out;
    if (!(e.end - e.start > 1e-9)) return out;
    const double maxV = std::max(std::fabs(e.valueStart), std::fabs(e.valueEnd));
    double hs, he;
    if (isTemperatureLoad(d.type)) {   // constant width along the bar (measured: no wedge)
        hs = he = barLoadDrawSizeM(scales, d, maxV, maxV);
    } else {
        hs = barLoadDrawSizeM(scales, d, e.valueStart, maxV);
        he = barLoadDrawSizeM(scales, d, e.valueEnd, maxV);
    }
    out.sizeM = std::max(hs, he);
    const Point3 a = at(e.start), b = at(e.end);
    out.outline = {a, b, offset(b, dir, he), offset(a, dir, hs)};
    const int n = std::max(1, static_cast<int>(std::ceil((e.end - e.start) / kBarLoadTickSpacingM - 1e-9)));
    for (int i = 0; i <= n; ++i) {
        const double t = static_cast<double>(i) / n;
        const Point3 base = lerp(a, b, t);
        out.ticks.push_back({base, offset(base, dir, hs + (he - hs) * t)});
    }
    const Point3& outerEnd = out.outline[2];
    const Point3& outerStart = out.outline[3];
    switch (d.type) {
        case BarLoadType::Uniform: out.labels.push_back({lerp(outerStart, outerEnd, 0.5), barLoadLabel(d)}); break;
        case BarLoadType::Strip:
            out.labels.push_back({outerStart, barLoadLabel(d)});
            out.labels.push_back({outerEnd, barLoadLabel(d)});
            break;
        case BarLoadType::TriangularLeft: out.labels.push_back({outerStart, barLoadValueLabel(d.value1)}); break;
        case BarLoadType::TriangularRight: out.labels.push_back({outerEnd, barLoadValueLabel(d.value1)}); break;
        case BarLoadType::Trapezoidal:
            out.labels.push_back({outerStart, barLoadValueLabel(d.value1)});
            out.labels.push_back({outerEnd, barLoadValueLabel(d.value2)});
            break;
        default: out.labels.push_back({lerp(outerStart, outerEnd, 0.5), barLoadLabel(d)}); break;  // temperatures: "T1/T2"
    }
    out.valid = true;
    return out;
}

// --- edits on the data ---

void applyBarLoadsBarsDeleted(BarLoads& loads, const std::vector<int>& barIds) {
    const std::set<int> gone(barIds.begin(), barIds.end());
    loads.items.erase(std::remove_if(loads.items.begin(), loads.items.end(), [&](const BarLoad& l) { return gone.count(l.barId) != 0; }),
                      loads.items.end());
    for (int id : gone) loads.selfWeightOff.erase(id);
}

void applyBarLoadsCaseDeleted(BarLoads& loads, int loadCaseId) {
    loads.items.erase(std::remove_if(loads.items.begin(), loads.items.end(), [&](const BarLoad& l) { return l.loadCaseId == loadCaseId; }),
                      loads.items.end());
}

namespace {

struct Segment {
    int barId;
    double s0, s1;
};

/// Linear value of the extent at x.
double valueAt(const BarLoadExtent& e, double x) {
    const double span = e.end - e.start;
    if (span <= 1e-12) return e.valueStart;
    return e.valueStart + (e.valueEnd - e.valueStart) * (x - e.start) / span;
}

/// Does the load still fit a bar of length L? Relative loads always do (REA: despl_final - 1e-3 < longitud).
bool fits(const BarLoad& l, double L) {
    if (l.relative) return true;
    if (isPointLikeLoad(l.type)) return l.start - kBarLoadPositionToleranceM < L;
    if (hasTwoPositions(l.type)) return l.end - kBarLoadPositionToleranceM < L;
    return true;
}

/// Pieces of `load` (defined on a bar of length `oldLength`) over the stretch `seg`.
bool pieceOn(const BarLoad& load, double oldLength, const Segment& seg, bool first, BarLoad& piece) {
    piece = load;
    piece.barId = seg.barId;
    const double len = seg.s1 - seg.s0;
    const BarLoadType t = load.type;
    if (t == BarLoadType::Uniform || isTemperatureLoad(t)) return true;
    const BarLoadExtent e = barLoadExtent(load, oldLength);
    const auto toField = [&](double x) { return load.relative ? x / len : x; };
    if (isPointLikeLoad(t)) {
        const double x = e.start;
        const bool in = first ? x <= seg.s1 + 1e-9 : (x > seg.s0 + 1e-9 && x <= seg.s1 + 1e-9);
        if (!in) return false;
        piece.start = toField(std::clamp(x - seg.s0, 0.0, len));
        return true;
    }
    const double a = std::max(e.start, seg.s0), b = std::min(e.end, seg.s1);
    if (b - a < kBarLoadPositionToleranceM) return false;
    const auto clean = [](double v) { return std::fabs(v) < 0.5 * kMinBarLoadLinear ? 0.0 : v; };
    const LinearLoadShape shape = normaliseLinearLoad(a - seg.s0, b - seg.s0, clean(valueAt(e, a)), clean(valueAt(e, b)), len);
    if (std::fabs(shape.value1) < 0.5 * kMinBarLoadLinear) return false;
    piece.type = shape.type;
    piece.value1 = shape.value1;
    piece.value2 = shape.value2;
    piece.start = toField(a - seg.s0);
    piece.end = toField(b - seg.s0);
    return true;
}

}  // namespace

BarLoads reconcileBarLoads(const BarLoads& loads, const StructuralModel& before, const StructuralModel& after, BarLoadReconcileInfo* info) {
    BarLoads out = loads;
    if (loads.empty()) return out;
    BarLoadReconcileInfo local;
    BarLoadReconcileInfo& stats = info ? *info : local;

    // Divided bars (same id and start node, new end node, followed by new bars up to the old end node), like the other per-bar data.
    std::map<int, std::vector<int>> chains;
    for (const Bar& b : after.bars) {
        const Bar* old = before.findBar(b.id);
        if (!old || old->startNode != b.startNode || old->endNode == b.endNode) continue;
        std::vector<int> chain;
        std::set<int> seen;
        int current = b.endNode;
        while (current != old->endNode) {
            int next = 0;
            for (const Bar& c : after.bars)
                if (!before.findBar(c.id) && c.startNode == current && !seen.count(c.id)) {
                    next = c.id;
                    break;
                }
            if (next == 0) break;
            seen.insert(next);
            chain.push_back(next);
            current = after.findBar(next)->endNode;
        }
        if (!chain.empty() && current == old->endNode) chains[b.id] = std::move(chain);
    }

    out.items.clear();
    for (const BarLoad& load : loads.items) {
        if (!after.findBar(load.barId)) continue;   // the bar is gone: its loads go with it, silently
        const double newLen = after.barLength(load.barId);
        const double oldLen = before.findBar(load.barId) ? before.barLength(load.barId) : newLen;
        const auto chain = chains.find(load.barId);
        if (chain == chains.end()) {
            if (std::fabs(newLen - oldLen) > 1e-9 && !fits(load, newLen)) {
                ++stats.removedOutOfLength;
                continue;
            }
            out.items.push_back(load);
            continue;
        }
        std::vector<Segment> segs{{load.barId, 0.0, newLen}};
        for (int id : chain->second) {
            const double s0 = segs.back().s1;
            segs.push_back({id, s0, s0 + after.barLength(id)});
        }
        bool idUsed = false;
        int produced = 0;
        for (std::size_t i = 0; i < segs.size(); ++i) {
            BarLoad piece;
            if (!pieceOn(load, oldLen, segs[i], i == 0, piece)) continue;
            static_cast<BarLoadData&>(piece) = normaliseBarLoadData(piece);
            if (hasTwoPositions(piece.type) && !(piece.start < piece.end)) continue;
            piece.id = idUsed ? out.nextId++ : load.id;
            idUsed = true;
            ++produced;
            out.items.push_back(piece);
        }
        if (produced > 1) stats.split += produced - 1;
    }
    for (const auto& [barId, chain] : chains)
        if (loads.selfWeightOff.count(barId))
            for (int id : chain) out.selfWeightOff.insert(id);
    for (auto it = out.selfWeightOff.begin(); it != out.selfWeightOff.end();) it = after.findBar(*it) ? std::next(it) : out.selfWeightOff.erase(it);
    return out;
}

std::string validateBarLoads(const BarLoads& loads, const LoadCaseSet& cases) {
    std::set<int> ids;
    for (const BarLoad& l : loads.items) {
        const std::string name = "Carga de barra " + std::to_string(l.id);
        if (l.id < 1 || l.id >= loads.nextId) return "Carga de barra con identificador no válido.";
        if (!ids.insert(l.id).second) return "Carga de barra con identificador repetido.";
        if (l.barId < 1) return name + " con barra no válida.";
        if (!findLoadCase(cases, l.loadCaseId)) return name + " en una hipótesis inexistente.";
        const BarLoadData data = barLoadToData(l);
        if (!validType(l.type) || normaliseBarLoadData(data) != data) return name + " con datos no canónicos.";
        if (!checkBarLoadData(data, 0.0).ok()) return name + " con datos no válidos.";
    }
    for (int id : loads.selfWeightOff)
        if (id < 1) return "Peso propio desactivado en una barra no válida.";
    return {};
}

}  // namespace cc::core
