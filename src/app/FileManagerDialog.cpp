#include "FileManagerDialog.h"

#include <QComboBox>
#include <QDir>
#include <QFileInfo>
#include <QFileSystemModel>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLineEdit>
#include <QSplitter>
#include <QTableWidget>
#include <QToolButton>
#include <QTreeView>
#include <QVBoxLayout>

#include <QFile>
#include <QFileDialog>
#include <QMessageBox>
#include <QSettings>

#include "FileOpsDialogs.h"
#include "Icons.h"
#include "core/project_archive.h"

namespace cc::app {

namespace {

QToolButton* barButton(const QString& text, const QString& icon, QWidget* parent, bool enabled) {
    auto* button = new QToolButton(parent);
    button->setText(text);
    if (!icon.isEmpty()) button->setIcon(themedIcon(icon));
    button->setIconSize(QSize(28, 28));
    button->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    button->setAutoRaise(true);
    button->setMinimumWidth(64);
    button->setEnabled(enabled);
    if (!enabled) button->setToolTip(QObject::tr("Pendiente de investigar en el original (ticket TK-018)."));
    return button;
}

QFrame* verticalSeparator(QWidget* parent) {
    auto* line = new QFrame(parent);
    line->setFrameShape(QFrame::VLine);
    line->setFrameShadow(QFrame::Sunken);
    return line;
}

}  // namespace

FileManagerDialog::FileManagerDialog(const QString& folder, QWidget* parent)
    : CypeDialog(tr("Gestión archivos"), parent) {
    setFixedSize(676, 525);
    contentLayout()->setContentsMargins(0, 0, 0, 0);
    contentLayout()->setSpacing(0);

    // Tool bar: Abrir, Nuevo, Copiar, Borrar, Buscar | Comprimir, Descomprimir | Compartir | Ejemplos.
    auto* bar = new QHBoxLayout;
    bar->setContentsMargins(6, 4, 6, 4);
    auto* open = barButton(tr("Abrir"), QStringLiteral("folder_open"), this, true);
    auto* create = barButton(tr("Nuevo"), QStringLiteral("file_new"), this, true);
    bar->addWidget(open);
    bar->addWidget(create);
    auto* copy = barButton(tr("Copiar"), QStringLiteral("copy"), this, true);
    auto* remove = barButton(tr("Borrar"), QStringLiteral("trash"), this, true);
    bar->addWidget(copy);
    bar->addWidget(remove);
    // Buscar showed nothing visible when driven in the original: behaviour still unknown (ticket TK-018).
    bar->addWidget(barButton(tr("Buscar"), QString(), this, false));
    bar->addWidget(verticalSeparator(this));
    auto* compress = barButton(tr("Comprimir"), QStringLiteral("compress"), this, true);
    auto* decompress = barButton(tr("Descomprimir"), QStringLiteral("decompress"), this, true);
    bar->addWidget(compress);
    bar->addWidget(decompress);
    bar->addWidget(verticalSeparator(this));
    bar->addWidget(barButton(tr("Compartir"), QString(), this, false));
    bar->addWidget(verticalSeparator(this));
    auto* examples = barButton(tr("Ejemplos"), QStringLiteral("gift"), this, false);
    bar->addWidget(examples);
    bar->addStretch(1);
    contentLayout()->addLayout(bar);

    auto* splitter = new QSplitter(Qt::Horizontal, this);
    auto* left = new QWidget(splitter);
    auto* leftLayout = new QVBoxLayout(left);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->setSpacing(0);
    auto* pathRow = new QHBoxLayout;
    m_path = new QLineEdit(left);
    m_path->setReadOnly(true);
    auto* favourites = new QToolButton(left);
    favourites->setText(QStringLiteral("★"));
    favourites->setEnabled(false);
    favourites->setToolTip(tr("Pendiente de investigar en el original (ticket TK-018)."));
    pathRow->addWidget(m_path, 1);
    pathRow->addWidget(favourites);
    leftLayout->addLayout(pathRow);

    m_model = new QFileSystemModel(this);
    m_model->setFilter(QDir::AllDirs | QDir::NoDotAndDotDot | QDir::Drives);
    m_model->setRootPath(QString());
    m_tree = new QTreeView(left);
    m_tree->setModel(m_model);
    for (int column = 1; column < m_model->columnCount(); ++column) m_tree->hideColumn(column);
    m_tree->setHeaderHidden(true);
    leftLayout->addWidget(m_tree, 1);

    m_table = new QTableWidget(0, 4, splitter);
    m_table->setHorizontalHeaderLabels({tr("Obra"), tr("Descripción"), tr("Versión"), tr("Fecha")});
    m_table->verticalHeader()->setVisible(false);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setShowGrid(false);
    m_table->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_table->horizontalHeader()->setStretchLastSection(false);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_table->setColumnWidth(0, 100);
    m_table->setColumnWidth(2, 60);
    m_table->setColumnWidth(3, 80);
    splitter->addWidget(left);
    splitter->addWidget(m_table);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({210, 466});
    contentLayout()->addWidget(splitter, 1);

    connect(m_tree->selectionModel(), &QItemSelectionModel::currentChanged, this, [this](const QModelIndex& index) {
        const QString path = m_model->filePath(index);
        if (!path.isEmpty() && path != m_folder) setFolder(path);
    });
    // The model loads folders lazily: reveal the current folder again as its ancestors finish loading.
    connect(m_model, &QFileSystemModel::directoryLoaded, this, [this] { revealFolder(); });
    connect(open, &QToolButton::clicked, this, [this] {
        if (!selectedFile().isEmpty()) emit openRequested(selectedFile());
    });
    connect(m_table, &QTableWidget::cellDoubleClicked, this, [this] {
        if (!selectedFile().isEmpty()) emit openRequested(selectedFile());
    });
    connect(create, &QToolButton::clicked, this, &FileManagerDialog::newRequested);
    connect(copy, &QToolButton::clicked, this, &FileManagerDialog::copyProject);
    connect(remove, &QToolButton::clicked, this, &FileManagerDialog::deleteProject);
    connect(compress, &QToolButton::clicked, this, &FileManagerDialog::compressProjectAction);
    connect(decompress, &QToolButton::clicked, this, &FileManagerDialog::decompressProjectAction);
    setFolder(folder);
}

void FileManagerDialog::setFolder(const QString& folder) {
    m_folder = QDir::cleanPath(folder);
    m_path->setText(QDir::toNativeSeparators(m_folder));
    m_model->setRootPath(m_folder);  // makes the model fetch the whole path so the tree can expand to it
    revealFolder();
    reloadList();
}

void FileManagerDialog::revealFolder() {
    const QModelIndex index = m_model->index(m_folder);
    if (!index.isValid()) return;
    for (QModelIndex ancestor = index.parent(); ancestor.isValid(); ancestor = ancestor.parent()) m_tree->expand(ancestor);
    if (m_tree->currentIndex() != index) {
        QSignalBlocker block(m_tree->selectionModel());
        m_tree->setCurrentIndex(index);
    }
    m_tree->scrollTo(index);
}

void FileManagerDialog::reloadList() {
    m_table->setRowCount(0);
    const QFileInfoList files = QDir(m_folder).entryInfoList({QStringLiteral("*.ed3")}, QDir::Files, QDir::Name | QDir::IgnoreCase);
    for (const QFileInfo& info : files) {
        const int row = m_table->rowCount();
        m_table->insertRow(row);
        auto* name = new QTableWidgetItem(info.completeBaseName());
        name->setData(Qt::UserRole, info.absoluteFilePath());
        m_table->setItem(row, 0, name);
        m_table->setItem(row, 1, new QTableWidgetItem());  // stored inside the file (T-P3-04, ticket TK-018)
        m_table->setItem(row, 2, new QTableWidgetItem());
        m_table->setItem(row, 3, new QTableWidgetItem(info.lastModified().toString(QStringLiteral("dd/MM/yyyy"))));
    }
}

bool FileManagerDialog::requireSelection() {
    if (!selectedFile().isEmpty()) return true;
    QMessageBox::information(this, tr("Gestión archivos"), tr("Seleccione una obra de la lista."));
    return false;
}

void FileManagerDialog::copyProject() {
    if (!requireSelection()) return;
    const QFileInfo source(selectedFile());
    QSettings settings;
    const QString startDir = settings.value(QStringLiteral("fileManager/copyFolder"), m_folder).toString();
    QString target = QFileDialog::getSaveFileName(this, tr("Copiar obra"), QDir(startDir).filePath(source.fileName()),
                                                  tr("Obra CYPE 3D (*.ed3)"));
    if (target.isEmpty()) return;
    if (QFileInfo(target).suffix().isEmpty()) target += QStringLiteral(".ed3");
    if (QFileInfo(target) == source) {
        QMessageBox::warning(this, tr("Gestión archivos"), tr("El destino coincide con la obra original."));
        return;
    }
    settings.setValue(QStringLiteral("fileManager/copyFolder"), QFileInfo(target).absolutePath());
    if (QFile::exists(target) && !QFile::remove(target)) {  // the save dialog already asked about overwriting
        QMessageBox::warning(this, tr("Gestión archivos"), tr("No se puede sobrescribir «%1».").arg(QDir::toNativeSeparators(target)));
        return;
    }
    if (!QFile::copy(source.absoluteFilePath(), target))
        QMessageBox::warning(this, tr("Gestión archivos"), tr("No se ha podido copiar la obra."));
    reloadList();
}

void FileManagerDialog::deleteProject() {
    if (!requireSelection()) return;
    DeleteProjectDialog confirm(this);
    if (confirm.exec() != QDialog::Accepted) return;
    const std::filesystem::path project = selectedFile().toStdWString();
    bool good = QFile::moveToTrash(selectedFile());
    if (good && confirm.includeBackups()) {
        for (const auto& backup : cc::core::backupFilesOf(project)) QFile::moveToTrash(QString::fromStdWString(backup.wstring()));
    }
    if (!good) QMessageBox::warning(this, tr("Gestión archivos"), tr("No se ha podido enviar la obra a la Papelera de reciclaje."));
    reloadList();
}

void FileManagerDialog::compressProjectAction() {
    if (!requireSelection()) return;
    CompressDialog dialog(selectedFile(), this);
    if (dialog.exec() != QDialog::Accepted) return;
    const QString archive = dialog.archivePath();
    if (archive.isEmpty() || QFileInfo(archive).isDir()) {
        QMessageBox::warning(this, tr("Gestión archivos"), tr("Indique un fichero de destino válido."));
        return;
    }
    if (QFile::exists(archive) &&
        QMessageBox::question(this, tr("Comprimir"), tr("«%1» ya existe. ¿Desea reemplazarlo?").arg(QDir::toNativeSeparators(archive))) !=
            QMessageBox::Yes)
        return;
    const auto result = cc::core::compressProject(selectedFile().toStdWString(), archive.toStdWString(), dialog.includeBackups());
    if (!result.ok) QMessageBox::warning(this, tr("Comprimir"), QString::fromStdString(result.error));
}

void FileManagerDialog::decompressProjectAction() {
    const QString archive = QFileDialog::getOpenFileName(this, tr("Selección de Obra Comprimida"), m_folder, tr("Obras comprimidas (*.cyp)"));
    if (archive.isEmpty()) return;
    const auto result = cc::core::extractProject(archive.toStdWString(), QString(m_folder).toStdWString());
    if (!result.ok) QMessageBox::warning(this, tr("Descomprimir"), QString::fromStdString(result.error));
    reloadList();
}

QString FileManagerDialog::selectedFile() const {
    const auto rows = m_table->selectionModel()->selectedRows();
    if (rows.isEmpty()) return {};
    return m_table->item(rows.first().row(), 0)->data(Qt::UserRole).toString();
}

}  // namespace cc::app
