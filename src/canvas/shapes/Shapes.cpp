#include "shapes/Shapes.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace prism::canvas {

using prism::core::geometry::Point;
using prism::core::geometry::Rect;
using prism::core::raster::Color;
using prism::core::raster::RasterBuffer;

namespace {

Rect boundsOf(const RasterBuffer& buffer)
{
    return Rect{ 0, 0, buffer.width(), buffer.height() };
}

/// Stamps the brush along a polyline outline.
Rect strokePath(RasterBuffer& buffer, const std::vector<StrokePoint>& path,
                const BrushSettings& brush)
{
    BrushSettings b = brush;
    b.erase = false; // shapes always deposit; erase is the eraser tool's job
    return BrushStroke::paint(buffer, b, path);
}

std::vector<StrokePoint> ellipsePath(Point a, Point b, int segments)
{
    const float cx = (static_cast<float>(a.x) + static_cast<float>(b.x)) * 0.5F;
    const float cy = (static_cast<float>(a.y) + static_cast<float>(b.y)) * 0.5F;
    const float rx = std::abs(static_cast<float>(b.x - a.x)) * 0.5F;
    const float ry = std::abs(static_cast<float>(b.y - a.y)) * 0.5F;

    std::vector<StrokePoint> path;
    path.reserve(static_cast<std::size_t>(segments) + 1);
    for (int i = 0; i <= segments; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(segments) * 6.2831853F;
        path.push_back({ cx + std::cos(t) * rx, cy + std::sin(t) * ry, 1.0F });
    }
    return path;
}

std::vector<StrokePoint> rectPath(Point a, Point b)
{
    const float x0 = static_cast<float>(std::min(a.x, b.x));
    const float y0 = static_cast<float>(std::min(a.y, b.y));
    const float x1 = static_cast<float>(std::max(a.x, b.x));
    const float y1 = static_cast<float>(std::max(a.y, b.y));
    return { { x0, y0, 1.0F }, { x1, y0, 1.0F }, { x1, y1, 1.0F }, { x0, y1, 1.0F },
             { x0, y0, 1.0F } };
}

} // namespace

Rect rasterizeShape(RasterBuffer& buffer, ShapeKind kind, Point start, Point end,
                    const BrushSettings& brush, bool filled)
{
    if (buffer.isEmpty())
        return {};

    const Rect area = Rect{ std::min(start.x, end.x), std::min(start.y, end.y),
                            std::abs(end.x - start.x) + 1, std::abs(end.y - start.y) + 1 }
                          .intersected(boundsOf(buffer));
    if (area.isEmpty())
        return {};

    switch (kind) {
    case ShapeKind::Line:
        return strokePath(buffer, { { static_cast<float>(start.x), static_cast<float>(start.y), 1.0F },
                                    { static_cast<float>(end.x), static_cast<float>(end.y), 1.0F } },
                          brush);

    case ShapeKind::Rectangle: {
        Rect touched = strokePath(buffer, rectPath(start, end), brush);
        if (filled) {
            const Color c = brush.color;
            const auto on = static_cast<std::uint8_t>(255.0F * std::clamp(brush.flow, 0.0F, 1.0F));
            for (int y = area.y; y < area.y + area.height; ++y)
                for (int x = area.x; x < area.x + area.width; ++x)
                    buffer.set(x, y, Color{ c.r, c.g, c.b, on });
            touched = Rect{ std::min(touched.x, area.x), std::min(touched.y, area.y),
                            std::max(touched.x + touched.width, area.x + area.width)
                                - std::min(touched.x, area.x),
                            std::max(touched.y + touched.height, area.y + area.height)
                                - std::min(touched.y, area.y) };
        }
        return touched;
    }

    case ShapeKind::Ellipse: {
        const int segments = std::max(24, (area.width + area.height) / 2);
        Rect touched = strokePath(buffer, ellipsePath(start, end, segments), brush);
        if (filled) {
            const Color c = brush.color;
            const auto on = static_cast<std::uint8_t>(255.0F * std::clamp(brush.flow, 0.0F, 1.0F));
            const float cx = (static_cast<float>(area.x) + static_cast<float>(area.x + area.width)) * 0.5F;
            const float cy = (static_cast<float>(area.y) + static_cast<float>(area.y + area.height)) * 0.5F;
            const float rx = static_cast<float>(area.width) * 0.5F;
            const float ry = static_cast<float>(area.height) * 0.5F;
            for (int y = area.y; y < area.y + area.height; ++y) {
                for (int x = area.x; x < area.x + area.width; ++x) {
                    const float dx = (static_cast<float>(x) + 0.5F - cx) / std::max(rx, 1.0F);
                    const float dy = (static_cast<float>(y) + 0.5F - cy) / std::max(ry, 1.0F);
                    if (dx * dx + dy * dy <= 1.0F)
                        buffer.set(x, y, Color{ c.r, c.g, c.b, on });
                }
            }
        }
        return touched;
    }
    }
    return {};
}

Rect linearGradient(RasterBuffer& buffer, Point start, Point end, Color from, Color to)
{
    if (buffer.isEmpty())
        return {};

    const float dx = static_cast<float>(end.x - start.x);
    const float dy = static_cast<float>(end.y - start.y);
    const float lenSq = dx * dx + dy * dy;
    if (lenSq < 1.0F)
        return {};

    const Rect area = boundsOf(buffer);
    for (int y = area.y; y < area.y + area.height; ++y) {
        for (int x = area.x; x < area.x + area.width; ++x) {
            const float t = std::clamp(((static_cast<float>(x) - start.x) * dx
                                        + (static_cast<float>(y) - start.y) * dy)
                                           / lenSq,
                                       0.0F, 1.0F);
            const auto lerp = [t](std::uint8_t a, std::uint8_t b) {
                return static_cast<std::uint8_t>(static_cast<float>(a)
                                                 + (static_cast<float>(b) - static_cast<float>(a)) * t
                                                 + 0.5F);
            };
            const Color c{ lerp(from.r, to.r), lerp(from.g, to.g), lerp(from.b, to.b),
                           lerp(from.a, to.a) };
            // Source-over so a gradient paints instead of replacing.
            const Color dst = buffer.at(x, y);
            const float sa = static_cast<float>(c.a) / 255.0F;
            buffer.set(x, y,
                       Color{ static_cast<std::uint8_t>(c.r * sa + dst.r * (1 - sa)),
                              static_cast<std::uint8_t>(c.g * sa + dst.g * (1 - sa)),
                              static_cast<std::uint8_t>(c.b * sa + dst.b * (1 - sa)),
                              std::max(dst.a, c.a) });
        }
    }
    return area;
}

} // namespace prism::canvas
