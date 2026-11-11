#pragma once
#include <map>

#include "CypeDialog.h"
#include "core/standards.h"

class QCheckBox;
class QComboBox;

namespace cc::app {

/// "Selección de normas" dialog (418x247): one combo per material family
/// (reference: docs/screenshots/orig_obra_vacia_02_normas.png).
class StandardsDialog : public CypeDialog {
    Q_OBJECT
public:
    explicit StandardsDialog(const cc::core::StandardsSelection& initial, QWidget* parent = nullptr);

    cc::core::StandardsSelection selection() const;
    /// The buttons "C.E." and "CTE": assign the whole set of codes (research 12.4).
    void applySet(const cc::core::StandardsSelection& set);
    QComboBox* foundationCombo() const { return m_foundation; }
    /// Aceptar: when a material standard changed, the original warns that the material selection of those bars is reset.
    void accept() override;
    /// True when the selection changes the standard of some material (the condition of the warning).
    bool materialsChanged() const;
    /// Text of the warning (tests).
    static QString materialsWarning();

private:
    void updateFoundationRow();
    bool foundationApplies() const;
    cc::core::StandardsSelection m_initial;
    std::map<cc::core::StandardFamily, QComboBox*> m_combos;
    QComboBox* m_foundation = nullptr;
    QWidget* m_foundationLabel = nullptr;
};

/// "Categorías de uso" opened after the choice of standards (research 14.5, capture cmb_43; ~634x245): one check box per category of the
/// norm, Aceptar / Cancelar. Eurocodes: A to H, all unchecked when the work is created; CTE: A to E, G1, G2 (G2 checked when the work had the
/// category "H. Cubiertas"). Nothing may be checked ("No hay categorías de uso definidas"). With mixed norms the original shows a list with
/// one combo per norm group instead: it needs the load-case module (ticket TK-102) and is not built here.
class UseCategorySelectionDialog : public CypeDialog {
    Q_OBJECT
public:
    enum class Scheme { Eurocode, Cte };
    /// Scheme of the standards: Eurocode when every material norm (concrete, steel, timber) is a Eurocode, CTE when they all follow the CTE /
    /// Código Estructural; nullopt-like false for mixed selections (`ok` = false).
    static Scheme schemeFor(const cc::core::StandardsSelection& standards, bool* ok = nullptr);
    /// Texts of the check boxes of the scheme, in order.
    static QStringList categoryTexts(Scheme scheme);
    /// `checked`: one flag per category of the scheme (shorter = the rest unchecked).
    UseCategorySelectionDialog(Scheme scheme, const std::vector<bool>& checked, QWidget* parent = nullptr);
    std::vector<bool> checked() const;
    /// Letters ("A", "G2") of the checked categories.
    std::vector<std::string> checkedReferences() const;
    const std::vector<QCheckBox*>& boxes() const { return m_boxes; }

private:
    Scheme m_scheme;
    std::vector<QCheckBox*> m_boxes;
};

}  // namespace cc::app
