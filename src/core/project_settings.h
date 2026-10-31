#pragma once
#include <string>
#include <vector>

namespace cc::core {

/// General project settings edited by the "Obra vacía" wizard (docs/research/ui/dialogs_nueva_obra_wizard.md, 3.2-3.3).
/// Only the sections implemented so far are present; Acciones, Opciones, Ambiente, Terreno and Pilotes
/// arrive with T-P2-05.

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

}  // namespace cc::core
