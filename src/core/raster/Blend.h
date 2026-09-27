#pragma once

#include "layer/BlendMode.h"
#include "raster/RasterBuffer.h"

#include <cstdint>

namespace prism::core::raster {

/// Blend a straight-alpha source pixel over a straight-alpha destination
/// pixel, for the given blend mode. Pure math, no state — unit-testable.
///
/// All modes blend in premultiplied space internally and return straight
/// alpha, so partial opacity layers composite identically to Photoshop's
/// straight-alpha model.
[[nodiscard]] Color blendPixel(Color dst, Color src, BlendMode mode) noexcept;

/// Per-channel blend formulas (on 0..1 floats) for the separable modes.
/// Kept public for tests; blendPixel is the API the compositor uses.
namespace blend_detail {

[[nodiscard]] inline float channelNormal(float, float s) noexcept { return s; }
[[nodiscard]] inline float channelMultiply(float b, float s) noexcept { return b * s; }
[[nodiscard]] inline float channelScreen(float b, float s) noexcept
{
    return b + s - b * s;
}
[[nodiscard]] inline float channelOverlay(float b, float s) noexcept
{
    return b <= 0.5F ? 2.0F * b * s : 1.0F - 2.0F * (1.0F - b) * (1.0F - s);
}

} // namespace blend_detail

} // namespace prism::core::raster
