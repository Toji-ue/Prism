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

Rect strokeAlong(RasterBuffer& buffer, const std::vector<StrokePoint>& path,
                 const BrushSettings& brush)
{
    if (path.empty())
        return {};
    BrushSettings b = brush;
    b.erase = false; // erasing stays the eraser tool's job
    return BrushStroke::paint(buffer, b, path);
}

std::vector<StrokePoint> bezierPath(Point p0, Point c0, Point c1, Point p3, int segments)
{
    std::vector<StrokePoint> out;
    out.reserve(static_cast<std::size_t>(segments) + 1);
    for (int i = 0; i <= segments; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(segments);
        const float u = 1.0F - t;
        const float x = u * u * u * static_cast<float>(p0.x)
                      + 3 * u * u * t * static_cast<float>(c0.x)
                      + 3 * u * t * t * static_cast<float>(c1.x)
                      + t * t * t * static_cast<float>(p3.x);
        const float y = u * u * u * static_cast<float>(p0.y)
                      + 3 * u * u * t * static_cast<float>(c0.y)
                      + 3 * u * t * t * static_cast<float>(c1.y)
                      + t * t * t * static_cast<float>(p3.y);
        out.push_back({ x, y, 1.0F });
    }
    return out;
}

std::vector<StrokePoint> arcPath(Point start, Point end, int segments)
{
    const float cx = (static_cast<float>(start.x) + static_cast<float>(end.x)) * 0.5F;
    const float cy = (static_cast<float>(start.y) + static_cast<float>(end.y)) * 0.5F;
    const float rx = std::abs(static_cast<float>(end.x - start.x)) * 0.5F;
    const float ry = std::abs(static_cast<float>(end.y - start.y)) * 0.5F;

    std::vector<StrokePoint> out;
    out.reserve(static_cast<std::size_t>(segments) + 1);
    // Upper half of the ellipse inscribed in the drag rectangle.
    for (int i = 0; i <= segments; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(segments) * 3.14159265F;
        out.push_back({ cx + std::cos(t) * rx, cy - std::sin(t) * ry, 1.0F });
    }
    return out;
}

std::vector<StrokePoint> smoothPath(const std::vector<StrokePoint>& input, int passes)
{
    if (input.size() < 3 || passes <= 0)
        return input;
    std::vector<StrokePoint> current = input;
    for (int p = 0; p < passes; ++p) {
        std::vector<StrokePoint> next;
        next.reserve(current.size() * 2);
        next.push_back(current.front());
        for (std::size_t i = 0; i + 1 < current.size(); ++i) {
            const auto& a = current[i];
            const auto& b = current[i + 1];
            next.push_back({ a.x * 0.75F + b.x * 0.25F, a.y * 0.75F + b.y * 0.25F, a.pressure });
            next.push_back({ a.x * 0.25F + b.x * 0.75F, a.y * 0.25F + b.y * 0.75F, b.pressure });
        }
        next.push_back(current.back());
        current = std::move(next);
    }
    return current;
}

Rect fillPattern(RasterBuffer& buffer, PatternKind kind, int scale, Color color,
                 const Rect& limit)
{
    if (buffer.isEmpty())
        return {};
    const int s = std::max(2, scale);
    const Rect area = (limit.isEmpty() ? Rect{ 0, 0, buffer.width(), buffer.height() }
                                       : limit.intersected(Rect{ 0, 0, buffer.width(),
                                                                 buffer.height() }));
    if (area.isEmpty())
        return {};

    for (int y = area.y; y < area.y + area.height; ++y) {
        for (int x = area.x; x < area.x + area.width; ++x) {
            bool on = false;
            switch (kind) {
            case PatternKind::Checker:
                on = ((x / s) + (y / s)) % 2 == 0;
                break;
            case PatternKind::Stripes:
                on = (x / s) % 2 == 0;
                break;
            case PatternKind::Dots: {
                const int px = x % s - s / 2;
                const int py = y % s - s / 2;
                on = px * px + py * py <= (s / 4) * (s / 4);
                break;
            }
            }
            if (!on)
                continue;
            const Color dst = buffer.at(x, y);
            const float sa = static_cast<float>(color.a) / 255.0F;
            buffer.set(x, y,
                       Color{ static_cast<std::uint8_t>(color.r * sa + dst.r * (1 - sa)),
                              static_cast<std::uint8_t>(color.g * sa + dst.g * (1 - sa)),
                              static_cast<std::uint8_t>(color.b * sa + dst.b * (1 - sa)),
                              std::max(dst.a, color.a) });
        }
    }
    return area;
}

Rect transformBuffer(RasterBuffer& buffer, float degrees, float scale, float cx, float cy)
{
    if (buffer.isEmpty() || (std::abs(degrees) < 0.01F && std::abs(scale - 1.0F) < 0.001F))
        return {};

    const RasterBuffer source = buffer;
    const float rad = degrees * 3.14159265F / 180.0F;
    const float c = std::cos(rad);
    const float s = std::sin(rad);
    const float inv = scale > 0.0001F ? 1.0F / scale : 0.0F;

    for (int y = 0; y < buffer.height(); ++y) {
        for (int x = 0; x < buffer.width(); ++x) {
            // Inverse map: destination -> source, so no gaps appear.
            const float dx = (static_cast<float>(x) - cx) * inv;
            const float dy = (static_cast<float>(y) - cy) * inv;
            const float sx = c * dx + s * dy + cx;
            const float sy = -s * dx + c * dy + cy;

            const int x0 = static_cast<int>(std::floor(sx));
            const int y0 = static_cast<int>(std::floor(sy));
            if (x0 < 0 || y0 < 0 || x0 + 1 >= source.width() || y0 + 1 >= source.height()) {
                buffer.set(x, y, prism::core::raster::kTransparent);
                continue;
            }
            // Bilinear sample.
            const float fx = sx - static_cast<float>(x0);
            const float fy = sy - static_cast<float>(y0);
            const Color p00 = source.at(x0, y0);
            const Color p10 = source.at(x0 + 1, y0);
            const Color p01 = source.at(x0, y0 + 1);
            const Color p11 = source.at(x0 + 1, y0 + 1);
            const auto mix = [&](std::uint8_t a, std::uint8_t b, std::uint8_t cc,
                                 std::uint8_t d) {
                const float top = a * (1 - fx) + b * fx;
                const float bottom = cc * (1 - fx) + d * fx;
                return static_cast<std::uint8_t>(top * (1 - fy) + bottom * fy + 0.5F);
            };
            buffer.set(x, y,
                       Color{ mix(p00.r, p10.r, p01.r, p11.r), mix(p00.g, p10.g, p01.g, p11.g),
                              mix(p00.b, p10.b, p01.b, p11.b),
                              mix(p00.a, p10.a, p01.a, p11.a) });
        }
    }
    return Rect{ 0, 0, buffer.width(), buffer.height() };
}

} // namespace prism::canvas
