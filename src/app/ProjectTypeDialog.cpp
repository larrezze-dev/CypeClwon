#include "ProjectTypeDialog.h"

#include <QFont>
#include <QLabel>
#include <QMessageBox>
#include <QPixmap>
#include <QRadioButton>
#include <QVBoxLayout>

namespace cc::app {

ProjectTypeDialog::ProjectTypeDialog(QWidget* parent) : CypeDialog(tr("Nueva obra"), parent) {
    setFixedSize(682, 457);
    contentLayout()->setContentsMargins(0, 0, 0, 0);
    contentLayout()->setSpacing(0);

    auto* options = new QWidget(this);
    options->setStyleSheet(QStringLiteral("background: #DCDCDC;"));
    auto* optionsLayout = new QVBoxLayout(options);
    optionsLayout->setContentsMargins(12, 8, 12, 8);
    optionsLayout->setSpacing(4);
    m_empty = new QRadioButton(tr("Obra vacía"), options);
    m_ifc = new QRadioButton(tr("Introducción automática IFC"), options);
    m_empty->setChecked(true);
    optionsLayout->addWidget(m_empty);
    optionsLayout->addWidget(m_ifc);
    contentLayout()->addWidget(options);

    // Original artwork (not taken from the CYPE application).
    auto* picture = new QLabel(this);
    picture->setStyleSheet(QStringLiteral("background: white;"));
    picture->setAlignment(Qt::AlignCenter);
    picture->setPixmap(QPixmap(QStringLiteral(":/icons/structure_sample.svg")).scaledToWidth(560, Qt::SmoothTransformation));
    contentLayout()->addWidget(picture, 1);

    setupFooter(tr("Aceptar"), tr("Cancelar"));
    connect(m_empty, &QRadioButton::toggled, this, &ProjectTypeDialog::updateBoldSelection);
    updateBoldSelection();
}

ProjectTypeDialog::ProjectType ProjectTypeDialog::projectType() const {
    return m_ifc->isChecked() ? ProjectType::Ifc : ProjectType::Empty;
}

void ProjectTypeDialog::updateBoldSelection() {
    // The selected option is shown in bold, as in the original.
    for (QRadioButton* radio : {m_empty, m_ifc}) {
        QFont font = radio->font();
        font.setBold(radio->isChecked());
        radio->setFont(font);
    }
}

void ProjectTypeDialog::accept() {
    if (!isAvailable(projectType())) {
        QMessageBox::information(this, tr("Aviso"),
                                 tr("La introducción automática desde IFC aún no está disponible en esta versión."));
        return;
    }
    CypeDialog::accept();
}

}  // namespace cc::app
