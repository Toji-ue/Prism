#pragma once

namespace prism::core::raster {

/// HSV in 0..1 ranges (hue 0..1 wraps, s/v 0..1), RGB in 0..1.
struct Hsv {
    float h = 0.0F;
    float s = 0.0F;
    float v = 0.0F;
};

struct Rgb {
    float r = 0.0F;
    float g = 0.0F;
    float b = 0.0F;
};

/// Pure color-space conversions. These are the single source of truth for
/// every picker and slider; pixel code only ever sees 8-bit RGBA.
[[nodiscard]] Hsv rgbToHsv(Rgb rgb) noexcept;
[[nodiscard]] Rgb hsvToRgb(Hsv hsv) noexcept;

} // namespace prism::core::raster
