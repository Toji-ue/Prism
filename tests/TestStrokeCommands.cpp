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
    Document doc{ CanvasSize{ 64, 64, 72 } };
    History history;
    BrushSettings brush{ .radius = 4.0F,
                         .hardness = 1.0F,
                         .color = raster::kOpaqueBlack,
                         .flow = 1.0F };

    void SetUp() override
    {
        doc.addRasterLayer("paint");
    }

    Layer& layer() { return *doc.layers().at(0); }
};

TEST_F(StrokeCommands, AddLayerUndoRedoRoundTrips)
{
    history.execute(std::make_unique<AddLayerCommand>(doc, "extra"));
    EXPECT_EQ(doc.layers().size(), std::size_t{ 2 });

    history.undo();
    EXPECT_EQ(doc.layers().size(), std::size_t{ 1 });

    history.redo();
    EXPECT_EQ(doc.layers().size(), std::size_t{ 2 });
    EXPECT_EQ(doc.layers().at(1)->name(), "extra");
}

TEST_F(StrokeCommands, StrokePaintsUndoesAndRedoes)
{
    // Paint through the command pipeline, exactly like AppController does.
    raster::RasterBuffer scratch(64, 64);
    const std::vector<prism::canvas::StrokePoint> points{ { 32.0F, 32.0F, 1.0F } };
    const auto rect = BrushStroke::paint(scratch, brush, points);
    ASSERT_FALSE(rect.isEmpty());

    auto painted = scratch.copyRect(rect.x, rect.y, rect.width, rect.height);
    history.execute(std::make_unique<StrokeCommand>(doc, 0, std::move(painted), rect.x,
                                                    rect.y));

    // Stroke is on the layer.
    EXPECT_EQ(layer().pixels().at(32, 32), raster::kOpaqueBlack);

    history.undo();
    EXPECT_EQ(layer().pixels().at(32, 32), raster::kTransparent);

    history.redo();
    EXPECT_EQ(layer().pixels().at(32, 32), raster::kOpaqueBlack);
}

TEST_F(StrokeCommands, StrokeOverExistingArtUndoRestoresBoth)
{
    // Seed existing art under where the stroke will land.
    layer().pixels().set(30, 30, raster::kOpaqueWhite);

    raster::RasterBuffer scratch(64, 64);
    const std::vector<prism::canvas::StrokePoint> points{ { 32.0F, 32.0F, 1.0F } };
    const auto rect = BrushStroke::paint(scratch, brush, points);
    auto painted = scratch.copyRect(rect.x, rect.y, rect.width, rect.height);
    history.execute(std::make_unique<StrokeCommand>(doc, 0, std::move(painted), rect.x,
                                                    rect.y));

    history.undo();
    // The white pixel must be restored exactly, not wiped by the stroke rect.
    EXPECT_EQ(layer().pixels().at(30, 30), raster::kOpaqueWhite);
    EXPECT_EQ(layer().pixels().at(32, 32), raster::kTransparent);
}

TEST(BrushStrokeTest, HardCircleCoversFullRadius)
{
    raster::RasterBuffer buf(32, 32);
    BrushSettings b{ .radius = 3.0F, .hardness = 1.0F, .color = raster::kOpaqueWhite, .flow = 1.0F };
    const auto rect = BrushStroke::paint(buf, b, { { 16.0F, 16.0F, 1.0F } });

    EXPECT_EQ(buf.at(16, 16), raster::kOpaqueWhite);
    // radius 3 strictly-inside falloff: pixels at |dx| < 3 from the integer
    // center, i.e. a 5px-wide footprint (not 7 — the rim sits at dist == r).
    EXPECT_EQ(rect.width, 5);
    EXPECT_EQ(rect.height, 5);
}

TEST(BrushStrokeTest, StrokeClipsAtCanvasEdge)
{
    raster::RasterBuffer buf(32, 32);
    BrushSettings b{ .radius = 5.0F, .hardness = 1.0F, .color = raster::kOpaqueWhite, .flow = 1.0F };
    const auto rect = BrushStroke::paint(buf, b, { { 0.0F, 0.0F, 1.0F } });
    // Stamps at the corner must clip, not crash, and the dirty rect stays
    // inside the buffer.
    EXPECT_GE(rect.x, 0);
    EXPECT_GE(rect.y, 0);
    EXPECT_LE(rect.x + rect.width, 32);
    EXPECT_LE(rect.y + rect.height, 32);
    EXPECT_EQ(buf.at(0, 0), raster::kOpaqueWhite);
}

} // namespace
