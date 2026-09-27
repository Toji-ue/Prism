#pragma once

#include "geometry/Types.h"
#include "raster/RasterBuffer.h"

#include <cmath>
#include <cstdlib>
#include <vector>

namespace prism::canvas {

using prism::core::geometry::Rect;
using prism::core::raster::RasterBuffer;

namespace detail {

inline bool matches(prism::core::raster::Color a, prism::core::raster::Color b,
                    int tolerance) noexcept
{
    return std::abs(static_cast<int>(a.r) - static_cast<int>(b.r)) <= tolerance
        && std::abs(static_cast<int>(a.g) - static_cast<int>(b.g)) <= tolerance
        && std::abs(static_cast<int>(a.b) - static_cast<int>(b.b)) <= tolerance
        && std::abs(static_cast<int>(a.a) - static_cast<int>(b.a)) <= tolerance;
}

} // namespace detail

/// Scanline flood fill of the connected region matching the seed pixel.
///
/// \p onPixel is called once per pixel in the region (in scanline order) and
/// may mutate the buffer. This is the single implementation of "connected
/// similar region": the bucket-fill tool, the magic wand, and anything else
/// needing region growing all go through it.
///
/// Returns the region's bounding rect, or an empty rect if nothing matched.
template <typename OnPixel>
Rect floodFillRegion(const RasterBuffer& buffer, int x, int y, int tolerance, OnPixel onPixel)
{
    if (buffer.isEmpty() || x < 0 || y < 0 || x >= buffer.width() || y >= buffer.height())
        return {};

    const auto seed = buffer.at(x, y);
    std::vector<bool> visited(static_cast<std::size_t>(buffer.width()) * buffer.height(),
                              false);

    int minX = x, maxX = x, minY = y, maxY = y;
    struct Span {
        int x;
        int y;
    };
    std::vector<Span> stack{ { x, y } };
    const auto matchesSeed = [&](int px, int py) {
        return detail::matches(buffer.at(px, py), seed, tolerance);
    };

    while (!stack.empty()) {
        const Span s = stack.back();
        stack.pop_back();
        const auto index = static_cast<std::size_t>(s.y) * buffer.width() + s.x;
        if (visited[index] || !matchesSeed(s.x, s.y))
            continue;

        int left = s.x;
        while (left > 0
               && !visited[static_cast<std::size_t>(s.y) * buffer.width() + (left - 1)]
               && matchesSeed(left - 1, s.y))
            --left;
        int right = s.x;
        while (right < buffer.width() - 1
               && !visited[static_cast<std::size_t>(s.y) * buffer.width() + (right + 1)]
               && matchesSeed(right + 1, s.y))
            ++right;

        for (int px = left; px <= right; ++px) {
            onPixel(px, s.y, buffer.at(px, s.y));
            visited[static_cast<std::size_t>(s.y) * buffer.width() + px] = true;
        }

        minX = minX < left ? minX : left;
        maxX = maxX > right ? maxX : right;
        minY = minY < s.y ? minY : s.y;
        maxY = maxY > s.y ? maxY : s.y;

        for (int px = left; px <= right; ++px) {
            if (s.y > 0)
                stack.push_back({ px, s.y - 1 });
            if (s.y < buffer.height() - 1)
                stack.push_back({ px, s.y + 1 });
        }
    }

    if (maxX < minX || maxY < minY)
        return {};
    return Rect{ minX, minY, maxX - minX + 1, maxY - minY + 1 };
}

/// Bucket fill: paints \p color into the connected region under (x, y).
[[nodiscard]] Rect floodFill(RasterBuffer& buffer, int x, int y,
                             prism::core::raster::Color color, int tolerance = 24);

} // namespace prism::canvas
