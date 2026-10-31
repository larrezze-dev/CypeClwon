#include "NewProjectDialog.h"

#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>
#include <QVBoxLayout>

namespace cc::app {

namespace {
constexpr int kMaxPathLength = 259;  // Windows MAX_PATH minus terminator; keeps the file creatable everywhere.
const QString kFolderKey = QStringLiteral("newProject/lastFolder");
}  // namespace

NewProjectDialog::NewProjectDialog(const QString& initialFolder, QWidget* parent)
    : CypeDialog(tr("Nueva obra"), parent) {
    setFixedSize(537, 285);

    auto* nameGroup = new QGroupBox(tr("Nombre de la obra"), this);
    auto* nameLayout = new QVBoxLayout(nameGroup);
    auto* folderRow = new QHBoxLayout;
    m_folderLabel = new QLabel(nameGroup);
    m_folderLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_folderLabel->setWordWrap(false);
    auto* browseButton = new QPushButton(tr("Examinar"), nameGroup);
    folderRow->addWidget(m_folderLabel, 1);
    folderRow->addWidget(browseButton);
    nameLayout->addLayout(folderRow);

    auto* fileRow = new QHBoxLayout;
    fileRow->addWidget(new QLabel(tr("Nombre del fichero"), nameGroup));
    m_nameEdit = new QLineEdit(QStringLiteral("nuevo"), nameGroup);
    m_nameEdit->setMaxLength(100);
    fileRow->addWidget(m_nameEdit, 1);
    fileRow->addWidget(new QLabel(QString::fromLatin1(kSuffix), nameGroup));
    nameLayout->addLayout(fileRow);
    contentLayout()->addWidget(nameGroup);

    auto* descGroup = new QGroupBox(tr("Descripción"), this);
    auto* descLayout = new QVBoxLayout(descGroup);
    m_descriptionEdit = new QLineEdit(descGroup);
    m_descriptionEdit->setMaxLength(250);
    descLayout->addWidget(m_descriptionEdit);
    contentLayout()->addWidget(descGroup);
    contentLayout()->addStretch(1);

    setupFooter(tr("Aceptar"), tr("Cancelar"));
    setFolder(initialFolder);
    connect(browseButton, &QPushButton::clicked, this, &NewProjectDialog::browse);

    m_nameEdit->setFocus();
    m_nameEdit->selectAll();
}

QString NewProjectDialog::fileName() const { return m_nameEdit->text().trimmed(); }
QString NewProjectDialog::description() const { return m_descriptionEdit->text().trimmed(); }

QString NewProjectDialog::filePath() const {
    return QDir(m_folder).filePath(fileName() + QString::fromLatin1(kSuffix));
}

void NewProjectDialog::setFolder(const QString& folder) {
    m_folder = QDir::cleanPath(folder);
    // Shown with a trailing separator, as in the original.
    m_folderLabel->setText(QDir::toNativeSeparators(m_folder) + QDir::separator());
    m_folderLabel->setToolTip(m_folderLabel->text());
}

QString NewProjectDialog::validateFileName(const QString& rawName) {
    const QString name = rawName.trimmed();
    if (name.isEmpty()) return tr("Debe indicar un nombre de fichero.");
    static const QRegularExpression invalidChars(QStringLiteral(R"([\\/:*?"<>|\x00-\x1f])"));
    if (invalidChars.match(name).hasMatch()) {
        return tr("El nombre del fichero no puede contener ninguno de los caracteres siguientes: \\ / : * ? \" < > |");
    }
    if (name.endsWith(QLatin1Char('.'))) return tr("El nombre del fichero no puede terminar en un punto.");
    static const QRegularExpression reserved(QStringLiteral(R"(^(CON|PRN|AUX|NUL|COM[1-9]|LPT[1-9])(\..*)?$)"),
                                             QRegularExpression::CaseInsensitiveOption);
    if (reserved.match(name).hasMatch()) return tr("El nombre «%1» está reservado por el sistema operativo.").arg(name);
    return {};
}

QString NewProjectDialog::defaultFolder() {
    const QString remembered = QSettings().value(kFolderKey).toString();
    if (!remembered.isEmpty() && QFileInfo(remembered).isDir()) return remembered;
    return QDir(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation))
        .filePath(QStringLiteral("CYPE Ingenieros/Proyectos/CYPE 3D"));
}

void NewProjectDialog::rememberFolder(const QString& folder) { QSettings().setValue(kFolderKey, folder); }

void NewProjectDialog::browse() {
    const QString chosen = QFileDialog::getSaveFileName(this, tr("Nueva obra"), filePath(),
                                                        tr("Obra CYPE 3D (*.ed3)"), nullptr,
                                                        QFileDialog::DontConfirmOverwrite);
    if (chosen.isEmpty()) return;
    const QFileInfo info(chosen);
    setFolder(info.absolutePath());
    m_nameEdit->setText(info.completeBaseName());
}

void NewProjectDialog::accept() {
    const QString error = validateFileName(m_nameEdit->text());
    if (!error.isEmpty()) {
        QMessageBox::warning(this, tr("Aviso"), error);
        m_nameEdit->setFocus();
        return;
    }
    // The folder is created on demand when the project is saved, but only if it is really creatable.
    if (QFileInfo(m_folder).exists() && !QFileInfo(m_folder).isDir()) {
        QMessageBox::warning(this, tr("Aviso"), tr("La ruta indicada no es una carpeta."));
        return;
    }
    if (filePath().size() > kMaxPathLength) {
        QMessageBox::warning(this, tr("Aviso"), tr("La ruta completa del fichero es demasiado larga."));
        return;
    }
    if (QFileInfo::exists(filePath())) {
        const auto answer = QMessageBox::question(
            this, tr("Aviso"), tr("El fichero «%1» ya existe. ¿Desea sustituirlo?").arg(QDir::toNativeSeparators(filePath())),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (answer != QMessageBox::Yes) return;
    }
    CypeDialog::accept();
}

}  // namespace cc::app
