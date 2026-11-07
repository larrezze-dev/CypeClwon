#include "BucklingTool.h"

#include <QAction>
#include <QActionGroup>
#include <QButtonGroup>
#include <QCheckBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPixmap>
#include <QRadioButton>
#include <QToolButton>
#include <QVBoxLayout>
#include <cmath>

#include "PiecePicture.h"
#include "RangedEdit.h"
#include "UserMessages.h"
#include "Viewport3D.h"
#include "core/buckling_edit.h"

namespace cc::app {

using namespace cc::core;

namespace {

constexpr int kButtonSize = 36;
constexpr int kFieldWidth = 62;
const QColor kPipe(30, 70, 190);  // blue of the member in the icons

void setBold(QWidget* widget, bool bold) {
    QFont font = widget->font();
    font.setBold(bold);
    widget->setFont(font);
}

/// Unchecking an "Asignar ..." header HIDES its controls but the dialog keeps its size (measured, pandeo.md 9.7).
void setShownKeepingSpace(QWidget* widget, bool shown) {
    QSizePolicy policy = widget->sizePolicy();
    policy.setRetainSizeWhenHidden(true);
    widget->setSizePolicy(policy);
    widget->setVisible(shown);
}

double round3(double value) { return std::round(value * 1000.0) / 1000.0; }

/// Grey header band with the check «Asignar ...» of every block of the dialogs (measured: orig_pandeo_05 / 21 / 41).
QWidget* headerBand(QCheckBox* check, QWidget* parent) {
    auto* band = new QFrame(parent);
    band->setObjectName(QStringLiteral("bucklingHeader"));
    band->setStyleSheet(QStringLiteral("QFrame#bucklingHeader { background: #DDDDDD; }"));
    auto* row = new QHBoxLayout(band);
    row->setContentsMargins(4, 0, 4, 0);
    row->addWidget(check);
    return band;
}

void hatch(QPainter& p, const QRectF& rect) {
    p.save();
    p.setClipRect(rect);
    p.setPen(QPen(QColor(90, 90, 90), 1));
    for (double x = rect.left() - rect.height(); x < rect.right(); x += 3.0) p.drawLine(QPointF(x, rect.bottom()), QPointF(x + rect.height(), rect.top()));
    p.restore();
    p.setPen(QPen(QColor(60, 60, 60), 1.2));
    p.drawLine(rect.topLeft(), rect.topRight());
}

void caption(QPainter& p, const QString& text) {
    QFont font = p.font();
    font.setPixelSize(8);
    font.setBold(true);
    p.setFont(font);
    p.setPen(Qt::black);
    p.drawText(QRectF(1, 1, kButtonSize - 4, 10), Qt::AlignLeft | Qt::AlignVCenter, text);
}

/// Icon of the 8 options of «Pandeo» (our own drawing after orig_pandeo_05: caption beta = .., the member in blue, its supports).
QIcon bucklingKindIcon(BucklingKind kind) {
    QPixmap pixmap(kButtonSize - 2, kButtonSize - 2);
    pixmap.fill(Qt::transparent);
    QPainter p(&pixmap);
    p.setRenderHint(QPainter::Antialiasing);
    static const char* captions[8] = {"β=0", "β=0.5", "β=0.7", "β=1.0", "β=2.0", "β=?", "Lk=?", ""};
    const int k = static_cast<int>(kind);
    if (kind != BucklingKind::Computed) caption(p, QString::fromUtf8(captions[k]));
    const double w = pixmap.width();
    const double cx = w / 2.0 + 2.0;
    if (kind == BucklingKind::Computed) {  // two-storey frame (the 8th icon of the original)
        p.setPen(QPen(kPipe, 2.0));
        p.drawRect(QRectF(5, 6, 22, 11));
        p.drawRect(QRectF(5, 17, 22, 11));
        hatch(p, QRectF(2, 28, 28, 4));
        return QIcon(pixmap);
    }
    const double top = 13.0, bottom = 27.0;
    if (kind == BucklingKind::None) {  // grey hatched block: nothing buckles
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(170, 175, 185));
        p.drawRect(QRectF(cx - 3, top, 6, bottom - top));
        hatch(p, QRectF(5, bottom, 26, 4));
        return QIcon(pixmap);
    }
    p.setPen(QPen(kPipe, 2.2));
    p.drawLine(QPointF(cx, top), QPointF(cx, bottom));
    p.drawLine(QPointF(cx - 4, top), QPointF(cx + 4, top));
    p.drawLine(QPointF(cx - 4, bottom), QPointF(cx + 4, bottom));
    const auto pin = [&](double y, bool up) {
        p.setPen(QPen(QColor(60, 60, 60), 1.0));
        p.setBrush(QColor(255, 220, 90));
        const double d = up ? -1.0 : 1.0;
        p.drawPolygon(QPolygonF({QPointF(cx, y), QPointF(cx - 4, y + d * 5), QPointF(cx + 4, y + d * 5)}));
    };
    switch (kind) {
        case BucklingKind::FixedFixed:
            hatch(p, QRectF(5, bottom + 1, 26, 3));
            p.setPen(QPen(QColor(60, 60, 60), 1.2));
            p.drawLine(QPointF(5, top - 2), QPointF(31, top - 2));
            break;
        case BucklingKind::PinnedFixed:
            hatch(p, QRectF(5, bottom + 1, 26, 3));
            pin(top, true);
            break;
        case BucklingKind::PinnedPinned:
            pin(top, true);
            pin(bottom + 1, false);
            break;
        case BucklingKind::Cantilever:
            hatch(p, QRectF(5, bottom + 1, 26, 3));
            break;
        case BucklingKind::UserBeta:
            hatch(p, QRectF(5, bottom + 1, 26, 3));
            break;
        case BucklingKind::UserLength:
            p.setPen(QPen(QColor(200, 30, 30), 1.0));
            p.drawLine(QPointF(cx + 8, top), QPointF(cx + 8, bottom));
            p.drawLine(QPointF(cx + 6, top + 2), QPointF(cx + 8, top));
            p.drawLine(QPointF(cx + 10, top + 2), QPointF(cx + 8, top));
            p.drawLine(QPointF(cx + 6, bottom - 2), QPointF(cx + 8, bottom));
            p.drawLine(QPointF(cx + 10, bottom - 2), QPointF(cx + 8, bottom));
            break;
        default: break;
    }
    return QIcon(pixmap);
}

/// Icon of the 6 options of «Pandeo lateral» (our own drawing after orig_pandeo_21: caption beta_v = .., horizontal member, its restraints).
QIcon lateralKindIcon(LateralKind kind) {
    QPixmap pixmap(kButtonSize - 2, kButtonSize - 2);
    pixmap.fill(Qt::transparent);
    QPainter p(&pixmap);
    p.setRenderHint(QPainter::Antialiasing);
    static const char* captions[6] = {"βv=0", "βv=0.5", "βv=1.0", "βv=2.0", "βv=?", "Lb=?"};
    caption(p, QString::fromUtf8(captions[static_cast<int>(kind)]));
    const double y = 22.0;
    if (kind == LateralKind::None) {
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(170, 175, 185));
        p.drawRect(QRectF(3, y - 3, 28, 6));
        hatch(p, QRectF(3, y + 3, 28, 4));
        return QIcon(pixmap);
    }
    p.setPen(QPen(kPipe, 2.2));
    p.drawLine(QPointF(4, y), QPointF(30, y));
    const auto restraint = [&](double x) {
        p.setPen(QPen(QColor(0, 150, 200), 1.4));
        p.drawLine(QPointF(x, y - 7), QPointF(x, y + 7));
        p.drawLine(QPointF(x - 2, y - 4), QPointF(x, y - 7));
        p.drawLine(QPointF(x + 2, y - 4), QPointF(x, y - 7));
    };
    switch (kind) {
        case LateralKind::HalfLength: restraint(5); restraint(17); restraint(29); break;
        case LateralKind::Length: restraint(5); restraint(29); break;
        case LateralKind::DoubleLength: restraint(5); break;
        case LateralKind::UserCoeff: restraint(17); break;
        case LateralKind::UserSpacing:
            restraint(5);
            restraint(29);
            p.setPen(QPen(QColor(200, 30, 30), 1.0));
            p.drawLine(QPointF(5, y + 8), QPointF(29, y + 8));
            break;
        default: break;
    }
    return QIcon(pixmap);
}

QString richTip(const QString& title, const QString& body) {
    return QStringLiteral("<p><b>%1</b></p><p>%2</p>").arg(title.toHtmlEscaped(), body.toHtmlEscaped().replace(QLatin1Char('\n'), QStringLiteral("<br>")));
}

QToolButton* makeKindButton(QWidget* parent, const QIcon& icon, const QString& tip) {
    auto* button = new QToolButton(parent);
    button->setCheckable(true);
    button->setAutoRaise(false);
    button->setFixedSize(kButtonSize, kButtonSize);
    button->setIconSize(QSize(kButtonSize - 2, kButtonSize - 2));
    button->setIcon(icon);
    button->setToolTip(tip);
    button->setStyleSheet(QStringLiteral(
        "QToolButton { background: #F4F4F4; border: 1px solid #B5B5B5; }"
        "QToolButton:checked { background: #BDBDBD; border: 1px solid #5A5A5A; }"));
    return button;
}

/// Picture of the two profiles of «Pandeo lateral» (our own drawing after orig_pandeo_21): grey profiles with the marks S (top flange) and I.
class LateralPicture : public QWidget {
public:
    explicit LateralPicture(QWidget* parent) : QWidget(parent) { setFixedSize(120, 56); }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        for (int i = 0; i < 2; ++i) {
            const double x = 12.0 + i * 52.0;
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(95, 95, 95));
            p.drawPolygon(QPolygonF({QPointF(x, 16), QPointF(x + 16, 10), QPointF(x + 18, 40), QPointF(x + 2, 46)}));
            p.setBrush(QColor(140, 140, 140));
            p.drawPolygon(QPolygonF({QPointF(x + 16, 10), QPointF(x + 26, 14), QPointF(x + 28, 42), QPointF(x + 18, 40)}));
            QFont f = font();
            f.setBold(true);
            p.setFont(f);
            p.setPen(Qt::black);
            p.drawText(QRectF(x + 20, 12, 12, 12), Qt::AlignCenter, QStringLiteral("S"));
            p.drawText(QRectF(x + 6, 44, 12, 12), Qt::AlignCenter, QStringLiteral("I"));
            p.setPen(QPen(QColor(0, 0, 230), 1.4));
            p.drawLine(QPointF(x + 8, 28), QPointF(x + 8, 4));
            p.setPen(QPen(QColor(220, 0, 0), 1.4));
            p.drawLine(QPointF(x + 8, 28), QPointF(x + 28, 26));
            p.setPen(QPen(QColor(0, 170, 0), 1.4));
            p.drawLine(QPointF(x + 8, 28), QPointF(x + 22, 22));
        }
    }
};

/// A "label: [field] unit" row.
QLabel* addLabel(QHBoxLayout* row, const QString& text, QWidget* parent) {
    auto* label = new QLabel(text, parent);
    row->addWidget(label);
    return label;
}

RangedEdit* makeEdit(const QString& label, ValueRange range, QWidget* parent, bool spaced, double initial = 1.0) {
    auto* edit = new RangedEdit(label, range, 3, parent);
    edit->setFixedWidth(kFieldWidth);
    edit->setValue(initial);  // measured defaults: 1.000 (3.000 for the spacing)
    if (spaced) edit->setBoundSuffix(QStringLiteral(" "));  // "entre 0.000  y 100.000 ." (measured)
    return edit;
}

}  // namespace

// ---- "Pandeo" ---------------------------------------------------------------------------------------------------------------

BucklingDialog::BucklingDialog(bool steel, QWidget* parent) : CypeDialog(tr("Pandeo"), parent), m_steel(steel) {
    setFixedWidth(365);
    auto* body = new QWidget(this);
    auto* outer = new QVBoxLayout(body);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(2);
    outer->addWidget(new PiecePicture(body), 0, Qt::AlignHCenter);
    for (int plane = 0; plane < 2; ++plane) buildPlane(body, outer, plane);
    if (m_steel)
        for (int plane = 0; plane < 2; ++plane) {
            const auto i = static_cast<std::size_t>(plane);
            m_assignMoment[i] = new QCheckBox(plane == 0 ? tr("Asignar coeficiente de momentos (Plano xy)") : tr("Asignar coeficiente de momentos (Plano xz)"), body);
            m_assignMoment[i]->setChecked(true);
            outer->addWidget(headerBand(m_assignMoment[i], body));
            auto* rowBox = new QWidget(body);
            auto* row = new QHBoxLayout(rowBox);
            row->setContentsMargins(4, 0, 4, 0);
            addLabel(row, tr("Coeficiente de momento equivalente Cm"), rowBox);
            m_moment[i] = makeEdit(tr("Coeficiente de momento equivalente Cm"), {kMomentCoeffMin, kMomentCoeffMax}, rowBox, true);
            row->addWidget(m_moment[i]);
            row->addStretch(1);
            outer->addWidget(rowBox);
            connect(m_assignMoment[i], &QCheckBox::toggled, rowBox, [rowBox](bool on) { setShownKeepingSpace(rowBox, on); });
        }
    m_criteria = new QCheckBox(tr("Asignar criterios para el análisis de pandeo"), body);
    m_criteria->setChecked(true);
    outer->addSpacing(4);
    outer->addWidget(headerBand(m_criteria, body));
    m_consider = new QCheckBox(tr("Considerar en el análisis de pandeo"), body);
    m_consider->setChecked(true);
    outer->addWidget(m_consider);
    m_update[0] = new QCheckBox(tr("Permitir actualizar los coeficientes de pandeo en el plano xy"), body);
    m_update[1] = new QCheckBox(tr("Permitir actualizar los coeficientes de pandeo en el plano xz"), body);
    outer->addWidget(m_update[0]);
    outer->addWidget(m_update[1]);
    for (QWidget* w : {static_cast<QWidget*>(m_consider), static_cast<QWidget*>(m_update[0]), static_cast<QWidget*>(m_update[1])})
        connect(m_criteria, &QCheckBox::toggled, w, [w](bool on) { setShownKeepingSpace(w, on); });
    outer->addStretch(1);
    contentLayout()->setContentsMargins(4, 4, 4, 4);
    contentLayout()->addWidget(body, 1);
    setupFooter(tr("Aceptar"), tr("Cancelar"));
    for (int plane = 0; plane < 2; ++plane) showFields(plane);
}

void BucklingDialog::buildPlane(QWidget* parent, QVBoxLayout* layout, int plane) {
    const auto i = static_cast<std::size_t>(plane);
    m_assign[i] = new QCheckBox(plane == 0 ? tr("Asignar longitud de pandeo (Plano xy)") : tr("Asignar longitud de pandeo (Plano xz)"), parent);
    m_assign[i]->setChecked(true);
    layout->addWidget(headerBand(m_assign[i], parent));
    const QString tips[8] = {tr("No considerar pandeo"),
                             tr("Barra biempotrada"),
                             tr("Barra apoyada en un extremo y empotrada en el otro."),
                             tr("Barra biapoyada"),
                             tr("Barra en voladizo"),
                             tr("Otros valores del coeficiente de pandeo"),
                             tr("Asignar longitud de pandeo"),
                             richTip(tr("Cálculo aproximado de longitudes de pandeo"),
                                     tr("Este cálculo está basado en fórmulas comúnmente aceptadas cuya validez está limitada a estructuras sensiblemente ortogonales. "
                                        "Posteriormente deberá indicar, a su criterio, si la estructura es traslacional o intraslacional.\n"
                                        "Además se aceptan las siguientes hipótesis:\n"
                                        "- Los soportes pandean simultáneamente.\n"
                                        "- Se desprecia el acortamiento elástico de los soportes.\n"
                                        "- Las vigas se comportan elásticamente y se unen de forma rígida a los soportes.\n"
                                        "- No se modifica la rigidez de las vigas por los esfuerzos normales.\n"
                                        "Existe una serie de limitaciones para la validez de los resultados de los coeficientes de pandeo que ofrece este método y que deberá tener en cuenta.\n"
                                        "- La existencia de nudos intermedios en barras en continuidad, a las que no acometen otras barras, invalida el método, por lo que, en estos casos, deben hacerse las correcciones manuales que se consideren oportunas.\n"
                                        "- Exige la clasificación de la estructura en traslacional o intraslacional, por lo que se debe tener cuidado en dicha definición.\n"
                                        "- Si la estructura introducida es un pórtico plano, los valores obtenidos son válidos en su plano, pudiendo no serlo en el plano perpendicular "
                                        "(por ejemplo, por que realmente existen elementos de arriostramiento no definidos en la estructura calculada)."))};
    m_group[i] = new QButtonGroup(this);
    m_group[i]->setExclusive(true);
    auto* buttons = new QHBoxLayout();
    buttons->setContentsMargins(6, 2, 6, 0);
    buttons->setSpacing(4);
    for (int k = 0; k < 8; ++k) {
        const auto kind = static_cast<BucklingKind>(k);
        QToolButton* button = makeKindButton(parent, bucklingKindIcon(kind), tips[k]);
        m_buttons[i][static_cast<std::size_t>(k)] = button;
        m_group[i]->addButton(button, k);
        buttons->addWidget(button);
        connect(button, &QToolButton::clicked, this, [this, plane, kind] { chooseKind(plane, kind); });
    }
    buttons->addStretch(1);
    layout->addLayout(buttons);
    m_buttons[i][static_cast<std::size_t>(BucklingKind::PinnedPinned)]->setChecked(true);

    // Row of the fields of the chosen option: fixed height so that the dialog keeps its size (measured).
    m_fieldRow[i] = new QWidget(parent);
    m_fieldRow[i]->setFixedHeight(34);
    auto* row = new QHBoxLayout(m_fieldRow[i]);
    row->setContentsMargins(6, 0, 6, 0);
    m_betaBox[i] = new QWidget(m_fieldRow[i]);
    auto* betaLayout = new QHBoxLayout(m_betaBox[i]);
    betaLayout->setContentsMargins(0, 0, 0, 0);
    addLabel(betaLayout, tr("Coeficiente de pandeo"), m_betaBox[i]);
    m_beta[i] = makeEdit(tr("Coeficiente de pandeo"), {kBucklingBetaMin, kBucklingBetaMax}, m_betaBox[i], true);
    betaLayout->addWidget(m_beta[i]);
    m_lengthBox[i] = new QWidget(m_fieldRow[i]);
    auto* lengthLayout = new QHBoxLayout(m_lengthBox[i]);
    lengthLayout->setContentsMargins(0, 0, 0, 0);
    addLabel(lengthLayout, tr("Longitud"), m_lengthBox[i]);
    m_length[i] = makeEdit(tr("Longitud"), {kBucklingLengthMin, kBucklingLengthMax}, m_lengthBox[i], false);
    lengthLayout->addWidget(m_length[i]);
    addLabel(lengthLayout, tr("m"), m_lengthBox[i]);
    m_computedBox[i] = new QWidget(m_fieldRow[i]);
    auto* computedLayout = new QHBoxLayout(m_computedBox[i]);
    computedLayout->setContentsMargins(0, 0, 0, 0);
    addLabel(computedLayout, tr("Tipo:"), m_computedBox[i]);
    m_translational[i] = new QRadioButton(tr("Traslacional"), m_computedBox[i]);
    m_intraslational[i] = new QRadioButton(tr("Intraslacional"), m_computedBox[i]);
    m_intraslational[i]->setChecked(true);  // measured default
    setBold(m_intraslational[i], true);
    connect(m_intraslational[i], &QRadioButton::toggled, this, [this, plane](bool on) {
        setBold(m_intraslational[static_cast<std::size_t>(plane)], on);
        setBold(m_translational[static_cast<std::size_t>(plane)], !on);
    });
    computedLayout->addWidget(m_translational[i]);
    computedLayout->addWidget(m_intraslational[i]);
    for (QWidget* box : {m_betaBox[i], m_lengthBox[i], m_computedBox[i]}) row->addWidget(box);
    row->addStretch(1);
    layout->addWidget(m_fieldRow[i]);
    connect(m_assign[i], &QCheckBox::toggled, this, [this, i](bool on) {
        for (QToolButton* b : m_buttons[i]) setShownKeepingSpace(b, on);
        setShownKeepingSpace(m_fieldRow[i], on);
    });
}

BucklingKind BucklingDialog::kind(int plane) const {
    const int id = m_group[static_cast<std::size_t>(plane)]->checkedId();
    return id < 0 ? BucklingKind::PinnedPinned : static_cast<BucklingKind>(id);
}

QToolButton* BucklingDialog::kindButton(int plane, BucklingKind kind) const {
    return m_buttons[static_cast<std::size_t>(plane)][static_cast<std::size_t>(kind)];
}

void BucklingDialog::showFields(int plane) {
    const auto i = static_cast<std::size_t>(plane);
    const BucklingKind k = kind(plane);
    m_betaBox[i]->setVisible(k == BucklingKind::UserBeta);
    m_lengthBox[i]->setVisible(k == BucklingKind::UserLength);
    m_computedBox[i]->setVisible(k == BucklingKind::Computed);
}

void BucklingDialog::chooseKind(int plane, BucklingKind wanted) {
    const auto i = static_cast<std::size_t>(plane);
    if (wanted == BucklingKind::Computed && m_computedBeta[i] < 0.0) {  // not loaded from a bar: the calculation does not exist yet
        // Keep the previous option (the click already checked the icon): the previous one is the one remembered in m_previous.
        m_buttons[i][static_cast<std::size_t>(m_previous[i])]->setChecked(true);
        showFields(plane);
        messages::show(this, messages::Kind::Information,
                       tr("El cálculo aproximado de longitudes de pandeo todavía no está disponible en esta versión."));
        return;
    }
    m_buttons[i][static_cast<std::size_t>(wanted)]->setChecked(true);
    m_previous[i] = wanted;
    showFields(plane);
}

void BucklingDialog::setBar(const BarBuckling& record) {
    for (int plane = 0; plane < 2; ++plane) {
        const auto i = static_cast<std::size_t>(plane);
        const BucklingPlane& value = plane == 0 ? record.xy : record.xz;
        m_computedBeta[i] = value.kind == BucklingKind::Computed ? value.beta : -1.0;
        chooseKind(plane, value.kind);
        if (value.kind == BucklingKind::UserBeta) m_beta[i]->setValue(round3(value.beta));
        if (value.kind == BucklingKind::UserLength) m_length[i]->setValue(round3(value.lengthM));
        (value.translational ? m_translational[i] : m_intraslational[i])->setChecked(true);
        if (m_moment[i]) m_moment[i]->setValue(round3(value.momentCoeff));
    }
    m_consider->setChecked(record.considerInAnalysis);
    m_update[0]->setChecked(record.updateXY);
    m_update[1]->setChecked(record.updateXZ);
}

BucklingEdit BucklingDialog::edit() const {
    BucklingEdit e;
    e.assignXY = m_assign[0]->isChecked();
    e.assignXZ = m_assign[1]->isChecked();
    e.assignMomentXY = m_steel && m_assignMoment[0]->isChecked();
    e.assignMomentXZ = m_steel && m_assignMoment[1]->isChecked();
    e.assignCriteria = m_criteria->isChecked();
    e.considerInAnalysis = m_consider->isChecked();
    e.updateXY = m_update[0]->isChecked();
    e.updateXZ = m_update[1]->isChecked();
    for (int plane = 0; plane < 2; ++plane) {
        const auto i = static_cast<std::size_t>(plane);
        BucklingPlaneEdit& value = plane == 0 ? e.xy : e.xz;
        value.kind = kind(plane);
        value.beta = value.kind == BucklingKind::Computed ? std::max(0.0, m_computedBeta[i]) : round3(m_beta[i]->value());
        value.lengthM = round3(m_length[i]->value());
        value.translational = m_translational[i]->isChecked();
        value.momentCoeff = m_moment[i] ? round3(m_moment[i]->value()) : 1.0;
    }
    return e;
}

bool BucklingDialog::commitAll() {
    for (RangedEdit* edit : {m_beta[0], m_beta[1], m_length[0], m_length[1], m_moment[0], m_moment[1]})
        if (edit && edit->isVisible() && edit->hasFocus() && !edit->commit()) return false;
    return true;
}

void BucklingDialog::accept() {
    if (!commitAll()) return;
    if (const auto issue = checkBucklingEdit(edit())) {
        messages::show(this, messages::Kind::Warning, QString::fromStdString(issue->text));
        return;
    }
    CypeDialog::accept();
}

// ---- "Pandeo lateral" ---------------------------------------------------------------------------------------------------------

LateralBucklingDialog::LateralBucklingDialog(QWidget* parent) : CypeDialog(tr("Pandeo lateral"), parent) {
    setFixedWidth(370);
    auto* body = new QWidget(this);
    auto* outer = new QVBoxLayout(body);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(2);
    outer->addWidget(new LateralPicture(body), 0, Qt::AlignHCenter);
    for (int flange = 0; flange < 2; ++flange) buildFlange(body, outer, flange);
    outer->addSpacing(4);
    m_assignC1 = new QCheckBox(tr("Asignar factor de momento crítico"), body);
    m_assignC1->setChecked(true);
    outer->addWidget(headerBand(m_assignC1, body));
    auto* c1Box = new QWidget(body);
    auto* row = new QHBoxLayout(c1Box);
    row->setContentsMargins(4, 0, 4, 0);
    addLabel(row, tr("Factor de modificación para el momento crítico C₁"), c1Box);
    m_c1 = makeEdit(QString(), {kMomentCoeffMin, kMomentCoeffMax}, c1Box, true);  // the original's message has an empty name
    row->addWidget(m_c1);
    row->addStretch(1);
    outer->addWidget(c1Box);
    connect(m_assignC1, &QCheckBox::toggled, c1Box, [c1Box](bool on) { setShownKeepingSpace(c1Box, on); });
    outer->addStretch(1);
    contentLayout()->setContentsMargins(4, 4, 4, 4);
    contentLayout()->addWidget(body, 1);
    setupFooter(tr("Aceptar"), tr("Cancelar"));
    for (int flange = 0; flange < 2; ++flange) showFields(flange);
}

void LateralBucklingDialog::buildFlange(QWidget* parent, QVBoxLayout* layout, int flange) {
    const auto i = static_cast<std::size_t>(flange);
    m_assign[i] = new QCheckBox(flange == 0 ? tr("Ala superior (S)") : tr("Ala inferior (I)"), parent);
    m_assign[i]->setChecked(true);
    layout->addWidget(headerBand(m_assign[i], parent));
    const QString tips[6] = {richTip(tr("No comprueba pandeo lateral"),
                                     tr("Permite indicar al programa que, para el ala escogida de las barras seleccionadas, no se realice la comprobación de pandeo lateral.")),
                             tr("Barra biempotrada"),
                             tr("Barra biapoyada"),
                             tr("Barra en voladizo"),
                             tr("Coeficiente de pandeo lateral"),
                             tr("Separación entre arriostramientos")};
    // Order of the icons of the original: 0 (no check), 0.5, 1.0, 2.0, user coefficient, user spacing.
    const LateralKind kinds[6] = {LateralKind::None, LateralKind::HalfLength, LateralKind::Length, LateralKind::DoubleLength, LateralKind::UserCoeff,
                                  LateralKind::UserSpacing};
    m_group[i] = new QButtonGroup(this);
    m_group[i]->setExclusive(true);
    auto* buttons = new QHBoxLayout();
    buttons->setContentsMargins(6, 2, 6, 0);
    buttons->setSpacing(4);
    for (int k = 0; k < 6; ++k) {
        QToolButton* button = makeKindButton(parent, lateralKindIcon(kinds[k]), tips[k]);
        m_buttons[i][static_cast<std::size_t>(k)] = button;
        m_group[i]->addButton(button, k);
        buttons->addWidget(button);
        connect(button, &QToolButton::clicked, this, [this, flange, k, kinds] { chooseKind(flange, kinds[k]); });
    }
    buttons->addStretch(1);
    layout->addLayout(buttons);
    m_buttons[i][0]->setChecked(true);

    auto* fields = new QWidget(parent);
    fields->setFixedHeight(66);  // two rows reserved: the dialog does not change size (measured)
    auto* rows = new QVBoxLayout(fields);
    rows->setContentsMargins(6, 4, 6, 0);
    rows->setSpacing(2);
    m_coeffBox[i] = new QWidget(fields);
    auto* first = new QHBoxLayout(m_coeffBox[i]);
    first->setContentsMargins(0, 0, 0, 0);
    m_firstLabel[i] = addLabel(first, tr("Coeficiente de pandeo lateral"), m_coeffBox[i]);
    m_coeff[i] = makeEdit(tr("Coeficiente de pandeo lateral"), {kLateralCoeffMin, kLateralCoeffMax}, m_coeffBox[i], true);
    m_spacing[i] = makeEdit(tr("Separación entre arriostramientos"), {kBucklingLengthMin, kBucklingLengthMax}, m_coeffBox[i], false, kDefaultLateralSpacingM);
    m_unit[i] = addLabel(first, tr("m"), m_coeffBox[i]);
    first->insertWidget(1, m_coeff[i]);
    first->insertWidget(2, m_spacing[i]);
    first->addStretch(1);
    rows->addWidget(m_coeffBox[i]);
    m_momentBox[i] = new QWidget(fields);
    auto* second = new QHBoxLayout(m_momentBox[i]);
    second->setContentsMargins(0, 0, 0, 0);
    m_momentLabel[i] = addLabel(second, tr("Coeficiente de momento equivalente Cm"), m_momentBox[i]);
    m_moment[i] = makeEdit(tr("Coeficiente de momento equivalente Cm"), {kMomentCoeffMin, kMomentCoeffMax}, m_momentBox[i], true);
    second->addWidget(m_moment[i]);
    second->addStretch(1);
    rows->addWidget(m_momentBox[i]);
    rows->addStretch(1);
    layout->addWidget(fields);
    connect(m_assign[i], &QCheckBox::toggled, this, [this, i, fields](bool on) {
        for (QToolButton* b : m_buttons[i]) setShownKeepingSpace(b, on);
        setShownKeepingSpace(fields, on);
    });
}

LateralKind LateralBucklingDialog::kind(int flange) const {
    const int id = m_group[static_cast<std::size_t>(flange)]->checkedId();
    return id < 0 ? LateralKind::None : static_cast<LateralKind>(id);
}

QToolButton* LateralBucklingDialog::kindButton(int flange, LateralKind kind) const {
    return m_buttons[static_cast<std::size_t>(flange)][static_cast<std::size_t>(kind)];
}

void LateralBucklingDialog::showFields(int flange) {
    const auto i = static_cast<std::size_t>(flange);
    const LateralKind k = kind(flange);
    const bool coeff = k == LateralKind::UserCoeff, spacing = k == LateralKind::UserSpacing;
    m_coeffBox[i]->setVisible(coeff || spacing);
    m_coeff[i]->setVisible(coeff);
    m_spacing[i]->setVisible(spacing);
    m_unit[i]->setVisible(spacing);
    m_firstLabel[i]->setText(spacing ? tr("Separación entre arriostramientos") : tr("Coeficiente de pandeo lateral"));
    m_momentBox[i]->setVisible(k != LateralKind::None);
}

void LateralBucklingDialog::chooseKind(int flange, LateralKind wanted) {
    m_buttons[static_cast<std::size_t>(flange)][static_cast<std::size_t>(wanted)]->setChecked(true);
    showFields(flange);
}

void LateralBucklingDialog::setBar(const BarBuckling& record) {
    for (int flange = 0; flange < 2; ++flange) {
        const auto i = static_cast<std::size_t>(flange);
        const LateralBuckling& value = flange == 0 ? record.top : record.bottom;
        chooseKind(flange, value.kind);
        m_coeff[i]->setValue(round3(value.coeff));
        m_spacing[i]->setValue(round3(value.spacingM));
        m_moment[i]->setValue(round3(value.momentCoeff));
    }
    m_c1->setValue(round3(record.c1));
}

LateralEdit LateralBucklingDialog::edit() const {
    LateralEdit e;
    e.assignTop = m_assign[0]->isChecked();
    e.assignBottom = m_assign[1]->isChecked();
    e.assignC1 = m_assignC1->isChecked();
    e.c1 = round3(m_c1->value());
    for (int flange = 0; flange < 2; ++flange) {
        const auto i = static_cast<std::size_t>(flange);
        LateralBuckling& value = flange == 0 ? e.top : e.bottom;
        value.kind = kind(flange);
        value.coeff = round3(m_coeff[i]->value());
        value.spacingM = round3(m_spacing[i]->value());
        value.momentCoeff = round3(m_moment[i]->value());
    }
    return e;
}

bool LateralBucklingDialog::commitAll() {
    for (RangedEdit* edit : {m_coeff[0], m_coeff[1], m_spacing[0], m_spacing[1], m_moment[0], m_moment[1], m_c1})
        if (edit->isVisible() && edit->hasFocus() && !edit->commit()) return false;
    return true;
}

void LateralBucklingDialog::accept() {
    if (!commitAll()) return;
    if (const auto issue = checkLateralEdit(edit())) {
        messages::show(this, messages::Kind::Warning, QString::fromStdString(issue->text));
        return;
    }
    CypeDialog::accept();
}

// ---- tool -------------------------------------------------------------------------------------------------------------------

BucklingTool::BucklingTool(QObject* parent) : QObject(parent) {
    auto* group = new QActionGroup(this);
    group->setExclusionPolicy(QActionGroup::ExclusionPolicy::ExclusiveOptional);
    const auto make = [this, group](const QString& text, Mode mode) {
        auto* action = new QAction(text, this);
        action->setCheckable(true);
        action->setEnabled(false);
        group->addAction(action);
        connect(action, &QAction::toggled, this, [this, mode](bool on) {
            if (on) start(mode);
            else if (m_mode == mode) stop();
        });
        return action;
    };
    m_beta = make(tr("Pandeo"), Mode::Beta);
    m_lateral = make(tr("Pandeo lateral"), Mode::Lateral);
    // Measured tooltips: title with the shortcut, body = the description of the original (#64628 / #64629).
    m_beta->setToolTip(tr("Pandeo (Ctrl+p)"));
    m_beta->setStatusTip(tr("Asignación o cálculo de los coeficientes de esbeltez para el cálculo a pandeo de las barras."));
    m_lateral->setToolTip(tr("Pandeo lateral"));
    m_lateral->setStatusTip(tr("Edición de los parámetros de pandeo lateral de las barras (distancia entre arriostramientos y coeficiente de momentos)."));
}

QString BucklingTool::statusText(Mode mode) {
    const QString group = tr(" - LMB: Seleccione un grupo de elementos. - RMB: Actuar sobre los elementos seleccionados.");
    switch (mode) {
        case Mode::Beta: return tr("Barras - Pandeo |") + QLatin1Char(' ') + group;
        case Mode::Lateral: return tr("Barras - Pandeo lateral |") + QLatin1Char(' ') + group;
        case Mode::None: break;
    }
    return tr("Seleccione una opción del menú.");
}

void BucklingTool::pruneViewports() {
    m_viewports.removeIf([](const QPointer<Viewport3D>& v) { return v.isNull(); });
}

int BucklingTool::run(QDialog& dialog) { return m_runner ? m_runner(dialog) : dialog.exec(); }

void BucklingTool::setProject(Project* project, UndoStack* undo) {
    deactivate();
    m_project = project;
    m_undo = undo;
    for (const QPointer<Viewport3D>& viewport : m_viewports)
        if (viewport) viewport->setBuckling(m_project ? &m_project->buckling : nullptr);
    updateEnabled();
}

void BucklingTool::addViewport(Viewport3D* viewport) {
    if (!viewport || m_viewports.contains(viewport)) return;
    m_viewports.push_back(viewport);
    viewport->setBuckling(m_project ? &m_project->buckling : nullptr);
    connect(viewport, &Viewport3D::actOnSelection, this, [this, viewport] { onActOnSelection(viewport); });
    connect(viewport, &Viewport3D::escapePressed, this, [this] { deactivate(); });
    if (m_mode != Mode::None) configureViewports();
}

void BucklingTool::updateEnabled() {
    const bool bars = m_project && !m_project->structure.bars.empty();
    for (QAction* action : {m_beta, m_lateral}) {
        action->setEnabled(bars);
        if (!bars && action->isChecked()) action->setChecked(false);
    }
}

void BucklingTool::structureChanged() {
    updateEnabled();
    pruneViewports();
    for (const QPointer<Viewport3D>& viewport : m_viewports) viewport->refreshStructure();
}

void BucklingTool::start(Mode mode) {
    if (!m_project || mode == m_mode) return;
    emit aboutToActivate();
    m_mode = mode;
    configureViewports();
    emit modeChanged(m_mode);
    emit statusChanged(statusText(m_mode));  // after the viewports: setSelectionMode announces its own, generic hint
}

void BucklingTool::deactivate() {
    for (QAction* action : {m_beta, m_lateral})
        if (action->isChecked()) action->setChecked(false);
    stop();
}

void BucklingTool::stop() {
    if (m_mode == Mode::None) return;
    m_mode = Mode::None;
    configureViewports();
    emit modeChanged(m_mode);
    emit statusChanged(statusText(m_mode));
}

void BucklingTool::configureViewports() {
    pruneViewports();
    const bool on = m_mode != Mode::None;
    for (const QPointer<Viewport3D>& viewport : m_viewports) {
        viewport->setSelectionMode(false);
        viewport->setBarAxesVisible(on);  // local axes at one end of every bar (#65297)
        viewport->setBucklingLabels(m_mode == Mode::Beta ? Viewport3D::BucklingLabels::Beta
                                    : m_mode == Mode::Lateral ? Viewport3D::BucklingLabels::Lateral
                                                              : Viewport3D::BucklingLabels::None);
        if (!on) continue;
        viewport->setSelectionFilter(SelectionFilter::only(ElementKind::Bar));
        viewport->setSelectionMode(true);
        viewport->setFocus();
    }
}

bool BucklingTool::anySteel(const std::vector<int>& barIds) const {
    for (int id : barIds) {
        const Bar* bar = m_project->structure.findBar(id);
        const Material* material = bar ? m_project->structure.findMaterial(bar->materialId) : nullptr;
        if (material && material->kind == MaterialKind::Steel) return true;
    }
    return false;
}

namespace {
std::vector<int> existing(const Project& project, const std::vector<int>& ids) {
    std::vector<int> out;
    for (int id : ids)
        if (project.structure.findBar(id)) out.push_back(id);
    return out;
}
}  // namespace

bool BucklingTool::editBuckling(const std::vector<int>& barIds) {
    if (!m_project || !m_undo) return false;
    const std::vector<int> bars = existing(*m_project, barIds);
    if (bars.empty()) return false;
    BucklingDialog dialog(anySteel(bars), m_dialogParent);
    dialog.setBar(bucklingOf(m_project->buckling, bars.front()));
    if (run(dialog) != QDialog::Accepted) return false;
    const std::size_t before = m_undo->count();
    if (const auto issue = assignBuckling(*m_project, *m_undo, bars, dialog.edit())) {
        messages::show(m_dialogParent, messages::Kind::Warning, QString::fromStdString(issue->text));
        return false;
    }
    return m_undo->count() != before;
}

bool BucklingTool::editLateral(const std::vector<int>& barIds) {
    if (!m_project || !m_undo) return false;
    const std::vector<int> bars = existing(*m_project, barIds);
    if (bars.empty()) return false;
    LateralBucklingDialog dialog(m_dialogParent);
    dialog.setBar(bucklingOf(m_project->buckling, bars.front()));
    if (run(dialog) != QDialog::Accepted) return false;
    const std::size_t before = m_undo->count();
    if (const auto issue = assignLateralBuckling(*m_project, *m_undo, bars, dialog.edit())) {
        messages::show(m_dialogParent, messages::Kind::Warning, QString::fromStdString(issue->text));
        return false;
    }
    return m_undo->count() != before;
}

void BucklingTool::onActOnSelection(Viewport3D* viewport) {
    if (m_mode == Mode::None || !m_project || !m_undo) return;
    const std::vector<int> bars = viewport->selection().ids(ElementKind::Bar);
    if (bars.empty()) return;
    if (m_mode == Mode::Beta) editBuckling(bars);
    else editLateral(bars);
    viewport->clearSelection();
    emit statusChanged(statusText(m_mode));
}

}  // namespace cc::app
