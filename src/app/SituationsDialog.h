#pragma once
#include "CypeDialog.h"
#include "core/combination_report.h"

class QLabel;
class QLineEdit;
class QSpinBox;
class QTextBrowser;

namespace cc::app {

enum class ExportFormat { Pdf, Docx, Text, Html, Rtf };

/// "Situaciones" (778x268, fixed size): report viewer opened by the pencils of "Estados límite" with zoom,
/// page navigation, Exportar (PDF, DOCX, text, HTML, RTF), Imprimir and Cerrar. Compartir publishes to CYPE's cloud and
/// is not implemented (disabled). Reference: docs/research/ui/dialogs_opciones_y_pilotes.md, "Captura E".
class SituationsDialog : public CypeDialog {
    Q_OBJECT
public:
    explicit SituationsDialog(const cc::core::CombinationReport& report, QWidget* parent = nullptr);

    /// Writes the report to `path` in the given format; returns false when the file cannot be written.
    bool exportTo(ExportFormat format, const QString& path, int textWidth = 120) const;
    /// Default folder offered by the export dialogs.
    static QString defaultExportDirectory();
    static QString extensionOf(ExportFormat format);

    int pageCount() const;
    int currentPage() const { return m_page; }
    void goToPage(int page);  ///< 1-based, clamped
    void zoomBy(int steps);

private:
    void askAndExport(ExportFormat format);
    void print();
    void updatePageLabel();

    cc::core::CombinationReport m_report;
    QTextBrowser* m_viewer = nullptr;
    QLabel* m_pageLabel = nullptr;
    QSpinBox* m_pageBox = nullptr;
    int m_page = 1;
};

/// Path chooser used by every export format (553x121; 553x150 for text with the page width; 553x207 for HTML).
class ExportPathDialog : public CypeDialog {
    Q_OBJECT
public:
    ExportPathDialog(const QString& title, const QString& defaultPath, ExportFormat format, QWidget* parent = nullptr);
    QString path() const;
    int textWidth() const;

private:
    QLineEdit* m_path = nullptr;
    QSpinBox* m_width = nullptr;
};

}  // namespace cc::app
