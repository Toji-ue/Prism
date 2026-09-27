#include "raster/RasterBuffer.h"

#include "layer/BlendMode.h"
#include "raster/Blend.h"

#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace prism::core::raster {

RasterBuffer::RasterBuffer(int width, int height, Color fill)
    : m_width(width)
    , m_height(height)
{
    if (width < 0 || height < 0)
        throw std::invalid_argument("RasterBuffer: negative size");
    m_pixels.resize(static_cast<std::size_t>(width) * height * kBytesPerPixel);
    this->fill(fill);
}

Color RasterBuffer::at(int x, int y) const
{
    if (x < 0 || y < 0 || x >= m_width || y >= m_height)
        throw std::out_of_range("RasterBuffer::at");
    const auto* p = m_pixels.data() + (static_cast<std::size_t>(y) * m_width + x) * kBytesPerPixel;
    return Color{ p[0], p[1], p[2], p[3] };
}

void RasterBuffer::set(int x, int y, Color c)
{
    if (x < 0 || y < 0 || x >= m_width || y >= m_height)
        return; // set() clips silently: brush stamps near edges rely on this
    auto* p = m_pixels.data() + (static_cast<std::size_t>(y) * m_width + x) * kBytesPerPixel;
    p[0] = c.r;
    p[1] = c.g;
    p[2] = c.b;
    p[3] = c.a;
}

void RasterBuffer::fill(Color c) noexcept
{
    if (m_pixels.empty())
        return;
    for (std::size_t i = 0; i < m_pixels.size(); i += kBytesPerPixel) {
        m_pixels[i] = c.r;
        m_pixels[i + 1] = c.g;
        m_pixels[i + 2] = c.b;
        m_pixels[i + 3] = c.a;
    }
}

RasterBuffer RasterBuffer::copyRect(int x, int y, int w, int h) const
{
    // Intersect with bounds
    const int x1 = std::max(x, 0);
    const int y1 = std::max(y, 0);
    const int x2 = std::min(x + w, m_width);
    const int y2 = std::min(y + h, m_height);
    if (x2 <= x1 || y2 <= y1)
        return {};

    RasterBuffer out(x2 - x1, y2 - y1);
    for (int row = y1; row < y2; ++row) {
        std::memcpy(out.data() + static_cast<std::size_t>(row - y1) * out.m_width * kBytesPerPixel,
                    m_pixels.data()
                        + (static_cast<std::size_t>(row) * m_width + x1) * kBytesPerPixel,
                    static_cast<std::size_t>(x2 - x1) * kBytesPerPixel);
    }
    return out;
}

void RasterBuffer::blit(const RasterBuffer& src, int dstX, int dstY)
{
    // Simple clipped row copy. Hot-path optimization (memcpy per row) can
    // come later with a benchmark; correctness first.
    for (int row = 0; row < src.height(); ++row) {
        const int y = dstY + row;
        if (y < 0 || y >= m_height)
            continue;
        for (int col = 0; col < src.width(); ++col) {
            const int x = dstX + col;
            if (x < 0 || x >= m_width)
                continue;
            std::memcpy(m_pixels.data() + (static_cast<std::size_t>(y) * m_width + x) * kBytesPerPixel,
                        src.data()
                            + (static_cast<std::size_t>(row) * src.width() + col) * kBytesPerPixel,
                        kBytesPerPixel);
        }
    }
}

void RasterBuffer::compositeOver(const RasterBuffer& src, int dstX, int dstY)
{
    for (int row = 0; row < src.height(); ++row) {
        const int y = dstY + row;
        if (y < 0 || y >= m_height)
            continue;
        for (int col = 0; col < src.width(); ++col) {
            const int x = dstX + col;
            if (x < 0 || x >= m_width)
                continue;
            set(x, y, blendPixel(at(x, y), src.at(col, row), BlendMode::Normal));
        }
    }
}

} // namespace prism::core::raster
