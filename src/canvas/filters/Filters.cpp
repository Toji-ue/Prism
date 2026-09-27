#include "filters/Filters.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace prism::canvas::filters {

using prism::core::raster::Color;

namespace {

Rect full(const RasterBuffer& buffer)
{
    return Rect{ 0, 0, buffer.width(), buffer.height() };
}

std::uint8_t clampByte(float v)
{
    return static_cast<std::uint8_t>(std::clamp(v, 0.0F, 255.0F));
}

/// One horizontal-then-vertical box blur pass over premultiplied channels.
void boxBlurPass(RasterBuffer& buffer, int radius)
{
    const int w = buffer.width();
    const int h = buffer.height();
    if (w <= 0 || h <= 0)
        return;

    const int window = radius * 2 + 1;
    std::vector<float> tmp(static_cast<std::size_t>(w) * h * 4, 0.0F);

    // Horizontal, weighted by alpha so transparent pixels don't darken edges.
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            float r = 0, g = 0, b = 0, a = 0;
            for (int k = -radius; k <= radius; ++k) {
                const int sx = std::clamp(x + k, 0, w - 1);
                const Color c = buffer.at(sx, y);
                const float ca = static_cast<float>(c.a) / 255.0F;
                r += static_cast<float>(c.r) * ca;
                g += static_cast<float>(c.g) * ca;
                b += static_cast<float>(c.b) * ca;
                a += static_cast<float>(c.a);
            }
            const std::size_t i = (static_cast<std::size_t>(y) * w + x) * 4;
            tmp[i] = r / window;
            tmp[i + 1] = g / window;
            tmp[i + 2] = b / window;
            tmp[i + 3] = a / window;
        }
    }

    // Vertical
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            float r = 0, g = 0, b = 0, a = 0;
            for (int k = -radius; k <= radius; ++k) {
                const int sy = std::clamp(y + k, 0, h - 1);
                const std::size_t i = (static_cast<std::size_t>(sy) * w + x) * 4;
                r += tmp[i];
                g += tmp[i + 1];
                b += tmp[i + 2];
                a += tmp[i + 3];
            }
            const float aAvg = a / window;
            // Un-premultiply for storage.
            if (aAvg <= 0.001F) {
                buffer.set(x, y, prism::core::raster::kTransparent);
                continue;
            }
            const float scale = static_cast<float>(window) / std::max(a, 0.001F);
            buffer.set(x, y,
                       Color{ clampByte(r * scale),
                              clampByte(g * scale),
                              clampByte(b * scale),
                              clampByte(aAvg) });
        }
    }
}

} // namespace

Rect invert(RasterBuffer& buffer)
{
    if (buffer.isEmpty())
        return {};
    for (int y = 0; y < buffer.height(); ++y) {
        for (int x = 0; x < buffer.width(); ++x) {
            const Color c = buffer.at(x, y);
            if (c.a == 0)
                continue; // leave untouched pixels transparent, not white
            buffer.set(x, y, Color{ static_cast<std::uint8_t>(255 - c.r),
                                    static_cast<std::uint8_t>(255 - c.g),
                                    static_cast<std::uint8_t>(255 - c.b), c.a });
        }
    }
    return full(buffer);
}

Rect desaturate(RasterBuffer& buffer)
{
    if (buffer.isEmpty())
        return {};
    for (int y = 0; y < buffer.height(); ++y) {
        for (int x = 0; x < buffer.width(); ++x) {
            const Color c = buffer.at(x, y);
            if (c.a == 0)
                continue;
            // Rec. 709 luma, the same weighting Krita uses.
            const auto luma = static_cast<std::uint8_t>(
                0.2126F * c.r + 0.7152F * c.g + 0.0722F * c.b + 0.5F);
            buffer.set(x, y, Color{ luma, luma, luma, c.a });
        }
    }
    return full(buffer);
}

Rect gaussianBlurApprox(RasterBuffer& buffer, int radius)
{
    if (buffer.isEmpty() || radius <= 0)
        return {};
    // Three box passes converge on a Gaussian (central limit theorem).
    for (int pass = 0; pass < 3; ++pass)
        boxBlurPass(buffer, radius);
    return full(buffer);
}

Rect brightnessContrast(RasterBuffer& buffer, float brightness, float contrast)
{
    if (buffer.isEmpty())
        return {};
    for (int y = 0; y < buffer.height(); ++y) {
        for (int x = 0; x < buffer.width(); ++x) {
            const Color c = buffer.at(x, y);
            if (c.a == 0)
                continue;
            const auto apply = [&](std::uint8_t v) {
                const float centred = (static_cast<float>(v) - 127.5F) * contrast + 127.5F;
                return clampByte(centred * brightness);
            };
            buffer.set(x, y, Color{ apply(c.r), apply(c.g), apply(c.b), c.a });
        }
    }
    return full(buffer);
}

} // namespace prism::canvas::filters
