#pragma once
#include <QString>

#include "CypeDialog.h"

class QFileSystemModel;
class QLineEdit;
class QTableWidget;
class QTreeView;

namespace cc::app {

/// "Gestión archivos" (676x525): folder tree on the left, list of projects (.ed3) of the selected folder on the
/// right with the columns Obra | Descripción | Versión | Fecha. Reference: docs/screenshots/orig_gestion_archivos_01.png
/// and docs/research/ui/dialogs_gestion_archivos.md. Only Abrir, Nuevo and Ejemplos are wired; the behaviour of the
/// other buttons and the Descripción/Versión columns (stored inside the file, T-P3-04) are gaps (ticket TK-018).
class FileManagerDialog : public CypeDialog {
    Q_OBJECT
public:
    explicit FileManagerDialog(const QString& folder, QWidget* parent = nullptr);

    void setFolder(const QString& folder);
    QString folder() const { return m_folder; }
    /// Project selected in the list (absolute path), empty when none.
    QString selectedFile() const;
    QTableWidget* table() const { return m_table; }

signals:
    void openRequested(const QString& path);
    void newRequested();

private:
    void reloadList();
    void revealFolder();
    QString m_folder;
    QFileSystemModel* m_model;
    QTreeView* m_tree;
    QTableWidget* m_table;
    QLineEdit* m_path;
};

}  // namespace cc::app
