#pragma once
#include <filesystem>
#include <set>
#include <string>
#include <vector>

#include "core/import_geometry.h"

namespace cc::core {

/// Minimal ASCII DXF reader for "Geometria > Importar > DXF o DWG" (T-P5-11). Behaviour source: docs/research/ui/importar.md
/// sections 2 and 3.2 ([REA] dxflib / ed3dlib): LINE -> bar; polyline (LWPOLYLINE / POLYLINE) -> chain of bars, a closed one adds the
/// closing bar; 3DFACE, polyface mesh and polygon mesh (M x N) -> polygons, which go to shells or, for the layers marked with
/// "Importar polígonos como paños de carga", to load panels. Consecutive coincident corners (<= 1e-5) are dropped and a polygon with
/// fewer than 3 left is discarded. Every other entity is ignored. PROVISIONAL: the names of the entity types the original ignores
/// (importar.md "Pendiente"); LWPOLYLINE is taken as a polyline. Only the ENTITIES (and the colours of the TABLES layers) are read. The drawing unit is the metre (#64736), so there is no
/// scaling. Binary DXF and DWG are not supported (DWG is a closed binary format): the result carries the error "formato no soportado".

/// Longest bar imported, metres ([CAD] #65115 "más de mil metros").
inline constexpr double kDxfMaxBarLengthM = 1000.0;
/// Distance under which two points of the drawing are the same node. NOT MEASURED (importar.md: tolerance of the original unknown).
inline constexpr double kDxfNodeToleranceM = 1e-6;

struct DxfOptions {
    /// Layers to import; empty = all of them ("Visibilidad de capas", #64295).
    std::set<std::string> layers;
    /// Layers with the column "Importar polígonos como paños de carga" (#399490) set: their polygons (3DFACE, meshes) become load
    /// panels instead of shells. Polylines always give bars only.
    std::set<std::string> panelLayers;
};

/// A layer of the drawing as the list of "Selección de entidades" shows it.
struct DxfLayerInfo {
    std::string name;
    int colorIndex = 7;   ///< AutoCAD colour index (1..255) of the layer; 7 when the drawing does not say
    int segments = 0;     ///< lines and polyline segments
    int polygons = 0;     ///< 3DFACE and mesh polygons
};

/// RGB (0xRRGGBB) of an AutoCAD colour index. Indices 1-9 are the fixed ones; 10-249 follow the usual hue / saturation / value
/// ramp and 250-255 are greys (an approximation of the AutoCAD palette, which the original adjusts anyway: importar.md 3.2).
unsigned aciToRgb(int index);

struct DxfReadResult {
    ImportedGeometry geometry;
    std::vector<std::string> layers;       ///< layers that hold at least one supported entity, in order of appearance
    std::vector<ImportMessage> errors;     ///< the import must not go on when not empty
    std::vector<ImportMessage> warnings;   ///< #65115 (excessive length), #64502 (repeated bar)
    int linesRead = 0;
    int skippedLongBars = 0;
    int skippedRepeatedBars = 0;
    bool ok() const { return errors.empty(); }
};

/// Reads the text of an ASCII DXF file.
DxfReadResult readDxf(const std::string& content, const DxfOptions& options = {});
/// Reads a file: DXF as above; a binary DXF, a DWG (by extension or "AC10xx" signature) or an unreadable file gives an error.
DxfReadResult readDxfFile(const std::filesystem::path& path, const DxfOptions& options = {});

/// Layer names of the supported entities of an ASCII DXF text (what the layer list of "Importar geometría" would show).
std::vector<std::string> dxfLayers(const std::string& content);
/// Same with colour and counts, in order of appearance.
std::vector<DxfLayerInfo> dxfLayerInfo(const std::string& content);

}  // namespace cc::core
