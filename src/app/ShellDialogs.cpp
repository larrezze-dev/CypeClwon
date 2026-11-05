#include "ShellDialogs.h"

#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QRadioButton>
#include <QVBoxLayout>

namespace cc::app {

using namespace cc::core;

ShellSectionDialog::ShellSectionDialog(const ShellSection& initial, LengthUnit unit, QWidget* parent)
    : CypeDialog(tr("Sección de la lámina"), parent), m_unit(unit) {
    auto* form = new QFormLayout();
    m_thickness = new QDoubleSpinBox(this);
    m_thickness->setDecimals(unit == LengthUnit::Metres ? 3 : 1);
    m_thickness->setRange(0.001 * unitsPerMetre(unit), 100.0 * unitsPerMetre(unit));
    m_thickness->setSuffix(QStringLiteral(" ") + QString::fromLatin1(unitSymbol(unit)));
    m_thickness->setValue(fromMetres(initial.thicknessM, unit));
    form->addRow(tr("Espesor"), m_thickness);
    contentLayout()->addLayout(form);

    m_ballast = new QCheckBox(tr("Módulo de balasto en Z local"), this);
    m_ballastValue = new QDoubleSpinBox(this);
    m_ballastValue->setDecimals(1);
    m_ballastValue->setRange(0.1, 1.0e9);
    m_ballastValue->setSuffix(QStringLiteral(" kN/m³"));
    m_ballastValue->setValue(initial.ballastKnM3 > 0.0 ? initial.ballastKnM3 : 10000.0);
    auto* ballastRow = new QHBoxLayout();
    ballastRow->addWidget(m_ballast);
    ballastRow->addWidget(m_ballastValue, 1);
    contentLayout()->addLayout(ballastRow);

    m_behaviourGroup = new QGroupBox(tr("Comportamiento"), this);
    auto* groupLayout = new QVBoxLayout(m_behaviourGroup);
    m_linear = new QRadioButton(tr("Lineal"), m_behaviourGroup);
    m_compression = new QRadioButton(tr("Sólo compresión"), m_behaviourGroup);
    groupLayout->addWidget(m_linear);
    groupLayout->addWidget(m_compression);
    auto* indent = new QHBoxLayout();
    indent->addSpacing(20);  // the original indents the group under the checkbox
    indent->addWidget(m_behaviourGroup, 1);
    contentLayout()->addLayout(indent);

    const auto sync = [this](bool on) {
        m_ballastValue->setEnabled(on);
        m_behaviourGroup->setEnabled(on);
    };
    connect(m_ballast, &QCheckBox::toggled, this, sync);
    setBehaviour(initial.ballastBehaviour);
    m_ballast->setChecked(initial.ballastEnabled);
    sync(initial.ballastEnabled);
    setupFooter(tr("Aceptar"), tr("Cancelar"));
}

ShellSection ShellSectionDialog::section() const {
    ShellSection s;
    s.thicknessM = toMetres(m_thickness->value(), m_unit);
    s.ballastEnabled = m_ballast->isChecked();
    s.ballastKnM3 = m_ballastValue->value();
    s.ballastBehaviour = m_compression->isChecked() ? BallastBehaviour::CompressionOnly : BallastBehaviour::Linear;
    return s;
}

void ShellSectionDialog::setThickness(double valueInUnit) { m_thickness->setValue(valueInUnit); }
void ShellSectionDialog::setBallastEnabled(bool on) { m_ballast->setChecked(on); }
void ShellSectionDialog::setBallastValue(double knM3) { m_ballastValue->setValue(knM3); }
void ShellSectionDialog::setBehaviour(BallastBehaviour behaviour) {
    (behaviour == BallastBehaviour::CompressionOnly ? m_compression : m_linear)->setChecked(true);
}
bool ShellSectionDialog::ballastEditable() const { return m_ballastValue->isEnabled(); }
bool ShellSectionDialog::behaviourEditable() const { return m_behaviourGroup->isEnabled(); }

}  // namespace cc::app
