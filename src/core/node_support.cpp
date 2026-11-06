#include "core/node_support.h"

#include <algorithm>
#include <cmath>

namespace cc::core {

namespace {

constexpr double kAxisTolerance = 1e-9;

bool finite3(const Point3& p) { return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z); }
double norm(const Point3& p) { return std::sqrt(p.x * p.x + p.y * p.y + p.z * p.z); }

/// Index (0..2) of the global axis the vector is parallel to (either sense), or -1 when it is skew or zero.
int parallelAxis(const Point3& v) {
    const double n = norm(v);
    if (!finite3(v) || n < 1e-12) return -1;
    const double c[3] = {std::abs(v.x) / n, std::abs(v.y) / n, std::abs(v.z) / n};
    for (int i = 0; i < 3; ++i)
        if (c[i] >= 1.0 - kAxisTolerance) return i;
    return -1;
}

/// Applies "free along `axis`" (line) or "blocked along `axis`" (plane normal) to the translations.
void applyLine(Restraint& r, int axis) {
    for (int i = 0; i < 3; ++i) r.fixed[i] = (i != axis);
}
void applyPlane(Restraint& r, int axis) {
    for (int i = 0; i < 3; ++i) r.fixed[i] = (i == axis);
}

void applyUserVector(Restraint& r, SupportKind kind, const Point3& v) {
    const double n = norm(v);
    if (!finite3(v) || n < 1e-12) return;  // invalid vector: reported by checkExternalSupport, no restriction here
    const int axis = parallelAxis(v);
    if (axis >= 0) {
        if (kind == SupportKind::Line) applyLine(r, axis);
        else applyPlane(r, axis);
        return;
    }
    r.oblique = kind == SupportKind::Line ? Restraint::Oblique::LineFree : Restraint::Oblique::PlaneFree;
    r.direction = {v.x / n, v.y / n, v.z / n};
}

}  // namespace

const std::array<ExternalSupportType, kExternalSupportTypeCount>& allExternalSupportTypes() {
    static const std::array<ExternalSupportType, kExternalSupportTypeCount> all = [] {
        std::array<ExternalSupportType, kExternalSupportTypeCount> a{};
        for (int i = 0; i < kExternalSupportTypeCount; ++i) a[i] = static_cast<ExternalSupportType>(i);
        return a;
    }();
    return all;
}

SupportTypeInfo supportTypeInfo(ExternalSupportType type) {
    const int v = supportTypeIndex(type);
    SupportTypeInfo info;
    switch (type) {
        case ExternalSupportType::Free: info.kind = SupportKind::Free; return info;
        case ExternalSupportType::Pinned: info.kind = SupportKind::Pinned; return info;
        case ExternalSupportType::Generic: info.kind = SupportKind::Generic; return info;
        case ExternalSupportType::Fixed: info.kind = SupportKind::Fixed; info.rotationsFixed = true; return info;
        default: break;
    }
    const bool fixedFamily = v <= 11;
    const int base = fixedFamily ? 4 : 12;
    const int k = v - base;  // 0..7: line x,y,z,user, plane xy,xz,yz,user
    info.rotationsFixed = fixedFamily;
    if (k < 4) {
        info.kind = SupportKind::Line;
        if (k < 3) info.axis = k; else info.userVector = true;
    } else {
        info.kind = SupportKind::Plane;
        // Plane XY blocks Z, XZ blocks Y, YZ blocks X: the axis stored is the BLOCKED (normal) one.
        const int p = k - 4;
        if (p < 3) info.axis = 2 - p; else info.userVector = true;
    }
    return info;
}

bool Restraint::any() const {
    return oblique != Oblique::None || std::any_of(fixed.begin(), fixed.end(), [](bool b) { return b; });
}

Restraint restraintOf(const ExternalSupport& s) {
    Restraint r;
    const SupportTypeInfo info = supportTypeInfo(s.type);
    switch (info.kind) {
        case SupportKind::Free: return r;
        case SupportKind::Pinned:
        case SupportKind::Fixed:
            for (int i = 0; i < 3; ++i) r.fixed[i] = true;
            break;
        case SupportKind::Line:
            if (info.userVector) applyUserVector(r, SupportKind::Line, s.userVector);
            else applyLine(r, info.axis);
            break;
        case SupportKind::Plane:
            if (info.userVector) applyUserVector(r, SupportKind::Plane, s.userVector);
            else applyPlane(r, info.axis);
            break;
        case SupportKind::Generic: {
            const GenericSupportData& g = s.generic;
            if (g.mode == GenericDisplacementMode::Independent) {
                for (int i = 0; i < 3; ++i) {
                    r.fixed[i] = g.translations[i] != DofMode::Free;
                    r.elastic[i] = g.translations[i] == DofMode::Elastic;
                }
            } else {
                applyUserVector(r, g.mode == GenericDisplacementMode::OnLine ? SupportKind::Line : SupportKind::Plane, g.vector);
            }
            for (int i = 0; i < 3; ++i) {
                r.fixed[3 + i] = g.rotations[i] != DofMode::Free;
                r.elastic[3 + i] = g.rotations[i] == DofMode::Elastic;
            }
            return r;
        }
    }
    if (info.rotationsFixed)
        for (int i = 3; i < 6; ++i) r.fixed[i] = true;
    if (s.elastic)
        for (int i = 0; i < 6; ++i) r.elastic[i] = r.fixed[i];
    return r;
}

// --- queries ---------------------------------------------------------------------------------------------------

const ExternalSupport* externalSupportOf(const StructuralModel& model, const NodeSupports& supports, int nodeId) {
    if (!model.findNode(nodeId)) return nullptr;
    const auto it = supports.external.find(nodeId);
    if (it == supports.external.end() || it->second.type == ExternalSupportType::Free) return nullptr;
    return &it->second;
}

bool isSupported(const StructuralModel& model, const NodeSupports& supports, int nodeId) {
    const ExternalSupport* s = externalSupportOf(model, supports, nodeId);
    return s && restraintOf(*s).any();
}

std::array<bool, 6> constrainedDofs(const StructuralModel& model, const NodeSupports& supports, int nodeId) {
    const ExternalSupport* s = externalSupportOf(model, supports, nodeId);
    return s ? restraintOf(*s).fixed : std::array<bool, 6>{};
}

InternalLink internalLinkOf(const NodeSupports& supports, int nodeId) {
    const auto it = supports.internal.find(nodeId);
    return it == supports.internal.end() ? InternalLink{} : it->second;
}

int elementCountAtNode(const StructuralModel& model, int nodeId) {
    return static_cast<int>(model.barsUsingNode(nodeId).size() + model.shellsUsingNode(nodeId).size());
}

// --- validation ------------------------------------------------------------------------------------------------

std::string supportMessage(int id) {
    switch (id) {
        case 65006: return "Debe coaccionar, al menos, un grado de libertad del nudo.";
        case 65010:
            return "El valor introducido para la constante de apoyo elástico (%s) coincide con el valor por defecto. Recuerde que debe asignar "
                   "un valor conforme al grado de empotramiento que posee la vinculación exterior en el nudo. ¿Desea continuar?";
        case 65465: return "Se ha introducido más de un conjunto de desplazamientos prescritos en la misma hipótesis de carga.";
        case 65466: return "Se ha introducido un conjunto de desplazamientos prescritos con todos los valores nulos.";
        case 65467:
            return "Algunos desplazamientos prescritos introducidos son incompatibles con la vinculación exterior actual en el nudo. Los valores "
                   "de los desplazamientos correspondientes a los grados de libertad no coaccionados no se tendrán en cuenta.";
        case 65473:
            return "Ha seleccionado nudos con distinto número de piezas. La configuración de la vinculación interior genérica editada sólo se "
                   "aplicará a nudos con el mismo número de piezas.";
        default: return {};
    }
}

namespace {

SupportIssue issue(int id, IssueSeverity severity) { return {id, severity, supportMessage(id)}; }

/// DOFs for which a prescribed value makes sense: blocked ones, and the three translations under an oblique restriction.
std::array<bool, 6> prescribableDofs(const ExternalSupport& s) {
    const Restraint r = restraintOf(s);
    std::array<bool, 6> p = r.fixed;
    if (r.oblique != Restraint::Oblique::None)
        for (int i = 0; i < 3; ++i) p[i] = true;
    return p;
}

}  // namespace

std::vector<SupportIssue> checkExternalSupport(const ExternalSupport& s) {
    std::vector<SupportIssue> out;
    if (s.type == ExternalSupportType::Free) return out;

    // User vectors must be usable.
    const SupportTypeInfo info = supportTypeInfo(s.type);
    const bool genericVector = s.type == ExternalSupportType::Generic && s.generic.mode != GenericDisplacementMode::Independent;
    if (info.userVector || genericVector) {
        const Point3& v = genericVector ? s.generic.vector : s.userVector;
        if (!finite3(v) || norm(v) < 1e-12)
            out.push_back({0, IssueSeverity::Error, "El vector director no puede ser nulo."});  // TODO(medir): literal of the original
    }
    // #65006: a generic restraint with nothing blocked.
    if (s.type == ExternalSupportType::Generic && !restraintOf(s).any()) out.push_back(issue(65006, IssueSeverity::Error));

    // Spring constants (only the ones that are used).
    const Restraint r = restraintOf(s);
    for (int i = 0; i < 6; ++i)
        if (r.elastic[i] && !(std::isfinite(s.elasticConstants[i]) && s.elasticConstants[i] > 0.0)) {
            out.push_back({0, IssueSeverity::Error, "La constante de apoyo elástico debe ser mayor que cero."});  // TODO(medir)
            break;
        }

    // Prescribed displacements.
    bool duplicate = false, allZero = false, incompatible = false;
    const std::array<bool, 6> ok = prescribableDofs(s);
    for (std::size_t i = 0; i < s.prescribed.size(); ++i) {
        const PrescribedDisplacement& p = s.prescribed[i];
        for (std::size_t j = 0; j < i; ++j)
            if (s.prescribed[j].loadCaseId == p.loadCaseId) duplicate = true;
        bool zero = true;
        for (int d = 0; d < 6; ++d) {
            if (p.values[d] != 0.0) {
                zero = false;
                if (!ok[d]) incompatible = true;
            }
        }
        if (zero) allZero = true;
    }
    if (duplicate) out.push_back(issue(65465, IssueSeverity::Error));
    if (allZero) out.push_back(issue(65466, IssueSeverity::Error));
    if (incompatible) out.push_back(issue(65467, IssueSeverity::Warning));
    return out;
}

bool elasticConstantIsDefault(const ExternalSupport& s) {
    const Restraint r = restraintOf(s);
    for (int i = 0; i < 6; ++i)
        if (r.elastic[i] && s.elasticConstants[i] == kDefaultElasticConstant) return true;
    return false;
}

std::string elasticDefaultQuestion(const std::string& constantText) {
    std::string text = supportMessage(65010);
    const std::size_t pos = text.find("%s");
    if (pos != std::string::npos) text.replace(pos, 2, constantText);
    return text;
}

std::vector<PrescribedDisplacement> effectivePrescribed(const ExternalSupport& s) {
    std::vector<PrescribedDisplacement> out = s.prescribed;
    const std::array<bool, 6> ok = prescribableDofs(s);
    for (PrescribedDisplacement& p : out)
        for (int d = 0; d < 6; ++d)
            if (!ok[d]) p.values[d] = 0.0;
    return out;
}

std::vector<SupportIssue> checkInternalLink(const InternalLink& l, int elementCount) {
    std::vector<SupportIssue> out;
    auto bad = [&](const char* text) { out.push_back({0, IssueSeverity::Error, text}); };  // TODO(medir): literals of the original
    if (l.type == InternalLinkType::PartialFixity && !(std::isfinite(l.partialCoefficient) && l.partialCoefficient >= 0.0))
        bad("El coeficiente de empotramiento parcial debe ser mayor o igual que cero.");
    if (l.type != InternalLinkType::Generic) return out;

    if (l.elementRow.empty()) bad("La vinculación genérica debe tener, al menos, un elemento.");
    if (elementCount > 0 && static_cast<int>(l.elementRow.size()) != elementCount)
        bad("El número de elementos de la vinculación no coincide con el del nudo.");
    for (int row : l.elementRow)
        if (row < 0 || row >= static_cast<int>(l.rows.size())) {
            bad("Cada elemento debe tener una y sólo una marca en alguno de los vínculos.");
            break;
        }
    for (const GenericLinkRow& row : l.rows)
        if (!(std::isfinite(row.coefficient) && row.coefficient >= 0.0)) {
            bad("El coeficiente de empotramiento parcial debe ser mayor o igual que cero.");
            break;
        }
    return out;
}

std::vector<SupportIssue> checkGenericInternalSelection(const std::vector<int>& counts) {
    std::vector<SupportIssue> out;
    if (!counts.empty() && std::any_of(counts.begin(), counts.end(), [&](int c) { return c != counts.front(); }))
        out.push_back(issue(65473, IssueSeverity::Warning));
    return out;
}

bool hasError(const std::vector<SupportIssue>& issues) {
    return std::any_of(issues.begin(), issues.end(), [](const SupportIssue& i) { return i.severity == IssueSeverity::Error; });
}

std::string validateSupports(const NodeSupports& s) {
    for (const auto& [id, support] : s.external) {
        if (id <= 0) return "Vinculación exterior con identificador de nudo no válido.";
        if (support.type == ExternalSupportType::Free) return "Vinculación exterior libre almacenada.";
        const int v = supportTypeIndex(support.type);
        if (v < 0 || v >= kExternalSupportTypeCount) return "Tipo de vinculación exterior no válido.";
        if (hasError(checkExternalSupport(support))) return "Vinculación exterior del nudo " + std::to_string(id) + " no válida.";
    }
    for (const auto& [id, link] : s.internal) {
        if (id <= 0) return "Vinculación interior con identificador de nudo no válido.";
        if (link.type == InternalLinkType::Fixed) return "Vinculación interior por defecto almacenada.";
        if (hasError(checkInternalLink(link))) return "Vinculación interior del nudo " + std::to_string(id) + " no válida.";
    }
    return {};
}

}  // namespace cc::core
