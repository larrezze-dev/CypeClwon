#include <gtest/gtest.h>

#include <memory>

#include "core/undo_stack.h"

using cc::core::UndoCommand;
using cc::core::UndoStack;

namespace {

/// Adds `delta` to a shared value; merges with other additions when `mergeable`.
class AddCommand : public UndoCommand {
public:
    AddCommand(int& target, int delta, bool mergeable = false, std::string name = "Sumar")
        : m_target(target), m_delta(delta), m_mergeable(mergeable), m_name(std::move(name)) {}
    void redo() override { m_target += m_delta; }
    void undo() override { m_target -= m_delta; }
    std::string text() const override { return m_name; }
    int mergeId() const override { return m_mergeable ? 7 : -1; }
    bool mergeWith(const UndoCommand& next) override {
        const auto* other = dynamic_cast<const AddCommand*>(&next);
        if (!other) return false;
        m_delta += other->m_delta;  // `next` was already applied to the target, so only the bookkeeping changes
        return true;
    }

private:
    int& m_target;
    int m_delta;
    bool m_mergeable;
    std::string m_name;
};

std::unique_ptr<UndoCommand> add(int& v, int d, bool merge = false, const char* name = "Sumar") {
    return std::make_unique<AddCommand>(v, d, merge, name);
}

}  // namespace

TEST(UndoStack, PushExecutesAndUndoRedoRestore) {
    int v = 0;
    UndoStack s;
    s.push(add(v, 5));
    s.push(add(v, 3));
    EXPECT_EQ(v, 8);
    s.undo();
    EXPECT_EQ(v, 5);
    s.undo();
    EXPECT_EQ(v, 0);
    EXPECT_FALSE(s.canUndo());
    s.redo();
    s.redo();
    EXPECT_EQ(v, 8);
    EXPECT_FALSE(s.canRedo());
}

TEST(UndoStack, UndoRedoOnEmptyStackAreNoOps) {
    UndoStack s;
    s.undo();
    s.redo();
    EXPECT_EQ(s.count(), 0u);
    EXPECT_EQ(s.undoText(), "");
    s.push(nullptr);
    EXPECT_EQ(s.count(), 0u);
}

TEST(UndoStack, NewEditDiscardsRedoBranch) {
    int v = 0;
    UndoStack s;
    s.push(add(v, 1));
    s.push(add(v, 2));
    s.undo();
    s.push(add(v, 10));
    EXPECT_EQ(v, 11);
    EXPECT_FALSE(s.canRedo());
    EXPECT_EQ(s.count(), 2u);
}

TEST(UndoStack, TextsDescribeTheNextSteps) {
    int v = 0;
    UndoStack s;
    s.push(add(v, 1, false, "Crear nudo"));
    s.push(add(v, 1, false, "Mover nudo"));
    EXPECT_EQ(s.undoText(), "Mover nudo");
    EXPECT_EQ(s.redoText(), "");
    s.undo();
    EXPECT_EQ(s.undoText(), "Crear nudo");
    EXPECT_EQ(s.redoText(), "Mover nudo");
}

TEST(UndoStack, CleanPointTracksSavedState) {
    int v = 0;
    UndoStack s;
    EXPECT_TRUE(s.isClean());
    s.push(add(v, 1));
    EXPECT_FALSE(s.isClean());
    s.setClean();
    EXPECT_TRUE(s.isClean());
    s.push(add(v, 1));
    EXPECT_FALSE(s.isClean());
    s.undo();
    EXPECT_TRUE(s.isClean());  // back at the saved state
    s.undo();
    EXPECT_FALSE(s.isClean());
}

TEST(UndoStack, CleanPointInCutBranchIsLost) {
    int v = 0;
    UndoStack s;
    s.push(add(v, 1));
    s.push(add(v, 1));
    s.setClean();  // saved after two edits
    s.undo();
    s.undo();
    s.push(add(v, 5));  // the branch holding the saved state is gone
    s.undo();
    s.redo();
    EXPECT_FALSE(s.isClean());
    s.setClean();
    EXPECT_TRUE(s.isClean());
}

TEST(UndoStack, LimitDropsOldestAndKeepsConsistency) {
    int v = 0;
    UndoStack s(2);
    s.push(add(v, 1));
    s.push(add(v, 2));
    s.push(add(v, 4));
    EXPECT_EQ(s.count(), 2u);
    s.undo();
    s.undo();
    EXPECT_FALSE(s.canUndo());
    EXPECT_EQ(v, 1);  // the first edit can no longer be undone
}

TEST(UndoStack, TrimmingKeepsOrLosesTheSavedState) {
    int v = 0;
    UndoStack kept(1);
    kept.push(add(v, 1));
    kept.setClean();         // saved after the first edit
    kept.push(add(v, 1));    // the first edit is dropped, but undoing the second returns to the saved state
    kept.undo();
    EXPECT_TRUE(kept.isClean());

    UndoStack lost(1);
    lost.setClean();         // saved before any edit
    lost.push(add(v, 1));
    lost.push(add(v, 1));    // dropping the first edit makes the saved state unreachable
    lost.undo();
    EXPECT_FALSE(lost.isClean());
}

TEST(UndoStack, MergesCommandsWithSameId) {
    int v = 0;
    UndoStack s;
    s.push(add(v, 1, true));
    s.push(add(v, 2, true));
    s.push(add(v, 3, true));
    EXPECT_EQ(s.count(), 1u);
    EXPECT_EQ(v, 6);
    s.undo();
    EXPECT_EQ(v, 0);
    s.redo();
    EXPECT_EQ(v, 6);
}

TEST(UndoStack, DoesNotMergeIntoTheSavedState) {
    int v = 0;
    UndoStack s;
    s.push(add(v, 1, true));
    s.setClean();
    s.push(add(v, 2, true));  // must stay a separate step so the saved state is reachable
    EXPECT_EQ(s.count(), 2u);
    s.undo();
    EXPECT_TRUE(s.isClean());
    EXPECT_EQ(v, 1);
}

TEST(UndoStack, MacroIsOneStep) {
    int v = 0;
    UndoStack s;
    s.beginMacro("Pegar");
    s.push(add(v, 1));
    s.push(add(v, 2));
    EXPECT_TRUE(s.inMacro());
    EXPECT_FALSE(s.canUndo());  // history is frozen while a macro is open
    s.endMacro();
    EXPECT_EQ(s.count(), 1u);
    EXPECT_EQ(s.undoText(), "Pegar");
    EXPECT_EQ(v, 3);
    s.undo();
    EXPECT_EQ(v, 0);
    s.redo();
    EXPECT_EQ(v, 3);
}

TEST(UndoStack, NestedMacrosAndEmptyMacro) {
    int v = 0;
    UndoStack s;
    s.beginMacro("Fuera");
    s.push(add(v, 1));
    s.beginMacro("Dentro");
    s.push(add(v, 10));
    s.endMacro();
    s.endMacro();
    EXPECT_EQ(s.count(), 1u);
    s.undo();
    EXPECT_EQ(v, 0);
    s.beginMacro("Vacio");
    s.endMacro();
    EXPECT_EQ(s.count(), 1u);  // an empty macro leaves no entry
    s.endMacro();              // unbalanced end is harmless
}

TEST(UndoStack, ChangeCallbackFiresOnEveryChange) {
    int v = 0, calls = 0;
    UndoStack s;
    s.setChangeCallback([&] { ++calls; });
    s.push(add(v, 1));
    s.undo();
    s.redo();
    s.setClean();
    s.clear();
    EXPECT_EQ(calls, 5);
    EXPECT_EQ(s.count(), 0u);
    EXPECT_TRUE(s.isClean());
}

TEST(UndoStack, ClearWithOpenMacroDoesNotLeakOrCrash) {
    int v = 0;
    UndoStack s;
    s.beginMacro("Abierto");
    s.push(add(v, 1));
    s.clear();
    EXPECT_FALSE(s.inMacro());
    s.push(add(v, 1));
    EXPECT_EQ(s.count(), 1u);
}
