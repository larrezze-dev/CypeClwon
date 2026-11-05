#pragma once
#include <string>
#include <vector>

namespace cc::core {

/// Data model of the "Edición de pilares/vigas" objects that sit on top of pieces (T-P5-08, core only). Behaviour source:
/// docs/research/ui/barras.md and pieza.md. Everything here is stored in its own project section "continuous_elements";
/// older files simply lack it and load as an empty set.
///
/// PROVISIONAL: the on-disk layout is the clone's own (the original .ed3 records are not decoded). References are
/// stored by id: piece ids and bar ids are never reused, but deleting or re-creating pieces can leave an entry
/// dangling; validateContinuous() (continuous_edit.h) reports it. Pruning after piece edits is ticketed.

/// Slab depth ("canto del forjado", #64855 / #65437) fixed by the user at a node of a pillar. Absent = automatic.
struct SlabDepth {
    int nodeId = 0;
    double depthM = 0.0;
    bool operator==(const SlabDepth&) const = default;
};

/// "Pilar continuo" (#64852): complete pillar pieces, ordered from the base upwards, described as a single pillar.
struct ContinuousColumn {
    int id = 0;
    std::vector<int> pieceIds;
    std::string reference;  ///< empty = automatic numbering
    bool operator==(const ContinuousColumn&) const = default;
};

/// "Viga continua" (#64862): connected beam pieces, ordered along their directrix.
struct ContinuousBeam {
    int id = 0;
    std::vector<int> pieceIds;
    std::string reference;  ///< empty = automatic numbering
    bool operator==(const ContinuousBeam&) const = default;
};

/// Reference typed by the user for a lone pillar or beam piece (one that belongs to no continuous element).
struct PieceReference {
    int pieceId = 0;
    std::string text;
    bool operator==(const PieceReference&) const = default;
};

/// Limiting deflection of a deflection group ("Flecha límite", #64343, #64874). NOT MEASURED: the original dialog was
/// not inspected, so the clone stores a relative limit (span / relative, e.g. 300) and an absolute one in millimetres;
/// 0 = that criterion is not set.
struct BeamDeflectionLimit {
    double relative = 0.0;
    double absoluteMm = 0.0;
    bool operator==(const BeamDeflectionLimit&) const = default;
    bool isSet() const { return relative > 0.0 || absoluteMm > 0.0; }
};

/// Explicit deflection group ("grupo de flecha") of beam bars with its own limit. By default each bar is its own
/// secant group with the limit of the general options (#64636), so only edited groups are stored.
struct DeflectionGroup {
    int id = 0;
    std::vector<int> barIds;  ///< sorted
    BeamDeflectionLimit limit;
    bool operator==(const DeflectionGroup&) const = default;
};

struct ContinuousElements {
    std::vector<ContinuousColumn> columns;          ///< sorted by id
    std::vector<ContinuousBeam> beams;              ///< sorted by id
    std::vector<PieceReference> pieceReferences;    ///< sorted by piece id
    std::vector<SlabDepth> slabDepths;              ///< sorted by node id
    std::vector<DeflectionGroup> deflectionGroups;  ///< sorted by id
    int nextColumnId = 1;
    int nextBeamId = 1;
    int nextGroupId = 1;

    const ContinuousColumn* findColumn(int id) const;
    const ContinuousBeam* findBeam(int id) const;
    const ContinuousColumn* columnOfPiece(int pieceId) const;
    const ContinuousBeam* beamOfPiece(int pieceId) const;
    const PieceReference* findPieceReference(int pieceId) const;
    const SlabDepth* findSlabDepth(int nodeId) const;
    const DeflectionGroup* groupOfBar(int barId) const;
    bool empty() const;
    /// Internal consistency (sorted unique ids, ids below the counters, positive values); the references to the
    /// structural model are checked by validateContinuous().
    bool isValid() const;
    bool operator==(const ContinuousElements&) const = default;
};

}  // namespace cc::core
