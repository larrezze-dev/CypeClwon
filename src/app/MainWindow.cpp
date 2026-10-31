#include "MainWindow.h"

#include "SARibbonBar.h"
#include "SARibbonCategory.h"
#include "core/version.h"

namespace cc::app {

MainWindow::MainWindow(QWidget* parent) : SARibbonMainWindow(parent) {
    setWindowTitle(QStringLiteral("%1 v%2")
                       .arg(QString::fromUtf8(cc::core::productName().data(),
                                              static_cast<qsizetype>(cc::core::productName().size())),
                            QString::fromUtf8(cc::core::versionString().data(),
                                              static_cast<qsizetype>(cc::core::versionString().size()))));
    resize(1280, 800);
    buildRibbonTabs();
}

void MainWindow::buildRibbonTabs() {
    SARibbonBar* ribbon = ribbonBar();
    // Order and names follow the original application (docs/research/02_ui_inventory.md).
    const QStringList tabs = {tr("Obra"),     tr("Geometría"), tr("Propiedades"), tr("Carga"),
                              tr("Uniones"),  tr("Cálculo"),   tr("Armaduras"),   tr("Ventana")};
    for (const QString& title : tabs) {
        ribbon->addCategoryPage(title);
    }
}

}  // namespace cc::app

