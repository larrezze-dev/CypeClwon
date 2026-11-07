#include "core/node_diaphragm.h"

#include <algorithm>
#include <cmath>
#include <map>
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

int diaphragmLevelColorId(int i) { return (i < 24 ? 12 : 11) + (i % 24) * 10; }

unsigned idColorToRgb(int idColor) {
    if (idColor == 13) return kDefaultDiaphragmColor;  // measured: idcolor_colorRGB(0xd) = magenta
    // Measured in the original (diafragmas.md section 10.2): level i = 1..8 has idColor 12 + 10 i = 22..92.
    if (idColor >= 22 && idColor <= 92 && (idColor - 12) % 10 == 0) {
        static constexpr unsigned kMeasured[8] = {0xCC3300, 0xCC6600, 0xCC9900, 0xCCCC00, 0x99CC00, 0x66CC00, 0x33CC00, 0x00CC00};
        return kMeasured[(idColor - 12) / 10 - 1];
    }
    // NOT MEASURED (levels beyond the eighth): provisional hue ramp, to be replaced when the original is measured.
    const double hue = std::fmod(idColor * 15.0, 360.0) / 60.0;
    const double x = 1.0 - std::fabs(std::fmod(hue, 2.0) - 1.0);
    double r = 0, g = 0, b = 0;
    switch (static_cast<int>(hue)) {
        case 0: r = 1; g = x; break;
        case 1: r = x; g = 1; break;
        case 2: g = 1; b = x; break;
        case 3: g = x; b = 1; break;
        case 4: r = x; b = 1; break;
        default: r = 1; b = x; break;
    }
    auto ch = [](double v) { return static_cast<unsigned>(std::lround(v * 255.0)); };
    return (ch(r) << 16) | (ch(g) << 8) | ch(b);
}

std::vector<Diaphragm> planDiaphragmsByLevels(const Project& project, int firstId) {
    // Sort by increasing elevation with tolerance (insertion sort: stable, and safe with a tolerance comparator).
    std::vector<Level> levels;
    for (const Level& l : project.workPlanes.levels) {
        auto pos = levels.end();
        while (pos != levels.begin() && (pos - 1)->elevation - l.elevation > kDiaphragmSameElevationM) --pos;
        levels.insert(pos, l);
    }
    std::vector<Diaphragm> out;
    std::map<std::string, int> repeats;
    int next = firstId;
    const std::size_t n = levels.size();
    for (std::size_t i = 1; i < n; ++i) {
        const Level& level = levels[n - i];
        Diaphragm d;
        d.id = next++;
        const int k = repeats[level.name]++;
        d.reference = k == 0 ? level.name : level.name + "_" + std::to_string(k);
        d.color = idColorToRgb(diaphragmLevelColorId(static_cast<int>(i)));
        auto atLevel = [&](int nodeId) {
            const Node* node = project.structure.findNode(nodeId);
            return node && std::fabs(node->position.z - level.elevation) <= kDiaphragmSameElevationM;
        };
        for (const Node& node : project.structure.nodes)
            if (atLevel(node.id)) d.nodeIds.push_back(node.id);
        for (const Shell& shell : project.structure.shells)
            if (!shell.nodeIds.empty() && std::all_of(shell.nodeIds.begin(), shell.nodeIds.end(), atLevel)) d.shellIds.push_back(shell.id);
        std::sort(d.nodeIds.begin(), d.nodeIds.end());
        std::sort(d.shellIds.begin(), d.shellIds.end());
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
