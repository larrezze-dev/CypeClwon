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

/// One line of "Límites de flecha": a description and the L/ denominator.
struct DeflectionLimit {
    std::string description;
    int denominator;
};

/// A material group of "Límites de flecha". An empty list means "No se ha definido ningún límite de flecha."
struct DeflectionLimitGroup {
    std::string material;
    std::vector<DeflectionLimit> limits;
};

/// Editable limits of one material (the four "Límites de flecha (material)" editors). A denominator of 0 shows
/// "----" (no limit). Combination order: Característica, Frecuente, Cuasipermanente.
struct DeflectionLimitSet {
    std::array<int, 2> instantSimple{0, 0};       ///< Carga permanente, Sobrecarga
    std::array<int, 3> instantCombination{0, 0, 0};
    std::array<int, 3> infinite{0, 0, 0};         ///< "A plazo infinito" (only when hasInfinite)
    std::array<int, 3> active{0, 0, 0};           ///< "Activa a largo plazo"
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

/// Cimentación > Opciones and Tablas de armado (captures of 2026-10-31; the Zapatas aisladas and Vigas de atado
/// option sheets, the other pile-cap tabs and the Vigas centradoras table are still ticket TK-019).
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

/// Source of an arrangement table (Zapatas aisladas / Vigas de atado): only the predefined one is selectable.
enum class ReinforcementTableSource { Predefined, Library, Special };

struct FoundationOptionsSettings {
    CleaningConcreteSettings cleaning;
    CrackingSettings cracking;
    PileCapGeneralSettings pileCap;
    CenteringBeamSettings centeringBeams;
    ReinforcementTableSource footingTable = ReinforcementTableSource::Predefined;
    ReinforcementTableSource tieBeamTable = ReinforcementTableSource::Predefined;
    bool isValid() const { return cracking.isValid() && pileCap.isValid() && centeringBeams.isValid(); }
};

/// Everything edited from the Opciones buttons of the wizard.
struct OptionsSettings {
    FoundationOptionsSettings foundation;
    BeamOptionsSettings beams;
    JointOptionsSettings joints;
    bool isValid() const { return foundation.isValid() && beams.isValid() && joints.isValid(); }
};

}  // namespace cc::core
