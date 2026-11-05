#pragma once
#include <QDialog>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QString>
#include <functional>
#include <vector>

#include "core/camera.h"
#include "core/project.h"
#include "core/selection.h"
#include "core/snap.h"
#include "core/tools_edit.h"
#include "core/undo_stack.h"

class QAction;
class QActionGroup;
class QDoubleSpinBox;
class QMenu;
class QSpinBox;
class QWidget;

namespace cc::app {

class Viewport3D;

/// Small modal dialog with one number: "Ángulo de giro" (degrees) or "Número de copias" (integer). Tests find the spin box with
/// findChild and set it before the runner returns.
class ToolValueDialog : public QDialog {
    Q_OBJECT
public:
    enum class Kind { Angle, Copies };
    explicit ToolValueDialog(Kind kind, QWidget* parent = nullptr);
    double angleDegrees() const;
    int copies() const;

private:
    Kind m_kind;
    QDoubleSpinBox* m_angle = nullptr;
    QSpinBox* m_copies = nullptr;
};

/// Result of "Reparar": the message #399478 / #399479 and the list of incidences found (herramientas.md section 3).
class RepairResultDialog : public QDialog {
    Q_OBJECT
public:
    RepairResultDialog(const QString& result, const std::vector<cc::core::RepairIssue>& issues, QWidget* parent = nullptr);
};

/// Geometria > Herramientas (T-P5-10, GUI part). The edits are the undoable commands of core/tools_edit.h; behaviour sources:
/// docs/research/ui/herramientas.md (help texts #64803-64822: select the elements, RMB, then the points). Layout of the group
/// (orig_geometria_ribbon.png): "Generar" (drop-down) large on the left, then two rows of icons, then "Reparar" large.
/// The commands live in an exclusive-optional QActionGroup like PieceTool; every other viewport tool is stopped through
/// aboutToActivate(). Common flow of the seven transform modes: LMB selects nodes, bars and shells (Ctrl adds, Shift removes,
/// window), RMB moves on to the points (snapped like Nudos > Nuevo); RMB in the points step clears them, then goes back to the
/// selection. Points needed: Mover 2 (vector), Copiar 2 (+ "Número de copias"), Girar 2 (axis, + "Ángulo de giro"),
/// Girar en el plano xy 2 and its three-point variant 4 (origin + 3), Simetría en el plano xy 2, Simetría respecto de un plano 3.
/// PROVISIONAL (the original does not say): "Girar en el plano xy" turns about the vertical through the first point by the
/// angle of the vector first -> second point measured from global X; the three-point variant turns about the vertical through
/// the origin by the angle at the second of the three points, from (second -> first) to (second -> third); the number of
/// copies and the angle of the axis rotation are asked in small dialogs.
/// "Generar" entries only emit notAvailable() (dialogs and generators arrive in another pass). The two unidentified icons
/// of the group ("ABC" and the magnifier) are not created.
class TransformTool : public QObject {
    Q_OBJECT
public:
    enum class Mode { None, Move, Copy, RotateAxis, RotateXY, RotateXY3, MirrorXY, MirrorPlane };

    explicit TransformTool(QObject* parent = nullptr);

    QAction* moveAction() const { return m_move; }
    QAction* copyAction() const { return m_copy; }
    QAction* rotateAxisAction() const { return m_rotateAxis; }
    QAction* rotateXYAction() const { return m_rotateXY; }
    QAction* rotateXY3Action() const { return m_rotateXY3; }
    QAction* mirrorXYAction() const { return m_mirrorXY; }
    QAction* mirrorPlaneAction() const { return m_mirrorPlane; }
    QAction* repairAction() const { return m_repair; }
    QMenu* generateMenu() const { return m_generateMenu; }
    const QList<QAction*>& generateActions() const { return m_generateActions; }
    QActionGroup* actionGroup() const { return m_actions; }

    void setProject(cc::core::Project* project, cc::core::UndoStack* undo);
    void addViewport(Viewport3D* viewport);

    Mode mode() const { return m_mode; }
    /// 0 = selecting the elements, 1 = entering the points.
    int step() const { return m_step; }
    const std::vector<cc::core::Vec3>& points() const { return m_points; }
    void deactivate();
    /// Call after every change of the history (push, undo, redo).
    void structureChanged();

    void setDialogParent(QWidget* parent) { m_dialogParent = parent; }
    /// Runs a modal dialog; returns QDialog::Accepted / Rejected. The default is exec(); tests replace it so that nothing blocks.
    void setDialogRunner(std::function<int(QDialog&)> runner) { m_runner = std::move(runner); }
    /// Asks a yes / no question (#399477 of Reparar). The main window installs a message box; without one the answer is yes.
    void setConfirmer(std::function<bool(const QString&)> confirmer) { m_confirmer = std::move(confirmer); }

    /// Number of points a mode needs in its second step.
    static int pointsNeeded(Mode mode);
    static QString statusText(Mode mode, int step = 0, int selected = 0);

    /// Direct entry points (what the modes call once the gesture is complete); each is one undo step. True on success.
    bool apply(Mode mode, const cc::core::ToolSelection& selection, const std::vector<cc::core::Vec3>& points);
    /// "Reparar": confirmation, repair, result dialog. True when the model was modified.
    bool repair();

signals:
    void statusChanged(const QString& text);
    /// A command is about to become active: other viewport tools must stop.
    void aboutToActivate();
    void modeChanged(cc::app::TransformTool::Mode mode);
    /// A message the main window shows to the user (the toolErrorMessage texts).
    void messageRequested(const QString& text);
    /// The entry has no implementation yet (the "Generar" entries): the main window says so in the status bar.
    void notAvailable();

private:
    void setMode(Mode mode);
    void configureViewports();
    void updateEnabled();
    void pruneViewports();
    void onSelectionChanged(Viewport3D* viewport);
    void onActOnSelection(Viewport3D* viewport);
    void onRightClicked();
    void onPointEntered(const cc::core::Vec3& world, const cc::core::SnapResult& snap);
    void resetSteps();
    int run(QDialog& dialog);

    QAction* m_move = nullptr;
    QAction* m_copy = nullptr;
    QAction* m_rotateAxis = nullptr;
    QAction* m_rotateXY = nullptr;
    QAction* m_rotateXY3 = nullptr;
    QAction* m_mirrorXY = nullptr;
    QAction* m_mirrorPlane = nullptr;
    QAction* m_repair = nullptr;
    QMenu* m_generateMenu = nullptr;
    QList<QAction*> m_generateActions;
    QActionGroup* m_actions = nullptr;
    cc::core::Project* m_project = nullptr;
    cc::core::UndoStack* m_undo = nullptr;
    QList<QPointer<Viewport3D>> m_viewports;
    QPointer<QWidget> m_dialogParent;
    std::function<int(QDialog&)> m_runner;
    std::function<bool(const QString&)> m_confirmer;
    Mode m_mode = Mode::None;
    int m_step = 0;
    std::vector<cc::core::Vec3> m_points;
    cc::core::ToolSelection m_selection;
    bool m_internalSelection = false;
};

}  // namespace cc::app

Q_DECLARE_METATYPE(cc::app::TransformTool::Mode)
