#include "compositor/Compositor.h"

#include "layer/Layer.h"
#include "raster/Blend.h"

#include <algorithm>

namespace prism::canvas {

using prism::core::Layer;
using prism::core::raster::blendPixel;
using prism::core::raster::Color;
using prism::core::raster::RasterBuffer;

namespace {
Color renderGroupPixel(const prism::core::Document& doc, std::uint64_t parentId, int x, int y,
                       long long overrideIndex, const RasterBuffer* overridePixels,
                       const RasterBuffer* overlay, bool overlayErases,
                       long long overlayLayerIndex, int depth) {
    if (depth > 64)
        return prism::core::raster::kTransparent;
    Color result = prism::core::raster::kTransparent;
    std::uint8_t baseAlpha = 0;
    bool hasBase = false;
    const auto origin = doc.canvasOrigin();
    const auto& layers = doc.layers();
    for (std::size_t i = 0; i < layers.size(); ++i) {
        const auto& layer = *layers[i];
        if (layer.parentId() != parentId || !layer.isVisible())
            continue;
        Color source = prism::core::raster::kTransparent;
        if (layer.type() == Layer::Type::Group) {
            source = renderGroupPixel(doc, layer.id(), x, y, overrideIndex, overridePixels,
                                      overlay, overlayErases, overlayLayerIndex, depth + 1);
        } else if (layer.type() == Layer::Type::Raster) {
            const bool overridden = overridePixels && overrideIndex == static_cast<long long>(i);
            const auto& pixels = overridden ? *overridePixels : layer.pixels();
            const int sx = x + (overridden ? 0 : origin.x);
            const int sy = y + (overridden ? 0 : origin.y);
            if (sx >= 0 && sy >= 0 && sx < pixels.width() && sy < pixels.height())
                source = pixels.at(sx, sy);
            if (overlay && overlayLayerIndex == static_cast<long long>(i)) {
                const Color mark = overlay->at(x, y);
                if (overlayErases) {
                    source.a = static_cast<std::uint8_t>(std::max(0, int(source.a) - int(mark.a)));
                } else if (mark.a) {
                    const auto originalAlpha = source.a;
                    source = originalAlpha ? blendPixel(source, mark, prism::core::BlendMode::Normal)
                                           : mark;
                    if (layer.isAlphaLocked())
                        source.a = originalAlpha;
                }
            }
        }
        source.a = static_cast<std::uint8_t>(source.a * layer.opacity() + 0.5F);
        if (layer.isClipped()) {
            source.a = hasBase ? static_cast<std::uint8_t>(source.a * baseAlpha / 255) : 0;
        } else {
            baseAlpha = source.a;
            hasBase = true;
        }
        if (source.a)
            result = result.a ? blendPixel(result, source, layer.blendMode()) : source;
    }
    return result;
}
} // namespace

Compositor::Compositor(prism::core::Document& doc)
    : m_doc(doc)
    , m_result(doc.canvasSize().width, doc.canvasSize().height)
{
}

void Compositor::canvasResized()
{
    m_result = RasterBuffer(m_doc.canvasSize().width, m_doc.canvasSize().height);
}

void Compositor::setLayerOverride(std::size_t layerIndex, const RasterBuffer* pixels)
{
    m_overrideIndex = static_cast<long long>(layerIndex);
    m_overridePixels = pixels;
}

void Compositor::clearLayerOverride()
{
    m_overrideIndex = -1;
    m_overridePixels = nullptr;
}

prism::core::geometry::Rect Compositor::composite(prism::core::geometry::Rect dirty,
                                                  const RasterBuffer* overlay,
                                                  bool overlayErases,
                                                  long long overlayLayerIndex)
{
    const auto& size = m_doc.canvasSize();
    // Undoing a crop resizes the document without anyone calling
    // canvasResized(): the result buffer then stayed smaller than the canvas and
    // the next composite wrote past its end. Reallocating here keeps the class
    // self-consistent and makes canvasResized() a plain hint.
    if (m_result.width() != size.width || m_result.height() != size.height)
        canvasResized();
    const prism::core::geometry::Rect canvasRect{ 0, 0, size.width, size.height };
    const auto area = dirty.intersected(canvasRect);
    if (area.isEmpty())
        return area;

    // Reset the dirty region so repeated composites of the same rect are
    // idempotent (the flattened result there is rebuilt from all layers).
    for (int y = area.y; y < area.y + area.height; ++y)
        for (int x = area.x; x < area.x + area.width; ++x)
            m_result.set(x, y, prism::core::raster::kTransparent);

    // Layers paint bottom-to-top. Layer opacity multiplies the source alpha
    // before blending; invisible layers are skipped entirely.
    //
    // The app works in CANVAS coordinates; a layer buffer lives in buffer space,
    // which a crop shifts by the canvas origin. Reads are bounds-checked, so a
    // layer smaller than the canvas cannot throw either.
    const bool hasGroupsOrClipping = std::any_of(m_doc.layers().begin(), m_doc.layers().end(),
        [](const auto& layer) { return layer->type() == Layer::Type::Group ||
                                        layer->parentId() != 0 || layer->isClipped(); });
    if (hasGroupsOrClipping) {
        for (int y = area.y; y < area.y + area.height; ++y)
            for (int x = area.x; x < area.x + area.width; ++x)
                m_result.set(x, y, renderGroupPixel(m_doc, 0, x, y, m_overrideIndex,
                                                   m_overridePixels, overlay, overlayErases,
                                                   overlayLayerIndex, 0));
    } else {
    const auto origin = m_doc.canvasOrigin();
    std::size_t layerIndex = 0;
    for (const auto& layer : m_doc.layers()) {
        const bool overridden = m_overridePixels
                             && m_overrideIndex == static_cast<long long>(layerIndex);
        const auto& pixels = overridden ? *m_overridePixels : layer->pixels();
        ++layerIndex;
        if (!layer->isVisible() || pixels.isEmpty())
            continue;

        // The eraser/blend override is a canvas-sized scratch buffer, so it is
        // read WITHOUT the origin.
        const int offsetX = overridden ? 0 : origin.x;
        const int offsetY = overridden ? 0 : origin.y;
        const auto& src = pixels;
        for (int y = area.y; y < area.y + area.height; ++y) {
            const int sy = y + offsetY;
            if (sy < 0 || sy >= src.height())
                continue;
            for (int x = area.x; x < area.x + area.width; ++x) {
                const int sx = x + offsetX;
                if (sx < 0 || sx >= src.width())
                    continue;
                Color s = src.at(sx, sy);
                if (overlay && overlayLayerIndex == static_cast<long long>(layerIndex - 1)) {
                    const Color mark = overlay->at(x, y);
                    if (overlayErases) {
                        s.a = static_cast<std::uint8_t>(std::max(0, int(s.a) - int(mark.a)));
                    } else if (mark.a) {
                        const auto originalAlpha = s.a;
                        s = originalAlpha ? blendPixel(s, mark, prism::core::BlendMode::Normal)
                                          : mark;
                        if (layer->isAlphaLocked())
                            s.a = originalAlpha;
                    }
                }
                if (layer->opacity() < 1.0F)
                    s.a = static_cast<std::uint8_t>(
                        static_cast<float>(s.a) * layer->opacity() + 0.5F);
                if (s.a == 0)
                    continue;
                const Color dst = m_result.at(x, y);
                m_result.set(x, y, dst.a ? blendPixel(dst, s, layer->blendMode()) : s);
            }
        }
    }
    }

    // Live-preview overlay paints last. For the eraser it removes coverage
    // instead, so the stroke disappears under the pointer as it is dragged.
    if (overlay && !overlay->isEmpty() && overlayLayerIndex < 0) {
        for (int y = area.y; y < area.y + area.height; ++y) {
            for (int x = area.x; x < area.x + area.width; ++x) {
                const Color s = overlay->at(x, y);
                if (s.a == 0)
                    continue;
                if (!overlayErases) {
                    m_result.set(x, y,
                                 blendPixel(m_result.at(x, y), s, prism::core::BlendMode::Normal));
                    continue;
                }
                const Color dst = m_result.at(x, y);
                const int remaining = static_cast<int>(dst.a) - static_cast<int>(s.a);
                m_result.set(x, y,
                             remaining <= 0 ? prism::core::raster::kTransparent
                                            : Color{ dst.r, dst.g, dst.b,
                                                     static_cast<std::uint8_t>(remaining) });
            }
        }
    }
    return area;
}

} // namespace prism::canvas
