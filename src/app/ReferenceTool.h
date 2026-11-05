#pragma once
#include <QDialog>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QString>
#include <functional>
#include <vector>

#include "CypeDialog.h"
#include "core/project.h"
#include "core/reference_edit.h"
#include "core/selection.h"
#include "core/undo_stack.h"

class QAction;
class QCheckBox;
class QComboBox;
class QLineEdit;
class QRadioButton;
class QWidget;

namespace cc::app {

class Viewport3D;

/// Floating window "Propiedades" of "Asignar referencia" (herramientas.md 6.1): "Tipo de elemento" combo (default Nudos) and the radios
/// "Individual" (default) / "Múltiple". Not modal: the viewport stays usable.
class ReferencePanel : public CypeDialog {
    Q_OBJECT
public:
    explicit ReferencePanel(QWidget* parent = nullptr);
    cc::core::ReferenceKind kind() const;
    bool multiple() const;
    QComboBox* kindCombo() const { return m_kind; }
    QRadioButton* individualRadio() const { return m_individual; }
    QRadioButton* multipleRadio() const { return m_multiple; }

signals:
    void kindChanged();
    void closed();

protected:
    void done(int result) override;

private:
    QComboBox* m_kind = nullptr;
    QRadioButton* m_individual = nullptr;
    QRadioButton* m_multiple = nullptr;
};

/// Floating window "Propiedades - Buscar": "Tipo de elemento", "Referencia" and "Sólo palabra completa". The search runs when the text
/// changes (and on Intro): what it finds is selected in the 3D windows.
class FindPanel : public CypeDialog {
    Q_OBJECT
public:
    explicit FindPanel(QWidget* parent = nullptr);
    cc::core::ReferenceKind kind() const;
    QString text() const;
    bool wholeWord() const;
    QComboBox* kindCombo() const { return m_kind; }
    QLineEdit* textEdit() const { return m_text; }
    QCheckBox* wholeWordBox() const { return m_whole; }

signals:
    void changed();

private:
    QComboBox* m_kind = nullptr;
    QLineEdit* m_text = nullptr;
    QCheckBox* m_whole = nullptr;
};

/// Small modal dialog that asks for a reference text ("Referencia").
class ReferenceTextDialog : public CypeDialog {
    Q_OBJECT
public:
    ReferenceTextDialog(const QString& title, const QString& text, QWidget* parent = nullptr);
    QString text() const;
    QLineEdit* edit() const { return m_edit; }

private:
    QLineEdit* m_edit = nullptr;
};

/// Geometria > Herramientas > "Asignar referencia" (ABC with a pencil) and "Buscar" (magnifier). Measured in the original: states
/// "Herramientas - Asignar referencia | - LMB: Seleccione un elemento." and the two floating windows above. The model keeps references
/// for nodes (automatic N<id>) and shells (automatic L<id>) only (core/reference_edit.h).
///  - Individual (PROVISIONAL, the original does not say how the text is typed): LMB on an element asks its new reference in a small
///    dialog.
///  - Múltiple (PROVISIONAL): LMB / window selects elements, RMB asks the first reference and the following ones (in id order) take the
///    consecutive references N1, N2... (nextReference).
class ReferenceTool : public QObject {
    Q_OBJECT
public:
    enum class Mode { None, Assign };

    explicit ReferenceTool(QObject* parent = nullptr);

    QAction* assignAction() const { return m_assign; }
    QAction* findAction() const { return m_find; }

    void setProject(cc::core::Project* project, cc::core::UndoStack* undo);
    void addViewport(Viewport3D* viewport);
    void deactivate();
    void structureChanged();
    Mode mode() const { return m_mode; }
    ReferencePanel* assignPanel() const { return m_panel.data(); }
    FindPanel* findPanel() const { return m_findPanel.data(); }

    void setDialogParent(QWidget* parent) { m_dialogParent = parent; }
    /// Runs a modal dialog; default exec(). Tests replace it.
    void setDialogRunner(std::function<int(QDialog&)> runner) { m_runner = std::move(runner); }

    static QString statusText(Mode mode);

    /// Direct entry points. True on success.
    bool assignOne(cc::core::ReferenceKind kind, int id);
    bool assignMultiple(cc::core::ReferenceKind kind, const std::vector<int>& ids);
    /// Runs the search of the open "Buscar" panel (or of the given values): selects the matches and reports how many.
    int search(cc::core::ReferenceKind kind, const QString& text, bool wholeWord);

signals:
    void statusChanged(const QString& text);
    void aboutToActivate();
    void modeChanged(cc::app::ReferenceTool::Mode mode);
    void messageRequested(const QString& text);

private:
    void start();
    void configureViewports();
    void updateEnabled();
    void pruneViewports();
    void onSelectionChanged(Viewport3D* viewport);
    void onActOnSelection(Viewport3D* viewport);
    void openFind();
    int run(QDialog& dialog);
    cc::core::ElementKind elementKind() const;

    QAction* m_assign = nullptr;
    QAction* m_find = nullptr;
    cc::core::Project* m_project = nullptr;
    cc::core::UndoStack* m_undo = nullptr;
    QList<QPointer<Viewport3D>> m_viewports;
    QPointer<QWidget> m_dialogParent;
    QPointer<ReferencePanel> m_panel;
    QPointer<FindPanel> m_findPanel;
    std::function<int(QDialog&)> m_runner;
    Mode m_mode = Mode::None;
    bool m_internalSelection = false;
};

}  // namespace cc::app

Q_DECLARE_METATYPE(cc::app::ReferenceTool::Mode)
