#pragma once
#include <QWidget>

class QToolButton;

namespace cc::app {

/// "Iniciar sesión" and "CYPE Mentor" buttons shown at the right of the red title bar.
/// Behaviour is stubbed until the licence/session phase (T-P2-09).
class SessionBar : public QWidget {
    Q_OBJECT
public:
    explicit SessionBar(QWidget* parent = nullptr);

signals:
    void signInRequested();
    void mentorRequested();

private:
    QToolButton* makeButton(const QString& text);
};

}  // namespace cc::app

