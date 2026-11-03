#pragma once
#include <QChar>
#include <QString>
#include <array>

#include "CypeDialog.h"

class QLabel;
class QPushButton;

namespace cc::app {

/// "Calculadora" of Geometria > Nudos (nudos.md section 8, docs/screenshots/orig_nudos_nuevo_02_calculadora.png): the modal window the
/// original opens when a digit is typed with the viewport focused in "Nuevo". It holds the three coordinates of the point; the
/// active field is the grey one. Digits pile up in the active field, Intro confirms it and moves on X -> Y -> Z, and Intro on Z
/// confirms the point (pointConfirmed) and closes the window. Esc closes it without a point. Tab does NOT change the field
/// (measured in the original).
///
/// Measured: layout, digits, ".", Backspace, Intro, Esc, Tab. NOT measured (the buttons are present and do nothing, "C" clears
/// the active field and "AC" the three fields as a minimal safe behaviour): "+", "-", "*", "/", "=", "+/-", "???", "MC", "M-",
/// "M+", "MR", "C", "AC" (arithmetic, sign change and memory).
class NodeCalculatorDialog : public CypeDialog {
    Q_OBJECT
public:
    explicit NodeCalculatorDialog(QWidget* parent = nullptr);

    /// Clears the three fields to "0", makes X active and types `first` (the digit that opened the window).
    void start(QChar first);
    int activeField() const { return m_active; }
    QString fieldText(int index) const { return m_text.at(static_cast<std::size_t>(index)); }
    double value(int index) const;
    /// Digit or '.' typed into the active field.
    void pressDigit(QChar ch);
    void pressBackspace();
    void pressEnter();
    void pressClear();     ///< "C": clears the active field (unmeasured)
    void pressClearAll();  ///< "AC": clears the three fields and goes back to X (unmeasured)
    /// Keyboard button with that caption ("7", ".", "AC", ...); nullptr when none.
    QPushButton* button(const QString& caption) const;
    QPushButton* enterButton() const { return m_enter; }
    QPushButton* backspaceButton() const { return m_back; }

signals:
    /// Intro on the Z field: the three coordinates, in metres.
    void pointConfirmed(double x, double y, double z);

protected:
    void keyPressEvent(QKeyEvent* event) override;
    bool focusNextPrevChild(bool next) override;

private:
    void refreshFields();

    std::array<QString, 3> m_text{QStringLiteral("0"), QStringLiteral("0"), QStringLiteral("0")};
    std::array<QLabel*, 3> m_fields{};
    int m_active = 0;
    std::array<QPushButton*, 28> m_buttons{};
    int m_buttonCount = 0;
    QPushButton* m_back = nullptr;
    QPushButton* m_enter = nullptr;
};

}  // namespace cc::app
