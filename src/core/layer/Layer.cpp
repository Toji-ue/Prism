#include "layer/Layer.h"

#include <algorithm>

namespace prism::core {

Layer::Layer(Type type, std::string name) noexcept
    : m_type(type)
    , m_name(std::move(name))
{
}

void Layer::setOpacity(float opacity) noexcept
{
    m_opacity = std::clamp(opacity, 0.0F, 1.0F);
}

std::unique_ptr<Layer> Layer::clone() const
{
    auto copy = std::make_unique<Layer>(m_type, m_name);
    copy->m_visible = m_visible;
    copy->m_locked = m_locked;
    copy->m_opacity = m_opacity;
    copy->m_blendMode = m_blendMode;
    copy->m_pixels = m_pixels;
    return copy;
}

} // namespace prism::core
