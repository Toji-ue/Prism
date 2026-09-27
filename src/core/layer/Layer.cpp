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

} // namespace prism::core
