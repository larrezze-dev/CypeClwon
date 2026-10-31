#pragma once
#include <QStringList>
#include <functional>
#include <vector>

#include "CypeDialog.h"
#include "core/options_settings.h"
#include "core/project_settings.h"

class QButtonGroup;
class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QTableWidget;
class QTabWidget;

namespace cc::app {

/// Small dialogs opened from the "Obra vacía" wizard. Every dialog takes the current values, lets the user
/// edit them and exposes the result through value(); callers apply it only when exec() returns Accepted.
/// References: docs/screenshots/orig_obra_vacia_04..10 and docs/research/ui/dialogs_nueva_obra_wizard.md.

/// "Madera" (492x285): material, strength class and service class.
class TimberDialog : public CypeDialog {
    Q_OBJECT
public:
    explicit TimberDialog(const cc::core::TimberSettings& initial, QWidget* parent = nullptr);
    cc::core::TimberSettings value() const;

private:
    void updateServiceText();
    QComboBox* m_material;
    QComboBox* m_strength;
    QButtonGroup* m_service;
    QLabel* m_serviceText;
};

/// "Aluminio" (308x321): alloy and temper (delivery state) as radio groups.
class AluminiumDialog : public CypeDialog {
    Q_OBJECT
public:
    explicit AluminiumDialog(const cc::core::AluminiumSettings& initial, QWidget* parent = nullptr);
    cc::core::AluminiumSettings value() const;

private:
    QButtonGroup* m_alloys;
    QButtonGroup* m_tempers;
};

/// "Biblioteca de aceros de usuario" (478x245), opened by the fy icon. The user libraries are empty by default;
/// editing them is not implemented yet (ticket TK-012).
class SteelLibraryDialog : public CypeDialog {
    Q_OBJECT
public:
    explicit SteelLibraryDialog(QWidget* parent = nullptr);
};

/// "Acero de barras" (277x480): rebar grade for every element.
class RebarSteelDialog : public CypeDialog {
    Q_OBJECT
public:
    explicit RebarSteelDialog(const cc::core::RebarSteelSettings& initial, QWidget* parent = nullptr);
    cc::core::RebarSteelSettings value() const;

private:
    std::vector<QComboBox*> m_combos;  ///< same order as the fields of RebarSteelSettings
};

/// "Características del árido" (241x397).
class AggregateDialog : public CypeDialog {
    Q_OBJECT
public:
    explicit AggregateDialog(const cc::core::AggregateSettings& initial, QWidget* parent = nullptr);
    cc::core::AggregateSettings value() const;

private:
    QComboBox* m_kind[3];
    QComboBox* m_size[3];
    QDoubleSpinBox* m_foundationSize;
};

/// Common base for the dialogs that carry "Grabar como opciones por defecto" and "Valores de instalación".
class DefaultsDialog : public CypeDialog {
    Q_OBJECT
public:
    using CypeDialog::CypeDialog;
    /// True when the user ticked "Grabar como opciones por defecto". Persisting the choice is ticket TK-012.
    bool saveAsDefault() const;

protected:
    /// Adds the footer: the checkbox row plus Aceptar / Valores de instalación / Cancelar.
    void setupDefaultsFooter();
    virtual void resetToInstallationValues() = 0;

private:
    QCheckBox* m_saveDefault = nullptr;
};

/// "Recubrimientos" menu (180x191): three buttons, one per element family.
class CoverMenuDialog : public CypeDialog {
    Q_OBJECT
public:
    explicit CoverMenuDialog(const cc::core::CoverSettings& initial, QWidget* parent = nullptr);
    /// Covers as edited so far (sub-dialog changes are applied immediately on their own Aceptar).
    cc::core::CoverSettings value() const { return m_covers; }

private:
    cc::core::CoverSettings m_covers;
};

/// Cover dialogs for columns (404x384), beams (398x468) and foundation (398x350).
class CoverDialog : public DefaultsDialog {
    Q_OBJECT
public:
    enum class Kind { Columns, Beams, Foundation };
    CoverDialog(Kind kind, const cc::core::CoverSettings& initial, QWidget* parent = nullptr);
    cc::core::CoverSettings value() const;

protected:
    void resetToInstallationValues() override;

private:
    void load(const cc::core::CoverSettings& settings);
    Kind m_kind;
    cc::core::CoverSettings m_base;       ///< fields of the other families are preserved
    std::vector<QDoubleSpinBox*> m_spins; ///< order depends on m_kind (see .cpp)
};

/// "Mermas de acero" (398x266): percentage per element.
class WasteDialog : public DefaultsDialog {
    Q_OBJECT
public:
    explicit WasteDialog(const cc::core::WasteSettings& initial, QWidget* parent = nullptr);
    cc::core::WasteSettings value() const;

protected:
    void resetToInstallationValues() override;

private:
    std::vector<QDoubleSpinBox*> m_spins;
};

/// "Resistencia al fuego" (598x335): tabs Acero / Madera. Only the Acero tab is implemented (TK-011).
class FireDialog : public CypeDialog {
    Q_OBJECT
public:
    explicit FireDialog(cc::core::SteelFireMode initial, QWidget* parent = nullptr);
    cc::core::SteelFireMode value() const;

private:
    QButtonGroup* m_modes;
};

/// "Hipótesis adicionales" (280x443): shows the automatic and additional load-case counts per action.
/// Editing the cases needs the load module (phase 7, ticket TK-013), so the edit buttons are disabled.
class AdditionalLoadsDialog : public CypeDialog {
    Q_OBJECT
public:
    explicit AdditionalLoadsDialog(QWidget* parent = nullptr);
};

/// "Cimentación" (253x136): wind and seismic combinations for the foundation.
class FoundationActionsDialog : public CypeDialog {
    Q_OBJECT
public:
    FoundationActionsDialog(bool wind, bool seism, QWidget* parent = nullptr);
    bool wind() const;
    bool seism() const;

private:
    QCheckBox* m_wind;
    QCheckBox* m_seism;
};

/// "Proceso constructivo" (398x152): formwork removal time. Only the "días" unit was seen (TK-011).
class ConstructionProcessDialog : public DefaultsDialog {
    Q_OBJECT
public:
    explicit ConstructionProcessDialog(double days, QWidget* parent = nullptr);
    double days() const;

protected:
    void resetToInstallationValues() override;

private:
    QDoubleSpinBox* m_days;
};

/// "Vigas" environment dialog (442x385): exposure class, designation and cracking.
class BeamEnvironmentDialog : public CypeDialog {
    Q_OBJECT
public:
    explicit BeamEnvironmentDialog(const cc::core::EnvironmentSettings& initial, QWidget* parent = nullptr);
    cc::core::EnvironmentSettings value() const;

private:
    QButtonGroup* m_classes;
    cc::core::EnvironmentSettings m_base;
};

/// "Importar valores usuales de proyecto" (530x296): pick a ground type to fill both admissible stresses.
class UsualGroundDialog : public CypeDialog {
    Q_OBJECT
public:
    explicit UsualGroundDialog(QWidget* parent = nullptr);
    /// Selected row of usualGroundValues(), or -1.
    int selectedIndex() const;

private:
    QTableWidget* m_table;
};

/// Shared "Hundimiento" tab of the pile and micropile "Configuración avanzada" dialogs.
class PileAdvancedBase : public DefaultsDialog {
    Q_OBJECT
public:
    using DefaultsDialog::DefaultsDialog;

protected:
    /// Builds the tab; `micro` selects the micropile variant (shaft-only geotechnical group, "micropilote" texts).
    QWidget* buildSinkingTab(bool micro, QTabWidget* tabs);
    void loadSinking(const cc::core::PileSinkingSettings& settings);
    cc::core::PileSinkingSettings sinkingValue(cc::core::PileSinkingSettings base) const;

private:
    QComboBox* m_resistance = nullptr;
    QLineEdit* m_reference = nullptr;
    std::vector<std::pair<QDoubleSpinBox*, double cc::core::PileSinkingSettings::*>> m_sinkingSpins;
    QComboBox* m_efficiency = nullptr;
    QCheckBox* m_ignoreFar = nullptr;
};

/// Pile "Configuración avanzada" (556x786): tabs Hundimiento, Comprobación estructural and Tope estructural.
class PileAdvancedDialog : public PileAdvancedBase {
    Q_OBJECT
public:
    explicit PileAdvancedDialog(const cc::core::PileAdvancedSettings& initial, QWidget* parent = nullptr);
    cc::core::PileAdvancedSettings value() const;

protected:
    void resetToInstallationValues() override;

private:
    void load(const cc::core::PileAdvancedSettings& settings);
    void updateStrengthUnits();
    QCheckBox* m_reduceStrength;
    QComboBox *m_precastBasis, *m_castBasis;
    QDoubleSpinBox *m_precastValue, *m_castValue, *m_cuFactor, *m_gammaFactor;
    QLabel *m_precastUnit, *m_castUnit;
    QCheckBox* m_structuralCap;
};

/// Micropile "Configuración avanzada" (587x728): tab Hundimiento; the Comprobación estructural tab is a
/// placeholder until it is captured from the original (ticket TK-016).
class MicropileAdvancedDialog : public PileAdvancedBase {
    Q_OBJECT
public:
    explicit MicropileAdvancedDialog(const cc::core::MicropileAdvancedSettings& initial, QWidget* parent = nullptr);
    cc::core::MicropileAdvancedSettings value() const;

protected:
    void resetToInstallationValues() override;
};

/// One page of an OptionsMenuDialog: a title (shown in the side list when there is more than one page), the
/// captions of its buttons and which of them are enabled. Buttons that were not captured from the original yet
/// (ticket TK-015) stay disabled.
struct OptionsPage {
    QString title;
    QStringList buttons;
    QStringList enabledButtons;
};

/// Menu opened by the Opciones buttons of the wizard (Pilares, Cimentación, Vigas, Uniones). The enabled buttons
/// call `onButton` with their caption.
class OptionsMenuDialog : public CypeDialog {
    Q_OBJECT
public:
    using ButtonHandler = std::function<void(const QString&)>;
    OptionsMenuDialog(const QString& title, const QSize& size, const std::vector<OptionsPage>& pages,
                      ButtonHandler onButton = {}, bool withCancel = false, QWidget* parent = nullptr);
    /// Menu for one of the four option families with the sizes and captions seen in the original. The settings
    /// are edited in place by the leaf dialogs and must outlive the menu.
    enum class Family { Columns, Foundation, Beams, Joints };
    static OptionsMenuDialog* create(Family family, cc::core::OptionsSettings& settings, QWidget* parent = nullptr);
};

}  // namespace cc::app
