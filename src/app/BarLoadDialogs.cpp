#include "BarLoadDialogs.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPixmap>
#include <QPolygonF>
#include <QRadioButton>
#include <QToolButton>
#include <QVBoxLayout>
#include <cmath>
#include <numbers>

#include "NodalLoadDialogs.h"
#include "UserMessages.h"

namespace cc::app {

using namespace cc::core;

namespace {

constexpr int kTypeButtonWidth = 36, kTypeButtonHeight = 30;
constexpr int kDirectionButtonSize = 46;

bool readNumber(const QString& text, double& value) {
    QString t = text.trimmed();
    t.replace(QLatin1Char(','), QLatin1Char('.'));
    bool ok = false;
    value = t.toDouble(&ok);
    return ok && std::isfinite(value);
}

void setBold(QWidget* widget) {
    QFont font = widget->font();
    font.setBold(true);
    widget->setFont(font);
}

QFrame* separator(QWidget* parent) {
    auto* line = new QFrame(parent);
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);
    return line;
}

/// Label of a field as the original writes it (the core keeps the es-ES source text).
QString fieldLabel(const std::string& text) {
    static const std::map<std::string, QString (*)()> table = {
        {"Valor", [] { return QObject::tr("Valor"); }},
        {"Valor inicial", [] { return QObject::tr("Valor inicial"); }},
        {"Valor final", [] { return QObject::tr("Valor final"); }},
        {"Valor 1", [] { return QObject::tr("Valor 1"); }},
        {"Valor 2", [] { return QObject::tr("Valor 2"); }},
        {"Distancia", [] { return QObject::tr("Distancia"); }},
        {"Posición inicial", [] { return QObject::tr("Posición inicial"); }},
        {"Posición final", [] { return QObject::tr("Posición final"); }},
        {"Ángulo", [] { return QObject::tr("Ángulo"); }},
        {"Ángulo en el plano 'XZ' local", [] { return QObject::tr("Ángulo en el plano 'XZ' local"); }},
        {"Ángulo con el plano 'XZ' local", [] { return QObject::tr("Ángulo con el plano 'XZ' local"); }},
    };
    const auto it = table.find(text);
    return it == table.end() ? QString::fromStdString(text) : it->second();
}

QString valueUnit(BarLoadType type) {
    if (type == BarLoadType::Point) return QObject::tr("kN");
    if (type == BarLoadType::PointMoment) return QObject::tr("kN·m");
    if (isTemperatureLoad(type)) return QObject::tr("°C");
    return QObject::tr("kN/m");
}

/// Common painter of the small pictograms (2x pixel ratio).
struct IconCanvas {
    IconCanvas(int w, int h) : pixmap(w * 2, h * 2) {
        pixmap.setDevicePixelRatio(2);
        pixmap.fill(Qt::transparent);
        painter.begin(&pixmap);
        painter.setRenderHint(QPainter::Antialiasing);
    }
    ~IconCanvas() {
        if (painter.isActive()) painter.end();
    }
    QIcon icon() {
        painter.end();
        return QIcon(pixmap);
    }
    QPixmap pixmap;
    QPainter painter;
};

void arrowHead(QPainter& p, const QPointF& tip, const QPointF& dir, double len, double half, const QColor& colour) {
    const QPointF n(-dir.y(), dir.x());
    QPolygonF poly;
    poly << tip << tip - dir * len + n * half << tip - dir * len - n * half;
    p.setBrush(colour);
    p.setPen(colour);
    p.drawPolygon(poly);
}

}  // namespace

// ---- pictograms -------------------------------------------------------------------------------------------------------------

QIcon barLoadTypeIcon(BarLoadType type) {
    constexpr int w = 30, h = 22;
    IconCanvas c(w, h);
    QPainter& p = c.painter;
    const QColor red(215, 20, 20);
    const double base = 16.0;
    p.setPen(QPen(Qt::black, 1.2));
    p.drawLine(QPointF(3, base), QPointF(w - 3, base));
    p.drawLine(QPointF(3, base - 2.5), QPointF(3, base + 2.5));
    p.drawLine(QPointF(w - 3, base - 2.5), QPointF(w - 3, base + 2.5));
    p.setPen(QPen(red, 1.2));
    const auto hatch = [&](double x0, double x1, double h0, double h1) {
        p.setPen(QPen(red, 1.1));
        p.drawLine(QPointF(x0, base - h0), QPointF(x1, base - h1));
        const int n = 7;
        for (int i = 0; i <= n; ++i) {
            const double t = static_cast<double>(i) / n;
            const double x = x0 + (x1 - x0) * t;
            p.drawLine(QPointF(x, base), QPointF(x, base - (h0 + (h1 - h0) * t)));
        }
    };
    switch (type) {
        case BarLoadType::Point:
            p.setPen(QPen(red, 1.4));
            p.drawLine(QPointF(w / 2.0, 4), QPointF(w / 2.0, base - 3));
            arrowHead(p, QPointF(w / 2.0, base - 0.5), QPointF(0, 1), 5, 2.4, red);
            break;
        case BarLoadType::Uniform: hatch(4, w - 4, 8, 8); break;
        case BarLoadType::Strip: hatch(8, w - 8, 8, 8); break;
        case BarLoadType::TriangularLeft: hatch(4, w - 4, 10, 0); break;
        case BarLoadType::TriangularRight: hatch(4, w - 4, 0, 10); break;
        case BarLoadType::Trapezoidal: hatch(4, w - 4, 4, 10); break;
        case BarLoadType::PointMoment: {
            p.setPen(QPen(red, 1.4));
            p.setBrush(Qt::NoBrush);
            p.drawArc(QRectF(w / 2.0 - 8, base - 9, 16, 16), 20 * 16, 250 * 16);
            arrowHead(p, QPointF(w / 2.0 + 7.5, base - 1.5), QPointF(0.2, 1), 4.5, 2.2, red);
            break;
        }
        case BarLoadType::UniformTemperature: {
            QFont font = p.font();
            font.setPixelSize(10);
            font.setBold(true);
            p.setFont(font);
            p.setPen(red);
            p.drawText(QRectF(0, 1, w, 12), Qt::AlignCenter, QStringLiteral("Δt"));
            break;
        }
        case BarLoadType::VariableTemperature: {
            QFont font = p.font();
            font.setPixelSize(8);
            font.setBold(true);
            p.setFont(font);
            p.setPen(red);
            p.drawText(QRectF(0, 0, w, 9), Qt::AlignCenter, QStringLiteral("Δt 2"));
            p.drawLine(QPointF(7, 10.5), QPointF(w - 7, 10.5));
            p.drawText(QRectF(0, 10.5, w, 9), Qt::AlignCenter, QStringLiteral("Δt 1"));
            break;
        }
    }
    return c.icon();
}

QIcon barLoadDirectionIcon(LoadDirection direction, bool local, bool moment) {
    if (!local && !moment) return loadDirectionIcon(direction);
    constexpr int size = 44;
    IconCanvas c(size, size);
    QPainter& p = c.painter;
    const QPointF o(22, 24);
    const QColor blue(0, 40, 220), red(235, 20, 20);
    // Screen directions of the axes of the pictogram: global (X right, Y towards the top left, Z up) or local (x right, z towards the top left, y down).
    const QPointF ax[3] = {QPointF(1, 0), local ? QPointF(-0.36, 0.93) : QPointF(-0.72, -0.69), local ? QPointF(-0.72, -0.69) : QPointF(0, -1)};
    const double reach[3] = {19.0, 17.0, 18.0};
    p.setPen(QPen(blue, 1.4));
    for (int i = 0; i < 3; ++i) p.drawLine(o - ax[i] * reach[i], o + ax[i] * reach[i]);
    for (int i = 0; i < 3; ++i) arrowHead(p, o + ax[i] * reach[i], ax[i], 5, 2.2, blue);
    QFont font = p.font();
    font.setPixelSize(9);
    font.setBold(true);
    p.setFont(font);
    p.setPen(blue);
    if (local) {
        p.drawText(QPointF(o.x() + 17, o.y() + 11), QStringLiteral("x"));
        p.drawText(QPointF(o.x() - 20, o.y() - 12), QStringLiteral("z"));
        p.drawText(QPointF(o.x() - 12, o.y() + 20), QStringLiteral("y"));
    } else {
        p.drawText(QPointF(o.x() + 17, o.y() + 11), QStringLiteral("x"));
        p.drawText(QPointF(o.x() - 20, o.y() - 12), QStringLiteral("Y"));
        p.drawText(QPointF(o.x() + 4, o.y() - 15), QStringLiteral("z"));
    }
    int axis = 0;
    double sign = 1.0;
    switch (direction) {
        case LoadDirection::PlusX: axis = 0; sign = 1; break;
        case LoadDirection::PlusY: axis = local ? 1 : 1; sign = 1; break;
        case LoadDirection::PlusZ: axis = 2; sign = 1; break;
        case LoadDirection::MinusX: axis = 0; sign = -1; break;
        case LoadDirection::MinusY: axis = 1; sign = -1; break;
        case LoadDirection::MinusZ: axis = 2; sign = -1; break;
        case LoadDirection::UserVector: axis = 1; sign = 1; break;
    }
    QPointF dir = ax[axis] * sign;
    if (direction == LoadDirection::UserVector) {
        p.setPen(QPen(QColor(255, 235, 0), 1.2));
        p.setBrush(QColor(255, 255, 160, 120));
        p.drawRect(QRectF(o.x() - 4, o.y() - 22, 24, 16));
        dir = QPointF(0.6, -0.8);
    }
    const QPointF from = o - dir * 16.0;
    p.setPen(QPen(red, 3.2, Qt::SolidLine, Qt::FlatCap));
    p.drawLine(from, o - dir * (moment ? 9.0 : 3.0));
    arrowHead(p, o, dir, 7, 4, red);
    if (moment) arrowHead(p, o - dir * 6.0, dir, 7, 4, red);  // measured: the moment has a double arrow head
    return c.icon();
}

QIcon barTemperatureDirectionIcon(TemperatureDirection direction) {
    constexpr int w = 44, h = 44;
    IconCanvas c(w, h);
    QPainter& p = c.painter;
    const QColor blue(0, 40, 220);
    const QPointF o(24, 22);
    p.setPen(QPen(blue, 1.3));
    p.drawLine(QPointF(4, o.y()), QPointF(41, o.y()));
    arrowHead(p, QPointF(41, o.y()), QPointF(1, 0), 5, 2.2, blue);
    const bool z = direction != TemperatureDirection::PlusY;
    p.drawLine(QPointF(o.x(), 40), QPointF(o.x(), 3));
    arrowHead(p, QPointF(o.x(), 3), QPointF(0, -1), 5, 2.2, blue);
    p.setPen(QPen(Qt::black, 4.0, Qt::SolidLine, Qt::FlatCap));  // the I profile
    p.drawLine(QPointF(o.x() - 7, o.y() - 8), QPointF(o.x() + 7, o.y() - 8));
    p.drawLine(QPointF(o.x() - 7, o.y() + 8), QPointF(o.x() + 7, o.y() + 8));
    p.drawLine(QPointF(o.x(), o.y() - 8), QPointF(o.x(), o.y() + 8));
    QFont font = p.font();
    font.setPixelSize(8);
    font.setBold(true);
    p.setFont(font);
    p.setPen(blue);
    p.drawText(QPointF(o.x() + 3, 8), z ? QStringLiteral("z") : QStringLiteral("y"));
    p.drawText(QPointF(37, o.y() + 9), z ? QStringLiteral("y") : QStringLiteral("z"));
    p.setPen(QColor(0, 0, 0));
    p.drawText(QPointF(2, o.y() - 10), direction == TemperatureDirection::PlusY ? QStringLiteral("T2") : QStringLiteral("T1"));
    p.drawText(QPointF(2, o.y() + 17), direction == TemperatureDirection::PlusY ? QStringLiteral("T1") : QStringLiteral("T2"));
    if (direction == TemperatureDirection::Angle) {
        p.setPen(QPen(QColor(255, 220, 0), 1.6));
        p.drawLine(QPointF(o.x() - 10, o.y() + 12), QPointF(o.x() + 10, o.y() - 12));
    }
    return c.icon();
}

// ---- BarLoadDialog ----------------------------------------------------------------------------------------------------------

BarLoadDialog::BarLoadDialog(const QString& title, const LoadCaseSet& cases, const BarLoadData& initial, double lengthM, QWidget* parent)
    : CypeDialog(title, parent), m_cases(cases), m_lengthM(lengthM), m_type(initial.type), m_axes(initial.axes), m_direction(initial.direction),
      m_temperatureDirection(initial.temperatureDirection), m_initial(initial) {
    if (!allowsGlobalAxes(m_type)) m_axes = BarLoadAxes::Local;
    auto* body = new QWidget(this);
    auto* column = new QVBoxLayout(body);
    column->setContentsMargins(4, 0, 4, 0);
    column->setSpacing(5);

    auto* caseRow = new QHBoxLayout();
    caseRow->addWidget(new QLabel(tr("Hipótesis"), body));
    m_case = new QComboBox(body);
    m_case->setObjectName(QStringLiteral("loadCase"));
    m_case->setMinimumWidth(98);
    fillLoadCaseCombo(m_case, m_cases, initial.loadCaseId);
    caseRow->addWidget(m_case);
    caseRow->addStretch(1);
    column->addLayout(caseRow);
    column->addWidget(separator(body));

    auto* typeHeading = new QLabel(tr("Tipo de carga"), body);
    setBold(typeHeading);
    column->addWidget(typeHeading);
    auto* types = new QHBoxLayout();
    types->setSpacing(6);
    for (int i = 0; i < kBarLoadTypeCount; ++i) {
        const BarLoadType t = kBarLoadTypeButtonOrder[i];
        auto* button = new QToolButton(body);
        button->setObjectName(QStringLiteral("type%1").arg(static_cast<int>(t)));
        button->setIcon(barLoadTypeIcon(t));
        button->setIconSize(QSize(30, 22));
        button->setFixedSize(kTypeButtonWidth, kTypeButtonHeight);
        button->setCheckable(true);
        button->setAutoExclusive(false);
        button->setFocusPolicy(Qt::NoFocus);  // measured: the type buttons are not tab stops
        button->setToolTip(tr(barLoadTypeName(t).c_str()));
        button->setStyleSheet(QStringLiteral("QToolButton{background:#F4F4F4;border:1px solid #D0D0D0;}"
                                             "QToolButton:checked{background:#BDBDBD;border:1px solid #8A8A8A;}"));
        connect(button, &QToolButton::clicked, this, [this, t] { setType(t); });
        m_typeButtons[static_cast<std::size_t>(i)] = button;
        types->addWidget(button);
    }
    types->addStretch(1);
    column->addLayout(types);

    // Values and positions. The block keeps its height whatever the type (the groups below do not move).
    const auto makeField = [&](BarLoadField field, QWidget* parent, int width) {
        auto* label = new QLabel(parent);
        auto* edit = new QLineEdit(parent);
        edit->setAlignment(Qt::AlignRight);
        edit->setFixedWidth(width);
        auto* unit = new QLabel(parent);
        m_labels[field] = label;
        m_edits[field] = edit;
        m_units[field] = unit;
        return std::array<QWidget*, 3>{label, edit, unit};
    };
    auto* values = new QWidget(body);
    values->setMinimumHeight(86);
    auto* valuesColumn = new QVBoxLayout(values);
    valuesColumn->setContentsMargins(0, 2, 0, 0);
    valuesColumn->setSpacing(4);
    auto* valueRow = new QHBoxLayout();
    valueRow->setSpacing(6);
    m_value1Block = new QWidget(values);
    auto* v1 = new QHBoxLayout(m_value1Block);
    v1->setContentsMargins(0, 0, 0, 0);
    for (QWidget* w : makeField(BarLoadField::Value1, m_value1Block, 62)) v1->addWidget(w);
    m_value2Block = new QWidget(values);
    auto* v2 = new QHBoxLayout(m_value2Block);
    v2->setContentsMargins(0, 0, 0, 0);
    for (QWidget* w : makeField(BarLoadField::Value2, m_value2Block, 62)) v2->addWidget(w);
    valueRow->addWidget(m_value1Block);
    valueRow->addWidget(m_value2Block);
    valueRow->addStretch(1);
    valuesColumn->addLayout(valueRow);
    m_relative = new QCheckBox(tr("Posición relativa"), values);
    m_relative->setObjectName(QStringLiteral("relative"));
    m_relative->setChecked(initial.relative);
    valuesColumn->addWidget(m_relative);
    m_positionRows = new QWidget(values);
    auto* posRow = new QHBoxLayout(m_positionRows);
    posRow->setContentsMargins(0, 0, 0, 0);
    posRow->setSpacing(6);
    m_startBlock = new QWidget(m_positionRows);
    auto* sb = new QHBoxLayout(m_startBlock);
    sb->setContentsMargins(0, 0, 0, 0);
    for (QWidget* w : makeField(BarLoadField::Start, m_startBlock, 52)) sb->addWidget(w);
    m_endBlock = new QWidget(m_positionRows);
    auto* eb = new QHBoxLayout(m_endBlock);
    eb->setContentsMargins(0, 0, 0, 0);
    for (QWidget* w : makeField(BarLoadField::End, m_endBlock, 52)) eb->addWidget(w);
    posRow->addWidget(m_startBlock);
    posRow->addWidget(m_endBlock);
    posRow->addStretch(1);
    valuesColumn->addWidget(m_positionRows);
    valuesColumn->addStretch(1);
    column->addWidget(values);

    // Axes.
    m_axesGroup = new QWidget(body);
    auto* axes = new QVBoxLayout(m_axesGroup);
    axes->setContentsMargins(0, 0, 0, 0);
    axes->setSpacing(4);
    axes->addWidget(separator(m_axesGroup));
    auto* axesHeading = new QLabel(tr("Ejes sobre los que se define la dirección de la carga"), m_axesGroup);
    setBold(axesHeading);
    axes->addWidget(axesHeading);
    m_globalRadio = new QRadioButton(tr("Ejes globales"), m_axesGroup);
    m_globalRadio->setObjectName(QStringLiteral("globalAxes"));
    m_localRadio = new QRadioButton(tr("Ejes locales de la barra"), m_axesGroup);
    m_localRadio->setObjectName(QStringLiteral("localAxes"));
    axes->addWidget(m_globalRadio);
    axes->addWidget(m_localRadio);
    column->addWidget(m_axesGroup);

    // Direction.
    m_directionGroup = new QWidget(body);
    auto* direction = new QVBoxLayout(m_directionGroup);
    direction->setContentsMargins(0, 0, 0, 0);
    direction->setSpacing(4);
    direction->addWidget(separator(m_directionGroup));
    m_directionTitle = new QLabel(m_directionGroup);
    setBold(m_directionTitle);
    direction->addWidget(m_directionTitle);
    m_forceButtons = new QWidget(m_directionGroup);
    auto* buttons = new QHBoxLayout(m_forceButtons);
    buttons->setContentsMargins(0, 0, 0, 0);
    buttons->setSpacing(6);
    for (int i = 0; i < kLoadDirectionCount; ++i) {
        auto* button = new QToolButton(m_forceButtons);
        button->setObjectName(QStringLiteral("direction%1").arg(i));
        button->setIconSize(QSize(44, 44));
        button->setFixedSize(kDirectionButtonSize, kDirectionButtonSize);
        button->setCheckable(true);
        button->setAutoExclusive(false);
        button->setFocusPolicy(Qt::NoFocus);
        button->setStyleSheet(QStringLiteral("QToolButton{background:#F4F4F4;border:1px solid #D0D0D0;}"
                                             "QToolButton:checked{background:#BDBDBD;border:1px solid #8A8A8A;}"));
        if (i == kLoadDirectionCount - 1)
            button->setToolTip(tr("Definir dirección") + QLatin1Char('\n') +
                               tr("La carga se aplica en la dirección definida por el sistema de referencia global introducido por el usuario."));
        connect(button, &QToolButton::clicked, this, [this, i] { setDirection(static_cast<LoadDirection>(i)); });
        m_directionButtons[static_cast<std::size_t>(i)] = button;
        buttons->addWidget(button);
    }
    buttons->addStretch(1);
    direction->addWidget(m_forceButtons);

    m_temperatureGroup = new QWidget(m_directionGroup);
    auto* temperature = new QHBoxLayout(m_temperatureGroup);
    temperature->setContentsMargins(0, 0, 0, 0);
    temperature->setSpacing(6);
    for (int i = 0; i < 3; ++i) {
        auto* button = new QToolButton(m_temperatureGroup);
        button->setObjectName(QStringLiteral("temperatureDirection%1").arg(i));
        button->setIcon(barTemperatureDirectionIcon(static_cast<TemperatureDirection>(i)));
        button->setIconSize(QSize(44, 44));
        button->setFixedSize(kDirectionButtonSize, kDirectionButtonSize);
        button->setCheckable(true);
        button->setAutoExclusive(false);
        button->setFocusPolicy(Qt::NoFocus);
        button->setStyleSheet(QStringLiteral("QToolButton{background:#F4F4F4;border:1px solid #D0D0D0;}"
                                             "QToolButton:checked{background:#BDBDBD;border:1px solid #8A8A8A;}"));
        connect(button, &QToolButton::clicked, this, [this, i] { setTemperatureDirection(static_cast<TemperatureDirection>(i)); });
        m_temperatureButtons[static_cast<std::size_t>(i)] = button;
        temperature->addWidget(button);
    }
    m_temperatureAngle = new QWidget(m_temperatureGroup);
    auto* angleRow = new QHBoxLayout(m_temperatureAngle);
    angleRow->setContentsMargins(24, 0, 0, 0);
    for (QWidget* w : makeField(BarLoadField::TemperatureAngle, m_temperatureAngle, 62)) angleRow->addWidget(w);
    temperature->addWidget(m_temperatureAngle);
    temperature->addStretch(1);
    direction->addWidget(m_temperatureGroup);

    m_vectorGroup = new QWidget(m_directionGroup);
    auto* vector = new QVBoxLayout(m_vectorGroup);
    vector->setContentsMargins(0, 0, 0, 0);
    vector->setSpacing(4);
    auto* vectorTitle = new QLabel(tr("Vector director"), m_vectorGroup);
    setBold(vectorTitle);
    vector->addWidget(vectorTitle);
    auto* vectorRow = new QHBoxLayout();
    for (BarLoadField f : {BarLoadField::VectorX, BarLoadField::VectorY, BarLoadField::VectorZ}) {
        const auto parts = makeField(f, m_vectorGroup, 70);
        vectorRow->addWidget(parts[0]);
        vectorRow->addWidget(parts[1]);
        vectorRow->addSpacing(8);
    }
    vectorRow->addStretch(1);
    vector->addLayout(vectorRow);
    direction->addWidget(m_vectorGroup);

    m_anglesGroup = new QWidget(m_directionGroup);
    auto* angles = new QVBoxLayout(m_anglesGroup);
    angles->setContentsMargins(0, 0, 0, 0);
    angles->setSpacing(4);
    auto* anglesTitle = new QLabel(tr("Dirección de la carga respecto al eje 'Z' local"), m_anglesGroup);
    setBold(anglesTitle);
    angles->addWidget(anglesTitle);
    for (BarLoadField f : {BarLoadField::AngleInPlane, BarLoadField::AngleToPlane}) {
        auto* row = new QHBoxLayout();
        const auto parts = makeField(f, m_anglesGroup, 62);
        row->addWidget(parts[0], 1);
        row->addWidget(parts[1]);
        row->addWidget(parts[2]);
        angles->addLayout(row);
    }
    direction->addWidget(m_anglesGroup);
    column->addWidget(m_directionGroup);
    column->addStretch(1);

    contentLayout()->setContentsMargins(0, 4, 0, 0);
    contentLayout()->addWidget(body, 1);
    setupFooter(tr("Aceptar"), tr("Cancelar"));

    // Initial texts.
    const auto set = [&](BarLoadField field, double value) {
        m_edits.at(field)->setText(formatted(field, value));
        m_previous[field] = m_edits.at(field)->text();
    };
    m_relative->setChecked(initial.relative);
    set(BarLoadField::Value1, initial.value1);
    set(BarLoadField::Value2, hasTwoValues(initial.type) ? initial.value2 : (isTemperatureLoad(initial.type) ? kBarLoadDefaultTemperature : kBarLoadDefaultValue));
    set(BarLoadField::Start, initial.start);
    set(BarLoadField::End, initial.end);
    set(BarLoadField::VectorX, initial.userVector.x);
    set(BarLoadField::VectorY, initial.userVector.y);
    set(BarLoadField::VectorZ, initial.userVector.z);
    set(BarLoadField::AngleInPlane, initial.angleInPlaneXz);
    set(BarLoadField::AngleToPlane, initial.angleToPlaneXz);
    set(BarLoadField::TemperatureAngle, initial.temperatureAngle);

    connect(m_relative, &QCheckBox::toggled, this, [this](bool) { setRelative(m_relative->isChecked()); });
    connect(m_globalRadio, &QRadioButton::clicked, this, [this] { setAxes(BarLoadAxes::Global); });
    connect(m_localRadio, &QRadioButton::clicked, this, [this] { setAxes(BarLoadAxes::Local); });

    // Tab order measured (cb_tabv_*): Hipótesis, value(s), Posición relativa, positions, the axes group (one stop), the user fields, Aceptar,
    // Cancelar.
    QWidget* previous = m_case;
    for (QWidget* w : {static_cast<QWidget*>(m_edits.at(BarLoadField::Value1)), static_cast<QWidget*>(m_edits.at(BarLoadField::Value2)),
                       static_cast<QWidget*>(m_relative), static_cast<QWidget*>(m_edits.at(BarLoadField::Start)),
                       static_cast<QWidget*>(m_edits.at(BarLoadField::End)), static_cast<QWidget*>(m_globalRadio),
                       static_cast<QWidget*>(m_localRadio), static_cast<QWidget*>(m_edits.at(BarLoadField::VectorX)),
                       static_cast<QWidget*>(m_edits.at(BarLoadField::VectorY)), static_cast<QWidget*>(m_edits.at(BarLoadField::VectorZ)),
                       static_cast<QWidget*>(m_edits.at(BarLoadField::AngleInPlane)), static_cast<QWidget*>(m_edits.at(BarLoadField::AngleToPlane)),
                       static_cast<QWidget*>(m_edits.at(BarLoadField::TemperatureAngle)), static_cast<QWidget*>(acceptButton()),
                       static_cast<QWidget*>(cancelButton())}) {
        setTabOrder(previous, w);
        previous = w;
    }
    setTabOrder(previous, m_case);

    refresh();
    // The height is fixed when the window opens and does not change with the type (measured).
    const int height = !hasDirection(m_type) ? 262 : (m_type == BarLoadType::VariableTemperature ? 416 : 516);
    setFixedSize(402, height);
    m_edits.at(BarLoadField::Value1)->setFocus();
    m_edits.at(BarLoadField::Value1)->selectAll();
}

QString BarLoadDialog::formatted(BarLoadField field, double value) const {
    BarLoadData probe;
    probe.type = m_type;
    probe.relative = m_relative ? m_relative->isChecked() : true;
    return QString::number(value, 'f', barLoadFieldSpec(probe, field).decimals);
}

bool BarLoadDialog::relative() const { return m_relative->isChecked(); }

void BarLoadDialog::setRelative(bool relative) {
    if (m_relative->isChecked() != relative) m_relative->setChecked(relative);
    refresh();
}

void BarLoadDialog::applyTypeDefaults(BarLoadType from, BarLoadType to) {
    const bool fromTemperature = isTemperatureLoad(from), toTemperature = isTemperatureLoad(to);
    const BarLoadData now = current();
    double v1 = now.value1, v2 = now.value2;
    if (fromTemperature != toTemperature) {
        v1 = toTemperature ? kBarLoadDefaultTemperature : kBarLoadDefaultValue;
        v2 = v1;
    } else if (!hasTwoValues(from) && hasTwoValues(to)) {
        v2 = toTemperature ? kBarLoadDefaultTemperature : kBarLoadDefaultValue;
    }
    m_type = to;
    m_edits.at(BarLoadField::Value1)->setText(formatted(BarLoadField::Value1, v1));
    m_edits.at(BarLoadField::Value2)->setText(formatted(BarLoadField::Value2, v2));
    m_previous[BarLoadField::Value1] = m_edits.at(BarLoadField::Value1)->text();
    m_previous[BarLoadField::Value2] = m_edits.at(BarLoadField::Value2)->text();
}

void BarLoadDialog::setType(BarLoadType type) {
    if (type == m_type) return;
    applyTypeDefaults(m_type, type);
    if (!allowsGlobalAxes(m_type)) m_axes = BarLoadAxes::Local;  // the variable temperature only has local axes (measured)
    refresh();
}

void BarLoadDialog::setAxes(BarLoadAxes axes) {
    if (axes == BarLoadAxes::Global && !allowsGlobalAxes(m_type)) return;
    m_axes = axes;
    refresh();
}

void BarLoadDialog::setDirection(LoadDirection direction) {
    m_direction = direction;
    refresh();
}

void BarLoadDialog::setTemperatureDirection(TemperatureDirection direction) {
    m_temperatureDirection = direction;
    refresh();
}

void BarLoadDialog::refresh() {
    if (m_refreshing) return;
    m_refreshing = true;
    const bool pointLike = isPointLikeLoad(m_type), two = hasTwoPositions(m_type), positions = pointLike || two;
    const bool force = hasDirection(m_type) && !isTemperatureLoad(m_type);
    BarLoadData probe = current();
    probe.type = m_type;
    for (int i = 0; i < kBarLoadTypeCount; ++i) m_typeButtons[static_cast<std::size_t>(i)]->setChecked(kBarLoadTypeButtonOrder[i] == m_type);
    // Labels, units and decimals.
    for (BarLoadField f : {BarLoadField::Value1, BarLoadField::Value2, BarLoadField::Start, BarLoadField::End, BarLoadField::AngleInPlane,
                           BarLoadField::AngleToPlane, BarLoadField::TemperatureAngle, BarLoadField::VectorX, BarLoadField::VectorY,
                           BarLoadField::VectorZ}) {
        const BarLoadFieldSpec spec = barLoadFieldSpec(probe, f);
        m_labels.at(f)->setText(fieldLabel(spec.label));
        m_edits.at(f)->setToolTip(tr("Debe estar entre %1 y %2.").arg(QString::number(spec.min, 'f', spec.decimals), QString::number(spec.max, 'f', spec.decimals)));
        m_labels.at(f)->setToolTip(m_edits.at(f)->toolTip());
        // Rewrites the text of the fields whose decimals changed (values keep what was typed otherwise).
        double v = 0.0;
        if (readNumber(m_edits.at(f)->text(), v)) m_edits.at(f)->setText(QString::number(v, 'f', spec.decimals));
    }
    m_units.at(BarLoadField::Value1)->setText(valueUnit(m_type));
    m_units.at(BarLoadField::Value2)->setText(valueUnit(m_type));
    const QString positionUnit = m_relative->isChecked() ? tr("(0..1)") : tr("m");
    m_units.at(BarLoadField::Start)->setText(positionUnit);
    m_units.at(BarLoadField::End)->setText(positionUnit);
    m_units.at(BarLoadField::AngleInPlane)->setText(tr("grados"));
    m_units.at(BarLoadField::AngleToPlane)->setText(tr("grados"));
    m_units.at(BarLoadField::TemperatureAngle)->setText(tr("grados"));
    m_units.at(BarLoadField::VectorX)->hide();
    m_units.at(BarLoadField::VectorY)->hide();
    m_units.at(BarLoadField::VectorZ)->hide();
    // Visibility.
    m_value2Block->setVisible(hasTwoValues(m_type));
    m_relative->setVisible(positions);
    m_positionRows->setVisible(positions);
    m_endBlock->setVisible(two);
    m_axesGroup->setVisible(hasDirection(m_type));
    m_globalRadio->setVisible(allowsGlobalAxes(m_type));
    m_globalRadio->setChecked(m_axes == BarLoadAxes::Global);
    m_localRadio->setChecked(m_axes == BarLoadAxes::Local);
    m_directionGroup->setVisible(hasDirection(m_type));
    const bool variable = m_type == BarLoadType::VariableTemperature;
    m_directionTitle->setText(variable ? tr("Dirección y sentido de la variación del incremento de temperatura")
                                       : tr("Dirección y sentido de aplicación de la carga"));
    m_forceButtons->setVisible(force);
    m_temperatureGroup->setVisible(variable);
    m_temperatureAngle->setVisible(variable && m_temperatureDirection == TemperatureDirection::Angle);
    const bool user = force && m_direction == LoadDirection::UserVector;
    m_vectorGroup->setVisible(user && m_axes == BarLoadAxes::Global);
    m_anglesGroup->setVisible(user && m_axes == BarLoadAxes::Local);
    // Buttons.
    const bool local = m_axes == BarLoadAxes::Local, moment = isMomentLoad(m_type);
    for (int i = 0; i < kLoadDirectionCount; ++i) {
        QToolButton* button = m_directionButtons[static_cast<std::size_t>(i)];
        button->setIcon(barLoadDirectionIcon(static_cast<LoadDirection>(i), local, moment));
        button->setChecked(i == static_cast<int>(m_direction));
    }
    for (int i = 0; i < 3; ++i) m_temperatureButtons[static_cast<std::size_t>(i)]->setChecked(i == static_cast<int>(m_temperatureDirection));
    m_refreshing = false;
}

std::vector<BarLoadField> BarLoadDialog::visibleFields() const {
    std::vector<BarLoadField> out;
    out.push_back(BarLoadField::Value1);
    if (hasTwoValues(m_type)) out.push_back(BarLoadField::Value2);
    if (isPointLikeLoad(m_type) || hasTwoPositions(m_type)) out.push_back(BarLoadField::Start);
    if (hasTwoPositions(m_type)) out.push_back(BarLoadField::End);
    const bool force = hasDirection(m_type) && !isTemperatureLoad(m_type);
    if (force && m_direction == LoadDirection::UserVector) {
        if (m_axes == BarLoadAxes::Global) {
            out.push_back(BarLoadField::VectorX);
            out.push_back(BarLoadField::VectorY);
            out.push_back(BarLoadField::VectorZ);
        } else {
            out.push_back(BarLoadField::AngleInPlane);
            out.push_back(BarLoadField::AngleToPlane);
        }
    }
    if (m_type == BarLoadType::VariableTemperature && m_temperatureDirection == TemperatureDirection::Angle) out.push_back(BarLoadField::TemperatureAngle);
    return out;
}

BarLoadData BarLoadDialog::current() const {
    BarLoadData d;
    d.loadCaseId = m_case->currentIndex() < 0 ? 0 : m_case->currentData().toInt();
    d.type = m_type;
    d.axes = m_axes;
    d.direction = m_direction;
    d.temperatureDirection = m_temperatureDirection;
    d.relative = m_relative->isChecked();
    const auto read = [&](BarLoadField field) {
        double v = 0.0;
        if (readNumber(m_edits.at(field)->text(), v)) return v;
        const auto it = m_previous.find(field);
        if (it != m_previous.end() && readNumber(it->second, v)) return v;
        return 0.0;
    };
    d.value1 = read(BarLoadField::Value1);
    d.value2 = read(BarLoadField::Value2);
    d.start = read(BarLoadField::Start);
    d.end = read(BarLoadField::End);
    d.userVector = {read(BarLoadField::VectorX), read(BarLoadField::VectorY), read(BarLoadField::VectorZ)};
    d.angleInPlaneXz = read(BarLoadField::AngleInPlane);
    d.angleToPlaneXz = read(BarLoadField::AngleToPlane);
    d.temperatureAngle = read(BarLoadField::TemperatureAngle);
    return d;
}

BarLoadData BarLoadDialog::data() const { return current(); }

void BarLoadDialog::accept() {
    BarLoadData d = current();
    // 1. Every visible field in the order of the tab sequence: not a number / out of range -> «Aviso», the field recovers its previous value.
    for (BarLoadField field : visibleFields()) {
        QLineEdit* edit = m_edits.at(field);
        const BarLoadFieldParse parsed = parseBarLoadField(d, field, edit->text().toStdString());
        if (!parsed.ok) {
            messages::show(this, messages::Kind::Warning, QString::fromStdString(parsed.message));
            edit->setText(m_previous[field]);
            edit->setFocus();
            edit->selectAll();
            return;
        }
        switch (field) {
            case BarLoadField::Value1: d.value1 = parsed.value; break;
            case BarLoadField::Value2: d.value2 = parsed.value; break;
            case BarLoadField::Start: d.start = parsed.value; break;
            case BarLoadField::End: d.end = parsed.value; break;
            case BarLoadField::VectorX: d.userVector.x = parsed.value; break;
            case BarLoadField::VectorY: d.userVector.y = parsed.value; break;
            case BarLoadField::VectorZ: d.userVector.z = parsed.value; break;
            case BarLoadField::AngleInPlane: d.angleInPlaneXz = parsed.value; break;
            case BarLoadField::AngleToPlane: d.angleToPlaneXz = parsed.value; break;
            case BarLoadField::TemperatureAngle: d.temperatureAngle = parsed.value; break;
        }
    }
    // 2. Business rules (Error, red) and the checks that need the whole data (Aviso, yellow).
    const BarLoadIssue issue = checkBarLoadData(d, m_lengthM);
    if (!issue.ok()) {
        messages::show(this, issue.kind == BarLoadIssueKind::Error ? messages::Kind::Error : messages::Kind::Warning,
                       QString::fromStdString(issue.message));
        if (issue.field && m_edits.count(*issue.field)) {
            QLineEdit* edit = m_edits.at(*issue.field);
            if (edit->isVisible()) {
                edit->setText(m_previous[*issue.field]);
                edit->setFocus();
                edit->selectAll();
            }
        }
        return;
    }
    // Written back rounded, like the original.
    const auto write = [&](BarLoadField field, double value) { m_edits.at(field)->setText(formatted(field, value)); };
    write(BarLoadField::Value1, d.value1);
    write(BarLoadField::Value2, d.value2);
    write(BarLoadField::Start, d.start);
    write(BarLoadField::End, d.end);
    CypeDialog::accept();
}

// ---- BarSelfWeightDialog ----------------------------------------------------------------------------------------------------

BarSelfWeightDialog::BarSelfWeightDialog(bool activate, QWidget* parent) : CypeDialog(tr("Generación de cargas"), parent) {
    auto* body = new QWidget(this);
    auto* column = new QVBoxLayout(body);
    column->setContentsMargins(8, 4, 8, 0);
    column->setSpacing(6);
    m_activate = new QRadioButton(tr("Activar"), body);
    m_activate->setObjectName(QStringLiteral("activate"));
    m_deactivate = new QRadioButton(tr("Desactivar"), body);
    m_deactivate->setObjectName(QStringLiteral("deactivate"));
    column->addWidget(m_activate);
    column->addWidget(m_deactivate);
    column->addStretch(1);
    contentLayout()->setContentsMargins(0, 4, 0, 0);
    contentLayout()->addWidget(body, 1);
    setupFooter(tr("Aceptar"), tr("Cancelar"));
    m_activate->setChecked(activate);
    m_deactivate->setChecked(!activate);
    setCompactTitle();  // the title does not fit between the two 30 px margins of a 186 px window
    setFixedSize(186, 134);  // measured (cb_pp_a)
}

bool BarSelfWeightDialog::activate() const { return m_activate->isChecked(); }

}  // namespace cc::app
