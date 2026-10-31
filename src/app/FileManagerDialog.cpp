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

#include "Icons.h"

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
    for (const QString& caption : {tr("Copiar"), tr("Borrar"), tr("Buscar")}) bar->addWidget(barButton(caption, QString(), this, false));
    bar->addWidget(verticalSeparator(this));
    for (const QString& caption : {tr("Comprimir"), tr("Descomprimir")}) bar->addWidget(barButton(caption, QString(), this, false));
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

QString FileManagerDialog::selectedFile() const {
    const auto rows = m_table->selectionModel()->selectedRows();
    if (rows.isEmpty()) return {};
    return m_table->item(rows.first().row(), 0)->data(Qt::UserRole).toString();
}

}  // namespace cc::app
