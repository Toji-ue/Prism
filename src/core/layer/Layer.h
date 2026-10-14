#pragma once

#include "layer/BlendMode.h"
#include "raster/RasterBuffer.h"

#include <memory>
#include <cstdint>
#include <string>

namespace prism::core {

/// One entry in a Document's layer stack.
///
/// Opacity is the layer-wide alpha factor (stored 0..1, display % in UI).
/// Flow is a brush-stroke property and intentionally does NOT live here —
/// mixing the two is a known bug farm (see AGENTS.md).
///
/// Raster layers own their pixel buffer. All buffer mutations must go
/// through history Commands or undo silently breaks.
class Layer {
public:
    enum class Type {
        Raster,
        Vector,
        Group,
    };

    explicit Layer(Type type, std::string name) noexcept;

    [[nodiscard]] Type type() const noexcept { return m_type; }
    [[nodiscard]] std::uint64_t id() const noexcept { return m_id; }
    void setId(std::uint64_t id) noexcept;
    [[nodiscard]] std::uint64_t parentId() const noexcept { return m_parentId; }
    void setParentId(std::uint64_t id) noexcept { m_parentId = id; }
    [[nodiscard]] bool isClipped() const noexcept { return m_clipped; }
    void setClipped(bool clipped) noexcept { m_clipped = clipped; }
    [[nodiscard]] bool isAlphaLocked() const noexcept { return m_alphaLocked; }
    void setAlphaLocked(bool locked) noexcept { m_alphaLocked = locked; }
    [[nodiscard]] const std::string& name() const noexcept { return m_name; }
    void setName(std::string name) noexcept { m_name = std::move(name); }

    [[nodiscard]] bool isVisible() const noexcept { return m_visible; }
    void setVisible(bool visible) noexcept { m_visible = visible; }

    [[nodiscard]] bool isLocked() const noexcept { return m_locked; }
    void setLocked(bool locked) noexcept { m_locked = locked; }

    /// Layer opacity, 0..1 clamped.
    [[nodiscard]] float opacity() const noexcept { return m_opacity; }
    void setOpacity(float opacity) noexcept;

    [[nodiscard]] BlendMode blendMode() const noexcept { return m_blendMode; }
    void setBlendMode(BlendMode mode) noexcept { m_blendMode = mode; }

    /// Pixel buffer. Empty for vector/group layers. Resizing invalidates
    /// content — only do it via a history Command.
    [[nodiscard]] raster::RasterBuffer& pixels() noexcept { return m_pixels; }
    [[nodiscard]] const raster::RasterBuffer& pixels() const noexcept { return m_pixels; }

    /// Deep copy of this layer (pixels + properties), for duplicate.
    [[nodiscard]] std::unique_ptr<Layer> clone() const;

private:
    Type m_type;
    std::uint64_t m_id = 0;
    std::uint64_t m_parentId = 0;
    bool m_clipped = false;
    bool m_alphaLocked = false;
    std::string m_name;
    bool m_visible = true;
    bool m_locked = false;
    float m_opacity = 1.0F;
    BlendMode m_blendMode = BlendMode::Normal;
    raster::RasterBuffer m_pixels;
};

} // namespace prism::core
