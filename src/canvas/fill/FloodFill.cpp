#include "fill/FloodFill.h"

namespace prism::canvas {

using prism::core::raster::Color;

Rect floodFill(RasterBuffer& buffer, int x, int y, Color color, int tolerance)
{
    if (buffer.isEmpty() || x < 0 || y < 0 || x >= buffer.width() || y >= buffer.height())
        return {};

    // Already the target color: nothing to do (avoids an infinite repaint of
    // an unchanged region).
    if (detail::matches(buffer.at(x, y), color, 0))
        return {};

    return floodFillRegion(buffer, x, y, tolerance,
                           [&](int px, int py, Color) { buffer.set(px, py, color); });
}

} // namespace prism::canvas
