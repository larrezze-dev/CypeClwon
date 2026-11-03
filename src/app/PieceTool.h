#pragma once
#include <QList>
#include <QObject>
#include <QPointer>
#include <QString>
#include <functional>
#include <vector>

#include "core/piece_edit.h"
#include "core/selection.h"
#include "core/project.h"
#include "core/undo_stack.h"

class QAction;
class QActionGroup;

namespace cc::app {

class Viewport3D;

/// Geometria > Barras, second row of commands (T-P5-04, GUI part): Pieza, Agrupar, Desagrupar, Mover extremo and Invertir
/// eje 'x'. The edits are the undoable commands of core/piece_edit.h; behaviour sources: docs/research/ui/pieza.md. Like
/// BarTool the five actions live in an exclusive-optional QActionGroup (the active one stays pressed until Esc, RMB on
/// nothing or another button) and every other viewport tool is stopped through aboutToActivate().
///  - Pieza (#64624 / #64625): LMB on the start node, LMB on the end node; the piece is created and the command goes on.
///    RMB goes back to the first step. A refused operation reports its message (#64226 / #64227 / #64229).
///  - Agrupar (#64626) / Desagrupar (#64627): LMB selects bars (Ctrl adds, Shift removes, window), RMB asks (#65100 /
///    #65101) and acts on the pieces of the selected bars.
///  - Mover extremo (#64645): PROVISIONAL flow (pieza.md section 4): LMB on a bar, LMB on one of its two nodes (the end
///    that moves), LMB on the destination node. RMB goes back to the first step.
///  - Invertir eje 'x' (#64640): a mode; every LMB on a bar inverts it at once, no confirmation.
class PieceTool : public QObject {
    Q_OBJECT
public:
    enum class Mode { None, Piece, Group, Ungroup, MoveEnd, InvertAxis };

    explicit PieceTool(QObject* parent = nullptr);

    QAction* pieceAction() const { return m_piece; }
    QAction* groupAction() const { return m_group; }
    QAction* ungroupAction() const { return m_ungroup; }
    QAction* moveEndAction() const { return m_moveEnd; }
    QAction* invertAction() const { return m_invert; }
    QActionGroup* actionGroup() const { return m_actions; }

    void setProject(cc::core::Project* project, cc::core::UndoStack* undo);
    void addViewport(Viewport3D* viewport);

    Mode mode() const { return m_mode; }
    /// Pieza: nodes chosen so far (0 or 1).
    const std::vector<int>& chain() const { return m_chain; }
    /// Mover extremo: step reached (0 = bar, 1 = end of the bar, 2 = destination node).
    int moveStep() const { return m_moveStep; }
    void deactivate();
    /// Call after every change of the history (push, undo, redo).
    void structureChanged();

    /// Asks a yes / no question (#65100 / #65101). The main window installs a message box; without one the answer is yes.
    void setConfirmer(std::function<bool(const QString&)> confirmer) { m_confirmer = std::move(confirmer); }

    /// Whether Pieza asks "¿Desea crear la pieza?" (#65126) before creating. NOT VERIFIED in the original: off by default.
    void setConfirmPiece(bool on) { m_confirmPiece = on; }

    /// Status bar text. `step` = chosen nodes (Pieza) or step reached (Mover extremo); `selected` = bars selected
    /// (Agrupar, Desagrupar). PROVISIONAL: assembled from the generic texts of entopane, not measured.
    static QString statusText(Mode mode, int step = 0, int selected = 0);

signals:
    void statusChanged(const QString& text);
    /// A command is about to become active: other viewport tools must stop.
    void aboutToActivate();
    void modeChanged(cc::app::PieceTool::Mode mode);
    /// A message the main window shows to the user (the PieceError texts).
    void messageRequested(const QString& text);

private:
    void setMode(Mode mode);
    void configureViewports();
    void updateEnabled();
    void pruneViewports();
    void onSelectionChanged(Viewport3D* viewport);
    void onActOnSelection(Viewport3D* viewport);
    void pickNode(int nodeId);
    void pickBar(int barId);
    void resetSteps();
    void report(cc::core::PieceError error);
    bool confirm(const QString& question) const;
    cc::core::ElementKind pickKind() const;

    QAction* m_piece = nullptr;
    QAction* m_group = nullptr;
    QAction* m_ungroup = nullptr;
    QAction* m_moveEnd = nullptr;
    QAction* m_invert = nullptr;
    QActionGroup* m_actions = nullptr;
    cc::core::Project* m_project = nullptr;
    cc::core::UndoStack* m_undo = nullptr;
    QList<QPointer<Viewport3D>> m_viewports;
    Mode m_mode = Mode::None;
    std::vector<int> m_chain;
    int m_moveStep = 0;
    int m_moveBar = 0;
    cc::core::BarEnd m_moveWhich = cc::core::BarEnd::Start;
    bool m_internalSelection = false;
    bool m_confirmPiece = false;
    std::function<bool(const QString&)> m_confirmer;
};

}  // namespace cc::app

Q_DECLARE_METATYPE(cc::app::PieceTool::Mode)
