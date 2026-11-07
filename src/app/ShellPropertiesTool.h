#pragma once
#include <QList>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QStringList>
#include <array>
#include <functional>
#include <optional>
#include <vector>

#include "core/project.h"
#include "core/shell_properties.h"
#include "core/undo_stack.h"

class QAction;
class QActionGroup;
class QDialog;
class QWidget;

namespace cc::app {

class Viewport3D;

/// Propiedades > Láminas (T-P6-07, GUI part; data and undoable commands in core/shell_properties*.h): the seven buttons of the group, in the order
/// of the original (docs/research/ui/propiedades_laminas.md section 0): Vinculación interior, Vinculación exterior, Bandas de integración,
/// Discretización, Factores de rigidez, Pandeo and Análisis modal. The command turns the 3D window into a selection surface (a shell is picked by
/// its outline; the two edge commands pick edges, LMB selects, window / crossing), RMB opens the dialog of the command on the selection and Aceptar
/// assigns as ONE undoable step. While a command is active the information box shows the "ficha" of the shell (or "Arista N4 / N1").
class ShellPropertiesTool : public QObject {
    Q_OBJECT
public:
    enum class Mode { None, Discretisation, Stiffness, Bands, InteriorEdge, ExteriorEdge, Buckling, Modal };

    explicit ShellPropertiesTool(QObject* parent = nullptr);

    QAction* discretisationAction() const { return m_actions[idx(Mode::Discretisation)]; }
    QAction* stiffnessAction() const { return m_actions[idx(Mode::Stiffness)]; }
    QAction* bandsAction() const { return m_actions[idx(Mode::Bands)]; }
    QAction* interiorAction() const { return m_actions[idx(Mode::InteriorEdge)]; }
    QAction* exteriorAction() const { return m_actions[idx(Mode::ExteriorEdge)]; }
    QAction* bucklingAction() const { return m_actions[idx(Mode::Buckling)]; }
    QAction* modalAction() const { return m_actions[idx(Mode::Modal)]; }

    void setProject(cc::core::Project* project, cc::core::UndoStack* undo);
    void addViewport(Viewport3D* viewport);
    Mode mode() const { return m_mode; }
    void deactivate();
    /// Starts a command as its ribbon button would.
    void activate(Mode mode);
    /// Call after every change of the history (push, undo, redo): enables / disables the buttons and refreshes the viewports.
    void structureChanged();
    void setDialogParent(QWidget* parent) { m_dialogParent = parent; }
    /// Runs a modal dialog; the default is exec(); tests replace it so that nothing blocks.
    void setDialogRunner(std::function<int(QDialog&)> runner) { m_runner = std::move(runner); }

    /// "Láminas - Discretización |  - LMB: Seleccione un grupo de elementos. - RMB: Actuar sobre los elementos seleccionados." (measured).
    static QString statusText(Mode mode);
    static QString commandName(Mode mode);

    /// Opens the dialog of the command on the shells / edges (initial state: the first existing one) and applies it when accepted. True when
    /// something changed.
    bool editDiscretisation(const std::vector<int>& shellIds);
    bool editStiffness(const std::vector<int>& shellIds);
    bool editBuckling(const std::vector<int>& shellIds);
    bool editModal(const std::vector<int>& shellIds);
    bool editBands(const std::vector<int>& shellIds);
    bool editInterior(const std::vector<cc::core::ShellEdgeKey>& edges);
    bool editExterior(const std::vector<cc::core::ShellEdgeKey>& edges);

    /// Rows of the information box ("ficha") of a shell, or of an edge in the edge commands. A leading '\x01' marks a compact row, '\x02' a grey header
    /// row (see Viewport3D::drawInfoBox).
    QStringList infoLines(int shellId, const std::optional<cc::core::ShellEdgeKey>& edge) const;
    /// "Arista N4 / N1": the nodes in contour order.
    QString edgeTitle(const cc::core::ShellEdgeKey& edge) const;

signals:
    void statusChanged(const QString& text);
    /// A command is about to become active: other viewport tools must stop.
    void aboutToActivate();
    void modeChanged(cc::app::ShellPropertiesTool::Mode mode);

private:
    static constexpr std::size_t idx(Mode mode) { return static_cast<std::size_t>(mode) - 1; }
    void setMode(Mode mode);
    void configureViewports();
    void updateEnabled();
    void onActOnSelection(Viewport3D* viewport);
    void pruneViewports();
    int run(QDialog& dialog);
    bool changedSince(std::size_t before) const { return m_undo && m_undo->count() != before; }

    std::array<QAction*, 7> m_actions{};
    QActionGroup* m_group = nullptr;
    cc::core::Project* m_project = nullptr;
    cc::core::UndoStack* m_undo = nullptr;
    QList<QPointer<Viewport3D>> m_viewports;
    Mode m_mode = Mode::None;
    QPointer<QWidget> m_dialogParent;
    std::function<int(QDialog&)> m_runner;
};

}  // namespace cc::app

Q_DECLARE_METATYPE(cc::app::ShellPropertiesTool::Mode)
