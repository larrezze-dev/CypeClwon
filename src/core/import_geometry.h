#pragma once
#include <string>
#include <vector>

#include "core/project.h"
#include "core/structure_snapshot.h"
#include "core/undo_stack.h"

namespace cc::core {

/// Geometry read from an external file (text file of the "Importar" group, DXF), before it enters the project (T-P5-11, core
/// only). Numbers are the user numbering of the file (positive, at most kMaxImportNumber); they are not model ids.
inline constexpr long long kMaxImportNumber = 999999;  ///< [CAD] #106855 / #106863 / #106886: "mayor que 999999"

struct ImportedNode {
    long long number = 0;
    Point3 position;
};
struct ImportedBar {
    long long number = 0;
    long long startNumber = 0, endNumber = 0;
};
/// A shell or a load panel: the file numbers of its corner nodes in contour order.
struct ImportedPolygon {
    long long number = 0;
    std::vector<long long> nodeNumbers;
};
struct ImportedGeometry {
    std::vector<ImportedNode> nodes;
    std::vector<ImportedBar> bars;
    std::vector<ImportedPolygon> shells;
    std::vector<ImportedPolygon> panels;  ///< "paños de carga": the model has no load panels yet (ticket), so they are not imported
};

/// A message of the importers: `id` is the string id of the original (0 = the clone's own wording), `line` the 1-based line
/// of the file (0 = not tied to a line).
struct ImportMessage {
    int id = 0;
    std::string text;
    long long line = 0;
};

/// What an import added to the project.
struct ImportSummary {
    int nodesCreated = 0;
    int barsCreated = 0;
    int shellsCreated = 0;
    int skippedBars = 0;    ///< repeated, zero-length or otherwise unusable bars
    int skippedShells = 0;  ///< repeated or invalid contours
    int ignoredPanels = 0;
};

/// Thickness given to imported shells. NOT MEASURED (the original does not say): provisional, equal to kDefaultShellThicknessM.
inline constexpr double kImportedShellThicknessM = 0.2;

/// Adds the geometry to the project: a node whose position coincides (kCoincidentNodeToleranceM, PROVISIONAL) with an existing one
/// is reused; new bars are not described (material / section 0) and everything goes to the active layer; a bar that repeats an
/// existing one is skipped; a polygon of 3 or 4 nodes becomes a shell (more nodes are cut into shells; invalid or repeated ones are
/// skipped). Not committed (nothing to push) when nothing was added.
class ImportGeometryCommand : public SnapshotCommand {
public:
    ImportGeometryCommand(Project& project, const ImportedGeometry& geometry);
    const ImportSummary& summary() const { return m_summary; }

private:
    ImportSummary m_summary;
};

/// Builds the command and pushes it when something was added; returns whether it did. `summary` is filled either way.
bool importGeometry(Project& project, UndoStack& stack, const ImportedGeometry& geometry, ImportSummary* summary = nullptr);

}  // namespace cc::core
