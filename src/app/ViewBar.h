#pragma once
#include <QString>
#include <QWidget>
#include <array>
#include <vector>

class QToolButton;

namespace cc::app {

class Viewport3D;

/// The 22-cell "Proyección" palette: a floating window of 3 columns of triad buttons, the last one centred
/// (docs/research/ui/barra_vertical_vistas.md; original size 116 x 292 px).
class ProjectionPalette : public QWidget {
    Q_OBJECT
public:
    explicit ProjectionPalette(QWidget* parent = nullptr);

    int cellCount() const { return static_cast<int>(m_cells.size()); }
    QToolButton* cell(int index) const { return m_cells.at(static_cast<std::size_t>(index)); }
    int enabledCellCount() const;

signals:
    /// A cell was clicked (0-based index into core::projectionPresets()).
    void presetChosen(int index);

private:
    std::vector<QToolButton*> m_cells;
};

/// Vertical bar of views of the 3D window (docs/research/ui/barra_vertical_vistas.md): ten icon buttons with the
/// tooltips of the original, stuck to the left edge of the viewport.
class ViewBar : public QWidget {
    Q_OBJECT
public:
    enum Button {
        Projection,
        ProjectionType,
        RotateAroundPoint,
        RotateAroundCamera,
        SectionX,
        SectionY,
        SectionZ,
        ClippingVolume,
        Unidentified1,
        Unidentified2,
        ButtonCount
    };

    explicit ViewBar(QWidget* parent = nullptr);

    QToolButton* button(Button which) const { return m_buttons.at(static_cast<std::size_t>(which)); }
    ProjectionPalette* palette() const { return m_palette; }
    /// Tooltip text of the original (es-ES).
    static QString tooltipText(Button which);
    /// Buttons that do something now; the others are present but disabled (sections, clipping volume, unidentified).
    static bool implemented(Button which);

    void setPaletteVisible(bool visible);
    bool paletteVisible() const;
    /// Mirrors the state of a viewport: projection type and the two rotation modes.
    void bind(Viewport3D* viewport);

signals:
    void paletteToggled(bool visible);
    void presetChosen(int index);
    /// "Tipo de proyección": switch between axonometric (orthographic) and conical (perspective).
    void projectionTypeToggled();
    void rotateAroundPointToggled(bool on);
    void rotateAroundCameraToggled(bool on);

protected:
    void hideEvent(QHideEvent* event) override;

private:
    void placePalette();

    std::array<QToolButton*, ButtonCount> m_buttons{};
    ProjectionPalette* m_palette = nullptr;
};

/// Content of the "3D" child window: the view bar beside the viewport.
class ViewportFrame : public QWidget {
    Q_OBJECT
public:
    explicit ViewportFrame(QWidget* parent = nullptr);
    Viewport3D* viewport() const { return m_viewport; }
    ViewBar* viewBar() const { return m_bar; }

private:
    Viewport3D* m_viewport = nullptr;
    ViewBar* m_bar = nullptr;
};

}  // namespace cc::app
