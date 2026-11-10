#include "ShellLoadTool.h"

#include <QAction>
#include <QActionGroup>
#include <QDialog>
#include <algorithm>

#include "NodalLoadTool.h"
#include "UserMessages.h"
#include "Viewport3D.h"

namespace cc::app {

using namespace cc::core;

ShellLoadTool::ShellLoadTool(QObject* parent) : QObject(parent) {
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

QString ShellLoadTool::statusText(Mode mode) {
    const QString hints = tr(" - LMB: Seleccione un grupo de elementos. - RMB: Actuar sobre los elementos seleccionados.");
    switch (mode) {
        case Mode::New: return tr("Láminas - Nueva |") + QLatin1Char(' ') + hints;
        case Mode::Edit: return tr("Láminas - Editar |") + QLatin1Char(' ') + hints;
        case Mode::Case: return tr("Láminas - Hipótesis |") + QLatin1Char(' ') + hints;
        case Mode::Delete: return tr("Láminas - Borrar |") + QLatin1Char(' ') + hints;
        case Mode::SelfWeight: return tr("Láminas - Peso propio |") + QLatin1Char(' ') + hints;
        case Mode::None: break;
    }
    return tr("Seleccione una opción del menú.");
}

QString ShellLoadTool::tipText(Mode mode) {
    const auto rich = [](const QString& title, const QString& text) {
        QString html = QStringLiteral("<b>") + title.toHtmlEscaped() + QStringLiteral("</b>");
        if (!text.isEmpty()) html += QStringLiteral("<br>") + text.toHtmlEscaped().replace(QLatin1Char('\n'), QStringLiteral("<br>"));
        return html;
    };
    switch (mode) {
        case Mode::New:  // measured (cl_tt_nueva, #64698)
            return rich(tr("Nueva"), tr("Introducción de cargas sobre un grupo de láminas. Seleccione una o varias láminas y pulse el botón derecho del ratón. "
                                        "Aparecerá el cuadro de diálogo de 'Introducir cargas sobre láminas', donde se introducen los valores que definen la carga."));
        case Mode::Edit:  // NOT MEASURED for the shells: the text of the bars (#64654 / #64655) with the title of the shell dialog
            return rich(tr("Editar"), tr("Permite modificar los datos de las cargas ya introducidas.\nSeleccione un grupo de cargas y pulse con el botón derecho del ratón. "
                                         "Aparecerá el diálogo de 'Descripción de la carga' para modificar los datos de las cargas."));
        case Mode::Delete:  // NOT MEASURED for the shells (#64656 / #64657, as the bars)
            return rich(tr("Borrar"), tr("Permite eliminar una o varias cargas vistas en pantalla.\nPara ello, seleccione el grupo de cargas que desea borrar y pulse el botón derecho del ratón."));
        case Mode::Case: return rich(tr("Hipótesis"), QString());
        case Mode::SelfWeight:  // #64702 + #64694 + #64696 (strings of ed3dlib)
            return rich(tr("Generación de las cargas de peso propio de las láminas"),
                        tr("Permite activar / desactivar la generación automática de la carga de peso propio.\nPara ello, seleccione las láminas para las que desea "
                           "activar o desactivar la generación de cargas de peso propio y pulse el botón derecho del ratón. Si pulsa el botón derecho del ratón sin haber "
                           "seleccionado ninguna lámina, volverá a aparecer el diálogo en el que puede escoger la acción a realizar.")) +
                   QStringLiteral("<br><br>") + tr("Pulse 'F1' alternativamente para mostrar u ocultar los textos de ayuda.").toHtmlEscaped();
        case Mode::None: break;
    }
    return {};
}

void ShellLoadTool::pruneViewports() {
    m_viewports.removeIf([](const QPointer<Viewport3D>& v) { return v.isNull(); });
}

int ShellLoadTool::run(QDialog& dialog) { return m_runner ? m_runner(dialog) : dialog.exec(); }

void ShellLoadTool::warn(const QString& text, bool error) {
    messages::show(m_dialogParent, error ? messages::Kind::Error : messages::Kind::Warning, text);
}

bool ShellLoadTool::viewAll() const { return m_host ? m_host->viewAll() : false; }
int ShellLoadTool::viewCase() const { return m_host ? m_host->viewCaseId() : 0; }

int ShellLoadTool::selfWeightCaseId() const {
    if (!m_project) return 0;
    for (const LoadCase& c : m_project->loadCases.cases)
        if (c.nature == LoadNature::SelfWeight) return c.id;
    return m_project->loadCases.cases.empty() ? 0 : m_project->loadCases.cases.front().id;
}

void ShellLoadTool::setHost(NodalLoadTool* host) {
    m_host = host;
    if (!host) return;
    connect(host, &NodalLoadTool::viewChanged, this, [this] {
        configureViewports();
        // configureViewports announces the generic selection hint: restore the command's own status (measured: the original keeps it)
        if (m_mode != Mode::None) emit statusChanged(statusText(m_mode));
    });
    connect(host, &NodalLoadTool::viewPanelClosed, this, [this] {
        if (m_mode != Mode::None) deactivate();  // closing the window ends the command (like the others)
    });
}

void ShellLoadTool::setProject(Project* project, UndoStack* undo) {
    deactivate();
    m_project = project;
    m_undo = undo;
    m_session = ShellLoadSession{};
    m_selfWeightAsked = false;
    pruneViewports();
    for (const QPointer<Viewport3D>& viewport : m_viewports) viewport->setShellLoads(m_project ? &m_project->shellLoads : nullptr, m_project ? &m_project->loadCases : nullptr);
    updateEnabled();
}

void ShellLoadTool::addViewport(Viewport3D* viewport) {
    if (!viewport || m_viewports.contains(viewport)) return;
    m_viewports.push_back(viewport);
    viewport->setShellLoads(m_project ? &m_project->shellLoads : nullptr, m_project ? &m_project->loadCases : nullptr);
    connect(viewport, &Viewport3D::actOnSelection, this, [this, viewport] { onActOnSelection(viewport); });
    connect(viewport, &Viewport3D::escapePressed, this, [this] { deactivate(); });
    if (m_mode != Mode::None) configureViewports();
}

void ShellLoadTool::updateEnabled() {
    const bool open = m_project != nullptr;
    const bool shells = open && !m_project->structure.shells.empty();
    const bool loads = open && hasShellLoads(m_project->shellLoads);
    m_new->setEnabled(shells);
    m_edit->setEnabled(loads);
    m_case->setEnabled(loads);
    m_delete->setEnabled(loads);
    m_selfWeight->setEnabled(shells);
    for (QAction* action : {m_new, m_edit, m_case, m_delete, m_selfWeight})
        if (!action->isEnabled() && action->isChecked()) action->setChecked(false);
}

void ShellLoadTool::structureChanged() {
    updateEnabled();
    pruneViewports();
    configureViewports();
    for (const QPointer<Viewport3D>& viewport : m_viewports) viewport->update();
}

void ShellLoadTool::setMode(Mode mode) {
    if (mode == m_mode) return;
    if (mode != Mode::None) emit aboutToActivate();
    m_mode = mode;
    m_selfWeightAsked = false;
    if (mode == Mode::None) {
        if (m_host) m_host->closeViewPanel();
    } else if (mode == Mode::SelfWeight) {
        // Measured: the click opens the small dialog at once; the status is only the title until Aceptar.
        emit statusChanged(tr("Láminas - Peso propio"));
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

void ShellLoadTool::deactivate() {
    if (QAction* checked = m_group->checkedAction()) checked->setChecked(false);
    setMode(Mode::None);
}

void ShellLoadTool::configureViewports() {
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
        viewport->setShellExclusion({});
        viewport->setWindowOnlySelection(false);
        viewport->setLoadSelectionHighlight(onLoads);
        // Measured (cargas_laminas.md 8.4): the self weight of the shells is drawn in every command of the group, as a pale prism.
        viewport->setShellSelfWeightDrawing(on, selfWeightCaseId());
        if (!on) continue;
        if (onLoads) {
            // Shells without a load in view cannot be captured (the loads are what is selected).
            viewport->setShellExclusion([project, all, caseId](int id) {
                if (!project) return true;
                for (const ShellLoad& load : project->shellLoads.items)
                    if (load.shellId == id && (all || load.loadCaseId == caseId)) return false;
                return true;
            });
        }
        viewport->setWindowOnlySelection(m_mode == Mode::SelfWeight);  // measured: in Peso propio a click does not select, only the window does
        viewport->setNodeExclusion([](int) { return true; });          // only shells are captured
        viewport->setBarExclusion([](int) { return true; });
        viewport->setSelectionFilter(SelectionFilter::only(ElementKind::Shell));
        viewport->setSelectionMode(true);
        viewport->setFocus();
    }
}

std::vector<int> ShellLoadTool::loadsOfShells(const std::vector<int>& shellIds) const {
    std::vector<int> out;
    if (!m_project) return out;
    std::vector<int> shells = shellIds;
    std::sort(shells.begin(), shells.end());
    shells.erase(std::unique(shells.begin(), shells.end()), shells.end());
    const bool all = viewAll();
    const int caseId = viewCase();
    for (int shell : shells)
        for (int id : shellLoadsOfShell(m_project->shellLoads, shell)) {
            const ShellLoad* load = findShellLoad(m_project->shellLoads, id);
            if (load && (all || load->loadCaseId == caseId)) {
                out.push_back(id);  // one RMB acts on one load of the shell: the first one created (measured)
                break;
            }
        }
    return out;
}

bool ShellLoadTool::createLoads(const std::vector<int>& shellIds) {
    if (!m_project || !m_undo || shellIds.empty()) return false;
    ShellLoadData initial = m_session.last;
    // Measured (cargas_laminas.md 13.1): the default is the last hypothesis used in this dialog, the shown one only the first time
    // (or when the remembered hypothesis no longer exists); the value is not remembered.
    const bool remembered = m_session.hasLastCase && std::any_of(m_project->loadCases.cases.begin(), m_project->loadCases.cases.end(),
                                                                   [&](const LoadCase& c) { return c.id == initial.loadCaseId; });
    if (!remembered) initial.loadCaseId = viewCase();
    const ShellLoadData typeDefaults = shellLoadDefaults(initial.loadCaseId, initial.type);
    initial.value1 = typeDefaults.value1;
    initial.value2 = typeDefaults.value2;
    ShellLoadDialog dialog(tr("Introducir cargas sobre láminas"), m_project->loadCases, initial, m_dialogParent);
    if (run(dialog) != QDialog::Accepted) return false;
    ShellLoadIssue issue;
    const ShellLoadError error = addShellLoads(*m_project, *m_undo, shellIds, dialog.data(), nullptr, &issue, &m_session);
    if (error != ShellLoadError::None) {
        if (error == ShellLoadError::InvalidData) warn(QString::fromStdString(issue.message), issue.kind == ShellLoadIssueKind::Error);
        else warn(QString::fromStdString(shellLoadErrorMessage(error)));
        return false;
    }
    return true;
}

bool ShellLoadTool::editLoads(const std::vector<int>& shellIds) {
    const std::vector<int> ids = loadsOfShells(shellIds);
    if (!m_project || !m_undo || ids.empty()) return false;
    const ShellLoad* first = findShellLoad(m_project->shellLoads, ids.front());  // measured: the dialog shows the data of the first load
    ShellLoadDialog dialog(tr("Descripción de la carga"), m_project->loadCases, shellLoadToData(*first), m_dialogParent);
    if (run(dialog) != QDialog::Accepted) return false;
    ShellLoadIssue issue;
    const ShellLoadError error = editShellLoads(*m_project, *m_undo, ids, dialog.data(), &issue);  // every selected load takes the data
    if (error == ShellLoadError::NothingToChange) return false;
    if (error != ShellLoadError::None) {
        if (error == ShellLoadError::InvalidData) warn(QString::fromStdString(issue.message), issue.kind == ShellLoadIssueKind::Error);
        else warn(QString::fromStdString(shellLoadErrorMessage(error)));
        return false;
    }
    return true;
}

bool ShellLoadTool::changeCase(const std::vector<int>& shellIds) {
    const std::vector<int> ids = loadsOfShells(shellIds);
    if (!m_project || !m_undo || ids.empty()) return false;
    const ShellLoad* first = findShellLoad(m_project->shellLoads, ids.front());
    LoadCaseChoiceDialog dialog(m_project->loadCases, first->loadCaseId, m_dialogParent);
    if (run(dialog) != QDialog::Accepted) return false;
    const ShellLoadError error = changeShellLoadCase(*m_project, *m_undo, ids, dialog.loadCaseId());
    if (error == ShellLoadError::NothingToChange) return false;
    if (error != ShellLoadError::None) {
        warn(QString::fromStdString(shellLoadErrorMessage(error)));
        return false;
    }
    return true;
}

bool ShellLoadTool::deleteLoads(const std::vector<int>& shellIds) {
    const std::vector<int> ids = loadsOfShells(shellIds);
    if (!m_project || !m_undo || ids.empty()) return false;
    return deleteShellLoads(*m_project, *m_undo, ids) == ShellLoadError::None;  // immediate, no question (measured)
}

bool ShellLoadTool::askSelfWeight() {
    ShellSelfWeightDialog dialog(m_session.selfWeightActivate, m_dialogParent);
    if (run(dialog) != QDialog::Accepted) return false;
    m_session.selfWeightActivate = dialog.activate();  // the dialog remembers the last option
    return true;
}

bool ShellLoadTool::applySelfWeight(const std::vector<int>& shellIds) {
    if (!m_project || !m_undo) return false;
    if (shellIds.empty()) {  // measured: RMB without a selection brings the dialog back
        askSelfWeight();
        return false;
    }
    // Without confirmation and without warning (measured: no box).
    return setShellSelfWeight(*m_project, *m_undo, shellIds, m_session.selfWeightActivate) == ShellLoadError::None;
}

void ShellLoadTool::onActOnSelection(Viewport3D* viewport) {
    if (m_mode == Mode::None || !m_project || !m_undo) return;
    const std::vector<int> shells = viewport->selection().ids(ElementKind::Shell);
    if (shells.empty() && m_mode != Mode::SelfWeight) return;
    switch (m_mode) {
        case Mode::New: createLoads(shells); break;
        case Mode::Edit: editLoads(shells); break;
        case Mode::Case: changeCase(shells); break;
        case Mode::Delete: deleteLoads(shells); break;
        case Mode::SelfWeight: applySelfWeight(shells); break;
        case Mode::None: break;
    }
    viewport->clearSelection();
    if (m_mode != Mode::None) emit statusChanged(statusText(m_mode));
}

}  // namespace cc::app
