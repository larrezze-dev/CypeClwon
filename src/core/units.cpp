#include "core/units.h"

#include <algorithm>
#include <cctype>
#include <cstdio>

namespace cc::core {

double unitsPerMetre(LengthUnit unit) {
    switch (unit) {
        case LengthUnit::Metres: return 1.0;
        case LengthUnit::Centimetres: return 100.0;
        case LengthUnit::Millimetres: return 1000.0;
    }
    return 1.0;
}

double fromMetres(double metres, LengthUnit unit) { return metres * unitsPerMetre(unit); }
double toMetres(double value, LengthUnit unit) { return value / unitsPerMetre(unit); }
double convertLength(double value, LengthUnit from, LengthUnit to) { return fromMetres(toMetres(value, from), to); }

const char* unitSymbol(LengthUnit unit) {
    switch (unit) {
        case LengthUnit::Metres: return "m";
        case LengthUnit::Centimetres: return "cm";
        case LengthUnit::Millimetres: return "mm";
    }
    return "m";
}

const char* unitName(LengthUnit unit) {
    switch (unit) {
        case LengthUnit::Metres: return "Metros";
        case LengthUnit::Centimetres: return "Centímetros";
        case LengthUnit::Millimetres: return "Milímetros";
    }
    return "Metros";
}

const std::array<LengthUnit, 3>& allLengthUnits() {
    static const std::array<LengthUnit, 3> all = {LengthUnit::Metres, LengthUnit::Centimetres, LengthUnit::Millimetres};
    return all;
}

bool parseUnit(const std::string& text, LengthUnit& out) {
    std::string lower;
    for (unsigned char c : text) lower += static_cast<char>(std::tolower(c));
    // The Spanish names contain UTF-8 accented letters that tolower() leaves alone; compare them in their stored case.
    for (LengthUnit u : allLengthUnits()) {
        std::string name = unitName(u);
        std::string lowerName;
        for (unsigned char c : name) lowerName += static_cast<char>(std::tolower(c));
        if (lower == unitSymbol(u) || lower == lowerName) {
            out = u;
            return true;
        }
    }
    return false;
}

std::string formatLength(double metres, LengthUnit unit, int decimals) {
    char buf[64];
    std::snprintf(buf, sizeof buf, "%.*f", std::clamp(decimals, 0, 12), fromMetres(metres, unit));
    std::string s = buf;
    if (s.rfind("-0", 0) == 0 && s.find_first_not_of("-0.") == std::string::npos) s.erase(0, 1);  // no "-0.0"
    return s;
}

std::string formatLengthWithUnit(double metres, LengthUnit unit, int decimals) {
    return formatLength(metres, unit, decimals) + " " + unitSymbol(unit);
}

}  // namespace cc::core
