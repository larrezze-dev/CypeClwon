#pragma once
#include <QList>
#include <QObject>
#include <QPointer>
#include <QPointF>
#include <functional>
#include <memory>
#include <optional>
#include <set>

#include "CalculationDialogs.h"
#include "core/fem/result_display.h"

class QAction;
class QActionGroup;
class QDialog;
class QPainter;
class QWidget;

namespace cc::core {
struct Project;
class UndoStack;
}  // namespace cc::core

namespace cc::app {

class ResultsPanel;
class Viewport3D;

/// Cálculo > Tensión / Deformación (T-P8-06): «Combinaciones no lineales», «Calcular» (dialog «Cálculo», analysis in a worker thread, «Informe
/// final de cálculo») and the result modes «Desplazamientos», «Reacciones», «Esfuerzos» with their floating panels and their drawing in the 3D
/// windows. Behaviour: docs/research/resultados_calculo.md, docs/research/ui/resultados_paneles.md, docs/research/ui/calculo_resultados.md.
///  - The result buttons are inactive until a calculation succeeds, and become inactive again as soon as the work changes (any new, undone or
///    redone edit) [V rp_33_after_delete.png]; saving does not count as a change.
///  - Desplazamientos: a click on a bar shows / hides the balloons of its two ends (bar end values, LoadCaseResult::barDisplacements) [V #64071].
///  - Reacciones: a click on a supported node shows / hides its balloon «N1 (0, 0, 0)» + Rx..Mz [V].
///  - Esfuerzos: laws of every bar or of the selected ones (click), deformed shape, «Consultar valores» with the right button on a selected bar [V].
class CalculationTool : public QObject {
    Q_OBJECT
public:
    enum class Mode { None, Displacements, Reactions, Forces };
    explicit CalculationTool(QObject* parent = nullptr);
    ~CalculationTool() override;

    QAction* nonlinearAction() const { return m_nonlinearAction; }
    QAction* calculateAction() const { return m_calculateAction; }
    QAction* displacementsAction() const { return m_displacementsAction; }
    QAction* reactionsAction() const { return m_reactionsAction; }
    QAction* forcesAction() const { return m_forcesAction; }

    void setDialogParent(QWidget* parent) { m_dialogParent = parent; }
    void setProject(cc::core::Project* project, cc::core::UndoStack* undo);
    void addViewport(Viewport3D* viewport);
    /// Called after every change of the edit history: a new / undone / redone edit invalidates the results.
    void structureChanged();
    void deactivate();
    Mode mode() const { return m_mode; }
    static QString statusText(Mode mode);

    /// The results of the last successful calculation (null when there are none or they were invalidated).
    const cc::fem::ProjectAnalysis* analysis() const { return m_analysis.get(); }
    bool calculated() const { return m_analysis && m_analysis->calculated; }
    ResultsPanel* panel() const;

    /// «Calcular»: the dialog, the analysis and the report. False when cancelled.
    bool calculate();
    /// «Combinaciones no lineales»: the dialog, written to the project as one undoable step on Aceptar.
    bool editNonlinearCombinations();

    // --- hooks for the tests ---
    void setDialogRunner(std::function<int(QDialog&)> runner) { m_runner = std::move(runner); }
    /// Runs the analysis on the calling thread (no busy window).
    void setSynchronous(bool on) { m_synchronous = on; }
    const CalculationChoices& choices() const { return m_choices; }
    const std::vector<cc::fem::ReportEntry>& lastReport() const { return m_lastReport; }
    const std::set<int>& shownBars() const { return m_shownBars; }
    const std::set<int>& shownNodes() const { return m_shownNodes; }
    const std::set<int>& selectedBars() const { return m_selectedBars; }
    /// What a click / right click at a widget position of the viewport does in the current mode.
    void clickAt(Viewport3D* viewport, const QPointF& pos);
    void rightClickAt(Viewport3D* viewport, const QPointF& pos);
    /// Value queried by «Consultar valores»: bar id and x (m).
    std::optional<std::pair<int, double>> query() const { return m_query; }
    /// Draws the results of the current mode (the overlay of the viewports).
    void paint(QPainter& painter, const Viewport3D& viewport) const;

signals:
    void statusChanged(const QString& text);
    void aboutToActivate();
    void modeChanged(Mode mode);
    void calculationFinished();

private:
    int run(QDialog& dialog);
    void setMode(Mode mode);
    void updateEnabled();
    void invalidate();
    void configureViewports();
    void showPanel();
    void closePanel();
    void repaint();
    std::optional<cc::fem::ResultRef> currentResult() const;
    void markHistory();

    cc::core::Project* m_project = nullptr;
    cc::core::UndoStack* m_undo = nullptr;
    QWidget* m_dialogParent = nullptr;
    QList<QPointer<Viewport3D>> m_viewports;
    QAction* m_nonlinearAction = nullptr;
    QAction* m_calculateAction = nullptr;
    QAction* m_displacementsAction = nullptr;
    QAction* m_reactionsAction = nullptr;
    QAction* m_forcesAction = nullptr;
    QActionGroup* m_group = nullptr;
    Mode m_mode = Mode::None;
    bool m_configured = false;
    std::unique_ptr<cc::fem::ProjectAnalysis> m_analysis;
    cc::fem::SolverOptions m_solverOptions;
    std::size_t m_undoIndex = 0, m_undoCount = 0;
    CalculationChoices m_choices;
    std::vector<cc::fem::ReportEntry> m_lastReport;
    std::function<int(QDialog&)> m_runner;
    bool m_synchronous = false;
    QPointer<ResultsPanel> m_panel;
    std::set<int> m_shownBars, m_shownNodes, m_selectedBars;
    std::optional<std::pair<int, double>> m_query;
};

}  // namespace cc::app
