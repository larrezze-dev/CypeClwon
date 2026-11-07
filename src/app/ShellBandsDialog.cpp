#include "ShellBandsDialog.h"

#include <QAction>
#include <QCheckBox>
#include <QCursor>
#include <QFileDialog>
#include <QFrame>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <algorithm>
#include <cmath>

#include "Icons.h"
#include "ShellPropertiesDialogs.h"
#include "UserMessages.h"

namespace cc::app {

using namespace cc::core;

namespace {

const QColor kGreen(0, 153, 0);
const QColor kMagenta(255, 0, 255);
const QColor kOrange(0xDA, 0x87, 0x35);
constexpr double kPickPx = 6.0;

std::function<int(QDialog&)>& runner() {
    static std::function<int(QDialog&)> r;
    return r;
}

std::vector<Point2> cornersOf(const ShellIntegrationBand& band) {
    const auto c = bandCorners(band);
    return {c.begin(), c.end()};
}

bool bandInside(const ShellIntegrationBand& band, double l, double b, double r, double t) {
    for (const Point2& p : cornersOf(band))
        if (p.x < l || p.x > r || p.y < b || p.y > t) return false;
    return true;
}

bool bandTouches(const ShellIntegrationBand& band, double l, double b, double r, double t) {
    double minX = 1e300, minY = 1e300, maxX = -1e300, maxY = -1e300;
    for (const Point2& p : cornersOf(band)) {
        minX = std::min(minX, p.x);
        maxX = std::max(maxX, p.x);
        minY = std::min(minY, p.y);
        maxY = std::max(maxY, p.y);
    }
    return !(maxX < l || minX > r || maxY < b || minY > t);
}

QString toolName(BandViewer::Tool tool) {
    switch (tool) {
        case BandViewer::Tool::New: return QObject::tr("Nueva");
        case BandViewer::Tool::Generate: return QObject::tr("Generar bandas de integración en zona");
        case BandViewer::Tool::Edit: return QObject::tr("Editar");
        case BandViewer::Tool::Delete: return QObject::tr("Borrar");
        case BandViewer::Tool::MoveEnd: return QObject::tr("Mover extremo");
        case BandViewer::Tool::Join: return QObject::tr("Unir");
        case BandViewer::Tool::Divide: return QObject::tr("Dividir");
        case BandViewer::Tool::None: break;
    }
    return {};
}

}  // namespace

// ---- viewer -------------------------------------------------------------------------------------------------------------------

BandViewer::BandViewer(QWidget* parent) : QWidget(parent) {
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setMinimumSize(200, 150);
    setAutoFillBackground(false);
}

void BandViewer::setShape(const std::vector<Point2>& contour) {
    m_contour = contour;
    m_fitted = false;
    update();
}

void BandViewer::setBands(const std::vector<ShellIntegrationBand>& bands) {
    m_bands = bands;
    std::erase_if(m_selected, [this](int i) { return i < 0 || i >= static_cast<int>(m_bands.size()); });
    update();
}

void BandViewer::setTool(Tool tool) {
    m_tool = tool;
    m_points.clear();
    m_selected.clear();
    m_windowStart.reset();
    m_pendingEnd.reset();
    announce();
    update();
}

void BandViewer::setShowReferences(bool on) {
    m_references = on;
    update();
}

void BandViewer::setShowIncidences(bool on) {
    m_incidences = on;
    update();
}

void BandViewer::clearSelection() {
    m_selected.clear();
    m_windowStart.reset();
    update();
}

void BandViewer::cancel() {
    m_points.clear();
    m_windowStart.reset();
    m_pendingEnd.reset();
    m_zoomStart.reset();
    update();
}

void BandViewer::announce() { emit hint(m_tool == Tool::None ? QString() : tr("Opciones - %1").arg(toolName(m_tool))); }

void BandViewer::fitAll() {
    pushView();
    double minX = 0, minY = 0, maxX = 1, maxY = 1;
    if (!m_contour.empty()) {
        minX = maxX = m_contour.front().x;
        minY = maxY = m_contour.front().y;
        for (const Point2& p : m_contour) {
            minX = std::min(minX, p.x);
            maxX = std::max(maxX, p.x);
            minY = std::min(minY, p.y);
            maxY = std::max(maxY, p.y);
        }
    }
    const double w = std::max(maxX - minX, 1e-3), h = std::max(maxY - minY, 1e-3);
    // Room for the dimension lines and the axes (the contour fills about 60 % of the window in the original).
    m_scale = std::max(1e-3, std::min(std::max(width(), 1) * 0.62 / w, std::max(height(), 1) * 0.62 / h));
    m_centre = QPointF((minX + maxX) / 2.0, (minY + maxY) / 2.0);
    m_fitted = true;
    update();
}

void BandViewer::pushView() {
    if (m_views.size() > 50) m_views.erase(m_views.begin());
    m_views.push_back({m_scale, m_centre.x(), m_centre.y()});
}

void BandViewer::previousView() {
    if (m_views.empty()) return;
    const auto v = m_views.back();
    m_views.pop_back();
    m_scale = v[0];
    m_centre = QPointF(v[1], v[2]);
    update();
}

void BandViewer::zoomBy(double factor) {
    pushView();
    m_scale = std::clamp(m_scale * factor, 1e-3, 1e6);
    update();
}

void BandViewer::setPanMode(bool on) {
    m_pan = on;
    if (on) m_zoomWindow = false;
    setCursor(on ? Qt::OpenHandCursor : Qt::ArrowCursor);
}

void BandViewer::setZoomWindowMode(bool on) {
    m_zoomWindow = on;
    m_zoomStart.reset();
    if (on) m_pan = false;
}

QPointF BandViewer::toScreen(const Point2& p) const {
    return QPointF(width() / 2.0 + (p.x - m_centre.x()) * m_scale, height() / 2.0 - (p.y - m_centre.y()) * m_scale);
}

Point2 BandViewer::toModel(const QPointF& s) const {
    return {m_centre.x() + (s.x() - width() / 2.0) / m_scale, m_centre.y() - (s.y() - height() / 2.0) / m_scale};
}

void BandViewer::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    if (!m_fitted) fitAll();
}

int BandViewer::bandAt(const Point2& p) const {
    int best = -1;
    double bestDistance = kPickPx / m_scale;
    for (int i = 0; i < static_cast<int>(m_bands.size()); ++i) {
        const ShellIntegrationBand& b = m_bands[static_cast<std::size_t>(i)];
        if (pointInPolygon2D(p, cornersOf(b), 1e-9)) return i;
        // distance to the centre line
        const double dx = b.x2 - b.x1, dy = b.y2 - b.y1, len2 = dx * dx + dy * dy;
        const double t = len2 > 0 ? std::clamp(((p.x - b.x1) * dx + (p.y - b.y1) * dy) / len2, 0.0, 1.0) : 0.0;
        const double d = std::hypot(p.x - (b.x1 + t * dx), p.y - (b.y1 + t * dy));
        if (d <= bestDistance) {
            bestDistance = d;
            best = i;
        }
    }
    return best;
}

std::optional<std::pair<int, int>> BandViewer::endAt(const Point2& p) const {
    std::optional<std::pair<int, int>> best;
    double bestDistance = 2.0 * kPickPx / m_scale;
    for (int i = 0; i < static_cast<int>(m_bands.size()); ++i) {
        const ShellIntegrationBand& b = m_bands[static_cast<std::size_t>(i)];
        const double d0 = std::hypot(p.x - b.x1, p.y - b.y1), d1 = std::hypot(p.x - b.x2, p.y - b.y2);
        if (d0 <= bestDistance) {
            bestDistance = d0;
            best = {i, 0};
        }
        if (d1 <= bestDistance) {
            bestDistance = d1;
            best = {i, 1};
        }
    }
    return best;
}

void BandViewer::applySelection(const std::vector<int>& hits, Qt::KeyboardModifiers modifiers) {
    const bool add = modifiers.testFlag(Qt::ControlModifier), remove = modifiers.testFlag(Qt::ShiftModifier);
    if (!add && !remove) m_selected.clear();
    for (int i : hits) {
        const auto it = std::find(m_selected.begin(), m_selected.end(), i);
        if (remove) {
            if (it != m_selected.end()) m_selected.erase(it);
        } else if (it == m_selected.end()) {
            m_selected.push_back(i);
        }
    }
    update();
}

void BandViewer::click(const Point2& p, Qt::KeyboardModifiers modifiers) {
    switch (m_tool) {
        case Tool::None: return;
        case Tool::New:
            if (m_points.size() >= 2) m_points.clear();
            if (pointInPolygon2D(p, m_contour)) m_points.push_back(p);  // "dos puntos dentro de la lámina"
            break;
        case Tool::Generate:
            if (m_points.size() >= 4) m_points.clear();
            m_points.push_back(p);
            break;
        case Tool::MoveEnd:
            if (!m_pendingEnd) {
                m_pendingEnd = endAt(p);
                if (m_pendingEnd) m_selected = {m_pendingEnd->first};
            } else {
                const auto [index, end] = *m_pendingEnd;
                m_pendingEnd.reset();
                m_selected.clear();
                emit requestMoveEnd(index, end, p);
            }
            break;
        case Tool::Edit:
        case Tool::Delete:
        case Tool::Join:
        case Tool::Divide: {
            if (m_windowStart) {
                const Point2 a = *m_windowStart;
                m_windowStart.reset();
                const bool window = p.x >= a.x;  // second corner to the right: only what lies inside; to the left: crossing
                const double l = std::min(a.x, p.x), r = std::max(a.x, p.x), b = std::min(a.y, p.y), t = std::max(a.y, p.y);
                std::vector<int> hits;
                for (int i = 0; i < static_cast<int>(m_bands.size()); ++i) {
                    const ShellIntegrationBand& band = m_bands[static_cast<std::size_t>(i)];
                    if (window ? bandInside(band, l, b, r, t) : bandTouches(band, l, b, r, t)) hits.push_back(i);
                }
                applySelection(hits, modifiers);
            } else if (const int hit = bandAt(p); hit >= 0) {
                applySelection({hit}, modifiers);
            } else {
                m_windowStart = p;
            }
            break;
        }
    }
    update();
}

void BandViewer::rightClick() {
    switch (m_tool) {
        case Tool::None: return;
        case Tool::New:
            if (m_points.size() == 2) {
                const Point2 a = m_points[0], b = m_points[1];
                m_points.clear();
                emit requestNew(a, b);
            } else {
                m_points.clear();
            }
            break;
        case Tool::Generate:
            if (m_points.size() == 4) {
                const std::array<Point2, 4> zone{m_points[0], m_points[1], m_points[2], m_points[3]};
                m_points.clear();
                emit requestGenerate(zone);
            } else {
                m_points.clear();
            }
            break;
        case Tool::MoveEnd: m_pendingEnd.reset(); break;
        case Tool::Edit:
        case Tool::Delete:
        case Tool::Join:
        case Tool::Divide:
            if (m_windowStart) {
                m_windowStart.reset();
            } else if (!m_selected.empty()) {
                const std::vector<int> picked = m_selected;
                m_selected.clear();
                emit requestAct(m_tool, picked);
            }
            break;
    }
    update();
}

void BandViewer::mousePressEvent(QMouseEvent* event) {
    setFocus();
    if (event->button() == Qt::MiddleButton || (m_pan && event->button() == Qt::LeftButton)) {
        m_dragLast = event->position();
        setCursor(Qt::ClosedHandCursor);
        return;
    }
    if (event->button() == Qt::RightButton) {
        rightClick();
        return;
    }
    if (event->button() != Qt::LeftButton) return;
    const Point2 p = toModel(event->position());
    if (m_zoomWindow) {
        if (!m_zoomStart) {
            m_zoomStart = p;
        } else {
            const Point2 a = *m_zoomStart;
            m_zoomStart.reset();
            const double w = std::abs(p.x - a.x), h = std::abs(p.y - a.y);
            if (w > 1e-9 && h > 1e-9) {
                pushView();
                m_scale = std::min(width() / w, height() / h) * 0.95;
                m_centre = QPointF((a.x + p.x) / 2, (a.y + p.y) / 2);
            }
            m_zoomWindow = false;
        }
        update();
        return;
    }
    click(p, event->modifiers());
}

void BandViewer::mouseMoveEvent(QMouseEvent* event) {
    m_cursor = toModel(event->position());
    m_cursorInside = true;
    if (m_dragLast) {
        const QPointF d = event->position() - *m_dragLast;
        m_centre -= QPointF(d.x() / m_scale, -d.y() / m_scale);
        m_dragLast = event->position();
    }
    update();
}

void BandViewer::mouseReleaseEvent(QMouseEvent* event) {
    Q_UNUSED(event);
    if (m_dragLast) {
        m_dragLast.reset();
        setCursor(m_pan ? Qt::OpenHandCursor : Qt::ArrowCursor);
    }
}

void BandViewer::wheelEvent(QWheelEvent* event) {
    const double factor = std::pow(1.065, event->angleDelta().y() / 120.0);  // ~6.5 % per notch, like the 3D window
    const Point2 before = toModel(event->position());
    m_scale = std::clamp(m_scale * factor, 1e-3, 1e6);
    const Point2 after = toModel(event->position());
    m_centre += QPointF(before.x - after.x, before.y - after.y);
    update();
}

void BandViewer::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Escape && (!m_points.empty() || m_windowStart || m_pendingEnd || !m_selected.empty())) {
        m_points.clear();
        m_windowStart.reset();
        m_pendingEnd.reset();
        m_selected.clear();
        update();
        return;
    }
    QWidget::keyPressEvent(event);
}

void BandViewer::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.fillRect(rect(), Qt::white);
    if (m_contour.size() >= 3) {
        // contour
        QPolygonF poly;
        for (const Point2& p : m_contour) poly << toScreen(p);
        painter.setPen(QPen(kGreen, 1.6));
        painter.setBrush(Qt::NoBrush);
        painter.drawPolygon(poly);

        // Dimension lines (magenta): the overall width below and the overall height at the left, like the original (p7_42).
        double minX = 1e300, minY = 1e300, maxX = -1e300, maxY = -1e300;
        for (const Point2& p : m_contour) {
            minX = std::min(minX, p.x);
            maxX = std::max(maxX, p.x);
            minY = std::min(minY, p.y);
            maxY = std::max(maxY, p.y);
        }
        QFont font = painter.font();
        font.setPixelSize(11);
        painter.setFont(font);
        painter.setPen(QPen(kMagenta, 1));
        const auto dimension = [&](const QPointF& a, const QPointF& b, const QPointF& n, double value) {
            const QPointF sa = a + n * 34.0, sb = b + n * 34.0;
            painter.drawLine(a + n * 8.0, sa + n * 5.0);
            painter.drawLine(b + n * 8.0, sb + n * 5.0);
            painter.drawLine(sa, sb);
            const QPointF tick(-n.y() * 5, n.x() * 5);
            painter.drawLine(sa - tick, sa + tick);
            painter.drawLine(sb - tick, sb + tick);
            const QPointF mid = (sa + sb) / 2.0;
            painter.drawText(QRectF(mid.x() - 20, mid.y() - 8, 40, 16), Qt::AlignCenter, QString::number(value, 'g', 4));
        };
        dimension(toScreen({minX, minY}), toScreen({maxX, minY}), QPointF(0, 1), maxX - minX);
        dimension(toScreen({minX, minY}), toScreen({minX, maxY}), QPointF(-1, 0), maxY - minY);
    }

    // bands
    for (int i = 0; i < static_cast<int>(m_bands.size()); ++i) {
        const ShellIntegrationBand& b = m_bands[static_cast<std::size_t>(i)];
        const bool selected = std::find(m_selected.begin(), m_selected.end(), i) != m_selected.end();
        const bool incident = m_incidences && checkBand(m_contour, b).has_value();
        const QColor colour = selected ? kOrange : (incident ? QColor(214, 32, 32) : kMagenta);
        QPolygonF quad;
        for (const Point2& p : cornersOf(b)) quad << toScreen(p);
        QPen dotted(colour, selected ? 2.0 : 1.2);
        dotted.setStyle(Qt::DotLine);
        painter.setPen(dotted);
        painter.setBrush(Qt::NoBrush);
        painter.drawPolygon(quad);
        painter.setPen(QPen(colour, selected ? 2.0 : 1.2));
        painter.drawLine(toScreen({b.x1, b.y1}), toScreen({b.x2, b.y2}));
        if (m_references) {
            painter.setPen(colour);
            const QPointF mid = toScreen({(b.x1 + b.x2) / 2, (b.y1 + b.y2) / 2});
            painter.drawText(mid + QPointF(4, -4), QStringLiteral("B%1").arg(i + 1));  // TODO(medir): text of the references of the bands
        }
    }

    // command feedback
    painter.setPen(QPen(kMagenta, 1.2, Qt::DotLine));
    if (m_tool == Tool::New && !m_points.empty()) {
        const Point2 second = m_points.size() == 2 ? m_points[1] : m_cursor;
        QPolygonF quad;
        for (const Point2& p : cornersOf(ShellIntegrationBand{m_points[0].x, m_points[0].y, second.x, second.y, m_previewWidth})) quad << toScreen(p);
        painter.drawPolygon(quad);
        painter.drawLine(toScreen(m_points[0]), toScreen(second));
    }
    if (m_tool == Tool::Generate && !m_points.empty()) {
        QPolygonF zone;
        for (const Point2& p : m_points) zone << toScreen(p);
        if (m_points.size() < 4 && m_cursorInside) zone << toScreen(m_cursor);
        painter.drawPolyline(zone);
        if (m_points.size() == 4) painter.drawLine(zone.last(), zone.first());
    }
    if (m_windowStart) {
        const QRectF r = QRectF(toScreen(*m_windowStart), toScreen(m_cursor)).normalized();
        QPen pen(QColor(40, 40, 40), 1);
        pen.setStyle(m_cursor.x >= m_windowStart->x ? Qt::SolidLine : Qt::DashLine);
        painter.setPen(pen);
        painter.setBrush(Qt::NoBrush);
        painter.drawRect(r);
    }
    if (m_pendingEnd && m_pendingEnd->first < static_cast<int>(m_bands.size())) {
        const ShellIntegrationBand& b = m_bands[static_cast<std::size_t>(m_pendingEnd->first)];
        const Point2 fixed = m_pendingEnd->second == 0 ? Point2{b.x2, b.y2} : Point2{b.x1, b.y1};
        painter.setPen(QPen(kOrange, 1.2, Qt::DotLine));
        painter.drawLine(toScreen(fixed), toScreen(m_cursor));
    }
    if (m_cursorInside && (m_tool == Tool::New || m_tool == Tool::Generate)) {  // cross-hair of the point entry
        const QPointF c = toScreen(m_cursor);
        painter.setPen(QPen(QColor(128, 128, 128), 1));
        painter.drawLine(c - QPointF(12, 0), c + QPointF(12, 0));
        painter.drawLine(c - QPointF(0, 12), c + QPointF(0, 12));
    }

    // local axes at the lower left corner (X red, Y blue)
    const QPointF o(34, height() - 30);
    painter.setPen(QPen(QColor(0, 0, 220), 2));
    painter.drawLine(o, o - QPointF(0, 44));
    painter.drawLine(o - QPointF(0, 44), o - QPointF(4, 36));
    painter.drawLine(o - QPointF(0, 44), o - QPointF(-4, 36));
    painter.drawText(o + QPointF(-3, -48), QStringLiteral("Y"));
    painter.setPen(QPen(QColor(220, 0, 0), 2));
    painter.drawLine(o, o + QPointF(44, 0));
    painter.drawLine(o + QPointF(44, 0), o + QPointF(36, -4));
    painter.drawLine(o + QPointF(44, 0), o + QPointF(36, 4));
    painter.drawText(o + QPointF(48, 4), QStringLiteral("X"));
}

// ---- dialog -------------------------------------------------------------------------------------------------------------------

void ShellBandsDialog::setDialogRunner(std::function<int(QDialog&)> r) { runner() = std::move(r); }

int ShellBandsDialog::run(QDialog& dialog) { return runner() ? runner()(dialog) : dialog.exec(); }

ShellBandsDialog::ShellBandsDialog(const std::vector<Point2>& contour, std::vector<ShellIntegrationBand> bands, const BandForceOptions& force,
                                   QWidget* parent)
    : CypeDialog(tr("Bandas de integración"), parent), m_contour(contour), m_force(force) {
    qRegisterMetaType<BandViewer::Tool>("cc::app::BandViewer::Tool");
    setMaximizable();
    resize(800, 520);
    setMinimumSize(560, 380);
    m_history.push_back(std::move(bands));

    auto* body = new QWidget(this);
    auto* column = new QVBoxLayout(body);
    column->setContentsMargins(4, 4, 4, 4);
    column->setSpacing(2);

    // toolbar
    auto* bar = new QHBoxLayout;
    bar->setSpacing(1);
    const auto make = [&](const QString& name, const QString& icon, const QString& tip, bool checkable) {
        auto* b = new QToolButton(body);
        b->setIcon(themedIcon(icon));
        b->setIconSize(QSize(18, 18));
        b->setFixedSize(24, 24);
        b->setToolTip(tip);
        b->setAutoRaise(true);
        b->setCheckable(checkable);
        b->setObjectName(name);
        b->setStyleSheet(QStringLiteral("QToolButton:checked { background: #CDE6F7; border: 1px solid #9CCBEA; } QToolButton:hover:enabled { background: #E4F0FB; }"));
        m_buttons.emplace_back(name, b);
        bar->addWidget(b);
        return b;
    };
    const auto separator = [&] {
        auto* line = new QFrame(body);
        line->setFrameShape(QFrame::VLine);
        line->setStyleSheet(QStringLiteral("color: #C9C9C9;"));
        bar->addWidget(line);
    };
    m_undoButton = make(QStringLiteral("undo"), QStringLiteral("undo"), tr("Deshacer (Ctrl+Z)"), false);
    m_redoButton = make(QStringLiteral("redo"), QStringLiteral("redo"), tr("Rehacer (Ctrl+Y)"), false);
    make(QStringLiteral("integration"), QStringLiteral("band_integration"), tr("Integración de esfuerzos en bandas"), false);
    make(QStringLiteral("generate"), QStringLiteral("band_generate"),
         tr("Generar bandas de integración en zona. Seleccione cuatro puntos que definan la zona y, a continuación, pulse el botón derecho del ratón."), true);
    separator();
    make(QStringLiteral("new"), QStringLiteral("band_new"),
         tr("Introducción de una nueva banda de integración. Seleccione dos puntos dentro de la lámina y, a continuación, pulse el botón derecho del ratón."),
         true);
    make(QStringLiteral("edit"), QStringLiteral("band_edit"),
         tr("Edición de las propiedades de varias bandas de integración simultáneamente. Seleccione con el botón izquierdo del ratón o mediante selección "
            "por rectángulo las bandas a editar y, a continuación, pulse el botón derecho del ratón."),
         true);
    make(QStringLiteral("delete"), QStringLiteral("band_delete"), tr("Borrar bandas de integración"), true);
    separator();
    make(QStringLiteral("move"), QStringLiteral("band_move_end"), tr("Mover el extremo de una banda de integración"), true);
    make(QStringLiteral("join"), QStringLiteral("band_join"), tr("Unir bandas de integración paralelas"), true);
    make(QStringLiteral("divide"), QStringLiteral("band_divide"),
         tr("Permite dividir varias bandas de integración simultáneamente en el número de partes indicado. Seleccione con el botón izquierdo del ratón o "
            "mediante selección por rectángulo las bandas a dividir y, a continuación, pulse el botón derecho del ratón."),
         true);
    separator();
    make(QStringLiteral("incidences"), QStringLiteral("band_incidences"),
         tr("Mostrar/Ocultar incidencias: si se activa esta opción se resaltarán los elementos en los que existan incidencias."), true);
    bar->addStretch(1);
    make(QStringLiteral("search"), QStringLiteral("view_search"), tr("Búsqueda y activación de comandos (Ctrl+F)"), false);
    separator();
    make(QStringLiteral("viewPrev"), QStringLiteral("view_prev"), tr("Ventana anterior"), false);
    make(QStringLiteral("viewAll"), QStringLiteral("view_all"), tr("Ventana completa"), false);
    make(QStringLiteral("zoom2"), QStringLiteral("view_zoom2"), tr("Zoom x2"), false);
    make(QStringLiteral("redraw"), QStringLiteral("view_redraw"), tr("Redibujar"), false);
    make(QStringLiteral("zoomMark"), QStringLiteral("view_zoom_window"), tr("Marcar zoom"), true);
    make(QStringLiteral("pan"), QStringLiteral("view_pan"), tr("Mover imagen"), true);
    make(QStringLiteral("export"), QStringLiteral("view_export"), tr("Exportar imagen"), false);
    column->addLayout(bar);

    // side panel + viewer
    auto* middle = new QHBoxLayout;
    middle->setSpacing(2);
    auto* side = new QWidget(body);
    side->setFixedWidth(150);
    auto* sideColumn = new QVBoxLayout(side);
    sideColumn->setContentsMargins(0, 0, 0, 0);
    sideColumn->setSpacing(2);
    auto* header = new QLabel(tr("Opciones"), side);
    header->setStyleSheet(QStringLiteral("QLabel { background: #E1E1E1; padding: 1px 3px; font-size: 12px; }"));
    sideColumn->addWidget(header);
    m_references = new QCheckBox(tr("Referencias de bandas"), side);
    m_references->setChecked(false);
    sideColumn->addWidget(m_references);
    sideColumn->addStretch(1);
    middle->addWidget(side);
    m_viewer = new BandViewer(body);
    m_viewer->setShape(m_contour);
    m_viewer->setBands(m_history[0]);
    middle->addWidget(m_viewer, 1);
    column->addLayout(middle, 1);
    contentLayout()->setContentsMargins(0, 0, 0, 0);
    contentLayout()->addWidget(body, 1);
    setupFooter(tr("Aceptar"), tr("Cancelar"));

    connect(m_references, &QCheckBox::toggled, m_viewer, &BandViewer::setShowReferences);
    connect(m_viewer, &BandViewer::hint, this, &ShellBandsDialog::statusChanged);
    connect(m_viewer, &BandViewer::requestNew, this, [this](Point2 a, Point2 b) { doNew(a, b); });
    connect(m_viewer, &BandViewer::requestGenerate, this, [this](BandZone z) { doGenerate(z); });
    connect(m_viewer, &BandViewer::requestAct, this, [this](BandViewer::Tool t, std::vector<int> i) { doAct(t, i); });
    connect(m_viewer, &BandViewer::requestMoveEnd, this, [this](int i, int e, Point2 p) { doMoveEnd(i, e, p); });
    connect(m_undoButton, &QToolButton::clicked, this, &ShellBandsDialog::undo);
    connect(m_redoButton, &QToolButton::clicked, this, &ShellBandsDialog::redo);
    connect(button(QStringLiteral("integration")), &QToolButton::clicked, this, &ShellBandsDialog::editIntegration);
    // Exclusive command buttons.
    const std::vector<std::pair<QString, BandViewer::Tool>> tools = {
        {QStringLiteral("generate"), BandViewer::Tool::Generate}, {QStringLiteral("new"), BandViewer::Tool::New},
        {QStringLiteral("edit"), BandViewer::Tool::Edit},         {QStringLiteral("delete"), BandViewer::Tool::Delete},
        {QStringLiteral("move"), BandViewer::Tool::MoveEnd},      {QStringLiteral("join"), BandViewer::Tool::Join},
        {QStringLiteral("divide"), BandViewer::Tool::Divide}};
    for (const auto& [name, tool] : tools) {
        connect(button(name), &QToolButton::clicked, this, [this, name, tool](bool on) {
            if (!on) {
                m_viewer->setTool(BandViewer::Tool::None);
                return;
            }
            activate(tool, name);
        });
    }
    connect(button(QStringLiteral("incidences")), &QToolButton::toggled, m_viewer, &BandViewer::setShowIncidences);
    connect(button(QStringLiteral("viewPrev")), &QToolButton::clicked, m_viewer, &BandViewer::previousView);
    connect(button(QStringLiteral("viewAll")), &QToolButton::clicked, m_viewer, &BandViewer::fitAll);
    connect(button(QStringLiteral("zoom2")), &QToolButton::clicked, m_viewer, [this] { m_viewer->zoomBy(2.0); });
    connect(button(QStringLiteral("redraw")), &QToolButton::clicked, m_viewer, qOverload<>(&QWidget::update));
    connect(button(QStringLiteral("zoomMark")), &QToolButton::toggled, m_viewer, &BandViewer::setZoomWindowMode);
    connect(button(QStringLiteral("pan")), &QToolButton::toggled, m_viewer, &BandViewer::setPanMode);
    connect(button(QStringLiteral("export")), &QToolButton::clicked, this, [this] {
        const QString file = QFileDialog::getSaveFileName(this, tr("Exportar imagen"), QString(), tr("Imagen PNG (*.png)"));
        if (!file.isEmpty()) m_viewer->grab().save(file);
    });
    connect(button(QStringLiteral("search")), &QToolButton::clicked, this, [this] {
        QMenu menu(this);  // "búsqueda y activación de comandos": the commands of the toolbar by name
        for (const auto& [name, b] : m_buttons)
            if (b->isEnabled() && !b->toolTip().isEmpty() && name != QLatin1String("search") && b->isCheckable()) {
                QAction* a = menu.addAction(b->icon(), b->toolTip().section(QLatin1Char('.'), 0, 0));
                connect(a, &QAction::triggered, b, &QToolButton::click);
            }
        menu.exec(QCursor::pos());
    });
    setTabOrder(m_viewer, acceptButton());
    refresh();
}

QToolButton* ShellBandsDialog::button(const QString& name) const {
    for (const auto& [n, b] : m_buttons)
        if (n == name) return b;
    return nullptr;
}

void ShellBandsDialog::refresh() {
    m_viewer->setBands(bands());
    m_undoButton->setEnabled(canUndo());
    m_redoButton->setEnabled(canRedo());
    m_viewer->update();
}

void ShellBandsDialog::undo() {
    if (!canUndo()) return;
    --m_pos;
    refresh();
}

void ShellBandsDialog::redo() {
    if (!canRedo()) return;
    ++m_pos;
    refresh();
}

void ShellBandsDialog::setWidth(double widthM) {
    m_width = widthM;
    m_viewer->setPreviewWidth(widthM);
}

void ShellBandsDialog::apply(const BandResult& result) {
    if (!result.ok()) {
        if (!result.issue->text.empty()) messages::show(this, messages::Kind::Warning, QString::fromStdString(result.issue->text));
        return;
    }
    if (result.bands == bands()) return;
    m_history.resize(m_pos + 1);  // a new edit drops the redo branch
    m_history.push_back(result.bands);
    ++m_pos;
    refresh();
}

bool ShellBandsDialog::askWidth(const QString& title) {
    BandWidthDialog dialog(title, m_width * 100.0, this);
    if (run(dialog) != QDialog::Accepted) return false;
    setWidth(dialog.widthM());
    return true;
}

void ShellBandsDialog::activate(BandViewer::Tool tool, const QString& name) {
    for (const auto& [n, b] : m_buttons)
        if (b->isCheckable() && n != QLatin1String("incidences") && n != QLatin1String("zoomMark") && n != QLatin1String("pan") && n != name) b->setChecked(false);
    m_viewer->setTool(tool);
    if (tool == BandViewer::Tool::New) beginNew();
    else if (tool == BandViewer::Tool::Generate) beginGenerate();
}

void ShellBandsDialog::beginNew() {
    if (m_viewer->tool() != BandViewer::Tool::New) m_viewer->setTool(BandViewer::Tool::New);
    if (!askWidth(tr("Nueva"))) {  // Cancelar leaves the command
        m_viewer->setTool(BandViewer::Tool::None);
        if (QToolButton* b = button(QStringLiteral("new"))) b->setChecked(false);
    }
}

void ShellBandsDialog::beginGenerate() {
    if (m_viewer->tool() != BandViewer::Tool::Generate) m_viewer->setTool(BandViewer::Tool::Generate);
    if (!askWidth(tr("Generar bandas de integración"))) {
        m_viewer->setTool(BandViewer::Tool::None);
        if (QToolButton* b = button(QStringLiteral("generate"))) b->setChecked(false);
    }
}

void ShellBandsDialog::editIntegration() {
    BandForceDialog dialog(m_force, this);
    emit statusChanged(tr("Opciones - Integración de esfuerzos en bandas"));
    if (run(dialog) == QDialog::Accepted) m_force = dialog.options();
}

void ShellBandsDialog::doNew(const Point2& a, const Point2& b) {
    apply(newBand(m_contour, bands(), a, b, m_width));
    beginNew();  // the right button opens «Nueva» again for the next band
}

void ShellBandsDialog::doGenerate(const std::array<Point2, 4>& zone) {
    apply(generateBands(m_contour, bands(), zone, m_width));
    beginGenerate();
}

void ShellBandsDialog::doAct(BandViewer::Tool tool, const std::vector<int>& indices) {
    switch (tool) {
        case BandViewer::Tool::Edit: {
            double current = m_width;
            if (!indices.empty() && indices.front() >= 0 && indices.front() < static_cast<int>(bands().size())) current = bands()[static_cast<std::size_t>(indices.front())].widthM;
            BandWidthDialog dialog(tr("Editar"), current * 100.0, this);
            if (run(dialog) != QDialog::Accepted) return;
            apply(editBands(m_contour, bands(), indices, dialog.widthM()));
            break;
        }
        case BandViewer::Tool::Delete: apply(deleteBands(bands(), indices)); break;
        case BandViewer::Tool::Join: apply(joinBands(m_contour, bands(), indices)); break;
        case BandViewer::Tool::Divide: {
            BandDivisionsDialog dialog(kBandMinDivisions, this);
            if (run(dialog) != QDialog::Accepted) return;
            apply(divideBands(m_contour, bands(), indices, dialog.parts()));
            break;
        }
        default: break;
    }
    m_viewer->clearSelection();
}

void ShellBandsDialog::doMoveEnd(int index, int end, const Point2& to) { apply(moveBandEnd(m_contour, bands(), index, end, to)); }

}  // namespace cc::app
