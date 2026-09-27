#pragma once

#include <cstdint>
#include <vector>

namespace prism::core::raster {

/// Straight (non-premultiplied) RGBA, 8 bits/channel, tightly packed rows.
struct Color {
    std::uint8_t r = 0;
    std::uint8_t g = 0;
    std::uint8_t b = 0;
    std::uint8_t a = 0;

    friend bool operator==(const Color& x, const Color& y) = default;
};

constexpr Color kOpaqueBlack{ 0, 0, 0, 255 };
constexpr Color kTransparent{ 0, 0, 0, 0 };
constexpr Color kOpaqueWhite{ 255, 255, 255, 255 };

/// A writable RGBA8 pixel buffer. Rows are contiguous; stride == width * 4.
///
/// Straight alpha everywhere — conversion to premultiplied happens only at
/// the blend boundary (see Blend.h) so stored data round-trips losslessly.
class RasterBuffer {
public:
    RasterBuffer() = default;
    RasterBuffer(int width, int height, Color fill = kTransparent);

    [[nodiscard]] int width() const noexcept { return m_width; }
    [[nodiscard]] int height() const noexcept { return m_height; }
    [[nodiscard]] bool isEmpty() const noexcept { return m_width <= 0 || m_height <= 0; }

    /// Raw access for compositor/brush hot paths. Bounds are the caller's
    /// responsibility — use at() anywhere correctness matters more than speed.
    [[nodiscard]] const std::uint8_t* data() const noexcept { return m_pixels.data(); }
    [[nodiscard]] std::uint8_t* data() noexcept { return m_pixels.data(); }
    static constexpr int kBytesPerPixel = 4;

    [[nodiscard]] Color at(int x, int y) const;
    void set(int x, int y, Color c);

    void fill(Color c) noexcept;

    /// Deep copy of a sub-rect. Out-of-bounds rect is intersected first;
    /// empty result if no overlap.
    [[nodiscard]] RasterBuffer copyRect(int x, int y, int w, int h) const;
    /// Blits \p src onto this buffer at (dstX, dstY), clipped. Replaces
    /// pixels (memcpy semantics).
    void blit(const RasterBuffer& src, int dstX, int dstY);
    /// Source-over composites \p src onto this buffer at (dstX, dstY),
    /// clipped, honoring per-pixel src alpha. Used by stroke redo.
    void compositeOver(const RasterBuffer& src, int dstX, int dstY);

    friend bool operator==(const RasterBuffer& x, const RasterBuffer& y) = default;

private:
    int m_width = 0;
    int m_height = 0;
    std::vector<std::uint8_t> m_pixels;
};

} // namespace prism::core::raster
