#pragma once
#include <QList>
#include <QString>
#include <QWidget>
#include <array>
#include <functional>
#include <vector>

#include "CypeDialog.h"
#include "core/node_support.h"

class QButtonGroup;
class QComboBox;
class QLabel;
class QRadioButton;
class QTableWidget;
class QTableWidgetItem;
class QToolButton;

namespace cc::app {

class RangedEdit;
class LinkSketch;

/// "Vinculación exterior" (T-P6-01). Window of 591x414 with a grid of 2x10 image buttons (the order of the original: orig_vinc_ext_*.png)
/// and, below it, the panel of the chosen type. Behaviour measured in the original: docs/research/ui/vinculaciones.md 5.1.
///  - Free: empty panel. Fixed: "Desplazamientos" Dx..Dz and "Giros" Gx..Gz with Fijo / Elástico. Pinned: only "Desplazamientos".
///  - Line / plane types: a row X Y Z with the director (or normal) vector, disabled for the predefined axes, and only the displacements that
///    the type blocks; the "empotradas" add "Giros".
///  - Generic: radios Independientes / Sobre una recta / Sobre un plano, Libre / Fijo / Elástico per DOF (also the rotations).
///  - Elástico shows "K" (integer 1..10000000, default 100000, kN/m or kN.m/rad) and the combo "Sentido" (Ambos, Positivo, Negativo) on that row.
/// Aceptar: a zero vector shows "La dirección introducida no es correcta."; an elastic K equal to the default asks #65010 (Sí / No).
class ExternalSupportDialog : public CypeDialog {
    Q_OBJECT
public:
    explicit ExternalSupportDialog(const cc::core::ExternalSupport& initial = {}, QWidget* parent = nullptr);

    /// What the dialog describes now.
    cc::core::ExternalSupport support() const;
    cc::core::ExternalSupportType selectedType() const { return m_type; }
    /// Presses the button of `type`: the panel starts from its defaults (pressing "Genérica" does not preload the previous state).
    void selectType(cc::core::ExternalSupportType type);

    /// The 20 buttons indexed by the value of the enumeration (the grid order is not the enumeration order).
    const std::array<QToolButton*, cc::core::kExternalSupportTypeCount>& typeButtons() const { return m_buttons; }
    /// Controls of DOF `dof` (0..5 = Dx, Dy, Dz, Gx, Gy, Gz).
    struct DofRow {
        QWidget* row = nullptr;
        QRadioButton* free = nullptr;
        QRadioButton* fixed = nullptr;
        QRadioButton* elastic = nullptr;
        RangedEdit* k = nullptr;
        QLabel* unit = nullptr;
        QComboBox* sense = nullptr;
    };
    const DofRow& dof(int index) const { return m_rows[static_cast<std::size_t>(index)]; }
    /// True when the panel shows the row of that DOF.
    bool dofVisible(int index) const;
    bool vectorVisible() const;
    bool vectorEditable() const;
    RangedEdit* vectorEdit(int axis) const { return m_vector[static_cast<std::size_t>(axis)]; }
    bool rotationsVisible() const;
    QRadioButton* modeRadio(cc::core::GenericDisplacementMode mode) const { return m_modeRadios[static_cast<int>(mode)]; }

    /// Tooltip of a button: title (bold first line) and description, the literal texts of the original.
    static QString tooltipTitle(cc::core::ExternalSupportType type);
    static QString tooltipText(cc::core::ExternalSupportType type);

    /// Question window (#65010): returns true for "Sí". The default is a QMessageBox titled "Pregunta" with Sí (default) / No; tests replace it.
    static void setQuestionHandler(std::function<bool(QWidget*, const QString&)> handler);
    static bool ask(QWidget* parent, const QString& text);

    void accept() override;

private:
    void buildGrid(QWidget* parent);
    void buildPanel(QWidget* parent);
    void applyDefaults(cc::core::ExternalSupportType type);
    void load(const cc::core::ExternalSupport& support);
    void refreshPanel();
    void refreshRow(int index, bool visible, bool allowFree);
    cc::core::ExternalSupport probe() const;
    bool genericSelected() const { return m_type == cc::core::ExternalSupportType::Generic; }
    cc::core::GenericDisplacementMode genericMode() const;
    cc::core::Point3 vector() const;
    void setVector(const cc::core::Point3& v);

    cc::core::ExternalSupportType m_type = cc::core::ExternalSupportType::Free;
    std::array<QToolButton*, cc::core::kExternalSupportTypeCount> m_buttons{};
    QWidget* m_panel = nullptr;
    QWidget* m_vectorRow = nullptr;
    std::array<RangedEdit*, 3> m_vector{};
    QWidget* m_modeBox = nullptr;
    std::array<QRadioButton*, 3> m_modeRadios{};
    QLabel* m_displacementsBand = nullptr;
    QLabel* m_rotationsBand = nullptr;
    QWidget* m_displacementsRows = nullptr;
    QWidget* m_rotationsRows = nullptr;
    std::array<DofRow, 6> m_rows{};
    std::array<bool, 6> m_visible{};
    bool m_loading = false;
};

/// "Vinculación interior" (T-P6-01). Window of 748x449 (503 with the band of the warning), four buttons: Nudo empotrado (default),
/// con coeficiente de empotramiento parcial, articulado, genérico (vinculaciones.md 5.4).
///  - Parcial: "Coef.Emp.Parc." (0.00, kN.m/rad, 0..10000000).
///  - Genérico: a sketch of the node on the left (pieces A, B, C...) and a table "Vínc | partial | Coef.Emp.Parc. | A | B | C" on the right,
///    one row per link; every piece has exactly one mark; pieces of the same row are fixed to each other (a circle in the sketch) and the
///    partial box of a row paints its pieces red.
///  - A light blue band warns when the selected nodes have different numbers of pieces (#65473).
class InternalLinkDialog : public CypeDialog {
    Q_OBJECT
public:
    /// `directions`: unit vector of every piece leaving the node, in the order of the pieces (bars then shells); its size is the number
    /// of columns of the table. `mixedPieceCounts`: the selection mixes nodes with different numbers of pieces.
    InternalLinkDialog(const cc::core::InternalLink& initial, const std::vector<cc::core::Point3>& directions, bool mixedPieceCounts,
                       QWidget* parent = nullptr);

    cc::core::InternalLink link() const;
    cc::core::InternalLinkType selectedType() const { return m_type; }
    void selectType(cc::core::InternalLinkType type);

    const std::array<QToolButton*, 4>& typeButtons() const { return m_buttons; }
    RangedEdit* coefficientEdit() const { return m_coefficient; }
    QTableWidget* table() const { return m_table; }
    QLabel* warningBand() const { return m_warning; }
    LinkSketch* sketch() const { return m_sketch; }
    /// Marks piece `piece` in link row `row` (as clicking its cell does: it leaves the row where it was).
    void markPiece(int piece, int row);
    void setPartial(int row, bool on);
    void setRowCoefficient(int row, double value);

    static QString tooltipText(cc::core::InternalLinkType type);

    void accept() override;

private:
    void buildTable();
    void refreshPanel();
    void onItemChanged(QTableWidgetItem* item);
    void syncSketch();

    cc::core::InternalLinkType m_type = cc::core::InternalLinkType::Fixed;
    std::size_t m_pieces = 0;
    bool m_mixed = false;
    std::array<QToolButton*, 4> m_buttons{};
    QLabel* m_coefficientLabel = nullptr;
    QLabel* m_coefficientUnit = nullptr;
    RangedEdit* m_coefficient = nullptr;
    QLabel* m_warning = nullptr;
    QWidget* m_warningBox = nullptr;
    QWidget* m_generic = nullptr;
    LinkSketch* m_sketch = nullptr;
    QTableWidget* m_table = nullptr;
    std::vector<cc::core::Point3> m_directions;
    bool m_updating = false;
};

/// Sketch of the node of "Vinculación interior" genérica: a line per piece ending at the node, the label (A, B...) in magenta, a circle
/// when two or more pieces share a row, the pieces of a row with the partial box in red.
class LinkSketch : public QWidget {
    Q_OBJECT
public:
    explicit LinkSketch(QWidget* parent = nullptr);
    void setPieces(const std::vector<cc::core::Point3>& directions);
    /// elementRow[i]: row of piece i; partial[r]: red line for the pieces of row r.
    void setState(const std::vector<int>& elementRow, const std::vector<bool>& partial);
    /// Screen angle (radians, 0 = right, counter-clockwise) at which the piece is drawn; tests check A left, B up, C right.
    double angleOf(int piece) const;
    bool circleDrawn() const;
    bool pieceRed(int piece) const;

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    std::vector<double> m_angles;
    std::vector<int> m_rows;
    std::vector<bool> m_partial;
};

}  // namespace cc::app
