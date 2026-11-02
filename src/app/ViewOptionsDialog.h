#pragma once
#include "CypeDialog.h"

class QCheckBox;

namespace cc::app {

/// «Opciones» of the 3D window (icon y=440 of the view bar; docs/research/ui/barra_vertical_vistas.md, session 21 II):
/// 265 x 136 px, two unchecked boxes and a single Aceptar button.
class ViewOptionsDialog : public CypeDialog {
    Q_OBJECT
public:
    explicit ViewOptionsDialog(bool objectEdges, bool ssao, QWidget* parent = nullptr);
    bool objectEdges() const;
    bool ssao() const;
    QCheckBox* edgesBox() const { return m_edges; }
    QCheckBox* ssaoBox() const { return m_ssao; }

private:
    QCheckBox* m_edges = nullptr;
    QCheckBox* m_ssao = nullptr;
};

}  // namespace cc::app
