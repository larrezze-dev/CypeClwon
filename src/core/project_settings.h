#pragma once
#include <string>
#include <vector>

namespace cc::core {

/// General project settings edited by the "Obra vacía" wizard (docs/research/ui/dialogs_nueva_obra_wizard.md, 3.2-3.3).
/// All sections of the wizard are modelled here; Opciones has no settings yet (leaf dialogs, ticket TK-015).

/// Value lists, in the order shown by the original application. Items missing from a list because they
/// were not verified on screen are recorded in ticket TK-011 and are never invented.
const std::vector<std::string>& hotRolledSteelGrades();
const std::vector<std::string>& coldFormedSteelGrades();
const std::vector<std::string>& concreteGrades();
const std::vector<std::string>& timberMaterials();
const std::vector<std::string>& timberStrengthClasses();
const std::vector<std::string>& aluminiumAlloys();
const std::vector<std::string>& aluminiumTempers();
const std::vector<std::string>& rebarSteels();
const std::vector<std::string>& aggregateKinds();
const std::vector<std::string>& aggregateMaxSizes();

struct TimberSettings {
    std::string material = "Aserrada, procedente de coníferas o chopos";
    std::string strengthClass = "C24";
    int serviceClass = 1;  ///< 1, 2 or 3
    bool isValid() const;
    /// Text of the button on the wizard, e.g. "Aserrada, procedente de coníferas o chopos".
    std::string summary() const { return material; }
};

struct AluminiumSettings {
    std::string alloy = "EN AW-5083";
    std::string temper = "F";
    bool isValid() const;
    /// Button text, e.g. "EN AW-5083 - F".
    std::string summary() const { return alloy + " - " + temper; }
};

struct ProfileSettings {
    std::string hotRolledSteel = "S275 (UNE-EN 10025-2)";
    std::string coldFormedSteel = "S235";
    TimberSettings timber;
    AluminiumSettings aluminium;
    std::string concrete = "C25/30";
    bool isValid() const;
};

/// Rebar grade per element (dialog "Acero de barras").
struct RebarSteelSettings {
    std::string columnLongitudinal = "S-400", columnTransverse = "S-400";
    std::string beamTopAssembly = "S-400", beamTopReinforcement = "S-400", beamBottom = "S-400",
                beamTransverse = "S-400", beamSkin = "S-400";
    std::string footings = "S-400", pileCaps = "S-400", tieBeams = "S-400";
    bool isValid() const;
    /// Single grade when every field agrees, otherwise "Varios".
    std::string summary() const;
};

struct AggregateSetting {
    std::string kind = "Cuarcita";
    std::string maxSize = "15 mm";
};

struct AggregateSettings {
    AggregateSetting columns, beams, foundationBeams;
    double foundationMaxSizeMm = 30.0;
    bool isValid() const;
    /// e.g. "Cuarcita (15 mm), 30 mm" (all kinds equal) or "Varios (...)" otherwise.
    std::string summary() const;
};

/// Concrete cover in centimetres.
struct CoverSettings {
    double columns = 3.0;
    double beamTop = 3.0, beamBottom = 3.0, beamSide = 3.0;
    double footingTop = 4.0, footingBottom = 4.0, footingSide = 7.5;
    double tieBeamTop = 5.0, tieBeamBottom = 5.0, tieBeamSide = 5.0;
    static constexpr double kMin = 0.0, kMax = 50.0;
    bool isValid() const;
};

/// Rebar waste (mermas) in percent.
struct WasteSettings {
    double columns = 10.0, beams = 10.0, footings = 10.0, pileCaps = 10.0, tieBeams = 10.0;
    static constexpr double kMin = 0.0, kMax = 100.0;
    bool isValid() const;
};

struct ReinforcedConcreteSettings {
    std::string columnConcrete = "C25/30";
    std::string beamConcrete = "C25/30";
    std::string foundationConcrete = "C25/30";
    RebarSteelSettings rebarSteel;
    AggregateSettings aggregate;
    CoverSettings cover;
    WasteSettings waste;
    bool isValid() const;
};

/// Fire situation for steel elements (dialog "Resistencia al fuego", tab Acero). The Madera tab content was
/// not captured from the original (ticket TK-011) and is not modelled yet.
enum class SteelFireMode { Ignore, CheckResistance, CriticalTemperature };

/// "Acciones" section of the wizard (docs/research/ui/dialogs_nueva_obra_wizard.md, 3.4).
struct ActionsSettings {
    bool dynamicSeismic = false;  ///< "Con sismo dinámico"
    SteelFireMode steelFire = SteelFireMode::Ignore;
    bool foundationWind = true;   ///< "Considerar combinaciones con viento" (Cimentación)
    bool foundationSeism = true;  ///< "Considerar combinaciones con sismo"
    double formworkRemovalDays = 28.0;  ///< "Instante de descimbrado" (Proceso constructivo), in days
    static constexpr double kMinDays = 0.0, kMaxDays = 3650.0;
    bool isValid() const;
};

/// Exposure class for beams ("Ambiente" > Vigas). Only X0 (no corrosion risk) has a verified designation; the
/// other classes appear disabled in the dialog until their sub-options are captured (ticket TK-011).
enum class ExposureClass { NoRisk, Carbonation, Chlorides, SeaWaterChlorides, FreezeThaw, Chemical };

struct EnvironmentSettings {
    ExposureClass beams = ExposureClass::NoRisk;
    double maxCrackWidthMm = 0.40;  ///< read-only in the original
    std::string beamsSummary() const;  ///< button text, "X0" for NoRisk
    bool isValid() const { return beams == ExposureClass::NoRisk; }
};

/// One row of "Importar valores usuales de proyecto" (MPa).
struct UsualGround {
    const char* type;
    double persistent;
    double seismic;
};
const std::vector<UsualGround>& usualGroundValues();

/// "Terreno de cimentación" section.
struct GroundSettings {
    bool checkFootingSliding = false;
    double persistentStressMPa = 0.200;  ///< Situaciones persistentes (A2 - Conjunto C)
    double seismicStressMPa = 0.300;     ///< Situaciones sísmicas y accidentales (A2 - Conjunto C)
    static constexpr double kMinStress = 0.001, kMaxStress = 50.0;
    static bool isValidStress(double value);
    bool isValid() const { return isValidStress(persistentStressMPa) && isValidStress(seismicStressMPa); }
};

/// "Configuración avanzada" of reinforced-concrete piles, tab "Hundimiento" (556x786). The tabs "Comprobación
/// estructural" and "Tope estructural" were never opened in the original: only the "Tope estructural" tick
/// (unchecked) is known (ticket TK-016). The valid ranges of every field are unverified and not enforced beyond
/// "finite and not negative".
struct PileSinkingSettings {
    std::string resistanceCalculation = "Automático";
    std::string reference = "A2 + M2 + R3";  ///< set A2 - Conjunto C, read-only
    // Partial safety factors for the ground.
    double frictionAngle = 1.25, cohesion = 1.25, undrainedShear = 1.40, unconfinedCompression = 1.40, unitWeight = 1.00;
    // Partial safety factors for the pile: tip, shaft (compression), total (compression), shaft (tension).
    double geotechnicalTip = 1.00, geotechnicalShaft = 1.00, geotechnicalTotal = 1.00, geotechnicalShaftTension = 1.10;
    double unitTip = 1.00, unitShaft = 1.00, unitTotal = 1.00, unitShaftTension = 1.10;
    std::string efficiencyFormula = "Aplicar la fórmula de Converse-Labarre";
    bool ignoreEfficiencyWhenFar = true;  ///< "No considerar el factor de eficiencia si ... es igual o superior a K·D"
    double efficiencyK = 2.00;
    bool isValid() const;
};

/// How a concrete-strength reduction is expressed: relative (times fc) or absolute (MPa).
enum class StrengthBasis { Relative, Absolute };

/// Tab "Comprobación estructural" (orig_pilotes_ca_comprobacion_estructural). The lists of the two Relativa/Absoluta
/// combos were not opened in the original; both entries are offered for each combo. Valid ranges are unverified
/// and not enforced beyond "finite and not negative" (ticket TK-016).
struct PileStructuralSettings {
    bool reduceConcreteStrength = true;  ///< "Considerar reducción de la resistencia del hormigón"
    StrengthBasis precastBasis = StrengthBasis::Relative;  ///< driven precast pile
    double precastValue = 1.00;                            ///< times fc when relative, MPa when absolute
    StrengthBasis castInSituBasis = StrengthBasis::Absolute;  ///< cast-in-situ pile
    double castInSituValue = 18.00;
    double undrainedShearFactor = 1.40;  ///< Cu, partial factor for p-y curves
    double unitWeightFactor = 1.00;      ///< gamma, partial factor for p-y curves
    bool isValid() const;
};

struct PileAdvancedSettings {
    PileSinkingSettings sinking;
    PileStructuralSettings structural;
    bool structuralCap = false;  ///< "Tope estructural" tab tick; the panel is empty in the original
    bool isValid() const { return sinking.isValid() && structural.isValid(); }
};

/// Tab "Pilotes (Hormigón armado)" of the wizard.
struct PileSettings {
    double minMomentEccentricityM = 0.10;
    bool reducedAxial = true;          ///< "Considerar esfuerzos con un axil reducido"
    double shaftAxialFactor = 0.00;    ///< piles working by shaft, times the axial load
    double tipOnlyAxialFactor = 0.80;  ///< piles working by tip only, times the axial load
    double bucklingCoefficient = 1.00;
    bool lateralStability = false;     ///< "Análisis de la estabilidad lateral del pilote"
    PileAdvancedSettings advanced;
    bool isValid() const;
};

/// "Configuración avanzada" of micropiles (587x728). Tab "Hundimiento" reuses PileSinkingSettings: the micropile
/// dialog shows only the shaft factors of the geotechnical group (geotechnicalShaft / geotechnicalShaftTension);
/// tip and total stay at their defaults. The tab "Comprobación estructural" was not captured yet (ticket TK-016).
struct MicropileAdvancedSettings {
    PileSinkingSettings sinking;
    bool isValid() const { return sinking.isValid(); }
};

/// Tab "Micropilotes" of the wizard.
struct MicropileSettings {
    bool reducedAxial = true;
    double reducedAxialFactor = 0.00;
    std::string bucklingAnalysis = "Coeficiente de pandeo";  ///< the only entry seen in the combo
    double bucklingCoefficient = 1.00;
    bool lateralStability = false;
    double corrosionThicknessMm = 0.60;  ///< "Reducción de espesor de la armadura por efecto de la corrosión"
    double tubularAreaFactor = 1.00;     ///< "Coeficiente de minoración del área de la armadura tubular"
    double executionFactor = 1.00;       ///< "Coeficiente de influencia del tipo de ejecución"
    MicropileAdvancedSettings advanced;
    bool isValid() const;
};

}  // namespace cc::core
