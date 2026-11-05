#include "BarDescribeDialog.h"

#include <QAbstractButton>
#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QLocale>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QPushButton>
#include <QRadioButton>
#include <QGuiApplication>
#include <QScreen>
#include <QSignalBlocker>
#include <QStackedWidget>
#include <QStyle>
#include <QToolButton>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>

#include "core/describir_catalogue.h"
#include "core/predefined_profiles.h"
#include "core/project_settings.h"
#include "core/section_properties.h"

namespace cc::app {

using namespace cc::core;

namespace {

#define N_(text) QT_TRANSLATE_NOOP("BarDescribeDialog", text)
QString trc(const char* text) { return QCoreApplication::translate("BarDescribeDialog", text); }
QString q(const std::string& s) { return QString::fromStdString(s); }
std::string s8(const QString& s) { return s.toStdString(); }

constexpr double kGravity = 9.80665;

// --- pictograms (original artwork drawn in code; the original's bitmaps are not copied) -----------------------------------------

enum class Glyph {
    IBeam, IBeamWide, Channel, Tee, AngleEqual, AngleUnequal, Flat, Round, Square, Rect, Trapezoid, TubeRound, TubeRect, Plate,
    ColdC, ColdZ, ColdOmega, ColdAngle, ColdBox, Wood, Concrete, Generic, Aluminium, BuiltUp
};

void paintGlyph(QPainter& p, Glyph glyph, const QRectF& r, const QColor& fill) {
    const auto pt = [&](double x, double y) { return QPointF(r.left() + x * r.width(), r.top() + y * r.height()); };
    const auto rect = [&](double x0, double y0, double x1, double y1) { return QRectF(pt(x0, y0), pt(x1, y1)); };
    const QColor edge = fill.darker(150);
    p.setPen(QPen(edge, 1));
    p.setBrush(fill);
    const auto polygon = [&](std::initializer_list<QPointF> points) { p.drawPolygon(QPolygonF(QList<QPointF>(points))); };
    const auto stroke = [&](std::initializer_list<QPointF> points) {
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(fill.darker(110), std::max(2.0, r.width() * 0.09), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.drawPolyline(QPolygonF(QList<QPointF>(points)));
    };
    switch (glyph) {
    case Glyph::IBeam:
    case Glyph::BuiltUp:
        p.drawRect(rect(0.18, 0.12, 0.82, 0.30));
        p.drawRect(rect(0.18, 0.70, 0.82, 0.88));
        p.drawRect(rect(0.43, 0.30, 0.57, 0.70));
        break;
    case Glyph::IBeamWide:
        p.drawRect(rect(0.08, 0.14, 0.92, 0.30));
        p.drawRect(rect(0.08, 0.70, 0.92, 0.86));
        p.drawRect(rect(0.43, 0.30, 0.57, 0.70));
        break;
    case Glyph::Channel:
        polygon({pt(0.2, 0.12), pt(0.8, 0.12), pt(0.8, 0.26), pt(0.38, 0.26), pt(0.38, 0.74), pt(0.8, 0.74), pt(0.8, 0.88), pt(0.2, 0.88)});
        break;
    case Glyph::Tee:
        polygon({pt(0.15, 0.12), pt(0.85, 0.12), pt(0.85, 0.28), pt(0.57, 0.28), pt(0.57, 0.88), pt(0.43, 0.88), pt(0.43, 0.28), pt(0.15, 0.28)});
        break;
    case Glyph::AngleEqual:
        polygon({pt(0.18, 0.12), pt(0.34, 0.12), pt(0.34, 0.66), pt(0.84, 0.66), pt(0.84, 0.82), pt(0.18, 0.82)});
        break;
    case Glyph::AngleUnequal:
        polygon({pt(0.18, 0.12), pt(0.34, 0.12), pt(0.34, 0.72), pt(0.9, 0.72), pt(0.9, 0.86), pt(0.18, 0.86)});
        break;
    case Glyph::Flat: p.drawRect(rect(0.42, 0.1, 0.58, 0.9)); break;
    case Glyph::Plate: p.drawRect(rect(0.1, 0.42, 0.9, 0.58)); break;
    case Glyph::Round: p.drawEllipse(rect(0.16, 0.16, 0.84, 0.84)); break;
    case Glyph::Square: p.drawRect(rect(0.2, 0.2, 0.8, 0.8)); break;
    case Glyph::Rect:
    case Glyph::Wood: p.drawRect(rect(0.12, 0.26, 0.88, 0.74)); break;
    case Glyph::Concrete: p.drawRect(rect(0.1, 0.22, 0.9, 0.78)); break;
    case Glyph::Trapezoid: polygon({pt(0.1, 0.7), pt(0.9, 0.7), pt(0.9, 0.3), pt(0.1, 0.5)}); break;
    case Glyph::TubeRound: {
        QPainterPath ring;
        ring.addEllipse(rect(0.16, 0.16, 0.84, 0.84));
        ring.addEllipse(rect(0.30, 0.30, 0.70, 0.70));
        p.drawPath(ring);
        break;
    }
    case Glyph::TubeRect: {
        QPainterPath ring;
        ring.addRoundedRect(rect(0.14, 0.22, 0.86, 0.78), 4, 4);
        ring.addRect(rect(0.28, 0.36, 0.72, 0.64));
        p.drawPath(ring);
        break;
    }
    case Glyph::ColdC: stroke({pt(0.75, 0.15), pt(0.3, 0.15), pt(0.3, 0.85), pt(0.75, 0.85)}); break;
    case Glyph::ColdZ: stroke({pt(0.2, 0.15), pt(0.6, 0.15), pt(0.4, 0.85), pt(0.8, 0.85)}); break;
    case Glyph::ColdOmega: stroke({pt(0.1, 0.8), pt(0.3, 0.8), pt(0.3, 0.2), pt(0.7, 0.2), pt(0.7, 0.8), pt(0.9, 0.8)}); break;
    case Glyph::ColdAngle: stroke({pt(0.3, 0.15), pt(0.3, 0.8), pt(0.85, 0.8)}); break;
    case Glyph::ColdBox: stroke({pt(0.2, 0.2), pt(0.8, 0.2), pt(0.8, 0.8), pt(0.2, 0.8), pt(0.2, 0.2)}); break;
    case Glyph::Aluminium:
        polygon({pt(0.15, 0.1), pt(0.85, 0.1), pt(0.85, 0.2), pt(0.55, 0.2), pt(0.55, 0.8), pt(0.85, 0.8), pt(0.85, 0.9), pt(0.15, 0.9), pt(0.15, 0.8), pt(0.45, 0.8), pt(0.45, 0.2), pt(0.15, 0.2)});
        break;
    case Glyph::Generic: {
        p.setPen(QPen(QColor(0xD0, 0x20, 0x20), std::max(2.0, r.width() * 0.1)));
        p.setBrush(Qt::NoBrush);
        p.drawArc(rect(0.3, 0.12, 0.7, 0.52), 40 * 16, 230 * 16);
        p.drawLine(pt(0.5, 0.52), pt(0.5, 0.68));
        p.setBrush(QColor(0xD0, 0x20, 0x20));
        p.drawEllipse(rect(0.46, 0.76, 0.54, 0.84));
        break;
    }
    }
}

QPixmap makePixmap(Glyph glyph, int size, const QColor& tile, const QColor& fill) {
    QPixmap pm(size, size);
    pm.fill(tile);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    paintGlyph(p, glyph, QRectF(0, 0, size, size), fill);
    return pm;
}

Glyph glyphOfIcon(BarMaterialIcon icon) {
    switch (icon) {
    case BarMaterialIcon::RolledSteel: return Glyph::IBeam;
    case BarMaterialIcon::RolledSteelFamily2: return Glyph::BuiltUp;
    case BarMaterialIcon::ColdFormedSteel: return Glyph::ColdZ;
    case BarMaterialIcon::Timber: return Glyph::Wood;
    case BarMaterialIcon::ExtrudedAluminium: return Glyph::TubeRect;
    case BarMaterialIcon::SpecialAluminium: return Glyph::Aluminium;
    case BarMaterialIcon::Concrete: return Glyph::Concrete;
    case BarMaterialIcon::Generic: return Glyph::Generic;
    }
    return Glyph::IBeam;
}
QColor fillOfIcon(BarMaterialIcon icon) {
    switch (icon) {
    case BarMaterialIcon::Timber: return QColor(0xC8, 0x8A, 0x4A);
    case BarMaterialIcon::Concrete: return QColor(0x9A, 0x9A, 0x9A);
    case BarMaterialIcon::ExtrudedAluminium:
    case BarMaterialIcon::SpecialAluminium: return QColor(0xD8, 0xD8, 0xC8);
    default: return QColor(0x8C, 0x8C, 0x94);
    }
}

// --- shape slotList ("Seleccion del perfil") --------------------------------------------------------------------------------------

struct ShapeSlot {
    const char* name;
    std::optional<ShapeKind> kind;  ///< nullopt: shape without data in the core (no series are offered for it)
    Glyph glyph;
};

const std::vector<ShapeSlot>& shapeSlots(BarMaterialIcon icon) {
    static const std::vector<ShapeSlot> rolled = {
        {N_("Doble T"), ShapeKind::IParallel, Glyph::IBeam},
        {N_("C simple"), ShapeKind::Channel, Glyph::Channel},
        {N_("T simple"), ShapeKind::Tee, Glyph::Tee},
        {N_("Angular simétrico"), ShapeKind::AngleEqual, Glyph::AngleEqual},
        {N_("Angular"), ShapeKind::AngleUnequal, Glyph::AngleUnequal},
        {N_("Platabanda"), ShapeKind::FlatBar, Glyph::Flat},
        {N_("Redondo macizo"), ShapeKind::RoundBar, Glyph::Round},
        {N_("Cuadrado macizo"), ShapeKind::SquareBar, Glyph::Square}};
    static const std::vector<ShapeSlot> builtUp = {
        {N_("Doble T simétrica armada"), ShapeKind::BuiltUpDoubleT, Glyph::IBeam},
        {N_("Doble T alas distintas armada"), std::nullopt, Glyph::IBeamWide},
        {N_("Doble T canto variable"), std::nullopt, Glyph::Trapezoid},
        {N_("Doble T, alas distintas, con canto variable"), std::nullopt, Glyph::Trapezoid},
        {N_("U simple armada"), std::nullopt, Glyph::Channel},
        {N_("T armada simple"), std::nullopt, Glyph::Tee},
        {N_("Cajón cuadrado conformado"), std::nullopt, Glyph::ColdBox},
        {N_("Cajón conformado rectangular"), std::nullopt, Glyph::TubeRect},
        {N_("Tubo circular"), std::nullopt, Glyph::TubeRound},
        {N_("Tubo rectangular"), std::nullopt, Glyph::TubeRect},
        {N_("Chapa"), std::nullopt, Glyph::Plate}};
    static const std::vector<ShapeSlot> cold = {
        {N_("Tubo circular"), std::nullopt, Glyph::TubeRound},
        {N_("Cajón cuadrado conformado"), std::nullopt, Glyph::ColdBox},
        {N_("U conformado rigidizado"), ShapeKind::ColdFormedC, Glyph::ColdC},
        {N_("Z conformada simple"), std::nullopt, Glyph::ColdZ},
        {N_("Omega conformada simple"), std::nullopt, Glyph::ColdOmega},
        {N_("Omega rigidizada almas inclinadas"), std::nullopt, Glyph::ColdOmega},
        {N_("Angular simétrico conformado"), std::nullopt, Glyph::ColdAngle},
        {N_("Angular simétrico rigidizado conformado"), std::nullopt, Glyph::ColdAngle},
        {N_("Cajón conformado rectangular"), std::nullopt, Glyph::TubeRect},
        {N_("U conformado simple"), ShapeKind::ColdFormedU, Glyph::ColdC},
        {N_("Canal con alma rigidizada"), std::nullopt, Glyph::ColdC},
        {N_("Z conformada con rigidizadores"), std::nullopt, Glyph::ColdZ},
        {N_("Omega simple almas inclinadas"), std::nullopt, Glyph::ColdOmega},
        {N_("Omega inclinada por canto exterior"), std::nullopt, Glyph::ColdOmega},
        {N_("Angular conformado"), std::nullopt, Glyph::ColdAngle},
        {N_("Angular rigidizado conformado"), std::nullopt, Glyph::ColdAngle}};
    static const std::vector<ShapeSlot> timber = {
        {N_("Redondo macizo"), ShapeKind::TimberRound, Glyph::Round},
        {N_("Cuadrado macizo"), ShapeKind::TimberSquare, Glyph::Square},
        {N_("Rectángulo macizo"), ShapeKind::TimberRectangular, Glyph::Rect},
        {N_("Rectangular, canto variable"), ShapeKind::TimberRectangularVariable, Glyph::Trapezoid}};
    static const std::vector<ShapeSlot> aluminium = {
        {N_("Doble T"), ShapeKind::AluminiumI, Glyph::IBeam},
        {N_("C simple"), std::nullopt, Glyph::Channel},
        {N_("T simple"), std::nullopt, Glyph::Tee},
        {N_("Angular simétrico"), std::nullopt, Glyph::AngleEqual},
        {N_("Angular"), std::nullopt, Glyph::AngleUnequal},
        {N_("Platabanda"), std::nullopt, Glyph::Flat},
        {N_("Tubo rectangular"), std::nullopt, Glyph::TubeRect},
        {N_("Tubo circular"), std::nullopt, Glyph::TubeRound},
        {N_("Redondo macizo"), std::nullopt, Glyph::Round},
        {N_("Cuadrado macizo"), std::nullopt, Glyph::Square}};
    static const std::vector<ShapeSlot> none;
    switch (icon) {
    case BarMaterialIcon::RolledSteel: return rolled;
    case BarMaterialIcon::RolledSteelFamily2: return builtUp;
    case BarMaterialIcon::ColdFormedSteel: return cold;
    case BarMaterialIcon::Timber: return timber;
    case BarMaterialIcon::ExtrudedAluminium: return aluminium;
    default: return none;
    }
}

/// The I icon of the original covers both families of I (parallel and inclined flanges); the series decides.
ShapeKind shapeGroup(ShapeKind kind) { return kind == ShapeKind::ISloped ? ShapeKind::IParallel : kind; }

/// A series is offered under a shape slot when it is of the family of the icon and has the shape of the slot; a slot without shape
/// (nullopt) offers none.
bool offeredUnder(const ProfileSeries& s, SectionFamily family, const std::optional<ShapeKind>& wanted) {
    return wanted && s.family == family && shapeGroup(s.kind) == shapeGroup(*wanted);
}

/// First slot of the icon that has a shape (the default one: the first slots of the conformado icon have none).
int firstShapedSlot(BarMaterialIcon icon) {
    const auto& slotList = shapeSlots(icon);
    for (std::size_t i = 0; i < slotList.size(); ++i)
        if (slotList[i].kind) return static_cast<int>(i);
    return 0;
}

bool isTimberInline(const BarDescription& d) { return d.materialIcon == BarMaterialIcon::Timber && d.seriesRef.empty(); }

QString arrangementText(Arrangement a) {
    if (a == Arrangement::Boyd) return trc(N_("Boyd (alma aligerada)"));  // text of the dialog (the core keeps the short name)
    return QCoreApplication::translate("BarDescribeDialog", arrangementNameEs(a));
}

QString makeReference(ShapeKind kind, const std::vector<double>& dims) {
    const auto num = [](double v) { return QString::number(v, 'g', 10); };
    if (kind == ShapeKind::TimberRound && !dims.empty()) return QStringLiteral("Ø") + num(std::round(dims[0] * 1000.0));
    QStringList parts;
    for (double d : dims) parts << num(std::round(d * 1000.0) / 10.0);  // centimetres
    return parts.join(QLatin1Char('x'));
}

std::vector<double> defaultInlineDims(ShapeKind kind) {
    if (kind == ShapeKind::TimberRound) return {0.10};  // serie "Redondo" Ø100 of the original
    return std::vector<double>(dimensionKeys(kind).size(), 0.30);
}

QWidget* headerBand(const QString& text, QWidget* parent) {
    auto* label = new QLabel(text, parent);
    label->setObjectName(QStringLiteral("describeHeader"));
    label->setStyleSheet(QStringLiteral("QLabel#describeHeader { background: #DCDCDC; padding: 1px 4px; }"));
    return label;
}

QLabel* boldLabel(const QString& text, QWidget* parent) {
    auto* label = new QLabel(text, parent);
    QFont f = label->font();
    f.setBold(true);
    label->setFont(f);
    return label;
}

QToolButton* iconButton(const QPixmap& pm, int box, const QString& tip, QWidget* parent) {
    auto* b = new QToolButton(parent);
    b->setCheckable(true);
    b->setIcon(QIcon(pm));
    b->setIconSize(pm.size());
    b->setFixedSize(box, box);
    b->setToolTip(tip);
    b->setAutoRaise(false);
    b->setStyleSheet(QStringLiteral("QToolButton { background: #F7F7F7; border: 3px solid #E6E6E6; }"
                                    "QToolButton:checked { background: #DCEBFA; border: 3px solid #8EC3F5; }"
                                    "QToolButton:disabled { background: #EDEDED; }"));
    return b;
}

QToolButton* toolGlyph(const QString& name, const QString& glyph, const QString& colour, const QString& tip, QWidget* parent) {
    auto* b = new QToolButton(parent);
    b->setObjectName(name);
    b->setText(glyph);
    b->setToolTip(tip);
    b->setFocusPolicy(Qt::NoFocus);  // the icons of the toolbars are not tab stops (tabulacion_regla.md)
    b->setAutoRaise(true);
    b->setFixedSize(24, 24);
    b->setStyleSheet(QStringLiteral("QToolButton { border: none; background: transparent; color: %1; font-size: 16px; font-weight: bold; }"
                                    "QToolButton:hover { background: #E5F1FB; }").arg(colour));
    return b;
}

void warn(QWidget* parent, const QString& text) { profileMessageSink().warn(parent, QObject::tr("Aviso"), text); }

// Info box (icon, text, Aceptar), like the "Aviso" box but with the information icon.
class InfoDialog : public CypeDialog {
public:
    InfoDialog(const QString& title, const QString& text, QWidget* parent) : CypeDialog(title, parent) {
        setFixedSize(420, 161);
        auto* row = new QHBoxLayout;
        row->setContentsMargins(14, 8, 14, 0);
        auto* icon = new QLabel(this);
        icon->setPixmap(style()->standardIcon(QStyle::SP_MessageBoxInformation).pixmap(40, 40));
        row->addWidget(icon);
        auto* label = new QLabel(text, this);
        label->setWordWrap(true);
        row->addWidget(label, 1);
        contentLayout()->addLayout(row);
        contentLayout()->addStretch(1);
        auto* footer = setupCustomFooter();
        auto* ok = new QPushButton(QObject::tr("Aceptar"), this);
        ok->setDefault(true);
        footer->addStretch(1);
        footer->addWidget(ok);
        footer->addStretch(1);
        connect(ok, &QPushButton::clicked, this, &QDialog::accept);
    }
};

// "Propiedades del perfil seleccionado" (~516x420): dimensioned drawing on the left, 13 rows on the right.
class SectionDrawing : public QWidget {
public:
    explicit SectionDrawing(const Section& section, QWidget* parent) : QWidget(parent), m_section(section) { setMinimumSize(220, 330); }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.fillRect(rect(), Qt::white);
        const auto& d = m_section.dims;
        const bool doubleT = m_section.kind == ShapeKind::IParallel || m_section.kind == ShapeKind::BuiltUpDoubleT || m_section.kind == ShapeKind::AluminiumI;
        if (!doubleT || d.size() < 4) {
            // PROVISIONAL (TODO(captura)): the drawings of the other shapes were not captured.
            paintGlyph(p, Glyph::Generic, QRectF(40, 80, 140, 140), QColor(0x80, 0x80, 0x80));
            return;
        }
        const double h = d[0], b = d[1], tw = d[2], tf = d[3];
        const double scale = std::min(150.0 / b, 230.0 / h);
        const QPointF origin(width() / 2.0 + 10, 40);
        const double w = b * scale, hh = h * scale, twS = tw * scale, tfS = tf * scale;
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0x80, 0x80, 0x80));
        p.drawRect(QRectF(origin.x() - w / 2, origin.y(), w, tfS));
        p.drawRect(QRectF(origin.x() - w / 2, origin.y() + hh - tfS, w, tfS));
        p.drawRect(QRectF(origin.x() - twS / 2, origin.y(), twS, hh));
        p.setPen(QPen(Qt::black, 1));
        p.drawText(QRectF(origin.x() - w / 2, origin.y() - 28, w, 16), Qt::AlignCenter, QString::number(b * 1000.0, 'g', 4));
        p.drawLine(QPointF(origin.x() - w / 2, origin.y() - 8), QPointF(origin.x() + w / 2, origin.y() - 8));
        p.save();
        p.translate(origin.x() - w / 2 - 22, origin.y() + hh / 2);
        p.rotate(-90);
        p.drawText(QRectF(-30, -10, 60, 16), Qt::AlignCenter, QString::number(h * 1000.0, 'g', 4));
        p.restore();
        p.drawLine(QPointF(origin.x() - w / 2 - 8, origin.y()), QPointF(origin.x() - w / 2 - 8, origin.y() + hh));
        p.drawText(QPointF(origin.x() + twS / 2 + 4, origin.y() + hh + 18), QString::number(tw * 1000.0, 'g', 3));
        p.drawText(QPointF(origin.x() + w / 2 + 4, origin.y() + hh - tfS / 2), QString::number(tf * 1000.0, 'g', 3));
    }

private:
    Section m_section;
};

class ProfilePropertiesDialog : public CypeDialog {
public:
    ProfilePropertiesDialog(const Section& section, QWidget* parent) : CypeDialog(QObject::tr("Propiedades del perfil seleccionado"), parent) {
        setFixedSize(516, 420);
        auto* row = new QHBoxLayout;
        row->addWidget(new SectionDrawing(section, this));
        auto* panel = new QFrame(this);
        panel->setStyleSheet(QStringLiteral("QFrame { background: #E3E3E3; border: 1px solid #D0D0D0; }"));
        auto* grid = new QGridLayout(panel);
        int r = 0;
        for (const auto& [label, value] : profilePropertyRows(section)) {
            auto* l = new QLabel(label, panel);
            auto* v = boldLabel(value, panel);
            l->setStyleSheet(QStringLiteral("border: none;"));
            v->setStyleSheet(QStringLiteral("border: none;"));
            grid->addWidget(l, r, 0);
            grid->addWidget(v, r, 1, Qt::AlignRight);
            ++r;
        }
        row->addWidget(panel, 1);
        contentLayout()->addLayout(row, 1);
        auto* footer = setupCustomFooter();
        auto* ok = new QPushButton(QObject::tr("Aceptar"), this);
        ok->setDefault(true);
        footer->addStretch(1);
        footer->addWidget(ok);
        footer->addStretch(1);
        connect(ok, &QPushButton::clicked, this, &QDialog::accept);
    }
};

}  // namespace

DescribeSink& describeSink() {
    static DescribeSink sink{
        [](QWidget* parent, const QString& title, const QString& text) { InfoDialog(title, text, parent).exec(); },
        [](QWidget* parent, const Section& section) { ProfilePropertiesDialog(section, parent).exec(); },
        [](QWidget* parent, const ProfileSeries& initial, bool isNew, LengthUnit unit) -> std::optional<ProfileSeries> {
            SeriesEditDialog dialog(initial, isNew, parent, unit);
            if (dialog.exec() != QDialog::Accepted) return std::nullopt;
            return dialog.value();
        },
        [](QWidget* parent, LibraryScratch& scratch) { SeriesManagerDialog(scratch, parent).exec(); },
        [](QWidget* parent, TimberSettings& settings) {
            TimberServiceDialog dialog(settings, parent);
            if (dialog.exec() != QDialog::Accepted) return false;
            settings = dialog.value();
            return true;
        }};
    return sink;
}

std::vector<std::pair<QString, QString>> profilePropertyRows(const Section& section) {
    const auto mm = [](double metres) { return QString::number(metres * 1000.0, 'f', 1) + QStringLiteral(" mm"); };
    const auto number = [](double v, double scale, const char* unit) {
        return QString::number(v * scale, 'f', 2) + QLatin1Char(' ') + QString::fromUtf8(unit);
    };
    const auto L = [](const char* text) { return QCoreApplication::translate("BarDescribeDialog", text) + QLatin1Char(':'); };
    std::vector<std::pair<QString, QString>> rows;
    const auto& d = section.dims;
    const auto pp = effectiveProperties(section);
    // Rows measured in the original for the timber round bar and the catalogue shapes (docs/research/ui/data/describir_info_perfiles.tsv):
    // timber round 5 rows, aluminium 12, cold-formed U 13, built-up 14. Order and labels verbatim.
    if (section.kind == ShapeKind::TimberRound && pp && d.size() == 1) {
        rows.emplace_back(L(N_("Diámetro")), mm(d[0]));
        rows.emplace_back(L(N_("Área")), number(pp->area, 1e4, "cm²"));
        rows.emplace_back(L(N_("Inercia a flexión")), number(pp->iy, 1e8, "cm4"));
        rows.emplace_back(L(N_("Módulo plástico")), number(pp->wplY, 1e6, "cm³"));
        rows.emplace_back(L(N_("Inercia a torsión")), number(pp->it, 1e8, "cm4"));
        return rows;
    }
    if (isCatalogueShape(section.kind) && pp) {
        const auto mass = catalogueMassPerMetre(section);
        const auto area = [&] { rows.emplace_back(L(N_("Área")), number(pp->area, 1e4, "cm²")); };
        const auto iyy = [&] { rows.emplace_back(L(N_("Inercia a flexión Iyy")), number(pp->iy, 1e8, "cm4")); };
        const auto izz = [&] { rows.emplace_back(L(N_("Inercia a flexión Izz")), number(pp->iz, 1e8, "cm4")); };
        const auto it = [&] { rows.emplace_back(L(N_("Inercia a torsión")), number(pp->it, 1e8, "cm4")); };
        const auto avy = [&] { rows.emplace_back(L(N_("Área de cortante y")), number(pp->avy, 1e4, "cm²")); };
        const auto avz = [&] { rows.emplace_back(L(N_("Área de cortante z")), number(pp->avz, 1e4, "cm²")); };
        const auto warping = [&] { if (pp->iw > 0.0) rows.emplace_back(L(N_("Módulo de alabeo")), number(pp->iw, 1e12, "cm6")); };
        const auto zyy = [&] { rows.emplace_back(L(N_("Módulo plástico Zyy")), number(pp->wplY, 1e6, "cm³")); };
        const auto zzz = [&] { rows.emplace_back(L(N_("Módulo plástico Zzz")), number(pp->wplZ, 1e6, "cm³")); };
        const auto weight = [&] { if (mass) rows.emplace_back(L(N_("Peso")), number(*mass, 1.0, "kg/m")); };
        const auto cgY = [&] {  // 'y' of the centre of gravity measured from the middle of the flange width (-6.1 mm for UF-60x3)
            rows.emplace_back(L(N_("Coordenada 'y' del centro de gravedad")), QString::number((pp->yCg - d[1] / 2) * 1000.0, 'f', 1) + QStringLiteral(" mm"));
        };
        switch (section.kind) {
        case ShapeKind::AluminiumI:
            rows.emplace_back(L(N_("Canto total")), mm(d[0]));
            rows.emplace_back(L(N_("Anchura del ala")), mm(d[1]));
            rows.emplace_back(L(N_("Espesor del alma")), mm(d[2]));
            rows.emplace_back(L(N_("Espesor del ala")), mm(d[3]));
            area(); iyy(); zyy(); izz(); zzz(); it(); warping(); weight();
            break;
        case ShapeKind::BuiltUpDoubleT:
            rows.emplace_back(L(N_("Canto exterior del alma")), mm(d[0]));
            rows.emplace_back(L(N_("Anchura del ala")), mm(d[1]));
            rows.emplace_back(L(N_("Espesor del alma")), mm(d[2]));
            rows.emplace_back(L(N_("Espesor del ala")), mm(d[3]));
            area(); iyy(); izz(); it(); avy(); avz(); warping(); zyy(); zzz(); weight();
            break;
        case ShapeKind::ColdFormedU:
            rows.emplace_back(L(N_("Canto total")), mm(d[0]));
            rows.emplace_back(L(N_("Anchura del ala")), mm(d[1]));
            rows.emplace_back(L(N_("Espesor")), mm(d[2]));
            rows.emplace_back(L(N_("Radio de acuerdo interior")), mm(d[3]));
            area(); cgY(); iyy(); izz(); it(); avy(); avz(); warping(); weight();
            break;
        default:  // ColdFormedC: PROVISIONAL, no capture of its dialog; the rows follow those of the U plus the lip
            rows.emplace_back(L(N_("Canto total")), mm(d[0]));
            rows.emplace_back(L(N_("Anchura del ala")), mm(d[1]));
            rows.emplace_back(L(N_("Longitud del labio")), mm(d[2]));  // TODO(captura): label not seen
            rows.emplace_back(L(N_("Espesor")), mm(d[3]));
            rows.emplace_back(L(N_("Radio de acuerdo interior")), mm(d[4]));
            area(); cgY(); iyy(); izz(); it(); avy(); avz(); weight();
            break;
        }
        return rows;
    }
    if (section.kind == ShapeKind::IParallel && d.size() >= 5) {
        rows.emplace_back(L(N_("Canto total")), mm(d[0]));
        rows.emplace_back(L(N_("Anchura del ala")), mm(d[1]));
        rows.emplace_back(L(N_("Espesor del alma")), mm(d[2]));
        rows.emplace_back(L(N_("Espesor del ala")), mm(d[3]));
        rows.emplace_back(L(N_("Radio de acuerdo entre ala y alma")), mm(d[4]));
    } else {
        // PROVISIONAL (TODO(captura)): the dimension rows of the other shapes were not measured.
        const auto& keys = dimensionKeys(section.kind);
        for (std::size_t i = 0; i < keys.size() && i < d.size(); ++i) {
            const bool slope = keys[i] == "slope";
            rows.emplace_back(q(keys[i]) + QLatin1Char(':'), slope ? QString::number(d[i], 'f', 1) + QStringLiteral(" %") : mm(d[i]));
        }
    }
    const auto& p = pp;
    if (!p) return rows;
    rows.emplace_back(L(N_("Área")), number(p->area, 1e4, "cm²"));
    rows.emplace_back(L(N_("Inercia a flexión Iyy")), number(p->iy, 1e8, "cm4"));
    rows.emplace_back(L(N_("Módulo plástico Zyy")), number(p->wplY, 1e6, "cm³"));
    rows.emplace_back(L(N_("Inercia a flexión Izz")), number(p->iz, 1e8, "cm4"));
    rows.emplace_back(L(N_("Módulo plástico Zzz")), number(p->wplZ, 1e6, "cm³"));
    rows.emplace_back(L(N_("Inercia a torsión")), number(p->it, 1e8, "cm4"));
    if (p->iw > 0.0) rows.emplace_back(L(N_("Módulo de alabeo")), number(p->iw, 1e12, "cm6"));
    // Weight: specific weight 77.01 kN/m3 (the steel value of the generic card) divided by g; gives the 5.97 kg/m of the IPE 80.
    rows.emplace_back(L(N_("Peso")), number(p->area * 77.01 * 1000.0 / kGravity, 1.0, "kg/m"));
    return rows;
}

// --- ValueEdit ---------------------------------------------------------------------------------------------------------------

ValueEdit::ValueEdit(const FieldSpec& spec, QWidget* parent) : QLineEdit(parent) {
    setAlignment(Qt::AlignRight);
    setFixedWidth(80);
    connect(this, &QLineEdit::editingFinished, this, [this] { commit(); });
    setSpec(spec);
}

void ValueEdit::setSpec(const FieldSpec& spec) {
    m_spec = spec;
    setToolTip(QStringLiteral("<b>%1</b><br>%2").arg(spec.name.toHtmlEscaped(),
                                                     tr("Debe estar entre %1 y %2.").arg(formatted(spec.minimum), formatted(spec.maximum))));
    setText(formatted(m_value));
}

QString ValueEdit::formatted(double value) const { return QString::number(value, 'f', m_spec.decimals); }

void ValueEdit::setValue(double value) {
    m_value = value;
    setText(formatted(value));
}

QString ValueEdit::rangeMessage() const {
    if (m_spec.poissonQuirk)  // verbatim from the original (describir_barras.md section 6)
        return tr("El valor de '%1' debe estar entre %2  y %3 .").arg(m_spec.name, formatted(m_spec.minimum), formatted(m_spec.maximum));
    return tr("El valor de '%1' debe estar entre %2 y %3.").arg(m_spec.name, formatted(m_spec.minimum), formatted(m_spec.maximum));
}

bool ValueEdit::commit() {
    if (m_busy) return true;
    QLocale c(QLocale::C);
    c.setNumberOptions(QLocale::RejectGroupSeparator);  // only the decimal point: "12,5" is not a number
    bool ok = false;
    const double v = c.toDouble(text().trimmed(), &ok);
    QString problem;
    if (!ok || !std::isfinite(v))
        problem = tr("La cadena introducida no es un número.");
    else if (v < m_spec.minimum - 1e-9 || v > m_spec.maximum + 1e-9)
        problem = rangeMessage();
    if (problem.isEmpty()) {
        const bool changed = v != m_value;
        m_value = v;
        setText(formatted(m_value));
        if (changed) emit valueCommitted(v);
        return true;
    }
    m_busy = true;  // the modal box takes the focus away and would validate again
    setText(formatted(m_value));
    warn(window(), problem);
    selectAll();
    setFocus();
    m_busy = false;
    return false;
}

// --- BarDescribeDialog ---------------------------------------------------------------------------------------------------------

QStringList BarDescribeDialog::materialTooltips() {
    // #131445-#131447, #65218, #65222, #65227, #65226, #65223 (describir_barras.md section 3).
    return {trc(N_("Perfil de acero laminado")), trc(N_("Perfil armado de chapas de acero laminado")),
            trc(N_("Perfil de acero conformado")), trc(N_("Perfil de madera")), trc(N_("Perfil de aluminio extruido")),
            trc(N_("Sección especial de aluminio")), trc(N_("Barra de hormigón")), trc(N_("Perfil genérico"))};
}

QStringList BarDescribeDialog::shapeTooltips(BarMaterialIcon icon) {
    QStringList out;
    for (const ShapeSlot& slot : shapeSlots(icon)) out << trc(slot.name);
    return out;
}

BarDescribeDialog::BarDescribeDialog(std::vector<DescribeEntry> entries, const ProfileLibrary& library, LengthUnit unit,
                                     bool showUsedList, QWidget* parent)
    : CypeDialog(tr("Describir"), parent),
      m_entries(std::move(entries)),
      m_original(m_entries),
      m_scratch(library),
      m_initialLibrary(library),
      m_unit(unit) {
    if (m_entries.empty()) m_entries.push_back(DescribeEntry{defaultDescription(), {}});
    m_original = m_entries;
    for (const DescribeEntry& e : m_entries) {
        const int type = e.description.elementType == ElementType::Pillar ? 2 : e.description.elementType == ElementType::Beam ? 3 : 0;
        m_typeUi.push_back(type);
        m_typeOriginal.push_back(type);
        int slot = 0;
        const auto& slotList = shapeSlots(e.description.materialIcon);
        for (std::size_t i = 0; i < slotList.size(); ++i)
            if (slotList[i].kind && shapeGroup(*slotList[i].kind) == shapeGroup(e.description.shape)) {
                slot = static_cast<int>(i);
                break;
            }
        m_slotUi.push_back(slot);
    }
    ensureLibrarySeries();
    setMaximizable();
    build(showUsedList);
    const int height = 890;
    const QScreen* screen = QGuiApplication::primaryScreen();
    const int maxHeight = screen ? screen->availableGeometry().height() - 40 : height;
    resize(showUsedList ? 1051 : 859, std::min(height, maxHeight));
    setMinimumSize(640, 480);
    selectEntry(0);
}

void BarDescribeDialog::ensureLibrarySeries() {
    // PROVISIONAL: a work of the clone starts with an empty library; the original's has the standard series. The predefined series of
    // every family the working copy does not have yet are added (steel from the catalogue, the rest from the captures of the original).
    std::vector<ProfileSeries> missing;
    for (const ProfileSeries& s : importablePredefinedSeries()) {
        const bool haveFamily = std::any_of(m_scratch.library().series.begin(), m_scratch.library().series.end(),
                                            [&](const ProfileSeries& o) { return o.family == s.family; });
        if (!haveFamily) missing.push_back(s);
    }
    if (!missing.empty()) importSeries(m_scratch.project, m_scratch.undo, missing, ImportPolicy::KeepExisting);
}

ValueEdit* BarDescribeDialog::makeField(const QString& objectName, const FieldSpec& spec, QWidget* parent, int width) {
    auto* edit = new ValueEdit(spec, parent);
    edit->setObjectName(objectName);
    edit->setFixedWidth(width);
    m_fields.append(edit);
    return edit;
}

void BarDescribeDialog::bind(ValueEdit* edit, std::function<double(const BarDescription&)> get,
                             std::function<void(BarDescription&, double)> set) {
    m_bindings.append({edit, get, set});
    connect(edit, &ValueEdit::valueCommitted, this, [this, edit](double v) {
        if (m_syncing) return;
        for (const Binding& b : m_bindings)
            if (b.edit == edit) b.set(current(), v);
        refreshUsedList();
    });
}

void BarDescribeDialog::build(bool showUsedList) {
    auto* root = new QHBoxLayout;
    root->setContentsMargins(8, 6, 8, 0);
    root->setSpacing(8);
    if (showUsedList) {
        auto* left = new QFrame(this);
        left->setFixedWidth(202);
        left->setStyleSheet(QStringLiteral("QFrame#usedFrame { background: #F0F0F0; border: 1px solid #C8C8C8; }"));
        left->setObjectName(QStringLiteral("usedFrame"));
        auto* lv = new QVBoxLayout(left);
        lv->setContentsMargins(4, 4, 4, 4);
        lv->addWidget(new QLabel(tr("Perfiles utilizados"), left));
        m_used = new QListWidget(left);
        m_used->setStyleSheet(QStringLiteral("QListWidget { background: #F0F0F0; border: none; } QListWidget::item { padding: 2px; }"
                                             "QListWidget::item:selected { background: #D9D9D9; color: black; }"));
        for (std::size_t i = 0; i < m_entries.size(); ++i) m_used->addItem(QString());
        m_used->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        m_used->setTextElideMode(Qt::ElideRight);
        lv->addWidget(m_used, 1);
        root->addWidget(left);
        connect(m_used, &QListWidget::currentRowChanged, this, [this](int row) {
            if (row >= 0 && row != m_row) selectEntry(row);
        });
    }
    auto* right = new QVBoxLayout;
    right->setSpacing(4);
    right->addWidget(headerBand(tr("Tipo de elemento estructural"), this));
    auto* typeRow = new QHBoxLayout;
    typeRow->setContentsMargins(4, 0, 0, 0);
    m_typeGroup = new QButtonGroup(this);
    const QStringList types = {tr("Genérico"), tr("Tirante"), tr("Pilar"), tr("Viga")};
    for (int i = 0; i < types.size(); ++i) {
        auto* radio = new QRadioButton(types[i], this);
        radio->setObjectName(QStringLiteral("type%1").arg(i));
        m_typeGroup->addButton(radio, i);
        typeRow->addWidget(radio);
    }
    typeRow->addStretch(1);
    right->addLayout(typeRow);
    connect(m_typeGroup, &QButtonGroup::idClicked, this, [this](int id) { onTypeChanged(id); });
    m_typePages = new QStackedWidget(this);
    m_typePages->addWidget(buildGenericPage());
    m_typePages->addWidget(buildTiePage());
    m_typePages->addWidget(buildPillarPage());
    m_typePages->addWidget(buildBeamPage());
    right->addWidget(m_typePages, 1);
    root->addLayout(right, 1);
    contentLayout()->addLayout(root, 1);
    setupFooter(tr("Aceptar"), tr("Cancelar"));
}

QWidget* BarDescribeDialog::buildGenericPage() {
    auto* page = new QWidget(this);
    auto* v = new QVBoxLayout(page);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(4);
    auto* behaviourRow = new QHBoxLayout;
    behaviourRow->setContentsMargins(4, 6, 0, 6);
    behaviourRow->addWidget(new QLabel(tr("Comportamiento"), page));
    m_behaviour = new QComboBox(page);
    for (BehaviourKind b : {BehaviourKind::Linear, BehaviourKind::TensionOnly, BehaviourKind::CompressionOnly})
        m_behaviour->addItem(QCoreApplication::translate("BarDescribeDialog", behaviourNameEs(b)), static_cast<int>(b));
    m_behaviour->setMinimumWidth(124);
    behaviourRow->addWidget(m_behaviour);
    behaviourRow->addStretch(1);
    v->addLayout(behaviourRow);
    connect(m_behaviour, &QComboBox::currentIndexChanged, this, [this] {
        if (!m_syncing) current().behaviour = static_cast<BehaviourKind>(m_behaviour->currentData().toInt());
    });

    v->addWidget(headerBand(tr("Selección del material"), page));
    auto* iconRow = new QHBoxLayout;
    iconRow->setContentsMargins(4, 4, 0, 4);
    m_materialGroup = new QButtonGroup(this);
    m_materialGroup->setExclusive(true);
    const QStringList tips = materialTooltips();
    int index = 0;
    for (BarMaterialIcon icon : allMaterialIcons()) {
        auto* b = iconButton(makePixmap(glyphOfIcon(icon), 60, QColor(0xF7, 0xF7, 0xF7), fillOfIcon(icon)), 72, tips[index], page);
        b->setObjectName(QStringLiteral("material%1").arg(index));
        m_materialGroup->addButton(b, index);
        m_materialButtons.append(b);
        iconRow->addWidget(b);
        ++index;
    }
    iconRow->addStretch(1);
    v->addLayout(iconRow);
    connect(m_materialGroup, &QButtonGroup::idClicked, this, [this](int id) { onIconChosen(allMaterialIcons()[static_cast<std::size_t>(id)]); });

    // Seleccion del perfil: shape icons + series / profile.
    m_profileSection = new QWidget(page);
    auto* ps = new QVBoxLayout(m_profileSection);
    ps->setContentsMargins(0, 0, 0, 0);
    ps->setSpacing(4);
    ps->addWidget(headerBand(tr("Selección del perfil"), m_profileSection));
    auto* shapeAndSeries = new QGridLayout;
    shapeAndSeries->setContentsMargins(4, 4, 0, 0);
    m_shapeHost = new QWidget(m_profileSection);
    m_shapeHost->setLayout(new QGridLayout);
    static_cast<QGridLayout*>(m_shapeHost->layout())->setContentsMargins(0, 0, 0, 0);
    static_cast<QGridLayout*>(m_shapeHost->layout())->setSpacing(6);
    auto* seriesForm = new QWidget(m_profileSection);
    seriesForm->setObjectName(QStringLiteral("seriesForm"));
    auto* sf = new QGridLayout(seriesForm);
    sf->setContentsMargins(0, 0, 0, 0);
    sf->setHorizontalSpacing(8);
    sf->addWidget(new QLabel(tr("Serie de perfiles"), seriesForm), 0, 0);
    m_series = new QComboBox(seriesForm);
    m_series->setMinimumWidth(104);
    sf->addWidget(m_series, 0, 1);
    auto* seriesTools = new QHBoxLayout;
    seriesTools->setSpacing(0);
    const auto addTool = [&](const QString& name, const QString& glyph, const QString& colour, const QString& tip, QLayout* layout) {
        auto* b = toolGlyph(name, glyph, colour, tip, seriesForm);
        m_tools.append(b);
        layout->addWidget(b);
        return b;
    };
    addTool(QStringLiteral("new"), QStringLiteral("+"), QStringLiteral("#202020"), tr("Nuevo"), seriesTools);
    addTool(QStringLiteral("edit"), QStringLiteral("✎"), QStringLiteral("#D9A400"), tr("Edición del tipo seleccionado."), seriesTools);
    addTool(QStringLiteral("table"), QStringLiteral("▦"), QStringLiteral("#404040"), tr("Editar la lista de elementos"), seriesTools);
    seriesTools->addStretch(1);
    sf->addLayout(seriesTools, 0, 2);
    sf->addWidget(new QLabel(tr("Perfil"), seriesForm), 1, 0);
    m_profile = new QComboBox(seriesForm);
    m_profile->setMinimumWidth(104);
    sf->addWidget(m_profile, 1, 1);
    auto* profileTools = new QHBoxLayout;
    profileTools->setSpacing(0);
    addTool(QStringLiteral("info"), QStringLiteral("i"), QStringLiteral("#1B7FCC"), tr("Propiedades del perfil seleccionado"), profileTools);
    profileTools->addStretch(1);
    sf->addLayout(profileTools, 1, 2);
    shapeAndSeries->addWidget(m_shapeHost, 0, 0, Qt::AlignLeft | Qt::AlignTop);
    shapeAndSeries->addWidget(seriesForm, 0, 1, Qt::AlignLeft | Qt::AlignTop);
    shapeAndSeries->setColumnStretch(2, 1);
    ps->addLayout(shapeAndSeries);
    v->addWidget(m_profileSection);
    connect(m_series, &QComboBox::activated, this, [this] { onSeriesChanged(); });
    connect(m_profile, &QComboBox::activated, this, [this] { onProfileChanged(); });
    connect(button(QStringLiteral("new")), &QToolButton::clicked, this, [this] { onNewSeries(); });
    connect(button(QStringLiteral("edit")), &QToolButton::clicked, this, [this] { onEditSeries(); });
    connect(button(QStringLiteral("table")), &QToolButton::clicked, this, [this] { onManageSeries(); });
    connect(button(QStringLiteral("info")), &QToolButton::clicked, this, [this] {
        const auto section = resolveSection(current(), m_scratch.library());
        if (section) describeSink().properties(this, *section);
    });
    v->addStretch(1);
    v->addWidget(headerBand(tr("Datos adicionales"), page));
    v->addWidget(buildExtraPages());
    return page;
}

QWidget* BarDescribeDialog::buildExtraPages() {
    m_extra = new QStackedWidget(this);
    m_extra->setMinimumHeight(250);

    // 0: Disposicion (+ stiffeners of the family 2).
    auto* arrangementPage = new QWidget(m_extra);
    auto* ah = new QHBoxLayout(arrangementPage);
    ah->setContentsMargins(0, 0, 0, 0);
    auto* column = new QVBoxLayout;
    column->setSpacing(2);
    column->addWidget(boldLabel(tr("Disposición"), arrangementPage));
    m_arrangements = new QButtonGroup(this);
    for (Arrangement a : {Arrangement::Simple, Arrangement::SimpleWithHaunches, Arrangement::WithConcreteSlab, Arrangement::DoubleBoxWelded,
                          Arrangement::DoubleBoxBattened, Arrangement::DoubleBoxGeneric, Arrangement::HalfProfile, Arrangement::SideFlatPlates,
                          Arrangement::Boyd, Arrangement::DoubleIWelded, Arrangement::DoubleIBattened, Arrangement::DoubleIGeneric}) {
        auto* radio = new QRadioButton(arrangementText(a), arrangementPage);
        radio->setObjectName(QString::fromLatin1(arrangementKey(a)));
        m_arrangements->addButton(radio, static_cast<int>(a));
        m_arrangementRadios.append(radio);
        column->addWidget(radio);
        connect(radio, &QRadioButton::clicked, this, [this, a] {
            if (m_syncing) return;
            current().arrangement = a;
            if (!stiffenersAllowed(current().materialIcon, a)) current().stiffeners.present = false;
            updateExtra();
        });
    }
    column->addStretch(1);
    auto* colWidget = new QWidget(arrangementPage);
    colWidget->setLayout(column);
    colWidget->setFixedWidth(210);
    ah->addWidget(colWidget);
    auto* line = new QFrame(arrangementPage);
    line->setFrameShape(QFrame::VLine);
    line->setFrameShadow(QFrame::Plain);
    ah->addWidget(line);
    m_stiffenerBox = new QWidget(arrangementPage);
    auto* sg = new QGridLayout(m_stiffenerBox);
    sg->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    m_stiffeners = new QCheckBox(tr("Hay rigidizadores de alma"), m_stiffenerBox);
    sg->addWidget(m_stiffeners, 0, 0, 1, 3);
    auto* spacing = makeField(QStringLiteral("stiffenerSpacing"), {tr("Separación entre rigidizadores"), 0.1, 100000.0, 1}, m_stiffenerBox);
    auto* thickness = makeField(QStringLiteral("stiffenerThickness"), {tr("Espesor"), 0.1, 1000.0, 1}, m_stiffenerBox);
    sg->addWidget(new QLabel(tr("Separación entre rigidizadores"), m_stiffenerBox), 1, 0);
    sg->addWidget(spacing, 1, 1);
    sg->addWidget(new QLabel(QStringLiteral("mm"), m_stiffenerBox), 1, 2);
    sg->addWidget(new QLabel(tr("Espesor"), m_stiffenerBox), 2, 0);
    sg->addWidget(thickness, 2, 1);
    sg->addWidget(new QLabel(QStringLiteral("mm"), m_stiffenerBox), 2, 2);
    ah->addWidget(m_stiffenerBox, 1);
    // "Canto del perfil" (timber of variable depth): 100.0 mm / 100.0 mm by default (capture 101); the ranges were not measured.
    m_depthBox = new QWidget(arrangementPage);
    auto* dg = new QGridLayout(m_depthBox);
    dg->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    dg->addWidget(boldLabel(tr("Canto del perfil"), m_depthBox), 0, 0, 1, 3);
    auto* depthStart = makeField(QStringLiteral("depthStart"), {tr("Canto inicial"), 1.0, 10000.0, 1}, m_depthBox);
    auto* depthEnd = makeField(QStringLiteral("depthEnd"), {tr("Canto final"), 1.0, 10000.0, 1}, m_depthBox);
    dg->addWidget(new QLabel(tr("Canto inicial"), m_depthBox), 1, 0);
    dg->addWidget(depthStart, 1, 1);
    dg->addWidget(new QLabel(QStringLiteral("mm"), m_depthBox), 1, 2);
    dg->addWidget(new QLabel(tr("Canto final"), m_depthBox), 2, 0);
    dg->addWidget(depthEnd, 2, 1);
    dg->addWidget(new QLabel(QStringLiteral("mm"), m_depthBox), 2, 2);
    ah->addWidget(m_depthBox, 1);
    bind(depthStart, [](const BarDescription& d) { return d.depthStartM * 1000.0; },
         [](BarDescription& d, double v) { d.depthStartM = v / 1000.0; });
    bind(depthEnd, [](const BarDescription& d) { return d.depthEndM * 1000.0; },
         [](BarDescription& d, double v) { d.depthEndM = v / 1000.0; });
    ah->addStretch(1);  // keeps the column on the left when the stiffeners box is hidden
    bind(spacing, [](const BarDescription& d) { return d.stiffeners.spacingM * 1000.0; },
         [](BarDescription& d, double v) { d.stiffeners.spacingM = v / 1000.0; });
    bind(thickness, [](const BarDescription& d) { return d.stiffeners.thicknessM * 1000.0; },
         [](BarDescription& d, double v) { d.stiffeners.thicknessM = v / 1000.0; });
    connect(m_stiffeners, &QCheckBox::toggled, this, [this](bool on) {
        if (m_syncing) return;
        current().stiffeners.present = on;
        updateExtra();
    });
    m_extra->addWidget(arrangementPage);

    // 1: concrete / inline timber: "Tipo de seccion" radios + "Dimensiones".
    auto* inlinePage = new QWidget(m_extra);
    auto* ih = new QHBoxLayout(inlinePage);
    ih->setContentsMargins(0, 0, 0, 0);
    m_sectionTypeColumn = new QWidget(inlinePage);
    auto* tc = new QVBoxLayout(m_sectionTypeColumn);
    tc->setSpacing(2);
    tc->addWidget(boldLabel(tr("Tipo de sección"), m_sectionTypeColumn));
    m_sectionTypes = new QButtonGroup(this);
    const QStringList typeNames = {tr("Circular"), tr("Rectangular"), tr("Rectangular de canto variable")};
    const ShapeKind typeKinds[] = {ShapeKind::ConcreteCircular, ShapeKind::ConcreteRectangular, ShapeKind::ConcreteRectangularVariable};
    for (int i = 0; i < 3; ++i) {
        auto* radio = new QRadioButton(typeNames[i], m_sectionTypeColumn);
        radio->setObjectName(QStringLiteral("sectionType%1").arg(i));
        m_sectionTypes->addButton(radio, i);
        m_sectionTypeRadios.append(radio);
        tc->addWidget(radio);
        connect(radio, &QRadioButton::clicked, this, [this, i, typeKinds] {
            if (m_syncing) return;
            setInlineShape(typeKinds[i], defaultInlineDims(typeKinds[i]));
            syncUi();
        });
    }
    tc->addStretch(1);
    m_sectionTypeColumn->setFixedWidth(210);
    ih->addWidget(m_sectionTypeColumn);
    auto* line2 = new QFrame(inlinePage);
    line2->setFrameShape(QFrame::VLine);
    line2->setFrameShadow(QFrame::Plain);
    ih->addWidget(line2);
    auto* dims = new QGridLayout;
    dims->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    dims->addWidget(boldLabel(tr("Dimensiones"), inlinePage), 0, 0, 1, 3);
    for (int i = 0; i < 3; ++i) {
        auto* label = new QLabel(inlinePage);
        auto* edit = makeField(QStringLiteral("dimension%1").arg(i), {QString(), 0.1, 100.0, 1}, inlinePage, 50);
        auto* unit = new QLabel(q(unitSymbol(m_unit)), inlinePage);
        m_dimensionLabels.append(label);
        m_dimensionEdits.append(edit);
        m_dimensionUnits.append(unit);
        dims->addWidget(label, i + 1, 0);
        dims->addWidget(edit, i + 1, 1);
        dims->addWidget(unit, i + 1, 2);
        bind(edit,
             [this, i](const BarDescription& d) {
                 return d.inlineSection && i < static_cast<int>(d.inlineSection->dims.size())
                            ? d.inlineSection->dims[static_cast<std::size_t>(i)] * unitsPerMetre(m_unit)
                            : 0.0;
             },
             [this, i](BarDescription& d, double v) {
                 if (!d.inlineSection || i >= static_cast<int>(d.inlineSection->dims.size())) return;
                 d.inlineSection->dims[static_cast<std::size_t>(i)] = v / unitsPerMetre(m_unit);
                 d.inlineSection->reference = s8(makeReference(d.inlineSection->kind, d.inlineSection->dims));
                 d.profileRef = d.seriesRef.empty() ? std::string() : d.profileRef;
             });
    }
    dims->setColumnStretch(3, 1);
    ih->addLayout(dims, 1);
    m_extra->addWidget(inlinePage);

    // 2: generic section: Geometria | Caracteristicas mecanicas | Material.
    auto* genericPage = new QWidget(m_extra);
    auto* gh = new QHBoxLayout(genericPage);
    gh->setContentsMargins(0, 0, 0, 0);
    const auto column3 = [&](const QString& title, QWidget* parent) {
        auto* w = new QWidget(parent);
        auto* g = new QGridLayout(w);
        g->setAlignment(Qt::AlignTop);
        g->setColumnStretch(0, 1);
        g->addWidget(boldLabel(title, w), 0, 0, 1, 3);
        return std::pair<QWidget*, QGridLayout*>(w, g);
    };
    struct Row {
        const char* object;
        const char* label;
        double min, max;
        int decimals;
        const char* unit;
        std::function<double(const BarDescription&)> get;
        std::function<void(BarDescription&, double)> set;
        bool poisson;
    };
    const Row geometry[] = {
        {"genericWidthLeft", N_("Ancho izquierda (1)"), 0.1, 10000.0, 1, "mm", [](const BarDescription& d) { return d.generic.widthLeftM * 1000.0; }, [](BarDescription& d, double v) { d.generic.widthLeftM = v / 1000.0; }, false},
        {"genericWidthRight", N_("Ancho derecha (2)"), 0.1, 10000.0, 1, "mm", [](const BarDescription& d) { return d.generic.widthRightM * 1000.0; }, [](BarDescription& d, double v) { d.generic.widthRightM = v / 1000.0; }, false},
        {"genericDepthBottom", N_("Canto inferior (3)"), 0.1, 10000.0, 1, "mm", [](const BarDescription& d) { return d.generic.depthBottomM * 1000.0; }, [](BarDescription& d, double v) { d.generic.depthBottomM = v / 1000.0; }, false},
        {"genericDepthTop", N_("Canto superior (4)"), 0.1, 10000.0, 1, "mm", [](const BarDescription& d) { return d.generic.depthTopM * 1000.0; }, [](BarDescription& d, double v) { d.generic.depthTopM = v / 1000.0; }, false}};
    // The mechanical characteristics are NOT recomputed when the geometry changes (measured): independent values.
    const Row mechanics[] = {
        {"genericArea", N_("Área (A)"), 0.01, 1000000.0, 2, "cm²", [](const BarDescription& d) { return d.generic.areaM2 * 1e4; }, [](BarDescription& d, double v) { d.generic.areaM2 = v / 1e4; }, false},
        {"genericShearY", N_("Área de cortante (Avy)"), 0.01, 1000000.0, 2, "cm²", [](const BarDescription& d) { return d.generic.shearAreaYM2 * 1e4; }, [](BarDescription& d, double v) { d.generic.shearAreaYM2 = v / 1e4; }, false},
        {"genericShearZ", N_("Área de cortante (Avz)"), 0.01, 1000000.0, 2, "cm²", [](const BarDescription& d) { return d.generic.shearAreaZM2 * 1e4; }, [](BarDescription& d, double v) { d.generic.shearAreaZM2 = v / 1e4; }, false},
        {"genericInertiaY", N_("Inercia a flexión (Iyy)"), 0.01, 100000000.0, 2, "cm4", [](const BarDescription& d) { return d.generic.inertiaYM4 * 1e8; }, [](BarDescription& d, double v) { d.generic.inertiaYM4 = v / 1e8; }, false},
        {"genericInertiaZ", N_("Inercia a flexión (Izz)"), 0.01, 100000000.0, 2, "cm4", [](const BarDescription& d) { return d.generic.inertiaZM4 * 1e8; }, [](BarDescription& d, double v) { d.generic.inertiaZM4 = v / 1e8; }, false},
        {"genericTorsion", N_("Inercia a torsión (It)"), 0.01, 100000000.0, 2, "cm4", [](const BarDescription& d) { return d.generic.torsionM4 * 1e8; }, [](BarDescription& d, double v) { d.generic.torsionM4 = v / 1e8; }, false}};
    const Row material[] = {
        {"genericE", N_("Módulo de elasticidad"), 0.10, 10000000.0, 2, "MPa", [](const BarDescription& d) { return d.material.elasticModulusMPa; }, [](BarDescription& d, double v) { d.material.elasticModulusMPa = v; }, false},
        {"genericPoisson", N_("Módulo de Poisson"), 0.001, 50.0, 3, "", [](const BarDescription& d) { return d.material.poissonRatio; }, [](BarDescription& d, double v) { d.material.poissonRatio = v; }, true},
        {"genericAlpha", N_("Coeficiente de dilatación"), 0.000001, 100.0, 6, "m/m°C", [](const BarDescription& d) { return d.material.thermalExpansion; }, [](BarDescription& d, double v) { d.material.thermalExpansion = v; }, false},
        {"genericWeight", N_("Peso específico"), 0.01, 100000.0, 2, "kN/m³", [](const BarDescription& d) { return d.material.densityKnM3; }, [](BarDescription& d, double v) { d.material.densityKnM3 = v; }, false}};
    const auto fill = [&](QGridLayout* g, QWidget* parent, const Row* rows, int count, int firstRow) {
        for (int i = 0; i < count; ++i) {
            const Row& r = rows[i];
            const QString name = trc(r.label);
            auto* edit = makeField(QString::fromLatin1(r.object), {name, r.min, r.max, r.decimals, r.poisson}, parent, 84);
            g->addWidget(new QLabel(name, parent), firstRow + i, 0);
            g->addWidget(edit, firstRow + i, 1);
            g->addWidget(new QLabel(QString::fromUtf8(r.unit), parent), firstRow + i, 2);
            bind(edit, r.get, r.set);
        }
    };
    auto [geometryWidget, geometryGrid] = column3(tr("Geometría"), genericPage);
    fill(geometryGrid, geometryWidget, geometry, 4, 1);
    auto* drawing = new QLabel(geometryWidget);
    drawing->setPixmap(makePixmap(Glyph::Generic, 70, QColor(0xF0, 0xF0, 0xF0), QColor(0x80, 0x80, 0x80)));
    geometryGrid->addWidget(drawing, 5, 0, 1, 3, Qt::AlignHCenter);
    gh->addWidget(geometryWidget, 1);
    auto* l1 = new QFrame(genericPage);
    l1->setFrameShape(QFrame::VLine);
    l1->setFrameShadow(QFrame::Plain);
    gh->addWidget(l1);
    auto [mechanicsWidget, mechanicsGrid] = column3(tr("Características mecánicas"), genericPage);
    fill(mechanicsGrid, mechanicsWidget, mechanics, 6, 1);
    auto* info = toolGlyph(QStringLiteral("genericInfo"), QStringLiteral("i"), QStringLiteral("#1B7FCC"), tr("Características mecánicas"), mechanicsWidget);
    m_tools.append(info);
    mechanicsGrid->addWidget(info, 0, 2);
    connect(info, &QToolButton::clicked, this, [this] {
        describeSink().info(this, tr("Características mecánicas"),
                            tr("En la sección transversal, se considera que los ejes de inercia principales coinciden con los ejes locales Y y Z."));  // #65241
    });
    gh->addWidget(mechanicsWidget, 1);
    auto* l2 = new QFrame(genericPage);
    l2->setFrameShape(QFrame::VLine);
    l2->setFrameShadow(QFrame::Plain);
    gh->addWidget(l2);
    auto [materialWidget, materialGrid] = column3(tr("Material"), genericPage);
    fill(materialGrid, materialWidget, material, 4, 1);
    gh->addWidget(materialWidget, 1);
    m_extra->addWidget(genericPage);

    // 3: special aluminium (graphic editor, not developed).
    auto* specialPage = new QLabel(tr("El editor gráfico de secciones especiales de aluminio aún no está disponible."), m_extra);
    specialPage->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    m_extra->addWidget(specialPage);  // TODO(captura): combo "Secciones especiales de aluminio" + toolbar + graphic area
    return m_extra;
}

QWidget* BarDescribeDialog::buildTiePage() {
    auto* page = new QWidget(this);
    auto* v = new QVBoxLayout(page);
    v->setContentsMargins(0, 0, 0, 0);
    v->addWidget(headerBand(tr("Selección del perfil"), page));
    auto* row = new QHBoxLayout;
    row->setContentsMargins(4, 4, 0, 0);
    // Five icons measured; the tooltips are those of the homologous shapes of the rolled steel.
    const struct { const char* name; Glyph glyph; } icons[] = {{N_("Angular simétrico"), Glyph::AngleEqual},
                                                               {N_("Angular"), Glyph::AngleUnequal},
                                                               {N_("Platabanda"), Glyph::Flat},
                                                               {N_("Redondo macizo"), Glyph::Round},
                                                               {N_("Cuadrado macizo"), Glyph::Square}};
    auto* group = new QButtonGroup(page);
    int i = 0;
    for (const auto& icon : icons) {
        auto* b = iconButton(makePixmap(icon.glyph, 44, QColor(0xF7, 0xF7, 0xF7), QColor(0x8C, 0x8C, 0x94)), 56, trc(icon.name), page);
        b->setObjectName(QStringLiteral("tieShape%1").arg(i));
        const int index = i;
        group->addButton(b, i++);
        connect(b, &QToolButton::clicked, this, [this, index] { selectTieShape(index); });
        m_tieButtons.append(b);
        row->addWidget(b);
    }
    row->addSpacing(16);
    auto* form = new QGridLayout;
    form->addWidget(new QLabel(tr("Serie de perfiles"), page), 0, 0);
    m_tieSeries = new QComboBox(page);
    form->addWidget(m_tieSeries, 0, 1);
    form->addWidget(new QLabel(tr("Perfil"), page), 1, 0);
    m_tieProfile = new QComboBox(page);
    form->addWidget(m_tieProfile, 1, 1);
    row->addLayout(form);
    row->addStretch(1);
    v->addLayout(row);
    selectTieShape(m_tieShape);
    // The series and profiles of the five tie shapes were transcribed from the captures (single series per shape: L, L, Pletinas, R,
    // Barras cuadradas). The document "CÁLCULO DE TIRANTES EN CYPE 3D" shown below the combos in the original needs the module of ties,
    // which is not developed (ticket candidate), so Aceptar stays blocked for this type.
    auto* note = new QLabel(tr("Los tirantes aún no están disponibles."), page);
    note->setAlignment(Qt::AlignCenter);
    v->addWidget(note, 1);
    return page;
}

void BarDescribeDialog::selectTieShape(int index) {
    const auto& shapes = tieShapes();
    if (index < 0 || index >= static_cast<int>(shapes.size())) return;
    m_tieShape = index;
    for (int i = 0; i < m_tieButtons.size(); ++i) m_tieButtons[i]->setChecked(i == index);
    m_tieSeries->clear();
    m_tieProfile->clear();
    if (const ProfileSeries* series = tieSeries(shapes[static_cast<std::size_t>(index)])) {
        m_tieSeries->addItem(q(series->reference));
        for (const Section& p : series->profiles) m_tieProfile->addItem(q(p.reference));
    }
}

QWidget* BarDescribeDialog::buildPillarPage() {
    auto* page = new QWidget(this);
    auto* v = new QVBoxLayout(page);
    v->setContentsMargins(0, 0, 0, 0);
    auto* row = new QHBoxLayout;
    row->setContentsMargins(4, 4, 0, 0);
    const auto group = [&](const QString& title, const QStringList& tips, const std::vector<Glyph>& glyphs, int checked) {
        auto* col = new QVBoxLayout;
        col->addWidget(headerBand(title, page));
        auto* icons = new QHBoxLayout;
        auto* bg = new QButtonGroup(page);
        for (int i = 0; i < tips.size(); ++i) {
            auto* b = iconButton(makePixmap(glyphs[static_cast<std::size_t>(i)], 54, QColor(0xF7, 0xF7, 0xF7), QColor(0x9A, 0x9A, 0x9A)), 66, tips[i], page);
            b->setChecked(i == checked);
            bg->addButton(b, i);
            icons->addWidget(b);
        }
        icons->addStretch(1);
        col->addLayout(icons);
        row->addLayout(col, 1);
    };
    group(tr("Material"), {tr("Hormigón armado"), tr("Metálico"), tr("Sección mixta")}, {Glyph::Concrete, Glyph::IBeam, Glyph::BuiltUp}, 0);
    group(tr("Tipo"), {tr("Rectangular"), tr("Circular")}, {Glyph::Square, Glyph::Round}, 0);
    v->addLayout(row);
    auto* fields = new QGridLayout;
    fields->setContentsMargins(8, 12, 0, 0);
    fields->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    const FieldSpec spec[] = {{tr("Dimensión X"), 10.0, 1000.0, 0}, {tr("Dimensión Y"), 10.0, 1000.0, 0}};
    const char* names[] = {"pillarX", "pillarY"};
    for (int i = 0; i < 2; ++i) {
        auto* edit = makeField(QString::fromLatin1(names[i]), spec[i], page, 60);
        edit->setValue(30.0);
        fields->addWidget(new QLabel(spec[i].name, page), i, 0);
        fields->addWidget(edit, i, 1);
        fields->addWidget(new QLabel(QStringLiteral("cm"), page), i, 2);
    }
    v->addLayout(fields);
    // TODO(captura): the drawing of the section with its dimensions (yellow background) and the metallic / mixed variants.
    v->addStretch(1);
    return page;
}

QWidget* BarDescribeDialog::buildBeamPage() {
    auto* page = new QWidget(this);
    auto* v = new QVBoxLayout(page);
    v->setContentsMargins(0, 0, 0, 0);
    v->addWidget(headerBand(tr("Familia"), page));
    auto* families = new QHBoxLayout;
    families->setContentsMargins(4, 4, 0, 4);
    const QStringList familyTips = {tr("Vigas planas"), tr("Vigas descolgadas"), tr("Vigas de celosía"), tr("Vigas pretensadas"),
                                    tr("Vigas metálicas"), tr("Vigas de madera")};
    auto* fg = new QButtonGroup(page);
    for (int i = 0; i < familyTips.size(); ++i) {
        auto* b = iconButton(makePixmap(i == 4 ? Glyph::IBeam : i == 5 ? Glyph::Wood : Glyph::Rect, 54, QColor(0xF7, 0xF7, 0xF7), QColor(0x9A, 0x9A, 0x9A)), 66, familyTips[i], page);
        b->setChecked(i == 1);  // descolgadas
        fg->addButton(b, i);
        families->addWidget(b);
    }
    families->addStretch(1);
    v->addLayout(families);
    v->addWidget(headerBand(tr("Tipo"), page));
    auto* types = new QHBoxLayout;
    types->setContentsMargins(4, 4, 0, 4);
    const QStringList typeTips = {tr("Viga descolgada rectangular"), tr("Viga descolgada rectangular con cabeza colaborante"),
                                  tr("Viga invertida rectangular"), tr("Viga descolgada en 'T'"),
                                  tr("Viga descolgada en 'T' con cabeza embebida en el forjado"),
                                  tr("Viga en 'T' invertida con cabeza embebida en el forjado"), tr("Viga invertida en 'T'"),
                                  tr("Viga descolgada rectangular con sección variable")};
    auto* tg = new QButtonGroup(page);
    for (int i = 0; i < typeTips.size(); ++i) {
        auto* b = iconButton(makePixmap(i == 7 ? Glyph::Trapezoid : (i == 3 || i == 4 || i == 5 || i == 6) ? Glyph::Tee : Glyph::Rect, 54, QColor(0xF7, 0xF7, 0xF7), QColor(0x9A, 0x9A, 0x9A)), 66, typeTips[i], page);
        b->setChecked(i == 0);
        tg->addButton(b, i);
        types->addWidget(b);
    }
    types->addStretch(1);
    v->addLayout(types);
    auto* fields = new QGridLayout;
    fields->setContentsMargins(8, 8, 0, 0);
    fields->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    // TODO(captura): ranges of the beam fields (only the default values 30 / 30 / 15 / 15 cm were measured).
    const struct { const char* object; const char* label; double value; } rows[] = {{"beamB", N_("Anchura (b)"), 30.0}, {"beamA", N_("Canto (a)"), 30.0},
                                                                                       {"beamI", N_("Ala izquierda (i)"), 15.0}, {"beamD", N_("Ala derecha (d)"), 15.0}};
    int i = 0;
    for (const auto& r : rows) {
        auto* edit = makeField(QString::fromLatin1(r.object), {trc(r.label), 1.0, 1000.0, 1}, page, 60);
        edit->setValue(r.value);
        fields->addWidget(new QLabel(trc(r.label), page), i, 0);
        fields->addWidget(edit, i, 1);
        fields->addWidget(new QLabel(QStringLiteral("cm"), page), i, 2);
        ++i;
    }
    v->addLayout(fields);
    v->addStretch(1);
    return page;
}

// --- state ------------------------------------------------------------------------------------------------------------------------

QRadioButton* BarDescribeDialog::typeRadio(int index) const { return qobject_cast<QRadioButton*>(m_typeGroup->button(index)); }
int BarDescribeDialog::typeIndex() const { return m_typeGroup->checkedId(); }
QToolButton* BarDescribeDialog::materialButton(BarMaterialIcon icon) const {
    const auto& all = allMaterialIcons();
    const auto it = std::find(all.begin(), all.end(), icon);
    return m_materialButtons.value(static_cast<int>(it - all.begin()));
}
QToolButton* BarDescribeDialog::button(const QString& name) const {
    for (QToolButton* b : m_tools)
        if (b->objectName() == name) return b;
    return nullptr;
}
QRadioButton* BarDescribeDialog::arrangementRadio(Arrangement a) const {
    return qobject_cast<QRadioButton*>(m_arrangements->button(static_cast<int>(a)));
}
ValueEdit* BarDescribeDialog::field(const QString& name) const {
    for (ValueEdit* e : m_fields)
        if (e->objectName() == name) return e;
    return nullptr;
}
QRadioButton* BarDescribeDialog::sectionTypeRadio(int index) const { return m_sectionTypeRadios.value(index); }

bool BarDescribeDialog::usesLibrary(BarMaterialIcon icon) const {
    if (!familyOfIcon(icon) || icon == BarMaterialIcon::SpecialAluminium) return false;
    if (icon == BarMaterialIcon::Timber)
        return std::any_of(m_scratch.library().series.begin(), m_scratch.library().series.end(),
                           [](const ProfileSeries& s) { return s.family == SectionFamily::Timber; });
    return true;
}

void BarDescribeDialog::refreshUsedList() {
    if (!m_used) return;
    QSignalBlocker block(m_used);
    for (std::size_t i = 0; i < m_entries.size(); ++i)
        m_used->item(static_cast<int>(i))->setText(QStringLiteral("[Barra] ") + q(barLabelText(m_entries[i].description)));
}

void BarDescribeDialog::selectEntry(int row) {
    if (row < 0 || row >= static_cast<int>(m_entries.size())) return;
    m_row = row;
    refreshUsedList();
    if (m_used) {
        QSignalBlocker block(m_used);
        m_used->setCurrentRow(row);
    }
    syncUi();
}

void BarDescribeDialog::syncUi() {
    m_syncing = true;
    const BarDescription& d = current();
    const std::size_t row = static_cast<std::size_t>(m_row);
    const int type = m_typeUi[row];
    if (auto* radio = m_typeGroup->button(type)) radio->setChecked(true);
    m_typePages->setCurrentIndex(type);
    m_behaviour->setCurrentIndex(m_behaviour->findData(static_cast<int>(d.behaviour)));
    const auto& icons = allMaterialIcons();
    const int iconIndex = static_cast<int>(std::find(icons.begin(), icons.end(), d.materialIcon) - icons.begin());
    if (auto* b = m_materialGroup->button(iconIndex)) b->setChecked(true);
    rebuildShapes();
    rebuildSeries();
    for (const Binding& b : m_bindings) b.edit->setValue(b.get(d));
    updateExtra();
    m_syncing = false;
    refreshUsedList();
    updateTabChain();
}

void BarDescribeDialog::rebuildShapes() {
    const BarDescription& d = current();
    auto* grid = static_cast<QGridLayout*>(m_shapeHost->layout());
    for (QToolButton* b : m_shapeButtons) {
        grid->removeWidget(b);
        b->deleteLater();
        b->hide();
    }
    m_shapeButtons.clear();
    const auto& slotList = shapeSlots(d.materialIcon);
    m_profileSection->setVisible(!slotList.empty() || usesLibrary(d.materialIcon));
    const int perRow = 8;
    for (std::size_t i = 0; i < slotList.size(); ++i) {
        auto* b = iconButton(makePixmap(slotList[i].glyph, 44, QColor(0xF7, 0xF7, 0xF7), fillOfIcon(d.materialIcon)), 56, trc(slotList[i].name), m_shapeHost);
        b->setObjectName(QStringLiteral("shape%1").arg(i));
        b->setChecked(static_cast<int>(i) == m_slotUi[static_cast<std::size_t>(m_row)]);
        connect(b, &QToolButton::clicked, this, [this, i] { onShapeChosen(static_cast<int>(i)); });
        grid->addWidget(b, static_cast<int>(i) / perRow, static_cast<int>(i) % perRow);
        m_shapeButtons.append(b);
    }
    m_shapeHost->setFixedWidth(static_cast<int>(std::min<std::size_t>(slotList.size(), perRow)) * 62);
    grid->invalidate();
    m_shapeHost->updateGeometry();
    // The series form goes below the icons when there is one row of them, to their right when there are two (conformado).
    auto* host = static_cast<QGridLayout*>(m_profileSection->layout()->itemAt(1)->layout());
    auto* seriesForm = m_profileSection->findChild<QWidget*>(QStringLiteral("seriesForm"));
    host->removeWidget(seriesForm);
    if (slotList.size() > static_cast<std::size_t>(perRow))
        host->addWidget(seriesForm, 0, 1, Qt::AlignLeft | Qt::AlignTop);
    else
        host->addWidget(seriesForm, 1, 0, 1, 2, Qt::AlignLeft | Qt::AlignTop);
    const bool library = usesLibrary(d.materialIcon);
    seriesForm->setVisible(library);
    if (!slotList.empty() && !library) {
        // timber without series in the library: shapes with typed dimensions
    }
}

void BarDescribeDialog::rebuildSeries() {
    const BarDescription& d = current();
    QSignalBlocker b1(m_series), b2(m_profile);
    m_series->clear();
    const auto family = familyOfIcon(d.materialIcon);
    if (usesLibrary(d.materialIcon) && family) {
        const auto& slotList = shapeSlots(d.materialIcon);
        const int slot = m_slotUi[static_cast<std::size_t>(m_row)];
        std::optional<ShapeKind> wanted;
        if (slot >= 0 && slot < static_cast<int>(slotList.size())) wanted = slotList[static_cast<std::size_t>(slot)].kind;
        for (const ProfileSeries& s : m_scratch.library().series)
            if (offeredUnder(s, *family, wanted)) m_series->addItem(q(s.reference), q(s.reference));
        const int at = m_series->findData(q(d.seriesRef));
        m_series->setCurrentIndex(at);
    }
    rebuildProfiles();
}

void BarDescribeDialog::rebuildProfiles() {
    const BarDescription& d = current();
    QSignalBlocker block(m_profile);
    m_profile->clear();
    if (const ProfileSeries* series = m_scratch.library().find(d.seriesRef); series && m_series->currentIndex() >= 0) {
        for (const Section& p : series->profiles) m_profile->addItem(q(p.reference), q(p.reference));
        m_profile->setCurrentIndex(m_profile->findData(q(d.profileRef)));
    }
}

void BarDescribeDialog::updateExtra() {
    const BarDescription& d = current();
    const bool wasSyncing = m_syncing;
    m_syncing = true;
    int page = 0;
    if (d.materialIcon == BarMaterialIcon::Generic) page = 2;
    else if (d.materialIcon == BarMaterialIcon::SpecialAluminium) page = 3;
    else if (d.materialIcon == BarMaterialIcon::Concrete || isTimberInline(d)) page = 1;
    m_extra->setCurrentIndex(page);
    if (page == 0) {
        const auto offered = arrangementsFor(d.materialIcon, d.shape);
        for (QRadioButton* radio : m_arrangementRadios) {
            const auto a = static_cast<Arrangement>(m_arrangements->id(radio));
            const bool shown = std::find(offered.begin(), offered.end(), a) != offered.end();
            radio->setVisible(shown);
            radio->setChecked(a == d.arrangement);
            QFont f = radio->font();
            f.setBold(a == d.arrangement);  // the active one is shown in bold
            radio->setFont(f);
        }
        const bool allowed = stiffenersAllowed(d.materialIcon, d.arrangement);
        m_stiffenerBox->setVisible(allowed);
        m_depthBox->setVisible(d.materialIcon == BarMaterialIcon::Timber && d.shape == ShapeKind::TimberRectangularVariable);
        m_stiffeners->setChecked(d.stiffeners.present);
        field(QStringLiteral("stiffenerSpacing"))->setEnabled(d.stiffeners.present);
        field(QStringLiteral("stiffenerThickness"))->setEnabled(d.stiffeners.present);
    } else if (page == 1 && d.inlineSection) {
        const bool concrete = d.materialIcon == BarMaterialIcon::Concrete;
        m_sectionTypeColumn->setVisible(concrete);
        const ShapeKind kind = d.inlineSection->kind;
        const int typeIndex = kind == ShapeKind::ConcreteCircular ? 0 : kind == ShapeKind::ConcreteRectangular ? 1 : 2;
        for (int i = 0; i < m_sectionTypeRadios.size(); ++i) {
            m_sectionTypeRadios[i]->setChecked(concrete && i == typeIndex);
            QFont f = m_sectionTypeRadios[i]->font();
            f.setBold(concrete && i == typeIndex);
            m_sectionTypeRadios[i]->setFont(f);
        }
        const QStringList labels = dimensionLabels();
        const double perMetre = unitsPerMetre(m_unit);
        for (int i = 0; i < 3; ++i) {
            const bool shown = i < labels.size();
            m_dimensionLabels[i]->setVisible(shown);
            m_dimensionEdits[i]->setVisible(shown);
            m_dimensionUnits[i]->setVisible(shown);
            if (shown) {
                m_dimensionLabels[i]->setText(labels[i]);
                m_dimensionEdits[i]->setSpec({labels[i], 0.01 * perMetre, 10.0 * perMetre, 1});  // 1.0 .. 1000.0 cm (hormigon, medido)
                m_dimensionEdits[i]->setValue(i < static_cast<int>(d.inlineSection->dims.size()) ? d.inlineSection->dims[static_cast<std::size_t>(i)] * perMetre : 0.0);
            }
        }
    }
    m_syncing = wasSyncing;
}

QStringList BarDescribeDialog::dimensionLabels() const {
    const BarDescription& d = current();
    if (!d.inlineSection) return {};
    switch (d.inlineSection->kind) {
    case ShapeKind::ConcreteCircular: return {tr("Diámetro")};
    case ShapeKind::ConcreteRectangular: return {tr("Ancho"), tr("Canto")};
    case ShapeKind::ConcreteRectangularVariable: return {tr("Ancho"), tr("Canto inicial"), tr("Canto final")};
    case ShapeKind::TimberRound: return {tr("Diámetro")};
    case ShapeKind::TimberSquare: return {tr("Lado")};
    case ShapeKind::TimberRectangular: return {tr("Ancho"), tr("Canto")};
    case ShapeKind::TimberRectangularVariable: return {tr("Ancho"), tr("Canto inicial"), tr("Canto final")};
    default: return {};
    }
}

void BarDescribeDialog::updateTabChain() {
    // Rule of tabulacion_regla.md: by position on screen, spot by spot (top to bottom, left to right); the footer buttons last.
    // The tool buttons next to the combos are not tab stops.
    QList<QWidget*> chain;
    if (m_used) chain << m_used;
    for (int i = 0; i < 4; ++i) chain << m_typeGroup->button(i);
    const int type = m_typeUi[static_cast<std::size_t>(m_row)];
    if (type == 0) {
        chain << m_behaviour;
        for (QToolButton* b : m_materialButtons) chain << b;
        for (QToolButton* b : m_shapeButtons) chain << b;
        chain << m_series << m_profile;
        for (QRadioButton* r : m_arrangementRadios) chain << r;
        chain << m_stiffeners << field(QStringLiteral("stiffenerSpacing")) << field(QStringLiteral("stiffenerThickness"));
        for (QRadioButton* r : m_sectionTypeRadios) chain << r;
        for (ValueEdit* e : m_dimensionEdits) chain << e;
        for (const char* name : {"genericWidthLeft", "genericWidthRight", "genericDepthBottom", "genericDepthTop", "genericArea", "genericShearY",
                                 "genericShearZ", "genericInertiaY", "genericInertiaZ", "genericTorsion", "genericE", "genericPoisson",
                                 "genericAlpha", "genericWeight"})
            chain << field(QString::fromLatin1(name));
    } else {
        QWidget* page = m_typePages->widget(type);
        for (QWidget* w : page->findChildren<QWidget*>())
            if (w->focusPolicy() & Qt::TabFocus) chain << w;
    }
    chain << acceptButton() << cancelButton();
    m_tabChain.clear();
    for (QWidget* w : chain)
        if (w) m_tabChain << w;
    for (int i = 1; i < m_tabChain.size(); ++i) QWidget::setTabOrder(m_tabChain[i - 1], m_tabChain[i]);
}

// --- reactions ---------------------------------------------------------------------------------------------------------------------

namespace {
void normaliseArrangement(BarDescription& d) {
    const auto offered = arrangementsFor(d.materialIcon, d.shape);
    if (std::find(offered.begin(), offered.end(), d.arrangement) == offered.end()) d.arrangement = Arrangement::Simple;
    if (!stiffenersAllowed(d.materialIcon, d.arrangement)) d.stiffeners.present = false;
}
}  // namespace

void BarDescribeDialog::setInlineShape(ShapeKind kind, const std::vector<double>& dims) {
    BarDescription& d = current();
    Section s;
    s.kind = kind;
    s.dims = dims;
    s.reference = s8(makeReference(kind, dims));
    d.inlineSection = s;
    d.shape = kind;
    d.seriesRef.clear();
    d.profileRef.clear();
}

void BarDescribeDialog::applyProfile(const QString& seriesReference, const QString& profileReference) {
    BarDescription& d = current();
    d.seriesRef = s8(seriesReference);
    d.profileRef = s8(profileReference);
    if (const ProfileSeries* series = m_scratch.library().find(d.seriesRef)) {
        const auto& slotList = shapeSlots(d.materialIcon);
        const int slot = m_slotUi[static_cast<std::size_t>(m_row)];
        const bool mapped = slot >= 0 && slot < static_cast<int>(slotList.size()) && slotList[static_cast<std::size_t>(slot)].kind;
        d.shape = mapped ? *slotList[static_cast<std::size_t>(slot)].kind : series->kind;  // IPN under the I icon keeps the group
    }
    normaliseArrangement(d);
}

void BarDescribeDialog::onIconChosen(BarMaterialIcon icon) {
    if (m_syncing) return;
    BarDescription& cur = current();
    if (cur.materialIcon == icon) return;
    BarDescription d = defaultDescription(icon);
    d.id = cur.id;
    d.sectionId = cur.sectionId;
    d.materialId = cur.materialId;
    d.elementType = cur.elementType;
    d.behaviour = cur.behaviour;
    cur = d;
    m_slotUi[static_cast<std::size_t>(m_row)] = firstShapedSlot(icon);
    if (icon == BarMaterialIcon::Timber) {
        if (usesLibrary(icon)) {
            // series of the library
        } else {
            setInlineShape(ShapeKind::TimberRound, defaultInlineDims(ShapeKind::TimberRound));  // serie "Redondo" / Ø100 of the original
        }
    }
    if (usesLibrary(icon)) {
        // First series of the family for the first slot; the original's defaults when they exist in the library (IPE / IPE 80, ...).
        const auto family = familyOfIcon(icon);
        const auto& slotList = shapeSlots(icon);
        const std::optional<ShapeKind> wanted = slotList.empty() ? std::nullopt : slotList[static_cast<std::size_t>(firstShapedSlot(icon))].kind;
        const ProfileSeries* pick = nullptr;
        if (!cur.seriesRef.empty()) pick = m_scratch.library().find(cur.seriesRef);
        if (!pick)
            for (const ProfileSeries& s : m_scratch.library().series)
                if (offeredUnder(s, *family, wanted)) {
                    pick = &s;
                    break;
                }
        cur.seriesRef.clear();
        cur.profileRef.clear();
        if (pick && !pick->profiles.empty()) {
            const std::string wantedProfile = icon == BarMaterialIcon::RolledSteel && pick->reference == "IPE" && pick->findProfile("IPE 80")
                                                  ? "IPE 80"
                                                  : pick->profiles.front().reference;
            applyProfile(q(pick->reference), q(wantedProfile));
        }
    }
    normaliseArrangement(cur);
    syncUi();
}

void BarDescribeDialog::onShapeChosen(int slot) {
    if (m_syncing) return;
    const auto& slotList = shapeSlots(current().materialIcon);
    if (slot < 0 || slot >= static_cast<int>(slotList.size())) return;
    m_slotUi[static_cast<std::size_t>(m_row)] = slot;
    BarDescription& d = current();
    const auto& kind = slotList[static_cast<std::size_t>(slot)].kind;
    if (isTimberInline(d) && kind) {
        setInlineShape(*kind, defaultInlineDims(*kind));
    } else {
        // first series (and its first profile) of the family that holds the shape
        const auto family = familyOfIcon(d.materialIcon);
        const ProfileSeries* pick = nullptr;
        if (family)
            for (const ProfileSeries& s : m_scratch.library().series)
                if (offeredUnder(s, *family, kind)) {
                    pick = &s;
                    break;
                }
        d.seriesRef.clear();
        d.profileRef.clear();
        if (kind) d.shape = *kind;
        if (pick && !pick->profiles.empty()) applyProfile(q(pick->reference), q(pick->profiles.front().reference));
    }
    normaliseArrangement(d);
    syncUi();
}

void BarDescribeDialog::onSeriesChanged() {
    if (m_syncing || m_series->currentIndex() < 0) return;
    const QString reference = m_series->currentData().toString();
    const ProfileSeries* series = m_scratch.library().find(s8(reference));
    if (!series || series->profiles.empty()) return;
    applyProfile(reference, q(series->profiles.front().reference));
    syncUi();
}

void BarDescribeDialog::onProfileChanged() {
    if (m_syncing || m_profile->currentIndex() < 0) return;
    applyProfile(q(current().seriesRef), m_profile->currentData().toString());
    refreshUsedList();
}

void BarDescribeDialog::onTypeChanged(int index) {
    if (m_syncing) return;
    m_typeUi[static_cast<std::size_t>(m_row)] = index;
    m_typePages->setCurrentIndex(index);
    updateTabChain();
}

void BarDescribeDialog::onNewSeries() {
    const auto family = familyOfIcon(current().materialIcon).value_or(SectionFamily::HotRolledSteel);
    ProfileSeries series;
    series.family = family;
    const auto& slotList = shapeSlots(current().materialIcon);
    const int slot = m_slotUi[static_cast<std::size_t>(m_row)];
    series.kind = slot >= 0 && slot < static_cast<int>(slotList.size()) && slotList[static_cast<std::size_t>(slot)].kind
                      ? *slotList[static_cast<std::size_t>(slot)].kind
                      : current().shape;
    const auto edited = describeSink().editSeries(this, series, true, m_unit);
    if (!edited) return;
    const std::string error = addSeries(m_scratch.project, m_scratch.undo, *edited);
    if (!error.empty()) {
        warn(this, q(error));
        return;
    }
    applyProfile(q(edited->reference), q(edited->profiles.empty() ? std::string() : edited->profiles.front().reference));
    syncUi();
}

void BarDescribeDialog::onEditSeries() {
    const ProfileSeries* series = m_scratch.library().find(current().seriesRef);
    if (!series) return;
    const std::string previous = series->reference;
    const auto edited = describeSink().editSeries(this, *series, false, m_unit);
    if (!edited) return;
    const std::string error = editSeries(m_scratch.project, m_scratch.undo, previous, *edited);
    if (!error.empty()) {
        warn(this, q(error));
        return;
    }
    const ProfileSeries* now = m_scratch.library().find(edited->reference);
    const std::string profile = now && now->findProfile(current().profileRef) ? current().profileRef
                                                                               : (now && !now->profiles.empty() ? now->profiles.front().reference : std::string());
    applyProfile(q(edited->reference), q(profile));
    syncUi();
}

void BarDescribeDialog::onManageSeries() {
    describeSink().manageSeries(this, m_scratch);
    const ProfileSeries* series = m_scratch.library().find(current().seriesRef);
    if (!series) {  // the series was deleted or renamed: the first of the family takes its place
        current().seriesRef.clear();
        current().profileRef.clear();
        const auto family = familyOfIcon(current().materialIcon);
        for (const ProfileSeries& s : m_scratch.library().series)
            if (family && s.family == *family && !s.profiles.empty()) {
                applyProfile(q(s.reference), q(s.profiles.front().reference));
                break;
            }
    } else if (!series->findProfile(current().profileRef) && !series->profiles.empty()) {
        applyProfile(q(series->reference), q(series->profiles.front().reference));
    }
    syncUi();
}

void BarDescribeDialog::accept() {
    if (auto* focused = qobject_cast<ValueEdit*>(focusWidget()))
        if (!focused->commit()) return;
    for (std::size_t i = 0; i < m_entries.size(); ++i) {
        // Tirante, Pilar and Viga need modules that do not exist yet (tirantes, pilares y vigas de hormigon, T-P5-08).
        if (m_typeUi[i] != m_typeOriginal[i] && m_typeUi[i] != 0) {
            selectEntry(static_cast<int>(i));
            warn(this, tr("Esta función aún no está disponible."));
            return;
        }
        if (m_typeUi[i] == 1) {
            selectEntry(static_cast<int>(i));
            warn(this, tr("Esta función aún no está disponible."));
            return;
        }
        const BarDescription& d = m_entries[i].description;
        if (d.materialIcon == BarMaterialIcon::SpecialAluminium) {
            selectEntry(static_cast<int>(i));
            warn(this, tr("Esta función aún no está disponible."));
            return;
        }
        if (d.materialIcon == BarMaterialIcon::Generic &&
            (d.generic.shearAreaYM2 > d.generic.areaM2 + 1e-12 || d.generic.shearAreaZM2 > d.generic.areaM2 + 1e-12)) {
            // #65239 / #65240 (checked on accepting is NOT verified in the original, describir_barras.md section 6)
            selectEntry(static_cast<int>(i));
            warn(this, tr("Los valores de las áreas de cortante '%1' y '%2' no pueden ser mayores que el valor del área de la sección.")
                           .arg(tr("Avy"), tr("Avz")));
            return;
        }
        const std::string problem = validateDescription(d, m_scratch.library());
        if (!problem.empty()) {
            selectEntry(static_cast<int>(i));
            warn(this, q(problem));
            return;
        }
    }
    // "Perfiles de madera" (service class + fire resistance): shown when an entry becomes timber (PROVISIONAL trigger).
    for (std::size_t i = 0; i < m_entries.size(); ++i)
        if (m_entries[i].description.materialIcon == BarMaterialIcon::Timber && m_original[i].description.materialIcon != BarMaterialIcon::Timber) {
            describeSink().timberService(this, m_timberSettings);
            break;
        }
    CypeDialog::accept();
}

// --- TimberServiceDialog ------------------------------------------------------------------------------------------------------------

TimberServiceDialog::TimberServiceDialog(const TimberSettings& initial, QWidget* parent)
    : CypeDialog(tr("Perfiles de madera"), parent), m_initial(initial) {
    setFixedSize(560, 250);
    auto* box = new QFrame(this);
    auto* v = new QVBoxLayout(box);
    v->addWidget(headerBand(tr("Clase de servicio"), box));
    auto* radios = new QHBoxLayout;
    m_service = new QButtonGroup(this);
    for (int i = 1; i <= 3; ++i) {
        auto* radio = new QRadioButton(QString::number(i), box);
        radio->setChecked(i == initial.serviceClass);
        m_service->addButton(radio, i);
        radios->addWidget(radio);
    }
    radios->addStretch(1);
    v->addLayout(radios);
    m_text = new QLabel(box);
    m_text->setWordWrap(true);
    m_text->setStyleSheet(QStringLiteral("background: #D6F1FA; border: 1px solid #9ED3E6; padding: 6px;"));
    m_text->setMinimumHeight(64);
    m_text->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    v->addWidget(m_text);
    m_fire = new QCheckBox(tr("Comprobar la resistencia al fuego (CTE DB SI. Anejo E)"), box);
    m_fire->setChecked(initial.fireResistance);
    v->addWidget(m_fire);
    contentLayout()->addWidget(box);
    contentLayout()->addStretch(1);
    connect(m_service, &QButtonGroup::idToggled, this, [this](int, bool on) {
        if (on) m_text->setText(serviceText());
    });
    m_text->setText(serviceText());
    QHBoxLayout* footer = setupCustomFooter();
    auto* ok = new QPushButton(tr("Aceptar"), this);
    ok->setDefault(true);
    footer->addStretch(1);
    footer->addWidget(ok);
    footer->addStretch(1);
    connect(ok, &QPushButton::clicked, this, &QDialog::accept);
}

QString TimberServiceDialog::serviceText() const { return q(timberServiceClassText(m_service->checkedId())); }

QRadioButton* TimberServiceDialog::serviceRadio(int serviceClass) const {
    return qobject_cast<QRadioButton*>(m_service->button(serviceClass));
}

TimberSettings TimberServiceDialog::value() const {
    TimberSettings result = m_initial;
    result.serviceClass = m_service->checkedId();
    result.fireResistance = m_fire->isChecked();
    return result;
}

// --- DescribeMaterialDialog -------------------------------------------------------------------------------------------------------

DescribeMaterialDialog::DescribeMaterialDialog(const std::vector<GroupSpec>& groups, QWidget* parent)
    : CypeDialog(tr("Describir material"), parent) {
    int height = 120;
    for (const GroupSpec& spec : groups) height += spec.group == Group::Timber ? 124 : 78;
    setFixedSize(426, height);
    auto* v = new QVBoxLayout;
    v->setContentsMargins(4, 0, 4, 0);
    for (const GroupSpec& spec : groups) {
        QString title;
        switch (spec.group) {
        case Group::RolledSteel: title = tr("Acero laminado"); break;
        case Group::Concrete: title = tr("Hormigón para vigas de forjado"); break;  // title of the group (captures 164-166)
        case Group::Timber: title = tr("Madera"); break;
        }
        v->addWidget(headerBand(title, this));
        auto* project = new QRadioButton(tr("Material seleccionado en datos generales (%1)").arg(spec.projectMaterial), this);
        auto* own = new QRadioButton(tr("Seleccionar material para la barra"), this);
        project->setChecked(true);
        QFont f = project->font();
        f.setBold(true);
        project->setFont(f);
        auto* group = new QButtonGroup(this);
        group->addButton(project);
        group->addButton(own);
        auto* combo = new QComboBox(this);
        QComboBox* typeCombo = nullptr;
        QLabel* classLabel = nullptr;
        auto* comboRow = new QHBoxLayout;
        comboRow->setContentsMargins(20, 0, 0, 0);
        v->addWidget(project);
        v->addWidget(own);
        if (spec.group == Group::Timber) {
            // Type of timber (no label) and, below, "Clase resistente": the classes depend on the type. Only the classes of the softwood
            // type were captured; the other three types show an empty, disabled combo (TODO(captura)).
            typeCombo = new QComboBox(this);
            for (const std::string& t : timberMaterials()) typeCombo->addItem(q(t));
            typeCombo->setCurrentText(spec.projectTimberType.isEmpty() ? typeCombo->itemText(0) : spec.projectTimberType);
            auto* typeRow = new QHBoxLayout;
            typeRow->setContentsMargins(20, 0, 0, 0);
            typeRow->addWidget(typeCombo, 1);
            v->addLayout(typeRow);
            classLabel = new QLabel(tr("Clase resistente"), this);
            comboRow->addWidget(classLabel);
            const auto fillClasses = [combo, typeCombo, spec] {
                combo->clear();
                for (const std::string& c : timberStrengthClassesFor(typeCombo->currentText().toStdString())) combo->addItem(q(c));
                const int at = combo->findText(spec.projectMaterial);
                if (at >= 0) combo->setCurrentIndex(at);
                combo->setEnabled(combo->count() > 0);
            };
            fillClasses();
            connect(typeCombo, &QComboBox::currentIndexChanged, this, [fillClasses](int) { fillClasses(); });
            typeCombo->setVisible(false);
            classLabel->setVisible(false);
        } else {
            combo->addItems(spec.grades);
            if (spec.group == Group::Concrete) comboRow->addWidget(new QLabel(tr("Tipo:"), this));
        }
        combo->setVisible(false);  // appears when the second radio is ticked
        comboRow->addWidget(combo);
        comboRow->addStretch(1);
        v->addLayout(comboRow);
        connect(own, &QRadioButton::toggled, this, [combo, typeCombo, classLabel, project, own](bool on) {
            combo->setVisible(on);
            if (typeCombo) typeCombo->setVisible(on);
            if (classLabel) classLabel->setVisible(on);
            QFont bold = project->font();
            bold.setBold(!on);
            project->setFont(bold);
            QFont other = own->font();
            other.setBold(on);
            own->setFont(other);
        });
        m_rows.append({spec.group, project, own, combo, title, typeCombo});
    }
    v->addStretch(1);
    contentLayout()->addLayout(v, 1);
    setupFooter(tr("Aceptar"), tr("Cancelar"));
}

MaterialChoice DescribeMaterialDialog::choice(Group group) const {
    for (const Row& r : m_rows)
        if (r.group == group)
            return {!r.own->isChecked(), r.own->isChecked() ? r.combo->currentText() : QString(),
                    r.own->isChecked() && r.type ? r.type->currentText() : QString()};
    return {};
}
QRadioButton* DescribeMaterialDialog::projectRadio(Group group) const {
    for (const Row& r : m_rows)
        if (r.group == group) return r.project;
    return nullptr;
}
QRadioButton* DescribeMaterialDialog::ownRadio(Group group) const {
    for (const Row& r : m_rows)
        if (r.group == group) return r.own;
    return nullptr;
}
QComboBox* DescribeMaterialDialog::gradeCombo(Group group) const {
    for (const Row& r : m_rows)
        if (r.group == group) return r.combo;
    return nullptr;
}
QComboBox* DescribeMaterialDialog::timberTypeCombo() const {
    for (const Row& r : m_rows)
        if (r.group == Group::Timber) return r.type;
    return nullptr;
}
QStringList DescribeMaterialDialog::groupTitles() const {
    QStringList out;
    for (const Row& r : m_rows) out << r.title;
    return out;
}

}  // namespace cc::app
