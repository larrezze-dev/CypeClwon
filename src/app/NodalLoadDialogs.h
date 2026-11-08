#pragma once
#include <QIcon>
#include <array>
#include <vector>

#include "CypeDialog.h"
#include "core/load_case.h"
#include "core/nodal_load.h"
#include "core/node_support.h"

class QCheckBox;
class QComboBox;
class QLineEdit;
class QRadioButton;
class QTableWidget;
class QToolButton;
class QWidget;

namespace cc::app {

/// Dialogs of Carga > Nudos (T-P7-02; behaviour: docs/research/ui/cargas_nudos.md, captures cn_*.png).

/// Pictogram of one of the seven direction buttons: global axes (X right, Z up, Y diagonal towards the top left) and the red arrow that arrives at
/// the origin from the side the load comes from (cn_dir_buttons_zoom.png); the last one is the yellow "Definir dirección" box.
QIcon loadDirectionIcon(cc::core::LoadDirection direction);

/// Text of a combo "Hipótesis": the reference of the case.
QString loadCaseText(const cc::core::LoadCase& loadCase);
/// Fills a combo with the load cases of the set (item data = case id) and selects `selectedId` when present.
void fillLoadCaseCombo(QComboBox* combo, const cc::core::LoadCaseSet& cases, int selectedId);

/// «Introducir cargas sobre nudos» (title of "Nueva") and «Descripción de la carga» (title of "Editar"): combo Hipótesis, Valor (kN), seven
/// direction buttons and, for "Definir dirección", the group «Vector director» (X / Y / Z). Aceptar checks the data with checkNodalLoadData and
/// keeps the window open with an «Aviso» when it is not valid.
class NodalLoadDialog : public CypeDialog {
    Q_OBJECT
public:
    NodalLoadDialog(const QString& title, const cc::core::LoadCaseSet& cases, const cc::core::NodalLoadData& initial, QWidget* parent = nullptr);

    /// The data of the fields (a value that is not a number reads as the last good one).
    cc::core::NodalLoadData data() const;
    void setDirection(cc::core::LoadDirection direction);
    cc::core::LoadDirection direction() const { return m_direction; }
    QComboBox* caseCombo() const { return m_case; }
    QLineEdit* valueEdit() const { return m_value; }
    QToolButton* directionButton(int index) const { return m_buttons[static_cast<std::size_t>(index)]; }
    QWidget* vectorGroup() const { return m_vectorGroup; }
    QLineEdit* vectorEdit(int axis) const { return m_vector[static_cast<std::size_t>(axis)]; }
    void accept() override;

private:
    void updateSize();
    cc::core::LoadCaseSet m_cases;
    cc::core::LoadDirection m_direction = cc::core::LoadDirection::MinusZ;
    QComboBox* m_case = nullptr;
    QLineEdit* m_value = nullptr;
    std::array<QToolButton*, cc::core::kLoadDirectionCount> m_buttons{};
    QWidget* m_vectorGroup = nullptr;
    std::array<QLineEdit*, 3> m_vector{};
};

/// «Hipótesis» (RMB of Carga > Nudos > Hipótesis): only the combo.
class LoadCaseChoiceDialog : public CypeDialog {
    Q_OBJECT
public:
    LoadCaseChoiceDialog(const cc::core::LoadCaseSet& cases, int selectedId, QWidget* parent = nullptr);
    int loadCaseId() const;
    QComboBox* combo() const { return m_combo; }

private:
    QComboBox* m_combo = nullptr;
};

/// «Escalas» (Carga > Visualización): radios «Proporcionales a un valor fijo» (Tamaño máximo para representación, m) and «Proporcionales al valor
/// de la carga» (table Hipótesis | Escala).
class LoadScalesDialog : public CypeDialog {
    Q_OBJECT
public:
    LoadScalesDialog(const cc::core::LoadCaseSet& cases, const cc::core::LoadScales& initial, QWidget* parent = nullptr);
    cc::core::LoadScales scales() const { return m_scales; }
    QRadioButton* fixedRadio() const { return m_fixed; }
    QRadioButton* proportionalRadio() const { return m_proportional; }
    QLineEdit* sizeEdit() const { return m_size; }
    QTableWidget* table() const { return m_table; }
    /// Field of "Escalas de cargas por tipos" (rows Uniforme .. Retracción, `type` = LoadScaleType).
    QLineEdit* typeEdit(int type) const { return m_types[static_cast<std::size_t>(type)]; }
    /// The block of the selected radio: the size row for the fixed mode, the table with the types for the proportional one.
    QWidget* fixedBlock() const { return m_fixedBlock; }
    QWidget* proportionalBlock() const { return m_proportionalBlock; }
    void accept() override;

private:
    cc::core::LoadCaseSet m_cases;
    cc::core::LoadScales m_scales;
    QWidget* m_fixedBlock = nullptr;
    QWidget* m_proportionalBlock = nullptr;
    std::array<QLineEdit*, cc::core::kLoadScaleTypeCount> m_types{};
    QRadioButton* m_fixed = nullptr;
    QRadioButton* m_proportional = nullptr;
    QLineEdit* m_size = nullptr;
    QTableWidget* m_table = nullptr;
};

/// «Desplazamientos prescritos» (530x370): toolbar Añadir / Copiar / Eliminar and the table Hipótesis | Dx (mm) | Dy (mm) | Dz (mm) | Gx (mRad) |
/// Gy (mRad) | Gz (mRad), with only the columns of `columns` (the constrained degrees of freedom). Aceptar runs checkExternalSupport on the sets
/// («Se ha introducido ...», #65466 / #65465) and keeps the window open with an «Aviso».
class PrescribedDisplacementsDialog : public CypeDialog {
    Q_OBJECT
public:
    PrescribedDisplacementsDialog(const cc::core::LoadCaseSet& cases, const std::array<bool, 6>& columns,
                                  const std::vector<cc::core::PrescribedDisplacement>& initial, QWidget* parent = nullptr);
    const std::vector<cc::core::PrescribedDisplacement>& rows() const { return m_rows; }
    /// "Añadir": a row with the first load case and all values 0.000. "Copiar": a copy of the selected row. "Eliminar": removes the selected row.
    void addRow();
    void copyRow();
    void removeRow();
    int currentRow() const;
    /// Sets the text of a cell (what typing in it does): `column` is the table column (1.. = visible DOF columns).
    void setCellText(int row, int column, const QString& text);
    /// Table column of the DOF `dof` (0..5), or -1 when it is hidden.
    int columnOfDof(int dof) const;
    QTableWidget* table() const { return m_table; }
    QToolButton* addButton() const { return m_add; }
    QToolButton* copyButton() const { return m_copy; }
    QToolButton* removeButton() const { return m_remove; }
    void accept() override;

private:
    void refresh(int select);
    void onCellChanged(int row, int column);
    cc::core::LoadCaseSet m_cases;
    std::array<bool, 6> m_columns{};
    std::vector<int> m_dofOfColumn;  ///< table column (from 1) -> DOF
    std::vector<cc::core::PrescribedDisplacement> m_rows;
    QTableWidget* m_table = nullptr;
    QToolButton* m_add = nullptr;
    QToolButton* m_copy = nullptr;
    QToolButton* m_remove = nullptr;
    bool m_loading = false;
};

/// Floating window «Propiedades - Hipótesis vista»: combo Hipótesis and «Ver todas». The loads that are introduced take the shown case by default
/// (tooltip #64651) and only the loads of that case (or all of them) are drawn.
class LoadViewPanel : public CypeDialog {
    Q_OBJECT
public:
    explicit LoadViewPanel(QWidget* parent = nullptr);
    void setCases(const cc::core::LoadCaseSet& cases);
    int caseId() const;
    bool viewAll() const;
    void setCaseId(int id);
    void setViewAll(bool on);
    QComboBox* combo() const { return m_combo; }
    QCheckBox* allCheck() const { return m_all; }

signals:
    void viewChanged(int caseId, bool all);
    void closed();

protected:
    void done(int result) override;

private:
    QComboBox* m_combo = nullptr;
    QCheckBox* m_all = nullptr;
    bool m_loading = false;
};

}  // namespace cc::app
