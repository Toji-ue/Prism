#pragma once

#include <cstdint>

namespace prism::core::geometry {

/// Integer pixel position / size, canvas space. Origin is top-left,
/// +x right, +y down — matching raster convention.
struct Point {
    int x = 0;
    int y = 0;
};

struct Size {
    int width = 0;
    int height = 0;

    [[nodiscard]] constexpr bool isEmpty() const noexcept
    {
        return width <= 0 || height <= 0;
    }
};

struct Rect {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;

    [[nodiscard]] constexpr bool isEmpty() const noexcept
    {
        return width <= 0 || height <= 0;
    }

    /// Intersection of two rects; empty rect if they don't overlap.
    [[nodiscard]] constexpr Rect intersected(const Rect& other) const noexcept
    {
        const int x1 = x > other.x ? x : other.x;
        const int y1 = y > other.y ? y : other.y;
        const int x2 = (x + width) < (other.x + other.width) ? (x + width)
                                                             : (other.x + other.width);
        const int y2 = (y + height) < (other.y + other.height)
                           ? (y + height)
                           : (other.y + other.height);
        return x2 > x1 && y2 > y1 ? Rect{ x1, y1, x2 - x1, y2 - y1 } : Rect{};
    }
};

} // namespace prism::core::geometry
