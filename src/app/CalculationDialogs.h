#pragma once
#include <array>
#include <functional>
#include <vector>

#include "CypeDialog.h"
#include "core/fem/project_analysis.h"
#include "core/nonlinear_combinations.h"

class QCheckBox;
class QRadioButton;
class QTableWidget;
class QTextBrowser;
class QToolButton;

namespace cc::core {
struct Project;
}

namespace cc::app {

/// Dialogs of «Cálculo > Tensión / Deformación» (T-P8-06). Sources: docs/research/resultados_calculo.md, docs/research/ui/calculo_resultados.md and
/// the screenshots cp_61_calcdlg.png (no non-linear elements), rr_e2_dlg.png (with a spring with «Sentido»), s_5.png (report), s_9.png / s_10.png
/// (non-linear combinations).

/// What the «Cálculo» dialog asks for.
struct CalculationChoices {
    bool linear = true;           ///< «Análisis lineal de tensión / deformación» (#377030)
    bool nonlinear = true;        ///< «Análisis no lineal de tensión / deformación» (#377031), only with non-linear elements
    bool updateNonlinear = true;  ///< «Actualizar las combinaciones no lineales tras el análisis lineal» (#377977) = NonlinearCombinations::updateAfterLinear
    bool checkBars = true;        ///< «Comprobar las barras» (#64348; checked by default in cp_61_calcdlg.png)
    bool finiteNodes = false;     ///< «Considerar la dimensión finita de los nudos» (#63989)
};

/// «Cálculo» (title bar with the help book). Without non-linear elements: the information band and the «Perfiles» radios (cp_61_calcdlg.png);
/// with them, first the band «Análisis a realizar» with the linear / non-linear check boxes, the update box (only when the work already has
/// non-linear combinations, [V section 12.5]) and «Análisis de segundo orden elástico» (rr_e2_dlg.png).
/// Not wired yet (later phases): the profile design radios other than «No dimensionar perfiles» (T-P9), «Análisis de segundo orden elástico»
/// (T-P10) and «Considerar la dimensión finita de los nudos» (TK-106): shown disabled / stored only.
class CalculationDialog : public CypeDialog {
    Q_OBJECT
public:
    CalculationDialog(bool nonlinearElements, bool hasNonlinearCombinations, const CalculationChoices& initial, QWidget* parent = nullptr);
    CalculationChoices choices() const;
    QCheckBox* linearBox() const { return m_linear; }
    QCheckBox* nonlinearBox() const { return m_nonlinear; }
    QCheckBox* updateBox() const { return m_update; }
    QCheckBox* checkBarsBox() const { return m_checkBars; }
    QCheckBox* finiteNodesBox() const { return m_finiteNodes; }
    QRadioButton* noDesignRadio() const { return m_noDesign; }

private:
    QCheckBox* m_linear = nullptr;
    QCheckBox* m_nonlinear = nullptr;
    QCheckBox* m_update = nullptr;
    QCheckBox* m_secondOrder = nullptr;
    QRadioButton* m_noDesign = nullptr;
    QCheckBox* m_checkBars = nullptr;
    QCheckBox* m_finiteNodes = nullptr;
};

/// «Informe final de cálculo»: a page with the title, the section «Cálculo de la estructura» and one line per message with its icon (red dot =
/// error, yellow triangle = warning) [V s_5.png]. The toolbar of the original's viewer (zoom, pages, Compartir, Exportar, Imprimir) is reproduced
/// with zoom and print; Compartir / Exportar are not wired (TK, listings phase).
class CalculationReportDialog : public CypeDialog {
    Q_OBJECT
public:
    explicit CalculationReportDialog(const std::vector<cc::fem::ReportEntry>& entries, QWidget* parent = nullptr);
    /// The HTML of the page (exposed for the tests).
    static QString pageHtml(const std::vector<cc::fem::ReportEntry>& entries);
    QTextBrowser* page() const { return m_page; }

private:
    QTextBrowser* m_page = nullptr;
};

/// «Generar combinaciones para las distintas situaciones de proyecto»: six check boxes, all clear the first time [V s_9.png]. The kinds the clone does
/// not generate yet (Fisuración, Fisuración bajo cargas de larga duración, Equilibrio de los elementos de cimentación, Tensiones en el terreno; see
/// core::generateNonlinearCombinations) are disabled.
class NonlinearGenerateDialog : public CypeDialog {
    Q_OBJECT
public:
    explicit NonlinearGenerateDialog(const cc::core::NonlinearKinds& initial, QWidget* parent = nullptr);
    cc::core::NonlinearKinds kinds() const;
    QCheckBox* box(cc::core::NonlinearCombinationKind kind) const { return m_boxes[static_cast<std::size_t>(kind)]; }

private:
    std::array<QCheckBox*, cc::core::kNonlinearCombinationKindCount> m_boxes{};
};

/// «Combinaciones no lineales» (resizable, maximisable; s_10.png): list Referencia | Actúa | Escalones de carga with its toolbar (+, copiar, subir,
/// bajar, borrar, borrar todas, flecha azul = generar, pincel), panel «Tipos de combinación» (Referencia | Usar) and panel «Factores de combinación»
/// (+, copiar, borrar; Hipótesis | coeficiente) of the selected combination. Works on a copy; value() on Aceptar.
class NonlinearCombinationsDialog : public CypeDialog {
    Q_OBJECT
public:
    NonlinearCombinationsDialog(const cc::core::Project& project, QWidget* parent = nullptr);
    cc::core::NonlinearCombinations value() const { return m_set; }

    /// Runs a nested dialog (tests replace it to answer without a screen).
    void setDialogRunner(std::function<int(QDialog&)> runner) { m_runner = std::move(runner); }
    QTableWidget* table() const { return m_table; }
    QTableWidget* kindsTable() const { return m_kinds; }
    QTableWidget* factorsTable() const { return m_factors; }
    QToolButton* generateButton() const { return m_generate; }
    QToolButton* addButton() const { return m_add; }
    QToolButton* copyButton() const { return m_copy; }
    QToolButton* upButton() const { return m_up; }
    QToolButton* downButton() const { return m_down; }
    QToolButton* deleteButton() const { return m_delete; }
    QToolButton* deleteAllButton() const { return m_deleteAll; }
    QToolButton* addFactorButton() const { return m_addFactor; }
    QToolButton* deleteFactorButton() const { return m_deleteFactor; }
    /// Rows of «Tipos de combinación» and the kind each one stands for (-1: «Resistencia en situación de incendio», no kind in the clone).
    static int kindOfRow(int row);
    /// The generation of the blue arrow (dialog + generation + message #378563); exposed for the tests.
    void generate();

protected:
    void done(int result) override;

private:
    int run(QDialog& dialog) { return m_runner ? m_runner(dialog) : dialog.exec(); }
    void message(const QString& text, bool error);
    void refreshList();
    void refreshPanels();
    int currentRow() const;
    void readListRow(int row);
    void readFactors();

    const cc::core::Project& m_project;
    cc::core::NonlinearCombinations m_set;
    std::function<int(QDialog&)> m_runner;
    bool m_refreshing = false;
    QTableWidget* m_table = nullptr;
    QTableWidget* m_kinds = nullptr;
    QTableWidget* m_factors = nullptr;
    QToolButton* m_add = nullptr;
    QToolButton* m_copy = nullptr;
    QToolButton* m_up = nullptr;
    QToolButton* m_down = nullptr;
    QToolButton* m_delete = nullptr;
    QToolButton* m_deleteAll = nullptr;
    QToolButton* m_generate = nullptr;
    QToolButton* m_brush = nullptr;
    QToolButton* m_addFactor = nullptr;
    QToolButton* m_copyFactor = nullptr;
    QToolButton* m_deleteFactor = nullptr;
};

}  // namespace cc::app
