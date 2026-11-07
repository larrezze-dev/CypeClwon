#pragma once
#include <QLineEdit>
#include <QString>
#include <functional>

#include "core/shell_material.h"

namespace cc::app {

/// Numeric editor of the dialogs of the original (laminas.md section 7): the user types a value, and on leaving the field (or on
/// Aceptar, through commit()) a value outside the range shows the window "Aviso" with "El valor de 'X' debe estar entre A y B." and the
/// field recovers its previous value. The tooltip says "Debe estar entre A y B." (measured).
class RangedEdit : public QLineEdit {
    Q_OBJECT
public:
    RangedEdit(const QString& label, const cc::core::ValueRange& range, int decimals, QWidget* parent = nullptr);

    double value() const { return m_value; }
    void setValue(double value);
    void setRange(const cc::core::ValueRange& range);
    const cc::core::ValueRange& range() const { return m_range; }
    int decimals() const { return m_decimals; }
    const QString& label() const { return m_label; }
    /// Unit written after each bound of the warning and the tooltip ("-360.000 grados y 360.000 grados", measured for the angle).
    void setBoundUnit(const QString& unit);
    /// Text written verbatim after each bound (no separating space added): " " gives the measured "entre 0.000  y 100.000 ." of the
    /// buckling dialogs (pandeo.md 9.2). Takes precedence over the bound unit.
    void setBoundSuffix(const QString& suffix);
    /// Strict number reading (vinculaciones.md 5.1): the text is not rewritten ("1,5" is not a number) and an unreadable text says
    /// "La cadena introducida no es un número." instead of the range warning. Off by default.
    void setStrictNumber(bool on) { m_strict = on; }
    /// Reads the text now. True when it is valid (the value is taken); otherwise warns, restores the previous text and returns false.
    bool commit();
    /// Warning text of the last rejected entry (tests).
    const QString& lastWarning() const { return m_lastWarning; }

    /// Shows the warning window. The default is a QMessageBox titled "Aviso" with the button "Aceptar"; tests replace it.
    static void setWarningHandler(std::function<void(QWidget*, const QString&)> handler);
    static void showWarning(QWidget* parent, const QString& text);

signals:
    void valueCommitted(double value);

private:
    QString formatted(double value) const;
    QString boundSuffix() const;

    QString m_label;
    QString m_boundUnit;
    QString m_rawSuffix;
    bool m_hasRawSuffix = false;
    cc::core::ValueRange m_range;
    int m_decimals;
    double m_value = 0.0;
    QString m_lastWarning;
    bool m_committing = false;
    bool m_strict = false;
};

}  // namespace cc::app
