#pragma once
#include <QString>
#include <QStringList>

#include "CypeDialog.h"

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QTreeWidget;

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

/// One project of the folder as the thesaurus sees it: name, description (both columns of the list) and the path to open.
struct SearchEntry {
    QString name;
    QString description;
    QString path;
};

/// "Localización de obra por tesauro" (id 139028), modal opened by "Gestión archivos" > Buscar. Measured in the original
/// (dialogs_gestion_archivos.md, 2026-11-06): an editable combo on top, a column of words (the thesaurus) on the left, a preview of the
/// chosen project on the right and an "Obra / Descripción" list below with the projects that contain the word; Aceptar / Cancelar.
/// The words are the whole file name without extension plus every word of the description, in lower case, sorted alphabetically,
/// each one once (a lone "-" is a word). The search ignores case. Aceptar OPENS the project chosen in the list.
/// TODO(medir): contents of the lower list before a word is chosen (the clone lists every project), partial words typed in the combo,
/// the preview (the original draws a document sheet "Obra: <name>.ed3" + description; the clone leaves the panel empty).
class ProjectThesaurusDialog : public CypeDialog {
    Q_OBJECT
public:
    ProjectThesaurusDialog(const QList<SearchEntry>& entries, QWidget* parent = nullptr);
    /// Index (into the entries given) of the accepted project, -1 when none.
    int chosenIndex() const { return m_chosen; }
    QComboBox* combo() const { return m_combo; }
    QListWidget* wordList() const { return m_words; }
    QTreeWidget* projectList() const { return m_projects; }
    QWidget* previewPanel() const { return m_preview; }

    /// Thesaurus of the entries: lower-case file names and description words, sorted, without repetitions.
    static QStringList thesaurusWords(const QList<SearchEntry>& entries);
    /// True when the entry contains the word (case-insensitive): its file name or one of the words of its description.
    static bool contains(const SearchEntry& entry, const QString& word);

private:
    void selectWord(const QString& word);
    void refreshProjects(const QString& word, bool noMatch = false);
    void choose();
    QList<SearchEntry> m_entries;
    QComboBox* m_combo;
    QListWidget* m_words;
    QWidget* m_preview;
    QTreeWidget* m_projects;
    bool m_syncing = false;
    int m_chosen = -1;
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
