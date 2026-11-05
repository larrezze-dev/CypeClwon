#pragma once
#include <string>
#include <vector>

#include "core/project.h"
#include "core/structure_model.h"

namespace cc::core {

/// Ranges, defaults and checks of the "Sección de la lámina", "Material" and "Disposición" panels of "Geometría > Láminas".
/// Source: docs/research/ui/laminas.md section 7 (measured in the original, 2026-11-05). Where a value was not measured the
/// comment says PROVISIONAL.

struct ValueRange {
    double min = 0.0;
    double max = 0.0;
    bool contains(double v) const { return v >= min && v <= max; }
};

// "Sección de la lámina" (millimetres in the dialog; metres in the model for the thickness).
inline constexpr double kDefaultShellThicknessMm = 100.0;
inline constexpr ValueRange kShellThicknessMm{0.10, 10000.0};
inline constexpr double kDefaultShellBallastKnM3 = 29430.0;
inline constexpr ValueRange kShellBallastKnM3{0.01, 1000000.0};
// Generic isotropic material.
inline constexpr ValueRange kIsoElasticModulusMPa{0.10, 10000000.0};
inline constexpr ValueRange kIsoPoissonRatio{0.01, 0.49};
inline constexpr ValueRange kThermalExpansion{0.000001, 100.0};
inline constexpr ValueRange kIsoDensityKnM3{0.01, 100000.0};
// Generic orthotropic material. The Poisson tooltip of the original says "-10000.00 .. 10000.00" (not restrictive).
inline constexpr ValueRange kOrthoElasticModulusMPa{0.10, 10000000.0};
inline constexpr ValueRange kOrthoPoissonRatio{-10000.0, 10000.0};
inline constexpr ValueRange kOrthoShearModulusMPa{0.10, 10000000.0};
/// PROVISIONAL: the range of the orthotropic "Peso específico" was not read; the isotropic one is used.
inline constexpr ValueRange kOrthoDensityKnM3{0.01, 100000.0};
// "Disposición": "Desplazamiento del plano central" Z, whole millimetres.
inline constexpr ValueRange kShellOffsetMm{-5000.0, 5000.0};

/// "El valor de 'Espesor' debe estar entre 0.10 y 10000.00." (the warning of the original; `decimals` digits after the point).
std::string rangeWarning(const std::string& label, const ValueRange& range, int decimals);

/// First problem of a material (a range warning of the original) or empty when it is valid. Only the fields of the active kind
/// are checked.
std::string validateShellMaterial(const ShellMaterial& material);

/// Name a shell shows in its information box ("C25/30", "S275 (UNE-EN 10025-2)", "S235", "EN AW-5083 - F"). With
/// `useGeneralData` it comes from the general data of the work (`Project::profiles`). The generic kinds show
/// "Genérico isótropo" / "Genérico ortótropo" (PROVISIONAL: the original text was not captured).
std::string shellMaterialName(const ShellMaterial& material, const ProfileSettings& profiles);

/// Grade a concrete / rolled steel / cold formed material would take when "Seleccionar el material de la lámina" is on and the
/// combo shows its first item (C12/15, S275 (UNE-EN 10025-2), S235). Empty for the other kinds.
std::string defaultShellGrade(ShellMaterialKind kind);

/// Whether the shell material is isotropic in the solver sense (everything but the orthotropic generic).
bool isIsotropic(const ShellMaterial& material);

}  // namespace cc::core
