#pragma once
#include "core/undo_stack.h"
#include "SARibbonMainWindow.h"
#include "core/project_file.h"
#include "core/recent_files.h"

#include <optional>
#include <QString>

class QAction;
class QCloseEvent;
class QStackedWidget;

namespace cc::app {

class SessionBar;
class StartPage;
class WorkArea;

/// Main application window: frameless ribbon window with the eight CYPE 3D tabs.
class MainWindow : public SARibbonMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);

    WorkArea* workArea() const { return m_workArea; }
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
    void createWorkArea();
    void newProject();
    void openFileManager();
    void showRecentFiles();
    void loadRecentFiles();
    void saveRecentFiles() const;
    void refreshRecentFiles();
    void openRecentFile(const QString& path);
    void refreshUndoActions();
    /// Obra > Datos generales: edits the settings of the open project as one undoable step.
    void editGeneralData();
    void updateWindowTitle();
    /// Offers to save a modified project; false when the user cancels.
    bool maybeSave();

    QAction* m_saveAction = nullptr;
    QAction* m_undoAction = nullptr;
    QAction* m_redoAction = nullptr;
    QAction* m_printAction = nullptr;
    QAction* m_saveAsAction = nullptr;
    QAction* m_generalDataAction = nullptr;

    SessionBar* m_sessionBar = nullptr;
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


