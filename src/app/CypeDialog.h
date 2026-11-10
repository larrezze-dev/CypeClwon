#pragma once
#include <QDialog>
#include <QRect>
#include <functional>
#include <vector>

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
    /// Adds the maximise / restore button the original shows on its resizable windows (Rejilla, Niveles).
    void setMaximizable();
    /// For long titles ("Importación de series de perfiles predefinidas"): 12 px font and the title centred over the whole bar.
    void setCompactTitle();
    /// Adds the book icon of the title bar. It switches the contextual help on and off (measured in «Cargas en paño», research 14.5): the controls with
    /// help are framed in blue, a click on one turns its frame yellow and shows a balloon with its text; F1 does nothing. The title is centred in the
    /// space left of the buttons (not balanced), as in the original.
    void setHelpable();
    /// A control with contextual help: `area` gives its rectangle in dialog coordinates (evaluated when the help is drawn or clicked), `text` the balloon.
    void addHelpItem(std::function<QRect()> area, const QString& text);
    bool helpMode() const { return m_helpMode; }
    QToolButton* helpButton() const { return m_helpButton; }
    /// What a click at `point` (dialog coordinates) does in help mode: index of the item selected, or -1. Exposed for the tests.
    int helpClick(const QPoint& point);
    int helpSelected() const { return m_helpSelected; }
    /// Text of the balloon shown now (empty when none).
    QString helpBalloonText() const;

protected:
    QVBoxLayout* contentLayout() const { return m_content; }
    /// Adds the footer band with the default Aceptar button and a Cancelar button.
    void setupFooter(const QString& acceptText, const QString& cancelText);
    /// Adds an empty footer band and returns its layout, for dialogs with their own button row.
    QHBoxLayout* setupCustomFooter();

    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;

private:
    QWidget* m_titleBar = nullptr;
    QHBoxLayout* m_titleLayout = nullptr;
    QVBoxLayout* m_content = nullptr;
    QVBoxLayout* m_root = nullptr;
    struct HelpItem {
        std::function<QRect()> area;
        QString text;
    };
    class HelpOverlay;
    void setHelpMode(bool on);
    void showBalloon(const QString& text, const QPoint& at);
    QLabel* m_balloon = nullptr;
    std::vector<HelpItem> m_helpItems;
    HelpOverlay* m_overlay = nullptr;
    QToolButton* m_helpButton = nullptr;
    bool m_helpMode = false;
    int m_helpSelected = -1;
    QPushButton* m_accept = nullptr;
    QPushButton* m_cancel = nullptr;
    QPoint m_dragOffset;
    bool m_dragging = false;
};

}  // namespace cc::app
