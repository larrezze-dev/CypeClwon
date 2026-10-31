#pragma once
#include <QString>
#include <QStringList>

#include "CypeDialog.h"

class QCheckBox;
class QLabel;
class QLineEdit;
class QListWidget;

namespace cc::app {

/// "Pregunta" box shown by Borrar: "¿Confirma que quiere enviar esta obra a la Papelera de reciclaje?" with the
/// checked box "Enviar copias de seguridad" and the buttons Sí / No. Reference: docs/screenshots/orig_ga_borrar.png.
class DeleteProjectDialog : public CypeDialog {
    Q_OBJECT
public:
    explicit DeleteProjectDialog(QWidget* parent = nullptr);
    bool includeBackups() const;

private:
    QCheckBox* m_backups;
};

/// "Comprimir": destination .cyp path (default: next to the project), "Incluir copias de seguridad" and the red
/// "Total bytes a enviar: N KBytes" line. Reference: docs/screenshots/orig_ga_comprimir.png.
class CompressDialog : public CypeDialog {
    Q_OBJECT
public:
    explicit CompressDialog(const QString& projectPath, QWidget* parent = nullptr);
    QString archivePath() const;
    bool includeBackups() const;
    /// "804.82" style text for a byte count (KBytes with two decimals, '.' separator as in the original).
    static QString kilobytesText(quint64 bytes);

private:
    void refreshSize();
    QString m_project;
    QLineEdit* m_path;
    QCheckBox* m_backups;
    QLabel* m_size;
};

/// "Últimos ficheros" list behind the "Más..." button of the start page. The original's dialog was never captured
/// (the button is disabled with few recent files): this is a provisional layout, see ticket TK-018.
class RecentFilesDialog : public CypeDialog {
    Q_OBJECT
public:
    RecentFilesDialog(const QStringList& paths, QWidget* parent = nullptr);
    QString chosenFile() const { return m_chosen; }
    QListWidget* list() const { return m_list; }

private:
    void choose();
    QListWidget* m_list;
    QString m_chosen;
};

}  // namespace cc::app
