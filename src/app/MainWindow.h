#pragma once
#include "SARibbonMainWindow.h"

namespace cc::app {

/// Main application window: frameless ribbon window with the eight CYPE 3D tabs.
class MainWindow : public SARibbonMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);

private:
    void buildRibbonTabs();
};

}  // namespace cc::app

