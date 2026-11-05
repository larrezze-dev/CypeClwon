#include "core/reference_edit.h"

#include <algorithm>
#include <cctype>

#include "core/work_planes.h"

namespace cc::core {

namespace {
std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}
std::string trimmed(const std::string& s) {
    const auto first = s.find_first_not_of(" \t");
    if (first == std::string::npos) return {};
    return s.substr(first, s.find_last_not_of(" \t") - first + 1);
}
}  // namespace

std::string referenceErrorMessage(ReferenceError error) {
    switch (error) {
        case ReferenceError::None: return {};
        case ReferenceError::UnknownElement: return "El elemento indicado no existe.";
        case ReferenceError::EmptyReference: return "La referencia no puede estar vacía.";
        case ReferenceError::NothingToChange: return "No hay nada que modificar.";
    }
    return {};
}

std::string elementReference(const StructuralModel& model, ReferenceKind kind, int id) {
    if (kind == ReferenceKind::Node) {
        const Node* n = model.findNode(id);
        if (!n) return {};
        return n->reference.empty() ? "N" + std::to_string(id) : n->reference;
    }
    const Shell* s = model.findShell(id);
    if (!s) return {};
    return s->reference.empty() ? "L" + std::to_string(id) : s->reference;
}

std::vector<int> findByReference(const StructuralModel& model, ReferenceKind kind, const std::string& text, bool wholeWord) {
    std::vector<int> out;
    const std::string needle = lower(trimmed(text));
    if (needle.empty()) return out;
    const auto test = [&](int id) {
        const std::string ref = lower(elementReference(model, kind, id));
        return wholeWord ? ref == needle : ref.find(needle) != std::string::npos;
    };
    if (kind == ReferenceKind::Node) {
        for (const Node& n : model.nodes)
            if (test(n.id)) out.push_back(n.id);
    } else {
        for (const Shell& s : model.shells)
            if (test(s.id)) out.push_back(s.id);
    }
    return out;
}

std::vector<std::string> consecutiveReferences(const std::string& first, std::size_t count) {
    std::vector<std::string> out;
    std::string current = trimmed(first);
    for (std::size_t i = 0; i < count; ++i) {
        out.push_back(current);
        current = nextReference(current);
    }
    return out;
}

SetReferencesCommand::SetReferencesCommand(Project& project, ReferenceKind kind, const std::vector<int>& ids, const std::vector<std::string>& texts)
    : SnapshotCommand(project, "Asignar referencia") {
    if (ids.empty() || ids.size() != texts.size()) {
        m_error = ReferenceError::NothingToChange;
        return;
    }
    bool changed = false;
    for (std::size_t i = 0; i < ids.size(); ++i) {
        const std::string text = trimmed(texts[i]);
        if (text.empty()) {
            m_error = ReferenceError::EmptyReference;
            return;
        }
        const std::string automatic = (kind == ReferenceKind::Node ? "N" : "L") + std::to_string(ids[i]);
        const std::string stored = text == automatic ? std::string() : text;
        std::string* target = nullptr;
        if (kind == ReferenceKind::Node) {
            if (Node* n = work.findNode(ids[i])) target = &n->reference;
        } else if (Shell* s = work.findShell(ids[i])) {
            target = &s->reference;
        }
        if (!target) {
            m_error = ReferenceError::UnknownElement;
            return;
        }
        if (*target != stored) {
            *target = stored;
            changed = true;
        }
    }
    if (!changed) {
        m_error = ReferenceError::NothingToChange;
        return;
    }
    commit();
}

ReferenceError assignReferences(Project& project, UndoStack& stack, ReferenceKind kind, const std::vector<int>& ids,
                                const std::vector<std::string>& texts) {
    auto cmd = std::make_unique<SetReferencesCommand>(project, kind, ids, texts);
    if (!cmd->valid()) return cmd->error();
    stack.push(std::move(cmd));
    return ReferenceError::None;
}

}  // namespace cc::core
