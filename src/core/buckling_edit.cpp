#include "core/buckling_edit.h"

#include <algorithm>
#include <memory>

#include "core/bar_ends_edit.h"  // existingBars

namespace cc::core {

namespace {

template <typename Apply>
void pushEdit(Project& project, UndoStack& stack, const std::vector<int>& barIds, const char* text, Apply apply) {
    const std::vector<int> ids = existingBars(project, barIds);
    if (ids.empty()) return;
    BucklingData after = project.buckling;
    for (int id : ids) {
        BarBuckling rec = bucklingOf(after, id);
        apply(rec);
        setBuckling(after, id, rec);
    }
    auto cmd = std::make_unique<BucklingCommand>(project, std::move(after), text);
    if (!cmd->valid()) return;
    stack.push(std::move(cmd));
}

}  // namespace

std::optional<BucklingIssue> assignBuckling(Project& project, UndoStack& stack, const std::vector<int>& barIds, const BucklingEdit& edit) {
    if (auto issue = checkBucklingEdit(edit)) return issue;
    pushEdit(project, stack, barIds, "Pandeo", [&](BarBuckling& r) { applyBucklingEdit(r, edit); });
    return std::nullopt;
}

std::optional<BucklingIssue> assignLateralBuckling(Project& project, UndoStack& stack, const std::vector<int>& barIds, const LateralEdit& edit) {
    if (auto issue = checkLateralEdit(edit)) return issue;
    pushEdit(project, stack, barIds, "Pandeo lateral", [&](BarBuckling& r) { applyLateralEdit(r, edit); });
    return std::nullopt;
}

}  // namespace cc::core
