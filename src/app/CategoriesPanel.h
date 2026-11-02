#pragma once
#include <QMetaType>
#include <QWidget>

#include "core/categories.h"

class QCheckBox;
class QComboBox;
class QTableWidget;

namespace cc::app {

/// "Categorías" section of the left panel (reference: docs/screenshots/k2_nudos_off.png): a fixed table of seven rows
/// (Referencia | eye | padlock), the "use layer colours" check box and the "Elementos considerados en el análisis" combo.
/// Holds the session's core::CategorySettings and emits settingsChanged() whenever a control changes them.
class CategoriesPanel : public QWidget {
    Q_OBJECT
public:
    enum Column { ColReference, ColVisible, ColLocked, ColumnCount };

    explicit CategoriesPanel(QWidget* parent = nullptr);

    cc::core::CategorySettings settings() const { return m_settings; }
    /// Shows `settings` in the controls without emitting settingsChanged().
    void setSettings(const cc::core::CategorySettings& settings);

    // Accessors for tests.
    QTableWidget* table() const { return m_table; }
    QCheckBox* layerColoursCheck() const { return m_layerColours; }
    QComboBox* analysisCombo() const { return m_analysis; }
    /// Same as clicking the eye / padlock cell of `row`.
    void toggleVisible(int row);
    void toggleLocked(int row);

signals:
    void settingsChanged(const cc::core::CategorySettings& settings);

private:
    void refreshTable();

    cc::core::CategorySettings m_settings;
    QTableWidget* m_table = nullptr;
    QCheckBox* m_layerColours = nullptr;
    QComboBox* m_analysis = nullptr;
    bool m_updating = false;
};

}  // namespace cc::app

Q_DECLARE_METATYPE(cc::core::CategorySettings)
