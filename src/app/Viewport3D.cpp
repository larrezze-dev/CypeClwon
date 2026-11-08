#include "Viewport3D.h"
#include "SupportPictograms.h"

#include <QFont>
#include <QFontMetrics>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPolygonF>
#include <QRegularExpression>
#include <QWheelEvent>
#include <algorithm>
#include <cmath>
#include <map>
#include <numbers>
#include <utility>

#include "core/geom3.h"
#include "core/shell_edit.h"
#include "core/shell_properties.h"

namespace cc::app {

using core::Camera;
using core::Vec3;

namespace {
constexpr double kGuideExtent = 1e5;  // the long guide lines are effectively infinite
constexpr double kStubPixels = 12.0;  // half-length of the short guide stubs on screen
const QColor kNodeBlue(QStringLiteral("#0066CC"));
const QColor kHighlight(QStringLiteral("#DA8735"));
const QColor kLoadYellow(QStringLiteral("#FFFF00"));      // measured: pure yellow lines and values of the nodal loads
const QColor kPrescribedGreen(QStringLiteral("#008000"));  // measured (0,128,0): support symbol of a node with prescribed displacements

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
    m_camera.setProjection(Camera::Projection::Orthographic);  // the original opens in axonometric (rq_t1/t2.png)
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
    if (!m_committed) m_committed = m_camera.view();
    m_camera.setStandardView(view);
    commitView();
    notifyCameraChanged();
}

void Viewport3D::notifyCameraChanged() {
    emit cameraChanged();
    update();
}

void Viewport3D::commitView() {
    const core::CameraView now = m_camera.view();
    if (!m_committed) m_committed = now;
    if (*m_committed == now) return;
    m_history.push_back(*m_committed);
    if (static_cast<int>(m_history.size()) > kMaxViewHistory) m_history.erase(m_history.begin());
    m_committed = now;
    emit historyChanged();
}

bool Viewport3D::previousView() {
    if (m_history.empty()) return false;
    m_camera.setView(m_history.back());
    m_committed = m_history.back();
    m_history.pop_back();
    emit historyChanged();
    notifyCameraChanged();
    return true;
}

bool Viewport3D::applyProjectionPreset(int index) {
    if (!m_committed) m_committed = m_camera.view();
    if (!m_camera.setPreset(index)) return false;
    commitView();
    notifyCameraChanged();
    return true;
}

void Viewport3D::fitAll() {
    if (m_nodes.empty()) {
        const core::Camera defaults;
        m_camera.setView(defaults.view());
        return;
    }
    Vec3 lo = m_nodes.front(), hi = m_nodes.front();
    for (const Vec3& n : m_nodes) {
        lo = {std::min(lo.x, n.x), std::min(lo.y, n.y), std::min(lo.z, n.z)};
        hi = {std::max(hi.x, n.x), std::max(hi.y, n.y), std::max(hi.z, n.z)};
    }
    m_camera.frameBox(lo, hi, aspect());
}

void Viewport3D::frameAll() {
    if (!m_committed) m_committed = m_camera.view();
    fitAll();
    commitView();
    notifyCameraChanged();
}

void Viewport3D::doubleWindow() {
    if (!m_committed) m_committed = m_camera.view();
    fitAll();  // measured in the original (TK-031): the view is identical to "Ventana completa" and repeating it changes nothing
    commitView();
    notifyCameraChanged();
}

void Viewport3D::setZoomMarkMode(bool on) {
    if (m_zoomMark == on) return;
    m_zoomMark = on;
    emit zoomMarkModeChanged(on);
}

void Viewport3D::setRotateAroundPoint(bool on) {
    if (m_rotateAroundPoint == on) return;
    m_rotateAroundPoint = on;
    emit rotateAroundPointChanged(on);
}

core::Vec3 Viewport3D::orbitPivotAt(const QPointF& pos) const {
    if (m_structure) {
        const auto hit = core::pickAt(*m_structure, mapper(), core::SelectionFilter{}, pos.x(), pos.y());
        if (hit && hit->kind == core::ElementKind::Node) {
            if (const core::Node* n = m_structure->findNode(hit->id)) return {n->position.x, n->position.y, n->position.z};
        } else if (hit && hit->kind == core::ElementKind::Bar) {
            const core::Bar* bar = m_structure->findBar(hit->id);
            const core::Node* a = bar ? m_structure->findNode(bar->startNode) : nullptr;
            const core::Node* b = bar ? m_structure->findNode(bar->endNode) : nullptr;
            double ax = 0, ay = 0, bx = 0, by = 0;
            if (a && b && mapper().toScreen(a->position, ax, ay) && mapper().toScreen(b->position, bx, by)) {
                const double dx = bx - ax, dy = by - ay, len2 = dx * dx + dy * dy;
                const double t = len2 > 0 ? std::clamp(((pos.x() - ax) * dx + (pos.y() - ay) * dy) / len2, 0.0, 1.0) : 0.0;
                return {a->position.x + t * (b->position.x - a->position.x), a->position.y + t * (b->position.y - a->position.y),
                        a->position.z + t * (b->position.z - a->position.z)};
            }
        }
    }
    return screenToTargetPlane(pos);
}

void Viewport3D::setRotateAroundCamera(bool on) {
    if (m_rotateAroundCamera == on) return;
    m_rotateAroundCamera = on;
    emit rotateAroundCameraChanged(on);
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
    if (event->button() == Qt::LeftButton && m_clip.anyActive() && m_clipPlanesVisible) {  // grabbing the red arrow of a section / the volume
        const int arcId = clipHandleAt(event->position()) ? -1 : clipArcIdAt(event->position());
        if (arcId >= 0) {  // each arc turns only its own plane (or the volume)
            m_clipArcDrag = true;
            m_clipArcId = arcId;
            m_clipDragLast = event->position();
            event->accept();
            return;
        }
        if (const auto handle = clipHandleAt(event->position())) {
            m_clipDrag = handle;
            m_clipDragLast = event->position();
            event->accept();
            return;
        }
    }
    m_drag = actionFor(event->buttons());
    const bool selectionAtStart = m_selectionMode, snapAtStart = m_snapMode;  // a handler may switch the mode under our feet
    if (!m_committed) m_committed = m_camera.view();
    if (m_drag == DragAction::Orbit && m_rotateAroundPoint) m_orbitPivot = orbitPivotAt(event->position());
    if (m_zoomMark && event->button() == Qt::LeftButton && width() > 0 && height() > 0) {
        // The marked rectangle (zoomMarkRect) becomes the whole view: centre on the cursor and come closer by width / rect width.
        const QRectF rect = zoomMarkRect(event->position(), QSizeF(width(), height()));
        m_camera.target = screenToTargetPlane(event->position());
        m_camera.distance = std::clamp(m_camera.distance * rect.width() / width(), Camera::kMinDistance, Camera::kMaxDistance);
        commitView();
        notifyCameraChanged();
        event->accept();
        return;
    }
    if (selectionAtStart && (event->button() == Qt::LeftButton || event->button() == Qt::RightButton)) selectionPress(event);
    if (snapAtStart && event->button() == Qt::LeftButton) {
        updateSnap(event->position());
        emit pointEntered(cursorWorld(), m_snap);
    }
    if (snapAtStart && event->button() == Qt::RightButton) emit rightClicked();
    event->accept();
}

QRectF Viewport3D::zoomMarkRect(const QPointF& cursor, const QSizeF& area) {
    // Measured in the original: centred on the cursor, the viewport's aspect, half width = distance from the cursor to the right edge.
    const double half = std::clamp(area.width() - cursor.x(), area.width() * kZoomMarkMinFraction / 2.0, area.width() / 2.0);
    const double halfH = half * area.height() / std::max(area.width(), 1.0);
    return QRectF(cursor.x() - half, cursor.y() - halfH, 2.0 * half, 2.0 * halfH);
}

void Viewport3D::drawZoomMark(QPainter& painter) const {
    if (!m_zoomMark || !m_cursorInside) return;
    const QRectF r = zoomMarkRect(m_cursor, QSizeF(width(), height()));
    painter.setPen(QPen(QColor(40, 40, 40), 1));
    painter.setBrush(QColor(245, 245, 245, 200));
    painter.drawRect(r);
    painter.setBrush(Qt::NoBrush);
    painter.drawLine(QPointF(m_cursor.x() - 9, m_cursor.y()), QPointF(m_cursor.x() + 9, m_cursor.y()));
    painter.drawLine(QPointF(m_cursor.x(), m_cursor.y() - 9), QPointF(m_cursor.x(), m_cursor.y() + 9));
}

void Viewport3D::mouseMoveEvent(QMouseEvent* event) {
    const QPoint pos = event->position().toPoint();
    if (m_clipArcDrag) {
        if (event->buttons().testFlag(Qt::LeftButton)) {
            dragClipArc(m_clipArcId, m_clipDragLast, event->position());
            m_clipDragLast = event->position();
        } else {
            m_clipArcDrag = false;
        }
        m_cursor = event->position();
        m_cursorInside = true;
        event->accept();
        return;
    }
    if (m_clipDrag) {
        if (event->buttons().testFlag(Qt::LeftButton)) {
            dragClipHandle(*m_clipDrag, m_clipDragLast, event->position());
            m_clipDragLast = event->position();
        } else {
            m_clipDrag.reset();
        }
        m_cursor = event->position();
        m_cursorInside = true;
        event->accept();
        return;
    }
    if (m_zoomMark) { m_cursor = event->position(); update(); }
    const QPoint delta = pos - m_last;
    m_last = pos;
    if (event->buttons() == Qt::NoButton) m_drag = DragAction::None;
    if (m_drag == DragAction::Orbit) {
        const double dx = delta.x() * m_bindings.orbitDegreesPerPixel, dy = delta.y() * m_bindings.orbitDegreesPerPixel;
        if (m_rotateAroundCamera) m_camera.orbitAboutEye(dx, dy);
        else if (m_rotateAroundPoint) m_camera.orbitAbout(m_orbitPivot, dx, dy);
        else m_camera.orbit(dx, dy);
        emit cameraChanged();
        update();
    } else if (m_drag == DragAction::Pan) {
        m_camera.pan(delta.x(), delta.y(), height());
        emit cameraChanged();
        update();
    }
    m_cursor = event->position();
    m_cursorInside = true;
    if (m_selectionMode) updateHover(m_cursor);
    if (m_snapMode) updateSnap(m_cursor);
    if (!m_rubberChain.empty()) update();  // the elastic line follows the pointer
    emit cursorMoved(screenToTargetPlane(event->position()));
    event->accept();
}

void Viewport3D::mouseReleaseEvent(QMouseEvent* event) {
    if (m_clipArcDrag && event->button() == Qt::LeftButton) {
        m_clipArcDrag = false;
        event->accept();
        return;
    }
    if (m_clipDrag && event->button() == Qt::LeftButton) {
        m_clipDrag.reset();
        event->accept();
        return;
    }
    const DragAction finished = m_drag;
    m_drag = actionFor(event->buttons());
    if (finished != DragAction::None && m_drag == DragAction::None) commitView();
    event->accept();
}

void Viewport3D::mouseDoubleClickEvent(QMouseEvent* event) {
    if (event->button() == Qt::MiddleButton) {  // "Ventana completa": also a double click with the middle button
        frameAll();
        event->accept();
        return;
    }
    if (event->button() == Qt::LeftButton && m_clip.anyActive() && m_clipPlanesVisible) {  // double click on a section arrow reverses its sense
        if (const auto handle = clipHandleAt(event->position()); handle && !handle->volume) {
            flipSection(handle->axis);
            m_clipDrag.reset();
            event->accept();
            return;
        }
    }
    QOpenGLWidget::mouseDoubleClickEvent(event);
}

void Viewport3D::wheelEvent(QWheelEvent* event) {
    const double notches = event->angleDelta().y() / 120.0;
    if (notches != 0.0 && width() > 0 && height() > 0) {
        const QPointF p = event->position();
        if (!m_committed) m_committed = m_camera.view();
        m_camera.zoomAt(notches, 2.0 * p.x() / width() - 1.0, 1.0 - 2.0 * p.y() / height(), aspect());
        commitView();
        notifyCameraChanged();
    }
    event->accept();
}

void Viewport3D::setNodes(std::vector<Vec3> nodes) {
    m_nodes = std::move(nodes);
    update();
}

void Viewport3D::setStructure(const core::StructuralModel* structure) {
    m_structure = structure;
    m_deflectionCacheValid = false;
    m_nodes.clear();
    if (structure)
        for (const core::Node& n : structure->nodes) m_nodes.push_back({n.position.x, n.position.y, n.position.z});
    m_hover.reset();
    m_windowStart.reset();
    m_movingNode.reset();
    m_snapModel.reset();
    m_snap = {};
    clearSelection();
    update();
}

bool Viewport3D::elementExists(const core::ElementRef& e) const {
    if (!m_structure) return false;
    switch (e.kind) {
        case core::ElementKind::Node: return m_structure->findNode(e.id) != nullptr;
        case core::ElementKind::Bar: return m_structure->findBar(e.id) != nullptr;
        case core::ElementKind::Shell: return m_structure->findShell(e.id) != nullptr;
    }
    return false;
}

void Viewport3D::refreshStructure() {
    m_deflectionCacheValid = false;
    m_nodes.clear();
    if (m_structure)
        for (const core::Node& n : m_structure->nodes) m_nodes.push_back({n.position.x, n.position.y, n.position.z});
    if (m_hover && !elementExists(*m_hover)) m_hover.reset();
    std::vector<core::ElementRef> stale;
    for (const core::ElementRef& e : m_selection.items())
        if (!elementExists(e)) stale.push_back(e);
    if (!stale.empty() && m_selection.apply(stale, core::SelectMode::Remove)) emit selectionChanged();
    if (m_movingNode && !(m_structure && m_structure->findNode(*m_movingNode))) m_movingNode.reset();
    if (!m_edgeSelection.empty() || m_edgeHover) {
        const auto stale = [this](const core::ShellEdgeKey& k) {
            if (!m_structure) return true;
            const std::vector<core::ShellEdgeKey> edges = core::shellEdges(*m_structure, k.shellId);
            return std::find(edges.begin(), edges.end(), k) == edges.end();
        };
        if (m_edgeHover && stale(*m_edgeHover)) m_edgeHover.reset();
        const std::size_t count = m_edgeSelection.size();
        std::erase_if(m_edgeSelection, stale);
        if (count != m_edgeSelection.size()) emit selectionChanged();
    }
    rebuildSnapModel();
    m_snap = {};
    update();
}

void Viewport3D::setNodeLabelsVisible(bool on) {
    if (m_nodeLabels == on) return;
    m_nodeLabels = on;
    update();
}

void Viewport3D::setBarLabels(std::vector<std::pair<int, QString>> labels) {
    if (m_barLabels == labels) return;
    m_barLabels = std::move(labels);
    update();
}

void Viewport3D::setMovingNode(std::optional<int> nodeId) {
    m_movingNode = nodeId;
    m_snap = {};
    rebuildSnapModel();
    update();
}

void Viewport3D::setRubberChain(std::vector<int> nodeIds) {
    if (nodeIds == m_rubberChain) return;
    m_rubberChain = std::move(nodeIds);
    update();
}

void Viewport3D::setRubberPoints(std::vector<core::Vec3> points) {
    if (points == m_rubberPoints) return;
    m_rubberPoints = std::move(points);
    update();
}

void Viewport3D::setShowShellAxes(bool on) {
    if (on == m_showShellAxes) return;
    m_showShellAxes = on;
    update();
}

void Viewport3D::drawPointRubber(QPainter& painter) const {
    if (m_rubberPoints.empty()) return;
    const core::ScreenMapper map = mapper();
    std::vector<QPointF> points;
    for (const core::Vec3& p : m_rubberPoints) {
        double x, y;
        if (map.toScreen(p, x, y)) points.emplace_back(x, y);
    }
    if (points.empty()) return;
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(Qt::black, 1.5));
    for (std::size_t i = 1; i < points.size(); ++i) painter.drawLine(points[i - 1], points[i]);
    QPointF end = m_cursor;
    if (m_snap.snapped()) {  // the line ends on the point about to be chosen
        double x, y;
        if (map.toScreen(m_snap.point, x, y)) end = QPointF(x, y);
    }
    if (m_cursorInside) painter.drawLine(points.back(), end);
    painter.setPen(QPen(kHighlight, 2));
    for (const QPointF& p : points) painter.drawRect(QRectF(p.x() - nodeHalf(), p.y() - nodeHalf(), 2 * nodeHalf(), 2 * nodeHalf()));
}

void Viewport3D::drawShellAxes(QPainter& painter) const {
    if (!m_showShellAxes || !m_structure || !m_categories.shellsDrawn()) return;
    const core::ScreenMapper map = mapper();
    struct Arrow { const char* label; QColor colour; };
    const Arrow arrows[3] = {{"x", QColor(220, 30, 30)}, {"y", QColor(30, 160, 30)}, {"z", QColor(30, 60, 220)}};
    QFont font = painter.font();
    font.setBold(true);
    painter.setFont(font);
    for (const core::Shell& shell : m_structure->shells) {
        if (!m_structure->elementVisible(shell.layerIds)) continue;
        const std::optional<core::ShellAxes> axes = core::shellLocalAxes(*m_structure, shell);
        if (!axes) continue;
        Vec3 centre;
        double reach = 0.0;
        std::vector<Vec3> corners;
        for (int id : shell.nodeIds)
            if (const core::Node* n = m_structure->findNode(id)) corners.push_back(Vec3{n->position.x, n->position.y, n->position.z});
        if (corners.empty()) continue;
        for (const Vec3& c : corners) centre = centre + c * (1.0 / static_cast<double>(corners.size()));
        for (const Vec3& c : corners) {
            const Vec3 d = c - centre;
            reach = std::max(reach, std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z));
        }
        const double length = reach * 0.5;  // half way to the farthest corner
        double cx, cy;
        if (!map.toScreen(centre, cx, cy)) continue;
        const core::Point3 dirs[3] = {axes->x, axes->y, axes->z};
        for (int i = 0; i < 3; ++i) {
            double tx, ty;
            const Vec3 tip = centre + Vec3{dirs[i].x, dirs[i].y, dirs[i].z} * length;
            if (!map.toScreen(tip, tx, ty)) continue;
            painter.setPen(QPen(arrows[i].colour, 1.5));
            painter.drawLine(QPointF(cx, cy), QPointF(tx, ty));
            painter.drawText(QPointF(tx + 3, ty - 3), QString::fromLatin1(arrows[i].label));
        }
    }
}

void Viewport3D::drawBarRubber(QPainter& painter) const {
    if (m_rubberChain.empty() || !m_structure) return;
    const core::ScreenMapper map = mapper();
    std::vector<QPointF> points;
    for (int id : m_rubberChain) {
        double x, y;
        const core::Node* node = m_structure->findNode(id);
        if (node && map.toScreen(node->position, x, y)) points.emplace_back(x, y);
    }
    if (points.empty()) return;
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(Qt::black, 1.5));
    for (std::size_t i = 1; i < points.size(); ++i) painter.drawLine(points[i - 1], points[i]);
    QPointF end = m_cursor;
    if (m_hover && m_hover->kind == core::ElementKind::Node)  // the line ends on the node about to be chosen
        if (const core::Node* node = m_structure->findNode(m_hover->id)) {
            double x, y;
            if (map.toScreen(node->position, x, y)) end = QPointF(x, y);
        }
    if (m_cursorInside) painter.drawLine(points.back(), end);
    painter.setPen(QPen(kHighlight, 2));
    for (const QPointF& p : points) painter.drawRect(QRectF(p.x() - nodeHalf(), p.y() - nodeHalf(), 2 * nodeHalf(), 2 * nodeHalf()));
}

void Viewport3D::rebuildSnapModel() {
    m_snapModel.reset();
    if (!m_structure || !m_movingNode) return;
    auto model = std::make_shared<core::StructuralModel>(*m_structure);
    const int id = *m_movingNode;
    std::erase_if(model->nodes, [id](const core::Node& n) { return n.id == id; });
    std::erase_if(model->bars, [id](const core::Bar& b) { return b.startNode == id || b.endNode == id; });
    std::erase_if(model->shells, [id](const core::Shell& s) { return std::find(s.nodeIds.begin(), s.nodeIds.end(), id) != s.nodeIds.end(); });
    m_snapModel = std::move(model);
}

void Viewport3D::updateFocusPolicy() { setFocusPolicy(m_selectionMode || m_snapMode ? Qt::ClickFocus : Qt::NoFocus); }

void Viewport3D::setTextSizePercent(double percent) {
    if (!(percent > 0.0) || m_textSizePercent == percent) return;
    m_textSizePercent = percent;
    update();
}

void Viewport3D::setNodeSizeCm(double cm) {
    if (!(cm > 0.0) || m_nodeSizeCm == cm) return;
    m_nodeSizeCm = cm;
    update();
}

void Viewport3D::setSnapMode(bool on) {
    if (m_snapMode == on) return;
    m_snapMode = on;
    m_snap = {};
    updateFocusPolicy();  // digits and Esc reach the viewport (calculator, leaving the command)
    emit snapChanged();
    update();
}

void Viewport3D::updateSnap(const QPointF& pos) {
    core::SnapResult snap;
    if (const core::StructuralModel* model = m_snapModel ? m_snapModel.get() : m_structure) {
        core::SnapParams params;
        params.nodesVisible = m_categories.nodesDrawn();  // hidden elements are not snapped to
        params.barsVisible = m_categories.barsDrawn();
        snap = core::snapCursor(*model, mapper(), pos.x(), pos.y(), m_snapOptions, params);
    }
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
    updateFocusPolicy();
    emit statusHint(on ? tr("LMB: Seleccione un grupo de elementos. RMB: Actuar sobre los elementos seleccionados") : QString());
    update();
}

void Viewport3D::setSelectionFilter(const core::SelectionFilter& filter) {
    m_filter = filter;
    if (m_selection.restrictTo(effectiveFilter())) emit selectionChanged();
    m_hover.reset();
    update();
}

void Viewport3D::setCategorySettings(const core::CategorySettings& settings) {
    if (m_categories == settings) return;
    m_categories = settings;
    if (m_selection.restrictTo(effectiveFilter())) emit selectionChanged();
    m_hover.reset();
    m_windowStart.reset();
    update();
}

core::SelectionFilter Viewport3D::effectiveFilter() const {
    core::SelectionFilter f = m_filter;
    f.nodes = f.nodes && m_categories.nodesDrawn();
    f.bars = f.bars && m_categories.barsDrawn();
    f.shells = f.shells && m_categories.shellsDrawn();
    return f;
}

void Viewport3D::clearSelection() {
    if (m_selection.empty() && m_edgeSelection.empty()) return;
    m_selection.clear();
    m_edgeSelection.clear();
    emit selectionChanged();
    update();
}

void Viewport3D::setShellContourPick(bool on) {
    if (m_shellContourPick == on) return;
    m_shellContourPick = on;
    m_hover.reset();
    update();
}

void Viewport3D::setEdgeSelectionMode(bool on) {
    if (m_edgeMode == on) return;
    m_edgeMode = on;
    m_edgeHover.reset();
    m_hover.reset();
    m_windowStart.reset();
    if (!m_edgeSelection.empty()) {
        m_edgeSelection.clear();
        emit selectionChanged();
    }
    update();
}

void Viewport3D::setEdgeSelection(const std::vector<core::ShellEdgeKey>& edges) {
    m_edgeSelection.clear();
    for (const core::ShellEdgeKey& e : edges)
        if (std::find(m_edgeSelection.begin(), m_edgeSelection.end(), e) == m_edgeSelection.end()) m_edgeSelection.push_back(e);
    emit selectionChanged();
    update();
}

void Viewport3D::setShellInfoProvider(ShellInfoProvider provider) {
    m_shellInfoProvider = std::move(provider);
    update();
}

void Viewport3D::setSelectionItems(const std::vector<core::ElementRef>& items) {
    m_selection.clear();
    m_selection.apply(items, core::SelectMode::Add);
    emit selectionChanged();
    update();
}

void Viewport3D::cancelWindow() {
    if (!m_windowStart) return;
    m_windowStart.reset();
    update();
}

void Viewport3D::updateHover(const QPointF& pos) {
    if (m_edgeMode) {
        std::optional<core::ShellEdgeKey> edge;
        if (m_structure && !m_windowStart) edge = core::pickShellEdgeAt(*m_structure, mapper(), pos.x(), pos.y());
        if (edge != m_edgeHover || m_windowStart) {
            m_edgeHover = edge;
            update();
        }
        return;
    }
    std::optional<core::ElementRef> hover;
    core::PickOptions options;
    options.shellContourOnly = m_shellContourPick;
    if (m_structure && !m_windowStart) hover = core::pickAt(*m_structure, mapper(), effectiveFilter(), pos.x(), pos.y(), options);
    if (hover && m_nodeExcluded && hover->kind == core::ElementKind::Node && m_nodeExcluded(hover->id)) hover.reset();
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
    if (m_edgeMode) {
        edgePress(event);
        return;
    }
    const QPointF pos = event->position();
    m_cursor = pos;
    core::PickOptions pickOptions;
    pickOptions.shellContourOnly = m_shellContourPick;
    const core::SelectMode mode = event->modifiers().testFlag(Qt::ControlModifier) ? core::SelectMode::Add
                                  : event->modifiers().testFlag(Qt::ShiftModifier) ? core::SelectMode::Remove
                                                                                    : core::SelectMode::Replace;
    bool changed = false;
    if (m_windowStart) {  // second corner
        const QPointF a = *m_windowStart;
        m_windowStart.reset();
        std::vector<core::ElementRef> hits = core::pickRect(*m_structure, mapper(), effectiveFilter(), a.x(), a.y(), pos.x(), pos.y());
        if (m_nodeExcluded)
            std::erase_if(hits, [this](const core::ElementRef& e) { return e.kind == core::ElementKind::Node && m_nodeExcluded(e.id); });
        changed = m_selection.apply(hits, mode);
    } else if (auto hit = core::pickAt(*m_structure, mapper(), effectiveFilter(), pos.x(), pos.y(), pickOptions);
               hit && m_nodeExcluded && hit->kind == core::ElementKind::Node && m_nodeExcluded(hit->id)) {
        // a node the tool cannot capture: the click does nothing (no window starts either)
    } else if (hit) {
        changed = m_selection.apply({*hit}, mode);
    } else {
        m_windowStart = pos;  // first corner; a plain click on empty space does not clear the selection
        m_cursor = pos;
    }
    m_hover.reset();
    if (changed) emit selectionChanged();
    update();
}

void Viewport3D::edgePress(QMouseEvent* event) {
    const QPointF pos = event->position();
    m_cursor = pos;
    const core::SelectMode mode = event->modifiers().testFlag(Qt::ControlModifier) ? core::SelectMode::Add
                                  : event->modifiers().testFlag(Qt::ShiftModifier) ? core::SelectMode::Remove
                                                                                    : core::SelectMode::Replace;
    std::vector<core::ShellEdgeKey> hits;
    if (m_windowStart) {
        const QPointF a = *m_windowStart;
        m_windowStart.reset();
        hits = core::pickShellEdgesRect(*m_structure, mapper(), a.x(), a.y(), pos.x(), pos.y());
    } else if (const auto hit = core::pickShellEdgeAt(*m_structure, mapper(), pos.x(), pos.y())) {
        hits.push_back(*hit);
    } else {
        m_windowStart = pos;
        update();
        return;
    }
    const std::vector<core::ShellEdgeKey> before = m_edgeSelection;
    const auto has = [this](const core::ShellEdgeKey& k) { return std::find(m_edgeSelection.begin(), m_edgeSelection.end(), k) != m_edgeSelection.end(); };
    if (mode == core::SelectMode::Replace) m_edgeSelection.clear();
    for (const core::ShellEdgeKey& k : hits) {
        if (mode == core::SelectMode::Remove) std::erase(m_edgeSelection, k);
        else if (!has(k)) m_edgeSelection.push_back(k);
    }
    m_edgeHover.reset();
    if (before != m_edgeSelection) emit selectionChanged();
    update();
}

void Viewport3D::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Escape && (m_selectionMode || m_snapMode)) {
        event->accept();
        if (m_selectionMode && m_windowStart) {  // first Esc only cancels the pending window
            cancelWindow();
            return;
        }
        if (m_selectionMode) clearSelection();
        emit escapePressed();
        return;
    }
    if (m_snapMode && !(event->modifiers() & (Qt::ControlModifier | Qt::AltModifier)) && event->text().size() == 1 &&
        event->text().at(0) >= QLatin1Char('0') && event->text().at(0) <= QLatin1Char('9')) {
        event->accept();
        emit digitTyped(event->text().at(0));
        return;
    }
    QOpenGLWidget::keyPressEvent(event);
}

void Viewport3D::leaveEvent(QEvent* event) {
    m_cursorInside = false;
    m_coordTooltipDismissed = false;
    if (m_hover) m_hover.reset();
    update();
    QOpenGLWidget::leaveEvent(event);
}

void Viewport3D::enterEvent(QEnterEvent* event) {
    m_cursorInside = true;
    m_cursor = event->position();
    if (focusPolicy() != Qt::NoFocus) setFocus();  // the keyboard acts on the window the pointer is in
    update();
    QOpenGLWidget::enterEvent(event);
}

void Viewport3D::setTooltipPosition(TooltipPosition position) {
    if (m_tooltipPosition == position) return;
    m_tooltipPosition = position;
    update();
}

void Viewport3D::setFillShells(bool on) {
    if (m_fillShells == on) return;
    m_fillShells = on;
    update();
}

void Viewport3D::setShowElementInfo(bool on) {
    if (m_showElementInfo == on) return;
    m_showElementInfo = on;
    update();
}

std::optional<int> Viewport3D::infoBarId() const {
    if (!m_showElementInfo || !m_structure) return std::nullopt;
    if (m_snapMode && m_snap.snapped() && m_snap.element) {
        if (m_snap.element->kind == core::ElementKind::Bar) return m_snap.element->id;
        if (m_snap.kind == core::SnapKind::Endpoint && m_snap.element->kind == core::ElementKind::Node) {
            std::optional<int> lowest;  // a bar that meets at the snapped node
            for (const core::Bar& b : m_structure->bars)
                if ((b.startNode == m_snap.element->id || b.endNode == m_snap.element->id) && (!lowest || b.id < *lowest)) lowest = b.id;
            return lowest;
        }
    }
    if (m_selectionMode && m_hover && m_hover->kind == core::ElementKind::Bar) return m_hover->id;
    return std::nullopt;
}

std::optional<int> Viewport3D::infoShellId() const {
    if (!m_showElementInfo || !m_structure || !m_selectionMode) return std::nullopt;
    if (m_edgeMode) {
        if (m_edgeHover) return m_edgeHover->shellId;
        if (m_shellInfoProvider && !m_edgeSelection.empty()) return m_edgeSelection.back().shellId;
        return std::nullopt;
    }
    if (m_hover && m_hover->kind == core::ElementKind::Shell) return m_hover->id;
    if (m_shellInfoProvider && !m_hover) {  // nothing under the cursor: the last selected shell keeps its box
        const auto& items = m_selection.items();
        for (auto it = items.rbegin(); it != items.rend(); ++it)
            if (it->kind == core::ElementKind::Shell) return it->id;
    }
    return std::nullopt;
}

QStringList Viewport3D::shellInfoLines(const core::StructuralModel& structure, int shellId, const core::ProfileSettings& profiles) {
    const core::Shell* shell = structure.findShell(shellId);
    if (!shell) return {};
    QStringList lines;
    lines << QStringLiteral("L%1").arg(shell->id);  // automatic reference (laminas.md 7.3)
    lines << QStringLiteral("Capa : %1").arg(QString::fromStdString(structure.layerNames(shell->layerIds)));
    lines << QStringLiteral("Espesor : %1 mm").arg(QString::number(shell->thicknessM * 1000.0, 'f', 2));
    lines << QStringLiteral("Material : %1").arg(QString::fromStdString(core::shellMaterialName(shell->material, profiles)));
    // Stiffness factors: always 1 for now (the original's default; there is no command that edits them yet).
    lines << QStringLiteral("Membrana Fxx : 1, Fyy : 1, Fxy : 1");
    lines << QStringLiteral("Flexión Mxx : 1, Myy : 1, Mxy : 1");
    lines << QStringLiteral("Cortante Vxz : 1, Vyz : 1");
    lines << QStringLiteral("Análisis de pandeo: No se considera en el análisis");
    lines << QStringLiteral("Análisis modal: Se considera en el análisis");
    return lines;
}

QString Viewport3D::coordinatesText(const Vec3& world) {
    const auto fmt = [](double v) {
        QString s = QString::number(v, 'f', 3);
        if (s == QLatin1String("-0.000")) s = QStringLiteral("0.000");  // no negative zero
        return s;
    };
    return QStringLiteral("%1, %2, %3 m").arg(fmt(world.x), fmt(world.y), fmt(world.z));
}

QStringList Viewport3D::barInfoLines(const core::StructuralModel& structure, int barId) {
    const core::Bar* bar = structure.findBar(barId);
    if (!bar) return {};
    const core::Node* a = structure.findNode(bar->startNode);
    const core::Node* b = structure.findNode(bar->endNode);
    double length = 0.0;
    if (a && b) {
        const double dx = b->position.x - a->position.x, dy = b->position.y - a->position.y, dz = b->position.z - a->position.z;
        length = std::sqrt(dx * dx + dy * dy + dz * dz);
    }
    QStringList lines;
    lines << QStringLiteral("Barra N%1/N%2: L. %3 m")
                 .arg(bar->startNode)
                 .arg(bar->endNode)
                 .arg(QString::number(length, 'g', 6).replace('.', ','));
    lines << QStringLiteral("Capa : %1").arg(QString::fromStdString(structure.layerNames(bar->layerIds)));
    if (const core::CrossSection* section = structure.findSection(bar->sectionId)) {
        QString text = QString::fromStdString(section->name);
        if (const core::Material* material = structure.findMaterial(bar->materialId))
            text += QStringLiteral(" (%1)").arg(QString::fromStdString(material->name));
        lines << text;
    }
    return lines;
}

QRectF Viewport3D::anchoredRect(const QSizeF& size, const QSizeF& area, TooltipPosition position, const QPointF& anchor) {
    if (position == TooltipPosition::TopRightCorner) return QRectF(QPointF(area.width() - size.width(), 0.0), size);
    // Original ("Sobre el elemento", user_sobre_elemento_barra.png): a callout whose bottom-left corner sits ~32 px to
    // the right of and just above the element; flipped to the left or below when it would leave the area.
    constexpr double kDx = 32.0, kDy = 2.0;
    double x = anchor.x() + kDx;
    if (x + size.width() > area.width()) x = anchor.x() - kDx - size.width();
    double y = anchor.y() + kDy - size.height();
    if (y < 0.0) y = anchor.y() - kDy + 24.0;
    return QRectF(QPointF(std::clamp(x, 0.0, std::max(0.0, area.width() - size.width())),
                          std::clamp(y, 0.0, std::max(0.0, area.height() - size.height()))), size);
}

QPointF Viewport3D::infoAnchor(const std::optional<int>& barId) const {
    const core::ScreenMapper map = mapper();
    double x = m_cursor.x(), y = m_cursor.y();
    if (m_snapMode && m_snap.snapped()) {
        if (!map.toScreen(m_snap.point, x, y)) { x = m_cursor.x(); y = m_cursor.y(); }
    } else if (m_structure && barId) {
        const core::Bar* b = m_structure->findBar(*barId);
        const core::Node* na = b ? m_structure->findNode(b->startNode) : nullptr;
        const core::Node* nb = b ? m_structure->findNode(b->endNode) : nullptr;
        double bx, by;
        if (na && nb && map.toScreen(na->position, x, y) && map.toScreen(nb->position, bx, by)) { x = (x + bx) / 2; y = (y + by) / 2; }
    }
    return {x, y};
}

QPointF Viewport3D::coordinateTooltipOrigin(const QPointF& cursor, const QSizeF& size, const QSizeF& area) {
    constexpr double kDx = 40.0, kDy = 43.0;  // measured in the original (sn2: tooltip at +40, +43 px from the cursor)
    double x = cursor.x() + kDx, y = cursor.y() + kDy;
    if (x + size.width() > area.width()) x = cursor.x() - kDx - size.width();
    if (y + size.height() > area.height()) y = cursor.y() - kDy - size.height();
    return {std::max(0.0, x), std::max(0.0, y)};
}

QString Viewport3D::tooltipPositionLabel(TooltipPosition position) {
    switch (position) {
        case TooltipPosition::TopRightCorner: return tr("En la esquina superior derecha");
        case TooltipPosition::OnElement: return tr("Sobre el elemento");
    }
    return {};
}

void Viewport3D::drawCoordinateTooltip(QPainter& painter) const {
    if (!coordinateTooltipVisible()) return;
    QFont font = painter.font();
    font.setPixelSize(16);
    painter.setFont(font);
    const QString text = coordinatesText(cursorWorld());
    const QSizeF size(painter.fontMetrics().horizontalAdvance(text) + 14, 28);
    const QRectF box(coordinateTooltipOrigin(m_cursor, size, QSizeF(width(), height())), size);
    painter.setPen(QPen(QColor(200, 200, 200), 1));  // original: dark grey box, white text
    painter.setBrush(QColor(64, 64, 64));
    painter.drawRoundedRect(box, 3, 3);
    painter.setPen(Qt::white);
    painter.drawText(box, Qt::AlignCenter, text);
}

void Viewport3D::drawInfoBox(QPainter& painter) const {
    const std::optional<int> id = infoBarId();
    const std::optional<int> shellId = id ? std::nullopt : infoShellId();
    if (!id && !shellId) return;
    const core::ProfileSettings noProfiles;
    std::optional<core::ShellEdgeKey> infoEdge;
    if (m_edgeMode) infoEdge = m_edgeHover ? m_edgeHover : (m_edgeSelection.empty() ? std::nullopt : std::optional<core::ShellEdgeKey>(m_edgeSelection.back()));
    QStringList lines = id ? barInfoLines(*m_structure, *id)
                           : (m_shellInfoProvider ? m_shellInfoProvider(*shellId, infoEdge)
                                                  : shellInfoLines(*m_structure, *shellId, m_profiles ? *m_profiles : noProfiles));
    if (lines.isEmpty()) return;
    if (id && m_endFixityLabels && m_barEnds) {  // Empotramiento: the fixity of both ends (articulaciones.md section 3)
        const core::BarEndsRecord record = core::barEndsOf(*m_barEnds, *id);
        lines << tr("Empotramiento en extremos") << QString::fromStdString(core::endTooltipLine(record, core::kBarOrigin))
              << QString::fromStdString(core::endTooltipLine(record, core::kBarFinal));
    }
    if (id && m_bucklingLabels == BucklingLabels::Beta && m_buckling) {  // Pandeo: the data of the bar (pandeo.md 9.5)
        const core::Bar* bar = m_structure->findBar(*id);
        const core::Node* na = bar ? m_structure->findNode(bar->startNode) : nullptr;
        const core::Node* nb = bar ? m_structure->findNode(bar->endNode) : nullptr;
        const double barLength = na && nb ? core::geom::length(core::geom::sub(nb->position, na->position)) : 0.0;
        for (const std::string& line : core::bucklingInfoLines(core::bucklingOf(*m_buckling, *id), barLength)) lines << QString::fromStdString(line);
    }
    if (id && m_bucklingLabels == BucklingLabels::Lateral && m_buckling) {  // Pandeo lateral: block only for a bar with lateral data (pandeo.md 9.6)
        for (const std::string& line : core::lateralInfoLines(core::bucklingOf(*m_buckling, *id))) lines << QString::fromStdString(line);
    }
    // Row kinds: a leading '\x01' is a compact row (the factor block of the shell "ficha"), '\x02' a grey header row.
    enum class RowKind { Normal, Compact, Header };
    std::vector<RowKind> kinds;
    for (QString& line : lines) {
        RowKind kind = RowKind::Normal;
        if (line.startsWith(QLatin1Char('\x01'))) kind = RowKind::Compact;
        else if (line.startsWith(QLatin1Char('\x02'))) kind = RowKind::Header;
        if (kind != RowKind::Normal) line.remove(0, 1);
        kinds.push_back(kind);
    }
    QFont font = painter.font();
    font.setPixelSize(14);
    const QFont bold = [&] { QFont f = font; f.setBold(true); return f; }();
    constexpr double kRow = 26.0, kCompact = 18.0, kPad = 6.0, kBlockPad = 5.0;
    const auto heightOf = [&](int i) { return kinds[static_cast<std::size_t>(i)] == RowKind::Compact ? kCompact : kRow; };
    double w = 0, total = 0;
    for (int i = 0; i < lines.size(); ++i) {
        w = std::max(w, static_cast<double>(QFontMetrics(i == 0 ? bold : font).horizontalAdvance(lines[i])));
        total += heightOf(i);
        const bool first = kinds[static_cast<std::size_t>(i)] == RowKind::Compact && (i == 0 || kinds[static_cast<std::size_t>(i - 1)] != RowKind::Compact);
        if (first) total += kBlockPad * 2;  // padding above and below a block of compact rows
    }
    const QSizeF size(w + 2 * kPad, total);
    const QRectF box = anchoredRect(size, QSizeF(width(), height()), m_tooltipPosition, infoAnchor(id));
    painter.setBrush(Qt::white);
    painter.setPen(QPen(QColor(190, 190, 190), 1));  // original: white table, one grey-ruled row per line
    painter.drawRect(box);
    if (m_tooltipPosition == TooltipPosition::OnElement) {  // callout tail pointing at the element
        const QPointF tip = infoAnchor(id);
        const bool right = box.left() > tip.x();
        const double edge = right ? box.left() : box.right();
        const QPointF tail[3] = {{edge, box.bottom() - 18}, {edge, box.bottom() - 2}, {tip.x() + (right ? 2 : -2), tip.y() - 2}};
        painter.setPen(QPen(QColor(190, 190, 190), 1));
        painter.setBrush(Qt::white);
        painter.drawPolygon(tail, 3);
    }
    double y = box.top();
    for (int i = 0; i < lines.size(); ++i) {
        const RowKind kind = kinds[static_cast<std::size_t>(i)];
        const bool prevCompact = i > 0 && kinds[static_cast<std::size_t>(i - 1)] == RowKind::Compact;
        if (kind == RowKind::Compact && !prevCompact) {  // block start: rule above and padding
            if (i > 0) { painter.setPen(QPen(QColor(190, 190, 190), 1)); painter.drawLine(QPointF(box.left(), y), QPointF(box.right(), y)); }
            y += kBlockPad;
        } else if (kind != RowKind::Compact && i > 0) {
            if (prevCompact) y += kBlockPad;
            painter.setPen(QPen(QColor(190, 190, 190), 1));
            painter.drawLine(QPointF(box.left(), y), QPointF(box.right(), y));
        }
        const QRectF row(box.left(), y, box.width(), heightOf(i));
        if (kind == RowKind::Header) painter.fillRect(row.adjusted(1, 1, -1, 0), QColor(238, 238, 238));
        painter.setFont(i == 0 ? bold : font);
        painter.setPen(QColor(20, 20, 20));
        painter.drawText(row.adjusted(kPad, 0, -kPad, 0), Qt::AlignVCenter | Qt::AlignLeft, lines[i]);
        y += heightOf(i);
    }
}

namespace {
/// Layer colour 0xRRGGBB; the original draws black elements with its dark grey default.
QColor layerColour(unsigned rgb) { return rgb == 0 ? QColor(32, 32, 32) : QColor::fromRgb(rgb); }
}  // namespace

void Viewport3D::drawStructure(QPainter& painter) const {
    if (!m_structure) return;
    const core::ScreenMapper map = mapper();
    const auto colourOf = [this](const std::vector<int>& layerIds) {
        return m_categories.useLayerColours ? layerColour(m_structure->elementColor(layerIds)) : layerColour(0);
    };
    const bool clipping = m_clip.anyActive();
    const std::vector<core::ClipHalfSpace> spaces = m_clip.halfSpaces();
    // Nudos > Mover: the original bars stay where they are until the new position is fixed; the rubber bars to the cursor are
    // drawn by drawMoveRubber() (nudos.md section 9.2).
    const auto positionOf = [&](const core::Node& n) -> Vec3 { return Vec3{n.position.x, n.position.y, n.position.z}; };
    for (const core::Shell& shell : m_structure->shells) {
        if (!m_categories.shellsDrawn()) break;
        if (!m_structure->elementVisible(shell.layerIds)) continue;  // layer hidden with the eye
        QPolygonF poly;
        std::vector<Vec3> corners;
        // The shell is drawn displaced by its eccentricity along local +Z; nodes and the definition contour stay (laminas.md 11.2).
        for (const core::Point3& c : core::shellDrawnCorners(*m_structure, shell)) corners.push_back(Vec3{c.x, c.y, c.z});
        if (corners.size() != shell.nodeIds.size()) continue;
        if (clipping) corners = core::clipPolygon(corners, spaces);  // sections / clipping volume cut the shell
        for (const Vec3& c : corners) {
            double x, y;
            if (map.toScreen(c, x, y)) poly << QPointF(x, y);
        }
        if (poly.size() < 3 || poly.size() != static_cast<int>(corners.size())) continue;
        const QColor colour = colourOf(shell.layerIds);
        const bool rendered = m_structure->elementRendered(shell.layerIds);
        painter.setPen(QPen(rendered && m_objectEdges ? QColor(30, 30, 30) : colour, rendered && m_objectEdges ? 2 : 1));  // "Aristas de objetos"
        if (rendered) {  // cube of the layer: solid
            QColor fill = colour.lightnessF() < 0.2 ? QColor(120, 160, 200) : colour;
            fill.setAlpha(170);
            painter.setBrush(fill);
        } else {
            painter.setBrush(m_fillShells ? QBrush(QColor(120, 160, 200, 60)) : QBrush(Qt::NoBrush));
        }
        painter.drawPolygon(poly);
    }
    painter.setBrush(Qt::NoBrush);
    for (const core::Bar& bar : m_structure->bars) {
        if (!m_categories.barsDrawn()) break;
        if (!m_structure->elementVisible(bar.layerIds)) continue;
        const core::Node* a = m_structure->findNode(bar.startNode);
        const core::Node* b = m_structure->findNode(bar.endNode);
        if (!(a && b)) continue;
        Vec3 pa = positionOf(*a), pb = positionOf(*b);
        if (clipping) {
            const auto cut = core::clipSegment(pa, pb, spaces);
            if (!cut) continue;
            pa = cut->first;
            pb = cut->second;
        }
        double ax, ay, bx, by;
        if (!(map.toScreen(pa, ax, ay) && map.toScreen(pb, bx, by))) continue;
        const QColor colour = colourOf(bar.layerIds);
        // Wireframe: thin line; rendered layer: the profile drawn solid (thick stroke), both in the layer colour.
        const bool rendered = m_structure->elementRendered(bar.layerIds);
        if (rendered && m_objectEdges) {  // "Aristas de objetos": dark edge around the solid profile
            painter.setPen(QPen(QColor(30, 30, 30), 10.0, Qt::SolidLine, Qt::FlatCap));
            painter.drawLine(QPointF(ax, ay), QPointF(bx, by));
        }
        painter.setPen(QPen(colour, rendered ? 7.0 : 1.5, Qt::SolidLine, rendered ? Qt::FlatCap : Qt::SquareCap));
        painter.drawLine(QPointF(ax, ay), QPointF(bx, by));
    }
}

void Viewport3D::setNodeSupports(const core::NodeSupports* supports) {
    m_nodeSupports = supports;
    update();
}

void Viewport3D::setNodalLoads(const core::NodalLoads* loads) {
    m_nodalLoads = loads;
    update();
}

void Viewport3D::setLoadsVisible(bool on) {
    m_loadsVisible = on;
    update();
}

void Viewport3D::setLoadViewCase(std::optional<int> loadCaseId) {
    m_loadViewCase = loadCaseId;
    update();
}

void Viewport3D::setLoadSelectionHighlight(bool on) {
    m_loadHighlight = on;
    update();
}

QString Viewport3D::nodalLoadText(double kn) {
    QString text = QString::number(core::roundLoadValue(kn), 'f', 2);
    while (text.endsWith(QLatin1Char('0'))) text.chop(1);
    if (text.endsWith(QLatin1Char('.'))) text.chop(1);
    return text;
}

bool Viewport3D::nodalLoadSegment(const core::NodalLoad& load, QPointF& start, QPointF& tip) const {
    if (!m_structure) return false;
    const core::Node* node = m_structure->findNode(load.nodeId);
    if (!node) return false;
    const core::ScreenMapper map = mapper();
    double x, y;
    if (!map.toScreen(node->position, x, y)) return false;
    tip = QPointF(x, y);
    // Measured (cargas_nudos.md 2.6): the line has a length in METRES (Escalas), drawn from the side the load comes from.
    const core::Point3 d = core::nodalLoadDirection(load);
    const double length = core::nodalLoadDrawLengthM(m_nodalLoads ? m_nodalLoads->scales : core::LoadScales{}, load);
    core::Point3 p = node->position;
    p.x -= d.x * length;
    p.y -= d.y * length;
    p.z -= d.z * length;
    double x2, y2;
    if (map.toScreen(p, x2, y2) && std::hypot(x2 - x, y2 - y) > 1e-6) {
        start = QPointF(x2, y2);
    } else {
        start = tip - QPointF(0, 50.0);  // a load seen end-on has no length on screen: a short vertical stub
    }
    return true;
}

void Viewport3D::drawNodalLoads(QPainter& painter) const {
    if (!m_loadsVisible || !m_nodalLoads || !m_structure || m_nodalLoads->items.empty()) return;
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);
    QFont font = painter.font();
    for (const core::NodalLoad& load : m_nodalLoads->items) {
        if (m_loadViewCase && load.loadCaseId != *m_loadViewCase) continue;
        const core::Node* node = m_structure->findNode(load.nodeId);
        if (!node || !visibleAfterClipping({node->position.x, node->position.y, node->position.z})) continue;
        QPointF start, tip;
        if (!nodalLoadSegment(load, start, tip)) continue;
        const bool selected = m_loadHighlight && m_selectionMode && m_selection.contains({core::ElementKind::Node, load.nodeId});
        const QColor colour = selected ? kHighlight : kLoadYellow;
        painter.setPen(QPen(colour, 1));
        painter.drawLine(start, tip);
        const double lengthPx = std::max(1e-9, std::hypot(tip.x() - start.x(), tip.y() - start.y()));
        const QPointF dir = (tip - start) / lengthPx;
        const QPointF normal(-dir.y(), dir.x());
        QPolygonF head;
        const double headLength = std::max(8.0, 0.1 * lengthPx);  // measured: a small V at the node that grows with the line
        head << tip << tip - dir * headLength + normal * (0.35 * headLength) << tip - dir * headLength - normal * (0.35 * headLength);
        painter.setBrush(colour);
        painter.drawPolygon(head);
        // The value is written along the line, upright, on the side of the text's "up".
        double angle = std::atan2(dir.y(), dir.x()) * 180.0 / std::numbers::pi;
        if (angle > 90.0 - 1e-6) angle -= 180.0;  // a vertical line reads from the bottom to the top (measured)
        if (angle < -90.0) angle += 180.0;
        const double rad = angle * std::numbers::pi / 180.0;
        const QPointF up(std::sin(rad), -std::cos(rad));
        const QString text = nodalLoadText(load.magnitude);
        // Measured: the glyph height grows with the line (about 0.44 of its length, i.e. a font of 0.6 of it), the value is centred on the line
        // and the gap to the line is a tenth of its length.
        font.setPixelSize(std::clamp(static_cast<int>(std::lround(0.6 * lengthPx * m_textSizePercent / 100.0)), 6, 400));
        painter.setFont(font);
        const QFontMetricsF metrics(font);
        const QPointF anchor = start + (tip - start) * 0.5 + up * (0.1 * lengthPx);
        painter.setPen(colour);
        painter.save();
        painter.translate(anchor);
        painter.rotate(angle);
        painter.drawText(QPointF(-metrics.horizontalAdvance(text) / 2.0, 0), text);
        painter.restore();
    }
    painter.restore();
}

void Viewport3D::setNodeLigatures(const core::NodeLigatures* ligatures) {
    m_nodeLigatures = ligatures;
    update();
}

void Viewport3D::setLigatureLabelsVisible(bool on) {
    if (m_ligatureLabels == on) return;
    m_ligatureLabels = on;
    update();
}

void Viewport3D::setNodeDiaphragms(const core::Diaphragms* diaphragms) {
    m_nodeDiaphragms = diaphragms;
    update();
}

void Viewport3D::setDiaphragmLabelsVisible(bool on) {
    if (m_diaphragmLabels == on) return;
    m_diaphragmLabels = on;
    update();
}

void Viewport3D::setBarEnds(const core::BarEnds* ends) {
    m_barEnds = ends;
    update();
}

void Viewport3D::setEndFixityLabelsVisible(bool on) {
    if (m_endFixityLabels == on) return;
    m_endFixityLabels = on;
    update();
}

void Viewport3D::setBuckling(const core::BucklingData* data) {
    m_buckling = data;
    update();
}

void Viewport3D::setShellBands(const core::ShellProperties* data) {
    m_shellBands = data;
    update();
}

void Viewport3D::setBucklingLabels(BucklingLabels labels) {
    if (m_bucklingLabels == labels) return;
    m_bucklingLabels = labels;
    update();
}

void Viewport3D::setBarAxesVisible(bool on) {
    if (m_barAxes == on) return;
    m_barAxes = on;
    update();
}

void Viewport3D::setNodeExclusion(std::function<bool(int)> excluded) {
    m_nodeExcluded = std::move(excluded);
    m_hover.reset();
    update();
}

void Viewport3D::drawSupports(QPainter& painter) const {
    if (!m_structure || !m_nodeSupports || m_nodeSupports->empty() || !m_categories.supportsDrawn()) return;
    const core::ScreenMapper map = mapper();
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);
    const auto colourOf = [&](int nodeId) {
        const core::ElementRef ref{core::ElementKind::Node, nodeId};
        if (m_selectionMode && m_selection.contains(ref)) return kHighlight;  // measured: orange when selected
        if (m_selectionMode && m_hover && *m_hover == ref) return QColor(QStringLiteral("#C8B68E"));  // beige under the cursor
        return kNodeBlue;
    };
    const auto screenOf = [&](const core::Node& n, QPointF& out) {
        double x, y;
        if (!map.toScreen(n.position, x, y)) return false;
        out = QPointF(x, y);
        return visibleAfterClipping({n.position.x, n.position.y, n.position.z});
    };
    for (const auto& [id, support] : m_nodeSupports->external) {
        const core::Node* n = m_structure->findNode(id);
        QPointF at;
        if (!n || !screenOf(*n, at)) continue;
        SymbolAxes axes{QPointF(1, 0), QPointF(0.87, 0.5), QPointF(0, -1)};  // fallbacks when an axis cannot be projected
        for (int i = 0; i < 3; ++i) {
            double x, y;
            core::Point3 p = n->position;
            (i == 0 ? p.x : (i == 1 ? p.y : p.z)) += 1.0;
            if (!map.toScreen(p, x, y)) continue;
            const QPointF d(x - at.x(), y - at.y());
            const double len = std::hypot(d.x(), d.y());
            if (len > 1e-6) axes[static_cast<std::size_t>(i)] = d / len;
        }
        QColor colour = colourOf(id);
        if (m_loadsVisible && !support.prescribed.empty() && colour == kNodeBlue) colour = kPrescribedGreen;  // measured: green with prescribed displacements
        drawExternalSupportSymbol(painter, support, at, axes, colour);
    }
    for (const auto& [id, link] : m_nodeSupports->internal) {
        const core::Node* n = m_structure->findNode(id);
        QPointF at;
        if (n && screenOf(*n, at)) drawInternalLinkSymbol(painter, link, at, colourOf(id));
    }
    painter.restore();
}

void Viewport3D::drawNodes(QPainter& painter) const {
    if (!m_structure || !m_categories.nodesDrawn()) return;
    drawSupports(painter);
    const core::ScreenMapper map = mapper();
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(kNodeBlue, 1));
    for (const core::Node& n : m_structure->nodes) {
        double x, y;
        if (m_movingNode && n.id == *m_movingNode) continue;  // drawn orange with its cross below
        if (!visibleAfterClipping({n.position.x, n.position.y, n.position.z})) continue;
        // Diafragmas: the square of a node of a diaphragm takes its colour (measured: magenta).
        const core::Diaphragm* diaphragm = m_diaphragmLabels && m_nodeDiaphragms ? core::diaphragmOfNode(*m_nodeDiaphragms, n.id) : nullptr;
        painter.setPen(QPen(diaphragm ? QColor::fromRgb(diaphragm->color) : kNodeBlue, 1));
        if (map.toScreen(n.position, x, y)) painter.drawRect(QRectF(x - nodeHalf(), y - nodeHalf(), 2 * nodeHalf(), 2 * nodeHalf()));
    }
    if (m_movingNode) {  // measured: the chosen node is an orange square with a long orange cross, the node in motion a black square
        double x, y;
        if (const core::Node* chosen = m_structure->findNode(*m_movingNode); chosen && map.toScreen(chosen->position, x, y)) {
            painter.setPen(QPen(kHighlight, 2));
            painter.drawRect(QRectF(x - nodeHalf(), y - nodeHalf(), 2 * nodeHalf(), 2 * nodeHalf()));
            painter.setPen(QPen(kHighlight, 1));
            painter.drawLine(QPointF(x - 65, y), QPointF(x + 65, y));
            painter.drawLine(QPointF(x, y - 65), QPointF(x, y + 65));
        }
        if (map.toScreen(cursorWorld(), x, y)) {
            painter.setPen(QPen(Qt::black, 1));
            painter.drawRect(QRectF(x - nodeHalf(), y - nodeHalf(), 2 * nodeHalf(), 2 * nodeHalf()));
        }
    }
}

void Viewport3D::drawMoveRubber(QPainter& painter) const {
    if (!m_movingNode || !m_structure) return;
    const core::Node* chosen = m_structure->findNode(*m_movingNode);
    if (!chosen) return;
    const core::ScreenMapper map = mapper();
    const Vec3 target = cursorWorld();
    double tx, ty;
    if (!map.toScreen(target, tx, ty)) return;
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(Qt::black, 1.5));  // the bars that reach the node follow the cursor in black
    for (int barId : m_structure->barsUsingNode(*m_movingNode)) {
        const core::Bar* bar = m_structure->findBar(barId);
        const core::Node* far = m_structure->findNode(bar->startNode == *m_movingNode ? bar->endNode : bar->startNode);
        double fx, fy;
        if (far && map.toScreen(far->position, fx, fy)) painter.drawLine(QPointF(fx, fy), QPointF(tx, ty));
    }
    // Orange dimension "<length> m < <angle>°": horizontal distance to the original node and angle from +X, rotated along the line.
    double ox, oy;
    if (!map.toScreen(chosen->position, ox, oy)) return;
    const double dx = target.x - chosen->position.x, dy = target.y - chosen->position.y;
    const double length = std::hypot(dx, dy);
    if (length < 1e-9) return;
    double angle = std::atan2(dy, dx) * 180.0 / 3.14159265358979323846;
    if (angle < 0.0) angle += 360.0;
    const QString text = QStringLiteral("%1 m < %2°").arg(length, 0, 'f', 4).arg(angle, 0, 'f', 2);
    const QPointF from(ox, oy), to(tx, ty);
    const double screenLength = std::hypot(tx - ox, ty - oy);
    if (screenLength < 20.0) return;
    painter.setPen(QPen(kHighlight, 1, Qt::DashLine));
    painter.drawLine(from, to);
    painter.save();
    painter.translate((from + to) / 2.0);
    double rotation = std::atan2(ty - oy, tx - ox) * 180.0 / 3.14159265358979323846;
    if (rotation > 90.0) rotation -= 180.0;  // keep the text upright
    if (rotation < -90.0) rotation += 180.0;
    painter.rotate(rotation);
    QFont font = painter.font();
    font.setPixelSize(13);
    painter.setFont(font);
    painter.setPen(kHighlight);
    painter.drawText(QRectF(-90, -22, 180, 18), Qt::AlignCenter, text);
    painter.restore();
}

void Viewport3D::drawNodeTags(QPainter& painter) const {
    // Nudos > Nuevo (nudos.md section 8, n5_on_bar.png): "N<id>" in bold green, its left edge 33 px right of the node and its
    // middle 7 px above it.
    if (!m_nodeLabels || !m_structure || !m_categories.nodesDrawn()) return;
    QFont font = painter.font();
    font.setPixelSize(13);
    font.setBold(true);
    painter.setFont(font);
    const QColor green(QStringLiteral("#008000"));
    // Ligaduras (ligaduras.md section 7): a tied node is written "N1: Dx(1)" in teal (measured RGB 0, 160, 176).
    const QColor teal(0, 160, 176);
    painter.setPen(green);
    const core::ScreenMapper map = mapper();
    const QFontMetrics metrics(font);
    for (const core::Node& n : m_structure->nodes) {
        if (m_movingNode && n.id == *m_movingNode) continue;
        double x, y;
        if (!visibleAfterClipping({n.position.x, n.position.y, n.position.z}) || !map.toScreen(n.position, x, y)) continue;
        QString text = n.reference.empty() ? QStringLiteral("N%1").arg(n.id) : QString::fromStdString(n.reference);
        painter.setPen(green);
        if (m_ligatureLabels && m_nodeLigatures) {
            const std::string tie = core::ligatureLabel(*m_nodeLigatures, n.id);
            if (!tie.empty()) {
                text += QStringLiteral(": ") + QString::fromStdString(tie);
                painter.setPen(teal);
            }
        }
        if (m_diaphragmLabels && m_nodeDiaphragms) {
            const std::string reference = core::diaphragmNodeLabel(*m_nodeDiaphragms, n.id);
            if (!reference.empty()) text += QStringLiteral(": ") + QString::fromStdString(reference);
        }
        painter.drawText(QRectF(x + 33, y - 7 - metrics.height() / 2.0, metrics.horizontalAdvance(text) + 2, metrics.height()),
                         Qt::AlignLeft | Qt::AlignVCenter, text);
    }
}

void Viewport3D::drawBarEnds(QPainter& painter) const {
    if (!m_structure || !m_categories.barsDrawn()) return;
    const bool pinnedSymbols = m_barEnds && !m_barEnds->empty();
    if (!pinnedSymbols && !m_endFixityLabels && !m_barAxes) return;
    const core::ScreenMapper map = mapper();
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);
    QFont font = painter.font();
    font.setPixelSize(13);
    font.setBold(true);
    const QFont constantFont = font;
    const QFontMetrics constantMetrics(font);
    for (const core::Bar& bar : m_structure->bars) {
        const core::Node* a = m_structure->findNode(bar.startNode);
        const core::Node* b = m_structure->findNode(bar.endNode);
        double ax, ay, bx, by;
        if (!a || !b || !map.toScreen(a->position, ax, ay) || !map.toScreen(b->position, bx, by)) continue;
        if (!visibleAfterClipping({a->position.x, a->position.y, a->position.z}) || !visibleAfterClipping({b->position.x, b->position.y, b->position.z})) continue;
        const QPointF pa(ax, ay), pb(bx, by);
        const double length = std::hypot(bx - ax, by - ay);
        if (length < 1e-6) continue;
        const QPointF along = (pb - pa) / length;
        const core::BarEndsRecord record = m_barEnds ? core::barEndsOf(*m_barEnds, bar.id) : core::BarEndsRecord{};
        for (int end = 0; end < 2; ++end) {
            const QPointF at = end == 0 ? pa : pb;
            const QPointF inward = end == 0 ? along : -along;
            const core::BarEndFixity& fixity = record.ends[static_cast<std::size_t>(end)];
            if (fixity.pinned) {  // navy filled circle (11,11,144) (measured: orig_artic_40, articulaciones.md 9.1)
                // Diameter = side of the node square (scales with the zoom); its centre lies on the bar axis 0.87 x side inwards.
                painter.setPen(Qt::NoPen);
                painter.setBrush(QColor(11, 11, 144));
                painter.drawEllipse(at + inward * (0.87 * nodeSquarePixels()), nodeHalf(), nodeHalf());
            }
            const bool springEnd = std::any_of(fixity.planes.begin(), fixity.planes.end(),
                                               [](const core::EndPlaneFixity& f) { return f.kind == core::EndFixityKind::Rotational; });
            if (springEnd) {  // magenta (255,0,255) 1 px outline of a node-sized square (measured: orig_artic_45, articulaciones.md 9.3)
                // The original shows a hollow square in the plane normal to the bar (a rhombus on a vertical bar in the isometric view),
                // centred on the bar axis 0.87 x side inwards. PROVISIONAL: the in-plane orientation of the square is not measured.
                const core::Point3& p3 = end == 0 ? a->position : b->position;
                const core::Point3& q3 = end == 0 ? b->position : a->position;
                const core::Point3 axis = core::geom::unit(core::geom::sub(q3, p3));
                core::Point3 up{0, 0, 1};
                if (std::abs(axis.z) > 0.999) up = {0, 1, 0};
                const core::Point3 u = core::geom::unit(core::geom::cross(up, axis));
                const core::Point3 v = core::geom::cross(axis, u);
                const double worldLength = core::geom::length(core::geom::sub(b->position, a->position));
                const double pxPerMetre = worldLength > 1e-9 ? length / worldLength : 0.0;
                if (pxPerMetre > 0.0) {
                    const double half = nodeHalf() / pxPerMetre;  // world half side
                    const double shift = 0.87 * nodeSquarePixels() / pxPerMetre;
                    QPolygonF square;
                    const double sx[4] = {-1, 1, 1, -1}, sy[4] = {-1, -1, 1, 1};
                    bool ok = true;
                    for (int i = 0; i < 4 && ok; ++i) {
                        const core::Point3 w{p3.x + axis.x * shift + (u.x * sx[i] + v.x * sy[i]) * half,
                                             p3.y + axis.y * shift + (u.y * sx[i] + v.y * sy[i]) * half,
                                             p3.z + axis.z * shift + (u.z * sx[i] + v.z * sy[i]) * half};
                        double px, py;
                        ok = map.toScreen(w, px, py);
                        square << QPointF(px, py);
                    }
                    if (ok) {
                        painter.setPen(QPen(QColor(255, 0, 255), 1));
                        painter.setBrush(Qt::NoBrush);
                        painter.drawPolygon(square);
                    }
                }
            }
            if (m_endFixityLabels) {  // green (0,128,0) bold lines "xy: 1.000", constant screen size (measured: orig_artic_41, 9.2)
                // One line per plane ("xy: 1.000", "xz: 0.00 kN·m/rad"): split before each plane tag.
                const QStringList lines = QString::fromStdString(core::endFixityLabel(fixity))
                                              .split(QRegularExpression(QStringLiteral("\\s(?=(?:xy|xz|yz):)")), Qt::SkipEmptyParts);
                constexpr double kPi = 3.14159265358979323846;
                double angle = std::atan2(inward.y(), inward.x()) * 180.0 / kPi;
                if (angle > 90.0 - 1e-6) angle -= 180.0;  // vertical text reads upwards, as in the original (orig_artic_41)
                if (angle < -90.0) angle += 180.0;
                const double rad = angle * kPi / 180.0;
                const QPointF textX(std::cos(rad), std::sin(rad)), textY(-std::sin(rad), std::cos(rad));
                // The block starts 0.4 m from the end (world distance, so it scales with the zoom) and sits to the screen-left of the bar.
                const core::Point3 pa3 = a->position, pb3 = b->position;
                const double worldLength = core::geom::length(core::geom::sub(pb3, pa3));
                const double startPx = worldLength > 1e-9 ? 0.4 * length / worldLength : 0.0;
                QPointF normal(-inward.y(), inward.x());
                if (normal.x() > 1e-9 || (std::abs(normal.x()) <= 1e-9 && normal.y() > 0)) normal = -normal;  // screen-left
                const double sideY = QPointF::dotProduct(normal, textY);  // +: the block hangs below the text baseline axis
                const bool flipped = QPointF::dotProduct(inward, textX) < 0;
                constexpr double kRow = 15.0;
                painter.save();
                painter.translate(at + inward * startPx + normal * 2.0);
                painter.rotate(angle);
                painter.setFont(constantFont);
                painter.setPen(QColor(0, 128, 0));
                painter.setBrush(Qt::NoBrush);
                double widest = 0;
                for (const QString& line : lines) widest = std::max(widest, static_cast<double>(constantMetrics.horizontalAdvance(line)));
                const double x0 = flipped ? -widest : 0.0;
                const double top = sideY > 0 ? 0.0 : -kRow * lines.size();
                for (int i = 0; i < lines.size(); ++i)
                    painter.drawText(QRectF(x0, top + i * kRow, widest + 2.0, kRow), Qt::AlignLeft | Qt::AlignVCenter, lines[i]);
                painter.restore();
            }
        }
        if (m_barAxes) {
            // Local axes at the end of the bar (orig_artic_01): x red along the bar, y green, z blue. PROVISIONAL: z is the global Z
            // projected on the plane normal to the bar (the global Y for a vertical bar), y = z x x.
            const core::Point3 axisX = core::geom::unit(core::geom::sub(b->position, a->position));
            core::Point3 up{0, 0, 1};
            if (std::abs(axisX.z) > 0.999) up = {0, 1, 0};
            const core::Point3 axisY = core::geom::unit(core::geom::cross(up, axisX));
            const core::Point3 axisZ = core::geom::cross(axisX, axisY);
            const core::Point3 anchor = b->position;
            const QPointF base = pb - along * 30.0;
            double bx0, by0;
            const core::Point3 dirs[3] = {axisX, axisY, axisZ};
            const QColor colours[3] = {QColor(230, 0, 0), QColor(0, 190, 0), QColor(0, 0, 230)};
            if (map.toScreen(anchor, bx0, by0)) {
                for (int i = 0; i < 3; ++i) {
                    double tx, ty;
                    const core::Point3 tip{anchor.x + dirs[i].x, anchor.y + dirs[i].y, anchor.z + dirs[i].z};
                    if (!map.toScreen(tip, tx, ty)) continue;
                    QPointF d(tx - bx0, ty - by0);
                    const double len = std::hypot(d.x(), d.y());
                    if (len < 1e-6) continue;
                    d /= len;
                    const QPointF to = base + d * 22.0;
                    painter.setPen(QPen(colours[i], 1.5));
                    painter.drawLine(base, to);
                    const QPointF side(-d.y(), d.x());
                    painter.drawLine(to, to - d * 6.0 + side * 3.0);
                    painter.drawLine(to, to - d * 6.0 - side * 3.0);
                }
            }
        }
    }
    painter.restore();
}

double Viewport3D::labelRowWorld() const {
    double diagonal = 0.0;
    if (m_structure) {
        bool any = false;
        core::Point3 lo{0, 0, 0}, hi{0, 0, 0};
        for (const core::Node& n : m_structure->nodes) {
            if (!any) lo = hi = {n.position.x, n.position.y, n.position.z};
            any = true;
            lo = {std::min(lo.x, n.position.x), std::min(lo.y, n.position.y), std::min(lo.z, n.position.z)};
            hi = {std::max(hi.x, n.position.x), std::max(hi.y, n.position.y), std::max(hi.z, n.position.z)};
        }
        diagonal = any ? core::geom::length(core::geom::sub(hi, lo)) : 0.0;
    }
    return 0.0072 * diagonal * m_textSizePercent / 100.0;
}

void Viewport3D::setDeflection(const core::DeflectionData* source) {
    m_deflection = source;
    m_deflectionCacheValid = false;
    update();
}

void Viewport3D::setDeflectionLabelsVisible(bool on) {
    if (m_deflectionLabels == on) return;
    m_deflectionLabels = on;
    update();
}

const std::vector<core::DeflectionGroupInfo>& Viewport3D::deflectionList() const {
    if (!m_deflectionCacheValid) {
        m_deflectionCache = m_deflection && m_structure ? core::deflectionGroups(*m_deflection, *m_structure) : std::vector<core::DeflectionGroupInfo>{};
        m_deflectionCacheValid = true;
    }
    return m_deflectionCache;
}

void Viewport3D::drawDeflectionLabels(QPainter& painter) const {
    // Same look as the buckling labels (green bold, parallel to the bar, world size): one label per bar, the groups of several bars repeat it
    // on every bar (grupos_flecha.md section 6, gf_10b_c / gf_22_edit_dlg).
    if (!m_deflectionLabels || !m_deflection || !m_structure || !m_categories.barsDrawn()) return;
    const core::ScreenMapper map = mapper();
    const double rowWorld = labelRowWorld();
    const std::vector<core::DeflectionGroupInfo>& list = deflectionList();
    painter.save();
    painter.setPen(QColor(0, 128, 0));
    for (const core::DeflectionGroupInfo& group : list) {
        const QString text = QString::fromStdString(core::deflectionGroupLabel(group));
        for (int barId : group.bars) {
            const core::Bar* bar = m_structure->findBar(barId);
            if (!bar || !m_structure->elementVisible(bar->layerIds)) continue;
            const core::Node* a = m_structure->findNode(bar->startNode);
            const core::Node* b = m_structure->findNode(bar->endNode);
            double ax, ay, bx, by;
            if (!a || !b || !map.toScreen(a->position, ax, ay) || !map.toScreen(b->position, bx, by)) continue;
            const core::Vec3 mid{(a->position.x + b->position.x) / 2.0, (a->position.y + b->position.y) / 2.0, (a->position.z + b->position.z) / 2.0};
            double x, y;
            if (!visibleAfterClipping({mid.x, mid.y, mid.z}) || !map.toScreen(mid, x, y)) continue;
            const double worldLength = core::geom::length(core::geom::sub(b->position, a->position));
            const double screenLength = std::hypot(bx - ax, by - ay);
            const double kRow = worldLength > 1e-9 ? rowWorld * screenLength / worldLength : 0.0;
            if (kRow < 3.0) continue;
            QFont font = painter.font();
            font.setPixelSize(std::max(1, static_cast<int>(std::lround(kRow * 0.88))));
            font.setBold(true);
            painter.setFont(font);
            const double widest = QFontMetrics(font).horizontalAdvance(text);
            if (screenLength < widest + 4.0) continue;  // short bars show no label
            double angle = std::atan2(by - ay, bx - ax) * 180.0 / 3.14159265358979323846;
            if (angle > 90.0) angle -= 180.0;
            if (angle < -90.0) angle += 180.0;
            painter.save();
            painter.translate(x, y);
            painter.rotate(angle);
            painter.drawText(QRectF(-widest / 2.0 - 2.0, -kRow * 0.3 - kRow, widest + 4.0, kRow), Qt::AlignCenter, text);
            painter.restore();
        }
    }
    painter.restore();
}

void Viewport3D::drawDeflectionGroup(QPainter& painter, int barId, bool selected) const {
    // The whole group of the bar in orange and the nodes 1 / 2 its tangent types refer to ("N1" / "N2", orange bold).
    if (!m_deflectionLabels || !m_structure) return;
    const std::vector<core::DeflectionGroupInfo>& list = deflectionList();
    const int index = core::deflectionGroupIndexOfBar(list, barId);
    if (index < 0) return;
    const core::DeflectionGroupInfo& group = list[static_cast<std::size_t>(index)];
    const core::ScreenMapper map = mapper();
    painter.save();
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(kHighlight, selected ? 3.0 : 2.0));
    for (int id : group.bars) {
        if (id == barId) continue;  // drawn by the generic highlight
        const core::Bar* bar = m_structure->findBar(id);
        const core::Node* a = bar ? m_structure->findNode(bar->startNode) : nullptr;
        const core::Node* b = bar ? m_structure->findNode(bar->endNode) : nullptr;
        double ax, ay, bx, by;
        if (a && b && map.toScreen(a->position, ax, ay) && map.toScreen(b->position, bx, by)) painter.drawLine(QPointF(ax, ay), QPointF(bx, by));
    }
    QFont font = painter.font();
    font.setPixelSize(12);
    font.setBold(true);
    painter.setFont(font);
    QColor text = kHighlight;
    text.setAlpha(190);
    painter.setPen(text);
    const std::pair<int, const char*> ends[2] = {{group.node1, "N1"}, {group.node2, "N2"}};
    for (const auto& [nodeId, name] : ends) {
        const core::Node* n = m_structure->findNode(nodeId);
        double x, y;
        if (n && map.toScreen(n->position, x, y)) painter.drawText(QRectF(x - 6.0, y - 24.0, 28.0, 16.0), Qt::AlignLeft | Qt::AlignVCenter, QString::fromLatin1(name));
    }
    painter.restore();
}

void Viewport3D::drawShellBands(QPainter& painter) const {
    if (!m_shellBands || !m_structure || !m_categories.shellsDrawn()) return;
    const core::ScreenMapper map = mapper();
    painter.save();
    for (const core::Shell& shell : m_structure->shells) {
        if (!m_structure->elementVisible(shell.layerIds)) continue;
        const auto record = m_shellBands->shells.find(shell.id);
        if (record == m_shellBands->shells.end() || record->second.bands.empty()) continue;
        const std::optional<core::ShellFrame> frame = core::shellFrame(*m_structure, shell);
        if (!frame) continue;
        const Vec3 zAxis{frame->axisZ.x, frame->axisZ.y, frame->axisZ.z};
        for (const core::ShellIntegrationBand& band : record->second.bands) {
            const core::Point3 a = frame->toWorld({band.x1, band.y1}), b = frame->toWorld({band.x2, band.y2});
            const Vec3 va{a.x, a.y, a.z}, vb{b.x, b.y, b.z};
            double ax, ay, bx, by;
            if (!map.toScreen(va, ax, ay) || !map.toScreen(vb, bx, by)) continue;
            painter.setPen(QPen(Qt::black, 1.0));
            painter.drawLine(QPointF(ax, ay), QPointF(bx, by));
            // Triad at 80 % of the length: X along the band, Z = z of the shell, Y = Z x X.
            const double length = std::hypot(band.x2 - band.x1, band.y2 - band.y1);
            if (length < 1e-9) continue;
            const Vec3 xAxis = (vb - va) * (1.0 / length);
            const Vec3 yAxis{zAxis.y * xAxis.z - zAxis.z * xAxis.y, zAxis.z * xAxis.x - zAxis.x * xAxis.z, zAxis.x * xAxis.y - zAxis.y * xAxis.x};
            const Vec3 origin = va + (vb - va) * 0.8;
            double ox, oy;
            if (!map.toScreen(origin, ox, oy)) continue;
            const Vec3 dirs[3] = {xAxis, yAxis, zAxis};
            const QColor colours[3] = {QColor(220, 30, 30), QColor(30, 170, 30), QColor(30, 60, 220)};
            for (int i = 0; i < 3; ++i) {
                double tx, ty;
                if (!map.toScreen(origin + dirs[i] * 0.1, tx, ty)) continue;
                const double reach = std::hypot(tx - ox, ty - oy);
                const double scale = reach > 1e-6 ? 22.0 / reach : 0.0;  // fixed screen size, like the axes of the viewport
                const QPointF tip(ox + (tx - ox) * scale, oy + (ty - oy) * scale);
                painter.setPen(QPen(colours[i], 1.2));
                painter.drawLine(QPointF(ox, oy), tip);
                if (reach > 1e-6) {
                    const QPointF u((tip.x() - ox) / 22.0, (tip.y() - oy) / 22.0), n(-u.y(), u.x());
                    painter.drawLine(tip, tip - u * 5.0 + n * 2.5);
                    painter.drawLine(tip, tip - u * 5.0 - n * 2.5);
                }
            }
        }
    }
    painter.restore();
}

void Viewport3D::drawBucklingLabels(QPainter& painter) const {
    // Green (0,128,0) bold text parallel to the bar, above its middle point, no background; bars too short for the text get none
    // (measured: orig_pandeo_16 / 28, pandeo.md section 9.5). The text has a WORLD size (measured: it grows and shrinks with the zoom, doubles at
    // "Tamaño de los textos" 200 % and halves at 50 %): the row spacing is 0.0072 of the diagonal of the structure (calibrated on the original at 100 %:
    // 10 px per row on a 41.5 m wide frame at 33.8 px/m; approximate, pandeo.md 9.7) times the percentage.
    if (m_bucklingLabels == BucklingLabels::None || !m_buckling || !m_structure || !m_categories.barsDrawn()) return;
    const core::ScreenMapper map = mapper();
    const double rowWorld = labelRowWorld();
    painter.save();
    painter.setPen(QColor(0, 128, 0));
    for (const core::Bar& bar : m_structure->bars) {
        if (!m_structure->elementVisible(bar.layerIds)) continue;
        const core::Node* a = m_structure->findNode(bar.startNode);
        const core::Node* b = m_structure->findNode(bar.endNode);
        double ax, ay, bx, by;
        if (!a || !b || !map.toScreen(a->position, ax, ay) || !map.toScreen(b->position, bx, by)) continue;
        const core::Vec3 mid{(a->position.x + b->position.x) / 2.0, (a->position.y + b->position.y) / 2.0, (a->position.z + b->position.z) / 2.0};
        double x, y;
        if (!visibleAfterClipping({mid.x, mid.y, mid.z}) || !map.toScreen(mid, x, y)) continue;
        const double worldLength = core::geom::length(core::geom::sub(b->position, a->position));
        const double screenLength = std::hypot(bx - ax, by - ay);
        const double kRow = worldLength > 1e-9 ? rowWorld * screenLength / worldLength : 0.0;  // pixels per row at this bar
        if (kRow < 3.0) continue;                                                              // too small to read: nothing is drawn
        QFont font = painter.font();
        font.setPixelSize(std::max(1, static_cast<int>(std::lround(kRow * 0.88))));
        font.setBold(true);
        painter.setFont(font);
        const QFontMetrics metrics(font);
        const core::BarBuckling record = core::bucklingOf(*m_buckling, bar.id);
        const std::vector<std::string> raw = m_bucklingLabels == BucklingLabels::Beta ? core::bucklingLabelLines(record, worldLength)
                                                                                     : core::lateralLabelLines(record, worldLength);
        QStringList lines;
        double widest = 0;
        for (const std::string& line : raw) {
            lines << QString::fromStdString(line);
            widest = std::max(widest, static_cast<double>(metrics.horizontalAdvance(lines.back())));
        }
        if (screenLength < widest + 4.0) continue;  // short bars show no label
        double angle = std::atan2(by - ay, bx - ax) * 180.0 / 3.14159265358979323846;
        if (angle > 90.0) angle -= 180.0;
        if (angle < -90.0) angle += 180.0;
        painter.save();
        painter.translate(x, y);
        painter.rotate(angle);
        for (int i = 0; i < lines.size(); ++i)
            painter.drawText(QRectF(-widest / 2.0 - 2.0, -kRow * 0.3 - (lines.size() - i) * kRow, widest + 4.0, kRow), Qt::AlignCenter, lines[i]);
        painter.restore();
    }
    painter.restore();
}

void Viewport3D::drawBarTags(QPainter& painter) const {
    // Label of a described bar (describir_barras.md section 1): vertical text in olive green. PROVISIONAL: colour, size and offset were
    // not measured on a screenshot; the placement is the middle of the bar, the text running upwards along the screen's vertical.
    if (m_barLabels.empty() || !m_structure) return;
    QFont font = painter.font();
    font.setPixelSize(12);
    painter.setFont(font);
    painter.setPen(QColor(QStringLiteral("#6B7F1C")));
    const core::ScreenMapper map = mapper();
    for (const auto& [id, text] : m_barLabels) {
        const core::Bar* bar = m_structure->findBar(id);
        if (!bar || text.isEmpty()) continue;
        const core::Node* a = m_structure->findNode(bar->startNode);
        const core::Node* b = m_structure->findNode(bar->endNode);
        if (!a || !b) continue;
        const core::Vec3 mid{(a->position.x + b->position.x) / 2.0, (a->position.y + b->position.y) / 2.0,
                             (a->position.z + b->position.z) / 2.0};
        double x, y, ax, ay, bx, by;
        if (!visibleAfterClipping({mid.x, mid.y, mid.z}) || !map.toScreen(mid, x, y)) continue;
        // The text runs along the bar (measured: "Diámetro 30" of a circular pillar, describir_barras.md section 24.4); read upwards /
        // left to right, so a vertical bar keeps the vertical text.
        double angle = -90.0;
        if (map.toScreen(a->position, ax, ay) && map.toScreen(b->position, bx, by) && (ax != bx || ay != by)) {
            angle = std::atan2(by - ay, bx - ax) * 180.0 / 3.14159265358979323846;
            if (angle >= 90.0) angle -= 180.0;
            if (angle < -90.0) angle += 180.0;
        }
        painter.save();
        painter.translate(x - 5, y);
        painter.rotate(angle);
        painter.drawText(QRectF(-100, -8, 200, 16), Qt::AlignCenter, text);
        painter.restore();
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
            if (n && map.toScreen(n->position, x, y)) painter.drawRect(QRectF(x - nodeHalf(), y - nodeHalf(), 2 * nodeHalf(), 2 * nodeHalf()));
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

void Viewport3D::drawShellEdges(QPainter& painter) const {
    if (!m_edgeMode || !m_structure) return;
    const core::ScreenMapper map = mapper();
    const auto draw = [&](const core::ShellEdgeKey& key, const QPen& pen) {
        const core::Node* a = m_structure->findNode(key.nodeA);
        const core::Node* b = m_structure->findNode(key.nodeB);
        double ax, ay, bx, by;
        if (!a || !b || !map.toScreen(a->position, ax, ay) || !map.toScreen(b->position, bx, by)) return;
        painter.setPen(pen);
        painter.drawLine(QPointF(ax, ay), QPointF(bx, by));
    };
    for (const core::ShellEdgeKey& key : m_edgeSelection) draw(key, QPen(kHighlight, 3.0));  // measured: orange when selected
    if (m_edgeHover && std::find(m_edgeSelection.begin(), m_edgeSelection.end(), *m_edgeHover) == m_edgeSelection.end())
        draw(*m_edgeHover, QPen(QColor(QStringLiteral("#C8B68E")), 3.0));  // beige under the cursor
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
    if (!m_structure) return;
    const core::Node* n = nullptr;
    if (m_hover && m_hover->kind == core::ElementKind::Node) n = m_structure->findNode(m_hover->id);
    else if (m_selection.size() == 1 && m_selection.count(core::ElementKind::Node) == 1) n = m_structure->findNode(m_selection.ids(core::ElementKind::Node).front());
    if (!n) return;
    const auto fmt = [](double v) { return QString::number(v, 'g', 6).replace('.', ','); };
    const QString text =
        QStringLiteral("N%1 (%2, %3, %4)").arg(n->id).arg(fmt(n->position.x), fmt(n->position.y), fmt(n->position.z));
    QFont font = painter.font();
    font.setPixelSize(13);
    painter.setFont(font);
    // Ligaduras: a second, regular-weight line "Dx(1)" under the title (orig_lig_rotulo_dx1_tooltip.png); Diafragmas: "Diafragma : D1"
    // (orig_diaf_23_tooltip_nudo.png).
    QStringList extras;
    if (m_ligatureLabels && m_nodeLigatures) {
        const QString tie = QString::fromStdString(core::ligatureLabel(*m_nodeLigatures, n->id));
        if (!tie.isEmpty()) extras << tie;
    }
    if (m_diaphragmLabels && m_nodeDiaphragms) {
        const QString line = QString::fromStdString(core::diaphragmTooltipLine(*m_nodeDiaphragms, n->id));
        if (!line.isEmpty()) extras << line;
    }
    double textWidth = painter.fontMetrics().horizontalAdvance(text);
    for (const QString& extra : extras) textWidth = std::max(textWidth, static_cast<double>(painter.fontMetrics().horizontalAdvance(extra)));
    const QSizeF size(textWidth + 16, extras.isEmpty() ? 28 : 28 + 26 * extras.size());
    double nx = 0, ny = 0;
    mapper().toScreen(n->position, nx, ny);
    const QRectF box = anchoredRect(size, QSizeF(width(), height()), m_tooltipPosition, {nx, ny});  // "Posición de los tooltips"
    // Original (c4_borrar_sel.png): white box with a dark border and bold black text.
    painter.setBrush(Qt::white);
    painter.setPen(QPen(QColor(30, 30, 30), 1));
    painter.drawRect(box);
    font.setBold(true);
    painter.setFont(font);
    painter.drawText(extras.isEmpty() ? box : QRectF(box.left(), box.top(), box.width(), 28), Qt::AlignCenter, text);
    font.setBold(false);
    for (int i = 0; i < extras.size(); ++i) {
        const double top = box.top() + 28 + 26 * i;
        painter.setFont(font);
        painter.drawLine(QPointF(box.left(), top), QPointF(box.right(), top));
        painter.drawText(QRectF(box.left(), top, box.width(), 26), Qt::AlignCenter, extras[i]);
    }
    painter.setBrush(Qt::NoBrush);
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
    // "Referencias" (planos.md 12.2): the master switch, the direction filters, "only with a node" and the colour by direction.
    const core::ReferenceOptions refs = m_planes ? m_planes->references : core::ReferenceOptions{};
    if (!refs.referenceLines) return;
    const bool wanted[3] = {refs.parallelX, refs.parallelY, refs.parallelZ};
    const char* directionColours[3] = {"#E00000", "#00B000", "#0000E0"};  // the colours of the axes triad
    const auto usePen = [&](int direction) {
        QPen pen(QColor(QLatin1String(refs.colourByDirection ? directionColours[direction] : "#D98A4A")), 1);
        pen.setStyle(Qt::DotLine);
        painter.setPen(pen);
    };
    std::vector<Vec3> nodes = m_nodes;
    if (m_movingNode && m_structure)
        if (const core::Node* moving = m_structure->findNode(*m_movingNode)) {
            const Vec3 old{moving->position.x, moving->position.y, moving->position.z};
            const auto it = std::find_if(nodes.begin(), nodes.end(), [&](const Vec3& v) { return v == old; });
            if (it != nodes.end()) nodes.erase(it);  // the old place of the node in motion no longer carries guide lines
        }
    if (nodes.empty()) {
        if (refs.onlyWithNode) return;  // the three lines through the origin have no node
        const Vec3 axes[3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
        for (int d = 0; d < 3; ++d) {
            if (!wanted[d]) continue;
            usePen(d);
            drawGuideLine(painter, axes[d] * -kGuideExtent, axes[d] * kGuideExtent);
        }
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
    if (wanted[1]) {
        usePen(1);
        for (const Vec3& n : nodes) drawGuideLine(painter, n + Vec3{0, -kGuideExtent, 0}, n + Vec3{0, kGuideExtent, 0});
    }
    // X and Z: nodes that share the other two coordinates are joined by one segment, extended a few pixels at both ends.
    const auto key = [](double a, double b) { return std::make_pair(std::llround(a * 1e6), std::llround(b * 1e6)); };
    std::map<std::pair<long long, long long>, std::pair<Vec3, Vec3>> alongX, alongZ;  // line -> (lowest, highest) node
    for (const Vec3& n : nodes) {
        auto [ix, newX] = alongX.try_emplace(key(n.y, n.z), n, n);
        if (!newX) { if (n.x < ix->second.first.x) ix->second.first = n; if (n.x > ix->second.second.x) ix->second.second = n; }
        auto [iz, newZ] = alongZ.try_emplace(key(n.x, n.y), n, n);
        if (!newZ) { if (n.z < iz->second.first.z) iz->second.first = n; if (n.z > iz->second.second.z) iz->second.second = n; }
    }
    if (wanted[0]) {
        usePen(0);
        for (const auto& [k, ends] : alongX)
            drawGuideLine(painter, ends.first - Vec3{stubAt(ends.first), 0, 0}, ends.second + Vec3{stubAt(ends.second), 0, 0});
    }
    if (wanted[2]) {
        usePen(2);
        for (const auto& [k, ends] : alongZ)
            drawGuideLine(painter, ends.first - Vec3{0, 0, stubAt(ends.first)}, ends.second + Vec3{0, 0, stubAt(ends.second)});
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

// ---------------------------------------------------------------------------------------------------------------------
// Work planes: grid lines and levels (Geometria > Planos)
namespace {
constexpr double kPlaneMargin = 2.0;          // metres the lines go beyond the outermost line of the other direction
constexpr double kDefaultPlaneHalfSpan = 5.0; // half length of a line when the other direction has no line yet

/// Interval covered by the coordinates of one direction, widened by the margin; a default span when there are none.
std::pair<double, double> spanOf(const std::vector<core::GridLine>& lines) {
    if (lines.empty()) return {-kDefaultPlaneHalfSpan, kDefaultPlaneHalfSpan};
    double lo = lines.front().coordinate, hi = lo;
    for (const core::GridLine& l : lines) {
        lo = std::min(lo, l.coordinate);
        hi = std::max(hi, l.coordinate);
    }
    return {lo - kPlaneMargin, hi + kPlaneMargin};
}
}  // namespace

void Viewport3D::setWorkPlanes(const core::WorkPlanes* planes) {
    m_planes = planes;
    update();
}

std::vector<Viewport3D::PlaneSegment> Viewport3D::workPlaneSegments() const {
    std::vector<PlaneSegment> out;
    if (!m_planes || !m_planes->references.showGrid) return out;
    const bool lines = m_planes->visible;
    const auto [y0, y1] = spanOf(m_planes->gridY);
    const auto [x0, x1] = spanOf(m_planes->gridX);
    using Kind = PlaneSegment::Kind;
    using LabelAt = PlaneSegment::LabelAt;
    // "Líneas en la dirección X" are drawn at x = coordinate, running along Y; the label sits at the "Origen" (lowest y)
    // or "Extremo" (highest y) end. Same for Y with the axes exchanged. (Orientation assumed: not verified in the original.)
    for (const core::GridLine& l : m_planes->gridX)
        out.push_back({Kind::GridX, {l.coordinate, y0, 0}, {l.coordinate, y1, 0}, QString::fromStdString(l.label),
                       l.labelPosition == core::LabelPosition::Origin ? LabelAt::A : LabelAt::B, lines});
    for (const core::GridLine& l : m_planes->gridY)
        out.push_back({Kind::GridY, {x0, l.coordinate, 0}, {x1, l.coordinate, 0}, QString::fromStdString(l.label),
                       l.labelPosition == core::LabelPosition::Origin ? LabelAt::A : LabelAt::B, lines});
    return out;
}

void Viewport3D::drawWorkPlanes(QPainter& painter) const {
    const std::vector<PlaneSegment> segments = workPlaneSegments();
    if (segments.empty()) return;
    const QColor magenta(QStringLiteral("#FF00FF"));  // measured in the original (planos.md 12.6)
    for (const PlaneSegment& s : segments) {
        if (s.lineVisible) {
            painter.setPen(QPen(QColor(QStringLiteral("#CC9966")), 1));  // grid and reference lines share this colour
            drawGuideLine(painter, s.a, s.b);
        }
        if (s.labelAt == PlaneSegment::LabelAt::None || s.label.isEmpty()) continue;
        const Vec3 anchor = s.labelAt == PlaneSegment::LabelAt::A ? s.a : s.b;
        const Vec3 other = s.labelAt == PlaneSegment::LabelAt::A ? s.b : s.a;
        QPointF pa, po;
        if (!worldToScreen(anchor, pa) || !worldToScreen(other, po)) continue;
        // Magenta ellipse with the label beyond the end of the line, joined to it by a segment (pl_h2.png).
        QPointF dir = pa - po;
        const double len = std::hypot(dir.x(), dir.y());
        dir = len > 1e-6 ? dir / len : QPointF(1, 0);
        const double rx = 18.0, ry = 12.0;
        const QPointF centre = pa + QPointF(dir.x() * rx * 1.6, dir.y() * ry * 1.6);
        painter.setPen(QPen(magenta, 1));
        painter.drawLine(pa, centre);
        painter.setBrush(Qt::white);
        painter.drawEllipse(centre, rx, ry);
        painter.setBrush(Qt::NoBrush);
        // The text of the original is turned along the projected line (pl_h2.png); kept upright-readable.
        double angle = std::atan2(po.y() - pa.y(), po.x() - pa.x()) * 180.0 / 3.14159265358979323846;
        if (angle > 90.0) angle -= 180.0;
        if (angle < -90.0) angle += 180.0;
        painter.save(); painter.translate(centre); painter.rotate(angle);
        painter.drawText(QRectF(-rx, -ry, 2 * rx, 2 * ry), Qt::AlignCenter, s.label);
        painter.restore();
    }
}

void Viewport3D::paintGL() {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.fillRect(rect(), Qt::white);
    if (!m_selectionMode) drawGuideLines(painter);  // the original hides the guide lines inside a command
    drawWorkPlanes(painter);
    drawStructure(painter);
    drawClipping(painter);
    drawShellBands(painter);
    drawNodes(painter);  // the original always shows the node squares (categorias_panel.md), not only in a command
    drawNodalLoads(painter);
    drawMoveRubber(painter);
    drawBarRubber(painter);
    drawBarEnds(painter);
    drawBucklingLabels(painter);
    drawDeflectionLabels(painter);
    drawNodeTags(painter);
    drawBarTags(painter);
    drawZoomMark(painter);
    if (m_selectionMode) {
        for (const core::ElementRef& e : m_selection.items()) {
            drawHighlight(painter, e, true);
            if (e.kind == core::ElementKind::Bar) drawDeflectionGroup(painter, e.id, true);
        }
        if (m_hover) {
            drawHighlight(painter, *m_hover, false);
            if (m_hover->kind == core::ElementKind::Bar) drawDeflectionGroup(painter, m_hover->id, false);
        }
        drawShellEdges(painter);
        drawRubberBand(painter);
        drawNodeLabel(painter);
    }
    if (m_snapMode) drawSnap(painter);
    drawInfoBox(painter);
    drawAxesTriad(painter);
    drawCoordinateTooltip(painter);
}

// ---------------------------------------------------------------------------------------------------------------------
// Sections X/Y/Z and clipping volume (TK-032)
namespace {
double axisCoord(const Vec3& p, int axis) { return axis == 0 ? p.x : (axis == 1 ? p.y : p.z); }
Vec3 withAxis(Vec3 p, int axis, double v) { (axis == 0 ? p.x : (axis == 1 ? p.y : p.z)) = v; return p; }
const QColor kClipRed(QStringLiteral("#D62020"));
constexpr double kPi = 3.14159265358979323846;
}  // namespace

std::pair<Vec3, Vec3> Viewport3D::clipEnvelope() const {
    auto [lo, hi] = core::envelope(m_nodes);
    for (int a = 0; a < 3; ++a) {  // margin of 10 % (at least 1 m) so that the plane overhangs the model
        const double m = std::max(1.0, 0.1 * (axisCoord(hi, a) - axisCoord(lo, a)));
        lo = withAxis(lo, a, axisCoord(lo, a) - m);
        hi = withAxis(hi, a, axisCoord(hi, a) + m);
    }
    return {lo, hi};
}

void Viewport3D::setSectionActive(int axis, bool on) {
    if (axis < 0 || axis > 2 || sectionActive(axis) == on) return;
    const auto [lo, hi] = clipEnvelope();
    const Vec3 centre = (lo + hi) * 0.5;
    m_clip.setSectionActive(axis, on, axisCoord(centre, axis), centre);
    emit sectionChanged(axis, on);
    update();
}

bool Viewport3D::flipSection(int axis) {
    if (!m_clip.flipSection(axis)) return false;
    update();
    return true;
}

void Viewport3D::setClippingVolumeActive(bool on) {
    if (m_clip.volume.active == on) return;
    const auto [lo, hi] = core::envelope(m_nodes);  // "a partir de la envolvente geométrica"
    m_clip.setVolumeActive(on, lo, hi);
    emit clippingVolumeChanged(on);
    update();
}

bool Viewport3D::visibleAfterClipping(const Vec3& p) const {
    for (const core::ClipHalfSpace& h : m_clip.halfSpaces())
        if (!h.contains(p)) return false;
    return true;
}

std::vector<Viewport3D::ClipHandle> Viewport3D::clipHandles() const {
    std::vector<ClipHandle> out;
    const auto [lo, hi] = clipEnvelope();
    const Vec3 centre = (lo + hi) * 0.5;
    for (int a = 0; a < 3; ++a) {
        const core::SectionPlane& s = m_clip.sections[static_cast<std::size_t>(a)];
        if (!s.active) continue;
        const double len = std::max(1.0, 0.25 * (axisCoord(hi, a) - axisCoord(lo, a)));
        const Vec3 base = m_clip.sectionPoint(a);
        out.push_back({false, a, s.arrowSign() > 0, base, base + m_clip.sectionNormal(a) * (len * s.arrowSign())});
    }
    if (m_clip.volume.active) {
        const Vec3 vc = (m_clip.volume.min + m_clip.volume.max) * 0.5;
        for (int a = 0; a < 3; ++a) {
            const double len = std::max(0.5, 0.15 * (axisCoord(m_clip.volume.max, a) - axisCoord(m_clip.volume.min, a)));
            for (bool maxFace : {false, true}) {
                const Vec3 base = m_clip.toWorld(withAxis(vc, a, axisCoord(maxFace ? m_clip.volume.max : m_clip.volume.min, a)));
                out.push_back({true, a, maxFace, base, base + m_clip.axisDirection(a) * (maxFace ? len : -len)});
            }
        }
    }
    (void)centre;
    return out;
}

std::optional<Viewport3D::ClipHandle> Viewport3D::clipHandleAt(const QPointF& pos, double tolerancePx) const {
    std::optional<ClipHandle> best;
    double bestDistance = tolerancePx;
    for (const ClipHandle& h : clipHandles()) {
        QPointF a, b;
        if (!worldToScreen(h.base, a) || !worldToScreen(h.tip, b)) continue;
        const QPointF d = b - a;
        const double len2 = d.x() * d.x() + d.y() * d.y();
        const double t = len2 > 0 ? std::clamp(((pos.x() - a.x()) * d.x() + (pos.y() - a.y()) * d.y()) / len2, 0.0, 1.0) : 0.0;
        const QPointF nearest = a + d * t;
        const double dist = std::hypot(pos.x() - nearest.x(), pos.y() - nearest.y());
        if (dist <= bestDistance) {
            bestDistance = dist;
            best = h;
        }
    }
    return best;
}

void Viewport3D::dragClipHandle(const ClipHandle& handle, const QPointF& from, const QPointF& to) {
    // A section moves along its own (turned) normal; the volume faces along the turned local axes.
    const Vec3 dir = handle.volume ? m_clip.axisDirection(handle.axis) : m_clip.sectionNormal(handle.axis);
    QPointF a, b;
    if (!worldToScreen(handle.base, a) || !worldToScreen(handle.base + dir, b)) return;
    const QPointF axisScreen = b - a;  // screen length of one metre along the axis
    const double len2 = axisScreen.x() * axisScreen.x() + axisScreen.y() * axisScreen.y();
    if (len2 < 1e-9) return;  // axis seen end-on: the drag direction is undefined
    const QPointF move = to - from;
    const double metres = (move.x() * axisScreen.x() + move.y() * axisScreen.y()) / len2;
    if (!handle.volume) {
        // The position is a world coordinate along the axis; a move of `metres` along the normal needs metres / n_axis
        // (n_axis >= cos 80 deg, so the division is safe).
        const double nAxis = axisCoord(dir, handle.axis);
        if (std::abs(nAxis) < 1e-6) return;
        m_clip.sections[static_cast<std::size_t>(handle.axis)].position += metres / nAxis;
    } else {
        const Vec3& edge = handle.faceMax ? m_clip.volume.max : m_clip.volume.min;
        m_clip.moveVolumeFace(handle.axis, handle.faceMax, axisCoord(edge, handle.axis) + metres);
    }
    update();
}

std::vector<Vec3> Viewport3D::clipArc(int id) const {
    // Circle in the plane of the turn (seen as an ellipse in the original, ar_*.png), three quarters of a turn, starting at
    // the current angle: X and Y sections turn about Z (horizontal circle), the Z section about Y (circle in XZ), the volume
    // about Z through its centre. Radii differ a little per arc so that arcs that start together can be told apart.
    std::vector<Vec3> arc;
    if (id < 0 || id > kVolumeArc) return arc;
    const auto [lo, hi] = clipEnvelope();
    Vec3 base, from, to;  // `from` goes towards `to` as the angle grows
    double angle, r;
    if (id == kVolumeArc) {
        if (!m_clip.volume.active) return arc;
        base = m_clip.centre;
        from = {1, 0, 0};
        to = {0, 1, 0};
        angle = m_clip.angleRad;
        r = 0.12 * std::max(hi.x - lo.x, hi.y - lo.y) * 1.6;
    } else {
        const core::SectionPlane& s = m_clip.sections[static_cast<std::size_t>(id)];
        if (!s.active) return arc;
        base = m_clip.sectionPoint(id);
        if (id == 2) {
            from = {0, 0, 1};
            to = {1, 0, 0};
            r = 0.12 * std::max(hi.x - lo.x, hi.z - lo.z);
        } else {
            from = {1, 0, 0};
            to = {0, 1, 0};
            r = 0.12 * std::max(hi.x - lo.x, hi.y - lo.y);
        }
        angle = s.angleRad;
        r *= 1.0 + 0.2 * id;
    }
    for (int k = 0; k <= 24; ++k) {
        const double ang = angle + (0.25 + 1.5 * k / 24.0) * kPi;
        arc.push_back(base + from * (r * std::cos(ang)) + to * (r * std::sin(ang)));
    }
    return arc;
}

std::vector<Vec3> Viewport3D::clipArc() const {
    for (int id = 0; id <= kVolumeArc; ++id)
        if (const auto arc = clipArc(id); !arc.empty()) return arc;
    return {};
}

int Viewport3D::clipArcIdAt(const QPointF& pos, double tolerancePx) const {
    if (!m_clip.anyActive() || !m_clipPlanesVisible) return -1;
    for (int id = 0; id <= kVolumeArc; ++id) {
        const std::vector<Vec3> arc = clipArc(id);
        for (std::size_t i = 0; i + 1 < arc.size(); ++i) {
            QPointF a, b;
            if (!worldToScreen(arc[i], a) || !worldToScreen(arc[i + 1], b)) continue;
            const QPointF d = b - a;
            const double len2 = d.x() * d.x() + d.y() * d.y();
            const double t = len2 > 0 ? std::clamp(((pos.x() - a.x()) * d.x() + (pos.y() - a.y()) * d.y()) / len2, 0.0, 1.0) : 0.0;
            const QPointF nearest = a + d * t;
            if (std::hypot(pos.x() - nearest.x(), pos.y() - nearest.y()) <= tolerancePx) return id;
        }
    }
    return -1;
}

bool Viewport3D::dragClipArc(int id, const QPointF& from, const QPointF& to) {
    // Law read from the original code (panelarea.dll, pnglslclip.c, 0x1800544a0/0x180054680): incremental,
    // only the dominant screen axis counts, delta = d * 2*pi / viewport size along that axis. This matches
    // the measured 0.48 deg/px vertical and 0.26 deg/px horizontal for a viewport of about 1385 x 750 px.
    if (id < 0 || id > kVolumeArc) return false;
    const QPointF move = to - from;
    if (std::hypot(move.x(), move.y()) <= 0.001 || width() <= 0 || height() <= 0) return false;
    const bool vertical = std::abs(move.y()) >= std::abs(move.x());
    const double delta = vertical ? move.y() * 2.0 * kPi / height() : move.x() * 2.0 * kPi / width();
    if (id == kVolumeArc) {
        if (!m_clip.volume.active) return false;
        m_clip.rotate(delta);
    } else if (!m_clip.rotateSection(id, delta)) {
        return false;  // out of +-80 degrees: the whole step is rejected
    }
    update();
    return true;
}

void Viewport3D::dragClipArc(const QPointF& from, const QPointF& to) {
    for (int id = 0; id <= kVolumeArc; ++id)
        if (!clipArc(id).empty()) { dragClipArc(id, from, to); return; }
}

void Viewport3D::drawClipping(QPainter& painter) const {
    if (!m_clip.anyActive() || !m_clipPlanesVisible) return;
    const core::ScreenMapper map = mapper();
    const auto poly = [&](const std::vector<Vec3>& pts, bool closed, const QPen& pen, const QBrush& brush) {
        QPolygonF p;
        for (const Vec3& w : pts) {
            double x, y;
            if (!map.toScreen(w, x, y)) return;
            p << QPointF(x, y);
        }
        painter.setPen(pen);
        painter.setBrush(brush);
        if (closed) painter.drawPolygon(p); else painter.drawPolyline(p);
    };
    const auto [lo, hi] = clipEnvelope();
    for (int a = 0; a < 3; ++a) {  // plane: outline spanning the envelope, turned with its own angle
        const core::SectionPlane& s = m_clip.sections[static_cast<std::size_t>(a)];
        if (!s.active) continue;
        const int u = (a + 1) % 3, v = (a + 2) % 3;
        const auto corner = [&](bool uMax, bool vMax) {
            return m_clip.sectionToWorld(a, withAxis(withAxis(withAxis({0, 0, 0}, a, s.position), u, axisCoord(uMax ? hi : lo, u)), v, axisCoord(vMax ? hi : lo, v)));
        };
        poly({corner(false, false), corner(true, false), corner(true, true), corner(false, true)}, true, QPen(QColor(60, 60, 60), 1), Qt::NoBrush);  // outline only (sp_1.png)
    }
    for (int id = 0; id <= kVolumeArc; ++id) poly(clipArc(id), false, QPen(kClipRed, 2), Qt::NoBrush);  // one arc per plane and one for the volume
    if (m_clip.volume.active) {  // box of six planes in wire frame
        const Vec3 mn = m_clip.volume.min, mx = m_clip.volume.max;
        const auto c = [&](int i) { return m_clip.toWorld(Vec3{(i & 1) ? mx.x : mn.x, (i & 2) ? mx.y : mn.y, (i & 4) ? mx.z : mn.z}); };
        const QPen pen(QColor(110, 110, 110), 1, Qt::DashLine);
        for (int i = 0; i < 8; ++i)
            for (int bit : {1, 2, 4})
                if (!(i & bit)) poly({c(i), c(i | bit)}, false, pen, Qt::NoBrush);
    }
    for (const ClipHandle& h : clipHandles()) {  // red arrows
        QPointF a, b;
        if (!worldToScreen(h.base, a) || !worldToScreen(h.tip, b)) continue;
        painter.setPen(QPen(kClipRed, 3, Qt::SolidLine, Qt::RoundCap));
        painter.drawLine(a, b);
        const QPointF d = b - a;
        const double len = std::hypot(d.x(), d.y());
        if (len < 1.0) continue;
        const QPointF dir = d / len, nrm(-dir.y(), dir.x());
        painter.setPen(Qt::NoPen);
        painter.setBrush(kClipRed);
        painter.drawPolygon(QPolygonF({b + dir * 8.0, b - dir * 6.0 + nrm * 6.0, b - dir * 6.0 - nrm * 6.0}));
    }
    painter.setBrush(Qt::NoBrush);
}

}  // namespace cc::app
