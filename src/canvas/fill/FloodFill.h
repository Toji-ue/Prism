#pragma once

#include "geometry/Types.h"
#include "raster/RasterBuffer.h"

namespace prism::canvas {

using prism::core::raster::RasterBuffer;

/// Scanline flood fill, 4-connected, with per-channel tolerance (0..255).
/// Paints \p color into the region of \p buffer connected to (x, y) whose
/// pixels match the seed color within \p tolerance.
///
/// Returns the bounding rect actually changed, or an empty rect if nothing
/// changed (seed already the target color, or seed out of bounds).
[[nodiscard]] prism::core::geometry::Rect floodFill(RasterBuffer& buffer, int x, int y,
                                                    prism::core::raster::Color color,
                                                    int tolerance = 24);

} // namespace prism::canvas
