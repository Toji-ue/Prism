#pragma once

namespace prism::core {

/// A reversible mutation of the Document.
///
/// Every state-changing operation must be expressed as a Command and run
/// through History — writing to the document outside a Command breaks undo
/// and is a correctness bug, not a UX gap.
class Command {
public:
    virtual ~Command() = default;

    /// Human-readable description for the history panel, e.g. "Brush stroke".
    [[nodiscard]] virtual const char* description() const = 0;

    virtual void undo() = 0;
    virtual void redo() = 0;
};

} // namespace prism::core
