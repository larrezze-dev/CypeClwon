#pragma once
#include <QOpenGLWidget>
#include <QPoint>

#include "core/camera.h"

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

protected:
    void paintGL() override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private:
    double aspect() const;
    DragAction actionFor(Qt::MouseButtons buttons) const;
    void drawGuideLines(class QPainter& painter) const;
    void drawAxesTriad(class QPainter& painter) const;

    core::Camera m_camera;
    MouseBindings m_bindings;
    QPoint m_last;
    DragAction m_drag = DragAction::None;
};

}  // namespace cc::app
