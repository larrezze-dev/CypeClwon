#include "core/project_json.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <functional>
#include <optional>
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
CC_ENUM(CombinationCode, {{CombinationCode::CTE, "CTE"}, {CombinationCode::EC, "EC"}, {CombinationCode::Mixed, "mixed"}})
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
CC_STRUCT(StandardsSelection, concrete, coldFormedSteel, hotRolledSteel, timber, aluminium, foundation)
CC_STRUCT(TimberSettings, material, strengthClass, serviceClass, fireResistance)
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
CC_ENUM(SituationType, {{SituationType::PersistentOrTransient, "persistent"}, {SituationType::Accidental, "accidental"},
                        {SituationType::Seismic, "seismic"}, {SituationType::PrestressInVacuum, "prestressInVacuum"},
                        {SituationType::Service, "service"}})
CC_ENUM(CoefficientMode, {{CoefficientMode::PartialFactors, "partialFactors"}, {CoefficientMode::PartialAndCombination, "partialAndCombination"}})
CC_STRUCT(SituationRow, favourable, unfavourable, principal, accompanying)
CC_STRUCT(UserSituation, name, type, permanent, prestress, shrinkage, wind, snow, temperature, earthThrust, imposed, seismic)
CC_STRUCT(UserSituationSet, reference, mode, categories, situations, service)
CC_STRUCT(LimitStatesSettings, groundSnow, coldFormedSnow, timberSnow, hotRolledSnow, hotRolledCode, codeChosen, userSets, userSetOfKind,
          userServiceSetOfKind)
CC_STRUCT(DimensioningUnits, dimensions, reinforcement, lengths)

// --- structure_model.h ---
CC_STRUCT(Point3, x, y, z)
CC_STRUCT(Node, id, position, reference)
CC_STRUCT(Material, id, name, kind, elasticModulusMPa, poissonRatio, densityKnM3, thermalExpansion)
CC_STRUCT(CrossSection, id, name, kind, areaM2, inertiaYM4, inertiaZM4, torsionM4, shearAreaYM2, shearAreaZM2)
CC_STRUCT(Layer, id, name, color, active, visible, locked, rendered)
CC_ENUM(ElementType, {{ElementType::Generic, "generic"}, {ElementType::Pillar, "pillar"}, {ElementType::Beam, "beam"}})
CC_STRUCT(Piece, id, barIds, nodeIds, type, groupId)
CC_STRUCT(SlabDepth, nodeId, depthM)
CC_STRUCT(ContinuousColumn, id, pieceIds, reference)
CC_STRUCT(ContinuousBeam, id, pieceIds, reference)
CC_STRUCT(PieceReference, pieceId, text)
CC_STRUCT(BeamDeflectionLimit, relative, absoluteMm)
CC_STRUCT(DeflectionGroup, id, barIds, limit)
CC_STRUCT(ContinuousElements, columns, beams, pieceReferences, slabDepths, deflectionGroups, nextColumnId, nextBeamId, nextGroupId)
CC_STRUCT(Bar, id, startNode, endNode, materialId, sectionId, rollAngleDeg, xAxisInverted, layerIds)
CC_ENUM(BallastBehaviour, {{BallastBehaviour::Linear, "linear"}, {BallastBehaviour::CompressionOnly, "compressionOnly"}})
CC_ENUM(ShellMaterialKind, {{ShellMaterialKind::Concrete, "concrete"},
                            {ShellMaterialKind::RolledSteel, "rolledSteel"},
                            {ShellMaterialKind::ColdFormedSteel, "coldFormedSteel"},
                            {ShellMaterialKind::Aluminium, "aluminium"},
                            {ShellMaterialKind::GenericIsotropic, "genericIsotropic"},
                            {ShellMaterialKind::GenericOrthotropic, "genericOrthotropic"}})
CC_STRUCT(ShellMaterial, kind, useGeneralData, grade, alloy, temper, isoElasticModulusMPa, isoPoissonRatio, isoThermalExpansion, isoDensityKnM3,
          orthoElasticModulusXMPa, orthoElasticModulusYMPa, orthoPoissonRatioXY, orthoShearModulusXYMPa, orthoShearModulusXZMPa,
          orthoShearModulusYZMPa, orthoThermalExpansionX, orthoThermalExpansionY, orthoDensityKnM3)
CC_STRUCT(ShellLayout, considerEccentricity, centralPlaneOffsetM)
CC_STRUCT(Shell, id, nodeIds, thicknessM, materialId, zAxisInverted, xEdgeStart, xEdgeEnd, ballastEnabled, ballastKnM3, ballastBehaviour,
          material, layout, reference, layerIds)

// --- node_support.h (T-P6-01) ---
CC_ENUM(ExternalSupportType, {{ExternalSupportType::Free, "free"}, {ExternalSupportType::Pinned, "pinned"}, {ExternalSupportType::Generic, "generic"},
                              {ExternalSupportType::Fixed, "fixed"}, {ExternalSupportType::LineXFixed, "lineXFixed"},
                              {ExternalSupportType::LineYFixed, "lineYFixed"}, {ExternalSupportType::LineZFixed, "lineZFixed"},
                              {ExternalSupportType::LineUserFixed, "lineUserFixed"}, {ExternalSupportType::PlaneXYFixed, "planeXYFixed"},
                              {ExternalSupportType::PlaneXZFixed, "planeXZFixed"}, {ExternalSupportType::PlaneYZFixed, "planeYZFixed"},
                              {ExternalSupportType::PlaneUserFixed, "planeUserFixed"}, {ExternalSupportType::LineXPinned, "lineXPinned"},
                              {ExternalSupportType::LineYPinned, "lineYPinned"}, {ExternalSupportType::LineZPinned, "lineZPinned"},
                              {ExternalSupportType::LineUserPinned, "lineUserPinned"}, {ExternalSupportType::PlaneXYPinned, "planeXYPinned"},
                              {ExternalSupportType::PlaneXZPinned, "planeXZPinned"}, {ExternalSupportType::PlaneYZPinned, "planeYZPinned"},
                              {ExternalSupportType::PlaneUserPinned, "planeUserPinned"}})
CC_ENUM(DofMode, {{DofMode::Free, "free"}, {DofMode::Fixed, "fixed"}, {DofMode::Elastic, "elastic"}})
CC_ENUM(SupportSense, {{SupportSense::Both, "both"}, {SupportSense::Positive, "positive"}, {SupportSense::Negative, "negative"}})
CC_ENUM(GenericDisplacementMode, {{GenericDisplacementMode::Independent, "independent"}, {GenericDisplacementMode::OnLine, "onLine"},
                                  {GenericDisplacementMode::OnPlane, "onPlane"}})
CC_ENUM(InternalLinkType, {{InternalLinkType::Fixed, "fixed"}, {InternalLinkType::PartialFixity, "partialFixity"},
                           {InternalLinkType::Pinned, "pinned"}, {InternalLinkType::Generic, "generic"}})
CC_STRUCT(GenericSupportData, mode, translations, vector, rotations)
CC_STRUCT(PrescribedDisplacement, loadCaseId, values)
CC_STRUCT(ExternalSupport, type, generic, userVector, elasticDofs, elasticConstants, senses, prescribed)
CC_STRUCT(GenericLinkRow, partialToNode, coefficient)
CC_STRUCT(InternalLink, type, partialCoefficient, elementRow, rows)

// --- bar_description.h (T-P5-07) ---
CC_ENUM(BarMaterialIcon, {{BarMaterialIcon::RolledSteel, "rolledSteel"},
                          {BarMaterialIcon::RolledSteelFamily2, "rolledSteelFamily2"},
                          {BarMaterialIcon::ColdFormedSteel, "coldFormedSteel"},
                          {BarMaterialIcon::Timber, "timber"},
                          {BarMaterialIcon::ExtrudedAluminium, "extrudedAluminium"},
                          {BarMaterialIcon::SpecialAluminium, "specialAluminium"},
                          {BarMaterialIcon::Concrete, "concrete"},
                          {BarMaterialIcon::Generic, "generic"}})
CC_ENUM(Arrangement, {{Arrangement::Simple, "simple"},
                      {Arrangement::SimpleWithHaunches, "simpleWithHaunches"},
                      {Arrangement::WithConcreteSlab, "withConcreteSlab"},
                      {Arrangement::DoubleBoxWelded, "doubleBoxWelded"},
                      {Arrangement::DoubleBoxBattened, "doubleBoxBattened"},
                      {Arrangement::DoubleBoxGeneric, "doubleBoxGeneric"},
                      {Arrangement::HalfProfile, "halfProfile"},
                      {Arrangement::SideFlatPlates, "sideFlatPlates"},
                      {Arrangement::Boyd, "boyd"},
                      {Arrangement::DoubleIWelded, "doubleIWelded"},
                      {Arrangement::DoubleIBattened, "doubleIBattened"},
                      {Arrangement::DoubleIGeneric, "doubleIGeneric"},
                      {Arrangement::DoubleBoxFlatPlates, "doubleBoxFlatPlates"}})
CC_ENUM(BehaviourKind, {{BehaviourKind::Linear, "linear"},
                        {BehaviourKind::TensionOnly, "tensionOnly"},
                        {BehaviourKind::CompressionOnly, "compressionOnly"}})
CC_STRUCT(WebStiffeners, present, spacingM, thicknessM)
CC_STRUCT(GenericSectionData, widthLeftM, widthRightM, depthBottomM, depthTopM, areaM2, shearAreaYM2, shearAreaZM2, inertiaYM4,
          inertiaZM4, torsionM4)
CC_STRUCT(MaterialData, elasticModulusMPa, poissonRatio, thermalExpansion, densityKnM3)

CC_ENUM(PillarMaterial, {{PillarMaterial::Concrete, "concrete"}, {PillarMaterial::Steel, "steel"}, {PillarMaterial::Mixed, "mixed"}})
CC_ENUM(PillarConcreteKind, {{PillarConcreteKind::Rectangular, "rectangular"}, {PillarConcreteKind::Circular, "circular"}})
CC_ENUM(MixedPillarKind, {{MixedPillarKind::RectangularEmbeddedProfile, "rectangularEmbeddedProfile"},
                          {MixedPillarKind::RectangularBox, "rectangularBox"},
                          {MixedPillarKind::RectangularTube, "rectangularTube"},
                          {MixedPillarKind::SquareTube, "squareTube"},
                          {MixedPillarKind::CircularEmbeddedProfile, "circularEmbeddedProfile"},
                          {MixedPillarKind::CircularTube, "circularTube"}})
CC_ENUM(BeamFamily, {{BeamFamily::Flat, "flat"}, {BeamFamily::Hung, "hung"}, {BeamFamily::Lattice, "lattice"},
                     {BeamFamily::Prestressed, "prestressed"}, {BeamFamily::Steel, "steel"}, {BeamFamily::Timber, "timber"}})

inline ShapeKind readShape(const json& j, const char* key, ShapeKind fallback) {
    if (!j.contains(key)) return fallback;
    const auto shape = shapeFromKey(j.at(key).get<std::string>());
    if (!shape) throw std::invalid_argument("forma de perfil desconocida");
    return *shape;
}
inline void to_json(json& j, const TieData& t) { j = json{{"shape", t.shape}, {"seriesRef", t.seriesRef}, {"profileRef", t.profileRef}}; }
inline void from_json(const json& j, TieData& t) {
    const TieData defaults;
    t.shape = j.value("shape", defaults.shape);
    t.seriesRef = j.value("seriesRef", defaults.seriesRef);
    t.profileRef = j.value("profileRef", defaults.profileRef);
}
inline void to_json(json& j, const PillarData& p) {
    j = json{{"material", p.material},   {"concreteKind", p.concreteKind}, {"mixedKind", p.mixedKind}, {"widthXM", p.widthXM},
             {"widthYM", p.widthYM},     {"diameterM", p.diameterM},       {"seriesRef", p.seriesRef}, {"profileRef", p.profileRef},
             {"shape", shapeKey(p.shape)},       {"arrangement", p.arrangement}};
}
inline void from_json(const json& j, PillarData& p) {
    const PillarData d;
    p.material = j.value("material", d.material);
    p.concreteKind = j.value("concreteKind", d.concreteKind);
    p.mixedKind = j.value("mixedKind", d.mixedKind);
    p.widthXM = j.value("widthXM", d.widthXM);
    p.widthYM = j.value("widthYM", d.widthYM);
    p.diameterM = j.value("diameterM", d.diameterM);
    p.seriesRef = j.value("seriesRef", d.seriesRef);
    p.profileRef = j.value("profileRef", d.profileRef);
    p.shape = readShape(j, "shape", d.shape);
    p.arrangement = j.value("arrangement", d.arrangement);  // absent in older files: Perfil simple
}
inline void to_json(json& j, const BeamData& b) {
    j = json{{"family", b.family},           {"type", b.type},           {"widthM", b.widthM},
             {"depthM", b.depthM},           {"flangeLeftM", b.flangeLeftM}, {"flangeRightM", b.flangeRightM},
             {"flangeDepthM", b.flangeDepthM}, {"depthStartM", b.depthStartM}, {"depthEndM", b.depthEndM},
             {"belowSlab", b.belowSlab},     {"seriesRef", b.seriesRef}, {"profileRef", b.profileRef},
             {"shape", shapeKey(b.shape)},   {"timberSeriesRef", b.timberSeriesRef}, {"timberProfileRef", b.timberProfileRef},
             {"timberShape", shapeKey(b.timberShape)}, {"arrangement", b.arrangement}};
}
inline void from_json(const json& j, BeamData& b) {
    const BeamData d;
    b.family = j.value("family", d.family);
    b.type = j.value("type", d.type);
    b.widthM = j.value("widthM", d.widthM);
    b.depthM = j.value("depthM", d.depthM);
    b.flangeLeftM = j.value("flangeLeftM", d.flangeLeftM);
    b.flangeRightM = j.value("flangeRightM", d.flangeRightM);
    b.flangeDepthM = j.value("flangeDepthM", d.flangeDepthM);
    b.depthStartM = j.value("depthStartM", d.depthStartM);
    b.depthEndM = j.value("depthEndM", d.depthEndM);
    b.belowSlab = j.value("belowSlab", d.belowSlab);
    b.seriesRef = j.value("seriesRef", d.seriesRef);
    b.profileRef = j.value("profileRef", d.profileRef);
    b.shape = readShape(j, "shape", d.shape);
    b.timberSeriesRef = j.value("timberSeriesRef", d.timberSeriesRef);
    b.timberProfileRef = j.value("timberProfileRef", d.timberProfileRef);
    b.timberShape = readShape(j, "timberShape", d.timberShape);
    b.arrangement = j.value("arrangement", d.arrangement);  // absent in older files: Perfil simple
    if (b.type < 0 || b.type >= std::max(1, beamTypeCount(b.family))) throw std::invalid_argument("tipo de viga no válido");
}

inline void to_json(json& j, const BarDescription& d) {
    j = json{{"id", d.id},
             {"elementType", d.elementType},
             {"tieSelected", d.tieSelected},
             {"tie", d.tie},
             {"pillar", d.pillar},
             {"beam", d.beam},
             {"behaviour", d.behaviour},
             {"materialIcon", d.materialIcon},
             {"shape", shapeKey(d.shape)},
             {"seriesRef", d.seriesRef},
             {"profileRef", d.profileRef},
             {"arrangement", d.arrangement},
             {"stiffeners", d.stiffeners},
             {"generic", d.generic},
             {"material", d.material},
             {"depthStartM", d.depthStartM},
             {"depthEndM", d.depthEndM},
             {"sectionId", d.sectionId},
             {"materialId", d.materialId}};
    if (d.inlineSection)
        j["inlineSection"] = json{{"reference", d.inlineSection->reference},
                                  {"kind", shapeKey(d.inlineSection->kind)},
                                  {"dims", d.inlineSection->dims}};
}
inline void from_json(const json& j, BarDescription& d) {
    const BarDescription defaults;
    d.id = j.value("id", defaults.id);
    d.elementType = j.value("elementType", defaults.elementType);
    d.tieSelected = j.value("tieSelected", defaults.tieSelected);
    d.tie = j.value("tie", defaults.tie);
    d.pillar = j.value("pillar", defaults.pillar);
    d.beam = j.value("beam", defaults.beam);
    d.behaviour = j.value("behaviour", defaults.behaviour);
    d.materialIcon = j.value("materialIcon", defaults.materialIcon);
    const auto shape = shapeFromKey(j.value("shape", std::string(shapeKey(defaults.shape))));
    if (!shape) throw std::invalid_argument("forma de perfil desconocida");
    d.shape = *shape;
    d.seriesRef = j.value("seriesRef", defaults.seriesRef);
    d.profileRef = j.value("profileRef", defaults.profileRef);
    d.arrangement = j.value("arrangement", defaults.arrangement);
    d.stiffeners = j.value("stiffeners", defaults.stiffeners);
    d.generic = j.value("generic", defaults.generic);
    d.material = j.value("material", defaults.material);
    d.depthStartM = j.value("depthStartM", defaults.depthStartM);
    d.depthEndM = j.value("depthEndM", defaults.depthEndM);
    d.sectionId = j.value("sectionId", 0);
    d.materialId = j.value("materialId", 0);
    d.inlineSection.reset();
    if (j.contains("inlineSection")) {
        const json& s = j.at("inlineSection");
        Section section;
        section.reference = s.at("reference").get<std::string>();
        const auto kind = shapeFromKey(s.at("kind").get<std::string>());
        if (!kind) throw std::invalid_argument("forma de perfil desconocida");
        section.kind = *kind;
        section.dims = s.at("dims").get<std::vector<double>>();
        d.inlineSection = section;
    }
}

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

json pointToJson(const Point3& p) { return json::array({p.x, p.y, p.z}); }
json pointsToJson(const std::vector<Point3>& points) {
    json a = json::array();
    for (const Point3& p : points) a.push_back(pointToJson(p));
    return a;
}
Point3 pointFromJson(const json& j) {
    Point3 p;
    if (j.is_array() && j.size() >= 3) {
        p.x = j[0].get<double>();
        p.y = j[1].get<double>();
        p.z = j[2].get<double>();
    }
    return p;
}
std::vector<Point3> pointsFromJson(const json& j) {
    std::vector<Point3> out;
    if (j.is_array())
        for (const json& e : j) out.push_back(pointFromJson(e));
    return out;
}
json surfaceLoadDataToJson(const SurfaceLoadData& d) {
    return json{{"load_case", d.loadCaseId}, {"type", static_cast<int>(d.type)}, {"value", d.value}, {"lower", d.lower}, {"upper", d.upper}};
}
/// Tolerant: nullopt for an unknown type; the canonical form (rounded, unused fields reset).
std::optional<SurfaceLoadData> surfaceLoadDataFromJson(const json& j) {
    const int type = j.value("type", 0);
    if (type < 0 || type > 1) return std::nullopt;
    SurfaceLoadData d;
    d.loadCaseId = j.value("load_case", 0);
    d.type = static_cast<SurfaceLoadType>(type);
    d.value = j.value("value", 0.0);
    d.lower = j.value("lower", 0.0);
    d.upper = j.value("upper", 0.0);
    return normaliseSurfaceLoadData(d);
}

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
        Section{"display_options", "Opciones de visualización",
                [](const Project& p) { return toJson(p.display); },
                [](Project& p, const std::string& text) {
                    const auto parsed = displayOptionsFromJson(text);
                    if (!parsed) throw std::invalid_argument("valores no válidos");
                    p.display = *parsed;
                },
                [](const Project& p) { return isValid(p.display); }},
        Section{"categories", "Categorías",
                [](const Project& p) { return toJson(p.categories); },
                [](Project& p, const std::string& text) {
                    const auto parsed = categorySettingsFromJson(text);
                    if (!parsed) throw std::invalid_argument("valores no válidos");
                    p.categories = *parsed;
                },
                [](const Project&) { return true; }},
        Section{"work_planes", "Planos",
                [](const Project& p) { return toJson(p.workPlanes); },
                [](Project& p, const std::string& text) {
                    const auto parsed = workPlanesFromJson(text);
                    if (!parsed) throw std::invalid_argument("valores no válidos");
                    p.workPlanes = *parsed;
                },
                [](const Project& p) { return p.workPlanes.isValid(); }},
        Section{"profile_library", "Biblioteca de perfiles",
                [](const Project& p) { return toJson(p.profileLibrary); },
                [](Project& p, const std::string& text) {
                    const auto parsed = profileLibraryFromJson(text);
                    if (!parsed) throw std::invalid_argument("valores no válidos");
                    p.profileLibrary = *parsed;
                },
                [](const Project& p) { return validateLibrary(p.profileLibrary).empty(); }},
        makeCollection<Node>("nodes", "Nudos", &StructuralModel::nodes, &StructuralModel::nextNodeId),
        makeCollection<Material>("materials", "Materiales", &StructuralModel::materials, &StructuralModel::nextMaterialId),
        makeCollection<CrossSection>("cross_sections", "Secciones", &StructuralModel::sections, &StructuralModel::nextSectionId),
        makeCollection<Bar>("bars", "Barras", &StructuralModel::bars, &StructuralModel::nextBarId),
        makeCollection<Shell>("shells", "Láminas", &StructuralModel::shells, &StructuralModel::nextShellId),
        makeCollection<Piece>("pieces", "Piezas", &StructuralModel::pieces, &StructuralModel::nextPieceId),
        makeCollection<Layer>("layers", "Capas", &StructuralModel::layers, &StructuralModel::nextLayerId),
        make<ContinuousElements>("continuous_elements", "Pilares y vigas continuos", &Project::continuous,
                                 [](const auto& v) { return v.isValid(); }),
        Section{"bar_descriptions", "Descripciones de barras",
                [](const Project& p) { return json{{"next", p.barDescriptions.next}, {"items", p.barDescriptions.items}}.dump(2); },
                [](Project& p, const std::string& text) {
                    const json j = json::parse(text);
                    BarDescriptions loaded;
                    loaded.items = j.value("items", std::vector<BarDescription>{});
                    int following = 1;
                    for (const BarDescription& d : loaded.items) following = std::max(following, d.id + 1);
                    loaded.next = j.value("next", following);
                    p.barDescriptions = std::move(loaded);
                },
                [](const Project& p) {
                    int previous = 0;
                    for (const BarDescription& d : p.barDescriptions.items) {
                        if (d.id <= previous || d.id >= p.barDescriptions.next) return false;
                        previous = d.id;
                        if (!p.structure.findSection(d.sectionId) || !p.structure.findMaterial(d.materialId)) return false;
                    }
                    return true;
                }},
        Section{"node_supports", "Vinculaciones de nudos",
                [](const Project& p) {
                    // Entries of nodes that no longer exist (deleted by an operation that does not know the supports) are not saved.
                    json ext = json::array(), in = json::array();
                    for (const auto& [id, v] : p.nodeSupports.external)
                        if (p.structure.findNode(id)) ext.push_back({{"node", id}, {"support", v}});
                    for (const auto& [id, v] : p.nodeSupports.internal)
                        if (p.structure.findNode(id)) in.push_back({{"node", id}, {"link", v}});
                    return json{{"external", ext}, {"internal", in}}.dump(2);
                },
                [](Project& p, const std::string& text) {
                    const json j = json::parse(text);
                    NodeSupports loaded;
                    if (j.contains("external"))
                        for (const json& e : j.at("external")) loaded.external[e.at("node").get<int>()] = e.at("support").get<ExternalSupport>();
                    if (j.contains("internal"))
                        for (const json& e : j.at("internal")) loaded.internal[e.at("node").get<int>()] = e.at("link").get<InternalLink>();
                    p.nodeSupports = std::move(loaded);
                },
                [](const Project& p) { return validateSupports(p.nodeSupports).empty(); }},
        Section{"node_ties", "Ligaduras de nudos",
                [](const Project& p) {
                    // Nodes that no longer exist are not saved; ties left with < 2 nodes by that are dropped, ties of a single node that were
                    // already there are kept.
                    json dirs = json::object();
                    static const char* const names[3] = {"dx", "dy", "dz"};
                    for (int d = 0; d < 3; ++d) {
                        json list = json::array();
                        for (const Ligature& t : p.ligatures.dirs[static_cast<std::size_t>(d)]) {
                            std::vector<int> nodes;
                            for (int n : t.nodeIds)
                                if (p.structure.findNode(n)) nodes.push_back(n);
                            if (nodes.size() == t.nodeIds.size() || nodes.size() >= 2) list.push_back({{"id", t.id}, {"nodes", nodes}});
                        }
                        dirs[names[d]] = list;
                    }
                    return json{{"next", p.ligatures.nextId}, {"ties", dirs}}.dump(2);
                },
                [](Project& p, const std::string& text) {
                    const json j = json::parse(text);
                    NodeLigatures loaded;
                    static const char* const names[3] = {"dx", "dy", "dz"};
                    int following = 1;
                    if (j.contains("ties"))
                        for (int d = 0; d < 3; ++d) {
                            if (!j.at("ties").contains(names[d])) continue;
                            for (const json& e : j.at("ties").at(names[d])) {
                                Ligature t;
                                t.id = e.at("id").get<int>();
                                t.nodeIds = e.at("nodes").get<std::vector<int>>();
                                following = std::max(following, t.id + 1);
                                loaded.dirs[static_cast<std::size_t>(d)].push_back(std::move(t));
                            }
                        }
                    loaded.nextId = j.value("next", following);
                    p.ligatures = std::move(loaded);
                },
                [](const Project& p) { return validateLigatures(p.ligatures, p.structure).empty(); }},
        Section{"diaphragms", "Diafragmas rígidos",
                [](const Project& p) {
                    // Nodes that no longer exist are not saved.
                    json items = json::array();
                    for (const Diaphragm& d : p.diaphragms.items) {
                        std::vector<int> nodes;
                        for (int n : d.nodeIds)
                            if (p.structure.findNode(n)) nodes.push_back(n);
                        json loads = json::array();
                        for (const DiaphragmLoad& l : d.loads)
                            loads.push_back({{"load_case", l.loadCaseId}, {"fx", l.fxKn}, {"fy", l.fyKn}, {"x", l.xM}, {"y", l.yM}});
                        items.push_back({{"id", d.id}, {"reference", d.reference}, {"color", d.color}, {"nodes", nodes}, {"shells", d.shellIds}, {"loads", loads}});
                    }
                    return json{{"next", p.diaphragms.nextId}, {"items", items}}.dump(2);
                },
                [](Project& p, const std::string& text) {
                    const json j = json::parse(text);
                    Diaphragms loaded;
                    int following = 1;
                    if (j.contains("items"))
                        for (const json& e : j.at("items")) {
                            Diaphragm d;
                            d.id = e.at("id").get<int>();
                            d.reference = e.value("reference", std::string{});
                            d.color = e.value("color", kDefaultDiaphragmColor);
                            d.nodeIds = e.value("nodes", std::vector<int>{});
                            std::sort(d.nodeIds.begin(), d.nodeIds.end());
                            d.shellIds = e.value("shells", std::vector<int>{});
                            std::sort(d.shellIds.begin(), d.shellIds.end());
                            if (e.contains("loads"))
                                for (const json& l : e.at("loads")) {
                                    DiaphragmLoad load;
                                    load.loadCaseId = l.value("load_case", 0);
                                    load.fxKn = l.value("fx", 0.0);
                                    load.fyKn = l.value("fy", 0.0);
                                    load.xM = l.value("x", 0.0);
                                    load.yM = l.value("y", 0.0);
                                    d.loads.push_back(load);
                                }
                            following = std::max(following, d.id + 1);
                            loaded.items.push_back(std::move(d));
                        }
                    loaded.nextId = j.value("next", following);
                    p.diaphragms = std::move(loaded);
                },
                [](const Project& p) { return validateDiaphragms(p.diaphragms, p.structure).empty(); }},
        Section{"bar_ends", "Extremos de barra (articulaciones y empotramiento parcial)",
                [](const Project& p) {
                    // Bars that no longer exist are not saved.
                    json bars = json::array();
                    for (const auto& [id, rec] : p.barEnds.bars) {
                        if (!p.structure.findBar(id)) continue;
                        json ends = json::array();
                        for (const BarEndFixity& e : rec.ends) {
                            json planes = json::array();
                            for (const EndPlaneFixity& f : e.planes)
                                planes.push_back({{"rotational", f.kind == EndFixityKind::Rotational}, {"coefficient", f.coefficient}, {"stiffness", f.stiffnessKnMPerRad}});
                            ends.push_back({{"pinned", e.pinned}, {"planes", planes}});
                        }
                        bars.push_back({{"bar", id}, {"ends", ends}});
                    }
                    return json{{"bars", bars}}.dump(2);
                },
                [](Project& p, const std::string& text) {
                    const json j = json::parse(text);
                    BarEnds loaded;
                    if (j.contains("bars"))
                        for (const json& b : j.at("bars")) {
                            BarEndsRecord rec;
                            const json& ends = b.at("ends");
                            for (std::size_t e = 0; e < 2 && e < ends.size(); ++e) {
                                rec.ends[e].pinned = ends[e].value("pinned", false);
                                if (!ends[e].contains("planes")) continue;
                                const json& planes = ends[e].at("planes");
                                for (std::size_t k = 0; k < 3 && k < planes.size(); ++k) {
                                    EndPlaneFixity& f = rec.ends[e].planes[k];
                                    f.kind = planes[k].value("rotational", false) ? EndFixityKind::Rotational : EndFixityKind::Coefficient;
                                    f.coefficient = planes[k].value("coefficient", 1.0);
                                    f.stiffnessKnMPerRad = planes[k].value("stiffness", kDefaultEndRotationalStiffness);
                                }
                            }
                            setBarEnds(loaded, b.at("bar").get<int>(), rec);
                        }
                    p.barEnds = std::move(loaded);
                },
                [](const Project& p) { return validateBarEnds(p.barEnds, p.structure).empty(); }},
        Section{"buckling", "Pandeo de barras y pandeo lateral",
                [](const Project& p) {
                    // Bars that no longer exist are not saved. The kind (the radio) is persisted together with beta and the
                    // lengths: measured, a typed 0 / 0.5 stays "Otros valores" when the dialog is reopened.
                    auto plane = [](const BucklingPlane& b) {
                        return json{{"kind", static_cast<int>(b.kind)}, {"beta", b.beta}, {"length", b.lengthM}, {"translational", b.translational}, {"moment", b.momentCoeff}, {"reviewed", b.reviewed}};
                    };
                    auto flange = [](const LateralBuckling& f) {
                        return json{{"kind", static_cast<int>(f.kind)}, {"coeff", f.coeff}, {"spacing", f.spacingM}, {"moment", f.momentCoeff}, {"reviewed", f.reviewed}};
                    };
                    json bars = json::array();
                    for (const auto& [id, r] : p.buckling.bars) {
                        if (!p.structure.findBar(id)) continue;
                        bars.push_back({{"bar", id}, {"c1", r.c1}, {"consider", r.considerInAnalysis}, {"updateXY", r.updateXY}, {"updateXZ", r.updateXZ}, {"xy", plane(r.xy)}, {"xz", plane(r.xz)}, {"top", flange(r.top)}, {"bottom", flange(r.bottom)}});
                    }
                    return json{{"bars", bars}}.dump(2);
                },
                [](Project& p, const std::string& text) {
                    const json j = json::parse(text);
                    auto plane = [](const json& o) {
                        BucklingPlane b;
                        const int kind = o.value("kind", static_cast<int>(BucklingKind::PinnedPinned));
                        b.kind = static_cast<BucklingKind>(kind);
                        b.beta = o.value("beta", 1.0);
                        b.lengthM = o.value("length", 1.0);
                        b.translational = o.value("translational", false);
                        b.momentCoeff = o.value("moment", 1.0);
                        b.reviewed = o.value("reviewed", false);
                        return b;
                    };
                    auto flange = [](const json& o) {
                        LateralBuckling f;
                        f.kind = static_cast<LateralKind>(o.value("kind", 0));
                        f.coeff = o.value("coeff", 1.0);
                        f.spacingM = o.value("spacing", kDefaultLateralSpacingM);
                        f.momentCoeff = o.value("moment", 1.0);
                        f.reviewed = o.value("reviewed", false);
                        return f;
                    };
                    BucklingData loaded;
                    if (j.contains("bars"))
                        for (const json& b : j.at("bars")) {
                            BarBuckling r;
                            r.c1 = b.value("c1", 1.0);
                            r.considerInAnalysis = b.value("consider", true);
                            r.updateXY = b.value("updateXY", false);
                            r.updateXZ = b.value("updateXZ", false);
                            if (b.contains("xy")) r.xy = plane(b.at("xy"));
                            if (b.contains("xz")) r.xz = plane(b.at("xz"));
                            if (b.contains("top")) r.top = flange(b.at("top"));
                            if (b.contains("bottom")) r.bottom = flange(b.at("bottom"));
                            setBuckling(loaded, b.at("bar").get<int>(), r);
                        }
                    p.buckling = std::move(loaded);
                },
                [](const Project& p) { return validateBuckling(p.buckling, p.structure).empty(); }},
        Section{"deflection_groups", "Grupos de flecha",
                [](const Project& p) {
                    // Only the groups that differ from "one secant group per bar"; bars that no longer exist are not saved.
                    json groups = json::array();
                    for (const DeflectionBarGroup& g : normaliseDeflection(p.deflection, p.structure).groups) {
                        json limits = json::array();
                        for (const DeflectionLimitItem& it : g.limits.items)
                            limits.push_back({{"enabled", it.enabled}, {"absolute_mm", it.absoluteMm}, {"divisor", it.relativeDivisor}});
                        groups.push_back({{"bars", g.bars}, {"node1", g.node1}, {"xy", static_cast<int>(g.xy)}, {"xz", static_cast<int>(g.xz)}, {"limits", limits}});
                    }
                    return json{{"groups", groups}}.dump(2);
                },
                [](Project& p, const std::string& text) {
                    const json j = json::parse(text);
                    DeflectionData loaded;
                    if (j.contains("groups"))
                        for (const json& e : j.at("groups")) {
                            DeflectionBarGroup g;
                            g.bars = e.value("bars", std::vector<int>{});
                            g.node1 = e.value("node1", 0);
                            g.xy = static_cast<DeflectionType>(e.value("xy", 0));
                            g.xz = static_cast<DeflectionType>(e.value("xz", 0));
                            if (e.contains("limits")) {
                                const json& limits = e.at("limits");
                                for (std::size_t i = 0; i < limits.size() && i < g.limits.items.size(); ++i) {
                                    g.limits.items[i].enabled = limits[i].value("enabled", false);
                                    g.limits.items[i].absoluteMm = limits[i].value("absolute_mm", kDefaultAbsoluteDeflectionLimit);
                                    g.limits.items[i].relativeDivisor = limits[i].value("divisor", kDefaultDeflectionDivisor);
                                }
                            }
                            if (!g.bars.empty()) loaded.groups.push_back(std::move(g));
                        }
                    p.deflection = std::move(loaded);
                },
                [](const Project& p) { return validateDeflection(p.deflection).empty(); }},
        Section{"load_cases", "Hipótesis de carga",
                [](const Project& p) {
                    // Versioned. "Peso propio" (id 0) is implicit and not stored.
                    const LoadCaseSet& d = p.loadCases;
                    json cases = json::array();
                    for (const LoadCase& c : d.cases) {
                        if (c.id == 0) continue;
                        cases.push_back({{"id", c.id}, {"nature", static_cast<int>(c.nature)}, {"reference", c.reference}, {"description", c.description},
                                         {"acts", c.acts}, {"arrangements", c.arrangements}, {"compat", static_cast<int>(c.compat)}, {"seismic", static_cast<int>(c.seismic)}});
                    }
                    json categories = json::array();
                    for (const UseCategory& u : d.useCategories)
                        categories.push_back({{"ref", u.ref}, {"description", u.description}, {"choices", u.choices}});
                    return json{{"version", 1}, {"cases", cases}, {"created", d.created}, {"nextId", d.nextId},
                                {"useCategories", categories}, {"seismicSecondOrder", d.seismicSecondOrder}}.dump(2);
                },
                [](Project& p, const std::string& text) {
                    const json j = json::parse(text);
                    LoadCaseSet loaded;  // starts with "Peso propio"
                    int following = 1;
                    if (j.contains("cases"))
                        for (const json& e : j.at("cases")) {
                            LoadCase c;
                            c.id = e.value("id", 0);
                            if (c.id < 1) continue;
                            const int nature = e.value("nature", 1);
                            c.nature = (nature >= 1 && nature < kLoadNatureCount) ? static_cast<LoadNature>(nature) : LoadNature::DeadLoad;
                            c.reference = e.value("reference", std::string());
                            c.description = e.value("description", std::string());
                            c.acts = e.value("acts", true);
                            c.arrangements = e.value("arrangements", std::vector<std::string>{});
                            const int compat = e.value("compat", 2);
                            c.compat = (compat >= 0 && compat <= 2) ? static_cast<ArrangementCompat>(compat) : ArrangementCompat::Simultaneous;
                            c.seismic = e.value("seismic", 0) == 1 ? SeismicType::XY : SeismicType::Simple;
                            following = std::max(following, c.id + 1);
                            ++loaded.created[static_cast<std::size_t>(c.nature)];
                            loaded.cases.push_back(std::move(c));
                        }
                    // Stored counters win when they are not below the number of cases found (they never go down when a case is deleted).
                    const std::vector<int> created = j.value("created", std::vector<int>{});
                    for (std::size_t i = 0; i < created.size() && i < loaded.created.size(); ++i) loaded.created[i] = std::max(loaded.created[i], created[i]);
                    loaded.nextId = std::max(following, j.value("nextId", following));
                    if (j.contains("useCategories"))
                        for (const json& e : j.at("useCategories")) {
                            UseCategory u;
                            u.ref = e.value("ref", std::string());
                            u.description = e.value("description", std::string());
                            const std::vector<std::string> choices = e.value("choices", std::vector<std::string>{});
                            for (std::size_t i = 0; i < choices.size() && i < u.choices.size(); ++i) u.choices[i] = choices[i];
                            loaded.useCategories.push_back(std::move(u));
                        }
                    loaded.seismicSecondOrder = j.value("seismicSecondOrder", false);
                    p.loadCases = std::move(loaded);
                },
                [](const Project& p) { return validateLoadCases(p.loadCases).empty(); }},
        Section{"nodal_loads", "Cargas sobre nudos",
                [](const Project& p) {
                    // Versioned. Loads of nodes that no longer exist are not saved.
                    const NodalLoads& d = p.nodalLoads;
                    json items = json::array();
                    for (const NodalLoad& l : d.items) {
                        if (!p.structure.findNode(l.nodeId)) continue;
                        json e = {{"id", l.id}, {"node", l.nodeId}, {"load_case", l.loadCaseId}, {"value", l.magnitude},
                                  {"direction", static_cast<int>(l.direction)}};
                        if (l.direction == LoadDirection::UserVector) e["vector"] = {l.userVector.x, l.userVector.y, l.userVector.z};
                        items.push_back(std::move(e));
                    }
                    json perCase = json::array();
                    for (const auto& [id, v] : d.scales.perCase) perCase.push_back({{"load_case", id}, {"scale", v}});
                    return json{{"version", 1}, {"next", d.nextId}, {"items", items},
                                {"scales", {{"mode", static_cast<int>(d.scales.mode)}, {"fixed", d.scales.fixedSizeM}, {"per_case", perCase},
                                            {"per_type", d.scales.perType}}}}
                        .dump(2);
                },
                [](Project& p, const std::string& text) {
                    const json j = json::parse(text);
                    NodalLoads loaded;
                    int following = 1;
                    if (j.contains("items"))
                        for (const json& e : j.at("items")) {
                            NodalLoad l;
                            l.id = e.value("id", 0);
                            l.nodeId = e.value("node", 0);
                            if (l.id < 1 || l.nodeId < 1) continue;
                            l.loadCaseId = e.value("load_case", 0);
                            l.magnitude = roundLoadValue(e.value("value", 0.0));  // tolerant: two decimals
                            const int dir = e.value("direction", static_cast<int>(LoadDirection::MinusZ));
                            l.direction = (dir >= 0 && dir < kLoadDirectionCount) ? static_cast<LoadDirection>(dir) : LoadDirection::MinusZ;
                            const std::vector<double> v = e.value("vector", std::vector<double>{});
                            if (v.size() == 3) l.userVector = {v[0], v[1], v[2]};
                            following = std::max(following, l.id + 1);
                            loaded.items.push_back(l);
                        }
                    loaded.nextId = std::max(following, j.value("next", following));
                    if (j.contains("scales")) {
                        const json& s = j.at("scales");
                        loaded.scales.mode = s.value("mode", 0) == 1 ? LoadScaleMode::Proportional : LoadScaleMode::Fixed;
                        loaded.scales.fixedSizeM = s.value("fixed", kDefaultLoadDrawSizeM);
                        if (s.contains("per_case"))
                            for (const json& c : s.at("per_case")) loaded.scales.perCase[c.value("load_case", 0)] = c.value("scale", kDefaultLoadCaseScale);
                        if (s.contains("per_type") && s.at("per_type").is_array() && s.at("per_type").size() == loaded.scales.perType.size())
                            for (std::size_t i = 0; i < loaded.scales.perType.size(); ++i) loaded.scales.perType[i] = s.at("per_type")[i].get<double>();
                    }
                    p.nodalLoads = std::move(loaded);
                },
                [](const Project& p) { return validateNodalLoads(p.nodalLoads, p.loadCases).empty(); }},
        Section{"bar_loads", "Cargas sobre barras",
                [](const Project& p) {
                    // Versioned. Loads (and self-weight flags) of bars that no longer exist are not saved.
                    const BarLoads& d = p.barLoads;
                    json items = json::array();
                    for (const BarLoad& l : d.items) {
                        if (!p.structure.findBar(l.barId)) continue;
                        json e = {{"id", l.id},
                                  {"bar", l.barId},
                                  {"load_case", l.loadCaseId},
                                  {"type", static_cast<int>(l.type)},
                                  {"axes", static_cast<int>(l.axes)},
                                  {"direction", static_cast<int>(l.direction)},
                                  {"vector", {l.userVector.x, l.userVector.y, l.userVector.z}},
                                  {"angle_in_plane", l.angleInPlaneXz},
                                  {"angle_to_plane", l.angleToPlaneXz},
                                  {"temperature_direction", static_cast<int>(l.temperatureDirection)},
                                  {"temperature_angle", l.temperatureAngle},
                                  {"relative", l.relative},
                                  {"start", l.start},
                                  {"end", l.end},
                                  {"value1", l.value1},
                                  {"value2", l.value2}};
                        items.push_back(std::move(e));
                    }
                    json off = json::array();
                    for (int id : d.selfWeightOff)
                        if (p.structure.findBar(id)) off.push_back(id);
                    return json{{"version", 1}, {"next", d.nextId}, {"items", items}, {"self_weight_off", off}}.dump(2);
                },
                [](Project& p, const std::string& text) {
                    const json j = json::parse(text);
                    BarLoads loaded;
                    int following = 1;
                    if (j.contains("items"))
                        for (const json& e : j.at("items")) {
                            BarLoad l;
                            l.id = e.value("id", 0);
                            l.barId = e.value("bar", 0);
                            if (l.id < 1 || l.barId < 1) continue;
                            l.loadCaseId = e.value("load_case", 0);
                            const int type = e.value("type", static_cast<int>(BarLoadType::Uniform));
                            if (type < 0 || type >= kBarLoadTypeCount) continue;
                            l.type = static_cast<BarLoadType>(type);
                            l.axes = e.value("axes", 0) == 1 ? BarLoadAxes::Local : BarLoadAxes::Global;
                            const int dir = e.value("direction", static_cast<int>(LoadDirection::MinusZ));
                            l.direction = (dir >= 0 && dir < kLoadDirectionCount) ? static_cast<LoadDirection>(dir) : LoadDirection::MinusZ;
                            const std::vector<double> v = e.value("vector", std::vector<double>{});
                            if (v.size() == 3) l.userVector = {v[0], v[1], v[2]};
                            l.angleInPlaneXz = e.value("angle_in_plane", 0.0);
                            l.angleToPlaneXz = e.value("angle_to_plane", 0.0);
                            const int td = e.value("temperature_direction", static_cast<int>(TemperatureDirection::PlusZ));
                            l.temperatureDirection = (td >= 0 && td <= 2) ? static_cast<TemperatureDirection>(td) : TemperatureDirection::PlusZ;
                            l.temperatureAngle = e.value("temperature_angle", 0.0);
                            l.relative = e.value("relative", true);
                            l.start = e.value("start", 0.0);
                            l.end = e.value("end", 1.0);
                            l.value1 = e.value("value1", kBarLoadDefaultValue);
                            l.value2 = e.value("value2", 0.0);
                            static_cast<BarLoadData&>(l) = normaliseBarLoadData(l);  // tolerant: canonical form (rounded, unused fields reset)
                            following = std::max(following, l.id + 1);
                            loaded.items.push_back(l);
                        }
                    loaded.nextId = std::max(following, j.value("next", following));
                    if (j.contains("self_weight_off"))
                        for (const json& id : j.at("self_weight_off"))
                            if (id.is_number_integer() && id.get<int>() >= 1) loaded.selfWeightOff.insert(id.get<int>());
                    p.barLoads = std::move(loaded);
                },
                [](const Project& p) { return validateBarLoads(p.barLoads, p.loadCases).empty(); }},
        Section{"shell_properties", "Propiedades de láminas",
                [](const Project& p) {
                    // Shells and edges that no longer exist are not saved.
                    const ShellProperties& d = p.shellProps;
                    auto edgeExists = [&](const ShellEdgeKey& k) {
                        for (const ShellEdgeKey& e : shellEdges(p.structure, k.shellId))
                            if (e == k) return true;
                        return false;
                    };
                    json shells = json::array();
                    for (const auto& [id, r] : d.shells) {
                        if (!p.structure.findShell(id)) continue;
                        json bands = json::array();
                        for (const ShellIntegrationBand& b : r.bands) bands.push_back({{"x1", b.x1}, {"y1", b.y1}, {"x2", b.x2}, {"y2", b.y2}, {"width", b.widthM}, {"force_points", b.forcePoints}});
                        json st = json::array();
                        for (int i = 0; i < kStiffnessFactorCount; ++i) st.push_back(r.stiffness.value(i));
                        shells.push_back({{"shell", id}, {"triangle", r.maxTriangleSideM}, {"stiffness", st}, {"bands", bands}, {"buckling", r.bucklingConsidered}, {"modal", r.modalConsidered}});
                    }
                    json interior = json::array();
                    for (const auto& [k, b] : d.interior) {
                        if (!edgeExists(k)) continue;
                        json dof = json::array();
                        for (DofMode m : b.dof) dof.push_back(static_cast<int>(m));
                        interior.push_back({{"shell", k.shellId}, {"a", k.nodeA}, {"b", k.nodeB}, {"kind", static_cast<int>(b.kind)}, {"dof", dof}, {"k", b.k}});
                    }
                    json exterior = json::array();
                    for (const auto& [k, sup] : d.exterior)
                        if (edgeExists(k)) exterior.push_back({{"shell", k.shellId}, {"a", k.nodeA}, {"b", k.nodeB}, {"support", sup}});
                    return json{{"shells", shells}, {"interior", interior}, {"exterior", exterior},
                                {"band_force", {{"method", static_cast<int>(d.bandForce.method)}, {"smoothed", d.bandForce.smoothedStresses}}}}.dump(2);
                },
                [](Project& p, const std::string& text) {
                    const json j = json::parse(text);
                    ShellProperties loaded;
                    if (j.contains("shells"))
                        for (const json& e : j.at("shells")) {
                            ShellRecord r;
                            r.maxTriangleSideM = e.value("triangle", kShellTriangleSideDefaultM);
                            if (e.contains("stiffness")) {
                                const json& st = e.at("stiffness");
                                for (std::size_t i = 0; i < st.size() && i < static_cast<std::size_t>(kStiffnessFactorCount); ++i) r.stiffness.set(static_cast<int>(i), st[i].get<double>());
                            }
                            if (e.contains("bands"))
                                for (const json& b : e.at("bands"))
                                {
                                    ShellIntegrationBand nb{b.value("x1", 0.0), b.value("y1", 0.0), b.value("x2", 0.0), b.value("y2", 0.0), b.value("width", kBandWidthDefaultM)};
                                    nb.forcePoints = b.value("force_points", kBandForcePointsDefault);  // older files: 10
                                    r.bands.push_back(normaliseBand(nb));
                                }
                            r.bucklingConsidered = e.value("buckling", false);
                            r.modalConsidered = e.value("modal", true);
                            setShellRecord(loaded, e.at("shell").get<int>(), r);
                        }
                    if (j.contains("interior"))
                        for (const json& e : j.at("interior")) {
                            EdgeInteriorBond b;
                            b.kind = static_cast<EdgeInteriorKind>(e.value("kind", static_cast<int>(EdgeInteriorKind::Fixed)));
                            if (e.contains("dof"))
                                for (std::size_t i = 0; i < 6 && i < e.at("dof").size(); ++i) b.dof[i] = static_cast<DofMode>(e.at("dof")[i].get<int>());
                            if (e.contains("k"))
                                for (std::size_t i = 0; i < 6 && i < e.at("k").size(); ++i) b.k[i] = e.at("k")[i].get<double>();
                            setEdgeInterior(loaded, makeEdgeKey(e.at("shell").get<int>(), e.at("a").get<int>(), e.at("b").get<int>()), b);
                        }
                    if (j.contains("exterior"))
                        for (const json& e : j.at("exterior"))
                            setEdgeExterior(loaded, makeEdgeKey(e.at("shell").get<int>(), e.at("a").get<int>(), e.at("b").get<int>()), e.at("support").get<ExternalSupport>());
                    if (j.contains("band_force")) {
                        const json& f = j.at("band_force");
                        loaded.bandForce.method = f.value("method", 0) == 1 ? BandForceMethod::NodalForces : BandForceMethod::InternalStresses;
                        loaded.bandForce.smoothedStresses = f.value("smoothed", true);
                    }
                    p.shellProps = std::move(loaded);
                },
                [](const Project& p) { return validateShellProperties(p.shellProps).empty(); }},
        Section{"shell_loads", "Cargas en láminas",
                [](const Project& p) {
                    // Versioned. Loads (and self-weight flags) of shells that no longer exist are not saved.
                    const ShellLoads& d = p.shellLoads;
                    json items = json::array();
                    for (const ShellLoad& l : d.items) {
                        if (!p.structure.findShell(l.shellId)) continue;
                        items.push_back({{"id", l.id},
                                         {"shell", l.shellId},
                                         {"load_case", l.loadCaseId},
                                         {"type", static_cast<int>(l.type)},
                                         {"axes", static_cast<int>(l.axes)},
                                         {"sense", static_cast<int>(l.sense)},
                                         {"value1", l.value1},
                                         {"value2", l.value2}});
                    }
                    json off = json::array();
                    for (int id : d.selfWeightOff)
                        if (p.structure.findShell(id)) off.push_back(id);
                    return json{{"version", 1}, {"next", d.nextId}, {"items", items}, {"self_weight_off", off}}.dump(2);
                },
                [](Project& p, const std::string& text) {
                    const json j = json::parse(text);
                    ShellLoads loaded;
                    int following = 1;
                    if (j.contains("items"))
                        for (const json& e : j.at("items")) {
                            ShellLoad l;
                            l.id = e.value("id", 0);
                            l.shellId = e.value("shell", 0);
                            if (l.id < 1 || l.shellId < 1) continue;
                            l.loadCaseId = e.value("load_case", 0);
                            const int type = e.value("type", static_cast<int>(ShellLoadType::Uniform));
                            if (type < 0 || type >= kShellLoadTypeCount) continue;
                            l.type = static_cast<ShellLoadType>(type);
                            l.axes = e.value("axes", 1) == 0 ? ShellLoadAxes::Global : ShellLoadAxes::Local;
                            l.sense = e.value("sense", 0) == 1 ? ShellLoadSense::MinusZ : ShellLoadSense::PlusZ;
                            l.value1 = e.value("value1", kShellLoadDefaultSurface);
                            l.value2 = e.value("value2", 0.0);
                            static_cast<ShellLoadData&>(l) = normaliseShellLoadData(l);  // tolerant: canonical form (rounded, unused fields reset)
                            following = std::max(following, l.id + 1);
                            loaded.items.push_back(l);
                        }
                    loaded.nextId = std::max(following, j.value("next", following));
                    if (j.contains("self_weight_off"))
                        for (const json& id : j.at("self_weight_off"))
                            if (id.is_number_integer() && id.get<int>() >= 1) loaded.selfWeightOff.insert(id.get<int>());
                    p.shellLoads = std::move(loaded);
                },
                [](const Project& p) { return validateShellLoads(p.shellLoads, p.loadCases).empty(); }},
        Section{"panels", "Paños",
                [](const Project& p) {
                    // Versioned. A panel is a closed planar polygon of free points with its direction of distribution and its rows of load.
                    json items = json::array();
                    for (const Panel& panel : p.panels.items) {
                        json loads = json::array();
                        for (const SurfaceLoadData& d : panel.loads) loads.push_back(surfaceLoadDataToJson(d));
                        items.push_back({{"id", panel.id}, {"vertices", pointsToJson(panel.vertices)}, {"direction", pointToJson(panel.direction)}, {"loads", loads}});
                    }
                    return json{{"version", 1}, {"next", p.panels.nextId}, {"items", items}}.dump(2);
                },
                [](Project& p, const std::string& text) {
                    const json j = json::parse(text);
                    Panels loaded;
                    int following = 1;
                    if (j.contains("items"))
                        for (const json& e : j.at("items")) {
                            Panel panel;
                            panel.id = e.value("id", 0);
                            if (panel.id < 1 || !e.contains("vertices")) continue;
                            panel.vertices = pointsFromJson(e.at("vertices"));
                            if (e.contains("direction")) panel.direction = pointFromJson(e.at("direction"));
                            if (e.contains("loads"))
                                for (const json& l : e.at("loads"))
                                    if (const auto d = surfaceLoadDataFromJson(l)) panel.loads.push_back(*d);
                            following = std::max(following, panel.id + 1);
                            loaded.items.push_back(std::move(panel));
                        }
                    loaded.nextId = std::max(following, j.value("next", following));
                    p.panels = std::move(loaded);
                },
                [](const Project& p) { return validatePanels(p.panels, p.loadCases).empty(); }},
        Section{"surface_loads", "Cargas superficiales",
                [](const Project& p) {
                    // Versioned. Switched-off bars that no longer exist are not saved.
                    const SurfaceLoads& d = p.surfaceLoads;
                    json items = json::array();
                    for (const SurfaceLoad& l : d.items) {
                        json e = surfaceLoadDataToJson(l.data);
                        e["id"] = l.id;
                        e["contour"] = pointsToJson(l.contour);
                        items.push_back(std::move(e));
                    }
                    json off = json::array();
                    for (int id : d.unassignedBars)
                        if (p.structure.findBar(id)) off.push_back(id);
                    return json{{"version", 1}, {"next", d.nextId}, {"items", items}, {"unassigned_bars", off}}.dump(2);
                },
                [](Project& p, const std::string& text) {
                    const json j = json::parse(text);
                    SurfaceLoads loaded;
                    int following = 1;
                    if (j.contains("items"))
                        for (const json& e : j.at("items")) {
                            SurfaceLoad l;
                            l.id = e.value("id", 0);
                            if (l.id < 1 || !e.contains("contour")) continue;
                            const auto data = surfaceLoadDataFromJson(e);
                            if (!data) continue;
                            l.data = *data;
                            l.contour = pointsFromJson(e.at("contour"));
                            following = std::max(following, l.id + 1);
                            loaded.items.push_back(std::move(l));
                        }
                    loaded.nextId = std::max(following, j.value("next", following));
                    if (j.contains("unassigned_bars"))
                        for (const json& id : j.at("unassigned_bars"))
                            if (id.is_number_integer() && id.get<int>() >= 1) loaded.unassignedBars.insert(id.get<int>());
                    p.surfaceLoads = std::move(loaded);
                },
                [](const Project& p) { return validateSurfaceLoads(p.surfaceLoads, p.loadCases).empty(); }},
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
