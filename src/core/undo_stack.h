#pragma once
#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace cc::core {

/// A reversible edit of the project (the "Deshacer" / "Rehacer" buttons of the quick-access bar).
/// Commands own the data needed to apply and revert themselves; the stack owns the commands.
class UndoCommand {
public:
    virtual ~UndoCommand() = default;
    /// Applies the edit. Called on push() and on every redo().
    virtual void redo() = 0;
    /// Reverts exactly what redo() did.
    virtual void undo() = 0;
    /// Text of the menu entry ("Deshacer <text>"); in Spanish, as shown in the GUI.
    virtual std::string text() const = 0;
    /// Commands with the same non-negative id may be merged (typing in a field, dragging a node). -1 = never merge.
    virtual int mergeId() const { return -1; }
    /// Absorbs `next` (already executed) into this command; returns false when it cannot.
    virtual bool mergeWith(const UndoCommand& next) { (void)next; return false; }
};

/// Linear undo history with a clean (saved) point, optional size limit, merging and macros.
/// Not thread-safe; single GUI thread.
class UndoStack {
public:
    /// `limit` = maximum number of commands kept (0 = unlimited). The oldest are dropped first.
    explicit UndoStack(std::size_t limit = 0);
    ~UndoStack();
    UndoStack(const UndoStack&) = delete;
    UndoStack& operator=(const UndoStack&) = delete;

    /// Executes `command` (redo()) and records it, discarding any redo history. Inside a macro the command joins the macro.
    void push(std::unique_ptr<UndoCommand> command);

    bool canUndo() const;
    bool canRedo() const;
    void undo();
    void redo();

    /// Texts of the commands that undo()/redo() would apply next; empty when not available.
    std::string undoText() const;
    std::string redoText() const;

    /// Groups the commands pushed until endMacro() into a single undo step named `text`. Macros may nest.
    void beginMacro(std::string text);
    void endMacro();
    bool inMacro() const { return !m_macroStack.empty(); }

    /// Marks the current state as saved; isClean() is true again only when the history returns to this point.
    void setClean();
    bool isClean() const;

    void clear();
    std::size_t count() const { return m_commands.size(); }
    /// Number of commands currently applied (undo() would revert the one at index()-1).
    std::size_t index() const { return m_index; }
    std::size_t limit() const { return m_limit; }
    void setLimit(std::size_t limit);

    /// Called after every change of the history (push, undo, redo, clear, setClean, macro end).
    void setChangeCallback(std::function<void()> callback) { m_onChange = std::move(callback); }

private:
    class Macro;
    void notify() const;
    void trim();

    std::vector<std::unique_ptr<UndoCommand>> m_commands;
    std::size_t m_index = 0;
    long m_cleanIndex = 0;  ///< -1 when the clean state was discarded (history trimmed or branch cut)
    std::size_t m_limit;
    std::vector<std::unique_ptr<Macro>> m_macroStack;  ///< open macros, innermost last
    std::function<void()> m_onChange;
};

}  // namespace cc::core
