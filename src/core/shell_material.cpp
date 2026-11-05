#include "core/shell_material.h"

#include <cstdio>

namespace cc::core {

namespace {
std::string number(double v, int decimals) {
    char buffer[64];
    std::snprintf(buffer, sizeof buffer, "%.*f", decimals, v);
    return buffer;
}
}  // namespace

std::string rangeWarning(const std::string& label, const ValueRange& range, int decimals) {
    return "El valor de '" + label + "' debe estar entre " + number(range.min, decimals) + " y " + number(range.max, decimals) + ".";
}

std::string defaultShellGrade(ShellMaterialKind kind) {
    switch (kind) {
        case ShellMaterialKind::Concrete: return "C12/15";
        case ShellMaterialKind::RolledSteel: return "S275 (UNE-EN 10025-2)";
        case ShellMaterialKind::ColdFormedSteel: return "S235";
        default: return {};
    }
}

bool isIsotropic(const ShellMaterial& material) { return material.kind != ShellMaterialKind::GenericOrthotropic; }

std::string validateShellMaterial(const ShellMaterial& m) {
    const auto check = [](double value, const ValueRange& range, const char* label, int decimals) -> std::string {
        // `!contains` also refuses NaN.
        return range.contains(value) ? std::string() : rangeWarning(label, range, decimals);
    };
    std::string error;
    const auto first = [&error](std::string message) {
        if (error.empty()) error = std::move(message);
    };
    switch (m.kind) {
        case ShellMaterialKind::GenericIsotropic:
            first(check(m.isoElasticModulusMPa, kIsoElasticModulusMPa, "Módulo de elasticidad", 2));
            first(check(m.isoPoissonRatio, kIsoPoissonRatio, "Módulo de Poisson", 2));
            first(check(m.isoThermalExpansion, kThermalExpansion, "Coeficiente de dilatación", 6));
            first(check(m.isoDensityKnM3, kIsoDensityKnM3, "Peso específico", 2));
            break;
        case ShellMaterialKind::GenericOrthotropic:
            first(check(m.orthoElasticModulusXMPa, kOrthoElasticModulusMPa, "Módulo de elasticidad (Ex)", 2));
            first(check(m.orthoElasticModulusYMPa, kOrthoElasticModulusMPa, "Módulo de elasticidad (Ey)", 2));
            first(check(m.orthoPoissonRatioXY, kOrthoPoissonRatio, "Módulo de Poisson (nu,xy)", 2));
            first(check(m.orthoShearModulusXYMPa, kOrthoShearModulusMPa, "Módulo de cortadura (xy)", 2));
            first(check(m.orthoShearModulusXZMPa, kOrthoShearModulusMPa, "Módulo de cortadura (xz)", 2));
            first(check(m.orthoShearModulusYZMPa, kOrthoShearModulusMPa, "Módulo de cortadura (yz)", 2));
            first(check(m.orthoThermalExpansionX, kThermalExpansion, "Coeficiente de dilatación (x)", 6));
            first(check(m.orthoThermalExpansionY, kThermalExpansion, "Coeficiente de dilatación (y)", 6));
            first(check(m.orthoDensityKnM3, kOrthoDensityKnM3, "Peso específico", 2));
            break;
        default: break;
    }
    return error;
}

std::string shellMaterialName(const ShellMaterial& m, const ProfileSettings& profiles) {
    switch (m.kind) {
        case ShellMaterialKind::Concrete:
            return m.useGeneralData ? profiles.concrete : (m.grade.empty() ? defaultShellGrade(m.kind) : m.grade);
        case ShellMaterialKind::RolledSteel:
            return m.useGeneralData ? profiles.hotRolledSteel : (m.grade.empty() ? defaultShellGrade(m.kind) : m.grade);
        case ShellMaterialKind::ColdFormedSteel:
            return m.useGeneralData ? profiles.coldFormedSteel : (m.grade.empty() ? defaultShellGrade(m.kind) : m.grade);
        case ShellMaterialKind::Aluminium:
            return m.useGeneralData ? profiles.aluminium.summary() : m.alloy + " - " + m.temper;
        case ShellMaterialKind::GenericIsotropic: return "Genérico isótropo";
        case ShellMaterialKind::GenericOrthotropic: return "Genérico ortótropo";
    }
    return {};
}

}  // namespace cc::core
