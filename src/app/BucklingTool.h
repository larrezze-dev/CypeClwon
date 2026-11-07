#pragma once
#include <QList>
#include <QObject>
#include <QPointer>
#include <QString>
#include <array>
#include <functional>
#include <vector>

#include "CypeDialog.h"
#include "core/buckling.h"
#include "core/project.h"
#include "core/undo_stack.h"

class QAction;
class QButtonGroup;
class QCheckBox;
class QLabel;
class QRadioButton;
class QToolButton;
class QWidget;

namespace cc::app {

class RangedEdit;
class Viewport3D;

/// Dialog "Pandeo" (pandeo.md sections 3 and 9.2 / 9.3): the picture of the profile with its local axes, two blocks «Asignar longitud de pandeo
/// (Plano xy / xz)» with 8 toggle icons each (default: the 4th, beta 1.0) and the field row of the chosen option, optionally (steel) the two
/// blocks «Asignar coeficiente de momentos (Plano ..)» with Cm, and the block «Asignar criterios para el análisis de pandeo». The fields of
/// the chosen option replace each other in a fixed row, so the dialog never changes size (measured).
/// The 8th option (cálculo aproximado) needs elements.dll (ticket): choosing it shows a notice and keeps the previous option, unless it was
/// loaded from the bar.
class BucklingDialog : public CypeDialog {
    Q_OBJECT
public:
    /// `steel`: some selected bar is steel -> the blocks of the equivalent moment coefficient Cm are shown (aluminium: no block, measured).
    explicit BucklingDialog(bool steel, QWidget* parent = nullptr);

    /// Initial state from the data of a bar (the dialog opens with the first selected bar, measured).
    void setBar(const cc::core::BarBuckling& record);
    /// What the dialog says now (the focused field commits first; values are rounded to the 3 decimals shown).
    cc::core::BucklingEdit edit() const;
    bool steel() const { return m_steel; }

    QCheckBox* assignCheck(int plane) const { return m_assign[static_cast<std::size_t>(plane)]; }
    QToolButton* kindButton(int plane, cc::core::BucklingKind kind) const;
    cc::core::BucklingKind kind(int plane) const;
    /// Chooses an option as a click on its icon would (the 8th shows the notice instead).
    void chooseKind(int plane, cc::core::BucklingKind kind);
    RangedEdit* betaEdit(int plane) const { return m_beta[static_cast<std::size_t>(plane)]; }
    RangedEdit* lengthEdit(int plane) const { return m_length[static_cast<std::size_t>(plane)]; }
    QRadioButton* translationalRadio(int plane) const { return m_translational[static_cast<std::size_t>(plane)]; }
    QRadioButton* intraslationalRadio(int plane) const { return m_intraslational[static_cast<std::size_t>(plane)]; }
    QCheckBox* assignMomentCheck(int plane) const { return m_assignMoment[static_cast<std::size_t>(plane)]; }
    RangedEdit* momentEdit(int plane) const { return m_moment[static_cast<std::size_t>(plane)]; }
    QCheckBox* criteriaCheck() const { return m_criteria; }
    QCheckBox* considerCheck() const { return m_consider; }
    QCheckBox* updateCheck(int plane) const { return m_update[static_cast<std::size_t>(plane)]; }
    QWidget* fieldRow(int plane) const { return m_fieldRow[static_cast<std::size_t>(plane)]; }

    void accept() override;

private:
    void buildPlane(QWidget* parent, class QVBoxLayout* layout, int plane);
    void showFields(int plane);
    bool commitAll();

    bool m_steel = false;
    std::array<QCheckBox*, 2> m_assign{};
    std::array<QButtonGroup*, 2> m_group{};
    std::array<std::array<QToolButton*, 8>, 2> m_buttons{};
    std::array<QWidget*, 2> m_fieldRow{};
    std::array<RangedEdit*, 2> m_beta{}, m_length{}, m_moment{};
    std::array<QRadioButton*, 2> m_translational{}, m_intraslational{};
    std::array<QWidget*, 2> m_betaBox{}, m_lengthBox{}, m_computedBox{};
    std::array<QCheckBox*, 2> m_assignMoment{}, m_update{};
    QCheckBox* m_criteria = nullptr;
    QCheckBox* m_consider = nullptr;
    std::array<double, 2> m_computedBeta{-1.0, -1.0};  ///< beta of a "Computed" plane loaded from a bar; < 0: none (the option is not available)
    std::array<cc::core::BucklingKind, 2> m_previous{cc::core::BucklingKind::PinnedPinned, cc::core::BucklingKind::PinnedPinned};
};

/// Dialog "Pandeo lateral" (pandeo.md section 9.4): the picture of the two profiles (S / I), the blocks «Ala superior (S)» and «Ala inferior (I)» with
/// 6 toggle icons each and the fields of the chosen option (coefficient or spacing, then Cm), and the block «Asignar factor de momento crítico» with
/// C1. No help icon in the title bar (measured).
class LateralBucklingDialog : public CypeDialog {
    Q_OBJECT
public:
    explicit LateralBucklingDialog(QWidget* parent = nullptr);

    void setBar(const cc::core::BarBuckling& record);
    cc::core::LateralEdit edit() const;

    QCheckBox* assignCheck(int flange) const { return m_assign[static_cast<std::size_t>(flange)]; }
    QToolButton* kindButton(int flange, cc::core::LateralKind kind) const;
    cc::core::LateralKind kind(int flange) const;
    void chooseKind(int flange, cc::core::LateralKind kind);
    RangedEdit* coeffEdit(int flange) const { return m_coeff[static_cast<std::size_t>(flange)]; }
    RangedEdit* spacingEdit(int flange) const { return m_spacing[static_cast<std::size_t>(flange)]; }
    RangedEdit* momentEdit(int flange) const { return m_moment[static_cast<std::size_t>(flange)]; }
    QLabel* firstFieldLabel(int flange) const { return m_firstLabel[static_cast<std::size_t>(flange)]; }
    QLabel* unitLabel(int flange) const { return m_unit[static_cast<std::size_t>(flange)]; }
    QCheckBox* assignC1Check() const { return m_assignC1; }
    RangedEdit* c1Edit() const { return m_c1; }

    void accept() override;

private:
    void buildFlange(QWidget* parent, class QVBoxLayout* layout, int flange);
    void showFields(int flange);
    bool commitAll();

    std::array<QCheckBox*, 2> m_assign{};
    std::array<QButtonGroup*, 2> m_group{};
    std::array<std::array<QToolButton*, 6>, 2> m_buttons{};
    std::array<RangedEdit*, 2> m_coeff{}, m_spacing{}, m_moment{};
    std::array<QLabel*, 2> m_firstLabel{}, m_unit{}, m_momentLabel{};
    std::array<QWidget*, 2> m_coeffBox{}, m_momentBox{};
    QCheckBox* m_assignC1 = nullptr;
    RangedEdit* m_c1 = nullptr;
};

/// Propiedades > Barras > Pandeo and Pandeo lateral (T-P6-04, GUI part; data and undoable commands in core/buckling*.h): window selection of
/// bars + RMB opens the dialog; Aceptar assigns as one undoable step. While the tool is active the bars carry the green labels, the local axes
/// at one end and (Pandeo) the extra lines of the information panel.
class BucklingTool : public QObject {
    Q_OBJECT
public:
    enum class Mode { None, Beta, Lateral };

    explicit BucklingTool(QObject* parent = nullptr);

    QAction* betaAction() const { return m_beta; }
    QAction* lateralAction() const { return m_lateral; }
    void setProject(cc::core::Project* project, cc::core::UndoStack* undo);
    void addViewport(Viewport3D* viewport);
    Mode mode() const { return m_mode; }
    void deactivate();
    /// Call after every change of the history (push, undo, redo).
    void structureChanged();
    void setDialogParent(QWidget* parent) { m_dialogParent = parent; }
    /// Runs a modal dialog; the default is exec(); tests replace it so that nothing blocks.
    void setDialogRunner(std::function<int(QDialog&)> runner) { m_runner = std::move(runner); }

    /// "Barras - Pandeo |  - LMB: Seleccione un grupo de elementos. - RMB: Actuar sobre los elementos seleccionados." (measured).
    static QString statusText(Mode mode);

    /// Opens the dialog for the bars (initial state: the first existing bar) and applies it when accepted. True when something changed.
    bool editBuckling(const std::vector<int>& barIds);
    bool editLateral(const std::vector<int>& barIds);

signals:
    void statusChanged(const QString& text);
    void aboutToActivate();
    void modeChanged(cc::app::BucklingTool::Mode mode);

private:
    void start(Mode mode);
    void stop();
    void configureViewports();
    void updateEnabled();
    void onActOnSelection(Viewport3D* viewport);
    void pruneViewports();
    int run(QDialog& dialog);
    bool anySteel(const std::vector<int>& barIds) const;

    QAction* m_beta = nullptr;
    QAction* m_lateral = nullptr;
    cc::core::Project* m_project = nullptr;
    cc::core::UndoStack* m_undo = nullptr;
    QList<QPointer<Viewport3D>> m_viewports;
    Mode m_mode = Mode::None;
    QPointer<QWidget> m_dialogParent;
    std::function<int(QDialog&)> m_runner;
};

}  // namespace cc::app

Q_DECLARE_METATYPE(cc::app::BucklingTool::Mode)
