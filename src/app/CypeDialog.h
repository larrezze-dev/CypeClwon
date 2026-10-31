#pragma once
#include <QDialog>

class QHBoxLayout;
class QLabel;
class QPushButton;
class QToolButton;
class QVBoxLayout;

namespace cc::app {

/// Base class for application dialogs: frameless window with the red title bar, centred title and
/// close button used by every dialog of the original (docs/research/ui/dialogs_nueva_obra_wizard.md).
/// Subclasses fill contentLayout() and call setupFooter() for the Aceptar / Cancelar buttons.
class CypeDialog : public QDialog {
    Q_OBJECT
public:
    explicit CypeDialog(const QString& title, QWidget* parent = nullptr);

    QPushButton* acceptButton() const { return m_accept; }
    QPushButton* cancelButton() const { return m_cancel; }

protected:
    QVBoxLayout* contentLayout() const { return m_content; }
    /// Adds the footer band with the default Aceptar button and a Cancelar button.
    void setupFooter(const QString& acceptText, const QString& cancelText);
    /// Adds an empty footer band and returns its layout, for dialogs with their own button row.
    QHBoxLayout* setupCustomFooter();

    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;

private:
    QWidget* m_titleBar = nullptr;
    QVBoxLayout* m_content = nullptr;
    QVBoxLayout* m_root = nullptr;
    QPushButton* m_accept = nullptr;
    QPushButton* m_cancel = nullptr;
    QPoint m_dragOffset;
    bool m_dragging = false;
};

}  // namespace cc::app
