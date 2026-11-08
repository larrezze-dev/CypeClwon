#pragma once
#include <QList>
#include <QObject>
#include <QPointer>
#include <QString>
#include <functional>
#include <optional>
#include <vector>

#include "CypeDialog.h"
#include "core/load_case.h"
#include "core/node_diaphragm.h"
#include "core/project.h"
#include "core/undo_stack.h"

class QAction;
class QComboBox;
class QLabel;
class QLineEdit;
class QRadioButton;
class QTableWidget;
class QToolButton;

namespace cc::app {

class Viewport3D;

/// Propiedades > Diafragmas > Editar (diafragmas.md section 3, orig_diaf_01 / 02 / 07 / 08 / 09 / 20): window "Editar" (697x520, maximisable) with
/// the list "Referencia | Color" (+ / duplicate / delete toolbar), the field "Referencia" and the group "Cargas introducidas" (own toolbar and
/// table "Hipótesis | Fx (kN) | Fy (kN) | Coord. x (m) | Coord. y (m)"). The dialog edits a copy (`rows()`); Aceptar validates with
/// checkDiaphragmAccept («Aviso») and the tool applies it with editDiaphragms.
class DiaphragmEditDialog : public CypeDialog {
    Q_OBJECT
public:
    DiaphragmEditDialog(const std::vector<cc::core::Diaphragm>& initial, QWidget* parent = nullptr);
    /// With the real load cases of the work (T-P7-01, closes the load part of TK-089): the combos of the table "Cargas introducidas" list them
    /// and a new row takes the first one ("Peso propio"). A row whose case no longer exists is moved to the first case.
    DiaphragmEditDialog(const std::vector<cc::core::Diaphragm>& initial, const cc::core::LoadCaseSet& cases, QWidget* parent = nullptr);

    /// The rows as the user left them (the nodes of existing rows are not touched: the project keeps them).
    const std::vector<cc::core::Diaphragm>& rows() const { return m_rows; }
    int currentRow() const;
    /// "+": checkDiaphragmAdd first («La referencia no puede quedar vacía.») and then a new magenta row. True when added.
    bool addRow();
    void duplicateRow();
    void removeRow();
    bool addLoad();
    void duplicateLoad();
    void removeLoad();
    /// Sets the colour of a row (what the colour picker gives).
    void setColour(int row, unsigned rgb);

    QToolButton* addButton() const { return m_add; }
    QToolButton* duplicateButton() const { return m_duplicate; }
    QToolButton* removeButton() const { return m_remove; }
    QToolButton* addLoadButton() const { return m_addLoad; }
    QToolButton* duplicateLoadButton() const { return m_duplicateLoad; }
    QToolButton* removeLoadButton() const { return m_removeLoad; }
    QTableWidget* table() const { return m_table; }
    QTableWidget* loadTable() const { return m_loadTable; }
    QLineEdit* referenceEdit() const { return m_reference; }
    QWidget* loadGroup() const { return m_loadGroup; }

    /// Name of a load case ("hipótesis") of the work. PROVISIONAL: the load cases arrive with T-P7-01; until then the only one is the first
    /// of every new work, "Peso propio" (id 1, measured as the default of a new load).
    static QString loadCaseName(int loadCaseId);
    /// Id of the case a new load row takes: the first of the set when there is one, else the placeholder below.
    int newLoadCaseId() const { return m_cases && !m_cases->cases.empty() ? m_cases->cases.front().id : firstLoadCaseId(); }
    static int firstLoadCaseId() { return 1; }

    /// Colour window "Selección del color". The default is Qt's colour dialog with that title; tests replace it. Returns false on Cancelar.
    static void setColourPicker(std::function<bool(QWidget*, unsigned&)> picker);

    void accept() override;

private:
    void refreshTable(int selectRow);
    void refreshRight();
    void refreshLoads(int selectRow);
    void onLoadChanged(int row, int column);
    void pickColour(int row);

    std::vector<cc::core::Diaphragm> m_rows;
    std::optional<cc::core::LoadCaseSet> m_cases;
    QTableWidget* m_table = nullptr;
    QLineEdit* m_reference = nullptr;
    QWidget* m_rightPane = nullptr;
    QWidget* m_loadGroup = nullptr;
    QTableWidget* m_loadTable = nullptr;
    QToolButton* m_add = nullptr;
    QToolButton* m_duplicate = nullptr;
    QToolButton* m_remove = nullptr;
    QToolButton* m_addLoad = nullptr;
    QToolButton* m_duplicateLoad = nullptr;
    QToolButton* m_removeLoad = nullptr;
    bool m_loading = false;
};

/// Floating window "Propiedades - Asignar" (orig_diaf_11 / 25): radios Asignar (default, bold) / Eliminar asignación and, for Asignar, the combo
/// "Diafragma" with the references defined.
class DiaphragmPanel : public CypeDialog {
    Q_OBJECT
public:
    explicit DiaphragmPanel(QWidget* parent = nullptr);

    bool assign() const;
    /// Id of the diaphragm chosen in the combo (0 when none).
    int diaphragmId() const;
    void setDiaphragms(const cc::core::Diaphragms& diaphragms);
    QRadioButton* assignRadio() const { return m_assign; }
    QRadioButton* removeRadio() const { return m_remove; }
    QComboBox* combo() const { return m_combo; }
    /// True while the combo row is shown (only for Asignar).
    bool comboVisible() const;

signals:
    void closed();

protected:
    void done(int result) override;

private:
    QRadioButton* m_assign = nullptr;
    QRadioButton* m_remove = nullptr;
    QWidget* m_comboRow = nullptr;
    QComboBox* m_combo = nullptr;
};

/// Propiedades > Diafragmas (T-P6-03, GUI part; data, checks and undoable commands in core/node_diaphragm*.h): the three buttons Editar,
/// Diafragma and Generar por niveles. "Diafragma" turns the 3D window into a node selection surface and RMB acts on the selected nodes; the
/// node texts show "N4: D1" and the squares the diaphragm colour while the tool is active.
class DiaphragmTool : public QObject {
    Q_OBJECT
public:
    enum class Mode { None, Active };

    explicit DiaphragmTool(QObject* parent = nullptr);

    QAction* editAction() const { return m_edit; }
    QAction* assignAction() const { return m_assign; }
    QAction* generateAction() const { return m_generate; }
    void setProject(cc::core::Project* project, cc::core::UndoStack* undo);
    void addViewport(Viewport3D* viewport);
    Mode mode() const { return m_mode; }
    void deactivate();
    /// Call after every change of the history (push, undo, redo).
    void structureChanged();
    void setDialogParent(QWidget* parent) { m_dialogParent = parent; }
    DiaphragmPanel* panel() const { return m_panel; }
    /// Runs a modal dialog; the default is exec(); tests replace it so that nothing blocks.
    void setDialogRunner(std::function<int(QDialog&)> runner) { m_runner = std::move(runner); }

    /// "Diafragmas - Diafragma |  - LMB: Seleccione un grupo de elementos. - RMB: Actuar sobre los elementos seleccionados."
    static QString statusText(Mode mode);

    /// "Editar": opens the dialog on the diaphragms of the work and applies it when accepted. True when something changed.
    bool edit();
    /// "Generar por niveles" (question / messages of the original; the algorithm of the core is provisional). True when diaphragms were generated.
    bool generate();
    /// Runs the chosen option of the panel on the nodes (selection order). True when the work changed. Errors are shown as an «Error» window.
    bool execute(const std::vector<int>& nodeIds);

signals:
    void statusChanged(const QString& text);
    void aboutToActivate();
    void modeChanged(cc::app::DiaphragmTool::Mode mode);

private:
    void start();
    void configureViewports();
    void updateEnabled();
    void onActOnSelection(Viewport3D* viewport);
    void pruneViewports();
    int run(QDialog& dialog);

    QAction* m_edit = nullptr;
    QAction* m_assign = nullptr;
    QAction* m_generate = nullptr;
    cc::core::Project* m_project = nullptr;
    cc::core::UndoStack* m_undo = nullptr;
    QList<QPointer<Viewport3D>> m_viewports;
    Mode m_mode = Mode::None;
    QPointer<QWidget> m_dialogParent;
    QPointer<DiaphragmPanel> m_panel;
    std::function<int(QDialog&)> m_runner;
};

}  // namespace cc::app

Q_DECLARE_METATYPE(cc::app::DiaphragmTool::Mode)
