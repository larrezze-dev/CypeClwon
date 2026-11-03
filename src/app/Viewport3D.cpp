#include "Viewport3D.h"

#include <QFont>
#include <QFontMetrics>
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
    m_nodes.clear();
    if (m_structure)
        for (const core::Node& n : m_structure->nodes) m_nodes.push_back({n.position.x, n.position.y, n.position.z});
    if (m_hover && !elementExists(*m_hover)) m_hover.reset();
    std::vector<core::ElementRef> stale;
    for (const core::ElementRef& e : m_selection.items())
        if (!elementExists(e)) stale.push_back(e);
    if (!stale.empty() && m_selection.apply(stale, core::SelectMode::Remove)) emit selectionChanged();
    if (m_movingNode && !(m_structure && m_structure->findNode(*m_movingNode))) m_movingNode.reset();
    rebuildSnapModel();
    m_snap = {};
    update();
}

void Viewport3D::setNodeLabelsVisible(bool on) {
    if (m_nodeLabels == on) return;
    m_nodeLabels = on;
    update();
}

void Viewport3D::setMovingNode(std::optional<int> nodeId) {
    m_movingNode = nodeId;
    m_snap = {};
    rebuildSnapModel();
    update();
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
    if (m_structure && !m_windowStart) hover = core::pickAt(*m_structure, mapper(), effectiveFilter(), pos.x(), pos.y());
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
        changed = m_selection.apply(core::pickRect(*m_structure, mapper(), effectiveFilter(), a.x(), a.y(), pos.x(), pos.y()), mode);
    } else if (const auto hit = core::pickAt(*m_structure, mapper(), effectiveFilter(), pos.x(), pos.y())) {
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
    if (!id) return;
    const QStringList lines = barInfoLines(*m_structure, *id);
    if (lines.isEmpty()) return;
    QFont font = painter.font();
    font.setPixelSize(14);
    const QFont bold = [&] { QFont f = font; f.setBold(true); return f; }();
    constexpr double kRow = 26.0, kPad = 6.0;
    double w = 0;
    for (int i = 0; i < lines.size(); ++i) w = std::max(w, static_cast<double>(QFontMetrics(i == 0 ? bold : font).horizontalAdvance(lines[i])));
    const QSizeF size(w + 2 * kPad, kRow * lines.size());
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
    for (int i = 0; i < lines.size(); ++i) {
        const QRectF row(box.left(), box.top() + i * kRow, box.width(), kRow);
        if (i > 0) painter.drawLine(row.topLeft(), row.topRight());
        painter.setFont(i == 0 ? bold : font);
        painter.setPen(QColor(20, 20, 20));
        painter.drawText(row.adjusted(kPad, 0, -kPad, 0), Qt::AlignVCenter | Qt::AlignLeft, lines[i]);
        painter.setPen(QPen(QColor(190, 190, 190), 1));
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
    // Nudos > Mover: the node being moved is drawn at the cursor and the bars that reach it follow it.
    const Vec3 movedTo = m_movingNode ? cursorWorld() : Vec3{};
    const auto positionOf = [&](const core::Node& n) -> Vec3 {
        return m_movingNode && n.id == *m_movingNode ? movedTo : Vec3{n.position.x, n.position.y, n.position.z};
    };
    for (const core::Shell& shell : m_structure->shells) {
        if (!m_categories.shellsDrawn()) break;
        if (!m_structure->elementVisible(shell.layerIds)) continue;  // layer hidden with the eye
        QPolygonF poly;
        std::vector<Vec3> corners;
        for (int id : shell.nodeIds) {
            const core::Node* n = m_structure->findNode(id);
            if (n) corners.push_back(positionOf(*n));
        }
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

void Viewport3D::drawNodes(QPainter& painter) const {
    // Support symbols are not drawn yet; they follow the same rule as the nodes (CategorySettings::supportsDrawn).
    if (!m_structure || !m_categories.nodesDrawn()) return;
    const core::ScreenMapper map = mapper();
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(kNodeBlue, 1));
    for (const core::Node& n : m_structure->nodes) {
        double x, y;
        if (m_movingNode && n.id == *m_movingNode) continue;  // drawn at the cursor below
        if (!visibleAfterClipping({n.position.x, n.position.y, n.position.z})) continue;
        if (map.toScreen(n.position, x, y)) painter.drawRect(QRectF(x - nodeHalf(), y - nodeHalf(), 2 * nodeHalf(), 2 * nodeHalf()));
    }
    if (m_movingNode) {
        double x, y;
        painter.setPen(QPen(kHighlight, 1));  // the node in motion is orange like a selected one
        if (map.toScreen(cursorWorld(), x, y)) painter.drawRect(QRectF(x - nodeHalf(), y - nodeHalf(), 2 * nodeHalf(), 2 * nodeHalf()));
    }
}

void Viewport3D::drawNodeTags(QPainter& painter) const {
    // Nudos > Nuevo (nudos.md section 8, n5_on_bar.png): "N<id>" in bold green, its left edge 33 px right of the node and its
    // middle 7 px above it.
    if (!m_nodeLabels || !m_structure || !m_categories.nodesDrawn()) return;
    QFont font = painter.font();
    font.setPixelSize(13);
    font.setBold(true);
    painter.setFont(font);
    painter.setPen(QColor(QStringLiteral("#008000")));
    const core::ScreenMapper map = mapper();
    const QFontMetrics metrics(font);
    for (const core::Node& n : m_structure->nodes) {
        if (m_movingNode && n.id == *m_movingNode) continue;
        double x, y;
        if (!visibleAfterClipping({n.position.x, n.position.y, n.position.z}) || !map.toScreen(n.position, x, y)) continue;
        const QString text = QStringLiteral("N%1").arg(n.id);
        painter.drawText(QRectF(x + 33, y - 7 - metrics.height() / 2.0, metrics.horizontalAdvance(text) + 2, metrics.height()),
                         Qt::AlignLeft | Qt::AlignVCenter, text);
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
    const QSizeF size(painter.fontMetrics().horizontalAdvance(text) + 16, 28);
    double nx = 0, ny = 0;
    mapper().toScreen(n->position, nx, ny);
    const QRectF box = anchoredRect(size, QSizeF(width(), height()), m_tooltipPosition, {nx, ny});  // "Posición de los tooltips"
    // Original (c4_borrar_sel.png): white box with a dark border and bold black text.
    painter.setBrush(Qt::white);
    painter.setPen(QPen(QColor(30, 30, 30), 1));
    painter.drawRect(box);
    font.setBold(true);
    painter.setFont(font);
    painter.drawText(box, Qt::AlignCenter, text);
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
    drawNodes(painter);  // the original always shows the node squares (categorias_panel.md), not only in a command
    drawNodeTags(painter);
    drawZoomMark(painter);
    if (m_selectionMode) {
        for (const core::ElementRef& e : m_selection.items()) drawHighlight(painter, e, true);
        if (m_hover) drawHighlight(painter, *m_hover, false);
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
