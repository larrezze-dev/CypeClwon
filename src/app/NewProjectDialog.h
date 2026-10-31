#pragma once
#include <QString>

#include "CypeDialog.h"

class QLabel;
class QLineEdit;

namespace cc::app {

/// "Nueva obra" dialog: folder, file name (fixed .ed3 suffix) and description of a new project.
class NewProjectDialog : public CypeDialog {
    Q_OBJECT
public:
    static constexpr const char* kSuffix = ".ed3";

    explicit NewProjectDialog(const QString& initialFolder, QWidget* parent = nullptr);

    QString folder() const { return m_folder; }
    QString fileName() const;     ///< Name without suffix.
    QString description() const;
    QString filePath() const;     ///< Folder + name + ".ed3", with native-independent '/' separators.

    /// Returns an error message (Spanish, ready to show) when `name` cannot be a file name; empty if valid.
    static QString validateFileName(const QString& name);

    /// Last folder used, persisted in QSettings; falls back to <Documents>/CYPE Ingenieros/Proyectos/CYPE 3D.
    static QString defaultFolder();
    static void rememberFolder(const QString& folder);

public slots:
    void accept() override;

private slots:
    void browse();

private:
    void setFolder(const QString& folder);

    QString m_folder;
    QLabel* m_folderLabel = nullptr;
    QLineEdit* m_nameEdit = nullptr;
    QLineEdit* m_descriptionEdit = nullptr;
};

}  // namespace cc::app
