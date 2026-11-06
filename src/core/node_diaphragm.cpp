#include "core/node_diaphragm.h"

#include <algorithm>
#include <cmath>
#include <set>

#include "core/project.h"

namespace cc::core {

namespace {

void removeFrom(std::vector<int>& v, const std::vector<int>& ids) {
    v.erase(std::remove_if(v.begin(), v.end(), [&](int n) { return std::find(ids.begin(), ids.end(), n) != ids.end(); }), v.end());
}

Diaphragm* mutableDiaphragm(Diaphragms& d, int id) {
    for (Diaphragm& x : d.items)
        if (x.id == id) return &x;
    return nullptr;
}

}  // namespace

const Diaphragm* findDiaphragm(const Diaphragms& d, int id) {
    for (const Diaphragm& x : d.items)
        if (x.id == id) return &x;
    return nullptr;
}

const Diaphragm* diaphragmOfNode(const Diaphragms& d, int nodeId) {
    for (const Diaphragm& x : d.items)
        if (std::find(x.nodeIds.begin(), x.nodeIds.end(), nodeId) != x.nodeIds.end()) return &x;
    return nullptr;
}

bool isNodeInDiaphragm(const Diaphragms& d, int nodeId) { return diaphragmOfNode(d, nodeId) != nullptr; }

std::string diaphragmNodeLabel(const Diaphragms& d, int nodeId) {
    const Diaphragm* x = diaphragmOfNode(d, nodeId);
    return x ? x->reference : std::string{};
}

std::string diaphragmTooltipLine(const Diaphragms& d, int nodeId) {
    const Diaphragm* x = diaphragmOfNode(d, nodeId);
    return x ? "Diafragma : " + x->reference : std::string{};
}

std::optional<double> diaphragmElevation(const Diaphragm& diaphragm, const StructuralModel& model) {
    double sum = 0.0;
    int count = 0;
    for (int id : diaphragm.nodeIds)
        if (const Node* n = model.findNode(id)) {
            sum += n->position.z;
            ++count;
        }
    if (count == 0) return std::nullopt;
    return sum / count;
}

bool diaphragmNodesSameElevation(const Diaphragm& diaphragm, const StructuralModel& model) {
    const Node* previous = nullptr;
    for (int id : diaphragm.nodeIds) {
        const Node* n = model.findNode(id);
        if (!n) continue;
        if (previous && std::fabs(n->position.z - previous->position.z) > kDiaphragmSameElevationM) return false;
        previous = n;
    }
    return true;
}

std::optional<double> diaphragmBelow(const std::vector<double>& elevations, double z) {
    std::optional<double> best;
    for (double e : elevations)
        if (e + 1e-5 < z && (!best || e > *best)) best = e;
    return best;
}

std::optional<double> diaphragmAbove(const std::vector<double>& elevations, double z) {
    std::optional<double> best;
    for (double e : elevations)
        if (z < e - 1e-5 && (!best || e < *best)) best = e;
    return best;
}

std::string diaphragmMessage(int messageId, const std::string& argument) {
    switch (messageId) {
        case 184611: return "No se ha definido ningún diafragma.";
        case 375635: return "Se borrarán los diafragmas previamente definidos. ¿Desea continuar?";
        case 375636: return "No se ha generado ningún diafragma";
        case 375637: return "Se ha generado un diafragma";
        case 375638: return "Se han generado " + argument + " diafragmas";
        case 375640: return "No es posible asignar un nudo con vinculación exterior a un diafragma.";
        case 375642: return "No todos los elementos del diafragma están contenidos en el mismo plano horizontal.";
        case 375648: return "La referencia '" + argument + "' está repetida.";
        case 375658: return "No ha introducido la referencia de todos los diafragmas introducidos.";
        case 404575: return "No es posible asignar a un diafragma un nudo con ligaduras en alguno de sus grados de libertad.";
        default: return {};
    }
}

std::string diaphragmEmptyReferenceMessage() { return "La referencia no puede quedar vacía."; }
std::string diaphragmNoneDefinedMessage() { return diaphragmMessage(184611); }
std::string diaphragmNoLevelsMessage() { return "No se ha definido ningún nivel"; }

Diaphragm newDiaphragm() { return {}; }

Diaphragm duplicateDiaphragm(const Diaphragm& source) {
    Diaphragm out;
    out.reference = source.reference;
    out.color = source.color;
    out.loads = source.loads;
    return out;
}

DiaphragmLoad defaultDiaphragmLoad(int firstLoadCaseId) {
    DiaphragmLoad l;
    l.loadCaseId = firstLoadCaseId;
    return l;
}

std::optional<DiaphragmIssue> checkDiaphragmAdd(const std::vector<Diaphragm>& rows) {
    for (const Diaphragm& r : rows)
        if (r.reference.empty()) return DiaphragmIssue{0, diaphragmEmptyReferenceMessage()};
    return std::nullopt;
}

std::optional<DiaphragmIssue> checkDiaphragmAccept(const std::vector<Diaphragm>& rows) {
    for (const Diaphragm& r : rows)
        if (r.reference.empty()) return DiaphragmIssue{375658, diaphragmMessage(375658)};
    std::set<std::string> seen;
    for (const Diaphragm& r : rows)
        if (!seen.insert(r.reference).second) return DiaphragmIssue{375648, diaphragmMessage(375648, r.reference)};
    return std::nullopt;
}

std::vector<int> diaphragmSelection(const Project& project, const std::vector<int>& nodeIds) {
    std::vector<int> out;
    for (int id : nodeIds)
        if (project.structure.findNode(id) && std::find(out.begin(), out.end(), id) == out.end()) out.push_back(id);
    return out;
}

DiaphragmCheck checkDiaphragmAssign(const Project& project, int diaphragmId, const std::vector<int>& nodeIds) {
    const std::vector<int> ids = diaphragmSelection(project, nodeIds);
    if (ids.empty() || !findDiaphragm(project.diaphragms, diaphragmId)) return {DiaphragmStatus::Silent, 0, {}};
    for (int id : ids)
        if (isSupported(project.structure, project.nodeSupports, id)) return {DiaphragmStatus::Error, 375640, diaphragmMessage(375640)};
    for (int id : ids)
        if (hasLigature(project.ligatures, id)) return {DiaphragmStatus::Error, 404575, diaphragmMessage(404575)};
    return {};
}

void applyDiaphragmAssign(Diaphragms& d, int diaphragmId, const std::vector<int>& nodeIds) {
    Diaphragm* target = mutableDiaphragm(d, diaphragmId);
    if (!target) return;
    for (Diaphragm& x : d.items)
        if (x.id != diaphragmId) removeFrom(x.nodeIds, nodeIds);
    for (int id : nodeIds)
        if (std::find(target->nodeIds.begin(), target->nodeIds.end(), id) == target->nodeIds.end()) target->nodeIds.push_back(id);
    std::sort(target->nodeIds.begin(), target->nodeIds.end());
}

void applyDiaphragmUnassign(Diaphragms& d, const std::vector<int>& nodeIds) {
    for (Diaphragm& x : d.items) removeFrom(x.nodeIds, nodeIds);
}

void applyDiaphragmNodesDeleted(Diaphragms& d, const std::vector<int>& nodeIds) { applyDiaphragmUnassign(d, nodeIds); }

std::vector<Diaphragm> planDiaphragmsByLevels(const Project& project, int firstId) {
    std::vector<Diaphragm> out;
    int next = firstId;
    for (const Level& level : sortedLevels(project.workPlanes.levels)) {
        Diaphragm d;
        for (const Node& n : project.structure.nodes) {
            if (std::fabs(n.position.z - level.elevation) > kDiaphragmSameElevationM) continue;
            if (isSupported(project.structure, project.nodeSupports, n.id) || hasLigature(project.ligatures, n.id)) continue;
            d.nodeIds.push_back(n.id);
        }
        if (d.nodeIds.empty()) continue;
        std::sort(d.nodeIds.begin(), d.nodeIds.end());
        d.id = next++;
        d.reference = "D" + std::to_string(out.size() + 1);
        out.push_back(std::move(d));
    }
    return out;
}

std::string validateDiaphragms(const Diaphragms& d, const StructuralModel& model) {
    if (d.nextId < 1) return "diafragmas: identificador siguiente no válido";
    std::set<int> ids, nodes;
    for (const Diaphragm& x : d.items) {
        if (x.id < 1 || x.id >= d.nextId || !ids.insert(x.id).second) return "diafragmas: identificador no válido o repetido";
        if (x.color > 0xFFFFFF) return "diafragmas: color no válido";
        for (int n : x.nodeIds) {
            (void)model;  // nodes that no longer exist are tolerated (never saved)
            if (!nodes.insert(n).second) return "diafragmas: nudo en más de un diafragma";
        }
    }
    return {};
}

}  // namespace cc::core
