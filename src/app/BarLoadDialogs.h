#pragma once
#include <QIcon>
#include <array>
#include <map>
#include <vector>

#include "CypeDialog.h"
#include "core/bar_load.h"
#include "core/load_case.h"

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QRadioButton;
class QToolButton;
class QWidget;

namespace cc::app {

/// Dialogs of Carga > Barras (T-P7-03; behaviour: docs/research/ui/cargas_barras.md sections 10-12, captures cb_*.png).

/// Pictogram of one of the nine buttons of «Tipo de carga» (red load over a black bar).
QIcon barLoadTypeIcon(cc::core::BarLoadType type);
/// Pictogram of one of the seven direction buttons: global axes (as in the nodal loads) or local axes of the bar; `moment` gives the double arrow
/// head of the point moment. The last one (UserVector) is the yellow "Definir dirección" box.
QIcon barLoadDirectionIcon(cc::core::LoadDirection direction, bool local, bool moment);
/// Pictogram of the three buttons of the variable temperature (I profile with T1 / T2): 0 = Y+, 1 = Z+, 2 = defined by an angle.
QIcon barTemperatureDirectionIcon(cc::core::TemperatureDirection direction);

/// «Introducir cargas sobre barras» (title of "Nueva") and «Descripción de la carga» (title of "Editar"): 402 wide, the height is fixed when the
/// window opens according to the type shown (516 with direction, 416 variable temperature, 262 uniform temperature). Aceptar checks every field in
/// the order of the tab sequence (parseBarLoadField, then checkBarLoadData with `lengthM` = the shortest selected bar) and keeps the window
/// open with an «Aviso» (yellow) or «Error» (red) otherwise; the field recovers its previous value.
class BarLoadDialog : public CypeDialog {
    Q_OBJECT
public:
    BarLoadDialog(const QString& title, const cc::core::LoadCaseSet& cases, const cc::core::BarLoadData& initial, double lengthM,
                  QWidget* parent = nullptr);

    /// The data of the fields (a text that is not a number reads as the last good value).
    cc::core::BarLoadData data() const;
    cc::core::BarLoadType type() const { return m_type; }
    void setType(cc::core::BarLoadType type);
    cc::core::BarLoadAxes axes() const { return m_axes; }
    void setAxes(cc::core::BarLoadAxes axes);
    cc::core::LoadDirection direction() const { return m_direction; }
    void setDirection(cc::core::LoadDirection direction);
    cc::core::TemperatureDirection temperatureDirection() const { return m_temperatureDirection; }
    void setTemperatureDirection(cc::core::TemperatureDirection direction);
    bool relative() const;
    void setRelative(bool relative);

    QComboBox* caseCombo() const { return m_case; }
    QToolButton* typeButton(int index) const { return m_typeButtons[static_cast<std::size_t>(index)]; }
    QToolButton* directionButton(int index) const { return m_directionButtons[static_cast<std::size_t>(index)]; }
    QToolButton* temperatureButton(int index) const { return m_temperatureButtons[static_cast<std::size_t>(index)]; }
    QLineEdit* edit(cc::core::BarLoadField field) const { return m_edits.at(field); }
    QLabel* label(cc::core::BarLoadField field) const { return m_labels.at(field); }
    QLabel* unitLabel(cc::core::BarLoadField field) const { return m_units.at(field); }
    QCheckBox* relativeCheck() const { return m_relative; }
    QRadioButton* globalRadio() const { return m_globalRadio; }
    QRadioButton* localRadio() const { return m_localRadio; }
    QWidget* axesGroup() const { return m_axesGroup; }
    QWidget* directionGroup() const { return m_directionGroup; }
    QWidget* vectorGroup() const { return m_vectorGroup; }
    QWidget* anglesGroup() const { return m_anglesGroup; }
    QWidget* temperatureGroup() const { return m_temperatureGroup; }
    /// Fields in the order of the tab sequence that are visible now.
    std::vector<cc::core::BarLoadField> visibleFields() const;
    void accept() override;

private:
    void refresh();
    void applyTypeDefaults(cc::core::BarLoadType from, cc::core::BarLoadType to);
    QString formatted(cc::core::BarLoadField field, double value) const;
    cc::core::BarLoadData current() const;  ///< state + the numbers of the fields

    cc::core::LoadCaseSet m_cases;
    double m_lengthM = 0.0;
    cc::core::BarLoadType m_type = cc::core::BarLoadType::Uniform;
    cc::core::BarLoadAxes m_axes = cc::core::BarLoadAxes::Global;
    cc::core::LoadDirection m_direction = cc::core::LoadDirection::MinusZ;
    cc::core::TemperatureDirection m_temperatureDirection = cc::core::TemperatureDirection::PlusZ;
    cc::core::BarLoadData m_initial;
    QComboBox* m_case = nullptr;
    std::array<QToolButton*, cc::core::kBarLoadTypeCount> m_typeButtons{};
    std::array<QToolButton*, cc::core::kLoadDirectionCount> m_directionButtons{};
    std::array<QToolButton*, 3> m_temperatureButtons{};
    std::map<cc::core::BarLoadField, QLineEdit*> m_edits;
    std::map<cc::core::BarLoadField, QLabel*> m_labels;
    std::map<cc::core::BarLoadField, QLabel*> m_units;
    std::map<cc::core::BarLoadField, QString> m_previous;
    QWidget* m_value2Block = nullptr;
    QWidget* m_value1Block = nullptr;
    QCheckBox* m_relative = nullptr;
    QWidget* m_startBlock = nullptr;
    QWidget* m_endBlock = nullptr;
    QWidget* m_axesGroup = nullptr;
    QRadioButton* m_globalRadio = nullptr;
    QRadioButton* m_localRadio = nullptr;
    QWidget* m_directionGroup = nullptr;
    QLabel* m_directionTitle = nullptr;
    QWidget* m_forceButtons = nullptr;
    QWidget* m_temperatureGroup = nullptr;
    QWidget* m_vectorGroup = nullptr;
    QWidget* m_anglesGroup = nullptr;
    QWidget* m_temperatureAngle = nullptr;
    QWidget* m_positionRows = nullptr;
    bool m_refreshing = false;
};

/// «Generación de cargas» (Peso propio): radios Activar (default) and Desactivar (186 x 134 as measured).
class BarSelfWeightDialog : public CypeDialog {
    Q_OBJECT
public:
    explicit BarSelfWeightDialog(bool activate, QWidget* parent = nullptr);
    bool activate() const;
    QRadioButton* activateRadio() const { return m_activate; }
    QRadioButton* deactivateRadio() const { return m_deactivate; }

private:
    QRadioButton* m_activate = nullptr;
    QRadioButton* m_deactivate = nullptr;
};

}  // namespace cc::app
