#pragma once
#include <QList>
#include <QObject>
#include <QPointer>
#include <QPolygonF>
#include <QString>
#include <functional>
#include <optional>
#include <set>
#include <vector>

#include "core/camera.h"
#include "core/panel_edit.h"
#include "core/project.h"
#include "core/undo_stack.h"

class QAction;
class QActionGroup;
class QDialog;
class QPainter;
class QPointF;
class QWidget;

namespace cc::app {

class NodalLoadTool;
class PanelDirectionPanel;
class Viewport3D;

/// Carga > Paños and Carga > Cargas superficiales (T-P7-05, GUI part; model, checks and undoable commands in core/panel*.h). Behaviour measured in
/// the original (docs/research/ui/cargas_panos.md sections 10-12): LMB selects (a click on the element toggles it), RMB acts. Panels and surface
/// loads are not elements of the structure, so the tool keeps its own selection and its own hit-testing (PanelPicking.h) on the viewport's pick mode.
///
/// Paños: Nuevo (points of the polyline with LMB, RMB closes, Información #65132, LMB on a prolonged side + RMB, then «Cargas en paño»), Editar,
/// Borrar (no question), Mover (panel, vertices + RMB, #65137, reference point, destination), Dividir (two points), Dirección de reparto.
/// Cargas superficiales: Nueva (polyline, RMB, dialog, question #63986), Editar, Borrar, Hipótesis, Copiar (one click opens the dialog), Mover (box
/// #65139 on activation), Asignar (RMB toggles the bars, no dialog). The floating window «Propiedades - Hipótesis vista» belongs to the
/// NodalLoadTool (`host`), shared with the other load tools.
class PanelTool : public QObject {
    Q_OBJECT
public:
    enum class Mode {
        None,
        PanelNew, PanelEdit, PanelDelete, PanelMove, PanelDivide, PanelDirection,
        SurfNew, SurfEdit, SurfDelete, SurfCase, SurfCopy, SurfMove, SurfAssign
    };

    explicit PanelTool(QObject* parent = nullptr);

    QAction* action(Mode mode) const;
    QAction* panelNewAction() const { return action(Mode::PanelNew); }
    QAction* surfNewAction() const { return action(Mode::SurfNew); }
    /// The ribbon buttons whose tooltip must stay empty (measured: Dividir and Hipótesis have none).
    std::vector<QAction*> actionsWithoutTooltip() const;

    void setHost(NodalLoadTool* host);
    void setProject(cc::core::Project* project, cc::core::UndoStack* undo);
    void addViewport(Viewport3D* viewport);
    Mode mode() const { return m_mode; }
    /// Step inside the command (0 = first).
    int step() const { return m_step; }
    void deactivate();
    /// Call after every change of the history (push, undo, redo): enables / disables the buttons, prunes the selections, refreshes the viewports.
    void structureChanged();

    void setDialogParent(QWidget* parent) { m_dialogParent = parent; }
    /// Runs a modal dialog; the default is exec(); tests replace it so that nothing blocks.
    void setDialogRunner(std::function<int(QDialog&)> runner) { m_runner = std::move(runner); }

    static bool isPanelMode(Mode mode) { return mode >= Mode::PanelNew && mode <= Mode::PanelDirection; }
    static bool isSurfaceMode(Mode mode) { return mode >= Mode::SurfNew && mode <= Mode::SurfAssign; }
    /// «Paños - Nuevo |  - LMB: Introduzca los puntos de la polilínea.» ... (measured texts, with the step inside the command).
    static QString statusText(Mode mode, int step = 0, bool hasSelection = false);
    /// Rich text of the tooltip of a button (title, then the help text of the original); empty when the original shows none.
    static QString tipText(Mode mode);

    // --- state, for the drawing and the tests ---
    const std::set<int>& selectedPanels() const { return m_selPanels; }
    const std::set<int>& selectedLoads() const { return m_selLoads; }
    const std::set<int>& selectedVertices() const { return m_selVertices; }
    const std::vector<cc::core::Point3>& points() const { return m_points; }
    std::optional<int> selectedSide() const { return m_side; }
    cc::core::PanelDirectionMode directionMode() const { return m_direction; }
    void setDirectionMode(cc::core::PanelDirectionMode mode);
    bool moveAlsoOtherCases() const { return m_alsoOther; }

    // --- the gestures (what the viewport signals call; public so that the tests can drive them without a GPU) ---
    /// LMB in pick mode at the widget position.
    void pickAt(Viewport3D* viewport, const QPointF& pos);
    /// RMB in pick mode / on the bar selection of Asignar.
    void act(Viewport3D* viewport);
    /// A point entered in snap mode / RMB in snap mode.
    void pointEntered(const cc::core::Vec3& world);
    void rightClicked();

    /// Direct actions (what RMB does once the gesture is complete); true when the work changed.
    bool editPanels(const std::vector<int>& panelIds);
    bool deletePanels(const std::vector<int>& panelIds);
    bool editSurfaceLoads(const std::vector<int>& loadIds);
    bool changeSurfaceCase(const std::vector<int>& loadIds);
    bool deleteSurfaceLoads(const std::vector<int>& loadIds);
    bool assignBars(const std::vector<int>& barIds);
    /// The polyline is closed with RMB: Nuevo (panel side step) / Nueva (dialog and question).
    void closePolyline();

    /// Surface loads that are in view (the viewed hypothesis, or all).
    bool surfaceLoadInView(const cc::core::SurfaceLoad& load) const;
    bool caseInView(int loadCaseId) const;

signals:
    void statusChanged(const QString& text);
    /// A command is about to become active: other viewport tools must stop.
    void aboutToActivate();
    void modeChanged(cc::app::PanelTool::Mode mode);

private:
    void setMode(Mode mode);
    void configureViewports();
    void updateEnabled();
    void pruneViewports();
    void resetSteps();
    void announce();
    std::vector<cc::core::Vec3> rubber() const;
    void warn(const QString& text, bool error = false);
    void info(const QString& text);
    bool ask(const QString& text);
    int run(QDialog& dialog);
    int viewCase() const;
    bool usesSnap() const;
    bool usesPick() const;
    bool showsViewPanel() const;
    void showDirectionPanel();
    void closeDirectionPanel();
    void showMoveBox();
    bool panelStepSides() const;  ///< the prolonged sides are drawn and can be picked
    const std::vector<cc::core::Point3>* sideContour() const;
    std::vector<std::pair<int, QPolygonF>> panelPolygons(const Viewport3D& viewport) const;
    std::vector<std::pair<int, QPolygonF>> loadPolygons(const Viewport3D& viewport) const;
    void togglePanels(const std::vector<int>& ids);
    void toggleLoads(const std::vector<int>& ids);
    void createPanelFromPolyline();
    void createSurfaceFromPolyline();
    void applyMove(const cc::core::Point3& delta);
    void paint(QPainter& painter, const Viewport3D& viewport) const;
    const std::vector<cc::core::Point3>* movingPolygon() const;

    std::vector<std::pair<Mode, QAction*>> m_actions;
    QActionGroup* m_group = nullptr;
    cc::core::Project* m_project = nullptr;
    cc::core::UndoStack* m_undo = nullptr;
    QPointer<NodalLoadTool> m_host;
    QList<QPointer<Viewport3D>> m_viewports;
    Mode m_mode = Mode::None;
    int m_step = 0;
    QPointer<QWidget> m_dialogParent;
    QPointer<PanelDirectionPanel> m_directionPanel;
    std::function<int(QDialog&)> m_runner;
    bool m_configured = false;
    std::vector<cc::core::Point3> m_points;     ///< polyline of Nuevo / Nueva, or the reference / destination of Mover, or the line of Dividir
    std::vector<cc::core::Point3> m_contour;    ///< closed polygon waiting for its side (Nuevo, step 1)
    std::set<int> m_selPanels;
    std::set<int> m_selLoads;
    std::set<int> m_selVertices;
    int m_movingId = 0;                         ///< panel / surface load of the Mover command
    std::optional<int> m_side;
    cc::core::PanelDirectionMode m_direction = cc::core::PanelDirectionMode::Parallel;
    bool m_alsoOther = true;                    ///< «Mover también las cargas ... otras hipótesis» (checked by default, measured)
    int m_copySource = 0;                       ///< highlighted (cyan) while the Copiar dialog is open
};

}  // namespace cc::app

Q_DECLARE_METATYPE(cc::app::PanelTool::Mode)
