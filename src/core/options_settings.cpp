#include "core/options_settings.h"

#include <algorithm>

#include "core/project_settings.h"

namespace cc::core {

const std::vector<DeflectionLimitGroup>& defaultDeflectionLimits() {
    static const std::vector<DeflectionLimitGroup> groups = {
        {"Hormigón",
         {{"A plazo infinito (Cuasipermanente)", 250}, {"Activa a largo plazo (Cuasipermanente)", 500}}},
        {"Acero laminado", {}},
        {"Acero conformado",
         {{"Instantánea de sobrecarga", 350},
          {"Instantánea total (Cuasipermanente)", 300},
          {"Activa a largo plazo (Característica)", 400}}},
        {"Madera",
         {{"Instantánea de sobrecarga", 350},
          {"A plazo infinito (Cuasipermanente)", 300},
          {"Activa a largo plazo (Característica)", 400}}}};
    return groups;
}

const std::vector<std::string>& boltSteelClasses() {
    static const std::vector<std::string> classes = {"4.6", "5.6", "6.8", "8.8", "10.9"};
    return classes;
}

const std::vector<std::string>& boltDiameterNames() {
    static const std::vector<std::string> names = {"M12", "M16", "M20", "M24", "M30",
                                                   "M36", "M42", "M48", "M56", "M64"};
    return names;
}

bool JointMaterials::isValid() const {
    const auto& steels = hotRolledSteelGrades();
    const auto& concretes = concreteGrades();
    return std::find(steels.begin(), steels.end(), rolledSteel) != steels.end() &&
           std::find(concretes.begin(), concretes.end(), concrete) != concretes.end() && !anchorBoltSteel.empty();
}

bool BoltSettings::isValid() const {
    const auto& classes = boltSteelClasses();
    return std::find(classes.begin(), classes.end(), steelClass) != classes.end();
}

}  // namespace cc::core
