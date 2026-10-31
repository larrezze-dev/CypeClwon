#pragma once
#include <map>

#include "CypeDialog.h"
#include "core/standards.h"

class QComboBox;

namespace cc::app {

/// "Selección de normas" dialog (418x247): one combo per material family
/// (reference: docs/screenshots/orig_obra_vacia_02_normas.png).
class StandardsDialog : public CypeDialog {
    Q_OBJECT
public:
    explicit StandardsDialog(const cc::core::StandardsSelection& initial, QWidget* parent = nullptr);

    cc::core::StandardsSelection selection() const;

private:
    std::map<cc::core::StandardFamily, QComboBox*> m_combos;
};

}  // namespace cc::app
