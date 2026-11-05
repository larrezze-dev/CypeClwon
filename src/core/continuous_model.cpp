#include "core/continuous_model.h"

#include <algorithm>
#include <cmath>
#include <set>

namespace cc::core {

namespace {

template <class T>
const T* byId(const std::vector<T>& items, int id) {
    auto it = std::lower_bound(items.begin(), items.end(), id, [](const T& a, int v) { return a.id < v; });
    return it != items.end() && it->id == id ? &*it : nullptr;
}

template <class T>
bool idsOk(const std::vector<T>& items, int next) {
    int previous = 0;
    for (const T& item : items) {
        if (item.id <= previous || item.id >= next) return false;
        previous = item.id;
    }
    return true;
}

}  // namespace

const ContinuousColumn* ContinuousElements::findColumn(int id) const { return byId(columns, id); }
const ContinuousBeam* ContinuousElements::findBeam(int id) const { return byId(beams, id); }

const ContinuousColumn* ContinuousElements::columnOfPiece(int pieceId) const {
    for (const ContinuousColumn& c : columns)
        if (std::find(c.pieceIds.begin(), c.pieceIds.end(), pieceId) != c.pieceIds.end()) return &c;
    return nullptr;
}

const ContinuousBeam* ContinuousElements::beamOfPiece(int pieceId) const {
    for (const ContinuousBeam& b : beams)
        if (std::find(b.pieceIds.begin(), b.pieceIds.end(), pieceId) != b.pieceIds.end()) return &b;
    return nullptr;
}

const PieceReference* ContinuousElements::findPieceReference(int pieceId) const {
    auto it = std::lower_bound(pieceReferences.begin(), pieceReferences.end(), pieceId,
                               [](const PieceReference& a, int v) { return a.pieceId < v; });
    return it != pieceReferences.end() && it->pieceId == pieceId ? &*it : nullptr;
}

const SlabDepth* ContinuousElements::findSlabDepth(int nodeId) const {
    auto it = std::lower_bound(slabDepths.begin(), slabDepths.end(), nodeId,
                               [](const SlabDepth& a, int v) { return a.nodeId < v; });
    return it != slabDepths.end() && it->nodeId == nodeId ? &*it : nullptr;
}

const DeflectionGroup* ContinuousElements::groupOfBar(int barId) const {
    for (const DeflectionGroup& g : deflectionGroups)
        if (std::binary_search(g.barIds.begin(), g.barIds.end(), barId)) return &g;
    return nullptr;
}

bool ContinuousElements::empty() const {
    return columns.empty() && beams.empty() && pieceReferences.empty() && slabDepths.empty() && deflectionGroups.empty();
}

bool ContinuousElements::isValid() const {
    if (!idsOk(columns, nextColumnId) || !idsOk(beams, nextBeamId) || !idsOk(deflectionGroups, nextGroupId)) return false;
    for (const ContinuousColumn& c : columns)
        if (c.pieceIds.size() < 2) return false;
    for (const ContinuousBeam& b : beams)
        if (b.pieceIds.size() < 2) return false;
    int previous = 0;
    for (const PieceReference& r : pieceReferences) {
        if (r.pieceId <= previous || r.text.empty()) return false;
        previous = r.pieceId;
    }
    previous = 0;
    for (const SlabDepth& d : slabDepths) {
        if (d.nodeId <= previous || !(d.depthM > 0.0) || !std::isfinite(d.depthM)) return false;
        previous = d.nodeId;
    }
    for (const DeflectionGroup& g : deflectionGroups) {
        if (g.barIds.empty() || !std::is_sorted(g.barIds.begin(), g.barIds.end())) return false;
        if (std::adjacent_find(g.barIds.begin(), g.barIds.end()) != g.barIds.end()) return false;
        if (!std::isfinite(g.limit.relative) || !std::isfinite(g.limit.absoluteMm) || g.limit.relative < 0.0 ||
            g.limit.absoluteMm < 0.0 || !g.limit.isSet())
            return false;
    }
    // a piece belongs to at most one continuous element of its kind; a bar to at most one deflection group
    std::set<int> seen;
    for (const ContinuousColumn& c : columns)
        for (int id : c.pieceIds)
            if (!seen.insert(id).second) return false;
    seen.clear();
    for (const ContinuousBeam& b : beams)
        for (int id : b.pieceIds)
            if (!seen.insert(id).second) return false;
    seen.clear();
    for (const DeflectionGroup& g : deflectionGroups)
        for (int id : g.barIds)
            if (!seen.insert(id).second) return false;
    return true;
}

}  // namespace cc::core
