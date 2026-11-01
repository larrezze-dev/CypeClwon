#pragma once
#include <QDialog>
#include <QWidget>
#include <optional>
#include <vector>

#include "core/structure_model.h"

class QButtonGroup;
class QComboBox;
class QDialogButtonBox;
class QRadioButton;
class QTableWidget;
class QToolButton;

namespace cc::app {

/// Dialog "Asignar elementos a etiquetas" (docs/research/ui/etiquetas_panel.md, ronda 3): an "Operación" group
/// (Asignar / Reemplazar / Eliminar) and an "Etiquetas" table (Seleccionado | Referencia) with one row per layer.
class AssignLayersDialog : public QDialog {
    Q_OBJECT
public:
    explicit AssignLayersDialog(const std::vector<core::Layer>& layers, QWidget* parent = nullptr);

    core::LayerOperation operation() const;
    void setOperation(core::LayerOperation operation);
    std::vector<int> selectedLayerIds() const;
    void setLayerChecked(int layerId, bool checked);

    QTableWidget* table() const { return m_table; }
    QDialogButtonBox* buttons() const { return m_buttons; }

private:
    void updateOk();
    QRadioButton* m_assign;
    QRadioButton* m_replace;
    QRadioButton* m_remove;
    QTableWidget* m_table;
    QDialogButtonBox* m_buttons;
};

/// Dialog "Borrar capa": asks whether the elements of the layer are deleted or moved to another layer.
class DeleteLayerDialog : public QDialog {
    Q_OBJECT
public:
    DeleteLayerDialog(const QString& layerName, const std::vector<core::Layer>& others, QWidget* parent = nullptr);

    core::LayerRemoval removal() const;
    void setRemoval(core::LayerRemoval removal);
    int destinationLayerId() const;  ///< chosen layer of the combo
    void setDestinationLayerId(int id);

    QComboBox* destinationCombo() const { return m_combo; }

private:
    QRadioButton* m_move;
    QRadioButton* m_delete;
    QComboBox* m_combo;
};

/// Every question the panel may ask the user. The default implementation shows the real dialogs; tests inject a fake.
class LayersPrompter {
public:
    virtual ~LayersPrompter() = default;
    /// Aviso "Debe definir, al menos, una capa." (the last layer cannot be removed).
    virtual void warnLastLayer(QWidget* parent) = 0;
    /// Pregunta "¿Desea borrar la capa 'X'?" (Sí / No), for a layer without elements.
    virtual bool confirmRemoval(QWidget* parent, const QString& layerName) = 0;
    /// "Borrar capa" for a layer with elements; nullopt when cancelled. Returns {mode, destination layer id}.
    virtual std::optional<std::pair<core::LayerRemoval, int>> chooseRemoval(QWidget* parent, const QString& layerName,
                                                                            const std::vector<core::Layer>& others) = 0;
    /// "Selección del color"; nullopt when cancelled.
    virtual std::optional<unsigned> chooseColor(QWidget* parent, unsigned current) = 0;
    /// "Asignar elementos a etiquetas"; nullopt when cancelled.
    virtual std::optional<std::pair<core::LayerOperation, std::vector<int>>> chooseAssignment(
        QWidget* parent, const std::vector<core::Layer>& layers) = 0;
};

/// Panel "Etiquetas" (layers) of the left panel. Works on a model it does not own (setStructure); every change made
/// through the panel goes to the model and is announced with layersChanged().
class LayersPanel : public QWidget {
    Q_OBJECT
public:
    enum Column { ColReference = 0, ColActive, ColColor, ColRendered, ColVisible, ColLocked, ColumnCount };

    explicit LayersPanel(QWidget* parent = nullptr);
    ~LayersPanel() override;

    void setStructure(core::StructuralModel* model);
    core::StructuralModel* structure() const { return m_model; }

    /// Replaces the question handler (not owned; nullptr restores the default dialogs).
    void setPrompter(LayersPrompter* prompter);

    /// Id of the selected layer, 0 when none.
    int selectedLayerId() const;
    void selectLayer(int id);

    /// Rebuilds the table from the model (call after external changes); keeps the selection.
    void refresh();

    // Actions (what the buttons and cells do).
    void addLayer();
    void removeSelectedLayer();
    void moveSelectedLayer(int direction);
    void toggleCell(int row, int column);  ///< rendered / visible / locked toggles and the colour chooser
    /// Opens "Asignar elementos a etiquetas" for the given elements and applies the result. False when cancelled or
    /// refused by the model.
    bool assignTo(const std::vector<int>& barIds, const std::vector<int>& shellIds);

    QTableWidget* table() const { return m_table; }
    QToolButton* addButton() const { return m_add; }
    QToolButton* removeButton() const { return m_remove; }
    QToolButton* upButton() const { return m_up; }
    QToolButton* downButton() const { return m_down; }
    QToolButton* assignToolButton() const { return m_assignTool; }

signals:
    void layersChanged();
    void assignToolToggled(bool active);

private:
    void onItemChanged(int row, int column);
    int layerIdAtRow(int row) const;
    void changed();

    core::StructuralModel* m_model = nullptr;
    LayersPrompter* m_prompter;
    QTableWidget* m_table;
    QToolButton* m_add;
    QToolButton* m_remove;
    QToolButton* m_up;
    QToolButton* m_down;
    QToolButton* m_assignTool;
    bool m_refreshing = false;
};

}  // namespace cc::app
