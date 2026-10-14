#include "brush/BrushPresets.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace prism::canvas {

using prism::core::raster::Color;

const std::vector<BrushPreset>& builtinBrushPresets() {
    static const std::vector<BrushPreset> presets = [] {
        auto make = [](std::string name, float radius, float hardness, float flow, float spacing,
                       float minimum, float curve, float stabilization, bool opacity,
                       bool erase = false) {
            BrushSettings brush;
            brush.radius = radius;
            brush.hardness = hardness;
            brush.flow = flow;
            brush.spacing = spacing;
            brush.pressureSizeMin = minimum;
            brush.pressureCurve = curve;
            brush.stabilization = stabilization;
            brush.pressureOpacity = opacity;
            brush.erase = erase;
            return BrushPreset{std::move(name), std::move(brush), {}};
        };
        auto pencil = [&](std::string name, float radius, float hardness, float flow, float grain,
                          float grainScale) {
            auto preset =
                make(std::move(name), radius, hardness, flow, 0.10F, 0.78F, 1.1F, 0.08F, true);
            preset.brush.material = BrushMaterial::Graphite;
            preset.brush.grain = grain;
            preset.brush.grainScale = grainScale;
            preset.brush.tiltShading = 0.85F;
            return preset;
        };
        auto cleanInk = make("Clean Ink", 5, 1, 1, 0.07F, 0.16F, 1.4F, 0.18F, false);
        cleanInk.brush.endTaperDiameters = 3.0F;
        auto boldInk = make("Bold Ink", 13, 1, 0.9F, 0.06F, 0.20F, 1.25F, 0.12F, false);
        boldInk.brush.endTaperDiameters = 2.0F;
        return std::vector<BrushPreset>{
            make("Prism Pen", 5, 0.9F, 1, 0.25F, 0.5F, 1, 0, false),
            pencil("HB Pencil", 4, 0.72F, 0.30F, 0.55F, 1.0F),
            pencil("2B Pencil", 9, 0.55F, 0.38F, 0.38F, 1.5F),
            std::move(cleanInk),
            std::move(boldInk),
            make("Soft Paint", 28, 0.22F, 0.16F, 0.06F, 0.65F, 0.9F, 0.06F, true),
            make("Opaque Paint", 22, 0.75F, 0.34F, 0.07F, 0.70F, 1, 0.04F, false),
            make("Airbrush", 36, 0.0F, 0.10F, 0.09F, 0.75F, 1, 0, true),
            make("Soft Eraser", 40, 0.0F, 0.5F, 0.10F, 0.5F, 1, 0, false, true),
            make("Hard Eraser", 16, 1, 1, 0.125F, 0.5F, 1, 0, false, true),
        };
    }();
    return presets;
}

RasterBuffer renderPresetThumbnail(const BrushSettings& brush, int width, int height,
                                   Color previewColor) {
    RasterBuffer thumb(width, height);
    if (thumb.isEmpty())
        return thumb;

    // Scale the brush so both a 2px pencil and a 40px airbrush read clearly
    // in a small thumbnail.
    BrushSettings b = brush;
    const float maxRadius =
        static_cast<float>(height) * (b.material == BrushMaterial::Graphite ? 0.055F : 0.22F);
    b.radius = b.radius > maxRadius ? maxRadius : (b.radius < 1.5F ? 1.5F : b.radius);
    if (!b.erase)
        b.color = previewColor;

    if (b.material == BrushMaterial::Graphite) {
        // Three real engine strokes make pressure and side-of-lead behavior
        // visible without a separate, misleading preview renderer.
        for (int row = 0; row < 3; ++row) {
            std::vector<StrokePoint> samples;
            samples.reserve(25);
            for (int i = 0; i <= 24; ++i) {
                const float t = static_cast<float>(i) / 24.0F;
                samples.push_back({12.0F + t * static_cast<float>(width - 24),
                                   (static_cast<float>(row) + 0.5F) * height / 3.0F,
                                   row == 0   ? 0.16F
                                   : row == 1 ? 0.85F
                                              : 0.55F,
                                   0.0F, row == 2 ? 55.0F : 0.0F, static_cast<float>(i * 8)});
            }
            (void)BrushStroke::paint(thumb, b, samples);
        }
        return thumb;
    }

    const float cy = static_cast<float>(height) * 0.5F;
    const float edge = maxRadius + 3.0F;
    std::vector<StrokePoint> points;
    const int steps = 24;
    for (int i = 0; i <= steps; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(steps);
        StrokePoint point{edge + t * std::max(0.0F, static_cast<float>(width) - 2.0F * edge),
                          cy + std::sin(t * 3.14159F * 1.4F) * static_cast<float>(height) * 0.18F,
                          0.35F + 0.65F * std::sin(t * 3.14159F)};
        if (!points.empty())
            point = BrushStroke::stabilizePoint(b, points.back(), point);
        points.push_back(point);
    }
    if (b.erase) {
        // Eraser presets read best as a stroke removed from a filled block.
        thumb.fill(Color{previewColor.r, previewColor.g, previewColor.b, 255});
    }
    (void)BrushStroke::paint(thumb, b, points);
    return thumb;
}

} // namespace prism::canvas
