#pragma once
#include <optional>
#include <string>
#include <vector>

#include "core/profile_series.h"
#include "core/section_properties.h"
#include "core/structure_model.h"

namespace cc::core {

/// Description of a bar ("Describir" dialog of the original, T-P5-07; behaviour source docs/research/ui/dialogs_biblioteca_perfiles.md
/// section 2). A description groups everything the dialog collects: structural element type, behaviour, material icon, shape,
/// series / profile of the library, arrangement ("disposición") and the data of the generic section. Descriptions are shared:
/// bars with equal descriptions point to one description (through Bar::sectionId, see BarDescriptions::findBySection) and to the
/// CrossSection / Material generated from it, so the solver keeps reading Bar::sectionId / materialId unchanged.
///
/// PROVISIONAL (not measured in the original, TK-065): the arrangement and the stiffeners are stored but the mechanical
/// properties generated are those of the single profile; the names of the 8 material icons; the arrangements offered for
/// shapes other than I and for aluminium; the materials generated for concrete, timber and aluminium (TK-063).

/// The 8 material icons of "Selección del material", in the order of the dialog.
enum class BarMaterialIcon {
    RolledSteel,         ///< 1: acero laminado (default)
    RolledSteelFamily2,  ///< 2: second steel family with IPC series, rolled / built-up (name not verified)
    ColdFormedSteel,     ///< 3: acero conformado
    Timber,              ///< 4: madera
    ExtrudedAluminium,   ///< 5: series "(EP) I", probably extruded aluminium (name not verified)
    SpecialAluminium,    ///< 6: "Secciones especiales de aluminio"
    Concrete,            ///< 7: hormigón
    Generic              ///< 8: sección genérica (values typed by the user)
};

/// "Disposición" (Datos adicionales).
enum class Arrangement {
    Simple,             ///< Perfil simple (default)
    SimpleWithHaunches, ///< Simple con cartelas
    WithConcreteSlab,   ///< Con losa de hormigón
    DoubleBoxWelded,    ///< Doble en cajón soldado
    DoubleBoxBattened,  ///< Doble en cajón con presillas
    DoubleBoxGeneric,   ///< Doble en cajón unión genérica
    HalfProfile,        ///< Medio perfil
    SideFlatPlates,     ///< Con platabandas laterales
    Boyd,               ///< Boyd (alma aligerada)
    DoubleIWelded,      ///< Doble en I unión soldada
    DoubleIBattened,    ///< Doble en I presillas
    DoubleIGeneric      ///< Doble en I unión genérica
};

/// "Comportamiento" combo.
enum class BehaviourKind { Linear, TensionOnly, CompressionOnly };

const char* materialIconKey(BarMaterialIcon icon);
std::optional<BarMaterialIcon> materialIconFromKey(const std::string& key);
const char* arrangementKey(Arrangement arrangement);
std::optional<Arrangement> arrangementFromKey(const std::string& key);
const char* behaviourKey(BehaviourKind behaviour);
std::optional<BehaviourKind> behaviourFromKey(const std::string& key);
/// Spanish names for the GUI (arrangement and behaviour are original strings; the icon names are the clone's, tooltips unmeasured).
const char* materialIconNameEs(BarMaterialIcon icon);
const char* arrangementNameEs(Arrangement arrangement);
const char* behaviourNameEs(BehaviourKind behaviour);
const std::vector<BarMaterialIcon>& allMaterialIcons();

/// Library family a material icon draws its series from; nullopt for Concrete and Generic (no library series).
std::optional<SectionFamily> familyOfIcon(BarMaterialIcon icon);
/// Shapes offered ("Selección del perfil") for an icon, in the order of the original's icons. Rolled steel = the 8 rolled
/// shapes; concrete / timber from section_properties; Generic = {Generic}. The other families have no shapes with formulas yet
/// (TK-052), so they offer none.
std::vector<ShapeKind> shapesFor(BarMaterialIcon icon);
/// Arrangements offered for the icon and shape (section 2): rolled steel I = 9; family 2 = Perfil simple, Con losa; cold-formed
/// = Perfil simple + 3 box + 3 double I; timber = Perfil simple. Anything else: Perfil simple only (PROVISIONAL for the shapes
/// of rolled steel other than I, aluminium, concrete and generic: the original shows nothing else in the captures).
std::vector<Arrangement> arrangementsFor(BarMaterialIcon icon, ShapeKind shape);
/// True when the "Hay rigidizadores de alma" check box exists (family 2, both arrangements).
bool stiffenersAllowed(BarMaterialIcon icon, Arrangement arrangement);

/// Web stiffeners ("Hay rigidizadores de alma"); the fields are disabled until the box is ticked.
struct WebStiffeners {
    bool present = false;
    double spacingM = 1.0;     ///< "Separación entre rigidizadores", default 1000.0 mm
    double thicknessM = 0.005; ///< "Espesor", default 5.0 mm
    bool operator==(const WebStiffeners&) const = default;
};

/// Section of the "genérica" icon. Defaults are the ones of the dialog: 25.0 mm on every side (a 50 x 50 mm square), the
/// mechanical characteristics consistent with it (A 25.00 cm2, Avy = Avz = 5/6 A = 20.83 cm2, Iyy = Izz = 52.08 cm4) and It 87.50 cm4.
struct GenericSectionData {
    double widthLeftM = 0.025, widthRightM = 0.025;   ///< "Ancho izquierda / derecha"
    double depthBottomM = 0.025, depthTopM = 0.025;   ///< "Canto inferior / superior"
    double areaM2 = 25.0e-4;
    double shearAreaYM2 = 25.0e-4 * 5.0 / 6.0;
    double shearAreaZM2 = 25.0e-4 * 5.0 / 6.0;
    double inertiaYM4 = 52.08333333333333e-8;
    double inertiaZM4 = 52.08333333333333e-8;
    double torsionM4 = 87.5e-8;
    bool operator==(const GenericSectionData&) const = default;
};

/// Material block of the generic icon (and the values the other icons generate).
struct MaterialData {
    double elasticModulusMPa = 206010.0;
    double poissonRatio = 0.3;
    double thermalExpansion = 1.2e-5;  ///< 1/ºC ("dilatación 0.000012 m/m°C")
    double densityKnM3 = 77.01;        ///< "peso" 77.01 kN/m3
    bool operator==(const MaterialData&) const = default;
};

struct BarDescription {
    int id = 0;
    ElementType elementType = ElementType::Generic;  ///< "Tipo de elemento estructural" (default Genérico)
    BehaviourKind behaviour = BehaviourKind::Linear;
    BarMaterialIcon materialIcon = BarMaterialIcon::RolledSteel;
    ShapeKind shape = ShapeKind::IParallel;
    std::string seriesRef = "IPE";       ///< library series (icons with a family); empty otherwise
    std::string profileRef = "IPE 80";   ///< library profile
    Arrangement arrangement = Arrangement::Simple;
    WebStiffeners stiffeners;
    GenericSectionData generic;          ///< used when materialIcon == Generic
    /// Section typed in the dialog for the icons without library series: concrete (type + dimensions) and timber when seriesRef is
    /// empty. Unused otherwise.
    std::optional<Section> inlineSection;
    /// "Canto del perfil" of the timber series of variable depth (RV-60...): initial / final depth of the piece, default 100.0 mm each
    /// (describir_barras_capturas_manuales.md, capture 101). Only read when the library profile has variable depth.
    double depthStartM = 0.1, depthEndM = 0.1;
    MaterialData material;               ///< generic: typed by the user; other icons: defaultMaterialData(icon)
    int sectionId = 0;                   ///< CrossSection generated from this description (0 = none yet)
    int materialId = 0;                  ///< Material generated from it
    bool operator==(const BarDescription&) const = default;
};

/// Default material values of an icon: steel 210000 MPa / 0.3 / 78.5 kN/m3 / 1.2e-5; aluminium (EN 1999-1-1) 70000 / 0.3 / 27 /
/// 2.3e-5; concrete C25/30 of the original (31476 MPa, 0.2, 1e-5, 25 kN/m3); timber provisional (11000 MPa, 4.2 kN/m3, 5e-6, TK-063);
/// generic: the dialog defaults (206010 / 0.3 / 77.01 / 1.2e-5).
MaterialData defaultMaterialData(BarMaterialIcon icon);
MaterialKind materialKindOf(BarMaterialIcon icon);
/// Property set of the timber strength classes. The original's ec5.dll holds two tables per class (REA): `En` = the first one (EC5 /
/// norm values 1, 4, 5 of the original), `CteSeM` = the second, identical to CTE DB SE-M Annex E (norm value 3). The mapping from
/// the project's standard to the value is NOT verified: the Spanish standard is ASSUMED to use CteSeM (reasonable, unconfirmed).
enum class TimberPropertySet { En, CteSeM };
struct TimberClassProperties {
    std::string name, type;       ///< "C24", REA type key ("coniferas", "frondosas", "laminada_homogenea", "laminada_combinada")
    double e0MeanMPa = 0;         ///< E0,mean
    double gMeanMPa = 0;          ///< G,mean
    double densityKgM3 = 0;       ///< mean density
    bool rare = false;            ///< "poco frecuente" flag of ec5.dll
};
std::optional<TimberClassProperties> timberClassProperties(const std::string& strengthClass, TimberPropertySet set = TimberPropertySet::CteSeM);

/// Material values of a concrete grade / a timber class of the project.
///  - concrete: E (MPa) of the grade from the block "Norma Eurocodigo 2" of datmater.dll (REA exact, C25/30 = 31476, the "Bc" grades
///    included), nu = 0.2, alpha = 1e-5 1/C, weight 25 kN/m3 (the original loads 2.5 T/m3; the factor T -> kN is taken as 10, not
///    verified). G = E / (2 (1 + nu)) as `cypemat_modulo_cortadura`.
///  - timber: E0,mean and mean density of the class from ec5.dll (REA; default set CteSeM, see TimberPropertySet), weight = rho * 9.80665.
///    nu stays 0.3 and alpha 5e-6 as defaultMaterialData(Timber): the original derives nu = E0 / (2 G) - 1 (about 7), a value this
///    model rejects (nu must be below 0.5), and the thermal expansion of timber was not found. The timber type must match the class
///    (a class of another type returns the default).
MaterialData concreteMaterialData(const std::string& grade);
MaterialData timberMaterialData(const std::string& timberMaterial, const std::string& strengthClass,
                                TimberPropertySet set = TimberPropertySet::CteSeM);
/// Shear modulus of a material, G = E / (2 (1 + nu)).
double shearModulusMPa(const MaterialData& material);
/// G,mean of a timber class (ec5.dll table); 0 when the class is unknown.
double timberShearModulusMPa(const std::string& strengthClass, TimberPropertySet set = TimberPropertySet::CteSeM);

/// Description with the defaults of the dialog for an icon (rolled steel: IPE / IPE 80; concrete: rectangular 30 x 30 cm; timber:
/// rectangular 30 x 30 cm clone default, TK-061; generic: GenericSectionData defaults; other icons: no profile chosen yet).
BarDescription defaultDescription(BarMaterialIcon icon = BarMaterialIcon::RolledSteel);

/// Descriptions of a project (section bar_descriptions of the file), sorted by id; ids are never reused.
struct BarDescriptions {
    std::vector<BarDescription> items;
    int next = 1;
    const BarDescription* find(int id) const;
    /// Description that generated the cross section (nullptr for 0 or a section that was not generated by a description).
    const BarDescription* findBySection(int sectionId) const;
    bool operator==(const BarDescriptions&) const = default;
};

/// Section (shape + dimensions) a description resolves to: generic -> Section of kind Generic with the typed values; concrete /
/// inline timber -> inlineSection; others -> the library profile. nullopt when it cannot be found.
std::optional<Section> resolveSection(const BarDescription& description, const ProfileLibrary& library);

/// Empty when the description is consistent, otherwise the Spanish message of the first problem: unknown series / profile, family
/// of the series not matching the icon, shape not matching the profile, arrangement not offered, stiffeners where not allowed or
/// with non-positive values, invalid generic / inline section, material values out of range.
std::string validateDescription(const BarDescription& description, const ProfileLibrary& library);

/// Content comparison ignoring id, sectionId and materialId (what makes two bars share a description).
bool sameDescription(const BarDescription& a, const BarDescription& b);

/// Text for the bar ("IPE 80"; the rolled steel adds the grade of the project "IPE 80 (S275 (UNE-EN 10025-2))", the only
/// combination verified in the original, nudos.md). Generic: "Genérico". Concrete / timber inline: the reference of the section.
std::string descriptionText(const BarDescription& description, const std::string& hotRolledSteelGrade);

/// Label the original shows on the bar in the 3D window and in "Perfiles utilizados" (measured, describir_barras.md section 1): the
/// reference of the profile ("IPE 80"); generic "Gen.(19.6 kg/m)" (A x specific weight / g, one decimal, NO space after the dot);
/// concrete "30 cm x 30 cm" (dimensions in centimetres); timber from the section reference ("Ø100"). The shapes not measured
/// (circular or variable-depth concrete) fall back to the reference of the section.
std::string barLabelText(const BarDescription& description);

inline constexpr const char* kNotDescribedText = "Sin describir";

}  // namespace cc::core
