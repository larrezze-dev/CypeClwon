#pragma once
#include <QEvent>
#include <QMouseEvent>
#include <QObject>
#include <QTimer>
#include <QToolButton>
#include <QToolTip>

namespace cc::app {

/// Tooltips of the icon bars of "Configurar situaciones" / "Crear - [Situaciones]" behave as measured in the original (research 14.3):
/// they do NOT appear with the mouse resting on the button (5.3 s tried) but about 0.7 s AFTER a click. The filter eats the hover tooltip
/// event and shows the text of toolTip() once the click is released and the cursor is still over the button.
class ClickTooltip : public QObject {
public:
    explicit ClickTooltip(QToolButton* button) : QObject(button), m_button(button) {
        button->installEventFilter(this);
        m_timer.setSingleShot(true);
        m_timer.setInterval(kDelayMs);
        connect(&m_timer, &QTimer::timeout, this, [this] { show(); });
    }
    /// Attaches the behaviour to `button`, with `text` as its (click-triggered) tooltip.
    static void attach(QToolButton* button, const QString& text) {
        button->setToolTip(text);
        new ClickTooltip(button);
    }
    /// Shows the tooltip now when the cursor is over the button (also what the timer does); returns whether it was shown.
    bool show() {
        if (!m_button->underMouse() || m_button->toolTip().isEmpty()) return false;
        QToolTip::showText(m_button->mapToGlobal(m_button->rect().bottomLeft()), m_button->toolTip(), m_button);
        return true;
    }
    static constexpr int kDelayMs = 700;

protected:
    bool eventFilter(QObject* watched, QEvent* event) override {
        if (watched == m_button) {
            if (event->type() == QEvent::ToolTip) return true;  // no tooltip while resting
            if (event->type() == QEvent::MouseButtonRelease) m_timer.start();
            if (event->type() == QEvent::Leave || event->type() == QEvent::Hide) {
                m_timer.stop();
                QToolTip::hideText();
            }
        }
        return QObject::eventFilter(watched, event);
    }

private:
    QToolButton* m_button;
    QTimer m_timer;
};

}  // namespace cc::app
