#include "raster/ColorMath.h"

#include <algorithm>
#include <cmath>

namespace prism::core::raster {

Hsv rgbToHsv(Rgb c) noexcept
{
    const float mx = std::max({ c.r, c.g, c.b });
    const float mn = std::min({ c.r, c.g, c.b });
    const float delta = mx - mn;

    Hsv out;
    out.v = mx;
    if (mx <= 0.0F) {
        out.s = 0.0F;
        out.h = 0.0F;
        return out;
    }
    out.s = delta / mx;
    if (delta <= 0.0F) {
        out.h = 0.0F;
        return out;
    }

    if (mx == c.r)
        out.h = (c.g - c.b) / delta + (c.g < c.b ? 6.0F : 0.0F);
    else if (mx == c.g)
        out.h = (c.b - c.r) / delta + 2.0F;
    else
        out.h = (c.r - c.g) / delta + 4.0F;
    out.h /= 6.0F;
    return out;
}

Rgb hsvToRgb(Hsv in) noexcept
{
    const float h = in.h - std::floor(in.h); // wrap hue
    const float s = std::clamp(in.s, 0.0F, 1.0F);
    const float v = std::clamp(in.v, 0.0F, 1.0F);

    const float f = h * 6.0F;
    const int i = static_cast<int>(f) % 6;
    const float p = v * (1.0F - s);
    const float q = v * (1.0F - s * (f - static_cast<float>(static_cast<int>(f))));
    const float t = v * (1.0F - s * (1.0F - (f - static_cast<float>(static_cast<int>(f)))));

    switch (i) {
    case 0: return Rgb{ v, t, p };
    case 1: return Rgb{ q, v, p };
    case 2: return Rgb{ p, v, t };
    case 3: return Rgb{ p, q, v };
    case 4: return Rgb{ t, p, v };
    default: return Rgb{ v, p, q };
    }
}

} // namespace prism::core::raster
