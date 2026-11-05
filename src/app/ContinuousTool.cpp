#include "ContinuousTool.h"

#include <QAction>
#include <QDialog>
#include <QMenu>
#include <algorithm>
#include <set>

#include "ContinuousDialogs.h"
#include "Viewport3D.h"

namespace cc::app {

using namespace cc::core;

ContinuousTool::ContinuousTool(QObject* parent) : QObject(parent) {
    m_pillarsMenu = new QMenu(tr("Edición de pilares"));
    m_beamsMenu = new QMenu(tr("Edición de vigas"));
    connect(this, &QObject::destroyed, m_pillarsMenu, &QObject::deleteLater);
    connect(this, &QObject::destroyed, m_beamsMenu, &QObject::deleteLater);
    // Tooltips: the help texts #64853-64881 of the original (first sentence also as status tip).
    m_createColumn = addEntry(m_pillarsMenu, tr("Crear pilar continuo"), tr("Permite describir conjuntamente un grupo de piezas alineadas como un único pilar."),
                              tr("Permite describir conjuntamente un grupo de piezas alineadas como un único pilar."));
    m_slabDepth = addEntry(m_pillarsMenu, tr("Editar canto del forjado"), tr("Permite editar el canto del forjado en un nudo de un pilar."),
                           tr("Permite editar el canto del forjado en un nudo de un pilar. El canto del forjado determina la altura libre del pilar entre dos niveles. Utilice esta opción si desea utilizar un canto de forjado diferente al determinado automáticamente por el programa."));
    m_columnReference = addEntry(m_pillarsMenu, tr("Editar referencia"), tr("Permite editar la referencia del pilar."), tr("Permite editar la referencia del pilar."));
    m_number = addEntry(m_pillarsMenu, tr("Numerar"), tr("Permite numerar consecutivamente todos los pilares de la obra a partir de una referencia inicial dada."),
                        tr("Permite numerar consecutivamente todos los pilares de la obra a partir de una referencia inicial dada."));

    m_createBeam = addEntry(m_beamsMenu, tr("Crear viga continua"), tr("Permite crear una nueva viga continua."),
                            tr("Permite crear una nueva viga continua. Seleccione las vigas que compondrán la nueva viga continua y pulse el botón derecho del ratón.\nPara poder crear una viga continua, las vigas seleccionadas deben estar conectadas entre sí de forma tal que el nudo final de una sea el inicial de la siguiente (en el sentido de su directriz). Además, todas deben ser de la misma clase de material (hormigón, acero laminado o acero conformado) y el plano xz local de cada una debe estar contenido en un plano vertical."));
    m_editNodes = addEntry(m_beamsMenu, tr("Editar nudos"), tr("Permite editar la geometría de un nudo de una viga continua."),
                           tr("Permite editar la geometría de un nudo de una viga continua. Utilice esta opción si desea una geometría diferente a la determinada automáticamente por el programa. Seleccione uno o varios nudos y pulse el botón derecho del ratón.\nLa geometría del nudo afecta a la luz libre de las vigas que se representa en el editor de armaduras de vigas y, por tanto, a la longitud de las armaduras dispuestas."));
    m_adjacentSlabs = addEntry(m_beamsMenu, tr("Editar forjados adyacentes"), tr("Permite describir los forjados adyacentes a las barras que componen las vigas."),
                               tr("Permite describir los forjados adyacentes a las barras que componen las vigas. Utilice esta opción para definir el canto de las vigas planas, así como para indicar los forjados que desea que se representen en el plano de despiece de vigas.\nAdemás, la definición de los forjados se tiene en cuenta para la comprobación de la resistencia a incendio de las vigas metálicas.\nSeleccione una o más vigas y pulse el botón derecho del ratón."));
    m_beamsMenu->addSeparator();
    m_beamReferences = addEntry(m_beamsMenu, tr("Editar referencias de vigas"), tr("Permite editar las referencias de las vigas."),
                                tr("Permite editar las referencias de las vigas. Utilice esta opción si desea utilizar una referencia distinta de la asignada por la numeración automática, en caso de haberla activado. El prefijo de la numeración se edita en los datos generales de la obra."));
    m_continuousBeamReferences = addEntry(m_beamsMenu, tr("Editar referencias de vigas continuas"), tr("Permite editar las referencias de las vigas continuas."),
                                          tr("Permite editar las referencias de las vigas continuas. Utilice esta opción si desea utilizar una referencia distinta de la asignada por la numeración automática. El prefijo de la numeración se edita en los datos generales de la obra."));
    m_beamsMenu->addSeparator();
    m_deflectionLimit = addEntry(m_beamsMenu, tr("Flecha límite de vigas"), tr("Permite editar la flecha límite de las vigas."),
                                 tr("Permite editar la flecha límite de las vigas. Seleccione uno o más grupos de flecha que contengan vigas y pulse el botón derecho del ratón. Las propiedades editadas se asignarán a todos ellos. Tenga en cuenta que los grupos de flecha definidos son los que se usan para realizar las comprobaciones de flecha de las vigas."));
    m_beamEnvironment = addEntry(m_beamsMenu, tr("Editar ambiente de vigas"), tr("Editar ambiente de vigas"), tr("Editar ambiente de vigas"));  // no help text found for #65423

    connect(m_createColumn, &QAction::triggered, this, [this] { startMode(Mode::CreateColumn); });
    connect(m_slabDepth, &QAction::triggered, this, [this] { startMode(Mode::SlabDepth); });
    connect(m_columnReference, &QAction::triggered, this, [this] { startMode(Mode::ColumnReference); });
    connect(m_number, &QAction::triggered, this, [this] {
        deactivate();
        numberColumns();
    });
    connect(m_createBeam, &QAction::triggered, this, [this] { startMode(Mode::CreateBeam); });
    connect(m_beamReferences, &QAction::triggered, this, [this] { startMode(Mode::BeamReferences); });
    connect(m_continuousBeamReferences, &QAction::triggered, this, [this] { startMode(Mode::ContinuousBeamReferences); });
    connect(m_deflectionLimit, &QAction::triggered, this, [this] { startMode(Mode::DeflectionLimit); });
    for (QAction* action : {m_editNodes, m_adjacentSlabs, m_beamEnvironment})  // TK-078: no core yet
        connect(action, &QAction::triggered, this, [this] {
            deactivate();
            emit notAvailable();
        });
    updateEnabled();
}

QAction* ContinuousTool::addEntry(QMenu* menu, const QString& text, const QString& tip, const QString& tooltip) {
    QAction* action = menu->addAction(text);
    action->setStatusTip(tip);
    action->setToolTip(tooltip);
    return action;
}

QString ContinuousTool::statusText(Mode mode, int step, int selected) {
    const QString lmb = tr(" - LMB: Seleccione un elemento.");
    const QString act = tr(" - RMB: Actuar sobre los elementos seleccionados.");
    const auto selectable = [&](const QString& name) {
        return name + QStringLiteral(" | ") + lmb + (selected > 0 ? act : QString());
    };
    switch (mode) {
        case Mode::None: return tr("Seleccione una opción del menú.");
        case Mode::CreateColumn: return tr("Seleccione el nudo inicial de la primera pieza y el nudo final de la última pieza que desea que formen el pilar.");  // #64854
        case Mode::CreateBeam: return tr("Permite crear una nueva viga continua. Seleccione las vigas que compondrán la nueva viga continua y pulse el botón derecho del ratón.");  // #64863
        case Mode::SlabDepth: return selectable(tr("Edición de pilares - Editar canto del forjado"));
        case Mode::ColumnReference: return selectable(tr("Edición de pilares - Editar referencia"));
        case Mode::BeamReferences: return selectable(tr("Edición de vigas - Editar referencias de vigas"));
        case Mode::ContinuousBeamReferences: return selectable(tr("Edición de vigas - Editar referencias de vigas continuas"));
        case Mode::DeflectionLimit: return selectable(tr("Edición de vigas - Flecha límite de vigas"));
    }
    (void)step;
    return {};
}

std::optional<ReferenceTarget> ContinuousTool::targetOfBar(const Project& project, int barId, ElementType wanted, Scope scope) {
    const Piece* piece = project.structure.pieceOfBar(barId);
    if (!piece) return std::nullopt;
    const bool pillar = wanted == ElementType::Pillar;
    if (scope != Scope::LoneOnly) {
        const int id = pillar ? (project.continuous.columnOfPiece(piece->id) ? project.continuous.columnOfPiece(piece->id)->id : 0)
                              : (project.continuous.beamOfPiece(piece->id) ? project.continuous.beamOfPiece(piece->id)->id : 0);
        if (id != 0) return ReferenceTarget{pillar ? ReferenceTarget::Kind::ContinuousColumn : ReferenceTarget::Kind::ContinuousBeam, id};
    }
    if (scope != Scope::ContinuousOnly && piece->type == wanted) return ReferenceTarget{ReferenceTarget::Kind::Piece, piece->id};
    return std::nullopt;
}

void ContinuousTool::pruneViewports() {
    m_viewports.removeIf([](const QPointer<Viewport3D>& v) { return v.isNull(); });
}

void ContinuousTool::setProject(Project* project, UndoStack* undo) {
    deactivate();
    m_project = project;
    m_undo = undo;
    updateEnabled();
}

void ContinuousTool::addViewport(Viewport3D* viewport) {
    if (!viewport || m_viewports.contains(viewport)) return;
    m_viewports.push_back(viewport);
    connect(viewport, &Viewport3D::selectionChanged, this, [this, viewport] { onSelectionChanged(viewport); });
    connect(viewport, &Viewport3D::actOnSelection, this, [this, viewport] { onActOnSelection(viewport); });
    connect(viewport, &Viewport3D::escapePressed, this, [this] { deactivate(); });
    if (m_mode != Mode::None) configureViewports();
}

void ContinuousTool::deactivate() { setMode(Mode::None); }

void ContinuousTool::updateEnabled() {
    // The original greys the buttons out while the work has no pillars / beams.
    const bool pillars = m_project && !listPillars(*m_project).empty();
    const bool beams = m_project && !listBeams(*m_project).empty();
    m_pillarsMenu->menuAction()->setEnabled(pillars);
    m_beamsMenu->menuAction()->setEnabled(beams);
    if ((!pillars && (m_mode == Mode::CreateColumn || m_mode == Mode::SlabDepth || m_mode == Mode::ColumnReference)) ||
        (!beams && (m_mode == Mode::CreateBeam || m_mode == Mode::BeamReferences || m_mode == Mode::ContinuousBeamReferences || m_mode == Mode::DeflectionLimit)))
        deactivate();
}

void ContinuousTool::structureChanged() {
    updateEnabled();
    pruneViewports();
    for (const QPointer<Viewport3D>& viewport : m_viewports) viewport->refreshStructure();
    if (!m_project) return;
    const bool nodeGone = std::any_of(m_chain.begin(), m_chain.end(), [this](int id) { return !m_project->structure.findNode(id); });
    if (nodeGone) resetSteps();
}

ElementKind ContinuousTool::pickKind() const {
    return (m_mode == Mode::CreateColumn || m_mode == Mode::SlabDepth) ? ElementKind::Node : ElementKind::Bar;
}

void ContinuousTool::startMode(Mode mode) {
    if (mode == m_mode) {  // choosing the same entry again starts it over
        resetSteps();
        for (const QPointer<Viewport3D>& viewport : m_viewports) viewport->clearSelection();
        return;
    }
    setMode(mode);
}

void ContinuousTool::setMode(Mode mode) {
    if (mode == m_mode) return;
    m_chain.clear();
    if (mode != Mode::None) emit aboutToActivate();
    m_mode = mode;
    configureViewports();
    emit modeChanged(mode);
    emit statusChanged(statusText(mode));
}

void ContinuousTool::configureViewports() {
    pruneViewports();
    for (const QPointer<Viewport3D>& viewport : m_viewports) {
        viewport->setSelectionMode(false);
        viewport->setSnapMode(false);
        viewport->setRubberChain(m_chain);
        viewport->setMovingNode(std::nullopt);
        viewport->setNodeLabelsVisible(m_mode != Mode::None);
        viewport->setCoordinateTooltipEnabled(true);
        if (m_mode != Mode::None) {
            viewport->setSelectionFilter(SelectionFilter::only(pickKind()));
            viewport->setSelectionMode(true);
            viewport->setFocus();
        }
    }
}

void ContinuousTool::resetSteps() {
    m_chain.clear();
    configureViewports();
    if (m_mode != Mode::None) emit statusChanged(statusText(m_mode));
}

int ContinuousTool::run(QDialog& dialog) { return m_runner ? m_runner(dialog) : dialog.exec(); }

void ContinuousTool::report(ContinuousError error, const std::string& detail) {
    if (error != ContinuousError::None) emit messageRequested(QString::fromStdString(continuousErrorMessage(error, detail)));
}

void ContinuousTool::onSelectionChanged(Viewport3D* viewport) {
    if (m_internalSelection || m_mode == Mode::None) return;
    if (m_mode != Mode::CreateColumn) {  // the selection stays until the RMB acts on it
        emit statusChanged(statusText(m_mode, 0, static_cast<int>(viewport->selection().count(pickKind()))));
        return;
    }
    const std::vector<int> ids = viewport->selection().ids(ElementKind::Node);
    if (ids.empty()) return;
    m_internalSelection = true;  // the pick is consumed so that the same node can be clicked again
    viewport->clearSelection();
    m_internalSelection = false;
    if (ids.size() != 1 || !m_project || !m_undo) return;  // a window that catches several nodes does nothing
    if (m_chain.empty()) {
        m_chain.push_back(ids.front());
    } else {
        if (m_chain.front() == ids.front()) return;  // the same node twice: waits for another one
        const int start = m_chain.front();
        m_chain.clear();
        createColumn(start, ids.front());  // a refused attempt starts over (the message is shown)
    }
    for (const QPointer<Viewport3D>& v : m_viewports) v->setRubberChain(m_chain);
}

void ContinuousTool::onActOnSelection(Viewport3D* viewport) {
    if (m_mode == Mode::None) return;
    if (m_mode == Mode::CreateColumn) {  // "Volver al paso anterior"
        if (!m_chain.empty()) resetSteps();
        return;
    }
    const std::vector<int> ids = viewport->selection().ids(pickKind());
    if (ids.empty() || !m_project || !m_undo) return;
    const Mode mode = m_mode;
    switch (mode) {
        case Mode::SlabDepth: editSlabDepth(ids); break;
        case Mode::ColumnReference: editColumnReferences(ids); break;
        case Mode::CreateBeam: createBeam(ids); break;
        case Mode::BeamReferences: editBeamReferences(ids, false); break;
        case Mode::ContinuousBeamReferences: editBeamReferences(ids, true); break;
        case Mode::DeflectionLimit: editDeflectionLimit(ids); break;
        default: break;
    }
    m_internalSelection = true;
    viewport->clearSelection();
    m_internalSelection = false;
    emit statusChanged(statusText(m_mode));
}

// ---- the edits -----------------------------------------------------------------------------------------------------------

template <class Item>
bool ContinuousTool::applyAll(const QString& macroName, const std::vector<Item>& items, const std::function<ContinuousError(const Item&)>& check,
                              const std::function<ContinuousError(const Item&)>& apply) {
    if (!m_project || !m_undo || items.empty()) return false;
    // The items are independent (distinct nodes / elements / groups): every one is checked against the current state first,
    // so a refusal leaves nothing half done and nothing in the history.
    for (const Item& item : items) {
        const ContinuousError error = check(item);
        if (error != ContinuousError::None) {
            report(error);
            return false;
        }
    }
    m_undo->beginMacro(macroName.toStdString());
    ContinuousError error = ContinuousError::None;
    for (const Item& item : items) {
        error = apply(item);
        if (error != ContinuousError::None) break;
    }
    m_undo->endMacro();
    report(error);
    return error == ContinuousError::None;
}

bool ContinuousTool::createColumn(int startNode, int endNode) {
    if (!m_project || !m_undo) return false;
    const ContinuousError error = createContinuousColumn(*m_project, *m_undo, startNode, endNode);
    report(error);
    return error == ContinuousError::None;
}

bool ContinuousTool::createBeam(const std::vector<int>& barIds) {
    if (!m_project || !m_undo) return false;
    std::string detail;
    const ContinuousError error = createContinuousBeam(*m_project, *m_undo, barIds, nullptr, &detail);
    report(error, detail);
    return error == ContinuousError::None;
}

bool ContinuousTool::editSlabDepth(const std::vector<int>& nodeIds) {
    if (!m_project || !m_undo || nodeIds.empty()) return false;
    SlabDepthDialog dialog(slabDepthAt(*m_project, nodeIds.front()), m_dialogParent);
    if (run(dialog) != QDialog::Accepted) return false;
    const std::optional<double> depth = dialog.depthM();
    return applyAll<int>(
        QStringLiteral("Editar canto del forjado"), nodeIds, [&](const int& node) { return SetSlabDepthCommand(*m_project, node, depth).error(); },
        [&](const int& node) { return setSlabDepth(*m_project, *m_undo, node, depth); });
}

bool ContinuousTool::editColumnReferences(const std::vector<int>& barIds) {
    if (!m_project || !m_undo) return false;
    std::vector<ReferenceTarget> targets;
    for (int bar : barIds)
        if (const auto target = targetOfBar(*m_project, bar, ElementType::Pillar, Scope::Any))
            if (std::find(targets.begin(), targets.end(), *target) == targets.end()) targets.push_back(*target);
    if (targets.empty()) {
        report(ContinuousError::NotPillar);
        return false;
    }
    std::vector<std::pair<ReferenceTarget, std::string>> edits;
    for (const ReferenceTarget& target : targets) {
        ReferenceDialog dialog(tr("Editar referencia"), tr("Referencia"),
                               QString::fromStdString(elementReference(*m_project, target, kDefaultPillarPrefix)), m_dialogParent);  // #65436
        if (run(dialog) != QDialog::Accepted) return false;
        edits.emplace_back(target, dialog.reference().trimmed().toStdString());
    }
    return applyAll<std::pair<ReferenceTarget, std::string>>(QStringLiteral("Editar referencia"), edits, [&](const auto& edit) { return SetReferenceCommand(*m_project, edit.first, edit.second).error(); },
        [&](const auto& edit) { return setReference(*m_project, *m_undo, edit.first, edit.second); });
}

bool ContinuousTool::editBeamReferences(const std::vector<int>& barIds, bool continuous) {
    if (!m_project || !m_undo) return false;
    std::vector<ReferenceTarget> targets;
    for (int bar : barIds)
        if (const auto target = targetOfBar(*m_project, bar, ElementType::Beam, continuous ? Scope::ContinuousOnly : Scope::LoneOnly))
            if (std::find(targets.begin(), targets.end(), *target) == targets.end()) targets.push_back(*target);
    if (targets.empty()) {
        report(ContinuousError::NotBeamBars);
        return false;
    }
    const QString title = continuous ? tr("Editar referencias de vigas continuas") : tr("Editar referencias de vigas");  // #64880 / #64878
    std::vector<std::pair<ReferenceTarget, std::string>> edits;
    for (const ReferenceTarget& target : targets) {
        ReferenceDialog dialog(title, tr("Referencia"), QString::fromStdString(elementReference(*m_project, target, kDefaultBeamPrefix)), m_dialogParent);
        if (run(dialog) != QDialog::Accepted) return false;
        edits.emplace_back(target, dialog.reference().trimmed().toStdString());
    }
    return applyAll<std::pair<ReferenceTarget, std::string>>(title, edits, [&](const auto& edit) { return SetReferenceCommand(*m_project, edit.first, edit.second).error(); },
        [&](const auto& edit) { return setReference(*m_project, *m_undo, edit.first, edit.second); });
}

bool ContinuousTool::editDeflectionLimit(const std::vector<int>& barIds) {
    if (!m_project || !m_undo) return false;
    std::vector<std::vector<int>> groups;  // one deflection group per beam (continuous or lone)
    std::set<std::vector<int>> seen;
    for (int bar : barIds) {
        const auto target = targetOfBar(*m_project, bar, ElementType::Beam, Scope::Any);
        if (!target) continue;
        std::vector<int> bars = beamBarIds(*m_project, *target);
        if (!bars.empty() && seen.insert(bars).second) groups.push_back(std::move(bars));
    }
    if (groups.empty()) {
        report(ContinuousError::NotBeamBars);
        return false;
    }
    const std::optional<BeamDeflectionLimit> current = deflectionLimitOfBar(*m_project, groups.front().front());
    DeflectionLimitDialog dialog(current ? current->relative : 0.0, current ? current->absoluteMm : 0.0, m_dialogParent);
    if (run(dialog) != QDialog::Accepted) return false;
    const BeamDeflectionLimit limit{dialog.relative(), dialog.absoluteMm()};
    return applyAll<std::vector<int>>(
        QStringLiteral("Editar flecha límite"), groups, [&](const std::vector<int>& bars) { return SetDeflectionLimitCommand(*m_project, bars, limit).error(); },
        [&](const std::vector<int>& bars) { return setDeflectionLimit(*m_project, *m_undo, bars, limit); });
}

bool ContinuousTool::numberColumns() {
    if (!m_project || !m_undo) return false;
    ReferenceDialog dialog(tr("Referencia inicial"), tr("Referencia inicial"), QString::fromStdString(std::string(kDefaultPillarPrefix) + "1"), m_dialogParent);  // #64972
    if (run(dialog) != QDialog::Accepted) return false;
    const ContinuousError error = numberPillars(*m_project, *m_undo, dialog.reference().trimmed().toStdString());
    report(error);
    return error == ContinuousError::None;
}

}  // namespace cc::app
