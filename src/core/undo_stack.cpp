#include "core/undo_stack.h"

#include <utility>

namespace cc::core {

/// Several commands applied and reverted as one.
class UndoStack::Macro : public UndoCommand {
public:
    explicit Macro(std::string text) : m_text(std::move(text)) {}
    void add(std::unique_ptr<UndoCommand> c) { m_children.push_back(std::move(c)); }
    bool empty() const { return m_children.empty(); }
    void redo() override {
        for (auto& c : m_children) c->redo();
    }
    void undo() override {
        for (auto it = m_children.rbegin(); it != m_children.rend(); ++it) (*it)->undo();
    }
    std::string text() const override { return m_text; }

private:
    std::string m_text;
    std::vector<std::unique_ptr<UndoCommand>> m_children;
};

UndoStack::UndoStack(std::size_t limit) : m_limit(limit) {}
UndoStack::~UndoStack() = default;

void UndoStack::notify() const {
    if (m_onChange) m_onChange();
}

void UndoStack::push(std::unique_ptr<UndoCommand> command) {
    if (!command) return;
    command->redo();
    if (!m_macroStack.empty()) {  // joins the innermost open macro
        m_macroStack.back()->add(std::move(command));
        return;
    }
    // A new edit cuts the redo branch; if the clean point was in it, it can never be reached again.
    if (m_index < m_commands.size()) {
        if (m_cleanIndex > static_cast<long>(m_index)) m_cleanIndex = -1;
        m_commands.resize(m_index);
    }
    // Never merge into the command that produced the saved state: that state must stay reachable.
    const bool mergeable = !m_commands.empty() && command->mergeId() >= 0 &&
                           m_commands.back()->mergeId() == command->mergeId() && m_cleanIndex != static_cast<long>(m_index);
    if (mergeable && m_commands.back()->mergeWith(*command)) {
        notify();
        return;
    }
    m_commands.push_back(std::move(command));
    ++m_index;
    trim();
    notify();
}

void UndoStack::trim() {
    if (m_limit == 0) return;
    while (m_commands.size() > m_limit) {
        m_commands.erase(m_commands.begin());
        --m_index;
        if (m_cleanIndex > 0) --m_cleanIndex;
        else if (m_cleanIndex == 0) m_cleanIndex = -1;  // the saved state is no longer reachable
    }
}

bool UndoStack::canUndo() const { return m_macroStack.empty() && m_index > 0; }
bool UndoStack::canRedo() const { return m_macroStack.empty() && m_index < m_commands.size(); }

void UndoStack::undo() {
    if (!canUndo()) return;
    m_commands[--m_index]->undo();
    notify();
}

void UndoStack::redo() {
    if (!canRedo()) return;
    m_commands[m_index++]->redo();
    notify();
}

std::string UndoStack::undoText() const { return canUndo() ? m_commands[m_index - 1]->text() : std::string(); }
std::string UndoStack::redoText() const { return canRedo() ? m_commands[m_index]->text() : std::string(); }

void UndoStack::beginMacro(std::string text) { m_macroStack.push_back(std::make_unique<Macro>(std::move(text))); }

void UndoStack::endMacro() {
    if (m_macroStack.empty()) return;
    std::unique_ptr<Macro> macro = std::move(m_macroStack.back());
    m_macroStack.pop_back();
    if (macro->empty()) return;  // nothing was done: leave no entry
    // Children were already executed by push(); store the macro without running it again.
    if (!m_macroStack.empty()) {
        m_macroStack.back()->add(std::move(macro));
        return;
    }
    if (m_index < m_commands.size()) {
        if (m_cleanIndex > static_cast<long>(m_index)) m_cleanIndex = -1;
        m_commands.resize(m_index);
    }
    m_commands.push_back(std::move(macro));
    ++m_index;
    trim();
    notify();
}

void UndoStack::setClean() {
    m_cleanIndex = static_cast<long>(m_index);
    notify();
}

bool UndoStack::isClean() const { return m_cleanIndex >= 0 && static_cast<std::size_t>(m_cleanIndex) == m_index; }

void UndoStack::clear() {
    m_macroStack.clear();  // open macros are abandoned (their children stay applied)
    m_commands.clear();
    m_index = 0;
    m_cleanIndex = 0;
    notify();
}

void UndoStack::setLimit(std::size_t limit) {
    m_limit = limit;
    trim();
    notify();
}

}  // namespace cc::core
