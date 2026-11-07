#pragma once
#include <algorithm>
#include <optional>
#include <string>
#include <vector>

#include "core/node_diaphragm.h"
#include "core/project.h"
#include "core/undo_stack.h"

namespace cc::core {

/// Undoable edits of the diaphragms (T-P6-03). See node_diaphragm.h for the data and the checks. The command keeps a snapshot of the whole
/// Diaphragms before and after (small), so undo / redo are exact (ids are not reused: nextId only grows with redo).
class DiaphragmCommand : public UndoCommand {
public:
    DiaphragmCommand(Project& project, Diaphragms after, std::string text);
    /// Variant that also replaces the node ties (assigning a node to a diaphragm strips its ties).
    DiaphragmCommand(Project& project, Diaphragms after, NodeLigatures ligaturesAfter, std::string text);
    /// False when the operation changes nothing.
    bool valid() const { return m_before != m_after || m_ligBefore != m_ligAfter; }
    void redo() override { restore(m_after); m_project.ligatures = m_ligAfter; }
    void undo() override { restore(m_before); m_project.ligatures = m_ligBefore; }
    std::string text() const override { return m_text; }

private:
    void restore(const Diaphragms& state) {
        const int next = std::max(m_project.diaphragms.nextId, state.nextId);  // ids are never reused
        m_project.diaphragms = state;
        m_project.diaphragms.nextId = next;
    }
    Project& m_project;
    Diaphragms m_before, m_after;
    NodeLigatures m_ligBefore, m_ligAfter;
    std::string m_text;
};

/// Aceptar of the dialog "Editar": `edited` is the list the user left (rows with id 0 are new, rows whose id is missing are deleted, the order
/// is the order of the rows). The nodes of existing rows are kept from the project (the dialog cannot change them). Runs checkDiaphragmAccept:
/// on error nothing is pushed and the issue is returned. nullopt on success (also when nothing changed).
std::optional<DiaphragmIssue> editDiaphragms(Project& project, UndoStack& stack, const std::vector<Diaphragm>& edited);

/// Tool "Diafragma" > Asignar (RMB on the selection). Runs checkDiaphragmAssign; on Ok pushes the command. Error / Silent: nothing is pushed and
/// the check is returned (the GUI shows Error; Silent does nothing).
DiaphragmCheck assignNodesToDiaphragm(Project& project, UndoStack& stack, int diaphragmId, const std::vector<int>& nodeIds);
/// Tool "Diafragma" > Eliminar asignación. Returns true when something changed.
bool removeNodesFromDiaphragm(Project& project, UndoStack& stack, const std::vector<int>& nodeIds);

enum class DiaphragmGenerationStatus {
    NoLevels,           ///< the work has no level: «No se ha definido ningún nivel» (no question first, measured)
    NeedsConfirmation,  ///< diaphragms already exist and `confirmedReplace` is false: ask #375635 and call again with true
    Generated           ///< done (also with count 0: #375636)
};
struct DiaphragmGeneration {
    DiaphragmGenerationStatus status = DiaphragmGenerationStatus::NoLevels;
    int count = 0;       ///< diaphragms generated
    int messageId = 0;   ///< 375635 / 375636 / 375637 / 375638 (25932 for NoLevels)
    std::string text;    ///< literal text to show
};

/// "Generar por niveles". The previous diaphragms are replaced (after the question #375635); algorithm in planDiaphragmsByLevels (REA). One
/// undoable command.
DiaphragmGeneration generateDiaphragmsByLevels(Project& project, UndoStack& stack, bool confirmedReplace);

}  // namespace cc::core
