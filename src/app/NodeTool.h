#pragma once
#include <QList>
#include <QObject>
#include <QPointer>
#include <QString>
#include <optional>

#include "core/project.h"
#include "core/snap.h"
#include "core/undo_stack.h"

class QAction;
class QActionGroup;

namespace cc::app {

class NodeCalculatorDialog;
class Viewport3D;

/// Geometria > Nudos: the three commands "Nuevo", "Borrar" and "Mover" (T-P5-02, GUI part; the edits themselves are the undoable
/// commands of core/node_edit.h). Behaviour sources: docs/research/ui/nudos.md sections 1-3 (code) and 8 (measured in the original).
///
/// The three actions live in an exclusive-optional QActionGroup: the active one stays pressed until Esc (in the viewport) or
/// another button. Each command drives the viewports it knows:
///  - Nuevo: snap mode; LMB on a reference line or a bar enters a node (ignored without a snap, as in the original, and ignored
///    when a node already lies there); a typed digit opens the "Calculadora". The node labels "N<id>" are shown.
///  - Borrar: node-only selection; LMB marks a node, RMB deletes the marked nodes at once (no confirmation).
///  - Mover: LMB on a node, then the node follows the pointer with the bars that reach it, LMB sets the new position (like
///    Nuevo), RMB cancels. The command stays active for another node. The second step has no measured text or labels (provisional).
class NodeTool : public QObject {
    Q_OBJECT
public:
    enum class Mode { None, New, Delete, Move };

    explicit NodeTool(QObject* parent = nullptr);

    QAction* newAction() const { return m_new; }
    QAction* deleteAction() const { return m_delete; }
    QAction* moveAction() const { return m_move; }
    QActionGroup* group() const { return m_group; }

    /// The open project and its edit history (not owned); nullptr when no project is open. Deactivates any command.
    void setProject(cc::core::Project* project, cc::core::UndoStack* undo);
    /// Viewport driven by the commands (not owned).
    void addViewport(Viewport3D* viewport);

    Mode mode() const { return m_mode; }
    /// True during the second step of "Mover" (a node is in motion).
    bool movePending() const { return m_mode == Mode::Move && m_moveNode.has_value(); }
    std::optional<int> movingNode() const { return m_moveNode; }
    /// The open "Calculadora", if any.
    NodeCalculatorDialog* calculator() const;
    /// Leaves the active command (as Esc does).
    void deactivate();
    /// To be called after every change of the history (push, undo, redo): enables / disables Borrar and Mover according to the
    /// nodes that exist, refreshes the viewports and drops a move whose node is gone.
    void structureChanged();

    /// Status bar text of a command, with the separators of the original: "Nudos - Nuevo |  - LMB: Introduzca un punto."
    static QString statusText(Mode mode, bool moveSecondStep = false);
    /// What the bar shows when no command is active.
    static QString defaultStatusText();

signals:
    /// New text of the status bar.
    void statusChanged(const QString& text);
    /// A command is about to become active: other viewport tools ("Asignar elementos a etiquetas") must stop.
    void aboutToActivate();
    /// The active command changed.
    void modeChanged(cc::app::NodeTool::Mode mode);

private:
    void setMode(Mode mode);
    void configureViewports();
    void updateEnabled();
    void onPointEntered(const cc::core::Vec3& world, const cc::core::SnapResult& snap);
    void onSelectionChanged(Viewport3D* viewport);
    void onActOnSelection(Viewport3D* viewport);
    void onDigitTyped(Viewport3D* viewport, QChar digit);
    void place(const cc::core::Point3& position);
    void beginMove(int nodeId);
    void endMove();
    void openCalculator(Viewport3D* viewport, QChar first);
    void closeCalculator();
    void pruneViewports();

    QAction* m_new = nullptr;
    QAction* m_delete = nullptr;
    QAction* m_move = nullptr;
    QActionGroup* m_group = nullptr;
    cc::core::Project* m_project = nullptr;
    cc::core::UndoStack* m_undo = nullptr;
    QList<QPointer<Viewport3D>> m_viewports;
    Mode m_mode = Mode::None;
    std::optional<int> m_moveNode;
    QPointer<NodeCalculatorDialog> m_calculator;
};

}  // namespace cc::app

Q_DECLARE_METATYPE(cc::app::NodeTool::Mode)
