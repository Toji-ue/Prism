#include "commands/Commands.h"
#include "document/Document.h"
#include "filters/Filters.h"
#include "history/History.h"
#include "layer/Layer.h"

#include <gtest/gtest.h>

namespace {

using namespace prism::core;
using prism::canvas::filters::brightnessContrast;
using prism::canvas::filters::desaturate;
using prism::canvas::filters::gaussianBlurApprox;
using prism::canvas::filters::invert;

class FilterTest : public ::testing::Test {
protected:
    Document doc{ CanvasSize{ 16, 16, 72 } };
    History history;

    void SetUp() override { doc.addRasterLayer("bottom"); }
    Layer& layer() { return *doc.layers().at(0); }
};

TEST_F(FilterTest, InvertFlipsRgbAndKeepsAlpha)
{
    layer().pixels().set(1, 1, raster::Color{ 10, 20, 30, 255 });
    invert(layer().pixels());

    const auto c = layer().pixels().at(1, 1);
    EXPECT_EQ(c.r, 245);
    EXPECT_EQ(c.g, 235);
    EXPECT_EQ(c.b, 225);
    EXPECT_EQ(c.a, 255);
}

TEST_F(FilterTest, InvertLeavesTransparentPixelsAlone)
{
    invert(layer().pixels());
    EXPECT_EQ(layer().pixels().at(5, 5), raster::kTransparent);
}

TEST_F(FilterTest, DesaturateProducesGray)
{
    layer().pixels().set(2, 2, raster::Color{ 255, 0, 0, 255 });
    desaturate(layer().pixels());

    const auto c = layer().pixels().at(2, 2);
    EXPECT_EQ(c.r, c.g);
    EXPECT_EQ(c.g, c.b);
    EXPECT_LT(c.r, 255); // pure red is not white after desaturation
    EXPECT_GT(c.r, 0);
}

TEST_F(FilterTest, BlurSpreadsPaintToNeighbours)
{
    layer().pixels().set(8, 8, raster::kOpaqueWhite);
    gaussianBlurApprox(layer().pixels(), 2);

    // The single dot must bleed into its neighbours, and stay non-empty.
    EXPECT_GT(layer().pixels().at(9, 8).a, 0);
    EXPECT_GT(layer().pixels().at(8, 8).a, 0);
    // Far away stays untouched.
    EXPECT_EQ(layer().pixels().at(0, 0).a, 0);
}

TEST_F(FilterTest, BlurWithZeroRadiusIsNoOp)
{
    layer().pixels().set(3, 3, raster::kOpaqueWhite);
    EXPECT_TRUE(gaussianBlurApprox(layer().pixels(), 0).isEmpty());
    EXPECT_EQ(layer().pixels().at(3, 3), raster::kOpaqueWhite);
}

TEST_F(FilterTest, BrightnessContrastIdentityKeepsValues)
{
    layer().pixels().set(4, 4, raster::Color{ 100, 150, 200, 255 });
    brightnessContrast(layer().pixels(), 1.0F, 1.0F);
    const auto c = layer().pixels().at(4, 4);
    EXPECT_NEAR(c.r, 100, 1);
    EXPECT_NEAR(c.g, 150, 1);
    EXPECT_NEAR(c.b, 200, 1);
}

// ------------------------------------------------------------- merge down

TEST_F(FilterTest, MergeDownCompositesUpperOntoLower)
{
    doc.addRasterLayer("top");
    layer().pixels().set(5, 5, raster::Color{ 255, 0, 0, 255 });
    doc.layers().at(1)->pixels().set(6, 6, raster::Color{ 0, 0, 255, 255 });

    history.execute(std::make_unique<MergeLayerDownCommand>(doc, 1));

    ASSERT_EQ(doc.layers().size(), std::size_t{ 1 });
    EXPECT_EQ(doc.layers().at(0)->pixels().at(5, 5), (raster::Color{ 255, 0, 0, 255 }));
    EXPECT_EQ(doc.layers().at(0)->pixels().at(6, 6), (raster::Color{ 0, 0, 255, 255 }));
}

TEST_F(FilterTest, MergeDownUndoRestoresBothLayers)
{
    doc.addRasterLayer("top");
    layer().pixels().set(5, 5, raster::kOpaqueWhite);
    doc.layers().at(1)->pixels().set(9, 9, raster::kOpaqueBlack);

    history.execute(std::make_unique<MergeLayerDownCommand>(doc, 1));
    history.undo();

    ASSERT_EQ(doc.layers().size(), std::size_t{ 2 });
    EXPECT_EQ(doc.layers().at(0)->name(), "bottom");
    EXPECT_EQ(doc.layers().at(1)->name(), "top");
    EXPECT_EQ(doc.layers().at(0)->pixels().at(5, 5), raster::kOpaqueWhite);
    EXPECT_EQ(doc.layers().at(1)->pixels().at(9, 9), raster::kOpaqueBlack);
    EXPECT_EQ(doc.layers().at(0)->pixels().at(9, 9), raster::kTransparent);
}

TEST_F(FilterTest, MergeDownHonoursUpperOpacity)
{
    doc.addRasterLayer("top");
    doc.layers().at(1)->setOpacity(0.5F);
    doc.layers().at(1)->pixels().set(3, 3, raster::kOpaqueWhite);

    history.execute(std::make_unique<MergeLayerDownCommand>(doc, 1));
    // Half-opacity white over transparent => alpha ~128, not 255.
    EXPECT_NEAR(doc.layers().at(0)->pixels().at(3, 3).a, 128, 2);
}

} // namespace
