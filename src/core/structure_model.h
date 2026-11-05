#pragma once
#include <string>
#include <vector>

namespace cc::core {

/// Geometric and mechanical model of a structure (T-P3-01): nodes, materials, cross sections, bars and shells.
///
/// Conventions
///  - Lengths in metres, angles in degrees, stresses in MPa, densities in kN/m3, thicknesses in metres. The display
///    unit ("Unidades de acotación") only affects the user interface, never this model.
///  - Every object has a positive integer id, unique inside its collection. Ids are never reused: the next free id is
///    stored in the model, so references (and undo history) stay valid after deletions.
///  - Collections are kept sorted by id (all mutators preserve that), so lookup is a binary search and the saved
///    files are deterministic.
///  - Material and cross-section fields are provisional: they hold what the solver needs and will be completed when
///    the profile libraries (T-P5-05, T-P5-06) are verified against the original application (see TK-021).

struct Point3 {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    bool operator==(const Point3&) const = default;
};

/// Distance between two points, in metres.
double distance(const Point3& a, const Point3& b);

struct Node {
    int id = 0;
    Point3 position;
};

enum class MaterialKind { Steel, Concrete, Timber, Aluminium, Generic };

struct Material {
    int id = 0;
    std::string name;
    MaterialKind kind = MaterialKind::Generic;
    double elasticModulusMPa = 210000.0;
    double poissonRatio = 0.3;
    double densityKnM3 = 78.5;
    double thermalExpansion = 1.2e-5;  ///< 1/ºC

    /// Shear modulus G = E / (2 (1 + nu)), in MPa.
    double shearModulusMPa() const;
    bool operator==(const Material&) const = default;
};

struct CrossSection {
    int id = 0;
    std::string name;
    MaterialKind kind = MaterialKind::Generic;  ///< material family the section is designed for
    double areaM2 = 0.0;
    double inertiaYM4 = 0.0;      ///< about the local y axis
    double inertiaZM4 = 0.0;      ///< about the local z axis
    double torsionM4 = 0.0;       ///< St. Venant torsion constant
    double shearAreaYM2 = 0.0;    ///< 0 = ignore shear deformation
    double shearAreaZM2 = 0.0;
    bool operator==(const CrossSection&) const = default;
};

/// Layer ("etiqueta" in the original application), verified in the original (docs/research/ui/etiquetas_panel.md).
/// An element can belong to several layers at once; the model always holds at least one layer and no layer is special
/// (the first one is just called "por defecto"). `active` marks the layer that receives newly created elements (only one
/// at a time); `visible` hides the elements of the layer, `locked` forbids editing them and `rendered` (the cube
/// column) switches the layer from the wireframe view to the rendered one. Elements are drawn with the colour of the
/// layer.
struct Layer {
    int id = 0;
    std::string name;
    unsigned color = 0x000000;  ///< 0xRRGGBB
    bool active = false;
    bool visible = true;
    bool locked = false;
    bool rendered = false;
    bool operator==(const Layer&) const = default;
};

inline constexpr int kDefaultLayerId = 1;
inline constexpr const char* kDefaultLayerName = "por defecto";

/// What to do with the elements of a layer that is being removed.
enum class LayerRemoval { MoveElements, DeleteElements };

/// Operation of the "Asignar elementos a etiquetas" dialog.
enum class LayerOperation { Assign, Replace, Remove };

/// Straight bar between two nodes. materialId / sectionId 0 mean "not assigned yet" (a freshly drawn bar).
struct Bar {
    int id = 0;
    int startNode = 0;
    int endNode = 0;
    int materialId = 0;
    int sectionId = 0;
    double rollAngleDeg = 0.0;   ///< rotation of the local axes about the bar axis (x)
    bool xAxisInverted = false;  ///< "invertir eje x": swaps the start and end of the local axes
    std::vector<int> layerIds{kDefaultLayerId};  ///< sorted, unique, never empty
    bool operator==(const Bar&) const = default;
};

/// Structural element type mark of a piece ("Tipo de elemento estructural", docs/research/ui/pieza.md section 6).
enum class ElementType { Generic, Pillar, Beam };

/// Piece ("Pieza", T-P5-04): a group of aligned, consecutive bars described as one element. A bar that belongs to no
/// record is an implicit one-bar generic piece, so only multi-bar pieces, marked pieces and grouped pieces are stored.
/// `nodeIds` is the chain of nodes from the start node of the first bar to the end node of the last one
/// (barIds.size() + 1 entries); bar i runs between nodeIds[i] and nodeIds[i + 1] (in either bar direction).
/// `groupId` (0 = none) is shared by the pieces of one "Agrupar" group.
struct Piece {
    int id = 0;
    std::vector<int> barIds;
    std::vector<int> nodeIds;
    ElementType type = ElementType::Generic;
    int groupId = 0;
    bool operator==(const Piece&) const = default;
};

/// Flat plate or shell with 3 (triangle) or 4 (quadrilateral) corner nodes, listed in order around the contour.
struct Shell {
    int id = 0;
    std::vector<int> nodeIds;
    double thicknessM = 0.0;
    int materialId = 0;
    bool zAxisInverted = false;  ///< "invertir z": flips the local normal
    /// "Definir eje x" (#64716 / #64717): the contour edge that gives the local x direction, as its two nodes in contour
    /// order. 0 / 0 = default direction (see shell_edit.h). A pair that is not a contour edge is ignored.
    int xEdgeStart = 0;
    int xEdgeEnd = 0;
    std::vector<int> layerIds{kDefaultLayerId};  ///< sorted, unique, never empty
    bool operator==(const Shell&) const = default;
};

class StructuralModel {
public:
    std::vector<Node> nodes;
    std::vector<Material> materials;
    std::vector<CrossSection> sections;
    std::vector<Bar> bars;
    std::vector<Shell> shells;
    std::vector<Piece> pieces;  ///< sorted by id; section absent in older files = empty
    std::vector<Layer> layers{Layer{kDefaultLayerId, kDefaultLayerName, 0x000000, true, true, false}};

    // Next free id of each collection. Public so the file loader can restore them; use the add* functions otherwise.
    int nextNodeId = 1;
    int nextMaterialId = 1;
    int nextSectionId = 1;
    int nextBarId = 1;
    int nextShellId = 1;
    int nextLayerId = kDefaultLayerId + 1;
    int nextPieceId = 1;

    // --- lookup (nullptr when absent) ---
    const Node* findNode(int id) const;
    const Material* findMaterial(int id) const;
    const CrossSection* findSection(int id) const;
    const Bar* findBar(int id) const;
    const Shell* findShell(int id) const;
    const Layer* findLayer(int id) const;
    Layer* findLayer(int id);
    Node* findNode(int id);
    Bar* findBar(int id);
    Shell* findShell(int id);

    // --- creation: return the new id, or 0 when the arguments are invalid (the model is left untouched) ---
    int addNode(const Point3& position);
    int addMaterial(Material material);          ///< `id` is ignored
    int addSection(CrossSection section);        ///< `id` is ignored
    int addBar(int startNode, int endNode, int materialId = 0, int sectionId = 0);
    int addShell(std::vector<int> nodeIds, double thicknessM, int materialId = 0);

    /// Restores an object with a known id (undo of a deletion, file loading). False when the id is taken or invalid.
    bool insertNode(const Node& node);
    bool insertBar(const Bar& bar);
    bool insertShell(const Shell& shell);

    // --- layers ---
    /// Adds a layer; 0 when the name is empty or already used (names compare case-insensitively).
    int addLayer(const std::string& name, unsigned color = 0x000000);
    /// Name the "+" button proposes: "Nueva capa N" with N the layer count after adding, skipping names in use.
    std::string suggestedLayerName() const;
    /// Removes a layer. Elements that belong only to it are moved to `moveTo` (MoveElements) or erased
    /// (DeleteElements); elements that also belong to other layers just lose the membership. Refused (false) for an
    /// unknown id, when it is the last layer, or when MoveElements gets an unknown / identical `moveTo`. When the
    /// removed layer was active the first remaining layer becomes active.
    bool removeLayer(int id, LayerRemoval mode = LayerRemoval::MoveElements, int moveTo = 0);
    /// Number of bars and shells that belong to the layer.
    int layerUsage(int id) const;
    /// Makes `id` the only active layer; false when it does not exist.
    bool setActiveLayer(int id);
    /// Renames a layer; false when the name is empty, used by another layer, or the id is unknown.
    bool renameLayer(int id, const std::string& name);
    /// Moves a layer one position up (-1) or down (+1) in the list; false at the ends or for an unknown id. Ids are
    /// unchanged, so unlike the other collections `layers` is ordered by position, not by id.
    bool moveLayer(int id, int direction);
    /// Id of the active layer (the first layer when none is flagged).
    int activeLayerId() const;
    /// Applies the "Asignar elementos a etiquetas" dialog to the given bars and shells. False (nothing changed) when
    /// no element is given, a layer / element does not exist, or Assign / Replace get no layer. After Remove, an
    /// element left without layers falls back to the active layer.
    bool assignLayers(const std::vector<int>& barIds, const std::vector<int>& shellIds, const std::vector<int>& layerIds,
                      LayerOperation operation);
    /// Name of a layer, or the first layer's name when the id is unknown.
    std::string layerName(int id) const;
    /// "a", "a y b", "a, b y c": the text of the "Capa" row of the element information box.
    std::string layerNames(const std::vector<int>& ids) const;
    /// Element-level view of the layer flags. Provisional rules (TK-029): an element is visible when any of its layers
    /// is, locked when any of its layers is, rendered when any of its layers is; it takes the colour of the first of
    /// its layers in list order.
    bool elementVisible(const std::vector<int>& ids) const;
    bool elementLocked(const std::vector<int>& ids) const;
    bool elementRendered(const std::vector<int>& ids) const;
    unsigned elementColor(const std::vector<int>& ids) const;
    bool layerNameTaken(const std::string& name, int exceptId) const;
    /// Empty when a layer list of an element is valid, otherwise the problem (without the element name).
    std::string checkLayerIds(const std::vector<int>& ids) const;

    // --- pieces ---
    const Piece* findPiece(int id) const;
    /// Record of the piece that contains the bar; nullptr for an implicit one-bar piece (or an unknown bar).
    const Piece* pieceOfBar(int barId) const;
    bool insertPiece(const Piece& piece);  ///< false when the id is taken or invalid
    bool removePiece(int id);

    // --- deletion: false when the object does not exist or is still referenced (see the cascade helpers) ---
    bool removeBar(int id);
    bool removeShell(int id);
    bool removeNode(int id);       ///< refused while a bar or shell uses the node
    bool removeMaterial(int id);   ///< refused while a bar or shell uses the material
    bool removeSection(int id);    ///< refused while a bar uses the section

    /// Ids of the bars / shells that use a node (what a cascading node deletion must remove first).
    std::vector<int> barsUsingNode(int nodeId) const;
    std::vector<int> shellsUsingNode(int nodeId) const;

    /// Length of a bar in metres; 0 when the bar or one of its nodes does not exist.
    double barLength(int barId) const;

    /// Empty when the model is consistent, otherwise a Spanish message naming the first problem found (duplicated or
    /// unsorted ids, dangling references, degenerate bars, shells with repeated nodes, out-of-range values...).
    std::string validate() const;

    bool empty() const;
};

}  // namespace cc::core
