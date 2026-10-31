#include "StandardsDialog.h"

#include <QComboBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>

#include "Icons.h"

namespace cc::app {

using cc::core::StandardFamily;

StandardsDialog::StandardsDialog(const cc::core::StandardsSelection& initial, QWidget* parent)
    : CypeDialog(tr("Selección de normas"), parent) {
    setFixedSize(418, 247);

    struct Row {
        StandardFamily family;
        QString label;
    };
    const Row rows[] = {
        {StandardFamily::Concrete, tr("Hormigón")},
        {StandardFamily::ColdFormedSteel, tr("Acero conformado")},
        {StandardFamily::HotRolledSteel, tr("Acero laminado")},
        {StandardFamily::Timber, tr("Madera")},
        {StandardFamily::Aluminium, tr("Aluminio")},
    };

    auto* body = new QHBoxLayout;
    auto* grid = new QGridLayout;
    grid->setVerticalSpacing(10);
    int r = 0;
    for (const Row& row : rows) {
        auto* combo = new QComboBox(this);
        for (const std::string& name : cc::core::availableStandards(row.family)) {
            combo->addItem(QString::fromStdString(name));
        }
        combo->setCurrentText(QString::fromStdString(initial.get(row.family)));
        combo->setMaxVisibleItems(20);
        m_combos[row.family] = combo;
        grid->addWidget(new QLabel(row.label, this), r, 0);
        grid->addWidget(combo, r, 1);
        ++r;
    }
    grid->setColumnStretch(1, 1);
    body->addLayout(grid, 1);

    // "C.E." and "CTE" apply a whole code set at once in the original. The exact sets have not been
    // verified yet (ticket TK-010), so the buttons are shown but disabled until then.
    auto* shortcuts = new QVBoxLayout;
    for (const QString& text : {tr("C.E."), tr("CTE")}) {
        auto* button = new QPushButton(themedIcon(QStringLiteral("arrow_left_blue")), text, this);
        button->setEnabled(false);
        button->setToolTip(tr("Aplica de golpe el conjunto de normas (pendiente de verificar)."));
        shortcuts->addWidget(button);
    }
    shortcuts->addStretch(1);
    body->addLayout(shortcuts);
    contentLayout()->addLayout(body);
    contentLayout()->addStretch(1);

    setupFooter(tr("Aceptar"), tr("Cancelar"));
}

cc::core::StandardsSelection StandardsDialog::selection() const {
    cc::core::StandardsSelection result;
    for (const auto& [family, combo] : m_combos) result.set(family, combo->currentText().toStdString());
    return result;
}

}  // namespace cc::app
