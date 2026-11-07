#include "ShellPropertiesDialogs.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QRadioButton>
#include <QVBoxLayout>
#include <cmath>

#include "RangedEdit.h"
#include "UserMessages.h"

namespace cc::app {

using namespace cc::core;

namespace {

void setBold(QWidget* widget, bool bold) {
    QFont font = widget->font();
    font.setBold(bold);
    widget->setFont(font);
}

/// A checked radio is bold in the original dialogs.
void boldWhenChecked(QRadioButton* radio) {
    setBold(radio, radio->isChecked());
    QObject::connect(radio, &QRadioButton::toggled, radio, [radio](bool on) { setBold(radio, on); });
}

QWidget* labelledEdit(QWidget* parent, const QString& label, RangedEdit* edit, const QString& unit, int labelWidth) {
    auto* row = new QWidget(parent);
    auto* line = new QHBoxLayout(row);
    line->setContentsMargins(0, 0, 0, 0);
    line->setSpacing(6);
    auto* text = new QLabel(label, row);
    if (labelWidth > 0) text->setFixedWidth(labelWidth);
    line->addWidget(text);
    line->addWidget(edit);
    if (!unit.isEmpty()) line->addWidget(new QLabel(unit, row));
    line->addStretch(1);
    return row;
}

}  // namespace

// ---- Discretización -----------------------------------------------------------------------------------------------------------

ShellDiscretisationDialog::ShellDiscretisationDialog(double maxSideM, const QString& unit, QWidget* parent) : CypeDialog(tr("Discretización"), parent) {
    setFixedWidth(634);
    auto* body = new QWidget(this);
    auto* column = new QVBoxLayout(body);
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(10);
    m_info = new QLabel(
        tr("Las láminas se discretizan mediante una malla de triángulos, teniendo en cuenta las condiciones de contorno de cada lámina. Es posible "
           "controlar la dimensión de los triángulos y, por tanto, la densidad del mallado, indicando la dimensión máxima para los lados de los "
           "triángulos.\nTenga en cuenta que el programa realiza la compatibilidad entre mallas en las aristas de las láminas, lo que puede afectar a "
           "la uniformidad de la discretización."),
        body);
    m_info->setObjectName(QStringLiteral("shellInfoBand"));
    m_info->setWordWrap(true);
    m_info->setStyleSheet(QStringLiteral("QLabel#shellInfoBand { background: #E3F4FB; border: 1px solid #9BD3EA; padding: 4px; font-size: 12px; }"));
    column->addWidget(m_info);
    m_edit = new RangedEdit(tr("Tamaño máximo de los lados de un triángulo"), {kShellTriangleSideMinM, kShellTriangleSideMaxM}, 2, body);
    m_edit->setBoundUnit(unit);
    m_edit->setFixedWidth(52);
    m_edit->setValue(maxSideM);
    column->addWidget(labelledEdit(body, tr("Tamaño máximo de los lados de un triángulo"), m_edit, unit, 0));
    contentLayout()->setContentsMargins(6, 6, 6, 6);
    contentLayout()->addWidget(body, 1);
    setupFooter(tr("Aceptar"), tr("Cancelar"));
    m_edit->setFocus();
    m_edit->selectAll();
}

double ShellDiscretisationDialog::maxSide() const { return m_edit->value(); }

void ShellDiscretisationDialog::accept() {
    if (!m_edit->commit()) return;
    CypeDialog::accept();
}

// ---- Factores de rigidez ------------------------------------------------------------------------------------------------------

ShellStiffnessDialog::ShellStiffnessDialog(const ShellStiffnessFactors& initial, QWidget* parent) : CypeDialog(tr("Factores de rigidez"), parent) {
    setFixedWidth(180);
    auto* body = new QWidget(this);
    auto* column = new QVBoxLayout(body);
    column->setContentsMargins(4, 4, 4, 4);
    column->setSpacing(8);
    for (int i = 0; i < kStiffnessFactorCount; ++i) {
        const QString label = QString::fromStdString(stiffnessFieldLabel(i));
        auto* edit = new RangedEdit(label, {kStiffnessFactorMin, kStiffnessFactorMax}, 3, body);
        edit->setBoundSuffix(QStringLiteral(" "));  // the template inserts the empty units: «entre 0.000  y 100.000 .»
        edit->setFixedWidth(62);
        edit->setValue(initial.value(i));
        m_edits[static_cast<std::size_t>(i)] = edit;
        column->addWidget(labelledEdit(body, label, edit, QString(), 86));
    }
    column->addStretch(1);
    contentLayout()->setContentsMargins(4, 4, 4, 4);
    contentLayout()->addWidget(body, 1);
    setupFooter(tr("Aceptar"), tr("Cancelar"));
    m_edits[0]->setFocus();
    m_edits[0]->selectAll();
}

ShellStiffnessFactors ShellStiffnessDialog::factors() const {
    ShellStiffnessFactors out;
    for (int i = 0; i < kStiffnessFactorCount; ++i) out.set(i, std::round(m_edits[static_cast<std::size_t>(i)]->value() * 1000.0) / 1000.0);  // 3 decimals, as shown
    return out;
}

void ShellStiffnessDialog::accept() {
    for (RangedEdit* edit : m_edits)
        if (!edit->commit()) return;
    CypeDialog::accept();
}

// ---- Vinculación interior de aristas ------------------------------------------------------------------------------------------

EdgeInteriorDialog::EdgeInteriorDialog(const EdgeInteriorBond& initial, QWidget* parent) : CypeDialog(tr("Vinculación interior"), parent) {
    setFixedSize(473, 310);
    auto* body = new QWidget(this);
    auto* outer = new QHBoxLayout(body);
    outer->setContentsMargins(8, 8, 8, 8);
    outer->setSpacing(10);

    auto* left = new QWidget(body);
    left->setFixedWidth(100);
    auto* leftColumn = new QVBoxLayout(left);
    leftColumn->setContentsMargins(0, 0, 0, 0);
    leftColumn->setSpacing(4);
    auto* group = new QButtonGroup(this);
    const QString names[4] = {tr("Libre"), tr("Empotrada"), tr("Articulada"), tr("Genérica")};
    for (int i = 0; i < 4; ++i) {
        auto* radio = new QRadioButton(names[i], left);
        m_kindRadios[static_cast<std::size_t>(i)] = radio;
        group->addButton(radio, i);
        leftColumn->addWidget(radio);
        connect(radio, &QRadioButton::toggled, this, [this, i, radio](bool on) {
            setBold(radio, on);
            if (!on) return;
            m_kind = static_cast<EdgeInteriorKind>(i);
            m_generic->setVisible(m_kind == EdgeInteriorKind::Generic);
        });
    }
    leftColumn->addStretch(1);
    outer->addWidget(left, 0, Qt::AlignTop | Qt::AlignLeft);

    m_generic = new QWidget(body);
    auto* right = new QVBoxLayout(m_generic);
    right->setContentsMargins(0, 0, 0, 0);
    right->setSpacing(0);
    const auto band = [&](const QString& text) {
        auto* label = new QLabel(text, m_generic);
        label->setStyleSheet(QStringLiteral("QLabel { background: #E3E3E3; padding: 1px 4px; font-size: 12px; }"));
        return label;
    };
    const QString dofNames[6] = {tr("Dx"), tr("Dy"), tr("Dz"), tr("Gx"), tr("Gy"), tr("Gz")};
    for (int block = 0; block < 2; ++block) {
        right->addWidget(band(block == 0 ? tr("Desplazamientos en ejes locales de la arista") : tr("Giros en ejes locales de la arista")));
        for (int r = 0; r < 3; ++r) {
            const int dof = block * 3 + r;
            const auto d = static_cast<std::size_t>(dof);
            auto* row = new QWidget(m_generic);
            row->setFixedHeight(29);
            auto* line = new QHBoxLayout(row);
            line->setContentsMargins(4, 0, 0, 0);
            line->setSpacing(4);
            auto* name = new QLabel(dofNames[dof], row);
            name->setFixedWidth(22);
            line->addWidget(name);
            auto* modes = new QButtonGroup(row);
            const QString modeNames[3] = {tr("Libre"), tr("Fijo"), tr("Elástico")};
            for (int m = 0; m < 3; ++m) {
                auto* radio = new QRadioButton(modeNames[m], row);
                m_modeRadios[d][static_cast<std::size_t>(m)] = radio;
                modes->addButton(radio, m);
                line->addWidget(radio);
                boldWhenChecked(radio);
            }
            m_modeRadios[d][0]->setChecked(true);  // default: Libre
            auto* kLabel = new QLabel(QStringLiteral("K"), row);
            m_k[d] = new RangedEdit(QStringLiteral("K"), {kMinElasticConstant, kMaxElasticConstant}, 0, row);
            m_k[d]->setStrictNumber(true);
            m_k[d]->setFixedWidth(62);
            m_k[d]->setValue(kEdgeElasticConstantDefault);
            auto* unit = new QLabel(block == 0 ? QStringLiteral("kN/m²") : QStringLiteral("kN·m/rad.m"), row);
            const QList<QWidget*> springWidgets{kLabel, m_k[d], unit};
            line->addSpacing(4);
            for (QWidget* w : springWidgets) {
                line->addWidget(w);
                w->setVisible(false);
            }
            line->addStretch(1);
            connect(m_modeRadios[d][2], &QRadioButton::toggled, this, [springWidgets](bool on) {
                for (QWidget* w : springWidgets) w->setVisible(on);
            });
            m_modeRadios[d][2]->setToolTip(
                tr("<p><b>Elástico</b></p><p>Permite indicar al programa que el desplazamiento impedido en la coacción exterior seleccionada, es realmente "
                   "un apoyo elástico definido por el coeficiente K que usted puede introducir. Este coeficiente es la constante elástica del muelle que "
                   "impide el desplazamiento o el módulo de balasto del terreno, en las unidades indicadas.</p>"));
            right->addWidget(row);
        }
        if (block == 0) right->addSpacing(14);
    }
    right->addStretch(1);
    outer->addWidget(m_generic, 1);

    contentLayout()->setContentsMargins(0, 0, 0, 0);
    contentLayout()->addWidget(body, 1);
    setupFooter(tr("Aceptar"), tr("Cancelar"));
    load(initial);
}

void EdgeInteriorDialog::load(const EdgeInteriorBond& bond) {
    for (int d = 0; d < 6; ++d) {
        const auto i = static_cast<std::size_t>(d);
        m_modeRadios[i][static_cast<std::size_t>(bond.dof[i])]->setChecked(true);
        m_k[i]->setValue(bond.k[i]);
    }
    selectKind(bond.kind);
}

void EdgeInteriorDialog::selectKind(EdgeInteriorKind kind) {
    m_kindRadios[static_cast<std::size_t>(kind)]->setChecked(true);
}

EdgeInteriorBond EdgeInteriorDialog::bond() const {
    EdgeInteriorBond out;
    out.kind = m_kind;
    for (int d = 0; d < 6; ++d) {
        const auto i = static_cast<std::size_t>(d);
        for (int m = 0; m < 3; ++m)
            if (m_modeRadios[i][static_cast<std::size_t>(m)]->isChecked()) out.dof[i] = static_cast<DofMode>(m);
        out.k[i] = m_k[i]->value();
    }
    return normaliseEdgeInterior(out);
}

void EdgeInteriorDialog::accept() {
    if (m_kind == EdgeInteriorKind::Generic)
        for (RangedEdit* edit : m_k)
            if (edit->isVisible() && !edit->commit()) return;
    if (const auto issue = checkEdgeInterior(bond())) {
        messages::show(this, messages::Kind::Warning, QString::fromStdString(issue->text));
        return;
    }
    CypeDialog::accept();
}

// ---- Pandeo / Análisis modal --------------------------------------------------------------------------------------------------

ShellConsiderDialog::ShellConsiderDialog(Kind kind, bool considered, QWidget* parent)
    : CypeDialog(kind == Kind::Buckling ? tr("Pandeo") : tr("Análisis modal"), parent) {
    setFixedWidth(kind == Kind::Buckling ? 255 : 180);
    m_check = new QCheckBox(kind == Kind::Buckling ? tr("Considerar en el análisis de pandeo") : tr("Considerar en el análisis"), this);
    m_check->setChecked(considered);
    contentLayout()->setContentsMargins(10, 10, 10, 10);
    contentLayout()->addWidget(m_check);
    contentLayout()->addStretch(1);
    setupFooter(tr("Aceptar"), tr("Cancelar"));
}

bool ShellConsiderDialog::considered() const { return m_check->isChecked(); }

// ---- Integración de esfuerzos en bandas ---------------------------------------------------------------------------------------

BandForceDialog::BandForceDialog(const BandForceOptions& initial, QWidget* parent) : CypeDialog(tr("Integración de esfuerzos en bandas"), parent) {
    setFixedWidth(306);
    auto* body = new QWidget(this);
    auto* column = new QVBoxLayout(body);
    column->setContentsMargins(8, 8, 8, 4);
    column->setSpacing(4);
    m_internal = new QRadioButton(tr("A partir de los esfuerzos internos"), body);
    m_nodal = new QRadioButton(tr("A partir de las fuerzas nodales"), body);
    m_smooth = new QCheckBox(tr("Utilizar esfuerzos alisados"), body);
    auto* group = new QButtonGroup(this);
    group->addButton(m_internal);
    group->addButton(m_nodal);
    column->addWidget(m_internal);
    column->addWidget(m_nodal);
    auto* indent = new QHBoxLayout;
    indent->setContentsMargins(16, 0, 0, 0);
    indent->addWidget(m_smooth);
    column->addLayout(indent);
    (initial.method == BandForceMethod::InternalStresses ? m_internal : m_nodal)->setChecked(true);
    m_smooth->setChecked(initial.smoothedStresses);
    for (QRadioButton* radio : {m_internal, m_nodal}) boldWhenChecked(radio);
    m_smooth->setEnabled(m_internal->isChecked());  // smoothing only applies to the internal stresses (#201692 / #201693)
    connect(m_internal, &QRadioButton::toggled, m_smooth, &QWidget::setEnabled);
    contentLayout()->setContentsMargins(0, 0, 0, 0);
    contentLayout()->addWidget(body, 1);
    setupFooter(tr("Aceptar"), tr("Cancelar"));
}

BandForceOptions BandForceDialog::options() const {
    BandForceOptions out;
    out.method = m_internal->isChecked() ? BandForceMethod::InternalStresses : BandForceMethod::NodalForces;
    out.smoothedStresses = m_smooth->isChecked();
    return out;
}

// ---- Anchura / Número de divisiones -------------------------------------------------------------------------------------------

BandWidthDialog::BandWidthDialog(const QString& title, double widthCm, QWidget* parent) : CypeDialog(title, parent) {
    setFixedWidth(230);
    m_edit = new RangedEdit(tr("Anchura"), {kBandWidthMinM * 100.0, kBandWidthMaxM * 100.0}, 1, this);
    m_edit->setFixedWidth(62);
    m_edit->setValue(widthCm);
    contentLayout()->setContentsMargins(10, 10, 10, 10);
    contentLayout()->addWidget(labelledEdit(this, tr("Anchura"), m_edit, tr("cm"), 0));
    contentLayout()->addStretch(1);
    setupFooter(tr("Aceptar"), tr("Cancelar"));
    m_edit->setFocus();
    m_edit->selectAll();
}

double BandWidthDialog::widthM() const { return m_edit->value() / 100.0; }

void BandWidthDialog::accept() {
    if (!m_edit->commit()) return;
    CypeDialog::accept();
}

BandDivisionsDialog::BandDivisionsDialog(int parts, QWidget* parent) : CypeDialog(tr("Dividir"), parent) {
    setFixedWidth(260);
    m_edit = new RangedEdit(tr("Número de divisiones"), {static_cast<double>(kBandMinDivisions), static_cast<double>(kBandMaxDivisions)}, 0, this);
    m_edit->setFixedWidth(52);
    m_edit->setValue(parts);
    contentLayout()->setContentsMargins(10, 10, 10, 10);
    contentLayout()->addWidget(labelledEdit(this, tr("Número de divisiones"), m_edit, QString(), 0));
    contentLayout()->addStretch(1);
    setupFooter(tr("Aceptar"), tr("Cancelar"));
    m_edit->setFocus();
    m_edit->selectAll();
}

int BandDivisionsDialog::parts() const { return static_cast<int>(m_edit->value()); }

void BandDivisionsDialog::accept() {
    if (!m_edit->commit()) return;
    CypeDialog::accept();
}

}  // namespace cc::app
