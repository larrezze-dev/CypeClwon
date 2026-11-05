#pragma once
#include <QDialog>
#include <QFrame>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QString>
#include <functional>
#include <optional>
#include <vector>

#include "NodeCalculatorDialog.h"
#include "RangedEdit.h"
#include "core/camera.h"
#include "core/project.h"
#include "core/selection.h"
#include "core/snap.h"
#include "core/tools_edit.h"
#include "core/undo_stack.h"

class QAction;
class QActionGroup;
class QToolButton;
class QWidget;

namespace cc::app {

class Viewport3D;

/// Inline editor of the original (herramientas.md 6.3): "Ángulo de giro [0.000] grados" with a green button (accept) and a red one
/// (cancel), shown next to the cursor once the two points of the axis are known. Default 0.000, range -360.000 to 360.000 degrees; a
/// value outside shows the warning "El valor de 'Ángulo de giro' debe estar entre -360.000 grados y 360.000 grados.". Intro accepts, Esc cancels.
class InlineAngleEditor : public QFrame {
    Q_OBJECT
public:
    explicit InlineAngleEditor(QWidget* parent = nullptr);
    RangedEdit* edit() const { return m_edit; }
    QToolButton* acceptButton() const { return m_accept; }
    QToolButton* cancelButton() const { return m_cancel; }
    /// Reads the field; true (and acceptedValue is emitted) when it is valid.
    bool tryAccept();

signals:
    void acceptedValue(double degrees);
    void cancelled();

protected:
    void keyPressEvent(QKeyEvent* event) override;

private:
    RangedEdit* m_edit = nullptr;
    QToolButton* m_accept = nullptr;
    QToolButton* m_cancel = nullptr;
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
/// selection. Points needed: Mover 2 (vector), Copiar 2, Girar 2 (axis, then the inline "Ángulo de giro"),
/// Girar en el plano xy 2 and its three-point variant 4 (origin + 3), Simetría en el plano xy 2, Simetría respecto de un plano 3.
/// PROVISIONAL (the original does not say): "Girar en el plano xy" turns about the vertical through the first point by the
/// angle of the vector first -> second point measured from global X; the three-point variant turns about the vertical through
/// the origin by the angle at the second of the three points, from (second -> first) to (second -> third).
/// Measured in the original (herramientas.md 6.3): there is no dialog for the number of copies (Copiar = selection, RMB, first point,
/// second point) and the angle of "Girar alrededor de un eje" is an inline editor (InlineAngleEditor). Typing a digit while a point is
/// expected opens the "Calculadora" (Coordenada X / Y / Z, NodeCalculatorDialog) whose Intro on Z gives the point. "Generar" lives in
/// GenerateTool and "Asignar referencia" / "Buscar" in ReferenceTool.
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
    /// `angleDegrees` is the angle of "Girar alrededor de un eje" (required for that mode; the inline editor supplies it).
    bool apply(Mode mode, const cc::core::ToolSelection& selection, const std::vector<cc::core::Vec3>& points,
               std::optional<double> angleDegrees = std::nullopt);
    /// The inline angle editor while it is open (nullptr otherwise) and the calculator window.
    InlineAngleEditor* angleEditor() const { return m_angleEditor.data(); }
    NodeCalculatorDialog* calculator() const { return m_calculator.data(); }
    /// Adds a point as if it had been clicked (the calculator uses it).
    void addPoint(const cc::core::Vec3& world);
    /// "Reparar": confirmation, repair, result dialog. True when the model was modified.
    bool repair();

signals:
    void statusChanged(const QString& text);
    /// A command is about to become active: other viewport tools must stop.
    void aboutToActivate();
    void modeChanged(cc::app::TransformTool::Mode mode);
    /// A message the main window shows to the user (the toolErrorMessage texts).
    void messageRequested(const QString& text);

private:
    void setMode(Mode mode);
    void configureViewports();
    void updateEnabled();
    void pruneViewports();
    void onSelectionChanged(Viewport3D* viewport);
    void onActOnSelection(Viewport3D* viewport);
    void onRightClicked();
    void onPointEntered(const cc::core::Vec3& world, const cc::core::SnapResult& snap);
    void onDigitTyped(Viewport3D* viewport, QChar digit);
    void askAngle(Viewport3D* viewport);
    void closeEditors();
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
    QPointer<InlineAngleEditor> m_angleEditor;
    QPointer<NodeCalculatorDialog> m_calculator;
    bool m_internalSelection = false;
};

}  // namespace cc::app

Q_DECLARE_METATYPE(cc::app::TransformTool::Mode)
