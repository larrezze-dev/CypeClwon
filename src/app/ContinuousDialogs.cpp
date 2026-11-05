#include "ContinuousDialogs.h"

#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QVBoxLayout>

namespace cc::app {

// ---- ReferenceDialog -----------------------------------------------------------------------------------------------------

ReferenceDialog::ReferenceDialog(const QString& title, const QString& label, const QString& initial, QWidget* parent) : CypeDialog(title, parent) {
    auto* form = new QFormLayout();
    m_edit = new QLineEdit(initial, this);
    m_edit->setMaxLength(kMaxLength);
    m_edit->setMinimumWidth(220);
    form->addRow(label, m_edit);
    contentLayout()->addLayout(form);
    m_error = new QLabel(this);
    m_error->setStyleSheet(QStringLiteral("color: #C00000;"));
    m_error->setWordWrap(true);
    m_error->hide();
    contentLayout()->addWidget(m_error);
    setupFooter(tr("Aceptar"), tr("Cancelar"));
}

QString ReferenceDialog::reference() const { return m_edit->text(); }
void ReferenceDialog::setReference(const QString& text) { m_edit->setText(text); }
QString ReferenceDialog::errorText() const { return m_error->isHidden() ? QString() : m_error->text(); }

void ReferenceDialog::accept() {
    if (m_edit->text().trimmed().isEmpty()) {  // #64971
        m_error->setText(tr("La referencia no puede quedar vacía."));
        m_error->show();
        return;
    }
    CypeDialog::accept();
}

// ---- SlabDepthDialog -----------------------------------------------------------------------------------------------------

SlabDepthDialog::SlabDepthDialog(std::optional<double> initialM, QWidget* parent) : CypeDialog(tr("Editar canto del forjado"), parent) {
    auto* form = new QFormLayout();
    m_auto = new QCheckBox(tr("Automático"), this);
    m_value = new QDoubleSpinBox(this);
    m_value->setRange(1.0, 1000.0);
    m_value->setDecimals(1);
    m_value->setSuffix(tr(" cm"));
    m_value->setValue(initialM ? *initialM * 100.0 : 30.0);
    m_auto->setChecked(!initialM);
    m_value->setEnabled(initialM.has_value());
    connect(m_auto, &QCheckBox::toggled, m_value, [this](bool on) { m_value->setEnabled(!on); });
    form->addRow(m_auto);
    form->addRow(tr("Canto del forjado"), m_value);
    contentLayout()->addLayout(form);
    setupFooter(tr("Aceptar"), tr("Cancelar"));
}

std::optional<double> SlabDepthDialog::depthM() const {
    if (m_auto->isChecked()) return std::nullopt;
    return m_value->value() / 100.0;
}
void SlabDepthDialog::setAutomatic(bool on) { m_auto->setChecked(on); }
void SlabDepthDialog::setDepthCm(double cm) {
    m_auto->setChecked(false);
    m_value->setValue(cm);
}

// ---- DeflectionLimitDialog -----------------------------------------------------------------------------------------------

DeflectionLimitDialog::DeflectionLimitDialog(double relative, double absoluteMm, QWidget* parent) : CypeDialog(tr("Flecha límite de vigas"), parent) {
    auto* form = new QFormLayout();
    m_relative = new QDoubleSpinBox(this);
    m_relative->setRange(0.0, 100000.0);
    m_relative->setDecimals(0);
    m_relative->setPrefix(tr("L / "));
    m_relative->setSpecialValueText(tr("No definida"));
    m_absolute = new QDoubleSpinBox(this);
    m_absolute->setRange(0.0, 100000.0);
    m_absolute->setDecimals(1);
    m_absolute->setSuffix(tr(" mm"));
    m_absolute->setSpecialValueText(tr("No definida"));
    setValues(relative, absoluteMm);
    form->addRow(tr("Flecha límite máxima relativa"), m_relative);
    form->addRow(tr("Flecha límite máxima absoluta"), m_absolute);
    contentLayout()->addLayout(form);
    auto* note = new QLabel(tr("Campos provisionales: el diálogo original no se ha medido."), this);
    note->setWordWrap(true);
    note->setStyleSheet(QStringLiteral("color: #808080;"));
    contentLayout()->addWidget(note);
    setupFooter(tr("Aceptar"), tr("Cancelar"));
}

double DeflectionLimitDialog::relative() const { return m_relative->value(); }
double DeflectionLimitDialog::absoluteMm() const { return m_absolute->value(); }
void DeflectionLimitDialog::setValues(double relative, double absoluteMm) {
    m_relative->setValue(relative);
    m_absolute->setValue(absoluteMm);
}

}  // namespace cc::app
