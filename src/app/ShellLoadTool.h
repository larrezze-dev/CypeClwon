#pragma once
#include <QList>
#include <QObject>
#include <QPointer>
#include <QString>
#include <functional>
#include <vector>

#include "ShellLoadDialogs.h"
#include "core/shell_load_edit.h"
#include "core/project.h"
#include "core/undo_stack.h"

class QAction;
class QActionGroup;
class QDialog;
class QWidget;

namespace cc::app {

class NodalLoadTool;
class Viewport3D;

/// Carga > Láminas (T-P7-04, GUI part; data and undoable commands in core/shell_load*.h). Behaviour measured in the original
/// (docs/research/ui/cargas_laminas.md sections 8-10): Nueva (large) | column Editar / Borrar | column Hipótesis / Peso propio. Every button turns the
/// 3D window into a shell selection surface (LMB selects, RMB acts on the selected shells, the selected loads are orange). Editar / Borrar /
/// Hipótesis act on ONE load per selected shell (the first one created that is in view: measured, one RMB edits / deletes one load) and are disabled
/// while no shell load exists. Peso propio opens its small dialog at once and then selects shells BY WINDOW only (measured: a plain click does not
/// select); RMB without a selection opens the dialog again. The floating window «Propiedades - Hipótesis vista» and the shown hypothesis belong to
/// the NodalLoadTool (`host`), which all the load tools share; the scales too (Visualización > Escalas).
class ShellLoadTool : public QObject {
    Q_OBJECT
public:
    enum class Mode { None, New, Edit, Case, Delete, SelfWeight };

    explicit ShellLoadTool(QObject* parent = nullptr);

    QAction* newAction() const { return m_new; }
    QAction* editAction() const { return m_edit; }
    QAction* caseAction() const { return m_case; }
    QAction* deleteAction() const { return m_delete; }
    QAction* selfWeightAction() const { return m_selfWeight; }

    /// The tool of the nodal loads that owns the hypothesis view window (must outlive this tool).
    void setHost(NodalLoadTool* host);
    void setProject(cc::core::Project* project, cc::core::UndoStack* undo);
    void addViewport(Viewport3D* viewport);
    Mode mode() const { return m_mode; }
    void deactivate();
    /// Call after every change of the history (push, undo, redo): enables / disables the buttons and refreshes the viewports.
    void structureChanged();

    void setDialogParent(QWidget* parent) { m_dialogParent = parent; }
    /// Runs a modal dialog; the default is exec(); tests replace it so that nothing blocks.
    void setDialogRunner(std::function<int(QDialog&)> runner) { m_runner = std::move(runner); }

    /// "Láminas - Nueva |  - LMB: Seleccione un grupo de elementos. - RMB: Actuar sobre los elementos seleccionados." (idem for the others).
    static QString statusText(Mode mode);
    /// Text of the tooltip of a button (title, then the help text of the original).
    static QString tipText(Mode mode);

    /// The loads Editar / Hipótesis / Borrar capture: the first load created of each given shell that is in view, in shell order.
    std::vector<int> loadsOfShells(const std::vector<int>& shellIds) const;
    /// The actions on the given shells (what RMB does); true when the work changed. Errors are shown as an «Aviso» / «Error».
    bool createLoads(const std::vector<int>& shellIds);
    bool editLoads(const std::vector<int>& shellIds);
    bool changeCase(const std::vector<int>& shellIds);
    bool deleteLoads(const std::vector<int>& shellIds);
    /// Peso propio: applies the option of the last dialog to the shells. Without shells it opens the dialog again.
    bool applySelfWeight(const std::vector<int>& shellIds);

    /// Last data of «Introducir cargas sobre láminas» and the Peso propio option (kept while the program runs).
    const cc::core::ShellLoadSession& session() const { return m_session; }

signals:
    void statusChanged(const QString& text);
    /// A command is about to become active: other viewport tools must stop.
    void aboutToActivate();
    void modeChanged(cc::app::ShellLoadTool::Mode mode);

private:
    void setMode(Mode mode);
    void configureViewports();
    void updateEnabled();
    void onActOnSelection(Viewport3D* viewport);
    void pruneViewports();
    void warn(const QString& text, bool error = false);
    int run(QDialog& dialog);
    bool viewAll() const;
    int viewCase() const;
    int selfWeightCaseId() const;
    bool askSelfWeight();

    QAction* m_new = nullptr;
    QAction* m_edit = nullptr;
    QAction* m_case = nullptr;
    QAction* m_delete = nullptr;
    QAction* m_selfWeight = nullptr;
    QActionGroup* m_group = nullptr;
    cc::core::Project* m_project = nullptr;
    cc::core::UndoStack* m_undo = nullptr;
    QPointer<NodalLoadTool> m_host;
    QList<QPointer<Viewport3D>> m_viewports;
    Mode m_mode = Mode::None;
    QPointer<QWidget> m_dialogParent;
    std::function<int(QDialog&)> m_runner;
    cc::core::ShellLoadSession m_session;
    bool m_configured = false;
    bool m_selfWeightAsked = false;  ///< the Peso propio dialog was accepted: the command is fully active
};

}  // namespace cc::app

Q_DECLARE_METATYPE(cc::app::ShellLoadTool::Mode)
