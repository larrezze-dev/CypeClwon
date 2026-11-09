#include "ShellLoadDialogs.h"

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
#include <algorithm>
#include <cmath>

#include "NodalLoadDialogs.h"
#include "UserMessages.h"

namespace cc::app {

using namespace cc::core;

namespace {

constexpr int kTypeButtonWidth = 36, kTypeButtonHeight = 30;
constexpr int kSenseButtonSize = 46;

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
    if (text == "Valor inicial") return QObject::tr("Valor inicial");
    if (text == "Valor final") return QObject::tr("Valor final");
    if (text == "Valor") return QObject::tr("Valor");
    return QString::fromStdString(text);
}

QString typeTooltip(ShellLoadType type) {
    switch (type) {
        case ShellLoadType::Uniform: return QObject::tr("Carga uniforme");  // NOT MEASURED (the tooltip of the first button was not captured)
        case ShellLoadType::VariableX: return QObject::tr("Carga variable en el eje x local");
        case ShellLoadType::VariableY: return QObject::tr("Carga variable en el eje y local");
        case ShellLoadType::UniformTemperature: return QObject::tr("Incremento de temperatura uniforme");
        case ShellLoadType::Shrinkage: return QObject::tr("Retracción");
    }
    return {};
}

QString valueUnit(ShellLoadType type) {
    if (type == ShellLoadType::UniformTemperature) return QObject::tr("°C");
    if (type == ShellLoadType::Shrinkage) return {};
    return QObject::tr("kN/m²");
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

QIcon shellLoadTypeIcon(ShellLoadType type) {
    constexpr int w = 30, h = 22;
    IconCanvas c(w, h);
    QPainter& p = c.painter;
    const QColor red(215, 20, 20), blue(0, 40, 220);
    const double base = 15.0;
    p.setPen(QPen(Qt::black, 1.2));
    p.drawLine(QPointF(3, base), QPointF(w - 3, base));
    p.drawLine(QPointF(3, base - 2.0), QPointF(3, base + 2.0));
    p.drawLine(QPointF(w - 3, base - 2.0), QPointF(w - 3, base + 2.0));
    const auto hatch = [&](double h0, double h1) {
        const double x0 = 4, x1 = w - 4;
        p.setPen(QPen(red, 1.1));
        p.drawLine(QPointF(x0, base - h0), QPointF(x1, base - h1));
        const int n = 7;
        for (int i = 0; i <= n; ++i) {
            const double t = static_cast<double>(i) / n;
            const double x = x0 + (x1 - x0) * t;
            p.drawLine(QPointF(x, base), QPointF(x, base - (h0 + (h1 - h0) * t)));
        }
    };
    QFont font = p.font();
    font.setBold(true);
    switch (type) {
        case ShellLoadType::Uniform: hatch(9, 9); break;
        case ShellLoadType::VariableX:
        case ShellLoadType::VariableY: {
            hatch(10, 2);
            font.setPixelSize(9);
            p.setFont(font);
            p.setPen(type == ShellLoadType::VariableX ? red : blue);
            p.drawText(QRectF(0, base + 0.5, w, 8), Qt::AlignCenter, type == ShellLoadType::VariableX ? QStringLiteral("x") : QStringLiteral("Y"));
            break;
        }
        case ShellLoadType::UniformTemperature:
        case ShellLoadType::Shrinkage: {
            font.setPixelSize(10);
            p.setFont(font);
            p.setPen(red);
            p.drawText(QRectF(0, 1, w, 12), Qt::AlignCenter, type == ShellLoadType::UniformTemperature ? QStringLiteral("Δt") : QStringLiteral("Δε"));
            break;
        }
    }
    return c.icon();
}

QIcon shellLoadSenseIcon(ShellLoadSense sense, bool local) {
    constexpr int size = 44;
    IconCanvas c(size, size);
    QPainter& p = c.painter;
    const QPointF o(22, 24);
    const QColor blue(0, 40, 220), red(235, 20, 20);
    QFont font = p.font();
    font.setPixelSize(9);
    font.setBold(true);
    p.setFont(font);
    QPointF zDir;
    if (local) {
        // Measured (cl_n2): x oblique (black, ticks at both ends, blue arrow at the top right), z towards the top left, y to the left.
        const QPointF xDir(0.76, -0.65);
        zDir = QPointF(-0.7, -0.7);
        p.setPen(QPen(Qt::black, 1.3));
        p.drawLine(o - xDir * 19.0, o + xDir * 19.0);
        p.drawLine(o - xDir * 19.0 + QPointF(-2, -2), o - xDir * 19.0 + QPointF(2, 2));
        p.setPen(QPen(blue, 1.4));
        p.drawLine(o - zDir * 17.0, o + zDir * 17.0);
        p.drawLine(QPointF(o.x() - 19, o.y()), QPointF(o.x() + 19, o.y()));
        arrowHead(p, o + xDir * 19.0, xDir, 5, 2.2, blue);
        arrowHead(p, o + zDir * 17.0, zDir, 5, 2.2, blue);
        arrowHead(p, QPointF(o.x() - 19, o.y()), QPointF(-1, 0), 5, 2.2, blue);
        p.setPen(blue);
        p.drawText(QPointF(o.x() + 7, o.y() - 12), QStringLiteral("x"));
        p.drawText(QPointF(o.x() - 11, o.y() - 12), QStringLiteral("z"));
        p.drawText(QPointF(o.x() - 20, o.y() + 11), QStringLiteral("y"));
    } else {
        zDir = QPointF(0, -1);
        const QPointF yDir(-0.72, -0.69);
        p.setPen(QPen(blue, 1.4));
        p.drawLine(QPointF(o.x() - 20, o.y()), QPointF(o.x() + 20, o.y()));
        p.drawLine(o - yDir * 18.0, o + yDir * 18.0);
        p.drawLine(QPointF(o.x(), o.y() + 19), QPointF(o.x(), o.y() - 19));
        arrowHead(p, QPointF(o.x() + 20, o.y()), QPointF(1, 0), 5, 2.2, blue);
        arrowHead(p, o + yDir * 18.0, yDir, 5, 2.2, blue);
        arrowHead(p, QPointF(o.x(), o.y() - 19), QPointF(0, -1), 5, 2.2, blue);
        p.setPen(blue);
        p.drawText(QPointF(o.x() + 15, o.y() + 11), QStringLiteral("X"));
        p.drawText(QPointF(o.x() - 20, o.y() - 14), QStringLiteral("Y"));
        p.drawText(QPointF(o.x() + 4, o.y() - 12), QStringLiteral("Z"));
    }
    // The red arrow ends at the origin and points along +Z (button 1, it comes from the -Z side) or -Z (button 2).
    const QPointF dir = sense == ShellLoadSense::PlusZ ? zDir : -zDir;
    p.setPen(QPen(red, 3.4, Qt::SolidLine, Qt::FlatCap));
    p.drawLine(o - dir * 17.0, o - dir * 3.0);
    arrowHead(p, o, dir, 7, 4, red);
    return c.icon();
}

// ---- ShellLoadDialog ---------------------------------------------------------------------------------------------------------

ShellLoadDialog::ShellLoadDialog(const QString& title, const LoadCaseSet& cases, const ShellLoadData& initial, QWidget* parent)
    : CypeDialog(title, parent), m_cases(cases), m_type(initial.type), m_axes(initial.axes), m_sense(initial.sense) {
    if (!hasShellAxesAndSense(m_type)) {
        m_axes = ShellLoadAxes::Local;
        m_sense = ShellLoadSense::PlusZ;
    }
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
    for (int i = 0; i < kShellLoadTypeCount; ++i) {
        const ShellLoadType t = kShellLoadTypeButtonOrder[i];
        auto* button = new QToolButton(body);
        button->setObjectName(QStringLiteral("type%1").arg(static_cast<int>(t)));
        button->setIcon(shellLoadTypeIcon(t));
        button->setIconSize(QSize(30, 22));
        button->setFixedSize(kTypeButtonWidth, kTypeButtonHeight);
        button->setCheckable(true);
        button->setAutoExclusive(false);
        button->setFocusPolicy(Qt::NoFocus);  // measured: the type buttons are not tab stops
        button->setToolTip(typeTooltip(t));
        button->setStyleSheet(QStringLiteral("QToolButton{background:#F4F4F4;border:1px solid #D0D0D0;}"
                                             "QToolButton:checked{background:#BDBDBD;border:1px solid #8A8A8A;}"));
        connect(button, &QToolButton::clicked, this, [this, t] { setType(t); });
        m_typeButtons[static_cast<std::size_t>(i)] = button;
        types->addWidget(button);
    }
    types->addStretch(1);
    column->addLayout(types);

    // Values. The block keeps its height whatever the type (the groups below do not move, measured).
    const auto makeField = [&](ShellLoadField field, QWidget* parent) {
        auto* label = new QLabel(parent);
        auto* edit = new QLineEdit(parent);
        edit->setAlignment(Qt::AlignRight);
        edit->setFixedWidth(62);
        auto* unit = new QLabel(parent);
        m_labels[field] = label;
        m_edits[field] = edit;
        m_units[field] = unit;
        return std::array<QWidget*, 3>{label, edit, unit};
    };
    auto* values = new QWidget(body);
    values->setFixedHeight(76);
    auto* valuesColumn = new QVBoxLayout(values);
    valuesColumn->setContentsMargins(0, 8, 0, 0);
    valuesColumn->setSpacing(4);
    auto* firstRow = new QWidget(values);
    auto* v1 = new QHBoxLayout(firstRow);
    v1->setContentsMargins(0, 0, 0, 0);
    v1->setSpacing(6);
    for (QWidget* w : makeField(ShellLoadField::Value1, firstRow)) v1->addWidget(w);
    v1->addStretch(1);
    valuesColumn->addWidget(firstRow);
    m_value2Block = new QWidget(values);
    auto* v2 = new QHBoxLayout(m_value2Block);
    v2->setContentsMargins(0, 0, 0, 0);
    v2->setSpacing(6);
    for (QWidget* w : makeField(ShellLoadField::Value2, m_value2Block)) v2->addWidget(w);
    v2->addStretch(1);
    valuesColumn->addWidget(m_value2Block);
    valuesColumn->addStretch(1);
    column->addWidget(values);

    // Axes (uniform and variable loads only; the temperature and the shrinkage leave the room empty).
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
    m_localRadio = new QRadioButton(tr("Ejes locales de la lámina"), m_axesGroup);
    m_localRadio->setObjectName(QStringLiteral("localAxes"));
    axes->addWidget(m_globalRadio);
    axes->addWidget(m_localRadio);
    column->addWidget(m_axesGroup);

    // Direction and sense: two buttons.
    m_senseGroup = new QWidget(body);
    auto* sense = new QVBoxLayout(m_senseGroup);
    sense->setContentsMargins(0, 0, 0, 0);
    sense->setSpacing(4);
    sense->addWidget(separator(m_senseGroup));
    auto* senseHeading = new QLabel(tr("Dirección y sentido de aplicación de la carga"), m_senseGroup);
    setBold(senseHeading);
    sense->addWidget(senseHeading);
    auto* buttons = new QHBoxLayout();
    buttons->setSpacing(7);
    for (int i = 0; i < 2; ++i) {
        auto* button = new QToolButton(m_senseGroup);
        button->setObjectName(QStringLiteral("sense%1").arg(i));
        button->setIconSize(QSize(44, 44));
        button->setFixedSize(kSenseButtonSize, kSenseButtonSize);
        button->setCheckable(true);
        button->setAutoExclusive(false);
        button->setFocusPolicy(Qt::NoFocus);  // measured: not in the tab order
        button->setToolTip(i == 0 ? tr("Dirección según el eje Z, en sentido positivo.")
                                  : tr("Dirección según el eje Z, en sentido negativo."));  // the second text is NOT MEASURED (symmetry)
        button->setStyleSheet(QStringLiteral("QToolButton{background:#F4F4F4;border:1px solid #D0D0D0;}"
                                             "QToolButton:checked{background:#BDBDBD;border:1px solid #8A8A8A;}"));
        connect(button, &QToolButton::clicked, this, [this, i] { setSense(static_cast<ShellLoadSense>(i)); });
        m_senseButtons[static_cast<std::size_t>(i)] = button;
        buttons->addWidget(button);
    }
    buttons->addStretch(1);
    sense->addLayout(buttons);
    column->addWidget(m_senseGroup);
    column->addStretch(1);

    contentLayout()->setContentsMargins(0, 4, 0, 0);
    contentLayout()->addWidget(body, 1);
    setupFooter(tr("Aceptar"), tr("Cancelar"));

    const auto set = [&](ShellLoadField field, double value) {
        m_edits.at(field)->setText(formatted(field, value));
        m_previous[field] = m_edits.at(field)->text();
    };
    set(ShellLoadField::Value1, initial.value1);
    set(ShellLoadField::Value2, isVariableShellLoad(initial.type) ? initial.value2 : kShellLoadDefaultSurface);

    connect(m_globalRadio, &QRadioButton::clicked, this, [this] { setAxes(ShellLoadAxes::Global); });
    connect(m_localRadio, &QRadioButton::clicked, this, [this] { setAxes(ShellLoadAxes::Local); });

    // Tab order measured (cl3_tab*): Valor(es), the axes group (one stop), Aceptar, Cancelar, then Hipótesis (the stop without a visible focus),
    // Valor. The type and sense buttons are not stops.
    QWidget* previous = m_case;
    for (QWidget* w : {static_cast<QWidget*>(m_edits.at(ShellLoadField::Value1)), static_cast<QWidget*>(m_edits.at(ShellLoadField::Value2)),
                       static_cast<QWidget*>(m_globalRadio), static_cast<QWidget*>(m_localRadio), static_cast<QWidget*>(acceptButton()),
                       static_cast<QWidget*>(cancelButton())}) {
        setTabOrder(previous, w);
        previous = w;
    }
    setTabOrder(previous, m_case);

    refresh();
    setFixedSize(310, 420);  // measured: the height does not depend on the type
    m_edits.at(ShellLoadField::Value1)->setFocus();
    m_edits.at(ShellLoadField::Value1)->selectAll();
}

QString ShellLoadDialog::formatted(ShellLoadField field, double value) const {
    ShellLoadData probe;
    probe.type = m_type;
    return QString::number(value, 'f', shellLoadFieldSpec(probe, field).decimals);
}

void ShellLoadDialog::applyTypeDefaults(ShellLoadType from, ShellLoadType to) {
    const ShellLoadData now = current();
    double v1 = now.value1, v2 = now.value2;
    if (isSurfaceLoad(from) != isSurfaceLoad(to) || !isSurfaceLoad(to)) {
        const ShellLoadData d = shellLoadDefaults(0, to);  // the panel shows the defaults of the new kind
        v1 = d.value1;
        v2 = isVariableShellLoad(to) ? d.value2 : kShellLoadDefaultSurface;
    } else if (!isVariableShellLoad(from) && isVariableShellLoad(to)) {
        v2 = kShellLoadDefaultSurface;
    }
    m_type = to;
    m_edits.at(ShellLoadField::Value1)->setText(formatted(ShellLoadField::Value1, v1));
    m_edits.at(ShellLoadField::Value2)->setText(formatted(ShellLoadField::Value2, v2));
    m_previous[ShellLoadField::Value1] = m_edits.at(ShellLoadField::Value1)->text();
    m_previous[ShellLoadField::Value2] = m_edits.at(ShellLoadField::Value2)->text();
}

void ShellLoadDialog::setType(ShellLoadType type) {
    if (type == m_type) return;
    applyTypeDefaults(m_type, type);
    refresh();
}

void ShellLoadDialog::setAxes(ShellLoadAxes axes) {
    if (!hasShellAxesAndSense(m_type)) return;
    m_axes = axes;
    refresh();
}

void ShellLoadDialog::setSense(ShellLoadSense sense) {
    if (!hasShellAxesAndSense(m_type)) return;
    m_sense = sense;
    refresh();
}

void ShellLoadDialog::refresh() {
    if (m_refreshing) return;
    m_refreshing = true;
    ShellLoadData probe = current();
    probe.type = m_type;
    for (int i = 0; i < kShellLoadTypeCount; ++i) m_typeButtons[static_cast<std::size_t>(i)]->setChecked(kShellLoadTypeButtonOrder[i] == m_type);
    for (ShellLoadField f : {ShellLoadField::Value1, ShellLoadField::Value2}) {
        const ShellLoadFieldSpec spec = shellLoadFieldSpec(probe, f);
        m_labels.at(f)->setText(fieldLabel(spec.label));
        // Measured tooltip of the shrinkage: «Debe estar entre -1000.0000  y 1000.0000 .» (the unit-less control leaves doubled spaces); the others
        // carry the unit like the warning message (NOT MEASURED for the surface loads and the temperature).
        const QString lo = QString::number(spec.min, 'f', spec.decimals), hi = QString::number(spec.max, 'f', spec.decimals);
        const QString unit = QString::fromStdString(spec.unit);
        const QString tip = spec.blankUnitSpacing ? tr("Debe estar entre %1  y %2 .").arg(lo, hi)
                            : unit.isEmpty()      ? tr("Debe estar entre %1 y %2.").arg(lo, hi)
                                                  : tr("Debe estar entre %1 %3 y %2 %3.").arg(lo, hi, unit);
        m_edits.at(f)->setToolTip(tip);
        m_labels.at(f)->setToolTip(tip);
        double v = 0.0;  // rewrites the text when the decimals changed
        if (readNumber(m_edits.at(f)->text(), v)) m_edits.at(f)->setText(QString::number(v, 'f', spec.decimals));
        m_units.at(f)->setText(valueUnit(m_type));
    }
    m_value2Block->setVisible(isVariableShellLoad(m_type));
    const bool directional = hasShellAxesAndSense(m_type);
    m_axesGroup->setVisible(directional);
    m_senseGroup->setVisible(directional);
    m_globalRadio->setChecked(m_axes == ShellLoadAxes::Global);
    m_localRadio->setChecked(m_axes == ShellLoadAxes::Local);
    QFont global = m_globalRadio->font(), local = m_localRadio->font();
    global.setBold(m_axes == ShellLoadAxes::Global);  // measured: the active radio is written in bold
    local.setBold(m_axes == ShellLoadAxes::Local);
    m_globalRadio->setFont(global);
    m_localRadio->setFont(local);
    const bool isLocal = m_axes == ShellLoadAxes::Local;
    for (int i = 0; i < 2; ++i) {
        QToolButton* button = m_senseButtons[static_cast<std::size_t>(i)];
        button->setIcon(shellLoadSenseIcon(static_cast<ShellLoadSense>(i), isLocal));
        button->setChecked(i == static_cast<int>(m_sense));
    }
    m_refreshing = false;
}

std::vector<ShellLoadField> ShellLoadDialog::visibleFields() const {
    std::vector<ShellLoadField> out{ShellLoadField::Value1};
    if (isVariableShellLoad(m_type)) out.push_back(ShellLoadField::Value2);
    return out;
}

ShellLoadData ShellLoadDialog::current() const {
    ShellLoadData d;
    d.loadCaseId = m_case->currentIndex() < 0 ? 0 : m_case->currentData().toInt();
    d.type = m_type;
    d.axes = m_axes;
    d.sense = m_sense;
    const auto read = [&](ShellLoadField field) {
        double v = 0.0;
        if (readNumber(m_edits.at(field)->text(), v)) return v;
        const auto it = m_previous.find(field);
        if (it != m_previous.end() && readNumber(it->second, v)) return v;
        return 0.0;
    };
    d.value1 = read(ShellLoadField::Value1);
    d.value2 = read(ShellLoadField::Value2);
    return d;
}

ShellLoadData ShellLoadDialog::data() const { return current(); }

void ShellLoadDialog::accept() {
    ShellLoadData d = current();
    // 1. Every visible field in the order of the tab sequence: not a number / out of range -> «Aviso», the field recovers its previous value.
    for (ShellLoadField field : visibleFields()) {
        QLineEdit* edit = m_edits.at(field);
        const ShellLoadFieldParse parsed = parseShellLoadField(d, field, edit->text().toStdString());
        if (!parsed.ok) {
            messages::show(this, messages::Kind::Warning, QString::fromStdString(parsed.message));
            edit->setText(m_previous[field]);
            edit->setFocus();
            edit->selectAll();
            return;
        }
        (field == ShellLoadField::Value1 ? d.value1 : d.value2) = parsed.value;
    }
    // 2. The null rule (Error, red).
    const ShellLoadIssue issue = checkShellLoadData(d);
    if (!issue.ok()) {
        messages::show(this, issue.kind == ShellLoadIssueKind::Error ? messages::Kind::Error : messages::Kind::Warning, QString::fromStdString(issue.message));
        // The null rule names no field: the focus goes back to the first one (the text stays, the user is asked to change it).
        const ShellLoadField field = issue.field.value_or(ShellLoadField::Value1);
        QLineEdit* edit = m_edits.at(field);
        const std::vector<ShellLoadField> visible = visibleFields();  // not isVisible(): the same branch with the window hidden
        if (issue.field && std::find(visible.begin(), visible.end(), field) != visible.end()) edit->setText(m_previous[field]);
        edit->setFocus();
        edit->selectAll();
        return;
    }
    // Written back rounded, like the original.
    m_edits.at(ShellLoadField::Value1)->setText(formatted(ShellLoadField::Value1, d.value1));
    if (isVariableShellLoad(m_type)) m_edits.at(ShellLoadField::Value2)->setText(formatted(ShellLoadField::Value2, d.value2));
    CypeDialog::accept();
}

// ---- ShellSelfWeightDialog ---------------------------------------------------------------------------------------------------

ShellSelfWeightDialog::ShellSelfWeightDialog(bool activate, QWidget* parent) : CypeDialog(tr("Generación de cargas"), parent) {
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
    setCompactTitle();
    setFixedSize(186, 134);  // measured (same window as the bars)
}

bool ShellSelfWeightDialog::activate() const { return m_activate->isChecked(); }

}  // namespace cc::app
