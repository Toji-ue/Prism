#include "history/History.h"

#include <stdexcept>

namespace prism::core {

void History::execute(std::unique_ptr<Command> command) {
    if (!command)
        throw std::invalid_argument("History::execute: null command");

    // Drop the redo tail: a new action forks the timeline.
    m_entries.resize(m_index);
    m_states.resize(m_index + 1);
    command->redo();
    m_entries.push_back(std::move(command));
    m_states.push_back(m_nextState++);
    ++m_index;
    trimToCapacity();
}

const char* History::undoDescription() const {
    return canUndo() ? m_entries[m_index - 1]->description() : nullptr;
}

const char* History::redoDescription() const {
    return canRedo() ? m_entries[m_index]->description() : nullptr;
}

void History::undo() {
    if (!canUndo())
        throw std::runtime_error("History::undo: nothing to undo");
    m_entries[--m_index]->undo();
}

void History::redo() {
    if (!canRedo())
        throw std::runtime_error("History::redo: nothing to redo");
    m_entries[m_index++]->redo();
}

void History::clear() noexcept {
    m_entries.clear();
    m_index = 0;
    m_states.assign(1, m_nextState++);
    m_savedState = stateId();
}

void History::trimToCapacity() {
    if (m_entries.size() <= m_capacity)
        return;
    // Evict the oldest entries; the owning window may shrink them on disk
    // later, but the in-memory stack just drops them.
    const auto excess = m_entries.size() - m_capacity;
    m_entries.erase(m_entries.begin(), m_entries.begin() + static_cast<std::ptrdiff_t>(excess));
    m_index -= excess;
    m_states.erase(m_states.begin(), m_states.begin() + static_cast<std::ptrdiff_t>(excess));
}

} // namespace prism::core
