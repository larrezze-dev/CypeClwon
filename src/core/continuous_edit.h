#pragma once
#include <optional>
#include <string>
#include <vector>

#include "core/project.h"
#include "core/undo_stack.h"

namespace cc::core {

/// Undoable commands of "Edición de pilares/vigas" (T-P5-08, core only): continuous pillars and beams, references and
/// numbering, slab depth at pillar nodes and the limiting deflection of beam deflection groups. Behaviour source:
/// docs/research/ui/barras.md, pieza.md and docs/research/ui/data/strings_merged_latin.tsv (message ids quoted below).
/// Data lives in Project::continuous (continuous_model.h, section "continuous_elements").
///
/// Every command edits a copy of Project::continuous when it is constructed and stores before / after, so redo / undo
/// are exact. As with the other edit commands, one must be pushed before another is constructed.
///
/// PROVISIONAL (not measured in the original; each is marked at its definition):
///  - the local-axes convention behind the "plane xz is
///    vertical" test;
///  - the order in which checks run and the clone's own messages (the ones without an original id);
///  - the order of automatic numbering, the default prefixes and what "Numerar" does with the reference it is given;
///  - the format of the deflection limit (BeamDeflectionLimit) and of the saved section.

/// Largest angle between a beam of a continuous beam and the horizontal plane (#64021 "... forma un ángulo con el plano
/// horizontal superior a %s"). Verified by REA (edicion_pilares_vigas.md section 1): 80 degrees; the original refuses
/// when angle - 0.0001 rad >= 80 degrees.
inline constexpr double kMaxContinuousBeamAngleDeg = 80.0;
inline constexpr double kBeamAngleSlackRad = 1e-4;
/// A pillar piece is vertical when |dx| and |dy| between its ends are below this (metres); verified by REA.
inline constexpr double kVerticalToleranceM = 0.0005;
/// Tolerance of "the local plane xz is vertical", degrees: refused from 0.5 degrees on (#64026); verified by REA.
inline constexpr double kPlaneVerticalToleranceDeg = 0.5;
/// Longest reference the original editor accepts (30 characters, REA); an empty one is refused (#64971).
inline constexpr std::size_t kMaxReferenceLength = 30;
/// Default prefixes of the automatic numbering. NOT MEASURED: the original reads the prefix of the beams from the
/// general data of the work (#64879), which the clone does not store yet; the pillar prefix is a guess.
inline constexpr const char* kDefaultPillarPrefix = "P";
inline constexpr const char* kDefaultBeamPrefix = "V";

/// Why a command was refused. Texts: see continuousErrorMessage.
enum class ContinuousError {
    None,
    UnknownElement,       ///< a node, bar, piece or element does not exist (no original message)
    NotPillar,            ///< #64233 some piece between the nodes is not of type 'pilar'
    AlreadyExists,        ///< #64234 a pillar with the same pieces exists (also: a lone piece is already a pillar / beam)
    IncompletePieces,     ///< #64235 the nodes cut a piece: whole pieces are needed
    NotAlignedConnected,  ///< #64027 pieces not aligned / connected through their end nodes
    NotVertical,          ///< #64985 some piece is not vertical
    NotAscending,         ///< #64986 / #64988 local x axis not pointing up
    NotBeam,              ///< #64023 with '%s' = viga
    NotConnected,         ///< #64864 the beams are not a chain (end node of one = start node of the next)
    MixedMaterial,        ///< #64025 not all of the same material class
    PlaneNotVertical,     ///< #64026 the local xz plane of a piece is not vertical
    TooSteep,             ///< #64021 angle with the horizontal above kMaxContinuousBeamAngleDeg
    TooFewPieces,         ///< a continuous beam needs two or more different pieces (no original message)
    PieceInOtherElement,  ///< a piece already belongs to another continuous element (no original message)
    PartOfContinuous,     ///< the piece belongs to a continuous element: edit the reference of that element
    NotPillarNode,        ///< the node does not belong to a pillar
    NotBeamBars,          ///< some bar is not part of a beam
    GroupConflict,        ///< a bar is already in a different deflection group
    InvalidReference,     ///< "Numerar" needs a reference ending in a number
    EmptyReference,       ///< #64971 "La referencia no puede quedar vacía."
    ReferenceTooLong,     ///< more than kMaxReferenceLength characters
    InvalidValue,         ///< a non-finite or non-positive value
    NoElements,           ///< nothing to number
    NothingToChange
};

/// Spanish text of an error: original string when it has one. `detail` replaces the %s of #64021 (angle, e.g. "45°");
/// when empty the provisional limit is used.
std::string continuousErrorMessage(ContinuousError error, const std::string& detail = {});

// --- queries -------------------------------------------------------------------------------------------------------

/// Which object a reference belongs to. `Piece` is a lone pillar / beam piece (one that is in no continuous element).
struct ReferenceTarget {
    enum class Kind { ContinuousColumn, ContinuousBeam, Piece };
    Kind kind = Kind::Piece;
    int id = 0;
    bool operator==(const ReferenceTarget&) const = default;
};

/// Pillars of the work in numbering order: continuous columns and lone pieces of type pillar, sorted by the base node
/// (y, then x, then z; #65039 "de menor a mayor coordenada Y y, a igualdad, de menor a mayor X").
std::vector<ReferenceTarget> listPillars(const Project& project);
/// Beams of the work: continuous beams and lone pieces of type beam, sorted by the start node (x, y, z). PROVISIONAL.
std::vector<ReferenceTarget> listBeams(const Project& project);
/// Reference of an element: the one set by the user, otherwise automatic = `prefix` + 1-based position in listPillars /
/// listBeams (PROVISIONAL scheme). Empty for an unknown target.
std::string elementReference(const Project& project, const ReferenceTarget& target, const std::string& prefix);
/// Bar ids (sorted) of a beam target (a continuous beam or a lone beam piece): the bars of one deflection group when the
/// user edits "Flecha límite" for that beam.
std::vector<int> beamBarIds(const Project& project, const ReferenceTarget& target);
/// Limit of the explicit deflection group of a bar; nullopt when the bar keeps the default group.
std::optional<BeamDeflectionLimit> deflectionLimitOfBar(const Project& project, int barId);
/// Slab depth set by the user at a node; nullopt = automatic.
std::optional<double> slabDepthAt(const Project& project, int nodeId);
/// Empty when Project::continuous is consistent with the structural model, otherwise a Spanish message naming the first
/// problem (dangling piece / bar / node ids, wrong element type). Pieces edited after the elements were created can
/// make entries dangle; pruning is not implemented yet (ticket).
std::string validateContinuous(const Project& project);

// --- commands ------------------------------------------------------------------------------------------------------

/// Shared machinery: subclasses edit a copy of Project::continuous in their constructor and call capture().
class ContinuousDiffCommand : public UndoCommand {
public:
    bool valid() const { return m_error == ContinuousError::None; }
    ContinuousError error() const { return m_error; }
    void redo() override;
    void undo() override;

protected:
    explicit ContinuousDiffCommand(Project& project) : m_project(project) {}
    void capture(ContinuousElements after);
    Project& m_project;
    ContinuousError m_error = ContinuousError::None;

private:
    ContinuousElements m_before, m_after;
};

/// "Crear pilar continuo" (#64852-64854): groups the existing pillar pieces between the start node of the first piece and
/// the end node of the last one. Check order (PROVISIONAL): aligned chain exists (#64027), nodes vertical (#64985), end
/// above start (#64986), every piece complete (#64235) and of type pillar (#64233, a bar with no piece record is a
/// generic piece), every piece with its local x axis pointing up (#64986), two or more pieces and not yet a pillar
/// (#64234), none already in another continuous column.
class CreateContinuousColumnCommand : public ContinuousDiffCommand {
public:
    CreateContinuousColumnCommand(Project& project, int startNode, int endNode);
    int columnId() const { return m_id; }  ///< 0 when invalid
    std::string text() const override { return "Crear pilar continuo"; }

private:
    int m_id = 0;
};

/// "Crear viga continua" (#64862-64864): groups the pieces of the given bars (selection order does not matter; they are
/// ordered along their directrix). Check order per piece, stopping at the first failure (REA): angle with the horizontal
/// (#64021), plane xz vertical (#64026), type beam (#64023); then two or more pieces, a chain (#64864), same material
/// class (#64025, order PROVISIONAL; an unassigned material counts as MaterialKind::Generic), not already a continuous
/// beam / none in another one. PROVISIONAL: the local axes convention (roll multiple of 180 degrees = plane xz vertical).
class CreateContinuousBeamCommand : public ContinuousDiffCommand {
public:
    CreateContinuousBeamCommand(Project& project, const std::vector<int>& barIds);
    int beamId() const { return m_id; }
    /// Angle text for #64021 (non-empty only when error() == TooSteep): the offending angle, e.g. "60°".
    const std::string& detail() const { return m_detail; }
    std::string text() const override { return "Crear viga continua"; }

private:
    int m_id = 0;
    std::string m_detail;
};

/// "Editar referencia" (#65436) / "Editar referencias de vigas" (#64878) / "... de vigas continuas" (#64880). An empty
/// text is refused (EmptyReference, #64971) and so is one over kMaxReferenceLength characters. A lone piece must be of type pillar or beam and in no continuous element.
class SetReferenceCommand : public ContinuousDiffCommand {
public:
    SetReferenceCommand(Project& project, const ReferenceTarget& target, const std::string& text);
    std::string text() const override { return "Editar referencia"; }
};

/// "Numerar" (#64861): gives every pillar of the work, in listPillars order, the reference `startReference`, +1, +2...
/// The reference must end in digits ("P1", "Pilar 07": prefix kept, number incremented, leading zeros kept) -
/// PROVISIONAL reading of "a partir de una referencia inicial dada".
class NumberPillarsCommand : public ContinuousDiffCommand {
public:
    NumberPillarsCommand(Project& project, const std::string& startReference);
    int count() const { return m_count; }
    std::string text() const override { return "Numerar pilares"; }

private:
    int m_count = 0;
};

/// "Editar canto del forjado" (#64855 / #65437): fixes the slab depth (metres) at a node of a pillar; nullopt goes back
/// to the depth the program determines automatically.
class SetSlabDepthCommand : public ContinuousDiffCommand {
public:
    SetSlabDepthCommand(Project& project, int nodeId, std::optional<double> depthM);
    std::string text() const override { return "Editar canto del forjado"; }
};

/// "Flecha límite" (#64874, #64343): sets the limit of the deflection group made of exactly `barIds` (bars of beam
/// pieces; use beamBarIds()). The group is created when absent; a limit with nothing set removes the explicit group
/// (back to the default one bar one group). GroupConflict when a bar is already in a group with different bars.
class SetDeflectionLimitCommand : public ContinuousDiffCommand {
public:
    SetDeflectionLimitCommand(Project& project, const std::vector<int>& barIds, const BeamDeflectionLimit& limit);
    std::string text() const override { return "Editar flecha límite"; }
};

/// Wrappers: build the command, push it on `stack` and return None; on error nothing is pushed and the error is returned.
ContinuousError createContinuousColumn(Project& project, UndoStack& stack, int startNode, int endNode, int* columnId = nullptr);
ContinuousError createContinuousBeam(Project& project, UndoStack& stack, const std::vector<int>& barIds, int* beamId = nullptr,
                                     std::string* detail = nullptr);
ContinuousError setReference(Project& project, UndoStack& stack, const ReferenceTarget& target, const std::string& text);
ContinuousError numberPillars(Project& project, UndoStack& stack, const std::string& startReference, int* count = nullptr);
ContinuousError setSlabDepth(Project& project, UndoStack& stack, int nodeId, std::optional<double> depthM);
ContinuousError setDeflectionLimit(Project& project, UndoStack& stack, const std::vector<int>& barIds, const BeamDeflectionLimit& limit);

}  // namespace cc::core
