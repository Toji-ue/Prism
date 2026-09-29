#pragma once

#include "geometry/Types.h"
#include "raster/RasterBuffer.h"

#include <vector>

namespace prism::canvas {

using prism::core::geometry::Rect;
using prism::core::raster::RasterBuffer;

/// A selection stored as an 8-bit coverage mask (in a RasterBuffer's alpha
/// channel), so it can be any shape: rectangle, lasso polygon, or the result
/// of a magic-wand flood fill. An empty mask means "no selection".
class Selection {
public:
    Selection() = default;
    explicit Selection(int width, int height);

    /// Reallocate to a new canvas size and clear.
    void reset(int width, int height);

    [[nodiscard]] bool isEmpty() const noexcept { return m_empty; }
    [[nodiscard]] int width() const noexcept { return m_mask.width(); }
    [[nodiscard]] int height() const noexcept { return m_mask.height(); }
    /// Bounding box of the selected (non-zero) area, in canvas pixels.
    [[nodiscard]] Rect bounds() const noexcept { return m_bounds; }

    /// Coverage 0..255 at a canvas pixel.
    [[nodiscard]] std::uint8_t coverageAt(int x, int y) const;

    void clear();
    void selectRect(const Rect& rect);
    void selectAll();
    void invert();

    /// Rasterizes a freehand polygon (lasso). Fewer than 3 points is empty.
    void selectPolygon(const std::vector<prism::core::geometry::Point>& points);

    /// Magic wand: the connected region of \p source matching the seed within
    /// \p tolerance becomes the selection.
    void selectSimilar(const RasterBuffer& source, int x, int y, int tolerance);

    /// Select Opaque: every pixel of \p source with non-zero alpha, with the
    /// alpha itself as coverage (Krita's "Select Opaque").
    void selectFromAlpha(const RasterBuffer& source);

    /// Elliptical marquee inscribed in \p rect (Krita's Elliptical Selection).
    void selectEllipse(const Rect& rect);

    /// Similar Colour Selection: every pixel on the canvas matching the seed
    /// within \p tolerance, connected or not (unlike the magic wand).
    void selectSimilarGlobal(const RasterBuffer& source, int x, int y, int tolerance);

    /// Outline Selection: the connected run of non-transparent pixels under
    /// (x, y) - selects the outline/object itself rather than matching colour.
    void selectContiguousOpaque(const RasterBuffer& source, int x, int y);

    /// Bezier Selection: a smoothed closed curve through \p points
    /// (Catmull-Rom densification of a Polygon Selection).
    void selectSmoothPolygon(const std::vector<prism::core::geometry::Point>& points);

    /// Magnetic Selection: like the freehand lasso, but each point is pulled
    /// onto the strongest nearby edge in \p source (within \p snapRadius).
    void selectMagnetic(const RasterBuffer& source,
                        const std::vector<prism::core::geometry::Point>& points,
                        int snapRadius = 6);

    /// Read-only mask access for rendering the marching-ants overlay.
    [[nodiscard]] const RasterBuffer& mask() const noexcept { return m_mask; }
    /// Bumped on every change, so QML can re-fetch the overlay image.
    [[nodiscard]] int revision() const noexcept { return m_revision; }

private:
    void recomputeBounds();

    RasterBuffer m_mask;
    Rect m_bounds;
    bool m_empty = true;
    int m_revision = 0;
};

/// Applies a selection to an edit.
///
/// With \p restoreFrom == nullptr, pixels outside the selection are cleared
/// (use on a scratch buffer before committing a stroke). With \p restoreFrom
/// given (the pre-edit copy of the same buffer), pixels outside the selection
/// come back from it — that is how fill and filters stay inside a selection.
/// Soft mask edges blend the edit with the original.
void clipToSelection(RasterBuffer& target, const Selection& selection,
                     const RasterBuffer* restoreFrom = nullptr);

/// Same, for a PATCH of the canvas (\p target is the rectangle at \p offsetX,
/// \p offsetY of the selection's coordinate space). Committing a stroke only
/// needs the stroke's own rect clipped, not a full-canvas pass.
void clipToSelection(RasterBuffer& target, const Selection& selection,
                     const RasterBuffer* restoreFrom, int offsetX, int offsetY);

} // namespace prism::canvas
