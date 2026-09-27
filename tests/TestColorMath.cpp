#include "raster/ColorMath.h"

#include <gtest/gtest.h>

namespace {

using prism::core::raster::Hsv;
using prism::core::raster::Rgb;
using prism::core::raster::hsvToRgb;
using prism::core::raster::rgbToHsv;

TEST(ColorMath, RedRoundTrips)
{
    const Hsv hsv = rgbToHsv({ 1.0F, 0.0F, 0.0F });
    EXPECT_NEAR(hsv.h, 0.0F, 0.001F);
    EXPECT_NEAR(hsv.s, 1.0F, 0.001F);
    EXPECT_NEAR(hsv.v, 1.0F, 0.001F);
    const Rgb rgb = hsvToRgb(hsv);
    EXPECT_NEAR(rgb.r, 1.0F, 0.001F);
    EXPECT_NEAR(rgb.g, 0.0F, 0.001F);
    EXPECT_NEAR(rgb.b, 0.0F, 0.001F);
}

TEST(ColorMath, GrayHasZeroSaturation)
{
    const Hsv hsv = rgbToHsv({ 0.5F, 0.5F, 0.5F });
    EXPECT_NEAR(hsv.s, 0.0F, 0.001F);
    EXPECT_NEAR(hsv.v, 0.5F, 0.001F);
}

TEST(ColorMath, HsvRoundTripMidHue)
{
    const Rgb in{ 0.2F, 0.7F, 0.4F };
    const Rgb out = hsvToRgb(rgbToHsv(in));
    EXPECT_NEAR(out.r, in.r, 0.001F);
    EXPECT_NEAR(out.g, in.g, 0.001F);
    EXPECT_NEAR(out.b, in.b, 0.001F);
}

TEST(ColorMath, HueWraps)
{
    const Rgb a = hsvToRgb({ 0.0F, 1.0F, 1.0F });
    const Rgb b = hsvToRgb({ 1.0F, 1.0F, 1.0F }); // 360° == 0°
    EXPECT_NEAR(a.r, b.r, 0.001F);
    EXPECT_NEAR(a.g, b.g, 0.001F);
    EXPECT_NEAR(a.b, b.b, 0.001F);
}

} // namespace
