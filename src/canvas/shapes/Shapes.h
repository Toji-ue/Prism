#pragma once

#include "brush/BrushStroke.h"
#include "geometry/Types.h"
#include "raster/RasterBuffer.h"

namespace prism::canvas {

using prism::core::geometry::Point;
using prism::core::geometry::Rect;
using prism::core::raster::RasterBuffer;

/// Drag-to-draw geometric primitives (Krita's Line / Rectangle / Ellipse
/// tools). Strokes follow the current brush, so a soft 40px brush makes a
/// soft-edged rectangle.
enum class ShapeKind {
    Line,
    Rectangle,
    Ellipse,
};

/// Rasterizes \p kind from \p start to \p end (canvas pixels) into \p buffer.
/// \p filled paints the interior for rectangle/ellipse; line ignores it.
/// Returns the rect actually touched, or an empty rect when nothing was drawn.
[[nodiscard]] Rect rasterizeShape(RasterBuffer& buffer, ShapeKind kind, Point start, Point end,
                                  const BrushSettings& brush, bool filled);

/// Linear gradient from \p start to \p end: \p from fades into \p to across
/// the drag axis, on the active layer (Krita's Gradient tool).
[[nodiscard]] Rect linearGradient(RasterBuffer& buffer, Point start, Point end,
                                  prism::core::raster::Color from,
                                  prism::core::raster::Color to);

} // namespace prism::canvas
