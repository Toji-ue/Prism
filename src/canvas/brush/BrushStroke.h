#pragma once

#include "geometry/Types.h"
#include "raster/RasterBuffer.h"

#include <cstdint>
#include <string>
#include <vector>

namespace prism::canvas {

using prism::core::raster::RasterBuffer;

/// A single pen input sample, canvas coordinates, pressure 0..1.
struct StrokePoint {
    float x = 0.0F;
    float y = 0.0F;
    float pressure = 1.0F;
    float tiltX = 0.0F; // degrees, -60..60; zero for mouse/no-tilt pens
    float tiltY = 0.0F;
    float timeMs = -1.0F; // elapsed time since pen-down; negative if unavailable
};

enum class BrushMaterial { Solid, Graphite };

/// Brush tip shape — procedural (math) or image (texture).
struct BrushTip {
    enum class Kind { Procedural, Image };

    Kind kind = Kind::Procedural;

    // when kind == Procedural
    enum class Shape { Round, Square };
    Shape shape = Shape::Round;
    float hardness = 1.0F;  // 0..1
    float roundness = 1.0F; // 0..1 (1 = round, low = flat ellipse)
    float angle = 0.0F;     // degrees, 0..360

    // when kind == Image
    struct Image {
        std::string mime;               // "image/png"
        std::vector<std::uint8_t> data; // PNG bytes (serialization)
        bool invert = false;            // black = paint or white = paint
        // Decoded grayscale mask, filled by the loader (Qt side) so the
        // paint loop never decodes PNG. Sampled nearest-neighbour.
        int maskW = 0;
        int maskH = 0;
        std::vector<std::uint8_t> mask;
    } image;
};

/// Brush settings for one stroke.
struct BrushSettings {
    BrushMaterial material = BrushMaterial::Solid;
    float radius = 8.0F;   // canvas pixels, at full pressure
    float hardness = 0.8F; // 0 = soft falloff, 1 = hard circle
    prism::core::raster::Color color = prism::core::raster::kOpaqueBlack;
    /// Flow: paint deposited per stamp, 0..1. A stroke property, NOT layer
    /// opacity — see AGENTS.md terminology note.
    float flow = 1.0F;
    /// Eraser: removes paint instead of depositing it.
    bool erase = false;

    /// Dynamics: pressure scales deposited alpha (not just size).
    bool pressureOpacity = false;

    /// Distance between stamps as a fraction of the tip diameter.
    float spacing = 0.125F;
    float pressureCurve = 1.0F;
    float pressureSizeMin = 0.5F;
    /// Position smoothing, 0 = direct input, 1 = strongest smoothing.
    float stabilization = 0.0F;
    float grain = 0.0F;       // 0..1, canvas-anchored graphite variation
    float grainScale = 1.0F;  // 0.5..4, paper grain size
    float tiltShading = 0.0F; // 0..1, broad side-of-lead response
    float endTaperDiameters = 0.0F; // 0..6, length of the pointed pen-up tail

    BrushTip tip;
};

/// Antialiased circular brush. Stamps are interpolated along the input path
/// so fast mouse movement never leaves gaps; overlapping stamps accumulate
/// paint up to full alpha, matching how a real brush builds up opacity.
class BrushStroke {
public:
    struct StrokeState {
        float distanceToNext = 0.0F;
        float filteredPressure = 1.0F;
        float lastTimeMs = -1.0F;
        float pathDistance = 0.0F;
        float totalDistance = -1.0F; // known for committed/replayed strokes
        bool started = false;
    };
    /// Pure pressure response shared by live strokes, replay, and previews.
    [[nodiscard]] static float shapedPressure(const BrushSettings& brush, float pressure) noexcept;
    /// Smooth small pen jitter while letting large fast moves catch up immediately.
    [[nodiscard]] static StrokePoint stabilizePoint(const BrushSettings& brush,
                                                    const StrokePoint& previous,
                                                    const StrokePoint& incoming) noexcept;
    /// Paints \p points into \p buffer with \p brush.
    /// Returns the bounding rect (buffer coordinates) actually touched, or
    /// an empty rect if nothing was painted.
    [[nodiscard]] static prism::core::geometry::Rect
    paint(RasterBuffer& buffer, const BrushSettings& brush, const std::vector<StrokePoint>& points);

    /// Paints only the segment [prev, current] (interpolated) into \p buffer.
    /// Used by the live-preview path, which reuses already-painted points.
    [[nodiscard]] static prism::core::geometry::Rect
    paintSegment(RasterBuffer& buffer, const BrushSettings& brush, const StrokePoint& prev,
                 const StrokePoint& current, StrokeState* state = nullptr);

    /// Covers the exact pen-up position when it falls between regular stamps.
    [[nodiscard]] static prism::core::geometry::Rect finishStroke(RasterBuffer& buffer,
                                                                  const BrushSettings& brush,
                                                                  const StrokePoint& current,
                                                                  StrokeState& state);

    /// Blend/Smudge: drags the canvas colour along the stroke instead of
    /// depositing the brush colour (Clip Studio Paint's Blend tool). \p carried
    /// is the colour being dragged between calls; it follows the canvas.
    ///
    /// ponytail: one averaged colour per stamp, not per-pixel buckets. A proper
    /// implementation keeps a colour buffer per pixel; upgrade when the smudge
    /// needs to preserve fine detail instead of blurring it.
    [[nodiscard]] static prism::core::geometry::Rect
    smudgeSegment(RasterBuffer& buffer, const BrushSettings& brush, const StrokePoint& prev,
                  const StrokePoint& current, prism::core::raster::Color& carried);

private:
    static void stamp(RasterBuffer& buffer, const BrushSettings& brush, float cx, float cy,
                      float pressure, float tiltX, float tiltY, float taper,
                      prism::core::geometry::Rect& bounds);
    static float coverage(float dist, float radius, float hardness) noexcept;
};

} // namespace prism::canvas
