#pragma once
#include "SARibbonMainWindow.h"

class QAction;

namespace cc::app {

class SessionBar;

/// Main application window: frameless ribbon window with the eight CYPE 3D tabs.
class MainWindow : public SARibbonMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void buildRibbonTabs();
    void applyTitleBarStyle();
    void placeSessionBar();
    void createQuickAccessBar();
    void createStatusBar();

    QAction* m_saveAction = nullptr;
    QAction* m_undoAction = nullptr;
    QAction* m_redoAction = nullptr;
    QAction* m_printAction = nullptr;

    SessionBar* m_sessionBar = nullptr;
};

}  // namespace cc::app


