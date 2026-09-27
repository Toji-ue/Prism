#pragma once

namespace prism::core {

/// Blend modes for layer compositing.
/// Values map 1:1 to the compositor's shader branches — append only, never
/// renumber: these get serialized into project files.
enum class BlendMode {
    Normal = 0,
    Multiply = 1,
    Screen = 2,
    Overlay = 3,
    // append new modes here; keep old values stable
};

} // namespace prism::core
