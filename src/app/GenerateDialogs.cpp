#include "GenerateDialogs.h"

#include <QCheckBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPaintEvent>
#include <QPainter>
#include <QVBoxLayout>

#include "core/shell_material.h"

namespace cc::app {

using namespace cc::core;

namespace {
RangedEdit* makeDivisionsEdit(const QString& label, QWidget* parent) {
    auto* edit = new RangedEdit(label, ValueRange{kMinDivisions, kMaxDivisions}, 0, parent);
    edit->setValue(1.0);
    return edit;
}
}  // namespace

GenerateSketch::GenerateSketch(int segments, QWidget* parent) : QWidget(parent), m_segments(segments) {
    setMinimumSize(180, 150);
}

void GenerateSketch::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    // A skewed cell: origin (bottom left), segment 1 along the base, segment 2 up-right, segment 3 (3D) straight up.
    const QPointF o(30, height() - 30), s1(width() - 70, 0), s2(40, -(height() - 90)), s3(0, -50);
    p.setPen(QPen(QColor(120, 120, 120), 1));
    for (int i = 0; i <= 3; ++i) {
        const double t = i / 3.0;
        p.drawLine(o + s1 * t, o + s1 * t + s2);
        p.drawLine(o + s2 * t, o + s2 * t + s1);
    }
    p.setPen(QPen(QColor(0, 102, 204), 2));
    p.drawLine(o, o + s1);
    p.drawLine(o, o + s2);
    if (m_segments >= 3) p.drawLine(o, o + s3);
    p.setPen(Qt::black);
    p.drawText(o + s1 * 0.5 + QPointF(-10, 16), QStringLiteral("1"));
    p.drawText(o + s2 * 0.5 + QPointF(-14, 0), QStringLiteral("2"));
    if (m_segments >= 3) p.drawText(o + s3 * 0.5 + QPointF(-14, 0), QStringLiteral("3"));
}

// ---- Planos ------------------------------------------------------------------------------------------------------------------

PlanesDialog::PlanesDialog(QWidget* parent) : CypeDialog(tr("Generación de planos"), parent) {
    auto* grid = new QGridLayout();
    m_count = new RangedEdit(tr("Nº de planos a generar"), ValueRange{kMinPlaneCount, kMaxPlaneCount}, 0, this);
    m_count->setValue(1.0);
    grid->addWidget(new QLabel(tr("Nº de planos a generar"), this), 0, 0);
    grid->addWidget(m_count, 0, 1);
    m_separation = new RangedEdit(tr("Separación entre planos"), ValueRange{kMinPlaneSeparationM, kMaxPlaneSeparationM}, 3, this);
    m_separation->setValue(1.0);
    grid->addWidget(new QLabel(tr("Separación entre planos"), this), 1, 0);
    grid->addWidget(m_separation, 1, 1);
    grid->addWidget(new QLabel(QStringLiteral("m"), this), 1, 2);
    grid->setColumnStretch(3, 1);
    contentLayout()->addLayout(grid);
    m_group = new QCheckBox(tr("Agrupar planos generados"), this);
    contentLayout()->addWidget(m_group);
    setupFooter(tr("Aceptar"), tr("Cancelar"));
    setMinimumWidth(380);
}

void PlanesDialog::accept() {
    if (!m_count->commit() || !m_separation->commit()) return;
    if (m_separation->value() == 0.0) {  // the original fails with an internal error here
        RangedEdit::showWarning(this, QString::fromStdString(generateErrorMessage(GenerateError::ZeroSeparation)));
        return;
    }
    CypeDialog::accept();
}

// ---- Nudos y barras -------------------------------------------------------------------------------------------------------------

QString NodeMeshDialog::titleFor(int segments) {
    switch (segments) {
        case 1: return tr("Generación de nudos sobre un segmento");
        case 2: return tr("Generación de una malla de nudos bidimensional");
        default: return tr("Generación de una malla de nudos tridimensional");
    }
}

NodeMeshDialog::NodeMeshDialog(int segments, QWidget* parent) : CypeDialog(titleFor(segments), parent), m_segments(std::clamp(segments, 1, 3)) {
    auto* row = new QHBoxLayout();
    auto* column = new QVBoxLayout();
    if (m_segments >= 2) {
        m_onlyNodes = new QCheckBox(tr("Generar sólo los nudos"), this);
        column->addWidget(m_onlyNodes);
    }
    for (int k = 0; k < m_segments; ++k) {
        const QString name = tr("Nº de divisiones del segmento %1").arg(k + 1);
        auto* line = new QHBoxLayout();
        line->addWidget(new QLabel(name, this));
        m_divisions[static_cast<std::size_t>(k)] = makeDivisionsEdit(name, this);
        line->addWidget(m_divisions[static_cast<std::size_t>(k)]);
        line->addStretch(1);
        column->addLayout(line);
        if (m_segments >= 2) {
            m_group[static_cast<std::size_t>(k)] = new QCheckBox(tr("Agrupar barras paralelas al segmento %1").arg(k + 1), this);
            m_piece[static_cast<std::size_t>(k)] = new QCheckBox(tr("Crear pieza entre barras alineadas"), this);
            column->addWidget(m_group[static_cast<std::size_t>(k)]);
            column->addWidget(m_piece[static_cast<std::size_t>(k)]);
        }
    }
    column->addStretch(1);
    row->addLayout(column, 1);
    if (m_segments >= 2) row->addWidget(new GenerateSketch(m_segments, this));
    contentLayout()->addLayout(row);
    if (m_onlyNodes) {
        // PROVISIONAL: with only the nodes there are no bars to group or join into pieces.
        connect(m_onlyNodes, &QCheckBox::toggled, this, [this](bool on) {
            for (int k = 0; k < m_segments; ++k) {
                m_group[static_cast<std::size_t>(k)]->setEnabled(!on);
                m_piece[static_cast<std::size_t>(k)]->setEnabled(!on);
            }
        });
    }
    setupFooter(tr("Aceptar"), tr("Cancelar"));
    setMinimumWidth(m_segments >= 2 ? 560 : 380);
}

void NodeMeshDialog::fill(NodeMeshSpec& spec) const {
    for (int k = 0; k < m_segments; ++k) {
        const std::size_t i = static_cast<std::size_t>(k);
        spec.divisions[i] = static_cast<int>(m_divisions[i]->value());
        spec.groupParallel[i] = m_group[i] && m_group[i]->isChecked();
        spec.makePieces[i] = m_piece[i] && m_piece[i]->isChecked();
    }
    spec.onlyNodes = m_onlyNodes && m_onlyNodes->isChecked();
}

void NodeMeshDialog::accept() {
    for (int k = 0; k < m_segments; ++k)
        if (!m_divisions[static_cast<std::size_t>(k)]->commit()) return;
    CypeDialog::accept();
}

// ---- Malla de tetraedros ---------------------------------------------------------------------------------------------------------

TetrahedraDialog::TetrahedraDialog(QWidget* parent) : CypeDialog(tr("Generación de una malla de tetraedros"), parent) {
    auto* row = new QHBoxLayout();
    auto* grid = new QGridLayout();
    const QString d1 = tr("Nº de divisiones del segmento 1"), d2 = tr("Nº de divisiones del segmento 2"), dist = tr("Distancia entre planos");
    m_div1 = makeDivisionsEdit(d1, this);
    m_div2 = makeDivisionsEdit(d2, this);
    m_distance = new RangedEdit(dist, ValueRange{-kMaxTetrahedraDistanceM, kMaxTetrahedraDistanceM}, 3, this);
    m_distance->setValue(1.0);
    grid->addWidget(new QLabel(d1, this), 0, 0);
    grid->addWidget(m_div1, 0, 1);
    grid->addWidget(new QLabel(d2, this), 1, 0);
    grid->addWidget(m_div2, 1, 1);
    grid->addWidget(new QLabel(dist, this), 2, 0);
    grid->addWidget(m_distance, 2, 1);
    grid->addWidget(new QLabel(QStringLiteral("m"), this), 2, 2);
    const QString names[3] = {tr("Agrupar barras de plano inferior"), tr("Agrupar barras de plano superior"), tr("Agrupar barras entre planos (diagonales)")};
    for (int i = 0; i < 3; ++i) {
        m_groups[static_cast<std::size_t>(i)] = new QCheckBox(names[i], this);
        m_groups[static_cast<std::size_t>(i)]->setChecked(true);
        grid->addWidget(m_groups[static_cast<std::size_t>(i)], 3 + i, 0, 1, 3);
    }
    grid->setRowStretch(6, 1);
    row->addLayout(grid, 1);
    row->addWidget(new GenerateSketch(2, this));
    contentLayout()->addLayout(row);
    setupFooter(tr("Aceptar"), tr("Cancelar"));
    setMinimumWidth(600);
}

void TetrahedraDialog::fill(TetrahedraSpec& spec) const {
    spec.divisions1 = static_cast<int>(m_div1->value());
    spec.divisions2 = static_cast<int>(m_div2->value());
    spec.distanceM = m_distance->value();
    spec.groupLowerBars = m_groups[0]->isChecked();
    spec.groupUpperBars = m_groups[1]->isChecked();
    spec.groupDiagonals = m_groups[2]->isChecked();
}

void TetrahedraDialog::accept() {
    if (!m_div1->commit() || !m_div2->commit() || !m_distance->commit()) return;
    if (m_distance->value() == 0.0) {
        RangedEdit::showWarning(this, QString::fromStdString(generateErrorMessage(GenerateError::DistanceOutOfRange)));
        return;
    }
    CypeDialog::accept();
}

}  // namespace cc::app
