#pragma once
#include <memory>
#include <string>
#include <vector>

#include "core/project.h"
#include "core/undo_stack.h"

namespace cc::core {

/// Undoable bar operations of "Geometria > Barras" (T-P5-03, core only). Behaviour source: docs/research/ui/barras.md.
/// New bars are "not described" (materialId = sectionId = 0, #65277) and belong to the active layer. Ids are reserved when
/// the command is constructed (next free id of the model) and are never reused; collections stay sorted by id.
/// A command must be pushed before another one is constructed (as with AddNodeCommand).

/// Shortest bar the original accepts (#64000 "La longitud de la barra es inferior a la longitud mínima permitida").
/// NOT MEASURED: the numeric value was not read; provisional, equal to the model limit.
inline constexpr double kMinBarLengthM = 1e-6;
/// Distance (metres) under which an existing node counts as lying on a new bar when "Generar nudos en puntos de corte"
/// splits it. NOT MEASURED in the original: provisional.
inline constexpr double kCutNodeToleranceM = 1e-6;

/// "Barra": adds a bar between two existing nodes.
/// With `generateCutNodes` (option #64638, default on) the bar is split at every existing node lying on it, one bar per
/// stretch (a stretch that already exists as a bar is skipped). Bar-bar crossings are NOT handled (ticket: needs new nodes
/// at intersections). Without the option the bar is added whole.
class AddBarCommand : public UndoCommand {
public:
    AddBarCommand(Project& project, int startNode, int endNode, bool generateCutNodes = true);
    /// False when a node does not exist, both are the same, the length is below kMinBarLengthM, a bar already joins the
    /// pair (either direction) or, after splitting, no new stretch remains.
    bool valid() const { return m_valid; }
    /// Ids of the bars created, in order from start to end (empty when invalid).
    std::vector<int> barIds() const;
    void redo() override;
    void undo() override;
    std::string text() const override { return "Nueva barra"; }

private:
    Project& m_project;
    std::vector<Bar> m_bars;
    std::vector<Piece> m_pieces;  // the divisions of one drawn bar form a single piece
    bool m_valid = false;
};

/// Piece marks (T-P5-04): the divisions of a bar split by "Generar nudos en puntos de corte" form one piece; "Viga" marks
/// the pieces it creates ElementType::Beam and "Pilar" ElementType::Pillar (one piece per bar). Deleting a bar splits or
/// drops its piece (StructuralModel::removeBar); undo restores the piece records.

/// "Viga": one bar per pair of consecutive nodes of an ordered list (continuous beam), in one command. Consecutive equal
/// nodes and pairs that already have a bar (existing or earlier in the list) are skipped. With `generateCutNodes` each
/// bar is split at existing nodes lying on it like AddBarCommand does.
class AddBeamChainCommand : public UndoCommand {
public:
    AddBeamChainCommand(Project& project, const std::vector<int>& nodeIds, bool generateCutNodes = true);
    /// False when fewer than 2 nodes are given, a node does not exist, a pair is shorter than kMinBarLengthM, or every
    /// pair was skipped.
    bool valid() const { return m_valid; }
    std::vector<int> barIds() const;
    void redo() override;
    void undo() override;
    std::string text() const override { return "Nueva viga"; }

private:
    Project& m_project;
    std::vector<Bar> m_bars;
    std::vector<Piece> m_pieces;  // marked ElementType::Beam
    bool m_valid = false;
};

/// "Pilar": a column at plan position (x, y) between levels `levelsZ` (heights, any order). Needs at least two distinct
/// heights (otherwise invalid, error() is #65140). Reuses the node existing at (x, y, z) or creates one; one bar per pair
/// of consecutive heights with the local x axis ascending (start = lower node). Pairs that already have a bar are
/// skipped. One undoable command.
class AddColumnCommand : public UndoCommand {
public:
    AddColumnCommand(Project& project, double x, double y, std::vector<double> levelsZ);
    bool valid() const { return m_valid; }
    /// Spanish message of the original (#65140) when fewer than two distinct levels were given; empty otherwise.
    const std::string& error() const { return m_error; }
    std::vector<int> barIds() const;
    /// Ids of the nodes created by the command (reused nodes are not listed).
    std::vector<int> createdNodeIds() const;
    void redo() override;
    void undo() override;
    std::string text() const override { return "Nuevo pilar"; }

private:
    Project& m_project;
    std::vector<Node> m_nodes;  // new nodes only
    std::vector<Bar> m_bars;
    std::vector<Piece> m_pieces;  // one piece per bar, marked ElementType::Pillar
    std::string m_error;
    bool m_valid = false;
};

/// "Borrar": deletes bars; the nodes stay (#64619).
class DeleteBarsCommand : public UndoCommand {
public:
    DeleteBarsCommand(Project& project, std::vector<int> barIds);
    /// False when no given id exists.
    bool valid() const { return !m_bars.empty(); }
    void redo() override;
    void undo() override;
    std::string text() const override { return "Borrar barras"; }

private:
    Project& m_project;
    std::vector<Bar> m_bars;
    std::vector<Piece> m_piecesBefore;
    int m_nextPieceBefore = 1;
    BarLoads m_barLoadsBefore, m_barLoadsAfter;  ///< the loads of the deleted bars go without warning
};

/// Wrappers: build the command, push it on `stack`; empty vector / false (and nothing pushed) when invalid.
std::vector<int> addBar(Project& project, UndoStack& stack, int startNode, int endNode, bool generateCutNodes = true);
std::vector<int> addBeamChain(Project& project, UndoStack& stack, const std::vector<int>& nodeIds,
                              bool generateCutNodes = true);
std::vector<int> addColumn(Project& project, UndoStack& stack, double x, double y, const std::vector<double>& levelsZ);
bool deleteBars(Project& project, UndoStack& stack, const std::vector<int>& barIds);

}  // namespace cc::core
