#include "RangedEdit.h"

#include <QDoubleValidator>
#include <QMessageBox>
#include <QPushButton>
#include <cmath>

namespace cc::app {

namespace {
std::function<void(QWidget*, const QString&)>& handler() {
    static std::function<void(QWidget*, const QString&)> h;
    return h;
}
}  // namespace

void RangedEdit::setWarningHandler(std::function<void(QWidget*, const QString&)> h) { handler() = std::move(h); }

void RangedEdit::showWarning(QWidget* parent, const QString& text) {
    if (handler()) {
        handler()(parent, text);
        return;
    }
    QMessageBox box(QMessageBox::Warning, tr("Aviso"), text, QMessageBox::NoButton, parent);
    box.addButton(tr("Aceptar"), QMessageBox::AcceptRole);
    box.exec();
}

RangedEdit::RangedEdit(const QString& label, const cc::core::ValueRange& range, int decimals, QWidget* parent)
    : QLineEdit(parent), m_label(label), m_range(range), m_decimals(decimals) {
    setAlignment(Qt::AlignRight);
    setMaximumWidth(110);
    setObjectName(QStringLiteral("ranged_") + label);
    setRange(range);
    setValue(range.min > 0.0 ? range.min : 0.0);
    // Leaving the field (TAB, click elsewhere) or pressing Intro commits.
    connect(this, &QLineEdit::editingFinished, this, [this] { commit(); });
}

QString RangedEdit::formatted(double value) const { return QString::number(value, 'f', m_decimals); }

void RangedEdit::setBoundUnit(const QString& unit) {
    m_boundUnit = unit;
    setRange(m_range);
}

void RangedEdit::setRange(const cc::core::ValueRange& range) {
    m_range = range;
    const QString suffix = m_boundUnit.isEmpty() ? QString() : QLatin1Char(' ') + m_boundUnit;
    setToolTip(tr("Debe estar entre %1 y %2.").arg(formatted(range.min) + suffix, formatted(range.max) + suffix));
}

void RangedEdit::setValue(double value) {
    m_value = value;
    setText(formatted(value));
}

bool RangedEdit::commit() {
    if (m_committing) return true;
    bool ok = false;
    double typed = text().trimmed().replace(QLatin1Char(','), QLatin1Char('.')).toDouble(&ok);
    if (ok && m_decimals == 0) typed = std::round(typed);  // whole-number editors (Z of "Disposición")
    // The text equal to the stored value (already formatted) is accepted as it is: the stored value may sit on a bound.
    if (ok && std::isfinite(typed) && m_range.contains(typed)) {
        const bool changed = typed != m_value;
        m_value = typed;
        setText(formatted(m_value));
        if (changed) emit valueCommitted(m_value);
        return true;
    }
    const QString suffix = m_boundUnit.isEmpty() ? QString() : QLatin1Char(' ') + m_boundUnit;
    m_lastWarning = tr("El valor de '%1' debe estar entre %2 y %3.").arg(m_label, formatted(m_range.min) + suffix, formatted(m_range.max) + suffix);
    setText(formatted(m_value));  // the field recovers the previous value
    m_committing = true;           // the warning window takes the focus: its editingFinished must not warn again
    showWarning(window(), m_lastWarning);
    m_committing = false;
    return false;
}

}  // namespace cc::app
