#pragma once

#include "document/Document.h"
#include "geometry/Types.h"
#include "raster/RasterBuffer.h"

namespace prism::canvas {

/// CPU compositor: flattens a Document's layer stack into a target buffer,
/// blending bottom-to-top. Only the requested dirty rect is recomputed —
/// callers pass the region that actually changed (see AGENTS.md: no
/// full-canvas recompute on a single-layer change).
///
/// This is the reference implementation; the GPU compositor must match its
/// output pixel-for-pixel (golden-image tests pin them together).
class Compositor {
public:
    explicit Compositor(prism::core::Document& doc);

    /// Recomputes \p dirty of the composite. Passing the full buffer rect
    /// recomputes everything. If \p overlay is non-null it is applied last.
    /// With \p overlayErases the overlay's alpha is SUBTRACTED from the
    /// composite instead of painted over it — that is what gives the eraser
    /// live feedback while the pointer is still down.
    /// Returns the rect actually composited (dirty intersected with canvas).
    prism::core::geometry::Rect composite(prism::core::geometry::Rect dirty,
                                          const prism::core::raster::RasterBuffer* overlay = nullptr,
                                          bool overlayErases = false);

    /// The flattened canvas. Owned here so it survives between frames.
    [[nodiscard]] const prism::core::raster::RasterBuffer& result() const noexcept
    {
        return m_result;
    }

    /// Reallocate caches after a canvas resize.
    void canvasResized();

private:
    prism::core::Document& m_doc;
    prism::core::raster::RasterBuffer m_result;
};

} // namespace prism::canvas
