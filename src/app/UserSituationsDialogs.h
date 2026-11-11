#pragma once
#include <vector>

#include "CypeDialog.h"
#include "core/user_situations.h"

class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QRadioButton;
class QTableWidget;
class QToolButton;
class QWidget;

namespace cc::app {

class RangedEdit;

/// Type combo + coefficient grid of a project situation (the body of "Situación de proyecto definida por el usuario", and the right pane of the
/// master-detail "Crear - [Situaciones]"). Measured in the original (research 12.4, 14.1, 14.4): cells 0.000 .. 10.000 (tooltip "Debe estar
/// entre 0.000 y 10.000 ."), "-" for the disabled cells, tab order type > rows top to bottom and Favorable, Desfavorable, Principal,
/// Acompañamiento left to right, select-all on entering a cell. A seismic situation adds the "Sismo" row (-1.000, 1.000, principal 1.000
/// fixed, orthogonal fraction 0..1 in "Acompañamiento" with the note "(*)"). `service` (E.L.S. sets): no type combo, no combination columns.
class SituationEditor : public QWidget {
    Q_OBJECT
public:
    SituationEditor(std::size_t categories, cc::core::CoefficientMode mode, bool service, QWidget* parent = nullptr);
    void setSituation(const cc::core::UserSituation& situation);
    /// The situation being edited (name untouched: it is edited elsewhere).
    cc::core::UserSituation situation() const;
    /// Takes the text of every cell; false when one was refused (the cell already warned and recovered its value).
    bool commitCells();
    std::vector<QWidget*> tabChain() const;
    RangedEdit* cell(int row, int column) const;  ///< row in grid order (the "Sismo" row is the last), column 0..3
    QComboBox* typeCombo() const { return m_type; }
    bool seismicRowVisible() const;
    std::size_t categories() const { return m_categories; }

private:
    void updateSeismicRow();
    cc::core::UserSituation m_situation;
    std::size_t m_categories;
    bool m_both;
    bool m_service;
    QComboBox* m_type = nullptr;
    std::vector<std::vector<RangedEdit*>> m_cells;  ///< [row][column], null where the cell is disabled ("-")
    std::vector<QWidget*> m_seismicWidgets;         ///< widgets of the "Sismo" row and its note
};

/// "Situación de proyecto definida por el usuario" (488x451 for an E.L.U. set, with or without combination factors; 475x357 without type
/// combo for an E.L.S. set). Aceptar validates: first favourable > unfavourable, then principal < accompanying (both "Aviso").
class SituationEditDialog : public CypeDialog {
    Q_OBJECT
public:
    SituationEditDialog(const cc::core::UserSituation& initial, std::size_t categories, cc::core::CoefficientMode mode,
                        const std::vector<std::string>& otherNames, const std::vector<std::string>& categoryNames = {},
                        QWidget* parent = nullptr);
    cc::core::UserSituation situation() const;
    /// Runs the checks of Aceptar; shows the message and returns the error (None when the window may close).
    cc::core::SituationError check();
    /// Cells of the grid in tab order (enabled ones only); used by the tests.
    std::vector<QWidget*> tabChain() const;
    RangedEdit* cell(int row, int column) const { return m_editor->cell(row, column); }
    QLineEdit* nameEdit() const { return m_name; }
    QComboBox* typeCombo() const { return m_editor->typeCombo(); }
    SituationEditor* editor() const { return m_editor; }
    void accept() override;

private:
    cc::core::UserSituation m_initial;
    cc::core::CoefficientMode m_mode;
    std::vector<std::string> m_otherNames;
    QLineEdit* m_name = nullptr;
    SituationEditor* m_editor = nullptr;
};

/// "Coeficientes de combinación": radios "Introducir coeficientes de mayoración" (default) / "... y combinación", each with its formula.
class CoefficientModeDialog : public CypeDialog {
    Q_OBJECT
public:
    explicit CoefficientModeDialog(cc::core::CoefficientMode initial = cc::core::CoefficientMode::PartialFactors, QWidget* parent = nullptr);
    cc::core::CoefficientMode mode() const;

private:
    QRadioButton* m_partial = nullptr;
    QRadioButton* m_both = nullptr;
};

/// "Categorías de uso" of the user situations: bar (+, copy, X) and the one-column table "Categoría de uso" with editable cells.
/// With a set that already has categories the radios "Definir nuevas categorías de uso" / "Categorías de uso ya definidas" (default) come
/// first [V, research 14.4]; accepting with the second one adopts the existing categories. An empty table + Aceptar: Aviso #94354.
class SituationCategoriesDialog : public CypeDialog {
    Q_OBJECT
public:
    explicit SituationCategoriesDialog(const std::vector<std::string>& initial, QWidget* parent = nullptr);
    std::vector<std::string> categories() const;
    /// Row of the initial list each current row comes from (-1 = added), see UserSituationSet::remapCategories.
    std::vector<int> origin() const;
    QTableWidget* table() const { return m_table; }
    QRadioButton* defineNewRadio() const { return m_new; }
    QRadioButton* alreadyDefinedRadio() const { return m_existing; }
    void addRow();
    void copyRow();
    void deleteRow();
    void accept() override;

private:
    void appendRow(const QString& text, int origin);
    void updateEnabled();
    std::vector<std::string> m_initial;
    QTableWidget* m_table = nullptr;
    QRadioButton* m_new = nullptr;
    QRadioButton* m_existing = nullptr;
    QWidget* m_bar = nullptr;
};

/// "Crear - [Situaciones]" / "Editar - [Situaciones]": reference, bar (+, copy, X) and the list of situations (406x355 while empty). The first
/// "+" asks for the coefficient mode and the use categories, later ones go straight to the situation dialog. With a situation the window is
/// master-detail [V, research 14.4]: the list on the left (double click renames) and the editor of the selected one on the right (566x475
/// for E.L.U., 406x380 for E.L.S.). The repeated reference is NOT detected here (only when accepting the limit state, #94676).
class SituationSetDialog : public CypeDialog {
    Q_OBJECT
public:
    SituationSetDialog(const cc::core::UserSituationSet& initial, const std::vector<std::string>& otherReferences, bool creating,
                       QWidget* parent = nullptr);
    cc::core::UserSituationSet set() const;
    cc::core::SituationError check();
    void accept() override;
    /// The work has seismic cases: a seismic situation is then required (#153641).
    void setProjectHasSeismic(bool on) { m_projectHasSeismic = on; }
    /// Categories of the other sets of the work: the first "+" offers them as "Categorías de uso ya definidas" (the default choice).
    void setExistingCategories(const std::vector<std::string>& categories) { m_existingCategories = categories; }
    QLineEdit* referenceEdit() const { return m_reference; }
    QListWidget* list() const { return m_list; }
    SituationEditor* editor() const { return m_editor; }
    bool masterDetail() const { return m_editor != nullptr; }
    QToolButton* addButton() const { return m_add; }
    QToolButton* copyButton() const { return m_copy; }
    QToolButton* deleteButton() const { return m_delete; }
    void addSituation();
    void copySituation();
    void deleteSituation();
    /// Opens the single-situation dialog on row `row` (not what a double click does: that renames in the list).
    void editSituation(int row);

private:
    void refresh(int select = -1);
    void rebuildEditor();
    void commitEditor();
    void updateSize();
    cc::core::UserSituationSet m_set;
    bool m_projectHasSeismic = false;
    std::vector<std::string> m_existingCategories;
    bool m_refreshing = false;
    int m_loadedRow = -1;  ///< row whose situation the editor holds
    QLineEdit* m_reference = nullptr;
    QListWidget* m_list = nullptr;
    SituationEditor* m_editor = nullptr;
    QWidget* m_listColumn = nullptr;
    QLayout* m_body = nullptr;
    QToolButton *m_add = nullptr, *m_copy = nullptr, *m_delete = nullptr;
};

}  // namespace cc::app
