#include "MainWindow.h"

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
#include "UnitsDialog.h"
#include "ProjectTypeDialog.h"
#include "StartPage.h"
#include <QMdiArea>
#include "Viewport3D.h"
#include "WorkArea.h"
#include "core/general_data.h"
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
    return project;  // units keep their default (cm / cm / m)
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
    createStatusBar();
    createWorkArea();
}

void MainWindow::createWorkArea() {
    m_stack = new QStackedWidget(this);
    m_startPage = new StartPage(m_stack);
    m_workArea = new WorkArea(m_stack);
    m_stack->addWidget(m_startPage);
    m_stack->addWidget(m_workArea);
    setCentralWidget(m_stack);
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
    m_undo.setChangeCallback([this] { refreshUndoActions(); });
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
    if (wizard.exec() != QDialog::Accepted) return;
    const cc::core::GeneralData edited = wizard.generalData();
    if (cc::core::sameGeneralData(edited, cc::core::generalDataOf(project))) return;  // nothing changed: not "modified"
    m_undo.push(std::make_unique<cc::core::EditGeneralDataCommand>(project, edited));
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
    m_undo.clear();  // every project starts with an empty history
    refreshUndoActions();
    // The 3D window lives as long as the project: opened with it, closed with it.
    if (open && m_workArea->windowCount() == 0) {
        auto* viewport = new Viewport3D;
        if (m_document) {
            std::vector<cc::core::Vec3> nodes;
            for (const cc::core::Node& n : m_document->project.structure.nodes) nodes.push_back({n.position.x, n.position.y, n.position.z});
            viewport->setNodes(std::move(nodes));
        }
        m_workArea->addDocumentWindow(tr("3D"), viewport);
    } else if (!open) {
        m_workArea->mdiArea()->closeAllSubWindows();
    }
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
        action->setToolTip(QStringLiteral("%1 (%2)").arg(text, key.toString(QKeySequence::NativeText)));
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
    m_printAction = make(QStringLiteral("print"), tr("Imprimir"), tr("Imprime la vista activa."), QKeySequence::Print);
    SARibbonQuickAccessBar* bar = ribbonBar()->quickAccessBar();
    for (QAction* action : {m_saveAction, m_undoAction, m_redoAction, m_printAction}) {
        bar->addAction(action);
    }
}

void MainWindow::createStatusBar() {
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
    obra->addSmallAction(make(QStringLiteral("profile_library"), tr("Biblioteca de perfiles"), true, notYet));
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
}

}  // namespace cc::app





