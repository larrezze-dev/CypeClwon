#pragma once
#include <compare>
#include <cstddef>
#include <set>
#include <vector>

namespace cc::core {

enum class ElementKind { Node, Bar, Shell };

/// Reference to one element of the structural model (kind + id).
struct ElementRef {
    ElementKind kind = ElementKind::Node;
    int id = 0;
    auto operator<=>(const ElementRef&) const = default;
};

/// Which kinds of element a selection command accepts.
struct SelectionFilter {
    bool nodes = true;
    bool bars = true;
    bool shells = true;
    bool allows(ElementKind kind) const;
    static SelectionFilter only(ElementKind kind);
};

/// How a set of hits combines with the current selection.
enum class SelectMode {
    Replace,  ///< plain click or window: the hits become the selection
    Add,      ///< Ctrl: the hits are added
    Remove    ///< Shift: the hits are removed
};

/// Ordered set of selected elements.
class Selection {
public:
    void clear() { m_items.clear(); }
    bool empty() const { return m_items.empty(); }
    std::size_t size() const { return m_items.size(); }
    bool contains(const ElementRef& e) const { return m_items.count(e) != 0; }
    const std::set<ElementRef>& items() const { return m_items; }
    std::vector<int> ids(ElementKind kind) const;
    std::size_t count(ElementKind kind) const;

    /// Combines `hits` with the selection. Returns true when the selection changed.
    bool apply(const std::vector<ElementRef>& hits, SelectMode mode);
    /// Drops every element rejected by the filter (e.g. when the filter changes). Returns true when something went.
    bool restrictTo(const SelectionFilter& filter);

private:
    std::set<ElementRef> m_items;
};

}  // namespace cc::core
