#include "WizardSubDialogs.h"

#include "OptionsDialogs.h"

#include <QButtonGroup>
#include <QEvent>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QLineEdit>
#include <QListWidget>
#include <QStackedWidget>
#include <QTabBar>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QRadioButton>
#include <QTabWidget>
#include <QTableWidget>
#include <QToolButton>
#include <QVBoxLayout>

namespace cc::app {

using namespace cc::core;

namespace {

QComboBox* makeCombo(const std::vector<std::string>& items, const std::string& current, QWidget* parent) {
    auto* combo = new QComboBox(parent);
    for (const std::string& item : items) combo->addItem(QString::fromStdString(item));
    combo->setCurrentText(QString::fromStdString(current));
    return combo;
}

QDoubleSpinBox* makeSpin(double value, double min, double max, int decimals, QWidget* parent) {
    auto* spin = new QDoubleSpinBox(parent);
    spin->setButtonSymbols(QAbstractSpinBox::NoButtons);
    spin->setDecimals(decimals);
    spin->setRange(min, max);
    spin->setValue(value);
    spin->setAlignment(Qt::AlignRight);
    spin->setLocale(QLocale::c());  // the original shows a decimal point (0.10)
    spin->setFixedWidth(64);
    return spin;
}

/// Radio buttons for `items` laid out in `layout`; the button ids are the indexes into `items`.
QButtonGroup* addRadios(const std::vector<std::string>& items, const std::string& current, QVBoxLayout* layout,
                        QWidget* parent) {
    auto* group = new QButtonGroup(parent);
    for (std::size_t i = 0; i < items.size(); ++i) {
        auto* radio = new QRadioButton(QString::fromStdString(items[i]), parent);
        radio->setChecked(items[i] == current);
        group->addButton(radio, static_cast<int>(i));
        layout->addWidget(radio);
    }
    if (!group->checkedButton() && group->button(0)) group->button(0)->setChecked(true);
    return group;
}

std::string checkedText(const QButtonGroup* group) { return group->checkedButton()->text().toStdString(); }

/// Original schematic of a reinforced section with cover arrows (not copied from the CYPE application).
class SectionDiagram : public QWidget {
public:
    explicit SectionDiagram(QWidget* parent) : QWidget(parent) { setFixedSize(300, 220); }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.fillRect(rect(), Qt::white);
        const QRect outer(30, 20, 240, 180);
        p.setPen(QPen(Qt::black, 3));
        p.drawRect(outer);
        const QRect stirrup = outer.adjusted(26, 26, -26, -26);
        p.setPen(QPen(QColor(0, 0, 160), 1.5));
        p.drawRoundedRect(stirrup, 10, 10);
        p.setBrush(Qt::white);
        p.setPen(QPen(Qt::black, 1.5));
        for (const QPoint c : {stirrup.topLeft(), stirrup.topRight(), stirrup.bottomLeft(), stirrup.bottomRight()}) {
            p.drawEllipse(c + QPoint(c.x() < stirrup.center().x() ? 12 : -12, c.y() < stirrup.center().y() ? 12 : -12), 9, 9);
        }
        p.setPen(QPen(Qt::red, 1.2));
        p.drawLine(outer.center().x(), outer.top(), outer.center().x(), stirrup.top());
        p.drawLine(outer.center().x(), stirrup.bottom(), outer.center().x(), outer.bottom());
        p.drawLine(outer.left(), outer.center().y(), stirrup.left(), outer.center().y());
    }
};

}  // namespace

// ---------------------------------------------------------------- Madera

TimberDialog::TimberDialog(const TimberSettings& initial, QWidget* parent) : CypeDialog(tr("Madera"), parent) {
    setFixedSize(492, 285);

    auto* material = new QGroupBox(tr("Material"), this);
    auto* materialLayout = new QVBoxLayout(material);
    m_material = makeCombo(timberMaterials(), initial.material, material);
    materialLayout->addWidget(m_material);
    auto* strengthRow = new QHBoxLayout;
    strengthRow->addWidget(new QLabel(tr("Clase resistente"), material));
    m_strength = makeCombo(timberStrengthClasses(), initial.strengthClass, material);
    strengthRow->addWidget(m_strength);
    strengthRow->addStretch(1);
    materialLayout->addLayout(strengthRow);
    contentLayout()->addWidget(material);

    auto* service = new QGroupBox(tr("Clase de servicio"), this);
    auto* serviceLayout = new QVBoxLayout(service);
    auto* radios = new QHBoxLayout;
    m_service = new QButtonGroup(this);
    for (int i = 1; i <= 3; ++i) {
        auto* radio = new QRadioButton(QString::number(i), service);
        radio->setChecked(i == initial.serviceClass);
        m_service->addButton(radio, i);
        radios->addWidget(radio);
    }
    radios->addStretch(1);
    serviceLayout->addLayout(radios);
    m_serviceText = new QLabel(service);
    m_serviceText->setWordWrap(true);
    m_serviceText->setStyleSheet(QStringLiteral("background: #D6F1FA; border: 1px solid #9ED3E6; padding: 6px;"));
    m_serviceText->setMinimumHeight(64);
    m_serviceText->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    serviceLayout->addWidget(m_serviceText);
    contentLayout()->addWidget(service);
    contentLayout()->addStretch(1);

    connect(m_service, &QButtonGroup::idToggled, this, [this](int, bool on) { if (on) updateServiceText(); });
    updateServiceText();
    setupFooter(tr("Aceptar"), tr("Cancelar"));
}

void TimberDialog::updateServiceText() {
    switch (m_service->checkedId()) {
    case 1:
        m_serviceText->setText(tr("Clase de servicio 1: se caracteriza por un contenido de humedad en los materiales "
                                  "correspondiente a una temperatura de 20°C y una humedad relativa del aire que sólo "
                                  "exceda el 65% unas pocas semanas al año."));
        break;
    default:
        // Texts of classes 2 and 3 were not captured from the original (ticket TK-011).
        m_serviceText->setText(tr("Clase de servicio %1: descripción pendiente de verificar.").arg(m_service->checkedId()));
    }
}

TimberSettings TimberDialog::value() const {
    TimberSettings result;
    result.material = m_material->currentText().toStdString();
    result.strengthClass = m_strength->currentText().toStdString();
    result.serviceClass = m_service->checkedId();
    return result;
}

// ---------------------------------------------------------------- Aluminio

AluminiumDialog::AluminiumDialog(const AluminiumSettings& initial, QWidget* parent) : CypeDialog(tr("Aluminio"), parent) {
    setFixedSize(308, 321);
    auto* alloy = new QGroupBox(tr("Aleación"), this);
    auto* alloyLayout = new QVBoxLayout(alloy);
    m_alloys = addRadios(aluminiumAlloys(), initial.alloy, alloyLayout, alloy);
    contentLayout()->addWidget(alloy);

    auto* temper = new QGroupBox(tr("Estado de entrega"), this);
    auto* temperLayout = new QVBoxLayout(temper);
    m_tempers = addRadios(aluminiumTempers(), initial.temper, temperLayout, temper);
    contentLayout()->addWidget(temper);
    contentLayout()->addStretch(1);
    setupFooter(tr("Aceptar"), tr("Cancelar"));
}

AluminiumSettings AluminiumDialog::value() const {
    AluminiumSettings result;
    result.alloy = checkedText(m_alloys);
    result.temper = checkedText(m_tempers);
    return result;
}

// ---------------------------------------------------------------- fy

SteelLibraryDialog::SteelLibraryDialog(QWidget* parent) : CypeDialog(tr("Biblioteca de aceros de usuario"), parent) {
    setFixedSize(478, 245);
    auto* row = new QHBoxLayout;
    for (const QString& title : {tr("Acero laminado"), tr("Acero conformado")}) {
        auto* group = new QGroupBox(title, this);
        auto* layout = new QVBoxLayout(group);
        auto* table = new QTableWidget(0, 3, group);
        table->setHorizontalHeaderLabels({tr("Tipo"), tr("Referencia"), tr("Exportar")});
        table->horizontalHeader()->setStretchLastSection(true);
        table->verticalHeader()->hide();
        table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        layout->addWidget(table);
        row->addWidget(group);
    }
    contentLayout()->addLayout(row);
    setupFooter(tr("Aceptar"), tr("Cancelar"));
}

// ---------------------------------------------------------------- Acero de barras

RebarSteelDialog::RebarSteelDialog(const RebarSteelSettings& initial, QWidget* parent)
    : CypeDialog(tr("Acero de barras"), parent) {
    setFixedSize(277, 480);
    struct Item {
        QString label;
        const std::string* value;
    };
    struct Group {
        QString title;
        std::vector<Item> items;
    };
    const std::vector<Group> groups = {
        {tr("Pilares"), {{tr("Armadura longitudinal"), &initial.columnLongitudinal}, {tr("Armadura transversal"), &initial.columnTransverse}}},
        {tr("Vigas"),
         {{tr("Armadura superior de montaje"), &initial.beamTopAssembly},
          {tr("Armadura superior de refuerzo"), &initial.beamTopReinforcement},
          {tr("Armadura inferior"), &initial.beamBottom},
          {tr("Armadura transversal"), &initial.beamTransverse},
          {tr("Armadura de piel"), &initial.beamSkin}}},
        {tr("Cimentación"),
         {{tr("Zapatas"), &initial.footings}, {tr("Encepados"), &initial.pileCaps}, {tr("Vigas centradoras y de atado"), &initial.tieBeams}}}};
    for (const Group& group : groups) {
        auto* box = new QGroupBox(group.title, this);
        auto* grid = new QGridLayout(box);
        int r = 0;
        for (const Item& item : group.items) {
            grid->addWidget(new QLabel(item.label, box), r, 0);
            auto* combo = makeCombo(rebarSteels(), *item.value, box);
            m_combos.push_back(combo);
            grid->addWidget(combo, r, 1);
            ++r;
        }
        contentLayout()->addWidget(box);
    }
    contentLayout()->addStretch(1);
    setupFooter(tr("Aceptar"), tr("Cancelar"));
}

RebarSteelSettings RebarSteelDialog::value() const {
    RebarSteelSettings s;
    std::string* fields[] = {&s.columnLongitudinal, &s.columnTransverse, &s.beamTopAssembly, &s.beamTopReinforcement,
                             &s.beamBottom,         &s.beamTransverse,   &s.beamSkin,        &s.footings,
                             &s.pileCaps,           &s.tieBeams};
    for (std::size_t i = 0; i < m_combos.size(); ++i) *fields[i] = m_combos[i]->currentText().toStdString();
    return s;
}

// ---------------------------------------------------------------- Árido

AggregateDialog::AggregateDialog(const AggregateSettings& initial, QWidget* parent)
    : CypeDialog(tr("Características del árido"), parent) {
    setFixedSize(241, 397);
    const AggregateSetting* sources[] = {&initial.columns, &initial.beams, &initial.foundationBeams};
    const QString titles[] = {tr("Pilares"), tr("Vigas de forjado"), tr("Vigas de cimentación")};
    for (int i = 0; i < 3; ++i) {
        auto* box = new QGroupBox(titles[i], this);
        auto* grid = new QGridLayout(box);
        grid->addWidget(new QLabel(tr("Naturaleza"), box), 0, 0);
        m_kind[i] = makeCombo(aggregateKinds(), sources[i]->kind, box);
        grid->addWidget(m_kind[i], 0, 1);
        grid->addWidget(new QLabel(tr("Tamaño máximo"), box), 1, 0);
        m_size[i] = makeCombo(aggregateMaxSizes(), sources[i]->maxSize, box);
        grid->addWidget(m_size[i], 1, 1);
        contentLayout()->addWidget(box);
    }
    auto* foundation = new QGroupBox(tr("Cimentación"), this);
    auto* row = new QHBoxLayout(foundation);
    row->addWidget(new QLabel(tr("Tamaño máximo de árido"), foundation));
    m_foundationSize = makeSpin(initial.foundationMaxSizeMm, 1.0, 100.0, 0, foundation);
    m_foundationSize->setSuffix(tr(" mm"));
    row->addWidget(m_foundationSize);
    contentLayout()->addWidget(foundation);
    contentLayout()->addStretch(1);
    setupFooter(tr("Aceptar"), tr("Cancelar"));
}

AggregateSettings AggregateDialog::value() const {
    AggregateSettings s;
    AggregateSetting* targets[] = {&s.columns, &s.beams, &s.foundationBeams};
    for (int i = 0; i < 3; ++i) {
        targets[i]->kind = m_kind[i]->currentText().toStdString();
        targets[i]->maxSize = m_size[i]->currentText().toStdString();
    }
    s.foundationMaxSizeMm = m_foundationSize->value();
    return s;
}

// ---------------------------------------------------------------- Defaults footer

void DefaultsDialog::setupDefaultsFooter() {
    m_saveDefault = new QCheckBox(tr("Grabar como opciones por defecto"), this);
    auto* checkRow = new QHBoxLayout;
    checkRow->addStretch(1);
    checkRow->addWidget(m_saveDefault);
    checkRow->addStretch(1);
    contentLayout()->addLayout(checkRow);

    QHBoxLayout* footer = setupCustomFooter();
    auto* ok = new QPushButton(tr("Aceptar"), this);
    ok->setDefault(true);
    auto* reset = new QPushButton(tr("Valores de instalación"), this);
    auto* cancel = new QPushButton(tr("Cancelar"), this);
    footer->addWidget(ok);
    footer->addStretch(1);
    footer->addWidget(reset);
    footer->addStretch(1);
    footer->addWidget(cancel);
    connect(ok, &QPushButton::clicked, this, &QDialog::accept);
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(reset, &QPushButton::clicked, this, [this] { resetToInstallationValues(); });
}

bool DefaultsDialog::saveAsDefault() const { return m_saveDefault && m_saveDefault->isChecked(); }

// ---------------------------------------------------------------- Recubrimientos

CoverMenuDialog::CoverMenuDialog(const CoverSettings& initial, QWidget* parent)
    : CypeDialog(tr("Recubrimientos"), parent), m_covers(initial) {
    setFixedSize(180, 191);
    const std::pair<QString, CoverDialog::Kind> entries[] = {{tr("Pilares"), CoverDialog::Kind::Columns},
                                                             {tr("Vigas"), CoverDialog::Kind::Beams},
                                                             {tr("Cimentación"), CoverDialog::Kind::Foundation}};
    for (const auto& [text, kind] : entries) {
        auto* button = new QPushButton(text, this);
        contentLayout()->addWidget(button);
        connect(button, &QPushButton::clicked, this, [this, kind] {
            CoverDialog dialog(kind, m_covers, this);
            if (dialog.exec() == QDialog::Accepted) m_covers = dialog.value();
        });
    }
    contentLayout()->addStretch(1);
}

CoverDialog::CoverDialog(Kind kind, const CoverSettings& initial, QWidget* parent)
    : DefaultsDialog(kind == Kind::Columns ? tr("Pilares") : kind == Kind::Beams ? tr("Vigas") : tr("Cimentación"), parent),
      m_kind(kind),
      m_base(initial) {
    const auto spin = [this](double v, int decimals) {
        auto* s = makeSpin(v, CoverSettings::kMin, CoverSettings::kMax, decimals, this);
        m_spins.push_back(s);
        return s;
    };
    const auto addRow = [this](QGridLayout* grid, int row, const QString& label, QDoubleSpinBox* s) {
        grid->addWidget(new QLabel(label, this), row, 0);
        grid->addWidget(s, row, 1);
        grid->addWidget(new QLabel(tr("cm"), this), row, 2);
    };
    auto* grid = new QGridLayout;
    grid->setColumnStretch(0, 1);
    if (kind == Kind::Columns) {
        setFixedSize(404, 384);
        addRow(grid, 0, tr("Recubrimiento"), spin(initial.columns, 1));
        auto* note = new QLabel(tr("Distancia de la cara del pilar a la cara de la barra del estribo."), this);
        note->setWordWrap(true);
        grid->addWidget(note, 1, 0, 1, 3);
    } else if (kind == Kind::Beams) {
        setFixedSize(398, 468);
        addRow(grid, 0, tr("Recubrimiento superior"), spin(initial.beamTop, 1));
        addRow(grid, 1, tr("Recubrimiento inferior"), spin(initial.beamBottom, 1));
        addRow(grid, 2, tr("Recubrimiento lateral"), spin(initial.beamSide, 1));
    } else {
        setFixedSize(398, 350);
        grid->addWidget(new QLabel(tr("<b>Zapatas y encepados</b>"), this), 0, 0, 1, 3);
        addRow(grid, 1, tr("Recubrimiento superior"), spin(initial.footingTop, 2));
        addRow(grid, 2, tr("Recubrimiento inferior"), spin(initial.footingBottom, 2));
        addRow(grid, 3, tr("Recubrimiento lateral"), spin(initial.footingSide, 2));
        grid->addWidget(new QLabel(tr("<b>Vigas centradoras y de atado</b>"), this), 4, 0, 1, 3);
        addRow(grid, 5, tr("Recubrimiento superior"), spin(initial.tieBeamTop, 2));
        addRow(grid, 6, tr("Recubrimiento inferior"), spin(initial.tieBeamBottom, 2));
        addRow(grid, 7, tr("Recubrimiento lateral"), spin(initial.tieBeamSide, 2));
    }
    contentLayout()->addLayout(grid);
    if (kind != Kind::Foundation) {
        auto* diagram = new SectionDiagram(this);
        contentLayout()->addWidget(diagram, 0, Qt::AlignHCenter);
    }
    contentLayout()->addStretch(1);
    setupDefaultsFooter();
    m_spins.front()->selectAll();
    m_spins.front()->setFocus();
}

CoverSettings CoverDialog::value() const {
    CoverSettings s = m_base;
    const auto v = [this](std::size_t i) { return m_spins[i]->value(); };
    switch (m_kind) {
    case Kind::Columns: s.columns = v(0); break;
    case Kind::Beams: s.beamTop = v(0); s.beamBottom = v(1); s.beamSide = v(2); break;
    case Kind::Foundation:
        s.footingTop = v(0); s.footingBottom = v(1); s.footingSide = v(2);
        s.tieBeamTop = v(3); s.tieBeamBottom = v(4); s.tieBeamSide = v(5);
        break;
    }
    return s;
}

void CoverDialog::resetToInstallationValues() {
    CoverSettings fresh = value();
    const CoverSettings installation;
    switch (m_kind) {
    case Kind::Columns: fresh.columns = installation.columns; break;
    case Kind::Beams: fresh.beamTop = installation.beamTop; fresh.beamBottom = installation.beamBottom; fresh.beamSide = installation.beamSide; break;
    case Kind::Foundation:
        fresh.footingTop = installation.footingTop; fresh.footingBottom = installation.footingBottom; fresh.footingSide = installation.footingSide;
        fresh.tieBeamTop = installation.tieBeamTop; fresh.tieBeamBottom = installation.tieBeamBottom; fresh.tieBeamSide = installation.tieBeamSide;
        break;
    }
    load(fresh);
}

void CoverDialog::load(const CoverSettings& s) {
    std::vector<double> values;
    switch (m_kind) {
    case Kind::Columns: values = {s.columns}; break;
    case Kind::Beams: values = {s.beamTop, s.beamBottom, s.beamSide}; break;
    case Kind::Foundation: values = {s.footingTop, s.footingBottom, s.footingSide, s.tieBeamTop, s.tieBeamBottom, s.tieBeamSide}; break;
    }
    for (std::size_t i = 0; i < values.size(); ++i) m_spins[i]->setValue(values[i]);
}

// ---------------------------------------------------------------- Mermas

WasteDialog::WasteDialog(const WasteSettings& initial, QWidget* parent) : DefaultsDialog(tr("Mermas de acero"), parent) {
    setFixedSize(398, 266);
    const std::pair<QString, double> rows[] = {{tr("Pilares"), initial.columns},
                                               {tr("Vigas"), initial.beams},
                                               {tr("Zapatas"), initial.footings},
                                               {tr("Encepados"), initial.pileCaps},
                                               {tr("Vigas centradoras y de atado"), initial.tieBeams}};
    auto* grid = new QGridLayout;
    grid->setColumnStretch(0, 1);
    int r = 0;
    for (const auto& [label, v] : rows) {
        auto* spin = makeSpin(v, WasteSettings::kMin, WasteSettings::kMax, 2, this);
        m_spins.push_back(spin);
        grid->addWidget(new QLabel(label, this), r, 0);
        grid->addWidget(spin, r, 1);
        grid->addWidget(new QLabel(QStringLiteral("%"), this), r, 2);
        ++r;
    }
    contentLayout()->addLayout(grid);
    contentLayout()->addStretch(1);
    setupDefaultsFooter();
    m_spins.front()->selectAll();
    m_spins.front()->setFocus();
}

WasteSettings WasteDialog::value() const {
    WasteSettings s;
    s.columns = m_spins[0]->value();
    s.beams = m_spins[1]->value();
    s.footings = m_spins[2]->value();
    s.pileCaps = m_spins[3]->value();
    s.tieBeams = m_spins[4]->value();
    return s;
}

void WasteDialog::resetToInstallationValues() {
    const WasteSettings installation;
    const double values[] = {installation.columns, installation.beams, installation.footings, installation.pileCaps,
                             installation.tieBeams};
    for (std::size_t i = 0; i < m_spins.size(); ++i) m_spins[i]->setValue(values[i]);
}

// ---------------------------------------------------------------- Resistencia al fuego

FireDialog::FireDialog(SteelFireMode initial, QWidget* parent) : CypeDialog(tr("Resistencia al fuego"), parent) {
    setFixedSize(598, 335);
    auto* tabs = new QTabWidget(this);
    auto* steel = new QWidget(tabs);
    auto* layout = new QVBoxLayout(steel);
    m_modes = new QButtonGroup(this);
    const QString texts[] = {tr("No considerar la situación de incendio"),
                             tr("Comprobar la resistencia al fuego (Código Estructural)"),
                             tr("Calcular la temperatura crítica en la situación de incendio")};
    for (int i = 0; i < 3; ++i) {
        auto* radio = new QRadioButton(texts[i], steel);
        radio->setChecked(i == static_cast<int>(initial));
        m_modes->addButton(radio, i);
        layout->addWidget(radio);
    }
    layout->addStretch(1);
    tabs->addTab(steel, tr("Acero"));
    auto* timber = new QLabel(tr("Contenido pendiente de verificar."), tabs);
    timber->setAlignment(Qt::AlignCenter);
    tabs->addTab(timber, tr("Madera"));
    contentLayout()->addWidget(tabs, 1);
    setupFooter(tr("Aceptar"), tr("Cancelar"));
}

SteelFireMode FireDialog::value() const { return static_cast<SteelFireMode>(m_modes->checkedId()); }

// ---------------------------------------------------------------- Hipótesis adicionales

AdditionalLoadsDialog::AdditionalLoadsDialog(QWidget* parent) : CypeDialog(tr("Hipótesis adicionales"), parent) {
    setFixedSize(280, 443);
    auto* uses = new QGroupBox(tr("Categorías de uso"), this);
    auto* usesLayout = new QHBoxLayout(uses);
    usesLayout->addWidget(new QLabel(tr("No hay categorías de uso definidas"), uses), 1);
    auto* editUses = new QToolButton(uses);
    editUses->setText(QStringLiteral("…"));
    editUses->setEnabled(false);
    usesLayout->addWidget(editUses);
    contentLayout()->addWidget(uses);

    auto* actions = new QGroupBox(tr("Acciones"), this);
    auto* grid = new QGridLayout(actions);
    grid->addWidget(new QLabel(tr("<b>Automáticas</b>"), actions), 0, 1, Qt::AlignRight);
    grid->addWidget(new QLabel(tr("<b>Adicionales</b>"), actions), 0, 2, Qt::AlignRight);
    struct Row {
        QString name;
        QString automatic;
        QString additional;
    };
    const Row rows[] = {{tr("Peso propio"), QStringLiteral("1"), QStringLiteral("---")},
                        {tr("Cargas muertas"), QStringLiteral("-"), QStringLiteral("0")},
                        {tr("Sobrecarga de uso"), QStringLiteral("-"), QStringLiteral("0")},
                        {tr("Temperatura"), QStringLiteral("-"), QStringLiteral("0")},
                        {tr("Retracción"), QStringLiteral("-"), QStringLiteral("0")},
                        {tr("Viento"), QStringLiteral("-"), QStringLiteral("0")},
                        {tr("Sismo"), QStringLiteral("-"), QStringLiteral("0")},
                        {tr("Nieve"), QStringLiteral("-"), QStringLiteral("0")},
                        {tr("Empujes del terreno"), QStringLiteral("-"), QStringLiteral("0")},
                        {tr("Accidental"), QStringLiteral("-"), QStringLiteral("0")}};
    int r = 1;
    for (const Row& row : rows) {
        grid->addWidget(new QLabel(row.name, actions), r, 0);
        grid->addWidget(new QLabel(row.automatic, actions), r, 1, Qt::AlignRight);
        grid->addWidget(new QLabel(row.additional, actions), r, 2, Qt::AlignRight);
        if (r > 1) {
            auto* edit = new QToolButton(actions);
            edit->setText(QStringLiteral("…"));
            edit->setEnabled(false);
            edit->setToolTip(tr("La edición de hipótesis estará disponible con el módulo de cargas."));
            grid->addWidget(edit, r, 3);
        }
        ++r;
    }
    grid->setColumnStretch(0, 1);
    contentLayout()->addWidget(actions);
    contentLayout()->addStretch(1);
    setupFooter(tr("Aceptar"), tr("Cancelar"));
}

// ---------------------------------------------------------------- Cimentación (acciones)

FoundationActionsDialog::FoundationActionsDialog(bool wind, bool seism, QWidget* parent)
    : CypeDialog(tr("Cimentación"), parent) {
    setFixedSize(253, 136);
    m_wind = new QCheckBox(tr("Considerar combinaciones con viento"), this);
    m_wind->setChecked(wind);
    m_seism = new QCheckBox(tr("Considerar combinaciones con sismo"), this);
    m_seism->setChecked(seism);
    contentLayout()->addWidget(m_wind);
    contentLayout()->addWidget(m_seism);
    contentLayout()->addStretch(1);
    setupFooter(tr("Aceptar"), tr("Cancelar"));
}

bool FoundationActionsDialog::wind() const { return m_wind->isChecked(); }
bool FoundationActionsDialog::seism() const { return m_seism->isChecked(); }

// ---------------------------------------------------------------- Proceso constructivo

ConstructionProcessDialog::ConstructionProcessDialog(double days, QWidget* parent)
    : DefaultsDialog(tr("Proceso constructivo"), parent) {
    setFixedSize(398, 152);
    auto* row = new QHBoxLayout;
    row->addStretch(1);
    row->addWidget(new QLabel(tr("<b>Instante de descimbrado</b>"), this));
    m_days = makeSpin(days, ActionsSettings::kMinDays, ActionsSettings::kMaxDays, 0, this);
    row->addWidget(m_days);
    auto* unit = new QComboBox(this);
    unit->addItem(tr("días"));
    row->addWidget(unit);
    row->addStretch(1);
    contentLayout()->addLayout(row);
    contentLayout()->addStretch(1);
    setupDefaultsFooter();
    m_days->selectAll();
    m_days->setFocus();
}

double ConstructionProcessDialog::days() const { return m_days->value(); }
void ConstructionProcessDialog::resetToInstallationValues() { m_days->setValue(ActionsSettings{}.formworkRemovalDays); }

// ---------------------------------------------------------------- Ambiente

BeamEnvironmentDialog::BeamEnvironmentDialog(const EnvironmentSettings& initial, QWidget* parent)
    : CypeDialog(tr("Vigas"), parent), m_base(initial) {
    setFixedSize(442, 385);
    auto* exposure = new QGroupBox(tr("Clase de exposición"), this);
    auto* layout = new QVBoxLayout(exposure);
    m_classes = new QButtonGroup(this);
    const QString names[] = {tr("Sin riesgo de ataque por corrosión"), tr("Corrosión provocada por carbonatación"),
                             tr("Corrosión provocada por cloruros"), tr("Corrosión inducida por cloruros de agua marina"),
                             tr("Ataque por congelación y deshielo"), tr("Ataque químico")};
    for (int i = 0; i < 6; ++i) {
        auto* radio = new QRadioButton(names[i], exposure);
        radio->setChecked(i == static_cast<int>(initial.beams));
        radio->setEnabled(i == 0);  // other classes: sub-options not captured yet (TK-011)
        m_classes->addButton(radio, i);
        layout->addWidget(radio);
    }
    contentLayout()->addWidget(exposure);

    auto* designation = new QGroupBox(tr("Designación"), this);
    auto* designationLayout = new QVBoxLayout(designation);
    auto* x0 = new QRadioButton(QStringLiteral("X0"), designation);
    x0->setChecked(true);
    designationLayout->addWidget(x0);
    contentLayout()->addWidget(designation);

    auto* cracking = new QGroupBox(tr("Fisuración"), this);
    auto* crackingLayout = new QHBoxLayout(cracking);
    crackingLayout->addWidget(new QLabel(tr("Abertura máxima de fisura"), cracking));
    auto* width = new QLabel(QString::number(initial.maxCrackWidthMm, 'f', 2) + tr(" mm"), cracking);
    crackingLayout->addWidget(width);
    crackingLayout->addStretch(1);
    contentLayout()->addWidget(cracking);
    contentLayout()->addStretch(1);
    setupFooter(tr("Aceptar"), tr("Cancelar"));
}

EnvironmentSettings BeamEnvironmentDialog::value() const {
    EnvironmentSettings s = m_base;
    s.beams = static_cast<ExposureClass>(m_classes->checkedId());
    return s;
}

// ---------------------------------------------------------------- Valores usuales del terreno

UsualGroundDialog::UsualGroundDialog(QWidget* parent) : CypeDialog(tr("Importar valores usuales de proyecto"), parent) {
    setFixedSize(530, 296);
    const auto& rows = usualGroundValues();
    m_table = new QTableWidget(static_cast<int>(rows.size()), 3, this);
    m_table->setHorizontalHeaderLabels({tr("Tipo de terreno"), tr("S1 (C) (MPa)"), tr("S2 (C) (MPa)")});
    m_table->verticalHeader()->hide();
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    for (int r = 0; r < static_cast<int>(rows.size()); ++r) {
        m_table->setItem(r, 0, new QTableWidgetItem(tr(rows[r].type)));
        m_table->setItem(r, 1, new QTableWidgetItem(QString::number(rows[r].persistent, 'f', 3)));
        m_table->setItem(r, 2, new QTableWidgetItem(QString::number(rows[r].seismic, 'f', 3)));
    }
    contentLayout()->addWidget(m_table);
    setupFooter(tr("Aceptar"), tr("Cancelar"));
}

int UsualGroundDialog::selectedIndex() const { return m_table->currentRow(); }

// ---------------------------------------------------------------- Información

InfoDialog::InfoDialog(const QString& title, const QString& text, QWidget* parent) : CypeDialog(title, parent) {
    setFixedSize(650, 218);
    auto* row = new QHBoxLayout;
    auto* icon = new QLabel(QStringLiteral("i"), this);
    icon->setFixedSize(34, 34);
    icon->setAlignment(Qt::AlignCenter);
    icon->setStyleSheet(QStringLiteral("background: #1E5AA8; color: white; border-radius: 17px; font-size: 20px; font-weight: bold;"));
    row->addWidget(icon, 0, Qt::AlignTop);
    auto* label = new QLabel(text, this);
    label->setWordWrap(true);
    row->addWidget(label, 1);
    contentLayout()->addLayout(row);
    contentLayout()->addStretch(1);
    QHBoxLayout* footer = setupCustomFooter();
    auto* ok = new QPushButton(tr("Aceptar"), this);
    ok->setDefault(true);
    footer->addStretch(1);
    footer->addWidget(ok);
    footer->addStretch(1);
    connect(ok, &QPushButton::clicked, this, &QDialog::accept);
}

// ---------------------------------------------------------------- Pilotes: Configuración avanzada

namespace {

/// Blue round "i" badge used by the original next to explanatory options.
QLabel* makeInfoBadge(QWidget* parent) {
    auto* badge = new QLabel(QStringLiteral("i"), parent);
    badge->setFixedSize(18, 18);
    badge->setAlignment(Qt::AlignCenter);
    badge->setStyleSheet(QStringLiteral("background: #1E88E5; color: white; border-radius: 9px; font-weight: bold;"));
    return badge;
}

}  // namespace

QWidget* PileAdvancedBase::buildSinkingTab(bool micro, QTabWidget* tabs) {
    auto* sinking = new QWidget(tabs);
    auto* grid = new QGridLayout(sinking);
    grid->setColumnStretch(0, 1);
    int row = 0;
    const auto addRow = [&](const QString& label, QWidget* field, int indent) {
        auto* text = new QLabel(label, sinking);
        text->setContentsMargins(indent, 0, 0, 0);
        grid->addWidget(text, row, 0);
        grid->addWidget(field, row, 1);
        ++row;
    };
    const auto addTitle = [&](const QString& label, int indent) {
        auto* text = new QLabel(label, sinking);
        text->setContentsMargins(indent, 6, 0, 0);
        grid->addWidget(text, row++, 0, 1, 2);
    };
    const auto addSpin = [&](const QString& label, double PileSinkingSettings::*field) {
        QDoubleSpinBox* spin = makeSpin(0.0, 0.0, 100.0, 2, sinking);
        m_sinkingSpins.emplace_back(spin, field);
        addRow(label, spin, 16);
    };

    m_resistance = new QComboBox(sinking);
    m_resistance->addItem(tr("Automático"));  // the only entry seen in the original
    addRow(tr("Cálculo de resistencia"), m_resistance, 0);
    addTitle(tr("A2 - Conjunto C"), 4);
    m_reference = new QLineEdit(sinking);
    m_reference->setReadOnly(true);
    m_reference->setAlignment(Qt::AlignRight);
    addRow(tr("Referencia"), m_reference, 16);

    addTitle(tr("Coeficientes parciales de seguridad para el terreno"), 16);
    addSpin(tr("Ángulo de rozamiento interno ϕ"), &PileSinkingSettings::frictionAngle);
    addSpin(tr("Cohesión c'"), &PileSinkingSettings::cohesion);
    addSpin(tr("Resistencia al corte sin drenaje Cu"), &PileSinkingSettings::undrainedShear);
    addSpin(tr("Resistencia a compresión simple qu"), &PileSinkingSettings::unconfinedCompression);
    addSpin(tr("Peso específico γ"), &PileSinkingSettings::unitWeight);
    addTitle(micro ? tr("Coeficientes parciales de seguridad para el micropilote (Parámetros geotécnicos)")
                   : tr("Coeficientes parciales de seguridad para el pilote (Parámetros geotécnicos)"),
             16);
    if (!micro) addSpin(tr("Punta γb"), &PileSinkingSettings::geotechnicalTip);
    addSpin(tr("Fuste (Compresión) γf,s"), &PileSinkingSettings::geotechnicalShaft);
    if (!micro) addSpin(tr("Total (Compresión) γt"), &PileSinkingSettings::geotechnicalTotal);
    addSpin(tr("Fuste (Tracción) γft,s"), &PileSinkingSettings::geotechnicalShaftTension);
    addTitle(micro ? tr("Coeficientes parciales de seguridad para el micropilote (Resistencias unitarias)")
                   : tr("Coeficientes parciales de seguridad para el pilote (Resistencias unitarias)"),
             16);
    addSpin(tr("Punta γb"), &PileSinkingSettings::unitTip);
    addSpin(tr("Fuste (Compresión) γf,s"), &PileSinkingSettings::unitShaft);
    addSpin(tr("Total (Compresión) γt"), &PileSinkingSettings::unitTotal);
    addSpin(tr("Fuste (Tracción) γft,s"), &PileSinkingSettings::unitShaftTension);

    m_efficiency = new QComboBox(sinking);
    m_efficiency->addItem(tr("Aplicar la fórmula de Converse-Labarre"));  // the only entry seen
    addRow(tr("Factor de eficiencia"), m_efficiency, 0);
    m_ignoreFar = new QCheckBox(
        micro ? tr("No considerar el factor de eficiencia si la distancia libre entre micropilotes es igual o superior a K·D")
              : tr("No considerar el factor de eficiencia si la distancia libre entre pilotes es igual o superior a K·D"),
        sinking);
    grid->addWidget(m_ignoreFar, row++, 0, 1, 2);
    auto* kRow = new QHBoxLayout;
    kRow->addSpacing(16);
    kRow->addWidget(new QLabel(tr("K"), sinking));
    QDoubleSpinBox* k = makeSpin(0.0, 0.0, 100.0, 2, sinking);
    m_sinkingSpins.emplace_back(k, &PileSinkingSettings::efficiencyK);
    kRow->addWidget(k);
    kRow->addStretch(1);
    grid->addLayout(kRow, row++, 0, 1, 2);
    grid->setRowStretch(row, 1);
    return sinking;
}

void PileAdvancedBase::loadSinking(const PileSinkingSettings& s) {
    m_resistance->setCurrentText(QString::fromStdString(s.resistanceCalculation));
    m_reference->setText(QString::fromStdString(s.reference));
    for (const auto& [spin, field] : m_sinkingSpins) spin->setValue(s.*field);
    m_efficiency->setCurrentText(QString::fromStdString(s.efficiencyFormula));
    m_ignoreFar->setChecked(s.ignoreEfficiencyWhenFar);
}

PileSinkingSettings PileAdvancedBase::sinkingValue(PileSinkingSettings base) const {
    base.resistanceCalculation = m_resistance->currentText().toStdString();
    base.reference = m_reference->text().toStdString();
    for (const auto& [spin, field] : m_sinkingSpins) base.*field = spin->value();
    base.efficiencyFormula = m_efficiency->currentText().toStdString();
    base.ignoreEfficiencyWhenFar = m_ignoreFar->isChecked();
    return base;
}

PileAdvancedDialog::StrengthRow PileAdvancedDialog::makeStrengthRow(QGridLayout* grid, int row, const QString& label,
                                                                    QWidget* page) {
    StrengthRow r;
    auto* text = new QLabel(label, page);
    text->setContentsMargins(26, 0, 0, 0);
    grid->addWidget(text, row, 0, 1, 2);
    r.basis = new QComboBox(page);
    // List order of the original: Absoluta, Relativa (orig_pilotes_ca_combo_precast).
    r.basis->addItem(tr("Absoluta"), static_cast<int>(StrengthBasis::Absolute));
    r.basis->addItem(tr("Relativa"), static_cast<int>(StrengthBasis::Relative));
    r.basis->setFixedWidth(84);
    grid->addWidget(r.basis, row, 2);
    r.value = makeSpin(0.0, 0.0, 1000.0, 2, page);
    grid->addWidget(r.value, row, 3);
    r.unit = new QLabel(page);
    r.unit->setMinimumWidth(32);
    grid->addWidget(r.unit, row, 4);
    connect(r.basis, &QComboBox::currentIndexChanged, this, [this] { updateStrengthUnits(); });
    return r;
}

namespace {
StrengthBasis basisOf(const QComboBox* combo) { return static_cast<StrengthBasis>(combo->currentData().toInt()); }
void setBasis(QComboBox* combo, StrengthBasis basis) { combo->setCurrentIndex(combo->findData(static_cast<int>(basis))); }
}  // namespace

PileAdvancedDialog::PileAdvancedDialog(const PileAdvancedSettings& initial, QWidget* parent)
    : PileAdvancedBase(tr("Configuración avanzada"), parent) {
    setFixedSize(556, 786);
    auto* tabs = new QTabWidget(this);
    contentLayout()->addWidget(tabs, 1);
    tabs->addTab(buildSinkingTab(false, tabs), tr("Hundimiento"));

    // --- Comprobación estructural (orig_pilotes_ca_comprobacion_estructural)
    auto* structural = new QWidget(tabs);
    auto* grid = new QGridLayout(structural);
    grid->setColumnStretch(1, 1);
    m_reduceStrength = new QCheckBox(tr("Considerar reducción de la resistencia del hormigón"), structural);
    grid->addWidget(m_reduceStrength, 0, 0, 1, 4);
    auto* info = makeInfoBadge(structural);
    info->setCursor(Qt::PointingHandCursor);
    info->installEventFilter(this);
    grid->addWidget(info, 0, 4);
    m_precast = makeStrengthRow(grid, 1, tr("Pilote prefabricado hincado"), structural);
    m_cast = makeStrengthRow(grid, 2, tr("Pilote hormigonado in situ"), structural);
    auto* group = new QLabel(tr("Coeficientes parciales de seguridad para el terreno (Curvas p-y)"), structural);
    group->setContentsMargins(0, 10, 0, 0);
    grid->addWidget(group, 3, 0, 1, 5);
    const auto addFactor = [&](int row, const QString& label, QDoubleSpinBox*& spin) {
        auto* text = new QLabel(label, structural);
        text->setContentsMargins(6, 0, 0, 0);
        grid->addWidget(text, row, 0, 1, 3);
        spin = makeSpin(0.0, 0.0, 100.0, 2, structural);
        grid->addWidget(spin, row, 3, 1, 2, Qt::AlignRight);
    };
    addFactor(4, tr("Resistencia al corte sin drenaje Cu"), m_cuFactor);
    addFactor(5, tr("Peso específico γ"), m_gammaFactor);
    grid->setRowStretch(6, 1);
    connect(m_reduceStrength, &QCheckBox::toggled, this, [this](bool) { updateStrengthUnits(); });
    tabs->addTab(structural, tr("Comprobación estructural"));

    // --- Tope estructural (orig_pilotes_ca_tope_marcado): the tab carries a tick that enables its two rows
    m_capPage = new QWidget(tabs);
    auto* capGrid = new QGridLayout(m_capPage);
    capGrid->setColumnStretch(1, 1);
    m_capPrecast = makeStrengthRow(capGrid, 0, tr("Pilote prefabricado hincado"), m_capPage);
    m_capCast = makeStrengthRow(capGrid, 1, tr("Pilote hormigonado in situ"), m_capPage);
    capGrid->setRowStretch(2, 1);
    tabs->addTab(m_capPage, tr("Tope estructural"));
    m_structuralCap = new QCheckBox(tabs->tabBar());
    tabs->tabBar()->setTabButton(2, QTabBar::LeftSide, m_structuralCap);
    connect(m_structuralCap, &QCheckBox::toggled, this, [this](bool) { updateStrengthUnits(); });

    setupDefaultsFooter();
    load(initial);
}

bool PileAdvancedDialog::eventFilter(QObject* watched, QEvent* event) {
    if (event->type() == QEvent::MouseButtonRelease && qobject_cast<QLabel*>(watched)) {
        // Text taken from orig_pilotes_ca_info.
        InfoDialog dialog(tr("Información"),
                          tr("Es recomendable considerar una reducción en la resistencia característica del hormigón por razones "
                             "constructivas relacionadas con la ejecución de los pilotes hormigonados in situ. Esta reducción se "
                             "considera en la comprobación de los estados límite de agotamiento tanto frente a solicitaciones "
                             "normales como de cortante."),
                          this);
        dialog.exec();
        return true;
    }
    return PileAdvancedBase::eventFilter(watched, event);
}

void PileAdvancedDialog::updateStrengthUnits() {
    const auto refresh = [this](const StrengthRow& row, bool on) {
        row.unit->setText(basisOf(row.basis) == StrengthBasis::Relative ? QStringLiteral("· fc") : tr("MPa"));
        row.basis->setEnabled(on);
        row.value->setEnabled(on);
    };
    refresh(m_precast, m_reduceStrength->isChecked());
    refresh(m_cast, m_reduceStrength->isChecked());
    refresh(m_capPrecast, m_structuralCap->isChecked());
    refresh(m_capCast, m_structuralCap->isChecked());
}

void PileAdvancedDialog::load(const PileAdvancedSettings& settings) {
    loadSinking(settings.sinking);
    const PileStructuralSettings& c = settings.structural;
    m_reduceStrength->setChecked(c.reduceConcreteStrength);
    setBasis(m_precast.basis, c.precastBasis);
    m_precast.value->setValue(c.precastValue);
    setBasis(m_cast.basis, c.castInSituBasis);
    m_cast.value->setValue(c.castInSituValue);
    m_cuFactor->setValue(c.undrainedShearFactor);
    m_gammaFactor->setValue(c.unitWeightFactor);
    setBasis(m_capPrecast.basis, settings.cap.precastBasis);
    m_capPrecast.value->setValue(settings.cap.precastValue);
    setBasis(m_capCast.basis, settings.cap.castInSituBasis);
    m_capCast.value->setValue(settings.cap.castInSituValue);
    m_structuralCap->setChecked(settings.structuralCap);
    updateStrengthUnits();
}

PileAdvancedSettings PileAdvancedDialog::value() const {
    PileAdvancedSettings out;
    out.sinking = sinkingValue(out.sinking);
    PileStructuralSettings& c = out.structural;
    c.reduceConcreteStrength = m_reduceStrength->isChecked();
    c.precastBasis = basisOf(m_precast.basis);
    c.precastValue = m_precast.value->value();
    c.castInSituBasis = basisOf(m_cast.basis);
    c.castInSituValue = m_cast.value->value();
    c.undrainedShearFactor = m_cuFactor->value();
    c.unitWeightFactor = m_gammaFactor->value();
    out.cap.precastBasis = basisOf(m_capPrecast.basis);
    out.cap.precastValue = m_capPrecast.value->value();
    out.cap.castInSituBasis = basisOf(m_capCast.basis);
    out.cap.castInSituValue = m_capCast.value->value();
    out.structuralCap = m_structuralCap->isChecked();
    return out;
}

void PileAdvancedDialog::resetToInstallationValues() { load(PileAdvancedSettings{}); }

MicropileAdvancedDialog::MicropileAdvancedDialog(const MicropileAdvancedSettings& initial, QWidget* parent)
    : PileAdvancedBase(tr("Configuración avanzada"), parent) {
    setFixedSize(587, 728);
    auto* tabs = new QTabWidget(this);
    contentLayout()->addWidget(tabs, 1);
    tabs->addTab(buildSinkingTab(true, tabs), tr("Hundimiento"));
    // --- Comprobación estructural (orig_micropilotes_ca_comprobacion_estructural)
    auto* page = new QWidget(tabs);
    auto* grid = new QGridLayout(page);
    auto* group = new QLabel(tr("Coeficientes parciales de seguridad para el terreno (Curvas p-y)"), page);
    grid->addWidget(group, 0, 0, 1, 2);
    const auto addFactor = [&](int row, const QString& label, QDoubleSpinBox*& spin) {
        auto* text = new QLabel(label, page);
        text->setContentsMargins(16, 0, 0, 0);
        grid->addWidget(text, row, 0);
        spin = makeSpin(0.0, 0.0, 100.0, 2, page);
        grid->addWidget(spin, row, 1, Qt::AlignRight);
    };
    addFactor(1, tr("Resistencia al corte sin drenaje Cu"), m_cuFactor);
    addFactor(2, tr("Peso específico γ"), m_gammaFactor);
    grid->setColumnStretch(0, 1);
    grid->setRowStretch(3, 1);
    tabs->addTab(page, tr("Comprobación estructural"));
    setupDefaultsFooter();
    loadSinking(initial.sinking);
    m_cuFactor->setValue(initial.undrainedShearFactor);
    m_gammaFactor->setValue(initial.unitWeightFactor);
}

MicropileAdvancedSettings MicropileAdvancedDialog::value() const {
    MicropileAdvancedSettings out;
    out.sinking = sinkingValue(out.sinking);
    out.undrainedShearFactor = m_cuFactor->value();
    out.unitWeightFactor = m_gammaFactor->value();
    return out;
}

void MicropileAdvancedDialog::resetToInstallationValues() {
    const MicropileAdvancedSettings installation;
    loadSinking(installation.sinking);
    m_cuFactor->setValue(installation.undrainedShearFactor);
    m_gammaFactor->setValue(installation.unitWeightFactor);
}

// ---------------------------------------------------------------- Opciones (menus)

OptionsMenuDialog::OptionsMenuDialog(const QString& title, const QSize& size, const std::vector<OptionsPage>& pages,
                                     ButtonHandler onButton, bool withCancel, QWidget* parent)
    : CypeDialog(title, parent) {
    setFixedSize(size);
    const QString note = tr("Diálogo pendiente de capturar en el original (ticket TK-015).");
    const auto fillPage = [&](QVBoxLayout* layout, const OptionsPage& page) {
        for (const QString& caption : page.buttons) {
            auto* button = new QPushButton(caption, this);
            if (page.enabledButtons.contains(caption)) {
                connect(button, &QPushButton::clicked, this, [onButton, caption] {
                    if (onButton) onButton(caption);
                });
            } else {
                button->setEnabled(false);
                button->setToolTip(note);
            }
            layout->addWidget(button);
        }
    };
    if (pages.size() == 1) {
        auto* layout = new QVBoxLayout;
        fillPage(layout, pages.front());
        layout->addStretch(1);
        contentLayout()->addLayout(layout, 1);
    } else {
        auto* row = new QHBoxLayout;
        auto* list = new QListWidget(this);
        list->setFixedWidth(227);
        list->setStyleSheet(QStringLiteral("QListWidget { background: #D9D9D9; border: none; } "
                                           "QListWidget::item { padding: 6px; } "
                                           "QListWidget::item:selected { background: #F0F0F0; color: black; }"));
        auto* stack = new QStackedWidget(this);
        for (const OptionsPage& page : pages) {
            list->addItem(page.title);
            auto* widget = new QWidget(stack);
            auto* layout = new QVBoxLayout(widget);
            layout->addStretch(1);
            fillPage(layout, page);
            layout->addStretch(1);
            stack->addWidget(widget);
        }
        connect(list, &QListWidget::currentRowChanged, stack, &QStackedWidget::setCurrentIndex);
        list->setCurrentRow(0);
        row->addWidget(list);
        row->addWidget(stack, 1);
        contentLayout()->addLayout(row, 1);
    }
    QHBoxLayout* footer = setupCustomFooter();
    auto* ok = new QPushButton(tr("Aceptar"), this);
    ok->setDefault(true);
    connect(ok, &QPushButton::clicked, this, &QDialog::accept);
    if (withCancel) {
        auto* cancel = new QPushButton(tr("Cancelar"), this);
        connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
        footer->addWidget(ok);
        footer->addStretch(1);
        footer->addWidget(cancel);
    } else {
        footer->addStretch(1);
        footer->addWidget(ok);
        footer->addStretch(1);
    }
}

OptionsMenuDialog* OptionsMenuDialog::create(Family family, OptionsSettings& settings, QWidget* parent) {
    switch (family) {
        case Family::Columns:
            return new OptionsMenuDialog(
                tr("Pilares"), QSize(575, 372),
                {{tr("Tablas de armado"), {tr("Tablas de armado")}, {}},
                 {tr("Dimensionamiento/Comprobación"),
                  {tr("Reducción de longitudes de anclaje en pilares"), tr("Criterios de simetría"),
                   tr("Criterios de continuidad"), tr("Pilares metálicos"),
                   tr("Ángulo de inclinación de las bielas de compresión"),
                   tr("Tolerancias en comprobaciones de separación"),
                   tr("Comprobación de capacidad en pilares, por flexión")},
                  {}},
                 {tr("Disposiciones de armado"),
                  {tr("Disposición de barras"), tr("Cortar esperas en último tramo"),
                   tr("Transiciones por cambio de dimensiones"), tr("Redondeo de longitud de barras"),
                   tr("Solapar en la zona central del tramo"), tr("Opciones de arranques"),
                   tr("Opciones para armado de estribos"), tr("Recubrimientos")},
                  {}},
                 {tr("Esfuerzos"),
                  {tr("Coeficiente de rigidez axil"), tr("Coeficiente de rigidez de torsión"),
                   tr("Coeficiente de rigidez a flexión")},
                  {}}},
                {}, false, parent);
        case Family::Foundation: {
            const QString options = tr("Opciones"), tables = tr("Tablas de armado");
            return new OptionsMenuDialog(
                tr("Cimentación"), QSize(150, 158), {{QString(), {options, tables}, {options, tables}}},
                [options, tables, parent](const QString& caption) {
                    // The leaf dialogs of these two menus were not opened in the original yet (TK-015).
                    if (caption == options) {
                        OptionsMenuDialog menu(
                            tr("Opciones"), QSize(207, 287),
                            {{QString(),
                              {tr("Hormigón de limpieza"), tr("Comprobación de fisuración"), tr("Zapatas aisladas"),
                               tr("Encepados"), tr("Vigas centradoras"), tr("Vigas de atado")},
                              {}}},
                            {}, false, parent);
                        menu.exec();
                    } else if (caption == tables) {
                        OptionsMenuDialog menu(
                            tr("Tablas de armado"), QSize(165, 155),
                            {{QString(), {tr("Zapatas aisladas"), tr("Vigas centradoras"), tr("Vigas de atado")}, {}}},
                            {}, false, parent);
                        menu.exec();
                    }
                },
                false, parent);
        }
        case Family::Beams: {
            const QString options = tr("Opciones"), errors = tr("Valoración de errores"), limits = tr("Límites de flecha"),
                          continuous = tr("Numeración de vigas continuas"), numbering = tr("Numeración de vigas");
            return new OptionsMenuDialog(
                tr("Vigas"), QSize(223, 257), {{QString(), {options, errors, limits, continuous, numbering},
                                                {options, errors, limits, continuous, numbering}}},
                [=, &settings](const QString& caption) {
                    BeamOptionsSettings& beams = settings.beams;
                    if (caption == options) {
                        // Pages 2-6 and the leaf dialogs of page 1 were not opened yet (TK-015).
                        OptionsMenuDialog menu(
                            tr("Opciones"), QSize(641, 537),
                            {{tr("Tablas de armado en vigas"),
                              {tr("Armadura de montaje en secciones rectangulares"),
                               tr("Armadura de montaje en secciones rectangulares con un ala"),
                               tr("Armadura de montaje en secciones rectangulares con dos alas"),
                               tr("Armadura transversal"), tr("Armadura portaestribos"), tr("Armadura de piel"),
                               tr("Armadura superior de refuerzo"), tr("Armadura inferior de refuerzo")},
                              {}},
                             {tr("Disposiciones de armado"), {}, {}},
                             {tr("Esfuerzos"), {}, {}},
                             {tr("Dimensionamiento / Comprobación"), {}, {}},
                             {tr("Vigas metálicas"), {}, {}},
                             {tr("Planos"), {}, {}}},
                            {}, true, parent);
                        menu.exec();
                    } else if (caption == errors) {
                        BeamErrorsDialog dialog(beams.errors, parent);
                        if (dialog.exec() == QDialog::Accepted) beams.errors = dialog.value();
                    } else if (caption == limits) {
                        DeflectionLimitsDialog dialog(parent);
                        dialog.exec();
                    } else if (caption == continuous) {
                        ContinuousBeamNumberingDialog dialog(beams.continuousNumbering, parent);
                        if (dialog.exec() == QDialog::Accepted) beams.continuousNumbering = dialog.value();
                    } else if (caption == numbering) {
                        BeamNumberingDialog dialog(beams.numbering, parent);
                        if (dialog.exec() == QDialog::Accepted) beams.numbering = dialog.value();
                    }
                },
                false, parent);
        }
        case Family::Joints: {
            const QString materials = tr("Materiales"), options = tr("Opciones");
            return new OptionsMenuDialog(
                tr("Uniones"), QSize(117, 158), {{QString(), {materials, options}, {materials, options}}},
                [=, &settings](const QString& caption) {
                    JointOptionsSettings& joints = settings.joints;
                    if (caption == materials) {
                        JointMaterialsDialog dialog(joints.materials, parent);
                        if (dialog.exec() == QDialog::Accepted) joints.materials = dialog.value();
                    } else if (caption == options) {
                        JointOptionsDialog dialog(joints, parent);
                        if (dialog.exec() == QDialog::Accepted) joints = dialog.value();
                    }
                },
                false, parent);
        }
    }
    return nullptr;
}

}  // namespace cc::app
