#pragma once

#include "geometry/Types.h"
#include "raster/RasterBuffer.h"

#include <vector>

namespace prism::canvas {

using prism::core::raster::RasterBuffer;

/// A single pen input sample, canvas coordinates, pressure 0..1.
struct StrokePoint {
    float x = 0.0F;
    float y = 0.0F;
    float pressure = 1.0F;
};

/// Brush settings for one stroke.
struct BrushSettings {
    float radius = 8.0F; // in canvas pixels, at full pressure
    float hardness = 1.0F; // 0 = soft falloff, 1 = hard circle
    prism::core::raster::Color color = prism::core::raster::kOpaqueBlack;
    /// Flow: how much paint each stamp deposits, 0..1. This is a stroke
    /// property, NOT layer opacity — see AGENTS.md terminology note.
    float flow = 1.0F;
};

/// Hard/soft circular brush stamping along a point path.
///
/// Pure CPU reference implementation. Points are stamped densely enough
/// that consecutive stamps overlap (spacing = radius / 2), which is the
/// classic approach; spacing curves arrive with the real brush engine.
class BrushStroke {
public:
    /// Stamps \p points into \p buffer with \p brush.
    /// Returns the bounding rect (buffer coordinates) actually touched, or
    /// an empty rect if nothing was painted.
    [[nodiscard]] static prism::core::geometry::Rect paint(RasterBuffer& buffer,
                                                           const BrushSettings& brush,
                                                           const std::vector<StrokePoint>& points);

private:
    static void stamp(RasterBuffer& buffer, const BrushSettings& brush,
                      const StrokePoint& center, prism::core::geometry::Rect& bounds);
};

} // namespace prism::canvas
