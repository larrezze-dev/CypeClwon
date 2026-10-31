#pragma once
#include "CypeDialog.h"
#include "core/project_settings.h"
#include "core/standards.h"

class QComboBox;
class QPushButton;
class QVBoxLayout;

namespace cc::app {

/// "Obra vacía" wizard (848x774, a single page; reference: docs/screenshots/orig_obra_vacia_01.png).
/// Implemented so far: Normas, Perfiles and Hormigón armado. Acciones, Opciones, Ambiente, Terreno and
/// Pilotes arrive with T-P2-05, and the Estados límite dialog shown after "Terminar" with T-P2-06.
class EmptyProjectWizard : public CypeDialog {
    Q_OBJECT
public:
    explicit EmptyProjectWizard(QWidget* parent = nullptr);

    const cc::core::StandardsSelection& standards() const { return m_standards; }
    const cc::core::ProfileSettings& profiles() const { return m_profiles; }
    const cc::core::ReinforcedConcreteSettings& reinforcedConcrete() const { return m_concrete; }

    QPushButton* finishButton() const { return m_finish; }
    QPushButton* previousButton() const { return m_previous; }
    QPushButton* nextButton() const { return m_next; }

private:
    void buildProfiles(QVBoxLayout* layout);
    void buildReinforcedConcrete(QVBoxLayout* layout);
    void refreshButtons();
    QComboBox* concreteCombo(const std::string& current, std::string& target);

    cc::core::StandardsSelection m_standards;
    cc::core::ProfileSettings m_profiles;
    cc::core::ReinforcedConcreteSettings m_concrete;

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
