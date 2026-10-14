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
    /// composite instead of painted over it.
    /// Returns the rect actually composited (dirty intersected with canvas).
    prism::core::geometry::Rect
    composite(prism::core::geometry::Rect dirty,
              const prism::core::raster::RasterBuffer* overlay = nullptr,
              bool overlayErases = false, long long overlayLayerIndex = -1);

    /// Substitutes a layer's pixels for the duration of an edit — the eraser
    /// preview uses this so ONLY the layer being erased changes on screen.
    /// Subtracting from the flattened composite instead would bite through
    /// every other layer and then "heal" on release.
    void setLayerOverride(std::size_t layerIndex, const prism::core::raster::RasterBuffer* pixels);
    void clearLayerOverride();

    /// The flattened canvas. Owned here so it survives between frames.
    [[nodiscard]] const prism::core::raster::RasterBuffer& result() const noexcept {
        return m_result;
    }

    /// Reallocate caches after a canvas resize.
    void canvasResized();
    [[nodiscard]] int canvasDpi() const noexcept { return m_doc.canvasSize().dpi; }

private:
    prism::core::Document& m_doc;
    prism::core::raster::RasterBuffer m_result;
    /// Layer whose pixels are substituted while an edit is previewed.
    long long m_overrideIndex = -1;
    const prism::core::raster::RasterBuffer* m_overridePixels = nullptr;
};

} // namespace prism::canvas
