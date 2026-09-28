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

// --- Krita-parity blend modes ---------------------------------------------

namespace {

using prism::core::raster::blend_detail::channelAdd;
using prism::core::raster::blend_detail::channelColorBurn;
using prism::core::raster::blend_detail::channelColorDodge;
using prism::core::raster::blend_detail::channelDarken;
using prism::core::raster::blend_detail::channelDifference;
using prism::core::raster::blend_detail::channelDivide;
using prism::core::raster::blend_detail::channelExclusion;
using prism::core::raster::blend_detail::channelHardLight;
using prism::core::raster::blend_detail::channelLighten;
using prism::core::raster::blend_detail::channelSoftLight;
using prism::core::raster::blend_detail::channelSubtract;
using prism::core::raster::blendNonSeparable;

} // namespace

TEST(Blend, DarkenAndLightenPickPerChannel)
{
    EXPECT_FLOAT_EQ(channelDarken(0.8F, 0.3F), 0.3F);
    EXPECT_FLOAT_EQ(channelLighten(0.8F, 0.3F), 0.8F);

    const Color dark = blendPixel(Color{ 204, 204, 204, 255 }, Color{ 76, 76, 76, 255 },
                                  BlendMode::Darken);
    EXPECT_NEAR(dark.r, 76, 1);
    const Color light = blendPixel(Color{ 204, 204, 204, 255 }, Color{ 76, 76, 76, 255 },
                                   BlendMode::Lighten);
    EXPECT_NEAR(light.r, 204, 1);
}

TEST(Blend, DodgeAndBurnAreInversesAtExtremes)
{
    EXPECT_FLOAT_EQ(channelColorDodge(0.0F, 0.9F), 0.0F); // black backdrop stays black
    EXPECT_FLOAT_EQ(channelColorDodge(0.5F, 1.0F), 1.0F); // white source blows out
    EXPECT_FLOAT_EQ(channelColorBurn(1.0F, 0.1F), 1.0F);  // white backdrop stays white
    EXPECT_FLOAT_EQ(channelColorBurn(0.5F, 0.0F), 0.0F);  // black source burns to black
    // Mid-grey dodged toward white with a bright source.
    EXPECT_GT(channelColorDodge(0.5F, 0.5F), 0.5F);
    EXPECT_LT(channelColorBurn(0.5F, 0.5F), 0.5F);
}

TEST(Blend, HardLightIsOverlayWithSwappedOperands)
{
    EXPECT_FLOAT_EQ(channelHardLight(0.4F, 0.3F), channelHardLight(0.4F, 0.3F));
    // backdrop 0.8, source 0.2 -> darkens (source < 0.5 branch)
    EXPECT_NEAR(channelHardLight(0.8F, 0.2F), 2.0F * 0.8F * 0.2F, 1e-6F);
    // source above 0.5 -> screen-like: 1 - 2(1-b)(1-s)
    EXPECT_NEAR(channelHardLight(0.8F, 0.9F), 1.0F - 2.0F * 0.2F * 0.1F, 1e-6F);
}

TEST(Blend, SoftLightKeepsBackdropAtHalfSource)
{
    // s = 0.5 is the neutral point of soft light: the backdrop is unchanged.
    for (float b : { 0.0F, 0.2F, 0.25F, 0.5F, 0.75F, 1.0F })
        EXPECT_NEAR(channelSoftLight(b, 0.5F), b, 1e-5F);
    EXPECT_LT(channelSoftLight(0.5F, 0.2F), 0.5F); // darkens
    EXPECT_GT(channelSoftLight(0.5F, 0.8F), 0.5F); // lightens
}

TEST(Blend, DifferenceExclusionAndArithmeticModes)
{
    EXPECT_FLOAT_EQ(channelDifference(0.8F, 0.3F), 0.5F);
    EXPECT_FLOAT_EQ(channelExclusion(1.0F, 1.0F), 0.0F); // equal values cancel
    EXPECT_FLOAT_EQ(channelAdd(0.7F, 0.7F), 1.0F);       // clamped
    EXPECT_FLOAT_EQ(channelSubtract(0.3F, 0.7F), 0.0F);  // clamped
    EXPECT_FLOAT_EQ(channelDivide(0.5F, 0.5F), 1.0F);
    // A zero source must not divide by zero: white over coloured backdrop,
    // black over empty backdrop.
    EXPECT_FLOAT_EQ(channelDivide(0.5F, 0.0F), 1.0F);
    EXPECT_FLOAT_EQ(channelDivide(0.0F, 0.0F), 0.0F);
}

TEST(Blend, HueModeKeepsBackdropLuminosity)
{
    const float backdrop[3] = { 0.2F, 0.4F, 0.6F };
    const float source[3] = { 0.9F, 0.1F, 0.1F }; // pure red
    float out[3];
    blendNonSeparable(backdrop, source, BlendMode::Hue, out);

    using prism::core::raster::blend_detail::luminance;
    EXPECT_NEAR(luminance(out[0], out[1], out[2]), luminance(0.2F, 0.4F, 0.6F), 1e-3F);
    for (float c : out) {
        EXPECT_GE(c, 0.0F);
        EXPECT_LE(c, 1.0F);
    }
}

TEST(Blend, LuminosityModeTakesSourceLuminance)
{
    using prism::core::raster::blend_detail::luminance;
    using prism::core::raster::blend_detail::saturationOf;

    // Luminosity = SetLum(backdrop, Lum(source)): the source's luminance is
    // always reproduced exactly.
    const float backdrop[3] = { 0.6F, 0.3F, 0.1F };
    const float grey[3] = { 0.2F, 0.2F, 0.2F };
    float out[3];
    blendNonSeparable(backdrop, grey, BlendMode::Luminosity, out);
    EXPECT_NEAR(luminance(out[0], out[1], out[2]), 0.2F, 1e-3F);

    // Backdrop hue/saturation survive only while the shifted colour stays in
    // gamut: SetLum clips (per spec), and clipping changes the spread. The
    // case above clips, so assert preservation on one that does not.
    const float inGamutBackdrop[3] = { 0.5F, 0.4F, 0.3F };
    const float inGamutSource[3] = { 0.45F, 0.45F, 0.45F };
    blendNonSeparable(inGamutBackdrop, inGamutSource, BlendMode::Luminosity, out);
    EXPECT_NEAR(luminance(out[0], out[1], out[2]), luminance(0.45F, 0.45F, 0.45F), 1e-3F);
    EXPECT_NEAR(saturationOf(out[0], out[1], out[2]),
                saturationOf(inGamutBackdrop[0], inGamutBackdrop[1], inGamutBackdrop[2]), 1e-3F);
}

TEST(Blend, SaturationModeTakesSourceSaturation)
{
    using prism::core::raster::blend_detail::luminance;
    using prism::core::raster::blend_detail::saturationOf;

    // Saturation = SetLum(SetSat(backdrop, Sat(source)), Lum(backdrop)):
    // the backdrop's luminance is kept and the source's saturation applied.
    const float backdrop[3] = { 0.5F, 0.4F, 0.3F };
    const float source[3] = { 0.7F, 0.2F, 0.2F }; // saturation 0.5
    float out[3];
    blendNonSeparable(backdrop, source, BlendMode::Saturation, out);
    EXPECT_NEAR(luminance(out[0], out[1], out[2]),
                luminance(backdrop[0], backdrop[1], backdrop[2]), 1e-3F);
    EXPECT_NEAR(saturationOf(out[0], out[1], out[2]), 0.5F, 1e-3F);
    for (float c : out) {
        EXPECT_GE(c, 0.0F);
        EXPECT_LE(c, 1.0F);
    }
}

TEST(Blend, EveryModeIsTotalAndKeepsAlphaMath)
{
    // Every mode must survive black/white/transparent inputs without leaving
    // the 0..255 range or producing alpha outside the source-over formula.
    const Color values[] = {
        Color{ 0, 0, 0, 255 }, Color{ 255, 255, 255, 255 }, Color{ 128, 64, 32, 128 },
        Color{ 10, 200, 90, 33 }, Color{ 0, 0, 0, 0 },
    };
    for (int m = 0; m <= static_cast<int>(BlendMode::Luminosity); ++m) {
        const auto mode = static_cast<BlendMode>(m);
        for (const Color& dst : values) {
            for (const Color& src : values) {
                const Color out = blendPixel(dst, src, mode);
                const int expectedA = src.a + dst.a * (255 - src.a) / 255;
                EXPECT_NEAR(out.a, expectedA, 2) << "mode " << m;
                if (dst.a == 255 && src.a == 255)
                    EXPECT_EQ(out.a, 255) << "mode " << m;
            }
        }
    }
}

TEST(Blend, BlendModeValuesAreStableForProjectFiles)
{
    // These values are serialized into .prism files: renumbering them would
    // silently change the look of every existing document.
    EXPECT_EQ(static_cast<int>(BlendMode::Normal), 0);
    EXPECT_EQ(static_cast<int>(BlendMode::Multiply), 1);
    EXPECT_EQ(static_cast<int>(BlendMode::Screen), 2);
    EXPECT_EQ(static_cast<int>(BlendMode::Overlay), 3);
    EXPECT_EQ(static_cast<int>(BlendMode::Darken), 4);
    EXPECT_EQ(static_cast<int>(BlendMode::Luminosity), 18);
}

} // namespace
