#include "document/Document.h"

#include "layer/Layer.h"

#include <algorithm>
#include <stdexcept>

namespace prism::core {

Document::Document(CanvasSize size, std::string colorProfile)
    : m_size(size)
    , m_colorProfile(std::move(colorProfile))
{
    if (!size.isValid())
        throw std::invalid_argument("Document: invalid canvas size");
}

void Document::setCanvasSize(CanvasSize size)
{
    if (!size.isValid())
        throw std::invalid_argument("Document::setCanvasSize: invalid canvas size");
    m_size = size;
}

void Document::resizeCanvas(CanvasSize size)
{
    if (!size.isValid())
        throw std::invalid_argument("Document::resizeCanvas: invalid canvas size");
    // Resize every raster layer to the new size, preserving top-left content
    // (extra space is transparent, overflow is cropped). This is the destructive
    // path: hidden margins outside the canvas are dropped, so the origin resets.
    for (auto& layer : m_layers) {
        if (layer->type() != Layer::Type::Raster)
            continue;
        const auto& old = layer->pixels();
        raster::RasterBuffer resized(size.width, size.height);
        // Resize the visible canvas, not the hidden buffer origin left by a
        // non-destructive crop.
        resized.blit(old.copyRect(m_origin.x, m_origin.y, m_size.width, m_size.height), 0, 0);
        layer->pixels() = std::move(resized);
    }
    m_size = size;
    m_origin = geometry::Point{ 0, 0 };
}

std::size_t Document::addLayer(std::unique_ptr<Layer> layer)
{
    m_layers.push_back(std::move(layer));
    return m_layers.size() - 1;
}

void Document::insertLayer(std::size_t index, std::unique_ptr<Layer> layer)
{
    if (index >= m_layers.size()) {
        m_layers.push_back(std::move(layer));
        return;
    }
    m_layers.insert(m_layers.begin() + static_cast<std::ptrdiff_t>(index), std::move(layer));
}

std::size_t Document::addRasterLayer(std::string name)
{
    auto layer = std::make_unique<Layer>(Layer::Type::Raster, std::move(name));
    // Allocated canvas-at-origin, so a layer added after a crop shares the
    // coordinate space of the layers that still hold cropped-away pixels.
    layer->pixels() = raster::RasterBuffer(m_size.width + m_origin.x, m_size.height + m_origin.y);
    return addLayer(std::move(layer));
}

bool Document::moveLayer(std::size_t fromIndex, std::size_t toIndex)
{
    if (fromIndex >= m_layers.size() || toIndex >= m_layers.size())
        return false;
    if (fromIndex == toIndex)
        return true;
    // rotate() moves the element at fromIndex to toIndex in one pass:
    // forward when moving up the stack, backward otherwise.
    if (fromIndex < toIndex)
        std::rotate(m_layers.begin() + static_cast<std::ptrdiff_t>(fromIndex),
                    m_layers.begin() + static_cast<std::ptrdiff_t>(fromIndex) + 1,
                    m_layers.begin() + static_cast<std::ptrdiff_t>(toIndex) + 1);
    else
        std::rotate(m_layers.begin() + static_cast<std::ptrdiff_t>(toIndex),
                    m_layers.begin() + static_cast<std::ptrdiff_t>(fromIndex),
                    m_layers.begin() + static_cast<std::ptrdiff_t>(fromIndex) + 1);
    return true;
}

std::unique_ptr<Layer> Document::takeLayer(std::size_t index)
{
    if (index >= m_layers.size())
        return nullptr;
    auto it = m_layers.begin() + static_cast<std::ptrdiff_t>(index);
    auto layer = std::move(*it);
    m_layers.erase(it);
    return layer;
}

} // namespace prism::core
