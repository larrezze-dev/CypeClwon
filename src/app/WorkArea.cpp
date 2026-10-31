#include "WorkArea.h"

#include <QAction>
#include <QKeySequence>
#include <QLabel>
#include <QList>
#include <QMdiArea>
#include <QMdiSubWindow>
#include <QVBoxLayout>

namespace cc::app {

WorkArea::WorkArea(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto* header = new QLabel(tr("Área de trabajo"), this);
    header->setContentsMargins(3, 2, 3, 2);
    layout->addWidget(header);

    m_mdi = new QMdiArea(this);
    m_mdi->setViewMode(QMdiArea::SubWindowView);
    m_mdi->setBackground(QColor(QStringLiteral("#ABABAB")));
    m_mdi->setActivationOrder(QMdiArea::ActivationHistoryOrder);
    layout->addWidget(m_mdi, 1);

    createActions();
    connect(m_mdi, &QMdiArea::subWindowActivated, this, [this] { updateActionStates(); });
    updateActionStates();
}

int WorkArea::windowCount() const { return static_cast<int>(m_mdi->subWindowList().size()); }

QMdiSubWindow* WorkArea::addDocumentWindow(const QString& title, QWidget* content) {
    const bool first = windowCount() == 0;
    QMdiSubWindow* sub = m_mdi->addSubWindow(content);
    sub->setWindowTitle(title);
    sub->setAttribute(Qt::WA_DeleteOnClose);
    // Re-evaluate action states once the window is really gone.
    connect(sub, &QObject::destroyed, this, [this] { updateActionStates(); });
    if (first) {
        sub->showMaximized();
    } else {
        sub->show();
    }
    m_mdi->setActiveSubWindow(sub);
    updateActionStates();
    return sub;
}

void WorkArea::createActions() {
    auto make = [this](const QString& text, const QString& tip) {
        auto* action = new QAction(text, this);
        action->setStatusTip(tip);
        return action;
    };
    m_cascade = make(tr("Cascada"), tr("Dispone las ventanas en cascada."));
    m_tile = make(tr("Mosaico"), tr("Dispone las ventanas en mosaico."));
    m_tileH = make(tr("Mosaico horizontal"), tr("Dispone las ventanas una encima de otra."));
    m_tileV = make(tr("Mosaico vertical"), tr("Dispone las ventanas una al lado de otra."));
    m_next = make(tr("Siguiente"), tr("Activa la ventana siguiente."));
    m_previous = make(tr("Anterior"), tr("Activa la ventana anterior."));
    m_close = make(tr("Cerrar"), tr("Cierra la ventana activa."));
    m_closeAll = make(tr("Cerrar todas"), tr("Cierra todas las ventanas."));
    m_next->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Tab));
    m_previous->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Tab));

    connect(m_cascade, &QAction::triggered, m_mdi, &QMdiArea::cascadeSubWindows);
    connect(m_tile, &QAction::triggered, m_mdi, &QMdiArea::tileSubWindows);
    connect(m_tileH, &QAction::triggered, this, &WorkArea::tileHorizontally);
    connect(m_tileV, &QAction::triggered, this, &WorkArea::tileVertically);
    connect(m_next, &QAction::triggered, m_mdi, &QMdiArea::activateNextSubWindow);
    connect(m_previous, &QAction::triggered, m_mdi, &QMdiArea::activatePreviousSubWindow);
    connect(m_close, &QAction::triggered, m_mdi, &QMdiArea::closeActiveSubWindow);
    connect(m_closeAll, &QAction::triggered, m_mdi, &QMdiArea::closeAllSubWindows);
}

void WorkArea::updateActionStates() {
    const int count = windowCount();
    const bool several = count > 1;
    for (QAction* a : {m_cascade, m_tile, m_tileH, m_tileV, m_next, m_previous}) a->setEnabled(several);
    m_close->setEnabled(count > 0 && m_mdi->activeSubWindow() != nullptr);
    m_closeAll->setEnabled(count > 0);
}

namespace {
// Lays windows out in a single row (side by side) or single column filling the MDI viewport.
void tileInLine(QMdiArea* mdi, bool sideBySide) {
    const QList<QMdiSubWindow*> windows = mdi->subWindowList(QMdiArea::CreationOrder);
    if (windows.isEmpty()) return;
    const QRect area = mdi->viewport()->rect();
    const int n = static_cast<int>(windows.size());
    for (int i = 0; i < n; ++i) {
        QMdiSubWindow* w = windows[i];
        if (w->isMaximized() || w->isMinimized()) w->showNormal();
        if (sideBySide) {
            const int x0 = area.left() + area.width() * i / n;
            const int x1 = area.left() + area.width() * (i + 1) / n;
            w->setGeometry(x0, area.top(), x1 - x0, area.height());
        } else {
            const int y0 = area.top() + area.height() * i / n;
            const int y1 = area.top() + area.height() * (i + 1) / n;
            w->setGeometry(area.left(), y0, area.width(), y1 - y0);
        }
    }
}
}  // namespace

void WorkArea::tileHorizontally() { tileInLine(m_mdi, false); }
void WorkArea::tileVertically() { tileInLine(m_mdi, true); }

}  // namespace cc::app
