#include "PieceTool.h"

#include <QAction>
#include <QActionGroup>
#include <algorithm>

#include "Viewport3D.h"

namespace cc::app {

PieceTool::PieceTool(QObject* parent) : QObject(parent) {
    m_actions = new QActionGroup(this);
    m_actions->setExclusionPolicy(QActionGroup::ExclusionPolicy::ExclusiveOptional);
    const auto make = [this](const QString& text, const QString& tip, const QString& tooltip, Mode mode) {
        auto* action = new QAction(text, this);
        action->setCheckable(true);
        action->setEnabled(false);
        action->setStatusTip(tip);
        action->setToolTip(tooltip);
        m_actions->addAction(action);
        connect(action, &QAction::toggled, this, [this, mode](bool on) {
            if (on)
                setMode(mode);
            else if (m_mode == mode)
                setMode(Mode::None);
        });
        return action;
    };
    // Tooltips #64624, #64626, #64627, #64645, #64640 (pieza.md); the status tip is the first sentence.
    m_piece = make(tr("Pieza"), tr("Permite describir conjuntamente un grupo de barras alineadas como una única pieza."),
                   tr("Permite describir conjuntamente un grupo de barras alineadas como una única pieza."), Mode::Piece);
    m_group = make(tr("Agrupar"), tr("Permite crear grupos de piezas que tendrán asignado el mismo perfil y material, incluso como resultado del proceso de dimensionamiento."),
                   tr("Permite crear grupos de piezas que tendrán asignado el mismo perfil y material, incluso como resultado del proceso de dimensionamiento.\nPara crear un grupo seleccione dos o más piezas y pulse el botón derecho del ratón."),
                   Mode::Group);
    m_ungroup = make(tr("Desagrupar"), tr("Permite independizar piezas de un grupo, de forma que su descripción ya es independiente."),
                     tr("Permite independizar piezas de un grupo, de forma que su descripción ya es independiente.\nSeleccione las piezas que desee desagrupar y pulse el botón derecho del ratón."),
                     Mode::Ungroup);
    m_moveEnd = make(tr("Mover extremo"), tr("Permite mover el extremo de una barra a una nueva posición."),
                     tr("Permite mover el extremo de una barra a una nueva posición."), Mode::MoveEnd);
    m_invert = make(tr("Invertir eje 'x'"), tr("Al pulsar sobre una barra con esta opción activada, se invierte la dirección del eje local X de la barra."),
                    tr("Al pulsar sobre una barra con esta opción activada, se invierte la dirección del eje local X de la barra. De esta forma puede conseguir disposiciones simétricas en la estructura con perfiles no simétricos, como pueden ser los Z o L."),
                    Mode::InvertAxis);
}

QString PieceTool::statusText(Mode mode, int step, int selected) {
    // PROVISIONAL (pieza.md section 8, item 1): same layout as the measured Nudos texts, generic pieces of entopane.
    switch (mode) {
        case Mode::None: return tr("Seleccione una opción del menú.");
        case Mode::Piece:
            return step > 0 ? tr("Barras - Pieza |  - LMB: Seleccione un elemento. - RMB: Volver al paso anterior.")
                            : tr("Barras - Pieza |  - LMB: Seleccione un elemento.");
        case Mode::Group:
            return selected > 0 ? tr("Barras - Agrupar |  - LMB: Seleccione un elemento. - RMB: Actuar sobre los elementos seleccionados.")
                                : tr("Barras - Agrupar |  - LMB: Seleccione un elemento.");
        case Mode::Ungroup:
            return selected > 0 ? tr("Barras - Desagrupar |  - LMB: Seleccione un elemento. - RMB: Actuar sobre los elementos seleccionados.")
                                : tr("Barras - Desagrupar |  - LMB: Seleccione un elemento.");
        case Mode::MoveEnd:
            return step > 0 ? tr("Barras - Mover extremo |  - LMB: Seleccione un elemento. - RMB: Volver al paso anterior.")
                            : tr("Barras - Mover extremo |  - LMB: Seleccione un elemento.");
        case Mode::InvertAxis: return tr("Barras - Invertir eje 'x' |  - LMB: Seleccione un elemento.");
    }
    return {};
}

void PieceTool::pruneViewports() {
    m_viewports.removeIf([](const QPointer<Viewport3D>& v) { return v.isNull(); });
}

void PieceTool::setProject(cc::core::Project* project, cc::core::UndoStack* undo) {
    deactivate();
    m_project = project;
    m_undo = undo;
    updateEnabled();
}

void PieceTool::addViewport(Viewport3D* viewport) {
    if (!viewport || m_viewports.contains(viewport)) return;
    m_viewports.push_back(viewport);
    connect(viewport, &Viewport3D::selectionChanged, this, [this, viewport] { onSelectionChanged(viewport); });
    connect(viewport, &Viewport3D::actOnSelection, this, [this, viewport] { onActOnSelection(viewport); });
    connect(viewport, &Viewport3D::escapePressed, this, [this] { deactivate(); });
    if (m_mode != Mode::None) configureViewports();
}

void PieceTool::deactivate() {
    if (QAction* checked = m_actions->checkedAction()) checked->setChecked(false);
    setMode(Mode::None);
}

void PieceTool::updateEnabled() {
    const bool project = m_project != nullptr;
    for (QAction* action : {m_piece, m_group, m_ungroup, m_moveEnd, m_invert}) {
        action->setEnabled(project);
        if (!project && action->isChecked()) action->setChecked(false);
    }
}

void PieceTool::structureChanged() {
    updateEnabled();
    pruneViewports();
    for (const QPointer<Viewport3D>& viewport : m_viewports) viewport->refreshStructure();
    if (!m_project) return;
    const bool nodeGone = std::any_of(m_chain.begin(), m_chain.end(), [this](int id) { return !m_project->structure.findNode(id); });
    const bool barGone = m_moveStep > 0 && !m_project->structure.findBar(m_moveBar);
    if (nodeGone || barGone) resetSteps();  // a chosen element that no longer exists (undo...) breaks the steps
}

cc::core::ElementKind PieceTool::pickKind() const {
    switch (m_mode) {
        case Mode::Piece: return cc::core::ElementKind::Node;
        case Mode::MoveEnd: return m_moveStep == 0 ? cc::core::ElementKind::Bar : cc::core::ElementKind::Node;
        default: return cc::core::ElementKind::Bar;
    }
}

void PieceTool::setMode(Mode mode) {
    if (mode == m_mode) return;
    m_chain.clear();
    m_moveStep = 0;
    if (mode != Mode::None) emit aboutToActivate();
    m_mode = mode;
    configureViewports();
    emit modeChanged(mode);
    emit statusChanged(statusText(mode));
}

void PieceTool::configureViewports() {
    pruneViewports();
    for (const QPointer<Viewport3D>& viewport : m_viewports) {
        viewport->setSelectionMode(false);
        viewport->setSnapMode(false);
        viewport->setRubberChain(m_chain);
        viewport->setMovingNode(std::nullopt);
        viewport->setNodeLabelsVisible(m_mode != Mode::None);
        viewport->setCoordinateTooltipEnabled(true);
        if (m_mode != Mode::None) {
            viewport->setSelectionFilter(cc::core::SelectionFilter::only(pickKind()));
            viewport->setSelectionMode(true);
            viewport->setFocus();
        }
    }
}

void PieceTool::resetSteps() {
    m_chain.clear();
    m_moveStep = 0;
    configureViewports();
    if (m_mode != Mode::None) emit statusChanged(statusText(m_mode));
}

void PieceTool::report(cc::core::PieceError error) {
    if (error != cc::core::PieceError::None) emit messageRequested(QString::fromStdString(cc::core::pieceErrorMessage(error)));
}

bool PieceTool::confirm(const QString& question) const { return m_confirmer ? m_confirmer(question) : true; }

void PieceTool::onSelectionChanged(Viewport3D* viewport) {
    if (m_internalSelection) return;
    if (m_mode == Mode::Group || m_mode == Mode::Ungroup) {  // the selection stays until the RMB acts on it
        emit statusChanged(statusText(m_mode, 0, static_cast<int>(viewport->selection().count(cc::core::ElementKind::Bar))));
        return;
    }
    if (m_mode == Mode::None) return;
    const std::vector<int> ids = viewport->selection().ids(pickKind());
    if (ids.empty()) return;
    m_internalSelection = true;  // the pick is consumed: the selection is cleared so that the same element can be clicked again
    viewport->clearSelection();
    m_internalSelection = false;
    if (ids.size() != 1) return;  // a window that catches several elements does nothing
    if (pickKind() == cc::core::ElementKind::Node)
        pickNode(ids.front());
    else
        pickBar(ids.front());
}

void PieceTool::pickBar(int barId) {
    if (!m_project || !m_undo) return;
    if (m_mode == Mode::InvertAxis) {
        report(cc::core::invertBarAxis(*m_project, *m_undo, barId));
    } else if (m_mode == Mode::MoveEnd && m_moveStep == 0 && m_project->structure.findBar(barId)) {
        m_moveBar = barId;
        m_moveStep = 1;
        configureViewports();
        emit statusChanged(statusText(m_mode, m_moveStep));
    }
}

void PieceTool::pickNode(int nodeId) {
    if (!m_project || !m_undo) return;
    if (m_mode == Mode::Piece) {
        if (m_chain.empty()) {
            m_chain.push_back(nodeId);
        } else {
            if (m_chain.front() == nodeId) return;  // the same node twice: waits for another one
            const int start = m_chain.front();
            m_chain.clear();
            // #65126 exists in the DLL but whether Pieza asks it is NOT verified: off by default (setConfirmPiece).
            if (!m_confirmPiece || confirm(tr("¿Desea crear la pieza?"))) report(cc::core::createPiece(*m_project, *m_undo, start, nodeId));
        }
        for (const QPointer<Viewport3D>& viewport : m_viewports) viewport->setRubberChain(m_chain);
        emit statusChanged(statusText(m_mode, static_cast<int>(m_chain.size())));
    } else if (m_mode == Mode::MoveEnd) {
        const cc::core::Bar* bar = m_project->structure.findBar(m_moveBar);
        if (!bar) return resetSteps();
        if (m_moveStep == 1) {  // which end of the bar moves: one of its two nodes
            if (nodeId != bar->startNode && nodeId != bar->endNode) return;
            m_moveWhich = nodeId == bar->startNode ? cc::core::BarEnd::Start : cc::core::BarEnd::End;
            m_moveStep = 2;
            configureViewports();
            emit statusChanged(statusText(m_mode, m_moveStep));
        } else if (m_moveStep == 2) {
            const int barId = m_moveBar;
            const cc::core::PieceError error = cc::core::moveBarEnd(*m_project, *m_undo, barId, m_moveWhich, nodeId);
            report(error);  // refused: the command waits for another destination
            if (error == cc::core::PieceError::None) resetSteps();
        }
    }
}

void PieceTool::onActOnSelection(Viewport3D* viewport) {
    switch (m_mode) {
        case Mode::Group:
        case Mode::Ungroup: {
            const std::vector<int> bars = viewport->selection().ids(cc::core::ElementKind::Bar);
            if (bars.empty() || !m_project || !m_undo) return;
            const bool group = m_mode == Mode::Group;
            const QString question = group ? tr("¿Desea agrupar las piezas seleccionadas?") : tr("¿Desea desagrupar las piezas seleccionadas?");  // #65100 / #65101
            if (confirm(question))
                report(group ? cc::core::groupPieces(*m_project, *m_undo, bars) : cc::core::ungroupPieces(*m_project, *m_undo, bars));
            m_internalSelection = true;
            viewport->clearSelection();
            m_internalSelection = false;
            emit statusChanged(statusText(m_mode));
            break;
        }
        case Mode::Piece:
        case Mode::MoveEnd:
            if (!m_chain.empty() || m_moveStep > 0) resetSteps();  // "Volver al paso anterior"
            break;
        default: break;
    }
}

}  // namespace cc::app
