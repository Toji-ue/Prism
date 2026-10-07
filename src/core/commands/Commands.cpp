#include "commands/Commands.h"

#include "layer/Layer.h"
#include "raster/Blend.h"

#include <stdexcept>

namespace prism::core {

// ------------------------------------------------------------------ AddLayer

AddLayerCommand::AddLayerCommand(Document& doc, std::string layerName)
    : m_doc(doc)
    , m_layerName(std::move(layerName))
{
}

AddLayerCommand::AddLayerCommand(Document& doc, std::string layerName,
                                 std::unique_ptr<Layer> layer)
    : m_doc(doc)
    , m_layerName(std::move(layerName))
    , m_layer(std::move(layer))
{
}

void AddLayerCommand::undo()
{
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
    m_before = layer().pixels().copyRect(rectX, rectY, m_after.width(), m_after.height());
}

StrokeCommand::StrokeCommand(Document& doc, std::size_t layerIndex, raster::RasterBuffer before,
                             raster::RasterBuffer after, int rectX, int rectY, std::string label)
    : m_doc(doc)
    , m_layerIndex(layerIndex)
    , m_rectX(rectX)
    , m_rectY(rectY)
    , m_label(std::move(label))
    , m_replace(true)
    , m_before(std::move(before))
    , m_after(std::move(after))
{
}

Layer& StrokeCommand::layer() const
{
    return *m_doc.layers().at(m_layerIndex);
}

void StrokeCommand::undo()
{
    layer().pixels().blit(m_before, m_rectX, m_rectY);
}

void StrokeCommand::redo()
{
    auto& px = layer().pixels();
    px.blit(m_before, m_rectX, m_rectY);
    if (m_replace) {
        // Replacement edits (erase, filters, shapes, fill) must overwrite the
        // patch. Compositing it source-over could never REMOVE alpha, which is
        // exactly why the eraser appeared to do nothing.
        px.blit(m_after, m_rectX, m_rectY);
    } else {
        // Paint strokes blend over the restored pixels so soft edges and
        // partial opacity behave like a real brush.
        px.compositeOver(m_after, m_rectX, m_rectY);
    }
}

// ---------------------------------------------------------------- RemoveLayer

RemoveLayerCommand::RemoveLayerCommand(Document& doc, std::size_t index)
    : m_doc(doc)
    , m_index(index)
{
}

void RemoveLayerCommand::undo()
{
    if (m_layer)
        m_doc.insertLayer(m_index, std::move(m_layer));
}

void RemoveLayerCommand::redo()
{
    m_layer = m_doc.takeLayer(m_index);
}

// ----------------------------------------------------------------- MoveLayer

MoveLayerCommand::MoveLayerCommand(Document& doc, std::size_t fromIndex, std::size_t toIndex)
    : m_doc(doc)
    , m_from(fromIndex)
    , m_to(toIndex)
{
}

void MoveLayerCommand::undo()
{
    m_doc.moveLayer(m_to, m_from);
}

void MoveLayerCommand::redo()
{
    m_doc.moveLayer(m_from, m_to);
}

// ------------------------------------------------------------- DuplicateLayer

DuplicateLayerCommand::DuplicateLayerCommand(Document& doc, std::size_t index)
    : m_doc(doc)
    , m_sourceIndex(index)
    , m_copy(doc.layers().at(index)->clone())
{
}

void DuplicateLayerCommand::undo()
{
    m_doc.takeLayer(m_newIndex);
}

void DuplicateLayerCommand::redo()
{
    if (m_copy) {
        m_newIndex = m_doc.addLayer(m_copy->clone());
    } else {
        // First redo was undone: the copy was consumed, re-clone from source.
        m_newIndex = m_doc.addLayer(m_doc.layers().at(m_sourceIndex)->clone());
    }
}

// -------------------------------------------------------------- ClearLayer

ClearLayerCommand::ClearLayerCommand(Document& doc, std::size_t index)
    : m_doc(doc)
    , m_index(index)
    , m_before(doc.layers().at(index)->pixels())
    , m_after(doc.layers().at(index)->pixels().width(), doc.layers().at(index)->pixels().height())
{
}

void ClearLayerCommand::undo()
{
    m_doc.layers().at(m_index)->pixels() = m_before;
}

void ClearLayerCommand::redo()
{
    m_doc.layers().at(m_index)->pixels() = m_after;
}

// ---------------------------------------------------------------- Crop

CropCommand::CropCommand(Document& doc, int x, int y, int width, int height)
    : m_doc(doc)
    , m_x(x)
    , m_y(y)
    , m_width(width)
    , m_height(height)
    , m_beforeSize(doc.canvasSize())
    , m_beforeOrigin(doc.canvasOrigin())
{
}

void CropCommand::redo()
{
    // Only the canvas window moves; the layers keep every pixel.
    m_doc.setCanvasOrigin(geometry::Point{ m_beforeOrigin.x + m_x, m_beforeOrigin.y + m_y });
    m_doc.setCanvasSize(CanvasSize{ m_width, m_height, m_beforeSize.dpi });
}

void CropCommand::undo()
{
    m_doc.setCanvasSize(m_beforeSize);
    m_doc.setCanvasOrigin(m_beforeOrigin);
}

// ------------------------------------------------------- MoveLayerContent

MoveLayerContentCommand::MoveLayerContentCommand(Document& doc, std::size_t index, int dx,
                                                 int dy)
    : m_doc(doc)
    , m_index(index)
    , m_dx(dx)
    , m_dy(dy)
    , m_before(doc.layers().at(index)->pixels())
{
}

MoveLayerContentCommand::MoveLayerContentCommand(Document& doc, std::size_t index, int dx,
                                                 int dy, raster::RasterBuffer before)
    : m_doc(doc)
    , m_index(index)
    , m_dx(dx)
    , m_dy(dy)
    , m_before(std::move(before))
{
}

void MoveLayerContentCommand::undo()
{
    m_doc.layers().at(m_index)->pixels() = m_before;
}

void MoveLayerContentCommand::redo()
{
    auto& pixels = m_doc.layers().at(m_index)->pixels();
    raster::RasterBuffer moved(pixels.width(), pixels.height());
    moved.blit(m_before, m_dx, m_dy);
    pixels = std::move(moved);
}

// ---------------------------------------------------------- MergeLayerDown

MergeLayerDownCommand::MergeLayerDownCommand(Document& doc, std::size_t upperIndex)
    : m_doc(doc)
    , m_upperIndex(upperIndex)
{
    auto& upper = *doc.layers().at(upperIndex);
    auto& lower = *doc.layers().at(upperIndex - 1);
    m_lowerBefore = lower.pixels();
    m_merged = lower.pixels();

    // Composite the upper layer onto the lower using its own blend mode and
    // opacity — the same math the compositor uses, so the result matches what
    // the artist already saw on screen.
    const std::uint8_t scaledAlphaOf = static_cast<std::uint8_t>(upper.opacity() * 255.0F);
    for (int y = 0; y < m_merged.height(); ++y) {
        for (int x = 0; x < m_merged.width(); ++x) {
            raster::Color src = upper.pixels().at(x, y);
            if (src.a == 0)
                continue;
            if (upper.opacity() < 1.0F)
                src.a = static_cast<std::uint8_t>(
                    static_cast<float>(src.a) * upper.opacity() + 0.5F);
            (void)scaledAlphaOf;
            m_merged.set(x, y,
                         raster::blendPixel(m_merged.at(x, y), src, upper.blendMode()));
        }
    }
}

void MergeLayerDownCommand::undo()
{
    // Put the upper layer back above the lower one, and restore the lower.
    if (m_upper)
        m_doc.layers().at(m_upperIndex - 1)->pixels() = m_lowerBefore;
    if (m_upper)
        m_doc.insertLayer(m_upperIndex, std::move(m_upper));
}

void MergeLayerDownCommand::redo()
{
    m_upper = m_doc.takeLayer(m_upperIndex);
    if (!m_upper)
        return;
    m_doc.layers().at(m_upperIndex - 1)->pixels() = m_merged;
}

// ----------------------------------------------------------- SetLayerProperty

SetLayerPropertyCommand::SetLayerPropertyCommand(Document& doc, std::size_t index,
                                                 Property prop, std::string value,
                                                 std::string prevValue)
    : m_doc(doc)
    , m_index(index)
    , m_prop(prop)
    , m_newValue{ Value::Kind::Str, std::move(value), 0.0F, false, BlendMode::Normal }
    , m_oldValue{ Value::Kind::Str, std::move(prevValue), 0.0F, false, BlendMode::Normal }
{
}

SetLayerPropertyCommand::SetLayerPropertyCommand(Document& doc, std::size_t index,
                                                 Property prop, float value, float prevValue)
    : m_doc(doc)
    , m_index(index)
    , m_prop(prop)
    , m_newValue{ Value::Kind::Float, {}, value, false, BlendMode::Normal }
    , m_oldValue{ Value::Kind::Float, {}, prevValue, false, BlendMode::Normal }
{
}

SetLayerPropertyCommand::SetLayerPropertyCommand(Document& doc, std::size_t index,
                                                 Property prop, bool value, bool prevValue)
    : m_doc(doc)
    , m_index(index)
    , m_prop(prop)
    , m_newValue{ Value::Kind::Bool, {}, 0.0F, value, BlendMode::Normal }
    , m_oldValue{ Value::Kind::Bool, {}, 0.0F, prevValue, BlendMode::Normal }
{
}

SetLayerPropertyCommand::SetLayerPropertyCommand(Document& doc, std::size_t index,
                                                 Property prop, BlendMode value,
                                                 BlendMode prevValue)
    : m_doc(doc)
    , m_index(index)
    , m_prop(prop)
    , m_newValue{ Value::Kind::Blend, {}, 0.0F, false, value }
    , m_oldValue{ Value::Kind::Blend, {}, 0.0F, false, prevValue }
{
}

void SetLayerPropertyCommand::apply(const Value& v)
{
    auto& layer = *m_doc.layers().at(m_index);
    switch (m_prop) {
    case Property::Name: layer.setName(v.s); break;
    case Property::Visible: layer.setVisible(v.b); break;
    case Property::Opacity: layer.setOpacity(v.f); break;
    case Property::BlendMode: layer.setBlendMode(v.blend); break;
    case Property::Locked: layer.setLocked(v.b); break;
    }
}

void SetLayerPropertyCommand::undo() { apply(m_oldValue); }

void SetLayerPropertyCommand::redo() { apply(m_newValue); }

} // namespace prism::core
