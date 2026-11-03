#include "CypeDialog.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QToolButton>
#include <QVBoxLayout>

namespace cc::app {

namespace {
// Measured on the "Nueva obra" window of the original (docs/research/ui/nudos.md section 8): the frame is 5 px on the left, right
// and bottom (1 px #841E14 outside + 4 px #B1291C inside); the title bar is #B1291C, about 31 px high with 1 px #841E14 on top.
constexpr int kTitleBarHeight = 30;  // below the 1 px outline: 31 px with it
constexpr int kFrame = 5;
const QColor kFrameDark(0x84, 0x1E, 0x14);
const QColor kFrameRed(0xB1, 0x29, 0x1C);
constexpr const char* kStyle =
    "QDialog#cypeDialog { background: transparent; }"
    "QWidget#dlgTitleBar { background: transparent; }"
    "QLabel#dlgTitle { color: white; font-size: 13px; }"
    "QToolButton#dlgClose { color: white; background: transparent; border: none; font-size: 18px; }"
    "QToolButton#dlgClose:hover { background: rgba(255,255,255,0.2); }"
    "QGroupBox { border: 1px solid #DADADA; margin-top: 8px; padding-top: 6px; background: #F0F0F0; }"
    "QGroupBox::title { subcontrol-origin: margin; left: 8px; padding: 0 2px; }"
    "QLineEdit { background: white; border: 1px solid #DADADA; border-bottom: 1px solid #7A7A7A; padding: 3px; }"
    "QLineEdit:focus { border-bottom: 2px solid #0067C0; }"
    "QCheckBox { spacing: 6px; min-height: 22px; }"
    "QCheckBox::indicator { width: 14px; height: 14px; border: 1px solid #6A6A6A; background: white; border-radius: 2px; }"
    "QCheckBox::indicator:checked { background: #0067C0; border: 1px solid #0067C0; image: url(:/icons/check_white.svg); }"
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
    m_root->setContentsMargins(kFrame, 1, kFrame, kFrame);
    m_root->setSpacing(0);

    m_titleBar = new QWidget(this);
    m_titleBar->setObjectName(QStringLiteral("dlgTitleBar"));
    m_titleBar->setFixedHeight(kTitleBarHeight);
    auto* bar = new QHBoxLayout(m_titleBar);
    m_titleLayout = bar;
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
    m_content->setContentsMargins(10, 12, 10, 12);  // 14 px side margin minus the 4 px the thicker frame takes
    m_root->addWidget(body, 1);
}

void CypeDialog::setMaximizable() {
    auto* maximise = new QToolButton(m_titleBar);
    maximise->setObjectName(QStringLiteral("dlgClose"));
    maximise->setText(QStringLiteral("☐"));
    maximise->setFixedSize(kTitleBarHeight, kTitleBarHeight);
    maximise->setCursor(Qt::ArrowCursor);
    connect(maximise, &QToolButton::clicked, this, [this] { isMaximized() ? showNormal() : showMaximized(); });
    m_titleLayout->insertWidget(m_titleLayout->count() - 1, maximise);
    m_titleLayout->setContentsMargins(2 * kTitleBarHeight, 0, 0, 0);  // balances both buttons
}

void CypeDialog::setupFooter(const QString& acceptText, const QString& cancelText) {
    auto* footer = new QWidget(this);
    footer->setObjectName(QStringLiteral("dlgFooter"));
    auto* row = new QHBoxLayout(footer);
    row->setContentsMargins(6, 10, 6, 10);
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

QHBoxLayout* CypeDialog::setupCustomFooter() {
    auto* footer = new QWidget(this);
    footer->setObjectName(QStringLiteral("dlgFooter"));
    auto* row = new QHBoxLayout(footer);
    row->setContentsMargins(6, 10, 6, 10);
    m_root->addWidget(footer);
    return row;
}

void CypeDialog::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    const QRect all = rect();
    painter.fillRect(all, QColor(0xF0, 0xF0, 0xF0));
    painter.fillRect(QRect(0, 0, all.width(), 1 + kTitleBarHeight), kFrameRed);                       // title bar
    painter.fillRect(QRect(0, 0, kFrame, all.height()), kFrameRed);                                   // left
    painter.fillRect(QRect(all.width() - kFrame, 0, kFrame, all.height()), kFrameRed);                // right
    painter.fillRect(QRect(0, all.height() - kFrame, all.width(), kFrame), kFrameRed);                // bottom
    painter.setPen(kFrameDark);                                                                       // 1 px outline all round
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(all.adjusted(0, 0, -1, -1));
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
