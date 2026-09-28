#include "raster/Blend.h"

#include "layer/BlendMode.h"

#include <algorithm>
#include <cmath>

namespace prism::core::raster {

namespace {

constexpr float toFloat(std::uint8_t v) noexcept
{
    return static_cast<float>(v) / 255.0F;
}

constexpr std::uint8_t toByte(float v) noexcept
{
    // round-half-up; clamped by construction (inputs are 0..1)
    const float x = v * 255.0F + 0.5F;
    return static_cast<std::uint8_t>(x > 255.0F ? 255.0F : (x < 0.0F ? 0.0F : x));
}

using ChannelFn = float (*)(float, float) noexcept;

ChannelFn channelFn(BlendMode mode) noexcept
{
    switch (mode) {
    case BlendMode::Multiply:
        return blend_detail::channelMultiply;
    case BlendMode::Screen:
        return blend_detail::channelScreen;
    case BlendMode::Overlay:
        return blend_detail::channelOverlay;
    case BlendMode::Darken:
        return blend_detail::channelDarken;
    case BlendMode::Lighten:
        return blend_detail::channelLighten;
    case BlendMode::ColorDodge:
        return blend_detail::channelColorDodge;
    case BlendMode::ColorBurn:
        return blend_detail::channelColorBurn;
    case BlendMode::HardLight:
        return blend_detail::channelHardLight;
    case BlendMode::SoftLight:
        return blend_detail::channelSoftLight;
    case BlendMode::Difference:
        return blend_detail::channelDifference;
    case BlendMode::Exclusion:
        return blend_detail::channelExclusion;
    case BlendMode::Add:
        return blend_detail::channelAdd;
    case BlendMode::Subtract:
        return blend_detail::channelSubtract;
    case BlendMode::Divide:
        return blend_detail::channelDivide;
    case BlendMode::Normal:
    default:
        return blend_detail::channelNormal;
    }
}

} // namespace

bool isSeparable(BlendMode mode) noexcept
{
    switch (mode) {
    case BlendMode::Hue:
    case BlendMode::Saturation:
    case BlendMode::Color:
    case BlendMode::Luminosity:
        return false;
    default:
        return true;
    }
}

/// Non-separable blend function B(b, s) per the W3C compositing spec:
///   Hue        = SetLum(SetSat(src, Sat(backdrop)), Lum(backdrop))
///   Saturation = SetLum(SetSat(backdrop, Sat(src)), Lum(backdrop))
///   Color      = SetLum(src, Lum(backdrop))
///   Luminosity = SetLum(backdrop, Lum(src))
/// Taking the wrong starting colour here is the classic mistake: it silently
/// swaps what "Hue" and "Luminosity" mean.
void blendNonSeparable(const float b[3], const float s[3], BlendMode mode, float out[3]) noexcept
{
    using namespace blend_detail;
    const bool fromBackdrop = mode == BlendMode::Saturation || mode == BlendMode::Luminosity;
    float r = fromBackdrop ? b[0] : s[0];
    float g = fromBackdrop ? b[1] : s[1];
    float bl = fromBackdrop ? b[2] : s[2];
    switch (mode) {
    case BlendMode::Hue:
        // Source hue and saturation, backdrop luminosity.
        setSaturation(r, g, bl, saturationOf(b[0], b[1], b[2]));
        setLuminance(r, g, bl, luminance(b[0], b[1], b[2]));
        break;
    case BlendMode::Saturation:
        // Backdrop hue, source saturation, backdrop luminosity.
        setSaturation(r, g, bl, saturationOf(s[0], s[1], s[2]));
        setLuminance(r, g, bl, luminance(b[0], b[1], b[2]));
        break;
    case BlendMode::Color:
        // Source hue and saturation, backdrop luminosity.
        setLuminance(r, g, bl, luminance(b[0], b[1], b[2]));
        break;
    case BlendMode::Luminosity:
        // Backdrop hue and saturation, source luminosity.
        setLuminance(r, g, bl, luminance(s[0], s[1], s[2]));
        break;
    default: break;
    }
    out[0] = r;
    out[1] = g;
    out[2] = bl;
}

namespace blend_detail {

float luminance(float r, float g, float b) noexcept
{
    return 0.3F * r + 0.59F * g + 0.11F * b;
}

float saturationOf(float r, float g, float b) noexcept
{
    const float mx = std::max(r, std::max(g, b));
    const float mn = std::min(r, std::min(g, b));
    return mx - mn;
}

void clipColor(float& r, float& g, float& b) noexcept
{
    const float lum = luminance(r, g, b);
    const float mn = std::min(r, std::min(g, b));
    const float mx = std::max(r, std::max(g, b));
    if (mn < 0.0F) {
        const float d = lum - mn;
        if (d > 0.0F) {
            r = lum + (r - lum) * lum / d;
            g = lum + (g - lum) * lum / d;
            b = lum + (b - lum) * lum / d;
        }
    }
    if (mx > 1.0F) {
        const float d = mx - lum;
        if (d > 0.0F) {
            r = lum + (r - lum) * (1.0F - lum) / d;
            g = lum + (g - lum) * (1.0F - lum) / d;
            b = lum + (b - lum) * (1.0F - lum) / d;
        }
    }
}

void setLuminance(float& r, float& g, float& b, float target) noexcept
{
    const float d = target - luminance(r, g, b);
    r += d;
    g += d;
    b += d;
    clipColor(r, g, b);
}

void setSaturation(float& r, float& g, float& b, float target) noexcept
{
    const float mn = std::min(r, std::min(g, b));
    const float mx = std::max(r, std::max(g, b));
    if (mx <= mn)
        return;
    const float scale = target / (mx - mn);
    // Rebase around the minimum, scale the spread, then shift back.
    r = (r - mn) * scale;
    g = (g - mn) * scale;
    b = (b - mn) * scale;
    const float shifted = mn;
    r += shifted;
    g += shifted;
    b += shifted;
}

} // namespace blend_detail

namespace {

/// Straight-color blend of src over dst for one pixel, in the W3C model:
///   Co = srcA * B(dstC, srcC) + (1 - srcA) * dstA * dstC   (premultiplied)
///   outA = srcA + dstA * (1 - srcA)
Color blendStraight(const float dstC[3], const float srcC[3], float srcA, float dstA,
                    BlendMode mode) noexcept
{
    float blended[3];
    if (isSeparable(mode)) {
        const ChannelFn f = channelFn(mode);
        for (int i = 0; i < 3; ++i)
            blended[i] = f(dstC[i], srcC[i]);
    } else {
        blendNonSeparable(dstC, srcC, mode, blended);
    }

    float outP[3];
    for (int i = 0; i < 3; ++i)
        outP[i] = srcA * blended[i] + (1.0F - srcA) * dstA * dstC[i];
    const float outA = srcA + dstA * (1.0F - srcA);
    if (outA <= 0.0F)
        return kTransparent;
    // Unpremultiply back to straight color for storage.
    return Color{ toByte(outP[0] / outA), toByte(outP[1] / outA), toByte(outP[2] / outA),
                  toByte(outA) };
}

} // namespace

Color blendPixel(Color dst, Color src, BlendMode mode) noexcept
{
    const float srcA = toFloat(src.a);
    if (srcA == 0.0F)
        return dst; // fully transparent source leaves dst untouched

    const float dstA = toFloat(dst.a);
    const float dstC[3] = { toFloat(dst.r), toFloat(dst.g), toFloat(dst.b) };
    const float srcC[3] = { toFloat(src.r), toFloat(src.g), toFloat(src.b) };
    return blendStraight(dstC, srcC, srcA, dstA, mode);
}

} // namespace prism::core::raster
