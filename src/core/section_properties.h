#pragma once
#include <optional>
#include <string>
#include <vector>

namespace cc::core {

/// Cross sections of the profile library (T-P5-05, core only) and the calculation of their mechanical properties.
/// Behaviour source: docs/research/ui/perfiles_biblioteca.md (REA pass) and docs/research/ui/perfiles_datos.md
/// (formulas, data sources, accuracy against published tables).
///
/// Conventions
///  - Metres, square metres (m2), m4 and m6 (warping constant); the display unit never reaches this layer.
///  - Local axes of the section: y is horizontal (flange width direction) and z vertical (depth direction); Iy is the
///    second moment of area ABOUT the y axis, so for an I section it is the strong axis (as in the original).
///  - A rolled section is described by nominal dimensions only (the same ones the original stores per shape, see
///    dimensionKeys()); the properties are computed with closed forms and contour integration (see section_properties.cpp).
///  - "Tabulated if it exists, otherwise calculated" (rule of the original, `func_valor_calculado`): every Section may carry
///    tabulated values (SectionOverrides) that replace the computed ones; the elastic moduli are always I / extreme fibre
///    distance (the original has no field of its own for them).
///
/// Extending to cold-formed and aluminium shapes: add a ShapeKind value, its dimension keys / names / count to the tables
/// at the top of section_properties.cpp, its contour (or closed forms) to calculated(), and its validation to
/// validateSection(). Nothing else changes: series, library, undo and JSON are shape agnostic. Those shapes are NOT
/// implemented (the original computes them with `creaconf_*` routines that are not analysed yet, TK-052).

/// Shapes with formulas. The eight hot-rolled steel shapes of the original plus the generic profile.
enum class ShapeKind {
    IParallel,     ///< I / H, parallel flanges (IPE, HEA, HEB, HEM): h, b, tw, tf, r
    ISloped,       ///< I with inclined flanges (IPN): h, b, tw, tf (at b/4 from the tip), r1, r2, slopePercent
    Channel,       ///< U with inclined flanges (UPN): h, b, tw, tf (at b/2 from the tip), r1, r2, slopePercent
    Tee,           ///< T: h, b, tw, tf, r
    AngleEqual,    ///< L, equal legs: h, t, r1, r2
    AngleUnequal,  ///< L, unequal legs: h (vertical leg), b (horizontal leg), t, r1, r2
    FlatBar,       ///< "Plano": h (depth, along z), b (width, along y)
    RoundBar,      ///< "Redondo": d
    SquareBar,     ///< "Cuadrado": a
    Generic        ///< "Genérico": distances from the centre of gravity to the envelope (yNeg, yPos, zNeg, zPos); values tabulated
};

/// Material family that a series belongs to ("Perfil de acero laminado", "conformado", ...). Only HotRolledSteel and Generic
/// have shapes with formulas today.
enum class SectionFamily { HotRolledSteel, BuiltUpSteel, ColdFormedSteel, Aluminium, Timber, Generic };

/// Stable machine name of a shape / family (JSON) and its Spanish name (GUI).
const char* shapeKey(ShapeKind kind);
const char* shapeNameEs(ShapeKind kind);
const char* familyKey(SectionFamily family);
const char* familyNameEs(SectionFamily family);
std::optional<ShapeKind> shapeFromKey(const std::string& key);
std::optional<SectionFamily> familyFromKey(const std::string& key);
/// Every shape, in declaration order.
const std::vector<ShapeKind>& allShapes();

/// Names of the dimensions of a shape, in the order of Section::dims (e.g. {"h","b","tw","tf","r"}).
const std::vector<std::string>& dimensionKeys(ShapeKind kind);
/// Slope of the inclined flanges, in percent, of the standard series (verified against the published areas and
/// inertias: IPN 14 %, UPN 8 %; see perfiles_datos.md).
inline constexpr double kIpnSlopePercent = 14.0;
inline constexpr double kUpnSlopePercent = 8.0;

/// Values supplied by a table (manufacturer / standard) instead of being computed. Unset = calculated.
struct SectionOverrides {
    std::optional<double> area, iy, iz, it, iw, wplY, wplZ, avy, avz;
    bool any() const;
    bool operator==(const SectionOverrides&) const = default;
};

/// One profile of a series: reference ("IPE 200"), shape, dimensions (metres, order of dimensionKeys()) and optional tabulated
/// values.
struct Section {
    std::string reference;
    ShapeKind kind = ShapeKind::IParallel;
    std::vector<double> dims;
    SectionOverrides tabulated;
    bool operator==(const Section&) const = default;
};

/// Mechanical properties. Elastic moduli are the smaller of the two extreme-fibre values.
struct SectionProperties {
    double area = 0;                 ///< A, m2
    double iy = 0, iz = 0;           ///< m4, about the centroidal y / z axes
    double it = 0;                   ///< St. Venant torsion constant, m4
    double iw = 0;                   ///< warping constant, m6
    double welY = 0, welZ = 0;       ///< m3
    double wplY = 0, wplZ = 0;       ///< m3 (neutral axis dividing the area in two equal parts)
    double avy = 0, avz = 0;         ///< shear areas, m2
    double yCg = 0, zCg = 0;         ///< centroid measured from the lowest-left point of the envelope, m
    double yNeg = 0, yPos = 0;       ///< distance from the centroid to the envelope towards -y / +y, m
    double zNeg = 0, zPos = 0;       ///< idem for z
};

/// Empty when the dimensions of the section are valid, otherwise the Spanish message of the first problem (original
/// strings where the REA pass found them, otherwise the clone's own wording).
std::string validateSection(const Section& section);

/// Properties from the geometry alone, ignoring tabulated values. nullopt when validateSection() fails or the shape has no
/// geometry (Generic).
std::optional<SectionProperties> calculatedProperties(const Section& section);

/// Properties with the rule "tabulated if it exists, otherwise calculated". nullopt when the section is invalid or a Generic
/// section lacks the tabulated area / inertias it needs.
std::optional<SectionProperties> effectiveProperties(const Section& section);

}  // namespace cc::core
