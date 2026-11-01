#pragma once
#include <QWidget>

#include "core/display_options.h"

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;

namespace cc::app {

class Viewport3D;

/// "Opciones de visualización" section of the left panel (reference: docs/screenshots/tp2_combo_open.png).
/// Holds the session's core::DisplayOptions and emits optionsChanged() whenever a control changes them.
class DisplayOptionsPanel : public QWidget {
    Q_OBJECT
public:
    explicit DisplayOptionsPanel(QWidget* parent = nullptr);

    cc::core::DisplayOptions options() const { return m_options; }
    /// Shows `options` in the controls without emitting optionsChanged(). Invalid values are ignored.
    void setOptions(const cc::core::DisplayOptions& options);

    /// Applies what the viewport can honour today: element information, tooltip position and shell filling.
    /// Texts, local axes, filled loads and node size are only stored (TK-028).
    static void applyTo(Viewport3D& viewport, const cc::core::DisplayOptions& options);

signals:
    void optionsChanged(const cc::core::DisplayOptions& options);

private:
    void readControls();

    cc::core::DisplayOptions m_options;
    QCheckBox* m_info = nullptr;
    QCheckBox* m_texts = nullptr;
    QCheckBox* m_axes = nullptr;
    QCheckBox* m_shells = nullptr;
    QCheckBox* m_loads = nullptr;
    QDoubleSpinBox* m_nodeSize = nullptr;
    QDoubleSpinBox* m_textSize = nullptr;
    QComboBox* m_tooltip = nullptr;
    bool m_updating = false;
};

}  // namespace cc::app
