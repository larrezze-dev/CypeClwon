#include "core/project_settings.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <initializer_list>

namespace cc::core {

namespace {

bool contains(const std::vector<std::string>& list, const std::string& value) {
    return std::find(list.begin(), list.end(), value) != list.end();
}

bool inRange(double value, double lo, double hi) { return std::isfinite(value) && value >= lo && value <= hi; }

bool allIn(const std::vector<std::string>& list, std::initializer_list<const std::string*> values) {
    return std::all_of(values.begin(), values.end(), [&](const std::string* v) { return contains(list, *v); });
}

}  // namespace

// Lists below contain only values seen on screen in the original (docs/screenshots/orig_obra_vacia_*).
// Lists with unverified gaps (rebar steels, aggregates, timber classes, concrete grades) are tracked in TK-011.

const std::vector<std::string>& hotRolledSteelGrades() {
    static const std::vector<std::string> v = {
        "S235", "S275", "S355", "S450", "S275M", "S355M", "S420M", "S460M",
        "S235 (UNE-EN 10025-2)", "S275 (UNE-EN 10025-2)", "S355 (UNE-EN 10025-2)", "S450 (UNE-EN 10025-2)",
        "S275M (UNE-EN 10025-4)", "S355M (UNE-EN 10025-4)", "S420M (UNE-EN 10025-4)", "S460M (UNE-EN 10025-4)",
        "S355 HISTAR®", "S460 HISTAR®"};
    return v;
}

const std::vector<std::string>& coldFormedSteelGrades() {
    static const std::vector<std::string> v = {"S235", "S275", "S355", "S420"};
    return v;
}

const std::vector<std::string>& concreteGrades() {
    // Verified by hand-made captures (Hormigon 1/2.png, 2026-11-04): full list, note there is no "Bc 45".
    static const std::vector<std::string> v = {"C12/15", "C16/20", "C20/25", "C25/30", "C30/37", "C35/45",
                                               "C40/50", "C45/55", "C50/60", "Bc 15",  "Bc 20",  "Bc 25",
                                               "Bc 30",  "Bc 35",  "Bc 40",  "Bc 50",  "Bc 60"};
    return v;
}

const std::vector<std::string>& timberMaterials() {
    // Verified (Madera (2).png): four materials.
    static const std::vector<std::string> v = {"Aserrada, procedente de coníferas o chopos",
                                               "Aserrada, procedente de especies frondosas",
                                               "Laminada encolada, homogénea", "Laminada encolada, combinada"};
    return v;
}

const std::vector<std::string>& timberStrengthClasses() {
    // Verified (Material de madera 1/2.png): C14 ... C50 (service classes 2 and 3 texts still pending capture).
    static const std::vector<std::string> v = {"C14", "C16", "C18", "C20", "C22", "C24",
                                               "C27", "C30", "C35", "C40", "C45", "C50"};
    return v;
}

const std::vector<std::string>& aluminiumAlloys() {
    static const std::vector<std::string> v = {"EN AW-5083", "EN AW-6060", "EN AW-6061", "EN AW-6063",
                                               "EN AW-6005A", "EN AW-6106", "EN AW-6082", "EN AW-7020"};
    return v;
}

const std::vector<std::string>& aluminiumTempers() {
    static const std::vector<std::string> v = {"F", "O/H111", "H112"};
    return v;
}

const std::vector<std::string>& rebarSteels() {
    // Verified (Acero en barras.png).
    static const std::vector<std::string> v = {"S-220", "S-400", "S-500", "PC 60", "PC 52", "OB 37", "fyk=400", "fyk=500"};
    return v;
}

const std::vector<std::string>& aggregateKinds() {
    // Verified (Naturaleza del arido.png).
    static const std::vector<std::string> v = {"Cuarcita", "Caliza", "Arenisca", "Basalto"};
    return v;
}

bool parseAggregateSizeMm(const std::string& text, double* mm) {
    // Accepts "15" or "15 mm" (the stored form of the maximum aggregate size).
    char* end = nullptr;
    const double value = std::strtod(text.c_str(), &end);
    if (end == text.c_str()) return false;
    std::string rest(end);
    rest.erase(0, rest.find_first_not_of(' '));
    if (!rest.empty() && rest != "mm") return false;
    if (mm) *mm = value;
    return inRange(value, kAggregateMinMm, kAggregateMaxMm);
}

const std::string& timberServiceClassText(int serviceClass) {
    // Verified texts (hand-made captures "Madera (1..8).png", 2026-11-04).
    static const std::string c1 =
        "Clase de servicio 1: se caracteriza por un contenido de humedad en los materiales correspondiente a una "
        "temperatura de 20°C y una humedad relativa del aire que sólo exceda el 65% unas pocas semanas al año.";
    static const std::string c2 =
        "Clase de servicio 2: se caracteriza por un contenido de humedad en los materiales correspondiente a una "
        "temperatura de 20°C y una humedad relativa del aire que sólo exceda el 85% unas pocas semanas al año.";
    static const std::string c3 =
        "Clase de servicio 3: condiciones climáticas que conduzcan a contenidos de humedad superiores a la clase de "
        "servicio 2.";
    static const std::string none;
    return serviceClass == 1 ? c1 : serviceClass == 2 ? c2 : serviceClass == 3 ? c3 : none;
}

bool TimberSettings::isValid() const {
    return contains(timberMaterials(), material) && contains(timberStrengthClasses(), strengthClass) &&
           serviceClass >= 1 && serviceClass <= 3;
}

bool AluminiumSettings::isValid() const {
    return contains(aluminiumAlloys(), alloy) && contains(aluminiumTempers(), temper);
}

bool ProfileSettings::isValid() const {
    return contains(hotRolledSteelGrades(), hotRolledSteel) && contains(coldFormedSteelGrades(), coldFormedSteel) &&
           timber.isValid() && aluminium.isValid() && contains(concreteGrades(), concrete);
}

bool RebarSteelSettings::isValid() const {
    return allIn(rebarSteels(), {&columnLongitudinal, &columnTransverse, &beamTopAssembly, &beamTopReinforcement,
                                 &beamBottom, &beamTransverse, &beamSkin, &footings, &pileCaps, &tieBeams});
}

std::string RebarSteelSettings::summary() const {
    const std::string* all[] = {&columnTransverse, &beamTopAssembly, &beamTopReinforcement, &beamBottom,
                                &beamTransverse,   &beamSkin,        &footings,            &pileCaps, &tieBeams};
    const bool same = std::all_of(std::begin(all), std::end(all), [&](const std::string* s) { return *s == columnLongitudinal; });
    return same ? columnLongitudinal : "Varios";
}

bool AggregateSettings::isValid() const {
    const AggregateSetting* items[] = {&columns, &beams, &foundationBeams};
    const bool ok = std::all_of(std::begin(items), std::end(items), [](const AggregateSetting* a) {
        return contains(aggregateKinds(), a->kind) && parseAggregateSizeMm(a->maxSize, nullptr);
    });
    return ok && inRange(foundationMaxSizeMm, kAggregateMinMm, kAggregateMaxMm);
}

std::string AggregateSettings::summary() const {
    const bool sameKind = columns.kind == beams.kind && beams.kind == foundationBeams.kind;
    const bool sameSize = columns.maxSize == beams.maxSize && beams.maxSize == foundationBeams.maxSize;
    std::string size = std::to_string(static_cast<int>(std::lround(foundationMaxSizeMm))) + " mm";
    const std::string kind = sameKind ? columns.kind : "Varios";
    // "Cuarcita (15 mm), 30 mm": columns/beams size in brackets, foundation size after the comma.
    return kind + " (" + (sameSize ? columns.maxSize : std::string("varios")) + "), " + size;
}

bool CoverSettings::isValid() const {
    for (double v : {columns, beamTop, beamBottom, beamSide, footingTop, footingBottom, footingSide, tieBeamTop,
                     tieBeamBottom, tieBeamSide}) {
        if (!inRange(v, kMin, kMax)) return false;
    }
    return true;
}

bool WasteSettings::isValid() const {
    for (double v : {columns, beams, footings, pileCaps, tieBeams}) {
        if (!inRange(v, kMin, kMax)) return false;
    }
    return true;
}

std::string EnvironmentSettings::beamsSummary() const { return "X0"; }

const std::vector<UsualGround>& usualGroundValues() {
    static const std::vector<UsualGround> v = {
        {"Grava", 0.294, 0.412},         {"Arena densa", 0.245, 0.343}, {"Arena semidensa", 0.196, 0.275},
        {"Arena suelta", 0.098, 0.137},  {"Limo", 0.118, 0.165},        {"Arcilla dura", 0.245, 0.343},
        {"Arcilla semidura", 0.147, 0.206}, {"Arcilla blanda", 0.098, 0.137}};
    return v;
}

bool GroundSettings::isValidStress(double value) { return inRange(value, kMinStress, kMaxStress); }

namespace {
bool nonNegative(double v) { return std::isfinite(v) && v >= 0.0; }
}  // namespace

bool PileSinkingSettings::isValid() const {
    return nonNegative(frictionAngle) && nonNegative(cohesion) && nonNegative(undrainedShear) &&
           nonNegative(unconfinedCompression) && nonNegative(unitWeight) && nonNegative(geotechnicalTip) &&
           nonNegative(geotechnicalShaft) && nonNegative(geotechnicalTotal) && nonNegative(geotechnicalShaftTension) &&
           nonNegative(unitTip) && nonNegative(unitShaft) && nonNegative(unitTotal) && nonNegative(unitShaftTension) &&
           nonNegative(efficiencyK);
}

bool PileStructuralSettings::isValid() const {
    return nonNegative(precastValue) && nonNegative(castInSituValue) && nonNegative(undrainedShearFactor) &&
           nonNegative(unitWeightFactor);
}

bool PileCapSettings::isValid() const { return nonNegative(precastValue) && nonNegative(castInSituValue); }

bool MicropileAdvancedSettings::isValid() const {
    return sinking.isValid() && nonNegative(undrainedShearFactor) && nonNegative(unitWeightFactor);
}

bool PileSettings::isValid() const {
    return nonNegative(minMomentEccentricityM) && nonNegative(shaftAxialFactor) && nonNegative(tipOnlyAxialFactor) &&
           nonNegative(bucklingCoefficient) && advanced.isValid();
}

bool MicropileSettings::isValid() const {
    return nonNegative(reducedAxialFactor) && nonNegative(bucklingCoefficient) && nonNegative(corrosionThicknessMm) &&
           nonNegative(tubularAreaFactor) && nonNegative(executionFactor) && advanced.isValid();
}

bool ActionsSettings::isValid() const {
    return inRange(formworkRemovalDays, kMinDays, kMaxDays) && static_cast<int>(steelFire) >= 0 &&
           static_cast<int>(steelFire) <= 2;
}

bool ReinforcedConcreteSettings::isValid() const {
    return contains(concreteGrades(), columnConcrete) && contains(concreteGrades(), beamConcrete) &&
           contains(concreteGrades(), foundationConcrete) && rebarSteel.isValid() && aggregate.isValid() &&
           cover.isValid() && waste.isValid();
}

}  // namespace cc::core
