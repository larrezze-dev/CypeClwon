#include "core/structure_model.h"

#include <algorithm>
#include <cmath>

namespace cc::core {

namespace {

constexpr double kMinBarLengthM = 1e-6;
constexpr double kMaxCoordinateM = 1e6;

template <class T>
const T* findById(const std::vector<T>& items, int id) {
    auto it = std::lower_bound(items.begin(), items.end(), id, [](const T& item, int value) { return item.id < value; });
    return it != items.end() && it->id == id ? &*it : nullptr;
}

template <class T>
T* findById(std::vector<T>& items, int id) {
    return const_cast<T*>(findById(static_cast<const std::vector<T>&>(items), id));
}

/// Inserts keeping the vector sorted; false when the id is already present.
template <class T>
bool insertSorted(std::vector<T>& items, const T& item) {
    auto it = std::lower_bound(items.begin(), items.end(), item.id, [](const T& a, int value) { return a.id < value; });
    if (it != items.end() && it->id == item.id) return false;
    items.insert(it, item);
    return true;
}

template <class T>
bool eraseById(std::vector<T>& items, int id) {
    auto it = std::lower_bound(items.begin(), items.end(), id, [](const T& item, int value) { return item.id < value; });
    if (it == items.end() || it->id != id) return false;
    items.erase(it);
    return true;
}

bool finite(const Point3& p) {
    return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z) && std::fabs(p.x) <= kMaxCoordinateM &&
           std::fabs(p.y) <= kMaxCoordinateM && std::fabs(p.z) <= kMaxCoordinateM;
}

bool positiveFinite(double v) { return std::isfinite(v) && v > 0.0; }
bool nonNegativeFinite(double v) { return std::isfinite(v) && v >= 0.0; }

template <class T>
bool strictlySorted(const std::vector<T>& items) {
    for (std::size_t i = 0; i < items.size(); ++i) {
        if (items[i].id <= 0) return false;
        if (i > 0 && items[i - 1].id >= items[i].id) return false;
    }
    return true;
}

template <class T>
bool counterAhead(const std::vector<T>& items, int next) {
    return next > 0 && (items.empty() || next > items.back().id);
}

bool validShellNodeCount(std::size_t n) { return n == 3 || n == 4; }

}  // namespace

double distance(const Point3& a, const Point3& b) { return std::sqrt((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y) + (a.z - b.z) * (a.z - b.z)); }

double Material::shearModulusMPa() const { return elasticModulusMPa / (2.0 * (1.0 + poissonRatio)); }

const Node* StructuralModel::findNode(int id) const { return findById(nodes, id); }
const Material* StructuralModel::findMaterial(int id) const { return findById(materials, id); }
const CrossSection* StructuralModel::findSection(int id) const { return findById(sections, id); }
const Bar* StructuralModel::findBar(int id) const { return findById(bars, id); }
const Shell* StructuralModel::findShell(int id) const { return findById(shells, id); }
Node* StructuralModel::findNode(int id) { return findById(nodes, id); }
Bar* StructuralModel::findBar(int id) { return findById(bars, id); }
Shell* StructuralModel::findShell(int id) { return findById(shells, id); }

int StructuralModel::addNode(const Point3& position) {
    if (!finite(position) || nextNodeId <= 0) return 0;
    Node node{nextNodeId, position};
    if (!insertSorted(nodes, node)) return 0;
    return nextNodeId++;
}

int StructuralModel::addMaterial(Material material) {
    if (!positiveFinite(material.elasticModulusMPa) || !std::isfinite(material.poissonRatio) || material.poissonRatio <= -1.0 ||
        material.poissonRatio >= 0.5 || !positiveFinite(material.densityKnM3) || !std::isfinite(material.thermalExpansion))
        return 0;
    material.id = nextMaterialId;
    if (!insertSorted(materials, material)) return 0;
    return nextMaterialId++;
}

int StructuralModel::addSection(CrossSection section) {
    if (!positiveFinite(section.areaM2) || !nonNegativeFinite(section.inertiaYM4) || !nonNegativeFinite(section.inertiaZM4) ||
        !nonNegativeFinite(section.torsionM4) || !nonNegativeFinite(section.shearAreaYM2) || !nonNegativeFinite(section.shearAreaZM2))
        return 0;
    section.id = nextSectionId;
    if (!insertSorted(sections, section)) return 0;
    return nextSectionId++;
}

int StructuralModel::addBar(int startNode, int endNode, int materialId, int sectionId) {
    Bar bar;
    bar.id = nextBarId;
    bar.startNode = startNode;
    bar.endNode = endNode;
    bar.materialId = materialId;
    bar.sectionId = sectionId;
    if (!insertBar(bar)) return 0;
    return nextBarId - 1;
}

int StructuralModel::addShell(std::vector<int> nodeIds, double thicknessM, int materialId) {
    Shell shell;
    shell.id = nextShellId;
    shell.nodeIds = std::move(nodeIds);
    shell.thicknessM = thicknessM;
    shell.materialId = materialId;
    if (!insertShell(shell)) return 0;
    return nextShellId - 1;
}

bool StructuralModel::insertNode(const Node& node) {
    if (node.id <= 0 || !finite(node.position)) return false;
    if (!insertSorted(nodes, node)) return false;
    nextNodeId = std::max(nextNodeId, node.id + 1);
    return true;
}

bool StructuralModel::insertBar(const Bar& bar) {
    if (bar.id <= 0 || bar.startNode == bar.endNode || !std::isfinite(bar.rollAngleDeg)) return false;
    const Node* a = findNode(bar.startNode);
    const Node* b = findNode(bar.endNode);
    if (!a || !b || distance(a->position, b->position) < kMinBarLengthM) return false;
    if (bar.materialId != 0 && !findMaterial(bar.materialId)) return false;
    if (bar.sectionId != 0 && !findSection(bar.sectionId)) return false;
    if (!insertSorted(bars, bar)) return false;
    nextBarId = std::max(nextBarId, bar.id + 1);
    return true;
}

bool StructuralModel::insertShell(const Shell& shell) {
    if (shell.id <= 0 || !validShellNodeCount(shell.nodeIds.size()) || !positiveFinite(shell.thicknessM)) return false;
    for (std::size_t i = 0; i < shell.nodeIds.size(); ++i) {
        if (!findNode(shell.nodeIds[i])) return false;
        for (std::size_t j = 0; j < i; ++j)
            if (shell.nodeIds[i] == shell.nodeIds[j]) return false;
    }
    if (shell.materialId != 0 && !findMaterial(shell.materialId)) return false;
    if (!insertSorted(shells, shell)) return false;
    nextShellId = std::max(nextShellId, shell.id + 1);
    return true;
}

bool StructuralModel::removeBar(int id) { return eraseById(bars, id); }
bool StructuralModel::removeShell(int id) { return eraseById(shells, id); }

bool StructuralModel::removeNode(int id) {
    if (!findNode(id) || !barsUsingNode(id).empty() || !shellsUsingNode(id).empty()) return false;
    return eraseById(nodes, id);
}

bool StructuralModel::removeMaterial(int id) {
    if (!findMaterial(id)) return false;
    for (const Bar& b : bars)
        if (b.materialId == id) return false;
    for (const Shell& s : shells)
        if (s.materialId == id) return false;
    return eraseById(materials, id);
}

bool StructuralModel::removeSection(int id) {
    if (!findSection(id)) return false;
    for (const Bar& b : bars)
        if (b.sectionId == id) return false;
    return eraseById(sections, id);
}

std::vector<int> StructuralModel::barsUsingNode(int nodeId) const {
    std::vector<int> out;
    for (const Bar& b : bars)
        if (b.startNode == nodeId || b.endNode == nodeId) out.push_back(b.id);
    return out;
}

std::vector<int> StructuralModel::shellsUsingNode(int nodeId) const {
    std::vector<int> out;
    for (const Shell& s : shells)
        if (std::find(s.nodeIds.begin(), s.nodeIds.end(), nodeId) != s.nodeIds.end()) out.push_back(s.id);
    return out;
}

double StructuralModel::barLength(int barId) const {
    const Bar* bar = findBar(barId);
    if (!bar) return 0.0;
    const Node* a = findNode(bar->startNode);
    const Node* b = findNode(bar->endNode);
    return a && b ? distance(a->position, b->position) : 0.0;
}

bool StructuralModel::empty() const { return nodes.empty() && materials.empty() && sections.empty() && bars.empty() && shells.empty(); }

std::string StructuralModel::validate() const {
    if (!strictlySorted(nodes) || !strictlySorted(materials) || !strictlySorted(sections) || !strictlySorted(bars) ||
        !strictlySorted(shells))
        return "Los identificadores de nudos, materiales, secciones, barras o láminas están duplicados o no son válidos.";
    if (!counterAhead(nodes, nextNodeId) || !counterAhead(materials, nextMaterialId) || !counterAhead(sections, nextSectionId) ||
        !counterAhead(bars, nextBarId) || !counterAhead(shells, nextShellId))
        return "El contador de identificadores del modelo es incoherente.";

    for (const Node& n : nodes)
        if (!finite(n.position)) return "El nudo " + std::to_string(n.id) + " tiene coordenadas no válidas.";

    for (const Material& m : materials) {
        if (!positiveFinite(m.elasticModulusMPa) || !std::isfinite(m.poissonRatio) || m.poissonRatio <= -1.0 || m.poissonRatio >= 0.5 ||
            !positiveFinite(m.densityKnM3) || !std::isfinite(m.thermalExpansion))
            return "El material " + std::to_string(m.id) + " tiene propiedades no válidas.";
    }
    for (const CrossSection& s : sections) {
        if (!positiveFinite(s.areaM2) || !nonNegativeFinite(s.inertiaYM4) || !nonNegativeFinite(s.inertiaZM4) ||
            !nonNegativeFinite(s.torsionM4) || !nonNegativeFinite(s.shearAreaYM2) || !nonNegativeFinite(s.shearAreaZM2))
            return "La sección " + std::to_string(s.id) + " tiene propiedades no válidas.";
    }
    for (const Bar& b : bars) {
        const std::string name = "La barra " + std::to_string(b.id);
        if (b.startNode == b.endNode) return name + " une un nudo consigo mismo.";
        const Node* a = findNode(b.startNode);
        const Node* e = findNode(b.endNode);
        if (!a || !e) return name + " hace referencia a un nudo que no existe.";
        if (distance(a->position, e->position) < kMinBarLengthM) return name + " tiene longitud nula.";
        if (b.materialId != 0 && !findMaterial(b.materialId)) return name + " hace referencia a un material que no existe.";
        if (b.sectionId != 0 && !findSection(b.sectionId)) return name + " hace referencia a una sección que no existe.";
        if (!std::isfinite(b.rollAngleDeg)) return name + " tiene un ángulo de giro no válido.";
    }
    for (const Shell& s : shells) {
        const std::string name = "La lámina " + std::to_string(s.id);
        if (!validShellNodeCount(s.nodeIds.size())) return name + " debe tener 3 o 4 nudos.";
        for (std::size_t i = 0; i < s.nodeIds.size(); ++i) {
            if (!findNode(s.nodeIds[i])) return name + " hace referencia a un nudo que no existe.";
            for (std::size_t j = 0; j < i; ++j)
                if (s.nodeIds[i] == s.nodeIds[j]) return name + " repite un nudo.";
        }
        if (!positiveFinite(s.thicknessM)) return name + " tiene un espesor no válido.";
        if (s.materialId != 0 && !findMaterial(s.materialId)) return name + " hace referencia a un material que no existe.";
    }
    return {};
}

}  // namespace cc::core
