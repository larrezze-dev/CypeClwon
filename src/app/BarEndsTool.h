#pragma once
#include <QList>
#include <QObject>
#include <QPointer>
#include <QString>
#include <array>
#include <functional>
#include <vector>

#include "CypeDialog.h"
#include "core/bar_ends.h"
#include "core/project.h"
#include "core/undo_stack.h"

class QAction;
class QCheckBox;
class QComboBox;
class QGridLayout;
class QLabel;
class QLineEdit;
class QRadioButton;

namespace cc::app {

class Viewport3D;

/// Floating window "Propiedades - Tipo de selección" of Articular (orig_artic_01 / 11): radios Individual (default) / Múltiple; the chosen one is bold.
class SelectionTypePanel : public CypeDialog {
    Q_OBJECT
public:
    explicit SelectionTypePanel(QWidget* parent = nullptr);
    bool multiple() const;
    QRadioButton* individualRadio() const { return m_individual; }
    QRadioButton* multipleRadio() const { return m_multiple; }

signals:
    void closed();
    void typeChanged();

protected:
    void done(int result) override;

private:
    QRadioButton* m_individual = nullptr;
    QRadioButton* m_multiple = nullptr;
};

/// Dialog "Empotramiento en extremos" (articulaciones.md section 3, orig_artic_08 / 14 / 15 / 20): the piece picture, the columns «Origen de la pieza»
/// / «Extremo de la pieza» and three rows (planes xy, xz, yz), each with the check «Asignar (Plano ..)» and, per end, the radios «Coef.
/// empotramiento» / «Rigidez rotacional» with their field (the yz coefficient is a popup 0.000 / 1.000). Aceptar validates (Aviso of the range)
/// and asks the question of the default rotational stiffness.
class EndFixityDialog : public CypeDialog {
    Q_OBJECT
public:
    explicit EndFixityDialog(QWidget* parent = nullptr);

    /// What the dialog says now (the text of the fields is read; an unreadable field keeps the last valid value).
    cc::core::EndFixityEdit edit() const;
    QCheckBox* assignCheck(int plane) const { return m_assign[static_cast<std::size_t>(plane)]; }
    QRadioButton* coefficientRadio(int end, int plane) const { return m_coefficient[static_cast<std::size_t>(end)][static_cast<std::size_t>(plane)]; }
    QRadioButton* stiffnessRadio(int end, int plane) const { return m_stiffness[static_cast<std::size_t>(end)][static_cast<std::size_t>(plane)]; }
    QLineEdit* field(int end, int plane) const { return m_field[static_cast<std::size_t>(end)][static_cast<std::size_t>(plane)]; }
    QComboBox* yzCombo(int end) const { return m_yz[static_cast<std::size_t>(end)]; }
    QLabel* unitLabel(int end, int plane) const { return m_unit[static_cast<std::size_t>(end)][static_cast<std::size_t>(plane)]; }
    /// Types `text` in the field as the user would (and commits it as leaving the field does).
    void typeField(int end, int plane, const QString& text);

    void accept() override;

private:
    void buildEnd(QWidget* parent, int end, int plane, QGridLayout* grid, int row, int column);
    void refreshPlane(int end, int plane);
    void commitField(int end, int plane);

    std::array<QCheckBox*, 3> m_assign{};
    std::array<std::array<QRadioButton*, 3>, 2> m_coefficient{};
    std::array<std::array<QRadioButton*, 3>, 2> m_stiffness{};
    std::array<std::array<QLineEdit*, 3>, 2> m_field{};
    std::array<std::array<QLabel*, 3>, 2> m_unit{};
    std::array<QComboBox*, 2> m_yz{};
    std::array<std::array<double, 3>, 2> m_coefficientValue{};  ///< last valid value of each plane (coefficient mode)
    std::array<std::array<double, 3>, 2> m_stiffnessValue{};    ///< last valid value (rotational mode)
    bool m_updating = false;
};

/// Propiedades > Barras > Articular and Empotramiento (T-P6-05, GUI part; data and undoable commands in core/bar_ends*.h).
///  - Articular: panel "Tipo de selección"; Múltiple: window selection + RMB marks both ends of the selected bars as pinned (black circles);
///    Individual: a click on a bar pins the END NEAREST to the click (PROVISIONAL: not measured which end the original takes).
///  - Empotramiento: window selection of bars + RMB opens the dialog; Aceptar assigns the fixity of the planes ticked as one undoable step.
class BarEndsTool : public QObject {
    Q_OBJECT
public:
    enum class Mode { None, Articulate, Fixity };

    explicit BarEndsTool(QObject* parent = nullptr);

    QAction* articulateAction() const { return m_articulate; }
    QAction* fixityAction() const { return m_fixity; }
    void setProject(cc::core::Project* project, cc::core::UndoStack* undo);
    void addViewport(Viewport3D* viewport);
    Mode mode() const { return m_mode; }
    void deactivate();
    /// Call after every change of the history (push, undo, redo).
    void structureChanged();
    void setDialogParent(QWidget* parent) { m_dialogParent = parent; }
    SelectionTypePanel* panel() const { return m_panel; }
    /// Runs a modal dialog; the default is exec(); tests replace it so that nothing blocks.
    void setDialogRunner(std::function<int(QDialog&)> runner) { m_runner = std::move(runner); }

    /// "Barras - Articular |  - LMB: Seleccione un elemento." (Individual) / "... - LMB: Seleccione un grupo de elementos. - RMB: Actuar sobre
    /// los elementos seleccionados." (Múltiple, Empotramiento).
    static QString statusText(Mode mode, bool multiple);

    /// Múltiple: pins both ends of the bars. True when something changed.
    bool articulate(const std::vector<int>& barIds);
    /// Individual: pins the end of `barId` nearest to the screen point `at` (needs a viewport). True when something changed.
    bool articulateNearest(Viewport3D& viewport, int barId, const QPointF& at);
    /// Empotramiento on the bars: opens the dialog (initial state: the defaults) and applies it when accepted. True when something changed.
    bool editFixity(const std::vector<int>& barIds);

signals:
    void statusChanged(const QString& text);
    void aboutToActivate();
    void modeChanged(cc::app::BarEndsTool::Mode mode);

private:
    void start(Mode mode);
    void stop();
    void configureViewports();
    void updateEnabled();
    void onActOnSelection(Viewport3D* viewport);
    void onSelectionChanged(Viewport3D* viewport);
    void pruneViewports();
    int run(QDialog& dialog);
    bool multipleSelection() const;

    QAction* m_articulate = nullptr;
    QAction* m_fixity = nullptr;
    cc::core::Project* m_project = nullptr;
    cc::core::UndoStack* m_undo = nullptr;
    QList<QPointer<Viewport3D>> m_viewports;
    Mode m_mode = Mode::None;
    QPointer<QWidget> m_dialogParent;
    QPointer<SelectionTypePanel> m_panel;
    std::function<int(QDialog&)> m_runner;
};

}  // namespace cc::app

Q_DECLARE_METATYPE(cc::app::BarEndsTool::Mode)
