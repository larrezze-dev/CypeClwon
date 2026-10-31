#pragma once
#include "SARibbonMainWindow.h"

class QAction;
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

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void buildRibbonTabs();
    void applyTitleBarStyle();
    void placeSessionBar();
    void createQuickAccessBar();
    void createStatusBar();
    void createWorkArea();

    QAction* m_saveAction = nullptr;
    QAction* m_undoAction = nullptr;
    QAction* m_redoAction = nullptr;
    QAction* m_printAction = nullptr;

    SessionBar* m_sessionBar = nullptr;
    WorkArea* m_workArea = nullptr;
    StartPage* m_startPage = nullptr;
    QStackedWidget* m_stack = nullptr;
};

}  // namespace cc::app


