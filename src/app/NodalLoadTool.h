#pragma once
#include <QList>
#include <QObject>
#include <QPointer>
#include <QString>
#include <functional>
#include <vector>

#include "NodalLoadDialogs.h"
#include "core/nodal_load_edit.h"
#include "core/project.h"
#include "core/undo_stack.h"

class QAction;
class QActionGroup;
class QDialog;
class QWidget;

namespace cc::app {

class Viewport3D;

/// Carga > Nudos and Carga > Visualización (T-P7-02, GUI part; data and undoable commands in core/nodal_load*.h and core/node_support*.h).
/// Behaviour measured in the original (docs/research/ui/cargas_nudos.md): five buttons Desplazamientos prescritos | Nueva | Editar | Hipótesis |
/// Borrar. Every one turns the 3D window into a node selection surface (LMB selects, RMB acts on the selected nodes); Editar / Hipótesis / Borrar
/// act on the loads of the selected nodes (the nodes without loads cannot be captured) and are disabled while no load exists; Prescribed is
/// disabled while no node has an external support. Nueva / Editar / Hipótesis / Borrar show the floating window «Propiedades - Hipótesis vista».
/// Each action is one undoable step.
class NodalLoadTool : public QObject {
    Q_OBJECT
public:
    enum class Mode { None, Prescribed, New, Edit, Case, Delete };

    explicit NodalLoadTool(QObject* parent = nullptr);

    QAction* prescribedAction() const { return m_prescribed; }
    QAction* newAction() const { return m_new; }
    QAction* editAction() const { return m_edit; }
    QAction* caseAction() const { return m_case; }
    QAction* deleteAction() const { return m_delete; }
    /// Carga > Visualización > «Hipótesis vista» (shows the floating window) and «Escalas» (dialog).
    QAction* viewAction() const { return m_view; }
    QAction* scalesAction() const { return m_scales; }

    void setProject(cc::core::Project* project, cc::core::UndoStack* undo);
    void addViewport(Viewport3D* viewport);
    Mode mode() const { return m_mode; }
    void deactivate();
    /// Call after every change of the history (push, undo, redo): enables / disables the buttons and refreshes the viewports.
    void structureChanged();
    /// The loads are drawn only while the ribbon tab «Carga» is the current one.
    void setLoadsVisible(bool on);
    bool loadsVisible() const { return m_loadsVisible; }

    void setDialogParent(QWidget* parent) { m_dialogParent = parent; }
    /// Runs a modal dialog; the default is exec(); tests replace it so that nothing blocks.
    void setDialogRunner(std::function<int(QDialog&)> runner) { m_runner = std::move(runner); }
    LoadViewPanel* panel() const { return m_panel; }

    /// "Nudos - Nueva |  - LMB: Seleccione un grupo de elementos. - RMB: Actuar sobre los elementos seleccionados." (idem for the others)
    static QString statusText(Mode mode);

    /// Shown hypothesis of the window «Propiedades - Hipótesis vista» (also kept while the window is closed).
    int viewCaseId() const { return m_viewCase; }
    bool viewAll() const { return m_viewAll; }
    void setView(int caseId, bool all);

    /// Loads of the given nodes that are in view (the ones the original lets Editar / Hipótesis / Borrar capture), in node and creation order.
    std::vector<int> loadsOfNodes(const std::vector<int>& nodeIds) const;
    /// The four actions on the given nodes (what RMB does); true when the work changed. Errors are shown as an «Aviso».
    bool createLoads(const std::vector<int>& nodeIds);
    bool editLoads(const std::vector<int>& nodeIds);
    bool changeCase(const std::vector<int>& nodeIds);
    bool deleteLoads(const std::vector<int>& nodeIds);
    bool editPrescribed(const std::vector<int>& nodeIds);
    /// Carga > Visualización > Escalas.
    bool editScales();
    /// Shows (or raises) the window «Propiedades - Hipótesis vista».
    void showViewPanel();

    /// Last data of «Introducir cargas sobre nudos» (the dialog remembers value and direction between uses).
    const cc::core::NodalLoadData& lastData() const { return m_last; }

signals:
    void statusChanged(const QString& text);
    /// A command is about to become active: other viewport tools must stop.
    void aboutToActivate();
    void modeChanged(cc::app::NodalLoadTool::Mode mode);

private:
    void setMode(Mode mode);
    void configureViewports();
    void updateEnabled();
    void applyView();
    void onActOnSelection(Viewport3D* viewport);
    void pruneViewports();
    void warn(const QString& text);
    int run(QDialog& dialog);

    QAction* m_prescribed = nullptr;
    QAction* m_new = nullptr;
    QAction* m_edit = nullptr;
    QAction* m_case = nullptr;
    QAction* m_delete = nullptr;
    QAction* m_view = nullptr;
    QAction* m_scales = nullptr;
    QActionGroup* m_group = nullptr;
    cc::core::Project* m_project = nullptr;
    cc::core::UndoStack* m_undo = nullptr;
    QList<QPointer<Viewport3D>> m_viewports;
    Mode m_mode = Mode::None;
    QPointer<QWidget> m_dialogParent;
    QPointer<LoadViewPanel> m_panel;
    std::function<int(QDialog&)> m_runner;
    cc::core::NodalLoadData m_last;
    int m_viewCase = 0;
    bool m_viewAll = false;
    bool m_loadsVisible = false;
};

}  // namespace cc::app

Q_DECLARE_METATYPE(cc::app::NodalLoadTool::Mode)
