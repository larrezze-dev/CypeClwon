#include "BarEndsTool.h"

#include <QAction>
#include <QActionGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPixmap>
#include <QRadioButton>
#include <QVBoxLayout>
#include <cmath>

#include "UserMessages.h"
#include "Viewport3D.h"
#include "core/bar_ends_edit.h"

namespace cc::app {

using namespace cc::core;

namespace {

void setBold(QWidget* widget, bool bold) {
    QFont font = widget->font();
    font.setBold(bold);
    widget->setFont(font);
}

/// Piece picture of the dialog (our own drawing of orig_artic_08): a grey I-beam with the axes X (red), Y (green) and Z (blue).
class PiecePicture : public QWidget {
public:
    explicit PiecePicture(QWidget* parent) : QWidget(parent) { setFixedSize(110, 84); }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const QPointF o(46, 60);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(110, 110, 110));
        p.drawPolygon(QPolygonF({QPointF(50, 22), QPointF(86, 14), QPointF(90, 22), QPointF(56, 30)}));
        p.setBrush(QColor(80, 80, 80));
        p.drawPolygon(QPolygonF({QPointF(56, 30), QPointF(90, 22), QPointF(92, 46), QPointF(58, 54)}));
        p.setBrush(QColor(140, 140, 140));
        p.drawPolygon(QPolygonF({QPointF(50, 46), QPointF(58, 54), QPointF(92, 46), QPointF(84, 40)}));
        const struct { QPointF to; QColor colour; const char* name; } axes[3] = {
            {QPointF(18, 74), QColor(220, 0, 0), "X"}, {QPointF(80, 74), QColor(0, 170, 0), "Y"}, {QPointF(46, 6), QColor(0, 0, 230), "Z"}};
        for (const auto& axis : axes) {
            p.setPen(QPen(axis.colour, 1.6));
            p.drawLine(o, axis.to);
            QFont f = font();
            f.setBold(true);
            p.setFont(f);
            p.drawText(QRectF(axis.to.x() - 8, axis.to.y() - 9, 16, 14), Qt::AlignCenter, QString::fromLatin1(axis.name));
        }
    }
};

QString decimals(double value, int count) { return QString::number(value, 'f', count); }

}  // namespace

// ---- panel ------------------------------------------------------------------------------------------------------------------

SelectionTypePanel::SelectionTypePanel(QWidget* parent) : CypeDialog(tr("Propiedades - Tipo de selección"), parent) {
    setModal(false);
    setMaximizable();
    setCompactTitle();
    contentLayout()->setContentsMargins(0, 0, 0, 0);
    contentLayout()->setSpacing(0);
    auto* options = new QVBoxLayout();
    options->setContentsMargins(6, 4, 6, 6);
    options->setSpacing(2);
    m_individual = new QRadioButton(tr("Individual"), this);
    m_multiple = new QRadioButton(tr("Múltiple"), this);
    m_individual->setChecked(true);
    setBold(m_individual, true);
    options->addWidget(m_individual);
    options->addWidget(m_multiple);
    contentLayout()->addLayout(options);
    contentLayout()->addStretch(1);
    connect(m_individual, &QRadioButton::toggled, this, [this](bool on) {
        setBold(m_individual, on);
        setBold(m_multiple, !on);
        emit typeChanged();
    });
    setMinimumSize(270, 360);
}

bool SelectionTypePanel::multiple() const { return m_multiple->isChecked(); }

void SelectionTypePanel::done(int result) {
    CypeDialog::done(result);
    emit closed();
}

// ---- "Empotramiento en extremos" ---------------------------------------------------------------------------------------------

EndFixityDialog::EndFixityDialog(QWidget* parent) : CypeDialog(tr("Empotramiento en extremos"), parent) {
    setMinimumSize(735, 410);
    resize(735, 410);
    for (auto& row : m_coefficientValue) row = {1.0, 1.0, 1.0};
    for (auto& row : m_stiffnessValue) row = {kDefaultEndRotationalStiffness, kDefaultEndRotationalStiffness, kDefaultEndRotationalStiffness};
    auto* body = new QWidget(this);
    auto* outer = new QVBoxLayout(body);
    outer->setContentsMargins(8, 4, 8, 4);
    outer->setSpacing(2);
    auto* picture = new PiecePicture(body);
    outer->addWidget(picture, 0, Qt::AlignHCenter);
    auto* line = new QFrame(body);
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);
    outer->addWidget(line);
    auto* grid = new QGridLayout();
    grid->setHorizontalSpacing(8);
    grid->setVerticalSpacing(0);
    grid->setColumnStretch(0, 1);
    const QString headers[2] = {tr("Origen de la pieza"), tr("Extremo de la pieza")};
    for (int end = 0; end < 2; ++end) {
        auto* header = new QLabel(headers[end], body);
        setBold(header, true);
        header->setAlignment(Qt::AlignCenter);
        grid->addWidget(header, 0, 1 + end);
    }
    const QString planeNames[3] = {tr("Asignar (Plano xy)"), tr("Asignar (Plano xz)"), tr("Asignar (Plano yz)")};
    for (int plane = 0; plane < 3; ++plane) {
        const int row = 1 + plane * 2;
        m_assign[static_cast<std::size_t>(plane)] = new QCheckBox(planeNames[plane], body);
        m_assign[static_cast<std::size_t>(plane)]->setChecked(true);
        grid->addWidget(m_assign[static_cast<std::size_t>(plane)], row, 0, Qt::AlignTop | Qt::AlignLeft);
        for (int end = 0; end < 2; ++end) buildEnd(body, end, plane, grid, row, 1 + end);
    }
    outer->addLayout(grid);
    outer->addStretch(1);
    contentLayout()->setContentsMargins(0, 0, 0, 0);
    contentLayout()->addWidget(body, 1);
    setupFooter(tr("Aceptar"), tr("Cancelar"));
    // The first field has the focus with its text selected (measured).
    m_field[0][0]->setFocus();
    m_field[0][0]->selectAll();
}

void EndFixityDialog::buildEnd(QWidget* parent, int end, int plane, QGridLayout* grid, int row, int column) {
    const auto e = static_cast<std::size_t>(end);
    const auto p = static_cast<std::size_t>(plane);
    auto* cell = new QWidget(parent);
    auto* layout = new QGridLayout(cell);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setHorizontalSpacing(6);
    layout->setVerticalSpacing(2);
    layout->setColumnStretch(0, 1);
    m_coefficient[e][p] = new QRadioButton(tr("Coef. empotramiento"), cell);
    m_stiffness[e][p] = new QRadioButton(tr("Rigidez rotacional"), cell);
    m_coefficient[e][p]->setChecked(true);
    setBold(m_coefficient[e][p], true);
    layout->addWidget(m_coefficient[e][p], 0, 0);
    layout->addWidget(m_stiffness[e][p], 1, 0);
    m_field[e][p] = new QLineEdit(cell);
    m_field[e][p]->setFixedWidth(plane == kPlaneYZ ? 60 : 60);
    m_field[e][p]->setAlignment(Qt::AlignRight);
    layout->addWidget(m_field[e][p], 0, 1);
    if (plane == kPlaneYZ) {  // the torsion coefficient is a popup 0.000 / 1.000 (measured)
        m_yz[e] = new QComboBox(cell);
        m_yz[e]->addItem(QStringLiteral("0.000"), 0.0);
        m_yz[e]->addItem(QStringLiteral("1.000"), 1.0);
        m_yz[e]->setCurrentIndex(1);
        m_yz[e]->setFixedWidth(72);
        layout->addWidget(m_yz[e], 0, 1);
    }
    m_unit[e][p] = new QLabel(tr("kN·m/rad"), cell);
    m_unit[e][p]->hide();
    layout->addWidget(m_unit[e][p], 0, 2);
    grid->addWidget(cell, row, column, 2, 1);

    connect(m_coefficient[e][p], &QRadioButton::toggled, this, [this, end, plane](bool) { refreshPlane(end, plane); });
    connect(m_field[e][p], &QLineEdit::editingFinished, this, [this, end, plane] { commitField(end, plane); });
    refreshPlane(end, plane);
}

void EndFixityDialog::refreshPlane(int end, int plane) {
    if (m_updating) return;
    const auto e = static_cast<std::size_t>(end);
    const auto p = static_cast<std::size_t>(plane);
    const bool coefficient = m_coefficient[e][p]->isChecked();
    setBold(m_coefficient[e][p], coefficient);
    setBold(m_stiffness[e][p], !coefficient);
    m_updating = true;
    const bool combo = plane == kPlaneYZ && coefficient;
    m_field[e][p]->setVisible(!combo);
    if (m_yz[e]) m_yz[e]->setVisible(combo);
    m_unit[e][p]->setVisible(!coefficient);
    m_field[e][p]->setText(coefficient ? decimals(m_coefficientValue[e][p], 3) : decimals(m_stiffnessValue[e][p], 2));
    if (combo) m_yz[e]->setCurrentIndex(m_coefficientValue[e][p] < 0.5 ? 0 : 1);
    m_updating = false;
}

void EndFixityDialog::commitField(int end, int plane) {
    if (m_updating) return;
    const auto e = static_cast<std::size_t>(end);
    const auto p = static_cast<std::size_t>(plane);
    const bool coefficient = m_coefficient[e][p]->isChecked();
    bool ok = false;
    const double value = m_field[e][p]->text().trimmed().replace(',', '.').toDouble(&ok);
    m_updating = true;
    if (coefficient) {
        if (!ok || value < 0.0 || value > 1.0) {  // «El valor de '' debe estar entre 0.000 y 1.000 .» (measured), the previous value comes back
            m_updating = false;
            messages::show(this, messages::Kind::Warning, QString::fromStdString(endFixityRangeMessage()));
            m_updating = true;
        } else {
            m_coefficientValue[e][p] = value;
        }
        m_field[e][p]->setText(decimals(m_coefficientValue[e][p], 3));
    } else {
        // Out-of-range values are kept so that Aceptar reports them (measured); the field shows 2 decimals (0.001 -> 0.00).
        if (ok && std::isfinite(value)) m_stiffnessValue[e][p] = std::round(value * 100.0) / 100.0;
        m_field[e][p]->setText(decimals(m_stiffnessValue[e][p], 2));
    }
    m_updating = false;
}

void EndFixityDialog::typeField(int end, int plane, const QString& text) {
    QLineEdit* edit = m_field[static_cast<std::size_t>(end)][static_cast<std::size_t>(plane)];
    edit->setText(text);
    commitField(end, plane);
}

EndFixityEdit EndFixityDialog::edit() const {
    EndFixityEdit edit;
    for (int plane = 0; plane < 3; ++plane) {
        const auto p = static_cast<std::size_t>(plane);
        edit.assignPlane[p] = m_assign[p]->isChecked();
        for (int end = 0; end < 2; ++end) {
            const auto e = static_cast<std::size_t>(end);
            EndPlaneFixity& value = edit.value[e][p];
            if (m_coefficient[e][p]->isChecked()) {
                value.kind = EndFixityKind::Coefficient;
                value.coefficient = plane == kPlaneYZ && m_yz[e] ? m_yz[e]->currentData().toDouble() : m_coefficientValue[e][p];
            } else {
                value.kind = EndFixityKind::Rotational;
                value.stiffnessKnMPerRad = m_stiffnessValue[e][p];
            }
        }
    }
    return edit;
}

void EndFixityDialog::accept() {
    for (int end = 0; end < 2; ++end)
        for (int plane = 0; plane < 3; ++plane) {
            const QLineEdit* field = m_field[static_cast<std::size_t>(end)][static_cast<std::size_t>(plane)];
            if (field->isVisible() && field->hasFocus()) commitField(end, plane);  // the field being edited commits first
        }
    const EndFixityEdit state = edit();
    if (const auto issue = checkEndFixityEdit(state)) {
        messages::show(this, messages::Kind::Warning, QString::fromStdString(issue->text));
        return;
    }
    if (endFixityEditAsksDefaultStiffness(state) &&
        !messages::show(this, messages::Kind::Question, QString::fromStdString(endFixityDefaultStiffnessQuestion())))
        return;
    CypeDialog::accept();
}

// ---- tool -------------------------------------------------------------------------------------------------------------------

BarEndsTool::BarEndsTool(QObject* parent) : QObject(parent) {
    auto* group = new QActionGroup(this);
    group->setExclusionPolicy(QActionGroup::ExclusionPolicy::ExclusiveOptional);
    const auto make = [this, group](const QString& text, Mode mode) {
        auto* action = new QAction(text, this);
        action->setCheckable(true);
        action->setEnabled(false);
        group->addAction(action);
        connect(action, &QAction::toggled, this, [this, mode](bool on) {
            if (on) start(mode);
            else if (m_mode == mode) stop();
        });
        return action;
    };
    m_articulate = make(tr("Articular"), Mode::Articulate);
    m_fixity = make(tr("Empotramiento"), Mode::Fixity);
}

QString BarEndsTool::statusText(Mode mode, bool multiple) {
    const QString group = tr(" - LMB: Seleccione un grupo de elementos. - RMB: Actuar sobre los elementos seleccionados.");
    switch (mode) {
        case Mode::Articulate:
            if (!multiple) return tr("Barras - Articular |") + QLatin1Char(' ') + tr(" - LMB: Seleccione un elemento.");
            return tr("Barras - Articular |") + QLatin1Char(' ') + group;
        case Mode::Fixity: return tr("Barras - Empotramiento |") + QLatin1Char(' ') + group;
        case Mode::None: break;
    }
    return tr("Seleccione una opción del menú.");
}

void BarEndsTool::pruneViewports() {
    m_viewports.removeIf([](const QPointer<Viewport3D>& v) { return v.isNull(); });
}

int BarEndsTool::run(QDialog& dialog) { return m_runner ? m_runner(dialog) : dialog.exec(); }

bool BarEndsTool::multipleSelection() const { return m_mode == Mode::Fixity || (m_panel && m_panel->multiple()); }

void BarEndsTool::setProject(Project* project, UndoStack* undo) {
    deactivate();
    m_project = project;
    m_undo = undo;
    for (const QPointer<Viewport3D>& viewport : m_viewports)
        if (viewport) viewport->setBarEnds(m_project ? &m_project->barEnds : nullptr);
    updateEnabled();
}

void BarEndsTool::addViewport(Viewport3D* viewport) {
    if (!viewport || m_viewports.contains(viewport)) return;
    m_viewports.push_back(viewport);
    viewport->setBarEnds(m_project ? &m_project->barEnds : nullptr);
    connect(viewport, &Viewport3D::actOnSelection, this, [this, viewport] { onActOnSelection(viewport); });
    connect(viewport, &Viewport3D::selectionChanged, this, [this, viewport] { onSelectionChanged(viewport); });
    connect(viewport, &Viewport3D::escapePressed, this, [this] { deactivate(); });
    if (m_mode != Mode::None) configureViewports();
}

void BarEndsTool::updateEnabled() {
    const bool bars = m_project && !m_project->structure.bars.empty();
    for (QAction* action : {m_articulate, m_fixity}) {
        action->setEnabled(bars);
        if (!bars && action->isChecked()) action->setChecked(false);
    }
}

void BarEndsTool::structureChanged() {
    updateEnabled();
    pruneViewports();
    for (const QPointer<Viewport3D>& viewport : m_viewports) viewport->refreshStructure();
}

void BarEndsTool::start(Mode mode) {
    if (!m_project || mode == m_mode) return;
    emit aboutToActivate();
    m_mode = mode;
    if (mode == Mode::Articulate) {
        if (!m_panel) {
            m_panel = new SelectionTypePanel(m_dialogParent);
            m_panel->setAttribute(Qt::WA_DeleteOnClose);
            connect(m_panel, &SelectionTypePanel::closed, this, [this] {
                if (m_mode == Mode::Articulate) deactivate();
            });
            connect(m_panel, &SelectionTypePanel::typeChanged, this, [this] {
                if (m_mode != Mode::Articulate) return;
                for (const QPointer<Viewport3D>& viewport : m_viewports)
                    if (viewport) viewport->clearSelection();
                emit statusChanged(statusText(m_mode, multipleSelection()));
            });
        }
        m_panel->show();
    } else if (m_panel) {
        m_panel->close();
    }
    configureViewports();
    emit modeChanged(m_mode);
    emit statusChanged(statusText(m_mode, multipleSelection()));  // after the viewports: setSelectionMode announces its own, generic hint
}

void BarEndsTool::deactivate() {
    for (QAction* action : {m_articulate, m_fixity})
        if (action->isChecked()) action->setChecked(false);
    stop();
}

void BarEndsTool::stop() {
    if (m_mode == Mode::None) return;
    m_mode = Mode::None;
    if (m_panel) m_panel->close();
    configureViewports();
    emit modeChanged(m_mode);
    emit statusChanged(statusText(m_mode, false));
}

void BarEndsTool::configureViewports() {
    pruneViewports();
    const bool on = m_mode != Mode::None;
    for (const QPointer<Viewport3D>& viewport : m_viewports) {
        viewport->setSelectionMode(false);
        viewport->setBarAxesVisible(m_mode == Mode::Articulate);
        viewport->setEndFixityLabelsVisible(m_mode == Mode::Fixity);
        if (!on) continue;
        viewport->setSelectionFilter(SelectionFilter::only(ElementKind::Bar));
        viewport->setSelectionMode(true);
        viewport->setFocus();
    }
}

bool BarEndsTool::articulate(const std::vector<int>& barIds) {
    if (!m_project || !m_undo) return false;
    return toggleBarArticulations(*m_project, *m_undo, barIds, ArticulationEnd::Both);
}

bool BarEndsTool::articulateNearest(Viewport3D& viewport, int barId, const QPointF& at) {
    if (!m_project || !m_undo) return false;
    const Bar* bar = m_project->structure.findBar(barId);
    if (!bar) return false;
    const Node* n0 = m_project->structure.findNode(bar->startNode);
    const Node* n1 = m_project->structure.findNode(bar->endNode);
    QPointF s0, s1;
    if (!n0 || !n1 || !viewport.worldToScreen({n0->position.x, n0->position.y, n0->position.z}, s0) ||
        !viewport.worldToScreen({n1->position.x, n1->position.y, n1->position.z}, s1))
        return false;
    // Parameter of the click along the bar (orthogonal projection on its screen segment); d0 = t L, d1 = (1 - t) L (REA third rule).
    const QPointF axis = s1 - s0;
    const double len2 = axis.x() * axis.x() + axis.y() * axis.y();
    const double t = len2 > 0.0 ? ((at.x() - s0.x()) * axis.x() + (at.y() - s0.y()) * axis.y()) / len2 : 0.5;
    return toggleBarArticulations(*m_project, *m_undo, {barId}, articulationEndForParameter(t));
}

bool BarEndsTool::editFixity(const std::vector<int>& barIds) {
    if (!m_project || !m_undo) return false;
    const std::vector<int> bars = existingBars(*m_project, barIds);
    if (bars.empty()) return false;
    EndFixityDialog dialog(m_dialogParent);
    if (run(dialog) != QDialog::Accepted) return false;
    const std::size_t before = m_undo->count();
    if (const auto issue = assignEndFixity(*m_project, *m_undo, bars, dialog.edit())) {
        messages::show(m_dialogParent, messages::Kind::Warning, QString::fromStdString(issue->text));
        return false;
    }
    return m_undo->count() != before;
}

void BarEndsTool::onActOnSelection(Viewport3D* viewport) {
    if (m_mode == Mode::None || !m_project || !m_undo) return;
    const std::vector<int> bars = viewport->selection().ids(ElementKind::Bar);
    if (bars.empty()) return;
    if (m_mode == Mode::Articulate) {
        if (!multipleSelection()) return;  // Individual acts on the click, not on RMB
        articulate(bars);
        viewport->clearSelection();
    } else {
        editFixity(bars);
        viewport->clearSelection();
    }
    emit statusChanged(statusText(m_mode, multipleSelection()));
}

void BarEndsTool::onSelectionChanged(Viewport3D* viewport) {
    if (m_mode != Mode::Articulate || multipleSelection() || !m_project || !m_undo) return;
    const std::vector<int> bars = viewport->selection().ids(ElementKind::Bar);
    if (bars.size() != 1) return;
    articulateNearest(*viewport, bars.front(), viewport->cursorPosition());
    viewport->clearSelection();
}

}  // namespace cc::app
