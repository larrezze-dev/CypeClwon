#include "StartPage.h"

#include <QDir>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include "Icons.h"

namespace cc::app {

namespace {
// Colours sampled from the original start screen (docs/screenshots/01_startup.png).
constexpr const char* kStyle =
    "StartPage { background: #787878; }"
    "QLabel#title { color: #F2F2F2; font-size: 30px; }"
    "QLabel#panelHeader { background: #D9D9D9; color: #202020; font-size: 17px; padding: 10px 6px; }"
    "QWidget#panelBody { background: white; }"
    "QPushButton[flat=\"true\"] { border: none; background: transparent; text-align: left; padding: 3px 4px; color: #101010; }"
    "QPushButton[flat=\"true\"]:hover { background: #E8EEF7; }"
    "QPushButton[flat=\"true\"]:disabled { color: #A8A8A8; }"
    "QPushButton#footer { background: white; border: 1px solid #C8C8C8; padding: 4px 8px; }";
}  // namespace

StartPage::StartPage(QWidget* parent) : QWidget(parent) {
    setObjectName(QStringLiteral("StartPage"));
    setAttribute(Qt::WA_StyledBackground, true);
    setStyleSheet(QString::fromLatin1(kStyle).replace(QStringLiteral("StartPage {"), QStringLiteral("QWidget#StartPage {")));

    auto* outer = new QVBoxLayout(this);
    outer->addStretch(3);

    // Centred column holding the title, the three panels (no licence button: the program is free to use).
    auto* column = new QWidget(this);
    column->setFixedWidth(830);
    auto* col = new QVBoxLayout(column);
    col->setContentsMargins(0, 0, 0, 0);
    col->setSpacing(8);

    auto* title = new QLabel(tr("CYPE 3D"), column);
    title->setObjectName(QStringLiteral("title"));
    col->addWidget(title);

    auto* panels = new QHBoxLayout;
    panels->setSpacing(8);
    panels->addWidget(makePanel(tr("Archivo"), buildArchiveBody()), 0);
    panels->addWidget(makePanel(tr("Últimos ficheros"), buildRecentBody()), 1);
    panels->addWidget(makePanel(tr("Ayuda"), buildHelpBody()), 0);
    col->addLayout(panels);


    auto* centre = new QHBoxLayout;
    centre->addStretch(1);
    centre->addWidget(column);
    centre->addStretch(1);
    outer->addLayout(centre);
    outer->addStretch(4);

    setRecentFiles({});
}

QWidget* StartPage::makePanel(const QString& title, QWidget* body) {
    auto* panel = new QWidget(this);
    auto* layout = new QVBoxLayout(panel);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    auto* header = new QLabel(title, panel);
    header->setObjectName(QStringLiteral("panelHeader"));
    layout->addWidget(header);
    body->setObjectName(QStringLiteral("panelBody"));
    body->setAttribute(Qt::WA_StyledBackground, true);
    layout->addWidget(body, 1);
    return panel;
}

QPushButton* StartPage::makeLink(const QString& text, const QString& icon) {
    auto* button = new QPushButton(text, this);
    if (!icon.isEmpty()) button->setIcon(themedIcon(icon));
    button->setCursor(Qt::PointingHandCursor);
    button->setFlat(true);
    return button;
}

QWidget* StartPage::buildArchiveBody() {
    auto* body = new QWidget(this);
    body->setFixedWidth(150);
    auto* layout = new QVBoxLayout(body);
    layout->setContentsMargins(8, 10, 8, 10);
    layout->setSpacing(10);
    struct Item {
        QString text;
        QString icon;
        void (StartPage::*signal)();
    };
    const Item items[] = {
        {tr("Nuevo..."), QStringLiteral("file_new"), &StartPage::newRequested},
        {tr("Gestión archivos"), QStringLiteral("folder_open"), &StartPage::fileManagerRequested},
        {tr("Ejemplos"), QStringLiteral("gift"), &StartPage::examplesRequested},
    };
    for (const Item& item : items) {
        QPushButton* button = makeLink(item.text, item.icon);
        button->setIconSize(QSize(30, 30));
        connect(button, &QPushButton::clicked, this, item.signal);
        layout->addWidget(button);
    }
    layout->addStretch(1);
    return body;
}

QWidget* StartPage::buildRecentBody() {
    auto* body = new QWidget(this);
    auto* layout = new QVBoxLayout(body);
    layout->setContentsMargins(8, 10, 8, 10);
    layout->setSpacing(6);
    for (int i = 0; i < kRecentSlots; ++i) {
        QPushButton* button = makeLink(QString(), QString());
        button->setMinimumHeight(29);
        connect(button, &QPushButton::clicked, this,
                [this, button] { emit recentFileRequested(button->property("path").toString()); });
        m_recentButtons.append(button);
        layout->addWidget(button);
    }
    layout->addStretch(1);
    auto* more = new QPushButton(tr("Más..."), body);
    more->setObjectName(QStringLiteral("footer"));
    more->setCursor(Qt::PointingHandCursor);
    connect(more, &QPushButton::clicked, this, &StartPage::moreRecentRequested);
    more->setEnabled(false);
    m_moreButton = more;
    layout->addWidget(more, 0, Qt::AlignHCenter);
    return body;
}

QWidget* StartPage::buildHelpBody() {
    auto* body = new QWidget(this);
    body->setFixedWidth(240);
    auto* layout = new QVBoxLayout(body);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(0);
    struct Item {
        QString text;
        QString icon;
        HelpEntry entry;
    };
    const QString doc = QStringLiteral("help_doc");
    const Item items[] = {
        {tr("Documentación del programa"), doc, HelpEntry::Documentation},
        {tr("CYPE 3D - Memoria de Cálculo"), doc, HelpEntry::CalculationReport},
        {tr("CYPE 3D - Ejemplo"), doc, HelpEntry::Example},
        {tr("Ir a la FAQ de CYPE"), doc, HelpEntry::Faq},
        {tr("Novedades"), doc, HelpEntry::News},
        {tr("Contrato de Licencia de Programas"), doc, HelpEntry::LicenceAgreement},
        {tr("Cláusula de Responsabilidades"), doc, HelpEntry::LiabilityClause},
        {tr("Soporte técnico"), QStringLiteral("support"), HelpEntry::TechnicalSupport},
        {tr("Comunidad de usuarios"), QStringLiteral("community"), HelpEntry::UserCommunity},
        {tr("Recursos de aprendizaje"), doc, HelpEntry::LearningResources},
        {tr("Acerca de ..."), doc, HelpEntry::About},
    };
    for (const Item& item : items) {
        QPushButton* button = makeLink(item.text, item.icon);
        const HelpEntry entry = item.entry;
        connect(button, &QPushButton::clicked, this, [this, entry] { emit helpRequested(entry); });
        layout->addWidget(button);
    }
    layout->addStretch(1);
    return body;
}

void StartPage::setRecentFiles(const QStringList& paths, const QStringList& missing, bool hasMore) {
    m_moreButton->setEnabled(hasMore);
    for (int i = 0; i < m_recentButtons.size(); ++i) {
        QPushButton* button = m_recentButtons[i];
        const bool filled = i < paths.size() && !paths[i].isEmpty();
        button->setEnabled(filled && !missing.contains(paths[i]));
        button->setProperty("path", filled ? paths[i] : QString());
        // Missing files keep their text (greyed out because the button is disabled).
        button->setText(filled ? QStringLiteral("%1 %2").arg(i + 1).arg(QDir::toNativeSeparators(paths[i]))
                               : tr("%1 Sin fichero").arg(i + 1));
        button->setToolTip(filled ? QFileInfo(paths[i]).fileName() : QString());
    }
}

}  // namespace cc::app
