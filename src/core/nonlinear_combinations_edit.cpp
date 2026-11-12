#include "core/nonlinear_combinations_edit.h"

#include <algorithm>
#include <cctype>

#include "core/project.h"

namespace cc::core {

NonlinearCombinationsCommand::NonlinearCombinationsCommand(Project& project, NonlinearCombinations after, std::string text)
    : m_project(project), m_before(project.nonlinearCombinations), m_after(std::move(after)), m_text(std::move(text)) {}

void NonlinearCombinationsCommand::redo() { m_project.nonlinearCombinations = m_after; }
void NonlinearCombinationsCommand::undo() { m_project.nonlinearCombinations = m_before; }

bool setNonlinearCombinations(Project& project, UndoStack& stack, NonlinearCombinations after, std::string text) {
    if (after == project.nonlinearCombinations) return false;
    stack.push(std::make_unique<NonlinearCombinationsCommand>(project, std::move(after), std::move(text)));
    return true;
}

std::string validateNonlinearCombinations(const NonlinearCombinations& set) {
    for (const NonlinearCombination& c : set.items) {
        const bool blank = std::all_of(c.reference.begin(), c.reference.end(), [](unsigned char ch) { return std::isspace(ch) != 0; });
        if (blank) return "Debe introducir una referencia para cada combinación.";
        if (c.loadSteps < 1) return "El número de escalones de carga debe ser mayor que cero.";
    }
    return {};
}

}  // namespace cc::core
