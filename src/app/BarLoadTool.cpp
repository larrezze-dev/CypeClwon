#include "BarLoadTool.h"

#include <QAction>
#include <QActionGroup>
#include <QDialog>
#include <algorithm>

#include "NodalLoadTool.h"
#include "UserMessages.h"
#include "Viewport3D.h"

namespace cc::app {

using namespace cc::core;

BarLoadTool::BarLoadTool(QObject* parent) : QObject(parent) {
    m_group = new QActionGroup(this);
    m_group->setExclusionPolicy(QActionGroup::ExclusionPolicy::ExclusiveOptional);
    const auto make = [this](const QString& text, Mode mode) {
        auto* action = new QAction(text, this);
        action->setCheckable(true);
        action->setEnabled(false);
        const QString tip = tipText(mode);
        action->setStatusTip(tip);
        action->setToolTip(tip);
        m_group->addAction(action);
        connect(action, &QAction::toggled, this, [this, mode](bool on) {
            if (on) setMode(mode);
            else if (m_mode == mode) setMode(Mode::None);
        });
        return action;
    };
    m_new = make(tr("Nueva"), Mode::New);
    m_edit = make(tr("Editar"), Mode::Edit);
    m_delete = make(tr("Borrar"), Mode::Delete);
    m_case = make(tr("Hipótesis"), Mode::Case);
    m_selfWeight = make(tr("Peso propio"), Mode::SelfWeight);
}

QString BarLoadTool::statusText(Mode mode) {
    const QString hints = tr(" - LMB: Seleccione un grupo de elementos. - RMB: Actuar sobre los elementos seleccionados.");
    switch (mode) {
        case Mode::New: return tr("Barras - Nueva |") + QLatin1Char(' ') + hints;
        case Mode::Edit: return tr("Barras - Editar |") + QLatin1Char(' ') + hints;
        case Mode::Case: return tr("Barras - Hipótesis |") + QLatin1Char(' ') + hints;
        case Mode::Delete: return tr("Barras - Borrar |") + QLatin1Char(' ') + hints;
        case Mode::SelfWeight: return tr("Barras - Peso propio |") + QLatin1Char(' ') + hints;
        case Mode::None: break;
    }
    return tr("Seleccione una opción del menú.");
}

QString BarLoadTool::tipText(Mode mode) {
    const auto rich = [](const QString& title, const QString& text) {
        QString html = QStringLiteral("<b>") + title.toHtmlEscaped() + QStringLiteral("</b>");
        if (!text.isEmpty()) html += QStringLiteral("<br>") + text.toHtmlEscaped().replace(QLatin1Char('\n'), QStringLiteral("<br>"));
        return html;
    };
    switch (mode) {
        case Mode::New:
            return rich(tr("Nueva"), tr("Permite introducir cargas sobre un grupo de barras.\nSeleccione una o varias barras y pulse el botón derecho del ratón. "
                                        "Aparecerá el cuadro de diálogo de 'Introducir cargas sobre barras', donde se introducen los valores que definen la carga."));
        case Mode::Edit:
            return rich(tr("Editar"), tr("Permite modificar los datos de las cargas ya introducidas.\nSeleccione un grupo de cargas y pulse con el botón derecho del ratón. "
                                         "Aparecerá el diálogo de 'Editar cargas sobre barras' para modificar los datos de las cargas."));
        case Mode::Delete:
            return rich(tr("Borrar"), tr("Permite eliminar una o varias cargas vistas en pantalla.\nPara ello, seleccione el grupo de cargas que desea borrar y pulse el botón derecho del ratón."));
        case Mode::Case: return rich(tr("Hipótesis"), QString());
        case Mode::SelfWeight:
            return rich(tr("Generación de las cargas de peso propio de las barras"),
                        tr("Permite activar / desactivar la generación automática de la carga de peso propio.\nPara ello, seleccione las barras para las que desea "
                           "activar o desactivar la generación de cargas de peso propio y pulse el botón derecho del ratón. Si pulsa el botón derecho del ratón sin haber "
                           "seleccionado ninguna barra, volverá a aparecer el diálogo en el que puede escoger la acción a realizar.")) +
                   QStringLiteral("<br><br>") + tr("Pulse 'F1' alternativamente para mostrar u ocultar los textos de ayuda.").toHtmlEscaped();
        case Mode::None: break;
    }
    return {};
}

void BarLoadTool::pruneViewports() {
    m_viewports.removeIf([](const QPointer<Viewport3D>& v) { return v.isNull(); });
}

int BarLoadTool::run(QDialog& dialog) { return m_runner ? m_runner(dialog) : dialog.exec(); }

void BarLoadTool::warn(const QString& text, bool error) {
    messages::show(m_dialogParent, error ? messages::Kind::Error : messages::Kind::Warning, text);
}

bool BarLoadTool::viewAll() const { return m_host ? m_host->viewAll() : false; }
int BarLoadTool::viewCase() const { return m_host ? m_host->viewCaseId() : 0; }

int BarLoadTool::selfWeightCaseId() const {
    if (!m_project) return 0;
    for (const LoadCase& c : m_project->loadCases.cases)
        if (c.nature == LoadNature::SelfWeight) return c.id;
    return m_project->loadCases.cases.empty() ? 0 : m_project->loadCases.cases.front().id;
}

void BarLoadTool::setHost(NodalLoadTool* host) {
    m_host = host;
    if (!host) return;
    connect(host, &NodalLoadTool::viewChanged, this, [this] { configureViewports(); });
    connect(host, &NodalLoadTool::viewPanelClosed, this, [this] {
        if (m_mode != Mode::None) deactivate();  // closing the window ends the command (like the others)
    });
}

void BarLoadTool::setProject(Project* project, UndoStack* undo) {
    deactivate();
    m_project = project;
    m_undo = undo;
    m_session = BarLoadSession{};
    m_selfWeightAsked = false;
    pruneViewports();
    for (const QPointer<Viewport3D>& viewport : m_viewports) viewport->setBarLoads(m_project ? &m_project->barLoads : nullptr);
    updateEnabled();
}

void BarLoadTool::addViewport(Viewport3D* viewport) {
    if (!viewport || m_viewports.contains(viewport)) return;
    m_viewports.push_back(viewport);
    viewport->setBarLoads(m_project ? &m_project->barLoads : nullptr);
    connect(viewport, &Viewport3D::actOnSelection, this, [this, viewport] { onActOnSelection(viewport); });
    connect(viewport, &Viewport3D::escapePressed, this, [this] { deactivate(); });
    if (m_mode != Mode::None) configureViewports();
}

void BarLoadTool::updateEnabled() {
    const bool open = m_project != nullptr;
    const bool bars = open && !m_project->structure.bars.empty();
    const bool loads = open && hasBarLoads(m_project->barLoads);
    m_new->setEnabled(bars);
    m_edit->setEnabled(loads);
    m_case->setEnabled(loads);
    m_delete->setEnabled(loads);
    m_selfWeight->setEnabled(bars);
    for (QAction* action : {m_new, m_edit, m_case, m_delete, m_selfWeight})
        if (!action->isEnabled() && action->isChecked()) action->setChecked(false);
}

void BarLoadTool::structureChanged() {
    updateEnabled();
    pruneViewports();
    configureViewports();
    for (const QPointer<Viewport3D>& viewport : m_viewports) viewport->update();
}

void BarLoadTool::setMode(Mode mode) {
    if (mode == m_mode) return;
    if (mode != Mode::None) emit aboutToActivate();
    m_mode = mode;
    m_selfWeightAsked = false;
    if (mode == Mode::None) {
        if (m_host) m_host->closeViewPanel();
    } else if (mode == Mode::SelfWeight) {
        // Measured: the click opens the small dialog at once; the status is only the title until Aceptar.
        emit statusChanged(tr("Barras - Peso propio"));
        if (!askSelfWeight()) {
            deactivate();
            return;
        }
        m_selfWeightAsked = true;
        if (m_host) m_host->showViewPanel();
    } else if (m_host) {
        m_host->showViewPanel();
    }
    configureViewports();
    emit modeChanged(mode);
    emit statusChanged(statusText(mode));  // after the viewports: setSelectionMode announces its own, generic hint
}

void BarLoadTool::deactivate() {
    if (QAction* checked = m_group->checkedAction()) checked->setChecked(false);
    setMode(Mode::None);
}

void BarLoadTool::configureViewports() {
    pruneViewports();
    const bool on = m_mode != Mode::None && (m_mode != Mode::SelfWeight || m_selfWeightAsked);
    if (!on && !m_configured) return;  // idle: the viewports belong to another command
    m_configured = on;
    const bool onLoads = m_mode == Mode::Edit || m_mode == Mode::Case || m_mode == Mode::Delete;
    const Project* project = m_project;
    const bool all = viewAll();
    const int caseId = viewCase();
    for (const QPointer<Viewport3D>& viewport : m_viewports) {
        viewport->setSelectionMode(false);
        viewport->setNodeLabelsVisible(false);
        viewport->setBarExclusion({});
        viewport->setNodeExclusion({});
        viewport->setLoadSelectionHighlight(onLoads);
        viewport->setSelfWeightDrawing(on && m_mode == Mode::SelfWeight, selfWeightCaseId());
        if (!on) continue;
        if (onLoads) {
            // Bars without a load in view cannot be captured (the loads are what is selected).
            viewport->setBarExclusion([project, all, caseId](int id) {
                if (!project) return true;
                for (const BarLoad& load : project->barLoads.items)
                    if (load.barId == id && (all || load.loadCaseId == caseId)) return false;
                return true;
            });
        }
        viewport->setNodeExclusion([](int) { return true; });  // only bars are captured
        viewport->setSelectionFilter(SelectionFilter::only(ElementKind::Bar));
        viewport->setSelectionMode(true);
        viewport->setFocus();
    }
}

std::vector<int> BarLoadTool::loadsOfBars(const std::vector<int>& barIds) const {
    std::vector<int> out;
    if (!m_project) return out;
    std::vector<int> bars = barIds;
    std::sort(bars.begin(), bars.end());
    bars.erase(std::unique(bars.begin(), bars.end()), bars.end());
    const bool all = viewAll();
    const int caseId = viewCase();
    for (int bar : bars)
        for (int id : barLoadsOfBar(m_project->barLoads, bar)) {
            const BarLoad* load = findBarLoad(m_project->barLoads, id);
            if (load && (all || load->loadCaseId == caseId)) out.push_back(id);
        }
    return out;
}

bool BarLoadTool::createLoads(const std::vector<int>& barIds) {
    if (!m_project || !m_undo || barIds.empty()) return false;
    BarLoadData initial = m_session.last;
    initial.loadCaseId = viewCase();  // by default the shown hypothesis (tooltip #64651)
    BarLoadDialog dialog(tr("Introducir cargas sobre barras"), m_project->loadCases, initial, shortestBarLength(m_project->structure, barIds), m_dialogParent);
    if (run(dialog) != QDialog::Accepted) return false;
    BarLoadIssue issue;
    const BarLoadError error = addBarLoads(*m_project, *m_undo, barIds, dialog.data(), nullptr, &issue, &m_session);
    if (error != BarLoadError::None) {
        if (error == BarLoadError::InvalidData) warn(QString::fromStdString(issue.message), issue.kind == BarLoadIssueKind::Error);
        else warn(QString::fromStdString(barLoadErrorMessage(error)));
        return false;
    }
    return true;
}

bool BarLoadTool::editLoads(const std::vector<int>& barIds) {
    const std::vector<int> ids = loadsOfBars(barIds);
    if (!m_project || !m_undo || ids.empty()) return false;
    const BarLoad* first = findBarLoad(m_project->barLoads, ids.front());  // measured: the dialog shows the data of the first load
    std::vector<int> bars;
    for (int id : ids) bars.push_back(findBarLoad(m_project->barLoads, id)->barId);
    BarLoadDialog dialog(tr("Descripción de la carga"), m_project->loadCases, barLoadToData(*first), shortestBarLength(m_project->structure, bars), m_dialogParent);
    if (run(dialog) != QDialog::Accepted) return false;
    BarLoadIssue issue;
    const BarLoadError error = editBarLoads(*m_project, *m_undo, ids, dialog.data(), &issue);  // every selected load takes the data (measured)
    if (error == BarLoadError::NothingToChange) return false;
    if (error != BarLoadError::None) {
        if (error == BarLoadError::InvalidData) warn(QString::fromStdString(issue.message), issue.kind == BarLoadIssueKind::Error);
        else warn(QString::fromStdString(barLoadErrorMessage(error)));
        return false;
    }
    return true;
}

bool BarLoadTool::changeCase(const std::vector<int>& barIds) {
    const std::vector<int> ids = loadsOfBars(barIds);
    if (!m_project || !m_undo || ids.empty()) return false;
    const BarLoad* first = findBarLoad(m_project->barLoads, ids.front());
    LoadCaseChoiceDialog dialog(m_project->loadCases, first->loadCaseId, m_dialogParent);
    if (run(dialog) != QDialog::Accepted) return false;
    const BarLoadError error = changeBarLoadCase(*m_project, *m_undo, ids, dialog.loadCaseId());
    if (error == BarLoadError::NothingToChange) return false;
    if (error != BarLoadError::None) {
        warn(QString::fromStdString(barLoadErrorMessage(error)));
        return false;
    }
    return true;
}

bool BarLoadTool::deleteLoads(const std::vector<int>& barIds) {
    const std::vector<int> ids = loadsOfBars(barIds);
    if (!m_project || !m_undo || ids.empty()) return false;
    return deleteBarLoads(*m_project, *m_undo, ids) == BarLoadError::None;  // immediate, no question (measured)
}

bool BarLoadTool::askSelfWeight() {
    BarSelfWeightDialog dialog(m_session.selfWeightActivate, m_dialogParent);
    if (run(dialog) != QDialog::Accepted) return false;
    m_session.selfWeightActivate = dialog.activate();  // the dialog remembers the last option
    return true;
}

bool BarLoadTool::applySelfWeight(const std::vector<int>& barIds) {
    if (!m_project || !m_undo) return false;
    if (barIds.empty()) {  // measured: RMB without a selection brings the dialog back
        askSelfWeight();
        return false;
    }
    // Without confirmation and without warning (the message #64002 was never seen).
    return setBarSelfWeight(*m_project, *m_undo, barIds, m_session.selfWeightActivate) == BarLoadError::None;
}

void BarLoadTool::showOutOfLengthWarning() { warn(QString::fromStdString(barLoadsOutOfLengthMessage())); }

void BarLoadTool::onActOnSelection(Viewport3D* viewport) {
    if (m_mode == Mode::None || !m_project || !m_undo) return;
    const std::vector<int> bars = viewport->selection().ids(ElementKind::Bar);
    if (bars.empty() && m_mode != Mode::SelfWeight) return;
    switch (m_mode) {
        case Mode::New: createLoads(bars); break;
        case Mode::Edit: editLoads(bars); break;
        case Mode::Case: changeCase(bars); break;
        case Mode::Delete: deleteLoads(bars); break;
        case Mode::SelfWeight: applySelfWeight(bars); break;
        case Mode::None: break;
    }
    viewport->clearSelection();
    if (m_mode != Mode::None) emit statusChanged(statusText(m_mode));
}

}  // namespace cc::app
