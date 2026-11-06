#pragma once
#include <string>
#include <vector>

#include "core/section_properties.h"

namespace cc::core {

/// Registers of the structural element types of "Describir" other than Generico (T-P5-07, TK-070 / TK-076). Source: describir_barras.md
/// section 19 (REA of ed3dlib.dll / pilargeo.dll / seccviga.dll / cypeud.dll). The original keeps one register per type (seccion:
/// datos_seccion_barra / tirante / viga ...): switching the type radio never converts or clears the registers of the other types, so
/// BarDescription holds all of them at once and only the active one is applied.

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
    DoubleIGeneric,     ///< Doble en I unión genérica
    DoubleBoxFlatPlates ///< Doble en cajón con platabandas (only offered to steel beams with a U profile, describir_barras.md 24.2)
};

// --- Pilar (pilargeo.dll, epseccion_tipo_t) --------------------------------------------------------------------------------------

/// Icons of the "Material" group of the Pilar page, in screen order (i_tipo_material_t 0, 1, 2; timber and generic section are hidden
/// in CYPE 3D).
enum class PillarMaterial { Concrete, Steel, Mixed };
/// "Tipo" of the concrete pillar: the two icons visible to the user (epseccion_tipo_t 1 and 4).
enum class PillarConcreteKind { Rectangular, Circular };
/// The 6 icons of the mixed pillar, in screen order (epseccion_tipo_t 7..12; tooltips #75576, #75584, #75577, #75583, #75581, #75582).
enum class MixedPillarKind {
    RectangularEmbeddedProfile,  ///< 1: Pilar rectangular con perfil embebido
    RectangularBox,              ///< 2: Cajón rectangular de chapas de acero laminado, relleno de hormigón
    RectangularTube,             ///< 3: Tubo rectangular de acero conformado, relleno de hormigón
    SquareTube,                  ///< 4: Tubo cuadrado de acero conformado, relleno de hormigón
    CircularEmbeddedProfile,     ///< 5: Pilar circular con perfil embebido
    CircularTube                 ///< 6: Tubo circular de acero conformado, relleno de hormigón
};

/// Range and default of the pillar dimensions (cypeud: metric 10 - 1000 cm, default 30 cm; the imperial 4 - 400 in / 10 in does not
/// apply: the clone has no imperial unit).
inline constexpr double kPillarMinM = 0.10, kPillarMaxM = 10.0, kPillarDefaultM = 0.30;
/// Range and default of the beam dimensions (vigas_ancho_viga_crea: 5 - 500 cm, default 30 cm; flanges vigas_ancho_vuelos_viga_crea:
/// 5 - 500 cm, default 15 cm).
inline constexpr double kBeamMinM = 0.05, kBeamMaxM = 5.0, kBeamDefaultM = 0.30, kBeamFlangeDefaultM = 0.15;

struct PillarData {
    PillarMaterial material = PillarMaterial::Concrete;
    PillarConcreteKind concreteKind = PillarConcreteKind::Rectangular;
    MixedPillarKind mixedKind = MixedPillarKind::RectangularEmbeddedProfile;
    double widthXM = kPillarDefaultM, widthYM = kPillarDefaultM;  ///< "Dimensión X" / "Dimensión Y" (rectangular concrete and mixed)
    double diameterM = kPillarDefaultM;                           ///< "Diámetro" (circular)
    /// Steel profile (metallic pillar and embedded profile of the mixed one): library series / profile. PROVISIONAL: the original
    /// builds the selector with pnpiezamet_panel_pieza_metalica_pilar (piezamet.dll, not read); the clone reuses the rolled steel series.
    std::string seriesRef = "IPE", profileRef = "IPE 80";
    ShapeKind shape = ShapeKind::IParallel;
    /// "Disposición" of the steel pillar (and of the embedded profile); filtered by arrangementsForElement(Pillar, shape).
    Arrangement arrangement = Arrangement::Simple;
    bool operator==(const PillarData&) const = default;
};

// --- Tirante ---------------------------------------------------------------------------------------------------------------------

/// Register of the tie page: shape icon (0 Angular simétrico, 1 Angular, 2 Platabanda, 3 Redondo macizo, 4 Cuadrado macizo; the
/// original opens on Platabanda), series and profile (describir_catalogue.h tieSeries). The clone has no tie module, so a description
/// of this type cannot be applied yet.
struct TieData {
    int shape = 2;
    std::string seriesRef = "Pletinas", profileRef = "FL 20 x 4";
    bool operator==(const TieData&) const = default;
};

// --- Viga (seccviga.dll, svtipos_grupo_viga_t) ------------------------------------------------------------------------------------

/// The 6 families visible in CYPE 3D, in screen order (svtipos_grupo_viga_t 0, 1, 2, 4, 7, 8).
enum class BeamFamily { Flat, Hung, Lattice, Prestressed, Steel, Timber };

struct BeamData {
    BeamFamily family = BeamFamily::Hung;  ///< "Vigas descolgadas" is the default (observed)
    int type = 0;                          ///< index of the "Tipo" icon inside the family (see beamTypeCount)
    double widthM = kBeamDefaultM;         ///< "Anchura (b)"
    double depthM = kBeamDefaultM;         ///< "Canto (a)"
    double flangeLeftM = kBeamFlangeDefaultM, flangeRightM = kBeamFlangeDefaultM;  ///< "Ala izquierda (i)" / "Ala derecha (d)"
    /// "canto_vuelo" of the original: «Canto del ala (s)», 15 cm, 5 - 500 cm (REA, describir_barras.md section 21.B.3).
    double flangeDepthM = kBeamFlangeDefaultM;
    /// Hung beam type 7 (sección variable): initial / final depth (names of #404312). Defaults PROVISIONAL (not read).
    double depthStartM = kBeamDefaultM, depthEndM = kBeamDefaultM;
    bool belowSlab = false;  ///< «Viga bajo forjado» (#56456); default unchecked as seen in the original, the code default was not read
    /// Steel / timber beam: library series and profile. The new steel beam starts on the U shape with UPE 80 (measured in the
    /// original, describir_barras.md section 24.2); the selector itself is PROVISIONAL like PillarData (pnpiezamet_panel_pieza_metalica_viga).
    std::string seriesRef = "UPE", profileRef = "UPE 80";
    ShapeKind shape = ShapeKind::Channel;
    std::string timberSeriesRef, timberProfileRef;
    ShapeKind timberShape = ShapeKind::TimberRectangular;
    /// "Disposición" of the steel beam; filtered by arrangementsForElement(Beam, shape). Timber beams only offer Perfil simple.
    Arrangement arrangement = Arrangement::Simple;
    bool operator==(const BeamData&) const = default;
};

/// Number of icons of the "Tipo" group of a family (0 for steel and timber: they show the profile selector instead).
int beamTypeCount(BeamFamily family);
/// True when the check box «Viga bajo forjado» is shown: hung type 0, lattice type 4, steel and timber (FUN_18005a5d0, FUN_180059d40).
bool beamBelowSlabOffered(BeamFamily family, int type);
/// svseccion_es_viga_bajo_forjado: the stored flag where the box exists, false elsewhere.
bool beamIsBelowSlab(const BeamData& beam);

/// Numeric fields shown for a family / type. [ORIG] only the rectangular hung beam (Anchura, Canto); the flange fields of the T and
/// one-flange types and the variable depth fields are PROVISIONAL (ids ce_e_ai / ce_e_ad and #404312 name them).
enum class BeamField { Width, Depth, FlangeLeft, FlangeRight, FlangeDepth, DepthStart, DepthEnd };
std::vector<BeamField> beamFields(BeamFamily family, int type);

/// Cross section geometry of a beam type. T and flanged types need a shape the clone does not have yet.
enum class BeamSection { Rectangular, Variable, Other };
BeamSection beamSectionOf(BeamFamily family, int type);

/// Empty when valid, otherwise the Spanish message the original shows when accepting ("Aceptar", FUN_1800568b0): #157403 (T and
/// inverted T: flange depth not below the web depth) and #404312 (variable depth: initial and final equal), plus the range of the
/// numeric fields. Profile existence is checked by the caller (needs the library).
std::string validateBeamData(const BeamData& beam);
/// Range check of the pillar fields; for the mixed pillar with an embedded profile the profile must fit in the concrete (#75573) when
/// `profileHeightM` / `profileWidthM` are given (> 0) (PROVISIONAL: the caller of #75573 was not followed in the REA).
std::string validatePillarData(const PillarData& pillar, double profileHeightM = 0.0, double profileWidthM = 0.0);

/// Message of "Esta función aún no está disponible." kind used for the registers the clone cannot resolve yet.
inline constexpr const char* kNotAvailableText = "Esta función aún no está disponible.";

}  // namespace cc::core
