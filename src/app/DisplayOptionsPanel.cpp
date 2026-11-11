#include "DisplayOptionsPanel.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QGridLayout>
#include <QLabel>
#include <QPainter>
#include <QPixmap>

#include "Viewport3D.h"

namespace cc::app {

namespace {
using cc::core::TooltipPlacement;

// The icons next to the checkboxes are simple original markers painted here (no suitable bundled artwork yet; the
// originals are not copied). 16x16 on transparent.
enum class Marker { Bubble, Text, Axes, Shell, Loads };

QPixmap markerPixmap(Marker marker) {
    QPixmap pixmap(16, 16);
    pixmap.fill(Qt::transparent);
    QPainter p(&pixmap);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(QColor(50, 50, 50), 1.2));
    switch (marker) {
        case Marker::Bubble:
            p.drawRoundedRect(QRectF(1.5, 2.5, 13, 8), 2, 2);
            p.drawLine(QPointF(5, 10.5), QPointF(4, 14));
            p.drawLine(QPointF(4, 14), QPointF(8, 10.5));
            break;
        case Marker::Text: {
            QFont f = p.font();
            f.setBold(true);
            f.setPixelSize(15);
            p.setFont(f);
            p.drawText(QRectF(0, 0, 16, 16), Qt::AlignCenter, QStringLiteral("T"));
            break;
        }
        case Marker::Axes:
            p.drawLine(QPointF(3, 2), QPointF(3, 13));
            p.drawLine(QPointF(3, 13), QPointF(14, 13));
            p.drawLine(QPointF(3, 13), QPointF(10, 6));
            break;
        case Marker::Shell:
            p.setPen(QPen(QColor(50, 90, 160), 1.2, Qt::DashLine));
            p.drawRect(QRectF(2, 2, 12, 12));
            break;
        case Marker::Loads:
            p.setPen(QPen(QColor(200, 60, 60), 1.4));
            for (int x = 3; x <= 13; x += 3) p.drawLine(QPointF(x, 3), QPointF(x, 13));
            break;
    }
    return pixmap;
}
}  // namespace

DisplayOptionsPanel::DisplayOptionsPanel(QWidget* parent) : QWidget(parent) {
    auto* grid = new QGridLayout(this);
    grid->setContentsMargins(4, 4, 4, 4);
    grid->setHorizontalSpacing(4);
    grid->setVerticalSpacing(4);

    auto addCheck = [&](int row, Marker marker, const QString& text, bool checked, const char* name) {
        auto* icon = new QLabel(this);
        icon->setPixmap(markerPixmap(marker));
        auto* box = new QCheckBox(text, this);
        box->setObjectName(QString::fromLatin1(name));
        box->setChecked(checked);
        grid->addWidget(icon, row, 0);
        grid->addWidget(box, row, 1, 1, 3);
        connect(box, &QCheckBox::toggled, this, &DisplayOptionsPanel::readControls);
        return box;
    };
    m_info = addCheck(0, Marker::Bubble, tr("Mostrar la información de los elementos"), m_options.showElementInfo, "showInfo");
    m_texts = addCheck(1, Marker::Text, tr("Mostrar textos sobre los elementos"), m_options.showTexts, "showTexts");
    m_axes = addCheck(2, Marker::Axes, tr("Mostrar los ejes locales de los elementos"), m_options.showLocalAxes, "showAxes");
    m_shells = addCheck(3, Marker::Shell, tr("Dibujar las láminas con relleno"), m_options.fillShells, "fillShells");
    m_loads = addCheck(4, Marker::Loads, tr("Dibujar las cargas con relleno"), m_options.fillLoads, "fillLoads");

    auto addSpin = [&](int row, const QString& label, const QString& unit, double value, const char* name, double minimum, double maximum) {
        auto* spin = new QDoubleSpinBox(this);
        spin->setObjectName(QString::fromLatin1(name));
        spin->setDecimals(1);
        spin->setSingleStep(0.1);
        spin->setMinimum(minimum);  // limits verified in the original; it rejects out-of-range input and keeps the old value
        spin->setMaximum(maximum);
        spin->setKeyboardTracking(false);
        spin->setToolTip(tr("Debe estar entre %1 y %2.").arg(QString::number(minimum, 'f', 1), QString::number(maximum, 'f', 1)));
        spin->setValue(value);
        spin->setButtonSymbols(QAbstractSpinBox::NoButtons);
        spin->setAlignment(Qt::AlignRight);
        spin->setFixedWidth(88);
        grid->addWidget(new QLabel(label, this), row, 1);
        grid->addWidget(spin, row, 2);
        grid->addWidget(new QLabel(unit, this), row, 3);
        connect(spin, &QDoubleSpinBox::valueChanged, this, &DisplayOptionsPanel::readControls);
        return spin;
    };
    m_nodeSize = addSpin(5, tr("Dimensiones de los nudos"), tr("cm"), m_options.nodeSizeCm, "nodeSize", core::kNodeSizeMinCm, core::kNodeSizeMaxCm);
    m_textSize = addSpin(6, tr("Tamaño de los textos"), tr("%"), m_options.textSizePercent, "textSize", core::kTextSizeMinPercent, core::kTextSizeMaxPercent);

    m_tooltip = new QComboBox(this);
    m_tooltip->setObjectName(QStringLiteral("tooltipPosition"));
    m_tooltip->addItem(Viewport3D::tooltipPositionLabel(TooltipPosition::TopRightCorner),
                       static_cast<int>(TooltipPlacement::TopRightCorner));
    m_tooltip->addItem(Viewport3D::tooltipPositionLabel(TooltipPosition::OnElement), static_cast<int>(TooltipPlacement::OnElement));
    grid->addWidget(new QLabel(tr("Posición de los tooltips"), this), 7, 1);
    grid->addWidget(m_tooltip, 7, 2, 1, 2);
    connect(m_tooltip, &QComboBox::currentIndexChanged, this, &DisplayOptionsPanel::readControls);

    grid->setColumnStretch(1, 1);
    grid->setRowStretch(8, 1);
}

void DisplayOptionsPanel::readControls() {
    if (m_updating) return;
    cc::core::DisplayOptions next;
    next.showElementInfo = m_info->isChecked();
    next.showTexts = m_texts->isChecked();
    next.showLocalAxes = m_axes->isChecked();
    next.fillShells = m_shells->isChecked();
    next.fillLoads = m_loads->isChecked();
    next.nodeSizeCm = m_nodeSize->value();
    next.textSizePercent = m_textSize->value();
    next.tooltipPlacement = static_cast<TooltipPlacement>(m_tooltip->currentData().toInt());
    if (next == m_options) return;
    m_options = next;
    emit optionsChanged(m_options);
}

void DisplayOptionsPanel::setOptions(const cc::core::DisplayOptions& options) {
    if (!cc::core::isValid(options)) return;
    m_updating = true;
    m_info->setChecked(options.showElementInfo);
    m_texts->setChecked(options.showTexts);
    m_axes->setChecked(options.showLocalAxes);
    m_shells->setChecked(options.fillShells);
    m_loads->setChecked(options.fillLoads);
    m_nodeSize->setValue(options.nodeSizeCm);
    m_textSize->setValue(options.textSizePercent);
    m_tooltip->setCurrentIndex(m_tooltip->findData(static_cast<int>(options.tooltipPlacement)));
    m_updating = false;
    m_options = options;
}

void DisplayOptionsPanel::applyTo(Viewport3D& viewport, const cc::core::DisplayOptions& options) {
    viewport.setShowElementInfo(options.showElementInfo);
    viewport.setFillShells(options.fillShells);
    viewport.setFillLoads(options.fillLoads);
    viewport.setNodeSizeCm(options.nodeSizeCm);
    viewport.setTextSizePercent(options.textSizePercent);
    viewport.setTooltipPosition(options.tooltipPlacement == TooltipPlacement::OnElement ? TooltipPosition::OnElement
                                                                                       : TooltipPosition::TopRightCorner);
}

}  // namespace cc::app
