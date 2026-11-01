#pragma once
#include <optional>
#include <string>

namespace cc::core {

/// "Posición de los tooltips" (the two entries of the original's combo, tp2_combo_open.png).
enum class TooltipPlacement { TopRightCorner, OnElement };

/// Values of the "Opciones de visualización" panel of the original (docs/screenshots/tp2_combo_open.png).
/// Defaults are those seen in the original on a new project. They are held in memory for the session only: whether
/// they belong to the project or to the user is not verified (TK-028).
struct DisplayOptions {
    bool showElementInfo = true;   ///< "Mostrar la información de los elementos"
    bool showTexts = true;         ///< "Mostrar textos sobre los elementos"
    bool showLocalAxes = true;     ///< "Mostrar los ejes locales de los elementos"
    bool fillShells = false;       ///< "Dibujar las láminas con relleno"
    bool fillLoads = true;         ///< "Dibujar las cargas con relleno"
    double nodeSizeCm = 15.0;      ///< "Dimensiones de los nudos" (cm)
    double textSizePercent = 100;  ///< "Tamaño de los textos" (%)
    TooltipPlacement tooltipPlacement = TooltipPlacement::TopRightCorner;  ///< "Posición de los tooltips"

    bool operator==(const DisplayOptions&) const = default;
};

/// Limits of the numeric fields, verified in the original (op2..op6 captures: "Debe estar entre 1.0 y 1000.0." and
/// "Debe estar entre 10.0 % y 1000.0 %.").
constexpr double kNodeSizeMinCm = 1.0, kNodeSizeMaxCm = 1000.0;
constexpr double kTextSizeMinPercent = 10.0, kTextSizeMaxPercent = 1000.0;

/// Sizes must be finite and inside the original's limits.
bool isValid(const DisplayOptions& options);

/// JSON text of `options` (enumeration stored by name).
std::string toJson(const DisplayOptions& options);

/// Parses `jsonText`. Missing fields keep their default; malformed text, wrong types, unknown enumeration names or
/// invalid values give an empty result.
std::optional<DisplayOptions> displayOptionsFromJson(const std::string& jsonText);

}  // namespace cc::core
