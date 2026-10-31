#include "core/project_settings.h"

#include <algorithm>
#include <cmath>
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
    // Verified: first entry C12/15, the run C25/30..C50/60 and Bc 15, Bc 20, Bc 25 ... Bc 60 (intermediate entries unverified).
    static const std::vector<std::string> v = {"C12/15", "C25/30", "C30/37", "C35/45", "C40/50", "C45/55",
                                               "C50/60", "Bc 15",  "Bc 20",  "Bc 25",  "Bc 60"};
    return v;
}

const std::vector<std::string>& timberMaterials() {
    static const std::vector<std::string> v = {"Aserrada, procedente de coníferas o chopos"};
    return v;
}

const std::vector<std::string>& timberStrengthClasses() {
    static const std::vector<std::string> v = {"C24"};
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
    static const std::vector<std::string> v = {"S-400"};
    return v;
}

const std::vector<std::string>& aggregateKinds() {
    static const std::vector<std::string> v = {"Cuarcita"};
    return v;
}

const std::vector<std::string>& aggregateMaxSizes() {
    static const std::vector<std::string> v = {"15 mm"};
    return v;
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
        return contains(aggregateKinds(), a->kind) && contains(aggregateMaxSizes(), a->maxSize);
    });
    return ok && inRange(foundationMaxSizeMm, 1.0, 100.0);
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

bool PileSettings::isValid() const {
    return nonNegative(minMomentEccentricityM) && nonNegative(shaftAxialFactor) && nonNegative(tipOnlyAxialFactor) &&
           nonNegative(bucklingCoefficient) && advanced.isValid();
}

bool MicropileSettings::isValid() const {
    return nonNegative(reducedAxialFactor) && nonNegative(bucklingCoefficient) && nonNegative(corrosionThicknessMm) &&
           nonNegative(tubularAreaFactor) && nonNegative(executionFactor);
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
