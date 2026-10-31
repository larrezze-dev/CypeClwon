#include "SituationsDialog.h"

#include <QAbstractTextDocumentLayout>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QComboBox>
#include <QMenu>
#include <QMessageBox>
#include <QPageSize>
#include <QPdfWriter>
#include <QPrintDialog>
#include <QPrinter>
#include <QPushButton>
#include <QScrollBar>
#include <QSpinBox>
#include <QStandardPaths>
#include <QTextBlock>
#include <QTextBrowser>
#include <QTextDocument>
#include <QToolButton>
#include <QVBoxLayout>

#include "core/combination_export.h"

namespace cc::app {

using namespace cc::core;

namespace {

QToolButton* barButton(const QString& text, const QString& tip, QWidget* parent) {
    auto* b = new QToolButton(parent);
    b->setText(text);
    b->setToolTip(tip);
    b->setAutoRaise(true);
    return b;
}

QString nameOf(ExportFormat f) {
    switch (f) {
        case ExportFormat::Pdf: return QObject::tr("PDF");
        case ExportFormat::Docx: return QObject::tr("DOCX (Word2007 - OfficeOpenXML)");
        case ExportFormat::Text: return QObject::tr("Texto");
        case ExportFormat::Html: return QObject::tr("HTML");
        case ExportFormat::Rtf: return QObject::tr("RTF (WordPad, Word2003 y anteriores)");
    }
    return {};
}

}  // namespace

QString SituationsDialog::extensionOf(ExportFormat f) {
    switch (f) {
        case ExportFormat::Pdf: return QStringLiteral("pdf");
        case ExportFormat::Docx: return QStringLiteral("docx");
        case ExportFormat::Text: return QStringLiteral("txt");
        case ExportFormat::Html: return QStringLiteral("html");
        case ExportFormat::Rtf: return QStringLiteral("rtf");
    }
    return {};
}

QString SituationsDialog::defaultExportDirectory() {
    return QDir::toNativeSeparators(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) +
                                    QStringLiteral("/CYPE Ingenieros/Listados/CYPE 3D"));
}

SituationsDialog::SituationsDialog(const CombinationReport& report, QWidget* parent)
    : CypeDialog(tr("Situaciones"), parent), m_report(report) {
    setFixedSize(778, 268);
    QVBoxLayout* body = contentLayout();
    body->setSpacing(2);

    auto* bar = new QHBoxLayout;
    auto* zoomOut = barButton(QStringLiteral("−"), tr("Reducir"), this);
    auto* zoomIn = barButton(QStringLiteral("+"), tr("Ampliar"), this);
    auto* zoomReset = barButton(QStringLiteral("100%"), tr("Tamaño real"), this);
    auto* first = barButton(QStringLiteral("|◀"), tr("Primera página"), this);
    auto* prev = barButton(QStringLiteral("◀"), tr("Página anterior"), this);
    auto* next = barButton(QStringLiteral("▶"), tr("Página siguiente"), this);
    auto* last = barButton(QStringLiteral("▶|"), tr("Última página"), this);
    m_pageBox = new QSpinBox(this);
    m_pageBox->setMinimum(1);
    m_pageBox->setFixedWidth(52);
    m_pageLabel = new QLabel(this);
    for (QWidget* w : {static_cast<QWidget*>(zoomOut), static_cast<QWidget*>(zoomIn), static_cast<QWidget*>(zoomReset),
                       static_cast<QWidget*>(first), static_cast<QWidget*>(prev), static_cast<QWidget*>(next),
                       static_cast<QWidget*>(last), static_cast<QWidget*>(m_pageBox), static_cast<QWidget*>(m_pageLabel)})
        bar->addWidget(w);
    bar->addStretch(1);

    auto* share = new QPushButton(tr("Compartir"), this);
    share->setEnabled(false);  // publishes to CYPE's cloud: not implemented
    share->setToolTip(tr("Publicar en la nube de CYPE no está disponible en esta versión."));
    auto* exportButton = new QToolButton(this);
    exportButton->setText(tr("Exportar"));
    exportButton->setPopupMode(QToolButton::MenuButtonPopup);
    auto* menu = new QMenu(exportButton);
    auto* listing = menu->addMenu(tr("Exportar listado"));
    for (ExportFormat f : {ExportFormat::Pdf, ExportFormat::Docx, ExportFormat::Text, ExportFormat::Html, ExportFormat::Rtf})
        connect(listing->addAction(nameOf(f)), &QAction::triggered, this, [this, f] { askAndExport(f); });
    exportButton->setMenu(menu);
    connect(exportButton, &QToolButton::clicked, exportButton, &QToolButton::showMenu);
    auto* printButton = new QPushButton(tr("Imprimir"), this);
    auto* close = new QPushButton(tr("Cerrar"), this);
    for (QWidget* w : {static_cast<QWidget*>(share), static_cast<QWidget*>(exportButton), static_cast<QWidget*>(printButton),
                       static_cast<QWidget*>(close)})
        bar->addWidget(w);
    body->addLayout(bar);

    m_viewer = new QTextBrowser(this);
    m_viewer->setHtml(QString::fromStdString(exportHtml(m_report)));
    body->addWidget(m_viewer, 1);

    m_pageBox->setMaximum(pageCount());
    updatePageLabel();
    connect(zoomIn, &QToolButton::clicked, this, [this] { zoomBy(1); });
    connect(zoomOut, &QToolButton::clicked, this, [this] { zoomBy(-1); });
    connect(zoomReset, &QToolButton::clicked, this, [this] { m_viewer->setHtml(QString::fromStdString(exportHtml(m_report))); });
    connect(first, &QToolButton::clicked, this, [this] { goToPage(1); });
    connect(prev, &QToolButton::clicked, this, [this] { goToPage(m_page - 1); });
    connect(next, &QToolButton::clicked, this, [this] { goToPage(m_page + 1); });
    connect(last, &QToolButton::clicked, this, [this] { goToPage(pageCount()); });
    connect(m_pageBox, &QSpinBox::editingFinished, this, [this] { goToPage(m_pageBox->value()); });
    connect(printButton, &QPushButton::clicked, this, &SituationsDialog::print);
    connect(close, &QPushButton::clicked, this, &QDialog::accept);
}

int SituationsDialog::pageCount() const { return static_cast<int>(m_report.sections.size()); }

void SituationsDialog::updatePageLabel() {
    m_pageBox->setValue(m_page);
    m_pageLabel->setText(tr("%1 de %2").arg(m_page).arg(pageCount()));
}

void SituationsDialog::goToPage(int page) {
    m_page = qBound(1, page, qMax(1, pageCount()));
    updatePageLabel();
    // One page per report section: scroll to the heading of the section.
    const QString title = QString::fromStdString(m_report.sections[static_cast<std::size_t>(m_page - 1)].titles.front());
    const QTextCursor c = m_viewer->document()->find(title);
    if (c.isNull()) return;
    const QRectF r = m_viewer->document()->documentLayout()->blockBoundingRect(c.block());
    m_viewer->verticalScrollBar()->setValue(m_page == 1 ? 0 : static_cast<int>(r.top()));
}

void SituationsDialog::zoomBy(int steps) {
    if (steps > 0) m_viewer->zoomIn(steps);
    else if (steps < 0) m_viewer->zoomOut(-steps);
}

bool SituationsDialog::exportTo(ExportFormat format, const QString& path, int textWidth) const {
    QDir().mkpath(QFileInfo(path).absolutePath());
    if (format == ExportFormat::Pdf) {
        QPdfWriter writer(path);
        writer.setPageSize(QPageSize(QPageSize::A4));
        QTextDocument doc;
        doc.setHtml(QString::fromStdString(exportHtml(m_report)));
        doc.print(&writer);
        return QFile::exists(path);
    }
    std::string bytes;
    switch (format) {
        case ExportFormat::Docx: bytes = exportDocx(m_report); break;
        case ExportFormat::Text: bytes = exportTextUtf16(m_report, textWidth); break;
        case ExportFormat::Html: bytes = exportHtml(m_report); break;
        case ExportFormat::Rtf: bytes = exportRtf(m_report); break;
        case ExportFormat::Pdf: break;
    }
    if (bytes.empty()) return false;
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) return false;
    return f.write(bytes.data(), static_cast<qint64>(bytes.size())) == static_cast<qint64>(bytes.size());
}

void SituationsDialog::askAndExport(ExportFormat format) {
    const QString def = defaultExportDirectory() + QDir::separator() + QString::fromStdString(m_report.documentName) +
                        QLatin1Char('.') + extensionOf(format);
    ExportPathDialog dlg(nameOf(format), def, format, this);
    if (dlg.exec() != QDialog::Accepted) return;
    if (exportTo(format, dlg.path(), dlg.textWidth()))
        QMessageBox::information(this, tr("Información"), tr("Exportación finalizada."));
    else
        QMessageBox::warning(this, tr("Error"), tr("No se ha podido escribir el fichero:\n%1").arg(dlg.path()));
}

void SituationsDialog::print() {
    QPrinter printer;
    QPrintDialog dialog(&printer, this);
    dialog.setWindowTitle(tr("Imprimir documento"));
    if (dialog.exec() != QDialog::Accepted) return;
    QTextDocument doc;
    doc.setHtml(QString::fromStdString(exportHtml(m_report)));
    doc.print(&printer);
}

ExportPathDialog::ExportPathDialog(const QString& title, const QString& defaultPath, ExportFormat format, QWidget* parent)
    : CypeDialog(tr("Exportar a %1").arg(title), parent) {
    setFixedWidth(553);
    QVBoxLayout* body = contentLayout();
    body->addWidget(new QLabel(tr("Nombre del fichero"), this));
    auto* row = new QHBoxLayout;
    m_path = new QLineEdit(defaultPath, this);
    auto* browse = new QToolButton(this);
    browse->setText(QStringLiteral("…"));
    row->addWidget(m_path, 1);
    row->addWidget(browse);
    body->addLayout(row);
    connect(browse, &QToolButton::clicked, this, [this, format] {
        const QString p = QFileDialog::getSaveFileName(this, tr("Exportar"), m_path->text(),
                                                       QStringLiteral("*.%1").arg(SituationsDialog::extensionOf(format)));
        if (!p.isEmpty()) m_path->setText(QDir::toNativeSeparators(p));
    });
    if (format == ExportFormat::Text) {
        auto* w = new QHBoxLayout;
        w->addWidget(new QLabel(tr("Ancho de la página (en caracteres)"), this));
        m_width = new QSpinBox(this);
        m_width->setRange(60, 400);
        m_width->setValue(120);
        w->addWidget(m_width);
        w->addStretch(1);
        body->addLayout(w);
    } else if (format == ExportFormat::Html) {
        auto* dest = new QComboBox(this);
        dest->addItems({tr("Navegador de internet (Internet Explorer)"), tr("Word 97 y anteriores"),
                        tr("Word 2000 y posteriores")});
        body->addWidget(new QLabel(tr("Configuración de la exportación a HTML"), this));
        body->addWidget(dest);
        auto* res = new QSpinBox(this);
        res->setRange(48, 600);
        res->setValue(96);
        res->setSuffix(tr(" puntos/pulgada"));
        res->setEnabled(false);  // only used by the Word targets, which this version does not distinguish
        body->addWidget(new QLabel(tr("Resolución de imágenes"), this));
        body->addWidget(res);
    }
    body->addStretch(1);
    setupFooter(tr("Aceptar"), tr("Cancelar"));
    adjustSize();
}

QString ExportPathDialog::path() const { return m_path->text(); }
int ExportPathDialog::textWidth() const { return m_width ? m_width->value() : 120; }

}  // namespace cc::app
