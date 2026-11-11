#include "PanelTool.h"

#include <QAction>
#include <QActionGroup>
#include <QDialog>
#include <QFontMetricsF>
#include <QPainter>
#include <QPen>
#include <algorithm>
#include <cmath>
#include <map>
#include <numbers>
#include <tuple>

#include "core/bar_load.h"
#include "core/panel_distribution.h"
#include "NodalLoadDialogs.h"
#include "NodalLoadTool.h"
#include "PanelDialogs.h"
#include "PanelPicking.h"
#include "UserMessages.h"
#include "Viewport3D.h"

namespace cc::app {

using namespace cc::core;

namespace {

const QColor kOrange(QStringLiteral("#DA8735"));  // selected element (same orange as the viewport)
const QColor kGreyLoad(150, 150, 150);            // measured: the loads of the other group are drawn pale grey
const QColor kSideGreen(0, 150, 0);               // measured: prolonged sides, green dashed
const QColor kSideBlue(0, 90, 220);               // the side under the cursor
const QColor kCopyCyan(0, 190, 220);              // the load being copied

Vec3 toVec(const Point3& p) { return Vec3{p.x, p.y, p.z}; }
Point3 toPoint(const Vec3& v) { return Point3{v.x, v.y, v.z}; }

QString rich(const QString& title, const QString& text, bool f1Footer) {
    QString html = QStringLiteral("<b>") + title.toHtmlEscaped() + QStringLiteral("</b>");
    if (!text.isEmpty()) html += QStringLiteral("<br>") + text.toHtmlEscaped().replace(QLatin1Char('\n'), QStringLiteral("<br>"));
    if (f1Footer) html += QStringLiteral("<br><br>") + PanelTool::tr("Pulse 'F1' alternativamente para mostrar u ocultar los textos de ayuda.").toHtmlEscaped();
    return html;
}

}  // namespace

PanelTool::PanelTool(QObject* parent) : QObject(parent) {
    m_group = new QActionGroup(this);
    m_group->setExclusionPolicy(QActionGroup::ExclusionPolicy::ExclusiveOptional);
    const auto make = [this](Mode mode, const QString& text) {
        auto* action = new QAction(text, this);
        action->setCheckable(true);
        action->setEnabled(false);
        const QString tip = tipText(mode);
        action->setStatusTip(tip);
        action->setToolTip(tip.isEmpty() ? QStringLiteral(" ") : tip);  // measured: Dividir and Hipótesis show no tooltip (the ribbon clears it)
        m_group->addAction(action);
        connect(action, &QAction::toggled, this, [this, mode](bool on) {
            if (on) setMode(mode);
            else if (m_mode == mode) setMode(Mode::None);
        });
        m_actions.emplace_back(mode, action);
    };
    make(Mode::PanelNew, tr("Nuevo"));
    make(Mode::PanelEdit, tr("Editar"));
    make(Mode::PanelDelete, tr("Borrar"));
    make(Mode::PanelMove, tr("Mover"));
    make(Mode::PanelDivide, tr("Dividir"));
    make(Mode::PanelDirection, tr("Dirección de reparto"));
    make(Mode::SurfNew, tr("Nueva"));
    make(Mode::SurfEdit, tr("Editar"));
    make(Mode::SurfDelete, tr("Borrar"));
    make(Mode::SurfCase, tr("Hipótesis"));
    make(Mode::SurfCopy, tr("Copiar"));
    make(Mode::SurfMove, tr("Mover"));
    make(Mode::SurfAssign, tr("Asignar"));
}

QAction* PanelTool::action(Mode mode) const {
    for (const auto& [m, a] : m_actions)
        if (m == mode) return a;
    return nullptr;
}

std::vector<QAction*> PanelTool::actionsWithoutTooltip() const { return {action(Mode::PanelDivide), action(Mode::SurfCase)}; }

// ---- texts -------------------------------------------------------------------------------------------------------------------

QString PanelTool::statusText(Mode mode, int step, bool hasSelection) {
    (void)hasSelection;
    const QString groupPanels = tr("Paños"), groupSurf = tr("Cargas superficiales");
    const QString selectHints = tr(" - LMB: Seleccione un grupo de elementos. - RMB: Actuar sobre los elementos seleccionados.");
    const QString moveHints[4] = {
        tr(" - LMB: Seleccione un paño. - RMB: Comenzar otra vez."),
        tr(" - LMB: Seleccione los vértices a desplazar. - RMB: Actuar sobre los elementos seleccionados. Si no hay ningún elemento seleccionado, se finaliza la ejecución."),
        tr(" - LMB: Seleccione el punto de referencia para el desplazamiento. - RMB: Volver al paso anterior."),
        tr(" - LMB: Seleccione un punto de destino al que desea desplazar el punto de referencia. - RMB: Volver al paso anterior.")};
    const int s = std::clamp(step, 0, 3);
    const auto head = [](const QString& group, const QString& name) { return group + QStringLiteral(" - ") + name + QStringLiteral(" |") + QLatin1Char(' '); };
    switch (mode) {
        case Mode::None: return tr("Seleccione una opción del menú.");
        case Mode::PanelNew:
            return head(groupPanels, tr("Nuevo")) +
                   (step == 0 ? tr(" - LMB: Introduzca los puntos de la polilínea.")
                              : tr(" - LMB: Seleccione la dirección de reparto para las cargas superficiales asignadas al paño. - RMB: Comenzar otra vez."));
        case Mode::PanelEdit: return head(groupPanels, tr("Editar")) + selectHints;
        case Mode::PanelDelete: return head(groupPanels, tr("Borrar")) + selectHints;
        case Mode::PanelMove: return head(groupPanels, tr("Mover")) + moveHints[s];
        case Mode::PanelDivide: return head(groupPanels, tr("Dividir")) + tr(" - LMB: Introduzca un punto.");
        case Mode::PanelDirection:
            // Step 1 is NOT measured (the original keeps the first text): the RMB hint is PROVISIONAL.
            return head(groupPanels, tr("Dirección de reparto")) + tr(" - LMB: Seleccione un elemento.") +
                   (step > 0 ? tr(" - RMB: Actuar sobre los elementos seleccionados.") : QString());
        case Mode::SurfNew: return head(groupSurf, tr("Nueva")) + tr(" - LMB: Introduzca los puntos de la polilínea.");
        case Mode::SurfEdit: return head(groupSurf, tr("Editar")) + selectHints;
        case Mode::SurfDelete: return head(groupSurf, tr("Borrar")) + selectHints;
        case Mode::SurfCase: return head(groupSurf, tr("Hipótesis")) + selectHints;
        case Mode::SurfCopy: return head(groupSurf, tr("Copiar")) + tr(" - LMB: Seleccione un elemento.");
        case Mode::SurfMove:
            return head(groupSurf, tr("Mover")) + (step == 0 ? tr(" - LMB: Seleccione una carga superficial. - RMB: Comenzar otra vez.") : moveHints[s]);
        case Mode::SurfAssign: return head(groupSurf, tr("Asignar")) + selectHints;
    }
    return {};
}

QString PanelTool::tipText(Mode mode) {
    // Texts of ed3dlib (#64667 ... #64690), measured identical in the original (cargas_panos.md 10.1).
    switch (mode) {
        case Mode::PanelNew:
            return rich(tr("Nuevo"),
                        tr("Permite introducir un paño formado por un polígono cerrado que puede tener cargas aplicadas en toda su superficie, o bien, cargas superficiales aplicadas sobre ciertas zonas del paño.") +
                            QLatin1Char('\n') +
                            tr("Es necesario indicar la dirección de reparto unidireccional que tendrán tanto las cargas del propio paño como las superficiales contenidas en él. Por ejemplo, en la cubierta de una nave, dicha dirección es la de las correas, si lo que quiere cargarse son los pórticos transversales."),
                        true);
        case Mode::PanelEdit:
            return rich(tr("Editar"),
                        tr("Permite modificar la lista de cargas aplicadas a los paños seleccionados.") + QLatin1Char('\n') +
                            tr("Si el paño no es paralelo al plano global XY, el diálogo muestra dos pestañas: una para cargas uniformes y otra para cargas variables en altura, mientras que si es un paño horizontal sólo se muestra la lista de cargas uniformes.") +
                            QLatin1Char('\n') + tr("Las cargas se introducen con su signo, ya que determinará el sentido de aplicación.") + QLatin1Char('\n') +
                            tr("Se sigue el criterio de que las cargas correspondientes a hipótesis de viento se aplican perpendicularmente al paño (teniendo en cuenta el sentido del vector de carga positiva), mientras que para el resto de las hipótesis las cargas se aplican verticalmente, según el signo de la carga: positivo indica sentido vertical hacia abajo.") +
                            QLatin1Char('\n') + tr("Esta opción permite, además, igualar cargas entre distintos paños si la selección es múltiple."),
                        true);
        case Mode::PanelDelete:
            return rich(tr("Borrar"), tr("Mediante esta opción se borran los paños seleccionados y, por tanto, sus cargas superficiales asociadas."), false);
        case Mode::PanelMove:
            return rich(tr("Mover"),
                        tr("Mediante esta opción es posible modificar la posición del polígono que define la geometría del paño.") + QLatin1Char('\n') +
                            tr("Para ello se deberá seleccionar primero el paño a modificar.") + QLatin1Char(' ') +
                            tr("A continuación seleccione los vértices del polígono que desea desplazar, un punto de referencia y el punto de destino al que desea mover el punto de referencia indicado."),
                        true);
        case Mode::PanelDivide: return {};  // measured: no tooltip
        case Mode::PanelDirection:
            return rich(tr("Modificar la dirección de reparto de cargas"),
                        tr("Con esta opción es posible modificar la dirección de reparto de las cargas de un paño. Una vez seleccionado el paño, para indicar la nueva dirección se debe seleccionar una de las rectas auxiliares que definen los lados del paño."),
                        false);
        case Mode::SurfNew:
            return rich(tr("Nueva"),
                        tr("Esta opción permite introducir una carga superficial de forma poligonal. Las cargas pueden ser uniformes o variables en altura si la superficie no es horizontal.") +
                            QLatin1Char('\n') +
                            tr("Esta superficie reparte las cargas según la dirección del paño al cual pertenece. Una carga superficial pertenece al paño en el cual está contenida completamente, por lo que no se permite que una carga solape con más de un paño. Un ejemplo de aplicación de cargas superficiales puede ser cargar zonas de un paño con distintas presiones de viento.") +
                            QLatin1Char('\n') +
                            tr("Al introducir una nueva carga superficial se detecta si solapa con algún paño existente. Si no lo hace, se pregunta al usuario si desea generar un paño de idéntica forma a la de la superficie creada. Este nuevo paño tiene la dirección de reparto dada por la recta que une los dos primeros puntos de introducción de la superficie, aunque dicha dirección se puede modificar posteriormente con la opción '%1'.")
                                .arg(tr("Modificar la dirección de reparto de cargas")),
                        true);
        case Mode::SurfEdit:
            return rich(tr("Editar"), tr("Permite modificar las cargas superficiales seleccionadas. En pantalla sólo se muestran las cargas asociadas a la 'hipótesis vista'."), false);
        case Mode::SurfDelete:
            return rich(tr("Borrar"), tr("Borra las cargas superficiales seleccionadas entre las correspondientes a la 'hipótesis vista'."), false);
        case Mode::SurfCase: return {};  // measured: no tooltip
        case Mode::SurfCopy:
            return rich(tr("Copiar"),
                        tr("Con esta opción se crea una nueva carga superficial a partir de los datos de una ya existente. La nueva carga superficial tiene la misma forma y posición que la existente, por lo que sólo habrá que indicar la hipótesis, el tipo y los valores de carga."),
                        false);
        case Mode::SurfMove:
            return rich(tr("Mover"),
                        tr("Mediante esta opción es posible modificar la posición del polígono que define la geometría de la carga superficial.") + QLatin1Char('\n') +
                            tr("Para ello se deberá seleccionar primero la carga a modificar.") + QLatin1Char(' ') +
                            tr("A continuación seleccione los vértices del polígono que desea desplazar, un punto de referencia y el punto de destino al que desea mover el punto de referencia indicado."),
                        true);
        case Mode::SurfAssign:
            return rich(tr("Activar/desactivar asignación de cargas superficiales"),
                        tr("Permite indicar si una o varias barras pueden ser cargadas tanto por paños como por cargas superficiales. Si se desactiva una barra, ésta no admite carga en ninguna hipótesis. Esta opción no tiene efecto sobre los tirantes, ya que éstos no admiten la aplicación de cargas.") +
                            QLatin1Char('\n') + tr("Las barras desactivadas se visualizan en líneas de trazos.") + QLatin1Char('\n') +
                            tr("Por defecto, todas las barras (excepto los tirantes) pueden soportar cargas superficiales."),
                        true);
        case Mode::None: break;
    }
    return {};
}

// ---- infrastructure ----------------------------------------------------------------------------------------------------------

void PanelTool::pruneViewports() {
    m_viewports.removeIf([](const QPointer<Viewport3D>& v) { return v.isNull(); });
}

int PanelTool::run(QDialog& dialog) { return m_runner ? m_runner(dialog) : dialog.exec(); }

void PanelTool::warn(const QString& text, bool error) {
    messages::show(m_dialogParent, error ? messages::Kind::Error : messages::Kind::Warning, text);
}
void PanelTool::info(const QString& text) { messages::show(m_dialogParent, messages::Kind::Information, text); }
bool PanelTool::ask(const QString& text) { return messages::show(m_dialogParent, messages::Kind::Question, text); }

int PanelTool::viewCase() const {
    if (m_host) return m_host->viewCaseId();
    return m_project && !m_project->loadCases.cases.empty() ? m_project->loadCases.cases.front().id : 0;
}

bool PanelTool::caseInView(int loadCaseId) const { return !m_host || m_host->viewAll() || loadCaseId == m_host->viewCaseId(); }

bool PanelTool::surfaceLoadInView(const SurfaceLoad& load) const { return caseInView(load.data.loadCaseId); }

void PanelTool::setHost(NodalLoadTool* host) {
    m_host = host;
    if (!host) return;
    connect(host, &NodalLoadTool::viewChanged, this, [this] {
        configureViewports();
        if (m_mode != Mode::None) announce();
    });
    connect(host, &NodalLoadTool::viewPanelClosed, this, [this] {
        if (isSurfaceMode(m_mode)) deactivate();  // closing the window ends the command (like the others)
    });
}

void PanelTool::setProject(Project* project, UndoStack* undo) {
    deactivate();
    m_project = project;
    m_undo = undo;
    m_selPanels.clear();
    m_selLoads.clear();
    pruneViewports();
    updateEnabled();
    for (const QPointer<Viewport3D>& viewport : m_viewports) viewport->update();
}

void PanelTool::addViewport(Viewport3D* viewport) {
    if (!viewport || m_viewports.contains(viewport)) return;
    m_viewports.push_back(viewport);
    const QPointer<Viewport3D> guard(viewport);
    viewport->setOverlay([this, guard](QPainter& painter) {
        if (guard) paint(painter, *guard);
    });
    viewport->setDashedBars([this](int barId) { return m_project && !surfaceBarAssigned(m_project->surfaceLoads, barId); });
    connect(viewport, &Viewport3D::pickClicked, this, [this, viewport](const QPointF& pos) { pickAt(viewport, pos); });
    connect(viewport, &Viewport3D::actOnSelection, this, [this, viewport] { act(viewport); });
    connect(viewport, &Viewport3D::pointEntered, this, [this](const Vec3& world, const SnapResult& snap) {
        if (snap.snapped()) pointEntered(world);  // like Nudos: the original needs a snap under the pointer
    });
    connect(viewport, &Viewport3D::rightClicked, this, [this] { rightClicked(); });
    connect(viewport, &Viewport3D::escapePressed, this, [this] { deactivate(); });
    if (m_mode != Mode::None) configureViewports();
}

void PanelTool::updateEnabled() {
    const bool open = m_project != nullptr;
    const bool nodes = open && !m_project->structure.nodes.empty();
    const bool panels = open && hasPanels(m_project->panels);
    const bool loads = open && hasSurfaceLoads(m_project->surfaceLoads);
    // Measured (cargas_panos.md 10.1 / 10.6): no nodes -> everything grey; nodes -> Nuevo / Nueva / Asignar; panels -> the other five of Paños;
    // surface loads -> Editar / Borrar / Hipótesis / Copiar / Mover.
    for (const auto& [mode, a] : m_actions) {
        bool on = false;
        switch (mode) {
            case Mode::PanelNew:
            case Mode::SurfNew:
            case Mode::SurfAssign: on = nodes; break;
            case Mode::PanelEdit:
            case Mode::PanelDelete:
            case Mode::PanelMove:
            case Mode::PanelDivide:
            case Mode::PanelDirection: on = panels; break;
            default: on = loads; break;
        }
        a->setEnabled(on);
        if (!on && a->isChecked()) a->setChecked(false);
    }
}

void PanelTool::structureChanged() {
    updateEnabled();
    pruneViewports();
    if (m_project) {  // drop what no longer exists
        for (auto it = m_selPanels.begin(); it != m_selPanels.end();) it = findPanel(m_project->panels, *it) ? std::next(it) : m_selPanels.erase(it);
        for (auto it = m_selLoads.begin(); it != m_selLoads.end();) it = findSurfaceLoad(m_project->surfaceLoads, *it) ? std::next(it) : m_selLoads.erase(it);
        if (m_movingId != 0) {
            const bool exists = isPanelMode(m_mode) ? findPanel(m_project->panels, m_movingId) != nullptr : findSurfaceLoad(m_project->surfaceLoads, m_movingId) != nullptr;
            if (!exists) resetSteps();
        }
    }
    configureViewports();
    for (const QPointer<Viewport3D>& viewport : m_viewports) viewport->update();
}

bool PanelTool::usesSnap() const {
    switch (m_mode) {
        case Mode::PanelNew: return m_step == 0;
        case Mode::PanelDivide: return true;
        case Mode::SurfNew: return true;
        case Mode::PanelMove:
        case Mode::SurfMove: return m_step >= 2;
        default: return false;
    }
}

bool PanelTool::usesPick() const {
    switch (m_mode) {
        case Mode::None:
        case Mode::SurfAssign: return false;
        default: return !usesSnap();
    }
}

bool PanelTool::showsViewPanel() const { return isSurfaceMode(m_mode) && m_mode != Mode::SurfAssign; }

bool PanelTool::panelStepSides() const {
    return (m_mode == Mode::PanelNew && m_step == 1) || (m_mode == Mode::PanelDirection && m_step == 1);
}

void PanelTool::setDirectionMode(PanelDirectionMode mode) {
    m_direction = mode;
    if (m_directionPanel) m_directionPanel->setMode(mode);
}

void PanelTool::showDirectionPanel() {
    if (m_directionPanel && !m_directionPanel->isVisible()) {
        m_directionPanel->deleteLater();
        m_directionPanel.clear();
    }
    if (!m_directionPanel) {
        m_directionPanel = new PanelDirectionPanel(m_dialogParent);
        m_directionPanel->setAttribute(Qt::WA_DeleteOnClose);
        m_directionPanel->setMode(m_direction);
        connect(m_directionPanel, &PanelDirectionPanel::modeChanged, this, [this](PanelDirectionMode mode) {
            m_direction = mode;
            for (const QPointer<Viewport3D>& viewport : m_viewports) viewport->update();
        });
        connect(m_directionPanel, &PanelDirectionPanel::closed, this, [this] {
            if (m_mode == Mode::PanelNew || m_mode == Mode::PanelDirection) deactivate();  // closing the window ends the command
        });
    }
    if (!m_directionPanel->isVisible()) {
        if (m_dialogParent) {  // measured: small window at the lower middle of the work area
            const QWidget* parent = m_dialogParent;
            m_directionPanel->move(parent->mapToGlobal(QPoint(410, std::max(40, parent->height() - 500))));
        }
        m_directionPanel->show();
    }
    m_directionPanel->raise();
}

void PanelTool::closeDirectionPanel() {
    if (!m_directionPanel) return;
    QPointer<PanelDirectionPanel> closing = m_directionPanel;
    m_directionPanel.clear();
    closing->close();
}

void PanelTool::showMoveBox() {
    MoveSurfaceLoadsDialog dialog(m_alsoOther, m_dialogParent);
    if (run(dialog) == QDialog::Accepted) m_alsoOther = dialog.alsoOtherCases();
}

std::vector<Vec3> PanelTool::rubber() const {
    std::vector<Vec3> out;
    // The elastic polyline of Nuevo / Nueva / Dividir; Mover draws its own reference and preview.
    if (m_mode == Mode::None || m_mode == Mode::PanelMove || m_mode == Mode::SurfMove) return out;
    for (const Point3& p : m_points) out.push_back(toVec(p));
    return out;
}

void PanelTool::announce() { emit statusChanged(statusText(m_mode, m_step)); }

void PanelTool::setMode(Mode mode) {
    if (mode == m_mode) return;
    if (mode != Mode::None) emit aboutToActivate();
    m_mode = mode;
    resetSteps();
    m_selPanels.clear();
    m_selLoads.clear();
    if (mode == Mode::None) {
        closeDirectionPanel();
        if (m_host) m_host->closeViewPanel();
    } else {
        if (mode == Mode::PanelNew || mode == Mode::PanelDirection) showDirectionPanel();
        else closeDirectionPanel();
        if (showsViewPanel() && m_host) m_host->showViewPanel();
        else if (m_host && !showsViewPanel()) m_host->closeViewPanel();
    }
    configureViewports();
    emit modeChanged(mode);
    announce();
    if (mode == Mode::SurfMove) showMoveBox();  // measured: the box appears at once, before anything is selected
}

void PanelTool::deactivate() {
    if (QAction* checked = m_group->checkedAction()) checked->setChecked(false);
    setMode(Mode::None);
}

void PanelTool::resetSteps() {
    m_step = 0;
    m_points.clear();
    m_contour.clear();
    m_selVertices.clear();
    m_pending.clear();
    m_movingId = 0;
    m_side.reset();
    m_copySource = 0;
    if (m_mode == Mode::PanelDirection || m_mode == Mode::PanelMove) m_selPanels.clear();
    if (m_mode == Mode::SurfMove) m_selLoads.clear();
    configureViewports();
    if (m_mode != Mode::None) announce();
}

void PanelTool::configureViewports() {
    pruneViewports();
    const bool on = m_mode != Mode::None;
    if (!on && !m_configured) return;  // idle: the viewports belong to another command
    m_configured = on;
    const Project* project = m_project;
    for (const QPointer<Viewport3D>& viewport : m_viewports) {
        viewport->setSelectionMode(false);
        viewport->setSnapMode(false);
        viewport->setPickMode(false);
        viewport->setRubberChain({});
        viewport->setRubberPoints(rubber());
        viewport->setNodeLabelsVisible(false);
        viewport->setBarExclusion({});
        viewport->setNodeExclusion({});
        viewport->setShellExclusion({});
        viewport->setWindowOnlySelection(false);
        viewport->setLoadSelectionHighlight(false);
        viewport->update();
        if (!on) continue;
        if (m_mode == Mode::SurfAssign) {
            // Bars only (measured: LMB selects, RMB toggles them); the ties never change, they are skipped by the command.
            viewport->setNodeExclusion([](int) { return true; });
            viewport->setShellExclusion([](int) { return true; });
            viewport->setSelectionFilter(SelectionFilter::only(ElementKind::Bar));
            viewport->setSelectionMode(true);
            (void)project;
        } else if (usesSnap()) {
            viewport->setSnapMode(true);
        } else {
            viewport->setPickMode(true);
        }
        viewport->setFocus();
    }
}

// ---- hit-testing -------------------------------------------------------------------------------------------------------------

std::vector<std::pair<int, QPolygonF>> PanelTool::panelPolygons(const Viewport3D& viewport) const {
    std::vector<std::pair<int, QPolygonF>> out;
    if (!m_project) return out;
    const picking::Projector project = [&viewport](const Point3& p, QPointF& s) { return viewport.worldToScreen(toVec(p), s); };
    for (const Panel& panel : m_project->panels.items) out.emplace_back(panel.id, picking::projectPolygon(project, panel.vertices));
    return out;
}

std::vector<std::pair<int, QPolygonF>> PanelTool::loadPolygons(const Viewport3D& viewport) const {
    std::vector<std::pair<int, QPolygonF>> out;
    if (!m_project) return out;
    const picking::Projector project = [&viewport](const Point3& p, QPointF& s) { return viewport.worldToScreen(toVec(p), s); };
    for (const SurfaceLoad& load : m_project->surfaceLoads.items)
        if (surfaceLoadInView(load)) out.emplace_back(load.id, picking::projectPolygon(project, load.contour));
    return out;
}

const std::vector<Point3>* PanelTool::sideContour() const {
    if (m_mode == Mode::PanelNew && m_step == 1) return &m_contour;
    if (m_mode == Mode::PanelDirection && m_step == 1 && m_project && !m_selPanels.empty())
        if (const Panel* panel = findPanel(m_project->panels, *m_selPanels.begin())) return &panel->vertices;
    return nullptr;
}

const std::vector<Point3>* PanelTool::movingPolygon() const {
    if (!m_project || m_movingId == 0) return nullptr;
    if (m_mode == Mode::PanelMove) {
        if (const Panel* panel = findPanel(m_project->panels, m_movingId)) return &panel->vertices;
    } else if (m_mode == Mode::SurfMove) {
        if (const SurfaceLoad* load = findSurfaceLoad(m_project->surfaceLoads, m_movingId)) return &load->contour;
    }
    return nullptr;
}

void PanelTool::togglePanels(const std::vector<int>& ids) {
    if (ids.empty()) return;
    const bool anySelected = std::any_of(ids.begin(), ids.end(), [&](int id) { return m_selPanels.count(id) != 0; });
    for (int id : ids) {
        if (anySelected) m_selPanels.erase(id);
        else m_selPanels.insert(id);
    }
}

void PanelTool::toggleLoads(const std::vector<int>& ids) {
    for (int id : ids) {
        if (m_selLoads.count(id)) m_selLoads.erase(id);
        else m_selLoads.insert(id);
    }
}

// ---- gestures ----------------------------------------------------------------------------------------------------------------

void PanelTool::pickAt(Viewport3D* viewport, const QPointF& pos) {
    if (!m_project || !m_undo || !viewport || m_mode == Mode::None) return;
    const picking::Projector project = [viewport](const Point3& p, QPointF& s) { return viewport->worldToScreen(toVec(p), s); };
    switch (m_mode) {
        case Mode::PanelEdit:
        case Mode::PanelDelete: togglePanels(picking::polygonsAt(panelPolygons(*viewport), pos)); break;
        case Mode::PanelNew:
            if (m_step == 1) {  // side of the polygon: LMB on a prolonged line (toggles)
                const QPolygonF poly = picking::projectPolygon(project, m_contour);
                if (const auto side = picking::sideLineAt(poly, pos)) m_side = (m_side && *m_side == *side) ? std::nullopt : std::optional<int>(*side);
            }
            break;
        case Mode::PanelDirection: {
            const std::vector<std::pair<int, QPolygonF>> polygons = panelPolygons(*viewport);
            if (m_step == 1) {
                if (const std::vector<Point3>* contour = sideContour()) {
                    const QPolygonF poly = picking::projectPolygon(project, *contour);
                    if (const auto side = picking::sideLineAt(poly, pos)) {
                        m_side = (m_side && *m_side == *side) ? std::nullopt : std::optional<int>(*side);
                        break;
                    }
                }
            }
            const std::vector<int> hits = picking::polygonsAt(polygons, pos);
            if (!hits.empty()) {  // measured: LMB on the panel selects it and shows its auxiliary lines
                m_selPanels = {hits.front()};
                m_side.reset();
                m_step = 1;
                announce();
            }
            break;
        }
        case Mode::PanelMove: {
            if (m_step == 0) {
                const std::vector<int> hits = picking::polygonsAt(panelPolygons(*viewport), pos);
                if (hits.empty()) break;
                m_movingId = hits.front();
                m_selPanels = {m_movingId};
                m_selVertices.clear();
                m_step = 1;
                announce();
            } else if (m_step == 1) {
                if (const Panel* panel = findPanel(m_project->panels, m_movingId))
                    if (const auto vertex = picking::vertexAt(picking::projectPolygon(project, panel->vertices), pos)) {
                        if (!m_selVertices.erase(*vertex)) m_selVertices.insert(*vertex);
                    }
            }
            break;
        }
        case Mode::SurfEdit:
        case Mode::SurfDelete:
        case Mode::SurfCase: {
            const std::vector<int> hits = picking::polygonsAt(loadPolygons(*viewport), pos);
            if (!hits.empty()) toggleLoads({hits.back()});  // of stacked loads only the last one (measured: Borrar removes one)
            break;
        }
        case Mode::SurfCopy: {
            const std::vector<int> hits = picking::polygonsAt(loadPolygons(*viewport), pos);
            if (hits.empty()) break;
            const SurfaceLoad* source = findSurfaceLoad(m_project->surfaceLoads, hits.back());
            if (!source) break;
            // Measured: one click opens the dialog with the data of the source, which is drawn in cyan meanwhile.
            m_copySource = source->id;
            for (const QPointer<Viewport3D>& v : m_viewports) v->update();
            SurfaceLoadDialog dialog(tr("Introducir cargas superficiales"), m_project->loadCases, source->contour, source->data, m_dialogParent);
            dialog.setMaximizable();
            const bool accepted = run(dialog) == QDialog::Accepted;
            m_copySource = 0;
            if (accepted) {
                SurfaceLoadIssue issue;
                const PanelError error = copySurfaceLoad(*m_project, *m_undo, source->id, dialog.data(), nullptr, &issue);
                if (error == PanelError::InvalidData) warn(QString::fromStdString(issue.message), issue.kind == SurfaceLoadIssueKind::Error);
                else if (error != PanelError::None) warn(QString::fromStdString(panelErrorMessage(error)));
            }
            break;
        }
        case Mode::SurfMove: {
            if (m_step == 0) {
                const std::vector<int> hits = picking::polygonsAt(loadPolygons(*viewport), pos);
                if (hits.empty()) break;
                m_movingId = hits.back();
                m_selLoads = {m_movingId};
                m_selVertices.clear();
                m_step = 1;
                announce();
            } else if (m_step == 1) {
                if (const SurfaceLoad* load = findSurfaceLoad(m_project->surfaceLoads, m_movingId))
                    if (const auto vertex = picking::vertexAt(picking::projectPolygon(project, load->contour), pos)) {
                        if (!m_selVertices.erase(*vertex)) m_selVertices.insert(*vertex);
                    }
            }
            break;
        }
        default: break;
    }
    for (const QPointer<Viewport3D>& v : m_viewports) v->update();
}

void PanelTool::act(Viewport3D* viewport) {
    if (!m_project || !m_undo || m_mode == Mode::None) return;
    switch (m_mode) {
        case Mode::PanelEdit:
            if (!m_selPanels.empty()) editPanels({m_selPanels.begin(), m_selPanels.end()});
            m_selPanels.clear();
            break;
        case Mode::PanelDelete:
            if (!m_selPanels.empty()) deletePanels({m_selPanels.begin(), m_selPanels.end()});  // immediate, no question (measured)
            m_selPanels.clear();
            break;
        case Mode::PanelNew:
            if (m_step == 1) {
                if (m_side) createPanelFromPolyline();
                else resetSteps();  // «Comenzar otra vez»
            }
            break;
        case Mode::PanelDirection:
            if (m_step == 1) {
                if (m_side && !m_selPanels.empty()) {
                    const PanelError error = setPanelDirection(*m_project, *m_undo, {m_selPanels.begin(), m_selPanels.end()}, *m_side, m_direction);
                    if (error != PanelError::None && error != PanelError::NothingToChange) warn(QString::fromStdString(panelErrorMessage(error)));
                }
                resetSteps();
            }
            break;
        case Mode::PanelMove:
        case Mode::SurfMove:
            if (m_step == 0) {
                if (m_mode == Mode::SurfMove) showMoveBox();  // measured: RMB without a selection shows the box again
                else resetSteps();
            } else if (m_step == 1) {
                if (!m_pending.empty()) {
                    commitMove();  // measured: the RMB applies the displacement chosen
                } else if (m_selVertices.empty()) {
                    resetSteps();  // «se finaliza la ejecución»
                } else {
                    info(tr("A continuación, seleccione el punto que se tomará como referencia para el desplazamiento y, posteriormente, seleccione el destino de dicho punto de referencia."));
                    m_step = 2;
                    m_points.clear();
                    configureViewports();
                    announce();
                }
            }
            break;
        case Mode::SurfEdit:
            if (!m_selLoads.empty()) editSurfaceLoads({m_selLoads.begin(), m_selLoads.end()});
            m_selLoads.clear();
            break;
        case Mode::SurfDelete:
            if (!m_selLoads.empty()) deleteSurfaceLoads({m_selLoads.begin(), m_selLoads.end()});
            m_selLoads.clear();
            break;
        case Mode::SurfCase:
            if (!m_selLoads.empty()) changeSurfaceCase({m_selLoads.begin(), m_selLoads.end()});
            m_selLoads.clear();
            break;
        case Mode::SurfAssign: {
            if (!viewport) break;
            const std::vector<int> bars = viewport->selection().ids(ElementKind::Bar);
            if (bars.empty()) break;
            assignBars(bars);
            viewport->clearSelection();
            break;
        }
        default: break;
    }
    for (const QPointer<Viewport3D>& v : m_viewports) v->update();
    if (m_mode != Mode::None) announce();
}

void PanelTool::pointEntered(const Vec3& world) {
    if (!m_project || !m_undo || !usesSnap()) return;
    if (!m_points.empty() && m_points.back().x == world.x && m_points.back().y == world.y && m_points.back().z == world.z) return;
    const Point3 p = toPoint(world);
    switch (m_mode) {
        case Mode::PanelNew:
        case Mode::SurfNew:
            m_points.push_back(p);
            break;
        case Mode::PanelDivide:
            m_points.push_back(p);
            if (m_points.size() == 2) {
                const PanelError error = dividePanels(*m_project, *m_undo, m_points[0], m_points[1]);
                if (error != PanelError::None) warn(QString::fromStdString(panelErrorMessage(error)));  // measured: no box on success
                m_points.clear();
            }
            break;
        case Mode::PanelMove:
        case Mode::SurfMove:
            if (m_step == 2) {
                m_points = {p};  // reference point
                m_step = 3;
                announce();
            } else if (m_step == 3) {
                const Point3 delta{p.x - m_points[0].x, p.y - m_points[0].y, p.z - m_points[0].z};
                applyMove(delta);
            }
            break;
        default: break;
    }
    for (const QPointer<Viewport3D>& viewport : m_viewports) {
        viewport->setRubberPoints(rubber());
        viewport->update();
    }
}

void PanelTool::rightClicked() {
    if (!m_project || !m_undo || !usesSnap()) return;
    switch (m_mode) {
        case Mode::PanelNew:
        case Mode::SurfNew: closePolyline(); return;
        case Mode::PanelDivide: m_points.clear(); break;
        case Mode::PanelMove:
        case Mode::SurfMove:
            m_points.clear();
            m_step = m_step == 3 ? 2 : 1;  // «Volver al paso anterior»
            configureViewports();
            announce();
            return;
        default: break;
    }
    configureViewports();
}

void PanelTool::applyMove(const Point3& delta) {
    for (int v : m_selVertices) {
        Point3& total = m_pending[v];
        total = {total.x + delta.x, total.y + delta.y, total.z + delta.z};
    }
    m_points.clear();
    m_selVertices.clear();
    m_step = 1;  // measured: back to the choice of vertices, the new edges stay green dashed until the RMB
    configureViewports();
    announce();
}

void PanelTool::commitMove() {
    // Vertices with the same displacement move together; usually there is a single group.
    std::vector<std::pair<Point3, std::vector<int>>> groups;
    for (const auto& [vertex, delta] : m_pending) {
        auto it = std::find_if(groups.begin(), groups.end(), [&](const auto& g) { return g.first == delta; });
        if (it == groups.end()) groups.push_back({delta, {vertex}});
        else it->second.push_back(vertex);
    }
    ContourError why = ContourError::None;
    bool failed = false;
    if (groups.size() > 1) m_undo->beginMacro(tr("Mover").toStdString());
    for (const auto& [delta, vertices] : groups) {
        if (m_mode == Mode::PanelMove) {
            if (movePanelVertices(*m_project, *m_undo, m_movingId, vertices, delta, &why) == PanelError::InvalidContour) failed = true;
        } else {
            std::vector<int> moved;
            if (moveSurfaceLoadVertices(*m_project, *m_undo, m_movingId, vertices, delta, m_alsoOther, &moved, &why) == PanelError::InvalidContour) failed = true;
        }
        if (failed) break;
    }
    if (groups.size() > 1) m_undo->endMacro();
    m_pending.clear();
    m_selVertices.clear();
    m_points.clear();
    if (failed) warn(QString::fromStdString(panelContourMessage(m_mode == Mode::SurfMove, true)), true);  // #65133 / #65134, measured «Error»
    m_step = 1;
    configureViewports();
    announce();
}

// ---- polylines and actions ---------------------------------------------------------------------------------------------------

void PanelTool::closePolyline() {
    if (!m_project || !m_undo || m_points.empty()) return;
    if (m_mode == Mode::PanelNew) {
        if (validatePanelContour(m_points) != ContourError::None) {
            warn(QString::fromStdString(panelContourMessage(false, false)), true);  // #65130
            m_points.clear();
            configureViewports();
            return;
        }
        m_contour = m_points;
        m_points.clear();
        info(tr("Seleccione la dirección de reparto para las cargas superficiales asignadas al paño."));  // #65132, measured «Información»
        m_step = 1;
        m_side.reset();
        configureViewports();
        announce();
    } else if (m_mode == Mode::SurfNew) {
        createSurfaceFromPolyline();
    }
    for (const QPointer<Viewport3D>& v : m_viewports) v->update();
}

void PanelTool::createPanelFromPolyline() {
    if (!m_project || !m_undo || !m_side) return;
    const std::vector<Point3> contour = m_contour;
    int panelId = 0;
    ContourError why = ContourError::None;
    m_undo->beginMacro(tr("Nuevo paño").toStdString());
    const PanelError error = addPanel(*m_project, *m_undo, contour, *m_side, m_direction, {}, &panelId, &why);
    if (error != PanelError::None) {
        m_undo->endMacro();
        if (error == PanelError::InvalidContour) warn(QString::fromStdString(panelContourMessage(false, false)), true);
        else warn(QString::fromStdString(panelErrorMessage(error)));
        resetSteps();
        return;
    }
    // Measured: the panel exists as soon as the side is chosen; «Cargas en paño» opens at once and Cancelar keeps the panel without loads.
    PanelLoadsDialog dialog(m_project->loadCases, contour, {}, viewCase(), m_dialogParent);
    if (run(dialog) == QDialog::Accepted) {
        SurfaceLoadIssue issue;
        const PanelError loads = setPanelLoads(*m_project, *m_undo, {panelId}, dialog.rows(), &issue);
        if (loads == PanelError::InvalidData) warn(QString::fromStdString(issue.message), issue.kind == SurfaceLoadIssueKind::Error);
        else if (loads != PanelError::None && loads != PanelError::NothingToChange) warn(QString::fromStdString(panelErrorMessage(loads)));
    }
    m_undo->endMacro();
    resetSteps();
}

void PanelTool::createSurfaceFromPolyline() {
    if (!m_project || !m_undo) return;
    const std::vector<Point3> contour = m_points;
    if (validatePanelContour(contour) != ContourError::None) {
        warn(QString::fromStdString(panelContourMessage(true, false)), true);  // #65131
        m_points.clear();
        configureViewports();
        return;
    }
    SurfaceLoadDialog dialog(tr("Introducir cargas superficiales"), m_project->loadCases, contour, surfaceLoadDefaults(viewCase()), m_dialogParent);
    const bool accepted = run(dialog) == QDialog::Accepted;
    m_points.clear();
    configureViewports();
    if (!accepted) return;
    bool createPanel = false;
    if (!surfaceLoadOverlapsPanel(m_project->panels, contour))
        createPanel = ask(tr("¿Desea crear un paño asociado a la nueva carga superficial?"));  // #63986: Sí (default) / No
    SurfaceLoadIssue issue;
    ContourError why = ContourError::None;
    const PanelError error = addSurfaceLoad(*m_project, *m_undo, contour, dialog.data(), createPanel, nullptr, nullptr, &issue, &why);
    if (error == PanelError::InvalidData) warn(QString::fromStdString(issue.message), issue.kind == SurfaceLoadIssueKind::Error);
    else if (error == PanelError::InvalidContour) warn(QString::fromStdString(panelContourMessage(true, false)), true);
    else if (error != PanelError::None) warn(QString::fromStdString(panelErrorMessage(error)));
}

bool PanelTool::editPanels(const std::vector<int>& panelIds) {
    if (!m_project || !m_undo || panelIds.empty()) return false;
    const Panel* first = nullptr;
    for (int id : panelIds)
        if ((first = findPanel(m_project->panels, id))) break;
    if (!first) return false;
    PanelLoadsDialog dialog(m_project->loadCases, first->vertices, first->loads, viewCase(), m_dialogParent);
    if (run(dialog) != QDialog::Accepted) return false;
    SurfaceLoadIssue issue;
    const PanelError error = setPanelLoads(*m_project, *m_undo, panelIds, dialog.rows(), &issue);  // every selected panel takes the list (measured)
    if (error == PanelError::NothingToChange) return false;
    if (error == PanelError::InvalidData) warn(QString::fromStdString(issue.message), issue.kind == SurfaceLoadIssueKind::Error);
    else if (error != PanelError::None) warn(QString::fromStdString(panelErrorMessage(error)));
    return error == PanelError::None;
}

bool PanelTool::deletePanels(const std::vector<int>& panelIds) {
    if (!m_project || !m_undo || panelIds.empty()) return false;
    return cc::core::deletePanels(*m_project, *m_undo, panelIds) == PanelError::None;
}

bool PanelTool::editSurfaceLoads(const std::vector<int>& loadIds) {
    if (!m_project || !m_undo || loadIds.empty()) return false;
    const SurfaceLoad* first = findSurfaceLoad(m_project->surfaceLoads, loadIds.front());
    if (!first) return false;
    SurfaceLoadDialog dialog(tr("Cargas superficiales"), m_project->loadCases, first->contour, first->data, m_dialogParent);  // measured title of Editar
    if (run(dialog) != QDialog::Accepted) return false;
    SurfaceLoadIssue issue;
    const PanelError error = cc::core::editSurfaceLoads(*m_project, *m_undo, loadIds, dialog.data(), &issue);
    if (error == PanelError::NothingToChange) return false;
    if (error == PanelError::InvalidData) warn(QString::fromStdString(issue.message), issue.kind == SurfaceLoadIssueKind::Error);
    else if (error != PanelError::None) warn(QString::fromStdString(panelErrorMessage(error)));
    return error == PanelError::None;
}

bool PanelTool::changeSurfaceCase(const std::vector<int>& loadIds) {
    if (!m_project || !m_undo || loadIds.empty()) return false;
    const SurfaceLoad* first = findSurfaceLoad(m_project->surfaceLoads, loadIds.front());
    if (!first) return false;
    LoadCaseChoiceDialog dialog(m_project->loadCases, first->data.loadCaseId, m_dialogParent);
    if (run(dialog) != QDialog::Accepted) return false;
    const PanelError error = changeSurfaceLoadCase(*m_project, *m_undo, loadIds, dialog.loadCaseId());
    if (error == PanelError::NothingToChange) return false;
    if (error != PanelError::None) warn(QString::fromStdString(panelErrorMessage(error)));
    return error == PanelError::None;
}

bool PanelTool::deleteSurfaceLoads(const std::vector<int>& loadIds) {
    if (!m_project || !m_undo || loadIds.empty()) return false;
    return cc::core::deleteSurfaceLoads(*m_project, *m_undo, loadIds) == PanelError::None;  // immediate, no question (measured)
}

bool PanelTool::assignBars(const std::vector<int>& barIds) {
    if (!m_project || !m_undo || barIds.empty()) return false;
    return toggleSurfaceBarAssignment(*m_project, *m_undo, barIds) == PanelError::None;  // each RMB toggles, without a box (measured)
}

// ---- drawing -----------------------------------------------------------------------------------------------------------------

void PanelTool::paint(QPainter& painter, const Viewport3D& viewport) const {
    if (!m_project) return;
    const auto project = [&viewport](const Point3& p, QPointF& out) { return viewport.worldToScreen(toVec(p), out); };
    const LoadScales scales = viewport.nodalLoads() ? viewport.nodalLoads()->scales : LoadScales{};
    const LoadCaseSet& cases = m_project->loadCases;
    const bool panelMode = isPanelMode(m_mode), surfaceMode = isSurfaceMode(m_mode);
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);
    QFont font = painter.font();
    font.setPixelSize(11);
    painter.setFont(font);
    const QFontMetricsF metrics(font);

    // One prism (base ring, top ring, ticks, labels). `selected` paints it orange, `grey` pale grey (the other group's loads).
    const auto drawPrism = [&](const PanelLoadDrawing& drawing, const QColor& colour, bool selected, bool grey) {
        if (!drawing.valid) return;
        std::vector<QPointF> base(drawing.base.size()), top(drawing.top.size());
        for (std::size_t k = 0; k < drawing.base.size(); ++k)
            if (!project(drawing.base[k], base[k]) || !project(drawing.top[k], top[k])) return;
        const QColor line = selected ? kOrange : (grey ? kGreyLoad : colour);
        QColor fill = line;
        fill.setAlpha(selected ? 70 : (grey ? 35 : 55));
        painter.setBrush(fill);
        painter.setPen(QPen(line, selected ? 2.5 : 1.0));
        const std::size_t n = base.size();
        for (std::size_t k = 0; k < n; ++k) {
            const std::size_t j = (k + 1) % n;
            painter.drawPolygon(QPolygonF({base[k], base[j], top[j], top[k]}));
        }
        painter.drawPolygon(QPolygonF(QList<QPointF>(base.begin(), base.end())));
        painter.drawPolygon(QPolygonF(QList<QPointF>(top.begin(), top.end())));
        painter.setPen(QPen(line, 1.0));
        for (const auto& [from, to] : drawing.ticks) {
            QPointF a, b;
            if (project(from, a) && project(to, b)) painter.drawLine(a, b);
        }
        if (grey) return;
        for (std::size_t k = 0; k < drawing.labels.size() && k < n; ++k) {
            QPointF at;
            if (!project(drawing.labels[k].anchor, at)) continue;
            double angle = std::atan2(top[k].y() - base[k].y(), top[k].x() - base[k].x()) * 180.0 / std::numbers::pi;
            if (std::hypot(top[k].x() - base[k].x(), top[k].y() - base[k].y()) < 1.0) angle = 0.0;
            if (angle > 90.0 + 1e-6) angle -= 180.0;
            if (angle < -90.0 + 1e-6) angle += 180.0;
            const QString text = QString::fromStdString(drawing.labels[k].text);
            painter.save();
            painter.setPen(line);
            painter.translate(at);
            painter.rotate(angle);
            painter.drawText(QPointF(-metrics.horizontalAdvance(text) / 2.0, -3.0), text);
            painter.restore();
        }
    };
    const auto caseColour = [&](int loadCaseId) { return QColor::fromRgb(loadCaseColourOf(cases, loadCaseId)); };

    if (viewport.loadsVisible()) {
        // Loads of the panels: in colour unless a surface-load command is active (measured: then they are pale grey).
        for (const Panel& panel : m_project->panels.items) {
            std::vector<PanelLoadView> views;
            std::vector<int> caseIds;
            for (const SurfaceLoadData& row : panel.loads) {
                if (!caseInView(row.loadCaseId)) continue;
                views.push_back({row, panelPositiveDirection(panel.vertices, isWindCase(cases, row.loadCaseId))});
                caseIds.push_back(row.loadCaseId);
            }
            if (views.empty() || !viewport.visibleAfterClipping(toVec(panel.vertices.front()))) continue;
            const std::vector<double> offsets = panelLoadStackOffsets(views, scales);
            for (std::size_t i = 0; i < views.size(); ++i)
                drawPrism(panelLoadDrawing(views[i], panel.vertices, scales, i < offsets.size() ? offsets[i] : 0.0), caseColour(caseIds[i]), false, surfaceMode);
        }
        // Surface loads: loads with the same contour stack; grey while a Paños command is active.
        std::map<std::size_t, std::vector<const SurfaceLoad*>> groups;  // by index of the first load with an equal contour
        std::vector<const SurfaceLoad*> firsts;
        for (const SurfaceLoad& load : m_project->surfaceLoads.items) {
            if (!surfaceLoadInView(load)) continue;
            std::size_t slot = firsts.size();
            for (std::size_t i = 0; i < firsts.size(); ++i)
                if (firsts[i]->contour == load.contour) slot = i;
            if (slot == firsts.size()) firsts.push_back(&load);
            groups[slot].push_back(&load);
        }
        for (const auto& [slot, loads] : groups) {
            const std::vector<Point3>& contour = loads.front()->contour;
            if (contour.empty() || !viewport.visibleAfterClipping(toVec(contour.front()))) continue;
            std::vector<PanelLoadView> views;
            for (const SurfaceLoad* load : loads) views.push_back({load->data, panelPositiveDirection(contour, isWindCase(cases, load->data.loadCaseId))});
            const std::vector<double> offsets = panelLoadStackOffsets(views, scales);
            for (std::size_t i = 0; i < loads.size(); ++i) {
                const bool selected = m_selLoads.count(loads[i]->id) != 0;
                const bool copy = loads[i]->id == m_copySource;
                drawPrism(panelLoadDrawing(views[i], contour, scales, i < offsets.size() ? offsets[i] : 0.0), copy ? kCopyCyan : caseColour(loads[i]->data.loadCaseId), selected,
                          panelMode && !selected);
            }
        }
    }

    // At rest (no command) the loads of the panels already distributed over the bars: vertical strips of the colour of the load case along every bar with
    // a label in kN/m, those acting upwards below the bar (measured, research 14.1).
    if (m_mode == Mode::None && viewport.loadsVisible() && (!m_project->panels.items.empty() || !m_project->surfaceLoads.items.empty())) {
        const PanelDistribution distribution = distributePanels(*m_project);
        // Loads of the same bar and the same side stack in the order of the hypotheses (measured: Peso propio nearest the bar, then CM, T, V ...).
        const auto caseIndex = [&cases](int id) {
            for (std::size_t i = 0; i < cases.cases.size(); ++i)
                if (cases.cases[i].id == id) return static_cast<int>(i);
            return -1;
        };
        std::vector<const PanelBarLoad*> ordered;
        for (const PanelBarLoad& load : distribution.loads)
            if (caseInView(load.loadCaseId)) ordered.push_back(&load);
        std::stable_sort(ordered.begin(), ordered.end(), [&](const PanelBarLoad* a, const PanelBarLoad* b) {
            return a->barId != b->barId ? a->barId < b->barId : caseIndex(a->loadCaseId) < caseIndex(b->loadCaseId);
        });
        std::map<std::tuple<int, long, long, long>, double> stacked;  // (bar, side) -> height already used
        for (const PanelBarLoad* ptr : ordered) {
            const PanelBarLoad& load = *ptr;
            const auto frame = barLocalAxes(m_project->structure, load.barId);
            if (!frame || frame->length <= 1e-9 || !viewport.visibleAfterClipping(toVec(frame->origin))) continue;
            BarLoadData data = barLoadDefaults(load.loadCaseId);
            data.axes = BarLoadAxes::Global;
            data.direction = LoadDirection::UserVector;
            data.userVector = load.direction;
            data.relative = false;
            data.value1 = load.q;
            data.value2 = 0.0;
            const bool whole = load.s1 <= 0.001 && load.s2 + 0.001 >= frame->length;
            data.type = whole ? BarLoadType::Uniform : BarLoadType::Strip;
            data.start = load.s1;
            data.end = load.s2;
            const BarLoadDrawing drawing = barLoadDrawing(data, *frame, scales);
            if (!drawing.valid) continue;
            const Point3 dir = barLoadDirection(data, *frame);
            const auto key = std::make_tuple(load.barId, std::lround(dir.x * 1000.0), std::lround(dir.y * 1000.0), std::lround(dir.z * 1000.0));
            double& used = stacked[key];
            const Point3 shift{-dir.x * used, -dir.y * used, -dir.z * used};
            used += drawing.sizeM;
            const auto moved = [&shift](const Point3& p) { return Point3{p.x + shift.x, p.y + shift.y, p.z + shift.z}; };
            const QColor line = caseColour(load.loadCaseId);
            QColor fill = line;
            fill.setAlpha(60);
            QPolygonF outline;
            bool visible = true;
            for (const Point3& point : drawing.outline) {
                QPointF q;
                if (!project(moved(point), q)) { visible = false; break; }
                outline << q;
            }
            if (!visible || outline.size() < 3) continue;
            painter.setBrush(fill);
            painter.setPen(QPen(line, 1.0));
            painter.drawPolygon(outline);
            for (const auto& [from, to] : drawing.ticks) {
                QPointF a, b;
                if (project(moved(from), a) && project(moved(to), b)) painter.drawLine(a, b);
            }
            const double angle0 = std::atan2(outline[1].y() - outline[0].y(), outline[1].x() - outline[0].x()) * 180.0 / std::numbers::pi;
            for (const BarLoadLabel& label : drawing.labels) {
                QPointF at;
                if (!project(moved(label.anchor), at)) continue;
                double angle = angle0;
                if (angle > 90.0 + 1e-6) angle -= 180.0;
                if (angle < -90.0 + 1e-6) angle += 180.0;
                const QString text = QString::fromStdString(label.text);
                painter.save();
                painter.setPen(line);
                painter.translate(at);
                painter.rotate(angle);
                painter.drawText(QPointF(-metrics.horizontalAdvance(text) / 2.0, -3.0), text);
                painter.restore();
            }
        }
    }

    // Panels: contour and arrow, only while a command of Paños is active (measured).
    if (panelMode) {
        for (const Panel& panel : m_project->panels.items) {
            const QPolygonF poly = picking::projectPolygon(project, panel.vertices);
            if (poly.size() < 3) continue;
            const bool selected = m_selPanels.count(panel.id) != 0;
            painter.setBrush(selected ? QColor(218, 135, 53, 50) : QBrush(Qt::NoBrush));
            painter.setPen(QPen(selected ? kOrange : Qt::black, selected ? 2.5 : 1.5));
            painter.drawPolygon(poly);
            const PanelArrow arrow = panelArrow(panel);
            QPointF a, b;
            if (arrow.valid && project(arrow.from, a) && project(arrow.to, b)) {
                // Measured: a large double black arrow in the centre, parallel to the direction of distribution (orange with the panel when selected).
                painter.setPen(QPen(selected ? kOrange : Qt::black, 3.0));
                painter.drawLine(a, b);
                const QPointF d = b - a;
                const double len = std::hypot(d.x(), d.y());
                if (len > 1.0) {
                    const QPointF u(d.x() / len, d.y() / len), n(-u.y(), u.x());
                    for (const auto& [tip, dir] : {std::pair{b, -1.0}, std::pair{a, 1.0}}) {
                        const QPointF back = tip + u * (dir * 11.0);
                        painter.setBrush(selected ? kOrange : Qt::black);
                        painter.drawPolygon(QPolygonF({tip, back + n * 5.0, back - n * 5.0}));
                    }
                }
            }
            if (selected && m_mode == Mode::PanelEdit) {  // measured: the positive sense (blue arrow with «+» and a circle) in the middle of the selected panel
                const Point3 centre = polygonCentroid(panel.vertices);
                const Point3 dir = panelPositiveDirection(panel.vertices, false);
                QPointF c, t;
                if (project(centre, c) && project({centre.x - dir.x * 0.6, centre.y - dir.y * 0.6, centre.z - dir.z * 0.6}, t)) {
                    painter.setPen(QPen(QColor(0, 70, 220), 2.0));
                    painter.drawLine(c, t);
                    painter.setBrush(Qt::white);
                    painter.drawEllipse(c, 6.0, 6.0);
                    painter.drawText(QRectF(c.x() - 6, c.y() - 7, 12, 14), Qt::AlignCenter, QStringLiteral("+"));
                }
            }
        }
    }

    // Prolonged sides of the polygon whose direction is being chosen (green dashed; blue under the cursor; orange when chosen).
    if (panelStepSides()) {
        if (const std::vector<Point3>* contour = sideContour()) {
            const QPolygonF poly = picking::projectPolygon(project, *contour);
            const double reach = std::hypot(viewport.width(), viewport.height());
            const std::optional<int> hover = poly.size() >= 3 ? picking::sideLineAt(poly, viewport.cursorPosition()) : std::nullopt;
            for (int i = 0; i < poly.size(); ++i) {
                const auto [p, q] = picking::prolongedLine(poly[i], poly[(i + 1) % poly.size()], reach);
                const bool chosen = m_side && *m_side == i;
                const bool hovered = hover && *hover == i;
                painter.setPen(QPen(chosen ? kOrange : (hovered ? kSideBlue : kSideGreen), (chosen || hovered) ? 3.0 : 1.2, Qt::DashLine));
                painter.drawLine(p, q);
            }
            painter.setBrush(Qt::NoBrush);
            painter.setPen(QPen(Qt::black, 1.5));
            painter.drawPolygon(poly);
        }
    }

    // Polygon in progress (grey fill), the vertices that can be chosen and the moved polygon (green dashed).
    if (!m_points.empty() && usesSnap() && (m_mode == Mode::PanelNew || m_mode == Mode::SurfNew) && m_points.size() >= 3) {
        const QPolygonF poly = picking::projectPolygon(project, m_points);
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(120, 120, 120, 70));
        painter.drawPolygon(poly);
    }
    if (const std::vector<Point3>* polygon = movingPolygon()) {
        if (m_step >= 1) {
            const QPolygonF poly = picking::projectPolygon(project, *polygon);
            painter.setBrush(QColor(120, 120, 120, 50));
            painter.setPen(QPen(Qt::black, 1.2));
            painter.drawPolygon(poly);
            for (int i = 0; i < poly.size(); ++i) {
                const bool chosen = m_selVertices.count(i) != 0;
                painter.setBrush(chosen ? kOrange : QColor(0, 90, 220));
                painter.setPen(Qt::NoPen);
                painter.drawRect(QRectF(poly[i].x() - 4, poly[i].y() - 4, 8, 8));
            }
            if (!m_pending.empty()) {  // displacement chosen, not yet applied: the new edges in green dashed with the new vertices (measured)
                const std::size_t count = polygon->size();
                const auto moved = [&](std::size_t i) {
                    Point3 p = (*polygon)[i];
                    if (const auto it = m_pending.find(static_cast<int>(i)); it != m_pending.end()) p = {p.x + it->second.x, p.y + it->second.y, p.z + it->second.z};
                    return p;
                };
                painter.setBrush(Qt::NoBrush);
                painter.setPen(QPen(kSideGreen, 1.5, Qt::DashLine));
                for (std::size_t i = 0; i < count; ++i) {
                    const std::size_t j = (i + 1) % count;
                    if (!m_pending.count(static_cast<int>(i)) && !m_pending.count(static_cast<int>(j))) continue;
                    QPointF a, b;
                    if (project(moved(i), a) && project(moved(j), b)) painter.drawLine(a, b);
                }
                painter.setPen(Qt::NoPen);
                painter.setBrush(QColor(0, 90, 220));
                for (const auto& [vertex, delta] : m_pending) {
                    QPointF a;
                    if (vertex >= 0 && static_cast<std::size_t>(vertex) < count && project(moved(static_cast<std::size_t>(vertex)), a))
                        painter.drawRect(QRectF(a.x() - 4, a.y() - 4, 8, 8));
                }
            }
            if (m_step == 3 && m_points.size() == 1 && viewport.snapMode()) {  // preview of the destination
                const Vec3 cursor = viewport.cursorWorld();
                const Point3 delta{cursor.x - m_points[0].x, cursor.y - m_points[0].y, cursor.z - m_points[0].z};
                std::vector<Point3> moved = *polygon;
                for (int v : m_selVertices)
                    if (v >= 0 && v < static_cast<int>(moved.size())) {
                        moved[static_cast<std::size_t>(v)].x += delta.x;
                        moved[static_cast<std::size_t>(v)].y += delta.y;
                        moved[static_cast<std::size_t>(v)].z += delta.z;
                    }
                const QPolygonF preview = picking::projectPolygon(project, moved);
                painter.setBrush(Qt::NoBrush);
                painter.setPen(QPen(kSideGreen, 1.5, Qt::DashLine));
                painter.drawPolygon(preview);
            }
        }
    }
    painter.restore();
}

}  // namespace cc::app
