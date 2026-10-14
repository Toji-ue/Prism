#include "layer/Layer.h"

#include <algorithm>
#include <atomic>

namespace prism::core {

namespace {
std::atomic<std::uint64_t> nextLayerId{1};
}

Layer::Layer(Type type, std::string name) noexcept
    : m_type(type)
    , m_id(nextLayerId.fetch_add(1))
    , m_name(std::move(name))
{
}

void Layer::setId(std::uint64_t id) noexcept {
    if (id == 0)
        return;
    m_id = id;
    auto next = nextLayerId.load();
    while (next <= id && !nextLayerId.compare_exchange_weak(next, id + 1)) {}
}

void Layer::setOpacity(float opacity) noexcept
{
    m_opacity = std::clamp(opacity, 0.0F, 1.0F);
}

std::unique_ptr<Layer> Layer::clone() const
{
    auto copy = std::make_unique<Layer>(m_type, m_name);
    copy->m_visible = m_visible;
    copy->m_parentId = m_parentId;
    copy->m_clipped = m_clipped;
    copy->m_alphaLocked = m_alphaLocked;
    copy->m_locked = m_locked;
    copy->m_opacity = m_opacity;
    copy->m_blendMode = m_blendMode;
    copy->m_pixels = m_pixels;
    return copy;
}

} // namespace prism::core
