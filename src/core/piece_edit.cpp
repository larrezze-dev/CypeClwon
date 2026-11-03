#include "core/piece_edit.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <utility>

#include "core/bar_edit.h"

namespace cc::core {

namespace {

Point3 minus(const Point3& a, const Point3& b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
double dot(const Point3& a, const Point3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }

/// Metres along the line s->e of node `p` and its distance to that line.
struct LinePosition {
    double along;
    double offLine;
};

LinePosition locate(const Point3& s, const Point3& e, const Point3& p) {
    const Point3 d = minus(e, s);
    const double len = std::sqrt(dot(d, d));
    const Point3 v = minus(p, s);
    const Point3 c{v.y * d.z - v.z * d.y, v.z * d.x - v.x * d.z, v.x * d.y - v.y * d.x};
    return {dot(v, d) / len, std::sqrt(dot(c, c)) / len};
}

/// Keeps the piece list tidy: a record that adds nothing over the implicit one (one generic ungrouped bar) is dropped.
void tidy(StructuralModel& m) {
    std::map<int, int> members;
    for (const Piece& p : m.pieces)
        if (p.groupId != 0) ++members[p.groupId];
    for (Piece& p : m.pieces)
        if (p.groupId != 0 && members[p.groupId] < 2) p.groupId = 0;  // a group needs two pieces
    m.pieces.erase(std::remove_if(m.pieces.begin(), m.pieces.end(),
                                  [](const Piece& p) {
                                      return p.barIds.size() == 1 && p.type == ElementType::Generic && p.groupId == 0;
                                  }),
                   m.pieces.end());
}

/// Id of the record of the piece that holds `barId`, creating the record of an implicit piece. 0 for an unknown bar.
int ensurePiece(StructuralModel& m, int barId) {
    if (const Piece* p = m.pieceOfBar(barId)) return p->id;
    const Bar* b = m.findBar(barId);
    if (!b) return 0;
    Piece p;
    p.id = m.nextPieceId;
    p.barIds = {barId};
    p.nodeIds = {b->startNode, b->endNode};
    m.insertPiece(p);
    return p.id;
}

Piece* mutablePiece(StructuralModel& m, int id) {
    for (Piece& p : m.pieces)
        if (p.id == id) return &p;
    return nullptr;
}

/// Splits a multi-bar piece record into one record per bar (type and group kept); the first keeps the id.
void splitPiece(StructuralModel& m, int pieceId) {
    const Piece* found = m.findPiece(pieceId);
    if (!found) return;
    const Piece old = *found;
    m.removePiece(pieceId);
    for (std::size_t i = 0; i < old.barIds.size(); ++i) {
        Piece part;
        part.id = i == 0 ? old.id : m.nextPieceId;
        part.barIds = {old.barIds[i]};
        part.nodeIds = {old.nodeIds[i], old.nodeIds[i + 1]};
        part.type = old.type;
        part.groupId = old.groupId;
        m.insertPiece(part);
    }
}

}  // namespace

std::vector<int> findAlignedChain(const StructuralModel& model, int startNode, int endNode) {
    const Node* s = model.findNode(startNode);
    const Node* e = model.findNode(endNode);
    if (!s || !e || startNode == endNode) return {};
    const double total = distance(s->position, e->position);
    if (total <= 0.0) return {};
    std::vector<int> chain;
    int current = startNode;
    double currentAlong = 0.0;
    while (current != endNode) {
        int bestBar = 0;
        int bestNode = 0;
        double bestAlong = total + kPieceAlignmentToleranceM;
        for (const Bar& b : model.bars) {
            if (b.startNode != current && b.endNode != current) continue;
            const int other = b.startNode == current ? b.endNode : b.startNode;
            const LinePosition pos = locate(s->position, e->position, model.findNode(other)->position);
            if (pos.offLine > kPieceAlignmentToleranceM) continue;
            if (pos.along <= currentAlong + kPieceAlignmentToleranceM || pos.along > total + kPieceAlignmentToleranceM) continue;
            if (pos.along < bestAlong || bestBar == 0) {
                bestAlong = pos.along;
                bestBar = b.id;
                bestNode = other;
            }
        }
        if (bestBar == 0) return {};
        chain.push_back(bestBar);
        current = bestNode;
        currentAlong = bestAlong;
    }
    return chain;
}

std::string pieceErrorMessage(PieceError error) {
    switch (error) {
    case PieceError::None: return {};
    case PieceError::EquivalentExists: return "Ya existe una pieza equivalente.";
    case PieceError::NoAlignedGroup:
        return "No se ha encontrado un grupo de barras alineadas que conecte los dos nudos seleccionados.";
    case PieceError::DifferentArrangement: return "Las barras seleccionadas no tienen la misma disposición (giro y enrase).";
    case PieceError::UnknownElement: return "El elemento seleccionado no existe.";
    case PieceError::TooFewPieces: return "Debe seleccionar dos o más piezas distintas.";
    case PieceError::NothingToChange: return "La operación no modifica ninguna pieza.";
    case PieceError::InvalidMove: return "No se puede mover el extremo de la barra a ese nudo.";
    }
    return {};
}

// --- PieceDiffCommand -------------------------------------------------------------------------------------------

void PieceDiffCommand::capture(const StructuralModel& edited) {
    const StructuralModel& now = m_project.structure;
    for (const Bar& after : edited.bars) {
        const Bar* before = now.findBar(after.id);
        if (before && !(*before == after)) {
            m_barsBefore.push_back(*before);
            m_barsAfter.push_back(after);
        }
    }
    m_piecesBefore = now.pieces;
    m_piecesAfter = edited.pieces;
    m_nextPieceBefore = now.nextPieceId;
    m_nextPieceAfter = edited.nextPieceId;
}

void PieceDiffCommand::redo() {
    if (m_error != PieceError::None) return;
    StructuralModel& m = m_project.structure;
    for (const Bar& b : m_barsAfter)
        if (Bar* target = m.findBar(b.id)) *target = b;
    m.pieces = m_piecesAfter;
    m.nextPieceId = std::max(m.nextPieceId, m_nextPieceAfter);
}

void PieceDiffCommand::undo() {
    if (m_error != PieceError::None) return;
    StructuralModel& m = m_project.structure;
    for (const Bar& b : m_barsBefore)
        if (Bar* target = m.findBar(b.id)) *target = b;
    m.pieces = m_piecesBefore;
    m.nextPieceId = std::max(m.nextPieceId, m_nextPieceBefore);  // ids are never reused
}

// --- CreatePieceCommand -----------------------------------------------------------------------------------------

CreatePieceCommand::CreatePieceCommand(Project& project, int startNode, int endNode) : PieceDiffCommand(project) {
    StructuralModel work = project.structure;
    const std::vector<int> chain = findAlignedChain(work, startNode, endNode);
    if (chain.empty()) {
        m_error = PieceError::NoAlignedGroup;
        return;
    }
    const std::set<int> inChain(chain.begin(), chain.end());

    // Pieces touched by the chain: all must lie entirely inside it (partial overlap is provisional NoAlignedGroup).
    std::set<int> absorbed;
    for (int barId : chain)
        if (const Piece* p = work.pieceOfBar(barId)) absorbed.insert(p->id);
    for (int id : absorbed)
        for (int barId : work.findPiece(id)->barIds)
            if (!inChain.count(barId)) {
                m_error = PieceError::NoAlignedGroup;
                return;
            }

    // The same piece already exists: a lone bar with no record is already an implicit piece.
    if (chain.size() == 1 && absorbed.empty()) {
        m_error = PieceError::EquivalentExists;
        return;
    }
    if (absorbed.size() == 1 && work.findPiece(*absorbed.begin())->barIds == chain) {
        m_error = PieceError::EquivalentExists;
        return;
    }

    // Same arrangement: equal roll angle and equal direction of the local x axis along the chain.
    std::vector<int> nodes{startNode};
    bool firstSet = false;
    double roll = 0.0;
    bool direction = false;
    int current = startNode;
    for (int barId : chain) {
        const Bar& b = *work.findBar(barId);
        const bool forward = b.startNode == current;
        const bool along = forward != b.xAxisInverted;
        if (!firstSet) {
            roll = b.rollAngleDeg;
            direction = along;
            firstSet = true;
        } else if (b.rollAngleDeg != roll || along != direction) {
            m_error = PieceError::DifferentArrangement;
            return;
        }
        current = forward ? b.endNode : b.startNode;
        nodes.push_back(current);
    }

    ElementType type = ElementType::Generic;
    for (int id : absorbed)
        if (type == ElementType::Generic) type = work.findPiece(id)->type;
    for (int id : absorbed) work.removePiece(id);
    Piece piece;
    piece.id = work.nextPieceId;
    piece.barIds = chain;
    piece.nodeIds = nodes;
    piece.type = type;
    work.insertPiece(piece);
    m_pieceId = piece.id;
    tidy(work);
    capture(work);
}

// --- SetPieceTypeCommand ----------------------------------------------------------------------------------------

SetPieceTypeCommand::SetPieceTypeCommand(Project& project, const std::vector<int>& barIds, ElementType type)
    : PieceDiffCommand(project) {
    StructuralModel work = project.structure;
    if (barIds.empty()) {
        m_error = PieceError::UnknownElement;
        return;
    }
    for (int barId : barIds) {
        const int id = ensurePiece(work, barId);
        if (id == 0) {
            m_error = PieceError::UnknownElement;
            return;
        }
        mutablePiece(work, id)->type = type;
    }
    tidy(work);
    if (work.pieces == project.structure.pieces) {
        m_error = PieceError::NothingToChange;
        return;
    }
    capture(work);
}

// --- GroupPiecesCommand -----------------------------------------------------------------------------------------

GroupPiecesCommand::GroupPiecesCommand(Project& project, const std::vector<int>& barIds) : PieceDiffCommand(project) {
    StructuralModel work = project.structure;
    std::vector<int> ids;  // distinct pieces, in selection order
    for (int barId : barIds) {
        const int id = ensurePiece(work, barId);
        if (id == 0) {
            m_error = PieceError::UnknownElement;
            return;
        }
        if (std::find(ids.begin(), ids.end(), id) == ids.end()) ids.push_back(id);
    }
    if (ids.size() < 2) {
        m_error = PieceError::TooFewPieces;
        return;
    }
    // Profile and material of the first selected piece (provisional): taken from its first bar.
    const Bar& reference = *work.findBar(work.findPiece(ids.front())->barIds.front());
    const int material = reference.materialId;
    const int section = reference.sectionId;

    std::set<int> oldGroups;
    for (int id : ids)
        if (int g = work.findPiece(id)->groupId; g != 0) oldGroups.insert(g);
    std::set<int> members(ids.begin(), ids.end());
    for (const Piece& p : work.pieces)
        if (oldGroups.count(p.groupId)) members.insert(p.id);

    m_groupId = *members.begin();
    for (int id : members) {
        Piece* p = mutablePiece(work, id);
        p->groupId = m_groupId;
        for (int barId : p->barIds) {
            Bar* b = work.findBar(barId);
            b->materialId = material;
            b->sectionId = section;
        }
    }
    tidy(work);
    // Compare after tidy: everything already grouped with the same description changes nothing.
    if (work.pieces == project.structure.pieces && work.bars == project.structure.bars) {
        m_error = PieceError::NothingToChange;
        m_groupId = 0;
        return;
    }
    capture(work);
}

// --- UngroupPiecesCommand ---------------------------------------------------------------------------------------

UngroupPiecesCommand::UngroupPiecesCommand(Project& project, const std::vector<int>& barIds) : PieceDiffCommand(project) {
    StructuralModel work = project.structure;
    if (barIds.empty()) {
        m_error = PieceError::UnknownElement;
        return;
    }
    for (int barId : barIds) {
        if (!work.findBar(barId)) {
            m_error = PieceError::UnknownElement;
            return;
        }
        if (const Piece* p = work.pieceOfBar(barId)) mutablePiece(work, p->id)->groupId = 0;
    }
    tidy(work);
    if (work.pieces == project.structure.pieces) {
        m_error = PieceError::NothingToChange;
        return;
    }
    capture(work);
}

// --- MoveBarEndCommand ------------------------------------------------------------------------------------------

MoveBarEndCommand::MoveBarEndCommand(Project& project, int barId, BarEnd end, int newNode) : PieceDiffCommand(project) {
    StructuralModel work = project.structure;
    Bar* bar = work.findBar(barId);
    if (!bar || !work.findNode(newNode)) {
        m_error = PieceError::UnknownElement;
        return;
    }
    int& moving = end == BarEnd::Start ? bar->startNode : bar->endNode;
    const int fixed = end == BarEnd::Start ? bar->endNode : bar->startNode;
    if (newNode == moving || newNode == fixed ||
        distance(work.findNode(newNode)->position, work.findNode(fixed)->position) < kMinBarLengthM) {
        m_error = PieceError::InvalidMove;
        return;
    }
    for (const Bar& other : work.bars)
        if (other.id != barId && ((other.startNode == newNode && other.endNode == fixed) ||
                                  (other.endNode == newNode && other.startNode == fixed))) {
            m_error = PieceError::InvalidMove;
            return;
        }
    moving = newNode;
    if (const Piece* p = work.pieceOfBar(barId)) {
        if (p->barIds.size() > 1) splitPiece(work, p->id);
        if (const Piece* single = work.pieceOfBar(barId))
            mutablePiece(work, single->id)->nodeIds = {bar->startNode, bar->endNode};
    }
    tidy(work);
    capture(work);
}

// --- InvertBarAxisCommand ---------------------------------------------------------------------------------------

InvertBarAxisCommand::InvertBarAxisCommand(Project& project, int barId) : PieceDiffCommand(project) {
    StructuralModel work = project.structure;
    if (!work.findBar(barId)) {
        m_error = PieceError::UnknownElement;
        return;
    }
    std::vector<int> bars{barId};
    if (const Piece* p = work.pieceOfBar(barId)) {
        Piece* piece = mutablePiece(work, p->id);
        bars = piece->barIds;
        std::reverse(piece->barIds.begin(), piece->barIds.end());
        std::reverse(piece->nodeIds.begin(), piece->nodeIds.end());
    }
    for (int id : bars) {
        Bar* b = work.findBar(id);
        std::swap(b->startNode, b->endNode);
    }
    capture(work);
}

// --- wrappers ---------------------------------------------------------------------------------------------------

namespace {

template <class Command>
PieceError push(UndoStack& stack, std::unique_ptr<Command> cmd) {
    const PieceError error = cmd->error();
    if (error == PieceError::None) stack.push(std::move(cmd));
    return error;
}

}  // namespace

PieceError createPiece(Project& project, UndoStack& stack, int startNode, int endNode, int* pieceId) {
    auto cmd = std::make_unique<CreatePieceCommand>(project, startNode, endNode);
    if (pieceId) *pieceId = cmd->pieceId();
    return push(stack, std::move(cmd));
}

PieceError setPieceType(Project& project, UndoStack& stack, const std::vector<int>& barIds, ElementType type) {
    return push(stack, std::make_unique<SetPieceTypeCommand>(project, barIds, type));
}

PieceError groupPieces(Project& project, UndoStack& stack, const std::vector<int>& barIds, int* groupId) {
    auto cmd = std::make_unique<GroupPiecesCommand>(project, barIds);
    if (groupId) *groupId = cmd->groupId();
    return push(stack, std::move(cmd));
}

PieceError ungroupPieces(Project& project, UndoStack& stack, const std::vector<int>& barIds) {
    return push(stack, std::make_unique<UngroupPiecesCommand>(project, barIds));
}

PieceError moveBarEnd(Project& project, UndoStack& stack, int barId, BarEnd end, int newNode) {
    return push(stack, std::make_unique<MoveBarEndCommand>(project, barId, end, newNode));
}

PieceError invertBarAxis(Project& project, UndoStack& stack, int barId) {
    return push(stack, std::make_unique<InvertBarAxisCommand>(project, barId));
}

}  // namespace cc::core
