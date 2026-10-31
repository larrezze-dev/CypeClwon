#include "CypeDialog.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPushButton>
#include <QToolButton>
#include <QVBoxLayout>

namespace cc::app {

namespace {
constexpr int kTitleBarHeight = 34;
constexpr const char* kStyle =
    "QDialog#cypeDialog { background: #F0F0F0; border: 1px solid #B22A20; }"
    "QWidget#dlgTitleBar { background: #B22A20; }"
    "QLabel#dlgTitle { color: white; font-size: 13px; }"
    "QToolButton#dlgClose { color: white; background: transparent; border: none; font-size: 18px; }"
    "QToolButton#dlgClose:hover { background: rgba(255,255,255,0.2); }"
    "QGroupBox { border: 1px solid #DADADA; margin-top: 8px; padding-top: 6px; background: #F0F0F0; }"
    "QGroupBox::title { subcontrol-origin: margin; left: 8px; padding: 0 2px; }"
    "QLineEdit { background: white; border: 1px solid #DADADA; border-bottom: 1px solid #7A7A7A; padding: 3px; }"
    "QLineEdit:focus { border-bottom: 2px solid #0067C0; }"
    "QPushButton { background: white; border: 1px solid #C8C8C8; border-radius: 3px; padding: 4px 14px; }"
    "QPushButton:default { border: 1px solid #0067C0; }"
    "QPushButton:hover { background: #E8EEF7; }"
    "QWidget#dlgFooter { background: #F0F0F0; border-top: 1px solid #DADADA; }";
}  // namespace

CypeDialog::CypeDialog(const QString& title, QWidget* parent) : QDialog(parent) {
    setWindowFlag(Qt::FramelessWindowHint, true);
    setObjectName(QStringLiteral("cypeDialog"));
    setWindowTitle(title);
    setModal(true);
    setStyleSheet(QString::fromLatin1(kStyle));

    m_root = new QVBoxLayout(this);
    m_root->setContentsMargins(1, 1, 1, 1);
    m_root->setSpacing(0);

    m_titleBar = new QWidget(this);
    m_titleBar->setObjectName(QStringLiteral("dlgTitleBar"));
    m_titleBar->setFixedHeight(kTitleBarHeight);
    auto* bar = new QHBoxLayout(m_titleBar);
    bar->setContentsMargins(kTitleBarHeight, 0, 0, 0);  // left spacer balances the close button
    bar->setSpacing(0);
    auto* label = new QLabel(title, m_titleBar);
    label->setObjectName(QStringLiteral("dlgTitle"));
    label->setAlignment(Qt::AlignCenter);
    bar->addWidget(label, 1);
    auto* close = new QToolButton(m_titleBar);
    close->setObjectName(QStringLiteral("dlgClose"));
    close->setText(QStringLiteral("✕"));
    close->setFixedSize(kTitleBarHeight, kTitleBarHeight);
    close->setCursor(Qt::ArrowCursor);
    connect(close, &QToolButton::clicked, this, &QDialog::reject);
    bar->addWidget(close);
    m_root->addWidget(m_titleBar);

    auto* body = new QWidget(this);
    m_content = new QVBoxLayout(body);
    m_content->setContentsMargins(14, 12, 14, 12);
    m_root->addWidget(body, 1);
}

void CypeDialog::setupFooter(const QString& acceptText, const QString& cancelText) {
    auto* footer = new QWidget(this);
    footer->setObjectName(QStringLiteral("dlgFooter"));
    auto* row = new QHBoxLayout(footer);
    row->setContentsMargins(10, 10, 10, 10);
    m_accept = new QPushButton(acceptText, footer);
    m_accept->setDefault(true);
    m_cancel = new QPushButton(cancelText, footer);
    row->addWidget(m_accept);
    row->addStretch(1);
    row->addWidget(m_cancel);
    connect(m_accept, &QPushButton::clicked, this, &QDialog::accept);
    connect(m_cancel, &QPushButton::clicked, this, &QDialog::reject);
    m_root->addWidget(footer);
}

void CypeDialog::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton && m_titleBar->geometry().contains(event->position().toPoint())) {
        m_dragging = true;
        m_dragOffset = event->globalPosition().toPoint() - frameGeometry().topLeft();
    }
    QDialog::mousePressEvent(event);
}

void CypeDialog::mouseMoveEvent(QMouseEvent* event) {
    if (m_dragging && (event->buttons() & Qt::LeftButton)) {
        move(event->globalPosition().toPoint() - m_dragOffset);
    } else {
        m_dragging = false;
    }
    QDialog::mouseMoveEvent(event);
}

}  // namespace cc::app
