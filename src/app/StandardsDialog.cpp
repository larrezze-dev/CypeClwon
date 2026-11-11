#include "StandardsDialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>

#include "Icons.h"
#include "UserMessages.h"
#include "core/limit_states.h"

namespace cc::app {

using cc::core::StandardFamily;

StandardsDialog::StandardsDialog(const cc::core::StandardsSelection& initial, QWidget* parent)
    : CypeDialog(tr("Selección de normas"), parent), m_initial(initial) {
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
    // "Cimentación" only with the Código Estructural / CTE (measured: it is not there with Eurocodes).
    m_foundationLabel = new QLabel(tr("Cimentación"), this);
    m_foundation = new QComboBox(this);
    for (const std::string& name : cc::core::StandardsSelection::foundationCriteria()) m_foundation->addItem(QString::fromStdString(name));
    m_foundation->setCurrentText(QString::fromStdString(initial.foundation));
    grid->addWidget(m_foundationLabel, r, 0);
    grid->addWidget(m_foundation, r, 1);
    grid->setColumnStretch(1, 1);
    body->addLayout(grid, 1);

    auto* shortcuts = new QVBoxLayout;
    struct Shortcut {
        QString text;
        cc::core::StandardsSelection set;
    };
    for (const Shortcut& sc : {Shortcut{tr("C.E."), cc::core::StandardsSelection::structuralCodeSet()},
                               Shortcut{tr("CTE"), cc::core::StandardsSelection::cteSet()}}) {
        auto* button = new QPushButton(themedIcon(QStringLiteral("arrow_left_blue")), sc.text, this);
        button->setToolTip(tr("Asignar normas correspondientes al Código Técnico de la Edificación"));
        if (sc.text == tr("C.E.")) button->setToolTip(tr("Asignar normas correspondientes al Código Estructural"));
        const cc::core::StandardsSelection set = sc.set;
        connect(button, &QPushButton::clicked, this, [this, set] { applySet(set); });
        shortcuts->addWidget(button);
    }
    shortcuts->addStretch(1);
    body->addLayout(shortcuts);
    contentLayout()->addLayout(body);
    contentLayout()->addStretch(1);

    setupFooter(tr("Aceptar"), tr("Cancelar"));
    connect(m_combos[StandardFamily::Concrete], &QComboBox::currentTextChanged, this, [this] { updateFoundationRow(); });
    updateFoundationRow();
}

bool StandardsDialog::foundationApplies() const {
    // Measured: the combo "Cimentación" exists only with the concrete "Código Estructural" (the CTE button assigns it too); 2 entries.
    return m_combos.at(StandardFamily::Concrete)->currentText().startsWith(QStringLiteral("Código Estructural"));
}

void StandardsDialog::updateFoundationRow() {
    const bool show = foundationApplies();
    m_foundation->setVisible(show);
    m_foundationLabel->setVisible(show);
}

void StandardsDialog::applySet(const cc::core::StandardsSelection& set) {
    for (auto& [family, combo] : m_combos) combo->setCurrentText(QString::fromStdString(set.get(family)));
    m_foundation->setCurrentText(QString::fromStdString(set.foundation));
    updateFoundationRow();
}

bool StandardsDialog::materialsChanged() const {
    const cc::core::StandardsSelection now = selection();
    for (const auto& [family, combo] : m_combos) {
        (void)combo;
        if (now.get(family) != m_initial.get(family)) return true;
    }
    return false;
}

QString StandardsDialog::materialsWarning() {
    return tr("Al cambiar la norma correspondiente a un tipo de material (acero laminado, madera, etc.), la selección de los materiales, para las barras de dicho tipo de material, se inicializa a su valor por defecto.");
}

void StandardsDialog::accept() {
    if (materialsChanged()) messages::show(this, messages::Kind::Warning, materialsWarning());
    CypeDialog::accept();
}

cc::core::StandardsSelection StandardsDialog::selection() const {
    cc::core::StandardsSelection result;
    for (const auto& [family, combo] : m_combos) result.set(family, combo->currentText().toStdString());
    result.foundation = foundationApplies() ? m_foundation->currentText().toStdString() : m_initial.foundation;
    return result;
}

// ------------------------------------------------------------------------------ Categorías de uso (casillas)

UseCategorySelectionDialog::Scheme UseCategorySelectionDialog::schemeFor(const cc::core::StandardsSelection& standards, bool* ok) {
    using cc::core::NormClass;
    const NormClass classes[] = {cc::core::normClassOf(standards.concrete), cc::core::normClassOf(standards.coldFormedSteel),
                                 cc::core::normClassOf(standards.hotRolledSteel), cc::core::normClassOf(standards.timber)};
    const auto all = [&](bool eurocode) {
        for (NormClass c : classes)
            if (eurocode ? c != NormClass::Eurocode : (c != NormClass::Cte && c != NormClass::StructuralCode)) return false;
        return true;
    };
    if (ok) *ok = all(true) || all(false);
    return all(true) ? Scheme::Eurocode : Scheme::Cte;
}

QStringList UseCategorySelectionDialog::categoryTexts(Scheme scheme) {
    if (scheme == Scheme::Eurocode)
        return {QObject::tr("A. Domésticos y residenciales"), QObject::tr("B. Oficinas"), QObject::tr("C. Áreas de reunión"),
                QObject::tr("D. Comercios"), QObject::tr("E. Almacenes"), QObject::tr("F. Peso de vehículo <= 30 kN"),
                QObject::tr("G. 30 kN < peso de vehículo <= 160 kN"), QObject::tr("H. Cubiertas")};
    return {QObject::tr("A. Zonas residenciales"), QObject::tr("B. Zonas administrativas"), QObject::tr("C. Zonas de acceso al público"),
            QObject::tr("D. Zonas comerciales"), QObject::tr("E. Zonas de tráfico y aparcamiento para vehículos ligeros"),
            QObject::tr("G1. Cubiertas accesibles únicamente para mantenimiento. No concomitante con el resto de acciones variables"),
            QObject::tr("G2. Cubiertas accesibles únicamente para mantenimiento")};
}

UseCategorySelectionDialog::UseCategorySelectionDialog(Scheme scheme, const std::vector<bool>& checked, QWidget* parent)
    : CypeDialog(tr("Categorías de uso"), parent), m_scheme(scheme) {
    setFixedSize(scheme == Scheme::Cte ? 634 : 330, scheme == Scheme::Cte ? 245 : 266);  // CTE measured (cmb_43); the Eurocode one [N]
    const QStringList texts = categoryTexts(scheme);
    for (int i = 0; i < texts.size(); ++i) {
        auto* box = new QCheckBox(texts[i], this);
        box->setChecked(static_cast<std::size_t>(i) < checked.size() && checked[static_cast<std::size_t>(i)]);
        contentLayout()->addWidget(box);
        m_boxes.push_back(box);
    }
    contentLayout()->addStretch(1);
    setupFooter(tr("Aceptar"), tr("Cancelar"));
}

std::vector<bool> UseCategorySelectionDialog::checked() const {
    std::vector<bool> out;
    for (const QCheckBox* b : m_boxes) out.push_back(b->isChecked());
    return out;
}

std::vector<std::string> UseCategorySelectionDialog::checkedReferences() const {
    std::vector<std::string> out;
    for (const QCheckBox* b : m_boxes)
        if (b->isChecked()) out.push_back(b->text().section(QLatin1Char('.'), 0, 0).toStdString());
    return out;
}

}  // namespace cc::app
