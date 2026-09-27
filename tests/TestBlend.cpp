#include "raster/Blend.h"

#include <gtest/gtest.h>

namespace {

using prism::core::BlendMode;
using prism::core::raster::Color;
using prism::core::raster::blendPixel;

TEST(Blend, TransparentSourceLeavesDestination)
{
    const Color dst{ 10, 20, 30, 255 };
    EXPECT_EQ(blendPixel(dst, Color{ 200, 0, 0, 0 }, BlendMode::Normal), dst);
}

TEST(Blend, NormalOpaqueSourceReplaces)
{
    const Color dst{ 10, 20, 30, 255 };
    const Color src{ 200, 100, 50, 255 };
    const Color out = blendPixel(dst, src, BlendMode::Normal);
    EXPECT_EQ(out, src);
}

TEST(Blend, NormalHalfAlphaIsMidpoint)
{
    const Color dst{ 0, 0, 0, 255 };
    const Color src{ 255, 255, 255, 128 }; // ~50% alpha
    const Color out = blendPixel(dst, src, BlendMode::Normal);
    EXPECT_NEAR(out.r, 128, 1);
    EXPECT_NEAR(out.g, 128, 1);
    EXPECT_NEAR(out.b, 128, 1);
    EXPECT_EQ(out.a, 255);
}

TEST(Blend, MultiplyDarkens)
{
    const Color dst{ 255, 255, 255, 255 };
    const Color src{ 128, 128, 128, 255 };
    const Color out = blendPixel(dst, src, BlendMode::Multiply);
    EXPECT_NEAR(out.r, 128, 1);
    EXPECT_NEAR(out.g, 128, 1);
    EXPECT_NEAR(out.b, 128, 1);
}

TEST(Blend, ScreenLightens)
{
    const Color dst{ 0, 0, 0, 255 };
    const Color src{ 128, 128, 128, 255 };
    const Color out = blendPixel(dst, src, BlendMode::Screen);
    // screen(0, 0.5) = 0.5
    EXPECT_NEAR(out.r, 128, 1);
    // screen over itself brightens: screen(0.5, 0.5) = 0.75
    const Color out2 = blendPixel(Color{ 128, 128, 128, 255 }, src, BlendMode::Screen);
    EXPECT_NEAR(out2.r, 191, 1);
}

TEST(Blend, OverlayPreservesExtremes)
{
    // overlay(anything, white) on dark base => brighter; on white base stays white
    const Color white = blendPixel(Color{ 255, 255, 255, 255 },
                                   Color{ 255, 255, 255, 255 }, BlendMode::Overlay);
    EXPECT_NEAR(white.r, 255, 1);
    const Color black = blendPixel(Color{ 0, 0, 0, 255 }, Color{ 0, 0, 0, 255 },
                                   BlendMode::Overlay);
    EXPECT_EQ(black.r, 0);
}

} // namespace
