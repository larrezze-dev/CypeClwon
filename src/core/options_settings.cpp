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

const std::vector<std::string>& preloadedBoltSeries() {
    static const std::vector<std::string> series = {"ISO 7411", "ISO 7412", "EN 14399-3 (HR)", "EN 14399-4 (HV)"};
    return series;
}

const std::vector<std::string>& preloadedBoltDiameterNames() {
    static const std::vector<std::string> names = {"M12", "M16", "M20", "M24", "M27", "M30", "M36"};
    return names;
}

double frictionCoefficient(char frictionClass) {
    switch (frictionClass) {
        case 'A': return 0.50;
        case 'B': return 0.40;
        case 'C': return 0.30;
        case 'D': return 0.20;
        default: return 0.0;
    }
}

std::string frictionDescription(char frictionClass) {
    if (frictionClass == 'C') return "Superficies limpiadas a cepillo metálico o con llama, con eliminación de partes oxidadas.";
    return {};  // the texts of A, B and D were not captured yet (ticket TK-019)
}

bool PreloadedBoltSettings::isValid() const {
    const auto& names = preloadedBoltSeries();
    return std::find(names.begin(), names.end(), series) != names.end() && (steelClass == "8.8" || steelClass == "10.9") &&
           frictionCoefficient(frictionClass) > 0.0;
}

bool BasePlateSettings::isValid() const {
    return maxOverhangThickness > 0 && maxWidthThickness > 0 && maxRecommendedThicknessMm > 0 && groutThicknessCm >= 0 &&
           stiffenerBevelDeg >= 0 && stiffenerBevelDeg < 90 && maxSidesBoltDiameter > 0;
}

}  // namespace cc::core
