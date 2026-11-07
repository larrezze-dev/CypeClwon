#include "ShellPropertiesTool.h"

#include <QAction>
#include <QActionGroup>
#include <QDialog>
#include <algorithm>

#include "ShellBandsDialog.h"
#include "ShellPropertiesDialogs.h"
#include "SupportDialogs.h"
#include "UserMessages.h"
#include "Viewport3D.h"
#include "core/shell_properties_edit.h"

namespace cc::app {

using namespace cc::core;

ShellPropertiesTool::ShellPropertiesTool(QObject* parent) : QObject(parent) {
    m_group = new QActionGroup(this);
    m_group->setExclusionPolicy(QActionGroup::ExclusionPolicy::ExclusiveOptional);
    const auto make = [this](Mode mode, const QString& tip) {
        auto* action = new QAction(commandName(mode), this);
        action->setCheckable(true);
        action->setEnabled(false);
        action->setStatusTip(tip);
        action->setToolTip(tip);
        m_group->addAction(action);
        connect(action, &QAction::toggled, this, [this, mode](bool on) {
            if (on) setMode(mode);
            else if (m_mode == mode) setMode(Mode::None);
        });
        m_actions[idx(mode)] = action;
    };
    // Help texts of the original: #64714, #374986, #64728, #64719, #64722. Pandeo / Análisis modal: the id of the help text was not found (clone wording).
    make(Mode::Discretisation, tr("Edición de los parámetros para configurar la discretización de un grupo de láminas."));
    make(Mode::Stiffness, tr("Permite editar los factores de rigidez de un grupo de láminas."));
    make(Mode::Bands, tr("Permite editar las bandas de integración de esfuerzos de varias láminas."));
    make(Mode::InteriorEdge, tr("Permite editar la vinculación interior de un grupo de aristas."));
    make(Mode::ExteriorEdge, tr("Permite editar la vinculación exterior de un grupo de aristas."));
    make(Mode::Buckling, tr("Permite indicar si un grupo de láminas se considera en el análisis de pandeo."));
    make(Mode::Modal, tr("Permite indicar si un grupo de láminas se considera en el análisis modal."));
}

QString ShellPropertiesTool::commandName(Mode mode) {
    switch (mode) {
        case Mode::Discretisation: return tr("Discretización");
        case Mode::Stiffness: return tr("Factores de rigidez");
        case Mode::Bands: return tr("Bandas de integración");
        case Mode::InteriorEdge: return tr("Vinculación interior");
        case Mode::ExteriorEdge: return tr("Vinculación exterior");
        case Mode::Buckling: return tr("Pandeo");
        case Mode::Modal: return tr("Análisis modal");
        case Mode::None: break;
    }
    return {};
}

QString ShellPropertiesTool::statusText(Mode mode) {
    if (mode == Mode::None) return tr("Seleccione una opción del menú.");
    const QString hints = tr(" - LMB: Seleccione un grupo de elementos. - RMB: Actuar sobre los elementos seleccionados.");
    return tr("Láminas - %1 |").arg(commandName(mode)) + QLatin1Char(' ') + hints;
}

void ShellPropertiesTool::pruneViewports() {
    m_viewports.removeIf([](const QPointer<Viewport3D>& v) { return v.isNull(); });
}

void ShellPropertiesTool::setProject(Project* project, UndoStack* undo) {
    deactivate();
    m_project = project;
    m_undo = undo;
    updateEnabled();
}

void ShellPropertiesTool::addViewport(Viewport3D* viewport) {
    if (!viewport || m_viewports.contains(viewport)) return;
    m_viewports.push_back(viewport);
    connect(viewport, &Viewport3D::actOnSelection, this, [this, viewport] { onActOnSelection(viewport); });
    connect(viewport, &Viewport3D::escapePressed, this, [this] { deactivate(); });
    if (m_mode != Mode::None) configureViewports();
}

void ShellPropertiesTool::deactivate() {
    if (QAction* checked = m_group->checkedAction()) checked->setChecked(false);
    setMode(Mode::None);
}

void ShellPropertiesTool::activate(Mode mode) {
    if (mode == Mode::None) return;
    if (QAction* action = m_actions[idx(mode)]; action->isEnabled()) action->setChecked(true);
}

void ShellPropertiesTool::updateEnabled() {
    const bool shells = m_project && !m_project->structure.shells.empty();  // enabled as soon as the work has a shell
    for (QAction* action : m_actions) {
        action->setEnabled(shells);
        if (!shells && action->isChecked()) action->setChecked(false);
    }
}

void ShellPropertiesTool::structureChanged() {
    updateEnabled();
    pruneViewports();
    for (const QPointer<Viewport3D>& viewport : m_viewports) viewport->refreshStructure();
}

void ShellPropertiesTool::setMode(Mode mode) {
    if (mode == m_mode) return;
    if (mode != Mode::None) emit aboutToActivate();
    m_mode = mode;
    configureViewports();
    emit modeChanged(mode);
    emit statusChanged(statusText(mode));  // after the viewports: setSelectionMode announces its own, generic hint
}

void ShellPropertiesTool::configureViewports() {
    pruneViewports();
    const bool on = m_mode != Mode::None;
    const bool edges = m_mode == Mode::InteriorEdge || m_mode == Mode::ExteriorEdge;
    for (const QPointer<Viewport3D>& viewport : m_viewports) {
        viewport->setSelectionMode(false);
        viewport->setEdgeSelectionMode(false);
        viewport->setShellContourPick(false);
        viewport->setShellInfoProvider({});
        if (!on) continue;
        viewport->setSelectionFilter(SelectionFilter::only(ElementKind::Shell));
        viewport->setShellContourPick(!edges);  // the original selects a shell by its outline; the centre does not select it
        viewport->setShellInfoProvider([this](int shellId, const std::optional<ShellEdgeKey>& edge) { return infoLines(shellId, edge); });
        viewport->setSelectionMode(true);
        viewport->setEdgeSelectionMode(edges);
        viewport->setFocus();
    }
}

int ShellPropertiesTool::run(QDialog& dialog) { return m_runner ? m_runner(dialog) : dialog.exec(); }

QString ShellPropertiesTool::edgeTitle(const ShellEdgeKey& edge) const {
    int a = edge.nodeA, b = edge.nodeB;
    if (m_project)
        if (const Shell* shell = m_project->structure.findShell(edge.shellId)) {
            const std::size_t n = shell->nodeIds.size();
            for (std::size_t i = 0; i < n; ++i) {
                const int p = shell->nodeIds[i], q = shell->nodeIds[(i + 1) % n];
                if (makeEdgeKey(edge.shellId, p, q) == edge) {
                    a = p;
                    b = q;
                    break;
                }
            }
        }
    return tr("Arista N%1 / N%2").arg(a).arg(b);
}

QStringList ShellPropertiesTool::infoLines(int shellId, const std::optional<ShellEdgeKey>& edge) const {
    if (!m_project) return {};
    if (m_mode == Mode::ExteriorEdge && edge) return {edgeTitle(*edge)};  // measured: the box is only the title (p7_ve_b)
    const QStringList base = Viewport3D::shellInfoLines(m_project->structure, shellId, m_project->profiles);  // L1, Capa, Espesor, Material, ...
    if (base.size() < 4) return {};
    const ShellRecord record = shellRecordOf(m_project->shellProps, shellId);
    QStringList lines{base[0], base[1]};
    if (m_mode == Mode::InteriorEdge && edge) lines << QString::fromStdString(edgeInteriorLine(edgeInteriorOf(m_project->shellProps, *edge)));
    lines << base[2] << base[3];
    const auto compact = [&](ShellInfoPart part) {
        for (const std::string& line : shellInfoLines(record, part)) lines << QLatin1Char('\x01') + QString::fromStdString(line);
    };
    compact(ShellInfoPart::Stiffness);
    if (m_mode == Mode::Discretisation) compact(ShellInfoPart::Discretisation);
    for (ShellInfoPart part : {ShellInfoPart::Buckling, ShellInfoPart::Modal}) {
        const std::vector<std::string> rows = shellInfoLines(record, part);
        lines << QLatin1Char('\x02') + QString::fromStdString(rows[0]) << QString::fromStdString(rows[1]);
    }
    return lines;
}

void ShellPropertiesTool::onActOnSelection(Viewport3D* viewport) {
    if (m_mode == Mode::None || !m_project || !m_undo) return;
    if (m_mode == Mode::InteriorEdge || m_mode == Mode::ExteriorEdge) {
        const std::vector<ShellEdgeKey> edges = viewport->edgeSelection();
        if (edges.empty()) return;
        if (m_mode == Mode::InteriorEdge) editInterior(edges);
        else editExterior(edges);
    } else {
        const std::vector<int> shells = viewport->selection().ids(ElementKind::Shell);
        if (shells.empty()) return;
        switch (m_mode) {
            case Mode::Discretisation: editDiscretisation(shells); break;
            case Mode::Stiffness: editStiffness(shells); break;
            case Mode::Bands: editBands(shells); break;
            case Mode::Buckling: editBuckling(shells); break;
            case Mode::Modal: editModal(shells); break;
            default: break;
        }
    }
    viewport->clearSelection();
    emit statusChanged(statusText(m_mode));
}

namespace {
void warn(QWidget* parent, const std::optional<ShellPropIssue>& issue) {
    if (issue && !issue->text.empty()) messages::show(parent, messages::Kind::Warning, QString::fromStdString(issue->text));
}
}  // namespace

bool ShellPropertiesTool::editDiscretisation(const std::vector<int>& shellIds) {
    if (!m_project || !m_undo) return false;
    const std::vector<int> ids = existingShells(*m_project, shellIds);
    if (ids.empty()) return false;
    ShellDiscretisationDialog dialog(shellRecordOf(m_project->shellProps, ids.front()).maxTriangleSideM, QStringLiteral("m"), m_dialogParent);
    if (run(dialog) != QDialog::Accepted) return false;
    const std::size_t before = m_undo->count();
    warn(m_dialogParent, assignShellDiscretisation(*m_project, *m_undo, ids, dialog.maxSide()));
    return changedSince(before);
}

bool ShellPropertiesTool::editStiffness(const std::vector<int>& shellIds) {
    if (!m_project || !m_undo) return false;
    const std::vector<int> ids = existingShells(*m_project, shellIds);
    if (ids.empty()) return false;
    ShellStiffnessDialog dialog(shellRecordOf(m_project->shellProps, ids.front()).stiffness, m_dialogParent);
    if (run(dialog) != QDialog::Accepted) return false;
    const std::size_t before = m_undo->count();
    warn(m_dialogParent, assignShellStiffness(*m_project, *m_undo, ids, dialog.factors()));
    return changedSince(before);
}

bool ShellPropertiesTool::editBuckling(const std::vector<int>& shellIds) {
    if (!m_project || !m_undo) return false;
    const std::vector<int> ids = existingShells(*m_project, shellIds);
    if (ids.empty()) return false;
    ShellConsiderDialog dialog(ShellConsiderDialog::Kind::Buckling, shellRecordOf(m_project->shellProps, ids.front()).bucklingConsidered, m_dialogParent);
    if (run(dialog) != QDialog::Accepted) return false;
    const std::size_t before = m_undo->count();
    warn(m_dialogParent, assignShellBuckling(*m_project, *m_undo, ids, dialog.considered()));
    return changedSince(before);
}

bool ShellPropertiesTool::editModal(const std::vector<int>& shellIds) {
    if (!m_project || !m_undo) return false;
    const std::vector<int> ids = existingShells(*m_project, shellIds);
    if (ids.empty()) return false;
    ShellConsiderDialog dialog(ShellConsiderDialog::Kind::Modal, shellRecordOf(m_project->shellProps, ids.front()).modalConsidered, m_dialogParent);
    if (run(dialog) != QDialog::Accepted) return false;
    const std::size_t before = m_undo->count();
    warn(m_dialogParent, assignShellModal(*m_project, *m_undo, ids, dialog.considered()));
    return changedSince(before);
}

bool ShellPropertiesTool::editBands(const std::vector<int>& shellIds) {
    if (!m_project || !m_undo) return false;
    const std::vector<int> ids = existingShells(*m_project, shellIds);
    if (ids.empty()) return false;
    // Only the geometry of the first shell is edited; the bands are copied to the rest when the dialog is accepted.
    const Shell* first = m_project->structure.findShell(ids.front());
    const std::vector<Point2> contour = shellContour2D(m_project->structure, *first);
    if (contour.empty()) return false;
    const BandForceOptions force = m_project->shellProps.bandForce;
    ShellBandsDialog dialog(contour, shellRecordOf(m_project->shellProps, ids.front()).bands, force, m_dialogParent);
    connect(&dialog, &ShellBandsDialog::statusChanged, this, &ShellPropertiesTool::statusChanged);
    const int result = run(dialog);
    emit statusChanged(statusText(m_mode));
    if (result != QDialog::Accepted) return false;
    const std::size_t before = m_undo->count();
    m_undo->beginMacro("Bandas de integración de láminas");  // one undoable step for the bands and the method of integration
    const auto issue = assignShellBands(*m_project, *m_undo, ids, dialog.bands());
    if (!issue && dialog.force() != force) assignBandForceOptions(*m_project, *m_undo, dialog.force());
    m_undo->endMacro();
    warn(m_dialogParent, issue);
    return changedSince(before);
}

bool ShellPropertiesTool::editInterior(const std::vector<ShellEdgeKey>& edges) {
    if (!m_project || !m_undo) return false;
    const std::vector<ShellEdgeKey> keys = existingShellEdges(*m_project, edges);
    if (keys.empty()) return false;
    EdgeInteriorDialog dialog(edgeInteriorOf(m_project->shellProps, keys.front()), m_dialogParent);
    if (run(dialog) != QDialog::Accepted) return false;
    const std::size_t before = m_undo->count();
    warn(m_dialogParent, assignEdgeInterior(*m_project, *m_undo, keys, dialog.bond()));
    return changedSince(before);
}

bool ShellPropertiesTool::editExterior(const std::vector<ShellEdgeKey>& edges) {
    if (!m_project || !m_undo) return false;
    const std::vector<ShellEdgeKey> keys = existingShellEdges(*m_project, edges);
    if (keys.empty()) return false;
    ExternalSupportDialog dialog(edgeExteriorOf(m_project->shellProps, keys.front()), m_dialogParent);  // the same dialog as the nodes (measured)
    if (run(dialog) != QDialog::Accepted) return false;
    const std::size_t before = m_undo->count();
    warn(m_dialogParent, assignEdgeExterior(*m_project, *m_undo, keys, dialog.support()));
    return changedSince(before);
}

}  // namespace cc::app
