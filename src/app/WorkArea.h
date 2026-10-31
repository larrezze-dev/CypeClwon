#pragma once
#include <QWidget>

class QAction;
class QMdiArea;
class QMdiSubWindow;

namespace cc::app {

/// MDI work area ("Área de trabajo"): a header label above a QMdiArea whose child windows
/// will host the 3D viewport. Exposes the window-management actions used by the Ventana tab.
class WorkArea : public QWidget {
    Q_OBJECT
public:
    explicit WorkArea(QWidget* parent = nullptr);

    /// Adds a child window holding `content` (ownership transferred) and returns it.
    /// The first window opens maximised, like the original; later ones are shown normally.
    QMdiSubWindow* addDocumentWindow(const QString& title, QWidget* content);

    QMdiArea* mdiArea() const { return m_mdi; }
    int windowCount() const;

    QAction* cascadeAction() const { return m_cascade; }
    QAction* tileAction() const { return m_tile; }
    QAction* tileHorizontallyAction() const { return m_tileH; }
    QAction* tileVerticallyAction() const { return m_tileV; }
    QAction* nextAction() const { return m_next; }
    QAction* previousAction() const { return m_previous; }
    QAction* closeActiveAction() const { return m_close; }
    QAction* closeAllAction() const { return m_closeAll; }

public slots:
    void tileHorizontally();
    void tileVertically();

private:
    void createActions();
    void updateActionStates();

    QMdiArea* m_mdi = nullptr;
    QAction* m_cascade = nullptr;
    QAction* m_tile = nullptr;
    QAction* m_tileH = nullptr;
    QAction* m_tileV = nullptr;
    QAction* m_next = nullptr;
    QAction* m_previous = nullptr;
    QAction* m_close = nullptr;
    QAction* m_closeAll = nullptr;
};

}  // namespace cc::app
