#pragma once

#include "geometry/Types.h"
#include "raster/RasterBuffer.h"

#include <string>

namespace prism::canvas {

using prism::core::geometry::Rect;
using prism::core::raster::RasterBuffer;

/// Pixel filters for the Filter menu. Pure CPU and framework-free so they can
/// be unit-tested and later matched by GPU implementations.
///
/// Each filter rewrites \p buffer in place and reports the rect it touched.
namespace filters {

/// Inverts RGB, leaving alpha alone (Krita's "Invert").
Rect invert(RasterBuffer& buffer);

/// Converts to luminance, keeping alpha (Krita's "Desaturate").
Rect desaturate(RasterBuffer& buffer);

/// Three box-blur passes approximate a Gaussian closely enough for a brush
/// app while staying O(n) per pass. \p radius is in pixels.
Rect gaussianBlurApprox(RasterBuffer& buffer, int radius);

/// Brightness/contrast: \p brightness and \p contrast are multipliers where
/// 1.0 leaves the image unchanged.
Rect brightnessContrast(RasterBuffer& buffer, float brightness, float contrast);

} // namespace filters

} // namespace prism::canvas
