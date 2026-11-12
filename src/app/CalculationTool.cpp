#include "CalculationTool.h"

#include <QAction>
#include <QColor>
#include <array>
#include <QActionGroup>
#include <QEventLoop>
#include <QFontMetricsF>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QProgressBar>
#include <QThread>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>

#include "ResultsPanel.h"
#include "UserMessages.h"
#include "Viewport3D.h"
#include "core/fem/frame_builder.h"
#include "core/fem/linear_solver.h"
#include "core/fem/result_display.h"
#include "core/fem/results.h"
#include "core/nonlinear_combinations_edit.h"
#include "core/picking.h"
#include "core/project.h"
#include "core/undo_stack.h"

namespace cc::app {

using namespace cc::fem;

namespace {

QString qs(const std::string& text) { return QString::fromStdString(text); }

core::Vec3 toCore(const Vec3& v) { return core::Vec3{v[0], v[1], v[2]}; }

/// White balloon with thin black border: optional bold header, then rows «label value» with the label in bold [V s_lin.png, rp_04].
QRectF balloonRect(const QFont& font, const QString& header, const std::vector<BalloonLine>& lines, const QPointF& anchor) {
    QFont bold = font;
    bold.setBold(true);
    const QFontMetricsF fb(bold), fn(font);
    double width = header.isEmpty() ? 0.0 : fb.horizontalAdvance(header);
    for (const BalloonLine& l : lines) width = std::max(width, fb.horizontalAdvance(qs(l.label)) + fn.horizontalAdvance(qs(l.value)));
    const double row = fn.height() + 2.0;
    const double height = row * (lines.size() + (header.isEmpty() ? 0 : 1)) + 8.0;
    return QRectF(anchor.x() + 4.0, anchor.y() - height - 6.0, width + 16.0, height);
}

void drawBalloon(QPainter& painter, const QString& header, const std::vector<BalloonLine>& lines, const QPointF& anchor) {
    const QFont font = painter.font();
    QFont bold = font;
    bold.setBold(true);
    const QRectF box = balloonRect(font, header, lines, anchor);
    painter.save();
    painter.setPen(QPen(Qt::black, 1.0));
    painter.setBrush(Qt::white);
    painter.drawRect(box);
    const QFontMetricsF fb(bold), fn(font);
    const double row = fn.height() + 2.0;
    double y = box.top() + 4.0 + fn.ascent();
    if (!header.isEmpty()) {
        painter.setFont(bold);
        painter.drawText(QPointF(box.left() + 8.0, y), header);
        y += row;
    }
    for (const BalloonLine& l : lines) {
        painter.setFont(bold);
        painter.drawText(QPointF(box.left() + 8.0, y), qs(l.label));
        painter.setFont(font);
        painter.drawText(QPointF(box.left() + 8.0 + fb.horizontalAdvance(qs(l.label)), y), qs(l.value));
        y += row;
    }
    painter.restore();
}

/// "0", "4", "2.5": coordinates of the header of the reaction balloon «N1 (0, 0, 0)» [V].
QString coordinate(double v) {
    QString text = qs(formatFixed(v, 3));
    if (text.contains(QLatin1Char('.'))) {
        while (text.endsWith(QLatin1Char('0'))) text.chop(1);
        if (text.endsWith(QLatin1Char('.'))) text.chop(1);
    }
    return text;
}

/// Distance along the bar (m) of the point of its screen segment nearest to `pos`.
double screenParameter(const QPointF& a, const QPointF& b, const QPointF& pos) {
    const QPointF ab = b - a;
    const double len2 = ab.x() * ab.x() + ab.y() * ab.y();
    if (len2 <= 0.0) return 0.0;
    const QPointF ap = pos - a;
    return std::clamp((ap.x() * ab.x() + ap.y() * ab.y()) / len2, 0.0, 1.0);
}

/// Busy window shown while the analysis runs in the worker thread (the text of the original's process window is [N]).
class BusyDialog : public CypeDialog {
public:
    explicit BusyDialog(QWidget* parent) : CypeDialog(QObject::tr("Cálculo"), parent) {
        auto* label = new QLabel(QObject::tr("Cálculo de la estructura"), this);
        auto* bar = new QProgressBar(this);
        bar->setRange(0, 0);
        contentLayout()->setContentsMargins(12, 10, 12, 12);
        contentLayout()->addWidget(label);
        contentLayout()->addWidget(bar);
        setFixedWidth(320);
    }
};


/// Local axes of a bar drawn at 0.8 L: black frame of the section plane and the arrows x (red), y (green), z (blue) [V rp_07_eff_panel.png].
void drawAxesGizmo(QPainter& painter, const Viewport3D& viewport, const FrameModel& model, std::size_t barIndex) {
    const Vec3 origin = barPointAt(model, barIndex, 0.8 * barLength(model, barIndex));
    QPointF o;
    if (!viewport.worldToScreen(toCore(origin), o)) return;
    const FemBar& bar = model.bars[barIndex];
    const Vec3 axes[3] = {bar.ex, bar.ey, bar.ez};
    const QColor colours[3] = {QColor(0xFF, 0x00, 0x00), QColor(0x00, 0xDD, 0x00), QColor(0x00, 0x00, 0xFF)};
    QPointF dir[3];
    double projected[3] = {0, 0, 0}, longest = 0.0;
    for (int i = 0; i < 3; ++i) {
        QPointF t;
        if (!viewport.worldToScreen(toCore(Vec3{origin[0] + axes[i][0], origin[1] + axes[i][1], origin[2] + axes[i][2]}), t)) return;
        const QPointF d = t - o;
        projected[i] = std::hypot(d.x(), d.y());
        longest = std::max(longest, projected[i]);
        if (projected[i] > 1e-9) dir[i] = d / projected[i];
    }
    if (longest < 1e-9) return;
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);
    // Section frame: spanned by y and z.
    const QPointF y = dir[1] * (14.0 * projected[1] / longest), z = dir[2] * (14.0 * projected[2] / longest);
    painter.setPen(QPen(Qt::black, 1.0));
    painter.setBrush(Qt::NoBrush);
    painter.drawPolygon(QPolygonF({o - y - z, o + y - z, o + y + z, o - y + z}));
    for (int i = 0; i < 3; ++i) {
        if (projected[i] < 1e-9) continue;
        const QPointF to = o + dir[i] * (90.0 * projected[i] / longest);
        painter.setPen(QPen(colours[i], 1.2));
        painter.drawLine(o, to);
        const QPointF side(-dir[i].y(), dir[i].x());
        painter.drawLine(to, to - dir[i] * 14.0 + side * 5.0);
        painter.drawLine(to, to - dir[i] * 14.0 - side * 5.0);
    }
    painter.restore();
}

}  // namespace

CalculationTool::CalculationTool(QObject* parent) : QObject(parent) {
    m_nonlinearAction = new QAction(tr("Combinaciones no lineales"), this);
    m_calculateAction = new QAction(tr("Calcular"), this);
    m_nonlinearAction->setStatusTip(tr("Tensión / Deformación - Combinaciones no lineales"));
    m_calculateAction->setStatusTip(tr("Tensión / Deformación - Calcular"));
    connect(m_nonlinearAction, &QAction::triggered, this, [this] { editNonlinearCombinations(); });
    connect(m_calculateAction, &QAction::triggered, this, [this] { calculate(); });
    m_group = new QActionGroup(this);
    m_group->setExclusionPolicy(QActionGroup::ExclusionPolicy::ExclusiveOptional);
    const auto make = [this](const QString& text, Mode mode) {
        auto* action = new QAction(text, this);
        action->setCheckable(true);
        action->setEnabled(false);
        action->setStatusTip(tr("Tensión / Deformación - %1").arg(text));
        m_group->addAction(action);
        connect(action, &QAction::toggled, this, [this, mode](bool on) {
            if (on) setMode(mode);
            else if (m_mode == mode) setMode(Mode::None);
        });
        return action;
    };
    m_displacementsAction = make(tr("Desplazamientos"), Mode::Displacements);
    m_reactionsAction = make(tr("Reacciones"), Mode::Reactions);
    m_forcesAction = make(tr("Esfuerzos"), Mode::Forces);
    updateEnabled();
}

CalculationTool::~CalculationTool() = default;

ResultsPanel* CalculationTool::panel() const { return m_panel.data(); }

QString CalculationTool::statusText(Mode mode) {
    switch (mode) {
        case Mode::Displacements: return tr("Tensión / Deformación - Desplazamientos");
        case Mode::Reactions: return tr("Tensión / Deformación - Reacciones");
        case Mode::Forces: return tr("Tensión / Deformación - Esfuerzos");
        case Mode::None: break;
    }
    return {};
}

int CalculationTool::run(QDialog& dialog) { return m_runner ? m_runner(dialog) : dialog.exec(); }

void CalculationTool::setProject(cc::core::Project* project, cc::core::UndoStack* undo) {
    deactivate();
    m_project = project;
    m_undo = undo;
    m_analysis.reset();
    m_lastReport.clear();
    m_panelShownInWork = false;
    updateEnabled();
}

void CalculationTool::addViewport(Viewport3D* viewport) {
    if (!viewport || m_viewports.contains(viewport)) return;
    m_viewports.push_back(viewport);
    connect(viewport, &Viewport3D::pickClicked, this, [this, viewport](const QPointF& pos, Qt::KeyboardModifiers) {
        if (m_mode != Mode::None) clickAt(viewport, pos);
    });
    connect(viewport, &Viewport3D::actOnSelection, this, [this, viewport] {
        if (m_mode != Mode::None) rightClickAt(viewport, viewport->cursorPosition());
    });
    connect(viewport, &Viewport3D::escapePressed, this, [this] {
        if (m_mode != Mode::None) deactivate();
    });
    if (m_mode != Mode::None) configureViewports();
}

void CalculationTool::updateEnabled() {
    const bool open = m_project != nullptr;
    m_nonlinearAction->setEnabled(open);
    m_calculateAction->setEnabled(open);
    const bool results = open && calculated();
    for (QAction* a : {m_displacementsAction, m_reactionsAction, m_forcesAction}) {
        a->setEnabled(results);
        if (!results && a->isChecked()) a->setChecked(false);
    }
}

void CalculationTool::markHistory() {
    m_undoIndex = m_undo ? m_undo->index() : 0;
    m_undoCount = m_undo ? m_undo->count() : 0;
}

void CalculationTool::structureChanged() {
    // Saving (setClean) and the end of a macro also notify: only a moved history means a changed work.
    if (m_analysis && m_undo && (m_undo->index() != m_undoIndex || m_undo->count() != m_undoCount)) invalidate();
    updateEnabled();
}

void CalculationTool::invalidate() {
    deactivate();
    m_analysis.reset();
    updateEnabled();
}

void CalculationTool::deactivate() {
    if (QAction* checked = m_group->checkedAction()) checked->setChecked(false);
    setMode(Mode::None);
}

void CalculationTool::setMode(Mode mode) {
    if (mode == m_mode) return;
    if (mode != Mode::None) emit aboutToActivate();
    m_mode = mode;
    m_shownBars.clear();
    m_shownNodes.clear();
    m_selectedBars.clear();
    m_query.reset();
    closePanel();
    if (mode != Mode::None) showPanel();
    configureViewports();
    emit modeChanged(mode);
    emit statusChanged(statusText(mode));
}

void CalculationTool::configureViewports() {
    m_viewports.removeIf([](const QPointer<Viewport3D>& v) { return v.isNull(); });
    const bool on = m_mode != Mode::None;
    if (!on && !m_configured) return;
    m_configured = on;
    for (const QPointer<Viewport3D>& viewport : m_viewports) {
        viewport->setSelectionMode(false);
        viewport->setPickMode(on);
        if (!on) viewport->setBarsHidden(false);
        Viewport3D* v = viewport.data();
        viewport->setSupportColour(m_mode == Mode::Reactions ? std::function<std::optional<QColor>(int)>([this](int nodeId) -> std::optional<QColor> {
            return m_shownNodes.count(nodeId) ? QColor(0xB0, 0x9A, 0x84) : QColor(0x00, 0x9A, 0xDC);  // light brown with balloon, turquoise without [V rm_*: core #B09A84]
        }) : std::function<std::optional<QColor>(int)>());
        viewport->setOverlay(on ? std::function<void(QPainter&)>([this, v](QPainter& p) { paint(p, *v); }) : std::function<void(QPainter&)>());
        if (on) viewport->setFocus();
        viewport->update();
    }
}

void CalculationTool::showPanel() {
    const ResultsPanel::Kind kind = m_mode == Mode::Displacements ? ResultsPanel::Kind::Displacements
                                    : m_mode == Mode::Reactions   ? ResultsPanel::Kind::Reactions
                                                                  : ResultsPanel::Kind::Forces;
    m_panel = new ResultsPanel(kind, m_dialogParent);
    m_panel->setAttribute(Qt::WA_DeleteOnClose);
    m_panel->setAnalysis(m_analysis.get());
    connect(m_panel, &ResultsPanel::changed, this, [this] {
        m_query.reset();
        repaint();
    });
    connect(m_panel, &ResultsPanel::closed, this, [this] {
        m_panel.clear();
        if (m_mode != Mode::None) deactivate();  // closing the window ends the mode
    });
    if (m_dialogParent) {
        // Over the left column (s_lin.png), ~520 px wide down to the bottom of the window [V rp_07_eff_panel.png]. The size must be set before
        // showMaximized(): Qt keeps it as the geometry the restore button returns to (otherwise the panel stays full screen).
        const QPoint corner = m_dialogParent->mapToGlobal(QPoint(14, 230));
        const int height = m_dialogParent->mapToGlobal(QPoint(0, m_dialogParent->height())).y() - corner.y();
        m_panel->setGeometry(corner.x(), corner.y(), 520, qMax(m_panel->sizeHint().height(), height));
    }
    if (!m_panelShownInWork) {
        // First panel of the work: opens maximised; its restore button leaves it in the left strip [V rp_03_react_open.png].
        m_panelShownInWork = true;
        m_panel->show();  // shown normally first so that the restore button has a geometry to return to
        m_panel->showMaximized();
    } else {
        m_panel->show();
    }
}

void CalculationTool::closePanel() {
    if (!m_panel) return;
    QPointer<ResultsPanel> closing = m_panel;
    m_panel.clear();
    closing->close();
}

void CalculationTool::updateViewportFlags() {
    // «Ver la estructura» unchecked hides the bars while something is drawn over them [V resultados_paneles.md 4.1, N for the exact condition].
    const bool hide = m_mode == Mode::Forces && m_panel && !m_panel->showStructure() && m_panel->anyRowChecked();
    for (const QPointer<Viewport3D>& viewport : m_viewports)
        if (viewport) viewport->setBarsHidden(hide);
}

void CalculationTool::repaint() {
    updateViewportFlags();
    for (const QPointer<Viewport3D>& viewport : m_viewports)
        if (viewport) viewport->update();
}

std::optional<ResultRef> CalculationTool::currentResult() const {
    if (!m_analysis || !m_panel) return std::nullopt;
    const auto family = m_panel->family();
    if (!family || m_panel->item() < 0) return std::nullopt;
    return resultOf(*m_analysis, *family, static_cast<std::size_t>(m_panel->item()));
}

// ---------------------------------------------------------------- calculation

bool CalculationTool::calculate() {
    if (!m_project || !m_undo) return false;
    bool nonlinearElements = false;
    if (!m_project->structure.bars.empty() || !m_project->structure.shells.empty()) {
        const FrameBuild build = buildFrameModel(*m_project);
        nonlinearElements = build.errors.empty() && hasUnilateralSprings(build.model);
    }
    CalculationChoices initial = m_choices;
    initial.updateNonlinear = m_project->nonlinearCombinations.updateAfterLinear;
    CalculationDialog dialog(nonlinearElements, !m_project->nonlinearCombinations.items.empty(), initial, m_dialogParent);
    if (run(dialog) != QDialog::Accepted) return false;
    const CalculationChoices choices = dialog.choices();
    m_choices = choices;
    if (nonlinearElements && !choices.linear && !choices.nonlinear) return false;  // nothing to do [N: the original's answer]
    deactivate();

    cc::core::Project copy = *m_project;
    if (nonlinearElements && !m_project->nonlinearCombinations.items.empty()) copy.nonlinearCombinations.updateAfterLinear = choices.updateNonlinear;
    AnalysisOptions options;
    options.solver = m_solverOptions;
    options.nonlinear = nonlinearElements && choices.nonlinear;
    auto result = std::make_unique<ProjectAnalysis>();
    if (m_synchronous) {
        *result = analyseProject(copy, options);
    } else {
        ProjectAnalysis* target = result.get();
        QThread* worker = QThread::create([target, &copy, options] { *target = analyseProject(copy, options); });
        BusyDialog busy(m_dialogParent);
        busy.setModal(true);
        QEventLoop loop;
        connect(worker, &QThread::finished, &loop, &QEventLoop::quit);
        worker->start();
        busy.show();
        loop.exec();
        worker->wait();
        delete worker;
        busy.hide();
    }
    m_lastReport = result->report;

    // Write the regenerated combinations and the update choice back to the work (one undoable step, not an invalidating change).
    cc::core::NonlinearCombinations after = m_project->nonlinearCombinations;
    if (result->nonlinearCombinationsUpdated) after.items = result->nonlinearCombinations.items;
    after.updateAfterLinear = copy.nonlinearCombinations.updateAfterLinear;
    cc::core::setNonlinearCombinations(*m_project, *m_undo, after, "Cálculo");

    m_analysis = result->calculated ? std::move(result) : nullptr;
    markHistory();
    updateEnabled();
    CalculationReportDialog report(m_lastReport, m_dialogParent);
    run(report);
    emit calculationFinished();
    return true;
}

bool CalculationTool::editNonlinearCombinations() {
    if (!m_project || !m_undo) return false;
    NonlinearCombinationsDialog dialog(*m_project, m_dialogParent);
    if (m_runner) dialog.setDialogRunner(m_runner);
    if (run(dialog) != QDialog::Accepted) return false;
    return cc::core::setNonlinearCombinations(*m_project, *m_undo, dialog.value());
}

// ---------------------------------------------------------------- interaction

void CalculationTool::clickAt(Viewport3D* viewport, const QPointF& pos) {
    if (!viewport || !viewport->structure() || !m_analysis) return;
    const core::ScreenMapper mapper{&viewport->camera(), static_cast<double>(viewport->width()), static_cast<double>(viewport->height())};
    const bool nodes = m_mode == Mode::Reactions;
    const auto hit = core::pickAt(*viewport->structure(), mapper, core::SelectionFilter::only(nodes ? core::ElementKind::Node : core::ElementKind::Bar),
                                  pos.x(), pos.y());
    if (!hit) return;
    const FrameModel& model = m_analysis->build.model;
    if (m_mode == Mode::Reactions) {
        const auto ref = currentResult();
        if (!ref || !ref->result->reactions.count(hit->id)) return;  // only supported nodes have a balloon
        if (!m_shownNodes.erase(hit->id)) m_shownNodes.insert(hit->id);
    } else if (m_mode == Mode::Displacements) {
        if (!barIndexOf(model, hit->id)) return;
        if (!m_shownBars.erase(hit->id)) m_shownBars.insert(hit->id);
    } else if (m_mode == Mode::Forces) {
        if (!m_panel || !m_panel->selectedOnly() || !barIndexOf(model, hit->id)) return;
        if (!m_selectedBars.erase(hit->id)) m_selectedBars.insert(hit->id);
        m_query.reset();
    }
    repaint();
}

void CalculationTool::rightClickAt(Viewport3D* viewport, const QPointF& pos) {
    if (m_mode != Mode::Forces || !m_panel || !m_panel->queryValues() || !viewport || !viewport->structure() || !m_analysis) return;
    const core::ScreenMapper mapper{&viewport->camera(), static_cast<double>(viewport->width()), static_cast<double>(viewport->height())};
    const auto hit = core::pickAt(*viewport->structure(), mapper, core::SelectionFilter::only(core::ElementKind::Bar), pos.x(), pos.y());
    if (!hit || !m_selectedBars.count(hit->id)) {
        m_query.reset();
        repaint();
        return;
    }
    const FrameModel& model = m_analysis->build.model;
    const auto index = barIndexOf(model, hit->id);
    if (!index) return;
    QPointF a, b;
    if (!viewport->worldToScreen(toCore(barPointAt(model, *index, 0.0)), a) ||
        !viewport->worldToScreen(toCore(barPointAt(model, *index, barLength(model, *index))), b))
        return;
    m_query = std::make_pair(hit->id, screenParameter(a, b, pos) * barLength(model, *index));
    repaint();
}

// ---------------------------------------------------------------- drawing

void CalculationTool::paint(QPainter& painter, const Viewport3D& viewport) const {
    if (!m_analysis || m_mode == Mode::None) return;
    const auto ref = currentResult();
    if (!ref) return;
    const FrameModel& model = m_analysis->build.model;
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);
    const auto screen = [&viewport](const Vec3& w, QPointF& out) { return viewport.worldToScreen(toCore(w), out); };
    const auto nodePoint = [&](int nodeId, QPointF& out) {
        const auto i = nodeIndexOf(model, nodeId);
        return i && screen(model.nodes[*i].position, out);
    };
    const auto drawBar = [&](std::size_t b, const QColor& colour, double width) {
        QPointF a, c;
        if (screen(barPointAt(model, b, 0.0), a) && screen(barPointAt(model, b, barLength(model, b)), c)) {
            painter.setPen(QPen(colour, width));
            painter.drawLine(a, c);
        }
    };
    const auto nodeLabel = [&](int nodeId, const QPointF& at) {
        QFont bold = painter.font();
        bold.setBold(true);
        painter.save();
        painter.setFont(bold);
        painter.setPen(QColor(0x00, 0x99, 0x00));
        painter.drawText(at + QPointF(10, 14), QStringLiteral("N%1").arg(nodeId));
        painter.restore();
    };

    if (m_mode == Mode::Displacements) {
        const ComponentMask mask = m_panel->components();
        for (int barId : m_shownBars) {
            const auto b = barIndexOf(model, barId);
            if (!b || *b >= ref->result->barDisplacements.size()) continue;
            drawBar(*b, QColor(0xB0, 0x84, 0x57), 2.0);  // brown while it has balloons [V]
            const BarEndDisplacements& d = ref->result->barDisplacements[*b];
            const FemBar& bar = model.bars[*b];
            for (int end = 0; end < 2; ++end) {
                QPointF at;
                const int node = end == 0 ? bar.nodeI : bar.nodeJ;
                if (!nodePoint(node, at)) continue;
                drawBalloon(painter, QString(), displacementLines(end == 0 ? d.origin : d.end, mask), at);
                nodeLabel(node, at);
            }
        }
    } else if (m_mode == Mode::Reactions) {
        const ComponentMask mask = m_panel->components();
        for (int nodeId : m_shownNodes) {
            const auto it = ref->result->reactions.find(nodeId);
            const auto i = nodeIndexOf(model, nodeId);
            QPointF at;
            if (it == ref->result->reactions.end() || !i || !nodePoint(nodeId, at)) continue;
            const Vec3& p = model.nodes[*i].position;
            const QString header = QStringLiteral("N%1 (%2, %3, %4)").arg(nodeId).arg(coordinate(p[0]), coordinate(p[1]), coordinate(p[2]));
            drawBalloon(painter, header, reactionLines(it->second, mask), at);
        }
    } else if (m_mode == Mode::Forces) {
        std::vector<std::size_t> bars;
        if (m_panel->selectedOnly()) {
            for (int id : m_selectedBars)
                if (const auto b = barIndexOf(model, id)) bars.push_back(*b);
        } else {
            for (std::size_t b = 0; b < model.bars.size(); ++b) bars.push_back(b);
        }
        for (std::size_t b : bars)
            if (m_panel->selectedOnly()) drawBar(b, QColor(0xE8, 0x8F, 0x38), 2.0);  // selected bar, orange [V]
        const ComponentMask laws = m_panel->components();
        QFont bold = painter.font();
        bold.setBold(true);
        const bool onWindow = m_panel->axesOnWindow();

        // Screen point of `base` displaced by the world vector `offset`. With «Ejes sobre el plano de la ventana» [N: interpretation of the
        // original's check box] the offset is laid in the plane of the window instead: perpendicular to the projected bar, same sense as the
        // projected offset, same length per metre as the bar.
        const auto offsetPoint = [&](std::size_t b, const Vec3& base, const Vec3& offset, QPointF& out) {
            const Vec3 target{base[0] + offset[0], base[1] + offset[1], base[2] + offset[2]};
            if (!onWindow) return screen(target, out);
            QPointF p0, a, c, tip;
            if (!screen(base, p0) || !screen(barPointAt(model, b, 0.0), a) || !screen(barPointAt(model, b, barLength(model, b)), c) || !screen(target, tip))
                return false;
            const QPointF along = c - a;
            const double screenLength = std::hypot(along.x(), along.y());
            const double worldLength = barLength(model, b);
            const double magnitude = std::sqrt(offset[0] * offset[0] + offset[1] * offset[1] + offset[2] * offset[2]);
            if (screenLength < 1e-6 || worldLength <= 0.0) return false;
            const QPointF perp(-along.y() / screenLength, along.x() / screenLength);
            const QPointF projected = tip - p0;
            const double sign = projected.x() * perp.x() + projected.y() * perp.y() < 0.0 ? -1.0 : 1.0;
            out = p0 + perp * (sign * magnitude * screenLength / worldLength);
            return true;
        };
        // Dashed dimension line from the bar to the extreme, a marker and the two-line label in the colour of the law [V rp_12 / rp_16].
        const auto drawExtreme = [&](const QPointF& from, const QPointF& at, const QColor& colour, const std::array<std::string, 2>& label) {
            painter.save();
            painter.setPen(QPen(colour, 1.0, Qt::DashLine));
            painter.drawLine(from, at);
            painter.setPen(QPen(colour, 1.0));
            painter.setBrush(colour);
            painter.drawEllipse(at, 2.5, 2.5);
            painter.setFont(bold);
            painter.drawText(at + QPointF(6, -4), qs(label[0]));
            painter.drawText(at + QPointF(6, 10), qs(label[1]));
            painter.restore();
        };

        for (int k = 0; k < 6; ++k) {
            if (!laws[static_cast<std::size_t>(k)]) continue;
            const QColor colour = QColor::fromRgb(ResultsPanel::rowColour(k));
            const double scale = m_panel->scale(k);
            for (std::size_t b : bars) {
                const std::vector<BarLawPoint> law = barLaw(model, *ref->result, *ref->input, b, 20, m_solverOptions);
                const Vec3 dir = lawDirection(model, b, k);
                QPolygonF outline;
                QPointF first, last;
                bool ok = true;
                for (std::size_t i = 0; i < law.size() && ok; ++i) {
                    const Vec3 base = barPointAt(model, b, law[i].x);
                    const double len = lawDrawLength(law[i].forces[static_cast<std::size_t>(k)], scale);
                    QPointF s;
                    ok = offsetPoint(b, base, Vec3{dir[0] * len, dir[1] * len, dir[2] * len}, s);
                    outline << s;
                }
                if (!ok || law.empty() || !screen(barPointAt(model, b, law.back().x), last) || !screen(barPointAt(model, b, law.front().x), first)) continue;
                QPolygonF area = outline;
                area << last << first;
                painter.setPen(Qt::NoPen);
                if (m_panel->fillLaws()) {
                    QColor fill = colour;
                    fill.setAlpha(70);
                    painter.setBrush(fill);
                    painter.drawPolygon(area);
                }
                // Without fill only the contour is drawn [N: the original's drawing was not captured].
                painter.setBrush(Qt::NoBrush);
                painter.setPen(QPen(colour, 1.2));
                painter.drawPolyline(outline);
                painter.drawLine(first, outline.front());
                painter.drawLine(last, outline.back());
                if (!m_panel->drawExtremes()) continue;
                // «Dibujar valores máximos y mínimos»: «My máx.: 40.00 kN·m» / «X: 0.000 m» in bold in the colour of the law [V rp_12].
                std::size_t lo = 0, hi = 0;
                for (std::size_t i = 1; i < law.size(); ++i) {
                    if (law[i].forces[static_cast<std::size_t>(k)] < law[lo].forces[static_cast<std::size_t>(k)]) lo = i;
                    if (law[i].forces[static_cast<std::size_t>(k)] > law[hi].forces[static_cast<std::size_t>(k)]) hi = i;
                }
                static const char* const names[6] = {"N", "Vy", "Vz", "Mt", "My", "Mz"};
                for (const auto& [index, isMax] : {std::pair{hi, true}, std::pair{lo, false}}) {
                    const double value = law[index].forces[static_cast<std::size_t>(k)];
                    QPointF onBar;
                    if (!screen(barPointAt(model, b, law[index].x), onBar)) continue;
                    drawExtreme(onBar, outline[static_cast<int>(index)], colour,
                                extremeLabel(names[k], isMax, value, k < 3 ? 3 : 2, k < 3 ? "kN" : "kN·m", law[index].x));
                }
            }
        }

        // Deflections Fxy, Fxz, F (rows 7..9): curve of the deflection from the chord on the local axes, dashed dimension line and label at the
        // largest value [≈V rp_16: F máx. 4.367 mm at X 1.750 m]. The curve is drawn as position + scale x deflection (m), like the deformed shape [N].
        for (int row = ResultsPanel::kDeflectionRow; row < ResultsPanel::kForceRows; ++row) {
            if (!m_panel->rowChecked(row)) continue;
            const QColor colour = QColor::fromRgb(ResultsPanel::rowColour(row));
            const double scale = m_panel->scale(row);
            static const char* const names[3] = {"Fxy", "Fxz", "F"};
            for (std::size_t b : bars) {
                const std::vector<DeflectionPoint> d = barDeflection(model, *ref, b, 16, m_solverOptions);
                if (d.empty()) continue;
                const FemBar& bar = model.bars[b];
                const int which = row - ResultsPanel::kDeflectionRow;
                std::vector<double> values;
                QPolygonF curve;
                bool ok = true;
                for (const DeflectionPoint& p : d) {
                    const double value = which == 0 ? p.fxy : which == 1 ? p.fxz : p.f;
                    values.push_back(value);
                    const double ay = which == 1 ? 0.0 : p.fxy, az = which == 0 ? 0.0 : p.fxz;  // F is drawn along the resultant of the two components
                    const Vec3 offset{scale * (ay * bar.ey[0] + az * bar.ez[0]), scale * (ay * bar.ey[1] + az * bar.ez[1]),
                                      scale * (ay * bar.ey[2] + az * bar.ez[2])};
                    QPointF s;
                    ok = ok && offsetPoint(b, barPointAt(model, b, p.x), offset, s);
                    curve << s;
                }
                if (!ok) continue;
                painter.setBrush(Qt::NoBrush);
                painter.setPen(QPen(colour, 1.0));
                painter.drawPolyline(curve);
                if (!m_panel->drawExtremes()) continue;
                const std::size_t i = largestAbsIndex(values);
                QPointF onBar;
                if (!screen(barPointAt(model, b, d[i].x), onBar)) continue;
                drawExtreme(onBar, curve[static_cast<int>(i)], colour, extremeLabel(names[which], values[i] >= 0.0, values[i] * 1000.0, 3, "mm", d[i].x));
            }
        }

        if (m_panel->deformed()) {
            const double scale = m_panel->scale(ResultsPanel::kDeformedRow);
            painter.setPen(QPen(QColor(0xFF, 0x3F, 0x00), 1.5));
            for (std::size_t b : bars) {
                QPolygonF line;
                bool ok = true;
                for (const Vec3& p : deformedBar(model, *ref, b, scale, 20, m_solverOptions)) {
                    QPointF s;
                    ok = ok && screen(p, s);
                    line << s;
                }
                if (ok) painter.drawPolyline(line);
            }
            // Shells: the edges of the mesh triangles through their mid-side nodes.
            const LinearResult& lin = *ref->linear;
            const auto& disp = ref->result->meshDisplacements;
            for (const auto& t : lin.mesh.triangles) {
                const int order[7] = {t.nodes[0], t.nodes[3], t.nodes[1], t.nodes[4], t.nodes[2], t.nodes[5], t.nodes[0]};
                QPolygonF line;
                bool ok = true;
                for (int n : order) {
                    if (n < 0 || static_cast<std::size_t>(n) >= lin.mesh.positions.size() || static_cast<std::size_t>(n) >= disp.size()) {
                        ok = false;
                        break;
                    }
                    const auto& p = lin.mesh.positions[static_cast<std::size_t>(n)];
                    const Dof6& d = disp[static_cast<std::size_t>(n)];
                    QPointF s;
                    ok = ok && screen(Vec3{p[0] + scale * d[0], p[1] + scale * d[1], p[2] + scale * d[2]}, s);
                    line << s;
                }
                if (ok) painter.drawPolyline(line);
            }
        }

        // Local axes gizmo of the bar while «Esfuerzos» is open: a small section frame with the arrows x red, y green, z blue [V rp_07; the point
        // of the bar (0.8 L here) and the bars that get one are N].
        constexpr std::size_t kMaxGizmos = 20;
        for (std::size_t n = 0; n < bars.size() && n < kMaxGizmos; ++n) drawAxesGizmo(painter, viewport, model, bars[n]);
        if (m_query) {
            const auto b = barIndexOf(model, m_query->first);
            QPointF at;
            if (b && screen(barPointAt(model, *b, m_query->second), at)) {
                std::vector<BalloonLine> lines;
                if (viewport.structure())
                    for (const QString& row : Viewport3D::barInfoLines(*viewport.structure(), m_query->first)) lines.push_back({row.toStdString(), ""});
                const Dof6 f = barForcesAt(model, *ref->result, *ref->input, *b, m_query->second);
                for (BalloonLine& l : forceLines(m_query->second, f, laws)) lines.push_back(std::move(l));
                drawBalloon(painter, QString(), lines, at);
            }
        }
    }
    painter.restore();
}

}  // namespace cc::app
