#include "ViewOptionsDialog.h"

#include <QCheckBox>
#include <QHBoxLayout>
#include <QPushButton>
#include <QVBoxLayout>

namespace cc::app {

ViewOptionsDialog::ViewOptionsDialog(bool objectEdges, bool ssao, QWidget* parent) : CypeDialog(tr("Opciones"), parent) {
    setFixedSize(265, 136);
    m_edges = new QCheckBox(tr("Aristas de objetos"), this);
    m_ssao = new QCheckBox(tr("Iluminación ambiental mejorada (SSAO)"), this);
    m_edges->setChecked(objectEdges);
    m_ssao->setChecked(ssao);
    contentLayout()->addWidget(m_edges);
    contentLayout()->addWidget(m_ssao);
    contentLayout()->addStretch(1);
    QHBoxLayout* footer = setupCustomFooter();  // only Aceptar, centred (no Cancelar in the original)
    auto* ok = new QPushButton(tr("Aceptar"), this);
    ok->setDefault(true);
    footer->addStretch(1);
    footer->addWidget(ok);
    footer->addStretch(1);
    connect(ok, &QPushButton::clicked, this, &QDialog::accept);
}

bool ViewOptionsDialog::objectEdges() const { return m_edges->isChecked(); }
bool ViewOptionsDialog::ssao() const { return m_ssao->isChecked(); }

}  // namespace cc::app
