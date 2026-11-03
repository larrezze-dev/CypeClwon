#pragma once
#include <QDialog>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QString>
#include <vector>

#include "core/project.h"
#include "core/snap.h"
#include "core/undo_stack.h"
#include "core/work_planes.h"

class QAction;
class QActionGroup;
class QComboBox;
class QDialogButtonBox;
class QDoubleSpinBox;

namespace cc::app {

class Viewport3D;

/// Entry of the "Pilar" command: plan position and the first and last level of the column (barras.md section 1, #64850).
/// PROVISIONAL: the original's way of choosing the levels was not measured (barras.md section 6, item 3); this is a plain
/// dialog with the same two pieces of data. The levels are those of "Planos > Niveles", in ascending order of elevation.
class ColumnDialog : public QDialog {
    Q_OBJECT
public:
    explicit ColumnDialog(const std::vector<cc::core::Level>& sortedLevels, QWidget* parent = nullptr);
    void setPosition(double x, double y);
    QDoubleSpinBox* xSpin() const { return m_x; }
    QDoubleSpinBox* ySpin() const { return m_y; }
    QComboBox* startCombo() const { return m_start; }
    QComboBox* endCombo() const { return m_end; }
    /// Heights of every level from the first to the last chosen one (the column has a piece between each pair).
    std::vector<double> chosenElevations() const;
    /// The OK button is enabled only when the column starts and ends at two different levels (#64844).
    bool canAccept() const;
    void accept() override;

signals:
    void columnConfirmed(double x, double y, const std::vector<double>& elevations);

private:
    void updateOk();
    std::vector<cc::core::Level> m_levels;
    QDoubleSpinBox* m_x = nullptr;
    QDoubleSpinBox* m_y = nullptr;
    QComboBox* m_start = nullptr;
    QComboBox* m_end = nullptr;
    QDialogButtonBox* m_buttons = nullptr;
};

/// Geometria > Barras: the three commands "Barra", "Pilar" and "Viga" (T-P5-03, GUI part; the edits are the undoable
/// commands of core/bar_edit.h). Behaviour sources: docs/research/ui/barras.md. Like NodeTool, the three actions live in an
/// exclusive-optional QActionGroup: the active one stays pressed until Esc or another button.
///  - Barra: LMB on a node = start node, LMB on another = end node (a bar is added); the command goes on for another bar and
///    RMB goes back to the first step.
///  - Viga: LMB on nodes in a chain, RMB ends it and creates one bar per pair (a continuous beam, one undoable step); a
///    chain of fewer than two nodes is dropped.
///  - Pilar: needs at least two levels (#65140, otherwise the message is requested and the command does not start). A click on a
///    snap point or a typed digit opens the ColumnDialog (plan position and levels).
/// The dynamic entry panel (TK-040) is NOT implemented: points are fixed by clicking on existing nodes.
class BarTool : public QObject {
    Q_OBJECT
public:
    enum class Mode { None, Bar, Column, Beam };

    explicit BarTool(QObject* parent = nullptr);

    QAction* barAction() const { return m_bar; }
    QAction* columnAction() const { return m_column; }
    QAction* beamAction() const { return m_beam; }
    QActionGroup* group() const { return m_group; }

    void setProject(cc::core::Project* project, cc::core::UndoStack* undo);
    void addViewport(Viewport3D* viewport);

    Mode mode() const { return m_mode; }
    /// Nodes chosen so far in Barra (0 or 1) or Viga (any number).
    const std::vector<int>& chain() const { return m_chain; }
    ColumnDialog* columnDialog() const { return m_columnDialog.data(); }
    void deactivate();
    /// Call after every change of the history (push, undo, redo): refreshes the viewports and drops chosen nodes that are gone.
    void structureChanged();

    /// Status bar text, same separators as NodeTool::statusText. `chosen` = number of nodes already chosen (Barra, Viga).
    /// PROVISIONAL: assembled from the generic texts of entopane (#62066, #62068, #62082, #62087), not measured.
    static QString statusText(Mode mode, int chosen = 0);

signals:
    void statusChanged(const QString& text);
    /// A command is about to become active: other viewport tools must stop.
    void aboutToActivate();
    void modeChanged(cc::app::BarTool::Mode mode);
    /// A message the main window shows to the user (#65140 for Pilar without levels).
    void messageRequested(const QString& text);

private:
    void setMode(Mode mode);
    void configureViewports();
    void updateEnabled();
    void onSelectionChanged(Viewport3D* viewport);
    void onActOnSelection();
    void onPointEntered(const cc::core::Vec3& world, const cc::core::SnapResult& snap);
    void onDigitTyped(Viewport3D* viewport, QChar digit);
    void pickNode(int nodeId);
    void resetChain();
    void openColumnDialog(Viewport3D* viewport, double x, double y, int firstDigit);
    bool hasTwoLevels() const;
    void pruneViewports();

    QAction* m_bar = nullptr;
    QAction* m_column = nullptr;
    QAction* m_beam = nullptr;
    QActionGroup* m_group = nullptr;
    cc::core::Project* m_project = nullptr;
    cc::core::UndoStack* m_undo = nullptr;
    QList<QPointer<Viewport3D>> m_viewports;
    Mode m_mode = Mode::None;
    std::vector<int> m_chain;
    bool m_internalSelection = false;
    QPointer<ColumnDialog> m_columnDialog;
};

}  // namespace cc::app

Q_DECLARE_METATYPE(cc::app::BarTool::Mode)
