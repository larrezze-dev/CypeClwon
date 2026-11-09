#pragma once
#include <QIcon>
#include <array>
#include <map>
#include <vector>

#include "CypeDialog.h"
#include "core/load_case.h"
#include "core/shell_load.h"

class QComboBox;
class QLabel;
class QLineEdit;
class QRadioButton;
class QToolButton;
class QWidget;

namespace cc::app {

/// Dialogs of Carga > Láminas (T-P7-04; behaviour: docs/research/ui/cargas_laminas.md sections 8-10, captures cl_*.png / cl3_*.png).

/// Pictogram of one of the five buttons of «Tipo de carga» (red load over a black bar; Δt and Δε for the temperature and the shrinkage).
QIcon shellLoadTypeIcon(cc::core::ShellLoadType type);
/// Pictogram of one of the two sense buttons: axes of the shell (x oblique, z towards the top left, y to the left) or global axes (X, Y, Z), with the
/// red arrow along +Z (button 1) or -Z (button 2).
QIcon shellLoadSenseIcon(cc::core::ShellLoadSense sense, bool local);

/// «Introducir cargas sobre láminas» (title of "Nueva") and «Descripción de la carga» (title of "Editar"): 310 x 420 as measured, the height does not
/// change with the type. Aceptar checks every visible field in the order of the tab sequence (parseShellLoadField, then checkShellLoadData) and keeps
/// the window open with an «Aviso» (yellow) or «Error» (red) otherwise; the field recovers its previous value.
class ShellLoadDialog : public CypeDialog {
    Q_OBJECT
public:
    ShellLoadDialog(const QString& title, const cc::core::LoadCaseSet& cases, const cc::core::ShellLoadData& initial, QWidget* parent = nullptr);

    /// The data of the fields (a text that is not a number reads as the last good value).
    cc::core::ShellLoadData data() const;
    cc::core::ShellLoadType type() const { return m_type; }
    void setType(cc::core::ShellLoadType type);
    cc::core::ShellLoadAxes axes() const { return m_axes; }
    void setAxes(cc::core::ShellLoadAxes axes);
    cc::core::ShellLoadSense sense() const { return m_sense; }
    void setSense(cc::core::ShellLoadSense sense);

    QComboBox* caseCombo() const { return m_case; }
    QToolButton* typeButton(int index) const { return m_typeButtons[static_cast<std::size_t>(index)]; }
    QToolButton* senseButton(int index) const { return m_senseButtons[static_cast<std::size_t>(index)]; }
    QLineEdit* edit(cc::core::ShellLoadField field) const { return m_edits.at(field); }
    QLabel* label(cc::core::ShellLoadField field) const { return m_labels.at(field); }
    QLabel* unitLabel(cc::core::ShellLoadField field) const { return m_units.at(field); }
    QRadioButton* globalRadio() const { return m_globalRadio; }
    QRadioButton* localRadio() const { return m_localRadio; }
    QWidget* axesGroup() const { return m_axesGroup; }
    QWidget* senseGroup() const { return m_senseGroup; }
    /// Fields in the order of the tab sequence that are visible now.
    std::vector<cc::core::ShellLoadField> visibleFields() const;
    void accept() override;

private:
    void refresh();
    void applyTypeDefaults(cc::core::ShellLoadType from, cc::core::ShellLoadType to);
    QString formatted(cc::core::ShellLoadField field, double value) const;
    cc::core::ShellLoadData current() const;  ///< state + the numbers of the fields

    cc::core::LoadCaseSet m_cases;
    cc::core::ShellLoadType m_type = cc::core::ShellLoadType::Uniform;
    cc::core::ShellLoadAxes m_axes = cc::core::ShellLoadAxes::Local;
    cc::core::ShellLoadSense m_sense = cc::core::ShellLoadSense::PlusZ;
    QComboBox* m_case = nullptr;
    std::array<QToolButton*, cc::core::kShellLoadTypeCount> m_typeButtons{};
    std::array<QToolButton*, 2> m_senseButtons{};
    std::map<cc::core::ShellLoadField, QLineEdit*> m_edits;
    std::map<cc::core::ShellLoadField, QLabel*> m_labels;
    std::map<cc::core::ShellLoadField, QLabel*> m_units;
    std::map<cc::core::ShellLoadField, QString> m_previous;
    QWidget* m_value2Block = nullptr;
    QWidget* m_axesGroup = nullptr;
    QRadioButton* m_globalRadio = nullptr;
    QRadioButton* m_localRadio = nullptr;
    QWidget* m_senseGroup = nullptr;
    bool m_refreshing = false;
};

/// «Generación de cargas» (Peso propio of the shells): radios Activar (default) and Desactivar (same window as the bars, measured).
class ShellSelfWeightDialog : public CypeDialog {
    Q_OBJECT
public:
    explicit ShellSelfWeightDialog(bool activate, QWidget* parent = nullptr);
    bool activate() const;
    QRadioButton* activateRadio() const { return m_activate; }
    QRadioButton* deactivateRadio() const { return m_deactivate; }

private:
    QRadioButton* m_activate = nullptr;
    QRadioButton* m_deactivate = nullptr;
};

}  // namespace cc::app
