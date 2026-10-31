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

/// Default (read-only) deflection limits, in the order shown by the original.
const std::vector<DeflectionLimitGroup>& defaultDeflectionLimits();

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
    ContinuousBeamNumbering continuousNumbering;
    BeamNumbering numbering = BeamNumbering::None;
    bool isValid() const { return errors.isValid() && continuousNumbering.isValid(); }
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

struct JointOptionsSettings {
    JointMaterials materials;
    BoltSettings bolts;
    bool isValid() const { return materials.isValid() && bolts.isValid(); }
};

/// Everything edited from the Opciones buttons of the wizard.
struct OptionsSettings {
    BeamOptionsSettings beams;
    JointOptionsSettings joints;
    bool isValid() const { return beams.isValid() && joints.isValid(); }
};

}  // namespace cc::core
