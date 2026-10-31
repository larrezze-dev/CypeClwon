#pragma once
#include <QStringList>
#include <QWidget>

class QPushButton;

namespace cc::app {

/// Start screen shown when no project is open: "Archivo", "Últimos ficheros" and "Ayuda"
/// panels centred on a grey background (reference: docs/screenshots/01_startup.png).
class StartPage : public QWidget {
    Q_OBJECT
public:
    /// Number of recent-file slots displayed, as in the original.
    static constexpr int kRecentSlots = 6;

    /// Help entries, in display order. The id is emitted by helpRequested().
    enum class HelpEntry {
        Documentation, CalculationReport, Example, Faq, News, LicenceAgreement,
        LiabilityClause, TechnicalSupport, UserCommunity, LearningResources, About
    };
    Q_ENUM(HelpEntry)

    explicit StartPage(QWidget* parent = nullptr);

    /// Shows up to kRecentSlots paths; empty slots read "Sin fichero" and are inert. Paths listed in `missing`
    /// (files that no longer exist) stay in the list but are greyed out and inert, as in the original. "Más..."
    /// is enabled only when `hasMore` (the history holds more than kRecentSlots files).
    void setRecentFiles(const QStringList& paths, const QStringList& missing = {}, bool hasMore = false);

signals:
    void newRequested();
    void fileManagerRequested();
    void examplesRequested();
    void recentFileRequested(const QString& path);
    void moreRecentRequested();
    void helpRequested(cc::app::StartPage::HelpEntry entry);
    void licenceManagerRequested();

private:
    QWidget* makePanel(const QString& title, QWidget* body);
    QPushButton* makeLink(const QString& text, const QString& icon);
    QWidget* buildArchiveBody();
    QWidget* buildRecentBody();
    QWidget* buildHelpBody();

    QList<QPushButton*> m_recentButtons;
    QPushButton* m_moreButton = nullptr;
};

}  // namespace cc::app
