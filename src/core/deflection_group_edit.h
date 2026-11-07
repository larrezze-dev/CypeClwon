#pragma once
#include <optional>
#include <string>
#include <vector>

#include "core/deflection_group.h"
#include "core/project.h"
#include "core/undo_stack.h"

namespace cc::core {

/// Undoable edits of the deflection groups (T-P6-06). See deflection_group.h for the data. The command keeps a snapshot of the whole
/// DeflectionData before and after (small), so undo / redo are exact.
///
/// Integration with structure edits: SnapshotCommand (dividing / deleting bars) reconciles with reconcileDeflection; DeleteNodesCommand
/// replaces the two bars it merges by the merged bar (which joins the explicit group of the first of them) and normalises; InvertBarAxisCommand
/// swaps node 1 / node 2 of the groups it inverts completely; MoveBarEndCommand and MoveNodeCommand normalise (a group that stops being a
/// straight chain is dissolved).

class DeflectionCommand : public UndoCommand {
public:
    DeflectionCommand(Project& project, DeflectionData after, std::string text)
        : m_project(project), m_before(project.deflection), m_after(std::move(after)), m_text(std::move(text)) {}
    /// False when the operation changes nothing.
    bool valid() const { return m_before != m_after; }
    void redo() override { m_project.deflection = m_after; }
    void undo() override { m_project.deflection = m_before; }
    std::string text() const override { return m_text; }

private:
    Project& m_project;
    DeflectionData m_before, m_after;
    std::string m_text;
};

/// Why "Crear" was refused. Messages: #64227 and #64228 are original strings; the others have none.
enum class DeflectionError {
    None,
    NoAlignedGroup,        ///< #64227 "No se ha encontrado un grupo de barras alineadas que conecte los dos nudos seleccionados."
    EquivalentExists,      ///< #64228 "Ya existe un grupo de flecha equivalente."
    DifferentArrangement,  ///< the bars do not have the same roll and local x direction (REA assertion; text of the piece error #64229)
    UnknownElement,        ///< a node / bar id does not exist
    NothingToChange,       ///< the edit leaves the data as it is
};
std::string deflectionErrorMessage(DeflectionError error);

/// Checks of "Crear" in the order of the original (grupos_flecha.md section 4): aligned chain between the nodes (#64227, shown BEFORE the
/// dialog of types), a group with exactly the same bars already exists (#64228), same roll and x direction along the chain. `chain`
/// receives the bars (ordered from node1 to node2) when the first check passes.
DeflectionError checkCreateDeflectionGroup(const Project& project, int node1, int node2, std::vector<int>* chain = nullptr);

/// "Crear": group of the aligned bars between the two nodes with the given types and default limits. Every existing group that contains any
/// of those bars is dissolved first (a new group prevails over the ones that overlap; the bars left out go back to one-bar secant groups).
/// Returns None on success (then `number` receives the new "Gn" when given); otherwise the error and nothing is pushed.
DeflectionError createDeflectionGroup(Project& project, UndoStack& stack, int node1, int node2, DeflectionType xy, DeflectionType xz,
                                      int* number = nullptr);

/// "Editar": sets the types of the groups that contain the given bars (several groups at once; the bars of a group need not all be given).
/// UnknownElement when no given bar exists, NothingToChange when nothing changes.
DeflectionError editDeflectionTypes(Project& project, UndoStack& stack, const std::vector<int>& barIds, DeflectionType xy, DeflectionType xz);

/// "Flecha Límite": assigns the eight blocks to the groups that contain the given bars. Runs checkDeflectionLimits: on error nothing is
/// pushed and the issue is returned. Returns nullopt on success (also when nothing changes: then nothing is pushed). Unknown bars are ignored.
std::optional<DeflectionIssue> setDeflectionLimits(Project& project, UndoStack& stack, const std::vector<int>& barIds, const DeflectionLimits& limits);

/// Bars of the model that exist among `ids` (used by the edits above).
std::vector<int> existingDeflectionBars(const Project& project, const std::vector<int>& ids);

}  // namespace cc::core
