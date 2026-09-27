#include "selection/Selection.h"

#include "fill/FloodFill.h"

#include <algorithm>
#include <cmath>
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

    for (int y = 0; y < target.height(); ++y) {
        for (int x = 0; x < target.width(); ++x) {
            const std::uint8_t cov = selection.coverageAt(x, y);
            if (cov == 255)
                continue;

            const Color original = restoreFrom && x < restoreFrom->width()
                                       && y < restoreFrom->height()
                                   ? restoreFrom->at(x, y)
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
