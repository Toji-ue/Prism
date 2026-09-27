#include "commands/Commands.h"
#include "document/Document.h"
#include "history/History.h"
#include "layer/Layer.h"
#include "selection/Selection.h"

#include <gtest/gtest.h>

namespace {

using namespace prism::core;
using prism::canvas::clipToSelection;
using prism::canvas::Selection;
using prism::core::geometry::Point;
using prism::core::geometry::Rect;
using prism::core::raster::kOpaqueBlack;
using prism::core::raster::kOpaqueWhite;
using prism::core::raster::kTransparent;
using prism::core::raster::RasterBuffer;

// ------------------------------------------------------------------ masks

TEST(SelectionMask, RectSelectsOnlyThatArea)
{
    Selection sel(16, 16);
    sel.selectRect(Rect{ 4, 4, 4, 4 });

    EXPECT_FALSE(sel.isEmpty());
    EXPECT_EQ(sel.coverageAt(5, 5), 255);
    EXPECT_EQ(sel.coverageAt(2, 2), 0);
    EXPECT_EQ(sel.bounds().x, 4);
    EXPECT_EQ(sel.bounds().width, 4);
}

TEST(SelectionMask, SelectAllThenClear)
{
    Selection sel(8, 8);
    sel.selectAll();
    EXPECT_EQ(sel.coverageAt(0, 0), 255);
    EXPECT_EQ(sel.bounds().width, 8);

    sel.clear();
    EXPECT_TRUE(sel.isEmpty());
    EXPECT_EQ(sel.coverageAt(0, 0), 0);
}

TEST(SelectionMask, InvertFlipsCoverage)
{
    Selection sel(10, 10);
    sel.selectRect(Rect{ 0, 0, 5, 10 });
    sel.invert();

    EXPECT_EQ(sel.coverageAt(1, 1), 0); // was selected
    EXPECT_EQ(sel.coverageAt(7, 1), 255); // now selected
    EXPECT_EQ(sel.bounds().x, 5);
}

TEST(SelectionMask, InvertOfEmptySelectsAll)
{
    Selection sel(6, 6);
    sel.invert();
    EXPECT_FALSE(sel.isEmpty());
    EXPECT_EQ(sel.coverageAt(3, 3), 255);
}

TEST(SelectionMask, LassoPolygonSelectsInside)
{
    Selection sel(20, 20);
    // Triangle with vertices (2,2), (16,2), (2,16)
    sel.selectPolygon({ Point{ 2, 2 }, Point{ 16, 2 }, Point{ 2, 16 } });

    EXPECT_FALSE(sel.isEmpty());
    EXPECT_EQ(sel.coverageAt(4, 4), 255); // inside
    EXPECT_EQ(sel.coverageAt(14, 14), 0); // outside the hypotenuse
    EXPECT_EQ(sel.coverageAt(1, 1), 0); // outside the corner
}

TEST(SelectionMask, LassoWithTooFewPointsIsEmpty)
{
    Selection sel(10, 10);
    sel.selectPolygon({ Point{ 1, 1 }, Point{ 5, 5 } });
    EXPECT_TRUE(sel.isEmpty());
}

TEST(SelectionMask, MagicWandSelectsConnectedRegion)
{
    // Two separate red squares on a transparent background: the wand must
    // pick only the one it was clicked on.
    RasterBuffer source(20, 20);
    for (int y = 2; y < 6; ++y)
        for (int x = 2; x < 6; ++x)
            source.set(x, y, kOpaqueWhite);
    for (int y = 12; y < 16; ++y)
        for (int x = 12; x < 16; ++x)
            source.set(x, y, kOpaqueWhite);

    Selection sel(20, 20);
    sel.selectSimilar(source, 3, 3, 0);

    EXPECT_FALSE(sel.isEmpty());
    EXPECT_EQ(sel.coverageAt(3, 3), 255); // clicked square
    EXPECT_EQ(sel.coverageAt(13, 13), 0); // other square untouched
    EXPECT_EQ(sel.bounds().x, 2);
    EXPECT_EQ(sel.bounds().width, 4);
}

// ------------------------------------------------------------- clipping

TEST(SelectionClip, ClearsScratchOutsideSelection)
{
    Selection sel(16, 16);
    sel.selectRect(Rect{ 4, 4, 4, 4 });

    RasterBuffer scratch(16, 16, kOpaqueWhite);
    clipToSelection(scratch, sel);

    EXPECT_EQ(scratch.at(5, 5), kOpaqueWhite);
    EXPECT_EQ(scratch.at(2, 2), kTransparent);
}

TEST(SelectionClip, EmptySelectionIsNoConstraint)
{
    Selection sel(16, 16);
    RasterBuffer scratch(16, 16, kOpaqueWhite);
    clipToSelection(scratch, sel);
    EXPECT_EQ(scratch.at(0, 0), kOpaqueWhite);
}

TEST(SelectionClip, RestoresOutsidePixelsFromSource)
{
    Selection sel(16, 16);
    sel.selectRect(Rect{ 4, 4, 4, 4 });

    RasterBuffer original(16, 16, kTransparent);
    original.set(1, 1, kOpaqueBlack);
    RasterBuffer edited = original;
    edited.fill(kOpaqueWhite); // simulate an unbounded fill

    clipToSelection(edited, sel, &original);

    EXPECT_EQ(edited.at(5, 5), kOpaqueWhite); // the fill inside survives
    EXPECT_EQ(edited.at(1, 1), kOpaqueBlack); // outside restored
    EXPECT_EQ(edited.at(10, 10), kTransparent);
}

TEST(SelectionClip, LassoShapeIsRespected)
{
    Selection sel(20, 20);
    sel.selectPolygon({ Point{ 2, 2 }, Point{ 16, 2 }, Point{ 2, 16 } });

    RasterBuffer scratch(20, 20, kOpaqueWhite);
    clipToSelection(scratch, sel);

    EXPECT_EQ(scratch.at(4, 4), kOpaqueWhite); // inside triangle
    EXPECT_EQ(scratch.at(14, 14), kTransparent); // outside
}

// ---------------------------------------------------------- layer move

class MoveTest : public ::testing::Test {
protected:
    Document doc{ CanvasSize{ 16, 16, 72 } };
    History history;

    void SetUp() override { doc.addRasterLayer("paint"); }
    Layer& layer() { return *doc.layers().at(0); }
};

TEST_F(MoveTest, TranslatesAndUndoes)
{
    layer().pixels().set(2, 3, kOpaqueWhite);
    history.execute(std::make_unique<MoveLayerContentCommand>(doc, 0, 5, 4));

    EXPECT_EQ(layer().pixels().at(7, 7), kOpaqueWhite);
    EXPECT_EQ(layer().pixels().at(2, 3), kTransparent);

    history.undo();
    EXPECT_EQ(layer().pixels().at(2, 3), kOpaqueWhite);
    EXPECT_EQ(layer().pixels().at(7, 7), kTransparent);
}

TEST_F(MoveTest, ClipsAtEdge)
{
    layer().pixels().set(15, 15, kOpaqueWhite);
    history.execute(std::make_unique<MoveLayerContentCommand>(doc, 0, 4, 4));

    EXPECT_EQ(layer().pixels().at(0, 0), kTransparent);
    history.undo();
    EXPECT_EQ(layer().pixels().at(15, 15), kOpaqueWhite);
}

TEST_F(MoveTest, LockedFlagRoundTrips)
{
    history.execute(std::make_unique<SetLayerPropertyCommand>(
        doc, 0, SetLayerPropertyCommand::Property::Locked, true, false));
    EXPECT_TRUE(layer().isLocked());
    history.undo();
    EXPECT_FALSE(layer().isLocked());
}

} // namespace
