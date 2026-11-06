#include "SupportDialogs.h"

#include <QButtonGroup>
#include <QFont>
#include <QIcon>
#include <algorithm>
#include <QComboBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPainter>
#include <QPushButton>
#include <QRadioButton>
#include <QStyle>
#include <QTableWidget>
#include <QToolButton>
#include <QVBoxLayout>
#include <cmath>

#include "RangedEdit.h"
#include "SupportPictograms.h"

namespace cc::app {

using namespace cc::core;

namespace {

constexpr const char* kDialogStyle =
    "QRadioButton { spacing: 4px; min-height: 22px; }"
    "QRadioButton::indicator { width: 12px; height: 12px; border-radius: 7px; border: 1px solid #6A6A6A; background: white; }"
    "QRadioButton::indicator:checked { width: 4px; height: 4px; border: 5px solid #0067C0; border-radius: 7px; background: white; }"
    "QRadioButton:checked { font-weight: bold; }"
    "QToolButton { background: #F4F4F4; border: 1px solid #B8B8B8; border-radius: 2px; }"
    "QToolButton:checked { background: #BCBCBC; border: 1px solid #888888; }"
    "QToolButton:disabled { background: #E4E4E4; }"
    "QLabel#supportBand { background: #D9D9D9; padding-left: 6px; min-height: 18px; max-height: 18px; }"
    "QWidget#supportWarning { background: #D3F0FC; border: 1px solid #B4E0F4; }"
    "QWidget#supportWarning QLabel { background: transparent; border: none; }"
    "QTableWidget { background: white; gridline-color: #E2E2E2; }"
    "QHeaderView::section { background: white; border: none; border-right: 1px solid #E2E2E2; border-bottom: 1px solid #E2E2E2; padding: 3px; }";

std::function<bool(QWidget*, const QString&)>& questionHandler() {
    static std::function<bool(QWidget*, const QString&)> h;
    return h;
}

const char* kAxisNames[3] = {"X", "Y", "Z"};
const char* kPlaneOf[3] = {"YZ", "XZ", "XY"};  // plane perpendicular to the axis

QString axisName(int i) { return QString::fromLatin1(kAxisNames[i]); }
QString dofName(int i) { return QString::fromLatin1(i < 3 ? "D" : "G") + axisName(i % 3).toLower(); }

}  // namespace

// ===== ExternalSupportDialog ===================================================================================================

ExternalSupportDialog::ExternalSupportDialog(const ExternalSupport& initial, QWidget* parent)
    : CypeDialog(tr("Vinculación exterior"), parent) {
    setFixedSize(591, 414);
    // The sheet goes on the containers: a rule added to the dialog after its base sheet does not reach the children (checked).
    auto* grid = new QWidget(this);
    grid->setStyleSheet(QString::fromLatin1(kDialogStyle));
    buildGrid(grid);
    contentLayout()->addWidget(grid);
    m_panel = new QWidget(this);
    m_panel->setStyleSheet(QString::fromLatin1(kDialogStyle));
    buildPanel(m_panel);
    contentLayout()->addWidget(m_panel, 1);
    setupFooter(tr("Aceptar"), tr("Cancelar"));
    contentLayout()->setSpacing(4);
    load(initial);
}

QString ExternalSupportDialog::tooltipTitle(ExternalSupportType type) {
    const SupportTypeInfo info = supportTypeInfo(type);
    const int v = supportTypeIndex(type);
    const bool pinned = v >= 12;
    const QString withoutRotations = pinned ? tr(" con giros sin coaccionar") : QString();
    switch (info.kind) {
        case SupportKind::Free: return tr("Libre");
        case SupportKind::Pinned: return tr("Articulación");
        case SupportKind::Generic: return tr("Genérica");
        case SupportKind::Fixed: return tr("Empotramiento");
        case SupportKind::Line:
            if (info.userVector) return tr("Desplazamiento libre sobre una recta cualquiera") + withoutRotations;
            return tr("Desplazamiento libre sobre una recta en la dirección %1").arg(axisName(info.axis)) + withoutRotations;
        case SupportKind::Plane:
            if (info.userVector) return tr("Desplazamiento libre sobre un plano cualquiera") + withoutRotations;
            return tr("Desplazamiento libre sobre un plano paralelo a los ejes %1").arg(QString::fromLatin1(kPlaneOf[info.axis])) + withoutRotations;
    }
    return {};
}

QString ExternalSupportDialog::tooltipText(ExternalSupportType type) {
    const SupportTypeInfo info = supportTypeInfo(type);
    const bool pinned = supportTypeIndex(type) >= 12;
    switch (info.kind) {
        case SupportKind::Free: return tr("Nudo sin vinculación exterior");
        case SupportKind::Pinned: return tr("Apoyo fijo. Impide el desplazamiento en cualquier dirección.");
        case SupportKind::Generic: return tr("Nudo con vinculaciones definidas por el usuario.");
        case SupportKind::Fixed: return tr("Empotramiento. Impide el desplazamiento y el giro en cualquier dirección.");
        case SupportKind::Line:
            if (info.userVector)
                return pinned ? tr("Apoyo con desplazamiento libre en una recta cualquiera (se define con su vector director) y desplazamiento impedido en cualquier plano perpendicular a la recta donde tiene libre desplazamiento.")
                              : tr("Empotramiento con desplazamiento libre en una recta cualquiera (se define con su vector director) y desplazamiento impedido en cualquier plano perpendicular a dicha recta.");
            return pinned ? tr("Apoyo con desplazamiento libre en una recta paralela al %1 global y desplazamiento impedido en un plano paralelo al plano %2 global.")
                                .arg(axisName(info.axis), QString::fromLatin1(kPlaneOf[info.axis]))
                          : tr("Empotramiento con desplazamiento libre en el eje %1 (impedido en el plano %2 global).")
                                .arg(axisName(info.axis), QString::fromLatin1(kPlaneOf[info.axis]));
        case SupportKind::Plane: {
            if (info.userVector)
                return pinned ? tr("Apoyo con desplazamiento libre en un plano cualquiera (se define con su vector director) y desplazamiento impedido en cualquier recta perpendicular al plano donde tiene libre desplazamiento.")
                              : tr("Empotramiento con desplazamiento libre en un plano cualquiera (se define con su vector director) y desplazamiento impedido fuera de dicho plano.");
            const QString plane = QString::fromLatin1(kPlaneOf[info.axis]);  // the stored axis is the blocked one: Z -> plane "XY"
            return pinned ? tr("Apoyo con desplazamiento libre en un plano paralelo al %1 global y desplazamiento impedido en una recta paralela al eje %2 global.")
                                .arg(plane, axisName(info.axis))
                          : tr("Empotramiento con desplazamiento libre en el plano %1 (impedido en la dirección %2 global).")
                                .arg(plane, axisName(info.axis));
        }
    }
    return {};
}

void ExternalSupportDialog::setQuestionHandler(std::function<bool(QWidget*, const QString&)> handler) { questionHandler() = std::move(handler); }

bool ExternalSupportDialog::ask(QWidget* parent, const QString& text) {
    if (questionHandler()) return questionHandler()(parent, text);
    QMessageBox box(QMessageBox::Question, tr("Pregunta"), text, QMessageBox::NoButton, parent);
    QPushButton* yes = box.addButton(tr("Sí"), QMessageBox::YesRole);
    box.addButton(tr("No"), QMessageBox::NoRole);
    box.setDefaultButton(yes);
    box.exec();
    return box.clickedButton() == yes;
}

void ExternalSupportDialog::buildGrid(QWidget* parent) {
    auto* layout = new QGridLayout(parent);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setHorizontalSpacing(6);
    layout->setVerticalSpacing(6);
    // Measured order (orig_vinc_ext_dialogo_inicial.png): row 1 = genérica, empotramiento, rectas X Y Z, recta usuario, planos XY XZ YZ,
    // plano usuario (empotradas); row 2 = libre, articulación, the same eight articuladas.
    const int order[2][10] = {{2, 3, 4, 5, 6, 7, 8, 9, 10, 11}, {0, 1, 12, 13, 14, 15, 16, 17, 18, 19}};
    const qreal dpr = devicePixelRatioF();
    auto* group = new QButtonGroup(parent);
    group->setExclusive(true);
    for (int row = 0; row < 2; ++row)
        for (int col = 0; col < 10; ++col) {
            const auto type = static_cast<ExternalSupportType>(order[row][col]);
            auto* button = new QToolButton(parent);
            button->setObjectName(QStringLiteral("supportButton"));
            button->setCheckable(true);
            button->setFixedSize(40, 40);
            button->setIconSize(QSize(34, 34));
            button->setIcon(QIcon(externalSupportPictogram(type, 34, dpr)));
            button->setToolTip(QStringLiteral("<p><b>%1</b><br/>%2</p>").arg(tooltipTitle(type).toHtmlEscaped(), tooltipText(type).toHtmlEscaped()));
            button->setAccessibleName(tooltipTitle(type));
            group->addButton(button);
            m_buttons[static_cast<std::size_t>(order[row][col])] = button;
            layout->addWidget(button, row, col);
            connect(button, &QToolButton::clicked, this, [this, type] { selectType(type); });
        }
    layout->setColumnStretch(10, 1);
}

void ExternalSupportDialog::buildPanel(QWidget* parent) {
    auto* layout = new QVBoxLayout(parent);
    layout->setContentsMargins(0, 8, 0, 0);
    layout->setSpacing(2);

    // Vector row: X Y Z.
    m_vectorRow = new QWidget(parent);
    auto* vec = new QHBoxLayout(m_vectorRow);
    vec->setContentsMargins(0, 0, 0, 0);
    vec->addSpacing(60);
    const ValueRange vectorRange{-kMaxVectorComponent, kMaxVectorComponent};
    for (int i = 0; i < 3; ++i) {
        vec->addWidget(new QLabel(axisName(i), m_vectorRow));
        auto* edit = new RangedEdit(axisName(i), vectorRange, 3, m_vectorRow);
        edit->setStrictNumber(true);
        edit->setFixedWidth(64);
        edit->setToolTip(QStringLiteral("<p><b>%1</b><br/>Debe estar entre -1000.000 y 1000.000 .</p>").arg(axisName(i)));  // literal, measured
        connect(edit, &RangedEdit::valueCommitted, this, [this] {
            if (!m_loading) refreshPanel();
        });
        m_vector[static_cast<std::size_t>(i)] = edit;
        vec->addWidget(edit);
    }
    vec->addStretch(1);
    layout->addWidget(m_vectorRow);

    const auto band = [&](const QString& text) {
        auto* label = new QLabel(text, parent);
        label->setObjectName(QStringLiteral("supportBand"));
        return label;
    };
    const auto makeRows = [&](QWidget* container, int first) {
        auto* column = new QVBoxLayout(container);
        column->setContentsMargins(0, 0, 0, 0);
        column->setSpacing(0);
        for (int i = first; i < first + 3; ++i) {
            DofRow& r = m_rows[static_cast<std::size_t>(i)];
            r.row = new QWidget(container);
            auto* h = new QHBoxLayout(r.row);
            h->setContentsMargins(0, 0, 0, 0);
            h->setSpacing(4);
            auto* name = new QLabel(dofName(i), r.row);
            name->setFixedWidth(22);
            h->addWidget(name);
            auto* group = new QButtonGroup(r.row);
            r.free = new QRadioButton(tr("Libre"), r.row);
            r.fixed = new QRadioButton(tr("Fijo"), r.row);
            r.elastic = new QRadioButton(tr("Elástico"), r.row);
            for (QRadioButton* radio : {r.free, r.fixed, r.elastic}) {
                group->addButton(radio);
                h->addWidget(radio);
            }
            r.fixed->setChecked(true);
            auto* kLabel = new QLabel(QStringLiteral("K"), r.row);
            r.k = new RangedEdit(QStringLiteral("K"), ValueRange{kMinElasticConstant, kMaxElasticConstant}, 0, r.row);
            r.k->setStrictNumber(true);
            r.k->setFixedWidth(70);
            r.k->setValue(kDefaultElasticConstant);
            r.unit = new QLabel(i < 3 ? QStringLiteral("kN/m") : QStringLiteral("kN·m/rad"), r.row);
            auto* senseLabel = new QLabel(tr("Sentido"), r.row);
            r.sense = new QComboBox(r.row);
            r.sense->addItems({tr("Ambos"), tr("Positivo"), tr("Negativo")});
            const QList<QWidget*> springWidgets{kLabel, r.k, r.unit, senseLabel, r.sense};
            h->addSpacing(6);
            for (QWidget* w : springWidgets) {
                h->addWidget(w);
                w->setVisible(false);
            }
            h->addStretch(1);
            connect(r.elastic, &QRadioButton::toggled, this, [springWidgets](bool on) {
                for (QWidget* w : springWidgets) w->setVisible(on);
            });
            r.row->setFixedHeight(30);
            column->addWidget(r.row);
        }
    };

    m_displacementsBand = band(tr("Desplazamientos"));
    layout->addWidget(m_displacementsBand);
    auto* dispLine = new QWidget(parent);
    auto* dispH = new QHBoxLayout(dispLine);
    dispH->setContentsMargins(0, 0, 0, 0);
    m_modeBox = new QWidget(dispLine);
    auto* modes = new QVBoxLayout(m_modeBox);
    modes->setContentsMargins(0, 0, 8, 0);
    modes->setSpacing(0);
    auto* modeGroup = new QButtonGroup(m_modeBox);
    const QString modeText[3] = {tr("Independientes"), tr("Sobre una recta"), tr("Sobre un plano")};
    for (int i = 0; i < 3; ++i) {
        m_modeRadios[static_cast<std::size_t>(i)] = new QRadioButton(modeText[i], m_modeBox);
        modeGroup->addButton(m_modeRadios[static_cast<std::size_t>(i)]);
        modes->addWidget(m_modeRadios[static_cast<std::size_t>(i)]);
        connect(m_modeRadios[static_cast<std::size_t>(i)], &QRadioButton::toggled, this, [this, i](bool on) {
            if (!on || m_loading) return;
            // Changing the mode starts from its defaults: independent = Libre, on a line / plane = Fijo, with the default vector.
            const auto mode = static_cast<GenericDisplacementMode>(i);
            m_loading = true;
            setVector(mode == GenericDisplacementMode::OnLine ? Point3{1, 0, 0} : Point3{0, 0, 1});
            for (int d = 0; d < 3; ++d) {
                DofRow& r = m_rows[static_cast<std::size_t>(d)];
                (mode == GenericDisplacementMode::Independent ? r.free : r.fixed)->setChecked(true);
            }
            m_loading = false;
            refreshPanel();
        });
    }
    m_loading = true;  // the rows do not exist yet: the mode handler must not run
    m_modeRadios[0]->setChecked(true);
    m_loading = false;
    dispH->addWidget(m_modeBox, 0, Qt::AlignTop);
    m_displacementsRows = new QWidget(dispLine);
    makeRows(m_displacementsRows, 0);
    dispH->addWidget(m_displacementsRows, 1);
    layout->addWidget(dispLine);

    m_rotationsBand = band(tr("Giros"));
    layout->addWidget(m_rotationsBand);
    m_rotationsRows = new QWidget(parent);
    makeRows(m_rotationsRows, 3);
    layout->addWidget(m_rotationsRows);
    layout->addStretch(1);
}

GenericDisplacementMode ExternalSupportDialog::genericMode() const {
    for (int i = 0; i < 3; ++i)
        if (m_modeRadios[static_cast<std::size_t>(i)]->isChecked()) return static_cast<GenericDisplacementMode>(i);
    return GenericDisplacementMode::Independent;
}

Point3 ExternalSupportDialog::vector() const { return {m_vector[0]->value(), m_vector[1]->value(), m_vector[2]->value()}; }

void ExternalSupportDialog::setVector(const Point3& v) {
    m_vector[0]->setValue(v.x);
    m_vector[1]->setValue(v.y);
    m_vector[2]->setValue(v.z);
}

void ExternalSupportDialog::selectType(ExternalSupportType type) {
    m_buttons[static_cast<std::size_t>(supportTypeIndex(type))]->setChecked(true);
    if (type == m_type) return;  // pressing the selected button again changes nothing
    m_type = type;
    applyDefaults(type);
}

void ExternalSupportDialog::applyDefaults(ExternalSupportType type) {
    m_loading = true;
    m_type = type;
    const SupportTypeInfo info = supportTypeInfo(type);
    Point3 v{0, 0, 1};
    if (info.kind == SupportKind::Line) v = info.userVector ? Point3{1, 0, 0} : Point3{info.axis == 0 ? 1.0 : 0.0, info.axis == 1 ? 1.0 : 0.0, info.axis == 2 ? 1.0 : 0.0};
    if (info.kind == SupportKind::Plane) v = info.userVector ? Point3{0, 0, 1} : Point3{info.axis == 0 ? 1.0 : 0.0, info.axis == 1 ? 1.0 : 0.0, info.axis == 2 ? 1.0 : 0.0};
    setVector(v);
    m_modeRadios[0]->setChecked(true);
    for (std::size_t i = 0; i < 6; ++i) {
        DofRow& r = m_rows[i];
        r.k->setValue(kDefaultElasticConstant);
        r.sense->setCurrentIndex(0);
        (type == ExternalSupportType::Generic ? r.free : r.fixed)->setChecked(true);
    }
    m_loading = false;
    refreshPanel();
}

ExternalSupport ExternalSupportDialog::probe() const {
    ExternalSupport s;
    s.type = m_type;
    s.userVector = vector();
    s.generic.mode = genericMode();
    s.generic.vector = vector();
    return s;
}

void ExternalSupportDialog::refreshRow(int index, bool visible, bool allowFree) {
    DofRow& r = m_rows[static_cast<std::size_t>(index)];
    m_visible[static_cast<std::size_t>(index)] = visible;
    r.row->setVisible(visible);
    r.free->setVisible(allowFree);
    if (!allowFree && r.free->isChecked()) r.fixed->setChecked(true);
}

void ExternalSupportDialog::refreshPanel() {
    const SupportTypeInfo info = supportTypeInfo(m_type);
    const bool isFree = info.kind == SupportKind::Free;
    const bool generic = genericSelected();
    const GenericDisplacementMode mode = genericMode();
    const bool dependent = generic && mode != GenericDisplacementMode::Independent;
    const bool vectorShown = info.kind == SupportKind::Line || info.kind == SupportKind::Plane || dependent;
    m_vectorRow->setVisible(vectorShown);
    for (RangedEdit* edit : m_vector) edit->setEnabled(info.userVector || generic);  // greyed for the predefined axes
    m_modeBox->setVisible(generic);

    // Displacements shown: the ones the type blocks (measured: recta X -> Dy, Dz; plano XY -> Dz; ...), derived from the mechanics so that
    // an edited user vector parallel to an axis follows it. A skew vector blocks no global displacement: no row (PROVISIONAL, TODO(medir)).
    const Restraint probed = restraintOf(probe());
    const bool rotations = !isFree && (generic || info.rotationsFixed);
    bool anyDisplacement = false;
    for (int i = 0; i < 6; ++i) {
        bool visible = false;
        bool allowFree = false;
        if (!isFree) {
            if (i < 3) {
                visible = (generic && mode == GenericDisplacementMode::Independent) || probed.fixed[static_cast<std::size_t>(i)];
                allowFree = generic && mode == GenericDisplacementMode::Independent;
                anyDisplacement = anyDisplacement || visible;
            } else {
                visible = rotations;
                allowFree = generic;
            }
        }
        refreshRow(i, visible, allowFree);
    }
    m_displacementsBand->setVisible(!isFree);
    m_displacementsRows->setVisible(anyDisplacement);
    m_rotationsBand->setVisible(rotations);
    m_rotationsRows->setVisible(rotations);
}

bool ExternalSupportDialog::dofVisible(int index) const { return index >= 0 && index < 6 && m_visible[static_cast<std::size_t>(index)]; }
bool ExternalSupportDialog::vectorVisible() const {
    const SupportTypeInfo info = supportTypeInfo(m_type);
    return info.kind == SupportKind::Line || info.kind == SupportKind::Plane ||
           (genericSelected() && genericMode() != GenericDisplacementMode::Independent);
}
bool ExternalSupportDialog::vectorEditable() const { return supportTypeInfo(m_type).userVector || genericSelected(); }
bool ExternalSupportDialog::rotationsVisible() const { return m_visible[3]; }

ExternalSupport ExternalSupportDialog::support() const {
    ExternalSupport s;
    s.type = m_type;
    const SupportTypeInfo info = supportTypeInfo(m_type);
    const bool generic = genericSelected();
    if (!generic && info.userVector) s.userVector = vector();
    if (generic) {
        s.generic.mode = genericMode();
        s.generic.vector = vector();
    }
    for (std::size_t i = 0; i < 6; ++i) {
        const DofRow& r = m_rows[i];
        s.elasticConstants[i] = r.k->value();
        s.senses[i] = static_cast<SupportSense>(std::max(0, r.sense->currentIndex()));
        DofMode m = DofMode::Fixed;
        if (r.free->isChecked() && r.free->isVisibleTo(r.row)) m = DofMode::Free;
        else if (r.elastic->isChecked()) m = DofMode::Elastic;
        if (generic) {
            if (i < 3) s.generic.translations[i] = m_visible[i] ? m : DofMode::Free;
            else s.generic.rotations[i - 3] = m_visible[i] ? m : DofMode::Free;
        } else {
            s.elasticDofs[i] = m_visible[i] && m == DofMode::Elastic;
        }
    }
    return s;
}

void ExternalSupportDialog::load(const ExternalSupport& initial) {
    applyDefaults(initial.type);
    m_buttons[static_cast<std::size_t>(supportTypeIndex(initial.type))]->setChecked(true);
    m_loading = true;
    const bool generic = initial.type == ExternalSupportType::Generic;
    if (generic) {
        m_modeRadios[static_cast<std::size_t>(initial.generic.mode)]->setChecked(true);
        setVector(initial.generic.vector);
    } else if (supportTypeInfo(initial.type).userVector) {
        setVector(initial.userVector);
    }
    refreshPanel();  // the visible rows follow the mode and the vector
    for (std::size_t i = 0; i < 6; ++i) {
        DofRow& r = m_rows[i];
        r.k->setValue(std::round(initial.elasticConstants[i]));
        r.sense->setCurrentIndex(static_cast<int>(initial.senses[i]));
        DofMode m = DofMode::Fixed;
        if (generic) m = i < 3 ? initial.generic.translations[i] : initial.generic.rotations[i - 3];
        else if (initial.elasticDofs[i]) m = DofMode::Elastic;
        if (m == DofMode::Free && r.free->isVisibleTo(r.row)) r.free->setChecked(true);
        else if (m == DofMode::Elastic) r.elastic->setChecked(true);
        else r.fixed->setChecked(true);
    }
    m_loading = false;
}

void ExternalSupportDialog::accept() {
    // Each visible field is read now: a wrong one warns and the window stays open.
    bool ok = true;
    if (vectorVisible())
        for (RangedEdit* edit : m_vector) ok = edit->commit() && ok;
    for (int i = 0; i < 6; ++i)
        if (m_visible[static_cast<std::size_t>(i)] && m_rows[static_cast<std::size_t>(i)].elastic->isChecked()) ok = m_rows[static_cast<std::size_t>(i)].k->commit() && ok;
    if (!ok) return;
    // A zero vector first (measured: it came before the question about K).
    if (vectorVisible()) {
        const Point3 v = vector();
        if (v.x == 0.0 && v.y == 0.0 && v.z == 0.0) {
            RangedEdit::showWarning(this, tr("La dirección introducida no es correcta."));
            return;
        }
    }
    if (elasticConstantIsDefault(support()) && !ask(this, tr("El valor introducido para la constante de apoyo elástico (%1) coincide con el valor por defecto. Recuerde que debe asignar un valor conforme al grado de empotramiento que posee la vinculación exterior en el nudo. ¿Desea continuar?").arg(QStringLiteral("K"))))
        return;
    CypeDialog::accept();
}

// ===== LinkSketch ==============================================================================================================

LinkSketch::LinkSketch(QWidget* parent) : QWidget(parent) {
    setAutoFillBackground(false);
    setMinimumSize(300, 200);
}

void LinkSketch::setPieces(const std::vector<Point3>& directions) {
    m_angles.clear();
    const std::size_t n = directions.size();
    for (std::size_t i = 0; i < n; ++i) {
        const Point3& d = directions[i];
        // Elevation-like view: x to the right, z up, y slightly oblique; degenerate directions are spread round the node.
        const double sx = d.x + 0.35 * d.y, sy = d.z + 0.35 * d.y;
        m_angles.push_back(std::hypot(sx, sy) < 1e-9 ? 2.0 * 3.14159265358979323846 * static_cast<double>(i) / static_cast<double>(n)
                                                     : std::atan2(sy, sx));
    }
    update();
}

void LinkSketch::setState(const std::vector<int>& elementRow, const std::vector<bool>& partial) {
    m_rows = elementRow;
    m_partial = partial;
    update();
}

double LinkSketch::angleOf(int piece) const { return piece >= 0 && piece < static_cast<int>(m_angles.size()) ? m_angles[static_cast<std::size_t>(piece)] : 0.0; }

bool LinkSketch::circleDrawn() const {
    for (std::size_t i = 0; i < m_rows.size(); ++i)
        for (std::size_t j = i + 1; j < m_rows.size(); ++j)
            if (m_rows[i] == m_rows[j]) return true;
    return false;
}

bool LinkSketch::pieceRed(int piece) const {
    if (piece < 0 || piece >= static_cast<int>(m_rows.size())) return false;
    const int row = m_rows[static_cast<std::size_t>(piece)];
    return row >= 0 && row < static_cast<int>(m_partial.size()) && m_partial[static_cast<std::size_t>(row)];
}

void LinkSketch::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), Qt::white);
    p.setPen(QColor(0xC8, 0xC8, 0xC8));
    p.drawRect(rect().adjusted(0, 0, -1, -1));
    const QPointF centre(width() / 2.0, height() * 0.6);
    const double arm = std::min(width(), height()) * 0.42;
    const double hub = circleDrawn() ? 14.0 : 9.0;
    if (circleDrawn()) {
        p.setPen(QPen(Qt::black, 1.2));
        p.setBrush(Qt::white);
        p.drawEllipse(centre, hub, hub);
    }
    QFont label = font();
    label.setPixelSize(14);
    for (std::size_t i = 0; i < m_angles.size(); ++i) {
        const QPointF dir(std::cos(m_angles[i]), -std::sin(m_angles[i]));
        const QPointF from = centre + dir * (hub + 2.0), to = centre + dir * arm;
        p.setPen(QPen(pieceRed(static_cast<int>(i)) ? QColor(0xE0, 0x10, 0x10) : Qt::black, 1.2));
        p.drawLine(from, to);
        p.setBrush(Qt::black);
        p.drawEllipse(from, 1.8, 1.8);
        p.setFont(label);
        p.setPen(QColor(0xFF, 0x00, 0xFF));
        const QPointF at = to + dir * 14.0;
        p.drawText(QRectF(at.x() - 10, at.y() - 10, 20, 20), Qt::AlignCenter, QString(QChar('A' + static_cast<int>(i % 26))));
    }
}

// ===== InternalLinkDialog ======================================================================================================

QString InternalLinkDialog::tooltipText(InternalLinkType type) {
    switch (type) {
        case InternalLinkType::Fixed: return tr("Nudo empotrado");
        case InternalLinkType::PartialFixity: return tr("Nudo empotrado con coeficiente de empotramiento parcial para todas las piezas");
        case InternalLinkType::Pinned: return tr("Nudo articulado");
        case InternalLinkType::Generic: return tr("Nudo genérico");
    }
    return {};
}

InternalLinkDialog::InternalLinkDialog(const InternalLink& initial, const std::vector<Point3>& directions, bool mixedPieceCounts, QWidget* parent)
    : CypeDialog(tr("Vinculación interior"), parent), m_pieces(directions.size()), m_mixed(mixedPieceCounts), m_directions(directions) {
    auto* top = new QHBoxLayout;
    top->setSpacing(2);
    auto* group = new QButtonGroup(this);
    const qreal dpr = devicePixelRatioF();
    for (int i = 0; i < 4; ++i) {
        const auto type = static_cast<InternalLinkType>(i);
        auto* button = new QToolButton(this);
        button->setObjectName(QStringLiteral("supportButton"));
        button->setStyleSheet(QString::fromLatin1(kDialogStyle));
        button->setCheckable(true);
        button->setFixedSize(38, 38);
        button->setIconSize(QSize(32, 32));
        button->setIcon(QIcon(internalLinkPictogram(type, 32, dpr)));
        button->setToolTip(tooltipText(type));
        button->setAccessibleName(tooltipText(type));
        group->addButton(button);
        m_buttons[static_cast<std::size_t>(i)] = button;
        top->addWidget(button);
        connect(button, &QToolButton::clicked, this, [this, type] { selectType(type); });
    }
    top->addStretch(1);
    contentLayout()->addLayout(top);
    m_buttons[3]->setEnabled(m_pieces > 0);

    auto* coefRow = new QHBoxLayout;
    m_coefficientLabel = new QLabel(tr("Coef.Emp.Parc."), this);
    m_coefficient = new RangedEdit(tr("Coef.Emp.Parc."), ValueRange{0.0, kMaxPartialCoefficient}, 2, this);
    m_coefficient->setStrictNumber(true);
    m_coefficient->setFixedWidth(80);
    m_coefficient->setToolTip(QStringLiteral("<p><b>%1</b><br/>%2<br/>%3</p>")
                                  .arg(tr("Coef.Emp.Parc."),
                                       tr("Define el grado de empotramiento entre las piezas que llegan al nudo. El valor de este coeficiente puede estar entre 0 (articulado) e infinito o valor muy grande (empotrado). En otra opción, el programa puede definir otro coeficiente de empotramiento entre pieza y nudo, que tiene valores comprendidos entre 0 y 1."),
                                       tr("Debe estar entre 0.00 y 10000000.00.")));
    m_coefficientUnit = new QLabel(QStringLiteral("kN·m/rad"), this);
    coefRow->addWidget(m_coefficientLabel);
    coefRow->addWidget(m_coefficient);
    coefRow->addWidget(m_coefficientUnit);
    coefRow->addStretch(1);
    contentLayout()->addLayout(coefRow);

    m_warningBox = new QWidget(this);
    m_warningBox->setObjectName(QStringLiteral("supportWarning"));
    m_warningBox->setStyleSheet(QString::fromLatin1(kDialogStyle));
    auto* warningRow = new QHBoxLayout(m_warningBox);
    warningRow->setContentsMargins(10, 6, 10, 6);
    auto* warningIcon = new QLabel(m_warningBox);
    warningIcon->setPixmap(style()->standardIcon(QStyle::SP_MessageBoxWarning).pixmap(18, 18));
    m_warning = new QLabel(tr("Ha seleccionado nudos con distinto número de piezas. La configuración de la vinculación interior genérica editada sólo se aplicará a nudos con el mismo número de piezas."), m_warningBox);
    m_warning->setWordWrap(true);
    warningRow->addWidget(warningIcon, 0, Qt::AlignTop);
    warningRow->addWidget(m_warning, 1);
    contentLayout()->addWidget(m_warningBox);

    m_generic = new QWidget(this);
    auto* gl = new QHBoxLayout(m_generic);
    gl->setContentsMargins(0, 0, 0, 0);
    gl->setSpacing(12);
    m_sketch = new LinkSketch(m_generic);
    m_sketch->setPieces(directions);
    gl->addWidget(m_sketch, 4);
    m_table = new QTableWidget(m_generic);
    m_table->setStyleSheet(QString::fromLatin1(kDialogStyle));
    buildTable();
    gl->addWidget(m_table, 5);
    contentLayout()->addWidget(m_generic, 1);

    setupFooter(tr("Aceptar"), tr("Cancelar"));

    // Initial state.
    m_coefficient->setValue(initial.partialCoefficient);
    m_type = initial.type == InternalLinkType::Generic && m_pieces == 0 ? InternalLinkType::Fixed : initial.type;
    if (initial.type == InternalLinkType::Generic && m_pieces > 0 && initial.elementRow.size() == m_pieces) {
        m_updating = true;
        for (std::size_t r = 0; r < m_pieces; ++r) {
            const GenericLinkRow row = r < initial.rows.size() ? initial.rows[r] : GenericLinkRow{};
            m_table->item(static_cast<int>(r), 1)->setCheckState(row.partialToNode ? Qt::Checked : Qt::Unchecked);
            m_table->item(static_cast<int>(r), 2)->setData(Qt::UserRole, row.coefficient);
            m_table->item(static_cast<int>(r), 2)->setText(QString::number(row.coefficient, 'f', 3));
        }
        for (std::size_t c = 0; c < m_pieces; ++c)
            for (std::size_t r = 0; r < m_pieces; ++r)
                m_table->item(static_cast<int>(r), 3 + static_cast<int>(c))
                    ->setCheckState(static_cast<std::size_t>(std::max(0, initial.elementRow[c])) == r ? Qt::Checked : Qt::Unchecked);
        m_updating = false;
    }
    m_buttons[static_cast<std::size_t>(m_type)]->setChecked(true);
    syncSketch();
    refreshPanel();
}

void InternalLinkDialog::buildTable() {
    const int n = static_cast<int>(m_pieces);
    m_table->setRowCount(n);
    m_table->setColumnCount(3 + n);
    QStringList headers{tr("Vínc"), QString(), tr("Coef.Emp.Parc.")};
    for (int c = 0; c < n; ++c) headers << QString(QChar('A' + c % 26));
    m_table->setHorizontalHeaderLabels(headers);
    m_table->verticalHeader()->setVisible(false);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed | QAbstractItemView::AnyKeyPressed);
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_table->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_table->horizontalHeader()->setHighlightSections(false);
    m_table->setColumnWidth(0, 56);
    m_table->setColumnWidth(1, 56);
    m_table->setColumnWidth(2, 120);
    for (int c = 0; c < n; ++c) m_table->setColumnWidth(3 + c, 56);
    m_table->setShowGrid(false);
    m_updating = true;
    for (int r = 0; r < n; ++r) {
        m_table->setRowHeight(r, 20);
        auto* number = new QTableWidgetItem(QString::number(r));
        number->setFlags(Qt::ItemIsEnabled);
        number->setTextAlignment(Qt::AlignCenter);
        m_table->setItem(r, 0, number);
        auto* partial = new QTableWidgetItem;
        partial->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
        partial->setCheckState(Qt::Unchecked);
        m_table->setItem(r, 1, partial);
        auto* coefficient = new QTableWidgetItem(QStringLiteral("0.000"));
        coefficient->setFlags(Qt::ItemIsEditable | Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        coefficient->setTextAlignment(Qt::AlignCenter);
        coefficient->setData(Qt::UserRole, 0.0);
        m_table->setItem(r, 2, coefficient);
        for (int c = 0; c < n; ++c) {
            auto* mark = new QTableWidgetItem;
            mark->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
            mark->setCheckState(r == 0 ? Qt::Checked : Qt::Unchecked);  // default: every piece in link 0 (all fixed to each other)
            m_table->setItem(r, 3 + c, mark);
        }
    }
    m_updating = false;
    connect(m_table, &QTableWidget::itemChanged, this, &InternalLinkDialog::onItemChanged);
}

void InternalLinkDialog::onItemChanged(QTableWidgetItem* item) {
    if (m_updating || !item) return;
    m_updating = true;
    const int row = item->row(), col = item->column();
    if (col == 2) {
        bool ok = false;
        const double typed = item->text().trimmed().toDouble(&ok);
        const double previous = item->data(Qt::UserRole).toDouble();
        if (!ok || !std::isfinite(typed)) {
            item->setText(QString::number(previous, 'f', 3));
            RangedEdit::showWarning(this, tr("La cadena introducida no es un número."));
        } else if (typed < 0.0 || typed > kMaxPartialCoefficient) {
            item->setText(QString::number(previous, 'f', 3));
            RangedEdit::showWarning(this, tr("El valor de 'Coef.Emp.Parc.' debe estar entre 0.00 y 10000000.00."));
        } else {
            item->setData(Qt::UserRole, typed);
            item->setText(QString::number(typed, 'f', 3));
        }
    } else if (col >= 3) {
        if (item->checkState() == Qt::Checked) {  // one mark per piece: it leaves the row where it was
            for (int r = 0; r < m_table->rowCount(); ++r)
                if (r != row) m_table->item(r, col)->setCheckState(Qt::Unchecked);
        } else {  // a piece cannot be left without a mark
            item->setCheckState(Qt::Checked);
        }
    }
    m_updating = false;
    syncSketch();
}

void InternalLinkDialog::markPiece(int piece, int row) {
    if (piece < 0 || piece >= static_cast<int>(m_pieces) || row < 0 || row >= m_table->rowCount()) return;
    m_table->item(row, 3 + piece)->setCheckState(Qt::Checked);
}

void InternalLinkDialog::setPartial(int row, bool on) {
    if (row >= 0 && row < m_table->rowCount()) m_table->item(row, 1)->setCheckState(on ? Qt::Checked : Qt::Unchecked);
}

void InternalLinkDialog::setRowCoefficient(int row, double value) {
    if (row >= 0 && row < m_table->rowCount()) m_table->item(row, 2)->setText(QString::number(value, 'f', 3));
}

void InternalLinkDialog::syncSketch() {
    const InternalLink l = link();
    std::vector<bool> partial;
    for (const GenericLinkRow& r : l.rows) partial.push_back(r.partialToNode);
    m_sketch->setState(l.type == InternalLinkType::Generic ? l.elementRow : std::vector<int>(m_pieces, 0), partial);
}

void InternalLinkDialog::selectType(InternalLinkType type) {
    m_buttons[static_cast<std::size_t>(type)]->setChecked(true);
    if (type == InternalLinkType::Generic && m_pieces == 0) return;
    m_type = type;
    refreshPanel();
}

void InternalLinkDialog::refreshPanel() {
    const bool partial = m_type == InternalLinkType::PartialFixity;
    const bool generic = m_type == InternalLinkType::Generic;
    m_coefficientLabel->setVisible(partial);
    m_coefficient->setVisible(partial);
    m_coefficientUnit->setVisible(partial);
    m_generic->setVisible(generic);
    m_warningBox->setVisible(generic && m_mixed);
    setFixedSize(748, generic && m_mixed ? 503 : 449);  // measured: the band adds 54 px
}

InternalLink InternalLinkDialog::link() const {
    InternalLink l;
    l.type = m_type;
    if (m_type == InternalLinkType::PartialFixity) l.partialCoefficient = m_coefficient->value();
    if (m_type != InternalLinkType::Generic) return l;
    const int n = static_cast<int>(m_pieces);
    l.elementRow.assign(m_pieces, 0);
    for (int c = 0; c < n; ++c)
        for (int r = 0; r < n; ++r)
            if (m_table->item(r, 3 + c)->checkState() == Qt::Checked) {
                l.elementRow[static_cast<std::size_t>(c)] = r;
                break;
            }
    for (int r = 0; r < n; ++r)
        l.rows.push_back({m_table->item(r, 1)->checkState() == Qt::Checked, m_table->item(r, 2)->data(Qt::UserRole).toDouble()});
    return l;
}

void InternalLinkDialog::accept() {
    if (m_type == InternalLinkType::PartialFixity && !m_coefficient->commit()) return;
    CypeDialog::accept();
}

}  // namespace cc::app
