#include "MainWindow.h"
#include "CategoriesPanel.h"
#include "DisplayOptionsPanel.h"
#include <QTabBar>

#include "LeftPanel.h"
#include <QHBoxLayout>
#include <QMdiArea>
#include <QMdiSubWindow>

#include <QCloseEvent>
#include <QDir>
#include <QFileDialog>
#include <QMessageBox>
#include <QEvent>
#include <QAction>
#include <QIcon>
#include <QKeySequence>
#include <QStackedWidget>
#include <QStatusBar>

#include "SARibbonBar.h"
#include "SARibbonCategory.h"
#include "SARibbonPanel.h"
#include "SARibbonToolButton.h"
#include "SARibbonQuickAccessBar.h"
#include "SARibbonSystemButtonBar.h"
#include <QFileInfo>
#include <QSettings>
#include "Icons.h"
#include "SessionBar.h"
#include "NewProjectDialog.h"
#include "EmptyProjectWizard.h"
#include "FileManagerDialog.h"
#include "FileOpsDialogs.h"
#include "LimitStatesDialog.h"
#include "ProfileLibraryDialogs.h"
#include "UnitsDialog.h"
#include "WorkPlanesDialogs.h"
#include <QMenu>
#include "ProjectTypeDialog.h"
#include "StartPage.h"
#include <QMdiArea>
#include "LayersPanel.h"
#include "BarTool.h"
#include "PieceTool.h"
#include "BarDescribeTool.h"
#include "ContinuousTool.h"
#include "ShellTool.h"
#include "TransformTool.h"
#include "GenerateTool.h"
#include "ReferenceTool.h"
#include "ImportTool.h"
#include "NodeTool.h"
#include "SupportTool.h"
#include "LigatureTool.h"
#include "DiaphragmTool.h"
#include "BarEndsTool.h"
#include "BucklingTool.h"
#include "DeflectionTool.h"
#include "ShellPropertiesTool.h"
#include "NodalLoadTool.h"
#include "BarLoadTool.h"
#include "CalculationTool.h"
#include "PanelTool.h"
#include "ShellLoadTool.h"
#include "UserMessages.h"
#include "ViewBar.h"
#include "ViewIcons.h"
#include "Viewport3D.h"
#include <QActionGroup>
#include "WorkArea.h"
#include "core/general_data.h"
#include "core/load_case_edit.h"
#include "core/project.h"
#include "core/project_file.h"
#include "core/version.h"

#include <filesystem>
#include <memory>

namespace cc::app {

namespace {
// Colours sampled from the original title bar (docs/screenshots/orig_asistente_02.png).
constexpr const char* kTitleRed = "#B1291C";
constexpr int kTitleBarHeight = 26;

std::filesystem::path toPath(const QString& path) { return std::filesystem::path(path.toStdWString()); }

/// The project described by the wizard and the limit-states dialog that follows it.
cc::core::Project projectFrom(const EmptyProjectWizard& wizard, const cc::core::LimitStatesSettings& limitStates) {
    cc::core::Project project;
    project.standards = wizard.standards();
    project.profiles = wizard.profiles();
    project.concrete = wizard.reinforcedConcrete();
    project.actions = wizard.actions();
    project.environment = wizard.environment();
    project.ground = wizard.ground();
    project.limitStates = limitStates;
    project.piles = wizard.piles();
    project.micropiles = wizard.micropiles();
    project.options = wizard.options();
    project.loadCases = wizard.loadCases();
    return project;  // units keep their default (cm / cm / m)
}
}  // namespace

namespace {
/// The viewport of a 3D child window (its content is a ViewportFrame; a bare Viewport3D is accepted too).
Viewport3D* viewportOf(QWidget* content) {
    if (auto* frame = qobject_cast<ViewportFrame*>(content)) return frame->viewport();
    return qobject_cast<Viewport3D*>(content);
}
}  // namespace

MainWindow::MainWindow(QWidget* parent) : SARibbonMainWindow(parent) {
    setWindowIcon(QIcon(QStringLiteral(":/icons/app_logo.svg")));
    setWindowTitle(QStringLiteral("%1 v%2")
                       .arg(QString::fromUtf8(cc::core::productName().data(),
                                              static_cast<qsizetype>(cc::core::productName().size())),
                            QString::fromUtf8(cc::core::versionString().data(),
                                              static_cast<qsizetype>(cc::core::versionString().size()))));
    resize(1280, 800);
    buildRibbonTabs();
    applyTitleBarStyle();
    createQuickAccessBar();
    createZoomToolbar();
    createStatusBar();
    createWorkArea();
}

void MainWindow::createWorkArea() {
    m_stack = new QStackedWidget(this);
    m_startPage = new StartPage(m_stack);
    m_workArea = new WorkArea(m_stack);
    m_stack->addWidget(m_startPage);
    m_stack->addWidget(m_workArea);
    // Left panel (Etiquetas / Categorías / Opciones de visualización) beside the stack; visible only with a project open.
    auto* central = new QWidget(this);
    auto* outerLayout = new QVBoxLayout(central);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->setSpacing(0);
    auto* body = new QWidget(central);
    auto* centralLayout = new QHBoxLayout(body);
    centralLayout->setContentsMargins(0, 0, 0, 0);
    centralLayout->setSpacing(0);
    m_leftPanel = new LeftPanel(body);
    centralLayout->addWidget(m_leftPanel);
    centralLayout->addWidget(m_stack, 1);
    outerLayout->addWidget(body, 1);
    // Mode tabs under the whole window, above the status bar (reference: docs/screenshots/vb_full.png). Bold = current.
    m_modeTabs = new QTabBar(central);
    m_modeTabs->setObjectName(QStringLiteral("modeTabs"));
    m_modeTabs->setDrawBase(false);
    m_modeTabs->setExpanding(false);
    m_modeTabs->addTab(tr("Estructura"));
    m_modeTabs->addTab(tr("Cimentación"));
    m_modeTabs->setStyleSheet(QStringLiteral(
        "QTabBar::tab { border: none; background: transparent; padding: 3px 12px; }"
        "QTabBar::tab:selected { font-weight: bold; }"));
    m_modeTabs->hide();
    connect(m_modeTabs, &QTabBar::currentChanged, this, [this](int index) {
        if (index == 0) return;
        // The foundation module is not cloned yet (TK-033): stay on the structure.
        m_modeTabs->setCurrentIndex(0);
        statusBar()->showMessage(tr("Esta función aún no está disponible."), 4000);
    });
    outerLayout->addWidget(m_modeTabs);
    setCentralWidget(central);
    // View settings are stored with the project (verified in the original: closing without saving restores defaults).
    connect(m_leftPanel->displayOptions(), &DisplayOptionsPanel::optionsChanged, this,
            [this](const cc::core::DisplayOptions& options) {
                if (m_document && !(m_document->project.display == options))
                    m_undo.push(std::make_unique<cc::core::EditViewSettingsCommand>(
                        m_document->project, options, m_document->project.categories));
                for (QMdiSubWindow* sub : m_workArea->mdiArea()->subWindowList())
                    if (auto* viewport = viewportOf(sub->widget())) DisplayOptionsPanel::applyTo(*viewport, options);
            });
    // Categorías: visibility and layer colours act on every 3D window (session-only, like the display options).
    connect(m_leftPanel->categoriesPanel(), &CategoriesPanel::settingsChanged, this,
            [this](const cc::core::CategorySettings& settings) {
                if (m_document && !(m_document->project.categories == settings))
                    m_undo.push(std::make_unique<cc::core::EditViewSettingsCommand>(
                        m_document->project, m_document->project.display, settings));
                for (QMdiSubWindow* sub : m_workArea->mdiArea()->subWindowList())
                    if (auto* viewport = viewportOf(sub->widget())) viewport->setCategorySettings(settings);
            });
    // Etiquetas: eye, padlock, colour and cube act on every 3D window; the assign tool turns the viewport into a selection
    // surface (LMB selects bars and shells, RMB opens "Asignar elementos a etiquetas").
    connect(m_leftPanel->layersPanel(), &LayersPanel::layersChanged, this, [this] {
        for (QMdiSubWindow* sub : m_workArea->mdiArea()->subWindowList())
            if (auto* viewport = viewportOf(sub->widget())) {
                viewport->clearSelection();  // a layer may have just been hidden
                viewport->update();
            }
    });
    connect(m_leftPanel->layersPanel(), &LayersPanel::assignToolToggled, this, [this](bool on) {
        if (on) {  // one viewport tool at a time: RMB must not also act as a node or bar command
            m_nodeTool->deactivate();
            m_barTool->deactivate();
            m_pieceTool->deactivate();
            m_describeTool->deactivate();
            m_continuousTool->deactivate();
            m_shellTool->deactivate();
            m_transformTool->deactivate();
            m_generateTool->deactivate();
            m_referenceTool->deactivate();
            m_supportTool->deactivate();
            m_ligatureTool->deactivate();
            m_diaphragmTool->deactivate();
            m_barEndsTool->deactivate();
            m_bucklingTool->deactivate();
            m_deflectionTool->deactivate();
            m_shellPropsTool->deactivate();
            m_nodalLoadTool->deactivate();
            m_barLoadTool->deactivate();
            m_shellLoadTool->deactivate();
            m_panelTool->deactivate();
        }
        for (QMdiSubWindow* sub : m_workArea->mdiArea()->subWindowList())
            if (auto* viewport = viewportOf(sub->widget())) {
                viewport->setSelectionFilter({false, true, true});
                viewport->setSelectionMode(on);
            }
        // After the viewport's own, generic hint; same format as the original (n7_borrar.png).
        statusBar()->showMessage(on ? tr("Herramientas - Asignar elementos a etiquetas |  - LMB: Seleccione un grupo de elementos. - RMB: Actuar sobre los elementos seleccionados.")
                                    : NodeTool::defaultStatusText());
    });
    // Geometria > Nudos: its status text goes to the bar; a command and the "Asignar elementos" tool exclude each other.
    connect(m_nodeTool, &NodeTool::statusChanged, this, [this](const QString& text) { statusBar()->showMessage(text); });
    // Leaving the ribbon button clears the bar (its status tip ends): while a command is active its text comes back.
    connect(statusBar(), &QStatusBar::messageChanged, this, [this](const QString& text) {
        if (!text.isEmpty()) return;
        if (m_nodeTool->mode() != NodeTool::Mode::None)
            statusBar()->showMessage(NodeTool::statusText(m_nodeTool->mode(), m_nodeTool->movePending()));
        else if (m_barTool->mode() != BarTool::Mode::None)
            statusBar()->showMessage(BarTool::statusText(m_barTool->mode(), static_cast<int>(m_barTool->chain().size())));
        else if (m_pieceTool->mode() != PieceTool::Mode::None)
            statusBar()->showMessage(PieceTool::statusText(m_pieceTool->mode(), static_cast<int>(m_pieceTool->chain().size()) + m_pieceTool->moveStep()));
        else if (m_describeTool->mode() != BarDescribeTool::Mode::None)
            statusBar()->showMessage(BarDescribeTool::statusText(m_describeTool->mode()));
        else if (m_continuousTool->mode() != ContinuousTool::Mode::None)
            statusBar()->showMessage(ContinuousTool::statusText(m_continuousTool->mode()));
        else if (m_shellTool->mode() != ShellTool::Mode::None)
            statusBar()->showMessage(ShellTool::statusText(m_shellTool->mode(), m_shellTool->step()));
        else if (m_transformTool->mode() != TransformTool::Mode::None)
            statusBar()->showMessage(TransformTool::statusText(m_transformTool->mode(), m_transformTool->step()));
        else if (m_supportTool->mode() != SupportTool::Mode::None)
            statusBar()->showMessage(SupportTool::statusText(m_supportTool->mode()));
        else if (m_ligatureTool->mode() != LigatureTool::Mode::None)
            statusBar()->showMessage(LigatureTool::statusText(m_ligatureTool->mode()));
        else if (m_diaphragmTool->mode() != DiaphragmTool::Mode::None)
            statusBar()->showMessage(DiaphragmTool::statusText(m_diaphragmTool->mode()));
        else if (m_barEndsTool->mode() != BarEndsTool::Mode::None)
            statusBar()->showMessage(BarEndsTool::statusText(m_barEndsTool->mode(), m_barEndsTool->panel() && m_barEndsTool->panel()->multiple()));
        else if (m_bucklingTool->mode() != BucklingTool::Mode::None)
            statusBar()->showMessage(BucklingTool::statusText(m_bucklingTool->mode()));
        else if (m_deflectionTool->mode() != DeflectionTool::Mode::None)
            statusBar()->showMessage(DeflectionTool::statusText(m_deflectionTool->mode()));
        else if (m_shellPropsTool->mode() != ShellPropertiesTool::Mode::None)
            statusBar()->showMessage(ShellPropertiesTool::statusText(m_shellPropsTool->mode()));
        else if (m_nodalLoadTool->mode() != NodalLoadTool::Mode::None)
            statusBar()->showMessage(NodalLoadTool::statusText(m_nodalLoadTool->mode()));
        else if (m_barLoadTool->mode() != BarLoadTool::Mode::None)
            statusBar()->showMessage(BarLoadTool::statusText(m_barLoadTool->mode()));
        else if (m_shellLoadTool->mode() != ShellLoadTool::Mode::None)
            statusBar()->showMessage(ShellLoadTool::statusText(m_shellLoadTool->mode()));
        else if (m_panelTool->mode() != PanelTool::Mode::None)
            statusBar()->showMessage(PanelTool::statusText(m_panelTool->mode(), m_panelTool->step()));
    });
    connect(m_nodeTool, &NodeTool::aboutToActivate, this, [this] {
        m_leftPanel->layersPanel()->assignToolButton()->setChecked(false);
        m_barTool->deactivate();
        m_pieceTool->deactivate();
        m_describeTool->deactivate();
        m_continuousTool->deactivate();
        m_shellTool->deactivate();
        m_transformTool->deactivate();
        m_generateTool->deactivate();
        m_referenceTool->deactivate();
    });
    // Geometria > Barras: same rules as Nudos; Pilar without levels asks for a message (#65140).
    connect(m_barTool, &BarTool::statusChanged, this, [this](const QString& text) { statusBar()->showMessage(text); });
    connect(m_barTool, &BarTool::aboutToActivate, this, [this] {
        m_leftPanel->layersPanel()->assignToolButton()->setChecked(false);
        m_nodeTool->deactivate();
        m_pieceTool->deactivate();
        m_describeTool->deactivate();
        m_continuousTool->deactivate();
        m_shellTool->deactivate();
        m_transformTool->deactivate();
        m_generateTool->deactivate();
        m_referenceTool->deactivate();
    });
    connect(m_pieceTool, &PieceTool::statusChanged, this, [this](const QString& text) { statusBar()->showMessage(text); });
    connect(m_pieceTool, &PieceTool::aboutToActivate, this, [this] {
        m_leftPanel->layersPanel()->assignToolButton()->setChecked(false);
        m_nodeTool->deactivate();
        m_barTool->deactivate();
        m_describeTool->deactivate();
        m_continuousTool->deactivate();
        m_shellTool->deactivate();
        m_transformTool->deactivate();
        m_generateTool->deactivate();
        m_referenceTool->deactivate();
    });
    // Geometria > Barras > Seccion / Material (T-P5-07): same rules; the dialogs are children of the main window.
    m_describeTool->setDialogParent(this);
    connect(m_describeTool, &BarDescribeTool::statusChanged, this, [this](const QString& text) { statusBar()->showMessage(text); });
    connect(m_describeTool, &BarDescribeTool::aboutToActivate, this, [this] {
        m_leftPanel->layersPanel()->assignToolButton()->setChecked(false);
        m_nodeTool->deactivate();
        m_barTool->deactivate();
        m_pieceTool->deactivate();
        m_continuousTool->deactivate();
        m_shellTool->deactivate();
        m_transformTool->deactivate();
        m_generateTool->deactivate();
        m_referenceTool->deactivate();
    });
    connect(m_describeTool, &BarDescribeTool::messageRequested, this, [this](const QString& text) { QMessageBox::warning(this, tr("Barras"), text); });
    connect(m_pieceTool, &PieceTool::messageRequested, this, [this](const QString& text) { QMessageBox::warning(this, tr("Barras"), text); });
    m_pieceTool->setConfirmer([this](const QString& question) {
        return QMessageBox::question(this, tr("Barras"), question, QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes) == QMessageBox::Yes;
    });
    connect(m_barTool, &BarTool::messageRequested, this, [this](const QString& text) { QMessageBox::warning(this, tr("Barras"), text); });
    // Geometria > Barras > Edición de pilares / vigas (T-P5-08): same rules; the entries without core only say so.
    m_continuousTool->setDialogParent(this);
    connect(m_continuousTool, &ContinuousTool::statusChanged, this, [this](const QString& text) { statusBar()->showMessage(text); });
    connect(m_continuousTool, &ContinuousTool::aboutToActivate, this, [this] {
        m_leftPanel->layersPanel()->assignToolButton()->setChecked(false);
        m_nodeTool->deactivate();
        m_barTool->deactivate();
        m_pieceTool->deactivate();
        m_describeTool->deactivate();
        m_shellTool->deactivate();
        m_transformTool->deactivate();
        m_generateTool->deactivate();
        m_referenceTool->deactivate();
    });
    connect(m_continuousTool, &ContinuousTool::messageRequested, this, [this](const QString& text) { QMessageBox::warning(this, tr("Edición de pilares y vigas"), text); });
    connect(m_continuousTool, &ContinuousTool::notAvailable, this, [this] { statusBar()->showMessage(tr("Esta función aún no está disponible."), 4000); });
    // Geometria > Laminas (T-P5-09) and Herramientas (T-P5-10): same rules; Seccion / Disposicion / Material and Generar only say so.
    m_shellTool->setDialogParent(this);
    connect(m_shellTool, &ShellTool::statusChanged, this, [this](const QString& text) { statusBar()->showMessage(text); });
    connect(m_shellTool, &ShellTool::aboutToActivate, this, [this] {
        m_leftPanel->layersPanel()->assignToolButton()->setChecked(false);
        m_nodeTool->deactivate();
        m_barTool->deactivate();
        m_pieceTool->deactivate();
        m_describeTool->deactivate();
        m_continuousTool->deactivate();
        m_transformTool->deactivate();
        m_generateTool->deactivate();
        m_referenceTool->deactivate();
    });
    connect(m_shellTool, &ShellTool::messageRequested, this, [this](const QString& text) { QMessageBox::warning(this, tr("Láminas"), text); });
    m_transformTool->setDialogParent(this);
    connect(m_transformTool, &TransformTool::statusChanged, this, [this](const QString& text) { statusBar()->showMessage(text); });
    connect(m_transformTool, &TransformTool::aboutToActivate, this, [this] {
        m_leftPanel->layersPanel()->assignToolButton()->setChecked(false);
        m_nodeTool->deactivate();
        m_barTool->deactivate();
        m_pieceTool->deactivate();
        m_describeTool->deactivate();
        m_continuousTool->deactivate();
        m_shellTool->deactivate();
        m_generateTool->deactivate();
        m_referenceTool->deactivate();
    });
    connect(m_transformTool, &TransformTool::messageRequested, this, [this](const QString& text) { QMessageBox::warning(this, tr("Herramientas"), text); });
    // Herramientas > Generar and Asignar referencia / Buscar: same rules.
    const auto stopOthers = [this] {
        m_leftPanel->layersPanel()->assignToolButton()->setChecked(false);
        m_nodeTool->deactivate();
        m_barTool->deactivate();
        m_pieceTool->deactivate();
        m_describeTool->deactivate();
        m_continuousTool->deactivate();
        m_shellTool->deactivate();
        m_transformTool->deactivate();
    };
    m_generateTool->setDialogParent(this);
    connect(m_generateTool, &GenerateTool::statusChanged, this, [this](const QString& text) { statusBar()->showMessage(text); });
    connect(m_generateTool, &GenerateTool::aboutToActivate, this, [this, stopOthers] {
        stopOthers();
        m_referenceTool->deactivate();
    });
    connect(m_generateTool, &GenerateTool::messageRequested, this, [this](const QString& text) { QMessageBox::warning(this, tr("Generar"), text); });
    m_generateTool->setConfirmer([this](const QString& question) {
        return QMessageBox::question(this, tr("Generar"), question, QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes) == QMessageBox::Yes;
    });
    // Propiedades > Nudos (T-P6-01): one viewport tool at a time, like the others.
    m_supportTool->setDialogParent(this);
    connect(m_supportTool, &SupportTool::statusChanged, this, [this](const QString& text) { statusBar()->showMessage(text); });
    connect(m_supportTool, &SupportTool::aboutToActivate, this, [this, stopOthers] {
        stopOthers();
        m_generateTool->deactivate();
        m_referenceTool->deactivate();
    });
    connect(m_supportTool, &SupportTool::aboutToActivate, m_ligatureTool, &LigatureTool::deactivate);
    // Propiedades > Nudos > Ligaduras (T-P6-02): same rules; its errors are the original's "Error" dialog.
    m_ligatureTool->setDialogParent(this);
    connect(m_ligatureTool, &LigatureTool::statusChanged, this, [this](const QString& text) { statusBar()->showMessage(text); });
    connect(m_ligatureTool, &LigatureTool::aboutToActivate, this, [this, stopOthers] {
        stopOthers();
        m_generateTool->deactivate();
        m_referenceTool->deactivate();
        m_supportTool->deactivate();
    });
    connect(m_ligatureTool, &LigatureTool::errorRequested, this, [this](const QString& text) { QMessageBox::critical(this, tr("Error"), text); });
    // Propiedades > Diafragmas (T-P6-03) and Propiedades > Barras > Articular / Empotramiento (T-P6-05): same rules.
    m_diaphragmTool->setDialogParent(this);
    m_barEndsTool->setDialogParent(this);
    m_bucklingTool->setDialogParent(this);
    m_deflectionTool->setDialogParent(this);
    connect(m_diaphragmTool, &DiaphragmTool::statusChanged, this, [this](const QString& text) { statusBar()->showMessage(text); });
    connect(m_barEndsTool, &BarEndsTool::statusChanged, this, [this](const QString& text) { statusBar()->showMessage(text); });
    connect(m_bucklingTool, &BucklingTool::statusChanged, this, [this](const QString& text) { statusBar()->showMessage(text); });
    connect(m_deflectionTool, &DeflectionTool::statusChanged, this, [this](const QString& text) { statusBar()->showMessage(text); });
    connect(m_diaphragmTool, &DiaphragmTool::aboutToActivate, this, [this, stopOthers] {
        stopOthers();
        m_generateTool->deactivate();
        m_referenceTool->deactivate();
        m_supportTool->deactivate();
        m_ligatureTool->deactivate();
        m_barEndsTool->deactivate();
        m_bucklingTool->deactivate();
        m_deflectionTool->deactivate();
    });
    connect(m_barEndsTool, &BarEndsTool::aboutToActivate, this, [this, stopOthers] {
        stopOthers();
        m_generateTool->deactivate();
        m_referenceTool->deactivate();
        m_supportTool->deactivate();
        m_ligatureTool->deactivate();
        m_diaphragmTool->deactivate();
        m_bucklingTool->deactivate();
        m_deflectionTool->deactivate();
    });
    connect(m_bucklingTool, &BucklingTool::aboutToActivate, this, [this, stopOthers] {
        stopOthers();
        m_generateTool->deactivate();
        m_referenceTool->deactivate();
        m_supportTool->deactivate();
        m_ligatureTool->deactivate();
        m_diaphragmTool->deactivate();
        m_barEndsTool->deactivate();
        m_deflectionTool->deactivate();
    });
    connect(m_deflectionTool, &DeflectionTool::aboutToActivate, this, [this, stopOthers] {
        stopOthers();
        m_generateTool->deactivate();
        m_referenceTool->deactivate();
        m_supportTool->deactivate();
        m_ligatureTool->deactivate();
        m_diaphragmTool->deactivate();
        m_barEndsTool->deactivate();
        m_bucklingTool->deactivate();
    });
    // Propiedades > Laminas (T-P6-07): same rules; every other viewport tool stops it and it stops them.
    m_shellPropsTool->setDialogParent(this);
    connect(m_shellPropsTool, &ShellPropertiesTool::statusChanged, this, [this](const QString& text) { statusBar()->showMessage(text); });
    connect(m_shellPropsTool, &ShellPropertiesTool::aboutToActivate, this, [this, stopOthers] {
        stopOthers();
        m_generateTool->deactivate();
        m_referenceTool->deactivate();
        m_supportTool->deactivate();
        m_ligatureTool->deactivate();
        m_diaphragmTool->deactivate();
        m_barEndsTool->deactivate();
        m_bucklingTool->deactivate();
        m_deflectionTool->deactivate();
    });
    // Carga > Nudos / Visualización (T-P7-02): same rules; every other viewport tool stops it and it stops them.
    m_nodalLoadTool->setDialogParent(this);
    connect(m_nodalLoadTool, &NodalLoadTool::statusChanged, this, [this](const QString& text) { statusBar()->showMessage(text); });
    connect(m_nodalLoadTool, &NodalLoadTool::aboutToActivate, this, [this, stopOthers] {
        stopOthers();
        m_generateTool->deactivate();
        m_referenceTool->deactivate();
        m_supportTool->deactivate();
        m_ligatureTool->deactivate();
        m_diaphragmTool->deactivate();
        m_barEndsTool->deactivate();
        m_bucklingTool->deactivate();
        m_deflectionTool->deactivate();
        m_shellPropsTool->deactivate();
        m_barLoadTool->deactivate();
        m_shellLoadTool->deactivate();
        m_panelTool->deactivate();
    });
    // Carga > Barras (T-P7-03): same rules; it shares the window «Propiedades - Hipótesis vista» with the nodal loads.
    m_barLoadTool->setHost(m_nodalLoadTool);
    m_barLoadTool->setDialogParent(this);
    connect(m_barLoadTool, &BarLoadTool::statusChanged, this, [this](const QString& text) { statusBar()->showMessage(text); });
    connect(m_barLoadTool, &BarLoadTool::aboutToActivate, this, [this, stopOthers] {
        stopOthers();
        m_generateTool->deactivate();
        m_referenceTool->deactivate();
        m_supportTool->deactivate();
        m_ligatureTool->deactivate();
        m_diaphragmTool->deactivate();
        m_barEndsTool->deactivate();
        m_bucklingTool->deactivate();
        m_deflectionTool->deactivate();
        m_shellPropsTool->deactivate();
        m_nodalLoadTool->deactivate();
        m_shellLoadTool->deactivate();
        m_panelTool->deactivate();
    });
    // Carga > Láminas (T-P7-04): same rules and the same shared window.
    m_shellLoadTool->setHost(m_nodalLoadTool);
    m_shellLoadTool->setDialogParent(this);
    connect(m_shellLoadTool, &ShellLoadTool::statusChanged, this, [this](const QString& text) { statusBar()->showMessage(text); });
    connect(m_shellLoadTool, &ShellLoadTool::aboutToActivate, this, [this, stopOthers] {
        stopOthers();
        m_generateTool->deactivate();
        m_referenceTool->deactivate();
        m_supportTool->deactivate();
        m_ligatureTool->deactivate();
        m_diaphragmTool->deactivate();
        m_barEndsTool->deactivate();
        m_bucklingTool->deactivate();
        m_deflectionTool->deactivate();
        m_shellPropsTool->deactivate();
        m_nodalLoadTool->deactivate();
        m_barLoadTool->deactivate();
        m_panelTool->deactivate();
    });
    // Carga > Paños / Cargas superficiales (T-P7-05): same rules and the same shared window.
    m_panelTool->setHost(m_nodalLoadTool);
    m_panelTool->setDialogParent(this);
    connect(m_panelTool, &PanelTool::statusChanged, this, [this](const QString& text) { statusBar()->showMessage(text); });
    connect(m_panelTool, &PanelTool::aboutToActivate, this, [this, stopOthers] {
        stopOthers();
        m_generateTool->deactivate();
        m_referenceTool->deactivate();
        m_supportTool->deactivate();
        m_ligatureTool->deactivate();
        m_diaphragmTool->deactivate();
        m_barEndsTool->deactivate();
        m_bucklingTool->deactivate();
        m_deflectionTool->deactivate();
        m_shellPropsTool->deactivate();
        m_nodalLoadTool->deactivate();
        m_barLoadTool->deactivate();
        m_shellLoadTool->deactivate();
    });
    const auto stopLoadsWhen = [this](auto* tool) {
        connect(tool, &std::remove_pointer_t<decltype(tool)>::aboutToActivate, m_nodalLoadTool, &NodalLoadTool::deactivate);
        connect(tool, &std::remove_pointer_t<decltype(tool)>::aboutToActivate, m_barLoadTool, &BarLoadTool::deactivate);
        connect(tool, &std::remove_pointer_t<decltype(tool)>::aboutToActivate, m_shellLoadTool, &ShellLoadTool::deactivate);
        connect(tool, &std::remove_pointer_t<decltype(tool)>::aboutToActivate, m_panelTool, &PanelTool::deactivate);
    };
    stopLoadsWhen(m_nodeTool);
    stopLoadsWhen(m_barTool);
    stopLoadsWhen(m_pieceTool);
    stopLoadsWhen(m_describeTool);
    stopLoadsWhen(m_continuousTool);
    stopLoadsWhen(m_shellTool);
    stopLoadsWhen(m_transformTool);
    stopLoadsWhen(m_generateTool);
    stopLoadsWhen(m_referenceTool);
    stopLoadsWhen(m_supportTool);
    stopLoadsWhen(m_ligatureTool);
    stopLoadsWhen(m_diaphragmTool);
    stopLoadsWhen(m_barEndsTool);
    stopLoadsWhen(m_bucklingTool);
    stopLoadsWhen(m_deflectionTool);
    stopLoadsWhen(m_shellPropsTool);
    // Cálculo > Tensión / Deformación (T-P8-06): its result modes exclude every other viewport tool.
    m_calculationTool->setDialogParent(this);
    connect(m_calculationTool, &CalculationTool::statusChanged, this, [this](const QString& text) { statusBar()->showMessage(text); });
    connect(m_calculationTool, &CalculationTool::aboutToActivate, this, [this, stopOthers] {
        stopOthers();
        m_generateTool->deactivate();
        m_referenceTool->deactivate();
        m_supportTool->deactivate();
        m_ligatureTool->deactivate();
        m_diaphragmTool->deactivate();
        m_barEndsTool->deactivate();
        m_bucklingTool->deactivate();
        m_deflectionTool->deactivate();
        m_shellPropsTool->deactivate();
        m_nodalLoadTool->deactivate();
        m_barLoadTool->deactivate();
        m_shellLoadTool->deactivate();
        m_panelTool->deactivate();
    });
    const auto stopResultsWhen = [this](auto* tool) {
        connect(tool, &std::remove_pointer_t<decltype(tool)>::aboutToActivate, m_calculationTool, &CalculationTool::deactivate);
    };
    stopResultsWhen(m_nodeTool);
    stopResultsWhen(m_barTool);
    stopResultsWhen(m_pieceTool);
    stopResultsWhen(m_describeTool);
    stopResultsWhen(m_continuousTool);
    stopResultsWhen(m_shellTool);
    stopResultsWhen(m_transformTool);
    stopResultsWhen(m_generateTool);
    stopResultsWhen(m_referenceTool);
    stopResultsWhen(m_supportTool);
    stopResultsWhen(m_ligatureTool);
    stopResultsWhen(m_diaphragmTool);
    stopResultsWhen(m_barEndsTool);
    stopResultsWhen(m_bucklingTool);
    stopResultsWhen(m_deflectionTool);
    stopResultsWhen(m_shellPropsTool);
    stopResultsWhen(m_nodalLoadTool);
    stopResultsWhen(m_barLoadTool);
    stopResultsWhen(m_shellLoadTool);
    stopResultsWhen(m_panelTool);
    connect(m_leftPanel->layersPanel(), &LayersPanel::assignToolToggled, m_calculationTool, [this](bool on) {
        if (on) m_calculationTool->deactivate();
    });
    // The nodal loads are drawn only while the ribbon tab «Carga» is current (cargas_nudos.md 2.3).
    connect(ribbonBar(), &SARibbonBar::currentRibbonTabChanged, this, [this](int index) { m_nodalLoadTool->setLoadsVisible(index == 3); });
    connect(m_nodeTool, &NodeTool::aboutToActivate, m_shellPropsTool, &ShellPropertiesTool::deactivate);
    connect(m_barTool, &BarTool::aboutToActivate, m_shellPropsTool, &ShellPropertiesTool::deactivate);
    connect(m_pieceTool, &PieceTool::aboutToActivate, m_shellPropsTool, &ShellPropertiesTool::deactivate);
    connect(m_describeTool, &BarDescribeTool::aboutToActivate, m_shellPropsTool, &ShellPropertiesTool::deactivate);
    connect(m_continuousTool, &ContinuousTool::aboutToActivate, m_shellPropsTool, &ShellPropertiesTool::deactivate);
    connect(m_shellTool, &ShellTool::aboutToActivate, m_shellPropsTool, &ShellPropertiesTool::deactivate);
    connect(m_transformTool, &TransformTool::aboutToActivate, m_shellPropsTool, &ShellPropertiesTool::deactivate);
    connect(m_generateTool, &GenerateTool::aboutToActivate, m_shellPropsTool, &ShellPropertiesTool::deactivate);
    connect(m_referenceTool, &ReferenceTool::aboutToActivate, m_shellPropsTool, &ShellPropertiesTool::deactivate);
    connect(m_supportTool, &SupportTool::aboutToActivate, m_shellPropsTool, &ShellPropertiesTool::deactivate);
    connect(m_ligatureTool, &LigatureTool::aboutToActivate, m_shellPropsTool, &ShellPropertiesTool::deactivate);
    connect(m_diaphragmTool, &DiaphragmTool::aboutToActivate, m_shellPropsTool, &ShellPropertiesTool::deactivate);
    connect(m_barEndsTool, &BarEndsTool::aboutToActivate, m_shellPropsTool, &ShellPropertiesTool::deactivate);
    connect(m_bucklingTool, &BucklingTool::aboutToActivate, m_shellPropsTool, &ShellPropertiesTool::deactivate);
    connect(m_deflectionTool, &DeflectionTool::aboutToActivate, m_shellPropsTool, &ShellPropertiesTool::deactivate);
    connect(m_supportTool, &SupportTool::aboutToActivate, m_diaphragmTool, &DiaphragmTool::deactivate);
    connect(m_supportTool, &SupportTool::aboutToActivate, m_barEndsTool, &BarEndsTool::deactivate);
    connect(m_supportTool, &SupportTool::aboutToActivate, m_bucklingTool, &BucklingTool::deactivate);
    connect(m_supportTool, &SupportTool::aboutToActivate, m_deflectionTool, &DeflectionTool::deactivate);
    connect(m_ligatureTool, &LigatureTool::aboutToActivate, m_diaphragmTool, &DiaphragmTool::deactivate);
    connect(m_ligatureTool, &LigatureTool::aboutToActivate, m_barEndsTool, &BarEndsTool::deactivate);
    connect(m_ligatureTool, &LigatureTool::aboutToActivate, m_bucklingTool, &BucklingTool::deactivate);
    connect(m_ligatureTool, &LigatureTool::aboutToActivate, m_deflectionTool, &DeflectionTool::deactivate);
    connect(m_nodeTool, &NodeTool::aboutToActivate, m_diaphragmTool, &DiaphragmTool::deactivate);
    connect(m_nodeTool, &NodeTool::aboutToActivate, m_barEndsTool, &BarEndsTool::deactivate);
    connect(m_nodeTool, &NodeTool::aboutToActivate, m_bucklingTool, &BucklingTool::deactivate);
    connect(m_nodeTool, &NodeTool::aboutToActivate, m_deflectionTool, &DeflectionTool::deactivate);
    connect(m_barTool, &BarTool::aboutToActivate, m_diaphragmTool, &DiaphragmTool::deactivate);
    connect(m_barTool, &BarTool::aboutToActivate, m_barEndsTool, &BarEndsTool::deactivate);
    connect(m_barTool, &BarTool::aboutToActivate, m_bucklingTool, &BucklingTool::deactivate);
    connect(m_barTool, &BarTool::aboutToActivate, m_deflectionTool, &DeflectionTool::deactivate);
    connect(m_pieceTool, &PieceTool::aboutToActivate, m_diaphragmTool, &DiaphragmTool::deactivate);
    connect(m_pieceTool, &PieceTool::aboutToActivate, m_barEndsTool, &BarEndsTool::deactivate);
    connect(m_pieceTool, &PieceTool::aboutToActivate, m_bucklingTool, &BucklingTool::deactivate);
    connect(m_pieceTool, &PieceTool::aboutToActivate, m_deflectionTool, &DeflectionTool::deactivate);
    connect(m_describeTool, &BarDescribeTool::aboutToActivate, m_diaphragmTool, &DiaphragmTool::deactivate);
    connect(m_describeTool, &BarDescribeTool::aboutToActivate, m_barEndsTool, &BarEndsTool::deactivate);
    connect(m_describeTool, &BarDescribeTool::aboutToActivate, m_bucklingTool, &BucklingTool::deactivate);
    connect(m_describeTool, &BarDescribeTool::aboutToActivate, m_deflectionTool, &DeflectionTool::deactivate);
    connect(m_continuousTool, &ContinuousTool::aboutToActivate, m_diaphragmTool, &DiaphragmTool::deactivate);
    connect(m_continuousTool, &ContinuousTool::aboutToActivate, m_barEndsTool, &BarEndsTool::deactivate);
    connect(m_continuousTool, &ContinuousTool::aboutToActivate, m_bucklingTool, &BucklingTool::deactivate);
    connect(m_continuousTool, &ContinuousTool::aboutToActivate, m_deflectionTool, &DeflectionTool::deactivate);
    connect(m_shellTool, &ShellTool::aboutToActivate, m_diaphragmTool, &DiaphragmTool::deactivate);
    connect(m_shellTool, &ShellTool::aboutToActivate, m_barEndsTool, &BarEndsTool::deactivate);
    connect(m_shellTool, &ShellTool::aboutToActivate, m_bucklingTool, &BucklingTool::deactivate);
    connect(m_shellTool, &ShellTool::aboutToActivate, m_deflectionTool, &DeflectionTool::deactivate);
    connect(m_transformTool, &TransformTool::aboutToActivate, m_diaphragmTool, &DiaphragmTool::deactivate);
    connect(m_transformTool, &TransformTool::aboutToActivate, m_barEndsTool, &BarEndsTool::deactivate);
    connect(m_transformTool, &TransformTool::aboutToActivate, m_bucklingTool, &BucklingTool::deactivate);
    connect(m_transformTool, &TransformTool::aboutToActivate, m_deflectionTool, &DeflectionTool::deactivate);
    connect(m_generateTool, &GenerateTool::aboutToActivate, m_diaphragmTool, &DiaphragmTool::deactivate);
    connect(m_generateTool, &GenerateTool::aboutToActivate, m_barEndsTool, &BarEndsTool::deactivate);
    connect(m_generateTool, &GenerateTool::aboutToActivate, m_bucklingTool, &BucklingTool::deactivate);
    connect(m_generateTool, &GenerateTool::aboutToActivate, m_deflectionTool, &DeflectionTool::deactivate);
    connect(m_referenceTool, &ReferenceTool::aboutToActivate, m_diaphragmTool, &DiaphragmTool::deactivate);
    connect(m_referenceTool, &ReferenceTool::aboutToActivate, m_barEndsTool, &BarEndsTool::deactivate);
    connect(m_referenceTool, &ReferenceTool::aboutToActivate, m_bucklingTool, &BucklingTool::deactivate);
    connect(m_referenceTool, &ReferenceTool::aboutToActivate, m_deflectionTool, &DeflectionTool::deactivate);
    connect(m_nodeTool, &NodeTool::aboutToActivate, m_supportTool, &SupportTool::deactivate);
    connect(m_nodeTool, &NodeTool::aboutToActivate, m_ligatureTool, &LigatureTool::deactivate);
    connect(m_barTool, &BarTool::aboutToActivate, m_supportTool, &SupportTool::deactivate);
    connect(m_barTool, &BarTool::aboutToActivate, m_ligatureTool, &LigatureTool::deactivate);
    connect(m_pieceTool, &PieceTool::aboutToActivate, m_supportTool, &SupportTool::deactivate);
    connect(m_pieceTool, &PieceTool::aboutToActivate, m_ligatureTool, &LigatureTool::deactivate);
    connect(m_describeTool, &BarDescribeTool::aboutToActivate, m_supportTool, &SupportTool::deactivate);
    connect(m_describeTool, &BarDescribeTool::aboutToActivate, m_ligatureTool, &LigatureTool::deactivate);
    connect(m_continuousTool, &ContinuousTool::aboutToActivate, m_supportTool, &SupportTool::deactivate);
    connect(m_continuousTool, &ContinuousTool::aboutToActivate, m_ligatureTool, &LigatureTool::deactivate);
    connect(m_shellTool, &ShellTool::aboutToActivate, m_supportTool, &SupportTool::deactivate);
    connect(m_shellTool, &ShellTool::aboutToActivate, m_ligatureTool, &LigatureTool::deactivate);
    connect(m_transformTool, &TransformTool::aboutToActivate, m_supportTool, &SupportTool::deactivate);
    connect(m_transformTool, &TransformTool::aboutToActivate, m_ligatureTool, &LigatureTool::deactivate);
    connect(m_generateTool, &GenerateTool::aboutToActivate, m_supportTool, &SupportTool::deactivate);
    connect(m_generateTool, &GenerateTool::aboutToActivate, m_ligatureTool, &LigatureTool::deactivate);
    connect(m_referenceTool, &ReferenceTool::aboutToActivate, m_supportTool, &SupportTool::deactivate);
    connect(m_referenceTool, &ReferenceTool::aboutToActivate, m_ligatureTool, &LigatureTool::deactivate);
    m_referenceTool->setDialogParent(this);
    connect(m_referenceTool, &ReferenceTool::statusChanged, this, [this](const QString& text) { statusBar()->showMessage(text); });
    connect(m_referenceTool, &ReferenceTool::aboutToActivate, this, [this, stopOthers] {
        stopOthers();
        m_generateTool->deactivate();
    });
    connect(m_referenceTool, &ReferenceTool::messageRequested, this, [this](const QString& text) { QMessageBox::warning(this, tr("Asignar referencia"), text); });
    m_importTool->setDialogParent(this);
    connect(m_importTool, &ImportTool::aboutToImport, this, [this] {
        m_leftPanel->layersPanel()->assignToolButton()->setChecked(false);
        m_nodeTool->deactivate();
        m_barTool->deactivate();
        m_pieceTool->deactivate();
        m_describeTool->deactivate();
        m_continuousTool->deactivate();
        m_shellTool->deactivate();
        m_transformTool->deactivate();
        m_generateTool->deactivate();
        m_referenceTool->deactivate();
        m_supportTool->deactivate();
        m_ligatureTool->deactivate();
        m_diaphragmTool->deactivate();
        m_barEndsTool->deactivate();
        m_bucklingTool->deactivate();
        m_deflectionTool->deactivate();
        m_shellPropsTool->deactivate();
        m_nodalLoadTool->deactivate();
        m_barLoadTool->deactivate();
        m_shellLoadTool->deactivate();
        m_panelTool->deactivate();
    });
    connect(m_importTool, &ImportTool::statusChanged, this, [this](const QString& text) { statusBar()->showMessage(text, 6000); });
    connect(m_importTool, &ImportTool::messageRequested, this, [this](const QString& title, const QString& text, bool error) {
        if (error) QMessageBox::warning(this, title, text);
        else QMessageBox::information(this, title, text);
    });
    m_importTool->setConfirmer([this](const QString& question) {
        return QMessageBox::question(this, tr("Importar"), question, QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes) == QMessageBox::Yes;
    });
    m_transformTool->setConfirmer([this](const QString& question) {
        return QMessageBox::question(this, tr("Reparar"), question, QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes) == QMessageBox::Yes;
    });
    setProjectOpen(false);

    // Project lifecycle arrives in T-P2-02..07; until then the entries only report their status.
    auto notYet = [this] { statusBar()->showMessage(tr("Esta función aún no está disponible."), 4000); };
    connect(m_startPage, &StartPage::newRequested, this, &MainWindow::newProject);
    connect(m_startPage, &StartPage::fileManagerRequested, this, &MainWindow::openFileManager);
    connect(m_startPage, &StartPage::examplesRequested, this, notYet);
    connect(m_startPage, &StartPage::moreRecentRequested, this, &MainWindow::showRecentFiles);
    connect(m_startPage, &StartPage::recentFileRequested, this, &MainWindow::openRecentFile);
    connect(m_startPage, &StartPage::helpRequested, this, notYet);
    connect(m_saveAction, &QAction::triggered, this, [this] { saveProject(); });
    connect(m_saveAsAction, &QAction::triggered, this, [this] { saveProjectAs(); });
    connect(m_undoAction, &QAction::triggered, this, [this] { m_undo.undo(); });
    connect(m_redoAction, &QAction::triggered, this, [this] { m_undo.redo(); });
    m_undo.setChangeCallback([this] {
        refreshUndoActions();
        syncViewSettings();
        if (m_projectOpen && m_document) {
            m_nodeTool->structureChanged();
            m_barTool->structureChanged();
            m_pieceTool->structureChanged();
            m_describeTool->structureChanged();
            m_continuousTool->structureChanged();
            m_shellTool->structureChanged();
            m_transformTool->structureChanged();
            m_generateTool->structureChanged();
            m_referenceTool->structureChanged();
            m_supportTool->structureChanged();
            m_ligatureTool->structureChanged();
            m_diaphragmTool->structureChanged();
            m_barEndsTool->structureChanged();
            m_bucklingTool->structureChanged();
            m_deflectionTool->structureChanged();
            m_shellPropsTool->structureChanged();
            m_nodalLoadTool->structureChanged();
            m_barLoadTool->structureChanged();
            m_shellLoadTool->structureChanged();
            m_panelTool->structureChanged();
            m_calculationTool->structureChanged();
            // The viewports follow the history (grouped planes are cleared by an import).
        }  // every push / undo / redo: node buttons and 3D windows follow
    });
    loadRecentFiles();
}

void MainWindow::openFileManager() {
    // Last folder used by "Nuevo", or the default projects folder.
    FileManagerDialog dialog(NewProjectDialog::defaultFolder(), this);
    QString chosen;
    bool wantsNew = false;
    connect(&dialog, &FileManagerDialog::openRequested, &dialog, [&](const QString& path) {
        chosen = path;
        dialog.accept();
    });
    connect(&dialog, &FileManagerDialog::newRequested, &dialog, [&] {
        wantsNew = true;
        dialog.accept();
    });
    dialog.exec();
    if (wantsNew) newProject();
    else if (!chosen.isEmpty()) openRecentFile(chosen);
}

void MainWindow::showRecentFiles() {
    QStringList all;
    for (const std::string& path : m_recent.paths()) all << QString::fromStdString(path);
    RecentFilesDialog dialog(all, this);
    if (dialog.exec() == QDialog::Accepted && !dialog.chosenFile().isEmpty()) openRecentFile(dialog.chosenFile());
}

void MainWindow::loadRecentFiles() {
    const QStringList stored = QSettings().value(QStringLiteral("recentFiles")).toStringList();
    std::vector<std::string> paths;
    for (const QString& path : stored) paths.push_back(path.toStdString());
    m_recent = cc::core::RecentFiles(std::move(paths));
    refreshRecentFiles();
}

void MainWindow::registerRecentFile(const QString& path) {
    m_recent.add(QDir::cleanPath(QFileInfo(path).absoluteFilePath()).toStdString());
    saveRecentFiles();
    refreshRecentFiles();
}

void MainWindow::saveRecentFiles() const {
    QStringList stored;
    for (const std::string& path : m_recent.paths()) stored << QString::fromStdString(path);
    QSettings().setValue(QStringLiteral("recentFiles"), stored);
}

void MainWindow::refreshRecentFiles() {
    QStringList shown, missing;
    for (const std::string& path : m_recent.top()) {
        shown << QString::fromStdString(path);
        if (!QFileInfo::exists(shown.back())) missing << shown.back();
    }
    m_startPage->setRecentFiles(shown, missing, m_recent.paths().size() > cc::core::RecentFiles::kStartPageSlots);
}

void MainWindow::openRecentFile(const QString& path) {
    if (!QFileInfo::exists(path)) {  // vanished since the list was drawn: grey it out, keep it in the list
        refreshRecentFiles();
        statusBar()->showMessage(tr("El fichero «%1» ya no existe.").arg(QDir::toNativeSeparators(path)), 6000);
        return;
    }
    openProjectFile(path);
}

bool MainWindow::openProjectFile(const QString& path) {
    const cc::core::LoadResult loaded = cc::core::loadProjectFile(toPath(path));
    if (!loaded.ok) {
        QMessageBox::warning(this, tr("Abrir obra"), QString::fromStdString(loaded.error));
        return false;
    }
    m_document = loaded.document;
    m_projectPath = QDir::cleanPath(QFileInfo(path).absoluteFilePath());
    setProjectOpen(true);
    m_undo.setClean();
    registerRecentFile(m_projectPath);
    updateWindowTitle();
    return true;
}

bool MainWindow::saveProject() {
    if (!m_document) return false;
    const cc::core::SaveResult saved = cc::core::saveProjectFile(toPath(m_projectPath), *m_document);
    if (!saved.ok) {
        QMessageBox::warning(this, tr("Guardar obra"), QString::fromStdString(saved.error));
        return false;
    }
    m_undo.setClean();
    registerRecentFile(m_projectPath);
    updateWindowTitle();
    statusBar()->showMessage(tr("Obra guardada en «%1».").arg(QDir::toNativeSeparators(m_projectPath)), 4000);
    return true;
}

bool MainWindow::saveProjectAs() {
    if (!m_document) return false;
    QString target = QFileDialog::getSaveFileName(this, tr("Guardar obra como"), m_projectPath, tr("Obra CYPE 3D (*.ed3)"));
    if (target.isEmpty()) return false;
    if (QFileInfo(target).suffix().isEmpty()) target += QStringLiteral(".ed3");
    const cc::core::SaveResult saved = cc::core::saveProjectFile(toPath(target), *m_document);
    if (!saved.ok) {
        QMessageBox::warning(this, tr("Guardar obra como"), QString::fromStdString(saved.error));
        return false;
    }
    m_projectPath = QDir::cleanPath(QFileInfo(target).absoluteFilePath());
    m_undo.setClean();
    registerRecentFile(m_projectPath);
    updateWindowTitle();
    statusBar()->showMessage(tr("Obra guardada en «%1».").arg(QDir::toNativeSeparators(m_projectPath)), 4000);
    return true;
}

bool MainWindow::maybeSave() {
    if (!isProjectModified()) return true;
    QMessageBox box(QMessageBox::Question, tr("Guardar obra"),
                    tr("La obra «%1» ha sido modificada. ¿Desea guardar los cambios?").arg(QFileInfo(m_projectPath).fileName()),
                    QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, this);
    box.button(QMessageBox::Save)->setText(tr("Guardar"));
    box.button(QMessageBox::Discard)->setText(tr("No guardar"));
    box.button(QMessageBox::Cancel)->setText(tr("Cancelar"));
    switch (box.exec()) {
        case QMessageBox::Save: return saveProject();
        case QMessageBox::Discard: return true;
        default: return false;
    }
}

void MainWindow::closeEvent(QCloseEvent* event) {
    if (maybeSave()) event->accept();
    else event->ignore();
}

void MainWindow::updateWindowTitle() {
    QString title = QStringLiteral("%1 v%2")
                        .arg(QString::fromUtf8(cc::core::productName().data(), static_cast<qsizetype>(cc::core::productName().size())),
                             QString::fromUtf8(cc::core::versionString().data(), static_cast<qsizetype>(cc::core::versionString().size())));
    if (m_projectOpen && m_document) {
        title += QStringLiteral(" - ") + QFileInfo(m_projectPath).fileName();
        if (!m_undo.isClean()) title += QLatin1Char('*');  // provisional marker: the one of the original is not documented
    }
    setWindowTitle(title);
    m_saveAction->setEnabled(m_projectOpen && m_document.has_value());
    m_saveAsAction->setEnabled(m_projectOpen && m_document.has_value());
}

void MainWindow::newProject() {
    NewProjectDialog dialog(NewProjectDialog::defaultFolder(), this);
    if (dialog.exec() != QDialog::Accepted) return;
    NewProjectDialog::rememberFolder(dialog.folder());
    ProjectTypeDialog typeDialog(this);
    if (typeDialog.exec() != QDialog::Accepted) return;
    EmptyProjectWizard wizard(this);
    // "Estados límite" follows "Terminar"; cancelling it returns to the wizard with its values intact.
    cc::core::LimitStatesSettings limitStatesValue;
    for (;;) {
        if (wizard.exec() != QDialog::Accepted) return;
        LimitStatesDialog limitStates(wizard.standards(), wizard.limitStates(), this);
        if (limitStates.exec() == QDialog::Accepted) {
            limitStatesValue = limitStates.value();
            break;
        }
    }
    // The file is written first and then opened from disk, so what the user sees is what was saved.
    cc::core::ProjectDocument document =
        cc::core::makeDocument(projectFrom(wizard, limitStatesValue), dialog.description().toStdString());
    const cc::core::SaveResult saved = cc::core::saveProjectFile(toPath(dialog.filePath()), document);
    if (!saved.ok) {
        QMessageBox::warning(this, tr("Nueva obra"), QString::fromStdString(saved.error));
        return;
    }
    openProjectFile(dialog.filePath());
}

void MainWindow::editGeneralData() {
    if (!m_projectOpen || !m_document) return;
    cc::core::Project& project = m_document->project;
    EmptyProjectWizard wizard(cc::core::generalDataOf(project), EmptyProjectWizard::Mode::Edit, this);
    wizard.setLoadsSource(&project);
    if (wizard.exec() != QDialog::Accepted) return;
    const cc::core::GeneralData edited = wizard.generalData();
    // Hypotheses deleted in "Hipótesis adicionales" take their loads with them (the window already asked when they had loads).
    std::vector<int> removed;
    for (const cc::core::LoadCase& before : project.loadCases.cases) {
        if (!cc::core::findLoadCase(edited.loadCases, before.id)) removed.push_back(before.id);
    }
    for (const int id : removed) cc::core::deleteLoadCase(project, m_undo, id);
    if (cc::core::sameGeneralData(edited, cc::core::generalDataOf(project))) return;  // nothing changed: not "modified"
    m_undo.push(std::make_unique<cc::core::EditGeneralDataCommand>(project, edited));
}

void MainWindow::syncViewSettings() {
    if (!m_document) return;
    const cc::core::Project& project = m_document->project;
    m_leftPanel->displayOptions()->setOptions(project.display);
    m_leftPanel->categoriesPanel()->setSettings(project.categories);
    for (QMdiSubWindow* sub : m_workArea->mdiArea()->subWindowList())
        if (auto* viewport = viewportOf(sub->widget())) {
            DisplayOptionsPanel::applyTo(*viewport, project.display);
            viewport->setCategorySettings(project.categories);
            viewport->setWorkPlanes(&project.workPlanes);
            viewport->setProfileSettings(&project.profiles);
            viewport->setNodeSupports(&project.nodeSupports);
        }
}

void MainWindow::refreshUndoActions() {
    const auto describe = [](QAction* action, const QString& verb, const std::string& text, bool enabled) {
        action->setEnabled(enabled);
        action->setStatusTip(enabled ? QStringLiteral("%1 %2").arg(verb, QString::fromStdString(text)) : verb);
    };
    describe(m_undoAction, tr("Deshacer"), m_undo.undoText(), m_projectOpen && m_undo.canUndo());
    describe(m_redoAction, tr("Rehacer"), m_undo.redoText(), m_projectOpen && m_undo.canRedo());
    updateWindowTitle();
}

void MainWindow::setProjectOpen(bool open) {
    m_projectOpen = open;
    if (!open) {
        m_document.reset();
        m_projectPath.clear();
    }
    m_leftPanel->layersPanel()->assignToolButton()->setChecked(false);
    m_nodeTool->setProject(m_document ? &m_document->project : nullptr, &m_undo);  // ends any node command
    m_barTool->setProject(m_document ? &m_document->project : nullptr, &m_undo);
    m_pieceTool->setProject(m_document ? &m_document->project : nullptr, &m_undo);
    m_describeTool->setProject(m_document ? &m_document->project : nullptr, &m_undo);
    m_continuousTool->setProject(m_document ? &m_document->project : nullptr, &m_undo);
    m_shellTool->setProject(m_document ? &m_document->project : nullptr, &m_undo);
    m_transformTool->setProject(m_document ? &m_document->project : nullptr, &m_undo);
    m_generateTool->setProject(m_document ? &m_document->project : nullptr, &m_undo);
    m_referenceTool->setProject(m_document ? &m_document->project : nullptr, &m_undo);
    m_supportTool->setProject(m_document ? &m_document->project : nullptr, &m_undo);
    m_ligatureTool->setProject(m_document ? &m_document->project : nullptr, &m_undo);
    m_diaphragmTool->setProject(m_document ? &m_document->project : nullptr, &m_undo);
    m_barEndsTool->setProject(m_document ? &m_document->project : nullptr, &m_undo);
    m_bucklingTool->setProject(m_document ? &m_document->project : nullptr, &m_undo);
    m_deflectionTool->setProject(m_document ? &m_document->project : nullptr, &m_undo);
    m_shellPropsTool->setProject(m_document ? &m_document->project : nullptr, &m_undo);
    m_nodalLoadTool->setProject(m_document ? &m_document->project : nullptr, &m_undo);
    m_barLoadTool->setProject(m_document ? &m_document->project : nullptr, &m_undo);
    m_shellLoadTool->setProject(m_document ? &m_document->project : nullptr, &m_undo);
    m_panelTool->setProject(m_document ? &m_document->project : nullptr, &m_undo);
    m_calculationTool->setProject(m_document ? &m_document->project : nullptr, &m_undo);
    m_importTool->setProject(m_document ? &m_document->project : nullptr, &m_undo);
    m_leftPanel->layersPanel()->setStructure(m_document ? &m_document->project.structure : nullptr);
    m_undo.clear();  // every project starts with an empty history
    refreshUndoActions();
    syncViewSettings();  // view settings travel with the project
    // The 3D window lives as long as the project: opened with it, closed with it.
    if (open && m_workArea->windowCount() == 0) {
        auto* frame = new ViewportFrame;
        Viewport3D* viewport = frame->viewport();
        applyViewTool(viewport);
        if (m_document) {
            viewport->setStructure(&m_document->project.structure);
            viewport->setWorkPlanes(&m_document->project.workPlanes);
            viewport->setProfileSettings(&m_document->project.profiles);
        }
        connect(viewport, &Viewport3D::actOnSelection, this, [this, viewport] {
            if (!m_leftPanel->layersPanel()->assignToolButton()->isChecked() || viewport->selection().empty()) return;
            m_leftPanel->layersPanel()->assignTo(viewport->selection().ids(cc::core::ElementKind::Bar),
                                                 viewport->selection().ids(cc::core::ElementKind::Shell));
            viewport->update();
        });
        connect(viewport, &Viewport3D::statusHint, this, [this](const QString& text) {
            if (text.isEmpty()) statusBar()->clearMessage();
            else statusBar()->showMessage(text);
        });
        DisplayOptionsPanel::applyTo(*viewport, m_leftPanel->displayOptions()->options());
        viewport->setCategorySettings(m_leftPanel->categoriesPanel()->settings());
        m_nodeTool->addViewport(viewport);
        m_barTool->addViewport(viewport);
        m_pieceTool->addViewport(viewport);
        m_describeTool->addViewport(viewport);
        m_continuousTool->addViewport(viewport);
        m_shellTool->addViewport(viewport);
        m_transformTool->addViewport(viewport);
        m_generateTool->addViewport(viewport);
        m_referenceTool->addViewport(viewport);
        m_supportTool->addViewport(viewport);
        m_ligatureTool->addViewport(viewport);
        m_diaphragmTool->addViewport(viewport);
        m_barEndsTool->addViewport(viewport);
        m_bucklingTool->addViewport(viewport);
        m_deflectionTool->addViewport(viewport);
        m_shellPropsTool->addViewport(viewport);
        m_nodalLoadTool->addViewport(viewport);
        m_barLoadTool->addViewport(viewport);
        m_shellLoadTool->addViewport(viewport);
        m_panelTool->addViewport(viewport);
        m_calculationTool->addViewport(viewport);
        m_workArea->addDocumentWindow(tr("3D"), frame);
    } else if (!open) {
        m_workArea->mdiArea()->closeAllSubWindows();
    }
    if (open) {
        m_nodeTool->structureChanged();  // enables Nuevo / Borrar / Mover and redraws a window kept from the previous project
        m_leftPanel->restoreSections();
    }
    m_leftPanel->setVisible(open);
    m_modeTabs->setVisible(open);
    updateViewActions();
    m_stack->setCurrentWidget(open ? static_cast<QWidget*>(m_workArea) : m_startPage);
    // Like the original, the ribbon shows no tabs until a project is open.
    ribbonBar()->setMinimumMode(!open);
    for (SARibbonCategory* category : ribbonBar()->categoryPages()) {
        if (open) {
            ribbonBar()->showCategory(category);
        } else {
            ribbonBar()->hideCategory(category);
        }
    }
    // After showing the tabs: showCategory leaves the last one selected, and the original opens on "Obra".
    if (open) ribbonBar()->setCurrentIndex(0);
    statusBar()->showMessage(open ? tr("Seleccione una opción del menú.")
                                  : tr("Para utilizar el programa, debe tener una obra abierta. Puede crear una obra nueva o abrir una existente."));
}

void MainWindow::createQuickAccessBar() {
    // Actions stay disabled until a project is open (project lifecycle arrives in phase 2/3).
    auto make = [this](const QString& icon, const QString& text, const QString& tip, const QKeySequence& key) {
        auto* action = new QAction(themedIcon(icon), text, this);
        action->setStatusTip(tip);
        action->setToolTip(key.isEmpty() ? text : QStringLiteral("%1 (%2)").arg(text, key.toString(QKeySequence::NativeText)));
        action->setShortcut(key);
        action->setEnabled(false);
        return action;
    };
    m_saveAction = make(QStringLiteral("save"), tr("Guardar"), tr("Guarda la obra actual."), QKeySequence::Save);
    m_saveAsAction = new QAction(tr("Guardar como..."), this);  // shortcut only: the entry point of the original is not captured
    m_saveAsAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_S));  // QKeySequence::SaveAs is empty on Windows
    m_saveAsAction->setStatusTip(tr("Guarda la obra actual con otro nombre."));
    m_saveAsAction->setEnabled(false);
    addAction(m_saveAsAction);
    m_undoAction = make(QStringLiteral("undo"), tr("Deshacer"), tr("Deshace la última acción."), QKeySequence::Undo);
    m_redoAction = make(QStringLiteral("redo"), tr("Rehacer"), tr("Rehace la última acción deshecha."), QKeySequence::Redo);
    m_printAction = make(QStringLiteral("print"), tr("Imprimir"), tr("Imprime la vista activa."), QKeySequence());  // Ctrl+P is "Ventana anterior"
    SARibbonQuickAccessBar* bar = ribbonBar()->quickAccessBar();
    for (QAction* action : {m_saveAction, m_undoAction, m_redoAction, m_printAction}) {
        bar->addAction(action);
    }
}

void MainWindow::createStatusBar() {
}

Viewport3D* MainWindow::activeViewport() const {
    QMdiSubWindow* sub = m_workArea ? m_workArea->mdiArea()->activeSubWindow() : nullptr;
    return sub ? viewportOf(sub->widget()) : nullptr;
}

void MainWindow::createZoomToolbar() {
    // Group of the top bar at x = 1310...1499 in the original (docs/research/ui/viewport_3d.md): beside the ribbon tabs.
    const auto make = [this](ZoomToolIcon icon, const QString& text, const QString& tip, const QKeySequence& key, bool checkable) {
        auto* action = new QAction(zoomToolIcon(icon), text, this);
        action->setToolTip(tip);
        action->setStatusTip(tip);
        action->setShortcut(key);
        action->setCheckable(checkable);
        action->setEnabled(false);
        return action;
    };
    m_previousViewAction = make(ZoomToolIcon::PreviousWindow, tr("Ventana anterior"), tr("Ventana anterior (Ctrl+P): recuperar la vista de dibujo anterior"),
                                QKeySequence(Qt::CTRL | Qt::Key_P), false);
    m_fullViewAction = make(ZoomToolIcon::FullWindow, tr("Ventana completa"), tr("Ventana completa (Ctrl+W): visualizar todo el dibujo; también doble clic con el botón central"),
                            QKeySequence(Qt::CTRL | Qt::Key_W), false);
    m_doubleViewAction = make(ZoomToolIcon::DoubleWindow, tr("Ventana doble"), tr("Ventana doble: ampliar al doble el tamaño de la vista respecto a 'Ventana completa'"),
                              QKeySequence(), false);
    m_redrawAction = make(ZoomToolIcon::Redraw, tr("Redibujar"), tr("Redibujar (Ctrl+R)"), QKeySequence(Qt::CTRL | Qt::Key_R), false);
    m_zoomMarkAction = make(ZoomToolIcon::ZoomMark, tr("Marcar zoom"), tr("Marcar zoom (Ctrl+M): ampliación pulsando sobre el dibujo; la rueda amplía y reduce"),
                            QKeySequence(Qt::CTRL | Qt::Key_M), true);
    m_moveImageAction = make(ZoomToolIcon::MoveImage, tr("Mover imagen"), tr("Mover imagen: pulsar y arrastrar con forma de mano; también botón central"),
                             QKeySequence(), true);
    m_orbitAction = make(ZoomToolIcon::Orbit3D, tr("Órbita 3D"),
                         tr("Órbita 3D: girar alrededor del pivote; con la opción 'Rotación alrededor de un punto' marcada el pivote es el punto bajo el cursor; F1 muestra/oculta los textos de ayuda"),
                         QKeySequence(), true);
    // Mover imagen, Órbita 3D and Marcar zoom are tools of the left mouse button: only one at a time, or none.
    m_viewTools = new QActionGroup(this);
    m_viewTools->setExclusionPolicy(QActionGroup::ExclusionPolicy::ExclusiveOptional);
    for (QAction* a : {m_zoomMarkAction, m_moveImageAction, m_orbitAction}) m_viewTools->addAction(a);

    SARibbonButtonGroupWidget* group = ribbonBar()->rightButtonGroup();
    for (QAction* a : {m_previousViewAction, m_fullViewAction, m_doubleViewAction, m_redrawAction, m_zoomMarkAction, m_moveImageAction,
                       m_orbitAction}) {
        group->addAction(a);
        addAction(a);  // shortcuts also work while the ribbon group is hidden
    }
    if (m_printAction) group->addAction(m_printAction);  // "Imprimir la vista actual" ends the group

    connect(m_previousViewAction, &QAction::triggered, this, [this] { if (auto* v = activeViewport()) v->previousView(); });
    connect(m_fullViewAction, &QAction::triggered, this, [this] { if (auto* v = activeViewport()) v->frameAll(); });
    connect(m_doubleViewAction, &QAction::triggered, this, [this] { if (auto* v = activeViewport()) v->doubleWindow(); });
    connect(m_redrawAction, &QAction::triggered, this, [this] { if (auto* v = activeViewport()) v->update(); });
    connect(m_viewTools, &QActionGroup::triggered, this, [this] {
        for (QMdiSubWindow* sub : m_workArea->mdiArea()->subWindowList())
            if (Viewport3D* v = viewportOf(sub->widget())) applyViewTool(v);
    });
}

void MainWindow::applyViewTool(Viewport3D* viewport) const {
    viewport->setZoomMarkMode(m_zoomMarkAction->isChecked());
    viewport->setLeftDragAction(m_orbitAction->isChecked() ? DragAction::Orbit : m_moveImageAction->isChecked() ? DragAction::Pan : DragAction::None);
}

void MainWindow::updateViewActions() {
    Viewport3D* v = activeViewport();
    const bool on = m_projectOpen && v != nullptr;
    for (QAction* a : {m_previousViewAction, m_fullViewAction, m_doubleViewAction, m_redrawAction, m_zoomMarkAction, m_moveImageAction,
                       m_orbitAction})
        a->setEnabled(on);
    if (on) m_previousViewAction->setEnabled(v->canGoPrevious());
    if (v) {
        // The history button follows the active viewport.
        disconnect(m_historyConnection);
        m_historyConnection = connect(v, &Viewport3D::historyChanged, this, [this, v] { m_previousViewAction->setEnabled(v->canGoPrevious()); });
    }
}

void MainWindow::applyTitleBarStyle() {
    SARibbonBar* ribbon = ribbonBar();
    ribbon->setTitleBarHeight(kTitleBarHeight);
    ribbon->setWindowTitleBackgroundBrush(QColor(kTitleRed));
    ribbon->setWindowTitleTextColor(Qt::white);
    ribbon->setWindowTitleAlignment(Qt::AlignCenter);

    if (SARibbonSystemButtonBar* buttons = windowButtonBar()) {
        buttons->setStyleSheet(QStringLiteral(
            "SARibbonSystemButtonBar { background: #B1291C; }"
            "SARibbonSystemToolButton { qproperty-glyphColor: white; background: transparent; }"
            "SARibbonSystemToolButton:hover { background: rgba(255,255,255,0.18); }"));
        buttons->installEventFilter(this);
    }
    m_sessionBar = new SessionBar(ribbon);
    ribbon->installEventFilter(this);
    placeSessionBar();
}

void MainWindow::placeSessionBar() {
    if (!m_sessionBar) return;
    int right = ribbonBar()->width();
    if (SARibbonSystemButtonBar* buttons = windowButtonBar()) {
        right = ribbonBar()->mapFromGlobal(buttons->mapToGlobal(QPoint(0, 0))).x();
    }
    m_sessionBar->adjustSize();
    const QSize size = m_sessionBar->sizeHint();
    m_sessionBar->setGeometry(right - size.width() - 8, (kTitleBarHeight - size.height()) / 2,
                              size.width(), size.height());
    m_sessionBar->raise();
}

bool MainWindow::eventFilter(QObject* watched, QEvent* event) {
    if (event->type() == QEvent::Resize || event->type() == QEvent::Move || event->type() == QEvent::Show) {
        placeSessionBar();
    }
    return SARibbonMainWindow::eventFilter(watched, event);
}

void MainWindow::editUnits() {
    if (!m_projectOpen || !m_document) return;
    cc::core::Project& project = m_document->project;
    UnitsDialog dialog(project.units, this);
    if (dialog.exec() != QDialog::Accepted) return;
    const cc::core::DimensioningUnits edited = dialog.value();
    if (edited == project.units) return;
    m_undo.push(std::make_unique<cc::core::EditUnitsCommand>(project, edited));
}

void MainWindow::editProfileLibrary() {
    if (!m_projectOpen || !m_document) return;
    cc::core::Project& project = m_document->project;
    ProfileLibraryDialog dialog(project.profileLibrary, this, project.units.dimensions);
    if (dialog.exec() != QDialog::Accepted) return;
    if (dialog.library() == project.profileLibrary) return;
    m_undo.push(std::make_unique<cc::core::ReplaceLibraryCommand>(project, dialog.library(), "Biblioteca de perfiles"));
}

void MainWindow::editGrid() {
    if (!m_projectOpen || !m_document) return;
    cc::core::Project& project = m_document->project;
    GridDialog dialog(project.workPlanes, this);
    if (dialog.exec() != QDialog::Accepted) return;
    const cc::core::WorkPlanes edited = dialog.value();
    if (edited == project.workPlanes) return;  // nothing changed: the project is not "modified"
    m_undo.push(std::make_unique<cc::core::EditWorkPlanesCommand>(project, edited, cc::core::WorkPlanesEdit::Grid));
}

void MainWindow::editLevels() {
    if (!m_projectOpen || !m_document) return;
    cc::core::Project& project = m_document->project;
    LevelsDialog dialog(project.workPlanes, this);
    if (dialog.exec() != QDialog::Accepted) return;
    const cc::core::WorkPlanes edited = dialog.value();
    if (edited == project.workPlanes) return;
    m_undo.push(std::make_unique<cc::core::EditWorkPlanesCommand>(project, edited, cc::core::WorkPlanesEdit::Levels));
}

void MainWindow::editPlanesVisibility() {
    if (!m_projectOpen || !m_document) return;
    cc::core::Project& project = m_document->project;
    PlanesVisibilityDialog dialog(this);  // opens on "Ocultar" like the original, whatever the current state
    if (dialog.exec() != QDialog::Accepted) return;
    cc::core::WorkPlanes edited = project.workPlanes;
    edited.visible = dialog.showPlanes();  // saved in the project (work_planes section); undoable
    if (edited == project.workPlanes) return;
    m_undo.push(std::make_unique<cc::core::EditWorkPlanesCommand>(project, edited, cc::core::WorkPlanesEdit::Visibility));
}

void MainWindow::editReferences() {
    if (!m_projectOpen || !m_document) return;
    cc::core::Project& project = m_document->project;
    ReferencesDialog dialog(project.workPlanes.references, this);
    if (dialog.exec() != QDialog::Accepted) return;
    cc::core::WorkPlanes edited = project.workPlanes;
    edited.references = dialog.value();
    if (edited == project.workPlanes) return;
    m_undo.push(std::make_unique<cc::core::EditWorkPlanesCommand>(project, edited, cc::core::WorkPlanesEdit::References));
}

void MainWindow::showDimensionsBar() {
    if (!m_projectOpen) return;
    if (!m_dimensionsBar) {
        m_dimensionsBar = new DimensionsBar(m_workArea);
        // Dimensioning needs nodes and elements (not developed yet): the buttons report it. While the pointer is over a
        // button the status bar shows the text of its command, as in the original.
        connect(m_dimensionsBar, &DimensionsBar::commandChosen, this,
                [this](DimensionsBar::Command) { statusBar()->showMessage(tr("Esta función aún no está disponible."), 4000); });
    }
    m_dimensionsBar->move(0, 160);  // top left of the work area, as measured in the original
    m_dimensionsBar->show();
    m_dimensionsBar->raise();
}

void MainWindow::buildGeometryTab() {
    SARibbonCategory* tab = ribbonBar()->categoryPages().at(1);
    const auto notYet = [this] { statusBar()->showMessage(tr("Esta función aún no está disponible."), 4000); };
    // Menu of the "Planos" button (docs/research/ui/planos.md section 11): Rejilla, Niveles, Cotas >, Referencias, Visibilidad,
    // Mostrar nuevos (a checkable entry), Agrupar, Desagrupar.
    auto* menu = new QMenu(tr("Planos"), this);
    menu->setIcon(themedIcon(QStringLiteral("planes")));
    const auto entry = [&](const QString& text, const QString& tip, auto handler) {
        QAction* action = menu->addAction(text);
        action->setStatusTip(tip);
        connect(action, &QAction::triggered, this, handler);
        return action;
    };
    m_gridAction = entry(tr("Rejilla"),
                         tr("Planos - Rejilla. Permite definir una rejilla compuesta por líneas de referencia etiquetadas que sirve como soporte para la introducción de elementos de la estructura."),
                         [this] { editGrid(); });
    m_levelsAction = entry(tr("Niveles"),
                           tr("Planos - Niveles. Permite editar los niveles de la obra. Los niveles son planos horizontales paralelos al plano xy global situados en una cota absoluta dada, que permiten introducir una subdivisión espacial propia de las estructuras de edificación. La asignación de los elementos a los distintos niveles es automática. Los elementos situados entre dos niveles se asignan al nivel inferior."),
                           [this] { editLevels(); });
    // "Cotas" is not a submenu in the original: it opens a floating bar with the four buttons Añadir/Borrar/Mostrar/Ocultar.
    entry(tr("Cotas"), tr("Planos - Cotas"), [this] { showDimensionsBar(); });
    entry(tr("Referencias"),
          tr("Planos - Referencias. Permite activar o desactivar algunas líneas de referencia de modo que sea más cómoda la visualización y acotación de la estructura."),
          [this] { editReferences(); });
    m_planesVisibilityAction = entry(tr("Visibilidad"), tr("Planos - Visibilidad"), [this] { editPlanesVisibility(); });
    m_showNewPlanesAction = menu->addAction(tr("Mostrar nuevos"));
    m_showNewPlanesAction->setCheckable(true);
    m_showNewPlanesAction->setChecked(true);  // checked by default and kept for the session, as in the original (no dialog)
    m_showNewPlanesAction->setStatusTip(tr("Seleccione una opción del menú."));  // the bar does not change (measured)
    entry(tr("Agrupar"), tr("Planos - Agrupar"), notYet);
    entry(tr("Desagrupar"), tr("Planos - Desagrupar"), notYet);
    tab->addPanel(tr("Planos"))->addLargeMenu(menu, QToolButton::InstantPopup);

    // "Nudos" (orig_geometria_ribbon.png): Nuevo large on the left, Borrar and Mover small and stacked on its right.
    m_nodeTool = new NodeTool(this);
    m_nodeTool->newAction()->setIcon(themedIcon(QStringLiteral("node_new")));
    m_nodeTool->deleteAction()->setIcon(themedIcon(QStringLiteral("node_delete")));
    m_nodeTool->moveAction()->setIcon(themedIcon(QStringLiteral("node_move")));
    SARibbonPanel* nodes = tab->addPanel(tr("Nudos"));
    nodes->addLargeAction(m_nodeTool->newAction());
    nodes->addSmallAction(m_nodeTool->deleteAction());
    nodes->addSmallAction(m_nodeTool->moveAction());

    // "Barras": Barra, Pilar and Viga (the other buttons of the group arrive with the rest of T-P5-03..).
    m_barTool = new BarTool(this);
    m_pieceTool = new PieceTool(this);
    m_describeTool = new BarDescribeTool(this);
    m_continuousTool = new ContinuousTool(this);
    m_barTool->barAction()->setIcon(themedIcon(QStringLiteral("bar_new")));
    m_barTool->columnAction()->setIcon(themedIcon(QStringLiteral("bar_column")));
    m_barTool->beamAction()->setIcon(themedIcon(QStringLiteral("bar_beam")));
    SARibbonPanel* bars = tab->addPanel(tr("Barras"));
    bars->addLargeAction(m_barTool->barAction());
    bars->addLargeAction(m_barTool->columnAction());
    bars->addLargeAction(m_barTool->beamAction());
    m_pieceTool->pieceAction()->setIcon(themedIcon(QStringLiteral("piece_new")));
    m_pieceTool->groupAction()->setIcon(themedIcon(QStringLiteral("piece_group")));
    m_pieceTool->ungroupAction()->setIcon(themedIcon(QStringLiteral("piece_ungroup")));
    m_pieceTool->moveEndAction()->setIcon(themedIcon(QStringLiteral("bar_move_end")));
    m_pieceTool->invertAction()->setIcon(themedIcon(QStringLiteral("bar_invert_axis")));
    m_describeTool->sectionAction()->setIcon(themedIcon(QStringLiteral("bar_section")));
    m_describeTool->materialAction()->setIcon(themedIcon(QStringLiteral("bar_material")));
    bars->addLargeAction(m_pieceTool->pieceAction());
    bars->addLargeAction(m_describeTool->sectionAction());
    bars->addLargeAction(m_describeTool->materialAction());
    bars->addLargeAction(m_pieceTool->groupAction());
    bars->addLargeAction(m_pieceTool->ungroupAction());
    bars->addLargeAction(m_pieceTool->moveEndAction());
    bars->addLargeAction(m_pieceTool->invertAction());
    // "Edición de pilares" / "Edición de vigas" (T-P5-08): drop-down buttons, grey while the work has no pillars / beams.
    m_continuousTool->pillarsMenu()->setIcon(themedIcon(QStringLiteral("edit_pillars")));
    m_continuousTool->beamsMenu()->setIcon(themedIcon(QStringLiteral("edit_beams")));
    bars->addLargeMenu(m_continuousTool->pillarsMenu(), QToolButton::InstantPopup);
    bars->addLargeMenu(m_continuousTool->beamsMenu(), QToolButton::InstantPopup);

    // "Láminas" (orig_geometria_ribbon.png): Nueva large, then three columns of three small buttons, then Ejes locales.
    m_shellTool = new ShellTool(this);
    const auto icon = [](QAction* action, const char* name) { action->setIcon(themedIcon(QString::fromLatin1(name))); };
    icon(m_shellTool->newAction(), "shell_new");
    icon(m_shellTool->deleteAction(), "shell_delete");
    icon(m_shellTool->sectionAction(), "shell_section");
    icon(m_shellTool->materialAction(), "shell_material");
    icon(m_shellTool->layoutAction(), "shell_layout");
    icon(m_shellTool->moveAction(), "shell_move");
    icon(m_shellTool->joinAction(), "shell_join");
    icon(m_shellTool->divideAction(), "shell_divide");
    icon(m_shellTool->invertZAction(), "shell_invert_z");
    icon(m_shellTool->defineXAction(), "shell_define_x");
    icon(m_shellTool->localAxesAction(), "shell_axes");
    SARibbonPanel* shells = tab->addPanel(tr("Láminas"));
    shells->addLargeAction(m_shellTool->newAction());
    for (QAction* action : {m_shellTool->deleteAction(), m_shellTool->sectionAction(), m_shellTool->materialAction(), m_shellTool->layoutAction(),
                            m_shellTool->moveAction(), m_shellTool->joinAction(), m_shellTool->divideAction(), m_shellTool->invertZAction(),
                            m_shellTool->defineXAction(), m_shellTool->localAxesAction()})
        shells->addSmallAction(action);

    // "Herramientas": Generar (drop-down) large, the icons in two rows (the last two are "Asignar referencia", ABC with a pencil, and
    // "Buscar", the magnifier: herramientas.md 6.1), Reparar large.
    m_transformTool = new TransformTool(this);
    m_generateTool = new GenerateTool(this);
    m_referenceTool = new ReferenceTool(this);
    m_generateTool->menu()->setIcon(themedIcon(QStringLiteral("tool_generate")));
    icon(m_referenceTool->assignAction(), "tool_assign_reference");
    icon(m_referenceTool->findAction(), "tool_find");
    icon(m_transformTool->moveAction(), "tool_move");
    icon(m_transformTool->copyAction(), "tool_copy");
    icon(m_transformTool->rotateAxisAction(), "tool_rotate_axis");
    icon(m_transformTool->rotateXYAction(), "tool_rotate_xy");
    icon(m_transformTool->rotateXY3Action(), "tool_rotate_xy3");
    icon(m_transformTool->mirrorXYAction(), "tool_mirror_xy");
    icon(m_transformTool->mirrorPlaneAction(), "tool_mirror_plane");
    icon(m_transformTool->repairAction(), "tool_repair");
    SARibbonPanel* tools = tab->addPanel(tr("Herramientas"));
    tools->addLargeMenu(m_generateTool->menu(), QToolButton::InstantPopup);
    // Two rows filled column by column, as in the original: Mover / Girar xy, Copiar / Simetría xy, Girar eje / Girar xy (3 puntos),
    // Simetría respecto de un plano / Buscar, Asignar referencia.
    for (QAction* action : {m_transformTool->moveAction(), m_transformTool->rotateXYAction(), m_transformTool->copyAction(),
                            m_transformTool->mirrorXYAction(), m_transformTool->rotateAxisAction(), m_transformTool->rotateXY3Action(),
                            m_transformTool->mirrorPlaneAction(), m_referenceTool->findAction(), m_referenceTool->assignAction()})
    {
        tools->addMediumAction(action);
        // Icon only, like the original (the title is the tooltip).
        if (SARibbonToolButton* button = tools->actionToRibbonToolButton(action)) button->setToolButtonStyle(Qt::ToolButtonIconOnly);
    }
    tools->addLargeAction(m_transformTool->repairAction());

    // "Importar": DXF o DWG and Fichero de texto, two small buttons with text stacked (orig_geometria_ribbon.png).
    m_importTool = new ImportTool(this);
    icon(m_importTool->dxfAction(), "import_dxf");
    icon(m_importTool->textAction(), "import_text");
    SARibbonPanel* import = tab->addPanel(tr("Importar"));
    import->addSmallAction(m_importTool->dxfAction());
    import->addSmallAction(m_importTool->textAction());

    // Propiedades > Nudos (orig_vinc_ext_dialogo_inicial.png): Vinculación interior and Vinculación exterior, two large buttons.
    m_supportTool = new SupportTool(this);
    icon(m_supportTool->internalAction(), "support_internal");
    icon(m_supportTool->externalAction(), "support_external");
    // Propiedades > Diafragmas (orig_diaf_01): Editar, Diafragma and Generar por niveles, three small buttons with text stacked; the panel is
    // the first of the tab, before Nudos and Barras.
    m_diaphragmTool = new DiaphragmTool(this);
    icon(m_diaphragmTool->editAction(), "diaphragm_edit");
    icon(m_diaphragmTool->assignAction(), "diaphragm_assign");
    icon(m_diaphragmTool->generateAction(), "diaphragm_generate");
    SARibbonPanel* diaphragms = ribbonBar()->categoryPages().at(2)->addPanel(tr("Diafragmas"));
    diaphragms->addSmallAction(m_diaphragmTool->editAction());
    diaphragms->addSmallAction(m_diaphragmTool->assignAction());
    diaphragms->addSmallAction(m_diaphragmTool->generateAction());
    SARibbonPanel* supports = ribbonBar()->categoryPages().at(2)->addPanel(tr("Nudos"));
    supports->addLargeAction(m_supportTool->internalAction());
    supports->addLargeAction(m_supportTool->externalAction());
    m_ligatureTool = new LigatureTool(this);
    icon(m_ligatureTool->action(), "support_ligature");
    supports->addLargeAction(m_ligatureTool->action());
    // Propiedades > Barras (orig_diaf_01): Articular and Empotramiento (the other buttons of the group belong to later tasks).
    m_barEndsTool = new BarEndsTool(this);
    icon(m_barEndsTool->articulateAction(), "bar_pin");
    icon(m_barEndsTool->fixityAction(), "bar_fixity");
    m_bucklingTool = new BucklingTool(this);
    icon(m_bucklingTool->betaAction(), "bar_buckling");
    icon(m_bucklingTool->lateralAction(), "bar_lateral_buckling");
    m_deflectionTool = new DeflectionTool(this);
    m_shellPropsTool = new ShellPropertiesTool(this);
    icon(m_shellPropsTool->interiorAction(), "shell_edge_interior");
    icon(m_shellPropsTool->exteriorAction(), "shell_edge_exterior");
    icon(m_shellPropsTool->bandsAction(), "shell_bands");
    icon(m_shellPropsTool->discretisationAction(), "shell_discretisation");
    icon(m_shellPropsTool->stiffnessAction(), "shell_stiffness");
    icon(m_shellPropsTool->bucklingAction(), "shell_buckling");
    icon(m_shellPropsTool->modalAction(), "shell_modal");
    SARibbonPanel* barsPanel = ribbonBar()->categoryPages().at(2)->addPanel(tr("Barras"));
    // Column order of the original (orig_pandeo_05): Pandeo / Pandeo lateral, then Articular / Empotramiento.
    barsPanel->addSmallAction(m_bucklingTool->betaAction());
    barsPanel->addSmallAction(m_bucklingTool->lateralAction());
    barsPanel->addSmallAction(m_barEndsTool->articulateAction());
    barsPanel->addSmallAction(m_barEndsTool->fixityAction());
    // Third column: the drop-down «Grupos de flecha» (Crear / Editar / Flecha Límite); the buttons of the other tasks follow it.
    barsPanel->addSmallMenu(m_deflectionTool->menu(), QToolButton::InstantPopup);
    // Propiedades > Laminas (orig_propiedades_ribbon.png, p7_21): three large buttons, a column of three small ones and Analisis modal.
    SARibbonPanel* shellProps = ribbonBar()->categoryPages().at(2)->addPanel(tr("Láminas"));
    shellProps->addLargeAction(m_shellPropsTool->interiorAction());
    shellProps->addLargeAction(m_shellPropsTool->exteriorAction());
    shellProps->addLargeAction(m_shellPropsTool->bandsAction());
    shellProps->addSmallAction(m_shellPropsTool->discretisationAction());
    shellProps->addSmallAction(m_shellPropsTool->stiffnessAction());
    shellProps->addSmallAction(m_shellPropsTool->bucklingAction());
    shellProps->addSmallAction(m_shellPropsTool->modalAction());

    // Carga > Visualización and Nudos (cn_c1): Hipótesis vista and Escalas; then Desplazamientos prescritos, Nueva and the column Editar /
    // Hipótesis / Borrar. The other panels of the tab (Barras, Láminas, Paños, Cargas superficiales) belong to later tasks.
    m_nodalLoadTool = new NodalLoadTool(this);
    icon(m_nodalLoadTool->viewAction(), "load_view_case");
    icon(m_nodalLoadTool->scalesAction(), "load_scales");
    icon(m_nodalLoadTool->prescribedAction(), "load_prescribed");
    icon(m_nodalLoadTool->newAction(), "node_load_new");
    icon(m_nodalLoadTool->editAction(), "node_load_edit");
    icon(m_nodalLoadTool->caseAction(), "node_load_case");
    icon(m_nodalLoadTool->deleteAction(), "node_load_delete");
    SARibbonCategory* loadTab = ribbonBar()->categoryPages().at(3);
    SARibbonPanel* loadView = loadTab->addPanel(tr("Visualización"));
    loadView->addLargeAction(m_nodalLoadTool->viewAction());
    loadView->addLargeAction(m_nodalLoadTool->scalesAction());
    SARibbonPanel* loadNodes = loadTab->addPanel(tr("Nudos"));
    loadNodes->addLargeAction(m_nodalLoadTool->prescribedAction());
    loadNodes->addLargeAction(m_nodalLoadTool->newAction());
    loadNodes->addSmallAction(m_nodalLoadTool->editAction());
    loadNodes->addSmallAction(m_nodalLoadTool->caseAction());
    loadNodes->addSmallAction(m_nodalLoadTool->deleteAction());

    // Carga > Barras (cb_b0): Nueva, then the columns Editar / Borrar and Hipótesis / Peso propio (no separator is visible, research 10.1).
    m_barLoadTool = new BarLoadTool(this);
    icon(m_barLoadTool->newAction(), "bar_load_new");
    icon(m_barLoadTool->editAction(), "bar_load_edit");
    icon(m_barLoadTool->deleteAction(), "bar_load_delete");
    icon(m_barLoadTool->caseAction(), "bar_load_case");
    icon(m_barLoadTool->selfWeightAction(), "bar_load_selfweight");
    SARibbonPanel* loadBars = loadTab->addPanel(tr("Barras"));
    loadBars->addLargeAction(m_barLoadTool->newAction());
    loadBars->addMediumAction(m_barLoadTool->editAction());
    loadBars->addMediumAction(m_barLoadTool->deleteAction());
    loadBars->addMediumAction(m_barLoadTool->caseAction());
    loadBars->addMediumAction(m_barLoadTool->selfWeightAction());

    // Carga > Láminas (cl_a, measured): Nueva (large), the columns Editar / Borrar and Hipótesis / Peso propio.
    m_shellLoadTool = new ShellLoadTool(this);
    icon(m_shellLoadTool->newAction(), "shell_load_new");
    icon(m_shellLoadTool->editAction(), "shell_load_edit");
    icon(m_shellLoadTool->deleteAction(), "shell_load_delete");
    icon(m_shellLoadTool->caseAction(), "shell_load_case");
    icon(m_shellLoadTool->selfWeightAction(), "shell_load_selfweight");
    SARibbonPanel* loadShells = loadTab->addPanel(tr("Láminas"));
    loadShells->addLargeAction(m_shellLoadTool->newAction());
    loadShells->addMediumAction(m_shellLoadTool->editAction());
    loadShells->addMediumAction(m_shellLoadTool->deleteAction());
    loadShells->addMediumAction(m_shellLoadTool->caseAction());
    loadShells->addMediumAction(m_shellLoadTool->selfWeightAction());

    // Carga > Paños and Cargas superficiales (cp_*, measured): Nuevo (large) | Editar / Borrar (stacked) | Mover | Dividir | Dirección de reparto (large);
    // Nueva | Editar / Borrar | Hipótesis (small, alone) | Copiar | Mover | Asignar (large).
    m_panelTool = new PanelTool(this);
    using PM = PanelTool::Mode;
    icon(m_panelTool->action(PM::PanelNew), "panel_new");
    icon(m_panelTool->action(PM::PanelEdit), "panel_edit");
    icon(m_panelTool->action(PM::PanelDelete), "panel_delete");
    icon(m_panelTool->action(PM::PanelMove), "panel_move");
    icon(m_panelTool->action(PM::PanelDivide), "panel_divide");
    icon(m_panelTool->action(PM::PanelDirection), "panel_direction");
    icon(m_panelTool->action(PM::SurfNew), "surface_load_new");
    icon(m_panelTool->action(PM::SurfEdit), "surface_load_edit");
    icon(m_panelTool->action(PM::SurfDelete), "surface_load_delete");
    icon(m_panelTool->action(PM::SurfCase), "surface_load_case");
    icon(m_panelTool->action(PM::SurfCopy), "surface_load_copy");
    icon(m_panelTool->action(PM::SurfMove), "surface_load_move");
    icon(m_panelTool->action(PM::SurfAssign), "surface_load_assign");
    SARibbonPanel* loadPanels = loadTab->addPanel(tr("Paños"));
    loadPanels->addLargeAction(m_panelTool->action(PM::PanelNew));
    loadPanels->addMediumAction(m_panelTool->action(PM::PanelEdit));
    loadPanels->addMediumAction(m_panelTool->action(PM::PanelDelete));
    loadPanels->addLargeAction(m_panelTool->action(PM::PanelMove));
    loadPanels->addLargeAction(m_panelTool->action(PM::PanelDivide));
    loadPanels->addLargeAction(m_panelTool->action(PM::PanelDirection));
    SARibbonPanel* loadSurfaces = loadTab->addPanel(tr("Cargas superficiales"));
    loadSurfaces->addLargeAction(m_panelTool->action(PM::SurfNew));
    loadSurfaces->addMediumAction(m_panelTool->action(PM::SurfEdit));
    loadSurfaces->addMediumAction(m_panelTool->action(PM::SurfDelete));
    loadSurfaces->addSmallAction(m_panelTool->action(PM::SurfCase));
    loadSurfaces->addLargeAction(m_panelTool->action(PM::SurfCopy));
    loadSurfaces->addLargeAction(m_panelTool->action(PM::SurfMove));
    loadSurfaces->addLargeAction(m_panelTool->action(PM::SurfAssign));
    // Measured: Dividir and Hipótesis show no tooltip at all (the action would fall back to its text): swallow the tooltip events of those buttons.
    struct NoTooltip : QObject {
        using QObject::QObject;
        bool eventFilter(QObject*, QEvent* event) override { return event->type() == QEvent::ToolTip; }
    };
    auto* noTooltip = new NoTooltip(this);
    for (QToolButton* button : loadTab->findChildren<QToolButton*>())
        for (QAction* quiet : m_panelTool->actionsWithoutTooltip())
            if (button->defaultAction() == quiet) button->installEventFilter(noTooltip);
}

void MainWindow::buildObraTab() {
    SARibbonCategory* tab = ribbonBar()->categoryPages().first();
    const auto notYet = [this] { statusBar()->showMessage(tr("Esta función aún no está disponible."), 4000); };
    // `enabled` follows the original on an empty project (Vista 3D and Listados need selected elements, Desvincular,
    // Consultar and Compartir need a linked BIMserver.center project).
    const auto make = [&](const QString& icon, const QString& text, bool enabled, auto handler) {
        auto* action = new QAction(themedIcon(icon), text, this);
        action->setEnabled(enabled);
        if (enabled) connect(action, &QAction::triggered, this, handler);
        return action;
    };

    // (no title) Datos generales
    m_generalDataAction = make(QStringLiteral("data_general"), tr("Datos generales"), true, [this] { editGeneralData(); });
    m_generalDataAction->setStatusTip(tr("Datos generales de la obra"));
    tab->addPanel(QString())->addLargeAction(m_generalDataAction);

    // Obra: three rows of small buttons, two columns
    SARibbonPanel* obra = tab->addPanel(tr("Obra"));
    obra->addSmallAction(make(QStringLiteral("profile_library"), tr("Biblioteca de perfiles"), true, [this] { editProfileLibrary(); }));
    obra->addSmallAction(make(QStringLiteral("manufacturer_catalogs"), tr("Catálogos de fabricante"), true, notYet));
    obra->addSmallAction(make(QStringLiteral("plastic_hinges"), tr("Biblioteca de rótulas plásticas"), true, notYet));
    obra->addSmallAction(make(QStringLiteral("performance"), tr("Rendimiento"), true, notYet));
    m_unitsAction = make(QStringLiteral("dimension_units"), tr("Unidades de acotación"), true, [this] { editUnits(); });
    obra->addSmallAction(m_unitsAction);

    SARibbonPanel* numbering = tab->addPanel(tr("Numeración"));
    numbering->addLargeAction(make(QStringLiteral("numbering_edit"), tr("Editar"), true, notYet));
    numbering->addLargeAction(make(QStringLiteral("numbering_reset"), tr("Reiniciar"), true, notYet));

    SARibbonPanel* view3d = tab->addPanel(tr("Vista 3D"));
    view3d->addLargeAction(make(QStringLiteral("view_full"), tr("Estructura completa"), false, notYet));
    view3d->addLargeAction(make(QStringLiteral("view_active"), tr("Ventana activa"), false, notYet));
    view3d->addLargeAction(make(QStringLiteral("view_selection"), tr("Selección de elementos"), false, notYet));
    auto* joints = make(QStringLiteral("joints"), tr("Uniones"), true, notYet);
    joints->setCheckable(true);
    joints->setChecked(true);  // pressed on a new project in the original
    // A pressed ribbon button is light blue in the original (the default style is dark blue and hides the icon).
    ribbonBar()->setStyleSheet(QStringLiteral(
        "SARibbonToolButton:checked { background-color: #CDE6F7; border: 1px solid #9CCBEA; color: black; }"));
    view3d->addLargeAction(joints);

    SARibbonPanel* lists = tab->addPanel(tr("Listados"));
    lists->addLargeAction(make(QStringLiteral("list_selected"), tr("Elementos seleccionados"), false, notYet));
    lists->addLargeAction(make(QStringLiteral("list_csv"), tr("CSV de los elementos seleccionados"), false, notYet));
    lists->addLargeAction(make(QStringLiteral("list_beams"), tr("Etiquetas de vigas"), false, notYet));

    SARibbonPanel* bim = tab->addPanel(tr("BIMserver.center"));
    bim->addLargeAction(make(QStringLiteral("bim_link"), tr("Vincular"), true, notYet));
    bim->addLargeAction(make(QStringLiteral("bim_unlink"), tr("Desvincular"), false, notYet));
    bim->addLargeAction(make(QStringLiteral("bim_query"), tr("Consultar"), false, notYet));
    bim->addLargeAction(make(QStringLiteral("bim_share"), tr("Compartir"), false, notYet));
}

void MainWindow::buildRibbonTabs() {
    SARibbonBar* ribbon = ribbonBar();
    // Order and names follow the original application (docs/research/02_ui_inventory.md).
    const QStringList tabs = {tr("Obra"),     tr("Geometría"), tr("Propiedades"), tr("Carga"),
                              tr("Uniones"),  tr("Cálculo"),   tr("Armaduras"),   tr("Ventana")};
    for (const QString& title : tabs) {
        ribbon->addCategoryPage(title);
    }
    buildObraTab();
    buildGeometryTab();
    buildCalculationTab();
}

void MainWindow::buildCalculationTab() {
    SARibbonCategory* tab = ribbonBar()->categoryPages().at(5);
    const auto notYet = [this] { statusBar()->showMessage(tr("Esta función aún no está disponible."), 4000); };
    // `enabled` = state of the original on a calculated work without checks (s_5.png / rp_02_postcalc.png); the buttons of later tasks report
    // that they are not available yet.
    const auto make = [&](const QString& icon, const QString& text, bool enabled) {
        auto* action = new QAction(themedIcon(icon), text, this);
        action->setEnabled(enabled);
        if (enabled) connect(action, &QAction::triggered, this, notYet);
        return action;
    };
    m_calculationTool = new CalculationTool(this);
    m_calculationTool->nonlinearAction()->setIcon(themedIcon(QStringLiteral("calc_nonlinear")));
    m_calculationTool->calculateAction()->setIcon(themedIcon(QStringLiteral("calc_run")));
    m_calculationTool->displacementsAction()->setIcon(themedIcon(QStringLiteral("calc_displacements")));
    m_calculationTool->reactionsAction()->setIcon(themedIcon(QStringLiteral("calc_reactions")));
    m_calculationTool->forcesAction()->setIcon(themedIcon(QStringLiteral("calc_forces")));
    SARibbonPanel* stress = tab->addPanel(tr("Tensión / Deformación"));
    stress->addLargeAction(m_calculationTool->nonlinearAction());
    stress->addLargeAction(m_calculationTool->calculateAction());
    stress->addLargeAction(make(QStringLiteral("calc_design"), tr("Dimensionar"), false));
    stress->addSmallAction(m_calculationTool->displacementsAction());
    stress->addSmallAction(m_calculationTool->reactionsAction());
    stress->addSmallAction(m_calculationTool->forcesAction());
    stress->addSmallAction(make(QStringLiteral("calc_isovalues"), tr("Isovalores"), false));    // T-P8-07
    stress->addSmallAction(make(QStringLiteral("calc_lacing"), tr("Empresillado"), false));     // T-P8-07
    stress->addLargeAction(make(QStringLiteral("calc_seismic"), tr("Sismo dinámico"), false));  // phase 10
    stress->addLargeAction(make(QStringLiteral("calc_check"), tr("Comprobar elementos"), false));       // phase 9
    stress->addLargeAction(make(QStringLiteral("calc_uls"), tr("Comprobaciones E.L.U."), false));      // phase 9

    SARibbonPanel* buckling = tab->addPanel(tr("Pandeo"));
    buckling->addLargeAction(make(QStringLiteral("buckling_groups"), tr("Grupos de análisis"), true));
    buckling->addLargeAction(make(QStringLiteral("buckling_combinations"), tr("Combinaciones"), true));
    buckling->addLargeAction(make(QStringLiteral("buckling_calc"), tr("Calcular"), true));
    buckling->addSmallAction(make(QStringLiteral("buckling_modes"), tr("Modos de pandeo"), false));
    buckling->addSmallAction(make(QStringLiteral("calc_isovalues"), tr("Isovalores"), false));
    buckling->addSmallAction(make(QStringLiteral("buckling_coefficients"), tr("Asignar coeficientes"), false));

    SARibbonPanel* modal = tab->addPanel(tr("Modal"));
    modal->addLargeAction(make(QStringLiteral("modal_cases"), tr("Casos de carga"), true));
    modal->addLargeAction(make(QStringLiteral("modal_calc"), tr("Calcular"), true));
    modal->addLargeAction(make(QStringLiteral("modal_modes"), tr("Modos de vibración"), false));
    modal->addLargeAction(make(QStringLiteral("modal_list"), tr("Listado"), false));

    SARibbonPanel* pushover = tab->addPanel(tr("Pushover"));
    pushover->addLargeAction(make(QStringLiteral("pushover_cases"), tr("Casos de carga"), true));
    pushover->addLargeAction(make(QStringLiteral("pushover_control"), tr("Nudo de control"), true));
    pushover->addLargeAction(make(QStringLiteral("pushover_calc"), tr("Calcular"), true));
    pushover->addSmallAction(make(QStringLiteral("calc_displacements"), tr("Desplazamientos"), false));
    pushover->addSmallAction(make(QStringLiteral("calc_reactions"), tr("Reacciones"), false));
    pushover->addSmallAction(make(QStringLiteral("calc_forces"), tr("Esfuerzos"), false));
    pushover->addSmallAction(make(QStringLiteral("calc_isovalues"), tr("Isovalores"), false));
    pushover->addSmallAction(make(QStringLiteral("pushover_capacity"), tr("Capacidad"), false));
    pushover->addSmallAction(make(QStringLiteral("pushover_hinges"), tr("Rótulas plásticas"), false));

    tab->addPanel(QString())->addLargeAction(make(QStringLiteral("incidences"), tr("Incidencias"), true));
}

}  // namespace cc::app





