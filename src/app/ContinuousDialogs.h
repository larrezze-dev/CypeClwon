#pragma once
#include <QString>
#include <optional>

#include "CypeDialog.h"

class QCheckBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;

namespace cc::app {

/// Small dialogs of "Edición de pilares / vigas" (T-P5-08, GUI part). Source: docs/research/ui/edicion_pilares_vigas.md.

/// One reference (#65436 "Editar referencia", #64878 / #64880, #64972 "Referencia inicial"): a text field of at most 30
/// characters (the original limit, section 2 of the document); an empty text is refused with #64971 (kept inside the dialog).
class ReferenceDialog : public CypeDialog {
    Q_OBJECT
public:
    static constexpr int kMaxLength = 30;
    ReferenceDialog(const QString& title, const QString& label, const QString& initial, QWidget* parent = nullptr);
    QString reference() const;
    void setReference(const QString& text);
    /// Message of the refusal currently shown (empty when none).
    QString errorText() const;
    void accept() override;

private:
    QLineEdit* m_edit = nullptr;
    QLabel* m_error = nullptr;
};

/// "Editar canto del forjado" (#65437): the slab depth fixed at a node, in centimetres, or "automático".
/// PROVISIONAL layout (the original dialog was not inspected).
class SlabDepthDialog : public CypeDialog {
    Q_OBJECT
public:
    explicit SlabDepthDialog(std::optional<double> initialM, QWidget* parent = nullptr);
    /// Depth in metres; nullopt = automatic.
    std::optional<double> depthM() const;
    void setAutomatic(bool on);
    void setDepthCm(double cm);

private:
    QCheckBox* m_auto = nullptr;
    QDoubleSpinBox* m_value = nullptr;
};

/// "Flecha límite de vigas" (#65422 / #64343): relative limit (L / n) and absolute one (mm), 0 = not set (labels #64291 and
/// #64290). PROVISIONAL: the original dialog and the ranges were not measured (the "activa" pair #64292 / #64293 is not stored).
class DeflectionLimitDialog : public CypeDialog {
    Q_OBJECT
public:
    DeflectionLimitDialog(double relative, double absoluteMm, QWidget* parent = nullptr);
    double relative() const;
    double absoluteMm() const;
    void setValues(double relative, double absoluteMm);

private:
    QDoubleSpinBox* m_relative = nullptr;
    QDoubleSpinBox* m_absolute = nullptr;
};

}  // namespace cc::app
