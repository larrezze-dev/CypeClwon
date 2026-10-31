#include "UnitsDialog.h"

#include <QComboBox>
#include <QFormLayout>

namespace cc::app {

using cc::core::LengthUnit;

QComboBox* UnitsDialog::makeCombo(LengthUnit current) {
    auto* combo = new QComboBox(this);
    for (LengthUnit unit : cc::core::allLengthUnits()) {
        combo->addItem(tr(cc::core::unitName(unit)), static_cast<int>(unit));
        if (unit == current) combo->setCurrentIndex(combo->count() - 1);
    }
    return combo;
}

UnitsDialog::UnitsDialog(const cc::core::DimensioningUnits& initial, QWidget* parent)
    : CypeDialog(tr("Unidades de acotación"), parent) {
    setFixedSize(224, 174);
    m_dimensions = makeCombo(initial.dimensions);
    m_reinforcement = makeCombo(initial.reinforcement);
    m_lengths = makeCombo(initial.lengths);
    auto* form = new QFormLayout;
    form->setContentsMargins(12, 10, 12, 6);
    form->setVerticalSpacing(10);
    form->addRow(tr("Dimensiones"), m_dimensions);
    form->addRow(tr("Armaduras"), m_reinforcement);
    form->addRow(tr("Longitudes"), m_lengths);
    contentLayout()->addLayout(form);
    contentLayout()->addStretch(1);
    setupFooter(tr("Aceptar"), tr("Cancelar"));
}

cc::core::DimensioningUnits UnitsDialog::value() const {
    const auto unitOf = [](const QComboBox* c) { return static_cast<LengthUnit>(c->currentData().toInt()); };
    return {unitOf(m_dimensions), unitOf(m_reinforcement), unitOf(m_lengths)};
}

}  // namespace cc::app
