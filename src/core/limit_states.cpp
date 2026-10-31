#include "core/limit_states.h"

#include <algorithm>

namespace cc::core {

const std::string& LimitStatesSettings::defaultSnow() {
    static const std::string v = "Altitud inferior o igual a 1000 m";
    return v;
}

const std::vector<std::string>& LimitStatesSettings::snowChoices() {
    static const std::vector<std::string> v = {defaultSnow()};
    return v;
}

std::vector<LimitStatesSettings::Row> LimitStatesSettings::rows() const {
    const std::string hot = hotRolledCode == CombinationCode::CTE ? "CTE" : "EC";
    return {{"Hormigón", "EC - " + defaultSnow()},
            {"Cimentación", "EC - " + defaultSnow()},
            {"Acero conformado", "CTE - " + coldFormedSnow},
            {"Acero laminado", hot + " - " + hotRolledSnow},
            {"Madera", "CTE - " + timberSnow},
            {"Aluminio", "EC - " + defaultSnow()},
            {"Tensiones sobre el terreno", "EC - " + groundSnow},
            {"Desplazamientos", "Acciones características"}};
}

bool LimitStatesSettings::isValid() const {
    const auto& c = snowChoices();
    const auto ok = [&c](const std::string& s) { return std::find(c.begin(), c.end(), s) != c.end(); };
    return ok(groundSnow) && ok(coldFormedSnow) && ok(timberSnow) && ok(hotRolledSnow);
}

}  // namespace cc::core
