#include "SessionBar.h"

#include <QHBoxLayout>
#include <QToolButton>

namespace cc::app {

SessionBar::SessionBar(QWidget* parent) : QWidget(parent) {
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);
    auto* signIn = makeButton(tr("Iniciar sesión"));
    auto* mentor = makeButton(tr("CYPE Mentor"));
    layout->addWidget(signIn);
    layout->addWidget(mentor);
    connect(signIn, &QToolButton::clicked, this, &SessionBar::signInRequested);
    connect(mentor, &QToolButton::clicked, this, &SessionBar::mentorRequested);
    setStyleSheet(QStringLiteral(
        "QToolButton { color: white; background: transparent; border: none; padding: 2px 6px; }"
        "QToolButton:hover { background: rgba(255,255,255,0.18); }"));
}

QToolButton* SessionBar::makeButton(const QString& text) {
    auto* button = new QToolButton(this);
    button->setText(text);
    button->setAutoRaise(true);
    button->setCursor(Qt::PointingHandCursor);
    return button;
}

}  // namespace cc::app

