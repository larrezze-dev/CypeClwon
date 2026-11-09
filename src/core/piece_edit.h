#pragma once
#include <optional>
#include <string>
#include <vector>

#include "core/project.h"
#include "core/undo_stack.h"

namespace cc::core {

/// Undoable piece operations of "Geometria > Barras" (T-P5-04, core only). Behaviour source: docs/research/ui/pieza.md.
/// A piece is a group of aligned, consecutive bars described as one element (#64624); a bar with no piece record is an
/// implicit one-bar generic piece, so commands that take "a piece" take any bar id of it. Every command here edits a
/// copy of the model when it is constructed and stores the difference (bars changed, piece records), so redo / undo
/// are exact. A command must be pushed before another one is constructed.

/// Distance (metres) within which a node counts as lying on the line of a piece. NOT MEASURED: provisional, equal to
/// kCutNodeToleranceM.
inline constexpr double kPieceAlignmentToleranceM = 1e-6;

/// Chain of bars (ids, ordered from `startNode` to `endNode`) that are aligned with the segment between the two nodes
/// and connect them consecutively. Empty when the nodes are unknown or equal or no such chain exists.
std::vector<int> findAlignedChain(const StructuralModel& model, int startNode, int endNode);

/// Why a piece operation was refused. The messages are the original strings (#64226, #64227, #64229).
enum class PieceError {
    None,
    EquivalentExists,      ///< #64226 "Ya existe una pieza equivalente."
    NoAlignedGroup,        ///< #64227 "No se ha encontrado un grupo de barras alineadas que conecte los dos nudos seleccionados."
    DifferentArrangement,  ///< #64229 "Las barras seleccionadas no tienen la misma disposición (giro y enrase)."
    UnknownElement,        ///< a node or bar id does not exist (no original message)
    TooFewPieces,          ///< Agrupar needs two or more different pieces (no original message)
    NothingToChange,       ///< the operation would leave the model as it is (no original message)
    InvalidMove            ///< Mover extremo: same node, other end of the bar, existing bar or too short (no original message)
};

/// Spanish text of a PieceError (original string when it has one, otherwise the clone's own wording).
std::string pieceErrorMessage(PieceError error);

/// Shared machinery: the constructor of each subclass edits a copy of the model and calls capture(); redo / undo then
/// write the stored bars and piece records back.
class PieceDiffCommand : public UndoCommand {
public:
    bool valid() const { return m_error == PieceError::None; }
    PieceError error() const { return m_error; }
    void redo() override;
    void undo() override;

protected:
    explicit PieceDiffCommand(Project& project) : m_project(project) {}
    /// Stores the difference between the project model and `edited` (same bars, possibly changed fields).
    void capture(const StructuralModel& edited);
    /// Also stores a new state of the bar end data (InvertBarAxisCommand swaps origin / end).
    void captureBarEnds(const BarEnds& edited);
    /// Also stores a new state of the deflection groups (inversion swaps node 1 / node 2; moving an end may break a group).
    void captureDeflection(const DeflectionData& edited);
    Project& m_project;
    PieceError m_error = PieceError::None;

private:
    std::vector<Bar> m_barsBefore, m_barsAfter;
    std::vector<Piece> m_piecesBefore, m_piecesAfter;
    int m_nextPieceBefore = 1, m_nextPieceAfter = 1;
    std::optional<BarEnds> m_endsBefore, m_endsAfter;
    std::optional<BarLoads> m_barLoadsBefore, m_barLoadsAfter;  ///< set when moving a bar end changes its length
    std::optional<DeflectionData> m_deflectionBefore, m_deflectionAfter;
};

/// "Pieza": defines a piece from its start node (start of the first bar) to its end node (end of the last bar).
/// Checks, in this order: aligned chain exists (also required: the chain bars only belong to pieces entirely inside
/// the chain; a partial overlap is reported as NoAlignedGroup, provisional), the same piece does not exist yet (a lone
/// bar with no record counts as an existing piece), all bars have the same arrangement (equal roll angle and equal
/// direction of the local x axis along the chain; "enrase" is not modelled yet). The pieces absorbed by the new one
/// disappear; the new piece takes the element type of the first absorbed piece that has a mark and joins no group.
class CreatePieceCommand : public PieceDiffCommand {
public:
    CreatePieceCommand(Project& project, int startNode, int endNode);
    /// Id of the new piece (0 when invalid).
    int pieceId() const { return m_pieceId; }
    std::string text() const override { return "Nueva pieza"; }

private:
    int m_pieceId = 0;
};

/// Sets the element type mark (pillar / beam / generic) of the pieces of the given bars.
class SetPieceTypeCommand : public PieceDiffCommand {
public:
    SetPieceTypeCommand(Project& project, const std::vector<int>& barIds, ElementType type);
    std::string text() const override { return "Tipo de elemento estructural"; }
};

/// "Agrupar": groups the pieces of the given bars (two or more different pieces). Pieces already in a group drag their
/// whole group in. The group gets one profile and material: PROVISIONAL (not measured, pieza.md section 8.4) those of
/// the first piece selected (the first bar of the first given bar's piece); every bar of the group receives them.
/// Cartelas (#65102) are not modelled yet.
class GroupPiecesCommand : public PieceDiffCommand {
public:
    GroupPiecesCommand(Project& project, const std::vector<int>& barIds);
    /// Id shared by the members of the new group (0 when invalid).
    int groupId() const { return m_groupId; }
    std::string text() const override { return "Agrupar piezas"; }

private:
    int m_groupId = 0;
};

/// "Desagrupar": the pieces of the given bars leave their group and keep their own copy of profile and material. A
/// group left with a single piece disappears. NothingToChange when none of the pieces is grouped.
class UngroupPiecesCommand : public PieceDiffCommand {
public:
    UngroupPiecesCommand(Project& project, const std::vector<int>& barIds);
    std::string text() const override { return "Desagrupar piezas"; }
};

/// Which end of a bar "Mover extremo" moves.
enum class BarEnd { Start, End };

/// "Mover extremo". PROVISIONAL (pieza.md section 4: steps, node/extreme semantics and messages not measured): moves
/// the chosen end of the bar to another EXISTING node; the bar keeps its id and every other bar is untouched. Refused
/// (InvalidMove) when the node is the one already used, the one at the other end, the bar would be shorter than
/// kMinBarLengthM or another bar already joins the new pair. A bar of a multi-bar piece breaks the alignment, so its
/// piece is split into one-bar pieces (element type and group kept).
class MoveBarEndCommand : public PieceDiffCommand {
public:
    MoveBarEndCommand(Project& project, int barId, BarEnd end, int newNode);
    std::string text() const override { return "Mover extremo"; }
};

/// "Invertir eje 'x'": swaps the start and end node of the bar (#64640 / #64641). PROVISIONAL decisions: Bar::xAxisInverted
/// and the roll angle are left alone; for a bar of a multi-bar piece the whole piece is inverted (chain and bar order
/// reversed) so the alignment stays consistent. The end data (articulations, fixity coefficients: bar_ends.h) are swapped origin <-> end so
/// the physical structure does not change (#64641). NOTE: loads (none in the model yet) must be mirrored too (x -> L - x).
class InvertBarAxisCommand : public PieceDiffCommand {
public:
    InvertBarAxisCommand(Project& project, int barId);
    std::string text() const override { return "Invertir eje x"; }
};

/// Wrappers: build the command and push it on `stack`; they return PieceError::None on success, otherwise the error and
/// nothing is pushed. `pieceId` / `groupId` receive the new id when given.
PieceError createPiece(Project& project, UndoStack& stack, int startNode, int endNode, int* pieceId = nullptr);
PieceError setPieceType(Project& project, UndoStack& stack, const std::vector<int>& barIds, ElementType type);
PieceError groupPieces(Project& project, UndoStack& stack, const std::vector<int>& barIds, int* groupId = nullptr);
PieceError ungroupPieces(Project& project, UndoStack& stack, const std::vector<int>& barIds);
PieceError moveBarEnd(Project& project, UndoStack& stack, int barId, BarEnd end, int newNode);
PieceError invertBarAxis(Project& project, UndoStack& stack, int barId);

}  // namespace cc::core
