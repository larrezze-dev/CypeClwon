#include "WizardSubDialogs.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QRadioButton>
#include <QTableWidget>
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

}  // namespace cc::app
