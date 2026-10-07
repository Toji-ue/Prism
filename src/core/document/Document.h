#pragma once

#include "document/Document.h"
#include "geometry/Types.h"
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

    /// Top-left corner of the canvas inside layer-buffer space.
    ///
    /// A crop only moves this window and shrinks the canvas; every layer keeps
    /// its pixels, so moving a layer brings the cropped-away content back in ALL
    /// four directions (Photoshop's crop with "delete cropped pixels" off).
    [[nodiscard]] geometry::Point canvasOrigin() const noexcept { return m_origin; }
    void setCanvasOrigin(geometry::Point origin) noexcept { m_origin = origin; }

    /// Changes the canvas size without touching layer pixels — what a crop
    /// needs. Layer buffers may stay larger than the canvas.
    void setCanvasSize(CanvasSize size);

    /// The canvas as a rectangle in layer-buffer space.
    [[nodiscard]] geometry::Rect canvasRect() const noexcept
    {
        return geometry::Rect{ m_origin.x, m_origin.y, m_size.width, m_size.height };
    }

    /// Destructive resize: every raster layer is reallocated to the new size
    /// (content anchored top-left, hidden margins dropped) and the origin resets.
    void resizeCanvas(CanvasSize size);

    /// Layers paint bottom-to-top; index 0 is the bottom of the stack.
    [[nodiscard]] const std::vector<std::unique_ptr<Layer>>& layers() const noexcept
    {
        return m_layers;
    }

    /// Adds a layer on top of the stack and returns its index.
    std::size_t addLayer(std::unique_ptr<Layer> layer);
    /// Inserts a layer at \p index, shifting later layers up.
    void insertLayer(std::size_t index, std::unique_ptr<Layer> layer);
    std::size_t addRasterLayer(std::string name);

    /// Moves a layer within the stack; returns false if the index is out of range.
    bool moveLayer(std::size_t fromIndex, std::size_t toIndex);
    /// Removes and returns a layer; returns nullptr if the index is out of range.
    std::unique_ptr<Layer> takeLayer(std::size_t index);

private:
    CanvasSize m_size;
    geometry::Point m_origin{ 0, 0 };
    std::string m_colorProfile;
    std::vector<std::unique_ptr<Layer>> m_layers;
};

} // namespace prism::core
