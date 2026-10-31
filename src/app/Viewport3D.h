#pragma once
#include <QOpenGLWidget>
#include <QPoint>
#include <QPointF>
#include <optional>
#include <vector>

#include "core/camera.h"
#include "core/picking.h"
#include "core/selection.h"
#include "core/structure_model.h"

namespace cc::app {

/// What a mouse button drag does in the 3D viewport.
enum class DragAction { None, Orbit, Pan };

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
    const core::SelectionFilter& selectionFilter() const { return m_filter; }
    const core::Selection& selection() const { return m_selection; }
    void clearSelection();
    /// Element under the cursor in selection mode.
    const std::optional<core::ElementRef>& hovered() const { return m_hover; }
    /// True between the first and the second click of a window.
    bool windowPending() const { return m_windowStart.has_value(); }
    void cancelWindow();

    bool isOrthographic() const { return m_camera.projection == core::Camera::Projection::Orthographic; }
    void setOrthographic(bool on);
    void setStandardView(core::StandardView view);

    /// Screen position (widget pixels, y down) of a world point; false when it cannot be seen (behind the camera).
    bool worldToScreen(const core::Vec3& world, QPointF& screen) const;
    /// World point under a widget position on the plane through the camera target.
    core::Vec3 screenToTargetPlane(const QPointF& screen) const;

signals:
    void cameraChanged();
    void projectionChanged(bool orthographic);
    /// Cursor position on the target plane in world coordinates; emitted on every mouse move.
    void cursorMoved(const cc::core::Vec3& world);
    void selectionChanged();
    /// Right button in selection mode: act on the selected elements.
    void actOnSelection();
    /// Status bar text of the current mode (empty when leaving selection mode).
    void statusHint(const QString& text);

protected:
    void paintGL() override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    double aspect() const;
    DragAction actionFor(Qt::MouseButtons buttons) const;
    void drawGuideLines(class QPainter& painter) const;
    void drawAxesTriad(class QPainter& painter) const;

    void drawGuideLine(class QPainter& painter, core::Vec3 a, core::Vec3 b) const;
    void drawStructure(class QPainter& painter) const;
    void drawNodes(class QPainter& painter) const;
    void drawHighlight(class QPainter& painter, const core::ElementRef& e, bool selected) const;
    void drawRubberBand(class QPainter& painter) const;
    void drawNodeLabel(class QPainter& painter) const;
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
    core::Selection m_selection;
    std::optional<core::ElementRef> m_hover;
    std::optional<QPointF> m_windowStart;
    QPointF m_cursor;
};

}  // namespace cc::app
