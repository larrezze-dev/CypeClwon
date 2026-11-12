#pragma once
#include <array>
#include <optional>

#include "CypeDialog.h"
#include "core/fem/result_display.h"

class QCheckBox;
class QComboBox;
class QLineEdit;
class QRadioButton;

namespace cc::app {

/// Floating panels «Propiedades - Desplazamientos / Reacciones / Esfuerzos» of Cálculo (T-P8-06). Layouts measured in the original:
/// docs/research/ui/resultados_paneles.md (rp_*.png) and s_lin.png. Envelopes are not wired yet (their radios are disabled).
class ResultsPanel : public CypeDialog {
    Q_OBJECT
public:
    enum class Kind { Displacements, Reactions, Forces };
    /// Rows of Esfuerzos: the six laws, Deformada (D) and the three deflections (Fxy, Fxz, F; not wired yet).
    static constexpr int kForceRows = 10;
    static constexpr int kDeformedRow = 6;
    explicit ResultsPanel(Kind kind, QWidget* parent = nullptr);

    Kind kind() const { return m_kind; }
    /// Fills the combos from the analysis (null = no results), keeping the current family and item when they still exist.
    void setAnalysis(const cc::fem::ProjectAnalysis* analysis);
    std::optional<cc::fem::ResultFamily> family() const;
    int item() const;
    /// Displacements / Reactions: check boxes Dx..Gz / Rx..Mz. Esfuerzos: the law rows N..Mz.
    cc::fem::ComponentMask components() const;
    /// Esfuerzos only.
    bool deformed() const;
    bool selectedOnly() const;
    bool queryValues() const;
    bool fillLaws() const;
    bool showStructure() const;
    bool drawExtremes() const;
    /// Scale field of a row of Esfuerzos (defaults 0.100 for the laws, 100.000 for the deformed shape and the deflections [V]).
    double scale(int row) const;

    QComboBox* familyCombo() const { return m_family; }
    QComboBox* itemCombo() const { return m_item; }
    QCheckBox* componentBox(int index) const { return m_rows[static_cast<std::size_t>(index)]; }
    QCheckBox* rowBox(int row) const { return m_rows[static_cast<std::size_t>(row)]; }
    QLineEdit* scaleEdit(int row) const { return m_scales[static_cast<std::size_t>(row)]; }
    QRadioButton* selectedOnlyRadio() const { return m_selectedOnly; }
    QCheckBox* queryBox() const { return m_query; }
    static QString title(Kind kind);
    /// Colour of a row of Esfuerzos (0xRRGGBB) [V rp_17_all_laws.png].
    static unsigned rowColour(int row);

signals:
    void changed();
    void closed();

protected:
    void done(int result) override;

private:
    void fillItems();
    void updateVisibility();
    Kind m_kind;
    const cc::fem::ProjectAnalysis* m_analysis = nullptr;
    bool m_loading = false;
    QWidget* m_comboGroup = nullptr;
    QComboBox* m_family = nullptr;
    QComboBox* m_item = nullptr;
    std::array<QCheckBox*, kForceRows> m_rows{};
    std::array<QLineEdit*, kForceRows> m_scales{};
    std::array<QWidget*, kForceRows> m_rowWidgets{};
    QRadioButton* m_laws = nullptr;
    QRadioButton* m_allBars = nullptr;
    QRadioButton* m_selectedOnly = nullptr;
    QCheckBox* m_query = nullptr;
    QCheckBox* m_axesOnWindow = nullptr;
    QCheckBox* m_drawExtremes = nullptr;
    QCheckBox* m_viewExtremes = nullptr;
    QCheckBox* m_fill = nullptr;
    QCheckBox* m_structure = nullptr;
};

}  // namespace cc::app
