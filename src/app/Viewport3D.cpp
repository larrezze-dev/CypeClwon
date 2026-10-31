#include "Viewport3D.h"

#include <QFont>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPolygonF>
#include <QWheelEvent>
#include <algorithm>
#include <cmath>
#include <map>
#include <utility>

namespace cc::app {

using core::Camera;
using core::Vec3;

namespace {
constexpr double kGuideExtent = 1e5;  // the long guide lines are effectively infinite
constexpr double kStubPixels = 12.0;  // half-length of the short guide stubs on screen
constexpr double kNodeHalf = 7.0;     // blue node squares are ~14 px wide, constant with the zoom
const QColor kNodeBlue(QStringLiteral("#0066CC"));
const QColor kHighlight(QStringLiteral("#DA8735"));

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
    if (m_selectionMode && (event->button() == Qt::LeftButton || event->button() == Qt::RightButton)) selectionPress(event);
    if (m_snapMode && event->button() == Qt::LeftButton) {
        updateSnap(event->position());
        emit pointEntered(cursorWorld(), m_snap);
    }
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
    m_cursor = event->position();
    if (m_selectionMode) updateHover(m_cursor);
    if (m_snapMode) updateSnap(m_cursor);
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

void Viewport3D::setNodes(std::vector<Vec3> nodes) {
    m_nodes = std::move(nodes);
    update();
}

void Viewport3D::setStructure(const core::StructuralModel* structure) {
    m_structure = structure;
    m_nodes.clear();
    if (structure)
        for (const core::Node& n : structure->nodes) m_nodes.push_back({n.position.x, n.position.y, n.position.z});
    m_hover.reset();
    m_windowStart.reset();
    clearSelection();
    update();
}

void Viewport3D::setSnapMode(bool on) {
    if (m_snapMode == on) return;
    m_snapMode = on;
    m_snap = {};
    emit snapChanged();
    update();
}

void Viewport3D::updateSnap(const QPointF& pos) {
    core::SnapResult snap;
    if (m_structure) snap = core::snapCursor(*m_structure, mapper(), pos.x(), pos.y(), m_snapOptions);
    m_cursor = pos;
    const bool changed = snap.kind != m_snap.kind || !(snap.point == m_snap.point);
    m_snap = snap;
    if (changed) emit snapChanged();
    update();
}

Vec3 Viewport3D::cursorWorld() const { return m_snap.snapped() ? m_snap.point : screenToTargetPlane(m_cursor); }

core::ScreenMapper Viewport3D::mapper() const { return {&m_camera, static_cast<double>(width()), static_cast<double>(height())}; }

void Viewport3D::setSelectionMode(bool on) {
    if (m_selectionMode == on) return;
    m_selectionMode = on;
    m_hover.reset();
    m_windowStart.reset();
    if (!on) clearSelection();
    setFocusPolicy(on ? Qt::ClickFocus : Qt::NoFocus);
    emit statusHint(on ? tr("LMB: Seleccione un grupo de elementos. RMB: Actuar sobre los elementos seleccionados") : QString());
    update();
}

void Viewport3D::setSelectionFilter(const core::SelectionFilter& filter) {
    m_filter = filter;
    if (m_selection.restrictTo(filter)) emit selectionChanged();
    m_hover.reset();
    update();
}

void Viewport3D::clearSelection() {
    if (m_selection.empty()) return;
    m_selection.clear();
    emit selectionChanged();
    update();
}

void Viewport3D::cancelWindow() {
    if (!m_windowStart) return;
    m_windowStart.reset();
    update();
}

void Viewport3D::updateHover(const QPointF& pos) {
    std::optional<core::ElementRef> hover;
    if (m_structure && !m_windowStart) hover = core::pickAt(*m_structure, mapper(), m_filter, pos.x(), pos.y());
    if (hover != m_hover || m_windowStart) {
        m_hover = hover;
        update();
    }
}

void Viewport3D::selectionPress(QMouseEvent* event) {
    if (event->button() == Qt::RightButton) {
        if (m_windowStart) cancelWindow();
        else emit actOnSelection();
        return;
    }
    if (!m_structure) return;
    const QPointF pos = event->position();
    const core::SelectMode mode = event->modifiers().testFlag(Qt::ControlModifier) ? core::SelectMode::Add
                                  : event->modifiers().testFlag(Qt::ShiftModifier) ? core::SelectMode::Remove
                                                                                    : core::SelectMode::Replace;
    bool changed = false;
    if (m_windowStart) {  // second corner
        const QPointF a = *m_windowStart;
        m_windowStart.reset();
        changed = m_selection.apply(core::pickRect(*m_structure, mapper(), m_filter, a.x(), a.y(), pos.x(), pos.y()), mode);
    } else if (const auto hit = core::pickAt(*m_structure, mapper(), m_filter, pos.x(), pos.y())) {
        changed = m_selection.apply({*hit}, mode);
    } else {
        m_windowStart = pos;  // first corner; a plain click on empty space does not clear the selection
        m_cursor = pos;
    }
    m_hover.reset();
    if (changed) emit selectionChanged();
    update();
}

void Viewport3D::keyPressEvent(QKeyEvent* event) {
    if (m_selectionMode && event->key() == Qt::Key_Escape) {
        if (m_windowStart) cancelWindow();
        else clearSelection();
        event->accept();
        return;
    }
    QOpenGLWidget::keyPressEvent(event);
}

void Viewport3D::leaveEvent(QEvent* event) {
    if (m_hover) {
        m_hover.reset();
        update();
    }
    QOpenGLWidget::leaveEvent(event);
}

void Viewport3D::drawStructure(QPainter& painter) const {
    if (!m_structure) return;
    const core::ScreenMapper map = mapper();
    for (const core::Shell& shell : m_structure->shells) {
        QPolygonF poly;
        for (int id : shell.nodeIds) {
            const core::Node* n = m_structure->findNode(id);
            double x, y;
            if (n && map.toScreen(n->position, x, y)) poly << QPointF(x, y);
        }
        if (poly.size() < 3 || poly.size() != static_cast<int>(shell.nodeIds.size())) continue;
        painter.setPen(QPen(QColor(60, 60, 60), 1));
        painter.setBrush(QColor(120, 160, 200, 60));
        painter.drawPolygon(poly);
    }
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(QColor(32, 32, 32), 1.5));
    for (const core::Bar& bar : m_structure->bars) {
        const core::Node* a = m_structure->findNode(bar.startNode);
        const core::Node* b = m_structure->findNode(bar.endNode);
        double ax, ay, bx, by;
        if (a && b && map.toScreen(a->position, ax, ay) && map.toScreen(b->position, bx, by))
            painter.drawLine(QPointF(ax, ay), QPointF(bx, by));
    }
}

void Viewport3D::drawNodes(QPainter& painter) const {
    if (!m_structure) return;
    const core::ScreenMapper map = mapper();
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(kNodeBlue, 1));
    for (const core::Node& n : m_structure->nodes) {
        double x, y;
        if (map.toScreen(n.position, x, y)) painter.drawRect(QRectF(x - kNodeHalf, y - kNodeHalf, 2 * kNodeHalf, 2 * kNodeHalf));
    }
}

void Viewport3D::drawHighlight(QPainter& painter, const core::ElementRef& e, bool selected) const {
    if (!m_structure) return;
    const core::ScreenMapper map = mapper();
    const double w = selected ? 3.0 : 2.0;  // original: orange squares 2-3 px thick
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(kHighlight, w));
    double x, y;
    switch (e.kind) {
        case core::ElementKind::Node: {
            const core::Node* n = m_structure->findNode(e.id);
            if (n && map.toScreen(n->position, x, y)) painter.drawRect(QRectF(x - kNodeHalf, y - kNodeHalf, 2 * kNodeHalf, 2 * kNodeHalf));
            break;
        }
        case core::ElementKind::Bar: {
            const core::Bar* b = m_structure->findBar(e.id);
            const core::Node* na = b ? m_structure->findNode(b->startNode) : nullptr;
            const core::Node* nb = b ? m_structure->findNode(b->endNode) : nullptr;
            double bx, by;
            if (na && nb && map.toScreen(na->position, x, y) && map.toScreen(nb->position, bx, by))
                painter.drawLine(QPointF(x, y), QPointF(bx, by));
            break;
        }
        case core::ElementKind::Shell: {
            const core::Shell* s = m_structure->findShell(e.id);
            if (!s) break;
            QPolygonF poly;
            for (int id : s->nodeIds) {
                const core::Node* n = m_structure->findNode(id);
                if (n && map.toScreen(n->position, x, y)) poly << QPointF(x, y);
            }
            if (poly.size() == static_cast<int>(s->nodeIds.size())) painter.drawPolygon(poly);
            break;
        }
    }
}

void Viewport3D::drawRubberBand(QPainter& painter) const {
    if (!m_windowStart) return;
    const QRectF r = QRectF(*m_windowStart, m_cursor).normalized();
    const bool window = core::rectModeFor(m_windowStart->x(), m_cursor.x()) == core::RectMode::Window;
    QPen pen(QColor(40, 40, 40), 1);
    pen.setStyle(window ? Qt::SolidLine : Qt::DashLine);  // original: continuous = window, dotted = crossing
    painter.setPen(pen);
    painter.setBrush(window ? QColor(0, 102, 204, 25) : QColor(60, 160, 60, 25));
    painter.drawRect(r);
    painter.setBrush(Qt::NoBrush);
}

void Viewport3D::drawSnap(QPainter& painter) const {
    if (!m_snap.snapped() || !m_structure) return;
    const core::ScreenMapper map = mapper();
    const QColor blue(QStringLiteral("#1E90C8"));
    const auto toPoint = [&](const Vec3& w, QPointF& out) { double x, y; if (!map.toScreen(w, x, y)) return false; out = QPointF(x, y); return true; };
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(blue, 2));
    const auto drawWorldLine = [&](const Vec3& a, const Vec3& b) { QPointF pa, pb; if (toPoint(a, pa) && toPoint(b, pb)) painter.drawLine(pa, pb); };
    const auto nodePos = [&](int id) { const core::Node* n = m_structure->findNode(id); return n ? Vec3{n->position.x, n->position.y, n->position.z} : Vec3{}; };
    switch (m_snap.kind) {
        case core::SnapKind::Endpoint:  // the bars that meet at the node
            for (const core::Bar& b : m_structure->bars)
                if (m_snap.element && (b.startNode == m_snap.element->id || b.endNode == m_snap.element->id)) drawWorldLine(nodePos(b.startNode), nodePos(b.endNode));
            break;
        case core::SnapKind::Nearest:
        case core::SnapKind::Midpoint:
            if (const core::Bar* b = m_snap.element ? m_structure->findBar(m_snap.element->id) : nullptr) drawWorldLine(nodePos(b->startNode), nodePos(b->endNode));
            break;
        case core::SnapKind::Extension:
            if (m_snap.element && m_snap.axis >= 0) {
                Vec3 axis{0, 0, 0};
                (m_snap.axis == 0 ? axis.x : m_snap.axis == 1 ? axis.y : axis.z) = 1.0;
                const Vec3 o = nodePos(m_snap.element->id);
                drawWorldLine(o - axis * kGuideExtent, o + axis * kGuideExtent);
            }
            break;
        case core::SnapKind::None: break;
    }
    QPointF p;
    if (!toPoint(m_snap.point, p)) return;
    // Dimension label: distance from the reference node, light blue, turned along the measured segment.
    if (m_snap.referenceNode && m_snap.referenceDistance > 1e-9) {
        QPointF r;
        if (toPoint(nodePos(*m_snap.referenceNode), r)) {
            const QPointF mid = (p + r) / 2.0;
            double angle = std::atan2(p.y() - r.y(), p.x() - r.x()) * 180.0 / 3.14159265358979323846;
            if (angle > 90.0) angle -= 180.0;
            if (angle < -90.0) angle += 180.0;
            const QString text = QString::number(m_snap.referenceDistance, 'f', 4) + QStringLiteral(" m");
            QFont font = painter.font();
            font.setPixelSize(12);
            painter.setFont(font);
            painter.save();
            painter.translate(mid);
            painter.rotate(angle);
            const QRectF box(-painter.fontMetrics().horizontalAdvance(text) / 2.0 - 4, -22, painter.fontMetrics().horizontalAdvance(text) + 8, 18);
            painter.setPen(Qt::NoPen);
            painter.setBrush(QColor(168, 216, 234));
            painter.drawRect(box);
            painter.setPen(QColor(30, 30, 30));
            painter.drawText(box, Qt::AlignCenter, text);
            painter.restore();
        }
    }
    // Hourglass marker (two triangles) at the snapped point.
    const QColor ochre(QStringLiteral("#8B6B1F"));
    painter.setPen(QPen(ochre, 2));
    painter.setBrush(Qt::NoBrush);
    const QPointF top[3] = {p + QPointF(-6, -8), p + QPointF(6, -8), p};
    const QPointF bottom[3] = {p + QPointF(-6, 8), p + QPointF(6, 8), p};
    painter.drawPolygon(top, 3);
    painter.drawPolygon(bottom, 3);
}

void Viewport3D::drawNodeLabel(QPainter& painter) const {
    if (!m_hover || m_hover->kind != core::ElementKind::Node || !m_structure) return;
    const core::Node* n = m_structure->findNode(m_hover->id);
    if (!n) return;
    const auto fmt = [](double v) { return QString::number(v, 'g', 6).replace('.', ','); };
    const QString text =
        QStringLiteral("N%1 (%2, %3, %4)").arg(n->id).arg(fmt(n->position.x), fmt(n->position.y), fmt(n->position.z));
    QFont font = painter.font();
    font.setPixelSize(13);
    painter.setFont(font);
    const int textWidth = painter.fontMetrics().horizontalAdvance(text);
    painter.setPen(kNodeBlue);
    painter.drawText(QPointF(width() - textWidth - 8, 20), text);  // top-right corner of the 3D area
}


void Viewport3D::drawGuideLine(QPainter& painter, Vec3 a, Vec3 b) const {
    if (!isOrthographic()) {  // clip against the near plane before projecting
        const core::Mat4 view = m_camera.viewMatrix();
        const double nearZ = -m_camera.nearPlane();
        const double za = toEye(view, a).z, zb = toEye(view, b).z;
        if (za > nearZ && zb > nearZ) return;
        if (za > nearZ) a = a + (b - a) * ((nearZ - za) / (zb - za));
        if (zb > nearZ) b = b + (a - b) * ((nearZ - zb) / (za - zb));
    }
    QPointF pa, pb;
    if (worldToScreen(a, pa) && worldToScreen(b, pb)) painter.drawLine(pa, pb);
}

void Viewport3D::drawGuideLines(QPainter& painter) const {
    // Original (vp_s5.png): thin dotted orange lines through every node: a full-length one along Y, plus X and Z lines
    // beyond the nodes along X and Z (nodes aligned along those axes are joined). With no nodes, the three axes through the origin are drawn.
    QPen pen(QColor(QStringLiteral("#D98A4A")), 1);
    pen.setStyle(Qt::DotLine);
    painter.setPen(pen);
    if (m_nodes.empty()) {
        const Vec3 axes[3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
        for (const Vec3& axis : axes) drawGuideLine(painter, axis * -kGuideExtent, axis * kGuideExtent);
        return;
    }
    const double pixelsToWorld = m_camera.worldPerPixel(height());
    const core::Mat4 view = m_camera.viewMatrix();
    // Constant on-screen length: in perspective, scale by the depth of the point relative to the target distance.
    const auto stubAt = [&](const Vec3& p) {
        const double depthScale = isOrthographic() ? 1.0 : std::max(1e-6, -toEye(view, p).z / m_camera.distance);
        return kStubPixels * pixelsToWorld * depthScale;
    };
    // Y: a full-length line through every node.
    for (const Vec3& n : m_nodes) drawGuideLine(painter, n + Vec3{0, -kGuideExtent, 0}, n + Vec3{0, kGuideExtent, 0});
    // X and Z: nodes that share the other two coordinates are joined by one segment, extended a few pixels at both ends.
    const auto key = [](double a, double b) { return std::make_pair(std::llround(a * 1e6), std::llround(b * 1e6)); };
    std::map<std::pair<long long, long long>, std::pair<Vec3, Vec3>> alongX, alongZ;  // line -> (lowest, highest) node
    for (const Vec3& n : m_nodes) {
        auto [ix, newX] = alongX.try_emplace(key(n.y, n.z), n, n);
        if (!newX) { if (n.x < ix->second.first.x) ix->second.first = n; if (n.x > ix->second.second.x) ix->second.second = n; }
        auto [iz, newZ] = alongZ.try_emplace(key(n.x, n.y), n, n);
        if (!newZ) { if (n.z < iz->second.first.z) iz->second.first = n; if (n.z > iz->second.second.z) iz->second.second = n; }
    }
    for (const auto& [k, ends] : alongX)
        drawGuideLine(painter, ends.first - Vec3{stubAt(ends.first), 0, 0}, ends.second + Vec3{stubAt(ends.second), 0, 0});
    for (const auto& [k, ends] : alongZ)
        drawGuideLine(painter, ends.first - Vec3{0, 0, stubAt(ends.first)}, ends.second + Vec3{0, 0, stubAt(ends.second)});
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
    if (!m_selectionMode) drawGuideLines(painter);  // the original hides the guide lines inside a command
    drawStructure(painter);
    if (m_selectionMode) {
        drawNodes(painter);
        for (const core::ElementRef& e : m_selection.items()) drawHighlight(painter, e, true);
        if (m_hover) drawHighlight(painter, *m_hover, false);
        drawRubberBand(painter);
        drawNodeLabel(painter);
    }
    if (m_snapMode) drawSnap(painter);
    drawAxesTriad(painter);
}

}  // namespace cc::app
