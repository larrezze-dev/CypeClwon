#pragma once
#include <memory>
#include <string>
#include <vector>

#include "core/project.h"
#include "core/undo_stack.h"

namespace cc::core {

/// Undoable node operations of "Geometria > Nudos" (T-P5-02, core only). Behaviour source: docs/research/ui/nudos.md
/// (texts #64743 .. #64750 of the original). Ids are never reused and every collection stays sorted by id.

/// Perpendicular distance (metres) under which the intermediate node of two bars counts as lying on the line joining
/// their far ends. NOT MEASURED in the original (see TK, docs/research/ui/nudos.md section 6.5): provisional value.
inline constexpr double kCollinearToleranceM = 1e-6;
/// Distance (metres) under which two nodes would be considered the same. NOT MEASURED (nudos.md section 6.2): it is
/// only exposed for the GUI layer; the core operations do not reject coincident nodes.
inline constexpr double kCoincidentNodeToleranceM = 1e-6;

/// "Nuevo": adds a node. `id()` is reserved on construction (next free id) and is valid once the command is pushed.
class AddNodeCommand : public UndoCommand {
public:
    AddNodeCommand(Project& project, const Point3& position);
    bool valid() const { return m_valid; }
    int id() const { return m_node.id; }
    void redo() override;
    void undo() override;
    std::string text() const override { return "Nuevo nudo"; }

private:
    Project& m_project;
    Node m_node;
    bool m_valid;
};

/// "Mover": moves a node; the bars and shells that use it follow by reference.
class MoveNodeCommand : public UndoCommand {
public:
    MoveNodeCommand(Project& project, int nodeId, const Point3& newPosition);
    /// False when the node does not exist, the position is not finite / out of range, or a connected bar would become
    /// degenerate (shorter than the minimum bar length).
    bool valid() const { return m_valid; }
    void redo() override;
    void undo() override;
    std::string text() const override { return "Mover nudo"; }

private:
    Project& m_project;
    int m_id;
    Point3 m_before;
    Point3 m_after;
    bool m_valid;
};

/// "Borrar": deletes nodes together with the bars and shells that use them. Exception: a deleted node that joins exactly
/// two collinear bars of the same material and section (and nothing else, no shell) is replaced by one new bar between
/// the far ends; that bar inherits material, section, roll angle, axis inversion and labels from the first bar (the one
/// with the lower id) and keeps its direction. A far end that is itself being deleted disables the merge for that node.
class DeleteNodesCommand : public UndoCommand {
public:
    DeleteNodesCommand(Project& project, std::vector<int> nodeIds);
    /// False when no given id exists (nothing would be deleted).
    bool valid() const { return !m_nodes.empty(); }
    const std::vector<Bar>& mergedBars() const { return m_merged; }
    void redo() override;
    void undo() override;
    std::string text() const override { return "Borrar nudos"; }

private:
    Project& m_project;
    std::vector<Node> m_nodes;
    std::vector<Bar> m_bars;
    std::vector<Shell> m_shells;
    std::vector<Bar> m_merged;
};

/// Convenience wrappers: build the command, push it on `stack` and return the result (0 / false when invalid, in which
/// case nothing is pushed).
int addNode(Project& project, UndoStack& stack, const Point3& position);
bool moveNode(Project& project, UndoStack& stack, int nodeId, const Point3& newPosition);
bool deleteNodes(Project& project, UndoStack& stack, const std::vector<int>& nodeIds);

}  // namespace cc::core
