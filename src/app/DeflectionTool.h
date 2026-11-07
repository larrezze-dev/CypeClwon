#pragma once
#include <QList>
#include <QObject>
#include <QPointer>
#include <QString>
#include <array>
#include <functional>
#include <optional>
#include <vector>

#include "CypeDialog.h"
#include "core/deflection_group.h"
#include "core/deflection_group_edit.h"
#include "core/project.h"
#include "core/undo_stack.h"

class QAction;
class QButtonGroup;
class QCheckBox;
class QLabel;
class QMenu;
class QRadioButton;

namespace cc::app {

class RangedEdit;
class Viewport3D;

/// Dialog "Grupo de flecha" (grupos_flecha.md section 5.1, gf_09_dialog / gf_22_edit_dlg; shared by Crear and Editar, about 206 x 270): two grey
/// headers «Plano xy» / «Plano xz», each over three radios «Secante» (default), «Tangente a nudo 1» and «Tangente a nudo 2»; the chosen one is
/// bold. Aceptar / Cancelar.
class DeflectionTypeDialog : public CypeDialog {
    Q_OBJECT
public:
    explicit DeflectionTypeDialog(QWidget* parent = nullptr);

    /// Plane 0 = xy, 1 = xz.
    cc::core::DeflectionType type(int plane) const;
    void setType(int plane, cc::core::DeflectionType type);
    QRadioButton* radio(int plane, cc::core::DeflectionType type) const;

private:
    std::array<std::array<QRadioButton*, 3>, 2> m_radios{};
    std::array<QButtonGroup*, 2> m_group{};
};

/// Dialog "Flecha Límite" (grupos_flecha.md section 5.2, gf_30..gf_32, about 390 x 545): the picture of the profile with its axes and the eight blocks
/// (LimitSlot order: maximum / active, absolute / relative, xy / xz). A block is a check box; ticked, an absolute one shows a field in mm (1.0),
/// a relative one the radios L / 250 (default), L / 300, L / 400, L / 500 and L / ? (user divisor, field next to it: the original's field was not
/// seen, TODO(medir)). Unticked controls are hidden but keep their space, so the dialog never changes size.
class DeflectionGroupLimitDialog : public CypeDialog {
    Q_OBJECT
public:
    explicit DeflectionGroupLimitDialog(QWidget* parent = nullptr);

    void setLimits(const cc::core::DeflectionLimits& limits);
    /// What the dialog says now (a focused field commits first).
    cc::core::DeflectionLimits limits() const;

    QCheckBox* check(cc::core::LimitSlot slot) const { return m_check[index(slot)]; }
    /// Absolute blocks: the field in mm. Relative blocks: the field of the user divisor "L / ?".
    RangedEdit* edit(cc::core::LimitSlot slot) const { return m_edit[index(slot)]; }
    /// Relative blocks only; `choice` 0..3 = L / 250, 300, 400, 500, 4 = L / ?.
    QRadioButton* radio(cc::core::LimitSlot slot, int choice) const { return m_radios[index(slot)][static_cast<std::size_t>(choice)]; }
    static constexpr int kPresetCount = 4;
    static double presetDivisor(int choice);

    void accept() override;

private:
    static std::size_t index(cc::core::LimitSlot slot) { return static_cast<std::size_t>(slot); }
    void buildAbsolute(QWidget* parent, class QVBoxLayout* layout, cc::core::LimitSlot slot);
    void buildRelative(QWidget* parent, class QVBoxLayout* layout, cc::core::LimitSlot slot);
    bool commitAll();

    std::array<QCheckBox*, cc::core::kLimitSlotCount> m_check{};
    std::array<RangedEdit*, cc::core::kLimitSlotCount> m_edit{};
    std::array<std::array<QRadioButton*, 5>, cc::core::kLimitSlotCount> m_radios{};
    std::array<QButtonGroup*, cc::core::kLimitSlotCount> m_group{};
};

/// Propiedades > Barras > Grupos de flecha (T-P6-06, GUI part; data and undoable commands in core/deflection_group*.h): a drop-down with three
/// entries.
///  - Crear: LMB on two nodes (the first stays orange); the aligned chain between them gets the dialog «Grupo de flecha». #64227 / #64228 are shown
///    before the dialog.
///  - Editar / Flecha Límite: window selection of bars (the whole group paints orange with N1 / N2) + RMB opens the dialog; Aceptar assigns as one
///    undoable step.
/// While a command is active the bars carry the green group labels and the local axes.
class DeflectionTool : public QObject {
    Q_OBJECT
public:
    enum class Mode { None, Create, Edit, Limit };

    explicit DeflectionTool(QObject* parent = nullptr);

    /// The drop-down of the ribbon button ("Grupos de flecha"): a title row and the entries Crear, Editar and Flecha Límite.
    QMenu* menu() const { return m_menu; }
    QAction* createAction() const { return m_create; }
    QAction* editAction() const { return m_edit; }
    QAction* limitAction() const { return m_limit; }
    void setProject(cc::core::Project* project, cc::core::UndoStack* undo);
    void addViewport(Viewport3D* viewport);
    Mode mode() const { return m_mode; }
    void deactivate();
    /// Starts a command as its menu entry would.
    void activate(Mode mode) { start(mode); }
    /// Call after every change of the history (push, undo, redo).
    void structureChanged();
    void setDialogParent(QWidget* parent) { m_dialogParent = parent; }
    /// Runs a modal dialog; the default is exec(); tests replace it so that nothing blocks.
    void setDialogRunner(std::function<int(QDialog&)> runner) { m_runner = std::move(runner); }
    /// First node chosen in Crear (an orange square waits for the second one); empty when none.
    std::optional<int> pendingNode() const { return m_pending; }

    /// "Grupos de flecha - Crear | - LMB: Seleccione un elemento." and the two group commands (measured, grupos_flecha.md section 2).
    static QString statusText(Mode mode);

    /// Crear between two nodes: runs the checks (the aviso of #64227 / #64228 first), the dialog and the command. True when a group was created.
    bool createBetween(int node1, int node2);
    /// Editar on the groups of the bars: dialog (initial state: the first group) and command. True when something changed.
    bool editTypes(const std::vector<int>& barIds);
    /// Flecha Límite on the groups of the bars: dialog (initial state: the first group) and command. True when something changed.
    bool editLimits(const std::vector<int>& barIds);

signals:
    void statusChanged(const QString& text);
    void aboutToActivate();
    void modeChanged(cc::app::DeflectionTool::Mode mode);

private:
    void start(Mode mode);
    void stop();
    void configureViewports();
    void updateEnabled();
    void onActOnSelection(Viewport3D* viewport);
    void onSelectionChanged(Viewport3D* viewport);
    void pruneViewports();
    int run(QDialog& dialog);
    void clearPending();

    QMenu* m_menu = nullptr;
    QAction* m_create = nullptr;
    QAction* m_edit = nullptr;
    QAction* m_limit = nullptr;
    cc::core::Project* m_project = nullptr;
    cc::core::UndoStack* m_undo = nullptr;
    QList<QPointer<Viewport3D>> m_viewports;
    Mode m_mode = Mode::None;
    std::optional<int> m_pending;
    QPointer<QWidget> m_dialogParent;
    std::function<int(QDialog&)> m_runner;
};

}  // namespace cc::app

Q_DECLARE_METATYPE(cc::app::DeflectionTool::Mode)
