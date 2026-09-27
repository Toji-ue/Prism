#pragma once

#include "document/Document.h"
#include "layer/Layer.h"

#include <memory>
#include <string>
#include <vector>

namespace prism::core {

/// Size of the canvas in whole pixels, plus print resolution.
/// Geometry only — color profiles live on Document.
struct CanvasSize {
    int width = 0;
    int height = 0;
    int dpi = 72;

    [[nodiscard]] constexpr bool isValid() const noexcept
    {
        return width > 0 && height > 0;
    }
};

/// The top-level project: canvas size, color profile, layer tree.
/// Owns layers in bottom-to-top paint order.
class Document {
public:
    explicit Document(CanvasSize size, std::string colorProfile = "sRGB");

    [[nodiscard]] const CanvasSize& canvasSize() const noexcept { return m_size; }
    [[nodiscard]] const std::string& colorProfile() const noexcept { return m_colorProfile; }

    void resizeCanvas(CanvasSize size);

    /// Layers paint bottom-to-top; index 0 is the bottom of the stack.
    [[nodiscard]] const std::vector<std::unique_ptr<Layer>>& layers() const noexcept
    {
        return m_layers;
    }

    /// Adds a layer on top of the stack and returns its index.
    std::size_t addLayer(std::unique_ptr<Layer> layer);
    std::size_t addRasterLayer(std::string name);

    /// Moves a layer within the stack; returns false if the index is out of range.
    bool moveLayer(std::size_t fromIndex, std::size_t toIndex);
    /// Removes and returns a layer; returns nullptr if the index is out of range.
    std::unique_ptr<Layer> takeLayer(std::size_t index);

private:
    CanvasSize m_size;
    std::string m_colorProfile;
    std::vector<std::unique_ptr<Layer>> m_layers;
};

} // namespace prism::core
