#include "fill/FloodFill.h"

#include <gtest/gtest.h>

namespace {

using prism::canvas::floodFill;
using prism::core::raster::Color;
using prism::core::raster::RasterBuffer;

TEST(FloodFill, FillsWholeTransparentBuffer)
{
    RasterBuffer buf(8, 8);
    const auto rect = floodFill(buf, 0, 0, prism::core::raster::kOpaqueWhite, 0);
    EXPECT_EQ(rect.width, 8);
    EXPECT_EQ(rect.height, 8);
    EXPECT_EQ(buf.at(4, 4), prism::core::raster::kOpaqueWhite);
    EXPECT_EQ(buf.at(7, 7), prism::core::raster::kOpaqueWhite);
}

TEST(FloodFill, StopsAtColorBoundary)
{
    RasterBuffer buf(10, 10);
    // Vertical wall at x == 5
    for (int y = 0; y < 10; ++y)
        buf.set(5, y, Color{ 255, 0, 0, 255 });

    const auto rect = floodFill(buf, 0, 0, prism::core::raster::kOpaqueWhite, 0);
    EXPECT_EQ(rect.x, 0);
    EXPECT_EQ(rect.width, 5); // left of the wall only
    EXPECT_EQ(buf.at(4, 4), prism::core::raster::kOpaqueWhite);
    EXPECT_EQ(buf.at(5, 4), (Color{ 255, 0, 0, 255 })); // wall untouched
    EXPECT_EQ(buf.at(6, 4), prism::core::raster::kTransparent); // beyond not filled
}

TEST(FloodFill, SameColorIsNoOp)
{
    RasterBuffer buf(4, 4, prism::core::raster::kOpaqueWhite);
    EXPECT_TRUE(floodFill(buf, 1, 1, prism::core::raster::kOpaqueWhite, 0).isEmpty());
}

TEST(FloodFill, OutOfBoundsSeedIsNoOp)
{
    RasterBuffer buf(4, 4);
    EXPECT_TRUE(floodFill(buf, -1, 0, prism::core::raster::kOpaqueWhite, 0).isEmpty());
    EXPECT_TRUE(floodFill(buf, 4, 0, prism::core::raster::kOpaqueWhite, 0).isEmpty());
}

TEST(FloodFill, RespectsTolerance)
{
    // A single varied pixel does NOT block a 4-connected fill — the region
    // can walk around it. What tolerance controls is whether that pixel
    // itself gets included.
    RasterBuffer buf(6, 6, Color{ 100, 100, 100, 255 });
    buf.set(3, 3, Color{ 120, 120, 120, 255 });

    const auto tight = floodFill(buf, 0, 0, prism::core::raster::kOpaqueWhite, 0);
    EXPECT_EQ(tight.width, 6); // everything reachable
    EXPECT_EQ(buf.at(3, 3), (Color{ 120, 120, 120, 255 })); // outlier excluded
    EXPECT_EQ(buf.at(0, 0), prism::core::raster::kOpaqueWhite);

    // With tolerance 30 the outlier matches, so it fills too.
    RasterBuffer buf2(6, 6, Color{ 100, 100, 100, 255 });
    buf2.set(3, 3, Color{ 120, 120, 120, 255 });
    const auto loose = floodFill(buf2, 0, 0, prism::core::raster::kOpaqueWhite, 30);
    EXPECT_EQ(loose.width, 6);
    EXPECT_EQ(buf2.at(3, 3), prism::core::raster::kOpaqueWhite);
}

TEST(FloodFill, ReturnsDirtyRectOnly)
{
    RasterBuffer buf(20, 20);
    // An opaque 4x3 block; filling inside it must touch only that block.
    for (int y = 5; y < 8; ++y)
        for (int x = 5; x < 9; ++x)
            buf.set(x, y, Color{ 0, 0, 0, 255 });

    const auto rect = floodFill(buf, 6, 6, Color{ 0, 128, 255, 255 }, 0);
    EXPECT_EQ(rect.x, 5);
    EXPECT_EQ(rect.y, 5);
    EXPECT_EQ(rect.width, 4);
    EXPECT_EQ(rect.height, 3);
}

} // namespace
