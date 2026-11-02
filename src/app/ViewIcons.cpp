#include "ViewIcons.h"

#include <QFont>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QPolygonF>
#include <cmath>

namespace cc::app {

namespace {
constexpr double kPi = 3.14159265358979323846;
const QColor kInk(QStringLiteral("#3A4A5C"));
const QColor kBlue(QStringLiteral("#1C6FC4"));
const QColor kRed(QStringLiteral("#D22B2B"));
const QColor kGreen(QStringLiteral("#2DA02D"));

// Every icon is drawn on a 32x32 canvas and scaled to the requested size.
QPixmap canvas(QPainter& p, QPixmap& pm, int size) {
    pm = QPixmap(size * 2, size * 2);
    pm.setDevicePixelRatio(2.0);
    pm.fill(Qt::transparent);
    p.begin(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.scale(size / 32.0, size / 32.0);
    return pm;
}

void arrowHead(QPainter& p, QPointF tip, QPointF from, double len, const QColor& colour) {
    const double a = std::atan2(tip.y() - from.y(), tip.x() - from.x());
    QPolygonF head;
    head << tip << tip - QPointF(len * std::cos(a - 0.45), len * std::sin(a - 0.45))
         << tip - QPointF(len * std::cos(a + 0.45), len * std::sin(a + 0.45));
    p.setPen(Qt::NoPen);
    p.setBrush(colour);
    p.drawPolygon(head);
}

void arrow(QPainter& p, QPointF from, QPointF to, const QColor& colour, double width = 2.0, double head = 7.0) {
    p.setPen(QPen(colour, width, Qt::SolidLine, Qt::RoundCap));
    p.drawLine(from, to);
    arrowHead(p, to, from, head, colour);
}

void cube(QPainter& p, QPointF c, double s, const QColor& colour, bool perspective) {
    p.setPen(QPen(colour, 1.6));
    p.setBrush(Qt::NoBrush);
    const double k = perspective ? 0.78 : 1.0;
    const QPointF a(c.x() - s, c.y() - s * 0.55), b(c.x(), c.y() - s * 1.05), cc(c.x() + s * k, c.y() - s * 0.55 * k),
        d(c.x(), c.y() - s * 0.05);
    p.drawPolygon(QPolygonF({a, b, cc, d}));
    const QPointF e(a.x(), a.y() + s * 1.1), f(d.x(), d.y() + s * 1.0), g(cc.x(), cc.y() + s * 1.1 * k);
    p.drawLine(a, e);
    p.drawLine(d, f);
    p.drawLine(cc, g);
    p.drawLine(e, f);
    p.drawLine(f, g);
}

void circularArrow(QPainter& p, QPointF c, double r, const QColor& colour) {
    p.setPen(QPen(colour, 2.0, Qt::SolidLine, Qt::RoundCap));
    p.setBrush(Qt::NoBrush);
    p.drawArc(QRectF(c.x() - r, c.y() - r, 2 * r, 2 * r), 30 * 16, 290 * 16);
    const double a = 30.0 * kPi / 180.0;
    const QPointF tip(c.x() + r * std::cos(a), c.y() - r * std::sin(a));
    arrowHead(p, tip, tip + QPointF(-3, -5), 7.0, colour);
}
}  // namespace

QIcon viewBarIcon(ViewBarIcon which, int size) {
    QPixmap pm;
    QPainter p;
    canvas(p, pm, size);
    switch (which) {
        case ViewBarIcon::Projection:  // little axes triad
            arrow(p, {10, 22}, {10, 5}, kBlue, 2.0, 6.0);
            arrow(p, {10, 22}, {27, 22}, kRed, 2.0, 6.0);
            arrow(p, {10, 22}, {20, 12}, kGreen, 2.0, 6.0);
            break;
        case ViewBarIcon::ProjectionType:
            cube(p, {16, 17}, 9.0, kInk, true);
            break;
        case ViewBarIcon::RotateAroundPoint:
            circularArrow(p, {16, 16}, 10.0, kBlue);
            p.setPen(Qt::NoPen);
            p.setBrush(kRed);
            p.drawEllipse(QPointF(16, 16), 3.0, 3.0);
            break;
        case ViewBarIcon::RotateAroundCamera:
            circularArrow(p, {16, 16}, 11.0, kBlue);
            p.setPen(QPen(kInk, 1.6));
            p.setBrush(Qt::NoBrush);
            p.drawRoundedRect(QRectF(10, 12, 12, 9), 1.5, 1.5);
            p.drawEllipse(QPointF(16, 16.5), 2.6, 2.6);
            break;
        case ViewBarIcon::SectionX:
        case ViewBarIcon::SectionY:
        case ViewBarIcon::SectionZ: {
            const QColor plane = which == ViewBarIcon::SectionX ? kRed : which == ViewBarIcon::SectionY ? kGreen : kBlue;
            cube(p, {16, 18}, 9.0, kInk, false);
            p.setPen(Qt::NoPen);
            QColor fill = plane;
            fill.setAlpha(120);
            p.setBrush(fill);
            if (which == ViewBarIcon::SectionZ) p.drawPolygon(QPolygonF({{6, 18}, {16, 12}, {26, 18}, {16, 24}}));
            else p.drawPolygon(QPolygonF({{16, 8}, {26, 14}, {26, 25}, {16, 30}}));
            arrow(p, {4, 5}, {11, 5}, plane, 2.0, 5.0);
            break;
        }
        case ViewBarIcon::ClippingVolume:
            cube(p, {16, 18}, 9.0, kInk, false);
            arrow(p, {16, 2}, {16, 8}, kRed, 1.8, 5.0);
            arrow(p, {3, 24}, {8, 21}, kGreen, 1.8, 5.0);
            arrow(p, {29, 24}, {24, 21}, kBlue, 1.8, 5.0);
            break;
        case ViewBarIcon::Unidentified1:  // drawing with camera and arrow (purpose not identified in the original)
            p.setPen(QPen(kInk, 1.6));
            p.setBrush(Qt::NoBrush);
            p.drawRoundedRect(QRectF(5, 12, 16, 11), 1.5, 1.5);
            p.drawEllipse(QPointF(13, 17.5), 3.2, 3.2);
            arrow(p, {16, 9}, {27, 4}, kBlue, 2.0, 6.0);
            break;
        case ViewBarIcon::Unidentified2: {  // "3D" with a cog
            QFont f(QStringLiteral("Arial"));
            f.setPixelSize(16);
            f.setBold(true);
            p.setFont(f);
            p.setPen(kInk);
            p.drawText(QRectF(1, 6, 22, 20), Qt::AlignCenter, QStringLiteral("3D"));
            p.setPen(QPen(kBlue, 1.6));
            p.setBrush(Qt::NoBrush);
            p.drawEllipse(QPointF(24, 23), 3.8, 3.8);
            p.setPen(QPen(kBlue, 2.2, Qt::SolidLine, Qt::FlatCap));
            for (int i = 0; i < 8; ++i) {
                const double a = i * kPi / 4.0;
                p.drawLine(QPointF(24 + 4.4 * std::cos(a), 23 + 4.4 * std::sin(a)), QPointF(24 + 6.2 * std::cos(a), 23 + 6.2 * std::sin(a)));
            }
            break;
        }
    }
    p.end();
    return QIcon(pm);
}

QIcon zoomToolIcon(ZoomToolIcon which, int size) {
    QPixmap pm;
    QPainter p;
    canvas(p, pm, size);
    p.setPen(QPen(kInk, 1.6));
    p.setBrush(Qt::NoBrush);
    switch (which) {
        case ZoomToolIcon::PreviousWindow:
            p.drawRect(QRectF(13, 6, 15, 14));
            p.setPen(QPen(kInk, 1.2, Qt::DashLine));
            p.drawRect(QRectF(4, 14, 15, 14));
            arrow(p, {26, 27}, {16, 27}, kBlue, 2.0, 6.0);
            break;
        case ZoomToolIcon::FullWindow:
            p.drawRect(QRectF(9, 9, 14, 14));
            p.setPen(QPen(kBlue, 2.0, Qt::SolidLine, Qt::RoundCap));
            for (const QPointF c : {QPointF(4, 4), QPointF(28, 4), QPointF(4, 28), QPointF(28, 28)}) {
                const double sx = c.x() < 16 ? 1 : -1, sy = c.y() < 16 ? 1 : -1;
                p.drawLine(c, c + QPointF(6 * sx, 0));
                p.drawLine(c, c + QPointF(0, 6 * sy));
            }
            break;
        case ZoomToolIcon::DoubleWindow:
            p.drawRect(QRectF(4, 4, 24, 24));
            p.setPen(QPen(kBlue, 1.6));
            p.drawRect(QRectF(10, 10, 12, 12));
            break;
        case ZoomToolIcon::Redraw:
            circularArrow(p, {16, 16}, 10.0, kBlue);
            break;
        case ZoomToolIcon::ZoomMark:
            p.setPen(QPen(kInk, 2.0, Qt::SolidLine, Qt::RoundCap));
            p.drawEllipse(QPointF(14, 14), 8.5, 8.5);
            p.drawLine(QPointF(20.5, 20.5), QPointF(28, 28));
            p.setPen(QPen(kBlue, 2.0, Qt::SolidLine, Qt::RoundCap));
            p.drawLine(QPointF(10, 14), QPointF(18, 14));
            p.drawLine(QPointF(14, 10), QPointF(14, 18));
            break;
        case ZoomToolIcon::MoveImage:  // hand: palm and four fingers
            p.setPen(QPen(kInk, 1.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            p.setBrush(QColor(QStringLiteral("#F4E3C8")));
            p.drawRoundedRect(QRectF(8, 15, 16, 13), 4, 4);
            for (int i = 0; i < 4; ++i) p.drawRoundedRect(QRectF(8.5 + i * 4.0, 5 + (i == 0 || i == 3 ? 3 : 0), 3.4, 14), 1.7, 1.7);
            p.drawRoundedRect(QRectF(3.5, 15, 3.4, 9), 1.7, 1.7);
            break;
        case ZoomToolIcon::Orbit3D:  // four arrows
            arrow(p, {16, 16}, {16, 3}, kBlue, 2.0, 6.0);
            arrow(p, {16, 16}, {16, 29}, kBlue, 2.0, 6.0);
            arrow(p, {16, 16}, {3, 16}, kBlue, 2.0, 6.0);
            arrow(p, {16, 16}, {29, 16}, kBlue, 2.0, 6.0);
            break;
    }
    p.end();
    return QIcon(pm);
}

QIcon triadIcon(double yawDeg, double pitchDeg, double rollDeg, bool faded, int size) {
    core::Camera cam;
    cam.yawDeg = yawDeg;
    cam.pitchDeg = pitchDeg;
    cam.rollDeg = rollDeg;
    QPixmap pm;
    QPainter p;
    canvas(p, pm, size);
    const QPointF centre(16, 16);
    struct Axis {
        core::Vec3 dir;
        QColor colour;
        const char* label;
    };
    const Axis axes[] = {{{1, 0, 0}, kRed, "X"}, {{0, 1, 0}, kGreen, "Y"}, {{0, 0, 1}, kBlue, "Z"}};
    QFont f(QStringLiteral("Arial"));
    f.setPixelSize(7);
    f.setBold(true);
    p.setFont(f);
    // Draw the axes pointing away from the viewer first so the nearer ones stay on top.
    std::vector<int> order = {0, 1, 2};
    const auto depth = [&](int i) { return core::dot(axes[i].dir, cam.forward()); };
    std::sort(order.begin(), order.end(), [&](int a, int b) { return depth(a) > depth(b); });
    for (int i : order) {
        const double sx = core::dot(axes[i].dir, cam.right()), sy = core::dot(axes[i].dir, cam.up());
        const double len = 12.0;
        const QPointF tip(centre.x() + sx * len, centre.y() + 3 - sy * len);
        const QColor colour = faded ? QColor(QStringLiteral("#B9B9B9")) : axes[i].colour;
        if (std::hypot(sx, sy) < 0.12) {  // axis pointing at / away from the viewer: dot only
            p.setPen(Qt::NoPen);
            p.setBrush(colour);
            p.drawEllipse(QPointF(centre.x(), centre.y() + 3), 2.0, 2.0);
        } else {
            arrow(p, QPointF(centre.x(), centre.y() + 3), tip, colour, 1.8, 5.0);
        }
        p.setPen(colour);
        p.drawText(QRectF(tip.x() - 5 + sx * 4, tip.y() - 5 - sy * 4, 10, 10), Qt::AlignCenter, QString::fromLatin1(axes[i].label));
    }
    p.end();
    return QIcon(pm);
}

}  // namespace cc::app
