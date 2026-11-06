#include "SupportPictograms.h"

#include <QFont>
#include <QPainter>
#include <QPainterPath>
#include <cmath>

namespace cc::app {

using namespace cc::core;

namespace {

constexpr double kPi = 3.14159265358979323846;

QPointF mixAxes(const SymbolAxes& ax, const Point3& v) { return ax[0] * v.x + ax[1] * v.y + ax[2] * v.z; }

QPointF unit(QPointF p, const QPointF& fallback) {
    const double n = std::hypot(p.x(), p.y());
    return n < 1e-9 ? fallback : QPointF(p.x() / n, p.y() / n);
}

Point3 normalised(const Point3& v) {
    const double n = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
    return n < 1e-12 ? Point3{0, 0, 1} : Point3{v.x / n, v.y / n, v.z / n};
}
Point3 cross(const Point3& a, const Point3& b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
Point3 axisVector(int axis) { return {axis == 0 ? 1.0 : 0.0, axis == 1 ? 1.0 : 0.0, axis == 2 ? 1.0 : 0.0}; }

/// Horizontal ground line of half width 45 at `base` with four oblique strokes hanging down-left.
void groundWithHatching(QPainter& p, const QPointF& base) {
    p.drawLine(base + QPointF(-45, 0), base + QPointF(45, 0));
    for (int i = 0; i < 4; ++i) {
        const QPointF from = base + QPointF(-23.0 + 22.0 * i, 0);
        p.drawLine(from, from + QPointF(-18, 18));
    }
}

/// "Wheel" of the line and plane symbols: a tilted ellipse next to a guide line.
void wheel(QPainter& p, const QPointF& centre) {
    p.save();
    p.translate(centre);
    p.rotate(12.0);
    p.drawEllipse(QPointF(0, 0), 5.5, 12.5);
    p.restore();
}

/// One guide line along the screen direction `u` with two wheels. The fixed family has it through the node; the pinned ("articulada")
/// family offsets it and joins it to the node with two lines forming a triangle (orig_vinc_ext_simbolos_2.png).
void guideLine(QPainter& p, const QPointF& node, const QPointF& u, bool pinned) {
    QPointF side = QPointF(-u.y(), u.x());
    if (side.y() < 0) side = -side;                          // hang towards the bottom of the screen
    if (std::abs(side.y()) < 0.35) side = QPointF(-1.0, 0);  // vertical guide: the wheels hang to its left
    const QPointF origin = pinned ? node + side * 16.0 : node;
    p.drawLine(origin - u * 45.0, origin + u * 45.0);
    for (int sign : {-1, 1}) {
        const QPointF on = origin + u * (27.0 * sign);
        wheel(p, on + side * 11.0);
        if (pinned) p.drawLine(node, on);
    }
}

}  // namespace

bool genericLinkIsNonTrivial(const InternalLink& link) {
    if (link.type != InternalLinkType::Generic) return false;
    for (int row : link.elementRow)
        if (row != 0) return true;
    for (const GenericLinkRow& row : link.rows)
        if (row.partialToNode) return true;
    return false;
}

bool drawExternalSupportSymbol(QPainter& p, const ExternalSupport& s, const QPointF& node, const SymbolAxes& axes, const QColor& colour) {
    const Restraint r = restraintOf(s);
    if (!r.any()) return false;
    p.save();
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(colour, 1.4));
    bool drawn = true;

    SupportKind kind = supportTypeInfo(s.type).kind;
    SupportTypeInfo info = supportTypeInfo(s.type);
    Point3 vector = s.userVector;
    bool rotationsFixed = info.rotationsFixed;
    if (kind == SupportKind::Generic) {
        const GenericSupportData& g = s.generic;
        rotationsFixed = r.fixed[3] && r.fixed[4] && r.fixed[5];
        if (g.mode == GenericDisplacementMode::OnLine) {
            kind = SupportKind::Line;
            info.userVector = true;
            vector = g.vector;
        } else if (g.mode == GenericDisplacementMode::OnPlane) {
            kind = SupportKind::Plane;
            info.userVector = true;
            vector = g.vector;
        }
    }

    switch (kind) {
        case SupportKind::Free: drawn = false; break;
        case SupportKind::Fixed: groundWithHatching(p, node); break;
        case SupportKind::Pinned: {
            p.drawLine(node, node + QPointF(-21, 36));
            p.drawLine(node, node + QPointF(21, 36));
            groundWithHatching(p, node + QPointF(0, 36));
            break;
        }
        case SupportKind::Line:
        case SupportKind::Plane: {
            const bool pinned = !rotationsFixed;
            if (kind == SupportKind::Line) {
                const Point3 d = info.userVector ? normalised(vector) : axisVector(info.axis);
                guideLine(p, node, unit(mixAxes(axes, d), QPointF(1, 0)), pinned);
            } else {
                const Point3 normal = info.userVector ? normalised(vector) : axisVector(info.axis);
                Point3 b1, b2;
                if (!info.userVector) {
                    b1 = axisVector((info.axis + 1) % 3);
                    b2 = axisVector((info.axis + 2) % 3);
                } else {
                    const Point3 helper = std::abs(normal.z) < 0.9 ? Point3{0, 0, 1} : Point3{1, 0, 0};
                    b1 = normalised(cross(normal, helper));
                    b2 = cross(normal, b1);
                }
                guideLine(p, node, unit(mixAxes(axes, b1), QPointF(1, 0)), pinned);
                guideLine(p, node, unit(mixAxes(axes, b2), QPointF(0, -1)), pinned);
            }
            break;
        }
        case SupportKind::Generic: {
            // A generic restraint with its three displacements blocked looks like the pinned support (measured, orig_vinc_ext_simbolo_generica).
            if (r.fixed[0] && r.fixed[1] && r.fixed[2]) {
                p.drawLine(node, node + QPointF(-21, 36));
                p.drawLine(node, node + QPointF(21, 36));
                groundWithHatching(p, node + QPointF(0, 36));
                break;
            }
            // PROVISIONAL (TODO(medir): the original glyph of the other combinations was not captured): a short bar with a cross stroke
            // along every blocked displacement axis and a small circle for every blocked rotation.
            for (int i = 0; i < 3; ++i) {
                const QPointF u = unit(axes[i], QPointF(1, 0));
                const QPointF side(-u.y(), u.x());
                if (r.fixed[i]) {
                    p.drawLine(node - u * 18.0, node + u * 18.0);
                    for (int sign : {-1, 1}) p.drawLine(node + u * (18.0 * sign) - side * 5.0, node + u * (18.0 * sign) + side * 5.0);
                }
                if (r.fixed[3 + i]) p.drawEllipse(node + u * 30.0, 4.0, 4.0);
            }
            break;
        }
    }
    p.restore();
    return drawn;
}

bool drawInternalLinkSymbol(QPainter& p, const InternalLink& link, const QPointF& node, const QColor& colour) {
    int circles = 0;
    switch (link.type) {
        case InternalLinkType::Fixed: break;
        case InternalLinkType::Pinned: circles = 1; break;
        case InternalLinkType::PartialFixity: circles = 2; break;
        case InternalLinkType::Generic: circles = genericLinkIsNonTrivial(link) ? 1 : 0; break;
    }
    if (circles == 0) return false;
    p.save();
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(colour, 1.0));
    p.drawEllipse(node, 5.0, 5.0);
    if (circles == 2) p.drawEllipse(node, 2.5, 2.5);
    p.restore();
    return true;
}

// --- pictograms -----------------------------------------------------------------------------------------------------------------

namespace {

const QColor kTan(0xEB, 0xC3, 0x8A);
const QColor kOrange(0xD0, 0x80, 0x10);
const QColor kTeal(0x10, 0xA8, 0xB8);
const QColor kBlue(0x10, 0x50, 0xC0);
const QColor kGreen(0x30, 0xB0, 0x50);

void beam(QPainter& p, QPointF from, QPointF to) {
    p.setPen(QPen(kOrange, 5.0, Qt::SolidLine, Qt::FlatCap));
    p.drawLine(from, to);
    p.setPen(QPen(kTan, 2.6, Qt::SolidLine, Qt::FlatCap));
    p.drawLine(from, to);
}

void smallText(QPainter& p, const QPointF& at, const QString& text) {
    QFont f = p.font();
    f.setPixelSize(7);
    p.setFont(f);
    p.setPen(Qt::black);
    p.drawText(at, text);
}

QString axisLetters(const SupportTypeInfo& info, bool plane) {
    static const char* line[] = {"X", "Y", "Z"};
    static const char* planes[] = {"YZ", "XZ", "XY"};  // the plane perpendicular to the blocked axis
    if (info.userVector) return {};
    return QString::fromLatin1(plane ? planes[info.axis] : line[info.axis]);
}

}  // namespace

QPixmap externalSupportPictogram(ExternalSupportType type, int size, qreal dpr) {
    QPixmap pm(QSize(size, size) * dpr);
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.scale(size / 38.0, size / 38.0);
    const SupportTypeInfo info = supportTypeInfo(type);
    const QPointF n(19, 17);  // the node
    p.setBrush(Qt::NoBrush);

    if (info.kind == SupportKind::Free) {  // a lone bar not held by anything
        beam(p, QPointF(6, 30), QPointF(26, 10));
        p.setPen(QPen(kOrange, 1.4));
        p.drawLine(QPointF(26, 10), QPointF(31, 10));
        p.drawLine(QPointF(26, 10), QPointF(26, 15));
        return pm;
    }
    if (info.kind == SupportKind::Generic) {  // the blue question mark between two stub bars
        beam(p, QPointF(3, 19), QPointF(11, 19));
        beam(p, QPointF(27, 19), QPointF(35, 19));
        QFont f = p.font();
        f.setPixelSize(22);
        f.setBold(true);
        p.setFont(f);
        p.setPen(kBlue);
        p.drawText(QRectF(8, 4, 22, 26), Qt::AlignCenter, QStringLiteral("?"));
        return pm;
    }

    // Bars arriving at the node from above.
    beam(p, n, QPointF(7, 5));
    beam(p, n, QPointF(19, 3));
    beam(p, n, QPointF(31, 5));
    const bool fixedFamily = info.kind == SupportKind::Fixed || info.rotationsFixed;
    p.setPen(QPen(kTeal, 1.6));
    const auto hatch = [&](double y) {
        p.drawLine(QPointF(5, y), QPointF(33, y));
        for (int i = 0; i < 4; ++i) p.drawLine(QPointF(9 + 7 * i, y), QPointF(5 + 7 * i, y + 4));
    };
    switch (info.kind) {
        case SupportKind::Fixed:
            p.setPen(QPen(kBlue, 1.6));
            hatch(n.y() + 6);
            p.drawLine(n, QPointF(n.x(), n.y() + 6));
            break;
        case SupportKind::Pinned:
            p.drawLine(n, QPointF(12, 29));
            p.drawLine(n, QPointF(26, 29));
            hatch(29);
            break;
        case SupportKind::Line:
        case SupportKind::Plane: {
            const bool plane = info.kind == SupportKind::Plane;
            p.setPen(QPen(fixedFamily ? kBlue : kTeal, 1.6));
            if (!fixedFamily) {
                p.drawLine(n, QPointF(10, 29));
                p.drawLine(n, QPointF(28, 29));
            } else {
                p.drawLine(n, QPointF(n.x(), n.y() + 5));
            }
            p.drawLine(QPointF(5, 29), QPointF(33, 29));
            p.drawEllipse(QPointF(13, 30), 3.2, 3.2);
            p.drawEllipse(QPointF(25, 30), 3.2, 3.2);
            if (plane) p.drawEllipse(QPointF(19, 33), 3.2, 3.2);
            if (!info.userVector) {  // the free direction(s) as small arrows in the lower left corner
                static const QPointF dirs[3] = {QPointF(9, 0), QPointF(7, -4), QPointF(0, -6)};
                const auto arrow = [&](const QPointF& d) {
                    const QPointF from(3, 36);
                    p.drawLine(from, from + d);
                    const double len = std::hypot(d.x(), d.y());
                    const QPointF u(d.x() / len, d.y() / len), side(-u.y(), u.x());
                    p.drawLine(from + d, from + d - u * 2.5 + side * 1.5);
                    p.drawLine(from + d, from + d - u * 2.5 - side * 1.5);
                };
                p.setPen(QPen(kOrange, 1.0));
                if (plane) {
                    for (int a = 0; a < 3; ++a)
                        if (a != info.axis) arrow(dirs[a]);
                } else {
                    arrow(dirs[info.axis]);
                }
            }
            if (info.userVector) {  // user defined: a green diamond with a small arrow
                p.setPen(QPen(kGreen, 1.4));
                p.setBrush(QColor(0xB8, 0xF0, 0xC8));
                QPolygonF diamond;
                diamond << QPointF(30, 21) << QPointF(35, 26) << QPointF(30, 31) << QPointF(25, 26);
                p.drawPolygon(diamond);
            } else {
                smallText(p, QPointF(3, 37), axisLetters(info, plane));
            }
            break;
        }
        default: break;
    }
    return pm;
}

QPixmap internalLinkPictogram(InternalLinkType type, int size, qreal dpr) {
    QPixmap pm(QSize(size, size) * dpr);
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.scale(size / 38.0, size / 38.0);
    const QColor magenta(0xFF, 0x00, 0xFF);
    if (type == InternalLinkType::Generic) {
        QFont f = p.font();
        f.setPixelSize(20);
        f.setBold(true);
        p.setFont(f);
        p.setPen(Qt::black);
        p.drawText(QRectF(0, 0, 38, 38), Qt::AlignCenter, QStringLiteral("?"));
        return pm;
    }
    const QColor line = type == InternalLinkType::Pinned ? Qt::black : magenta;
    p.setPen(QPen(line, 1.2));
    p.drawLine(QPointF(4, 19), QPointF(34, 19));
    p.drawLine(QPointF(19, 4), QPointF(19, 34));
    p.setBrush(Qt::white);
    if (type == InternalLinkType::PartialFixity) {
        p.setPen(QPen(Qt::black, 1.0));
        p.drawEllipse(QPointF(19, 19), 3.5, 3.5);
        p.setBrush(Qt::black);
        p.drawEllipse(QPointF(19, 19), 1.2, 1.2);
    } else if (type == InternalLinkType::Pinned) {
        p.setPen(QPen(Qt::black, 1.0));
        p.drawEllipse(QPointF(19, 19), 3.5, 3.5);
    }
    return pm;
}

}  // namespace cc::app
