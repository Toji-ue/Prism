#include "commands/Commands.h"
#include "document/Document.h"
#include "history/History.h"

#include <gtest/gtest.h>

namespace {

using namespace prism::core;

class LayerCommandsTest : public ::testing::Test {
protected:
    Document doc{ CanvasSize{ 32, 32, 72 } };
    History history;

    void SetUp() override
    {
        doc.addRasterLayer("a");
        doc.addRasterLayer("b");
    }
};

TEST_F(LayerCommandsTest, RemoveLayerUndoRedoRestoresIndex)
{
    history.execute(std::make_unique<RemoveLayerCommand>(doc, 0));
    ASSERT_EQ(doc.layers().size(), std::size_t{ 1 });
    EXPECT_EQ(doc.layers()[0]->name(), "b");

    history.undo();
    ASSERT_EQ(doc.layers().size(), std::size_t{ 2 });
    EXPECT_EQ(doc.layers()[0]->name(), "a");
}

TEST_F(LayerCommandsTest, MoveLayerRoundTrip)
{
    history.execute(std::make_unique<MoveLayerCommand>(doc, 0, 1));
    EXPECT_EQ(doc.layers()[1]->name(), "a"); // moved to top

    history.undo();
    EXPECT_EQ(doc.layers()[0]->name(), "a");
}

TEST_F(LayerCommandsTest, DuplicateCopiesPixelsAndName)
{
    doc.layers()[0]->pixels().set(5, 5, raster::kOpaqueWhite);
    history.execute(std::make_unique<DuplicateLayerCommand>(doc, 0));

    ASSERT_EQ(doc.layers().size(), std::size_t{ 3 });
    EXPECT_EQ(doc.layers()[2]->name(), "a");
    EXPECT_EQ(doc.layers()[2]->pixels().at(5, 5), raster::kOpaqueWhite);

    history.undo();
    EXPECT_EQ(doc.layers().size(), std::size_t{ 2 });
}

TEST_F(LayerCommandsTest, SetPropertyUndoRestores)
{
    history.execute(std::make_unique<SetLayerPropertyCommand>(
        doc, 0, SetLayerPropertyCommand::Property::Opacity, 0.5F, 1.0F));
    EXPECT_FLOAT_EQ(doc.layers()[0]->opacity(), 0.5F);

    history.execute(std::make_unique<SetLayerPropertyCommand>(
        doc, 0, SetLayerPropertyCommand::Property::Visible, false, true));
    EXPECT_FALSE(doc.layers()[0]->isVisible());

    history.undo();
    EXPECT_TRUE(doc.layers()[0]->isVisible());
    history.undo();
    EXPECT_FLOAT_EQ(doc.layers()[0]->opacity(), 1.0F);
}

TEST_F(LayerCommandsTest, ClearLayerUndoRestoresPixels)
{
    doc.layers()[0]->pixels().set(5, 5, raster::kOpaqueWhite);
    history.execute(std::make_unique<ClearLayerCommand>(doc, 0));
    EXPECT_EQ(doc.layers()[0]->pixels().at(5, 5), raster::kTransparent);

    history.undo();
    EXPECT_EQ(doc.layers()[0]->pixels().at(5, 5), raster::kOpaqueWhite);
}

} // namespace
