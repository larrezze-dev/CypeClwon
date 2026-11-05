#pragma once
#include <string>
#include <vector>

#include "core/project.h"
#include "core/structure_snapshot.h"
#include "core/undo_stack.h"

namespace cc::core {

/// "Asignar referencia" and "Buscar" of Geometría > Herramientas (herramientas.md 6.1, measured in the original): a floating window
/// "Propiedades" with "Tipo de elemento" (default Nudos) and the modes "Individual" (default) / "Múltiple"; and "Propiedades - Buscar" with
/// "Tipo de elemento", "Referencia" and "Sólo palabra completa". The model keeps references only where it has them: nodes (automatic
/// "N<id>") and shells (automatic "L<id>"); bars have no reference of their own (PROVISIONAL: the items of the combo other than "Nudos"
/// were not seen in the original; "Láminas" is added because the shells have a measured automatic reference, L1).

enum class ReferenceKind { Node, Shell };

enum class ReferenceError { None, UnknownElement, EmptyReference, NothingToChange };
std::string referenceErrorMessage(ReferenceError error);

/// Reference an element shows: the one assigned, or the automatic "N<id>" / "L<id>". Empty when the element does not exist.
std::string elementReference(const StructuralModel& model, ReferenceKind kind, int id);

/// Ids whose reference matches `text`: contains it (case-insensitive) or, with `wholeWord`, equals it exactly (case-insensitive).
/// An empty text matches nothing.
std::vector<int> findByReference(const StructuralModel& model, ReferenceKind kind, const std::string& text, bool wholeWord);

/// References of "Múltiple": the first element gets `first`, each following one the reference that follows it (nextReference of the
/// work planes: "N1" -> "N2", "A" -> "B", "9" -> "10").
std::vector<std::string> consecutiveReferences(const std::string& first, std::size_t count);

/// Sets the reference of the given elements, one text per element (same order). A text equal to the automatic reference clears the stored
/// one. One undo step; EmptyReference when a text is blank, NothingToChange when no element changes.
class SetReferencesCommand : public SnapshotCommand {
public:
    SetReferencesCommand(Project& project, ReferenceKind kind, const std::vector<int>& ids, const std::vector<std::string>& texts);
    bool valid() const { return m_error == ReferenceError::None; }
    ReferenceError error() const { return m_error; }

private:
    ReferenceError m_error = ReferenceError::None;
};

ReferenceError assignReferences(Project& project, UndoStack& stack, ReferenceKind kind, const std::vector<int>& ids,
                                const std::vector<std::string>& texts);

}  // namespace cc::core
