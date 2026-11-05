#pragma once
#include "CypeDialog.h"
#include "core/shell_edit.h"
#include "core/units.h"

class QCheckBox;
class QDoubleSpinBox;
class QGroupBox;
class QRadioButton;

namespace cc::app {

/// "Sección de la lámina" (#65122, Geometria > Laminas > Seccion). Fields from docs/research/ui/laminas.md section 5 (REA,
/// ed3dlib panel "seccion"): "Espesor" (#156855, length > 0), the checkbox "Módulo de balasto en Z local" (#65405) that enables a
/// value editor (> 0, kN/m3) and the group "Comportamiento" (#377936) with "Lineal" (#379172) / "Sólo compresión" (#377979),
/// which depends on the checkbox. PROVISIONAL: the arrangement (the original builds it in code, no dialog resource) and the
/// editor ranges. The "Material" panel of the original dialog is pending (its fields are unknown).
class ShellSectionDialog : public CypeDialog {
    Q_OBJECT
public:
    ShellSectionDialog(const cc::core::ShellSection& initial, cc::core::LengthUnit unit, QWidget* parent = nullptr);
    /// The values entered (thickness converted to metres).
    cc::core::ShellSection section() const;
    void setThickness(double valueInUnit);
    void setBallastEnabled(bool on);
    void setBallastValue(double knM3);
    void setBehaviour(cc::core::BallastBehaviour behaviour);
    /// Whether the value editor and the behaviour group are enabled (they follow the checkbox).
    bool ballastEditable() const;
    bool behaviourEditable() const;

private:
    cc::core::LengthUnit m_unit;
    QDoubleSpinBox* m_thickness = nullptr;
    QCheckBox* m_ballast = nullptr;
    QDoubleSpinBox* m_ballastValue = nullptr;
    QGroupBox* m_behaviourGroup = nullptr;
    QRadioButton* m_linear = nullptr;
    QRadioButton* m_compression = nullptr;
};

}  // namespace cc::app
