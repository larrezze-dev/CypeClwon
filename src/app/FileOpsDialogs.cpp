#include "FileOpsDialogs.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFrame>
#include <QRegularExpression>
#include <QSignalBlocker>
#include <QSplitter>
#include <QTreeWidget>
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

namespace cc::app {

ProjectThesaurusDialog::ProjectThesaurusDialog(const QList<SearchEntry>& entries, QWidget* parent)
    : CypeDialog(tr("Localización de obra por tesauro"), parent), m_entries(entries) {
    setMinimumSize(560, 460);
    const QStringList words = thesaurusWords(entries);
    m_combo = new QComboBox(this);
    m_combo->setEditable(true);
    m_combo->setInsertPolicy(QComboBox::NoInsert);
    m_combo->addItems(words);
    m_combo->setCurrentIndex(-1);
    m_combo->clearEditText();
    contentLayout()->addWidget(m_combo);

    auto* splitter = new QSplitter(Qt::Horizontal, this);
    m_words = new QListWidget(splitter);
    m_words->addItems(words);
    m_preview = new QFrame(splitter);
    m_preview->setObjectName(QStringLiteral("thesaurusPreview"));
    m_preview->setAutoFillBackground(true);
    m_preview->setBackgroundRole(QPalette::Base);
    static_cast<QFrame*>(m_preview)->setFrameShape(QFrame::StyledPanel);
    splitter->addWidget(m_words);
    splitter->addWidget(m_preview);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({170, 360});
    contentLayout()->addWidget(splitter, 2);

    m_projects = new QTreeWidget(this);
    m_projects->setColumnCount(2);
    m_projects->setHeaderLabels({tr("Obra"), tr("Descripción")});
    m_projects->setRootIsDecorated(false);
    contentLayout()->addWidget(m_projects, 1);
    setupFooter(tr("Aceptar"), tr("Cancelar"));

    connect(m_words, &QListWidget::currentTextChanged, this, [this](const QString& word) {
        if (!m_syncing && !word.isEmpty()) selectWord(word);
    });
    connect(m_combo, &QComboBox::editTextChanged, this, [this](const QString& text) {
        if (!m_syncing) selectWord(text.trimmed());
    });
    connect(m_projects, &QTreeWidget::itemDoubleClicked, this, [this] { choose(); });
    disconnect(acceptButton(), &QPushButton::clicked, this, &QDialog::accept);
    connect(acceptButton(), &QPushButton::clicked, this, [this] { choose(); });
    refreshProjects(QString());
    m_combo->setFocus();
}

QStringList ProjectThesaurusDialog::thesaurusWords(const QList<SearchEntry>& entries) {
    QStringList words;
    for (const SearchEntry& entry : entries) {
        words << entry.name.toLower();
        words << entry.description.toLower().split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
    }
    words.removeDuplicates();
    words.sort();
    return words;
}

bool ProjectThesaurusDialog::contains(const SearchEntry& entry, const QString& word) {
    const QString wanted = word.toLower();
    if (entry.name.toLower() == wanted) return true;
    return entry.description.toLower().split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts).contains(wanted);
}

void ProjectThesaurusDialog::selectWord(const QString& word) {
    const QSignalBlocker blockWords(m_words);
    const QSignalBlocker blockCombo(m_combo);
    m_syncing = true;
    const auto found = m_words->findItems(word.toLower(), Qt::MatchExactly);  // case-insensitive: the thesaurus is lower case
    m_words->setCurrentItem(found.isEmpty() ? nullptr : found.first());
    if (found.isEmpty()) m_words->clearSelection();
    else if (m_combo->currentText() != found.first()->text() && m_combo->currentText().toLower() != word.toLower())
        m_combo->setEditText(found.first()->text());
    m_syncing = false;
    refreshProjects(word.isEmpty() ? QString() : (found.isEmpty() ? QStringLiteral("") : found.first()->text()));
}

void ProjectThesaurusDialog::refreshProjects(const QString& word, bool noMatch) {
    m_projects->clear();
    for (int i = 0; i < m_entries.size() && !noMatch; ++i) {
        if (!word.isEmpty() && !contains(m_entries[i], word)) continue;
        auto* item = new QTreeWidgetItem(m_projects, {m_entries[i].name, m_entries[i].description});
        item->setData(0, Qt::UserRole, i);
    }
    if (m_projects->topLevelItemCount() > 0) m_projects->setCurrentItem(m_projects->topLevelItem(0));
    acceptButton()->setEnabled(m_projects->topLevelItemCount() > 0);
}

void ProjectThesaurusDialog::choose() {
    const QTreeWidgetItem* item = m_projects->currentItem();
    if (!item) return;
    m_chosen = item->data(0, Qt::UserRole).toInt();
    accept();
}

}  // namespace cc::app
