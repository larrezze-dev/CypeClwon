#pragma once
#include <QString>

#include "CypeDialog.h"

class QFileSystemModel;
class QLineEdit;
class QTableWidget;
class QToolButton;
class QTreeView;

namespace cc::app {

/// "Gestión archivos" (676x525): folder tree on the left, list of projects (.ed3) of the selected folder on the
/// right with the columns Obra | Descripción | Versión | Fecha. Reference: docs/screenshots/orig_gestion_archivos_01.png
/// and docs/research/ui/dialogs_gestion_archivos.md. Abrir, Nuevo, Copiar, Borrar, Comprimir and Descomprimir are wired (messages captured in
/// docs/screenshots/orig_ga_*.png; .cyp is a gzip container and backups are the sibling .ed$ files, see the research note); Buscar (enabled with more than one project; provisional dialog), Compartir (disabled: online service), Ejemplos (T-P2-08) and the Descripción/Versión columns (stored inside the file, T-P3-04) are gaps (ticket TK-018).
class FileManagerDialog : public CypeDialog {
    Q_OBJECT
public:
    explicit FileManagerDialog(const QString& folder, QWidget* parent = nullptr);

    void setFolder(const QString& folder);
    QString folder() const { return m_folder; }
    /// Project selected in the list (absolute path), empty when none.
    QString selectedFile() const;
    QTableWidget* table() const { return m_table; }

    /// Original messages (ids 139020, 46956, 402368), exposed for testing.
    static QString compressedMessage(const QString& archive);
    static QString decompressConfirmationMessage(const QString& folder);
    static QString decompressErrorMessage(const QString& archive, const QString& reason);

signals:
    void openRequested(const QString& path);
    void newRequested();

private:
    bool requireSelection();
    void copyProject();
    void deleteProject();
    void searchProject();
    void compressProjectAction();
    void decompressProjectAction();
    void reloadList();
    void revealFolder();
    QString m_folder;
    QFileSystemModel* m_model;
    QTreeView* m_tree;
    QTableWidget* m_table;
    QLineEdit* m_path;
    QToolButton* m_search = nullptr;
};

}  // namespace cc::app
