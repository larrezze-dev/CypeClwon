#pragma once
#include <string>
#include <vector>

#include "core/load_case.h"
#include "core/project.h"
#include "core/undo_stack.h"

namespace cc::core {

/// Undoable edits of the load cases (T-P7-01). Every command keeps a snapshot of the whole LoadCaseSet before and after (small), so
/// undo / redo are exact. See load_case.h for the data.

class LoadCaseCommand : public UndoCommand {
public:
    LoadCaseCommand(Project& project, LoadCaseSet after, std::string text)
        : m_project(project), m_before(project.loadCases), m_after(std::move(after)), m_text(std::move(text)) {}
    bool valid() const { return m_before != m_after; }
    void redo() override { m_project.loadCases = m_after; }
    void undo() override { m_project.loadCases = m_before; }
    std::string text() const override { return m_text; }

private:
    Project& m_project;
    LoadCaseSet m_before, m_after;
    std::string m_text;
};

enum class LoadCaseError {
    None,
    BlankReference,      ///< "'Referencia' no puede quedar en blanco."
    NeedsUseCategories,  ///< #39206, a live-load case needs use categories first (the GUI then opens "Categorías de uso")
    UnknownCase,         ///< the id does not exist
    ProtectedCase,       ///< "Peso propio" cannot be edited or deleted; SelfWeight takes no additional cases
    InvalidArrangement,  ///< arrangements for a nature that has none (Seismic) / index out of range / case without arrangements
    UnknownCategory,     ///< use category index out of range
    NothingToChange,
};
std::string loadCaseErrorMessage(LoadCaseError error);

/// Data of the dialog "Nueva hipótesis adicional".
struct NewLoadCase {
    LoadNature nature = LoadNature::DeadLoad;
    std::string reference;
    std::string description;
    bool withArrangements = false;  ///< the check box "Con distintas disposiciones de cargas" (not for Seismic)
    ArrangementCompat compat = ArrangementCompat::Simultaneous;
    SeismicType seismic = SeismicType::Simple;  ///< only for Seismic
};

/// "Nueva hipótesis adicional". Checks in order: nature takes additional cases, arrangements allowed, blank reference, live load without
/// categories. With arrangements the case starts with "1" and "2". Returns None on success (then `id` receives the new case id).
LoadCaseError createLoadCase(Project& project, UndoStack& stack, const NewLoadCase& data, int* id = nullptr);
/// "Editar hipótesis adicional": reference and description (arrangements and nature cannot change).
LoadCaseError editLoadCase(Project& project, UndoStack& stack, int id, const std::string& reference, const std::string& description);
/// Clicking the "Actúa" cell.
LoadCaseError toggleLoadCaseActs(Project& project, UndoStack& stack, int id);

/// Dependents of a case in the project. TODO(T-P7-02..05, T-P7-06): collect the loads and the non-linear / buckling combinations that use
/// it; today there are none, so the list is always empty.
LoadCaseDependents loadCaseDependents(const Project& project, int id);
/// "Borrar". Without dependents it is immediate (measured); the dependents are to be deleted with the case (#64905 / #380751 / #380752: the
/// GUI asks first when loadCaseDependents is not empty). TODO(T-P7-02..06): delete them inside the same command.
LoadCaseError deleteLoadCase(Project& project, UndoStack& stack, int id);

/// "+" of the arrangements table: appends the next number ("3", "4", ...); `name` receives it.
LoadCaseError addLoadArrangement(Project& project, UndoStack& stack, int id, std::string* name = nullptr);
/// Aspa of the arrangements table (removes the entry at `index`).
LoadCaseError deleteLoadArrangement(Project& project, UndoStack& stack, int id, std::size_t index);

/// "Considerar efectos de segundo orden" of the Sismo window.
LoadCaseError setSeismicSecondOrder(Project& project, UndoStack& stack, bool value);

/// Categorías de uso: "+" (next letter, description = first choice), copy of the entry at `index`, delete the entry at `index`.
LoadCaseError addUseCategory(Project& project, UndoStack& stack, const std::array<std::string, kUseCategoryNormGroups>& choices);
/// Changes the choices of the category at `index` (the combos beside the list of "Categorías de uso"); the description follows the first one.
LoadCaseError setUseCategoryChoices(Project& project, UndoStack& stack, std::size_t index,
                                    const std::array<std::string, kUseCategoryNormGroups>& choices);
LoadCaseError copyUseCategory(Project& project, UndoStack& stack, std::size_t index);
LoadCaseError deleteUseCategory(Project& project, UndoStack& stack, std::size_t index);

}  // namespace cc::core
