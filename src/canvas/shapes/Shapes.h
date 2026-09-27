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

/// Strokes the brush along an arbitrary path (Polyline / Freehand Path /
/// Bezier / Arc all rasterize through this).
[[nodiscard]] Rect strokeAlong(RasterBuffer& buffer,
                               const std::vector<StrokePoint>& path,
                               const BrushSettings& brush);

/// Samples a cubic Bezier into a stroke path.
[[nodiscard]] std::vector<StrokePoint> bezierPath(Point p0, Point c0, Point c1, Point p3,
                                                  int segments = 48);

/// Half-ellipse arc inscribed in the drag rectangle (Arc tool).
[[nodiscard]] std::vector<StrokePoint> arcPath(Point start, Point end, int segments = 48);

/// Chaikin corner-cutting: turns a sampled drag into a smooth path
/// (Freehand Path tool).
[[nodiscard]] std::vector<StrokePoint> smoothPath(const std::vector<StrokePoint>& input,
                                                  int passes = 2);

/// Procedural patterns for the Pattern tool.
enum class PatternKind {
    Checker,
    Stripes,
    Dots,
};

/// Fills \p buffer with \p kind at \p scale pixels, in \p color, compositing
/// over what is there. \p limit restricts the fill (empty = whole buffer).
[[nodiscard]] Rect fillPattern(RasterBuffer& buffer, PatternKind kind, int scale,
                               prism::core::raster::Color color, const Rect& limit);

/// Affine transform of a buffer's content about (cx, cy): rotation in degrees
/// plus uniform scale, sampled bilinearly (Transform tool).
[[nodiscard]] Rect transformBuffer(RasterBuffer& buffer, float degrees, float scale, float cx,
                                   float cy);

} // namespace prism::canvas
