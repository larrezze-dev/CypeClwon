#pragma once
#include <functional>

#include "CypeDialog.h"
#include "core/work_planes.h"

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLineEdit;
class QRadioButton;
class QSpinBox;
class QTableWidget;
class QTableWidgetItem;

namespace cc::app {

/// Message boxes of the "Planos" dialogs, replaceable so that the tests never block on a modal box.
struct PlanesPrompts {
    /// Warning with only an "Aceptar" button (validation messages).
    std::function<void(QWidget*, const QString&)> warn;
    /// Question with "Sí" / "No" where "No" is the default button (dlgavis_preguntar_no_por_defecto_u); true for "Sí".
    std::function<bool(QWidget*, const QString&)> askNoDefault;
    /// The prompts that show real message boxes.
    static PlanesPrompts interactive();
};

/// "Rejilla" (678x621): two tables of reference lines, X and Y, plus the generator. Reference:
/// docs/research/ui/planos.md section 11, captures pl_f1.png and pl_g1.png.
class GridDialog : public CypeDialog {
    Q_OBJECT
public:
    explicit GridDialog(const cc::core::WorkPlanes& initial, QWidget* parent = nullptr);

    /// The planes with the grid (and the "Ajustar los elementos" flag) as currently edited; levels and groups unchanged.
    cc::core::WorkPlanes value() const { return m_planes; }
    PlanesPrompts prompts = PlanesPrompts::interactive();

    /// Replaces both axes by the generated lines. When lines exist, `confirm` is asked first (message 65026); false from it,
    /// or an invalid specification (a message is shown), leaves the grid untouched and returns false.
    bool applyGeneration(const cc::core::GridSpec& spec, const std::function<bool()>& confirm);
    /// Opens the generator ("Generación de líneas de rejilla") and applies it.
    void runGenerator();
    /// Adds a line at the end of an axis (0: X, 1: Y).
    void addLine(int axis, const cc::core::GridLine& line);
    /// Duplicates / deletes the line at `row` of the axis (the selected one in the dialog).
    void duplicateLine(int axis, int row);
    void deleteLine(int axis, int row);
    const cc::core::GridSpec& lastSpec() const { return m_spec; }

    void accept() override;

private:
    std::vector<cc::core::GridLine>& linesOf(int axis) { return axis == 0 ? m_planes.gridX : m_planes.gridY; }
    void rebuild(int axis);
    void cellEdited(int axis, QTableWidgetItem* item);
    void askNewLine(int axis);

    cc::core::WorkPlanes m_planes;
    cc::core::GridSpec m_spec;  ///< kept between openings of the generator, like the original
    QTableWidget* m_tables[2] = {nullptr, nullptr};
    QCheckBox* m_adjust = nullptr;
    bool m_rebuilding = false;
};

/// "Generación de líneas de rejilla" (487x227): number, separation, first label and label position for X and Y.
class GridGenerationDialog : public CypeDialog {
    Q_OBJECT
public:
    explicit GridGenerationDialog(const cc::core::GridSpec& initial, QWidget* parent = nullptr);
    cc::core::GridSpec value() const;

private:
    struct AxisControls {
        QSpinBox* count = nullptr;
        QDoubleSpinBox* spacing = nullptr;
        QLineEdit* label = nullptr;
        QComboBox* position = nullptr;
    };
    QWidget* buildAxis(const QString& title, const cc::core::GridAxisSpec& spec, AxisControls& controls);
    AxisControls m_x, m_y;
};

/// "Nueva línea": coordinate, label and label position of one reference line.
class NewGridLineDialog : public CypeDialog {
    Q_OBJECT
public:
    explicit NewGridLineDialog(QWidget* parent = nullptr);
    cc::core::GridLine value() const;

private:
    QDoubleSpinBox* m_coordinate = nullptr;
    QLineEdit* m_label = nullptr;
    QComboBox* m_position = nullptr;
};

/// "Niveles" (280x420): list of horizontal planes at absolute elevations. Reference: planos.md section 11, pl_n1..n4.
class LevelsDialog : public CypeDialog {
    Q_OBJECT
public:
    explicit LevelsDialog(const cc::core::WorkPlanes& initial, QWidget* parent = nullptr);

    /// The planes with the levels as edited (sorted by elevation); grid and groups unchanged.
    cc::core::WorkPlanes value() const { return m_planes; }
    PlanesPrompts prompts = PlanesPrompts::interactive();

    void addLevel(const cc::core::Level& level);
    void deleteLevel(int row);
    /// Replaces the levels by "<prefix> <i>" ...; when levels exist `confirm` is asked first (message 65036).
    bool applyGeneration(const std::string& prefix, int count, double start, double spacing, const std::function<bool()>& confirm);
    void runGenerator();

    void accept() override;

private:
    void rebuild();
    void cellEdited(QTableWidgetItem* item);

    cc::core::WorkPlanes m_planes;
    QTableWidget* m_table = nullptr;
    bool m_rebuilding = false;
};

/// "Nuevo nivel" (~220x155): reference and absolute elevation.
class NewLevelDialog : public CypeDialog {
    Q_OBJECT
public:
    explicit NewLevelDialog(QWidget* parent = nullptr);
    cc::core::Level value() const;

private:
    QLineEdit* m_name = nullptr;
    QDoubleSpinBox* m_elevation = nullptr;
};

/// "Generar niveles" (~385x180): prefix, number of levels, start elevation and separation.
class GenerateLevelsDialog : public CypeDialog {
    Q_OBJECT
public:
    explicit GenerateLevelsDialog(QWidget* parent = nullptr);
    std::string prefix() const;
    int count() const;
    double start() const;
    double spacing() const;

private:
    QLineEdit* m_prefix = nullptr;
    QSpinBox* m_count = nullptr;
    QDoubleSpinBox* m_start = nullptr;
    QDoubleSpinBox* m_spacing = nullptr;
};

/// "Mostrar/ocultar planos" (~243x135): radios Mostrar / Ocultar, "Ocultar" by default.
class PlanesVisibilityDialog : public CypeDialog {
    Q_OBJECT
public:
    explicit PlanesVisibilityDialog(QWidget* parent = nullptr);
    bool showPlanes() const;
    void setShowPlanes(bool show);

private:
    QRadioButton* m_show = nullptr;
    QRadioButton* m_hide = nullptr;
};

}  // namespace cc::app
