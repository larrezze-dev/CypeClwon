#pragma once
#include "core/undo_stack.h"
#include "SARibbonMainWindow.h"
#include "core/project_file.h"
#include "core/recent_files.h"

#include <optional>
#include <QString>

class QAction;
class QActionGroup;
class QCloseEvent;
class QStackedWidget;

class QTabBar;

namespace cc::app {

class LeftPanel;
class NodeTool;
class SupportTool;
class LigatureTool;
class DiaphragmTool;
class BarEndsTool;
class BucklingTool;
class DeflectionTool;
class ShellPropertiesTool;
class NodalLoadTool;
class BarLoadTool;
class PanelTool;
class ShellLoadTool;
class BarTool;
class PieceTool;
class BarDescribeTool;
class ContinuousTool;
class ShellTool;
class TransformTool;
class GenerateTool;
class ReferenceTool;
class ImportTool;
class Viewport3D;
class SessionBar;
class StartPage;
class WorkArea;

/// Main application window: frameless ribbon window with the eight CYPE 3D tabs.
class MainWindow : public SARibbonMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);

    WorkArea* workArea() const { return m_workArea; }
    QAction* previousViewAction() const { return m_previousViewAction; }
    QAction* fullViewAction() const { return m_fullViewAction; }
    QAction* doubleViewAction() const { return m_doubleViewAction; }
    QAction* redrawAction() const { return m_redrawAction; }
    QAction* zoomMarkAction() const { return m_zoomMarkAction; }
    QAction* moveImageAction() const { return m_moveImageAction; }
    QAction* orbitAction() const { return m_orbitAction; }
    StartPage* startPage() const { return m_startPage; }

    /// Switches between the start screen (no project open) and the ribbon + work area.
    void setProjectOpen(bool open);
    /// Puts `path` at the top of "Últimos ficheros" and persists the list (called when a project is saved or opened).
    void registerRecentFile(const QString& path);

    /// Loads the project file at `path` and makes it the open project. Shows the (Spanish) error and returns false
    /// when it cannot be opened; the current project is left untouched in that case.
    bool openProjectFile(const QString& path);
    /// Writes the open project to its file ("Guardar"). Returns false (after telling the user) on failure.
    bool saveProject();
    /// "Guardar como": asks for a new file and, once written, makes it the project file.
    bool saveProjectAs();
    /// True when the open project is not saved ("obra modificada").
    bool isProjectModified() const { return m_projectOpen && !m_undo.isClean(); }
    const QString& projectPath() const { return m_projectPath; }
    const std::optional<cc::core::ProjectDocument>& document() const { return m_document; }

protected:
    void closeEvent(QCloseEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void buildRibbonTabs();
    void applyTitleBarStyle();
    void placeSessionBar();
    void createQuickAccessBar();
    void createStatusBar();
    /// Zoom / framing group of the top bar (Ventana anterior ... Órbita 3D), at the right of the ribbon tabs.
    void createZoomToolbar();
    Viewport3D* activeViewport() const;
    /// Enables the zoom group when a 3D window is active and applies the active tool to every 3D window.
    void updateViewActions();
    void applyViewTool(Viewport3D* viewport) const;
    void createWorkArea();
    void newProject();
    void openFileManager();
    void showRecentFiles();
    void loadRecentFiles();
    void saveRecentFiles() const;
    void refreshRecentFiles();
    void openRecentFile(const QString& path);
    void refreshUndoActions();
    /// Shows the project's view settings in the left panel and applies them to the 3D windows.
    void syncViewSettings();
    /// Obra > Datos generales: edits the settings of the open project as one undoable step.
    void editGeneralData();
    /// Obra > Unidades de acotación.
    void editUnits();
    void editProfileLibrary();
    /// Builds the groups of the Obra tab (reference: docs/screenshots/orig_obra_tab.png).
    void buildObraTab();
    /// Builds the groups of the Geometria tab built so far ("Planos", "Nudos"; the rest arrives with the bars).
    void buildGeometryTab();
    /// Geometria > Planos > Rejilla / Niveles / Visibilidad (each edit is one undoable step).
    void editGrid();
    void editLevels();
    void editPlanesVisibility();
    void editReferences();
    /// "Cotas": shows the floating bar with its four buttons (the actions need nodes, ticket TK-035).
    void showDimensionsBar();
    void updateWindowTitle();
    /// Offers to save a modified project; false when the user cancels.
    bool maybeSave();

    QAction* m_saveAction = nullptr;
    QAction* m_undoAction = nullptr;
    QAction* m_redoAction = nullptr;
    QAction* m_printAction = nullptr;
    QAction* m_saveAsAction = nullptr;
    QAction* m_generalDataAction = nullptr;
    QAction* m_unitsAction = nullptr;
    QAction* m_gridAction = nullptr;
    QAction* m_levelsAction = nullptr;
    QAction* m_planesVisibilityAction = nullptr;
    QAction* m_showNewPlanesAction = nullptr;
    class DimensionsBar* m_dimensionsBar = nullptr;
    NodeTool* m_nodeTool = nullptr;  ///< Geometria > Nudos (Nuevo / Borrar / Mover)
    LigatureTool* m_ligatureTool = nullptr;  ///< Propiedades > Nudos > Ligaduras (T-P6-02)
    DiaphragmTool* m_diaphragmTool = nullptr;  ///< Propiedades > Diafragmas (T-P6-03)
    ShellPropertiesTool* m_shellPropsTool = nullptr;  ///< Propiedades > Laminas (T-P6-07)
    NodalLoadTool* m_nodalLoadTool = nullptr;         ///< Carga > Nudos / Visualización (T-P7-02)
    BarLoadTool* m_barLoadTool = nullptr;             ///< Carga > Barras (T-P7-03)
    ShellLoadTool* m_shellLoadTool = nullptr;         ///< Carga > Láminas (T-P7-04)
    PanelTool* m_panelTool = nullptr;                 ///< Carga > Paños y Cargas superficiales (T-P7-05)
    DeflectionTool* m_deflectionTool = nullptr;  ///< Propiedades > Barras > Grupos de flecha (T-P6-06)
    BucklingTool* m_bucklingTool = nullptr;  ///< Propiedades > Barras > Pandeo / Pandeo lateral (T-P6-04)
    BarEndsTool* m_barEndsTool = nullptr;  ///< Propiedades > Barras > Articular / Empotramiento (T-P6-05)
    SupportTool* m_supportTool = nullptr;  ///< Propiedades > Nudos (Vinculación exterior / interior, T-P6-01)
    BarDescribeTool* m_describeTool = nullptr;  ///< Geometria > Barras (Seccion / Material, T-P5-07)
    PieceTool* m_pieceTool = nullptr;  ///< Geometria > Barras (Pieza / Agrupar / Desagrupar / Mover extremo / Invertir eje)
    ContinuousTool* m_continuousTool = nullptr;  ///< Geometria > Barras (Edición de pilares / vigas, T-P5-08)
    ShellTool* m_shellTool = nullptr;  ///< Geometria > Láminas (T-P5-09)
    TransformTool* m_transformTool = nullptr;
    GenerateTool* m_generateTool = nullptr;
    ReferenceTool* m_referenceTool = nullptr;  ///< Geometria > Herramientas (T-P5-10)
    ImportTool* m_importTool = nullptr;        ///< Geometria > Importar (T-P5-11)
    BarTool* m_barTool = nullptr;    ///< Geometria > Barras (Barra / Pilar / Viga)
    QAction* m_previousViewAction = nullptr;
    QAction* m_fullViewAction = nullptr;
    QAction* m_doubleViewAction = nullptr;
    QAction* m_redrawAction = nullptr;
    QAction* m_zoomMarkAction = nullptr;
    QAction* m_moveImageAction = nullptr;
    QAction* m_orbitAction = nullptr;
    QActionGroup* m_viewTools = nullptr;
    QMetaObject::Connection m_historyConnection;

    SessionBar* m_sessionBar = nullptr;
    LeftPanel* m_leftPanel = nullptr;
    QTabBar* m_modeTabs = nullptr;
    WorkArea* m_workArea = nullptr;
    StartPage* m_startPage = nullptr;
    cc::core::RecentFiles m_recent;
    cc::core::UndoStack m_undo;  ///< edit history of the open project (commands arrive with the project model, phase 3)
    bool m_projectOpen = false;
    std::optional<cc::core::ProjectDocument> m_document;  ///< the open project (empty on the start page)
    QString m_projectPath;                                ///< its file, with '/' separators
    QStackedWidget* m_stack = nullptr;
};

}  // namespace cc::app


