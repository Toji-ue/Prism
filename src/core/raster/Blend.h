#pragma once

#include "layer/BlendMode.h"
#include "raster/RasterBuffer.h"

#include <cstdint>
#include <cmath>

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
/// Formulas follow the W3C compositing spec, matching Krita/Photoshop.
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
[[nodiscard]] inline float channelDarken(float b, float s) noexcept { return b < s ? b : s; }
[[nodiscard]] inline float channelLighten(float b, float s) noexcept { return b > s ? b : s; }
[[nodiscard]] inline float channelColorDodge(float b, float s) noexcept
{
    if (b <= 0.0F)
        return 0.0F;
    if (s >= 1.0F)
        return 1.0F;
    const float r = b / (1.0F - s);
    return r > 1.0F ? 1.0F : r;
}
[[nodiscard]] inline float channelColorBurn(float b, float s) noexcept
{
    if (b >= 1.0F)
        return 1.0F;
    if (s <= 0.0F)
        return 0.0F;
    const float r = 1.0F - (1.0F - b) / s;
    return r < 0.0F ? 0.0F : r;
}
[[nodiscard]] inline float channelHardLight(float b, float s) noexcept
{
    return channelOverlay(s, b); // overlay with source and backdrop swapped
}
[[nodiscard]] inline float channelSoftLight(float b, float s) noexcept
{
    if (s <= 0.5F)
        return b - (1.0F - 2.0F * s) * b * (1.0F - b);
    const float d = b <= 0.25F ? ((16.0F * b - 12.0F) * b + 4.0F) * b
                               : static_cast<float>(std::sqrt(b));
    return b + (2.0F * s - 1.0F) * (d - b);
}
[[nodiscard]] inline float channelDifference(float b, float s) noexcept
{
    return b > s ? b - s : s - b;
}
[[nodiscard]] inline float channelExclusion(float b, float s) noexcept
{
    return b + s - 2.0F * b * s;
}
[[nodiscard]] inline float channelAdd(float b, float s) noexcept
{
    const float r = b + s;
    return r > 1.0F ? 1.0F : r;
}
[[nodiscard]] inline float channelSubtract(float b, float s) noexcept
{
    const float r = b - s;
    return r < 0.0F ? 0.0F : r;
}
/// Divide (Krita): the backdrop divided by the source. A zero source yields
/// white where the backdrop has any colour and black where it has none, so the
/// mode stays total (Krita leaves this undefined; we pick a stable answer).
[[nodiscard]] inline float channelDivide(float b, float s) noexcept
{
    if (s <= 0.0F)
        return b > 0.0F ? 1.0F : 0.0F;
    const float r = b / s;
    return r > 1.0F ? 1.0F : r;
}

/// Non-separable (HSL) modes. They need the whole colour at once, so they are
/// implemented in Blend.cpp against these primitives (kept here for tests).
[[nodiscard]] float luminance(float r, float g, float b) noexcept;
[[nodiscard]] float saturationOf(float r, float g, float b) noexcept;
void setLuminance(float& r, float& g, float& b, float target) noexcept;
void clipColor(float& r, float& g, float& b) noexcept;
void setSaturation(float& r, float& g, float& b, float target) noexcept;

} // namespace blend_detail

/// True for modes that blend each channel independently.
[[nodiscard]] bool isSeparable(BlendMode mode) noexcept;

/// Blends two straight-alpha colours in place for the HSL group (Hue,
/// Saturation, Color, Luminosity). Exposed for tests of the invariants.
void blendNonSeparable(const float backdrop[3], const float source[3], BlendMode mode,
                       float out[3]) noexcept;

} // namespace prism::core::raster
