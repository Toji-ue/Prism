#pragma once

#include "brush/BrushStroke.h"
#include "raster/RasterBuffer.h"

#include <string>
#include <vector>

namespace prism::canvas {

using prism::core::raster::RasterBuffer;

/// A named brush preset — the unit the Brush Presets docker shows and the
/// toolbar's preset selector switches between.
struct BrushPreset {
    std::string name;
    BrushSettings brush;
    std::string id; // Stable UI identity; .prismbrush stores only the brush itself.
};

/// The built-in starter set. Sizes are in canvas pixels.
[[nodiscard]] const std::vector<BrushPreset>& builtinBrushPresets();

/// Renders a representative stroke of \p brush into \p width x \p height,
/// transparent background — used for preset thumbnails. Pure CPU so it works
/// with no GPU context (and is unit-testable).
[[nodiscard]] RasterBuffer renderPresetThumbnail(const BrushSettings& brush, int width,
                                                 int height,
                                                 prism::core::raster::Color previewColor
                                                 = prism::core::raster::kOpaqueBlack);

} // namespace prism::canvas
