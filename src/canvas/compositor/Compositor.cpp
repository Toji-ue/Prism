#include "compositor/Compositor.h"

#include "layer/Layer.h"
#include "raster/Blend.h"

namespace prism::canvas {

using prism::core::Layer;
using prism::core::raster::blendPixel;
using prism::core::raster::Color;
using prism::core::raster::RasterBuffer;

Compositor::Compositor(prism::core::Document& doc)
    : m_doc(doc)
    , m_result(doc.canvasSize().width, doc.canvasSize().height)
{
}

void Compositor::canvasResized()
{
    m_result = RasterBuffer(m_doc.canvasSize().width, m_doc.canvasSize().height);
}

prism::core::geometry::Rect Compositor::composite(prism::core::geometry::Rect dirty)
{
    const auto& size = m_doc.canvasSize();
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
    for (const auto& layer : m_doc.layers()) {
        if (!layer->isVisible() || layer->pixels().isEmpty())
            continue;

        const auto& src = layer->pixels();
        for (int y = area.y; y < area.y + area.height; ++y) {
            for (int x = area.x; x < area.x + area.width; ++x) {
                Color s = src.at(x, y);
                if (layer->opacity() < 1.0F)
                    s.a = static_cast<std::uint8_t>(
                        static_cast<float>(s.a) * layer->opacity() + 0.5F);
                if (s.a == 0)
                    continue;
                m_result.set(x, y, blendPixel(m_result.at(x, y), s, layer->blendMode()));
            }
        }
    }
    return area;
}

} // namespace prism::canvas
