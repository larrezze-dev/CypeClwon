#pragma once
#include <QList>
#include <QObject>
#include <QPointer>
#include <QString>
#include <functional>
#include <vector>

#include "core/camera.h"
#include "core/project.h"
#include "core/selection.h"
#include "core/shell_edit.h"
#include "core/snap.h"
#include "core/undo_stack.h"

class QAction;
class QActionGroup;
class QDialog;
class QWidget;

namespace cc::app {

class Viewport3D;

/// Geometria > Laminas (T-P5-09, GUI part). The edits are the undoable commands of core/shell_edit.h; behaviour sources:
/// docs/research/ui/laminas.md. Like PieceTool the commands live in an exclusive-optional QActionGroup (the active one stays
/// pressed until Esc, RMB on nothing or another button) and every other viewport tool is stopped through aboutToActivate().
///  - Nueva (#64705 / #64706): LMB enters the points of the contour (snapped to nodes, bars and guide lines; a point without a
///    node gets one when the shell is created), RMB closes it. More than 4 points are cut into shells of 3 or 4 nodes
///    (decomposeContour). A contour that is not valid shows #65121 and nothing is created.
///  - Borrar (#64710), Unir (#64724), Invertir eje 'z' (#64715): LMB selects shells (Ctrl adds, Shift removes, window), RMB acts.
///  - Mover (#64708 / #64709): step 1 click a shell, step 2 select its nodes + RMB, step 3 two points (vector and magnitude).
///  - Dividir (#64725 / #64726): select shells + RMB, then the two points of the dividing segment. PROVISIONAL order: the text
///    only says "dos puntos ... y botón derecho" (laminas.md section 1).
///  - Definir eje 'x' (#64716 / #64717): click a shell, then the two nodes of the edge. PROVISIONAL: the original clicks the edge.
///  - Ejes locales (#392577): a toggle that draws the local axes of the shells in the 3D windows.
///  - Seccion (#65122, laminas.md section 5): LMB selects shells, RMB opens "Sección de la lámina" (thickness, local-Z ballast
///    modulus and its behaviour) and applies it to all of them as one undo step.
///  - Disposicion and Material: the buttons exist but their dialogs are pending (laminas.md section 4: the fields are not
///    known; the "Describir material" dialog of the bars is for bars and its groups differ from #94482-94486) and only
///    emit notAvailable(). TODO(TK-shell-dialogs): link them once the fields are known.
class ShellTool : public QObject {
    Q_OBJECT
public:
    enum class Mode { None, New, Delete, Move, Join, Divide, InvertZ, DefineX, Section };

    explicit ShellTool(QObject* parent = nullptr);

    QAction* newAction() const { return m_new; }
    QAction* deleteAction() const { return m_delete; }
    QAction* moveAction() const { return m_move; }
    QAction* joinAction() const { return m_join; }
    QAction* divideAction() const { return m_divide; }
    QAction* invertZAction() const { return m_invertZ; }
    QAction* defineXAction() const { return m_defineX; }
    QAction* localAxesAction() const { return m_localAxes; }
    QAction* layoutAction() const { return m_layout; }
    QAction* sectionAction() const { return m_section; }
    QAction* materialAction() const { return m_material; }
    QActionGroup* actionGroup() const { return m_actions; }

    void setProject(cc::core::Project* project, cc::core::UndoStack* undo);
    void addViewport(Viewport3D* viewport);

    Mode mode() const { return m_mode; }
    /// Step reached inside the current command (0 = first).
    int step() const { return m_step; }
    /// Points entered so far (Nueva, Mover step 3, Dividir step 2).
    const std::vector<cc::core::Vec3>& points() const { return m_points; }
    void deactivate();
    /// Call after every change of the history (push, undo, redo).
    void structureChanged();

    /// Widget the dialogs are parented to (the main window).
    void setDialogParent(QWidget* parent) { m_dialogParent = parent; }
    /// Runs a modal dialog; returns QDialog::Accepted / Rejected. The default is exec(); tests replace it so that nothing blocks.
    /// (Used by the Seccion dialog; Disposicion / Material will plug in the same way.)
    void setDialogRunner(std::function<int(QDialog&)> runner) { m_runner = std::move(runner); }

    /// Status bar text. PROVISIONAL: assembled from the generic texts of entopane (laminas.md has no measured bar text).
    static QString statusText(Mode mode, int step = 0, int selected = 0);

    // Direct entry points (what the modes call once the gesture is complete); each is one undo step. True on success.
    bool createShell(const std::vector<cc::core::Vec3>& contour);
    bool deleteShells(const std::vector<int>& shellIds);
    bool joinShells(const std::vector<int>& shellIds);
    bool invertZ(const std::vector<int>& shellIds);
    /// Opens "Sección de la lámina" (initial values of the first shell) and applies it to every given shell; false if cancelled.
    bool editSection(const std::vector<int>& shellIds);

signals:
    void statusChanged(const QString& text);
    /// A command is about to become active: other viewport tools must stop.
    void aboutToActivate();
    void modeChanged(cc::app::ShellTool::Mode mode);
    /// A message the main window shows to the user (the shellErrorMessage texts).
    void messageRequested(const QString& text);
    /// The button has no dialog yet (pending): the main window says so in the status bar.
    void notAvailable();

private:
    void setMode(Mode mode);
    void configureViewports();
    void updateEnabled();
    void pruneViewports();
    int run(QDialog& dialog);
    void onSelectionChanged(Viewport3D* viewport);
    void onActOnSelection(Viewport3D* viewport);
    void onRightClicked();
    void onPointEntered(const cc::core::Vec3& world, const cc::core::SnapResult& snap);
    void resetSteps();
    void pickShell(int shellId);
    void pickNode(int nodeId);
    bool report(cc::core::ShellError error);
    /// What the viewport picks in the current step.
    bool snapping() const;
    cc::core::ElementKind pickKind() const;
    /// Whether the step takes a click on one element that is consumed at once (instead of a selection that RMB acts on).
    bool consumesPick() const;

    QAction* m_new = nullptr;
    QAction* m_delete = nullptr;
    QAction* m_move = nullptr;
    QAction* m_join = nullptr;
    QAction* m_divide = nullptr;
    QAction* m_invertZ = nullptr;
    QAction* m_defineX = nullptr;
    QAction* m_localAxes = nullptr;
    QAction* m_layout = nullptr;
    QAction* m_section = nullptr;
    QAction* m_material = nullptr;
    QActionGroup* m_actions = nullptr;
    cc::core::Project* m_project = nullptr;
    cc::core::UndoStack* m_undo = nullptr;
    QList<QPointer<Viewport3D>> m_viewports;
    QPointer<QWidget> m_dialogParent;
    std::function<int(QDialog&)> m_runner;
    Mode m_mode = Mode::None;
    int m_step = 0;
    std::vector<cc::core::Vec3> m_points;
    int m_shellId = 0;                ///< Mover / Definir eje x: the shell chosen in step 1
    std::vector<int> m_shellIds;      ///< Dividir: the selection kept between steps
    std::vector<int> m_nodeIds;       ///< Mover: nodes chosen in step 2; Definir eje x: the first node of the edge
    bool m_internalSelection = false;
};

}  // namespace cc::app

Q_DECLARE_METATYPE(cc::app::ShellTool::Mode)
