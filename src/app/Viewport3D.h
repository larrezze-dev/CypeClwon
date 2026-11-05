#pragma once
#include <QEnterEvent>
#include <QOpenGLWidget>
#include <QPoint>
#include <QMetaType>
#include <QPointF>
#include <QRectF>
#include <QSizeF>
#include <QChar>
#include <QString>
#include <QStringList>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "core/camera.h"
#include "core/categories.h"
#include "core/clipping.h"
#include "core/picking.h"
#include "core/project_settings.h"
#include "core/selection.h"
#include "core/snap.h"
#include "core/structure_model.h"
#include "core/work_planes.h"

namespace cc::app {

/// What a mouse button drag does in the 3D viewport.
enum class DragAction { None, Orbit, Pan };

/// Where the anchored information (node label, bar information box) is shown: the two entries of the original's
/// "Posición de los tooltips" combo (captured in tp2_combo_open.png). Default: "En la esquina superior derecha".
enum class TooltipPosition {
    TopRightCorner,  ///< "En la esquina superior derecha": flush against the corner of the 3D area
    OnElement        ///< "Sobre el elemento": just above the element the information is about
};

/// Mouse button assignment of the viewport (see docs/research/ui/viewport_3d.md for what the original does).
struct MouseBindings {
    // Original (verified): the middle button always pans; left and right drags do nothing by themselves. Orbiting is a
    // toolbar tool (the four-arrows icon of the top toolbar) that makes the left button orbit, ~0.25 degrees per pixel.
    DragAction left = DragAction::None;
    DragAction middle = DragAction::Pan;
    DragAction right = DragAction::None;
    double orbitDegreesPerPixel = 0.25;
};

/// 3D viewport: a QOpenGLWidget painted with QPainter (white background, axis guide lines, axes triad). The camera
/// logic lives in cc::core::Camera; this widget only translates input into camera calls, so it can be tested without a GPU.
class Viewport3D : public QOpenGLWidget {
    Q_OBJECT
public:
    explicit Viewport3D(QWidget* parent = nullptr);

    core::Camera& camera() { return m_camera; }
    const core::Camera& camera() const { return m_camera; }
    MouseBindings& bindings() { return m_bindings; }
    /// Navigation tool: what a left-button drag does (Orbit, Pan or None for the plain pointer).
    void setLeftDragAction(DragAction action) { m_bindings.left = action; }

    /// Nodes of the open structure: the guide lines are drawn through them. Without nodes, through the world origin.
    void setNodes(std::vector<core::Vec3> nodes);
    const std::vector<core::Vec3>& nodes() const { return m_nodes; }

    /// Structure shown (not owned; must outlive the viewport or be reset to nullptr). Bars and shells are drawn from it,
    /// it feeds the guide lines and it is what picking works on. Clears the selection.
    void setStructure(const core::StructuralModel* structure);
    /// The structure changed in place (an edit, an undo or a redo): rebuilds the node list the guide lines are drawn through,
    /// drops the hover / selection / moving node that no longer exist and repaints.
    void refreshStructure();
    /// "N<id>" labels (bold green, right of and above every node) shown while a command of Geometria > Nudos is active; in the
    /// normal view the original draws none (nudos.md section 8).
    void setNodeLabelsVisible(bool on);
    /// Bar labels of Geometria > Barras > Seccion / Material (describir_barras.md section 1): bar id -> text ("IPE 80", "30 cm x 30 cm",
    /// "Gen.(19.6 kg/m)", "C25/30"), drawn vertically in olive green at the middle of the bar. An empty map hides them.
    void setBarLabels(std::vector<std::pair<int, QString>> labels);
    const std::vector<std::pair<int, QString>>& barLabels() const { return m_barLabels; }
    bool nodeLabelsVisible() const { return m_nodeLabels; }
    /// Node being moved (Nudos > Mover, second step): drawn at the cursor position with the bars that reach it following it,
    /// and left out of the snap together with those bars so that it cannot snap to itself.
    void setMovingNode(std::optional<int> nodeId);
    const std::optional<int>& movingNode() const { return m_movingNode; }
    /// Geometria > Barras (Barra / Viga): nodes already chosen, in order. They are drawn as orange squares, joined by black
    /// lines, with an elastic black line from the last one to the cursor (the node under it when there is one). Empty: none.
    void setRubberChain(std::vector<int> nodeIds);
    const std::vector<int>& rubberChain() const { return m_rubberChain; }
    /// Geometria > Laminas / Herramientas: points entered so far that are not nodes (yet), in order. Same look as the chain of
    /// nodes: orange squares joined by black lines, plus an elastic line from the last one to the cursor (the snapped point
    /// when there is one). Empty: none.
    void setRubberPoints(std::vector<core::Vec3> points);
    const std::vector<core::Vec3>& rubberPoints() const { return m_rubberPoints; }
    /// "Ejes locales" of Geometria > Laminas: draws the local axes x (red), y (green) and z (blue) of every shell at its centre.
    void setShowShellAxes(bool on);
    bool showShellAxes() const { return m_showShellAxes; }
    const core::StructuralModel* structure() const { return m_structure; }

    /// Selection mode: what the original does inside a command of the Geometria ribbon tab. The guide lines are hidden,
    /// nodes show as blue squares, the element under the cursor is highlighted orange and the mouse selects:
    ///  - click on an element: selects it (Ctrl adds to the selection, Shift removes from it);
    ///  - click on empty space, move, click again: window (second corner to the right: only what is fully inside) or
    ///    crossing (to the left: whatever the rectangle touches), shown as a rubber band, continuous or dotted;
    ///  - right button: emits actOnSelection() (cancels a pending window first); Esc cancels a pending window.
    void setSelectionMode(bool on);
    bool selectionMode() const { return m_selectionMode; }
    void setSelectionFilter(const core::SelectionFilter& filter);
    /// "Categorías" panel: hidden categories are not drawn and cannot be picked or selected; with the layer colours off,
    /// bars and shells are drawn in the default colour. Ties, columns, beams and panels have no element type yet and the
    /// padlocks only matter for editing commands (TK-029): those fields are stored, not honoured.
    void setCategorySettings(const core::CategorySettings& settings);
    const core::CategorySettings& categorySettings() const { return m_categories; }
    /// The selection filter of the command narrowed by the hidden categories.
    core::SelectionFilter effectiveFilter() const;
    const core::SelectionFilter& selectionFilter() const { return m_filter; }
    const core::Selection& selection() const { return m_selection; }
    void clearSelection();
    /// Replaces the selection with the given elements (used by "Buscar" to show what it found).
    void setSelectionItems(const std::vector<core::ElementRef>& items);
    /// Element under the cursor in selection mode.
    const std::optional<core::ElementRef>& hovered() const { return m_hover; }
    /// True between the first and the second click of a window.
    bool windowPending() const { return m_windowStart.has_value(); }
    void cancelWindow();

    /// Point-entry mode (Nudos > Nuevo, Barra...): the cursor snaps to nodes, bars and guide lines (core/snap.h), the
    /// snap is shown as an hourglass marker with the snapped element in blue and a dimension label, and a left click
    /// emits pointEntered() with the snapped (or free) position. Independent of the selection mode.
    void setSnapMode(bool on);
    bool snapMode() const { return m_snapMode; }
    core::SnapOptions& snapOptions() { return m_snapOptions; }
    const core::SnapResult& currentSnap() const { return m_snap; }
    /// Position of the cursor in world coordinates: snapped when a snap applies, else on the target plane.
    core::Vec3 cursorWorld() const;

    /// Where the anchored information is shown ("Posición de los tooltips").
    void setTooltipPosition(TooltipPosition position);
    TooltipPosition tooltipPosition() const { return m_tooltipPosition; }
    /// "Mostrar la información de los elementos": the bar information box (snap mode and bar hover).
    /// "Dibujar las láminas con relleno": shells are drawn filled (true) or as an outline only (false, original default).
    /// "Dimensiones de los nudos": the blue/orange node square follows it, constant with the zoom (original: 15 cm ->
    /// ~14 px, 40 cm -> ~35 px, i.e. ~0.9 px per cm; captures op1_node_zoom.png).
    void setNodeSizeCm(double cm);
    double nodeSizeCm() const { return m_nodeSizeCm; }
    double nodeSquarePixels() const { return m_nodeSizeCm * 0.9; }
    double nodeHalf() const { return nodeSquarePixels() / 2.0; }
    void setFillShells(bool on);
    bool fillShells() const { return m_fillShells; }

    void setShowElementInfo(bool on);
    bool showElementInfo() const { return m_showElementInfo; }
    /// Bar the information box describes: the snapped bar (or a bar meeting at the snapped node) in snap mode, the
    /// hovered bar in selection mode. Empty when nothing applies or the option is off.
    std::optional<int> infoBarId() const;
    /// Shell the information box describes: the hovered shell in selection mode (the "ficha" of the original, laminas.md 7.3).
    std::optional<int> infoShellId() const;
    /// Profile settings of the work, used to name the "Material" of a shell that takes it from the general data.
    void setProfileSettings(const core::ProfileSettings* profiles) { m_profiles = profiles; update(); }
    /// Rows of the shell information box: "L1", "Capa : ...", "Espesor : 100.00 mm", "Material : C25/30", the stiffness factors (1 by
    /// default) and the buckling / modal analysis lines, as measured in the original.
    static QStringList shellInfoLines(const core::StructuralModel& structure, int shellId, const core::ProfileSettings& profiles);
    /// True while the coordinate tooltip is visible: whenever the cursor is inside the viewport (the original shows it
    /// without any command active too: tp1_after_esc.png), snapped to elements in snap mode, free otherwise.
    bool coordinateTooltipVisible() const { return m_cursorInside && m_coordTooltipEnabled && !m_coordTooltipDismissed; }
    /// Nudos > Borrar shows no coordinate tooltip next to the cursor (nudos.md section 9.3).
    void setCoordinateTooltipEnabled(bool on) { m_coordTooltipEnabled = on; update(); }
    /// Esc hides the tooltip (nudos.md section 9.4) until the pointer leaves and re-enters the window.
    void hideCoordinateTooltip() { m_coordTooltipDismissed = true; update(); }

    /// "187.596, -186.215, 153.756 m": three decimals, point as decimal separator, unit at the end.
    static QString coordinatesText(const core::Vec3& world);
    /// Rows of the bar information box: "Barra N2/N3: L. 4 m", "Capa : por defecto" and, when assigned, "IPE 80 (S275)".
    static QStringList barInfoLines(const core::StructuralModel& structure, int barId);
    /// Rectangle of a box of `size` inside an area of `area` size: flush against the top-right corner, or centred above
    /// `anchor` (screen position of the element) and kept inside the area.
    static QRectF anchoredRect(const QSizeF& size, const QSizeF& area, TooltipPosition position, const QPointF& anchor = {});
    /// Top-left corner of the coordinate tooltip: (+40, +43) px from the cursor, flipped to the other side of the
    /// cursor when it would leave the area.
    static QPointF coordinateTooltipOrigin(const QPointF& cursor, const QSizeF& size, const QSizeF& area);
    /// es-ES text of a position, as the options panel shows it.
    static QString tooltipPositionLabel(TooltipPosition position);

    bool isOrthographic() const { return m_camera.projection == core::Camera::Projection::Orthographic; }
    void setOrthographic(bool on);
    void setStandardView(core::StandardView view);

    // --- View bar and zoom toolbar (T-P4-09) -------------------------------------------------------------------------
    /// "Proyección" palette: applies the 0-based cell `index` (yaw and pitch only). False when it is unavailable.
    bool applyProjectionPreset(int index);
    /// "Ventana completa": centres on the bounding box of the nodes and fits it; without any node the default view
    /// (isometric, target at the origin) is restored. Remembered as the reference of "Ventana doble".
    void frameAll();
    /// "Ventana doble": the original's tooltip says "ampliar al doble el tamaño de la vista respecto a 'Ventana completa'".
    /// Measured (TK-031): the original shows the same view as "Ventana completa" and repeating it changes nothing.
    void doubleWindow();
    /// "Ventana anterior": restores the view before the last zoom / pan / orbit / preset / frame gesture.
    bool previousView();
    bool canGoPrevious() const { return !m_history.empty(); }
    int historySize() const { return static_cast<int>(m_history.size()); }
    /// Records the end of a view gesture: the view in force before it goes onto the (bounded) history.
    void commitView();
    static constexpr int kMaxViewHistory = 50;
    /// "Marcar zoom": while on, a left click zooms in x2 centred on the clicked point (the mode stays on until switched
    /// off; the original's exact behaviour is unverified).
    void setZoomMarkMode(bool on);
    bool zoomMarkMode() const { return m_zoomMark; }
    /// Rectangle shown (and zoomed to) for a cursor position: centred on it, with the aspect of `area`, half width = distance
    /// from the cursor to the right edge (measured in the original, TK-031), never narrower than kZoomMarkMinFraction.
    static QRectF zoomMarkRect(const QPointF& cursor, const QSizeF& area);
    static constexpr double kZoomMarkMinFraction = 0.05;
    /// "Rotación alrededor de un punto" / "... de la cámara": the state of the two buttons of the view bar. The orbit
    /// pivot is still the camera target (TK-032: pivot under the cursor, rotation about the camera axis).
    void setRotateAroundPoint(bool on);
    bool rotateAroundPoint() const { return m_rotateAroundPoint; }
    void setRotateAroundCamera(bool on);
    bool rotateAroundCamera() const { return m_rotateAroundCamera; }

    /// Sections X/Y/Z and clipping volume (TK-032). A section removes the geometry on the side of its red arrow.
    const core::ClipSettings& clipping() const { return m_clip; }
    void setSectionActive(int axis, bool on);
    bool sectionActive(int axis) const { return axis >= 0 && axis < 3 && m_clip.sections[static_cast<std::size_t>(axis)].active; }
    bool flipSection(int axis);
    void setClippingVolumeActive(bool on);
    bool clippingVolumeActive() const { return m_clip.volume.active; }
    /// Options of the 3D window (icon y=440): edges of the objects and screen-space ambient occlusion. SSAO has no effect on
    /// the software painter (ticket for the OpenGL renderer).
    /// "Mostrar / Ocultar planos de sección" (y=411): hides the planes, arrows and arcs but keeps the cut (measured, sp_*.png).
    void setClipPlanesVisible(bool on) { m_clipPlanesVisible = on; update(); }
    bool clipPlanesVisible() const { return m_clipPlanesVisible; }
    void setObjectEdges(bool on) { m_objectEdges = on; update(); }
    bool objectEdges() const { return m_objectEdges; }
    void setSsao(bool on) { m_ssao = on; update(); }
    bool ssao() const { return m_ssao; }
    /// An arrow of the section planes / clipping volume that the mouse can grab.
    struct ClipHandle {
        bool volume = false;   ///< false: section plane
        int axis = 0;
        bool faceMax = false;  ///< volume: the face with the larger coordinate
        core::Vec3 base;       ///< plane centre / face centre
        core::Vec3 tip;        ///< arrow head
    };
    std::vector<ClipHandle> clipHandles() const;
    /// Handle whose arrow is within `tolerancePx` of the widget position.
    std::optional<ClipHandle> clipHandleAt(const QPointF& pos, double tolerancePx = 8.0) const;
    /// Moves the arrow of `handle` so that it follows the cursor (as if dragged from `from` to `to`).
    void dragClipHandle(const ClipHandle& handle, const QPointF& from, const QPointF& to);
    /// Arcs: ids 0..2 are the section planes X, Y, Z (each turns only its own plane, within +-80 degrees, a step that would
    /// leave the range being refused) and kVolumeArc turns the clipping volume (global turn, unlimited). Law from the
    /// original code: delta = d * 2*pi / viewport size, dominant axis only.
    static constexpr int kVolumeArc = 3;
    /// Id of the arc within `tolerancePx` of `pos`, or -1.
    int clipArcIdAt(const QPointF& pos, double tolerancePx = 8.0) const;
    bool clipArcAt(const QPointF& pos, double tolerancePx = 8.0) const { return clipArcIdAt(pos, tolerancePx) >= 0; }
    /// Turns what arc `id` governs; false when refused (inactive, or the section would leave +-80 degrees).
    bool dragClipArc(int id, const QPointF& from, const QPointF& to);
    /// Same on the first existing arc (a section before the volume).
    void dragClipArc(const QPointF& from, const QPointF& to);
    std::vector<core::Vec3> clipArc(int id) const;
    /// First existing arc, empty when nothing is active.
    std::vector<core::Vec3> clipArc() const;
    /// Whether a world point survives every active section and the clipping volume.
    bool visibleAfterClipping(const core::Vec3& p) const;

    /// "Planos": the grid lines drawn, and the reference lines filtered by "Referencias" (not owned; must outlive the viewport
    /// or be reset to nullptr). "Visibilidad" and "Referencias" are read from the planes. Levels are not drawn in 3D (measured).
    void setWorkPlanes(const core::WorkPlanes* planes);
    /// False when "Visibilidad" hides the grid lines (their labels stay).
    bool workPlanesVisible() const { return !m_planes || m_planes->visible; }
    /// One grid line. Grid lines carry their label at one end (the magenta circle). `lineVisible` is false when "Visibilidad"
    /// hides the lines: the label and its stretch are still drawn. Empty when undefined or when "Mostrar la rejilla" is off.
    struct PlaneSegment {
        enum class Kind { GridX, GridY };
        enum class LabelAt { None, A, B };
        Kind kind = Kind::GridX;
        core::Vec3 a, b;
        QString label;
        LabelAt labelAt = LabelAt::None;
        bool lineVisible = true;
    };
    std::vector<PlaneSegment> workPlaneSegments() const;

    /// Screen position (widget pixels, y down) of a world point; false when it cannot be seen (behind the camera).
    bool worldToScreen(const core::Vec3& world, QPointF& screen) const;
    /// World point under a widget position on the plane through the camera target.
    core::Vec3 screenToTargetPlane(const QPointF& screen) const;

signals:
    void cameraChanged();
    void projectionChanged(bool orthographic);
    void historyChanged();
    void zoomMarkModeChanged(bool on);
    void rotateAroundPointChanged(bool on);
    void rotateAroundCameraChanged(bool on);
    void sectionChanged(int axis, bool active);
    void clippingVolumeChanged(bool active);
    /// Cursor position on the target plane in world coordinates; emitted on every mouse move.
    void cursorMoved(const cc::core::Vec3& world);
    void selectionChanged();
    void snapChanged();
    /// Left click in snap mode: the world position entered (snapped or free) and the snap that produced it.
    void pointEntered(const cc::core::Vec3& world, const cc::core::SnapResult& snap);
    /// Right button in selection mode: act on the selected elements.
    void actOnSelection();
    /// Right button in snap mode (Nudos > Mover cancels with it).
    void rightClicked();
    /// A digit typed while the snap mode is active: the original opens its "Calculadora" with it.
    void digitTyped(QChar digit);
    /// Esc pressed in selection or snap mode (after the viewport cancelled a pending window / cleared the selection).
    void escapePressed();
    /// Status bar text of the current mode (empty when leaving selection mode).
    void statusHint(const QString& text);

protected:
    void paintGL() override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void enterEvent(QEnterEvent* event) override;

private:
    void drawCoordinateTooltip(class QPainter& painter) const;
    void drawInfoBox(class QPainter& painter) const;
    /// Screen position of the element the information is about (snapped point, node, or middle of the bar).
    QPointF infoAnchor(const std::optional<int>& barId) const;

    double aspect() const;
    DragAction actionFor(Qt::MouseButtons buttons) const;
    void drawGuideLines(class QPainter& painter) const;
    void drawAxesTriad(class QPainter& painter) const;
    void drawWorkPlanes(class QPainter& painter) const;

    void drawGuideLine(class QPainter& painter, core::Vec3 a, core::Vec3 b) const;
    void drawStructure(class QPainter& painter) const;
    void drawNodes(class QPainter& painter) const;
    void drawMoveRubber(class QPainter& painter) const;  ///< Nudos > Mover: black bars to the cursor and the orange dimension
    void drawBarRubber(class QPainter& painter) const;  ///< Barras > Barra / Viga: chosen nodes and the elastic line
    void drawPointRubber(class QPainter& painter) const;  ///< Laminas / Herramientas: chosen points and the elastic line
    void drawShellAxes(class QPainter& painter) const;    ///< Laminas > Ejes locales
    void drawHighlight(class QPainter& painter, const core::ElementRef& e, bool selected) const;
    void drawRubberBand(class QPainter& painter) const;
    void drawZoomMark(class QPainter& painter) const;
    void drawNodeLabel(class QPainter& painter) const;
    void drawNodeTags(class QPainter& painter) const;
    void drawBarTags(class QPainter& painter) const;
    void rebuildSnapModel();
    void updateFocusPolicy();
    bool elementExists(const core::ElementRef& e) const;
    void drawSnap(class QPainter& painter) const;
    void updateSnap(const QPointF& pos);
    core::ScreenMapper mapper() const;
    void selectionPress(class QMouseEvent* event);
    void updateHover(const QPointF& pos);

    std::vector<core::Vec3> m_nodes;
    core::Camera m_camera;
    MouseBindings m_bindings;
    QPoint m_last;
    DragAction m_drag = DragAction::None;

    const core::StructuralModel* m_structure = nullptr;
    const core::WorkPlanes* m_planes = nullptr;
    bool m_selectionMode = false;
    core::SelectionFilter m_filter;
    core::CategorySettings m_categories;
    core::Selection m_selection;
    std::optional<core::ElementRef> m_hover;
    const core::ProfileSettings* m_profiles = nullptr;
    std::optional<QPointF> m_windowStart;
    QPointF m_cursor;
    bool m_snapMode = false;
    core::SnapOptions m_snapOptions;
    core::SnapResult m_snap;
    TooltipPosition m_tooltipPosition = TooltipPosition::TopRightCorner;
    bool m_showElementInfo = true;
    bool m_fillShells = false;
    double m_nodeSizeCm = 15.0;
    bool m_cursorInside = false;
    bool m_coordTooltipEnabled = true;
    bool m_coordTooltipDismissed = false;
    bool m_nodeLabels = false;
    std::vector<std::pair<int, QString>> m_barLabels;
    std::optional<int> m_movingNode;
    std::vector<int> m_rubberChain;
    std::vector<core::Vec3> m_rubberPoints;
    bool m_showShellAxes = false;
    std::shared_ptr<core::StructuralModel> m_snapModel;  ///< structure without the moving node and its bars (null when none moves)

    void notifyCameraChanged();
    void fitAll();  ///< the camera part of frameAll(), without history
    std::optional<core::CameraView> m_committed;  ///< view at the end of the previous gesture
    std::vector<core::CameraView> m_history;
    bool m_zoomMark = false;
    bool m_rotateAroundPoint = false;
    bool m_rotateAroundCamera = false;
    core::ClipSettings m_clip;
    bool m_clipPlanesVisible = true;
    bool m_objectEdges = false;
    bool m_ssao = false;
    std::optional<ClipHandle> m_clipDrag;
    bool m_clipArcDrag = false;
    int m_clipArcId = -1;  ///< arc being dragged
    QPointF m_clipDragLast;
    void drawClipping(class QPainter& painter) const;
    std::pair<core::Vec3, core::Vec3> clipEnvelope() const;
    core::Vec3 m_orbitPivot;  ///< fixed at the start of an orbit drag when rotating about a point
    /// Object point under the cursor (node, point of a bar), else the point of the target plane seen there.
    core::Vec3 orbitPivotAt(const QPointF& pos) const;
};

}  // namespace cc::app

Q_DECLARE_METATYPE(cc::core::Vec3)
Q_DECLARE_METATYPE(cc::core::SnapResult)
