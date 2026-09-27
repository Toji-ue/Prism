#include "commands/Commands.h"
#include "document/Document.h"
#include "history/History.h"
#include "layer/Layer.h"
#include "selection/Selection.h"

#include <gtest/gtest.h>

namespace {

using namespace prism::core;
using prism::canvas::clipToSelection;

class SelectionTest : public ::testing::Test {
protected:
    Document doc{ CanvasSize{ 16, 16, 72 } };
    History history;

    void SetUp() override { doc.addRasterLayer("paint"); }
    Layer& layer() { return *doc.layers().at(0); }
};

TEST_F(SelectionTest, ClipsScratchBufferOutsideSelection)
{
    prism::core::raster::RasterBuffer scratch(16, 16, prism::core::raster::kOpaqueWhite);
    const prism::core::geometry::Rect sel{ 4, 4, 4, 4 };
    clipToSelection(scratch, sel);

    EXPECT_EQ(scratch.at(5, 5), prism::core::raster::kOpaqueWhite); // inside kept
    EXPECT_EQ(scratch.at(2, 2), prism::core::raster::kTransparent); // outside cleared
}

TEST_F(SelectionTest, EmptySelectionIsNoConstraint)
{
    prism::core::raster::RasterBuffer scratch(16, 16, prism::core::raster::kOpaqueWhite);
    clipToSelection(scratch, {});
    EXPECT_EQ(scratch.at(0, 0), prism::core::raster::kOpaqueWhite);
}

TEST_F(SelectionTest, RestoresOutsidePixelsFromSource)
{
    prism::core::raster::RasterBuffer original(16, 16, prism::core::raster::kTransparent);
    original.set(1, 1, prism::core::raster::kOpaqueBlack);
    prism::core::raster::RasterBuffer edited = original;
    edited.fill(prism::core::raster::kOpaqueWhite); // simulate an unbounded fill

    clipToSelection(edited, prism::core::geometry::Rect{ 4, 4, 4, 4 }, &original);

    EXPECT_EQ(edited.at(5, 5), prism::core::raster::kOpaqueWhite); // inside the fill stays
    EXPECT_EQ(edited.at(1, 1), prism::core::raster::kOpaqueBlack); // outside restored
    EXPECT_EQ(edited.at(10, 10), prism::core::raster::kTransparent); // restored too
}

TEST_F(SelectionTest, MoveLayerContentTranslatesAndUndoes)
{
    layer().pixels().set(2, 3, prism::core::raster::kOpaqueWhite);
    history.execute(std::make_unique<MoveLayerContentCommand>(doc, 0, 5, 4));

    EXPECT_EQ(layer().pixels().at(7, 7), prism::core::raster::kOpaqueWhite);
    EXPECT_EQ(layer().pixels().at(2, 3), prism::core::raster::kTransparent);

    history.undo();
    EXPECT_EQ(layer().pixels().at(2, 3), prism::core::raster::kOpaqueWhite);
    EXPECT_EQ(layer().pixels().at(7, 7), prism::core::raster::kTransparent);
}

TEST_F(SelectionTest, MoveLayerContentClipsAtEdge)
{
    layer().pixels().set(15, 15, prism::core::raster::kOpaqueWhite);
    history.execute(std::make_unique<MoveLayerContentCommand>(doc, 0, 4, 4));

    // Pushed off the canvas: content is gone, no crash, no wraparound.
    EXPECT_EQ(layer().pixels().at(0, 0), prism::core::raster::kTransparent);
    history.undo();
    EXPECT_EQ(layer().pixels().at(15, 15), prism::core::raster::kOpaqueWhite);
}

TEST_F(SelectionTest, LockedFlagRoundTrips)
{
    history.execute(std::make_unique<SetLayerPropertyCommand>(
        doc, 0, SetLayerPropertyCommand::Property::Locked, true, false));
    EXPECT_TRUE(layer().isLocked());
    history.undo();
    EXPECT_FALSE(layer().isLocked());
}

} // namespace
