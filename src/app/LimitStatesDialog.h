#pragma once
#include "CypeDialog.h"
#include "core/limit_states.h"
#include "core/standards.h"

class QComboBox;
class QRadioButton;

namespace cc::app {

/// "Estados límite" (358x438): combinations used for each limit state, opened from the wizard and after
/// "Terminar". Reference: docs/screenshots/orig_obra_vacia_12_estados_limite.png and
/// docs/research/ui/dialogs_estados_limite.md. The pencil buttons and the per-row editors are gaps of the
/// research (ticket TK-017), so they are disabled.
class LimitStatesDialog : public CypeDialog {
    Q_OBJECT
public:
    LimitStatesDialog(const cc::core::StandardsSelection& standards, const cc::core::LimitStatesSettings& initial,
                      QWidget* parent = nullptr);
    cc::core::LimitStatesSettings value() const;

private:
    cc::core::LimitStatesSettings m_base;
    QComboBox* m_ground;
    QComboBox* m_coldFormed;  ///< shared by cold-formed steel and timber
    QComboBox* m_hotRolled;
    QRadioButton* m_cte;
};

/// "Estados límite a verificar" (398x356): eight buttons with the combination currently in force. Read-only
/// until the editors are captured (ticket TK-017).
class LimitStatesVerifyDialog : public CypeDialog {
    Q_OBJECT
public:
    explicit LimitStatesVerifyDialog(const cc::core::LimitStatesSettings& settings, QWidget* parent = nullptr);
};

}  // namespace cc::app
