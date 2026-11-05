#pragma once
#include <QList>
#include <QObject>
#include <QPointer>
#include <QString>
#include <functional>
#include <vector>

#include "core/project.h"
#include "core/undo_stack.h"

class QAction;
class QActionGroup;
class QDialog;
class QWidget;

namespace cc::app {

class Viewport3D;

/// Geometria > Barras > Seccion / Material (T-P5-07, GUI part). Behaviour: docs/research/ui/describir_barras.md.
/// Both are modes of the viewport like Agrupar: LMB selects bars (click or window, Ctrl adds, Shift removes), RMB acts on the
/// selection and opens "Describir" (Seccion) or "Describir material" (Material); the selection is emptied afterwards and the
/// command stays active. One undo step per accepted "Describir": the edits of the library made from the dialog, the refresh of
/// the descriptions that use an edited profile and the description of the bars.
class BarDescribeTool : public QObject {
    Q_OBJECT
public:
    enum class Mode { None, Section, Material };

    explicit BarDescribeTool(QObject* parent = nullptr);

    QAction* sectionAction() const { return m_section; }
    QAction* materialAction() const { return m_material; }
    QActionGroup* actionGroup() const { return m_actions; }

    void setProject(cc::core::Project* project, cc::core::UndoStack* undo);
    void addViewport(Viewport3D* viewport);
    Mode mode() const { return m_mode; }
    void deactivate();
    /// Call after every change of the history (push, undo, redo).
    void structureChanged();

    /// Widget the dialogs are parented to (the main window).
    void setDialogParent(QWidget* parent) { m_dialogParent = parent; }
    /// Runs a modal dialog; returns QDialog::Accepted / Rejected. The default is exec(); tests replace it so that nothing blocks.
    void setDialogRunner(std::function<int(QDialog&)> runner) { m_runner = std::move(runner); }

    /// Opens "Describir" for the bars (the bars of the same description share an entry of "Perfiles utilizados"). Returns true
    /// when the dialog was accepted and the bars were described.
    bool describeBars(const std::vector<int>& barIds);
    /// Opens "Describir material" for the bars. Returns true when accepted. PROVISIONAL: the core has no grade per bar yet, so
    /// nothing is stored (ticket candidate).
    bool describeMaterial(const std::vector<int>& barIds);

    /// Status bar text of the commands (measured: identical but for the name).
    static QString statusText(Mode mode);

signals:
    void statusChanged(const QString& text);
    /// A command is about to become active: other viewport tools must stop.
    void aboutToActivate();
    void modeChanged(cc::app::BarDescribeTool::Mode mode);
    void messageRequested(const QString& text);

private:
    void setMode(Mode mode);
    void configureViewports();
    void updateEnabled();
    void pruneViewports();
    void onActOnSelection(Viewport3D* viewport);
    int run(QDialog& dialog);

    QAction* m_section = nullptr;
    QAction* m_material = nullptr;
    QActionGroup* m_actions = nullptr;
    cc::core::Project* m_project = nullptr;
    cc::core::UndoStack* m_undo = nullptr;
    QList<QPointer<Viewport3D>> m_viewports;
    QPointer<QWidget> m_dialogParent;
    Mode m_mode = Mode::None;
    std::function<int(QDialog&)> m_runner;
};

}  // namespace cc::app

Q_DECLARE_METATYPE(cc::app::BarDescribeTool::Mode)
