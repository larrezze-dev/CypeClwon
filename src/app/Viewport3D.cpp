#include "Viewport3D.h"

#include <QFont>
#include <QMouseEvent>
#include <QPainter>
#include <QWheelEvent>
#include <algorithm>
#include <cmath>

namespace cc::app {

using core::Camera;
using core::Vec3;

namespace {
constexpr double kGuideExtent = 1e5;  // the axis guide lines are effectively infinite

// Eye-space position of a world point (the camera looks along -z).
Vec3 toEye(const core::Mat4& view, const Vec3& p) {
    return {view.at(0, 0) * p.x + view.at(0, 1) * p.y + view.at(0, 2) * p.z + view.at(0, 3),
            view.at(1, 0) * p.x + view.at(1, 1) * p.y + view.at(1, 2) * p.z + view.at(1, 3),
            view.at(2, 0) * p.x + view.at(2, 1) * p.y + view.at(2, 2) * p.z + view.at(2, 3)};
}
}  // namespace

Viewport3D::Viewport3D(QWidget* parent) : QOpenGLWidget(parent) {
    setMouseTracking(true);
    setMinimumSize(100, 100);
}

double Viewport3D::aspect() const { return height() > 0 ? static_cast<double>(width()) / height() : 1.0; }

void Viewport3D::setOrthographic(bool on) {
    if (isOrthographic() == on) return;
    m_camera.setProjection(on ? Camera::Projection::Orthographic : Camera::Projection::Perspective);
    emit projectionChanged(on);
    emit cameraChanged();
    update();
}

void Viewport3D::setStandardView(core::StandardView view) {
    m_camera.setStandardView(view);
    emit cameraChanged();
    update();
}

bool Viewport3D::worldToScreen(const Vec3& world, QPointF& screen) const {
    Vec3 ndc;
    if (!m_camera.project(world, aspect(), ndc)) return false;
    screen = QPointF((ndc.x + 1.0) * 0.5 * width(), (1.0 - ndc.y) * 0.5 * height());
    return true;
}

Vec3 Viewport3D::screenToTargetPlane(const QPointF& screen) const {
    const double ndcX = width() > 0 ? 2.0 * screen.x() / width() - 1.0 : 0.0;
    const double ndcY = height() > 0 ? 1.0 - 2.0 * screen.y() / height() : 0.0;
    return m_camera.pointOnTargetPlane(ndcX, ndcY, aspect());
}

DragAction Viewport3D::actionFor(Qt::MouseButtons buttons) const {
    if (buttons.testFlag(Qt::LeftButton)) return m_bindings.left;
    if (buttons.testFlag(Qt::MiddleButton)) return m_bindings.middle;
    if (buttons.testFlag(Qt::RightButton)) return m_bindings.right;
    return DragAction::None;
}

void Viewport3D::mousePressEvent(QMouseEvent* event) {
    m_last = event->position().toPoint();
    m_drag = actionFor(event->buttons());
    event->accept();
}

void Viewport3D::mouseMoveEvent(QMouseEvent* event) {
    const QPoint pos = event->position().toPoint();
    const QPoint delta = pos - m_last;
    m_last = pos;
    if (event->buttons() == Qt::NoButton) m_drag = DragAction::None;
    if (m_drag == DragAction::Orbit) {
        m_camera.orbit(delta.x() * m_bindings.orbitDegreesPerPixel, delta.y() * m_bindings.orbitDegreesPerPixel);
        emit cameraChanged();
        update();
    } else if (m_drag == DragAction::Pan) {
        m_camera.pan(delta.x(), delta.y(), height());
        emit cameraChanged();
        update();
    }
    emit cursorMoved(screenToTargetPlane(event->position()));
    event->accept();
}

void Viewport3D::mouseReleaseEvent(QMouseEvent* event) {
    m_drag = actionFor(event->buttons());
    event->accept();
}

void Viewport3D::wheelEvent(QWheelEvent* event) {
    const double notches = event->angleDelta().y() / 120.0;
    if (notches != 0.0 && width() > 0 && height() > 0) {
        const QPointF p = event->position();
        m_camera.zoomAt(notches, 2.0 * p.x() / width() - 1.0, 1.0 - 2.0 * p.y() / height(), aspect());
        emit cameraChanged();
        update();
    }
    event->accept();
}

void Viewport3D::drawGuideLines(QPainter& painter) const {
    const core::Mat4 view = m_camera.viewMatrix();
    const double nearZ = -m_camera.nearPlane();
    painter.setPen(QPen(QColor(QStringLiteral("#D98A4A")), 1));
    const Vec3 axes[3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
    for (const Vec3& axis : axes) {
        Vec3 a = axis * -kGuideExtent, b = axis * kGuideExtent;
        if (!isOrthographic()) {  // clip against the near plane before projecting
            const double za = toEye(view, a).z, zb = toEye(view, b).z;
            if (za > nearZ && zb > nearZ) continue;
            if (za > nearZ) a = a + (b - a) * ((nearZ - za) / (zb - za));
            if (zb > nearZ) b = b + (a - b) * ((nearZ - zb) / (za - zb));
        }
        QPointF pa, pb;
        if (worldToScreen(a, pa) && worldToScreen(b, pb)) painter.drawLine(pa, pb);
    }
}

void Viewport3D::drawAxesTriad(QPainter& painter) const {
    // Fixed-size triad in the bottom-left corner showing the orientation of the world axes.
    const QPointF origin(24.0, height() - 28.0);
    const double len = 38.0;
    struct AxisDef { Vec3 dir; QColor colour; const char* label; };
    const AxisDef defs[3] = {{{1, 0, 0}, QColor(QStringLiteral("#E00000")), "X"},
                             {{0, 1, 0}, QColor(QStringLiteral("#00B000")), "Y"},
                             {{0, 0, 1}, QColor(QStringLiteral("#0000E0")), "Z"}};
    const Vec3 right = m_camera.right(), up = m_camera.up();
    QFont font = painter.font();
    font.setBold(true);
    font.setPixelSize(12);
    painter.setFont(font);
    for (const AxisDef& d : defs) {
        const QPointF tip(origin.x() + len * core::dot(d.dir, right), origin.y() - len * core::dot(d.dir, up));
        painter.setPen(QPen(d.colour, 2));
        painter.drawLine(origin, tip);
        const QPointF dir = tip - origin;
        const double l = std::hypot(dir.x(), dir.y());
        if (l > 1e-6) {  // small arrow head
            const QPointF u = dir / l, n(-u.y(), u.x());
            painter.setBrush(d.colour);
            const QPointF head[3] = {tip, tip - u * 7.0 + n * 3.0, tip - u * 7.0 - n * 3.0};
            painter.drawPolygon(head, 3);
            painter.setBrush(Qt::NoBrush);
        }
        const QPointF labelPos = tip + (l > 1e-6 ? dir / l * 9.0 : QPointF(0, 0));
        painter.drawText(QRectF(labelPos.x() - 8, labelPos.y() - 8, 16, 16), Qt::AlignCenter, QString::fromLatin1(d.label));
    }
}

void Viewport3D::paintGL() {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.fillRect(rect(), Qt::white);
    drawGuideLines(painter);
    drawAxesTriad(painter);
}

}  // namespace cc::app
