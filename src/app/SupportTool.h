#pragma once
#include <QList>
#include <QObject>
#include <QPointer>
#include <QString>
#include <functional>
#include <vector>

#include "core/project.h"
#include "core/undo_stack.h"

class QAction;
class QActionGroup;
class QDialog;
class QWidget;

namespace cc::app {

class Viewport3D;

/// Propiedades > Nudos: "Vinculación exterior" and "Vinculación interior" (T-P6-01, GUI part; the data and the undoable commands are in
/// core/node_support.h and core/node_support_edit.h). Behaviour measured in the original (docs/research/ui/vinculaciones.md 5.1 / 5.4):
/// the command turns the 3D window into a node selection surface (LMB selects, window / crossing); RMB opens the dialog of the command on
/// the selected nodes and Aceptar applies it as one undoable step. The command stays active for the next selection.
class SupportTool : public QObject {
    Q_OBJECT
public:
    enum class Mode { None, External, Internal };

    explicit SupportTool(QObject* parent = nullptr);

    QAction* externalAction() const { return m_external; }
    QAction* internalAction() const { return m_internal; }

    void setProject(cc::core::Project* project, cc::core::UndoStack* undo);
    void addViewport(Viewport3D* viewport);
    Mode mode() const { return m_mode; }
    void deactivate();
    /// Call after every change of the history (push, undo, redo): enables / disables the buttons and refreshes the viewports.
    void structureChanged();

    /// Widget the dialogs are parented to (the main window).
    void setDialogParent(QWidget* parent) { m_dialogParent = parent; }
    /// Runs a modal dialog; returns QDialog::Accepted / Rejected. The default is exec(); tests replace it so that nothing blocks.
    void setDialogRunner(std::function<int(QDialog&)> runner) { m_runner = std::move(runner); }

    /// "Nudos - Vinculación exterior |  - LMB: Seleccione un grupo de elementos. - RMB: Actuar sobre los elementos seleccionados."
    static QString statusText(Mode mode);

    /// Opens "Vinculación exterior" on the given nodes (initial state: the support of the first one) and applies it when accepted; true when
    /// something changed.
    bool editExternal(const std::vector<int>& nodeIds);
    /// Opens "Vinculación interior": the pieces of the node with the most pieces give the table; applies it when accepted.
    bool editInternal(const std::vector<int>& nodeIds);

signals:
    void statusChanged(const QString& text);
    /// A command is about to become active: other viewport tools must stop.
    void aboutToActivate();
    void modeChanged(cc::app::SupportTool::Mode mode);

private:
    void setMode(Mode mode);
    void configureViewports();
    void updateEnabled();
    void onActOnSelection(Viewport3D* viewport);
    void pruneViewports();
    int run(QDialog& dialog);

    QAction* m_external = nullptr;
    QAction* m_internal = nullptr;
    QActionGroup* m_group = nullptr;
    cc::core::Project* m_project = nullptr;
    cc::core::UndoStack* m_undo = nullptr;
    QList<QPointer<Viewport3D>> m_viewports;
    Mode m_mode = Mode::None;
    QPointer<QWidget> m_dialogParent;
    std::function<int(QDialog&)> m_runner;
};

}  // namespace cc::app

Q_DECLARE_METATYPE(cc::app::SupportTool::Mode)
