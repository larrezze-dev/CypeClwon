#pragma once
#include "CypeDialog.h"
#include "core/units.h"

class QComboBox;

namespace cc::app {

/// "Unidades de acotación" (Obra > Unidades de acotación): three combos (Dimensiones, Armaduras, Longitudes), each
/// with Metros / Centímetros / Milímetros, and Aceptar / Cancelar. Reference: docs/screenshots/u_units1.png and
/// docs/research/ui/dialogs_unidades.md. The ribbon command that opens it arrives with the Obra tab (ticket TK-019).
class UnitsDialog : public CypeDialog {
    Q_OBJECT
public:
    explicit UnitsDialog(const cc::core::DimensioningUnits& initial, QWidget* parent = nullptr);
    cc::core::DimensioningUnits value() const;
    QComboBox* dimensionsCombo() const { return m_dimensions; }
    QComboBox* reinforcementCombo() const { return m_reinforcement; }
    QComboBox* lengthsCombo() const { return m_lengths; }

private:
    QComboBox* makeCombo(cc::core::LengthUnit current);
    QComboBox* m_dimensions;
    QComboBox* m_reinforcement;
    QComboBox* m_lengths;
};

}  // namespace cc::app
