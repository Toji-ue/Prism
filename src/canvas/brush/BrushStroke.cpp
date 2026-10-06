#include "brush/BrushStroke.h"

#include <algorithm>
#include <cmath>

namespace prism::canvas {

using prism::core::geometry::Rect;
using prism::core::raster::Color;
using prism::core::raster::RasterBuffer;

float BrushStroke::coverage(float dist, float radius, float hardness) noexcept
{
    // Soft brushes: solid core, linear falloff to a soft rim.
    // Hard brushes: solid to the radius, with a 0.5px anti-aliased rim so
    // edges aren't jagged.
    const float core = radius * std::clamp(hardness, 0.0F, 1.0F);
    if (dist <= core)
        return 1.0F;
    const float outer = radius + 0.5F;
    if (dist >= outer)
        return 0.0F;
    return (outer - dist) / (outer - core);
}

void BrushStroke::stamp(RasterBuffer& buffer, const BrushSettings& brush, float cx, float cy,
                        float pressure, Rect& bounds)
{
    const float p = std::clamp(pressure, 0.05F, 1.0F);
    // Pressure scales the stamp, but only across the top half of the range:
    // a pen that barely touches the tablet (drivers report 0.01-0.2 for a light
    // touch) must still draw with the brush the user picked. Scaling linearly
    // from zero turned every light stroke into a 0.6px scratch and made the
    // width swing 3x within one stroke, which reads as broken, angular lines.
    const float sizePressure = 0.5F + 0.5F * p;
    const float radius = std::max(brush.radius * sizePressure, 1.0F);
    const float rCeil = std::ceil(radius + 0.5F);
    const int x0 = static_cast<int>(std::floor(cx - rCeil));
    const int y0 = static_cast<int>(std::floor(cy - rCeil));
    const int x1 = static_cast<int>(std::ceil(cx + rCeil));
    const int y1 = static_cast<int>(std::ceil(cy + rCeil));

    // Tip geometry: rotate the sample point by -angle, then stretch y by
    // 1/roundness. Round, square and image tips are all evaluated in that
    // rotated/stretched space, so one path serves every tip kind.
    const bool isImage = brush.tip.kind == BrushTip::Kind::Image && brush.tip.image.maskW > 0
                         && brush.tip.image.maskH > 0;
    const float rad = brush.tip.angle * 3.14159265F / 180.0F;
    const float cosA = std::cos(rad);
    const float sinA = std::sin(rad);
    // roundness 0 would divide by zero; clamp to a 2% sliver.
    const float stretch = 1.0F / std::max(brush.tip.roundness, 0.02F);
    const bool square = brush.tip.shape == BrushTip::Shape::Square;

    for (int y = y0; y <= y1; ++y) {
        for (int x = x0; x <= x1; ++x) {
            if (x < 0 || y < 0 || x >= buffer.width() || y >= buffer.height())
                continue;
            const float dx = static_cast<float>(x) - cx;
            const float dy = static_cast<float>(y) - cy;
            const float rx = dx * cosA + dy * sinA;
            const float ry = (-dx * sinA + dy * cosA) * stretch;

            float cov;
            if (isImage) {
                // Nearest-neighbour sample of the grayscale mask: the stamp's
                // rotated extent mapped onto the whole image.
                const auto& img = brush.tip.image;
                const float u = (rx / radius * 0.5F + 0.5F) * static_cast<float>(img.maskW);
                const float v = (ry / radius * 0.5F + 0.5F) * static_cast<float>(img.maskH);
                const int mx = static_cast<int>(u);
                const int my = static_cast<int>(v);
                if (mx < 0 || my < 0 || mx >= img.maskW || my >= img.maskH)
                    continue;
                cov = static_cast<float>(img.mask[static_cast<std::size_t>(my) * img.maskW + mx])
                      / 255.0F;
                if (img.invert)
                    cov = 1.0F - cov;
            } else if (square) {
                // Chebyshev distance gives a square with a soft rim.
                cov = coverage(std::max(std::fabs(rx), std::fabs(ry)), radius, brush.hardness);
            } else {
                cov = coverage(std::sqrt(rx * rx + ry * ry), radius, brush.hardness);
            }
            if (cov <= 0.0F)
                continue;

            // Dynamics: pressure scales deposited alpha, on top of the size
            // scaling above. Off by default so mouse input (pressure always
            // 1.0) paints exactly as before.
            const float dyn = brush.pressureOpacity ? p : 1.0F;
            const std::uint8_t deposit = static_cast<std::uint8_t>(
                255.0F * cov * std::clamp(brush.flow, 0.0F, 1.0F) * dyn + 0.5F);
            if (deposit == 0)
                continue;

            const Color existing = buffer.at(x, y);
            if (brush.erase) {
                const std::uint8_t a = static_cast<std::uint8_t>(
                    std::max(0, static_cast<int>(existing.a) - deposit));
                if (a != existing.a) {
                    buffer.set(x, y, Color{ existing.r, existing.g, existing.b, a });
                    bounds = Rect{ std::min(bounds.x, x), std::min(bounds.y, y),
                                   std::max(bounds.x + bounds.width, x + 1) - std::min(bounds.x, x),
                                   std::max(bounds.y + bounds.height, y + 1) - std::min(bounds.y, y) };
                }
            } else {
                // Accumulate paint within a stroke (scratch buffer is
                // per-stroke, so alpha only ever grows toward full).
                const std::uint8_t a = static_cast<std::uint8_t>(
                    std::min(255, static_cast<int>(existing.a) + deposit));
                if (a != existing.a) {
                    buffer.set(x, y, Color{ brush.color.r, brush.color.g, brush.color.b, a });
                    bounds = Rect{ std::min(bounds.x, x), std::min(bounds.y, y),
                                   std::max(bounds.x + bounds.width, x + 1) - std::min(bounds.x, x),
                                   std::max(bounds.y + bounds.height, y + 1) - std::min(bounds.y, y) };
                }
            }
        }
    }
}

Rect BrushStroke::paintSegment(RasterBuffer& buffer, const BrushSettings& brush,
                               const StrokePoint& prev, const StrokePoint& current)
{
    Rect bounds{ static_cast<int>(prev.x), static_cast<int>(prev.y), 0, 0 };
    const float dx = current.x - prev.x;
    const float dy = current.y - prev.y;
    const float dist = std::sqrt(dx * dx + dy * dy);
    const float step = std::max(brush.radius * 0.25F, 1.0F);
    // A zero-length segment (a click) must stamp ONCE: n=1 made the loop run
    // twice at t=0, depositing double alpha on every click — invisible while
    // deposits saturated at 255, glaring once pressureOpacity scaled them.
    const int n = dist < 1e-4F ? 0 : std::max(1, static_cast<int>(std::ceil(dist / step)));
    for (int i = 0; i <= n; ++i) {
        const float t = dist < 1e-4F ? 0.0F : static_cast<float>(i) / static_cast<float>(n);
        const float px = prev.x + dx * t;
        const float py = prev.y + dy * t;
        const float pp = prev.pressure + (current.pressure - prev.pressure) * t;
        stamp(buffer, brush, px, py, pp, bounds);
    }
    // Ensure at least the starting pixel is part of the bounds.
    if (bounds.width == 0 || bounds.height == 0)
        bounds = Rect{ bounds.x, bounds.y, 1, 1 };
    return bounds.intersected(Rect{ 0, 0, buffer.width(), buffer.height() });
}

Rect BrushStroke::smudgeSegment(RasterBuffer& buffer, const BrushSettings& brush,
                                const StrokePoint& prev, const StrokePoint& current,
                                prism::core::raster::Color& carried)
{
    if (buffer.isEmpty())
        return {};

    using prism::core::raster::Color;
    const auto mix = [](Color a, Color b, float t) {
        const auto ch = [t](std::uint8_t x, std::uint8_t y) {
            return static_cast<std::uint8_t>(x * (1.0F - t) + y * t + 0.5F);
        };
        return Color{ ch(a.r, b.r), ch(a.g, b.g), ch(a.b, b.b), ch(a.a, b.a) };
    };

    Rect bounds{ static_cast<int>(prev.x), static_cast<int>(prev.y), 0, 0 };
    const float dx = current.x - prev.x;
    const float dy = current.y - prev.y;
    const float dist = std::sqrt(dx * dx + dy * dy);
    const float step = std::max(brush.radius * 0.5F, 1.0F);
    const int n = std::max(1, static_cast<int>(std::ceil(dist / step)));
    const int r = std::max(1, static_cast<int>(std::ceil(brush.radius)));
    const float strength = std::clamp(brush.flow, 0.0F, 1.0F);

    for (int i = 0; i <= n; ++i) {
        const float t = dist < 1e-4F ? 0.0F : static_cast<float>(i) / static_cast<float>(n);
        const float px = prev.x + dx * t;
        const float py = prev.y + dy * t;

        // What the brush is standing on right now: plain average over the disc,
        // sampled every other pixel — a full scan per stamp pushed the stamp
        // cost past the brush's own.
        int sr = 0, sg = 0, sb = 0, sa = 0, sn = 0;
        for (int oy = -r; oy <= r; oy += 2) {
            for (int ox = -r; ox <= r; ox += 2) {
                if (ox * ox + oy * oy > r * r)
                    continue;
                const int sx = static_cast<int>(px) + ox;
                const int sy = static_cast<int>(py) + oy;
                if (sx < 0 || sy < 0 || sx >= buffer.width() || sy >= buffer.height())
                    continue;
                const Color c = buffer.at(sx, sy);
                sr += c.r;
                sg += c.g;
                sb += c.b;
                sa += c.a;
                ++sn;
            }
        }
        if (sn > 0) {
            const Color here{ static_cast<std::uint8_t>(sr / sn),
                              static_cast<std::uint8_t>(sg / sn),
                              static_cast<std::uint8_t>(sb / sn),
                              static_cast<std::uint8_t>(sa / sn) };
            // First stamp picks up what is under the brush; afterwards the load
            // follows the canvas so the smear keeps pulling in new colour.
            carried = carried.a == 0 && t == 0.0F ? here : mix(carried, here, 0.5F);
        }

        for (int oy = -r; oy <= r; ++oy) {
            for (int ox = -r; ox <= r; ++ox) {
                const float d = std::sqrt(static_cast<float>(ox * ox + oy * oy));
                const float cov = coverage(d, brush.radius, brush.hardness);
                if (cov <= 0.0F)
                    continue;
                const int sx = static_cast<int>(px) + ox;
                const int sy = static_cast<int>(py) + oy;
                if (sx < 0 || sy < 0 || sx >= buffer.width() || sy >= buffer.height())
                    continue;
                const Color dst = buffer.at(sx, sy);
                const Color out = mix(dst, carried, cov * strength);
                if (out.r == dst.r && out.g == dst.g && out.b == dst.b && out.a == dst.a)
                    continue;
                buffer.set(sx, sy, out);
                bounds = Rect{ std::min(bounds.x, sx), std::min(bounds.y, sy),
                               std::max(bounds.x + bounds.width, sx + 1) - std::min(bounds.x, sx),
                               std::max(bounds.y + bounds.height, sy + 1) - std::min(bounds.y, sy) };
            }
        }
    }

    if (bounds.width == 0 || bounds.height == 0)
        bounds = Rect{ bounds.x, bounds.y, 1, 1 };
    return bounds.intersected(Rect{ 0, 0, buffer.width(), buffer.height() });
}

Rect BrushStroke::paint(RasterBuffer& buffer, const BrushSettings& brush,
                        const std::vector<StrokePoint>& points)
{
    if (points.empty() || buffer.isEmpty())
        return {};

    Rect total{ static_cast<int>(points[0].x), static_cast<int>(points[0].y), 0, 0 };
    bool touched = false;
    if (points.size() == 1) {
        stamp(buffer, brush, points[0].x, points[0].y, points[0].pressure, total);
        touched = total.width > 0 && total.height > 0;
    } else {
        for (std::size_t i = 1; i < points.size(); ++i) {
            auto r = paintSegment(buffer, brush, points[i - 1], points[i]);
            if (!r.isEmpty()) {
                total = Rect{ std::min(total.x, r.x), std::min(total.y, r.y),
                              std::max(total.x + total.width, r.x + r.width) - std::min(total.x, r.x),
                              std::max(total.y + total.height, r.y + r.height)
                                  - std::min(total.y, r.y) };
                touched = true;
            }
        }
    }
    if (!touched)
        return {};
    if (total.width == 0 || total.height == 0)
        total = Rect{ total.x, total.y, 1, 1 };
    return total.intersected(Rect{ 0, 0, buffer.width(), buffer.height() });
}

} // namespace prism::canvas
