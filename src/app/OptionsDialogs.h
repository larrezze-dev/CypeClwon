#pragma once
#include <vector>

#include "WizardSubDialogs.h"
#include "core/options_settings.h"

class QButtonGroup;
class QCheckBox;
class QComboBox;
class QLineEdit;

namespace cc::app {

/// Leaf dialogs behind the Opciones menus of the "Obra vacía" wizard (Vigas and Uniones). References:
/// docs/research/ui/dialogs_opciones_y_pilotes.md and the orig_opc_* captures. Every dialog takes the current
/// values and exposes the edited ones through value(); callers apply it only when exec() returns Accepted.

/// "Valoración de errores" (437x514).
class BeamErrorsDialog : public DefaultsDialog {
    Q_OBJECT
public:
    explicit BeamErrorsDialog(const cc::core::BeamErrorSettings& initial, QWidget* parent = nullptr);
    cc::core::BeamErrorSettings value() const;

protected:
    void resetToInstallationValues() override;

private:
    void load(const cc::core::BeamErrorSettings& settings);
    std::vector<QCheckBox*> m_checks;  ///< same order as the member list in the .cpp
    QComboBox* m_stirrupDiameter;
};

/// "Límites de flecha" (309x371): read-only; the pencil buttons of the original open editors that were not
/// captured yet (ticket TK-015), so they are disabled.
class DeflectionLimitsDialog : public CypeDialog {
    Q_OBJECT
public:
    explicit DeflectionLimitsDialog(QWidget* parent = nullptr);
};

/// "Numeración de vigas continuas" (531x372).
class ContinuousBeamNumberingDialog : public DefaultsDialog {
    Q_OBJECT
public:
    explicit ContinuousBeamNumberingDialog(const cc::core::ContinuousBeamNumbering& initial, QWidget* parent = nullptr);
    cc::core::ContinuousBeamNumbering value() const;

protected:
    void resetToInstallationValues() override;

private:
    void load(const cc::core::ContinuousBeamNumbering& settings);
    QLineEdit* m_reference;
    QButtonGroup *m_first, *m_xFrames, *m_yFrames;
};

/// "Numeración de vigas" (397x233).
class BeamNumberingDialog : public DefaultsDialog {
    Q_OBJECT
public:
    explicit BeamNumberingDialog(cc::core::BeamNumbering initial, QWidget* parent = nullptr);
    cc::core::BeamNumbering value() const;

protected:
    void resetToInstallationValues() override;

private:
    QButtonGroup* m_modes;
};

/// Uniones > "Materiales" (291x181).
class JointMaterialsDialog : public CypeDialog {
    Q_OBJECT
public:
    explicit JointMaterialsDialog(const cc::core::JointMaterials& initial, QWidget* parent = nullptr);
    cc::core::JointMaterials value() const;

private:
    QComboBox *m_steel, *m_bolts, *m_concrete;
};

/// Uniones > "Opciones" (671x460): four tabs (Tornillería no pretensada, Tornillería pretensada, Rigidizadores and
/// Placa de anclaje). The materials of JointOptionsSettings are passed through untouched.
class JointOptionsDialog : public DefaultsDialog {
    Q_OBJECT
public:
    explicit JointOptionsDialog(const cc::core::JointOptionsSettings& initial, QWidget* parent = nullptr);
    cc::core::JointOptionsSettings value() const;

protected:
    void resetToInstallationValues() override;

private:
    QWidget* buildBoltsTab(QWidget* parent);
    QWidget* buildPreloadedTab(QWidget* parent);
    QWidget* buildStiffenersTab(QWidget* parent);
    QWidget* buildBasePlateTab(QWidget* parent);
    void load(const cc::core::JointOptionsSettings& settings);
    cc::core::JointOptionsSettings m_passThrough;
    // Tornillería no pretensada
    QCheckBox *m_iso4014, *m_iso4017, *m_unusual;
    QButtonGroup* m_classes;
    std::vector<QCheckBox*> m_diameters;
    // Tornillería pretensada
    QButtonGroup *m_pSeries, *m_pClasses, *m_pFriction;
    std::vector<QCheckBox*> m_pDiameters;
    QCheckBox* m_pUnusual;
    QLabel* m_frictionText;
    // Rigidizadores
    QCheckBox *m_columnBeam, *m_ridge;
    QButtonGroup* m_cutouts;
    // Placa de anclaje
    std::vector<QLineEdit*> m_plateFields;
    QComboBox* m_plateAnchorage;
};

}  // namespace cc::app
