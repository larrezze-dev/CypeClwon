#pragma once
#include <QList>
#include <QObject>
#include <QPointer>
#include <QString>
#include <vector>

#include "CypeDialog.h"
#include "core/node_ligature.h"
#include "core/project.h"
#include "core/undo_stack.h"

class QAction;
class QCheckBox;
class QRadioButton;
class QStackedWidget;

namespace cc::app {

class Viewport3D;

/// Floating window "Propiedades - Ligaduras" (ligaduras.md section 7): group "Opciones" with the radios Asignar (default, bold) / Eliminar and
/// group "Desplazamientos" with the checkboxes "Desplazamiento en X / Y / Z" (all unchecked at the start). Each radio keeps its own set of
/// checkboxes: switching the radio shows the set of that radio.
class LigaturePanel : public CypeDialog {
    Q_OBJECT
public:
    explicit LigaturePanel(QWidget* parent = nullptr);

    bool assign() const;
    /// Directions ticked in the set of the current radio.
    cc::core::LigatureDirections directions() const;
    QRadioButton* assignRadio() const { return m_assign; }
    QRadioButton* removeRadio() const { return m_remove; }
    /// Checkbox `dir` (0 = X, 1 = Y, 2 = Z) of the set of "Asignar" (`forAssign`) or "Eliminar".
    QCheckBox* check(bool forAssign, int dir) const { return m_checks[forAssign ? 0 : 1][dir]; }

signals:
    void closed();

protected:
    void done(int result) override;

private:
    QRadioButton* m_assign = nullptr;
    QRadioButton* m_remove = nullptr;
    QStackedWidget* m_pages = nullptr;
    QCheckBox* m_checks[2][3] = {};
};

/// Propiedades > Nudos > Ligaduras (T-P6-02, GUI part; data, checks and undoable commands in core/node_ligature*.h). The button opens the panel
/// and turns the 3D window into a node selection surface (two-click window, orange selected nodes); RMB executes the chosen option on the
/// selected nodes as one undoable step. The node texts show their ties while the tool is active.
class LigatureTool : public QObject {
    Q_OBJECT
public:
    enum class Mode { None, Active };

    explicit LigatureTool(QObject* parent = nullptr);

    QAction* action() const { return m_action; }
    void setProject(cc::core::Project* project, cc::core::UndoStack* undo);
    void addViewport(Viewport3D* viewport);
    Mode mode() const { return m_mode; }
    void deactivate();
    /// Call after every change of the history (push, undo, redo).
    void structureChanged();
    void setDialogParent(QWidget* parent) { m_dialogParent = parent; }
    LigaturePanel* panel() const { return m_panel; }

    /// "Nudos - Ligaduras |  - LMB: Seleccione un grupo de elementos." (measured: no RMB hint).
    static QString statusText(Mode mode);

    /// Runs the chosen option of the panel on the nodes (selection order). Returns true when the work changed. Errors are announced through
    /// errorRequested; a silent check does nothing.
    bool execute(const std::vector<int>& nodeIds);

signals:
    void statusChanged(const QString& text);
    void aboutToActivate();
    void modeChanged(cc::app::LigatureTool::Mode mode);
    /// Text of the "Error" dialog (the window shows it).
    void errorRequested(const QString& text);

private:
    void start();
    void configureViewports();
    void updateEnabled();
    void onActOnSelection(Viewport3D* viewport);
    void pruneViewports();

    QAction* m_action = nullptr;
    cc::core::Project* m_project = nullptr;
    cc::core::UndoStack* m_undo = nullptr;
    QList<QPointer<Viewport3D>> m_viewports;
    Mode m_mode = Mode::None;
    QPointer<QWidget> m_dialogParent;
    QPointer<LigaturePanel> m_panel;
};

}  // namespace cc::app

Q_DECLARE_METATYPE(cc::app::LigatureTool::Mode)
