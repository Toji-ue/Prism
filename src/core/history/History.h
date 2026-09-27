#pragma once

#include "history/Command.h"

#include <memory>
#include <vector>

namespace prism::core {

/// Undo/redo stack. Owns executed Commands; indices stay valid because
/// undone commands above the current position are dropped on new execution
/// (standard linear-history model, same as Photoshop).
class History {
public:
    static constexpr std::size_t kDefaultCapacity = 100;

    /// Executes \p command (calls redo()) and pushes it onto the stack.
    void execute(std::unique_ptr<Command> command);

    [[nodiscard]] bool canUndo() const noexcept { return m_index > 0; }
    [[nodiscard]] bool canRedo() const noexcept { return m_index < m_entries.size(); }

    /// Description of the command undo() would run next, or nullptr.
    [[nodiscard]] const char* undoDescription() const;
    /// Description of the command redo() would run next, or nullptr.
    [[nodiscard]] const char* redoDescription() const;

    void undo();
    void redo();

    void clear() noexcept;

    [[nodiscard]] std::size_t size() const noexcept { return m_entries.size(); }

private:
    void trimToCapacity();

    std::vector<std::unique_ptr<Command>> m_entries;
    /// Number of entries still "live" (entries above were undone).
    std::size_t m_index = 0;
    std::size_t m_capacity = kDefaultCapacity;
};

} // namespace prism::core
