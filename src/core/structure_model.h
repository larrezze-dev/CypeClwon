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

/// Straight bar between two nodes. materialId / sectionId 0 mean "not assigned yet" (a freshly drawn bar).
struct Bar {
    int id = 0;
    int startNode = 0;
    int endNode = 0;
    int materialId = 0;
    int sectionId = 0;
    double rollAngleDeg = 0.0;   ///< rotation of the local axes about the bar axis (x)
    bool xAxisInverted = false;  ///< "invertir eje x": swaps the start and end of the local axes
    bool operator==(const Bar&) const = default;
};

/// Flat plate or shell with 3 (triangle) or 4 (quadrilateral) corner nodes, listed in order around the contour.
struct Shell {
    int id = 0;
    std::vector<int> nodeIds;
    double thicknessM = 0.0;
    int materialId = 0;
    bool zAxisInverted = false;  ///< "invertir z": flips the local normal
    bool operator==(const Shell&) const = default;
};

class StructuralModel {
public:
    std::vector<Node> nodes;
    std::vector<Material> materials;
    std::vector<CrossSection> sections;
    std::vector<Bar> bars;
    std::vector<Shell> shells;

    // Next free id of each collection. Public so the file loader can restore them; use the add* functions otherwise.
    int nextNodeId = 1;
    int nextMaterialId = 1;
    int nextSectionId = 1;
    int nextBarId = 1;
    int nextShellId = 1;

    // --- lookup (nullptr when absent) ---
    const Node* findNode(int id) const;
    const Material* findMaterial(int id) const;
    const CrossSection* findSection(int id) const;
    const Bar* findBar(int id) const;
    const Shell* findShell(int id) const;
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
