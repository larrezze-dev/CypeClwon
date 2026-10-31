#pragma once
#include "SARibbonMainWindow.h"

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

    SessionBar* m_sessionBar = nullptr;
};

}  // namespace cc::app

