#pragma once
#include <QGroupBox>
#include <QPointF>
#include <QWidget>
#include <array>
#include <functional>
#include <vector>

#include "CypeDialog.h"
#include "RangedEdit.h"
#include "core/project.h"
#include "core/shell_edit.h"
#include "core/units.h"

class QCheckBox;
class QComboBox;
class QRadioButton;
class QStackedWidget;
class QButtonGroup;
class QToolButton;

namespace cc::app {

/// Panel "Sección de la lámina" (#65122) of the dialogs "Nueva" and "Describir sección". Measured in the original (laminas.md 7.1):
/// "Espesor" 100.00 mm (0.10 - 10000.00; always in mm, whatever the unit of the work), casilla "Módulo de balasto en Z local" (off) with the editor 29430.00 kN/m3
/// (0.01 - 1000000.00) and, once the box is checked, the row "Comportamiento" as a combo: "Lineal" / "Sólo compresión".
class ShellSectionPanel : public QGroupBox {
    Q_OBJECT
public:
    ShellSectionPanel(const cc::core::ShellSection& initial, cc::core::LengthUnit unit, QWidget* parent = nullptr);
    /// The values entered (thickness converted to metres). Call validate() first when the fields may hold an uncommitted text.
    cc::core::ShellSection section() const;
    /// Commits the editors; false (after the warning of the original) when a value is out of range.
    bool validate();
    void setThickness(double valueInUnit);
    void setBallastEnabled(bool on);
    void setBallastValue(double knM3);
    void setBehaviour(cc::core::BallastBehaviour behaviour);
    bool ballastEditable() const;
    bool behaviourEditable() const;
    RangedEdit* thicknessEdit() const { return m_thickness; }
    RangedEdit* ballastEdit() const { return m_ballastValue; }
    QComboBox* behaviourCombo() const { return m_behaviour; }

private:
    cc::core::LengthUnit m_unit;
    RangedEdit* m_thickness = nullptr;
    QCheckBox* m_ballast = nullptr;
    RangedEdit* m_ballastValue = nullptr;
    QWidget* m_behaviourRow = nullptr;
    QComboBox* m_behaviour = nullptr;
};

/// Runs a modal dialog and answers whether it was accepted; tests replace the runner through CltAssistantDialog::setRunner.
bool runDialog(QDialog& dialog);

/// Assistant for the input of the properties of CLT panels (`dlg_asist_clt`, laminas.md section 8.3): E along / across the fibres of the outer
/// boards, shear modulus (xy = xz, and yz) and density, with the defaults of the original (11000, 360, 690, 50 N/mm2; 450 kg/m3).
class CltAssistantDialog : public CypeDialog {
    Q_OBJECT
public:
    explicit CltAssistantDialog(QWidget* parent = nullptr);
    cc::core::CltInput input() const;
    const std::vector<RangedEdit*>& edits() const { return m_edits; }
    static void setRunner(std::function<bool(QDialog&)> runner);

private:
    void accept() override;
    std::vector<RangedEdit*> m_edits;
};

/// Panel "Material" (#261366) of the dialogs "Nueva" and "Describir material": the six types on the left (Hormigón, Acero laminado,
/// Acero conformado, Aluminio extruido, Genérico isótropo, Genérico ortótropo) and the detail of the chosen one on the right, with the
/// defaults, ranges and warnings measured in the original (laminas.md 7.1). The blue arrow next to the orthotropic Ex opens the CLT
/// assistant (laminas.md 8.3).
class ShellMaterialPanel : public QGroupBox {
    Q_OBJECT
public:
    ShellMaterialPanel(const cc::core::ShellMaterial& initial, const cc::core::Project& project, QWidget* parent = nullptr);
    cc::core::ShellMaterial material() const;
    bool validate();
    void setKind(cc::core::ShellMaterialKind kind);
    cc::core::ShellMaterialKind kind() const;
    /// Widgets of the page of the chosen kind: the "Material seleccionado en datos generales" / "Seleccionar el material de la lámina"
    /// radios and the combo of the grades (concrete, rolled steel), or the alloy combo (aluminium).
    QRadioButton* generalDataRadio() const { return m_gradePages.at(pageIndex()).general; }
    QRadioButton* ownRadio() const { return m_gradePages.at(pageIndex()).own; }
    QComboBox* gradeCombo() const { return m_gradePages.at(pageIndex()).combo; }
    QComboBox* alloyCombo() const { return m_gradePages[3].combo; }
    QComboBox* temperCombo() const { return m_gradePages[3].combo2; }
    /// Editors of the generic kinds, in the order of the dialog.
    const std::vector<RangedEdit*>& isotropicEdits() const { return m_iso; }
    const std::vector<RangedEdit*>& orthotropicEdits() const { return m_ortho; }
    const std::vector<QRadioButton*>& coldFormedRadios() const { return m_gradePages[2].radios; }
    QRadioButton* kindRadio(cc::core::ShellMaterialKind kind) const { return m_kindRadios.at(static_cast<std::size_t>(kind)); }
    QToolButton* cltButton() const { return m_cltButton; }
    /// Opens the CLT assistant and, once accepted, writes its result into the orthotropic editors.
    void runCltAssistant();

private:
    std::size_t pageIndex() const;
    void refreshEnabled();

    const cc::core::Project& m_project;
    std::array<QRadioButton*, 6> m_kindRadios{};
    QStackedWidget* m_pages = nullptr;
    struct GradePage {
        QRadioButton* general = nullptr;
        QRadioButton* own = nullptr;
        QComboBox* combo = nullptr;
        QComboBox* combo2 = nullptr;
        std::vector<QRadioButton*> radios;
    };
    std::array<GradePage, 4> m_gradePages{};  ///< concrete, rolled steel, cold formed, aluminium
    std::vector<RangedEdit*> m_iso, m_ortho;
    QToolButton* m_cltButton = nullptr;
};

/// "Nueva" (title "Nueva"): the two panels stacked, Sección de la lámina over Material (laminas.md 5 and 7.1).
class ShellNewDialog : public CypeDialog {
    Q_OBJECT
public:
    ShellNewDialog(const cc::core::ShellSection& section, const cc::core::ShellMaterial& material, const cc::core::Project& project, QWidget* parent = nullptr);
    cc::core::ShellSection section() const { return m_section->section(); }
    cc::core::ShellMaterial material() const { return m_material->material(); }
    ShellSectionPanel* sectionPanel() const { return m_section; }
    ShellMaterialPanel* materialPanel() const { return m_material; }

private:
    void accept() override;
    ShellSectionPanel* m_section = nullptr;
    ShellMaterialPanel* m_material = nullptr;
};

/// "Describir sección": the section panel alone (Sección command).
class ShellSectionDialog : public CypeDialog {
    Q_OBJECT
public:
    ShellSectionDialog(const cc::core::ShellSection& initial, cc::core::LengthUnit unit, QWidget* parent = nullptr);
    cc::core::ShellSection section() const { return m_panel->section(); }
    ShellSectionPanel* panel() const { return m_panel; }
    void setThickness(double valueInUnit) { m_panel->setThickness(valueInUnit); }
    void setBallastEnabled(bool on) { m_panel->setBallastEnabled(on); }
    void setBallastValue(double knM3) { m_panel->setBallastValue(knM3); }
    void setBehaviour(cc::core::BallastBehaviour behaviour) { m_panel->setBehaviour(behaviour); }
    bool ballastEditable() const { return m_panel->ballastEditable(); }
    bool behaviourEditable() const { return m_panel->behaviourEditable(); }

private:
    void accept() override;
    ShellSectionPanel* m_panel = nullptr;
};

/// "Describir material": the material panel alone (Material command).
class ShellMaterialDialog : public CypeDialog {
    Q_OBJECT
public:
    ShellMaterialDialog(const cc::core::ShellMaterial& initial, const cc::core::Project& project, QWidget* parent = nullptr);
    cc::core::ShellMaterial material() const { return m_panel->material(); }
    ShellMaterialPanel* panel() const { return m_panel; }

private:
    void accept() override;
    ShellMaterialPanel* m_panel = nullptr;
};

/// Cross-section viewer of "Describir disposición": the grey section of the plate, the local Z axis (blue) and Y axis (green), and three
/// dash-dot lines (top face, central plane, bottom face) each with a purple cross. Zoom / pan like the viewers of the original.
class ShellSectionViewer : public QWidget {
    Q_OBJECT
public:
    enum class Tool { None, ZoomWindow, Pan };
    explicit ShellSectionViewer(QWidget* parent = nullptr);
    /// Displacement of the central plane in millimetres (the viewer draws the plate shifted accordingly).
    void setOffsetMm(double mm);
    void setThicknessMm(double mm);
    void previousView();
    void fullView();
    void doubleView();
    void redraw() { update(); }
    void setTool(Tool tool);
    Tool tool() const { return m_tool; }
    /// Visible half-height in millimetres (tests: the zoom changes it).
    double viewHeightMm() const { return m_viewHeightMm; }
    QPointF viewCentreMm() const { return m_centre; }
    /// Positions of the three crosses in widget coordinates: top face, central plane, bottom face.
    std::array<QPointF, 3> crossPositions() const;

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    QPointF toScreen(double yMm, double zMm) const;
    QPointF toModel(const QPointF& screen) const;
    void pushView();

    double m_offsetMm = 0.0;
    double m_thicknessMm = 100.0;
    double m_viewHeightMm = 0.0;  ///< total visible height; 0 = fit
    QPointF m_centre{0.0, 0.0};
    std::vector<std::pair<double, QPointF>> m_history;
    Tool m_tool = Tool::None;
    QPointF m_pressed;
    QPointF m_current;
    bool m_dragging = false;
};

/// "Describir disposición" (laminas.md 7.2): group "Cálculo" (checkbox "Considerar las excentricidades en el cálculo", on by default),
/// group "Posición" (the 7 icons of the viewer and the section viewer) and group "Desplazamiento del plano central" with the integer
/// editor "Z" in mm (-5000 to 5000). Pressing the crosses of the viewer did not change Z in the original (not reproduced).
class ShellLayoutDialog : public CypeDialog {
    Q_OBJECT
public:
    ShellLayoutDialog(const cc::core::ShellLayout& initial, double thicknessM, QWidget* parent = nullptr);
    cc::core::ShellLayout layout() const;
    QCheckBox* eccentricityBox() const { return m_eccentricity; }
    RangedEdit* offsetEdit() const { return m_offset; }
    ShellSectionViewer* viewer() const { return m_viewer; }
    const std::vector<QToolButton*>& viewerButtons() const { return m_buttons; }

private:
    void accept() override;
    void printViewer();
    QCheckBox* m_eccentricity = nullptr;
    RangedEdit* m_offset = nullptr;
    ShellSectionViewer* m_viewer = nullptr;
    std::vector<QToolButton*> m_buttons;
};

}  // namespace cc::app
