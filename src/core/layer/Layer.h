#pragma once

#include "layer/BlendMode.h"

#include <string>

namespace prism::core {

/// One entry in a Document's layer stack.
///
/// Opacity is the layer-wide alpha factor (stored 0..1, display % in UI).
/// Flow is a brush-stroke property and intentionally does NOT live here —
/// mixing the two is a known bug farm (see AGENTS.md).
///
/// Pixel data is not owned yet: the raster buffer arrives with the canvas
/// module, and all mutations must go through history Commands.
class Layer {
public:
    enum class Type {
        Raster,
        Vector,
        Group,
    };

    explicit Layer(Type type, std::string name) noexcept;

    [[nodiscard]] Type type() const noexcept { return m_type; }
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

private:
    Type m_type;
    std::string m_name;
    bool m_visible = true;
    bool m_locked = false;
    float m_opacity = 1.0F;
    BlendMode m_blendMode = BlendMode::Normal;
};

} // namespace prism::core
