#include "ViewBar.h"

#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHideEvent>
#include <QToolButton>
#include <QVBoxLayout>

#include "ViewIcons.h"
#include "Viewport3D.h"
#include "core/camera.h"

namespace cc::app {

// ---------------------------------------------------------------------------------------------------------------------
ProjectionPalette::ProjectionPalette(QWidget* parent) : QWidget(parent, Qt::Tool | Qt::FramelessWindowHint) {
    setObjectName(QStringLiteral("projectionPalette"));
    setStyleSheet(QStringLiteral("#projectionPalette { background: #FFFFFF; border: 1px solid #7A7A7A; }"
                                 "QToolButton { border: 1px solid transparent; background: transparent; }"
                                 "QToolButton:hover:enabled { border: 1px solid #5A9BD5; background: #E4F0FB; }"));
    auto* grid = new QGridLayout(this);
    grid->setContentsMargins(2, 2, 2, 2);
    grid->setSpacing(0);
    const auto& presets = core::projectionPresets();
    for (int i = 0; i < core::kProjectionPresetCount; ++i) {
        const core::ProjectionPreset& preset = presets[static_cast<std::size_t>(i)];
        auto* b = new QToolButton(this);
        b->setAutoRaise(true);
        b->setFixedSize(36, 36);
        b->setIconSize(QSize(30, 30));
        b->setIcon(triadIcon(preset.yawDeg, preset.pitchDeg, preset.rollDeg, !preset.available));
        b->setEnabled(preset.available);
        b->setToolTip(preset.available ? tr("Proyección %1").arg(i + 1) : tr("Proyección %1 (no disponible)").arg(i + 1));
        connect(b, &QToolButton::clicked, this, [this, i] { emit presetChosen(i); });
        if (i == core::kProjectionPresetCount - 1) grid->addWidget(b, i / 3, 1);  // last cell centred
        else grid->addWidget(b, i / 3, i % 3);
        m_cells.push_back(b);
    }
    adjustSize();
}

int ProjectionPalette::enabledCellCount() const {
    int n = 0;
    for (const QToolButton* b : m_cells) n += b->isEnabled() ? 1 : 0;
    return n;
}

// ---------------------------------------------------------------------------------------------------------------------
QString ViewBar::tooltipText(Button which) {
    switch (which) {
        case Projection:
            return tr("Proyección: abre una ventana que permite seleccionar proyecciones predeterminadas. Clic alternativo sobre el mismo botón muestra/oculta la ventana.");
        case ProjectionType:
            return tr("Tipo de proyección: cambia el tipo alternativamente (axonométrica/cónica).");
        case RotateAroundPoint:
            return tr("Rotación alrededor de un punto: activa/desactiva la rotación alrededor de un punto de un objeto situado bajo el cursor.");
        case RotateAroundCamera:
            return tr("Rotación alrededor de la cámara: activa/desactiva la rotación alrededor del eje vertical de la cámara.");
        case SectionX:
            return tr("Sección perpendicular al eje 'X' global: define una sección del modelo; doble clic en la flecha invierte el sentido del plano de recorte.");
        case SectionY:
            return tr("Sección perpendicular al eje 'Y' global: define una sección del modelo; doble clic en la flecha invierte el sentido del plano de recorte.");
        case SectionZ:
            return tr("Sección perpendicular al eje 'Z' global: define una sección del modelo; doble clic en la flecha invierte el sentido del plano de recorte.");
        case ClippingVolume:
            return tr("Volumen de recorte: volumen de visualización mediante 6 planos de corte a partir de la envolvente geométrica; flechas para ajustar los planos y arcos para su orientación.");
        case Unidentified1:
        case Unidentified2:
        case ButtonCount:
            break;
    }
    return QString();  // the two last buttons showed no tooltip in the original
}

bool ViewBar::implemented(Button which) {
    return which == Projection || which == ProjectionType || which == RotateAroundPoint || which == RotateAroundCamera;
}

ViewBar::ViewBar(QWidget* parent) : QWidget(parent) {
    setObjectName(QStringLiteral("viewBar"));
    setFixedWidth(24);
    setStyleSheet(QStringLiteral("#viewBar { background: #F0F0F0; }"
                                 "QToolButton { border: 1px solid transparent; background: transparent; }"
                                 "QToolButton:hover:enabled { border: 1px solid #5A9BD5; background: #E4F0FB; }"
                                 "QToolButton:checked { border: 1px solid #5A9BD5; background: #CFE4F7; }"));
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(1, 4, 1, 4);
    layout->setSpacing(2);
    const ViewBarIcon icons[ButtonCount] = {ViewBarIcon::Projection,        ViewBarIcon::ProjectionType, ViewBarIcon::RotateAroundPoint,
                                            ViewBarIcon::RotateAroundCamera, ViewBarIcon::SectionX,       ViewBarIcon::SectionY,
                                            ViewBarIcon::SectionZ,           ViewBarIcon::ClippingVolume, ViewBarIcon::Unidentified1,
                                            ViewBarIcon::Unidentified2};
    for (int i = 0; i < ButtonCount; ++i) {
        const Button which = static_cast<Button>(i);
        auto* b = new QToolButton(this);
        b->setAutoRaise(true);
        b->setFixedSize(22, 22);
        b->setIconSize(QSize(18, 18));
        b->setIcon(viewBarIcon(icons[i]));
        b->setToolTip(tooltipText(which));
        b->setEnabled(implemented(which));
        b->setCheckable(which == Projection || which == RotateAroundPoint || which == RotateAroundCamera);
        m_buttons[static_cast<std::size_t>(i)] = b;
        if (which == Unidentified2) {  // separated by a line, like the original
            auto* line = new QFrame(this);
            line->setFrameShape(QFrame::HLine);
            line->setFrameShadow(QFrame::Sunken);
            layout->addWidget(line);
        }
        layout->addWidget(b);
        if (which == RotateAroundCamera || which == SectionZ || which == ClippingVolume) layout->addSpacing(8);
    }
    layout->addStretch(1);

    m_palette = new ProjectionPalette(this);
    m_palette->hide();
    connect(m_palette, &ProjectionPalette::presetChosen, this, &ViewBar::presetChosen);
    connect(button(Projection), &QToolButton::clicked, this, [this](bool checked) { setPaletteVisible(checked); });
    connect(button(ProjectionType), &QToolButton::clicked, this, &ViewBar::projectionTypeToggled);
    connect(button(RotateAroundPoint), &QToolButton::toggled, this, &ViewBar::rotateAroundPointToggled);
    connect(button(RotateAroundCamera), &QToolButton::toggled, this, &ViewBar::rotateAroundCameraToggled);
}

bool ViewBar::paletteVisible() const { return button(Projection)->isChecked(); }

void ViewBar::setPaletteVisible(bool visible) {
    if (button(Projection)->isChecked() != visible) button(Projection)->setChecked(visible);
    if (visible) placePalette();
    m_palette->setVisible(visible);
    emit paletteToggled(visible);
}

void ViewBar::placePalette() {
    // Flush against the right edge of the bar, level with its top.
    m_palette->move(mapToGlobal(QPoint(width(), 0)));
}

void ViewBar::hideEvent(QHideEvent* event) {
    if (m_palette && m_palette->isVisible()) m_palette->hide();
    QWidget::hideEvent(event);
}

void ViewBar::bind(Viewport3D* viewport) {
    connect(this, &ViewBar::presetChosen, viewport, &Viewport3D::applyProjectionPreset);
    connect(this, &ViewBar::projectionTypeToggled, viewport, [viewport] { viewport->setOrthographic(!viewport->isOrthographic()); });
    connect(this, &ViewBar::rotateAroundPointToggled, viewport, &Viewport3D::setRotateAroundPoint);
    connect(this, &ViewBar::rotateAroundCameraToggled, viewport, &Viewport3D::setRotateAroundCamera);
    connect(viewport, &Viewport3D::rotateAroundPointChanged, button(RotateAroundPoint), &QToolButton::setChecked);
    connect(viewport, &Viewport3D::rotateAroundCameraChanged, button(RotateAroundCamera), &QToolButton::setChecked);
}

// ---------------------------------------------------------------------------------------------------------------------
ViewportFrame::ViewportFrame(QWidget* parent) : QWidget(parent) {
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    m_bar = new ViewBar(this);
    m_viewport = new Viewport3D(this);
    layout->addWidget(m_bar);
    layout->addWidget(m_viewport, 1);
    m_bar->bind(m_viewport);
}

}  // namespace cc::app
