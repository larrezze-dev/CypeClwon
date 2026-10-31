#include "EmptyProjectWizard.h"

#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QTabWidget>
#include <memory>
#include <QComboBox>
#include <QFontMetrics>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QStyleOptionButton>
#include <QStylePainter>
#include <QToolButton>
#include <QVBoxLayout>

#include "Icons.h"
#include "LimitStatesDialog.h"
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
    columns->addLayout(left, 41);
    columns->addLayout(right, 59);
    contentLayout()->addLayout(columns, 1);

    buildProfiles(left);
    left->addStretch(1);
    buildActions(left);
    left->addSpacing(12);
    buildOptions(left);
    left->addSpacing(12);
    buildEnvironment(left);
    buildReinforcedConcrete(right);
    right->addStretch(1);
    buildGround(right);
    right->addSpacing(8);
    buildPiles(right);

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
    connect(addButton(tr("Estados límite (combinaciones)")), &QPushButton::clicked, this, [this] {
        LimitStatesDialog dialog(m_standards, m_limitStates, this);
        if (dialog.exec() == QDialog::Accepted) m_limitStates = dialog.value();
    });
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

void EmptyProjectWizard::buildOptions(QVBoxLayout* layout) {
    layout->addWidget(sectionHeader(tr("Opciones"), this));
    auto* grid = new QGridLayout;
    const std::pair<QString, OptionsMenuDialog::Family> entries[] = {{tr("Pilares"), OptionsMenuDialog::Family::Columns},
                                                                     {tr("Cimentación"), OptionsMenuDialog::Family::Foundation},
                                                                     {tr("Vigas"), OptionsMenuDialog::Family::Beams},
                                                                     {tr("Uniones"), OptionsMenuDialog::Family::Joints}};
    int index = 0;
    for (const auto& [caption, family] : entries) {
        auto* button = new QPushButton(caption, this);
        grid->addWidget(button, index / 2, index % 2);
        ++index;
        connect(button, &QPushButton::clicked, this, [this, family = family] {
            std::unique_ptr<OptionsMenuDialog> menu(OptionsMenuDialog::create(family, m_options, this));
            menu->exec();
        });
    }
    layout->addLayout(grid);
}

void EmptyProjectWizard::buildEnvironment(QVBoxLayout* layout) {
    layout->addWidget(sectionHeader(tr("Ambiente"), this));
    auto* row = new QHBoxLayout;
    row->addWidget(new QLabel(tr("Vigas"), this));
    m_environmentButton = new QPushButton(this);
    row->addWidget(m_environmentButton, 1);
    layout->addLayout(row);
    connect(m_environmentButton, &QPushButton::clicked, this, [this] {
        BeamEnvironmentDialog dialog(m_environment, this);
        if (dialog.exec() != QDialog::Accepted) return;
        m_environment = dialog.value();
        refreshButtons();
    });
}

void EmptyProjectWizard::buildGround(QVBoxLayout* layout) {
    layout->addWidget(sectionHeader(tr("Terreno de cimentación"), this));
    auto* sliding = new QCheckBox(tr("Verificar deslizamiento de zapatas"), this);
    sliding->setChecked(m_ground.checkFootingSliding);
    connect(sliding, &QCheckBox::toggled, this, [this](bool on) { m_ground.checkFootingSliding = on; });
    layout->addWidget(sliding);
    layout->addWidget(new QLabel(tr("Tensiones admisibles del terreno"), this));

    auto* grid = new QGridLayout;
    grid->setColumnStretch(0, 1);
    struct Row {
        QString label;
        double* target;
        QLineEdit** edit;
    };
    const Row rows[] = {{tr("Situaciones persistentes (A2 - Conjunto C)"), &m_ground.persistentStressMPa, &m_persistentEdit},
                        {tr("Situaciones sísmicas y accidentales (A2 - Conjunto C)"), &m_ground.seismicStressMPa, &m_seismicEdit}};
    int r = 0;
    for (const Row& row : rows) {
        auto* edit = new QLineEdit(QString::number(*row.target, 'f', 3), this);
        edit->setFixedWidth(64);
        edit->setAlignment(Qt::AlignRight);
        *row.edit = edit;
        const QString label = row.label;
        double* target = row.target;
        // Out-of-range or non-numeric input shows the "Aviso" message and restores the previous value.
        connect(edit, &QLineEdit::editingFinished, this, [this, edit, label, target] {
            bool ok = false;
            const double value = edit->text().replace(',', '.').toDouble(&ok);
            if (ok && GroundSettings::isValidStress(value)) {
                *target = value;
            } else {
                QMessageBox::warning(this, tr("Aviso"),
                                     tr("El valor de '%1' debe estar entre %2 y %3.")
                                         .arg(label)
                                         .arg(GroundSettings::kMinStress, 0, 'f', 3)
                                         .arg(GroundSettings::kMaxStress, 0, 'f', 3));
            }
            edit->setText(QString::number(*target, 'f', 3));
        });
        grid->addWidget(new QLabel(row.label, this), r, 0);
        grid->addWidget(edit, r, 1);
        grid->addWidget(new QLabel(tr("MPa"), this), r, 2);
        ++r;
    }
    auto* import = new QToolButton(this);
    import->setIcon(themedIcon(QStringLiteral("arrow_left_blue")));
    import->setToolTip(tr("Importar valores usuales de proyecto"));
    grid->addWidget(import, 0, 3, 2, 1);
    layout->addLayout(grid);
    connect(import, &QToolButton::clicked, this, [this] {
        UsualGroundDialog dialog(this);
        if (dialog.exec() != QDialog::Accepted || dialog.selectedIndex() < 0) return;
        const auto& usual = usualGroundValues()[static_cast<std::size_t>(dialog.selectedIndex())];
        m_ground.persistentStressMPa = usual.persistent;
        m_ground.seismicStressMPa = usual.seismic;
        m_persistentEdit->setText(QString::number(usual.persistent, 'f', 3));
        m_seismicEdit->setText(QString::number(usual.seismic, 'f', 3));
    });
}

namespace {

/// Right-aligned numeric field of the Pilotes tabs; edits are written straight into `target`.
QDoubleSpinBox* pileSpin(double& target, QWidget* parent) {
    auto* spin = new QDoubleSpinBox(parent);
    spin->setButtonSymbols(QAbstractSpinBox::NoButtons);
    spin->setDecimals(2);
    spin->setRange(0.0, 99.99);
    spin->setValue(target);
    spin->setAlignment(Qt::AlignRight);
    spin->setLocale(QLocale::c());  // the original shows a decimal point (0.10)
    spin->setFixedWidth(72);
    QObject::connect(spin, &QDoubleSpinBox::valueChanged, parent, [&target](double v) { target = v; });
    return spin;
}

}  // namespace

void EmptyProjectWizard::buildPiles(QVBoxLayout* layout) {
    auto* tabs = new QTabWidget(this);
    tabs->setObjectName(QStringLiteral("pilesTabs"));

    // --- Pilotes (Hormigón armado)
    auto* piles = new QWidget(tabs);
    auto* grid = new QGridLayout(piles);
    grid->setColumnStretch(0, 1);
    const auto unit = [piles](const QString& text) { return new QLabel(text, piles); };
    grid->addWidget(new QLabel(tr("Excentricidad para el momento mínimo"), piles), 0, 0, 1, 2);
    grid->addWidget(pileSpin(m_piles.minMomentEccentricityM, piles), 0, 2);
    grid->addWidget(unit(tr("m")), 0, 3);
    auto* reduced = new QCheckBox(tr("Considerar esfuerzos con un axil reducido"), piles);
    reduced->setChecked(m_piles.reducedAxial);
    connect(reduced, &QCheckBox::toggled, this, [this](bool on) { m_piles.reducedAxial = on; });
    grid->addWidget(reduced, 1, 0, 1, 4);
    grid->addWidget(new QLabel(tr("Pilotes que trabajan por fuste"), piles), 2, 0);
    grid->addWidget(pileSpin(m_piles.shaftAxialFactor, piles), 2, 2);
    grid->addWidget(unit(tr("· Axil")), 2, 3);
    grid->addWidget(new QLabel(tr("Pilotes que trabajan solo por punta"), piles), 3, 0);
    grid->addWidget(pileSpin(m_piles.tipOnlyAxialFactor, piles), 3, 2);
    grid->addWidget(unit(tr("· Axil")), 3, 3);
    grid->addWidget(new QLabel(tr("Coeficiente de pandeo"), piles), 4, 0, 1, 2);
    grid->addWidget(pileSpin(m_piles.bucklingCoefficient, piles), 4, 2);
    auto* lateral = new QCheckBox(tr("Análisis de la estabilidad lateral del pilote"), piles);
    lateral->setChecked(m_piles.lateralStability);
    connect(lateral, &QCheckBox::toggled, this, [this](bool on) { m_piles.lateralStability = on; });
    grid->addWidget(lateral, 5, 0, 1, 4);
    auto* advanced = new QPushButton(tr("Configuración avanzada"), piles);
    grid->addWidget(advanced, 6, 0, 1, 4);
    grid->setRowStretch(7, 1);
    connect(advanced, &QPushButton::clicked, this, [this] {
        PileAdvancedDialog dialog(m_piles.advanced, this);
        if (dialog.exec() == QDialog::Accepted) m_piles.advanced = dialog.value();
    });
    tabs->addTab(piles, tr("Pilotes (Hormigón armado)"));

    // --- Micropilotes
    auto* micro = new QWidget(tabs);
    auto* mg = new QGridLayout(micro);
    mg->setColumnStretch(0, 1);
    auto* mReduced = new QCheckBox(tr("Considerar esfuerzos con un axil reducido"), micro);
    mReduced->setChecked(m_micropiles.reducedAxial);
    connect(mReduced, &QCheckBox::toggled, this, [this](bool on) { m_micropiles.reducedAxial = on; });
    mg->addWidget(mReduced, 0, 0, 1, 2);
    mg->addWidget(pileSpin(m_micropiles.reducedAxialFactor, micro), 0, 2);
    mg->addWidget(new QLabel(tr("· Axil"), micro), 0, 3);
    auto* bucklingRow = new QHBoxLayout;
    bucklingRow->addWidget(new QLabel(tr("Análisis de pandeo"), micro));
    auto* bucklingMode = new QComboBox(micro);
    bucklingMode->addItem(tr("Coeficiente de pandeo"));  // the only entry seen in the original
    bucklingRow->addWidget(bucklingMode);
    bucklingRow->addStretch(1);
    mg->addLayout(bucklingRow, 1, 0, 1, 2);
    mg->addWidget(pileSpin(m_micropiles.bucklingCoefficient, micro), 1, 2);
    auto* mLateral = new QCheckBox(tr("Análisis de la estabilidad lateral del micropilote"), micro);
    mLateral->setChecked(m_micropiles.lateralStability);
    connect(mLateral, &QCheckBox::toggled, this, [this](bool on) { m_micropiles.lateralStability = on; });
    mg->addWidget(mLateral, 2, 0, 1, 4);

    // Rows with a blue arrow that restores the installation value.
    const MicropileSettings installation;
    struct ArrowRow {
        QString label;
        QString unitText;
        double* target;
        double installationValue;
    };
    const ArrowRow arrowRows[] = {
        {tr("Reducción de espesor de la armadura por efecto de la corrosión"), tr("mm"),
         &m_micropiles.corrosionThicknessMm, installation.corrosionThicknessMm},
        {tr("Coeficiente de minoración del área de la armadura tubular"), QString(), &m_micropiles.tubularAreaFactor,
         installation.tubularAreaFactor},
        {tr("Coeficiente de influencia del tipo de ejecución"), QString(), &m_micropiles.executionFactor,
         installation.executionFactor}};
    int mrow = 3;
    for (const ArrowRow& r : arrowRows) {
        mg->addWidget(new QLabel(r.label, micro), mrow, 0, 1, 2);
        QDoubleSpinBox* spin = pileSpin(*r.target, micro);
        mg->addWidget(spin, mrow, 2);
        if (!r.unitText.isEmpty()) mg->addWidget(new QLabel(r.unitText, micro), mrow, 3);
        auto* arrow = new QToolButton(micro);
        arrow->setIcon(themedIcon(QStringLiteral("arrow_left_blue")));
        arrow->setToolTip(tr("Valor por defecto"));
        connect(arrow, &QToolButton::clicked, this, [spin, value = r.installationValue] { spin->setValue(value); });
        mg->addWidget(arrow, mrow, 4);
        ++mrow;
    }
    auto* mAdvanced = new QPushButton(tr("Configuración avanzada"), micro);
    connect(mAdvanced, &QPushButton::clicked, this, [this] {
        MicropileAdvancedDialog dialog(m_micropiles.advanced, this);
        if (dialog.exec() == QDialog::Accepted) m_micropiles.advanced = dialog.value();
    });
    mg->addWidget(mAdvanced, mrow, 0, 1, 5);
    mg->setRowStretch(mrow + 1, 1);
    tabs->addTab(micro, tr("Micropilotes"));

    layout->addWidget(tabs, 1);
}

void EmptyProjectWizard::refreshButtons() {
    m_environmentButton->setText(qs(m_environment.beamsSummary()));
    m_standardsButton->setText(qs(m_standards.summary()));
    m_timberButton->setText(qs(m_profiles.timber.summary()));
    m_aluminiumButton->setText(qs(m_profiles.aluminium.summary()));
    m_rebarButton->setText(qs(m_concrete.rebarSteel.summary()));
    m_aggregateButton->setText(qs(m_concrete.aggregate.summary()));
}

}  // namespace cc::app
