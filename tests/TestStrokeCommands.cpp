#include "brush/BrushPresets.h"
#include "brush/BrushStroke.h"
#include "commands/Commands.h"
#include "document/Document.h"
#include "history/History.h"
#include "layer/Layer.h"

#include <gtest/gtest.h>

namespace {

using namespace prism::core;
using prism::canvas::BrushSettings;
using prism::canvas::BrushStroke;
using prism::canvas::StrokePoint;

class StrokeCommands : public ::testing::Test {
protected:
    Document doc{CanvasSize{64, 64, 72}};
    History history;
    BrushSettings brush{
        .radius = 4.0F, .hardness = 1.0F, .color = raster::kOpaqueBlack, .flow = 1.0F};

    void SetUp() override { doc.addRasterLayer("paint"); }

    Layer& layer() { return *doc.layers().at(0); }
};

TEST_F(StrokeCommands, AddLayerUndoRedoRoundTrips) {
    history.execute(std::make_unique<AddLayerCommand>(doc, "extra"));
    EXPECT_EQ(doc.layers().size(), std::size_t{2});

    history.undo();
    EXPECT_EQ(doc.layers().size(), std::size_t{1});

    history.redo();
    EXPECT_EQ(doc.layers().size(), std::size_t{2});
    EXPECT_EQ(doc.layers().at(1)->name(), "extra");
}

TEST_F(StrokeCommands, StrokePaintsUndoesAndRedoes) {
    // Paint through the command pipeline, exactly like AppController does.
    raster::RasterBuffer scratch(64, 64);
    const std::vector<prism::canvas::StrokePoint> points{{32.0F, 32.0F, 1.0F}};
    const auto rect = BrushStroke::paint(scratch, brush, points);
    ASSERT_FALSE(rect.isEmpty());

    auto painted = scratch.copyRect(rect.x, rect.y, rect.width, rect.height);
    history.execute(std::make_unique<StrokeCommand>(doc, 0, std::move(painted), rect.x, rect.y));

    // Stroke is on the layer.
    EXPECT_EQ(layer().pixels().at(32, 32), raster::kOpaqueBlack);

    history.undo();
    EXPECT_EQ(layer().pixels().at(32, 32), raster::kTransparent);

    history.redo();
    EXPECT_EQ(layer().pixels().at(32, 32), raster::kOpaqueBlack);
}

TEST_F(StrokeCommands, StrokeOverExistingArtUndoRestoresBoth) {
    // Seed existing art under where the stroke will land.
    layer().pixels().set(30, 30, raster::kOpaqueWhite);

    raster::RasterBuffer scratch(64, 64);
    const std::vector<prism::canvas::StrokePoint> points{{32.0F, 32.0F, 1.0F}};
    const auto rect = BrushStroke::paint(scratch, brush, points);
    auto painted = scratch.copyRect(rect.x, rect.y, rect.width, rect.height);
    history.execute(std::make_unique<StrokeCommand>(doc, 0, std::move(painted), rect.x, rect.y));

    history.undo();
    // The white pixel must be restored exactly, not wiped by the stroke rect.
    EXPECT_EQ(layer().pixels().at(30, 30), raster::kOpaqueWhite);
    EXPECT_EQ(layer().pixels().at(32, 32), raster::kTransparent);
}

TEST_F(StrokeCommands, GraphiteStrokeUndoRedoPreservesGrain) {
    const auto pencil = prism::canvas::builtinBrushPresets().at(1).brush;
    raster::RasterBuffer preview(64, 64);
    const std::vector<StrokePoint> points{{8, 32, 0.2F}, {52, 32, 0.7F}};
    const auto rect = BrushStroke::paint(preview, pencil, points);
    ASSERT_FALSE(rect.isEmpty());
    const auto expected = preview.at(32, 32);
    ASSERT_GT(expected.a, 0);
    history.execute(std::make_unique<StrokeCommand>(
        doc, 0, preview.copyRect(rect.x, rect.y, rect.width, rect.height), rect.x, rect.y));
    EXPECT_EQ(layer().pixels().at(32, 32), expected);
    history.undo();
    EXPECT_EQ(layer().pixels().at(32, 32), raster::kTransparent);
    history.redo();
    EXPECT_EQ(layer().pixels().at(32, 32), expected);
}

TEST(BrushStrokeTest, HardCircleCoversFullRadius) {
    raster::RasterBuffer buf(32, 32);
    BrushSettings b{.radius = 3.0F, .hardness = 1.0F, .color = raster::kOpaqueWhite, .flow = 1.0F};
    const auto rect = BrushStroke::paint(buf, b, {{16.0F, 16.0F, 1.0F}});

    EXPECT_EQ(buf.at(16, 16), raster::kOpaqueWhite);
    // radius 3 hard brush with 0.5px AA rim: full coverage to dist <= 3,
    // i.e. axis pixels from 13..19 (7px footprint).
    EXPECT_EQ(rect.width, 7);
    EXPECT_EQ(rect.height, 7);
}

TEST(BrushStrokeTest, LightPenTouchKeepsMostOfTheBrushSize) {
    // A real tablet reports 0.01-0.2 for a light touch. Scaling the radius from
    // zero turned those strokes into 0.6px scratches and swung the width 3x
    // inside one stroke, which the user saw as broken, angular lines.
    const auto width = [](float pressure) {
        raster::RasterBuffer buf(64, 64);
        BrushSettings b{
            .radius = 20.0F, .hardness = 1.0F, .color = raster::kOpaqueWhite, .flow = 1.0F};
        const auto rect = BrushStroke::paint(buf, b, {{32.0F, 32.0F, pressure}});
        return rect.width;
    };
    const int light = width(0.15F);
    const int full = width(1.0F);
    EXPECT_GT(light, 0);
    // Half the brush at the lightest touch, full size at full pressure, and
    // never more than a 2.1x swing between them.
    EXPECT_GE(light, full * 0.5F);
    EXPECT_LE(static_cast<float>(full) / static_cast<float>(light), 2.1F);
}

TEST(BrushStrokeTest, SketchPencilKeepsRoundedLightEnds) {
    auto brush = prism::canvas::builtinBrushPresets().at(1).brush;
    brush.radius = 12.0F;
    raster::RasterBuffer ink(180, 64);
    const std::vector<StrokePoint> points{{20, 32, 0.12F}, {80, 32, 0.9F}, {140, 32, 0.12F}};
    (void)BrushStroke::paint(ink, brush, points);
    const auto widthAt = [&](int x) {
        int width = 0;
        for (int y = 8; y < 56; ++y)
            width += ink.at(x, y).a > 0 ? 1 : 0;
        return width;
    };
    EXPECT_GE(widthAt(22), widthAt(80) * 0.7F);
    EXPECT_GE(widthAt(138), widthAt(80) * 0.7F);
    EXPECT_LT(ink.at(20, 32).a, ink.at(80, 32).a);
    EXPECT_LT(ink.at(140, 32).a, ink.at(80, 32).a);
}

TEST(BrushStrokeTest, GraphiteConstantPressureHasNoTaperOrDarkTerminalDot) {
    auto brush = prism::canvas::builtinBrushPresets().at(1).brush;
    brush.radius = 10.0F;
    raster::RasterBuffer ink(180, 64);
    (void)BrushStroke::paint(ink, brush, {{20, 32, 0.5F}, {140, 32, 0.5F}});
    const auto widthAt = [&](int x) {
        int width = 0;
        for (int y = 8; y < 56; ++y)
            width += ink.at(x, y).a > 0 ? 1 : 0;
        return width;
    };
    EXPECT_GE(widthAt(24), widthAt(80) * 0.7F);
    EXPECT_GE(widthAt(136), widthAt(80) * 0.7F);
    const auto patchAlpha = [&](int cx) {
        int total = 0;
        for (int y = 30; y <= 34; ++y)
            for (int x = cx - 2; x <= cx + 2; ++x)
                total += ink.at(x, y).a;
        return total;
    };
    EXPECT_LT(patchAlpha(20), patchAlpha(80));
    EXPECT_LT(patchAlpha(140), patchAlpha(80));
}

TEST(BrushStrokeTest, InkTapersOnlyAtReleaseAndLegacyPencilStaysRound) {
    auto inkBrush = prism::canvas::builtinBrushPresets().at(3).brush;
    EXPECT_GT(inkBrush.endTaperDiameters, 0.0F);
    inkBrush.radius = 9.0F;
    raster::RasterBuffer ink(220, 80);
    const std::vector<StrokePoint> path{{20, 40, 1.0F}, {100, 40, 1.0F}, {195, 40, 1.0F}};
    (void)BrushStroke::paint(ink, inkBrush, path);
    const auto widthAt = [](const raster::RasterBuffer& image, int x) {
        int width = 0;
        for (int y = 0; y < image.height(); ++y)
            width += image.at(x, y).a > 0 ? 1 : 0;
        return width;
    };
    EXPECT_GE(widthAt(ink, 50), 12);
    EXPECT_LT(widthAt(ink, 188), widthAt(ink, 100) * 0.65F);
    raster::RasterBuffer click(80, 80);
    (void)BrushStroke::paint(click, inkBrush, {{40, 40, 1.0F}});
    EXPECT_GE(widthAt(click, 40), 12);

    auto pencil = prism::canvas::builtinBrushPresets().at(1).brush;
    EXPECT_FLOAT_EQ(pencil.endTaperDiameters, 0.0F);
    raster::RasterBuffer graphite(220, 80);
    (void)BrushStroke::paint(graphite, pencil, path);
    EXPECT_GE(widthAt(graphite, 190), widthAt(graphite, 100) * 0.7F);
}

TEST(BrushStrokeTest, GraphiteBuildsUpAndTiltMakesBroadSideStroke) {
    auto brush = prism::canvas::builtinBrushPresets().at(1).brush;
    brush.radius = 8.0F;
    raster::RasterBuffer one(128, 64);
    raster::RasterBuffer layered(128, 64);
    const std::vector<StrokePoint> path{{15, 32, 0.2F}, {110, 32, 0.2F}};
    (void)BrushStroke::paint(one, brush, path);
    (void)BrushStroke::paint(layered, brush, path);
    (void)BrushStroke::paint(layered, brush, path);
    EXPECT_GT(one.at(60, 32).a, 0);
    EXPECT_LT(one.at(60, 32).a, 150);
    EXPECT_GT(layered.at(60, 32).a, one.at(60, 32).a);

    raster::RasterBuffer upright(100, 100);
    raster::RasterBuffer tilted(100, 100);
    (void)BrushStroke::paint(upright, brush, {{50, 50, 0.5F}});
    (void)BrushStroke::paint(tilted, brush, {{50, 50, 0.5F, 55, 0, 0}});
    const auto footprint = [](const raster::RasterBuffer& image) {
        int count = 0;
        for (int x = 0; x < image.width(); ++x)
            count += image.at(x, 50).a > 0 ? 1 : 0;
        return count;
    };
    EXPECT_GT(footprint(tilted), footprint(upright) * 1.5F);
}

TEST(BrushStrokeTest, GraphiteLiveReplayAndInputSamplingMatch) {
    auto brush = prism::canvas::builtinBrushPresets().at(1).brush;
    brush.radius = 7.0F;
    const std::vector<StrokePoint> points{
        {12, 25, 0.16F, 0, 0, 0}, {40, 25, 0.8F, 0, 0, 12}, {75, 25, 0.4F, 50, 0, 30}};
    raster::RasterBuffer live(100, 60);
    raster::RasterBuffer replay(100, 60);
    BrushStroke::StrokeState state;
    (void)BrushStroke::paintSegment(live, brush, points.front(), points.front(), &state);
    for (std::size_t i = 1; i < points.size(); ++i)
        (void)BrushStroke::paintSegment(live, brush, points[i - 1], points[i], &state);
    (void)BrushStroke::finishStroke(live, brush, points.back(), state);
    (void)BrushStroke::paint(replay, brush, points);
    EXPECT_EQ(live, replay);

    raster::RasterBuffer sparse(100, 60);
    raster::RasterBuffer dense(100, 60);
    (void)BrushStroke::paint(sparse, brush, {{12, 25, 0.5F}, {75, 25, 0.5F}});
    std::vector<StrokePoint> samples;
    for (int x = 12; x <= 75; x += 3)
        samples.push_back({static_cast<float>(x), 25, 0.5F});
    (void)BrushStroke::paint(dense, brush, samples);
    EXPECT_EQ(sparse, dense);
}

TEST(BrushStrokeTest, GraphiteSpacingDoesNotSetDarkness) {
    auto fine = prism::canvas::builtinBrushPresets().at(1).brush;
    fine.radius = 10.0F;
    fine.spacing = 0.08F;
    auto coarse = fine;
    coarse.spacing = 0.24F;
    raster::RasterBuffer a(180, 60);
    raster::RasterBuffer b(180, 60);
    const std::vector<StrokePoint> points{{20, 30, 0.6F}, {160, 30, 0.6F}};
    (void)BrushStroke::paint(a, fine, points);
    (void)BrushStroke::paint(b, coarse, points);
    int sumA = 0;
    int sumB = 0;
    for (int x = 45; x < 135; ++x) {
        sumA += a.at(x, 30).a;
        sumB += b.at(x, 30).a;
    }
    EXPECT_GT(sumA, 0);
    EXPECT_NEAR(sumA, sumB, sumA * 0.15F);
}

TEST(BrushStrokeTest, GraphitePressureSmoothingUsesElapsedTime) {
    auto brush = prism::canvas::builtinBrushPresets().at(1).brush;
    raster::RasterBuffer slow(32, 32);
    raster::RasterBuffer fast(32, 32);
    BrushStroke::StrokeState oneSample;
    BrushStroke::StrokeState fourSamples;
    const StrokePoint start{16, 16, 0.2F, 0, 0, 0};
    const StrokePoint end{16, 16, 0.8F, 0, 0, 20};
    (void)BrushStroke::paintSegment(slow, brush, start, end, &oneSample);
    StrokePoint previous = start;
    for (int ms : {5, 10, 15, 20}) {
        const StrokePoint current{16, 16, 0.8F, 0, 0, static_cast<float>(ms)};
        (void)BrushStroke::paintSegment(fast, brush, previous, current, &fourSamples);
        previous = current;
    }
    EXPECT_NEAR(oneSample.filteredPressure, fourSamples.filteredPressure, 1.0e-5F);
}

TEST(BrushStrokeTest, GraphitePreviewShowsLightFirmAndTilt) {
    const auto brush = prism::canvas::builtinBrushPresets().at(1).brush;
    const auto image = prism::canvas::renderPresetThumbnail(brush, 220, 84, raster::kOpaqueBlack);
    const auto row = [&](int centre) {
        int total = 0;
        int footprint = 0;
        for (int y = centre - 12; y <= centre + 12; ++y) {
            total += image.at(110, y).a;
            footprint += image.at(110, y).a > 0 ? 1 : 0;
        }
        return std::pair{total, footprint};
    };
    const auto light = row(14);
    const auto firm = row(42);
    const auto tilted = row(70);
    EXPECT_GT(firm.first, light.first);
    EXPECT_GT(tilted.second, firm.second);
}

TEST(BrushStrokeTest, GraphiteLightClickMatchesLiveDot) {
    const auto brush = prism::canvas::builtinBrushPresets().at(1).brush;
    const StrokePoint point{32, 32, 0.05F, 0, 0, 0};
    raster::RasterBuffer offline(64, 64);
    raster::RasterBuffer live(64, 64);
    const auto rect = BrushStroke::paint(offline, brush, {point});
    BrushStroke::StrokeState state;
    (void)BrushStroke::paintSegment(live, brush, point, point, &state);
    (void)BrushStroke::finishStroke(live, brush, point, state);
    EXPECT_FALSE(rect.isEmpty());
    EXPECT_EQ(offline, live);
}

TEST(BrushStrokeTest, StrokeClipsAtCanvasEdge) {
    raster::RasterBuffer buf(32, 32);
    BrushSettings b{.radius = 5.0F, .hardness = 1.0F, .color = raster::kOpaqueWhite, .flow = 1.0F};
    const auto rect = BrushStroke::paint(buf, b, {{0.0F, 0.0F, 1.0F}});
    // Stamps at the corner must clip, not crash, and the dirty rect stays
    // inside the buffer.
    EXPECT_GE(rect.x, 0);
    EXPECT_GE(rect.y, 0);
    EXPECT_LE(rect.x + rect.width, 32);
    EXPECT_LE(rect.y + rect.height, 32);
    EXPECT_EQ(buf.at(0, 0), raster::kOpaqueWhite);
}

TEST(BrushStrokeTest, SpacingIsIndependentOfInputSampleCount) {
    BrushSettings brush;
    brush.radius = 5.0F;
    brush.hardness = 1.0F;
    brush.flow = 0.2F;
    brush.spacing = 0.35F;
    raster::RasterBuffer sparse(128, 48);
    raster::RasterBuffer dense(128, 48);
    const std::vector<StrokePoint> few{{8, 24, 0.5F}, {108, 24, 0.5F}};
    std::vector<StrokePoint> many;
    for (int x = 8; x <= 108; x += 5)
        many.push_back({static_cast<float>(x), 24, 0.5F});
    EXPECT_FALSE(BrushStroke::paint(sparse, brush, few).isEmpty());
    EXPECT_FALSE(BrushStroke::paint(dense, brush, many).isEmpty());
    EXPECT_EQ(sparse, dense);
    EXPECT_GT(sparse.at(108, 24).a, 0);
}

TEST(BrushStrokeTest, LiveSegmentsMatchOfflineStroke) {
    BrushSettings brush;
    brush.radius = 7.0F;
    brush.flow = 0.3F;
    brush.spacing = 0.2F;
    brush.pressureOpacity = true;
    brush.pressureCurve = 1.6F;
    const std::vector<StrokePoint> points{{10, 10, 0.15F}, {33, 15, 0.5F}, {55, 35, 0.9F}};
    raster::RasterBuffer live(80, 60);
    raster::RasterBuffer offline(80, 60);
    BrushStroke::StrokeState state;
    for (std::size_t i = 1; i < points.size(); ++i)
        (void)BrushStroke::paintSegment(live, brush, points[i - 1], points[i], &state);
    (void)BrushStroke::finishStroke(live, brush, points.back(), state);
    (void)BrushStroke::paint(offline, brush, points);
    EXPECT_EQ(live, offline);
}

TEST(BrushStrokeTest, PressureCurveChangesLowPressureDeposit) {
    BrushSettings soft;
    soft.radius = 8.0F;
    soft.flow = 0.5F;
    soft.pressureOpacity = true;
    BrushSettings firm = soft;
    firm.pressureCurve = 2.0F;
    raster::RasterBuffer a(40, 40);
    raster::RasterBuffer b(40, 40);
    (void)BrushStroke::paint(a, soft, {{20, 20, 0.25F}});
    (void)BrushStroke::paint(b, firm, {{20, 20, 0.25F}});
    EXPECT_GT(a.at(20, 20).a, b.at(20, 20).a);
    EXPECT_FLOAT_EQ(BrushStroke::shapedPressure(firm, 1.0F), 1.0F);
}

TEST(BrushStrokeTest, StabilizationKeepsFastMovesResponsive) {
    BrushSettings brush;
    brush.radius = 5.0F;
    brush.stabilization = 1.0F;
    const StrokePoint origin{10, 10, 0.4F};
    const auto slow = BrushStroke::stabilizePoint(brush, origin, {11, 10, 0.6F});
    const auto fast = BrushStroke::stabilizePoint(brush, origin, {30, 10, 0.6F});
    EXPECT_GT(slow.x, origin.x);
    EXPECT_LT(slow.x, 11.0F);
    EXPECT_FLOAT_EQ(fast.x, 30.0F);
    EXPECT_FLOAT_EQ(fast.pressure, 0.6F);
}

} // namespace
