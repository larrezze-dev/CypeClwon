#include "FileOpsDialogs.h"

#include <QCheckBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QToolButton>
#include <QVBoxLayout>

#include "core/project_archive.h"

namespace cc::app {

DeleteProjectDialog::DeleteProjectDialog(QWidget* parent) : CypeDialog(tr("Pregunta"), parent) {
    setFixedSize(472, 168);
    auto* question = new QLabel(tr("¿Confirma que quiere enviar esta obra a la Papelera de reciclaje?"), this);
    question->setWordWrap(true);
    m_backups = new QCheckBox(tr("Enviar copias de seguridad"), this);
    m_backups->setChecked(true);
    contentLayout()->setContentsMargins(20, 20, 20, 10);
    contentLayout()->addWidget(question);
    contentLayout()->addWidget(m_backups);
    contentLayout()->addStretch(1);
    setupFooter(tr("Sí"), tr("No"));
}

bool DeleteProjectDialog::includeBackups() const { return m_backups->isChecked(); }

CompressDialog::CompressDialog(const QString& projectPath, QWidget* parent)
    : CypeDialog(tr("Comprimir"), parent), m_project(projectPath) {
    setFixedSize(547, 190);
    const QFileInfo info(projectPath);
    m_path = new QLineEdit(QDir::toNativeSeparators(info.dir().filePath(info.completeBaseName() + QStringLiteral(".cyp"))), this);
    auto* browse = new QToolButton(this);
    browse->setText(QStringLiteral("…"));
    browse->setToolTip(tr("Seleccionar fichero de destino"));
    m_backups = new QCheckBox(tr("Incluir copias de seguridad"), this);
    m_size = new QLabel(this);
    m_size->setTextFormat(Qt::RichText);
    auto* row = new QHBoxLayout;
    row->addWidget(m_path, 1);
    row->addWidget(browse);
    contentLayout()->setContentsMargins(14, 10, 14, 10);
    contentLayout()->addLayout(row);
    contentLayout()->addWidget(m_backups);
    contentLayout()->addWidget(m_size);
    contentLayout()->addStretch(1);
    setupFooter(tr("Aceptar"), tr("Cancelar"));
    connect(m_backups, &QCheckBox::toggled, this, [this] { refreshSize(); });
    connect(browse, &QToolButton::clicked, this, [this] {
        const QString chosen = QFileDialog::getSaveFileName(this, tr("Comprimir"), m_path->text(), tr("Obras comprimidas (*.cyp)"));
        if (!chosen.isEmpty()) m_path->setText(QDir::toNativeSeparators(chosen));
    });
    refreshSize();
}

QString CompressDialog::archivePath() const { return QDir::cleanPath(m_path->text().trimmed()); }
bool CompressDialog::includeBackups() const { return m_backups->isChecked(); }

QString CompressDialog::kilobytesText(quint64 bytes) { return QString::number(static_cast<double>(bytes) / 1024.0, 'f', 2); }

void CompressDialog::refreshSize() {
    const quint64 bytes = cc::core::archiveSourceBytes(m_project.toStdWString(), m_backups->isChecked());
    m_size->setText(tr("Total bytes a enviar: <span style=\"color:#c00000\">%1</span> KBytes").arg(kilobytesText(bytes)));
}

RecentFilesDialog::RecentFilesDialog(const QStringList& paths, QWidget* parent) : CypeDialog(tr("Últimos ficheros"), parent) {
    setMinimumSize(620, 380);
    m_list = new QListWidget(this);
    for (int i = 0; i < paths.size(); ++i) {
        auto* item = new QListWidgetItem(QStringLiteral("%1 %2").arg(i + 1).arg(QDir::toNativeSeparators(paths[i])), m_list);
        item->setData(Qt::UserRole, paths[i]);
        if (!QFileInfo::exists(paths[i])) item->setForeground(Qt::gray);  // vanished files stay listed, in grey
    }
    if (m_list->count() > 0) m_list->setCurrentRow(0);
    contentLayout()->addWidget(m_list, 1);
    setupFooter(tr("Abrir"), tr("Cancelar"));
    connect(m_list, &QListWidget::itemDoubleClicked, this, [this] { choose(); });
    disconnect(acceptButton(), &QPushButton::clicked, this, &QDialog::accept);
    connect(acceptButton(), &QPushButton::clicked, this, [this] { choose(); });
}

void RecentFilesDialog::choose() {
    const auto* item = m_list->currentItem();
    if (!item) return;
    m_chosen = item->data(Qt::UserRole).toString();
    accept();
}

}  // namespace cc::app
