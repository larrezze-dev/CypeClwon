#pragma once
#include <optional>
#include <string>
#include <vector>

namespace cc::core {

/// Where the label of a grid line is drawn ("Posición de la etiqueta"): "Origen" (0) or "Extremo" (1) in the original.
enum class LabelPosition { Origin, End };

/// One labelled reference line of the grid (docs/research/ui/planos.md section 10.2).
struct GridLine {
    std::string label;                           ///< "Etiqueta"
    double coordinate = 0.0;                     ///< metres, within +-kMaxGridCoordinate
    LabelPosition labelPosition = LabelPosition::Origin;

    bool operator==(const GridLine&) const = default;
};

/// A horizontal plane at an absolute elevation ("Nivel"), planos.md section 4 and 5.
struct Level {
    std::string name;        ///< "Referencia"
    double elevation = 0.0;  ///< "Cota absoluta", metres

    bool operator==(const Level&) const = default;
};

/// Groups of parallel planes ("Agrupar planos"). A plane is identified by its coordinate along the axis it is
/// perpendicular to; a plane belongs to at most one group of its axis. Minimum viable model: the grouping commands
/// themselves are not implemented yet.
struct PlaneGroups {
    std::vector<std::vector<double>> x;  ///< planes perpendicular to X (coordinates in metres)
    std::vector<std::vector<double>> y;
    std::vector<std::vector<double>> z;

    bool operator==(const PlaneGroups&) const = default;
};

/// "Referencias" dialog (planos.md section 12.2): which reference lines are drawn. Defaults are those of the original.
struct ReferenceOptions {
    bool showGrid = true;            ///< "Mostrar la rejilla"
    bool referenceLines = true;      ///< "Líneas de referencia" (master switch of the five below)
    bool onlyWithNode = false;       ///< "Sólo las que tienen nudo"
    bool parallelX = true;           ///< "Paralelas a X"
    bool parallelY = true;           ///< "Paralelas a Y"
    bool parallelZ = true;           ///< "Paralelas a Z"
    bool colourByDirection = false;  ///< "Dibujar con color según dirección" (X red, Y green, Z blue)

    bool operator==(const ReferenceOptions&) const = default;
};

/// Everything stored by "Geometria > Planos": the grid, the levels and the plane groups.
struct WorkPlanes {
    std::vector<GridLine> gridX;  ///< lines "en la direccion X" (kept in the order they were entered, not sorted)
    std::vector<GridLine> gridY;
    /// "Ajustar los elementos del modelo por cambios en la posicion de las lineas de la rejilla" (checked by default).
    bool adjustElements = true;
    std::vector<Level> levels;  ///< in the order entered; use sortedLevels() for the display order
    PlaneGroups groups;
    /// "Visibilidad" (Mostrar/ocultar planos): false hides the grid lines (labels stay). Saved in the project; absent = visible.
    /// The original applies it to a chosen group of nodes; without nodes the clone applies it to the whole grid (ticket TK-035).
    bool visible = true;
    ReferenceOptions references;  ///< "Referencias"

    bool operator==(const WorkPlanes&) const = default;

    /// True when the whole content is acceptable for the project file: valid grid lines and plane groups, and either
    /// no level at all (levels are optional) or a valid set of levels (validateLevels).
    bool isValid() const;
};

constexpr double kMaxGridCoordinate = 50000.0;  ///< limit of the "Coordenada" column (+-50000 m)
constexpr int kMinGridLines = 2;                ///< "Numero de lineas" of the generator: 2..1000
constexpr int kMaxGridLines = 1000;
constexpr int kMinLevels = 2;
constexpr double kSameCoordinate = 0.001;  ///< two grid lines closer than or exactly this apart are "en la misma coordenada" (measured)

/// Rounds a grid coordinate to three decimals, as the original does when it is typed.
double roundCoordinate(double value);

// --- Defaults of the original dialogs ---------------------------------------------------------------------------

/// "Nueva linea": coordinate 0, empty label, label at the origin. The caller appends it to the end of the axis.
GridLine newGridLine();
/// "Nuevo nivel": reference "Nuevo nivel", elevation 0.
Level newLevel();

// --- Labels -------------------------------------------------------------------------------------------------------

/// Label that follows `label` ("cyperef_nueva_referencia_u"): letters count A..Z, AA, AB..AZ, BA..ZZ, AAA..; the case is kept;
/// a trailing run of digits is incremented keeping its width ("1" -> "2", "9" -> "10", "A09" -> "A10"). An empty label
/// gives "1". Other endings (not a letter or digit) give "1" like the original when it cannot increment.
/// The exact rule of the original (BASICOS.DLL FUN_1800a59c0) was not read: this is the documented minimum.
std::string nextReference(const std::string& label);

// --- Generation ("Generacion de lineas de rejilla" / "Generar niveles") --------------------------------------------

/// One direction of the grid generator; the defaults are those of the original (n = 4, 3 m, origin).
struct GridAxisSpec {
    int count = 4;               ///< 2..1000
    double spacing = 3.0;        ///< metres
    std::string firstLabel;      ///< "Etiqueta inicial"
    LabelPosition position = LabelPosition::Origin;
};

/// Both directions of the generator: X starts at "A", Y at "1".
struct GridSpec {
    GridAxisSpec x{4, 3.0, "A", LabelPosition::Origin};
    GridAxisSpec y{4, 3.0, "1", LabelPosition::Origin};
};

/// Lines i = 0..count-1 at coordinate i * spacing (the generator always starts at 0), first label = `firstLabel`, the rest
/// by nextReference. Throws std::invalid_argument (Spanish message) when count is outside 2..1000 or the spacing is not
/// finite or a coordinate leaves +-50000 m.
std::vector<GridLine> generateGridAxis(const GridAxisSpec& spec);

/// Replaces gridX and gridY of `planes` by the generated lines (the caller asks for confirmation first when lines exist).
/// Throws like generateGridAxis; on error `planes` is left unchanged.
void generateGrid(WorkPlanes& planes, const GridSpec& spec);

/// Levels "<prefix> <i>" for i = 0..count-1 at elevation start + i * spacing (the index starts at 0). Throws
/// std::invalid_argument("Debe definir al menos dos niveles.") when count < 2 or a non finite value is given.
std::vector<Level> generateLevels(const std::string& prefix, int count, double startElevation, double spacing);

// --- Validation (messages in Spanish, as in the original; empty string = valid) -----------------------------------

/// First problem of one grid direction: repeated label ("La etiqueta 'X' está repetida."), then two lines at the same
/// coordinate, 0.001 m or less apart ("Las líneas 'A' y 'B' están situadas en la misma coordenada."), then a coordinate that is not finite or
/// outside +-50000 m. Empty labels are not compared with each other (a new line has no label yet).
std::string validateGridLines(const std::vector<GridLine>& lines);

/// First problem of a level list: fewer than two levels ("Debe definir al menos dos niveles."), repeated reference
/// ("La referencia 'X' esta repetida."), two levels with the same elevation ("Los niveles 'A' y 'B' tienen asignada la
/// misma cota."). Strict: an empty list is an error here (the dialog asks for at least two); WorkPlanes::isValid
/// accepts "no levels defined".
std::string validateLevels(const std::vector<Level>& levels);

/// Levels sorted by elevation (stable, so equal elevations keep their order): the order in which they are shown.
std::vector<Level> sortedLevels(const std::vector<Level>& levels);
/// Grid lines sorted by coordinate (stable); the stored order is kept in WorkPlanes.
std::vector<GridLine> sortedGridLines(const std::vector<GridLine>& lines);

// --- Storage ------------------------------------------------------------------------------------------------------

/// JSON text of `planes` (section "work_planes" of the project file; enumerations stored by name).
std::string toJson(const WorkPlanes& planes);

/// Parses `jsonText`. Missing fields keep their default; malformed text, wrong types or unknown names give an empty result.
std::optional<WorkPlanes> workPlanesFromJson(const std::string& jsonText);

}  // namespace cc::core
