#pragma once
#include <QDialog>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QString>
#include <functional>
#include <vector>

#include "core/generate_edit.h"
#include "core/project.h"
#include "core/undo_stack.h"

class QAction;
class QMenu;
class QWidget;

namespace cc::app {

class Viewport3D;

/// Geometria > Herramientas > Generar (T-P5-10). Measured in the original (herramientas.md 6.2): the drop-down has THREE entries, Planos,
/// Nudos y barras and Malla de tetraedros. The generators are the undoable commands of core/generate_edit.h.
///  - Planos: LMB on two reference lines (bars) that define the plane, then the dialog "Generación de planos"; the command stays active
///    for another plane (RMB: "Comenzar otra vez").
///  - Nudos y barras: LMB on existing nodes: start and end of segment 1, then only the end of segment 2 and of segment 3 (a pick that is
///    collinear / coplanar with the previous ones is ignored, like the original keeps the dialog of two segments); RMB opens the dialog,
///    whose title depends on the number of segments (1 / 2 / 3).
///  - Malla de tetraedros: nodes start and end of segment 1 and end of segment 2; RMB opens the dialog (PROVISIONAL: the original may open
///    it after the third pick).
/// Every other viewport tool is stopped through aboutToActivate(); Esc stops the command.
class GenerateTool : public QObject {
    Q_OBJECT
public:
    enum class Mode { None, Planes, NodeMesh, Tetrahedra };

    explicit GenerateTool(QObject* parent = nullptr);

    QMenu* menu() const { return m_menu; }
    QAction* planesAction() const { return m_planes; }
    QAction* nodeMeshAction() const { return m_nodeMesh; }
    QAction* tetrahedraAction() const { return m_tetrahedra; }

    void setProject(cc::core::Project* project, cc::core::UndoStack* undo);
    void addViewport(Viewport3D* viewport);

    Mode mode() const { return m_mode; }
    /// Elements picked so far: bars (Planos) or nodes (the other two).
    const std::vector<int>& picks() const { return m_picks; }
    void deactivate();
    void structureChanged();

    void setDialogParent(QWidget* parent) { m_dialogParent = parent; }
    /// Runs a modal dialog; the default is exec(), tests replace it.
    void setDialogRunner(std::function<int(QDialog&)> runner) { m_runner = std::move(runner); }
    /// Asks the yes / no question of #63970 (the planes to generate are grouped already). Without one the answer is yes.
    void setConfirmer(std::function<bool(const QString&)> confirmer) { m_confirmer = std::move(confirmer); }

    static QString statusText(Mode mode, int picks = 0);

    // Entry points that open the dialog for the current picks; each success is one undo step. True on success.
    bool runPlanes();
    bool runNodeMesh();
    bool runTetrahedra();

signals:
    void statusChanged(const QString& text);
    void aboutToActivate();
    void modeChanged(cc::app::GenerateTool::Mode mode);
    void messageRequested(const QString& text);

private:
    void start(Mode mode);
    void configureViewports();
    void resetPicks();
    void updateEnabled();
    void pruneViewports();
    void onSelectionChanged(Viewport3D* viewport);
    void onActOnSelection();
    void onPick(int id);
    bool report(cc::core::GenerateError error);
    int run(QDialog& dialog);

    QMenu* m_menu = nullptr;
    QAction* m_planes = nullptr;
    QAction* m_nodeMesh = nullptr;
    QAction* m_tetrahedra = nullptr;
    cc::core::Project* m_project = nullptr;
    cc::core::UndoStack* m_undo = nullptr;
    QList<QPointer<Viewport3D>> m_viewports;
    QPointer<QWidget> m_dialogParent;
    std::function<int(QDialog&)> m_runner;
    std::function<bool(const QString&)> m_confirmer;
    Mode m_mode = Mode::None;
    std::vector<int> m_picks;
    bool m_internalSelection = false;
};

}  // namespace cc::app

Q_DECLARE_METATYPE(cc::app::GenerateTool::Mode)
