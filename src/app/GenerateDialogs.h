#pragma once
#include <QCheckBox>
#include <QWidget>
#include <array>
#include <vector>

#include "CypeDialog.h"
#include "RangedEdit.h"
#include "core/generate_edit.h"



namespace cc::app {

/// Schematic drawing of the generation dialogs (the original shows an illustration with the segments numbered). Original artwork.
class GenerateSketch : public QWidget {
    Q_OBJECT
public:
    explicit GenerateSketch(int segments, QWidget* parent = nullptr);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    int m_segments;
};

/// "Generación de planos" (herramientas.md 6.2): "Nº de planos a generar" 1 (1 - 100), "Separación entre planos" 1.000 m
/// (-10000.000 to 10000.000) and the checkbox "Agrupar planos generados" (off). Separation 0 is refused with a warning: the original crashes.
class PlanesDialog : public CypeDialog {
    Q_OBJECT
public:
    explicit PlanesDialog(QWidget* parent = nullptr);
    int count() const { return static_cast<int>(m_count->value()); }
    double separationM() const { return m_separation->value(); }
    bool group() const { return m_group->isChecked(); }
    RangedEdit* countEdit() const { return m_count; }
    RangedEdit* separationEdit() const { return m_separation; }
    QCheckBox* groupBox() const { return m_group; }

private:
    void accept() override;
    RangedEdit* m_count = nullptr;
    RangedEdit* m_separation = nullptr;
    QCheckBox* m_group = nullptr;
};

/// "Nudos y barras": the title follows the number of segments (1 "Generación de nudos sobre un segmento", 2 "Generación de una malla de
/// nudos bidimensional", 3 "Generación de una malla de nudos tridimensional"). One block per segment: "Nº de divisiones del segmento k"
/// (1 - 1000, default 1) and, from two segments on, "Agrupar barras paralelas al segmento k" and "Crear pieza entre barras alineadas"
/// (both off); from two segments on also "Generar sólo los nudos" (off).
class NodeMeshDialog : public CypeDialog {
    Q_OBJECT
public:
    explicit NodeMeshDialog(int segments, QWidget* parent = nullptr);
    static QString titleFor(int segments);
    /// Fills the divisions, flags and so on of `spec` (its nodeIds are the caller's).
    void fill(cc::core::NodeMeshSpec& spec) const;
    int segments() const { return m_segments; }
    RangedEdit* divisionsEdit(int segment) const { return m_divisions.at(static_cast<std::size_t>(segment)); }
    QCheckBox* onlyNodesBox() const { return m_onlyNodes; }
    QCheckBox* groupBox(int segment) const { return m_group.at(static_cast<std::size_t>(segment)); }
    QCheckBox* pieceBox(int segment) const { return m_piece.at(static_cast<std::size_t>(segment)); }

private:
    void accept() override;
    int m_segments;
    std::array<RangedEdit*, 3> m_divisions{};
    std::array<QCheckBox*, 3> m_group{}, m_piece{};
    QCheckBox* m_onlyNodes = nullptr;
};

/// "Generación de una malla de tetraedros": divisions of segments 1 and 2 (1), "Distancia entre planos" (1.000 m) and the three
/// checkboxes "Agrupar barras de plano inferior / de plano superior / entre planos (diagonales)" (on). The ranges are NOT measured:
/// divisions 1 - 1000 like the other generators, distance +-10000 m and not zero (PROVISIONAL).
class TetrahedraDialog : public CypeDialog {
    Q_OBJECT
public:
    explicit TetrahedraDialog(QWidget* parent = nullptr);
    void fill(cc::core::TetrahedraSpec& spec) const;
    RangedEdit* divisions1Edit() const { return m_div1; }
    RangedEdit* divisions2Edit() const { return m_div2; }
    RangedEdit* distanceEdit() const { return m_distance; }
    const std::array<QCheckBox*, 3>& groupBoxes() const { return m_groups; }

private:
    void accept() override;
    RangedEdit* m_div1 = nullptr;
    RangedEdit* m_div2 = nullptr;
    RangedEdit* m_distance = nullptr;
    std::array<QCheckBox*, 3> m_groups{};
};

}  // namespace cc::app
