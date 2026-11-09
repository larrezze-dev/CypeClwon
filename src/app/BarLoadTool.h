#pragma once
#include <QList>
#include <QObject>
#include <QPointer>
#include <QString>
#include <functional>
#include <vector>

#include "BarLoadDialogs.h"
#include "core/bar_load_edit.h"
#include "core/project.h"
#include "core/undo_stack.h"

class QAction;
class QActionGroup;
class QDialog;
class QWidget;

namespace cc::app {

class NodalLoadTool;
class Viewport3D;

/// Carga > Barras (T-P7-03, GUI part; data and undoable commands in core/bar_load*.h). Behaviour measured in the original
/// (docs/research/ui/cargas_barras.md section 10-12): Nueva | column Editar / Borrar | column Hipótesis / Peso propio. Every button turns the 3D
/// window into a bar selection surface (LMB selects, RMB acts on the selected bars, the selected loads are orange). Editar / Borrar / Hipótesis act
/// on the loads of the selected bars (the bars without loads in view cannot be captured) and are disabled while no bar load exists. Peso propio
/// opens its small dialog at once and then selects bars; RMB without a selection opens the dialog again. The floating window «Propiedades -
/// Hipótesis vista» and the shown hypothesis belong to the NodalLoadTool (`host`), which both tools share; the scales too (Visualización > Escalas).
class BarLoadTool : public QObject {
    Q_OBJECT
public:
    enum class Mode { None, New, Edit, Case, Delete, SelfWeight };

    explicit BarLoadTool(QObject* parent = nullptr);

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

    /// "Barras - Nueva |  - LMB: Seleccione un grupo de elementos. - RMB: Actuar sobre los elementos seleccionados." (idem for the others).
    static QString statusText(Mode mode);
    /// Text of the tooltip of a button (title, then the help text of the original).
    static QString tipText(Mode mode);

    /// Loads of the given bars that are in view (the ones Editar / Hipótesis / Borrar capture), in bar and creation order.
    std::vector<int> loadsOfBars(const std::vector<int>& barIds) const;
    /// The actions on the given bars (what RMB does); true when the work changed. Errors are shown as an «Aviso» / «Error».
    bool createLoads(const std::vector<int>& barIds);
    bool editLoads(const std::vector<int>& barIds);
    bool changeCase(const std::vector<int>& barIds);
    bool deleteLoads(const std::vector<int>& barIds);
    /// Peso propio: applies the option of the last dialog to the bars. Without bars it opens the dialog again.
    bool applySelfWeight(const std::vector<int>& barIds);
    /// «Se han borrado cargas incompatibles con la longitud de la barra.» (#65192): for the edits that shorten bars.
    void showOutOfLengthWarning();

    /// Last data of «Introducir cargas sobre barras» and the Peso propio option (kept while the program runs).
    const cc::core::BarLoadSession& session() const { return m_session; }

signals:
    void statusChanged(const QString& text);
    /// A command is about to become active: other viewport tools must stop.
    void aboutToActivate();
    void modeChanged(cc::app::BarLoadTool::Mode mode);

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
    cc::core::BarLoadSession m_session;
    bool m_configured = false;
    bool m_selfWeightAsked = false;  ///< the Peso propio dialog was accepted: the command is fully active
};

}  // namespace cc::app

Q_DECLARE_METATYPE(cc::app::BarLoadTool::Mode)
