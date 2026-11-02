#pragma once
#include <QEnterEvent>
#include <QOpenGLWidget>
#include <QPoint>
#include <QMetaType>
#include <QPointF>
#include <QRectF>
#include <QSizeF>
#include <QString>
#include <QStringList>
#include <optional>
#include <vector>

#include "core/camera.h"
#include "core/categories.h"
#include "core/picking.h"
#include "core/selection.h"
#include "core/snap.h"
#include "core/structure_model.h"

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
    /// True while the coordinate tooltip is visible: whenever the cursor is inside the viewport (the original shows it
    /// without any command active too: tp1_after_esc.png), snapped to elements in snap mode, free otherwise.
    bool coordinateTooltipVisible() const { return m_cursorInside; }

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
    /// Cursor position on the target plane in world coordinates; emitted on every mouse move.
    void cursorMoved(const cc::core::Vec3& world);
    void selectionChanged();
    void snapChanged();
    /// Left click in snap mode: the world position entered (snapped or free) and the snap that produced it.
    void pointEntered(const cc::core::Vec3& world, const cc::core::SnapResult& snap);
    /// Right button in selection mode: act on the selected elements.
    void actOnSelection();
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

    void drawGuideLine(class QPainter& painter, core::Vec3 a, core::Vec3 b) const;
    void drawStructure(class QPainter& painter) const;
    void drawNodes(class QPainter& painter) const;
    void drawHighlight(class QPainter& painter, const core::ElementRef& e, bool selected) const;
    void drawRubberBand(class QPainter& painter) const;
    void drawZoomMark(class QPainter& painter) const;
    void drawNodeLabel(class QPainter& painter) const;
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
    bool m_selectionMode = false;
    core::SelectionFilter m_filter;
    core::CategorySettings m_categories;
    core::Selection m_selection;
    std::optional<core::ElementRef> m_hover;
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

    void notifyCameraChanged();
    void fitAll();  ///< the camera part of frameAll(), without history
    std::optional<core::CameraView> m_committed;  ///< view at the end of the previous gesture
    std::vector<core::CameraView> m_history;
    bool m_zoomMark = false;
    bool m_rotateAroundPoint = false;
    bool m_rotateAroundCamera = false;
    core::Vec3 m_orbitPivot;  ///< fixed at the start of an orbit drag when rotating about a point
    /// Object point under the cursor (node, point of a bar), else the point of the target plane seen there.
    core::Vec3 orbitPivotAt(const QPointF& pos) const;
};

}  // namespace cc::app

Q_DECLARE_METATYPE(cc::core::Vec3)
Q_DECLARE_METATYPE(cc::core::SnapResult)
