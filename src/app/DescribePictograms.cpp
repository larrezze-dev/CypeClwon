#include "DescribePictograms.h"

#include <QPainter>
#include <QSvgRenderer>

namespace cc::app {

using namespace cc::core;

namespace {

const char* const kConcrete = "#CBCBCB";
const char* const kConcreteLight = "#E2E2E2";
const char* const kSlab = "#E9893F";
const char* const kSteel = "#8C93A3";
const char* const kWood = "#CDA66E";
const char* const kEdge = "#4A4A4A";

QString rect(double x0, double y0, double x1, double y1, const char* fill, const char* stroke = kEdge, double width = 1.2) {
    return QStringLiteral("<rect x='%1' y='%2' width='%3' height='%4' fill='%5' stroke='%6' stroke-width='%7'/>")
        .arg(x0).arg(y0).arg(x1 - x0).arg(y1 - y0).arg(QLatin1String(fill), QLatin1String(stroke)).arg(width);
}

QString poly(const QString& points, const char* fill, const char* stroke = kEdge) {
    return QStringLiteral("<polygon points='%1' fill='%2' stroke='%3' stroke-width='1.2' stroke-linejoin='round'/>")
        .arg(points, QLatin1String(fill), QLatin1String(stroke));
}

QString dot(double cx, double cy, const char* fill = "#A01818") {
    return QStringLiteral("<circle cx='%1' cy='%2' r='1.8' fill='%3'/>").arg(cx).arg(cy).arg(QLatin1String(fill));
}

/// Reinforcement dots at the four corners of a box, inset by 3.
QString corners(double x0, double y0, double x1, double y1, const char* fill = "#A01818") {
    return dot(x0 + 3, y0 + 3, fill) + dot(x1 - 3, y0 + 3, fill) + dot(x0 + 3, y1 - 3, fill) + dot(x1 - 3, y1 - 3, fill);
}

QString lattice(double x0, double y0, double x1, double y1) {
    const double mx = (x0 + x1) / 2.0, base = y1 - 4, top = y0 + 6;
    return QStringLiteral("<path d='M%1 %2 L%3 %4 L%5 %2 Z M%6 %4 L%7 %2 L%8 %4 Z' fill='#FFFFFF' stroke='%9' stroke-width='0.8'/>")
        .arg(x0 + 3).arg(base).arg((x0 + mx) / 2.0).arg(top).arg(mx - 1).arg(mx + 1).arg((mx + x1) / 2.0).arg(x1 - 3).arg(QLatin1String(kEdge));
}

/// I / H steel section in the box (x0, y0)-(x1, y1).
QString iSection(double x0, double y0, double x1, double y1, const char* fill = kSteel) {
    const double t = (y1 - y0) * 0.18, w = (x1 - x0) * 0.16, mx = (x0 + x1) / 2.0;
    const double xa = mx - w / 2.0, xb = mx + w / 2.0;
    QString points;
    const double xs[] = {x0, x1, x1, xb, xb, x1, x1, x0, x0, xa, xa, x0};
    const double ys[] = {y0, y0, y0 + t, y0 + t, y1 - t, y1 - t, y1, y1, y1 - t, y1 - t, y0 + t, y0 + t};
    for (int i = 0; i < 12; ++i) points += QStringLiteral("%1,%2 ").arg(xs[i]).arg(ys[i]);
    return poly(points, fill);
}

QString document(const QString& body) {
    return QStringLiteral("<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 64 64'>%1</svg>").arg(body);
}

QPixmap render(const QString& svg, int size) {
    QPixmap pm(size, size);
    pm.fill(QColor(0xF7, 0xF7, 0xF7));
    QSvgRenderer renderer(svg.toUtf8());
    QPainter painter(&pm);
    painter.setRenderHint(QPainter::Antialiasing);
    renderer.render(&painter);
    return pm;
}

// Beam types. The slab is the orange band; the beam is drawn over it where it is embedded.
QString flatLike(int type, bool lattice_, bool prestressed) {
    QString s;
    const bool left = type == 1 || type == 3, right = type == 1 || type == 2;
    if (left) s += rect(4, 18, 22, 34, kSlab);
    if (right) s += rect(42, 18, 60, 34, kSlab);
    if (!left) s += rect(4, 18, 9, 34, kSlab);   // the slab always shows a short stub so the beam reads as "inside the slab"
    if (!right) s += rect(55, 18, 60, 34, kSlab);
    s += rect(22, 18, 42, 34, prestressed ? "#BFC4CC" : kConcrete);
    if (lattice_) s += lattice(22, 18, 42, 34);
    else if (prestressed) s += dot(27, 30, "#2060B0") + dot(32, 30, "#2060B0") + dot(37, 30, "#2060B0") + dot(32, 26, "#2060B0");
    else s += corners(22, 18, 42, 34);
    return s;
}

QString hungLike(int type, bool lattice_, bool prestressed) {
    Q_UNUSED(type)
    QString s = rect(4, 6, 60, 18, kSlab);
    s += rect(22, 18, 42, 54, prestressed ? "#BFC4CC" : kConcrete);
    if (lattice_) s += lattice(22, 18, 42, 54);
    else if (prestressed) s += dot(27, 49, "#2060B0") + dot(32, 49, "#2060B0") + dot(37, 49, "#2060B0") + dot(32, 44, "#2060B0");
    else s += corners(22, 18, 42, 54);
    return s;
}

QString hungType(int type) {
    switch (type) {
    case 0: return hungLike(0, false, false);
    case 1:  // with collaborating head
        return rect(4, 6, 60, 18, kSlab) + rect(12, 12, 52, 18, kConcreteLight) + rect(22, 18, 42, 54, kConcrete) + corners(22, 18, 42, 54);
    case 2:  // inverted rectangular: the beam rises above the slab
        return rect(4, 40, 60, 52, kSlab) + rect(22, 8, 42, 52, kConcrete) + corners(22, 8, 42, 52);
    case 3:  // hung 'T'
        return rect(4, 6, 60, 18, kSlab) + rect(10, 18, 54, 28, kConcrete) + rect(24, 28, 40, 56, kConcrete) + corners(24, 28, 40, 56);
    case 4:  // 'T' with the head embedded in the slab
        return rect(4, 6, 60, 24, kSlab) + rect(10, 12, 54, 24, kConcrete) + rect(24, 24, 40, 56, kConcrete) + corners(24, 24, 40, 56);
    case 5:  // inverted 'T' with the head embedded
        return rect(4, 40, 60, 56, kSlab) + rect(10, 40, 54, 52, kConcrete) + rect(24, 8, 40, 40, kConcrete) + corners(24, 8, 40, 40);
    case 6:  // inverted 'T'
        return rect(4, 40, 60, 52, kSlab) + rect(10, 30, 54, 40, kConcrete) + rect(24, 8, 40, 30, kConcrete) + corners(24, 8, 40, 30);
    default:  // 7: variable depth
        return rect(4, 6, 60, 18, kSlab) + poly(QStringLiteral("12,18 52,18 52,40 12,56"), kConcrete) + dot(16, 51) + dot(48, 36);
    }
}

}  // namespace

QString pillarMaterialSvg(PillarMaterial material) {
    switch (material) {
    case PillarMaterial::Concrete: return document(rect(14, 14, 50, 50, kConcrete) + corners(14, 14, 50, 50));
    case PillarMaterial::Steel: return document(iSection(14, 12, 50, 52));
    case PillarMaterial::Mixed: return document(rect(8, 8, 56, 56, kConcrete) + iSection(18, 16, 46, 48));
    }
    return document({});
}

QString pillarConcreteSvg(PillarConcreteKind kind) {
    if (kind == PillarConcreteKind::Circular)
        return document(QStringLiteral("<circle cx='32' cy='32' r='20' fill='%1' stroke='%2' stroke-width='1.2'/>").arg(QLatin1String(kConcrete), QLatin1String(kEdge)) +
                        dot(32, 17) + dot(32, 47) + dot(17, 32) + dot(47, 32));
    return document(rect(12, 12, 52, 52, kConcrete) + corners(12, 12, 52, 52));
}

QString mixedPillarSvg(MixedPillarKind kind) {
    const QString circle = QStringLiteral("<circle cx='32' cy='32' r='24' fill='%1' stroke='%2' stroke-width='1.2'/>").arg(QLatin1String(kConcrete), QLatin1String(kEdge));
    switch (kind) {
    case MixedPillarKind::RectangularEmbeddedProfile: return document(rect(8, 10, 56, 54, kConcrete) + iSection(20, 18, 44, 46) + corners(8, 10, 56, 54));
    case MixedPillarKind::RectangularBox:  // box of plates filled with concrete
        return document(rect(8, 12, 56, 52, kSteel) + rect(14, 18, 50, 46, kConcrete));
    case MixedPillarKind::RectangularTube:
        return document(rect(8, 16, 56, 48, kSteel, kEdge, 1.2) + rect(12, 20, 52, 44, kConcrete, kEdge, 0.8));
    case MixedPillarKind::SquareTube: return document(rect(12, 12, 52, 52, kSteel) + rect(16, 16, 48, 48, kConcrete, kEdge, 0.8));
    case MixedPillarKind::CircularEmbeddedProfile: return document(circle + iSection(21, 19, 43, 45) + dot(32, 10) + dot(32, 54));
    case MixedPillarKind::CircularTube:
        return document(QStringLiteral("<circle cx='32' cy='32' r='24' fill='%1' stroke='%2' stroke-width='1.2'/><circle cx='32' cy='32' r='19' fill='%3' stroke='%2' stroke-width='0.8'/>")
                            .arg(QLatin1String(kSteel), QLatin1String(kEdge), QLatin1String(kConcrete)));
    }
    return document({});
}

QString beamFamilySvg(BeamFamily family) {
    switch (family) {
    case BeamFamily::Flat: return document(flatLike(1, false, false));
    case BeamFamily::Hung: return document(hungLike(0, false, false));
    case BeamFamily::Lattice: return document(hungLike(0, true, false));
    case BeamFamily::Prestressed: return document(hungLike(0, false, true));
    case BeamFamily::Steel: return document(rect(4, 6, 60, 18, kSlab) + iSection(18, 18, 46, 54));
    case BeamFamily::Timber:
        return document(rect(4, 6, 60, 18, kSlab) + rect(20, 18, 44, 52, kWood) +
                        QStringLiteral("<path d='M26 22 V48 M32 22 V48 M38 22 V48' stroke='#9C7A47' stroke-width='0.8' fill='none'/>"));
    }
    return document({});
}

QString beamTypeSvg(BeamFamily family, int type) {
    switch (family) {
    case BeamFamily::Flat: return document(flatLike(type, false, false));
    case BeamFamily::Prestressed: return document(flatLike(type, false, true));
    case BeamFamily::Lattice: return type == 4 ? document(hungLike(0, true, false)) : document(flatLike(type, true, false));
    case BeamFamily::Hung: return document(hungType(type));
    default: return beamFamilySvg(family);
    }
}

QPixmap pillarMaterialPictogram(PillarMaterial material, int size) { return render(pillarMaterialSvg(material), size); }
QPixmap pillarConcretePictogram(PillarConcreteKind kind, int size) { return render(pillarConcreteSvg(kind), size); }
QPixmap mixedPillarPictogram(MixedPillarKind kind, int size) { return render(mixedPillarSvg(kind), size); }
QPixmap beamFamilyPictogram(BeamFamily family, int size) { return render(beamFamilySvg(family), size); }
QPixmap beamTypePictogram(BeamFamily family, int type, int size) { return render(beamTypeSvg(family, type), size); }

}  // namespace cc::app
