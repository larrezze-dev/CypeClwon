#pragma once
#include <QString>
#include <array>

#include "CypeDialog.h"
#include "core/shell_properties.h"

class QButtonGroup;
class QCheckBox;
class QLabel;
class QRadioButton;

namespace cc::app {

class RangedEdit;

/// Dialog "Discretización" (propiedades_laminas.md section 1, p7_21, about 634 x 214): a light-blue information frame with the two measured
/// paragraphs and the editor «Tamaño máximo de los lados de un triángulo» (default 1.00, 0.01 .. 100.00 in the unit of length of the work).
class ShellDiscretisationDialog : public CypeDialog {
    Q_OBJECT
public:
    explicit ShellDiscretisationDialog(double maxSideM, const QString& unit = QStringLiteral("m"), QWidget* parent = nullptr);
    double maxSide() const;
    RangedEdit* edit() const { return m_edit; }
    QLabel* infoLabel() const { return m_info; }
    void accept() override;

private:
    RangedEdit* m_edit = nullptr;
    QLabel* m_info = nullptr;
};

/// Dialog "Factores de rigidez" (section 2, p7_30, about 180 x 323): eight editors in the order Membrana Fxx, Fxy, Fyy, Flexión Mxx, Mxy, Myy,
/// Cortante Vxz, Vyz; 3 decimals, 0.000 .. 100.000 (the warning keeps the measured double space: «entre 0.000  y 100.000 .»).
class ShellStiffnessDialog : public CypeDialog {
    Q_OBJECT
public:
    explicit ShellStiffnessDialog(const cc::core::ShellStiffnessFactors& initial, QWidget* parent = nullptr);
    cc::core::ShellStiffnessFactors factors() const;
    RangedEdit* edit(int dialogIndex) const { return m_edits[static_cast<std::size_t>(dialogIndex)]; }
    void accept() override;

private:
    std::array<RangedEdit*, cc::core::kStiffnessFactorCount> m_edits{};
};

/// Dialog «Vinculación interior» of shell edges (section 4, p7_vi_b..d, about 473 x 310): four radios Libre / Empotrada (default) / Articulada /
/// Genérica; «Genérica» shows «Desplazamientos en ejes locales de la arista» (Dx Dy Dz) and «Giros en ejes locales de la arista» (Gx Gy Gz), each row
/// Libre (default) / Fijo / Elástico with K (default 100000) in kN/m² or kN·m/rad.m.
class EdgeInteriorDialog : public CypeDialog {
    Q_OBJECT
public:
    explicit EdgeInteriorDialog(const cc::core::EdgeInteriorBond& initial = {}, QWidget* parent = nullptr);
    cc::core::EdgeInteriorBond bond() const;
    cc::core::EdgeInteriorKind kind() const { return m_kind; }
    void selectKind(cc::core::EdgeInteriorKind kind);
    QRadioButton* kindRadio(cc::core::EdgeInteriorKind kind) const { return m_kindRadios[static_cast<std::size_t>(kind)]; }
    /// Rows Dx..Gz (0..5).
    QRadioButton* modeRadio(int dof, cc::core::DofMode mode) const { return m_modeRadios[static_cast<std::size_t>(dof)][static_cast<std::size_t>(mode)]; }
    RangedEdit* kEdit(int dof) const { return m_k[static_cast<std::size_t>(dof)]; }
    QWidget* genericPanel() const { return m_generic; }
    void accept() override;

private:
    void load(const cc::core::EdgeInteriorBond& bond);

    cc::core::EdgeInteriorKind m_kind = cc::core::EdgeInteriorKind::Fixed;
    std::array<QRadioButton*, 4> m_kindRadios{};
    std::array<std::array<QRadioButton*, 3>, 6> m_modeRadios{};
    std::array<RangedEdit*, 6> m_k{};
    QWidget* m_generic = nullptr;
};

/// One-check-box dialog: «Pandeo» (section 6, «Considerar en el análisis de pandeo», default off, about 255 x 130) and «Análisis modal» («Considerar en
/// el análisis», default on, about 180 x 112).
class ShellConsiderDialog : public CypeDialog {
    Q_OBJECT
public:
    enum class Kind { Buckling, Modal };
    ShellConsiderDialog(Kind kind, bool considered, QWidget* parent = nullptr);
    bool considered() const;
    QCheckBox* check() const { return m_check; }

private:
    QCheckBox* m_check = nullptr;
};

/// Dialog «Integración de esfuerzos en bandas» (section 3, p7_i1): radios «A partir de los esfuerzos internos» (default, bold) / «A partir de las fuerzas
/// nodales» and the indented check box «Utilizar esfuerzos alisados» (only for the first method).
class BandForceDialog : public CypeDialog {
    Q_OBJECT
public:
    explicit BandForceDialog(const cc::core::BandForceOptions& initial = {}, QWidget* parent = nullptr);
    cc::core::BandForceOptions options() const;
    QRadioButton* internalRadio() const { return m_internal; }
    QRadioButton* nodalRadio() const { return m_nodal; }
    QCheckBox* smoothCheck() const { return m_smooth; }

private:
    QRadioButton* m_internal = nullptr;
    QRadioButton* m_nodal = nullptr;
    QCheckBox* m_smooth = nullptr;
};

/// Dialogs «Nueva», «Generar bandas de integración» and «Editar» of the band editor: «Anchura» in cm (default 15.0, 1.0 .. 10000.0).
class BandWidthDialog : public CypeDialog {
    Q_OBJECT
public:
    explicit BandWidthDialog(const QString& title, double widthCm = cc::core::kBandWidthDefaultM * 100.0, QWidget* parent = nullptr);
    double widthM() const;
    RangedEdit* edit() const { return m_edit; }
    void accept() override;

private:
    RangedEdit* m_edit = nullptr;
};

/// Dialog «Dividir» of the band editor: «Número de divisiones» (default 2).
class BandDivisionsDialog : public CypeDialog {
    Q_OBJECT
public:
    explicit BandDivisionsDialog(int parts = cc::core::kBandMinDivisions, QWidget* parent = nullptr);
    int parts() const;
    RangedEdit* edit() const { return m_edit; }
    void accept() override;

private:
    RangedEdit* m_edit = nullptr;
};

}  // namespace cc::app
