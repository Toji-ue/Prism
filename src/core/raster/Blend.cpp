#include "raster/Blend.h"

#include "layer/BlendMode.h"

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
    case BlendMode::Normal:
    default:
        return blend_detail::channelNormal;
    }
}

} // namespace

Color blendPixel(Color dst, Color src, BlendMode mode) noexcept
{
    const float srcA = toFloat(src.a);
    if (srcA == 0.0F)
        return dst; // fully transparent source leaves dst untouched

    const float dstA = toFloat(dst.a);

    const ChannelFn f = channelFn(mode);

    // W3C compositing model: the blend function B runs on straight colors,
    // the result combines in premultiplied space, then we unpremultiply for
    // storage. Doing it in straight space silently darkens color over
    // transparent destinations (a real bug the compositor tests caught).
    //   Co = srcA * B(dstC, srcC) + (1 - srcA) * dstA * dstC   (premultiplied)
    //   outA = srcA + dstA * (1 - srcA)
    float outP[3];
    const float dstC[3] = { toFloat(dst.r), toFloat(dst.g), toFloat(dst.b) };
    const float srcC[3] = { toFloat(src.r), toFloat(src.g), toFloat(src.b) };
    for (int i = 0; i < 3; ++i) {
        const float blended = f(dstC[i], srcC[i]);
        outP[i] = srcA * blended + (1.0F - srcA) * dstA * dstC[i];
    }
    const float outA = srcA + dstA * (1.0F - srcA);

    if (outA <= 0.0F)
        return kTransparent;

    // Unpremultiply back to straight color for storage.
    return Color{ toByte(outP[0] / outA), toByte(outP[1] / outA), toByte(outP[2] / outA),
                  toByte(outA) };
}

} // namespace prism::core::raster
