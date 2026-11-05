#include "ShellDialogs.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QPrintDialog>
#include <QPrinter>
#include <QRadioButton>
#include <QStackedWidget>
#include <QToolButton>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>

#include "ViewIcons.h"
#include "core/project_settings.h"
#include "core/shell_material.h"

namespace cc::app {

using namespace cc::core;

namespace {
QString unitLabel(const char* text) { return QString::fromUtf8(text); }
}  // namespace

// ---- Sección de la lámina ------------------------------------------------------------------------------------------------------

ShellSectionPanel::ShellSectionPanel(const ShellSection& initial, LengthUnit unit, QWidget* parent)
    : QGroupBox(tr("Sección de la lámina"), parent), m_unit(unit) {
    auto* grid = new QGridLayout(this);
    grid->setColumnStretch(3, 1);
    // The range of the original is in mm; it follows the dimension unit of the work.
    const double perMm = unitsPerMetre(unit) / 1000.0;
    const int decimals = unit == LengthUnit::Millimetres ? 2 : (unit == LengthUnit::Centimetres ? 3 : 5);
    m_thickness = new RangedEdit(tr("Espesor"), ValueRange{kShellThicknessMm.min * perMm, kShellThicknessMm.max * perMm}, decimals, this);
    m_thickness->setValue(fromMetres(initial.thicknessM, unit));
    grid->addWidget(new QLabel(tr("Espesor"), this), 0, 0);
    grid->addWidget(m_thickness, 0, 1);
    grid->addWidget(new QLabel(QString::fromLatin1(unitSymbol(unit)), this), 0, 2);

    m_ballast = new QCheckBox(tr("Módulo de balasto en Z local"), this);
    m_ballastValue = new RangedEdit(tr("Módulo de balasto en Z local"), kShellBallastKnM3, 2, this);
    m_ballastValue->setValue(kShellBallastKnM3.contains(initial.ballastKnM3) ? initial.ballastKnM3 : kDefaultShellBallastKnM3);
    grid->addWidget(m_ballast, 1, 0);
    grid->addWidget(m_ballastValue, 1, 1);
    grid->addWidget(new QLabel(unitLabel("kN/m³"), this), 1, 2);

    // "Comportamiento": a row that depends on the checkbox (hidden in the original while it is unchecked: the combo appears on checking).
    m_behaviourRow = new QWidget(this);
    auto* row = new QHBoxLayout(m_behaviourRow);
    row->setContentsMargins(20, 0, 0, 0);
    row->addWidget(new QLabel(tr("Comportamiento"), m_behaviourRow));
    m_behaviour = new QComboBox(m_behaviourRow);
    m_behaviour->addItem(tr("Lineal"));
    m_behaviour->addItem(tr("Sólo compresión"));
    row->addWidget(m_behaviour, 1);
    grid->addWidget(m_behaviourRow, 2, 0, 1, 4);

    const auto sync = [this](bool on) {
        m_ballastValue->setEnabled(on);
        m_behaviourRow->setVisible(on);
        m_behaviour->setEnabled(on);
    };
    connect(m_ballast, &QCheckBox::toggled, this, sync);
    setBehaviour(initial.ballastBehaviour);
    m_ballast->setChecked(initial.ballastEnabled);
    sync(initial.ballastEnabled);
}

bool ShellSectionPanel::validate() { return m_thickness->commit() && (!m_ballast->isChecked() || m_ballastValue->commit()); }

ShellSection ShellSectionPanel::section() const {
    ShellSection s;
    s.thicknessM = toMetres(m_thickness->value(), m_unit);
    s.ballastEnabled = m_ballast->isChecked();
    s.ballastKnM3 = m_ballastValue->value();
    s.ballastBehaviour = m_behaviour->currentIndex() == 1 ? BallastBehaviour::CompressionOnly : BallastBehaviour::Linear;
    return s;
}

void ShellSectionPanel::setThickness(double valueInUnit) { m_thickness->setValue(valueInUnit); }
void ShellSectionPanel::setBallastEnabled(bool on) { m_ballast->setChecked(on); }
void ShellSectionPanel::setBallastValue(double knM3) { m_ballastValue->setValue(knM3); }
void ShellSectionPanel::setBehaviour(BallastBehaviour behaviour) { m_behaviour->setCurrentIndex(behaviour == BallastBehaviour::CompressionOnly ? 1 : 0); }
bool ShellSectionPanel::ballastEditable() const { return m_ballastValue->isEnabled(); }
bool ShellSectionPanel::behaviourEditable() const { return m_behaviourRow->isVisibleTo(this) && m_behaviour->isEnabled(); }

// ---- Material -----------------------------------------------------------------------------------------------------------------

namespace {

struct KindName {
    ShellMaterialKind kind;
    const char* text;
};
constexpr KindName kKinds[] = {{ShellMaterialKind::Concrete, QT_TRANSLATE_NOOP("cc::app::ShellMaterialPanel", "Hormigón")},
                               {ShellMaterialKind::RolledSteel, QT_TRANSLATE_NOOP("cc::app::ShellMaterialPanel", "Acero laminado")},
                               {ShellMaterialKind::ColdFormedSteel, QT_TRANSLATE_NOOP("cc::app::ShellMaterialPanel", "Acero conformado")},
                               {ShellMaterialKind::Aluminium, QT_TRANSLATE_NOOP("cc::app::ShellMaterialPanel", "Aluminio extruido")},
                               {ShellMaterialKind::GenericIsotropic, QT_TRANSLATE_NOOP("cc::app::ShellMaterialPanel", "Genérico, isótropo")},
                               {ShellMaterialKind::GenericOrthotropic, QT_TRANSLATE_NOOP("cc::app::ShellMaterialPanel", "Genérico, ortótropo")}};

int comboIndex(const QComboBox* box, const std::string& text) {
    const int i = box->findText(QString::fromStdString(text));
    return i < 0 ? 0 : i;
}

}  // namespace

std::size_t ShellMaterialPanel::pageIndex() const {
    const std::size_t k = static_cast<std::size_t>(kind());
    return std::min<std::size_t>(k, 3);
}

ShellMaterialPanel::ShellMaterialPanel(const ShellMaterial& initial, const Project& project, QWidget* parent)
    : QGroupBox(tr("Material"), parent), m_project(project) {
    auto* root = new QHBoxLayout(this);
    // Left column: the type, one radio each.
    auto* left = new QVBoxLayout();
    auto* kinds = new QButtonGroup(this);
    for (const KindName& k : kKinds) {
        auto* radio = new QRadioButton(tr(k.text), this);
        radio->setObjectName(QStringLiteral("kind_%1").arg(static_cast<int>(k.kind)));
        kinds->addButton(radio);
        left->addWidget(radio);
        m_kindRadios[static_cast<std::size_t>(k.kind)] = radio;
    }
    left->addStretch(1);
    root->addLayout(left);
    auto* line = new QFrame(this);
    line->setFrameShape(QFrame::VLine);
    line->setFrameShadow(QFrame::Sunken);
    root->addWidget(line);
    m_pages = new QStackedWidget(this);
    root->addWidget(m_pages, 1);

    // Pages 0..3: concrete, rolled steel, cold formed steel, aluminium. All start with the two radios.
    const std::string generalNames[4] = {project.profiles.concrete, project.profiles.hotRolledSteel, project.profiles.coldFormedSteel,
                                         project.profiles.aluminium.summary()};
    for (int i = 0; i < 4; ++i) {
        auto* page = new QWidget(m_pages);
        auto* box = new QVBoxLayout(page);
        GradePage& g = m_gradePages[static_cast<std::size_t>(i)];
        g.general = new QRadioButton(tr("Material seleccionado en datos generales (%1)").arg(QString::fromStdString(generalNames[i])), page);
        g.own = new QRadioButton(tr("Seleccionar el material de la lámina"), page);
        g.general->setChecked(true);
        box->addWidget(g.general);
        box->addWidget(g.own);
        auto* detail = new QWidget(page);
        auto* d = new QGridLayout(detail);
        d->setContentsMargins(20, 0, 0, 0);
        if (i == 0 || i == 1) {
            d->addWidget(new QLabel(tr("Tipo:"), detail), 0, 0);
            g.combo = new QComboBox(detail);
            for (const std::string& name : i == 0 ? concreteGrades() : hotRolledSteelGrades()) g.combo->addItem(QString::fromStdString(name));
            g.combo->setCurrentIndex(comboIndex(g.combo, defaultShellGrade(i == 0 ? ShellMaterialKind::Concrete : ShellMaterialKind::RolledSteel)));
            d->addWidget(g.combo, 0, 1);
            d->setColumnStretch(2, 1);
        } else if (i == 2) {
            auto* group = new QButtonGroup(page);
            int row = 0;
            for (const std::string& name : coldFormedSteelGrades()) {
                auto* radio = new QRadioButton(QString::fromStdString(name), detail);
                group->addButton(radio);
                g.radios.push_back(radio);
                d->addWidget(radio, row++, 0);
            }
            g.radios.front()->setChecked(true);
        } else {
            d->addWidget(new QLabel(tr("Aleación"), detail), 0, 0);
            g.combo = new QComboBox(detail);
            for (const std::string& name : aluminiumAlloys()) g.combo->addItem(QString::fromStdString(name));
            d->addWidget(g.combo, 0, 1);
            d->addWidget(new QLabel(tr("Estado de entrega"), detail), 1, 0);
            g.combo2 = new QComboBox(detail);
            for (const std::string& name : aluminiumTempers()) g.combo2->addItem(QString::fromStdString(name));
            d->addWidget(g.combo2, 1, 1);
            d->setColumnStretch(2, 1);
        }
        box->addWidget(detail);
        box->addStretch(1);
        connect(g.general, &QRadioButton::toggled, this, [this] { refreshEnabled(); });
        m_pages->addWidget(page);
    }

    // Pages 4 and 5: generic isotropic / orthotropic. Rows: label, editor, unit.
    const auto genericPage = [&](const std::vector<std::tuple<QString, ValueRange, int, double, QString>>& rows, std::vector<RangedEdit*>& edits) {
        auto* page = new QWidget(m_pages);
        auto* grid = new QGridLayout(page);
        int row = 0;
        for (const auto& [label, range, decimals, value, unit] : rows) {
            auto* edit = new RangedEdit(label, range, decimals, page);
            edit->setValue(value);
            edits.push_back(edit);
            grid->addWidget(new QLabel(label, page), row, 0);
            grid->addWidget(edit, row, 1);
            grid->addWidget(new QLabel(unit, page), row, 2);
            ++row;
        }
        grid->setColumnStretch(3, 1);
        grid->setRowStretch(row, 1);
        m_pages->addWidget(page);
    };
    const QString mpa = QStringLiteral("MPa"), expansion = QStringLiteral("m/m°C"), weight = QStringLiteral("kN/m³");
    genericPage({{tr("Módulo de elasticidad"), kIsoElasticModulusMPa, 2, initial.isoElasticModulusMPa, mpa},
                 {tr("Módulo de Poisson"), kIsoPoissonRatio, 2, initial.isoPoissonRatio, QString()},
                 {tr("Coeficiente de dilatación"), kThermalExpansion, 6, initial.isoThermalExpansion, expansion},
                 {tr("Peso específico"), kIsoDensityKnM3, 2, initial.isoDensityKnM3, weight}},
                m_iso);
    genericPage({{tr("Módulo de elasticidad (Ex)"), kOrthoElasticModulusMPa, 2, initial.orthoElasticModulusXMPa, mpa},
                 {tr("Módulo de elasticidad (Ey)"), kOrthoElasticModulusMPa, 2, initial.orthoElasticModulusYMPa, mpa},
                 {tr("Módulo de Poisson (nu,xy)"), kOrthoPoissonRatio, 2, initial.orthoPoissonRatioXY, QString()},
                 {tr("Módulo de cortadura (xy)"), kOrthoShearModulusMPa, 2, initial.orthoShearModulusXYMPa, mpa},
                 {tr("Módulo de cortadura (xz)"), kOrthoShearModulusMPa, 2, initial.orthoShearModulusXZMPa, mpa},
                 {tr("Módulo de cortadura (yz)"), kOrthoShearModulusMPa, 2, initial.orthoShearModulusYZMPa, mpa},
                 {tr("Coeficiente de dilatación (x)"), kThermalExpansion, 6, initial.orthoThermalExpansionX, expansion},
                 {tr("Coeficiente de dilatación (y)"), kThermalExpansion, 6, initial.orthoThermalExpansionY, expansion},
                 {tr("Peso específico"), kOrthoDensityKnM3, 2, initial.orthoDensityKnM3, weight}},
                m_ortho);

    // Initial state.
    GradePage& g = m_gradePages[std::min<std::size_t>(static_cast<std::size_t>(initial.kind), 3)];
    if (initial.kind <= ShellMaterialKind::Aluminium) {
        (initial.useGeneralData ? g.general : g.own)->setChecked(true);
        if (initial.kind == ShellMaterialKind::Concrete || initial.kind == ShellMaterialKind::RolledSteel) {
            if (!initial.grade.empty()) g.combo->setCurrentIndex(comboIndex(g.combo, initial.grade));
        } else if (initial.kind == ShellMaterialKind::ColdFormedSteel) {
            for (QRadioButton* r : g.radios)
                if (r->text().toStdString() == initial.grade) r->setChecked(true);
        } else {
            g.combo->setCurrentIndex(comboIndex(g.combo, initial.alloy));
            g.combo2->setCurrentIndex(comboIndex(g.combo2, initial.temper));
        }
    }
    for (const KindName& k : kKinds)
        connect(m_kindRadios[static_cast<std::size_t>(k.kind)], &QRadioButton::toggled, this, [this](bool on) {
            if (on) {
                m_pages->setCurrentIndex(static_cast<int>(kind()));
                refreshEnabled();
            }
        });
    setKind(initial.kind);
    refreshEnabled();
}

ShellMaterialKind ShellMaterialPanel::kind() const {
    for (const KindName& k : kKinds)
        if (m_kindRadios[static_cast<std::size_t>(k.kind)]->isChecked()) return k.kind;
    return ShellMaterialKind::Concrete;
}

void ShellMaterialPanel::setKind(ShellMaterialKind k) {
    m_kindRadios[static_cast<std::size_t>(k)]->setChecked(true);
    m_pages->setCurrentIndex(static_cast<int>(k));
    refreshEnabled();
}

void ShellMaterialPanel::refreshEnabled() {
    for (std::size_t i = 0; i < m_gradePages.size(); ++i) {
        GradePage& g = m_gradePages[i];
        const bool own = g.own->isChecked();
        if (g.combo) g.combo->setEnabled(own);
        if (g.combo2) g.combo2->setEnabled(own);
        for (QRadioButton* r : g.radios) r->setEnabled(own);
    }
}

bool ShellMaterialPanel::validate() {
    const ShellMaterialKind k = kind();
    if (k == ShellMaterialKind::GenericIsotropic) {
        for (RangedEdit* e : m_iso)
            if (!e->commit()) return false;
    } else if (k == ShellMaterialKind::GenericOrthotropic) {
        for (RangedEdit* e : m_ortho)
            if (!e->commit()) return false;
    }
    return true;
}

ShellMaterial ShellMaterialPanel::material() const {
    ShellMaterial m;
    m.kind = kind();
    const GradePage& g = m_gradePages[pageIndex()];
    if (m.kind <= ShellMaterialKind::Aluminium) {
        m.useGeneralData = g.general->isChecked();
        if (m.kind == ShellMaterialKind::Concrete || m.kind == ShellMaterialKind::RolledSteel) {
            m.grade = g.combo->currentText().toStdString();
        } else if (m.kind == ShellMaterialKind::ColdFormedSteel) {
            for (const QRadioButton* r : g.radios)
                if (r->isChecked()) m.grade = r->text().toStdString();
        } else {
            m.alloy = g.combo->currentText().toStdString();
            m.temper = g.combo2->currentText().toStdString();
        }
    } else {
        m.useGeneralData = false;
    }
    m.isoElasticModulusMPa = m_iso[0]->value();
    m.isoPoissonRatio = m_iso[1]->value();
    m.isoThermalExpansion = m_iso[2]->value();
    m.isoDensityKnM3 = m_iso[3]->value();
    m.orthoElasticModulusXMPa = m_ortho[0]->value();
    m.orthoElasticModulusYMPa = m_ortho[1]->value();
    m.orthoPoissonRatioXY = m_ortho[2]->value();
    m.orthoShearModulusXYMPa = m_ortho[3]->value();
    m.orthoShearModulusXZMPa = m_ortho[4]->value();
    m.orthoShearModulusYZMPa = m_ortho[5]->value();
    m.orthoThermalExpansionX = m_ortho[6]->value();
    m.orthoThermalExpansionY = m_ortho[7]->value();
    m.orthoDensityKnM3 = m_ortho[8]->value();
    return m;
}

// ---- dialogs ------------------------------------------------------------------------------------------------------------------

ShellNewDialog::ShellNewDialog(const ShellSection& section, const ShellMaterial& material, const Project& project, QWidget* parent)
    : CypeDialog(tr("Nueva"), parent) {
    m_section = new ShellSectionPanel(section, project.units.dimensions, this);
    m_material = new ShellMaterialPanel(material, project, this);
    contentLayout()->addWidget(m_section);
    contentLayout()->addWidget(m_material, 1);
    setupFooter(tr("Aceptar"), tr("Cancelar"));
    setMinimumWidth(590);
}

void ShellNewDialog::accept() {
    if (m_section->validate() && m_material->validate()) CypeDialog::accept();
}

ShellSectionDialog::ShellSectionDialog(const ShellSection& initial, LengthUnit unit, QWidget* parent) : CypeDialog(tr("Describir sección"), parent) {
    m_panel = new ShellSectionPanel(initial, unit, this);
    contentLayout()->addWidget(m_panel);
    setupFooter(tr("Aceptar"), tr("Cancelar"));
    setMinimumWidth(420);
}

void ShellSectionDialog::accept() {
    if (m_panel->validate()) CypeDialog::accept();
}

ShellMaterialDialog::ShellMaterialDialog(const ShellMaterial& initial, const Project& project, QWidget* parent) : CypeDialog(tr("Describir material"), parent) {
    m_panel = new ShellMaterialPanel(initial, project, this);
    contentLayout()->addWidget(m_panel);
    setupFooter(tr("Aceptar"), tr("Cancelar"));
    setMinimumWidth(590);
}

void ShellMaterialDialog::accept() {
    if (m_panel->validate()) CypeDialog::accept();
}

// ---- Disposición ----------------------------------------------------------------------------------------------------------------

ShellSectionViewer::ShellSectionViewer(QWidget* parent) : QWidget(parent) {
    setMinimumSize(340, 340);
    setCursor(Qt::ArrowCursor);
}

void ShellSectionViewer::setOffsetMm(double mm) {
    m_offsetMm = mm;
    update();
}
void ShellSectionViewer::setThicknessMm(double mm) {
    m_thicknessMm = mm;
    update();
}

void ShellSectionViewer::pushView() { m_history.push_back({m_viewHeightMm, m_centre}); }

void ShellSectionViewer::previousView() {
    if (m_history.empty()) return;
    m_viewHeightMm = m_history.back().first;
    m_centre = m_history.back().second;
    m_history.pop_back();
    update();
}

void ShellSectionViewer::fullView() {
    pushView();
    m_viewHeightMm = 0.0;
    m_centre = {0.0, 0.0};
    update();
}

void ShellSectionViewer::doubleView() {
    pushView();
    if (m_viewHeightMm <= 0.0) m_viewHeightMm = std::max(2.4 * (std::fabs(m_offsetMm) + m_thicknessMm / 2.0), 2.2 * m_thicknessMm);
    m_viewHeightMm *= 2.0;
    update();
}

void ShellSectionViewer::setTool(Tool tool) {
    m_tool = tool;
    setCursor(tool == Tool::Pan ? Qt::OpenHandCursor : tool == Tool::ZoomWindow ? Qt::CrossCursor : Qt::ArrowCursor);
}

QPointF ShellSectionViewer::toScreen(double y, double z) const {
    const double height = m_viewHeightMm > 0.0 ? m_viewHeightMm : std::max(2.4 * (std::fabs(m_offsetMm) + m_thicknessMm / 2.0), 2.2 * m_thicknessMm);
    const double scale = this->height() / height;
    return {width() / 2.0 + (y - m_centre.x()) * scale, this->height() / 2.0 - (z - m_centre.y()) * scale};
}

QPointF ShellSectionViewer::toModel(const QPointF& s) const {
    const QPointF origin = toScreen(0.0, 0.0), unit = toScreen(1.0, 1.0);
    return {(s.x() - origin.x()) / (unit.x() - origin.x()), (s.y() - origin.y()) / (unit.y() - origin.y())};
}

std::array<QPointF, 3> ShellSectionViewer::crossPositions() const {
    return {toScreen(0.0, m_offsetMm + m_thicknessMm / 2.0), toScreen(0.0, m_offsetMm), toScreen(0.0, m_offsetMm - m_thicknessMm / 2.0)};
}

void ShellSectionViewer::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.fillRect(rect(), Qt::white);
    p.setPen(QColor(200, 200, 200));
    p.drawRect(rect().adjusted(0, 0, -1, -1));
    p.setClipRect(rect().adjusted(1, 1, -1, -1));
    // Plate: width = thickness (the original draws a square), from the bottom to the top face.
    const double half = m_thicknessMm / 2.0;
    const QPointF topLeft = toScreen(-half, m_offsetMm + half), bottomRight = toScreen(half, m_offsetMm - half);
    p.setBrush(QColor(192, 192, 192));
    p.setPen(QPen(Qt::black, 1));
    p.drawRect(QRectF(topLeft, bottomRight));
    // Dash-dot lines of the top face, the central plane and the bottom face.
    QPen dash(Qt::black, 1, Qt::DashDotLine);
    p.setPen(dash);
    const auto crosses = crossPositions();
    for (const QPointF& c : crosses) p.drawLine(QPointF(width() * 0.06, c.y()), QPointF(width() * 0.94, c.y()));
    // Axes: Z (blue) up, Y (green) to the right, from the origin of the definition plane.
    const QPointF origin = toScreen(0.0, 0.0);
    p.setPen(QPen(QColor(0, 0, 255), 2));
    p.drawLine(origin, QPointF(origin.x(), height() * 0.06));
    p.drawLine(QPointF(origin.x(), height() * 0.06), QPointF(origin.x() - 5, height() * 0.06 + 12));
    p.drawLine(QPointF(origin.x(), height() * 0.06), QPointF(origin.x() + 5, height() * 0.06 + 12));
    p.drawText(QPointF(origin.x() - 12, height() * 0.06 + 10), QStringLiteral("z"));
    p.setPen(QPen(QColor(0, 255, 0), 2));
    p.drawLine(origin, QPointF(width() * 0.94, origin.y()));
    p.drawLine(QPointF(width() * 0.94, origin.y()), QPointF(width() * 0.94 - 12, origin.y() - 5));
    p.drawLine(QPointF(width() * 0.94, origin.y()), QPointF(width() * 0.94 - 12, origin.y() + 5));
    // Purple crosses.
    p.setPen(QPen(QColor(128, 0, 128), 3));
    for (const QPointF& c : crosses) {
        p.drawLine(c + QPointF(-9, 0), c + QPointF(9, 0));
        p.drawLine(c + QPointF(0, -9), c + QPointF(0, 9));
    }
    if (m_tool == Tool::ZoomWindow && m_dragging) {
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(Qt::black, 1, Qt::DashLine));
        p.drawRect(QRectF(m_pressed, m_current).normalized());
    }
}

void ShellSectionViewer::mousePressEvent(QMouseEvent* e) {
    if (e->button() != Qt::LeftButton || m_tool == Tool::None) return;
    m_pressed = m_current = e->position();
    m_dragging = true;
    if (m_tool == Tool::Pan) setCursor(Qt::ClosedHandCursor);
}

void ShellSectionViewer::mouseMoveEvent(QMouseEvent* e) {
    if (!m_dragging) return;
    if (m_tool == Tool::Pan) {
        // Freeze the scale: when the view fits (height 0) pan from the current fitted height.
        if (m_viewHeightMm <= 0.0) m_viewHeightMm = std::max(2.4 * (std::fabs(m_offsetMm) + m_thicknessMm / 2.0), 2.2 * m_thicknessMm);
        const double scale = height() / m_viewHeightMm;
        const QPointF d = e->position() - m_current;
        m_centre -= QPointF(d.x() / scale, -d.y() / scale);
    }
    m_current = e->position();
    update();
}

void ShellSectionViewer::mouseReleaseEvent(QMouseEvent* e) {
    if (!m_dragging) return;
    m_dragging = false;
    if (m_tool == Tool::Pan) {
        setCursor(Qt::OpenHandCursor);
    } else if (m_tool == Tool::ZoomWindow) {
        const QRectF r = QRectF(m_pressed, e->position()).normalized();
        if (r.width() > 4 && r.height() > 4) {
            const QPointF a = toModel(r.topLeft()), b = toModel(r.bottomRight());
            const double wMm = std::fabs(b.x() - a.x()), hMm = std::fabs(a.y() - b.y());
            pushView();
            // The window must fit in both directions: the height shown is the larger of its height and its width scaled by the aspect.
            m_viewHeightMm = std::max(hMm, wMm * height() / std::max(1, width()));
            m_centre = QPointF((a.x() + b.x()) / 2.0, (a.y() + b.y()) / 2.0);
        }
    }
    update();
}

namespace {
QIcon printIcon() {
    QPixmap px(20, 20);
    px.fill(Qt::transparent);
    QPainter p(&px);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(QPen(QColor(60, 60, 60), 1.4));
    p.setBrush(Qt::white);
    p.drawRect(QRectF(5, 2.5, 10, 5));
    p.setBrush(QColor(220, 233, 247));
    p.drawRoundedRect(QRectF(2.5, 7.5, 15, 6.5), 1.5, 1.5);
    p.setBrush(Qt::white);
    p.drawRect(QRectF(5, 12, 10, 5.5));
    return QIcon(px);
}
}  // namespace

ShellLayoutDialog::ShellLayoutDialog(const ShellLayout& initial, double thicknessM, QWidget* parent) : CypeDialog(tr("Describir disposición"), parent) {
    auto* calc = new QGroupBox(tr("Cálculo"), this);
    auto* calcLayout = new QVBoxLayout(calc);
    m_eccentricity = new QCheckBox(tr("Considerar las excentricidades en el cálculo"), calc);
    m_eccentricity->setChecked(initial.considerEccentricity);
    calcLayout->addWidget(m_eccentricity);
    contentLayout()->addWidget(calc);

    auto* position = new QGroupBox(tr("Posición"), this);
    auto* positionLayout = new QVBoxLayout(position);
    auto* bar = new QHBoxLayout();
    m_viewer = new ShellSectionViewer(position);
    m_viewer->setThicknessMm(thicknessM * 1000.0);
    // The 7 icons of the viewers of the original, with their shortcuts (laminas.md 7.2).
    struct Entry {
        QIcon icon;
        QString tip;
        const char* shortcut;
    };
    const Entry entries[] = {{zoomToolIcon(ZoomToolIcon::PreviousWindow), tr("Ventana anterior"), "Ctrl+P"},
                             {zoomToolIcon(ZoomToolIcon::FullWindow), tr("Ventana completa"), "Ctrl+W"},
                             {zoomToolIcon(ZoomToolIcon::DoubleWindow), tr("Ventana doble"), nullptr},
                             {zoomToolIcon(ZoomToolIcon::Redraw), tr("Redibujar"), "Ctrl+R"},
                             {zoomToolIcon(ZoomToolIcon::ZoomMark), tr("Marcar zoom"), "Ctrl+M"},
                             {zoomToolIcon(ZoomToolIcon::MoveImage), tr("Mover imagen"), nullptr},
                             {printIcon(), tr("Imprimir"), nullptr}};
    for (const Entry& entry : entries) {
        auto* button = new QToolButton(position);
        button->setIcon(entry.icon);
        button->setToolTip(entry.shortcut ? QStringLiteral("%1 (%2)").arg(entry.tip, QString::fromLatin1(entry.shortcut)) : entry.tip);
        button->setAutoRaise(true);
        button->setFocusPolicy(Qt::NoFocus);
        if (entry.shortcut) button->setShortcut(QKeySequence(QString::fromLatin1(entry.shortcut)));
        bar->addWidget(button);
        m_buttons.push_back(button);
    }
    bar->addStretch(1);
    m_buttons[4]->setCheckable(true);
    m_buttons[5]->setCheckable(true);
    connect(m_buttons[0], &QToolButton::clicked, m_viewer, &ShellSectionViewer::previousView);
    connect(m_buttons[1], &QToolButton::clicked, m_viewer, &ShellSectionViewer::fullView);
    connect(m_buttons[2], &QToolButton::clicked, m_viewer, &ShellSectionViewer::doubleView);
    connect(m_buttons[3], &QToolButton::clicked, m_viewer, &ShellSectionViewer::redraw);
    connect(m_buttons[4], &QToolButton::toggled, this, [this](bool on) {
        if (on) m_buttons[5]->setChecked(false);
        m_viewer->setTool(on ? ShellSectionViewer::Tool::ZoomWindow : ShellSectionViewer::Tool::None);
    });
    connect(m_buttons[5], &QToolButton::toggled, this, [this](bool on) {
        if (on) m_buttons[4]->setChecked(false);
        m_viewer->setTool(on ? ShellSectionViewer::Tool::Pan : ShellSectionViewer::Tool::None);
    });
    connect(m_buttons[6], &QToolButton::clicked, this, [this] { printViewer(); });
    positionLayout->addLayout(bar);
    positionLayout->addWidget(m_viewer, 1);
    contentLayout()->addWidget(position, 1);

    auto* offset = new QGroupBox(tr("Desplazamiento del plano central"), this);
    auto* offsetLayout = new QHBoxLayout(offset);
    offsetLayout->addWidget(new QLabel(QStringLiteral("Z"), offset));
    m_offset = new RangedEdit(QStringLiteral("Z"), kShellOffsetMm, 0, offset);
    m_offset->setValue(std::round(initial.centralPlaneOffsetM * 1000.0));
    m_viewer->setOffsetMm(m_offset->value());
    connect(m_offset, &RangedEdit::valueCommitted, m_viewer, &ShellSectionViewer::setOffsetMm);
    offsetLayout->addWidget(m_offset);
    offsetLayout->addWidget(new QLabel(QStringLiteral("mm"), offset));
    offsetLayout->addStretch(1);
    contentLayout()->addWidget(offset);
    setupFooter(tr("Aceptar"), tr("Cancelar"));
    setMinimumWidth(410);
}

ShellLayout ShellLayoutDialog::layout() const {
    ShellLayout l;
    l.considerEccentricity = m_eccentricity->isChecked();
    l.centralPlaneOffsetM = m_offset->value() / 1000.0;
    return l;
}

void ShellLayoutDialog::accept() {
    if (m_offset->commit()) CypeDialog::accept();
}

void ShellLayoutDialog::printViewer() {
    QPrinter printer;
    QPrintDialog dialog(&printer, this);
    dialog.setWindowTitle(tr("Imprimir"));
    if (dialog.exec() != QDialog::Accepted) return;
    QPainter painter(&printer);
    const QRectF page = printer.pageRect(QPrinter::DevicePixel);
    const double scale = std::min(page.width() / m_viewer->width(), page.height() / m_viewer->height());
    painter.scale(scale, scale);
    m_viewer->render(&painter);
}

}  // namespace cc::app
