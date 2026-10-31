#include "EmptyProjectWizard.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFontMetrics>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QStyleOptionButton>
#include <QStylePainter>
#include <QToolButton>
#include <QVBoxLayout>

#include "StandardsDialog.h"
#include "WizardSubDialogs.h"

namespace cc::app {

using namespace cc::core;

namespace {

/// Push button whose text is elided on the right when too long, as the value buttons of the original.
class ElidedButton : public QPushButton {
public:
    using QPushButton::QPushButton;

protected:
    void paintEvent(QPaintEvent*) override {
        QStylePainter painter(this);
        QStyleOptionButton option;
        initStyleOption(&option);
        option.text = fontMetrics().elidedText(text(), Qt::ElideRight, width() - 16);
        painter.drawControl(QStyle::CE_PushButton, option);
    }
};

QLabel* sectionHeader(const QString& text, QWidget* parent) {
    auto* label = new QLabel(text, parent);
    label->setStyleSheet(QStringLiteral("background: #DADADA; padding: 2px 4px; font-weight: 600;"));
    return label;
}

QString qs(const std::string& s) { return QString::fromStdString(s); }

}  // namespace

EmptyProjectWizard::EmptyProjectWizard(QWidget* parent) : CypeDialog(tr("Obra vacía"), parent) {
    setFixedSize(848, 774);

    auto* normsRow = new QHBoxLayout;
    auto* normsLabel = new QLabel(tr("<b>Normas:</b>"), this);
    m_standardsButton = new ElidedButton(this);
    normsRow->addWidget(normsLabel);
    normsRow->addWidget(m_standardsButton, 1);
    contentLayout()->addLayout(normsRow);
    connect(m_standardsButton, &QPushButton::clicked, this, [this] {
        StandardsDialog dialog(m_standards, this);
        if (dialog.exec() != QDialog::Accepted) return;
        m_standards = dialog.selection();
        refreshButtons();
    });

    auto* columns = new QHBoxLayout;
    columns->setSpacing(10);
    auto* left = new QVBoxLayout;
    auto* right = new QVBoxLayout;
    columns->addLayout(left, 4);
    columns->addLayout(right, 6);
    contentLayout()->addLayout(columns, 1);

    buildProfiles(left);
    left->addSpacing(12);
    buildActions(left);
    left->addStretch(1);  // Opciones and Ambiente arrive with T-P2-05.
    buildReinforcedConcrete(right);
    right->addStretch(1);  // Terreno de cimentación and Pilotes arrive with T-P2-05.

    QHBoxLayout* footer = setupCustomFooter();
    auto* cancel = new QPushButton(tr("Cancelar"), this);
    m_previous = new QPushButton(tr("< Anterior"), this);
    m_next = new QPushButton(tr("Siguiente >"), this);
    m_finish = new QPushButton(tr("Terminar"), this);
    m_previous->setEnabled(false);  // single-page wizard: navigation buttons are always disabled
    m_next->setEnabled(false);
    m_finish->setDefault(true);
    footer->addWidget(cancel);
    footer->addStretch(1);
    footer->addWidget(m_previous);
    footer->addWidget(m_next);
    footer->addWidget(m_finish);
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_finish, &QPushButton::clicked, this, &QDialog::accept);

    refreshButtons();
}

QComboBox* EmptyProjectWizard::concreteCombo(const std::string& current, std::string& target) {
    auto* combo = new QComboBox(this);
    for (const std::string& name : concreteGrades()) combo->addItem(qs(name));
    combo->setCurrentText(qs(current));
    connect(combo, &QComboBox::currentTextChanged, this, [&target](const QString& text) { target = text.toStdString(); });
    return combo;
}

void EmptyProjectWizard::buildProfiles(QVBoxLayout* layout) {
    auto* header = new QWidget(this);
    auto* headerRow = new QHBoxLayout(header);
    headerRow->setContentsMargins(0, 0, 0, 0);
    headerRow->setSpacing(0);
    auto* title = sectionHeader(tr("Perfiles"), header);
    headerRow->addWidget(title, 1);
    auto* fy = new QToolButton(header);
    fy->setText(QStringLiteral("fy"));
    fy->setToolTip(tr("Biblioteca de aceros de usuario"));
    fy->setStyleSheet(QStringLiteral("QToolButton { background: #F4E9A8; border: 1px solid #8A8A8A; font-style: italic; }"));
    headerRow->addWidget(fy);
    layout->addWidget(header);
    connect(fy, &QToolButton::clicked, this, [this] { SteelLibraryDialog(this).exec(); });

    auto* grid = new QGridLayout;
    grid->setColumnStretch(1, 1);
    const auto steelCombo = [this](const std::vector<std::string>& items, const std::string& current, std::string& target) {
        auto* combo = new QComboBox(this);
        for (const std::string& item : items) combo->addItem(qs(item));
        combo->setCurrentText(qs(current));
        combo->setMaxVisibleItems(10);
        connect(combo, &QComboBox::currentTextChanged, this, [&target](const QString& text) { target = text.toStdString(); });
        return combo;
    };
    grid->addWidget(new QLabel(tr("Acero laminado"), this), 0, 0);
    grid->addWidget(steelCombo(hotRolledSteelGrades(), m_profiles.hotRolledSteel, m_profiles.hotRolledSteel), 0, 1);
    grid->addWidget(new QLabel(tr("Acero conformado"), this), 1, 0);
    grid->addWidget(steelCombo(coldFormedSteelGrades(), m_profiles.coldFormedSteel, m_profiles.coldFormedSteel), 1, 1);

    grid->addWidget(new QLabel(tr("Madera"), this), 2, 0);
    m_timberButton = new ElidedButton(this);
    grid->addWidget(m_timberButton, 2, 1);
    connect(m_timberButton, &QPushButton::clicked, this, [this] {
        TimberDialog dialog(m_profiles.timber, this);
        if (dialog.exec() != QDialog::Accepted) return;
        m_profiles.timber = dialog.value();
        refreshButtons();
    });

    grid->addWidget(new QLabel(tr("Aluminio"), this), 3, 0);
    m_aluminiumButton = new ElidedButton(this);
    grid->addWidget(m_aluminiumButton, 3, 1);
    connect(m_aluminiumButton, &QPushButton::clicked, this, [this] {
        AluminiumDialog dialog(m_profiles.aluminium, this);
        if (dialog.exec() != QDialog::Accepted) return;
        m_profiles.aluminium = dialog.value();
        refreshButtons();
    });

    grid->addWidget(new QLabel(tr("Hormigón"), this), 4, 0);
    grid->addWidget(concreteCombo(m_profiles.concrete, m_profiles.concrete), 4, 1);
    layout->addLayout(grid);
}

void EmptyProjectWizard::buildReinforcedConcrete(QVBoxLayout* layout) {
    layout->addWidget(sectionHeader(tr("Hormigón armado"), this));

    auto* grid = new QGridLayout;
    grid->setColumnStretch(0, 1);
    const std::pair<QString, std::string*> concretes[] = {{tr("Hormigón para pilares"), &m_concrete.columnConcrete},
                                                          {tr("Hormigón para vigas de forjado"), &m_concrete.beamConcrete},
                                                          {tr("Hormigón para elementos de cimentación"), &m_concrete.foundationConcrete}};
    int row = 0;
    for (const auto& [label, target] : concretes) {
        grid->addWidget(new QLabel(label, this), row, 0);
        grid->addWidget(concreteCombo(*target, *target), row, 1, 1, 1, Qt::AlignRight);
        ++row;
    }
    grid->addWidget(new QLabel(tr("Acero de barras"), this), row, 0);
    m_rebarButton = new ElidedButton(this);
    grid->addWidget(m_rebarButton, row++, 1);
    grid->addWidget(new QLabel(tr("Características del árido"), this), row, 0);
    m_aggregateButton = new ElidedButton(this);
    grid->addWidget(m_aggregateButton, row++, 1);
    grid->setColumnMinimumWidth(1, 200);
    layout->addLayout(grid);

    auto* bottom = new QHBoxLayout;
    auto* covers = new QPushButton(tr("Recubrimientos"), this);
    auto* waste = new QPushButton(tr("Mermas de acero"), this);
    bottom->addWidget(covers);
    bottom->addWidget(waste);
    layout->addLayout(bottom);

    connect(m_rebarButton, &QPushButton::clicked, this, [this] {
        RebarSteelDialog dialog(m_concrete.rebarSteel, this);
        if (dialog.exec() != QDialog::Accepted) return;
        m_concrete.rebarSteel = dialog.value();
        refreshButtons();
    });
    connect(m_aggregateButton, &QPushButton::clicked, this, [this] {
        AggregateDialog dialog(m_concrete.aggregate, this);
        if (dialog.exec() != QDialog::Accepted) return;
        m_concrete.aggregate = dialog.value();
        refreshButtons();
    });
    connect(covers, &QPushButton::clicked, this, [this] {
        CoverMenuDialog menu(m_concrete.cover, this);
        menu.exec();
        m_concrete.cover = menu.value();
    });
    connect(waste, &QPushButton::clicked, this, [this] {
        WasteDialog dialog(m_concrete.waste, this);
        if (dialog.exec() != QDialog::Accepted) return;
        m_concrete.waste = dialog.value();
    });
}

void EmptyProjectWizard::buildActions(QVBoxLayout* layout) {
    layout->addWidget(sectionHeader(tr("Acciones"), this));
    auto* seismic = new QCheckBox(tr("Con sismo dinámico"), this);
    seismic->setChecked(m_actions.dynamicSeismic);
    connect(seismic, &QCheckBox::toggled, this, [this](bool on) { m_actions.dynamicSeismic = on; });
    layout->addWidget(seismic);

    const auto addButton = [this, layout](const QString& text) {
        auto* button = new QPushButton(text, this);
        layout->addWidget(button);
        return button;
    };
    connect(addButton(tr("Resistencia al fuego")), &QPushButton::clicked, this, [this] {
        FireDialog dialog(m_actions.steelFire, this);
        if (dialog.exec() == QDialog::Accepted) m_actions.steelFire = dialog.value();
    });
    auto* limitStates = addButton(tr("Estados límite (combinaciones)"));
    limitStates->setEnabled(false);  // dialog arrives with T-P2-06
    limitStates->setToolTip(tr("Disponible próximamente."));
    connect(addButton(tr("Hipótesis adicionales")), &QPushButton::clicked, this, [this] { AdditionalLoadsDialog(this).exec(); });
    connect(addButton(tr("Cimentación")), &QPushButton::clicked, this, [this] {
        FoundationActionsDialog dialog(m_actions.foundationWind, m_actions.foundationSeism, this);
        if (dialog.exec() != QDialog::Accepted) return;
        m_actions.foundationWind = dialog.wind();
        m_actions.foundationSeism = dialog.seism();
    });
    connect(addButton(tr("Proceso constructivo")), &QPushButton::clicked, this, [this] {
        ConstructionProcessDialog dialog(m_actions.formworkRemovalDays, this);
        if (dialog.exec() == QDialog::Accepted) m_actions.formworkRemovalDays = dialog.days();
    });
}

void EmptyProjectWizard::refreshButtons() {
    m_standardsButton->setText(qs(m_standards.summary()));
    m_timberButton->setText(qs(m_profiles.timber.summary()));
    m_aluminiumButton->setText(qs(m_profiles.aluminium.summary()));
    m_rebarButton->setText(qs(m_concrete.rebarSteel.summary()));
    m_aggregateButton->setText(qs(m_concrete.aggregate.summary()));
}

}  // namespace cc::app
