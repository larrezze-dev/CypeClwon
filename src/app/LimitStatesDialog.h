#pragma once
#include <vector>

#include "CypeDialog.h"
#include "core/limit_states.h"
#include "core/standards.h"

class QComboBox;
class QLabel;
class QRadioButton;

namespace cc::app {

/// "Estados límite": combinations used for each limit state, opened from the wizard / Datos generales and after "Terminar".
/// The blocks (headings, radios CTE | EC | "Hormigón: EC, Acero: CTE", snow combo, pencil) follow the standards chosen, see
/// core::buildLimitStateLayout and docs/research/ui/combinaciones_normas.md section 13.
class LimitStatesDialog : public CypeDialog {
    Q_OBJECT
public:
    LimitStatesDialog(const cc::core::StandardsSelection& standards, const cc::core::LimitStatesSettings& initial,
                      QWidget* parent = nullptr);
    cc::core::LimitStatesSettings value() const;
    /// Combos in display order (one per block with snow), the radios of the structural-code block (empty when none).
    const std::vector<QComboBox*>& snowCombos() const { return m_combos; }
    const std::vector<QRadioButton*>& codeRadios() const { return m_radios; }

private:
    void buildBody();
    struct BlockUi {
        cc::core::NormClass cls = cc::core::NormClass::Eurocode;
        QComboBox* combo = nullptr;
        QLabel* label = nullptr;
        std::vector<QRadioButton*> radios;
    };
    cc::core::StandardsSelection m_standards;
    cc::core::LimitStatesSettings m_base;
    cc::core::LimitStateLayout m_layout;
    std::vector<BlockUi> m_blocks;
    std::vector<QComboBox*> m_combos;
    std::vector<QRadioButton*> m_radios;
};

/// "Estados límite a verificar": eight buttons with the combination currently in force. Each opens the dialog of its limit state
/// (radio "Datos generales" / "Situaciones de proyecto introducidas por el usuario").
class LimitStatesVerifyDialog : public CypeDialog {
    Q_OBJECT
public:
    explicit LimitStatesVerifyDialog(const cc::core::LimitStatesSettings& settings,
                                     const cc::core::StandardsSelection& standards = {}, QWidget* parent = nullptr);
    /// Settings with the user situations and assignments edited here.
    const cc::core::LimitStatesSettings& settings() const { return m_settings; }
    /// Opens the dialog of one limit state (what the buttons do); returns true when it was accepted.
    bool editKind(cc::core::LimitStateKind kind);
    std::vector<QPushButton*> kindButtons() const { return m_buttons; }

private:
    void refreshValues();
    cc::core::LimitStatesSettings m_settings;
    cc::core::StandardsSelection m_standards;
    std::vector<QPushButton*> m_buttons;
    std::vector<QLabel*> m_values;
};

/// Dialog of one limit state (title = its name, e.g. "Aluminio"): radio "Datos generales" (text with the snow or "Acciones
/// características" and the pencil that opens the "Situaciones" viewer) and radio "Situaciones de proyecto introducidas por el usuario"
/// (combo of sets + bar: new, delete, copy, edit, library menu + arrow "Crear a partir de las situaciones de la norma"). Hormigón and
/// Cimentación have two rows, "E.L.U. de rotura" and "E.L.S. de fisuración", each with its own combo and bar over separate libraries
/// [V, research 14.3]. Every function takes `service` = the E.L.S. row (the E.L.U. one by default).
class LimitStateKindDialog : public CypeDialog {
    Q_OBJECT
public:
    LimitStateKindDialog(cc::core::LimitStateKind kind, const cc::core::StandardsSelection& standards,
                         const cc::core::LimitStatesSettings& settings, QWidget* parent = nullptr);
    /// Settings with the sets edited here and the assignment of this limit state.
    cc::core::LimitStatesSettings settings() const { return m_settings; }
    QRadioButton* generalRadio() const { return m_general; }
    QRadioButton* userRadio() const { return m_user; }
    QComboBox* setCombo(bool service = false) const { return service ? m_serviceSets : m_sets; }  ///< null for a missing E.L.S. row
    bool hasServiceRow() const { return m_serviceSets != nullptr; }
    /// The work has seismic cases: a seismic situation is then required in the sets created or edited here (#153641).
    void setProjectHasSeismic(bool on) { m_projectHasSeismic = on; }
    void newSet(bool service = false);
    void copySet(bool service = false);
    void deleteSet(bool service = false);
    void editSet(bool service = false);
    void fromNorm();
    /// Pencil of a user row: listing of the selected set (Aviso when there is none).
    void viewSet(bool service = false);
    /// Entries of the book menu of the bar (Editar / Importar de / Exportar a biblioteca).
    static QStringList libraryMenuEntries();
    void accept() override;

private:
    QLayout* buildRow(bool service);
    void refreshCombo(bool service, const QString& select);
    void updateEnabled();
    std::vector<std::string> existingCategories() const;
    cc::core::LimitStateKind m_kind;
    cc::core::StandardsSelection m_standards;
    cc::core::LimitStatesSettings m_settings;
    QRadioButton* m_general = nullptr;
    QRadioButton* m_user = nullptr;
    QComboBox* m_sets = nullptr;
    QComboBox* m_serviceSets = nullptr;
    std::vector<QWidget*> m_userControls;
    QWidget* m_generalInfo = nullptr;
    QWidget* m_generalPencil = nullptr;
    bool m_hasGeneralReport = false;
    bool m_projectHasSeismic = false;
};

}  // namespace cc::app
