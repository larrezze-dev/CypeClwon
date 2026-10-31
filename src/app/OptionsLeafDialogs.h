#pragma once
#include <array>
#include <vector>

#include "WizardSubDialogs.h"
#include "core/options_settings.h"

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QRadioButton;
class QSpinBox;
class QTableWidget;
class QToolButton;

namespace cc::app {

/// Leaf dialogs of Cimentación > Opciones / Tablas de armado and of Vigas > Opciones (page "Tablas de armado en
/// vigas" and the "Límites de flecha" editors). References: docs/research/ui/dialogs_opciones_y_pilotes.md,
/// section "Captura B". Every dialog takes the current values and exposes the edited ones through value().

/// "Hormigón de limpieza" (277x178).
class CleaningConcreteDialog : public CypeDialog {
    Q_OBJECT
public:
    explicit CleaningConcreteDialog(const cc::core::CleaningConcreteSettings& initial, QWidget* parent = nullptr);
    cc::core::CleaningConcreteSettings value() const;

private:
    QCheckBox *m_footings, *m_pileCaps, *m_beams;
};

/// "Comprobación de fisuración" (589x435).
class CrackingDialog : public CypeDialog {
    Q_OBJECT
public:
    explicit CrackingDialog(const cc::core::CrackingSettings& initial, QWidget* parent = nullptr);
    cc::core::CrackingSettings value() const;

private:
    QCheckBox *m_footingCheck, *m_beamCheck;
    QDoubleSpinBox *m_footingLimit, *m_beamLimit;
};

/// Encepados > Opciones (428x631). Only the tab "Generales" was captured; the other tabs are placeholders.
class PileCapOptionsDialog : public DefaultsDialog {
    Q_OBJECT
public:
    explicit PileCapOptionsDialog(const cc::core::PileCapGeneralSettings& initial, QWidget* parent = nullptr);
    cc::core::PileCapGeneralSettings value() const;

protected:
    void resetToInstallationValues() override;

private:
    void load(const cc::core::PileCapGeneralSettings& settings);
    QDoubleSpinBox *m_overPile, *m_overSupport, *m_minDepth, *m_maxDepth, *m_barSpacing, *m_maxDiameter, *m_fraction,
        *m_factor;
    QComboBox* m_criterion;
    QCheckBox* m_withLoads;
};

/// Vigas centradoras > Opciones (399x245).
class CenteringBeamDialog : public DefaultsDialog {
    Q_OBJECT
public:
    explicit CenteringBeamDialog(const cc::core::CenteringBeamSettings& initial, QWidget* parent = nullptr);
    cc::core::CenteringBeamSettings value() const;

protected:
    void resetToInstallationValues() override;

private:
    void load(const cc::core::CenteringBeamSettings& settings);
    QDoubleSpinBox *m_angle, *m_minTension;
    QCheckBox *m_stirrups, *m_minGeometric;
    QComboBox* m_type;
};

/// Cimentación > Tablas de armado > Zapatas aisladas (481x267) / Vigas de atado (491x267).
class FoundationTableDialog : public CypeDialog {
    Q_OBJECT
public:
    enum class Kind { Footings, TieBeams };
    FoundationTableDialog(Kind kind, cc::core::ReinforcementTableSource initial, QWidget* parent = nullptr);
    cc::core::ReinforcementTableSource value() const;

private:
    QRadioButton *m_predefined, *m_library, *m_special;
};

/// Vigas > Tablas de armado > "Armadura transversal" (343x229).
class TransverseTableDialog : public CypeDialog {
    Q_OBJECT
public:
    explicit TransverseTableDialog(const cc::core::TransverseReinforcementTable& initial, QWidget* parent = nullptr);
    cc::core::TransverseReinforcementTable value() const { return m_table; }

    /// Text shown for a list, as the original does: all values up to five, otherwise the first four, "..." and the last.
    static QString summarise(const std::vector<double>& values, int decimals);

private:
    void refresh();
    cc::core::TransverseReinforcementTable m_table;
    QLabel *m_diameters, *m_spacings;
};

/// "Armadura portaestribos" (359x347) and "Armadura de piel" (229x315): table of diameters with an admissible flag.
class DiameterAdmissibilityDialog : public CypeDialog {
    Q_OBJECT
public:
    enum class Kind { StirrupHolder, Skin };
    DiameterAdmissibilityDialog(Kind kind, const cc::core::DiameterAdmissibility& initial, QWidget* parent = nullptr);
    cc::core::DiameterAdmissibility value() const;
    /// Toggles the admissible flag of a diameter row (what a click on the cell does).
    void toggleRow(int row);

private:
    QTableWidget* m_table;
    QCheckBox* m_assembly = nullptr;
};

/// "Armadura superior de refuerzo" / "Armadura inferior de refuerzo" (356x582).
class ReinforcementBarDialog : public CypeDialog {
    Q_OBJECT
public:
    ReinforcementBarDialog(const QString& title, const cc::core::ReinforcementBarTable& initial, QWidget* parent = nullptr);
    cc::core::ReinforcementBarTable value() const;
    /// Toggles a cell of the first (assembly x reinforcement 1) or the second matrix.
    void toggleCell(int matrix, int row, int column);

private:
    void refresh();
    cc::core::ReinforcementBarTable m_table;
    QLabel* m_diameters;
    QDoubleSpinBox* m_spacing;
    QTableWidget *m_first, *m_second;
};

/// "Límites de flecha (<material>)" editor opened from a pencil of DeflectionLimitsDialog (405x481, 385x356,
/// 385x391 or 405x481 according to the material).
class DeflectionEditorDialog : public CypeDialog {
    Q_OBJECT
public:
    enum class Section { InstantSimple, InstantCombination, Infinite, Active };
    DeflectionEditorDialog(const QString& material, const cc::core::DeflectionLimitSet& initial, QWidget* parent = nullptr);
    cc::core::DeflectionLimitSet value() const;
    /// Sets one limit (0 = "----"); the pencil of each row asks for it.
    void setLimit(Section section, int index, int denominator);
    int limit(Section section, int index) const;

private:
    void addRows(QVBoxLayout* layout, const QString& title, Section section, const QStringList& names);
    int* slot(Section section, int index);
    const int* slot(Section section, int index) const;
    cc::core::DeflectionLimitSet m_set;
    std::vector<std::pair<std::pair<Section, int>, QLabel*>> m_labels;
    QSpinBox* m_months = nullptr;
};

}  // namespace cc::app
