#include "NodeCalculatorDialog.h"

#include <QFrame>
#include <QKeyEvent>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace cc::app {

namespace {
// Window of the original: 322 x 286 px with the 5 px frame and 31 px title bar of every dialog. The body below is 312 x 250 px,
// its origin at (5, 31) of the window; coordinates below are those of the measured capture (window coordinates).
constexpr int kOriginX = 5, kOriginY = 31;
constexpr int kMaxLength = 12;  // characters of a number typed into a field

struct KeySpec {
    const char* caption;
    int x, y, w, h;  // window coordinates
};

// Digits and "." "=" (left block), then the operator columns (middle) as in orig_nudos_nuevo_02_calculadora.png.
constexpr KeySpec kKeys[] = {
    {"9", 19, 136, 29, 25}, {"8", 55, 136, 29, 25}, {"7", 91, 136, 29, 25},
    {"6", 19, 171, 29, 25}, {"5", 55, 171, 29, 25}, {"4", 91, 171, 29, 25},
    {"3", 19, 206, 29, 25}, {"2", 55, 206, 29, 25}, {"1", 91, 206, 29, 25},
    {"0", 19, 241, 29, 25}, {".", 55, 241, 29, 25}, {"=", 91, 241, 29, 25},
    {"+", 134, 139, 30, 25}, {"-", 134, 176, 30, 25}, {"*", 134, 209, 30, 25}, {"/", 134, 242, 30, 25},
    {"+/-", 172, 139, 40, 25}, {"???", 172, 176, 40, 25}, {"C", 172, 209, 40, 25}, {"AC", 172, 242, 40, 25},
    {"MC", 220, 139, 41, 25}, {"M-", 220, 176, 41, 25}, {"M+", 220, 209, 41, 25}, {"MR", 220, 242, 41, 25},
};

QRect bodyRect(int x, int y, int w, int h) { return QRect(x - kOriginX, y - kOriginY, w, h); }
}  // namespace

NodeCalculatorDialog::NodeCalculatorDialog(QWidget* parent) : CypeDialog(tr("Calculadora"), parent) {
    setFixedSize(322, 286);
    contentLayout()->setContentsMargins(0, 0, 0, 0);
    auto* body = new QWidget(this);
    body->setFixedSize(312, 250);
    contentLayout()->addWidget(body);
    setFocusPolicy(Qt::StrongFocus);

    const QString axisNames[3] = {tr("Coordenada X"), tr("Coordenada Y"), tr("Coordenada Z")};
    for (int i = 0; i < 3; ++i) {
        auto* caption = new QLabel(axisNames[i], body);
        caption->setGeometry(bodyRect(17, 40 + 27 * i, 100, 25));
        m_fields[static_cast<std::size_t>(i)] = new QLabel(body);
        QLabel* field = m_fields[static_cast<std::size_t>(i)];
        field->setGeometry(bodyRect(120, 40 + 27 * i, 189, 25));
        field->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    }

    auto* frame = new QFrame(body);  // frame round the keyboard with the separator between the two key blocks
    frame->setGeometry(bodyRect(12, 127, 297, 146));
    frame->setStyleSheet(QStringLiteral("QFrame { border: 1px solid #DCDCDC; background: transparent; }"));
    auto* separator = new QFrame(body);
    separator->setGeometry(bodyRect(127, 128, 2, 144));
    separator->setStyleSheet(QStringLiteral("QFrame { border: none; background: #DCDCDC; }"));

    const QString keyStyle = QStringLiteral(
        "QPushButton { background: white; border: 1px solid #C8C8C8; border-radius: 3px; padding: 0; }"
        "QPushButton:pressed { background: #E0E8F4; }");
    const auto makeKey = [&](const QString& caption, const QRect& rect) {
        auto* key = new QPushButton(caption, body);
        key->setGeometry(rect);
        key->setFocusPolicy(Qt::NoFocus);  // the keyboard keeps acting on the dialog itself
        key->setStyleSheet(keyStyle);
        return key;
    };
    for (const KeySpec& spec : kKeys) {
        QPushButton* key = makeKey(QString::fromLatin1(spec.caption), bodyRect(spec.x, spec.y, spec.w, spec.h));
        m_buttons[static_cast<std::size_t>(m_buttonCount++)] = key;
        const QString caption = key->text();
        if (caption.size() == 1 && (caption.at(0).isDigit() || caption == QLatin1String(".")))
            connect(key, &QPushButton::clicked, this, [this, caption] { pressDigit(caption.at(0)); });
        else if (caption == QLatin1String("C"))
            connect(key, &QPushButton::clicked, this, &NodeCalculatorDialog::pressClear);
        else if (caption == QLatin1String("AC"))
            connect(key, &QPushButton::clicked, this, &NodeCalculatorDialog::pressClearAll);
        // The remaining keys ("+", "-", "*", "/", "=", "+/-", "???", memory) are present but not measured: no behaviour.
    }
    m_back = makeKey(QStringLiteral("←"), bodyRect(269, 134, 34, 34));
    QFont big = m_back->font();
    big.setPixelSize(22);
    m_back->setFont(big);
    connect(m_back, &QPushButton::clicked, this, &NodeCalculatorDialog::pressBackspace);
    m_enter = makeKey(QStringLiteral("↵"), bodyRect(269, 176, 34, 91));
    m_enter->setFont(big);
    connect(m_enter, &QPushButton::clicked, this, &NodeCalculatorDialog::pressEnter);
    refreshFields();
}

QPushButton* NodeCalculatorDialog::button(const QString& caption) const {
    for (int i = 0; i < m_buttonCount; ++i)
        if (m_buttons[static_cast<std::size_t>(i)]->text() == caption) return m_buttons[static_cast<std::size_t>(i)];
    return nullptr;
}

double NodeCalculatorDialog::value(int index) const { return fieldText(index).toDouble(); }

void NodeCalculatorDialog::refreshFields() {
    for (int i = 0; i < 3; ++i) {
        QLabel* field = m_fields[static_cast<std::size_t>(i)];
        field->setText(m_text[static_cast<std::size_t>(i)]);
        // The active field is the grey one (measured: orig_nudos_nuevo_02_calculadora.png, n9_after_enter.png).
        field->setStyleSheet(QStringLiteral("QLabel { background: %1; border: 1px solid #C8C8C8; border-radius: 3px; padding-right: 6px; }")
                                 .arg(i == m_active ? QStringLiteral("#F0F0F0") : QStringLiteral("white")));
    }
}

void NodeCalculatorDialog::start(QChar first) {
    m_text = {QStringLiteral("0"), QStringLiteral("0"), QStringLiteral("0")};
    m_preset = {false, false, false};
    m_active = 0;
    pressDigit(first);
}

void NodeCalculatorDialog::presetYZ(double y, double z) {
    m_text[1] = QString::number(y, 'g', 10);
    m_text[2] = QString::number(z, 'g', 10);
    m_preset = {false, true, true};
    refreshFields();
}

void NodeCalculatorDialog::pressDigit(QChar ch) {
    QString& text = m_text[static_cast<std::size_t>(m_active)];
    bool& preset = m_preset[static_cast<std::size_t>(m_active)];
    if (preset) {  // a preset value is replaced by the first key typed (assumed: the original's behaviour was not measured)
        text = QStringLiteral("0");
        preset = false;
    }
    if (ch == QLatin1Char('.') || ch == QLatin1Char(',')) {
        if (!text.contains(QLatin1Char('.')) && text.size() < kMaxLength) text += QLatin1Char('.');
    } else if (ch.isDigit() && ch.unicode() < 128) {
        if (text == QLatin1String("0")) text = QString(ch);
        else if (text.size() < kMaxLength) text += ch;
    }
    refreshFields();
}

void NodeCalculatorDialog::pressBackspace() {
    QString& text = m_text[static_cast<std::size_t>(m_active)];
    text.chop(1);
    if (text.isEmpty()) text = QStringLiteral("0");
    refreshFields();
}

void NodeCalculatorDialog::pressEnter() {
    if (m_active < 2) {
        ++m_active;
        refreshFields();
        return;
    }
    emit pointConfirmed(value(0), value(1), value(2));
    accept();
}

void NodeCalculatorDialog::pressClear() {
    m_text[static_cast<std::size_t>(m_active)] = QStringLiteral("0");
    refreshFields();
}

void NodeCalculatorDialog::pressClearAll() {
    m_text = {QStringLiteral("0"), QStringLiteral("0"), QStringLiteral("0")};
    m_active = 0;
    refreshFields();
}

void NodeCalculatorDialog::keyPressEvent(QKeyEvent* event) {
    switch (event->key()) {
        case Qt::Key_Tab:
        case Qt::Key_Backtab:
            event->accept();  // measured: Tab does not change the field
            return;
        case Qt::Key_Return:
        case Qt::Key_Enter:
            pressEnter();
            event->accept();
            return;
        case Qt::Key_Backspace:
            pressBackspace();
            event->accept();
            return;
        default: break;
    }
    const QString text = event->text();
    if (text.size() == 1 && !(event->modifiers() & (Qt::ControlModifier | Qt::AltModifier)) &&
        ((text.at(0).isDigit() && text.at(0).unicode() < 128) || text == QLatin1String(".") || text == QLatin1String(","))) {
        pressDigit(text.at(0));
        event->accept();
        return;
    }
    CypeDialog::keyPressEvent(event);  // Esc closes without a point
}

bool NodeCalculatorDialog::focusNextPrevChild(bool) { return false; }

}  // namespace cc::app
