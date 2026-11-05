#include "core/continuous_edit.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <memory>
#include <map>
#include <set>
#include <tuple>

#include "core/piece_edit.h"

namespace cc::core {

namespace {

constexpr double kPi = 3.14159265358979323846;

/// First and last node of a piece in the sense of its local x axis (bar start to end, flipped by xAxisInverted).
/// False when the piece record is inconsistent with the model.
bool pieceEnds(const StructuralModel& m, const Piece& piece, int& first, int& last) {
    if (piece.barIds.empty() || piece.nodeIds.size() != piece.barIds.size() + 1) return false;
    const Bar* b = m.findBar(piece.barIds.front());
    if (!b) return false;
    const bool forward = b->startNode == piece.nodeIds.front();
    const bool along = forward != b->xAxisInverted;
    first = along ? piece.nodeIds.front() : piece.nodeIds.back();
    last = along ? piece.nodeIds.back() : piece.nodeIds.front();
    return true;
}

const Node* nodeOf(const StructuralModel& m, int id) { return m.findNode(id); }

MaterialKind kindOfBar(const StructuralModel& m, const Bar& bar) {
    const Material* mat = m.findMaterial(bar.materialId);
    return mat ? mat->kind : MaterialKind::Generic;
}

std::string angleText(double degrees) {
    char buf[32];
    std::snprintf(buf, sizeof buf, "%g", std::round(degrees * 100.0) / 100.0);
    return std::string(buf) + "°";
}

std::vector<int> sortedUnique(std::vector<int> v) {
    std::sort(v.begin(), v.end());
    v.erase(std::unique(v.begin(), v.end()), v.end());
    return v;
}

template <class T, class Less>
void insertSorted(std::vector<T>& v, const T& item, Less less) {
    v.insert(std::upper_bound(v.begin(), v.end(), item, less), item);
}

void setReferenceIn(ContinuousElements& c, const ReferenceTarget& t, const std::string& text) {
    switch (t.kind) {
    case ReferenceTarget::Kind::ContinuousColumn:
        for (ContinuousColumn& col : c.columns)
            if (col.id == t.id) col.reference = text;
        break;
    case ReferenceTarget::Kind::ContinuousBeam:
        for (ContinuousBeam& beam : c.beams)
            if (beam.id == t.id) beam.reference = text;
        break;
    case ReferenceTarget::Kind::Piece: {
        auto it = std::find_if(c.pieceReferences.begin(), c.pieceReferences.end(),
                               [&](const PieceReference& r) { return r.pieceId == t.id; });
        if (text.empty()) {
            if (it != c.pieceReferences.end()) c.pieceReferences.erase(it);
        } else if (it != c.pieceReferences.end()) {
            it->text = text;
        } else {
            insertSorted(c.pieceReferences, PieceReference{t.id, text},
                         [](const PieceReference& a, const PieceReference& b) { return a.pieceId < b.pieceId; });
        }
        break;
    }
    }
}

/// Anchor node (first node of the first piece) of a target, for ordering. Zero when unresolved.
int anchorNode(const Project& p, const ReferenceTarget& t) {
    const StructuralModel& m = p.structure;
    int pieceId = t.id;
    if (t.kind == ReferenceTarget::Kind::ContinuousColumn) {
        const ContinuousColumn* c = p.continuous.findColumn(t.id);
        if (!c || c->pieceIds.empty()) return 0;
        pieceId = c->pieceIds.front();
    } else if (t.kind == ReferenceTarget::Kind::ContinuousBeam) {
        const ContinuousBeam* b = p.continuous.findBeam(t.id);
        if (!b || b->pieceIds.empty()) return 0;
        pieceId = b->pieceIds.front();
    }
    const Piece* piece = m.findPiece(pieceId);
    int first = 0, last = 0;
    return piece && pieceEnds(m, *piece, first, last) ? first : 0;
}

std::vector<ReferenceTarget> sortedByAnchor(const Project& p, std::vector<ReferenceTarget> list, bool yFirst) {
    auto key = [&](const ReferenceTarget& t) {
        const Node* n = nodeOf(p.structure, anchorNode(p, t));
        return n ? (yFirst ? std::make_tuple(n->position.y, n->position.x, n->position.z) : std::make_tuple(n->position.x, n->position.y, n->position.z)) : std::make_tuple(0.0, 0.0, 0.0);
    };
    std::stable_sort(list.begin(), list.end(), [&](const ReferenceTarget& a, const ReferenceTarget& b) { return key(a) < key(b); });
    return list;
}

/// Splits "Pilar 07" into ("Pilar ", 7, width 2). False when the text does not end in digits.
bool splitReference(const std::string& ref, std::string& prefix, long long& number, std::size_t& width) {
    std::size_t end = ref.size();
    std::size_t begin = end;
    while (begin > 0 && ref[begin - 1] >= '0' && ref[begin - 1] <= '9') --begin;
    if (begin == end || end - begin > 9) return false;
    prefix = ref.substr(0, begin);
    number = std::stoll(ref.substr(begin));
    width = end - begin;
    return true;
}

}  // namespace

std::string continuousErrorMessage(ContinuousError error, const std::string& detail) {
    switch (error) {
    case ContinuousError::None: return {};
    case ContinuousError::UnknownElement: return "El elemento seleccionado no existe.";
    case ContinuousError::NotPillar: return "Alguna de las piezas entre los nudos seleccionados no es de tipo 'pilar'.";
    case ContinuousError::AlreadyExists: return "Ya existe un pilar con las mismas piezas.";
    case ContinuousError::IncompletePieces: return "Para crear el pilar debe seleccionar piezas completas.";
    case ContinuousError::NotAlignedConnected:
        return "Las piezas del pilar deben estar alineadas y conectadas entre sí a través de sus nudos extremos, de forma tal "
               "que el nudo final de una pieza sea el nudo inicial de la otra en el sentido de su directriz. La dirección x "
               "local de todas las piezas debe ser ascendente.";
    case ContinuousError::NotVertical: return "Alguna de las piezas no es vertical.";
    case ContinuousError::NotAscending: return "La directriz de alguna pieza no está orientada en dirección ascendente.";
    case ContinuousError::NotBeam: return "Alguna de las piezas seleccionadas no está definida como elemento estructural de tipo 'viga'.";
    case ContinuousError::NotConnected:
        return "Para poder crear una viga continua, las vigas seleccionadas deben estar conectadas entre sí de forma tal que "
               "el nudo final de una sea el inicial de la siguiente (en el sentido de su directriz).";
    case ContinuousError::MixedMaterial:
        return "Las piezas seleccionadas deben ser todas del mismo material (hormigón, acero laminado o acero conformado).";
    case ContinuousError::PlaneNotVertical: return "El plano xz de alguna de las piezas no está contenido en un plano vertical.";
    case ContinuousError::TooSteep:
        return "Alguna de las piezas de la viga continua forma un ángulo con el plano horizontal superior a " +
               (detail.empty() ? angleText(kMaxContinuousBeamAngleDeg) : detail) + ".";
    case ContinuousError::TooFewPieces: return "Debe seleccionar dos o más piezas distintas.";
    case ContinuousError::PieceInOtherElement: return "Alguna de las piezas ya forma parte de otro elemento continuo.";
    case ContinuousError::PartOfContinuous: return "La pieza forma parte de un elemento continuo: edite la referencia de éste.";
    case ContinuousError::NotPillarNode: return "El nudo no pertenece a ningún pilar.";
    case ContinuousError::NotBeamBars: return "Alguna de las barras no forma parte de una viga.";
    case ContinuousError::GroupConflict: return "Alguna de las barras ya pertenece a otro grupo de flecha.";
    case ContinuousError::InvalidReference: return "La referencia inicial debe terminar en un número.";
    case ContinuousError::EmptyReference: return "La referencia no puede quedar vacía.";
    case ContinuousError::ReferenceTooLong: return "La referencia no puede tener más de 30 caracteres.";
    case ContinuousError::InvalidValue: return "El valor introducido no es válido.";
    case ContinuousError::NoElements: return "No hay ningún elemento que numerar.";
    case ContinuousError::NothingToChange: return "La operación no modifica nada.";
    }
    return {};
}

// --- queries -------------------------------------------------------------------------------------------------------

std::vector<ReferenceTarget> listPillars(const Project& p) {
    std::vector<ReferenceTarget> list;
    for (const ContinuousColumn& c : p.continuous.columns) list.push_back({ReferenceTarget::Kind::ContinuousColumn, c.id});
    for (const Piece& piece : p.structure.pieces)
        if (piece.type == ElementType::Pillar && !p.continuous.columnOfPiece(piece.id))
            list.push_back({ReferenceTarget::Kind::Piece, piece.id});
    return sortedByAnchor(p, std::move(list), true);
}

std::vector<ReferenceTarget> listBeams(const Project& p) {
    std::vector<ReferenceTarget> list;
    for (const ContinuousBeam& b : p.continuous.beams) list.push_back({ReferenceTarget::Kind::ContinuousBeam, b.id});
    for (const Piece& piece : p.structure.pieces)
        if (piece.type == ElementType::Beam && !p.continuous.beamOfPiece(piece.id))
            list.push_back({ReferenceTarget::Kind::Piece, piece.id});
    return sortedByAnchor(p, std::move(list), false);
}

std::string elementReference(const Project& p, const ReferenceTarget& t, const std::string& prefix) {
    std::string explicitText;
    bool known = false;
    bool pillar = true;
    switch (t.kind) {
    case ReferenceTarget::Kind::ContinuousColumn:
        if (const ContinuousColumn* c = p.continuous.findColumn(t.id)) known = true, explicitText = c->reference;
        break;
    case ReferenceTarget::Kind::ContinuousBeam:
        pillar = false;
        if (const ContinuousBeam* b = p.continuous.findBeam(t.id)) known = true, explicitText = b->reference;
        break;
    case ReferenceTarget::Kind::Piece:
        if (const Piece* piece = p.structure.findPiece(t.id)) {
            known = piece->type == ElementType::Pillar || piece->type == ElementType::Beam;
            pillar = piece->type == ElementType::Pillar;
            if (const PieceReference* r = p.continuous.findPieceReference(t.id)) explicitText = r->text;
        }
        break;
    }
    if (!known) return {};
    if (!explicitText.empty()) return explicitText;
    const std::vector<ReferenceTarget> list = pillar ? listPillars(p) : listBeams(p);
    const auto it = std::find(list.begin(), list.end(), t);
    return it == list.end() ? std::string{} : prefix + std::to_string(it - list.begin() + 1);
}

std::vector<int> beamBarIds(const Project& p, const ReferenceTarget& t) {
    std::vector<int> pieces;
    if (t.kind == ReferenceTarget::Kind::ContinuousBeam) {
        if (const ContinuousBeam* b = p.continuous.findBeam(t.id)) pieces = b->pieceIds;
    } else if (t.kind == ReferenceTarget::Kind::Piece) {
        pieces = {t.id};
    }
    std::vector<int> bars;
    for (int id : pieces)
        if (const Piece* piece = p.structure.findPiece(id))
            if (piece->type == ElementType::Beam) bars.insert(bars.end(), piece->barIds.begin(), piece->barIds.end());
    return sortedUnique(bars);
}

std::optional<BeamDeflectionLimit> deflectionLimitOfBar(const Project& p, int barId) {
    if (const DeflectionGroup* g = p.continuous.groupOfBar(barId)) return g->limit;
    return std::nullopt;
}

std::optional<double> slabDepthAt(const Project& p, int nodeId) {
    if (const SlabDepth* d = p.continuous.findSlabDepth(nodeId)) return d->depthM;
    return std::nullopt;
}

std::string validateContinuous(const Project& p) {
    const ContinuousElements& c = p.continuous;
    const StructuralModel& m = p.structure;
    if (!c.isValid()) return "Los pilares y vigas continuos contienen valores no válidos.";
    for (const ContinuousColumn& col : c.columns)
        for (int id : col.pieceIds) {
            const Piece* piece = m.findPiece(id);
            if (!piece || piece->type != ElementType::Pillar) return "Un pilar continuo referencia una pieza que no es un pilar.";
        }
    for (const ContinuousBeam& b : c.beams)
        for (int id : b.pieceIds) {
            const Piece* piece = m.findPiece(id);
            if (!piece || piece->type != ElementType::Beam) return "Una viga continua referencia una pieza que no es una viga.";
        }
    for (const PieceReference& r : c.pieceReferences) {
        const Piece* piece = m.findPiece(r.pieceId);
        if (!piece || piece->type == ElementType::Generic) return "Una referencia pertenece a una pieza que no es pilar ni viga.";
    }
    for (const SlabDepth& d : c.slabDepths)
        if (!m.findNode(d.nodeId)) return "Un canto de forjado referencia un nudo que no existe.";
    for (const DeflectionGroup& g : c.deflectionGroups)
        for (int id : g.barIds)
            if (!m.findBar(id)) return "Un grupo de flecha referencia una barra que no existe.";
    return {};
}

// --- ContinuousDiffCommand -----------------------------------------------------------------------------------------

void ContinuousDiffCommand::capture(ContinuousElements after) {
    m_before = m_project.continuous;
    m_after = std::move(after);
}

void ContinuousDiffCommand::redo() {
    if (m_error != ContinuousError::None) return;
    ContinuousElements next = m_after;
    next.nextColumnId = std::max(next.nextColumnId, m_project.continuous.nextColumnId);
    next.nextBeamId = std::max(next.nextBeamId, m_project.continuous.nextBeamId);
    next.nextGroupId = std::max(next.nextGroupId, m_project.continuous.nextGroupId);
    m_project.continuous = std::move(next);
}

void ContinuousDiffCommand::undo() {
    if (m_error != ContinuousError::None) return;
    const ContinuousElements now = m_project.continuous;
    m_project.continuous = m_before;
    // ids are never reused
    m_project.continuous.nextColumnId = std::max(m_before.nextColumnId, now.nextColumnId);
    m_project.continuous.nextBeamId = std::max(m_before.nextBeamId, now.nextBeamId);
    m_project.continuous.nextGroupId = std::max(m_before.nextGroupId, now.nextGroupId);
}

// --- CreateContinuousColumnCommand ---------------------------------------------------------------------------------

CreateContinuousColumnCommand::CreateContinuousColumnCommand(Project& project, int startNode, int endNode)
    : ContinuousDiffCommand(project) {
    const StructuralModel& m = project.structure;
    const Node* s = m.findNode(startNode);
    const Node* e = m.findNode(endNode);
    if (!s || !e || startNode == endNode) {
        m_error = ContinuousError::UnknownElement;
        return;
    }
    const std::vector<int> chain = findAlignedChain(m, startNode, endNode);
    if (chain.empty()) {
        m_error = ContinuousError::NotAlignedConnected;
        return;
    }
    if (std::fabs(e->position.x - s->position.x) >= kVerticalToleranceM ||
        std::fabs(e->position.y - s->position.y) >= kVerticalToleranceM) {
        m_error = ContinuousError::NotVertical;
        return;
    }
    if (e->position.z <= s->position.z) {
        m_error = ContinuousError::NotAscending;
        return;
    }

    // Pieces along the chain, in order. A bar with no record is an implicit generic piece.
    std::vector<const Piece*> pieces;
    const std::set<int> inChain(chain.begin(), chain.end());
    bool implicit = false;
    for (int barId : chain) {
        const Piece* piece = m.pieceOfBar(barId);
        if (!piece) {
            implicit = true;
            continue;
        }
        if (std::find(pieces.begin(), pieces.end(), piece) == pieces.end()) pieces.push_back(piece);
    }
    for (const Piece* piece : pieces)
        for (int barId : piece->barIds)
            if (!inChain.count(barId)) {
                m_error = ContinuousError::IncompletePieces;
                return;
            }
    if (implicit || std::any_of(pieces.begin(), pieces.end(), [](const Piece* p) { return p->type != ElementType::Pillar; })) {
        m_error = ContinuousError::NotPillar;
        return;
    }
    for (const Piece* piece : pieces) {
        int first = 0, last = 0;
        const Node* a = pieceEnds(m, *piece, first, last) ? m.findNode(first) : nullptr;
        const Node* b = a ? m.findNode(last) : nullptr;
        if (!a || !b || b->position.z <= a->position.z) {
            m_error = ContinuousError::NotAscending;
            return;
        }
    }

    std::vector<int> ids;
    for (const Piece* piece : pieces) ids.push_back(piece->id);
    const ContinuousElements& cur = project.continuous;
    for (const ContinuousColumn& c : cur.columns)
        if (c.pieceIds == ids) {
            m_error = ContinuousError::AlreadyExists;
            return;
        }
    if (ids.size() < 2) {  // one pillar piece is already a pillar
        m_error = ContinuousError::AlreadyExists;
        return;
    }
    for (int id : ids)
        if (cur.columnOfPiece(id)) {
            m_error = ContinuousError::PieceInOtherElement;
            return;
        }

    ContinuousElements work = cur;
    ContinuousColumn col;
    col.id = work.nextColumnId++;
    col.pieceIds = ids;
    work.columns.push_back(col);  // ids increase, so the list stays sorted
    m_id = col.id;
    capture(std::move(work));
}

// --- CreateContinuousBeamCommand -----------------------------------------------------------------------------------

CreateContinuousBeamCommand::CreateContinuousBeamCommand(Project& project, const std::vector<int>& barIds)
    : ContinuousDiffCommand(project) {
    const StructuralModel& m = project.structure;
    if (barIds.empty()) {
        m_error = ContinuousError::UnknownElement;
        return;
    }
    // Per piece, in selection order, stopping at the first failure (REA, FUN_180068840): angle with the horizontal
    // (#64021), plane xz vertical (#64026), then the element type (#64023). A bar with no record is a generic piece.
    std::vector<const Piece*> pieces;
    int implicitCount = 0;
    std::set<int> seenImplicit;
    for (int barId : barIds) {
        if (!m.findBar(barId)) {
            m_error = ContinuousError::UnknownElement;
            return;
        }
        const Piece* piece = m.pieceOfBar(barId);
        if (piece && std::find(pieces.begin(), pieces.end(), piece) != pieces.end()) continue;
        if (!piece && !seenImplicit.insert(barId).second) continue;
        const std::vector<int> bars = piece ? piece->barIds : std::vector<int>{barId};
        for (int id : bars) {
            const Bar& b = *m.findBar(id);
            const Node* a = m.findNode(b.startNode);
            const Node* z = m.findNode(b.endNode);
            const double len = distance(a->position, z->position);
            const double angle = len > 0.0 ? std::asin(std::min(1.0, std::fabs(z->position.z - a->position.z) / len)) * 180.0 / kPi : 0.0;
            if (angle * kPi / 180.0 - kBeamAngleSlackRad >= kMaxContinuousBeamAngleDeg * kPi / 180.0) {
                m_error = ContinuousError::TooSteep;
                m_detail = angleText(kMaxContinuousBeamAngleDeg);
                return;
            }
            const double r = std::fmod(std::fabs(b.rollAngleDeg), 180.0);
            if (std::min(r, 180.0 - r) >= kPlaneVerticalToleranceDeg) {
                m_error = ContinuousError::PlaneNotVertical;
                return;
            }
        }
        if (!piece || piece->type != ElementType::Beam) {
            m_error = ContinuousError::NotBeam;
            return;
        }
        pieces.push_back(piece);
    }
    if (pieces.size() + static_cast<std::size_t>(implicitCount) < 2) {
        m_error = ContinuousError::TooFewPieces;
        return;
    }

    // Order along the directrix: exactly one piece starts where no other ends, and each next starts where one ends.
    struct Ends { const Piece* piece; int first, last; };
    std::vector<Ends> ends;
    for (const Piece* piece : pieces) {
        Ends e{piece, 0, 0};
        if (!pieceEnds(m, *piece, e.first, e.last)) {
            m_error = ContinuousError::UnknownElement;
            return;
        }
        ends.push_back(e);
    }
    std::vector<Ends> ordered;
    {
        std::vector<Ends> pending = ends;
        int starts = 0;
        std::size_t startIndex = 0;
        for (std::size_t i = 0; i < pending.size(); ++i) {
            const bool endsHere = std::any_of(pending.begin(), pending.end(), [&](const Ends& o) { return o.last == pending[i].first; });
            if (!endsHere) ++starts, startIndex = i;
        }
        if (starts != 1) {
            m_error = ContinuousError::NotConnected;
            return;
        }
        ordered.push_back(pending[startIndex]);
        pending.erase(pending.begin() + static_cast<long>(startIndex));
        while (!pending.empty()) {
            std::vector<std::size_t> next;
            for (std::size_t i = 0; i < pending.size(); ++i)
                if (pending[i].first == ordered.back().last) next.push_back(i);
            if (next.size() != 1) {
                m_error = ContinuousError::NotConnected;
                return;
            }
            ordered.push_back(pending[next[0]]);
            pending.erase(pending.begin() + static_cast<long>(next[0]));
        }
    }

    // Same material class.
    std::optional<MaterialKind> kind;
    for (const Ends& e : ordered)
        for (int barId : e.piece->barIds) {
            const MaterialKind k = kindOfBar(m, *m.findBar(barId));
            if (kind && *kind != k) {
                m_error = ContinuousError::MixedMaterial;
                return;
            }
            kind = k;
        }
    std::vector<int> ids;
    for (const Ends& e : ordered) ids.push_back(e.piece->id);
    const ContinuousElements& cur = project.continuous;
    for (const ContinuousBeam& b : cur.beams)
        if (b.pieceIds == ids) {
            m_error = ContinuousError::AlreadyExists;
            return;
        }
    for (int id : ids)
        if (cur.beamOfPiece(id)) {
            m_error = ContinuousError::PieceInOtherElement;
            return;
        }
    ContinuousElements work = cur;
    ContinuousBeam beam;
    beam.id = work.nextBeamId++;
    beam.pieceIds = ids;
    work.beams.push_back(beam);
    m_id = beam.id;
    capture(std::move(work));
}

// --- SetReferenceCommand -------------------------------------------------------------------------------------------

SetReferenceCommand::SetReferenceCommand(Project& project, const ReferenceTarget& target, const std::string& text)
    : ContinuousDiffCommand(project) {
    const ContinuousElements& cur = project.continuous;
    switch (target.kind) {
    case ReferenceTarget::Kind::ContinuousColumn:
        if (!cur.findColumn(target.id)) m_error = ContinuousError::UnknownElement;
        break;
    case ReferenceTarget::Kind::ContinuousBeam:
        if (!cur.findBeam(target.id)) m_error = ContinuousError::UnknownElement;
        break;
    case ReferenceTarget::Kind::Piece: {
        const Piece* piece = project.structure.findPiece(target.id);
        if (!piece || piece->type == ElementType::Generic) m_error = ContinuousError::UnknownElement;
        else if (cur.columnOfPiece(target.id) || cur.beamOfPiece(target.id)) m_error = ContinuousError::PartOfContinuous;
        break;
    }
    }
    if (m_error != ContinuousError::None) return;
    if (text.empty()) {
        m_error = ContinuousError::EmptyReference;
        return;
    }
    if (text.size() > kMaxReferenceLength) {
        m_error = ContinuousError::ReferenceTooLong;
        return;
    }
    ContinuousElements work = cur;
    setReferenceIn(work, target, text);
    if (work == cur) {
        m_error = ContinuousError::NothingToChange;
        return;
    }
    capture(std::move(work));
}

// --- NumberPillarsCommand ------------------------------------------------------------------------------------------

NumberPillarsCommand::NumberPillarsCommand(Project& project, const std::string& startReference) : ContinuousDiffCommand(project) {
    std::string prefix;
    long long number = 0;
    std::size_t width = 0;
    if (startReference.empty()) {
        m_error = ContinuousError::EmptyReference;
        return;
    }
    if (startReference.size() > kMaxReferenceLength) {
        m_error = ContinuousError::ReferenceTooLong;
        return;
    }
    if (!splitReference(startReference, prefix, number, width)) {
        m_error = ContinuousError::InvalidReference;
        return;
    }
    const std::vector<ReferenceTarget> pillars = listPillars(project);
    if (pillars.empty()) {
        m_error = ContinuousError::NoElements;
        return;
    }
    ContinuousElements work = project.continuous;
    for (const ReferenceTarget& t : pillars) {
        std::string digits = std::to_string(number++);
        if (digits.size() < width) digits.insert(0, width - digits.size(), '0');
        if (prefix.size() + digits.size() > kMaxReferenceLength) {
            m_error = ContinuousError::ReferenceTooLong;
            return;
        }
        setReferenceIn(work, t, prefix + digits);
    }
    if (work == project.continuous) {
        m_error = ContinuousError::NothingToChange;
        return;
    }
    m_count = static_cast<int>(pillars.size());
    capture(std::move(work));
}

// --- SetSlabDepthCommand -------------------------------------------------------------------------------------------

SetSlabDepthCommand::SetSlabDepthCommand(Project& project, int nodeId, std::optional<double> depthM)
    : ContinuousDiffCommand(project) {
    const StructuralModel& m = project.structure;
    if (!m.findNode(nodeId)) {
        m_error = ContinuousError::UnknownElement;
        return;
    }
    const bool onPillar = std::any_of(m.pieces.begin(), m.pieces.end(), [&](const Piece& p) {
        return p.type == ElementType::Pillar && std::find(p.nodeIds.begin(), p.nodeIds.end(), nodeId) != p.nodeIds.end();
    });
    if (!onPillar) {
        m_error = ContinuousError::NotPillarNode;
        return;
    }
    if (depthM && !(std::isfinite(*depthM) && *depthM > 0.0)) {
        m_error = ContinuousError::InvalidValue;
        return;
    }
    ContinuousElements work = project.continuous;
    auto it = std::find_if(work.slabDepths.begin(), work.slabDepths.end(), [&](const SlabDepth& d) { return d.nodeId == nodeId; });
    if (!depthM) {
        if (it != work.slabDepths.end()) work.slabDepths.erase(it);
    } else if (it != work.slabDepths.end()) {
        it->depthM = *depthM;
    } else {
        insertSorted(work.slabDepths, SlabDepth{nodeId, *depthM},
                     [](const SlabDepth& a, const SlabDepth& b) { return a.nodeId < b.nodeId; });
    }
    if (work == project.continuous) {
        m_error = ContinuousError::NothingToChange;
        return;
    }
    capture(std::move(work));
}

// --- SetDeflectionLimitCommand -------------------------------------------------------------------------------------

SetDeflectionLimitCommand::SetDeflectionLimitCommand(Project& project, const std::vector<int>& barIds, const BeamDeflectionLimit& limit)
    : ContinuousDiffCommand(project) {
    const StructuralModel& m = project.structure;
    const std::vector<int> bars = sortedUnique(barIds);
    if (bars.empty()) {
        m_error = ContinuousError::UnknownElement;
        return;
    }
    for (int id : bars) {
        if (!m.findBar(id)) {
            m_error = ContinuousError::UnknownElement;
            return;
        }
        const Piece* piece = m.pieceOfBar(id);
        if (!piece || piece->type != ElementType::Beam) {
            m_error = ContinuousError::NotBeamBars;
            return;
        }
    }
    if (!std::isfinite(limit.relative) || !std::isfinite(limit.absoluteMm) || limit.relative < 0.0 || limit.absoluteMm < 0.0) {
        m_error = ContinuousError::InvalidValue;
        return;
    }
    ContinuousElements work = project.continuous;
    const DeflectionGroup* existing = work.groupOfBar(bars.front());
    for (int id : bars) {
        const DeflectionGroup* g = work.groupOfBar(id);
        if (g != existing || (g && g->barIds != bars)) {
            m_error = ContinuousError::GroupConflict;
            return;
        }
    }
    if (existing && existing->barIds != bars) {
        m_error = ContinuousError::GroupConflict;
        return;
    }
    if (existing) {
        const int id = existing->id;
        auto it = std::find_if(work.deflectionGroups.begin(), work.deflectionGroups.end(), [&](const DeflectionGroup& g) { return g.id == id; });
        if (limit.isSet()) it->limit = limit;
        else work.deflectionGroups.erase(it);
    } else if (limit.isSet()) {
        DeflectionGroup g;
        g.id = work.nextGroupId++;
        g.barIds = bars;
        g.limit = limit;
        work.deflectionGroups.push_back(g);
    }
    if (work == project.continuous) {
        m_error = ContinuousError::NothingToChange;
        return;
    }
    capture(std::move(work));
}

// --- wrappers ------------------------------------------------------------------------------------------------------

namespace {

template <class Command>
ContinuousError pushCommand(UndoStack& stack, std::unique_ptr<Command> command) {
    if (!command->valid()) return command->error();
    stack.push(std::move(command));
    return ContinuousError::None;
}

}  // namespace

ContinuousError createContinuousColumn(Project& project, UndoStack& stack, int startNode, int endNode, int* columnId) {
    auto command = std::make_unique<CreateContinuousColumnCommand>(project, startNode, endNode);
    if (columnId) *columnId = command->valid() ? command->columnId() : 0;
    return pushCommand(stack, std::move(command));
}

ContinuousError createContinuousBeam(Project& project, UndoStack& stack, const std::vector<int>& barIds, int* beamId, std::string* detail) {
    auto command = std::make_unique<CreateContinuousBeamCommand>(project, barIds);
    if (beamId) *beamId = command->valid() ? command->beamId() : 0;
    if (detail) *detail = command->detail();
    return pushCommand(stack, std::move(command));
}

ContinuousError setReference(Project& project, UndoStack& stack, const ReferenceTarget& target, const std::string& text) {
    return pushCommand(stack, std::make_unique<SetReferenceCommand>(project, target, text));
}

ContinuousError numberPillars(Project& project, UndoStack& stack, const std::string& startReference, int* count) {
    auto command = std::make_unique<NumberPillarsCommand>(project, startReference);
    if (count) *count = command->valid() ? command->count() : 0;
    return pushCommand(stack, std::move(command));
}

ContinuousError setSlabDepth(Project& project, UndoStack& stack, int nodeId, std::optional<double> depthM) {
    return pushCommand(stack, std::make_unique<SetSlabDepthCommand>(project, nodeId, depthM));
}

ContinuousError setDeflectionLimit(Project& project, UndoStack& stack, const std::vector<int>& barIds, const BeamDeflectionLimit& limit) {
    return pushCommand(stack, std::make_unique<SetDeflectionLimitCommand>(project, barIds, limit));
}

}  // namespace cc::core
