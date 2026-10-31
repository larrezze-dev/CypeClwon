#include "MainWindow.h"

#include <QDir>
#include <QEvent>
#include <QAction>
#include <QIcon>
#include <QKeySequence>
#include <QStackedWidget>
#include <QStatusBar>

#include "SARibbonBar.h"
#include "SARibbonCategory.h"
#include "SARibbonQuickAccessBar.h"
#include "SARibbonSystemButtonBar.h"
#include "Icons.h"
#include "SessionBar.h"
#include "NewProjectDialog.h"
#include "EmptyProjectWizard.h"
#include "ProjectTypeDialog.h"
#include "StartPage.h"
#include "WorkArea.h"
#include "core/version.h"

namespace cc::app {

namespace {
// Colours sampled from the original title bar (docs/screenshots/orig_asistente_02.png).
constexpr const char* kTitleRed = "#B1291C";
constexpr int kTitleBarHeight = 26;
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
    connect(m_startPage, &StartPage::fileManagerRequested, this, notYet);
    connect(m_startPage, &StartPage::examplesRequested, this, notYet);
    connect(m_startPage, &StartPage::moreRecentRequested, this, notYet);
    connect(m_startPage, &StartPage::recentFileRequested, this, notYet);
    connect(m_startPage, &StartPage::helpRequested, this, notYet);
    connect(m_startPage, &StartPage::licenceManagerRequested, this, notYet);
}

void MainWindow::newProject() {
    NewProjectDialog dialog(NewProjectDialog::defaultFolder(), this);
    if (dialog.exec() != QDialog::Accepted) return;
    NewProjectDialog::rememberFolder(dialog.folder());
    ProjectTypeDialog typeDialog(this);
    if (typeDialog.exec() != QDialog::Accepted) return;
    EmptyProjectWizard wizard(this);
    if (wizard.exec() != QDialog::Accepted) return;
    // The Estados límite dialog (T-P2-06) and the editor follow in later tasks.
    statusBar()->showMessage(tr("Nueva obra «%1». El asistente de obra vacía aún no está disponible.")
                                 .arg(QDir::toNativeSeparators(dialog.filePath())), 6000);
}

void MainWindow::setProjectOpen(bool open) {
    m_stack->setCurrentWidget(open ? static_cast<QWidget*>(m_workArea) : m_startPage);
    // Like the original, the ribbon shows no tabs until a project is open.
    ribbonBar()->setMinimumMode(!open);
    if (open) ribbonBar()->setCurrentIndex(0);
    for (SARibbonCategory* category : ribbonBar()->categoryPages()) {
        if (open) {
            ribbonBar()->showCategory(category);
        } else {
            ribbonBar()->hideCategory(category);
        }
    }
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





