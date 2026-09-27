#pragma once

#include "document/Document.h"
#include "history/Command.h"
#include "raster/RasterBuffer.h"

#include <memory>
#include <string>

namespace prism::core {

/// Adds a raster layer on redo, removes it on undo.
class AddLayerCommand final : public Command {
public:
    AddLayerCommand(Document& doc, std::string layerName);

    const char* description() const override { return "Add layer"; }

    void undo() override;
    void redo() override;

private:
    Document& m_doc;
    std::string m_layerName;
    std::unique_ptr<Layer> m_layer; // carried between undo/redo
    std::size_t m_index = 0;
};

/// Paints a stroke into a raster layer on redo; restores the previous
/// pixels on undo. Snapshots the stroke's bounding box only — full-buffer
/// copies would burn ~4 MB of history per stroke on a 1000x1000 canvas.
class StrokeCommand final : public Command {
public:
    StrokeCommand(Document& doc, std::size_t layerIndex,
                  raster::RasterBuffer paintedPixels, int rectX, int rectY);

    const char* description() const override { return "Brush stroke"; }

    void undo() override;
    void redo() override;

private:
    Layer& layer() const;

    Document& m_doc;
    std::size_t m_layerIndex;
    int m_rectX;
    int m_rectY;
    raster::RasterBuffer m_before; // what undo restores
    raster::RasterBuffer m_after; // what redo stamps in
};

} // namespace prism::core
