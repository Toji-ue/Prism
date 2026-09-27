#include "brush/BrushPresets.h"

#include <cmath>

namespace prism::canvas {

using prism::core::raster::Color;

const std::vector<BrushPreset>& builtinBrushPresets()
{
    // The names mirror the classic Krita starter set so muscle memory carries
    // over; sizes/hardness/flow are ours.
    static const std::vector<BrushPreset> presets = {
        { "Basic-5 Size Opacity", { 5.0F, 0.9F, Color{ 0, 0, 0, 255 }, 1.0F, false } },
        { "Ink Pen", { 3.0F, 1.0F, Color{ 0, 0, 0, 255 }, 1.0F, false } },
        { "Hard Round", { 12.0F, 1.0F, Color{ 0, 0, 0, 255 }, 1.0F, false } },
        { "Soft Airbrush", { 32.0F, 0.0F, Color{ 0, 0, 0, 255 }, 0.15F, false } },
        { "Pencil", { 2.0F, 1.0F, Color{ 0, 0, 0, 255 }, 0.75F, false } },
        { "Marker", { 22.0F, 0.85F, Color{ 0, 0, 0, 255 }, 0.5F, false } },
        { "Bristles", { 18.0F, 0.5F, Color{ 0, 0, 0, 255 }, 0.6F, false } },
        { "Wet Bristles", { 26.0F, 0.3F, Color{ 0, 0, 0, 255 }, 0.35F, false } },
        { "Eraser Soft", { 40.0F, 0.0F, Color{ 0, 0, 0, 255 }, 0.5F, true } },
        { "Eraser Hard", { 16.0F, 1.0F, Color{ 0, 0, 0, 255 }, 1.0F, true } },
        { "Eraser Pen", { 4.0F, 1.0F, Color{ 0, 0, 0, 255 }, 1.0F, true } },
        { "Airbrush Eraser", { 34.0F, 0.15F, Color{ 0, 0, 0, 255 }, 0.25F, true } },
    };
    return presets;
}

RasterBuffer renderPresetThumbnail(const BrushSettings& brush, int width, int height,
                                   Color previewColor)
{
    RasterBuffer thumb(width, height);
    if (thumb.isEmpty())
        return thumb;

    // Scale the brush so both a 2px pencil and a 40px airbrush read clearly
    // in a small thumbnail.
    BrushSettings b = brush;
    const float maxRadius = static_cast<float>(height) * 0.22F;
    b.radius = b.radius > maxRadius ? maxRadius : (b.radius < 1.5F ? 1.5F : b.radius);
    if (!b.erase)
        b.color = previewColor;

    const float cy = static_cast<float>(height) * 0.5F;
    std::vector<StrokePoint> points;
    const int steps = 24;
    for (int i = 0; i <= steps; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(steps);
        points.push_back({ 4.0F + t * static_cast<float>(width - 8),
                           cy + std::sin(t * 3.14159F * 1.4F) * static_cast<float>(height) * 0.18F,
                           0.35F + 0.65F * std::sin(t * 3.14159F) });
    }
    if (b.erase) {
        // Eraser presets read best as a stroke removed from a filled block.
        thumb.fill(Color{ previewColor.r, previewColor.g, previewColor.b, 255 });
        BrushStroke::paint(thumb, b, points);
    } else {
        BrushStroke::paint(thumb, b, points);
    }
    return thumb;
}

} // namespace prism::canvas
