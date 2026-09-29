#include "selection/Selection.h"

#include "fill/FloodFill.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <vector>

namespace prism::canvas {

using prism::core::geometry::Point;
using prism::core::raster::Color;
using prism::core::raster::kTransparent;

namespace {

/// Even-odd point-in-polygon test on a scanline's centre.
bool insidePolygon(const std::vector<Point>& poly, float px, float py)
{
    bool inside = false;
    const std::size_t n = poly.size();
    for (std::size_t i = 0, j = n - 1; i < n; j = i++) {
        const float yi = static_cast<float>(poly[i].y);
        const float yj = static_cast<float>(poly[j].y);
        if ((yi > py) == (yj > py))
            continue;
        const float xi = static_cast<float>(poly[i].x);
        const float xj = static_cast<float>(poly[j].x);
        const float xAt = xi + (py - yi) / (yj - yi) * (xj - xi);
        if (px < xAt)
            inside = !inside;
    }
    return inside;
}

} // namespace

Selection::Selection(int width, int height)
    : m_mask(width, height)
{
}

void Selection::reset(int width, int height)
{
    m_mask = RasterBuffer(width, height);
    m_empty = true;
    m_bounds = {};
    ++m_revision;
}

std::uint8_t Selection::coverageAt(int x, int y) const
{
    if (x < 0 || y < 0 || x >= m_mask.width() || y >= m_mask.height())
        return 0;
    return m_mask.at(x, y).a;
}

void Selection::clear()
{
    if (m_empty)
        return;
    m_mask.fill(kTransparent);
    m_empty = true;
    m_bounds = {};
    ++m_revision;
}

void Selection::selectRect(const Rect& rect)
{
    m_mask.fill(kTransparent);
    const Rect area = rect.intersected(Rect{ 0, 0, m_mask.width(), m_mask.height() });
    if (area.isEmpty()) {
        m_empty = true;
        m_bounds = {};
        ++m_revision;
        return;
    }
    const Color on{ 255, 255, 255, 255 };
    for (int y = area.y; y < area.y + area.height; ++y)
        for (int x = area.x; x < area.x + area.width; ++x)
            m_mask.set(x, y, on);

    m_empty = false;
    m_bounds = area;
    ++m_revision;
}

void Selection::selectAll()
{
    selectRect(Rect{ 0, 0, m_mask.width(), m_mask.height() });
}

void Selection::invert()
{
    if (m_mask.isEmpty())
        return;
    if (m_empty) {
        selectAll();
        return;
    }
    const Color on{ 255, 255, 255, 255 };
    for (int y = 0; y < m_mask.height(); ++y) {
        for (int x = 0; x < m_mask.width(); ++x) {
            const bool selected = m_mask.at(x, y).a > 0;
            m_mask.set(x, y, selected ? kTransparent : on);
        }
    }
    m_empty = false;
    recomputeBounds();
    ++m_revision;
}

void Selection::selectPolygon(const std::vector<Point>& points)
{
    m_mask.fill(kTransparent);
    if (points.size() < 3) {
        m_empty = true;
        m_bounds = {};
        ++m_revision;
        return;
    }

    // Compute the polygon's bounds, then test only pixels inside them.
    int minX = points[0].x, maxX = points[0].x, minY = points[0].y, maxY = points[0].y;
    for (const auto& p : points) {
        minX = std::min(minX, p.x);
        maxX = std::max(maxX, p.x);
        minY = std::min(minY, p.y);
        maxY = std::max(maxY, p.y);
    }
    const Rect area{ minX, minY, maxX - minX + 1, maxY - minY + 1 };
    const Rect clipped = area.intersected(Rect{ 0, 0, m_mask.width(), m_mask.height() });
    if (clipped.isEmpty()) {
        m_empty = true;
        m_bounds = {};
        ++m_revision;
        return;
    }

    const Color on{ 255, 255, 255, 255 };
    for (int y = clipped.y; y < clipped.y + clipped.height; ++y)
        for (int x = clipped.x; x < clipped.x + clipped.width; ++x)
            if (insidePolygon(points, static_cast<float>(x) + 0.5F, static_cast<float>(y) + 0.5F))
                m_mask.set(x, y, on);

    m_empty = false;
    recomputeBounds();
    ++m_revision;
}

void Selection::selectSimilar(const RasterBuffer& source, int x, int y, int tolerance)
{
    m_mask.fill(kTransparent);
    if (source.isEmpty()) {
        m_empty = true;
        ++m_revision;
        return;
    }

    // Region-grow directly into the mask. Using floodFill with a sentinel
    // color would break when the region already has that color.
    const Color on{ 255, 255, 255, 255 };
    const auto rect = floodFillRegion(source, x, y, tolerance,
                                      [&](int px, int py, Color) { m_mask.set(px, py, on); });
    if (rect.isEmpty()) {
        m_empty = true;
        m_bounds = {};
        ++m_revision;
        return;
    }

    m_empty = false;
    recomputeBounds();
    ++m_revision;
}

void Selection::selectFromAlpha(const RasterBuffer& source)
{
    m_mask.fill(kTransparent);
    if (source.isEmpty() || source.width() != m_mask.width()
        || source.height() != m_mask.height()) {
        m_empty = true;
        m_bounds = {};
        ++m_revision;
        return;
    }

    bool any = false;
    for (int y = 0; y < m_mask.height(); ++y) {
        for (int x = 0; x < m_mask.width(); ++x) {
            const std::uint8_t a = source.at(x, y).a;
            if (a == 0)
                continue;
            m_mask.set(x, y, Color{ 255, 255, 255, a });
            any = true;
        }
    }
    m_empty = !any;
    recomputeBounds();
    ++m_revision;
}

void Selection::selectEllipse(const Rect& rect)
{
    m_mask.fill(kTransparent);
    const Rect area = rect.intersected(Rect{ 0, 0, m_mask.width(), m_mask.height() });
    if (area.isEmpty()) {
        m_empty = true;
        m_bounds = {};
        ++m_revision;
        return;
    }

    const float cx = static_cast<float>(area.x) + static_cast<float>(area.width) * 0.5F;
    const float cy = static_cast<float>(area.y) + static_cast<float>(area.height) * 0.5F;
    const float rx = static_cast<float>(area.width) * 0.5F;
    const float ry = static_cast<float>(area.height) * 0.5F;

    const Color on{ 255, 255, 255, 255 };
    for (int y = area.y; y < area.y + area.height; ++y) {
        for (int x = area.x; x < area.x + area.width; ++x) {
            const float dx = (static_cast<float>(x) + 0.5F - cx) / std::max(rx, 0.5F);
            const float dy = (static_cast<float>(y) + 0.5F - cy) / std::max(ry, 0.5F);
            if (dx * dx + dy * dy <= 1.0F)
                m_mask.set(x, y, on);
        }
    }
    m_empty = false;
    recomputeBounds();
    ++m_revision;
}

void Selection::selectSimilarGlobal(const RasterBuffer& source, int x, int y, int tolerance)
{
    m_mask.fill(kTransparent);
    if (source.isEmpty() || x < 0 || y < 0 || x >= source.width() || y >= source.height()) {
        m_empty = true;
        m_bounds = {};
        ++m_revision;
        return;
    }

    const auto seed = source.at(x, y);
    const Color on{ 255, 255, 255, 255 };
    const auto close = [tolerance](Color a, Color b) {
        return std::abs(static_cast<int>(a.r) - static_cast<int>(b.r)) <= tolerance
            && std::abs(static_cast<int>(a.g) - static_cast<int>(b.g)) <= tolerance
            && std::abs(static_cast<int>(a.b) - static_cast<int>(b.b)) <= tolerance
            && std::abs(static_cast<int>(a.a) - static_cast<int>(b.a)) <= tolerance;
    };

    bool any = false;
    for (int py = 0; py < source.height(); ++py) {
        for (int px = 0; px < source.width(); ++px) {
            if (!close(source.at(px, py), seed))
                continue;
            m_mask.set(px, py, on);
            any = true;
        }
    }
    m_empty = !any;
    recomputeBounds();
    ++m_revision;
}

void Selection::selectContiguousOpaque(const RasterBuffer& source, int x, int y)
{
    m_mask.fill(kTransparent);
    if (source.isEmpty() || source.width() != m_mask.width()
        || source.height() != m_mask.height() || x < 0 || y < 0 || x >= source.width()
        || y >= source.height() || source.at(x, y).a == 0) {
        m_empty = true;
        m_bounds = {};
        ++m_revision;
        return;
    }

    // Region-grow on alpha only: "select this outline", ignoring colour.
    const Color on{ 255, 255, 255, 255 };
    floodFillRegion(source, x, y, 0, [&](int px, int py, Color c) {
        if (c.a > 0)
            m_mask.set(px, py, on);
    });
    // floodFillRegion matches on all channels; restrict to the opaque shell by
    // running it on a copy where alpha is the only signal.
    RasterBuffer alphaOnly(source.width(), source.height());
    for (int py = 0; py < source.height(); ++py)
        for (int px = 0; px < source.width(); ++px) {
            const std::uint8_t a = source.at(px, py).a;
            alphaOnly.set(px, py, Color{ 255, 255, 255, a == 0 ? 0 : 255 });
        }
    m_mask.fill(kTransparent);
    floodFillRegion(alphaOnly, x, y, 0,
                    [&](int px, int py, Color) { m_mask.set(px, py, on); });

    m_empty = false;
    recomputeBounds();
    ++m_revision;
}

void Selection::selectSmoothPolygon(const std::vector<Point>& points)
{
    if (points.size() < 3) {
        selectPolygon(points);
        return;
    }
    // Catmull-Rom through the control points, then reuse the polygon rasterizer.
    std::vector<Point> dense;
    const int steps = 12;
    const std::size_t n = points.size();
    for (std::size_t i = 0; i < n; ++i) {
        const Point& p0 = points[(i + n - 1) % n];
        const Point& p1 = points[i];
        const Point& p2 = points[(i + 1) % n];
        const Point& p3 = points[(i + 2) % n];
        for (int s = 0; s < steps; ++s) {
            const float t = static_cast<float>(s) / static_cast<float>(steps);
            const float t2 = t * t;
            const float t3 = t2 * t;
            const auto cr = [&](float a, float b, float c, float d) {
                return 0.5F * ((2 * b) + (-a + c) * t + (2 * a - 5 * b + 4 * c - d) * t2
                               + (-a + 3 * b - 3 * c + d) * t3);
            };
            dense.push_back(Point{ static_cast<int>(cr(static_cast<float>(p0.x),
                                                       static_cast<float>(p1.x),
                                                       static_cast<float>(p2.x),
                                                       static_cast<float>(p3.x))),
                                   static_cast<int>(cr(static_cast<float>(p0.y),
                                                       static_cast<float>(p1.y),
                                                       static_cast<float>(p2.y),
                                                       static_cast<float>(p3.y))) });
        }
    }
    selectPolygon(dense);
}

void Selection::selectMagnetic(const RasterBuffer& source,
                               const std::vector<Point>& points, int snapRadius)
{
    if (points.size() < 3) {
        selectPolygon(points);
        return;
    }

    // Snap each point to the strongest gradient within snapRadius: that is the
    // "sticks to edges" behaviour the magnetic tool is named for.
    const auto luma = [&](int x, int y) {
        if (x < 0 || y < 0 || x >= source.width() || y >= source.height())
            return 0.0F;
        const Color c = source.at(x, y);
        return 0.2126F * c.r + 0.7152F * c.g + 0.0722F * c.b;
    };

    std::vector<Point> snapped;
    snapped.reserve(points.size());
    for (const auto& p : points) {
        int bestX = p.x;
        int bestY = p.y;
        float best = -1.0F;
        for (int dy = -snapRadius; dy <= snapRadius; ++dy) {
            for (int dx = -snapRadius; dx <= snapRadius; ++dx) {
                const int sx = p.x + dx;
                const int sy = p.y + dy;
                const float gx = luma(sx + 1, sy) - luma(sx - 1, sy);
                const float gy = luma(sx, sy + 1) - luma(sx, sy - 1);
                const float mag = std::sqrt(gx * gx + gy * gy);
                if (mag > best) {
                    best = mag;
                    bestX = sx;
                    bestY = sy;
                }
            }
        }
        snapped.push_back(Point{ bestX, bestY });
    }
    selectPolygon(snapped);
}

void Selection::recomputeBounds(){
    int minX = m_mask.width(), minY = m_mask.height(), maxX = -1, maxY = -1;
    for (int y = 0; y < m_mask.height(); ++y) {
        for (int x = 0; x < m_mask.width(); ++x) {
            if (m_mask.at(x, y).a == 0)
                continue;
            minX = std::min(minX, x);
            maxX = std::max(maxX, x);
            minY = std::min(minY, y);
            maxY = std::max(maxY, y);
        }
    }
    if (maxX < 0) {
        m_empty = true;
        m_bounds = {};
        return;
    }
    m_bounds = Rect{ minX, minY, maxX - minX + 1, maxY - minY + 1 };
}

void clipToSelection(RasterBuffer& target, const Selection& selection,
                     const RasterBuffer* restoreFrom)
{
    if (selection.isEmpty() || target.isEmpty())
        return;
    if (selection.width() != target.width() || selection.height() != target.height())
        return; // mismatched sizes: treat as no constraint rather than guess
    clipToSelection(target, selection, restoreFrom, 0, 0);
}

void clipToSelection(RasterBuffer& target, const Selection& selection,
                     const RasterBuffer* restoreFrom, int offsetX, int offsetY)
{
    if (selection.isEmpty() || target.isEmpty())
        return;

    for (int y = 0; y < target.height(); ++y) {
        for (int x = 0; x < target.width(); ++x) {
            const int sx = x + offsetX;
            const int sy = y + offsetY;
            const std::uint8_t cov = selection.coverageAt(sx, sy);
            if (cov == 255)
                continue;

            const Color original = restoreFrom && sx < restoreFrom->width()
                                       && sy < restoreFrom->height()
                                   ? restoreFrom->at(sx, sy)
                                   : kTransparent;
            if (cov == 0) {
                target.set(x, y, original);
                continue;
            }
            // Soft edge: blend the edit with the original by mask coverage.
            const Color edit = target.at(x, y);
            const float t = static_cast<float>(cov) / 255.0F;
            const float alpha = (static_cast<float>(edit.a) * t
                                 + static_cast<float>(original.a) * (1.0F - t));
            target.set(x, y,
                       Color{ static_cast<std::uint8_t>(edit.r * t + original.r * (1 - t)),
                              static_cast<std::uint8_t>(edit.g * t + original.g * (1 - t)),
                              static_cast<std::uint8_t>(edit.b * t + original.b * (1 - t)),
                              static_cast<std::uint8_t>(alpha) });
        }
    }
}

} // namespace prism::canvas
