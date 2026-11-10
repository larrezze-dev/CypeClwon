#pragma once
#include <QStringList>
#include <array>
#include <string>
#include <vector>

#include "CypeDialog.h"
#include "core/load_case.h"
#include "core/load_case_edit.h"
#include "core/project.h"
#include "core/undo_stack.h"

class QCheckBox;
class QComboBox;
class QGridLayout;
class QLabel;
class QLineEdit;
class QRadioButton;
class QTableWidget;
class QToolButton;
class QWidget;

namespace cc::app {

/// Dialogs of the load cases (T-P7-01; behaviour: docs/research/ui/hipotesis_carga.md, captures hy_*.png). All of them edit a LoadCaseSet
/// through the undoable commands of load_case_edit.h. The windows work on a scratch Project owned by AdditionalLoadsDialog (the wizard
/// "Datos generales" / "Obra vacía" has no project yet); the caller takes the result with value() on Aceptar.

/// Entries of the combos of "Categoría" per norm group. Only the entries seen in the original are real: "A. Domésticos y residenciales"
/// (concrete group) and the six CTE names of REA #52945-#52950 (steel / timber group, letters beyond "A." not verified).
/// TODO(medir): complete lists per norm and the letters.
QStringList useCategoryChoices(int normGroup);

/// Everything a hypothesis deletion can touch in the scratch project of «Hipótesis adicionales»: the hypotheses and the loads that go with
/// them. Cancelar in a nested window restores all of it (so that a second deletion asks again).
struct LoadCaseState {
    core::LoadCaseSet cases;
    core::NodalLoads nodalLoads;
    core::BarLoads barLoads;
    core::ShellLoads shellLoads;
    core::Panels panels;
    core::SurfaceLoads surfaceLoads;
    core::NodeSupports nodeSupports;
    static LoadCaseState capture(const core::Project& project);
    void restoreTo(core::Project& project) const;
};

/// Question shown before deleting a hypothesis that has loads (#64905; the «.:» is literal, as measured in the original).
QString deleteLoadCaseQuestion(const QString& label);

/// The two combos of a use category with their norm headings ("Hormigón: Eurocódigo 2" ...). Shared by "Categoría" and "Categorías de uso".
class UseCategoryChoicesPanel : public QWidget {
    Q_OBJECT
public:
    explicit UseCategoryChoicesPanel(QWidget* parent = nullptr);
    std::array<std::string, core::kUseCategoryNormGroups> choices() const;
    void setChoices(const std::array<std::string, core::kUseCategoryNormGroups>& choices);
    QComboBox* combo(int group) const { return m_combos[static_cast<std::size_t>(group)]; }
signals:
    void changed();

private:
    std::array<QComboBox*, core::kUseCategoryNormGroups> m_combos{};
};

/// "Categoría" (opened by "+" of "Categorías de uso").
class UseCategoryDialog : public CypeDialog {
    Q_OBJECT
public:
    explicit UseCategoryDialog(QWidget* parent = nullptr);
    std::array<std::string, core::kUseCategoryNormGroups> choices() const { return m_panel->choices(); }

private:
    UseCategoryChoicesPanel* m_panel;
};

/// "Categorías de uso": bar + / copy / X, table Ref. | Descripción, combos of the selected category. Cancelar restores the initial set.
class UseCategoriesDialog : public CypeDialog {
    Q_OBJECT
public:
    UseCategoriesDialog(core::Project& project, core::UndoStack& stack, QWidget* parent = nullptr);
    QTableWidget* table() const { return m_table; }
    void done(int result) override;

    void addCategory(const std::array<std::string, core::kUseCategoryNormGroups>& choices);
    void copySelected();
    void deleteSelected();

private:
    void refresh(int select);
    int selectedRow() const;
    core::Project& m_project;
    core::UndoStack& m_stack;
    LoadCaseState m_initial;
    QTableWidget* m_table = nullptr;
    UseCategoryChoicesPanel* m_panel = nullptr;
    QToolButton* m_copy = nullptr;
    QToolButton* m_delete = nullptr;
    bool m_refreshing = false;
};

/// "Nueva hipótesis adicional" / "Editar hipótesis adicional".
class LoadCaseDataDialog : public CypeDialog {
    Q_OBJECT
public:
    /// New: `proposed` is the proposed reference.
    LoadCaseDataDialog(core::LoadNature nature, const QString& proposed, QWidget* parent = nullptr);
    /// Edit: reference and description only.
    explicit LoadCaseDataDialog(const core::LoadCase& existing, QWidget* parent = nullptr);

    std::string reference() const;
    std::string description() const;
    bool withArrangements() const;
    const std::vector<std::string>& arrangements() const { return m_arrangements; }
    core::ArrangementCompat compat() const;
    core::SeismicType seismic() const;
    /// Data for core::createLoadCase.
    core::NewLoadCase value() const;

    QLineEdit* referenceEdit() const { return m_reference; }
    QLineEdit* descriptionEdit() const { return m_description; }
    QCheckBox* arrangementsCheck() const { return m_withArrangements; }
    QTableWidget* arrangementsTable() const { return m_table; }
    QRadioButton* compatRadio(core::ArrangementCompat c) const { return m_compat[static_cast<int>(c)]; }
    QRadioButton* seismicRadio(core::SeismicType t) const { return m_seismic[static_cast<int>(t)]; }
    void addArrangement();
    void deleteSelectedArrangement();
    void accept() override;

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void init(const QString& title, const QString& reference, const QString& description);
    void fillArrangements();
    void resizeForState();
    core::LoadNature m_nature = core::LoadNature::DeadLoad;
    QLineEdit* m_reference = nullptr;
    QLineEdit* m_description = nullptr;
    QCheckBox* m_withArrangements = nullptr;
    QWidget* m_arrangementsPanel = nullptr;
    QTableWidget* m_table = nullptr;
    std::array<QRadioButton*, 3> m_compat{};
    std::array<QRadioButton*, 2> m_seismic{};
    QWidget* m_seismicPanel = nullptr;
    std::vector<std::string> m_arrangements;
    bool m_edit = false;
};

/// Window of one nature ("Cargas muertas", "Sismo", ...): link "+ Nueva hipótesis adicional" and table Actúa | Hipótesis adicionales |
/// (Tipo) | Editar | Borrar. Cancelar restores the cases the set had on opening.
class LoadNatureDialog : public CypeDialog {
    Q_OBJECT
public:
    LoadNatureDialog(core::LoadNature nature, core::Project& project, core::UndoStack& stack, QWidget* parent = nullptr);
    QTableWidget* table() const { return m_table; }
    QCheckBox* secondOrderCheck() const { return m_secondOrder; }
    void done(int result) override;

    /// Opens "Nueva hipótesis adicional" and creates the case; false when cancelled or refused.
    bool createCase();
    /// Opens "Editar hipótesis adicional" for the case of table row `row`.
    void editRow(int row);
    void toggleRow(int row);
    void deleteRow(int row);
    int caseIdOfRow(int row) const;
    static int columnEdit(core::LoadNature nature) { return nature == core::LoadNature::Seismic ? 3 : 2; }

private:
    void refresh();
    void onCellClicked(int row, int column);
    core::LoadNature m_nature;
    core::Project& m_project;
    core::UndoStack& m_stack;
    LoadCaseState m_initial;
    QTableWidget* m_table = nullptr;
    QCheckBox* m_secondOrder = nullptr;
};

/// "Hipótesis adicionales" (280x443). Datos generales > Acciones.
class AdditionalLoadsDialog : public CypeDialog {
    Q_OBJECT
public:
    explicit AdditionalLoadsDialog(const core::LoadCaseSet& initial, QWidget* parent = nullptr);
    const core::LoadCaseSet& value() const { return m_project.loadCases; }
    core::Project& scratchProject() { return m_project; }
    QToolButton* editButton(core::LoadNature nature) const { return m_edit[static_cast<std::size_t>(nature)]; }
    QToolButton* categoriesButton() const { return m_categoriesEdit; }
    QLabel* counter(core::LoadNature nature) const { return m_counters[static_cast<std::size_t>(nature)]; }
    QLabel* categoriesLabel() const { return m_categoriesLabel; }

    /// What the edit button of a nature does: for "Sobrecarga de uso" without categories, the notice and then "Categorías de uso".
    void openNature(core::LoadNature nature);
    void openCategories();

private:
    void refresh();
    core::Project m_project;
    core::UndoStack m_stack;
    QLabel* m_categoriesLabel = nullptr;
    QToolButton* m_categoriesEdit = nullptr;
    std::array<QLabel*, core::kLoadNatureCount> m_counters{};
    std::array<QToolButton*, core::kLoadNatureCount> m_edit{};
};

}  // namespace cc::app
