#include "core/structure_model.h"

#include <algorithm>
#include <set>
#include <cctype>
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
        !positiveFinite(material.densityKnM3) || !std::isfinite(material.thermalExpansion))
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
    bar.layerIds = {activeLayerId()};
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
    shell.layerIds = {activeLayerId()};
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

bool StructuralModel::removeBar(int id) {
    if (!findBar(id)) return false;
    // The bar leaves its piece; the stretches left on each side become pieces of their own (the first keeps the id).
    if (const Piece* found = pieceOfBar(id)) {
        const Piece old = *found;
        removePiece(old.id);
        const auto at = static_cast<std::size_t>(std::find(old.barIds.begin(), old.barIds.end(), id) - old.barIds.begin());
        const std::pair<std::size_t, std::size_t> runs[2] = {{0, at}, {at + 1, old.barIds.size()}};
        bool first = true;
        for (const auto& [from, to] : runs) {
            if (from >= to) continue;
            Piece part;
            part.id = first ? old.id : nextPieceId++;
            first = false;
            part.barIds.assign(old.barIds.begin() + from, old.barIds.begin() + to);
            part.nodeIds.assign(old.nodeIds.begin() + from, old.nodeIds.begin() + to + 1);
            part.type = old.type;
            part.groupId = old.groupId;
            if (part.barIds.size() > 1 || part.type != ElementType::Generic || part.groupId != 0) insertPiece(part);
        }
    }
    return eraseById(bars, id);
}

const Piece* StructuralModel::findPiece(int id) const {
    const auto it = std::lower_bound(pieces.begin(), pieces.end(), id, [](const Piece& p, int v) { return p.id < v; });
    return it != pieces.end() && it->id == id ? &*it : nullptr;
}

const Piece* StructuralModel::pieceOfBar(int barId) const {
    for (const Piece& p : pieces)
        if (std::find(p.barIds.begin(), p.barIds.end(), barId) != p.barIds.end()) return &p;
    return nullptr;
}

bool StructuralModel::insertPiece(const Piece& piece) {
    if (piece.id <= 0 || piece.barIds.empty() || piece.nodeIds.size() != piece.barIds.size() + 1) return false;
    const auto it = std::lower_bound(pieces.begin(), pieces.end(), piece.id, [](const Piece& p, int v) { return p.id < v; });
    if (it != pieces.end() && it->id == piece.id) return false;
    pieces.insert(it, piece);
    nextPieceId = std::max(nextPieceId, piece.id + 1);
    return true;
}

bool StructuralModel::removePiece(int id) {
    const auto it = std::lower_bound(pieces.begin(), pieces.end(), id, [](const Piece& p, int v) { return p.id < v; });
    if (it == pieces.end() || it->id != id) return false;
    pieces.erase(it);
    return true;
}
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

namespace {
bool sameName(const std::string& a, const std::string& b) {
    return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(), [](unsigned char x, unsigned char y) {
               return std::tolower(x) == std::tolower(y);
           });
}
}  // namespace

const Layer* StructuralModel::findLayer(int id) const {
    for (const Layer& l : layers)
        if (l.id == id) return &l;
    return nullptr;
}

Layer* StructuralModel::findLayer(int id) {
    return const_cast<Layer*>(static_cast<const StructuralModel*>(this)->findLayer(id));
}

bool StructuralModel::layerNameTaken(const std::string& name, int exceptId) const {
    for (const Layer& l : layers)
        if (l.id != exceptId && sameName(l.name, name)) return true;
    return false;
}

int StructuralModel::addLayer(const std::string& name, unsigned color) {
    if (name.empty() || layerNameTaken(name, 0)) return 0;
    Layer layer;
    layer.id = nextLayerId++;
    layer.name = name;
    layer.color = color & 0xFFFFFF;
    layers.push_back(layer);
    return layer.id;
}

std::string StructuralModel::suggestedLayerName() const {
    for (std::size_t n = layers.size() + 1;; ++n) {
        const std::string name = "Nueva capa " + std::to_string(n);
        if (!layerNameTaken(name, 0)) return name;
    }
}

namespace {
/// Inserts `id` into a sorted unique vector.
void addSorted(std::vector<int>& ids, int id) {
    auto it = std::lower_bound(ids.begin(), ids.end(), id);
    if (it == ids.end() || *it != id) ids.insert(it, id);
}
bool contains(const std::vector<int>& ids, int id) { return std::binary_search(ids.begin(), ids.end(), id); }
void eraseId(std::vector<int>& ids, int id) { ids.erase(std::remove(ids.begin(), ids.end(), id), ids.end()); }
}  // namespace

int StructuralModel::layerUsage(int id) const {
    int count = 0;
    for (const Bar& b : bars) count += contains(b.layerIds, id);
    for (const Shell& s : shells) count += contains(s.layerIds, id);
    return count;
}

bool StructuralModel::removeLayer(int id, LayerRemoval mode, int moveTo) {
    if (!findLayer(id) || layers.size() < 2) return false;
    if (mode == LayerRemoval::MoveElements && (moveTo == id || !findLayer(moveTo))) return false;
    const bool wasActive = findLayer(id)->active;

    auto detach = [&](std::vector<int>& ids) {  // returns true when the element had only this layer
        if (!contains(ids, id)) return false;
        eraseId(ids, id);
        if (!ids.empty()) return false;
        return true;
    };
    std::vector<int> orphanBars, orphanShells;
    for (Bar& b : bars)
        if (detach(b.layerIds)) {
            if (mode == LayerRemoval::MoveElements) b.layerIds = {moveTo};
            else orphanBars.push_back(b.id);
        }
    for (Shell& s : shells)
        if (detach(s.layerIds)) {
            if (mode == LayerRemoval::MoveElements) s.layerIds = {moveTo};
            else orphanShells.push_back(s.id);
        }
    // Elements that kept other layers and were also asked to move keep just those layers, which is what the dialog
    // promises ("mover" only concerns the elements left without a layer).
    for (int barId : orphanBars) removeBar(barId);
    for (int shellId : orphanShells) removeShell(shellId);

    layers.erase(std::remove_if(layers.begin(), layers.end(), [id](const Layer& l) { return l.id == id; }), layers.end());
    if (wasActive) setActiveLayer(layers.front().id);
    return true;
}

bool StructuralModel::setActiveLayer(int id) {
    if (!findLayer(id)) return false;
    for (Layer& l : layers) l.active = l.id == id;
    return true;
}

bool StructuralModel::renameLayer(int id, const std::string& name) {
    Layer* l = findLayer(id);
    if (!l || name.empty() || layerNameTaken(name, id)) return false;
    l->name = name;
    return true;
}

bool StructuralModel::moveLayer(int id, int direction) {
    auto it = std::find_if(layers.begin(), layers.end(), [id](const Layer& l) { return l.id == id; });
    if (it == layers.end() || (direction != -1 && direction != 1)) return false;
    if (direction < 0 ? it == layers.begin() : it + 1 == layers.end()) return false;
    std::iter_swap(it, it + direction);
    return true;
}

int StructuralModel::activeLayerId() const {
    for (const Layer& l : layers)
        if (l.active) return l.id;
    return layers.empty() ? kDefaultLayerId : layers.front().id;
}

bool StructuralModel::assignLayers(const std::vector<int>& barIds, const std::vector<int>& shellIds,
                                   const std::vector<int>& layerIds, LayerOperation operation) {
    if (barIds.empty() && shellIds.empty()) return false;
    if (operation != LayerOperation::Remove && layerIds.empty()) return false;
    for (int id : layerIds)
        if (!findLayer(id)) return false;
    for (int id : barIds)
        if (!findBar(id)) return false;
    for (int id : shellIds)
        if (!findShell(id)) return false;

    std::vector<int> wanted = layerIds;
    std::sort(wanted.begin(), wanted.end());
    wanted.erase(std::unique(wanted.begin(), wanted.end()), wanted.end());
    const int fallback = activeLayerId();
    auto apply = [&](std::vector<int>& ids) {
        switch (operation) {
            case LayerOperation::Assign:
                for (int id : wanted) addSorted(ids, id);
                break;
            case LayerOperation::Replace:
                ids = wanted;
                break;
            case LayerOperation::Remove:
                for (int id : wanted) eraseId(ids, id);
                if (ids.empty()) ids = {fallback};
                break;
        }
    };
    for (int id : barIds) apply(findBar(id)->layerIds);
    for (int id : shellIds) apply(findShell(id)->layerIds);
    return true;
}

std::string StructuralModel::layerName(int id) const {
    const Layer* l = findLayer(id);
    if (!l && !layers.empty()) l = &layers.front();
    return l ? l->name : kDefaultLayerName;
}

std::string StructuralModel::layerNames(const std::vector<int>& ids) const {
    std::string out;
    for (std::size_t i = 0; i < ids.size(); ++i) {
        if (i > 0) out += i + 1 == ids.size() ? " y " : ", ";
        out += layerName(ids[i]);
    }
    return out;
}

bool StructuralModel::elementVisible(const std::vector<int>& ids) const {
    for (int id : ids) {
        const Layer* l = findLayer(id);
        if (!l || l->visible) return true;
    }
    return ids.empty();
}

bool StructuralModel::elementLocked(const std::vector<int>& ids) const {
    for (int id : ids) {
        const Layer* l = findLayer(id);
        if (l && l->locked) return true;
    }
    return false;
}

bool StructuralModel::elementRendered(const std::vector<int>& ids) const {
    for (int id : ids) {
        const Layer* l = findLayer(id);
        if (l && l->rendered) return true;
    }
    return false;
}

unsigned StructuralModel::elementColor(const std::vector<int>& ids) const {
    for (const Layer& l : layers)
        if (contains(ids, l.id)) return l.color;
    return layers.empty() ? 0u : layers.front().color;
}

double StructuralModel::barLength(int barId) const {
    const Bar* bar = findBar(barId);
    if (!bar) return 0.0;
    const Node* a = findNode(bar->startNode);
    const Node* b = findNode(bar->endNode);
    return a && b ? distance(a->position, b->position) : 0.0;
}

bool StructuralModel::empty() const { return nodes.empty() && materials.empty() && sections.empty() && bars.empty() && shells.empty(); }

std::string StructuralModel::checkLayerIds(const std::vector<int>& ids) const {
    if (ids.empty()) return "no pertenece a ninguna capa.";
    for (std::size_t i = 0; i < ids.size(); ++i) {
        if (!findLayer(ids[i])) return "hace referencia a una capa que no existe.";
        if (i > 0 && ids[i - 1] >= ids[i]) return "tiene las capas desordenadas o repetidas.";
    }
    return {};
}

std::string StructuralModel::validate() const {
    if (!strictlySorted(nodes) || !strictlySorted(materials) || !strictlySorted(sections) || !strictlySorted(bars) ||
        !strictlySorted(shells))
        return "Los identificadores de nudos, materiales, secciones, barras o láminas están duplicados o no son válidos.";
    if (!counterAhead(nodes, nextNodeId) || !counterAhead(materials, nextMaterialId) || !counterAhead(sections, nextSectionId) ||
        !counterAhead(bars, nextBarId) || !counterAhead(shells, nextShellId))
        return "El contador de identificadores del modelo es incoherente.";

    if (layers.empty()) return "Debe definir, al menos, una capa.";
    {
        int active = 0;
        for (std::size_t i = 0; i < layers.size(); ++i) {
            const Layer& l = layers[i];
            if (l.id <= 0 || l.id >= nextLayerId || l.name.empty() || l.color > 0xFFFFFF)
                return "La capa " + std::to_string(l.id) + " no es válida.";
            for (std::size_t j = 0; j < i; ++j)
                if (layers[j].id == l.id || sameName(layers[j].name, l.name))
                    return "La capa " + std::to_string(l.id) + " está duplicada.";
            if (l.active) ++active;
        }
        if (active != 1) return "Debe haber exactamente una capa activa.";
    }
    for (const Node& n : nodes)
        if (!finite(n.position)) return "El nudo " + std::to_string(n.id) + " tiene coordenadas no válidas.";

    for (const Material& m : materials) {
        if (!positiveFinite(m.elasticModulusMPa) || !std::isfinite(m.poissonRatio) || m.poissonRatio <= -1.0 ||
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
        if (const std::string e = checkLayerIds(b.layerIds); !e.empty()) return name + " " + e;
        if (!std::isfinite(b.rollAngleDeg)) return name + " tiene un ángulo de giro no válido.";
    }
    {
        std::set<int> used;
        int previous = 0;
        for (const Piece& p : pieces) {
            const std::string name = "La pieza " + std::to_string(p.id);
            if (p.id <= previous || p.id >= nextPieceId) return "Los identificadores de piezas no son válidos.";
            previous = p.id;
            if (p.barIds.empty() || p.nodeIds.size() != p.barIds.size() + 1) return name + " no es coherente.";
            if (p.groupId < 0) return name + " tiene un grupo no válido.";
            for (std::size_t i = 0; i < p.barIds.size(); ++i) {
                const Bar* b = findBar(p.barIds[i]);
                if (!b) return name + " hace referencia a una barra que no existe.";
                if (!used.insert(p.barIds[i]).second) return "La barra " + std::to_string(b->id) + " está en más de una pieza.";
                const bool forward = b->startNode == p.nodeIds[i] && b->endNode == p.nodeIds[i + 1];
                const bool backward = b->endNode == p.nodeIds[i] && b->startNode == p.nodeIds[i + 1];
                if (!forward && !backward) return name + " no está conectada por sus nudos.";
            }
        }
    }
    for (const Shell& s : shells) {
        const std::string name = "La lámina " + std::to_string(s.id);
        if (!validShellNodeCount(s.nodeIds.size())) return name + " debe tener 3 o 4 nudos.";
        for (std::size_t i = 0; i < s.nodeIds.size(); ++i) {
            if (!findNode(s.nodeIds[i])) return name + " hace referencia a un nudo que no existe.";
            for (std::size_t j = 0; j < i; ++j)
                if (s.nodeIds[i] == s.nodeIds[j]) return name + " repite un nudo.";
        }
        if (const std::string e = checkLayerIds(s.layerIds); !e.empty()) return name + " " + e;
        if (!positiveFinite(s.thicknessM)) return name + " tiene un espesor no válido.";
        if (s.materialId != 0 && !findMaterial(s.materialId)) return name + " hace referencia a un material que no existe.";
    }
    return {};
}

}  // namespace cc::core
