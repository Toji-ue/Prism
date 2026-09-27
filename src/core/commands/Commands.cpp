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
    px.compositeOver(m_after, m_rectX, m_rectY);
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

// ---------------------------------------------------------- SetLayerProperty

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
    }
}

void SetLayerPropertyCommand::undo() { apply(m_oldValue); }

void SetLayerPropertyCommand::redo() { apply(m_newValue); }

} // namespace prism::core
