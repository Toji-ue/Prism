#include "compositor/Compositor.h"

#include "document/Document.h"
#include "layer/Layer.h"

#include <gtest/gtest.h>

namespace {

using namespace prism::core;
using prism::canvas::Compositor;
using Raster = raster::RasterBuffer;

class CompositorTest : public ::testing::Test {
protected:
    Document doc{ CanvasSize{ 16, 16, 72 } };
    Compositor compositor{ doc };

    Layer& addLayerWithPixel(int x, int y, raster::Color c)
    {
        doc.addRasterLayer("l");
        auto& layer = *doc.layers().back();
        layer.pixels().set(x, y, c);
        return layer;
    }
};

TEST_F(CompositorTest, EmptyDocumentCompositesToTransparent)
{
    compositor.composite({ 0, 0, 16, 16 });
    EXPECT_EQ(compositor.result().at(5, 5), raster::kTransparent);
}

TEST_F(CompositorTest, SingleLayerPixelShowsThrough)
{
    addLayerWithPixel(5, 5, raster::kOpaqueWhite);
    compositor.composite({ 0, 0, 16, 16 });
    EXPECT_EQ(compositor.result().at(5, 5), raster::kOpaqueWhite);
    EXPECT_EQ(compositor.result().at(6, 5), raster::kTransparent);
}

TEST_F(CompositorTest, TopLayerCoversBottom)
{
    addLayerWithPixel(5, 5, raster::Color{ 255, 0, 0, 255 });
    addLayerWithPixel(5, 5, raster::Color{ 0, 0, 255, 255 });
    compositor.composite({ 0, 0, 16, 16 });
    EXPECT_EQ(compositor.result().at(5, 5), (raster::Color{ 0, 0, 255, 255 }));
}

TEST_F(CompositorTest, LayerOpacityScalesAlpha)
{
    auto& layer = addLayerWithPixel(5, 5, raster::Color{ 255, 0, 0, 255 });
    layer.setOpacity(0.5F);
    compositor.composite({ 0, 0, 16, 16 });
    const auto px = compositor.result().at(5, 5);
    EXPECT_EQ(px.a, 128);
    EXPECT_EQ(px.r, 255);
}

TEST_F(CompositorTest, DirtyRectRecomputeIsIdempotent)
{
    addLayerWithPixel(5, 5, raster::kOpaqueWhite);
    compositor.composite({ 0, 0, 16, 16 });
    const auto first = compositor.result().at(5, 5);

    // Recompute just the dirty region containing the pixel, twice.
    compositor.composite({ 4, 4, 4, 4 });
    compositor.composite({ 4, 4, 4, 4 });
    EXPECT_EQ(compositor.result().at(5, 5), first);
    // And the untouched neighbor is still clean.
    EXPECT_EQ(compositor.result().at(6, 5), raster::kTransparent);
}

TEST_F(CompositorTest, OutOfBoundsDirtyRectIsClipped)
{
    addLayerWithPixel(15, 15, raster::kOpaqueWhite);
    const auto rect = compositor.composite({ 8, 8, 100, 100 });
    EXPECT_EQ(rect.x, 8);
    EXPECT_EQ(rect.y, 8);
    EXPECT_EQ(rect.width, 8);
    EXPECT_EQ(rect.height, 8);
    EXPECT_EQ(compositor.result().at(15, 15), raster::kOpaqueWhite);
}

} // namespace
