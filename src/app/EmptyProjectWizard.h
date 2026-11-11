#pragma once
#include "CypeDialog.h"
#include "core/general_data.h"
#include "core/limit_states.h"
#include "core/options_settings.h"
#include "core/project_settings.h"
#include "core/standards.h"

class QComboBox;
class QLineEdit;
class QPushButton;
class QVBoxLayout;

namespace cc::app {

/// "Obra vacía" wizard (848x774, a single page; reference: docs/screenshots/orig_obra_vacia_01.png).
/// All sections are present; the Estados límite dialog shown after "Terminar" arrives with T-P2-06 and the
/// leaf dialogs of Opciones with ticket TK-015.
class EmptyProjectWizard : public CypeDialog {
    Q_OBJECT
public:
    /// Create: the page of Nueva obra (Cancelar / < Anterior / Siguiente > / Terminar). Edit: the same page reused by
    /// Obra > Datos generales (title "Datos generales", a single Aceptar button), filled with the project's values.
    enum class Mode { Create, Edit };

    explicit EmptyProjectWizard(QWidget* parent = nullptr);
    EmptyProjectWizard(const cc::core::GeneralData& initial, Mode mode, QWidget* parent = nullptr);

    /// What the page currently describes.
    cc::core::GeneralData generalData() const;

    /// Obra > Datos generales on an open obra: the loads of `project` (nodal, bar, shell, prescribed displacements) are copied into
    /// "Hipótesis adicionales" so that deleting a hypothesis that has loads asks first (#64905, measured on shell loads). The pointer is
    /// only read while the window is open; the caller deletes the loads of the removed hypotheses on Aceptar.
    void setLoadsSource(const cc::core::Project* project) { m_loadsSource = project; }

    const cc::core::StandardsSelection& standards() const { return m_standards; }
    const cc::core::ProfileSettings& profiles() const { return m_profiles; }
    const cc::core::ReinforcedConcreteSettings& reinforcedConcrete() const { return m_concrete; }
    const cc::core::ActionsSettings& actions() const { return m_actions; }
    const cc::core::EnvironmentSettings& environment() const { return m_environment; }
    const cc::core::GroundSettings& ground() const { return m_ground; }
    const cc::core::LimitStatesSettings& limitStates() const { return m_limitStates; }
    const cc::core::PileSettings& piles() const { return m_piles; }
    const cc::core::MicropileSettings& micropiles() const { return m_micropiles; }
    const cc::core::OptionsSettings& options() const { return m_options; }
    const cc::core::LoadCaseSet& loadCases() const { return m_loadCases; }
    /// Letters of the use categories ticked in "Categorías de uso" after the choice of standards (opens every time; not yet linked to the load
    /// cases: ticket TK-102).
    const std::vector<std::string>& useCategoryReferences() const { return m_useCategoryRefs; }

    QPushButton* finishButton() const { return m_finish; }
    QPushButton* previousButton() const { return m_previous; }
    QPushButton* nextButton() const { return m_next; }

private:
    void buildProfiles(QVBoxLayout* layout);
    void buildReinforcedConcrete(QVBoxLayout* layout);
    void buildActions(QVBoxLayout* layout);
    void buildOptions(QVBoxLayout* layout);
    void buildEnvironment(QVBoxLayout* layout);
    void buildPiles(QVBoxLayout* layout);
    void buildGround(QVBoxLayout* layout);
    void refreshButtons();
    QComboBox* concreteCombo(const std::string& current, std::string& target);

    cc::core::StandardsSelection m_standards;
    cc::core::ProfileSettings m_profiles;
    cc::core::ReinforcedConcreteSettings m_concrete;
    cc::core::ActionsSettings m_actions;
    cc::core::EnvironmentSettings m_environment;
    cc::core::GroundSettings m_ground;
    cc::core::LimitStatesSettings m_limitStates;
    cc::core::PileSettings m_piles;
    cc::core::MicropileSettings m_micropiles;
    cc::core::OptionsSettings m_options;
    cc::core::LoadCaseSet m_loadCases;
    std::vector<std::string> m_useCategoryRefs;
    const cc::core::Project* m_loadsSource = nullptr;
    QPushButton* m_environmentButton = nullptr;
    QLineEdit* m_persistentEdit = nullptr;
    QLineEdit* m_seismicEdit = nullptr;

    QPushButton* m_standardsButton = nullptr;
    QPushButton* m_timberButton = nullptr;
    QPushButton* m_aluminiumButton = nullptr;
    QPushButton* m_rebarButton = nullptr;
    QPushButton* m_aggregateButton = nullptr;
    QPushButton* m_previous = nullptr;
    QPushButton* m_next = nullptr;
    QPushButton* m_finish = nullptr;
};

}  // namespace cc::app
