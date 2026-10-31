#include "selection.h"

namespace cc::core {

bool SelectionFilter::allows(ElementKind kind) const {
    switch (kind) {
        case ElementKind::Node: return nodes;
        case ElementKind::Bar: return bars;
        case ElementKind::Shell: return shells;
    }
    return false;
}

SelectionFilter SelectionFilter::only(ElementKind kind) {
    return {kind == ElementKind::Node, kind == ElementKind::Bar, kind == ElementKind::Shell};
}

std::vector<int> Selection::ids(ElementKind kind) const {
    std::vector<int> out;
    for (const ElementRef& e : m_items)
        if (e.kind == kind) out.push_back(e.id);
    return out;
}

std::size_t Selection::count(ElementKind kind) const {
    std::size_t n = 0;
    for (const ElementRef& e : m_items) n += e.kind == kind;
    return n;
}

bool Selection::apply(const std::vector<ElementRef>& hits, SelectMode mode) {
    const std::set<ElementRef> before = m_items;
    if (mode == SelectMode::Replace) m_items.clear();
    for (const ElementRef& h : hits) {
        if (mode == SelectMode::Remove) m_items.erase(h);
        else m_items.insert(h);
    }
    return m_items != before;
}

bool Selection::restrictTo(const SelectionFilter& filter) {
    bool changed = false;
    for (auto it = m_items.begin(); it != m_items.end();) {
        if (filter.allows(it->kind)) {
            ++it;
        } else {
            it = m_items.erase(it);
            changed = true;
        }
    }
    return changed;
}

}  // namespace cc::core
