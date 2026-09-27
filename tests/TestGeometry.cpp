#include "geometry/Types.h"

#include <gtest/gtest.h>

namespace {

using prism::core::geometry::Rect;

TEST(Geometry, RectIntersection)
{
    const Rect a{ 0, 0, 100, 100 };
    const Rect b{ 50, 50, 100, 100 };
    const Rect i = a.intersected(b);
    EXPECT_EQ(i.x, 50);
    EXPECT_EQ(i.y, 50);
    EXPECT_EQ(i.width, 50);
    EXPECT_EQ(i.height, 50);
}

TEST(Geometry, RectIntersectionDisjointIsEmpty)
{
    const Rect a{ 0, 0, 10, 10 };
    const Rect b{ 20, 20, 5, 5 };
    const Rect i = a.intersected(b);
    EXPECT_TRUE(i.isEmpty());
}

TEST(Geometry, RectIntersectionTouchingEdgesIsEmpty)
{
    const Rect a{ 0, 0, 10, 10 };
    const Rect b{ 10, 0, 10, 10 }; // shares an edge only
    EXPECT_TRUE(a.intersected(b).isEmpty());
}

} // namespace
