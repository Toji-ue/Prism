#include "commands/Commands.h"

#include "layer/Layer.h"

#include <stdexcept>

namespace prism::core {

// ------------------------------------------------------------------ AddLayer

AddLayerCommand::AddLayerCommand(Document& doc, std::string layerName)
    : m_doc(doc)
    , m_layerName(std::move(layerName))
{
}

void AddLayerCommand::undo()
{
    // Remove exactly the layer we added, keeping its pixels for the redo.
    auto layer = m_doc.takeLayer(m_index);
    if (layer)
        m_layer = std::move(layer);
}

void AddLayerCommand::redo()
{
    if (!m_layer) {
        m_index = m_doc.addRasterLayer(m_layerName);
    } else {
        m_index = m_doc.addLayer(std::move(m_layer));
    }
}

// -------------------------------------------------------------------- Stroke

StrokeCommand::StrokeCommand(Document& doc, std::size_t layerIndex,
                             raster::RasterBuffer paintedPixels, int rectX, int rectY)
    : m_doc(doc)
    , m_layerIndex(layerIndex)
    , m_rectX(rectX)
    , m_rectY(rectY)
    , m_after(std::move(paintedPixels))
{
    // Snapshot the pre-stroke pixels of the affected rect here — the
    // controller calls this constructor BEFORE painting into the layer.
    auto layerIt = m_doc.layers().at(m_layerIndex).get();
    m_before = layerIt->pixels().copyRect(rectX, rectY, m_after.width(), m_after.height());
}

Layer& StrokeCommand::layer() const
{
    return *m_doc.layers().at(m_layerIndex);
}

void StrokeCommand::undo()
{
    // Restore the pre-stroke pixels of the affected rect verbatim.
    layer().pixels().blit(m_before, m_rectX, m_rectY);
}

void StrokeCommand::redo()
{
    // Reset the rect, then re-composite the stroke on top: blitting the
    // stroke directly would not erase older pixels where its alpha is 0.
    auto& px = layer().pixels();
    px.blit(m_before, m_rectX, m_rectY);
    px.compositeOver(m_after, m_rectX, m_rectY);
}

} // namespace prism::core
