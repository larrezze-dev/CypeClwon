#include "core/options_settings.h"

#include <algorithm>

#include "core/project_settings.h"

namespace cc::core {

namespace {
bool nonNegative(const std::array<int, 3>& values) {
    return std::all_of(values.begin(), values.end(), [](int v) { return v >= 0; });
}
const char* const kCombinations[3] = {"Característica", "Frecuente", "Cuasipermanente"};
}  // namespace

bool DeflectionLimitSet::isValid() const {
    return instantSimple[0] >= 0 && instantSimple[1] >= 0 && nonNegative(instantCombination) && nonNegative(infinite) &&
           nonNegative(active) && damageableMonths >= 0;
}

DeflectionLimitsSettings::DeflectionLimitsSettings() {
    DeflectionLimitSet& concrete = materials[0];
    concrete.hasInfinite = concrete.hasDamageable = true;
    concrete.infinite[2] = 250;
    concrete.active[2] = 500;
    // Acero laminado: no limit and no infinite / damageable sections.
    DeflectionLimitSet& cold = materials[2];
    cold.hasDamageable = true;
    cold.instantSimple[1] = 350;
    cold.instantCombination[2] = 300;
    cold.active[0] = 400;
    DeflectionLimitSet& timber = materials[3];
    timber.hasInfinite = timber.hasDamageable = true;
    timber.instantSimple[1] = 350;
    timber.infinite[2] = 300;
    timber.active[0] = 400;
}

const char* DeflectionLimitsSettings::materialName(std::size_t index) {
    static const char* const names[4] = {"Hormigón", "Acero laminado", "Acero conformado", "Madera"};
    return names[index];
}

std::vector<DeflectionLimitGroup> DeflectionLimitsSettings::groups() const {
    std::vector<DeflectionLimitGroup> result;
    for (std::size_t m = 0; m < materials.size(); ++m) {
        const DeflectionLimitSet& set = materials[m];
        DeflectionLimitGroup group{materialName(m), {}};
        const auto add = [&](const std::string& text, int denominator) {
            if (denominator > 0) group.limits.push_back({text, denominator});
        };
        add("Instantánea de carga permanente", set.instantSimple[0]);
        add("Instantánea de sobrecarga", set.instantSimple[1]);
        for (int i = 0; i < 3; ++i) add(std::string("Instantánea total (") + kCombinations[i] + ")", set.instantCombination[i]);
        if (set.hasInfinite) {
            for (int i = 0; i < 3; ++i) add(std::string("A plazo infinito (") + kCombinations[i] + ")", set.infinite[i]);
        }
        for (int i = 0; i < 3; ++i) add(std::string("Activa a largo plazo (") + kCombinations[i] + ")", set.active[i]);
        result.push_back(std::move(group));
    }
    return result;
}

bool DeflectionLimitsSettings::isValid() const {
    return std::all_of(materials.begin(), materials.end(), [](const DeflectionLimitSet& s) { return s.isValid(); });
}

const std::vector<DeflectionLimitGroup>& defaultDeflectionLimits() {
    static const std::vector<DeflectionLimitGroup> groups = DeflectionLimitsSettings().groups();
    return groups;
}

TransverseReinforcementTable::TransverseReinforcementTable() {
    for (int cm = 5; cm <= 30; ++cm) spacingsCm.push_back(cm);
}

bool TransverseReinforcementTable::isValid() const {
    return !diameters.empty() && !spacingsCm.empty() &&
           std::all_of(diameters.begin(), diameters.end(), [](int d) { return d > 0; }) &&
           std::all_of(spacingsCm.begin(), spacingsCm.end(), [](double s) { return s > 0; });
}

const std::array<int, 8>& DiameterAdmissibility::diameters() {
    static const std::array<int, 8> values = {6, 8, 10, 12, 16, 20, 25, 32};
    return values;
}

const std::array<int, 5>& ReinforcementBarTable::reinforcementDiameters() {
    static const std::array<int, 5> values = {10, 12, 16, 20, 25};
    return values;
}

ReinforcementBarTable::ReinforcementBarTable() {
    // A combination is rejected when the reinforcement bar is smaller than the one it is paired with: assembly
    // O20 with O10, O25 and O32 with O10 and O12 (first matrix) and O20 with O10, O25 with O10 and O12 (second).
    const auto& bars = reinforcementDiameters();
    const auto& assembly = DiameterAdmissibility::diameters();
    for (std::size_t r = 0; r < assembly.size(); ++r) {
        for (std::size_t c = 0; c < bars.size(); ++c) assemblyVsFirst[r][c] = !(assembly[r] >= 20 && bars[c] < (assembly[r] == 20 ? 12 : 16));
    }
    for (std::size_t r = 0; r < bars.size(); ++r) {
        for (std::size_t c = 0; c < bars.size(); ++c) firstVsSecond[r][c] = !(bars[r] >= 20 && bars[c] < (bars[r] == 20 ? 12 : 16));
    }
}

bool ReinforcementBarTable::isValid() const {
    return !diameters.empty() && minClearSpacingCm >= 0 &&
           std::all_of(diameters.begin(), diameters.end(), [](int d) { return d > 0; });
}

BeamTablesSettings::BeamTablesSettings() {
    stirrupHolder.admissible = {true, true, true, true, true, true, true, false};  // O6-O25 yes, O32 no
    skin.admissible = {false, true, true, true, true, true, true, false};           // O6 no, O8-O25 yes, O32 no
}

bool BeamTablesSettings::isValid() const {
    return transverse.isValid() && topReinforcement.isValid() && bottomReinforcement.isValid();
}

bool PileCapGeneralSettings::isValid() const {
    return minOverhangOverPileM >= 0 && minOverhangOverSupportM >= 0 && minDepthM > 0 && maxDepthM >= minDepthM &&
           minBarSpacingM > 0 && maxBarDiameterMm > 0 && loadFraction > 0 && loadFactor > 0;
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
