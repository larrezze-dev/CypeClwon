#pragma once
#include <filesystem>
#include <set>
#include <string>
#include <vector>

#include "core/import_geometry.h"

namespace cc::core {

/// Minimal ASCII DXF reader for "Geometria > Importar > DXF o DWG" (T-P5-11, core only). Behaviour source:
/// docs/research/ui/importar.md section 2. PROVISIONAL as a whole: the entity types the original accepts and their conversion were not
/// found (importar.md "Pendiente"); this reader takes LINE -> bar, LWPOLYLINE / POLYLINE -> chain of bars (closed ones close the loop),
/// 3DFACE -> shell (3 or 4 corners). Only the ENTITIES section is read. The drawing unit is the metre (#64736), so there is no
/// scaling. Binary DXF and DWG are not supported (DWG is a closed binary format): the result carries the error "formato no soportado".

/// Longest bar imported, metres ([CAD] #65115 "más de mil metros").
inline constexpr double kDxfMaxBarLengthM = 1000.0;
/// Distance under which two points of the drawing are the same node. NOT MEASURED (importar.md: tolerance of the original unknown).
inline constexpr double kDxfNodeToleranceM = 1e-6;

struct DxfOptions {
    /// Layers to import; empty = all of them ("Visibilidad de capas", #64295).
    std::set<std::string> layers;
    /// "Importar polígonos como paños de carga" (#399490): closed polylines become load panels instead of being ignored as polygons.
    /// Without it closed polylines only give their edges as bars.
    bool polygonsAsPanels = false;
};

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

}  // namespace cc::core
