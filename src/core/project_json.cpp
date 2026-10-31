#include "core/project_json.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <functional>
#include <stdexcept>
#include <utility>

namespace cc::core {

using json = nlohmann::json;

// Enumerations are stored by name. Unlike NLOHMANN_JSON_SERIALIZE_ENUM an unknown name is an error, never a silent
// fallback to the first value.
#define CC_ENUM(Type, ...)                                                       \
    inline void to_json(json& j, const Type& value) {                            \
        static const std::pair<Type, const char*> names[] = __VA_ARGS__;         \
        for (const auto& entry : names)                                          \
            if (entry.first == value) {                                          \
                j = entry.second;                                                \
                return;                                                          \
            }                                                                    \
        throw std::invalid_argument("valor de enumeración no válido");           \
    }                                                                            \
    inline void from_json(const json& j, Type& value) {                          \
        static const std::pair<Type, const char*> names[] = __VA_ARGS__;         \
        const std::string text = j.get<std::string>();                           \
        for (const auto& entry : names)                                          \
            if (text == entry.second) {                                          \
                value = entry.first;                                             \
                return;                                                          \
            }                                                                    \
        throw std::invalid_argument("valor desconocido «" + text + "»");         \
    }

CC_ENUM(SteelFireMode, {{SteelFireMode::Ignore, "ignore"},
                        {SteelFireMode::CheckResistance, "checkResistance"},
                        {SteelFireMode::CriticalTemperature, "criticalTemperature"}})
CC_ENUM(ExposureClass, {{ExposureClass::NoRisk, "X0"},
                        {ExposureClass::Carbonation, "carbonation"},
                        {ExposureClass::Chlorides, "chlorides"},
                        {ExposureClass::SeaWaterChlorides, "seaWaterChlorides"},
                        {ExposureClass::FreezeThaw, "freezeThaw"},
                        {ExposureClass::Chemical, "chemical"}})
CC_ENUM(StrengthBasis, {{StrengthBasis::Relative, "relative"}, {StrengthBasis::Absolute, "absolute"}})
CC_ENUM(CombinationCode, {{CombinationCode::CTE, "CTE"}, {CombinationCode::EC, "EC"}})
CC_ENUM(LengthUnit, {{LengthUnit::Metres, "m"}, {LengthUnit::Centimetres, "cm"}, {LengthUnit::Millimetres, "mm"}})
CC_ENUM(BeamNumbering, {{BeamNumbering::None, "none"},
                        {BeamNumbering::Consecutive, "consecutive"},
                        {BeamNumbering::ByLevel, "byLevel"}})
CC_ENUM(ContinuousBeamNumbering::Axis,
        {{ContinuousBeamNumbering::Axis::X, "x"}, {ContinuousBeamNumbering::Axis::Y, "y"}})
CC_ENUM(ContinuousBeamNumbering::Sense,
        {{ContinuousBeamNumbering::Sense::Positive, "positive"}, {ContinuousBeamNumbering::Sense::Negative, "negative"}})
CC_ENUM(MaterialKind, {{MaterialKind::Steel, "steel"},
                       {MaterialKind::Concrete, "concrete"},
                       {MaterialKind::Timber, "timber"},
                       {MaterialKind::Aluminium, "aluminium"},
                       {MaterialKind::Generic, "generic"}})
CC_ENUM(ReinforcementTableSource, {{ReinforcementTableSource::Predefined, "predefined"},
                                   {ReinforcementTableSource::Library, "library"},
                                   {ReinforcementTableSource::Special, "special"}})

#define CC_STRUCT(Type, ...) NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(Type, __VA_ARGS__)

// --- project_settings.h ---
CC_STRUCT(StandardsSelection, concrete, coldFormedSteel, hotRolledSteel, timber, aluminium)
CC_STRUCT(TimberSettings, material, strengthClass, serviceClass)
CC_STRUCT(AluminiumSettings, alloy, temper)
CC_STRUCT(ProfileSettings, hotRolledSteel, coldFormedSteel, timber, aluminium, concrete)
CC_STRUCT(RebarSteelSettings, columnLongitudinal, columnTransverse, beamTopAssembly, beamTopReinforcement, beamBottom,
          beamTransverse, beamSkin, footings, pileCaps, tieBeams)
CC_STRUCT(AggregateSetting, kind, maxSize)
CC_STRUCT(AggregateSettings, columns, beams, foundationBeams, foundationMaxSizeMm)
CC_STRUCT(CoverSettings, columns, beamTop, beamBottom, beamSide, footingTop, footingBottom, footingSide, tieBeamTop,
          tieBeamBottom, tieBeamSide)
CC_STRUCT(WasteSettings, columns, beams, footings, pileCaps, tieBeams)
CC_STRUCT(ReinforcedConcreteSettings, columnConcrete, beamConcrete, foundationConcrete, rebarSteel, aggregate, cover, waste)
CC_STRUCT(ActionsSettings, dynamicSeismic, steelFire, foundationWind, foundationSeism, formworkRemovalDays)
CC_STRUCT(EnvironmentSettings, beams, maxCrackWidthMm)
CC_STRUCT(GroundSettings, checkFootingSliding, persistentStressMPa, seismicStressMPa)
CC_STRUCT(PileSinkingSettings, resistanceCalculation, reference, frictionAngle, cohesion, undrainedShear,
          unconfinedCompression, unitWeight, geotechnicalTip, geotechnicalShaft, geotechnicalTotal,
          geotechnicalShaftTension, unitTip, unitShaft, unitTotal, unitShaftTension, efficiencyFormula,
          ignoreEfficiencyWhenFar, efficiencyK)
CC_STRUCT(PileStructuralSettings, reduceConcreteStrength, precastBasis, precastValue, castInSituBasis, castInSituValue,
          undrainedShearFactor, unitWeightFactor)
CC_STRUCT(PileCapSettings, precastBasis, precastValue, castInSituBasis, castInSituValue)
CC_STRUCT(PileAdvancedSettings, sinking, structural, structuralCap, cap)
CC_STRUCT(PileSettings, minMomentEccentricityM, reducedAxial, shaftAxialFactor, tipOnlyAxialFactor, bucklingCoefficient,
          lateralStability, advanced)
CC_STRUCT(MicropileAdvancedSettings, sinking, undrainedShearFactor, unitWeightFactor)
CC_STRUCT(MicropileSettings, reducedAxial, reducedAxialFactor, bucklingAnalysis, bucklingCoefficient, lateralStability,
          corrosionThicknessMm, tubularAreaFactor, executionFactor, advanced)

// --- limit_states.h / units.h ---
CC_STRUCT(LimitStatesSettings, groundSnow, coldFormedSnow, timberSnow, hotRolledSnow, hotRolledCode)
CC_STRUCT(DimensioningUnits, dimensions, reinforcement, lengths)

// --- structure_model.h ---
CC_STRUCT(Point3, x, y, z)
CC_STRUCT(Node, id, position)
CC_STRUCT(Material, id, name, kind, elasticModulusMPa, poissonRatio, densityKnM3, thermalExpansion)
CC_STRUCT(CrossSection, id, name, kind, areaM2, inertiaYM4, inertiaZM4, torsionM4, shearAreaYM2, shearAreaZM2)
CC_STRUCT(Bar, id, startNode, endNode, materialId, sectionId, rollAngleDeg, xAxisInverted)
CC_STRUCT(Shell, id, nodeIds, thicknessM, materialId, zAxisInverted)

// --- options_settings.h ---
CC_STRUCT(BeamErrorSettings, compressionHook, topCompressionSteel, bottomCompressionSteel, manualShortCorbel,
          minStirrupDiameter, minStirrupDiameterMm, veryShortTruss, skinAnchorage, shortSpan, levelDifference, barTooLong,
          assemblyAnchorage, topAnchorage, topMultiLayer, bottomAnchorage, bottomMultiLayer)
CC_STRUCT(DeflectionRow, absolute, absoluteCm, mixed, mixedDenominator, mixedCm, relative, relativeDenominator)
CC_STRUCT(DeflectionLimitSet, instantSimple, instantCombination, infinite, active, damageableMonths, hasInfinite,
          hasDamageable)
CC_STRUCT(DeflectionLimitsSettings, materials)
CC_STRUCT(TransverseReinforcementTable, diameters, spacingsCm)
CC_STRUCT(DiameterAdmissibility, admissible, assemblyInFlanges)
CC_STRUCT(ReinforcementBarTable, diameters, minClearSpacingCm, assemblyVsFirst, firstVsSecond)
CC_STRUCT(BeamTablesSettings, transverse, stirrupHolder, skin, topReinforcement, bottomReinforcement)
CC_STRUCT(ContinuousBeamNumbering, baseReference, firstAxis, xFramesAlongY, yFramesAlongX)
CC_STRUCT(BeamOptionsSettings, errors, deflection, tables, continuousNumbering, numbering)
CC_STRUCT(JointMaterials, rolledSteel, anchorBoltSteel, concrete)
CC_STRUCT(BoltSettings, iso4014, iso4017, steelClass, diameters, unusualDiameters)
CC_STRUCT(StiffenerSettings, alwaysColumnBeam, alwaysRidge, withCutouts)
CC_STRUCT(BasePlateSettings, maxOverhangThickness, maxWidthThickness, maxRecommendedThicknessMm, groutThicknessCm,
          stiffenerBevelDeg, maxSidesBoltDiameter, boltPlateAnchorage)
CC_STRUCT(CleaningConcreteSettings, footings, pileCaps, tieAndCenteringBeams)
CC_STRUCT(CrackingSettings, checkFootings, checkBeams, footingLimitMm, beamLimitMm)
CC_STRUCT(PileCapGeneralSettings, minOverhangOverPileM, minOverhangOverSupportM, depthCriterion, minDepthM, maxDepthM,
          minBarSpacingM, maxBarDiameterMm, withPileLoads, loadFraction, loadFactor)
CC_STRUCT(CenteringBeamSettings, toleranceAngleDeg, stirrupsInsideFoundation, centeringType, minTensionGeometric,
          minTensionRatio)
CC_STRUCT(PileCapSingleSettings, armingDepthPercent, diagonalStirrups)
CC_STRUCT(PileCapBeamSettings, topHook, bottomHook, topHookMaxLengthM, bottomHookMaxLengthM, maxHoopDiameterMm,
          strutAngleDeg)
CC_STRUCT(PileCapGridSettings, designTopGrid, criterion, alternativeCriterion)
CC_STRUCT(PileCapReinforcementSettings, perimeterReinforcement, fromAxialKn)
CC_STRUCT(PileCapOptions, general, single, beams, grid, reinforcement)
CC_STRUCT(FootingPerimeterSettings, enabled, minDiameterMm, minBars, minAxialKn)
CC_STRUCT(FootingTypeSettings, equalReinforcement, squareMinOverhangCm, centredMinOverhangCm, centredMaxSideRatio,
          eccentricMinOverhangCm, eccentricMaxSideRatio, pyramidMaxAngleDeg, pyramidMinPedestalOverhangCm,
          pyramidMinEdgeDepthCm)
CC_STRUCT(FootingCombinedSettings, maxSpanDepthRatio, maxOverhangDepthRatio)
CC_STRUCT(FootingStressIncrease, persistentNoWind, persistentWind, accidental)
CC_STRUCT(FootingSettings, widthIncrementCm, depthIncrementCm, minDepthCm, minGeometricReinforcement, minGeometricRatio,
          reduceFlexureMinimums, topGridIfNeeded, equalHooksPerDirection, allowLiftOff, noTopGridIfPlainConcrete,
          defaultType, depthCriterion, perimeter, types, combined, stressIncrease)
CC_STRUCT(TieBeamSettings, compactionSurchargeKnPerM, minAxialPercent, stirrupsInsideFoundation)
CC_STRUCT(FoundationOptionsSettings, cleaning, cracking, footings, pileCap, centeringBeams, tieBeams, footingTable,
          centeringTable, tieBeamTable)
// The slip-factor class is a single letter; stored as a one-character string rather than a number.
inline void to_json(json& j, const PreloadedBoltSettings& s) {
    j = json{{"series", s.series},
             {"steelClass", s.steelClass},
             {"diameters", s.diameters},
             {"unusualDiameters", s.unusualDiameters},
             {"frictionClass", std::string(1, s.frictionClass)}};
}
inline void from_json(const json& j, PreloadedBoltSettings& s) {
    const PreloadedBoltSettings defaults;
    s.series = j.value("series", defaults.series);
    s.steelClass = j.value("steelClass", defaults.steelClass);
    s.diameters = j.value("diameters", defaults.diameters);
    s.unusualDiameters = j.value("unusualDiameters", defaults.unusualDiameters);
    const std::string friction = j.value("frictionClass", std::string(1, defaults.frictionClass));
    if (friction.size() != 1) throw std::invalid_argument("clase de rozamiento no válida");
    s.frictionClass = friction[0];
}

CC_STRUCT(JointOptionsSettings, materials, bolts, preloaded, stiffeners, basePlate)
CC_STRUCT(OptionsSettings, foundation, beams, joints)

namespace project_json {
namespace {

struct Section {
    const char* name;
    const char* title;
    std::function<std::string(const Project&)> dump;
    std::function<void(Project&, const std::string&)> load;
    std::function<bool(const Project&)> valid;
};

template <class T>
Section make(const char* name, const char* title, T Project::*member, std::function<bool(const T&)> valid) {
    return Section{name, title, [member](const Project& p) { return json(p.*member).dump(2); },
                   [member](Project& p, const std::string& text) { p.*member = json::parse(text).get<T>(); },
                   [member, valid](const Project& p) { return !valid || valid(p.*member); }};
}

/// A collection of the structural model, stored as {"next": <next free id>, "items": [...]}. Whole-model consistency
/// (references between collections) is checked by StructuralModel::validate, not here.
template <class T>
Section makeCollection(const char* name, const char* title, std::vector<T> StructuralModel::*items, int StructuralModel::*next) {
    return Section{
        name, title,
        [items, next](const Project& p) { return json{{"next", p.structure.*next}, {"items", p.structure.*items}}.dump(2); },
        [items, next](Project& p, const std::string& text) {
            const json j = json::parse(text);
            std::vector<T> loaded = j.value("items", std::vector<T>{});
            int following = 1;
            for (const T& item : loaded) following = std::max(following, item.id + 1);
            p.structure.*items = std::move(loaded);
            p.structure.*next = j.value("next", following);
        },
        [](const Project&) { return true; }};
}

const std::vector<Section>& sections() {
    static const std::vector<Section> all = {
        make<StandardsSelection>("standards", "Normas", &Project::standards, [](const auto& v) { return v.isValid(); }),
        make<ProfileSettings>("profiles", "Perfiles", &Project::profiles, [](const auto& v) { return v.isValid(); }),
        make<ReinforcedConcreteSettings>("concrete", "Hormigón armado", &Project::concrete,
                                         [](const auto& v) { return v.isValid(); }),
        make<ActionsSettings>("actions", "Acciones", &Project::actions, [](const auto& v) { return v.isValid(); }),
        make<EnvironmentSettings>("environment", "Ambiente", &Project::environment,
                                  [](const auto& v) { return v.isValid(); }),
        make<GroundSettings>("ground", "Terreno de cimentación", &Project::ground, [](const auto& v) { return v.isValid(); }),
        make<LimitStatesSettings>("limit_states", "Estados límite", &Project::limitStates,
                                  [](const auto& v) { return v.isValid(); }),
        make<PileSettings>("piles", "Pilotes", &Project::piles, [](const auto& v) { return v.isValid(); }),
        make<MicropileSettings>("micropiles", "Micropilotes", &Project::micropiles,
                                [](const auto& v) { return v.isValid(); }),
        make<OptionsSettings>("options", "Opciones", &Project::options, [](const auto& v) { return v.isValid(); }),
        make<DimensioningUnits>("units", "Unidades de acotación", &Project::units, nullptr),
        makeCollection<Node>("nodes", "Nudos", &StructuralModel::nodes, &StructuralModel::nextNodeId),
        makeCollection<Material>("materials", "Materiales", &StructuralModel::materials, &StructuralModel::nextMaterialId),
        makeCollection<CrossSection>("cross_sections", "Secciones", &StructuralModel::sections, &StructuralModel::nextSectionId),
        makeCollection<Bar>("bars", "Barras", &StructuralModel::bars, &StructuralModel::nextBarId),
        makeCollection<Shell>("shells", "Láminas", &StructuralModel::shells, &StructuralModel::nextShellId),
    };
    return all;
}

const Section* find(const std::string& name) {
    for (const Section& s : sections())
        if (name == s.name) return &s;
    return nullptr;
}

}  // namespace

const std::vector<std::string>& sectionNames() {
    static const std::vector<std::string> names = [] {
        std::vector<std::string> out;
        for (const Section& s : sections()) out.push_back(s.name);
        return out;
    }();
    return names;
}

bool isKnownSection(const std::string& name) { return find(name) != nullptr; }

std::string sectionTitle(const std::string& name) {
    const Section* s = find(name);
    return s ? s->title : name;
}

std::string dumpSection(const Project& project, const std::string& name) {
    const Section* s = find(name);
    if (!s) throw std::invalid_argument("Sección desconocida «" + name + "».");
    return s->dump(project);
}

void loadSection(Project& project, const std::string& name, const std::string& jsonText) {
    const Section* s = find(name);
    if (!s) throw std::invalid_argument("Sección desconocida «" + name + "».");
    try {
        s->load(project, jsonText);
    } catch (const std::exception& e) {
        throw std::runtime_error("La sección «" + std::string(s->title) +
                                 "» está dañada o no tiene un formato válido (" + e.what() + ").");
    }
}

std::string validate(const Project& project) {
    for (const Section& s : sections())
        if (!s.valid(project)) return "La sección «" + std::string(s.title) + "» contiene valores no válidos.";
    return project.structure.validate();
}

}  // namespace project_json
}  // namespace cc::core
