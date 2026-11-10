#pragma once
#include <QPointer>
#include <vector>

#include "CypeDialog.h"
#include "core/load_case.h"
#include "core/panel.h"

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QRadioButton;
class QTabWidget;
class QTableWidget;
class QToolButton;
class QWidget;

namespace cc::app {

/// Dialogs of Carga > Paños and Carga > Cargas superficiales (T-P7-05; behaviour: docs/research/ui/cargas_panos.md sections 10-12 [CAP]).

/// «Cargas en paño»: a list of rows Hipótesis | Sentido positivo | Valor (uniform loads) and, when the panel is NOT horizontal, a second tab «Variables
/// con la altura» with Hipótesis | Sentido positivo | Valor en la cota inferior | Valor en la cota superior (the lower value first, as measured).
/// Toolbar Añadir / Copiar / Borrar. A new row takes `defaultCaseId` (the viewed hypothesis) and 0.000. Several rows can be selected and a typed value
/// is applied to all of them (measured). A cell is checked when it is committed (range: «Aviso»); Aceptar checks every row of both tabs (null value:
/// «Error», #65050) and keeps the window open otherwise. An empty list is accepted.
class PanelLoadsDialog : public CypeDialog {
    Q_OBJECT
public:
    PanelLoadsDialog(const cc::core::LoadCaseSet& cases, const std::vector<cc::core::Point3>& polygon, const std::vector<cc::core::SurfaceLoadData>& initial,
                     int defaultCaseId, QWidget* parent = nullptr);

    /// Uniform rows first, then the variable ones (the order of the tabs).
    std::vector<cc::core::SurfaceLoadData> rows() const;
    bool tabbed() const { return m_tabs != nullptr; }
    QTabWidget* tabs() const { return m_tabs; }
    QTableWidget* uniformTable() const { return m_uniformTable; }
    QTableWidget* variableTable() const { return m_variableTable; }
    QToolButton* addButton() const { return m_add; }
    QToolButton* copyButton() const { return m_copy; }
    QToolButton* removeButton() const { return m_remove; }
    /// The table of the current tab.
    QTableWidget* currentTable() const;
    /// Buttons of the list (what the clicks do).
    void addRow();
    void copyRow();
    void removeRows();
    /// Types the text in a cell of the table (what the user does): column 2 = Valor of the uniform table, 2 / 3 = inferior / superior of the other.
    void setCellText(bool variable, int row, int column, const QString& text);
    void setRowCase(bool variable, int row, int loadCaseId);
    void accept() override;

private:
    struct Table {
        QTableWidget* widget = nullptr;
        std::vector<cc::core::SurfaceLoadData> rows;
        bool variable = false;
    };
    Table& table(bool variable) { return variable ? m_variable : m_uniform; }
    QWidget* buildTable(Table& table, QWidget* parent);
    void refresh(bool variable, const std::vector<int>& select);
    void onCellChanged(bool variable, int row, int column);
    std::vector<int> selectedRows(bool variable) const;
    QString senseText(int loadCaseId) const;

    cc::core::LoadCaseSet m_cases;
    std::vector<cc::core::Point3> m_polygon;
    int m_defaultCase = 0;
    Table m_uniform, m_variable;
    QTableWidget* m_uniformTable = nullptr;
    QTableWidget* m_variableTable = nullptr;
    QTabWidget* m_tabs = nullptr;
    QToolButton* m_add = nullptr;
    QToolButton* m_copy = nullptr;
    QToolButton* m_remove = nullptr;
    bool m_loading = false;
};

/// «Introducir cargas superficiales» (Nueva / Copiar) and «Cargas superficiales» (Editar), measured ~280x185 (horizontal) / 280x215: row Hipótesis,
/// «Sentido positivo: ...» (follows the hypothesis), a line and the values. A horizontal surface has the uniform value only («Valor», kN/m²); an
/// inclined one adds the radios «Uniforme» / «Variable con la altura» with «Valor en la cota superior» first and «Valor en la cota inferior» second.
/// Aceptar parses the visible fields (range: «Aviso») and checks the data (null: «Error», #65050); the window stays open otherwise.
class SurfaceLoadDialog : public CypeDialog {
    Q_OBJECT
public:
    SurfaceLoadDialog(const QString& title, const cc::core::LoadCaseSet& cases, const std::vector<cc::core::Point3>& polygon,
                      const cc::core::SurfaceLoadData& initial, QWidget* parent = nullptr);
    cc::core::SurfaceLoadData data() const;
    QComboBox* caseCombo() const { return m_case; }
    QLabel* senseLabel() const { return m_sense; }
    QRadioButton* uniformRadio() const { return m_uniformRadio; }
    QRadioButton* variableRadio() const { return m_variableRadio; }
    QLineEdit* valueEdit() const { return m_value; }
    QLineEdit* upperEdit() const { return m_upper; }
    QLineEdit* lowerEdit() const { return m_lower; }
    bool horizontal() const { return m_horizontal; }
    void accept() override;

private:
    void refresh();
    cc::core::SurfaceLoadData current() const;
    QString senseText() const;

    cc::core::LoadCaseSet m_cases;
    std::vector<cc::core::Point3> m_polygon;
    bool m_horizontal = true;
    QComboBox* m_case = nullptr;
    QLabel* m_sense = nullptr;
    QRadioButton* m_uniformRadio = nullptr;
    QRadioButton* m_variableRadio = nullptr;
    QWidget* m_uniformBlock = nullptr;
    QWidget* m_variableBlock = nullptr;
    QLineEdit* m_value = nullptr;
    QLineEdit* m_upper = nullptr;
    QLineEdit* m_lower = nullptr;
    cc::core::SurfaceLoadData m_last;
};

/// «Mover cargas superficiales»: the box that Carga > Cargas superficiales > Mover shows when it is activated (and again with RMB while nothing is
/// selected): the check «Mover también las cargas aplicadas sobre la misma zona pertenecientes a otras hipótesis» (checked by default) and only Aceptar.
class MoveSurfaceLoadsDialog : public CypeDialog {
    Q_OBJECT
public:
    explicit MoveSurfaceLoadsDialog(bool alsoOtherCases, QWidget* parent = nullptr);
    bool alsoOtherCases() const;
    QCheckBox* check() const { return m_check; }

private:
    QCheckBox* m_check = nullptr;
};

/// Floating window «Propiedades» of Paños > Nuevo / Dirección de reparto: the combo «Dirección de reparto de las cargas» with «Paralela al lado
/// seleccionado» (default) and «Perpendicular al lado seleccionado» (measured: a combo in the window, not radios).
class PanelDirectionPanel : public CypeDialog {
    Q_OBJECT
public:
    explicit PanelDirectionPanel(QWidget* parent = nullptr);
    cc::core::PanelDirectionMode mode() const;
    void setMode(cc::core::PanelDirectionMode mode);
    QComboBox* combo() const { return m_combo; }

signals:
    void modeChanged(cc::core::PanelDirectionMode mode);
    void closed();

protected:
    void done(int result) override;

private:
    QComboBox* m_combo = nullptr;
};

}  // namespace cc::app
