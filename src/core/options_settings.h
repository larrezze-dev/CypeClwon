#pragma once
#include <array>
#include <string>
#include <vector>

namespace cc::core {

/// Settings behind the leaf dialogs of "Opciones" in the "Obra vacía" wizard (Vigas and Uniones; the Cimentación
/// and Pilares leaves are not captured yet, ticket TK-015). Reference:
/// docs/research/ui/dialogs_opciones_y_pilotes.md. Values lists that were not opened in the original offer only
/// the entries seen on screen.

/// "Valoración de errores" (Vigas): which checks raise an error.
struct BeamErrorSettings {
    // Vigas de hormigón
    bool compressionHook = true, topCompressionSteel = true, bottomCompressionSteel = true, manualShortCorbel = true;
    // Estribos
    bool minStirrupDiameter = true;
    int minStirrupDiameterMm = 16;  ///< "Estribos de diámetro igual o superior a", combo Ø16
    bool veryShortTruss = true;
    // Armados
    bool skinAnchorage = false, shortSpan = true, levelDifference = true, barTooLong = true, assemblyAnchorage = true,
         topAnchorage = true, topMultiLayer = false, bottomAnchorage = true, bottomMultiLayer = false;
    bool isValid() const { return minStirrupDiameterMm > 0; }
};

/// One line of "Límites de flecha" as summarised by the read-only dialog: a description and the limit text
/// ("L/ 250", "1.00 cm" or "L/ 1000 + 0.50 cm"; several criteria are joined with "; ").
struct DeflectionLimit {
    std::string description;
    std::string text;
};

/// Limit of one row of a "Límites de flecha" editor. The row editor (283x197 in the original) has three
/// independent check boxes: Absoluta (cm), Mixta (L/n + cm) and Relativa (L/n). The value fields keep their
/// content while the box is unchecked; the row shows "----" when no box is checked.
struct DeflectionRow {
    static constexpr int kMinDenominator = 100, kMaxDenominator = 2000;  ///< "Debe estar entre 100 y 2000."
    bool absolute = false;
    double absoluteCm = 1.0;
    bool mixed = false;
    int mixedDenominator = 1000;
    double mixedCm = 0.5;
    bool relative = false;
    int relativeDenominator = 250;
    bool isDefined() const { return absolute || mixed || relative; }
    bool isValid() const;
    /// Row with only the relative criterion checked.
    static DeflectionRow relativeLimit(int denominator);
    /// Text shown in the editor and the summary ("----" when undefined).
    std::string summary() const;
};

/// A material group of "Límites de flecha". An empty list means "No se ha definido ningún límite de flecha."
struct DeflectionLimitGroup {
    std::string material;
    std::vector<DeflectionLimit> limits;
};

/// Editable limits of one material (the four "Límites de flecha (material)" editors). A row without any checked
/// criterion shows "----" (no limit). Combination order: Característica, Frecuente, Cuasipermanente.
struct DeflectionLimitSet {
    std::array<DeflectionRow, 2> instantSimple;   ///< Carga permanente, Sobrecarga
    std::array<DeflectionRow, 3> instantCombination;
    std::array<DeflectionRow, 3> infinite;        ///< "A plazo infinito" (only when hasInfinite)
    std::array<DeflectionRow, 3> active;          ///< "Activa a largo plazo"
    int damageableMonths = 3;                      ///< "Construcción del elemento dañable" (only when hasDamageable)
    bool hasInfinite = false, hasDamageable = false;
    bool isValid() const;
};

/// Materials in the order of the original: Hormigón, Acero laminado, Acero conformado, Madera.
struct DeflectionLimitsSettings {
    std::array<DeflectionLimitSet, 4> materials;
    DeflectionLimitsSettings();
    static const char* materialName(std::size_t index);
    /// Summary lines shown by the read-only "Límites de flecha" dialog.
    std::vector<DeflectionLimitGroup> groups() const;
    bool isValid() const;
};

/// Default deflection limits summary, in the order shown by the original.
const std::vector<DeflectionLimitGroup>& defaultDeflectionLimits();

/// Vigas > Tablas de armado: Armadura transversal, portaestribos, piel and refuerzos superior/inferior.
struct TransverseReinforcementTable {
    std::vector<int> diameters{6, 8, 10, 12, 16};  ///< "Diámetros admisibles" (mm)
    std::vector<double> spacingsCm;                 ///< "Separaciones (cm)": 5.0 ... 30.0 step 1.0
    TransverseReinforcementTable();
    bool isValid() const;
};

/// Diameters Ø6-Ø32 with an admissible flag (portaestribos and piel).
struct DiameterAdmissibility {
    static const std::array<int, 8>& diameters();  ///< 6 8 10 12 16 20 25 32
    std::array<bool, 8> admissible{};
    bool assemblyInFlanges = false;  ///< portaestribos only: "Disponer armadura de montaje en alas"
};

/// Armadura superior/inferior de refuerzo (both tables are identical in the original).
struct ReinforcementBarTable {
    std::vector<int> diameters{10, 12, 16, 20, 25};  ///< "Diámetros admisibles"
    double minClearSpacingCm = 2.0;                   ///< "Separación libre mínima entre barras"
    /// Montaje (rows, Ø6-Ø32) x Refuerzo 1 (columns, Ø10-Ø25) and Refuerzo 1 x Refuerzo 2: true = ✓.
    std::array<std::array<bool, 5>, 8> assemblyVsFirst{};
    std::array<std::array<bool, 5>, 5> firstVsSecond{};
    ReinforcementBarTable();
    static const std::array<int, 5>& reinforcementDiameters();  ///< 10 12 16 20 25
    bool isValid() const;
};

struct BeamTablesSettings {
    TransverseReinforcementTable transverse;
    DiameterAdmissibility stirrupHolder, skin;
    ReinforcementBarTable topReinforcement, bottomReinforcement;
    BeamTablesSettings();
    bool isValid() const;
};

/// "Numeración de vigas continuas".
struct ContinuousBeamNumbering {
    enum class Axis { X, Y };
    enum class Sense { Positive, Negative };
    std::string baseReference = "Pórtico 1";
    Axis firstAxis = Axis::X;            ///< which frames are numbered first
    Sense xFramesAlongY = Sense::Positive;  ///< frames parallel to X, sense along the global Y axis
    Sense yFramesAlongX = Sense::Positive;  ///< frames parallel to Y, sense along the global X axis
    bool isValid() const { return !baseReference.empty(); }
};

/// "Numeración de vigas".
enum class BeamNumbering { None, Consecutive, ByLevel };

struct BeamOptionsSettings {
    BeamErrorSettings errors;
    DeflectionLimitsSettings deflection;
    BeamTablesSettings tables;
    ContinuousBeamNumbering continuousNumbering;
    BeamNumbering numbering = BeamNumbering::None;
    bool isValid() const { return errors.isValid() && deflection.isValid() && tables.isValid() && continuousNumbering.isValid(); }
};

/// Uniones > Materiales.
struct JointMaterials {
    std::string rolledSteel = "S275 (UNE-EN 10025-2)";
    std::string anchorBoltSteel = "B 400 S, Ys = 1.15";
    std::string concrete = "C25/30";
    bool isValid() const;
};

/// Uniones > Opciones, tab "Tornillería no pretensada".
struct BoltSettings {
    bool iso4014 = true, iso4017 = true;
    std::string steelClass = "8.8";  ///< 4.6, 5.6, 6.8, 8.8 or 10.9
    static constexpr std::size_t kDiameterCount = 10;
    /// M12 M16 M20 M24 M30 M36 M42 M48 M56 M64
    std::array<bool, kDiameterCount> diameters{true, true, true, true, false, false, false, false, false, false};
    bool unusualDiameters = false;
    bool isValid() const;
};

const std::vector<std::string>& boltSteelClasses();
const std::vector<std::string>& boltDiameterNames();

/// Uniones > Opciones, tab "Tornillería pretensada".
struct PreloadedBoltSettings {
    std::string series = "EN 14399-3 (HR)";  ///< ISO 7411, ISO 7412, EN 14399-3 (HR) or EN 14399-4 (HV)
    std::string steelClass = "10.9";          ///< 8.8 or 10.9
    static constexpr std::size_t kDiameterCount = 7;
    /// M12 M16 M20 M24 M27 M30 M36
    std::array<bool, kDiameterCount> diameters{true, true, true, true, true, true, true};
    bool unusualDiameters = false;
    char frictionClass = 'C';  ///< slip-factor surface class A, B, C or D
    bool isValid() const;
};

const std::vector<std::string>& preloadedBoltSeries();
const std::vector<std::string>& preloadedBoltDiameterNames();
/// Slip factor (mu) of a friction surface class A-D (Código Estructural, Anejo 21); 0 when unknown.
double frictionCoefficient(char frictionClass);
/// Description shown under the friction classes; empty when only the class C text was seen in the original.
std::string frictionDescription(char frictionClass);

/// Uniones > Opciones, tab "Rigidizadores".
struct StiffenerSettings {
    bool alwaysColumnBeam = false;  ///< "Disponer siempre en uniones pilar-viga"
    bool alwaysRidge = false;       ///< "Disponer siempre en uniones de cumbrera"
    bool withCutouts = true;        ///< "Rigidizadores con recortes" (false: sin recortes)
};

/// Uniones > Opciones, tab "Placa de anclaje".
struct BasePlateSettings {
    double maxOverhangThickness = 20.0, maxWidthThickness = 30.0;
    double maxRecommendedThicknessMm = 15.0, groutThicknessCm = 2.0;
    double stiffenerBevelDeg = 45.0, maxSidesBoltDiameter = 50.0;
    std::string boltPlateAnchorage = "Soldados con preparación de bordes";
    bool isValid() const;
};

struct JointOptionsSettings {
    JointMaterials materials;
    BoltSettings bolts;
    PreloadedBoltSettings preloaded;
    StiffenerSettings stiffeners;
    BasePlateSettings basePlate;
    bool isValid() const { return materials.isValid() && bolts.isValid() && preloaded.isValid() && basePlate.isValid(); }
};

/// Cimentación > Opciones and Tablas de armado (captures of 2026-10-31; see ticket TK-019 for what is still missing).
struct CleaningConcreteSettings {
    bool footings = false, pileCaps = false, tieAndCenteringBeams = false;
};

struct CrackingSettings {
    bool checkFootings = false, checkBeams = false;
    double footingLimitMm = 0.30, beamLimitMm = 0.30;
    bool isValid() const { return footingLimitMm > 0 && beamLimitMm > 0; }
};

/// Encepados > Generales.
struct PileCapGeneralSettings {
    double minOverhangOverPileM = 0.25, minOverhangOverSupportM = 0.15;
    std::string depthCriterion = "Canto mínimo según norma EHE";
    double minDepthM = 0.20, maxDepthM = 2.50;
    double minBarSpacingM = 0.05, maxBarDiameterMm = 25.0;
    bool withPileLoads = false;
    double loadFraction = 1.00, loadFactor = 1.50;
    bool isValid() const;
};

struct CenteringBeamSettings {
    double toleranceAngleDeg = 15.0;
    bool stirrupsInsideFoundation = false;
    std::string centeringType = "Ley rectangular de tensiones bajo la zapata";
    bool minTensionGeometric = false;
    double minTensionRatio = 0.00100;
    bool isValid() const { return toleranceAngleDeg >= 0 && toleranceAngleDeg < 90 && minTensionRatio >= 0; }
};

/// Encepados > "Un pilote".
struct PileCapSingleSettings {
    double armingDepthPercent = 1.00;
    bool diagonalStirrups = false;
    bool isValid() const { return armingDepthPercent > 0; }
};

/// Encepados > "Vigas de encepado". Only the first entry of each hook combo was seen in the original.
struct PileCapBeamSettings {
    std::string topHook = "Colocar patilla mínima", bottomHook = "Colocar patilla mínima";
    double topHookMaxLengthM = 10.00, bottomHookMaxLengthM = 10.00;
    double maxHoopDiameterMm = 16.0, strutAngleDeg = 45.00;
    bool isValid() const { return topHookMaxLengthM > 0 && bottomHookMaxLengthM > 0 && maxHoopDiameterMm > 0 && strutAngleDeg > 0 && strutAngleDeg < 90; }
};

/// Encepados > "Vigas y parrillas".
struct PileCapGridSettings {
    bool designTopGrid = false;
    std::string criterion = "Armado según norma EHE";
    std::string alternativeCriterion = "Armado según norma EHE";  ///< enabled by the other criteria (not captured)
};

/// Encepados > "Armado".
struct PileCapReinforcementSettings {
    bool perimeterReinforcement = true;  ///< "Disponer armado perimetral"
    double fromAxialKn = 2000.00;        ///< "A partir de"
    bool isValid() const { return fromAxialKn >= 0; }
};

/// All the tabs of Encepados > Opciones.
struct PileCapOptions {
    PileCapGeneralSettings general;
    PileCapSingleSettings single;
    PileCapBeamSettings beams;
    PileCapGridSettings grid;
    PileCapReinforcementSettings reinforcement;
    bool isValid() const { return general.isValid() && single.isValid() && beams.isValid() && reinforcement.isValid(); }
};

/// Zapatas aisladas > "Armado perimetral" (the three fields are enabled by the check box).
struct FootingPerimeterSettings {
    bool enabled = false;  ///< "Disponer armado perimetral de zunchado"
    double minDiameterMm = 12.00;
    int minBars = 3;
    double minAxialKn = 2000.00;  ///< "Axil mínimo para disponer"
    bool isValid() const { return minDiameterMm > 0 && minBars > 0 && minAxialKn >= 0; }
};

/// Zapatas aisladas > "Tipos".
struct FootingTypeSettings {
    bool equalReinforcement = true;  ///< square footing: "Igualar armadura en las dos direcciones"
    double squareMinOverhangCm = 25.00;
    double centredMinOverhangCm = 25.00, centredMaxSideRatio = 1.50;
    double eccentricMinOverhangCm = 25.00, eccentricMaxSideRatio = 2.00;
    double pyramidMaxAngleDeg = 25.00, pyramidMinPedestalOverhangCm = 5.00, pyramidMinEdgeDepthCm = 15.00;
    bool isValid() const {
        return squareMinOverhangCm >= 0 && centredMinOverhangCm >= 0 && centredMaxSideRatio >= 1 && eccentricMinOverhangCm >= 0 &&
               eccentricMaxSideRatio >= 1 && pyramidMaxAngleDeg > 0 && pyramidMaxAngleDeg < 90 &&
               pyramidMinPedestalOverhangCm >= 0 && pyramidMinEdgeDepthCm >= 0;
    }
};

/// Zapatas aisladas > "Zapatas combinadas".
struct FootingCombinedSettings {
    double maxSpanDepthRatio = 8.00;      ///< "Máxima relación luz máxima entre pilares-canto"
    double maxOverhangDepthRatio = 2.00;  ///< "Máxima relación vuelo máximo-canto"
    bool isValid() const { return maxSpanDepthRatio > 0 && maxOverhangDepthRatio > 0; }
};

/// Zapatas aisladas > "Incremento tensión": allowed increase of the maximum stress when the load is eccentric,
/// "Tensión máxima cálculo <= ( 1 + x ) tensión admisible", per group of situations.
struct FootingStressIncrease {
    double persistentNoWind = 0.25, persistentWind = 0.25, accidental = 0.25;
    bool isValid() const { return persistentNoWind >= 0 && persistentWind >= 0 && accidental >= 0; }
};

/// Zapatas aisladas > Opciones: tab "Generales" plus the fields of the other four tabs.
struct FootingSettings {
    double widthIncrementCm = 5.00, depthIncrementCm = 5.00, minDepthCm = 30.00;
    bool minGeometricReinforcement = false;
    double minGeometricRatio = 0.0010;
    bool reduceFlexureMinimums = true;
    bool topGridIfNeeded = true;
    bool equalHooksPerDirection = true;
    bool allowLiftOff = true;
    bool noTopGridIfPlainConcrete = false;
    std::string defaultType = "Zapata de canto constante";
    std::string depthCriterion = "Canto zapatas >= canto viga sin excepción";
    FootingPerimeterSettings perimeter;
    FootingTypeSettings types;
    FootingCombinedSettings combined;
    FootingStressIncrease stressIncrease;
    bool isValid() const {
        return widthIncrementCm >= 0 && depthIncrementCm >= 0 && minDepthCm > 0 && minGeometricRatio >= 0 && perimeter.isValid() &&
               types.isValid() && combined.isValid() && stressIncrease.isValid();
    }
};

/// Vigas de atado > Opciones.
struct TieBeamSettings {
    double compactionSurchargeKnPerM = 0.00;
    double minAxialPercent = 0.0;
    bool stirrupsInsideFoundation = false;
    bool isValid() const { return compactionSurchargeKnPerM >= 0 && minAxialPercent >= 0 && minAxialPercent <= 100; }
};

/// Source of an arrangement table (Zapatas aisladas / Vigas de atado / Vigas centradoras): only the predefined one is selectable.
enum class ReinforcementTableSource { Predefined, Library, Special };

struct FoundationOptionsSettings {
    CleaningConcreteSettings cleaning;
    CrackingSettings cracking;
    FootingSettings footings;
    PileCapOptions pileCap;
    CenteringBeamSettings centeringBeams;
    TieBeamSettings tieBeams;
    ReinforcementTableSource footingTable = ReinforcementTableSource::Predefined;
    ReinforcementTableSource centeringTable = ReinforcementTableSource::Predefined;
    ReinforcementTableSource tieBeamTable = ReinforcementTableSource::Predefined;
    bool isValid() const {
        return cracking.isValid() && footings.isValid() && pileCap.isValid() && centeringBeams.isValid() && tieBeams.isValid();
    }
};

/// Everything edited from the Opciones buttons of the wizard.
struct OptionsSettings {
    FoundationOptionsSettings foundation;
    BeamOptionsSettings beams;
    JointOptionsSettings joints;
    bool isValid() const { return foundation.isValid() && beams.isValid() && joints.isValid(); }
};

}  // namespace cc::core
