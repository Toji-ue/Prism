#include "fill/FloodFill.h"

#include <algorithm>
#include <cstdlib>
#include <vector>

namespace prism::canvas {

using prism::core::geometry::Rect;
using prism::core::raster::Color;
using prism::core::raster::RasterBuffer;

namespace {

bool matches(Color a, Color b, int tolerance) noexcept
{
    return std::abs(static_cast<int>(a.r) - static_cast<int>(b.r)) <= tolerance
        && std::abs(static_cast<int>(a.g) - static_cast<int>(b.g)) <= tolerance
        && std::abs(static_cast<int>(a.b) - static_cast<int>(b.b)) <= tolerance
        && std::abs(static_cast<int>(a.a) - static_cast<int>(b.a)) <= tolerance;
}

} // namespace

Rect floodFill(RasterBuffer& buffer, int x, int y, Color color, int tolerance)
{
    if (buffer.isEmpty() || x < 0 || y < 0 || x >= buffer.width() || y >= buffer.height())
        return {};

    const Color seed = buffer.at(x, y);
    if (matches(seed, color, 0)) // already exactly the target color
        return {};

    // Visited mask: avoids re-testing filled pixels after the color changed.
    std::vector<bool> visited(static_cast<std::size_t>(buffer.width()) * buffer.height(), false);

    int minX = x;
    int maxX = x;
    int minY = y;
    int maxY = y;

    struct Span {
        int x;
        int y;
    };
    std::vector<Span> stack{ { x, y } };

    while (!stack.empty()) {
        const Span s = stack.back();
        stack.pop_back();

        if (visited[static_cast<std::size_t>(s.y) * buffer.width() + s.x])
            continue;
        if (!matches(buffer.at(s.x, s.y), seed, tolerance))
            continue;

        // Extend the run left and right from the seed pixel.
        int left = s.x;
        while (left > 0
               && !visited[static_cast<std::size_t>(s.y) * buffer.width() + (left - 1)]
               && matches(buffer.at(left - 1, s.y), seed, tolerance))
            --left;
        int right = s.x;
        while (right < buffer.width() - 1
               && !visited[static_cast<std::size_t>(s.y) * buffer.width() + (right + 1)]
               && matches(buffer.at(right + 1, s.y), seed, tolerance))
            ++right;

        for (int px = left; px <= right; ++px) {
            buffer.set(px, s.y, color);
            visited[static_cast<std::size_t>(s.y) * buffer.width() + px] = true;
        }

        minX = std::min(minX, left);
        maxX = std::max(maxX, right);
        minY = std::min(minY, s.y);
        maxY = std::max(maxY, s.y);

        // Seed the rows above and below.
        for (int px = left; px <= right; ++px) {
            if (s.y > 0)
                stack.push_back({ px, s.y - 1 });
            if (s.y < buffer.height() - 1)
                stack.push_back({ px, s.y + 1 });
        }
    }

    return Rect{ minX, minY, maxX - minX + 1, maxY - minY + 1 };
}

} // namespace prism::canvas
