#pragma once
#include <QList>
#include <QObject>
#include <QPointer>
#include <QString>
#include <functional>
#include <optional>
#include <vector>

#include "core/continuous_edit.h"
#include "core/project.h"
#include "core/selection.h"
#include "core/undo_stack.h"

class QAction;
class QActionGroup;
class QDialog;
class QMenu;
class QWidget;

namespace cc::app {

class Viewport3D;

/// Geometria > Barras: the two drop-down buttons "Edición de pilares" and "Edición de vigas" (T-P5-08, GUI part). The edits
/// are the undoable commands of core/continuous_edit.h; behaviour sources: docs/research/ui/edicion_pilares_vigas.md and
/// barras.md. Menus (the beam one measured in docs/screenshots/t08_vigas_menu.png, the pillar one deduced from the strings):
///   pillars: Crear pilar continuo | Editar canto del forjado | Editar referencia | Numerar
///   beams:   Crear viga continua | Editar nudos | Editar forjados adyacentes | - | Editar referencias de vigas |
///            Editar referencias de vigas continuas | - | Flecha límite de vigas | Editar ambiente de vigas
/// Every entry except "Numerar" is a mode of the viewport (one at a time, like the other Geometria commands; Esc or another
/// command stops it). LMB selects (Ctrl adds, Shift removes, window), RMB acts on the selection. "Crear pilar continuo" takes
/// two node clicks (#64854). "Numerar" opens its dialog at once. "Editar nudos", "Editar forjados adyacentes" and "Editar
/// ambiente de vigas" are enabled but only emit notAvailable() (ticket TK-078: no core yet).
class ContinuousTool : public QObject {
    Q_OBJECT
public:
    enum class Mode {
        None,
        CreateColumn,         ///< two nodes: start of the first piece, end of the last
        SlabDepth,            ///< nodes of pillars + RMB
        ColumnReference,      ///< bars of pillars + RMB
        CreateBeam,           ///< bars + RMB
        BeamReferences,       ///< bars of lone beams + RMB
        ContinuousBeamReferences,  ///< bars of continuous beams + RMB
        DeflectionLimit       ///< bars of beams + RMB
    };

    explicit ContinuousTool(QObject* parent = nullptr);

    QMenu* pillarsMenu() const { return m_pillarsMenu; }
    QMenu* beamsMenu() const { return m_beamsMenu; }
    QAction* createColumnAction() const { return m_createColumn; }
    QAction* slabDepthAction() const { return m_slabDepth; }
    QAction* columnReferenceAction() const { return m_columnReference; }
    QAction* numberAction() const { return m_number; }
    QAction* createBeamAction() const { return m_createBeam; }
    QAction* editNodesAction() const { return m_editNodes; }
    QAction* adjacentSlabsAction() const { return m_adjacentSlabs; }
    QAction* beamReferencesAction() const { return m_beamReferences; }
    QAction* continuousBeamReferencesAction() const { return m_continuousBeamReferences; }
    QAction* deflectionLimitAction() const { return m_deflectionLimit; }
    QAction* beamEnvironmentAction() const { return m_beamEnvironment; }

    void setProject(cc::core::Project* project, cc::core::UndoStack* undo);
    void addViewport(Viewport3D* viewport);

    Mode mode() const { return m_mode; }
    /// "Crear pilar continuo": node chosen so far (0 or 1).
    const std::vector<int>& chain() const { return m_chain; }
    void deactivate();
    /// Call after every change of the history (push, undo, redo): refreshes the enabled state of the two buttons.
    void structureChanged();

    /// Widget the dialogs are parented to (the main window).
    void setDialogParent(QWidget* parent) { m_dialogParent = parent; }
    /// Runs a modal dialog; returns QDialog::Accepted / Rejected. The default is exec(); tests replace it so that nothing blocks.
    void setDialogRunner(std::function<int(QDialog&)> runner) { m_runner = std::move(runner); }

    /// Status bar text of a mode. `selected` = elements selected so far. PROVISIONAL (assembled from the generic texts of
    /// entopane); the two "Crear" modes use the help sentences of the original (#64854 and #64863).
    static QString statusText(Mode mode, int step = 0, int selected = 0);

    /// Which kind of element a reference command accepts.
    enum class Scope { Any, LoneOnly, ContinuousOnly };
    /// Target of a bar for the reference commands: the continuous column / beam its piece belongs to or the lone piece of
    /// type `wanted` (Pillar / Beam). nullopt when the bar is not part of such an element (or the scope excludes it).
    static std::optional<cc::core::ReferenceTarget> targetOfBar(const cc::core::Project& project, int barId, cc::core::ElementType wanted, Scope scope);

    // Direct entry points (what the modes call once the selection is made); each is one undo step. Return true on success.
    bool createColumn(int startNode, int endNode);
    bool createBeam(const std::vector<int>& barIds);
    bool editSlabDepth(const std::vector<int>& nodeIds);
    bool editColumnReferences(const std::vector<int>& barIds);
    bool editBeamReferences(const std::vector<int>& barIds, bool continuous);  // false: lone beams (#64878)
    bool editDeflectionLimit(const std::vector<int>& barIds);
    bool numberColumns();

signals:
    void statusChanged(const QString& text);
    /// A command is about to become active: other viewport tools must stop.
    void aboutToActivate();
    void modeChanged(cc::app::ContinuousTool::Mode mode);
    void messageRequested(const QString& text);
    /// The entry has no core yet (ticket): the main window says so in the status bar.
    void notAvailable();

private:
    QAction* addEntry(QMenu* menu, const QString& text, const QString& tip, const QString& tooltip);
    void startMode(Mode mode);
    void setMode(Mode mode);
    void configureViewports();
    void updateEnabled();
    void pruneViewports();
    void onSelectionChanged(Viewport3D* viewport);
    void onActOnSelection(Viewport3D* viewport);
    void resetSteps();
    cc::core::ElementKind pickKind() const;
    int run(QDialog& dialog);
    /// Checks every item (`check`: a command built and dropped) and, when all are accepted, applies them (`apply`: pushes)
    /// inside one undo step; on a refusal the message is shown and nothing changes.
    template <class Item>
    bool applyAll(const QString& macroName, const std::vector<Item>& items, const std::function<cc::core::ContinuousError(const Item&)>& check,
                  const std::function<cc::core::ContinuousError(const Item&)>& apply);
    void report(cc::core::ContinuousError error, const std::string& detail = {});

    QMenu* m_pillarsMenu = nullptr;
    QMenu* m_beamsMenu = nullptr;
    QAction* m_createColumn = nullptr;
    QAction* m_slabDepth = nullptr;
    QAction* m_columnReference = nullptr;
    QAction* m_number = nullptr;
    QAction* m_createBeam = nullptr;
    QAction* m_editNodes = nullptr;
    QAction* m_adjacentSlabs = nullptr;
    QAction* m_beamReferences = nullptr;
    QAction* m_continuousBeamReferences = nullptr;
    QAction* m_deflectionLimit = nullptr;
    QAction* m_beamEnvironment = nullptr;
    cc::core::Project* m_project = nullptr;
    cc::core::UndoStack* m_undo = nullptr;
    QList<QPointer<Viewport3D>> m_viewports;
    QPointer<QWidget> m_dialogParent;
    std::function<int(QDialog&)> m_runner;
    Mode m_mode = Mode::None;
    std::vector<int> m_chain;
    bool m_internalSelection = false;
};

}  // namespace cc::app

Q_DECLARE_METATYPE(cc::app::ContinuousTool::Mode)
