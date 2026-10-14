#include "brush/BrushStroke.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace prism::canvas {

namespace {
float grainNoise(int x, int y) noexcept {
    std::uint32_t h =
        static_cast<std::uint32_t>(x) * 0x9e3779b1U ^ static_cast<std::uint32_t>(y) * 0x85ebca77U;
    h ^= h >> 16;
    h *= 0x7feb352dU;
    h ^= h >> 15;
    return static_cast<float>(h & 255U) / 255.0F;
}

const std::array<std::uint8_t, 256 * 256>& graphiteGrain() noexcept {
    static const auto tile = [] {
        std::array<std::uint8_t, 256 * 256> pixels{};
        for (int y = 0; y < 256; ++y)
            for (int x = 0; x < 256; ++x) {
                const float grain = 0.6F * grainNoise(x, y) + 0.4F * grainNoise(x / 4, y / 4);
                pixels[static_cast<std::size_t>(y * 256 + x)] =
                    static_cast<std::uint8_t>(grain * 255.0F + 0.5F);
            }
        return pixels;
    }();
    return tile;
}

float tailScale(const BrushSettings& brush, float position, float total) noexcept {
    if (total < 0.0F || brush.endTaperDiameters <= 0.0F)
        return 1.0F;
    const float length = std::min(total * 0.5F, 2.0F * brush.radius *
                                                   std::clamp(brush.endTaperDiameters, 0.0F, 6.0F));
    if (length <= 0.0F)
        return 1.0F;
    const float u = std::clamp((total - position) / length, 0.0F, 1.0F);
    return u * u * (3.0F - 2.0F * u);
}
} // namespace

using prism::core::geometry::Rect;
using prism::core::raster::Color;
using prism::core::raster::RasterBuffer;

float BrushStroke::shapedPressure(const BrushSettings& brush, float pressure) noexcept {
    return std::pow(std::clamp(pressure, 0.0F, 1.0F), std::clamp(brush.pressureCurve, 0.3F, 3.0F));
}

StrokePoint BrushStroke::stabilizePoint(const BrushSettings& brush, const StrokePoint& previous,
                                        const StrokePoint& incoming) noexcept {
    const float dx = incoming.x - previous.x;
    const float dy = incoming.y - previous.y;
    const float distance = std::sqrt(dx * dx + dy * dy);
    const float largeMove = std::clamp(distance / std::max(2.0F * brush.radius, 1.0F), 0.0F, 1.0F);
    const float follow =
        1.0F - std::clamp(brush.stabilization, 0.0F, 1.0F) * 0.75F * (1.0F - largeMove);
    return {previous.x + dx * follow, previous.y + dy * follow, incoming.pressure,
            incoming.tiltX,           incoming.tiltY,           incoming.timeMs};
}

float BrushStroke::coverage(float dist, float radius, float hardness) noexcept {
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
                        float pressure, float tiltX, float tiltY, float taper, Rect& bounds) {
    if (taper <= 0.0F)
        return;
    const bool graphite = brush.material == BrushMaterial::Graphite && !brush.erase;
    const float p = std::clamp(pressure, 0.05F, 1.0F);
    // Pressure scales the stamp, but only across the top half of the range:
    // a pen that barely touches the tablet (drivers report 0.01-0.2 for a light
    // touch) must still draw with the brush the user picked. Scaling linearly
    // from zero turned every light stroke into a 0.6px scratch and made the
    // width swing 3x within one stroke, which reads as broken, angular lines.
    const float minimum = graphite ? std::max(0.75F, std::clamp(brush.pressureSizeMin, 0.05F, 1.0F))
                                   : std::clamp(brush.pressureSizeMin, 0.05F, 1.0F);
    const float sizePressure = minimum + (1.0F - minimum) * p;
    const float radius = std::max(brush.radius * sizePressure * taper, 0.35F);
    const float tiltLength =
        graphite && (tiltX != 0.0F || tiltY != 0.0F) ? std::hypot(tiltX, tiltY) : 0.0F;
    const float tilt = graphite ? std::clamp((tiltLength - 18.0F) / 42.0F, 0.0F, 1.0F) : 0.0F;
    const float shading = tilt * std::clamp(brush.tiltShading, 0.0F, 1.0F);
    const float longAxis = 1.0F + 2.0F * shading;

    // Tip geometry: rotate the sample point by -angle, then stretch y by
    // 1/roundness. Round, square and image tips are all evaluated in that
    // rotated/stretched space, so one path serves every tip kind.
    const bool isImage = brush.tip.kind == BrushTip::Kind::Image && brush.tip.image.maskW > 0 &&
                         brush.tip.image.maskH > 0;
    const float rad = brush.tip.angle * 3.14159265F / 180.0F;
    const float cosA = shading > 0.0F && tiltLength > 0.0F ? tiltX / tiltLength : std::cos(rad);
    const float sinA = shading > 0.0F && tiltLength > 0.0F ? tiltY / tiltLength : std::sin(rad);
    // roundness 0 would divide by zero; clamp to a 2% sliver.
    const float stretch = 1.0F / std::max(brush.tip.roundness, 0.02F);
    const float extentX =
        graphite ? radius * (std::fabs(cosA) * longAxis + std::fabs(sinA) / stretch) : radius;
    const float extentY =
        graphite ? radius * (std::fabs(sinA) * longAxis + std::fabs(cosA) / stretch) : radius;
    const float xCeil = std::ceil(extentX + 0.5F);
    const float yCeil = std::ceil(extentY + 0.5F);
    const int x0 = static_cast<int>(std::floor(cx - xCeil));
    const int y0 = static_cast<int>(std::floor(cy - yCeil));
    const int x1 = static_cast<int>(std::ceil(cx + xCeil));
    const int y1 = static_cast<int>(std::ceil(cy + yCeil));
    const bool square = brush.tip.shape == BrushTip::Shape::Square;
    const float scale = std::clamp(brush.grainScale, 0.5F, 4.0F);
    const float inverseScale = 1.0F / scale;
    const auto* grainTile = graphite ? graphiteGrain().data() : nullptr;
    const float grainAmount = std::clamp(brush.grain, 0.0F, 1.0F);
    const float graphiteDeposit = std::clamp(brush.spacing, 0.02F, 1.0F) * (1.0F - 0.28F * shading);
    const float dyn = graphite ? (0.12F + 0.88F * p) : (brush.pressureOpacity ? p : 1.0F);
    // A hard press should reach dark graphite within one pass, while a light
    // press keeps its many translucent layers. It also lets full-pressure
    // mouse strokes stop touching pixels once they are saturated.
    const float force = graphite ? 1.0F + 5.0F * p * p * p * p : 1.0F;
    const float baseDeposit = 255.0F * std::clamp(brush.flow, 0.0F, 1.0F) * dyn * force * taper *
                              (graphite ? graphiteDeposit : 1.0F);

    for (int y = std::max(0, y0); y <= std::min(buffer.height() - 1, y1); ++y) {
        const int grainY = scale == 1.0F ? y & 255 : static_cast<int>(y * inverseScale) & 255;
        for (int x = std::max(0, x0); x <= std::min(buffer.width() - 1, x1); ++x) {
            const Color existing = buffer.at(x, y);
            if ((!brush.erase && existing.a == 255) || (brush.erase && existing.a == 0))
                continue;
            const float dx = static_cast<float>(x) - cx;
            const float dy = static_cast<float>(y) - cy;
            const float rx = (dx * cosA + dy * sinA) / longAxis;
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
                cov = static_cast<float>(img.mask[static_cast<std::size_t>(my) * img.maskW + mx]) /
                      255.0F;
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
            float material = 1.0F;
            if (graphite) {
                const int grainX =
                    scale == 1.0F ? x & 255 : static_cast<int>(x * inverseScale) & 255;
                const float grain = grainTile[grainY * 256 + grainX] / 255.0F;
                material = 1.0F - grainAmount * (1.0F - grain);
            }
            const std::uint8_t deposit =
                static_cast<std::uint8_t>(baseDeposit * cov * material + 0.5F);
            if (deposit == 0)
                continue;

            if (brush.erase) {
                const std::uint8_t a =
                    static_cast<std::uint8_t>(std::max(0, static_cast<int>(existing.a) - deposit));
                if (a != existing.a) {
                    buffer.set(x, y, Color{existing.r, existing.g, existing.b, a});
                    bounds =
                        Rect{std::min(bounds.x, x), std::min(bounds.y, y),
                             std::max(bounds.x + bounds.width, x + 1) - std::min(bounds.x, x),
                             std::max(bounds.y + bounds.height, y + 1) - std::min(bounds.y, y)};
                }
            } else {
                // Accumulate paint within a stroke (scratch buffer is
                // per-stroke, so alpha only ever grows toward full).
                const std::uint8_t a = static_cast<std::uint8_t>(
                    std::min(255, static_cast<int>(existing.a) + deposit));
                if (a != existing.a) {
                    buffer.set(x, y, Color{brush.color.r, brush.color.g, brush.color.b, a});
                    bounds =
                        Rect{std::min(bounds.x, x), std::min(bounds.y, y),
                             std::max(bounds.x + bounds.width, x + 1) - std::min(bounds.x, x),
                             std::max(bounds.y + bounds.height, y + 1) - std::min(bounds.y, y)};
                }
            }
        }
    }
}

Rect BrushStroke::paintSegment(RasterBuffer& buffer, const BrushSettings& brush,
                               const StrokePoint& prev, const StrokePoint& current,
                               StrokeState* state) {
    StrokeState localState;
    if (!state)
        state = &localState;
    Rect bounds{static_cast<int>(prev.x), static_cast<int>(prev.y), 0, 0};
    const float dx = current.x - prev.x;
    const float dy = current.y - prev.y;
    if (!state->started) {
        state->filteredPressure = prev.pressure;
        state->lastTimeMs = prev.timeMs;
    }
    const float previousPressure = shapedPressure(brush, state->filteredPressure);
    if (brush.material == BrushMaterial::Graphite) {
        const float dt = current.timeMs >= 0.0F && state->lastTimeMs >= 0.0F
                             ? std::clamp(current.timeMs - state->lastTimeMs, 0.0F, 64.0F)
                             : 8.0F;
        state->filteredPressure +=
            (current.pressure - state->filteredPressure) * (1.0F - std::exp(-dt / 10.0F));
    } else {
        state->filteredPressure = current.pressure;
    }
    state->lastTimeMs = current.timeMs;
    const float currentPressure = shapedPressure(brush, state->filteredPressure);
    const float dist = std::sqrt(dx * dx + dy * dy);
    const float step = std::max(2.0F * brush.radius * std::clamp(brush.spacing, 0.02F, 1.0F), 1.0F);
    if (!state->started) {
        stamp(buffer, brush, prev.x, prev.y, previousPressure, prev.tiltX, prev.tiltY,
              tailScale(brush, state->pathDistance, state->totalDistance), bounds);
        state->started = true;
        state->distanceToNext = step;
    }
    float travelled = 0.0F;
    while (dist > 1e-4F && state->distanceToNext <= dist - travelled + 1e-4F) {
        travelled += state->distanceToNext;
        const float t = std::min(travelled / dist, 1.0F);
        stamp(buffer, brush, prev.x + dx * t, prev.y + dy * t,
              previousPressure + (currentPressure - previousPressure) * t,
              prev.tiltX + (current.tiltX - prev.tiltX) * t,
              prev.tiltY + (current.tiltY - prev.tiltY) * t,
              tailScale(brush, state->pathDistance + travelled, state->totalDistance), bounds);
        state->distanceToNext = step;
    }
    state->distanceToNext -= dist - travelled;
    state->pathDistance += dist;
    return bounds.intersected(Rect{0, 0, buffer.width(), buffer.height()});
}

Rect BrushStroke::finishStroke(RasterBuffer& buffer, const BrushSettings& brush,
                               const StrokePoint& current, StrokeState& state) {
    if (!state.started ||
        state.distanceToNext >=
            std::max(2.0F * brush.radius * std::clamp(brush.spacing, 0.02F, 1.0F), 1.0F) - 0.5F)
        return {};
    Rect bounds{static_cast<int>(current.x), static_cast<int>(current.y), 0, 0};
    stamp(buffer, brush, current.x, current.y, shapedPressure(brush, state.filteredPressure),
          current.tiltX, current.tiltY,
          tailScale(brush, state.pathDistance, state.totalDistance), bounds);
    return bounds.intersected(Rect{0, 0, buffer.width(), buffer.height()});
}

Rect BrushStroke::smudgeSegment(RasterBuffer& buffer, const BrushSettings& brush,
                                const StrokePoint& prev, const StrokePoint& current,
                                prism::core::raster::Color& carried) {
    if (buffer.isEmpty())
        return {};

    using prism::core::raster::Color;
    const auto mix = [](Color a, Color b, float t) {
        const auto ch = [t](std::uint8_t x, std::uint8_t y) {
            return static_cast<std::uint8_t>(x * (1.0F - t) + y * t + 0.5F);
        };
        return Color{ch(a.r, b.r), ch(a.g, b.g), ch(a.b, b.b), ch(a.a, b.a)};
    };

    Rect bounds{static_cast<int>(prev.x), static_cast<int>(prev.y), 0, 0};
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
            const Color here{static_cast<std::uint8_t>(sr / sn), static_cast<std::uint8_t>(sg / sn),
                             static_cast<std::uint8_t>(sb / sn),
                             static_cast<std::uint8_t>(sa / sn)};
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
                bounds = Rect{std::min(bounds.x, sx), std::min(bounds.y, sy),
                              std::max(bounds.x + bounds.width, sx + 1) - std::min(bounds.x, sx),
                              std::max(bounds.y + bounds.height, sy + 1) - std::min(bounds.y, sy)};
            }
        }
    }

    if (bounds.width == 0 || bounds.height == 0)
        bounds = Rect{bounds.x, bounds.y, 1, 1};
    return bounds.intersected(Rect{0, 0, buffer.width(), buffer.height()});
}

Rect BrushStroke::paint(RasterBuffer& buffer, const BrushSettings& brush,
                        const std::vector<StrokePoint>& points) {
    if (points.empty() || buffer.isEmpty())
        return {};

    Rect total{static_cast<int>(points[0].x), static_cast<int>(points[0].y), 0, 0};
    bool touched = false;
    if (points.size() == 1) {
        stamp(buffer, brush, points[0].x, points[0].y, shapedPressure(brush, points[0].pressure),
              points[0].tiltX, points[0].tiltY, 1.0F, total);
        touched = total.width > 0 && total.height > 0;
    } else {
        StrokeState state;
        if (brush.endTaperDiameters > 0.0F) {
            state.totalDistance = 0.0F;
            for (std::size_t i = 1; i < points.size(); ++i)
                state.totalDistance += std::hypot(points[i].x - points[i - 1].x,
                                                  points[i].y - points[i - 1].y);
        }
        for (std::size_t i = 1; i < points.size(); ++i) {
            auto r = paintSegment(buffer, brush, points[i - 1], points[i], &state);
            if (!r.isEmpty()) {
                total =
                    Rect{std::min(total.x, r.x), std::min(total.y, r.y),
                         std::max(total.x + total.width, r.x + r.width) - std::min(total.x, r.x),
                         std::max(total.y + total.height, r.y + r.height) - std::min(total.y, r.y)};
                touched = true;
            }
        }
        const auto r = finishStroke(buffer, brush, points.back(), state);
        if (!r.isEmpty()) {
            total = Rect{std::min(total.x, r.x), std::min(total.y, r.y),
                         std::max(total.x + total.width, r.x + r.width) - std::min(total.x, r.x),
                         std::max(total.y + total.height, r.y + r.height) - std::min(total.y, r.y)};
            touched = true;
        }
    }
    if (!touched)
        return {};
    if (total.width == 0 || total.height == 0)
        total = Rect{total.x, total.y, 1, 1};
    return total.intersected(Rect{0, 0, buffer.width(), buffer.height()});
}

} // namespace prism::canvas
