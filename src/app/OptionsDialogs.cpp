#include "OptionsDialogs.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QRadioButton>
#include <QTabWidget>
#include <QToolButton>
#include <QVBoxLayout>

#include "Icons.h"
#include "OptionsLeafDialogs.h"

namespace cc::app {

using namespace cc::core;

namespace {

QLabel* sectionBar(const QString& text, QWidget* parent) {
    auto* label = new QLabel(text, parent);
    label->setStyleSheet(QStringLiteral("background: #E4E4E4; padding: 2px 4px;"));
    return label;
}

QRadioButton* addRadio(QButtonGroup* group, QVBoxLayout* layout, const QString& text, int id, QWidget* parent) {
    auto* radio = new QRadioButton(text, parent);
    group->addButton(radio, id);
    layout->addWidget(radio);
    return radio;
}

/// Simple original schematic of a hexagon-head bolt (not copied from the CYPE application).
class BoltDiagram : public QWidget {
public:
    explicit BoltDiagram(QWidget* parent) : QWidget(parent) { setFixedWidth(120); }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.fillRect(rect(), QColor(0xEA, 0xEA, 0xEC));
        const int cx = width() / 2;
        p.setPen(QPen(QColor(90, 90, 96), 1.5));
        p.setBrush(QColor(170, 170, 176));
        p.drawRoundedRect(cx - 28, 70, 56, 30, 6, 6);  // head
        p.setBrush(QColor(200, 200, 205));
        p.drawRect(cx - 14, 100, 28, 150);              // shank
        for (int y = 140; y < 250; y += 8) p.drawLine(cx - 14, y, cx + 14, y + 4);
    }
};

}  // namespace

// ---------------------------------------------------------------- Valoración de errores

namespace {
struct ErrorField {
    const char* text;
    bool BeamErrorSettings::*member;
    int group;  ///< 0 Vigas de hormigón, 1 Estribos, 2 Armados
};
// The stirrup diameter row is handled separately: it sits between the first and second group.
const ErrorField kErrorFields[] = {
    {QT_TRANSLATE_NOOP("BeamErrorsDialog", "Patilla por compresión"), &BeamErrorSettings::compressionHook, 0},
    {QT_TRANSLATE_NOOP("BeamErrorsDialog", "Armadura compresión superior"), &BeamErrorSettings::topCompressionSteel, 0},
    {QT_TRANSLATE_NOOP("BeamErrorsDialog", "Armadura compresión inferior"), &BeamErrorSettings::bottomCompressionSteel, 0},
    {QT_TRANSLATE_NOOP("BeamErrorsDialog", "Armado manual de ménsula corta"), &BeamErrorSettings::manualShortCorbel, 0},
    {QT_TRANSLATE_NOOP("BeamErrorsDialog", "Estribos de diámetro igual o superior a:"), &BeamErrorSettings::minStirrupDiameter, 1},
    {QT_TRANSLATE_NOOP("BeamErrorsDialog", "Celosía muy corta"), &BeamErrorSettings::veryShortTruss, 1},
    {QT_TRANSLATE_NOOP("BeamErrorsDialog", "Anclaje de la armadura de piel"), &BeamErrorSettings::skinAnchorage, 2},
    {QT_TRANSLATE_NOOP("BeamErrorsDialog", "Tramo corto, longitud de barra menor que la longitud mínima de anclaje"),
     &BeamErrorSettings::shortSpan, 2},
    {QT_TRANSLATE_NOOP("BeamErrorsDialog", "Desnivel que necesita detalle constructivo de continuidad de barras"),
     &BeamErrorSettings::levelDifference, 2},
    {QT_TRANSLATE_NOOP("BeamErrorsDialog", "Longitud de la barra mayor que la longitud máxima"), &BeamErrorSettings::barTooLong, 2},
    {QT_TRANSLATE_NOOP("BeamErrorsDialog", "Anclaje de la armadura de montaje"), &BeamErrorSettings::assemblyAnchorage, 2},
    {QT_TRANSLATE_NOOP("BeamErrorsDialog", "Anclaje de la armadura superior"), &BeamErrorSettings::topAnchorage, 2},
    {QT_TRANSLATE_NOOP("BeamErrorsDialog", "Armadura superior en más de una capa"), &BeamErrorSettings::topMultiLayer, 2},
    {QT_TRANSLATE_NOOP("BeamErrorsDialog", "Anclaje de la armadura inferior"), &BeamErrorSettings::bottomAnchorage, 2},
    {QT_TRANSLATE_NOOP("BeamErrorsDialog", "Armadura inferior en más de una capa"), &BeamErrorSettings::bottomMultiLayer, 2}};
}  // namespace

BeamErrorsDialog::BeamErrorsDialog(const BeamErrorSettings& initial, QWidget* parent)
    : DefaultsDialog(tr("Valoración de errores"), parent) {
    setFixedSize(437, 514);
    const QString titles[] = {tr("Vigas de hormigón"), tr("Estribos"), tr("Armados")};
    auto* layout = contentLayout();
    layout->setSpacing(4);
    int group = -1;
    for (const ErrorField& field : kErrorFields) {
        if (field.group != group) {
            group = field.group;
            layout->addWidget(sectionBar(titles[group], this));
        }
        auto* check = new QCheckBox(tr(field.text), this);
        m_checks.push_back(check);
        if (&field == &kErrorFields[4]) {
            auto* row = new QHBoxLayout;
            row->addWidget(check);
            m_stirrupDiameter = new QComboBox(this);
            m_stirrupDiameter->addItem(QStringLiteral("Ø16"));  // the only entry seen in the original (TK-015)
            row->addWidget(m_stirrupDiameter);
            row->addStretch(1);
            layout->addLayout(row);
        } else {
            layout->addWidget(check);
        }
    }
    layout->addStretch(1);
    setupDefaultsFooter();
    load(initial);
}

void BeamErrorsDialog::load(const BeamErrorSettings& s) {
    for (std::size_t i = 0; i < m_checks.size(); ++i) m_checks[i]->setChecked(s.*kErrorFields[i].member);
    m_stirrupDiameter->setCurrentText(QStringLiteral("Ø%1").arg(s.minStirrupDiameterMm));
    m_stirrupDiameter->setEnabled(s.minStirrupDiameter);
    connect(m_checks[4], &QCheckBox::toggled, m_stirrupDiameter, &QWidget::setEnabled, Qt::UniqueConnection);
}

BeamErrorSettings BeamErrorsDialog::value() const {
    BeamErrorSettings s;
    for (std::size_t i = 0; i < m_checks.size(); ++i) s.*kErrorFields[i].member = m_checks[i]->isChecked();
    s.minStirrupDiameterMm = m_stirrupDiameter->currentText().mid(1).toInt();
    return s;
}

void BeamErrorsDialog::resetToInstallationValues() { load(BeamErrorSettings{}); }

// ---------------------------------------------------------------- Límites de flecha

DeflectionLimitsDialog::DeflectionLimitsDialog(const DeflectionLimitsSettings& initial, QWidget* parent)
    : CypeDialog(tr("Límites de flecha"), parent), m_settings(initial) {
    setFixedSize(309, 371);
    m_body = new QWidget(this);
    contentLayout()->addWidget(m_body, 1);
    rebuild();
    setupFooter(tr("Aceptar"), tr("Cancelar"));
}

void DeflectionLimitsDialog::rebuild() {
    delete m_body->layout();
    qDeleteAll(m_body->findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly));
    auto* layout = new QVBoxLayout(m_body);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);
    const std::vector<DeflectionLimitGroup> groups = m_settings.groups();
    for (std::size_t m = 0; m < groups.size(); ++m) {
        const DeflectionLimitGroup& group = groups[m];
        layout->addWidget(sectionBar(tr(group.material.c_str()), m_body));
        auto* row = new QHBoxLayout;
        auto* lines = new QVBoxLayout;
        if (group.limits.empty()) lines->addWidget(new QLabel(tr("No se ha definido ningún límite de flecha."), m_body));
        for (const DeflectionLimit& limit : group.limits) {
            lines->addWidget(new QLabel(QStringLiteral("%1: %2").arg(tr(limit.description.c_str()), QString::fromStdString(limit.text)), m_body));
        }
        row->addLayout(lines, 1);
        auto* edit = new QToolButton(m_body);
        edit->setIcon(themedIcon(QStringLiteral("edit_pencil")));
        edit->setToolTip(tr("Editar límites de flecha"));
        connect(edit, &QToolButton::clicked, this, [this, m] { editMaterial(m); });
        row->addWidget(edit, 0, Qt::AlignVCenter);
        layout->addLayout(row);
    }
    layout->addStretch(1);
    m_body->show();
}

void DeflectionLimitsDialog::editMaterial(std::size_t index) {
    DeflectionEditorDialog editor(tr(DeflectionLimitsSettings::materialName(index)), m_settings.materials[index], this);
    if (editor.exec() != QDialog::Accepted) return;
    m_settings.materials[index] = editor.value();
    rebuild();
}

// ---------------------------------------------------------------- Numeración de vigas continuas

ContinuousBeamNumberingDialog::ContinuousBeamNumberingDialog(const ContinuousBeamNumbering& initial, QWidget* parent)
    : DefaultsDialog(tr("Numeración de vigas continuas"), parent) {
    setFixedSize(531, 372);
    auto* row = new QHBoxLayout;
    row->addWidget(new QLabel(tr("Referencia base"), this));
    m_reference = new QLineEdit(this);
    row->addWidget(m_reference, 1);
    contentLayout()->addLayout(row);

    const auto addGroup = [&](QButtonGroup*& group, const QString& first, const QString& second) {
        auto* box = new QGroupBox(this);
        auto* layout = new QVBoxLayout(box);
        group = new QButtonGroup(this);
        addRadio(group, layout, first, 0, box);
        addRadio(group, layout, second, 1, box);
        contentLayout()->addWidget(box);
    };
    addGroup(m_first, tr("Numerar primero los pórticos paralelos al eje X global"),
             tr("Numerar primero los pórticos paralelos al eje Y global"));
    addGroup(m_xFrames, tr("Numerar los pórticos paralelos al eje X global en el sentido positivo del eje Y global."),
             tr("Numerar los pórticos paralelos al eje X global en el sentido negativo del eje Y global."));
    addGroup(m_yFrames, tr("Numerar los pórticos paralelos al eje Y global en el sentido positivo del eje X global."),
             tr("Numerar los pórticos paralelos al eje Y global en el sentido negativo del eje X global."));
    for (QButtonGroup* group : {m_first, m_xFrames, m_yFrames}) {
        // the selected radio of each group is shown in bold, as in the original
        connect(group, &QButtonGroup::idToggled, this, [group](int id, bool on) {
            QFont font = group->button(id)->font();
            font.setBold(on);
            group->button(id)->setFont(font);
        });
    }
    contentLayout()->addStretch(1);
    setupDefaultsFooter();
    load(initial);
    m_reference->selectAll();
    m_reference->setFocus();
}

void ContinuousBeamNumberingDialog::load(const ContinuousBeamNumbering& s) {
    m_reference->setText(QString::fromStdString(s.baseReference));
    m_first->button(s.firstAxis == ContinuousBeamNumbering::Axis::X ? 0 : 1)->setChecked(true);
    m_xFrames->button(s.xFramesAlongY == ContinuousBeamNumbering::Sense::Positive ? 0 : 1)->setChecked(true);
    m_yFrames->button(s.yFramesAlongX == ContinuousBeamNumbering::Sense::Positive ? 0 : 1)->setChecked(true);
}

ContinuousBeamNumbering ContinuousBeamNumberingDialog::value() const {
    ContinuousBeamNumbering s;
    s.baseReference = m_reference->text().toStdString();
    s.firstAxis = m_first->checkedId() == 0 ? ContinuousBeamNumbering::Axis::X : ContinuousBeamNumbering::Axis::Y;
    s.xFramesAlongY = m_xFrames->checkedId() == 0 ? ContinuousBeamNumbering::Sense::Positive : ContinuousBeamNumbering::Sense::Negative;
    s.yFramesAlongX = m_yFrames->checkedId() == 0 ? ContinuousBeamNumbering::Sense::Positive : ContinuousBeamNumbering::Sense::Negative;
    return s;
}

void ContinuousBeamNumberingDialog::resetToInstallationValues() { load(ContinuousBeamNumbering{}); }

// ---------------------------------------------------------------- Numeración de vigas

BeamNumberingDialog::BeamNumberingDialog(BeamNumbering initial, QWidget* parent)
    : DefaultsDialog(tr("Numeración de vigas"), parent) {
    setFixedSize(397, 233);
    auto* box = new QGroupBox(this);
    auto* layout = new QVBoxLayout(box);
    m_modes = new QButtonGroup(this);
    addRadio(m_modes, layout, tr("No numerar"), 0, box);
    addRadio(m_modes, layout, tr("Numeración consecutiva de la obra"), 1, box);
    addRadio(m_modes, layout, tr("Numeración por niveles"), 2, box);
    m_modes->button(static_cast<int>(initial))->setChecked(true);
    contentLayout()->addWidget(box);
    contentLayout()->addStretch(1);
    setupDefaultsFooter();
}

BeamNumbering BeamNumberingDialog::value() const { return static_cast<BeamNumbering>(m_modes->checkedId()); }
void BeamNumberingDialog::resetToInstallationValues() { m_modes->button(0)->setChecked(true); }

// ---------------------------------------------------------------- Uniones: Materiales

JointMaterialsDialog::JointMaterialsDialog(const JointMaterials& initial, QWidget* parent)
    : CypeDialog(tr("Materiales"), parent) {
    setFixedSize(291, 181);
    auto* grid = new QGridLayout;
    const auto fill = [&](int row, const QString& label, QComboBox*& combo, const std::vector<std::string>& items,
                          const std::string& current) {
        grid->addWidget(new QLabel(label, this), row, 0);
        combo = new QComboBox(this);
        for (const std::string& item : items) combo->addItem(QString::fromStdString(item));
        combo->setCurrentText(QString::fromStdString(current));
        grid->addWidget(combo, row, 1);
    };
    fill(0, tr("Acero laminado"), m_steel, hotRolledSteelGrades(), initial.rolledSteel);
    // Only "B 400 S, Ys = 1.15" was seen in the anchor-bolt steel combo (ticket TK-015).
    fill(1, tr("Acero de pernos"), m_bolts, {initial.anchorBoltSteel}, initial.anchorBoltSteel);
    fill(2, tr("Hormigón:"), m_concrete, concreteGrades(), initial.concrete);
    grid->setColumnStretch(1, 1);
    contentLayout()->addLayout(grid);
    contentLayout()->addStretch(1);
    setupFooter(tr("Aceptar"), tr("Cancelar"));
}

JointMaterials JointMaterialsDialog::value() const {
    JointMaterials s;
    s.rolledSteel = m_steel->currentText().toStdString();
    s.anchorBoltSteel = m_bolts->currentText().toStdString();
    s.concrete = m_concrete->currentText().toStdString();
    return s;
}

// ---------------------------------------------------------------- Uniones: Opciones

namespace {
QLabel* boldLabel(const QString& text, QWidget* parent) { return new QLabel(QStringLiteral("<b>%1</b>").arg(text), parent); }

/// Tab page with the decorative bolt picture on the left and a column of controls on the right.
QVBoxLayout* illustratedPage(QWidget* page) {
    auto* row = new QHBoxLayout(page);
    row->setContentsMargins(0, 0, 0, 0);
    row->addWidget(new BoltDiagram(page));
    auto* column = new QVBoxLayout;
    row->addLayout(column, 1);
    return column;
}

QLineEdit* numberField(int decimals, QWidget* parent) {
    auto* edit = new QLineEdit(QString::number(0.0, 'f', decimals), parent);
    edit->setFixedWidth(60);
    edit->setAlignment(Qt::AlignRight);
    return edit;
}

void markBold(QButtonGroup* group) {
    QObject::connect(group, &QButtonGroup::idToggled, group, [group](int id, bool on) {
        QFont font = group->button(id)->font();
        font.setBold(on);
        group->button(id)->setFont(font);
    });
}
}  // namespace

JointOptionsDialog::JointOptionsDialog(const JointOptionsSettings& initial, QWidget* parent)
    : DefaultsDialog(tr("Opciones"), parent), m_passThrough(initial) {
    setFixedSize(671, 460);
    auto* tabs = new QTabWidget(this);
    tabs->addTab(buildBoltsTab(tabs), tr("Tornillería no pretensada"));
    tabs->addTab(buildPreloadedTab(tabs), tr("Tornillería pretensada"));
    tabs->addTab(buildStiffenersTab(tabs), tr("Rigidizadores"));
    tabs->addTab(buildBasePlateTab(tabs), tr("Placa de anclaje"));
    contentLayout()->addWidget(tabs, 1);
    setupDefaultsFooter();
    load(initial);
}

QWidget* JointOptionsDialog::buildBoltsTab(QWidget* parent) {
    auto* page = new QWidget(parent);
    auto* column = illustratedPage(page);
    column->addWidget(boldLabel(tr("Series de tornillos"), page));
    auto* series = new QHBoxLayout;
    m_iso4014 = new QCheckBox(tr("ISO 4014"), page);
    m_iso4017 = new QCheckBox(tr("ISO 4017"), page);
    series->addWidget(m_iso4014);
    series->addWidget(m_iso4017);
    series->addStretch(1);
    column->addLayout(series);

    column->addWidget(boldLabel(tr("Clase de acero de los tornillos"), page));
    auto* classes = new QHBoxLayout;
    m_classes = new QButtonGroup(this);
    int id = 0;
    for (const std::string& name : boltSteelClasses()) {
        auto* radio = new QRadioButton(QString::fromStdString(name), page);
        m_classes->addButton(radio, id++);
        classes->addWidget(radio);
    }
    classes->addStretch(1);
    column->addLayout(classes);

    column->addWidget(boldLabel(tr("Diámetros disponibles"), page));
    auto* diameters = new QHBoxLayout;
    for (const std::string& name : boltDiameterNames()) {
        auto* check = new QCheckBox(QString::fromStdString(name), page);
        m_diameters.push_back(check);
        diameters->addWidget(check);
    }
    diameters->addStretch(1);
    column->addLayout(diameters);
    m_unusual = new QCheckBox(tr("Utilizar diámetros no habituales"), page);
    column->addWidget(m_unusual);
    column->addStretch(1);
    return page;
}

QWidget* JointOptionsDialog::buildPreloadedTab(QWidget* parent) {
    auto* page = new QWidget(parent);
    auto* column = illustratedPage(page);
    column->addWidget(boldLabel(tr("Series de tornillos"), page));
    auto* series = new QHBoxLayout;
    m_pSeries = new QButtonGroup(this);
    int id = 0;
    for (const std::string& name : preloadedBoltSeries()) {
        auto* radio = new QRadioButton(QString::fromStdString(name), page);
        m_pSeries->addButton(radio, id++);
        series->addWidget(radio);
    }
    series->addStretch(1);
    column->addLayout(series);
    markBold(m_pSeries);

    column->addWidget(boldLabel(tr("Clase de acero de los tornillos"), page));
    auto* classes = new QHBoxLayout;
    m_pClasses = new QButtonGroup(this);
    id = 0;
    for (const char* name : {"8.8", "10.9"}) {
        auto* radio = new QRadioButton(QString::fromLatin1(name), page);
        m_pClasses->addButton(radio, id++);
        classes->addWidget(radio);
    }
    classes->addStretch(1);
    column->addLayout(classes);
    markBold(m_pClasses);

    column->addWidget(boldLabel(tr("Diámetros disponibles"), page));
    auto* diameters = new QHBoxLayout;
    for (const std::string& name : preloadedBoltDiameterNames()) {
        auto* check = new QCheckBox(QString::fromStdString(name), page);
        m_pDiameters.push_back(check);
        diameters->addWidget(check);
    }
    diameters->addStretch(1);
    column->addLayout(diameters);
    m_pUnusual = new QCheckBox(tr("Utilizar diámetros no habituales"), page);
    column->addWidget(m_pUnusual);
    column->addSpacing(12);

    column->addWidget(boldLabel(tr("Clase de la superficie de rozamiento (Código Estructural, Anejo 21)"), page));
    auto* friction = new QHBoxLayout;
    m_pFriction = new QButtonGroup(this);
    for (char c : {'A', 'B', 'C', 'D'}) {
        auto* radio = new QRadioButton(
            QStringLiteral("%1 (µ = %2)").arg(QChar::fromLatin1(c)).arg(frictionCoefficient(c), 0, 'f', 2), page);
        m_pFriction->addButton(radio, c);
        friction->addWidget(radio);
    }
    friction->addStretch(1);
    column->addLayout(friction);
    markBold(m_pFriction);
    m_frictionText = new QLabel(page);
    m_frictionText->setWordWrap(true);
    m_frictionText->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    m_frictionText->setMinimumHeight(60);
    m_frictionText->setStyleSheet(QStringLiteral("background: #D8F2FB; border: 1px solid #8CC8DC; padding: 4px;"));
    column->addWidget(m_frictionText);
    column->addStretch(1);
    connect(m_pFriction, &QButtonGroup::idToggled, this, [this](int id, bool on) {
        if (on) m_frictionText->setText(QString::fromStdString(frictionDescription(static_cast<char>(id))));
    });
    return page;
}

QWidget* JointOptionsDialog::buildStiffenersTab(QWidget* parent) {
    auto* page = new QWidget(parent);
    auto* column = illustratedPage(page);
    column->addWidget(boldLabel(tr("Rigidizadores en el extremo de las cartelas"), page));
    m_columnBeam = new QCheckBox(tr("Disponer siempre en uniones pilar-viga"), page);
    m_ridge = new QCheckBox(tr("Disponer siempre en uniones de cumbrera"), page);
    column->addWidget(m_columnBeam);
    column->addWidget(m_ridge);
    column->addSpacing(8);
    column->addWidget(boldLabel(tr("Rigidizadores para vigas empotradas en el alma del pilar"), page));
    m_cutouts = new QButtonGroup(this);
    // the cut-out pictures of the original are not copied: plain radios (ticket TK-019)
    auto* without = new QRadioButton(tr("Rigidizadores sin recortes"), page);
    auto* with = new QRadioButton(tr("Rigidizadores con recortes"), page);
    m_cutouts->addButton(without, 0);
    m_cutouts->addButton(with, 1);
    column->addWidget(without);
    column->addWidget(with);
    column->addStretch(1);
    markBold(m_cutouts);
    return page;
}

namespace {
struct PlateField {
    const char* label;
    double BasePlateSettings::*member;
    int decimals;
    const char* unit;
};
const PlateField kPlateFields[] = {
    {QT_TRANSLATE_NOOP("JointOptionsDialog", "Relación máxima vuelo-espesor"), &BasePlateSettings::maxOverhangThickness, 2, ""},
    {QT_TRANSLATE_NOOP("JointOptionsDialog", "Relación máxima ancho-espesor"), &BasePlateSettings::maxWidthThickness, 2, ""},
    {QT_TRANSLATE_NOOP("JointOptionsDialog", "Espesor máximo recomendable para placas"),
     &BasePlateSettings::maxRecommendedThicknessMm, 1, "mm"},
    {QT_TRANSLATE_NOOP("JointOptionsDialog", "Espesor del mortero de nivelación"), &BasePlateSettings::groutThicknessCm, 2, "cm"},
    {QT_TRANSLATE_NOOP("JointOptionsDialog", "Ángulo en bisel para rigidizadores"), &BasePlateSettings::stiffenerBevelDeg, 2,
     "Grados"},
    {QT_TRANSLATE_NOOP("JointOptionsDialog", "Relación máxima suma de lados-diámetro de pernos"),
     &BasePlateSettings::maxSidesBoltDiameter, 2, ""}};
}  // namespace

QWidget* JointOptionsDialog::buildBasePlateTab(QWidget* parent) {
    auto* page = new QWidget(parent);
    auto* grid = new QGridLayout(page);
    int row = 0;
    for (const PlateField& field : kPlateFields) {
        grid->addWidget(new QLabel(tr(field.label), page), row, 0);
        auto* edit = numberField(field.decimals, page);
        m_plateFields.push_back(edit);
        grid->addWidget(edit, row, 1);
        grid->addWidget(new QLabel(*field.unit ? tr(field.unit) : QString(), page), row, 2);
        ++row;
    }
    grid->addWidget(new QLabel(tr("Anclaje pernos-placa"), page), row, 0);
    m_plateAnchorage = new QComboBox(page);
    m_plateAnchorage->addItem(tr("Soldados con preparación de bordes"));  // only entry seen (ticket TK-019)
    grid->addWidget(m_plateAnchorage, row, 1, 1, 2);
    grid->setRowStretch(row + 1, 1);
    grid->setColumnStretch(0, 1);
    return page;
}

void JointOptionsDialog::load(const JointOptionsSettings& all) {
    m_passThrough = all;
    const BoltSettings& s = all.bolts;
    m_iso4014->setChecked(s.iso4014);
    m_iso4017->setChecked(s.iso4017);
    const auto& classes = boltSteelClasses();
    for (std::size_t i = 0; i < classes.size(); ++i) m_classes->button(static_cast<int>(i))->setChecked(classes[i] == s.steelClass);
    for (std::size_t i = 0; i < m_diameters.size(); ++i) m_diameters[i]->setChecked(s.diameters[i]);
    m_unusual->setChecked(s.unusualDiameters);

    const PreloadedBoltSettings& p = all.preloaded;
    const auto& series = preloadedBoltSeries();
    for (std::size_t i = 0; i < series.size(); ++i) m_pSeries->button(static_cast<int>(i))->setChecked(series[i] == p.series);
    m_pClasses->button(p.steelClass == "8.8" ? 0 : 1)->setChecked(true);
    for (std::size_t i = 0; i < m_pDiameters.size(); ++i) m_pDiameters[i]->setChecked(p.diameters[i]);
    m_pUnusual->setChecked(p.unusualDiameters);
    m_pFriction->button(p.frictionClass)->setChecked(true);

    m_columnBeam->setChecked(all.stiffeners.alwaysColumnBeam);
    m_ridge->setChecked(all.stiffeners.alwaysRidge);
    m_cutouts->button(all.stiffeners.withCutouts ? 1 : 0)->setChecked(true);

    for (std::size_t i = 0; i < m_plateFields.size(); ++i) {
        m_plateFields[i]->setText(QString::number(all.basePlate.*kPlateFields[i].member, 'f', kPlateFields[i].decimals));
    }
    m_plateAnchorage->setCurrentText(QString::fromStdString(all.basePlate.boltPlateAnchorage));
}

JointOptionsSettings JointOptionsDialog::value() const {
    JointOptionsSettings all = m_passThrough;
    BoltSettings& s = all.bolts;
    s.iso4014 = m_iso4014->isChecked();
    s.iso4017 = m_iso4017->isChecked();
    s.steelClass = boltSteelClasses().at(static_cast<std::size_t>(m_classes->checkedId()));
    for (std::size_t i = 0; i < m_diameters.size(); ++i) s.diameters[i] = m_diameters[i]->isChecked();
    s.unusualDiameters = m_unusual->isChecked();

    PreloadedBoltSettings& p = all.preloaded;
    p.series = preloadedBoltSeries().at(static_cast<std::size_t>(m_pSeries->checkedId()));
    p.steelClass = m_pClasses->checkedId() == 0 ? "8.8" : "10.9";
    for (std::size_t i = 0; i < m_pDiameters.size(); ++i) p.diameters[i] = m_pDiameters[i]->isChecked();
    p.unusualDiameters = m_pUnusual->isChecked();
    p.frictionClass = static_cast<char>(m_pFriction->checkedId());

    all.stiffeners.alwaysColumnBeam = m_columnBeam->isChecked();
    all.stiffeners.alwaysRidge = m_ridge->isChecked();
    all.stiffeners.withCutouts = m_cutouts->checkedId() == 1;

    for (std::size_t i = 0; i < m_plateFields.size(); ++i) {
        bool ok = false;
        const double v = m_plateFields[i]->text().replace(',', '.').toDouble(&ok);
        if (ok) all.basePlate.*kPlateFields[i].member = v;
    }
    all.basePlate.boltPlateAnchorage = m_plateAnchorage->currentText().toStdString();
    return all;
}

void JointOptionsDialog::resetToInstallationValues() {
    JointOptionsSettings fresh;
    fresh.materials = m_passThrough.materials;
    load(fresh);
}

}  // namespace cc::app
