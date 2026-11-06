#include "core/node_ligature.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <set>

#include "core/project.h"

namespace cc::core {

namespace {

constexpr const char* kDirName[3] = {"Dx", "Dy", "Dz"};

bool anyDirection(const LigatureDirections& d) { return d[0] || d[1] || d[2]; }

Ligature* findTie(std::vector<Ligature>& ties, int nodeId) {
    for (Ligature& t : ties)
        if (std::find(t.nodeIds.begin(), t.nodeIds.end(), nodeId) != t.nodeIds.end()) return &t;
    return nullptr;
}

void removeNodes(std::vector<Ligature>& ties, const std::vector<int>& nodeIds) {
    for (Ligature& t : ties)
        t.nodeIds.erase(std::remove_if(t.nodeIds.begin(), t.nodeIds.end(),
                                       [&](int id) { return std::find(nodeIds.begin(), nodeIds.end(), id) != nodeIds.end(); }),
                        t.nodeIds.end());
}

void pruneDegenerate(std::vector<Ligature>& ties) {
    ties.erase(std::remove_if(ties.begin(), ties.end(), [](const Ligature& t) { return t.nodeIds.size() < 2; }), ties.end());
}

/// REA FUN_180185340: does this direction need linking? No when it is not marked or when all the nodes already share one single tie.
bool needsLinking(const NodeLigatures& ligatures, int dir, const std::vector<int>& nodeIds) {
    std::set<int> distinct;
    for (int id : nodeIds) {
        const auto idx = ligatureIndexOf(ligatures, dir, id);
        if (!idx) return true;
        distinct.insert(*idx);
    }
    return distinct.size() != 1;
}

}  // namespace

std::optional<int> ligatureIndexOf(const NodeLigatures& ligatures, int dir, int nodeId) {
    if (dir < 0 || dir > 2) return std::nullopt;
    const auto& ties = ligatures.dirs[static_cast<std::size_t>(dir)];
    for (std::size_t i = 0; i < ties.size(); ++i)
        if (std::find(ties[i].nodeIds.begin(), ties[i].nodeIds.end(), nodeId) != ties[i].nodeIds.end()) return static_cast<int>(i);
    return std::nullopt;
}

bool hasLigature(const NodeLigatures& ligatures, int nodeId) {
    for (int d = 0; d < 3; ++d)
        if (ligatureIndexOf(ligatures, d, nodeId)) return true;
    return false;
}

std::string ligatureLabel(const NodeLigatures& ligatures, int nodeId) {
    std::string out;
    for (int d = 0; d < 3; ++d) {
        const auto idx = ligatureIndexOf(ligatures, d, nodeId);
        if (!idx) continue;
        if (!out.empty()) out += ", ";
        out += std::string(kDirName[d]) + "(" + std::to_string(*idx + 1) + ")";
    }
    return out;
}

bool isLigatureCapturable(const StructuralModel& model, const NodeSupports& supports, int nodeId) {
    if (!model.findNode(nodeId)) return false;
    const auto it = supports.external.find(nodeId);
    if (it == supports.external.end()) return true;
    const ExternalSupport& s = it->second;
    if (s.type == ExternalSupportType::Generic)
        return s.generic.mode == GenericDisplacementMode::Independent && restraintOf(s).oblique == Restraint::Oblique::None;
    const SupportKind kind = supportTypeInfo(s.type).kind;
    return kind != SupportKind::Line && kind != SupportKind::Plane;
}

bool isNodeInDiaphragm(int) { return false; }  // TODO(T-P6-04): diaphragms do not exist yet

std::string ligatureMessage(int messageId) {
    switch (messageId) {
        case 388777: return "No se ha seleccionado el tipo de ligadura a establecer.";
        case 64230: return "Los nudos seleccionados ya pertenecen a la misma ligadura.";
        case 64231: return "No es posible asignar ligaduras a nudos cuyos desplazamientos prescritos sean diferentes.";
        case 375639: return "No es posible asignar ligaduras a nudos que pertenecen a diafragmas";
        case 64200: return "No es posible asignar ligaduras a un nudo con vinculación exterior definida por una recta o por un plano.";
        case 64139: return "No es posible asignar ligaduras a nudos correspondientes a las conexiones.";
        default: return {};
    }
}

std::vector<int> ligatureSelection(const Project& project, const std::vector<int>& nodeIds) {
    std::vector<int> out;
    for (int id : nodeIds) {
        if (std::find(out.begin(), out.end(), id) != out.end()) continue;
        if (!isLigatureCapturable(project.structure, project.nodeSupports, id)) continue;
        out.push_back(id);
    }
    return out;
}

namespace {

LigatureCheck error(int id) { return {LigatureStatus::Error, id, ligatureMessage(id)}; }

/// Different prescribed values of the DOF `dof` among the nodes that store a prescribed set with that DOF blocked (per load case).
bool prescribedDiffer(const Project& project, const std::vector<int>& nodeIds, int dof) {
    std::map<int, double> reference;  // load case -> first value found
    for (int id : nodeIds) {
        const auto it = project.nodeSupports.external.find(id);
        if (it == project.nodeSupports.external.end()) continue;
        if (!restraintOf(it->second).fixed[static_cast<std::size_t>(dof)]) continue;
        for (const PrescribedDisplacement& p : it->second.prescribed) {
            const double v = p.values[static_cast<std::size_t>(dof)];
            const auto [pos, inserted] = reference.emplace(p.loadCaseId, v);
            if (!inserted && std::fabs(pos->second - v) > 1e-12) return true;
        }
    }
    return false;
}

}  // namespace

LigatureCheck checkLigatureAssign(const Project& project, const std::vector<int>& nodeIds, const LigatureDirections& directions) {
    if (!anyDirection(directions)) return error(388777);
    const std::vector<int> ids = ligatureSelection(project, nodeIds);
    if (ids.size() < 2) return {LigatureStatus::Silent, 0, {}};
    for (int id : ids)
        if (isNodeInDiaphragm(id)) return error(375639);
    std::array<bool, 3> link{};
    bool any = false;
    for (int d = 0; d < 3; ++d) {
        link[static_cast<std::size_t>(d)] = directions[static_cast<std::size_t>(d)] && needsLinking(project.ligatures, d, ids);
        any = any || link[static_cast<std::size_t>(d)];
    }
    if (!any) return error(64230);
    for (int d = 0; d < 3; ++d)
        if (link[static_cast<std::size_t>(d)] && prescribedDiffer(project, ids, d)) return error(64231);
    return {};
}

void applyLigatureAssign(NodeLigatures& ligatures, const std::vector<int>& nodeIds, const LigatureDirections& directions) {
    if (nodeIds.size() < 2) return;
    for (int d = 0; d < 3; ++d) {
        if (!directions[static_cast<std::size_t>(d)] || !needsLinking(ligatures, d, nodeIds)) continue;
        auto& ties = ligatures.dirs[static_cast<std::size_t>(d)];
        const int first = nodeIds.front();
        const std::vector<int> rest(nodeIds.begin() + 1, nodeIds.end());
        removeNodes(ties, rest);  // the representative stays where it is: only the others leave their ties
        if (Ligature* own = findTie(ties, first)) {
            for (int id : rest)
                if (std::find(own->nodeIds.begin(), own->nodeIds.end(), id) == own->nodeIds.end()) own->nodeIds.push_back(id);
        } else {
            Ligature t;
            t.id = ligatures.nextId++;
            t.nodeIds = nodeIds;
            ties.push_back(std::move(t));
        }
    }
}

void applyLigatureRemove(NodeLigatures& ligatures, const std::vector<int>& nodeIds, const LigatureDirections& directions) {
    if (!anyDirection(directions)) {
        for (auto& ties : ligatures.dirs) ties.clear();
        return;
    }
    for (int d = 0; d < 3; ++d) {
        if (!directions[static_cast<std::size_t>(d)]) continue;
        auto& ties = ligatures.dirs[static_cast<std::size_t>(d)];
        removeNodes(ties, nodeIds);
        pruneDegenerate(ties);
    }
}

void applyLigatureNodesDeleted(NodeLigatures& ligatures, const std::vector<int>& nodeIds) {
    for (auto& ties : ligatures.dirs) {
        removeNodes(ties, nodeIds);
        pruneDegenerate(ties);
    }
}

std::string validateLigatures(const NodeLigatures& ligatures, const StructuralModel& model) {
    if (ligatures.nextId < 1) return "ligaduras: identificador siguiente no válido";
    std::set<int> allIds;
    for (int d = 0; d < 3; ++d) {
        std::set<int> used;
        for (const Ligature& t : ligatures.dirs[static_cast<std::size_t>(d)]) {
            if (t.id < 1 || t.id >= ligatures.nextId || !allIds.insert(t.id).second) return "ligaduras: identificador no válido o repetido";
            if (t.nodeIds.empty()) return "ligaduras: ligadura sin nudos";
            for (int n : t.nodeIds) {
                if (!model.findNode(n)) return "ligaduras: nudo inexistente";
                if (!used.insert(n).second) return "ligaduras: nudo en más de una ligadura de la misma dirección";
            }
        }
    }
    return {};
}

}  // namespace cc::core
