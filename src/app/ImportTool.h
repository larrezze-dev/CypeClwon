#pragma once
#include <QObject>
#include <QPointer>
#include <QString>
#include <functional>

#include "core/import_dxf.h"
#include "core/project.h"
#include "core/undo_stack.h"

class QAction;
class QDialog;
class QWidget;

namespace cc::app {

/// Geometria > Importar (T-P5-11, GUI part): "DXF o DWG" and "Fichero de texto". The geometry comes from the cores of
/// core/import_dxf.h, import_text.h and import_geometry.h; behaviour source: docs/research/ui/importar.md (sections 3.1-3.5).
///
/// DXF o DWG: file selector (filters #61042 / #61043) -> if the work has grouped planes, the question #63970 + #322563 (Sí ungroups
/// them, No cancels) -> "Selección de entidades" (#60990) -> reading -> if the work already has nodes, "Importar" (#29651) asks where
/// the geometry goes -> import (one undo step) -> warnings #65115 / #64502. DWG is not readable yet (the original uses the ODA SDK;
/// ticket TK-079): the error is shown at once. Load panels are not imported (the model has none yet; ticket TK-079).
/// Fichero de texto: selector "Nombre del fichero" (*.txt) -> reading window with the log -> Sí / No (#106880) -> import.
class ImportTool : public QObject {
    Q_OBJECT
public:
    enum class Kind { Dxf, Text };

    explicit ImportTool(QObject* parent = nullptr);

    QAction* dxfAction() const { return m_dxf; }
    QAction* textAction() const { return m_text; }

    void setProject(cc::core::Project* project, cc::core::UndoStack* undo);
    void setDialogParent(QWidget* parent) { m_dialogParent = parent; }
    /// Runs a modal dialog; returns QDialog::Accepted / Rejected. The default is exec(); tests replace it so that nothing blocks.
    void setDialogRunner(std::function<int(QDialog&)> runner) { m_runner = std::move(runner); }
    /// Chooses the file of a command (empty = cancelled). The default opens a QFileDialog; tests replace it.
    void setFileChooser(std::function<QString(Kind)> chooser) { m_chooser = std::move(chooser); }
    /// Asks a yes / no question (the planes to ungroup). The main window installs a message box; without one the answer is yes.
    void setConfirmer(std::function<bool(const QString&)> confirmer) { m_confirmer = std::move(confirmer); }

    /// Entry points of the buttons: choose the file and import it. True when something was added to the work.
    bool importDxf();
    bool importText();
    /// The same on a given file (what the buttons call once the file is chosen).
    bool importDxfFile(const QString& path);
    bool importTextFile(const QString& path);

    /// Number of the last ImportSummary items, for the status bar.
    const cc::core::ImportSummary& lastSummary() const { return m_summary; }
    /// Log lines of the last text reading (the lines the reading window shows).
    const QStringList& lastLog() const { return m_log; }

    static QString dxfFilter();
    static QString textFilter();

signals:
    /// Something the user must read (error or warning); `error` selects the icon.
    void messageRequested(const QString& title, const QString& text, bool error);
    void statusChanged(const QString& text);
    /// A button was pressed: any viewport command in progress must stop.
    void aboutToImport();
    /// The work changed through an import (the history changed too).
    void imported();

private:
    int run(QDialog& dialog);
    bool confirm(const QString& question);
    bool hasGroupedPlanes() const;
    bool commit(const cc::core::ImportedGeometry& geometry, bool ungroup);
    void updateEnabled();

    QAction* m_dxf = nullptr;
    QAction* m_text = nullptr;
    cc::core::Project* m_project = nullptr;
    cc::core::UndoStack* m_undo = nullptr;
    QPointer<QWidget> m_dialogParent;
    std::function<int(QDialog&)> m_runner;
    std::function<QString(Kind)> m_chooser;
    std::function<bool(const QString&)> m_confirmer;
    cc::core::ImportSummary m_summary;
    QStringList m_log;
};

}  // namespace cc::app
