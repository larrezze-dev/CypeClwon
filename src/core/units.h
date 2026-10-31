#pragma once
#include <array>
#include <string>

namespace cc::core {

/// Unit system of the project. The original has no switch between unit systems: it always works in kN, m, MPa
/// (shown as kN, kN/m, MPa, mm, cm, cm2 in its dialogs) and only lets the user choose the length unit used to
/// *display* dimensions, reinforcement and lengths ("Obra > Unidades de acotación"). Reference:
/// docs/research/ui/dialogs_unidades.md and docs/screenshots/u_units1.png.
///
/// All model values are stored in metres (SI); a LengthUnit only affects presentation.
enum class LengthUnit { Metres, Centimetres, Millimetres };

/// Number of units that make one metre: 1, 100 or 1000.
double unitsPerMetre(LengthUnit unit);
double fromMetres(double metres, LengthUnit unit);
double toMetres(double value, LengthUnit unit);
double convertLength(double value, LengthUnit from, LengthUnit to);

/// "m", "cm", "mm".
const char* unitSymbol(LengthUnit unit);
/// Spanish name shown in the combos: "Metros", "Centímetros", "Milímetros".
const char* unitName(LengthUnit unit);
/// All choices of the combos, in the order of the original.
const std::array<LengthUnit, 3>& allLengthUnits();
/// Stable key for files ("m", "cm", "mm"); parseUnit() also accepts the Spanish names, case-insensitively.
bool parseUnit(const std::string& text, LengthUnit& out);

/// "Unidades de acotación": one length unit per kind of quantity.
struct DimensioningUnits {
    LengthUnit dimensions = LengthUnit::Centimetres;  ///< "Dimensiones": section sizes, cover, node size...
    LengthUnit reinforcement = LengthUnit::Centimetres;  ///< "Armaduras": bar lengths, spacings
    LengthUnit lengths = LengthUnit::Metres;           ///< "Longitudes": member and structure lengths, coordinates

    bool operator==(const DimensioningUnits&) const = default;
};

/// Value in metres formatted for display with `decimals` decimals and '.' as separator ("15.0", like the original),
/// without the unit symbol.
std::string formatLength(double metres, LengthUnit unit, int decimals);
/// Same with the symbol appended ("15.0 cm").
std::string formatLengthWithUnit(double metres, LengthUnit unit, int decimals);

}  // namespace cc::core
