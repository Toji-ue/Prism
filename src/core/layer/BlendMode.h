#pragma once

namespace prism::core {

/// Blend modes for layer compositing, covering the set Krita offers.
/// Values map 1:1 to the compositor's blend branches AND are serialized into
/// project files — append only, never renumber.
enum class BlendMode {
    Normal = 0,
    Multiply = 1,
    Screen = 2,
    Overlay = 3,
    Darken = 4,
    Lighten = 5,
    ColorDodge = 6,
    ColorBurn = 7,
    HardLight = 8,
    SoftLight = 9,
    Difference = 10,
    Exclusion = 11,
    /// Linear add (Krita/Photoshop "Linear Dodge (Add)" / "Plus").
    Add = 12,
    Subtract = 13,
    Divide = 14,
    // Non-separable (HSL) group: these work on luminance/hue as a whole, not
    // per channel.
    Hue = 15,
    Saturation = 16,
    Color = 17,
    Luminosity = 18,
    // append new modes here; keep old values stable
};

} // namespace prism::core
