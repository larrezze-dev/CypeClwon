#pragma once
#include <string>
#include <vector>

#include "core/panel.h"
#include "core/panel_distribution.h"
#include "core/project.h"
#include "core/undo_stack.h"

namespace cc::core {

/// Undoable edits of panels and surface loads (T-P7-05). See panel.h for the data and the checks and docs/research/ui/cargas_panos.md for the behaviour.
/// Every command keeps a snapshot of Panels and SurfaceLoads before and after (small), so undo / redo are exact; ids are never reused.
class PanelCommand : public UndoCommand {
public:
    PanelCommand(Project& project, Panels panels, SurfaceLoads loads, std::string text)
        : m_project(project), m_panelsBefore(project.panels), m_loadsBefore(project.surfaceLoads), m_panelsAfter(std::move(panels)),
          m_loadsAfter(std::move(loads)), m_text(std::move(text)) {}
    bool valid() const { return m_panelsBefore != m_panelsAfter || m_loadsBefore != m_loadsAfter; }
    void redo() override {
        restorePanels(m_project.panels, m_panelsAfter);
        restoreSurfaceLoads(m_project.surfaceLoads, m_loadsAfter);
    }
    void undo() override {
        restorePanels(m_project.panels, m_panelsBefore);
        restoreSurfaceLoads(m_project.surfaceLoads, m_loadsBefore);
    }
    std::string text() const override { return m_text; }

private:
    Project& m_project;
    Panels m_panelsBefore;
    SurfaceLoads m_loadsBefore;
    Panels m_panelsAfter;
    SurfaceLoads m_loadsAfter;
    std::string m_text;
};

enum class PanelError {
    None,
    NoPanels,            ///< no given panel exists
    UnknownLoad,         ///< no given surface load exists
    UnknownLoadCase,     ///< the load case does not exist
    InvalidData,         ///< the dialog data fail checkSurfaceLoadData (the issue carries the message)
    InvalidContour,      ///< the polygon does not define a panel / a surface load (#65130/#65133, #65131/#65134: the GUI picks the message)
    InvalidSide,         ///< the side of the direction does not exist / the polygon has no plane
    InvalidVertex,       ///< a vertex index does not exist
    VariableOnHorizontal,///< variable-with-height rows on a horizontal panel / surface
    NothingToDivide,     ///< the line crosses no panel
    CannotDivide,        ///< the line crosses a panel at other than two points
    NoBars,              ///< "Asignar": no given bar exists (or all are ties)
    NothingToChange,
};
/// Literal es-ES text of the errors that have none of their own (InvalidData: the issue message; InvalidContour: panelContourMessage).
std::string panelErrorMessage(PanelError error);

// --- panels ---

/// "Nuevo": a panel on `vertices` (RMB closes the polyline) with the direction of the side `side` (parallel / perpendicular, the combo of the panel
/// "Propiedades"; default side 0 Parallel = the first two points). The measured flow creates the panel first and then opens "Cargas en paño": Cancelar
/// keeps the panel without loads, so `loads` is normally empty. `why` receives the reason of InvalidContour; `issue` the dialog message of InvalidData.
PanelError addPanel(Project& project, UndoStack& stack, const std::vector<Point3>& vertices, int side = 0, PanelDirectionMode mode = PanelDirectionMode::Parallel,
                    const std::vector<SurfaceLoadData>& loads = {}, int* panelId = nullptr, ContourError* why = nullptr, SurfaceLoadIssue* issue = nullptr);
/// Same with an explicit direction (projected onto the plane; the first side when it is parallel to the normal).
PanelError addPanelWithDirection(Project& project, UndoStack& stack, const std::vector<Point3>& vertices, const Point3& direction,
                                 const std::vector<SurfaceLoadData>& loads = {}, int* panelId = nullptr, ContourError* why = nullptr,
                                 SurfaceLoadIssue* issue = nullptr);
/// "Editar": EVERY selected panel takes the given list of rows (measured: "igualar cargas entre distintos paños"). Rows are checked in order (range,
/// null value) and normalised; variable rows need all the selected panels to be non-horizontal. An empty list clears the loads. Unknown ids are skipped.
PanelError setPanelLoads(Project& project, UndoStack& stack, const std::vector<int>& panelIds, const std::vector<SurfaceLoadData>& rows,
                         SurfaceLoadIssue* issue = nullptr);
/// "Borrar": immediate, no question (measured). The surface loads contained in a deleted panel (and in no remaining one) go with it.
PanelError deletePanels(Project& project, UndoStack& stack, const std::vector<int>& panelIds, std::vector<int>* removedSurfaceLoads = nullptr);
/// "Mover": the vertices `vertexIndices` of the panel move by `delta` (destination minus reference point). A polygon that stops being valid
/// (non-planar: #65133 «Los puntos indicados ...») gives InvalidContour and nothing changes. The surface loads are left where they are (NOT measured).
PanelError movePanelVertices(Project& project, UndoStack& stack, int panelId, const std::vector<int>& vertexIndices, const Point3& delta,
                             ContourError* why = nullptr);
/// "Dividir": the line through p and q divides EVERY panel it crosses completely (measured: no previous selection; the cut plane contains the line and the
/// normal of the panel). The piece with the first vertex keeps the id, the others get new ids, all with a copy of the rows and the same direction
/// (measured). Not crossed panels stay; none crossed -> NothingToDivide; a panel cut at other than two points -> CannotDivide. Surface loads untouched.
PanelError dividePanels(Project& project, UndoStack& stack, const Point3& p, const Point3& q, std::vector<int>* createdIds = nullptr);
/// "Dirección de reparto": the direction of every given panel follows its side `side` (parallel / perpendicular). InvalidSide when no panel has it.
PanelError setPanelDirection(Project& project, UndoStack& stack, const std::vector<int>& panelIds, int side, PanelDirectionMode mode);

// --- surface loads ---

/// True when the polygon overlaps (or lies in) some panel: only then the original does NOT ask #63986 «¿Desea crear un paño asociado a la nueva carga
/// superficial?». Asked when false.
bool surfaceLoadOverlapsPanel(const Panels& panels, const std::vector<Point3>& contour);
/// "Nueva": a surface load on `contour` (RMB closes) in the case of `data`. `createPanel` (answer Sí of #63986) also creates a panel of the same shape with
/// the direction of the first two points (parallel, no loads, no further dialog: measured). Checks in order: contour, data, case, variable on horizontal.
PanelError addSurfaceLoad(Project& project, UndoStack& stack, const std::vector<Point3>& contour, const SurfaceLoadData& data, bool createPanel = false,
                          int* loadId = nullptr, int* panelId = nullptr, SurfaceLoadIssue* issue = nullptr, ContourError* why = nullptr);
/// "Editar": every selected load takes the whole definition of the dialog (case, type, values). Unknown ids skipped; none -> UnknownLoad.
PanelError editSurfaceLoads(Project& project, UndoStack& stack, const std::vector<int>& loadIds, const SurfaceLoadData& data, SurfaceLoadIssue* issue = nullptr);
/// "Hipótesis": moves the loads to another case, keeping the values (measured; the sense becomes that of the new case).
PanelError changeSurfaceLoadCase(Project& project, UndoStack& stack, const std::vector<int>& loadIds, int loadCaseId);
/// "Borrar": immediate, no question; of two stacked loads only the selected one goes (measured).
PanelError deleteSurfaceLoads(Project& project, UndoStack& stack, const std::vector<int>& loadIds);
/// "Copiar": a new load with the shape and position of `sourceId` and the data of the dialog (hipótesis, type, values). Equal loads are allowed and add up.
PanelError copySurfaceLoad(Project& project, UndoStack& stack, int sourceId, const SurfaceLoadData& data, int* newId = nullptr, SurfaceLoadIssue* issue = nullptr);
/// "Mover": the vertices `vertexIndices` of the load move by `delta`. With `alsoOtherCases` (the box «Mover también las cargas aplicadas sobre la misma
/// zona pertenecientes a otras hipótesis», checked by default) the loads of other cases with the same contour follow. An invalid polygon gives
/// InvalidContour (#65134) and nothing changes. `movedIds` receives the ids that moved.
PanelError moveSurfaceLoadVertices(Project& project, UndoStack& stack, int loadId, const std::vector<int>& vertexIndices, const Point3& delta,
                                   bool alsoOtherCases = true, std::vector<int>* movedIds = nullptr, ContourError* why = nullptr);
/// "Asignar": every given bar flips between active and inactive for surface loads, without question (measured: each RMB toggles). Ties and unknown bars
/// are ignored; none left -> NoBars.
PanelError toggleSurfaceBarAssignment(Project& project, UndoStack& stack, const std::vector<int>& barIds);
/// Same but sets the state instead of flipping it.
PanelError setSurfaceBarAssignment(Project& project, UndoStack& stack, const std::vector<int>& barIds, bool active);

}  // namespace cc::core
