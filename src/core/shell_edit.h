#pragma once
#include <optional>
#include <string>
#include <vector>

#include "core/project.h"
#include "core/shell_material.h"
#include "core/structure_snapshot.h"
#include "core/undo_stack.h"

namespace cc::core {

/// Undoable shell operations of "Geometria > Laminas" (T-P5-09, core only). Behaviour source: docs/research/ui/laminas.md.
/// A shell is a flat plate whose contour is 3 or 4 nodes listed in order (the model limit, see Shell). Every command edits
/// a copy of the model and swaps it on redo / undo (SnapshotCommand).

/// Tolerance of the coplanarity check of "Unir" (metres). [REA] ed3dlib FUN_18003c480: the constant 0x3f50624dd2f1a9fc is 0.001.
inline constexpr double kShellCoplanarityToleranceM = 0.001;
/// Distance under which a point of a division counts as an existing node / a vertex of the contour. NOT MEASURED: provisional.
inline constexpr double kShellDivideToleranceM = 1e-6;
/// Thickness proposed for a new shell: 100 mm, measured in the original (laminas.md section 7.1).
inline constexpr double kDefaultShellThicknessM = kDefaultShellThicknessMm / 1000.0;

/// Why a shell operation was refused. Messages are the original strings where there is one.
enum class ShellError {
    None,
    InvalidContour,    ///< #65121 "Los puntos introducidos no permiten definir correctamente la lámina."
    NotCoplanar,       ///< #64004 "Las láminas seleccionadas no están contenidas en el mismo plano."
    NotClosedRegion,   ///< #64005 "No es posible unir las láminas seleccionadas porque no delimitan una región cerrada sin huecos."
    TooFewShells,      ///< #64232 "Debe seleccionar, al menos, dos láminas situadas en el mismo plano para poder realizar la unión."
    UnknownElement,    ///< a node or shell id does not exist (no original message)
    NothingToDivide,   ///< no selected shell is crossed by the segment (no original message)
    CannotDivide,      ///< the cut would not give valid shells of 3 or 4 nodes (no original message)
    ContourTooComplex, ///< the union has more than 4 corner nodes, which the model cannot hold (PROVISIONAL, no original message)
    InvalidMove,       ///< the move would break a bar or a shell (no original message)
    InvalidEdge,       ///< the pair of nodes is not an edge of the shell (no original message)
    InvalidThickness,  ///< thickness outside 0.10 .. 10000.00 mm (measured, laminas.md section 7.1)
    InvalidBallast,    ///< ballast modulus enabled with a value outside 0.01 .. 1000000.00 kN/m3 (measured)
    InvalidMaterial,   ///< a material value outside its measured range (the command's message() has the warning of the original)
    InvalidOffset,     ///< central plane displacement outside -5000 .. 5000 mm (measured)
    NothingToChange    ///< nothing selected / the operation changes nothing (no original message)
};

/// Spanish text of a ShellError (original string when it has one, otherwise the clone's own wording).
std::string shellErrorMessage(ShellError error);

/// True when all the points lie within `toleranceM` of one plane (three or fewer points, or collinear ones, always do).
/// Mirrors ejes2d_punto3D_forman_ejes_con_precision(points, 0.001).
bool pointsCoplanar(const std::vector<Point3>& points, double toleranceM = kShellCoplanarityToleranceM);

/// None when `nodeIds` is a valid contour: 3 or 4 distinct existing nodes, coplanar, non-zero area, and a simple polygon
/// (the edges do not cross). Otherwise InvalidContour (or NotCoplanar for a non-planar quadrilateral).
ShellError checkContour(const StructuralModel& model, const std::vector<int>& nodeIds);

/// Splits a contour of more than 4 nodes into contours of 3 or 4 by cutting off ears. Returns {nodeIds} for 3 or 4 nodes
/// and an empty list when it cannot be done (degenerate polygon). The vertex order (and so the normal) is kept.
std::vector<std::vector<int>> decomposeContour(const StructuralModel& model, const std::vector<int>& nodeIds);

/// Local axes of a shell. z = normal of the contour (right-hand rule of the node order; flipped by Shell::zAxisInverted),
/// x = direction of the edge chosen with "Definir eje x" (#64717), y = z x x.
/// PROVISIONAL default of x (laminas.md section 4: not found in the original): the global X axis projected on the plane of
/// the shell, or the global Y axis when the shell is perpendicular to X.
struct ShellAxes {
    Point3 x, y, z;
};
std::optional<ShellAxes> shellLocalAxes(const StructuralModel& model, const Shell& shell);

/// "Sección de la lámina" values (see SetShellSectionCommand).
struct ShellSection {
    double thicknessM = kDefaultShellThicknessM;
    bool ballastEnabled = false;
    double ballastKnM3 = kDefaultShellBallastKnM3;
    BallastBehaviour ballastBehaviour = BallastBehaviour::Linear;
};

/// "Nueva": adds a shell on 3 or 4 nodes given in contour order. The new shell takes the active layer.
class AddShellCommand : public SnapshotCommand {
public:
    /// `section`, `material` and `layout` (when given) are the panels of the dialog "Nueva" and replace `thicknessM` / the defaults.
    AddShellCommand(Project& project, const std::vector<int>& nodeIds, double thicknessM = kDefaultShellThicknessM, int materialId = 0,
                    const ShellSection* section = nullptr, const ShellMaterial* material = nullptr, const ShellLayout* layout = nullptr);
    bool valid() const { return m_error == ShellError::None; }
    ShellError error() const { return m_error; }
    /// Warning of the original for a value out of range; empty otherwise.
    const std::string& message() const { return m_message; }
    int shellId() const { return m_shellId; }  ///< 0 when invalid

private:
    ShellError m_error = ShellError::None;
    std::string m_message;
    int m_shellId = 0;
};

/// "Borrar": deletes shells (their nodes stay). UnknownElement when an id does not exist or the list is empty.
class DeleteShellsCommand : public SnapshotCommand {
public:
    DeleteShellsCommand(Project& project, const std::vector<int>& shellIds);
    bool valid() const { return m_error == ShellError::None; }
    ShellError error() const { return m_error; }

private:
    ShellError m_error = ShellError::None;
};

/// "Dividir": the line through the segment p-q divides every selected shell it crosses completely, even if the segment
/// only penetrates it partially (#64726). The cut plane contains the line and the normal of each shell. A shell that is
/// not crossed is left alone; if none is, NothingToDivide. The cut points reuse an existing node or create one; one part
/// keeps the shell id and the other parts get new ids with the same thickness, material, layers and z inversion.
/// A cut that does not cross the contour at exactly two points (non-convex shells) is CannotDivide. Bars and neighbour
/// shells whose edge is cut are not split here ("Reparar" divides them by the new node). Grouped planes (homologous
/// shells of the other planes of a group, #64726) are the caller's job: PROVISIONAL, not modelled here.
class DivideShellsCommand : public SnapshotCommand {
public:
    DivideShellsCommand(Project& project, const std::vector<int>& shellIds, const Point3& p, const Point3& q);
    bool valid() const { return m_error == ShellError::None; }
    ShellError error() const { return m_error; }
    /// Ids of the shells created by the division, in creation order.
    const std::vector<int>& createdShellIds() const { return m_created; }

private:
    ShellError m_error = ShellError::None;
    std::vector<int> m_created;
};

/// "Unir": merges two or more coplanar shells (tolerance kShellCoplanarityToleranceM) that share edges into one whose
/// contour is the outline of the union. Checks in the order of the original: at least two shells (#64232), coplanar
/// (#64004), the boundary is one simple closed polygon without holes (#64005). The result keeps the lowest id and the
/// properties (thickness, material, layers, z inversion) of the lowest-id shell and the orientation of its normal; the other
/// shells disappear. PROVISIONAL: corner nodes that lie on a straight stretch of the outline and are used by nothing else are
/// dropped from the contour (a union of two quads is a quad); if more than 4 corners remain the union is ContourTooComplex.
/// The nodes themselves are never deleted.
class JoinShellsCommand : public SnapshotCommand {
public:
    JoinShellsCommand(Project& project, const std::vector<int>& shellIds);
    bool valid() const { return m_error == ShellError::None; }
    ShellError error() const { return m_error; }
    int resultShellId() const { return m_result; }  ///< 0 when invalid

private:
    ShellError m_error = ShellError::None;
    int m_result = 0;
};

/// "Invertir eje z": toggles Shell::zAxisInverted of every given shell (#64715).
class InvertShellZCommand : public SnapshotCommand {
public:
    InvertShellZCommand(Project& project, const std::vector<int>& shellIds);
    bool valid() const { return m_error == ShellError::None; }
    ShellError error() const { return m_error; }

private:
    ShellError m_error = ShellError::None;
};

/// "Definir eje x": the edge between nodes a and b (either order) defines the local x direction (#64717). Stored in
/// contour order, so the direction is that of the contour (PROVISIONAL: the original does not say how the sense is chosen).
class SetShellXEdgeCommand : public SnapshotCommand {
public:
    SetShellXEdgeCommand(Project& project, int shellId, int nodeA, int nodeB);
    bool valid() const { return m_error == ShellError::None; }
    ShellError error() const { return m_error; }

private:
    ShellError m_error = ShellError::None;
};

/// "Mover": moves the given contour nodes of a shell by `delta` (the two points of the original give vector and
/// magnitude, #65123). Bars and other shells that use the nodes follow. InvalidMove when a bar would become shorter than
/// kMinBarLengthM or a shell using a moved node would stop being a valid contour (NotCoplanar if it stops being planar).
class MoveShellNodesCommand : public SnapshotCommand {
public:
    MoveShellNodesCommand(Project& project, int shellId, const std::vector<int>& nodeIds, const Point3& delta);
    bool valid() const { return m_error == ShellError::None; }
    ShellError error() const { return m_error; }

private:
    ShellError m_error = ShellError::None;
};

/// "Sección": thickness and local-Z ballast modulus of every given shell in one step (panel "Sección de la lámina", #65122).
/// A disabled ballast keeps the stored value and behaviour untouched. InvalidThickness / InvalidBallast refuse the whole edit.
class SetShellSectionCommand : public SnapshotCommand {
public:
    SetShellSectionCommand(Project& project, const std::vector<int>& shellIds, const ShellSection& section);
    bool valid() const { return m_error == ShellError::None; }
    ShellError error() const { return m_error; }

private:
    ShellError m_error = ShellError::None;
};

/// "Material" / "Describir material": the material of every given shell (panel "Material", laminas.md section 7.1). An out-of-range
/// value refuses the whole edit (InvalidMaterial; message() is the warning of the original).
class SetShellMaterialCommand : public SnapshotCommand {
public:
    SetShellMaterialCommand(Project& project, const std::vector<int>& shellIds, const ShellMaterial& material);
    bool valid() const { return m_error == ShellError::None; }
    ShellError error() const { return m_error; }
    const std::string& message() const { return m_message; }

private:
    ShellError m_error = ShellError::None;
    std::string m_message;
};

/// "Disposición" / "Describir disposición": the eccentricity flag and the central plane displacement of every given shell.
class SetShellLayoutCommand : public SnapshotCommand {
public:
    SetShellLayoutCommand(Project& project, const std::vector<int>& shellIds, const ShellLayout& layout);
    bool valid() const { return m_error == ShellError::None; }
    ShellError error() const { return m_error; }

private:
    ShellError m_error = ShellError::None;
};

/// Wrappers: build the command and push it on `stack`; they return ShellError::None on success, otherwise the error and
/// nothing is pushed.
ShellError addShell(Project& project, UndoStack& stack, const std::vector<int>& nodeIds, double thicknessM = kDefaultShellThicknessM,
                    int materialId = 0, int* shellId = nullptr, const ShellSection* section = nullptr,
                    const ShellMaterial* material = nullptr, const ShellLayout* layout = nullptr);
ShellError deleteShells(Project& project, UndoStack& stack, const std::vector<int>& shellIds);
ShellError divideShells(Project& project, UndoStack& stack, const std::vector<int>& shellIds, const Point3& p, const Point3& q,
                        std::vector<int>* createdIds = nullptr);
ShellError joinShells(Project& project, UndoStack& stack, const std::vector<int>& shellIds, int* resultId = nullptr);
ShellError invertShellZ(Project& project, UndoStack& stack, const std::vector<int>& shellIds);
ShellError setShellXEdge(Project& project, UndoStack& stack, int shellId, int nodeA, int nodeB);
ShellError setShellSection(Project& project, UndoStack& stack, const std::vector<int>& shellIds, const ShellSection& section);
ShellError setShellMaterial(Project& project, UndoStack& stack, const std::vector<int>& shellIds, const ShellMaterial& material,
                            std::string* message = nullptr);
ShellError setShellLayout(Project& project, UndoStack& stack, const std::vector<int>& shellIds, const ShellLayout& layout);
ShellError moveShellNodes(Project& project, UndoStack& stack, int shellId, const std::vector<int>& nodeIds, const Point3& delta);

}  // namespace cc::core
