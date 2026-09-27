#include "brush/BrushStroke.h"

#include "geometry/Types.h"

#include <algorithm>
#include <cmath>

namespace prism::canvas {

using prism::core::geometry::Rect;
using prism::core::raster::Color;
using prism::core::raster::RasterBuffer;

namespace {

float falloff(float dist, float radius, float hardness) noexcept
{
    if (dist >= radius)
        return 0.0F;
    if (hardness >= 1.0F)
        return 1.0F;
    // linear ramp between the hard core and the soft edge
    const float softStart = radius * hardness;
    if (dist <= softStart)
        return 1.0F;
    return (radius - dist) / (radius - softStart);
}

} // namespace

void BrushStroke::stamp(RasterBuffer& buffer, const BrushSettings& brush,
                        const StrokePoint& center, Rect& bounds)
{
    const float radius = std::max(brush.radius * center.pressure, 0.5F);
    const float rCeil = std::ceil(radius);
    const int x0 = static_cast<int>(std::floor(center.x - rCeil));
    const int y0 = static_cast<int>(std::floor(center.y - rCeil));
    const int x1 = static_cast<int>(std::ceil(center.x + rCeil));
    const int y1 = static_cast<int>(std::ceil(center.y + rCeil));

    for (int y = y0; y <= y1; ++y) {
        for (int x = x0; x <= x1; ++x) {
            if (x < 0 || y < 0 || x >= buffer.width() || y >= buffer.height())
                continue; // stamps near canvas edges clip silently
            const float dx = static_cast<float>(x) - center.x;
            const float dy = static_cast<float>(y) - center.y;
            const float dist = std::sqrt(dx * dx + dy * dy);
            const float fall = falloff(dist, radius, brush.hardness);
            if (fall <= 0.0F)
                continue;

            const std::uint8_t alpha = static_cast<std::uint8_t>(
                255.0F * fall * brush.flow + 0.5F);
            if (alpha == 0)
                continue;
            // Max-composite within one stroke: overlapping stamps of the
            // same stroke never exceed the stroke's own alpha (flow 1 =
            // solid). Per-stamp accumulation would need a separate buffer.
            const Color existing = buffer.at(x, y);
            if (alpha > existing.a) {
                buffer.set(x, y, Color{ brush.color.r, brush.color.g, brush.color.b, alpha });
                bounds = Rect{ std::min(bounds.x, x), std::min(bounds.y, y),
                               std::max(bounds.x + bounds.width, x + 1) - std::min(bounds.x, x),
                               std::max(bounds.y + bounds.height, y + 1)
                                   - std::min(bounds.y, y) };
            }
        }
    }
}

Rect BrushStroke::paint(RasterBuffer& buffer, const BrushSettings& brush,
                        const std::vector<StrokePoint>& points)
{
    if (points.empty() || buffer.isEmpty())
        return {};

    Rect bounds{ points[0].x, points[0].y, 0, 0 };
    bounds.x = static_cast<int>(points[0].x);
    bounds.y = static_cast<int>(points[0].y);
    bounds.width = 0;
    bounds.height = 0;
    // seed with a 1px cell so union math works even for zero stamps
    bounds = Rect{ bounds.x, bounds.y, 1, 1 };

    for (const auto& p : points)
        stamp(buffer, brush, p, bounds);

    // Clip to the buffer.
    const Rect bufferRect{ 0, 0, buffer.width(), buffer.height() };
    return bounds.intersected(bufferRect);
}

} // namespace prism::canvas
