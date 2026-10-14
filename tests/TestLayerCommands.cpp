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

TEST_F(LayerCommandsTest, AddLayerWithPixelsUndoRedoKeepsImage)
{
    auto imported = std::make_unique<Layer>(Layer::Type::Raster, "photo");
    imported->pixels() = raster::RasterBuffer(32, 32);
    imported->pixels().set(10, 10, raster::kOpaqueWhite);
    history.execute(
        std::make_unique<AddLayerCommand>(doc, "photo", std::move(imported)));

    ASSERT_EQ(doc.layers().size(), std::size_t{ 3 });
    EXPECT_EQ(doc.layers()[2]->name(), "photo");
    EXPECT_EQ(doc.layers()[2]->pixels().at(10, 10), raster::kOpaqueWhite);

    history.undo();
    ASSERT_EQ(doc.layers().size(), std::size_t{ 2 });

    history.redo();
    ASSERT_EQ(doc.layers().size(), std::size_t{ 3 });
    EXPECT_EQ(doc.layers()[2]->pixels().at(10, 10), raster::kOpaqueWhite);
}

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

TEST_F(LayerCommandsTest, GroupAndDeleteRoundTripParentLinks) {
    history.execute(std::make_unique<GroupLayerCommand>(doc, 0, "group"));
    ASSERT_EQ(doc.layers().size(), 3u);
    const auto groupId = doc.layers()[1]->id();
    EXPECT_EQ(doc.layers()[0]->parentId(), groupId);
    history.execute(std::make_unique<SetLayerParentCommand>(doc, 2, groupId));
    EXPECT_EQ(doc.layers()[2]->parentId(), groupId);
    history.execute(std::make_unique<RemoveLayerCommand>(doc, 1));
    EXPECT_EQ(doc.layers()[0]->parentId(), 0u);
    EXPECT_EQ(doc.layers()[1]->parentId(), 0u);
    history.undo();
    EXPECT_EQ(doc.layers()[0]->parentId(), groupId);
    EXPECT_EQ(doc.layers()[2]->parentId(), groupId);
    history.undo();
    EXPECT_EQ(doc.layers()[2]->parentId(), 0u);
    history.undo();
    EXPECT_EQ(doc.layers().size(), 2u);
    EXPECT_EQ(doc.layers()[0]->parentId(), 0u);
}

TEST_F(LayerCommandsTest, DuplicateGroupCopiesNestedChildrenAndRedoKeepsLinks) {
    history.execute(std::make_unique<GroupLayerCommand>(doc, 0, "inner"));
    history.execute(std::make_unique<GroupLayerCommand>(doc, 1, "outer"));
    const auto outerId = doc.layers()[2]->id();
    const auto innerId = doc.layers()[1]->id();
    ASSERT_EQ(doc.layers()[0]->parentId(), innerId);
    ASSERT_EQ(doc.layers()[1]->parentId(), outerId);

    history.execute(std::make_unique<DuplicateLayerCommand>(doc, 2));
    ASSERT_EQ(doc.layers().size(), 7u);
    const auto copiedOuterId = doc.layers()[4]->id();
    const auto copiedInnerId = doc.layers()[6]->id();
    EXPECT_NE(copiedOuterId, outerId);
    EXPECT_EQ(doc.layers()[6]->parentId(), copiedOuterId);
    EXPECT_EQ(doc.layers()[5]->parentId(), copiedInnerId);

    history.undo();
    ASSERT_EQ(doc.layers().size(), 4u);
    history.redo();
    ASSERT_EQ(doc.layers().size(), 7u);
    EXPECT_EQ(doc.layers()[6]->id(), copiedInnerId);
    EXPECT_EQ(doc.layers()[6]->parentId(), copiedOuterId);
    EXPECT_EQ(doc.layers()[5]->parentId(), copiedInnerId);
}

TEST_F(LayerCommandsTest, ResizeAfterCropUsesVisibleOriginAndUndoes) {
    doc.layers()[0]->pixels().set(7, 6, raster::Color{2, 3, 4, 255});
    history.execute(std::make_unique<CropCommand>(doc, 5, 4, 20, 20));
    history.execute(std::make_unique<ResizeCanvasCommand>(doc, CanvasSize{24, 24, 72}));
    EXPECT_EQ(doc.canvasOrigin().x, 0);
    EXPECT_EQ(doc.layers()[0]->pixels().at(2, 2), (raster::Color{2, 3, 4, 255}));
    history.undo();
    EXPECT_EQ(doc.canvasOrigin().x, 5);
    EXPECT_EQ(doc.layers()[0]->pixels().at(7, 6), (raster::Color{2, 3, 4, 255}));
    history.redo();
    EXPECT_EQ(doc.layers()[0]->pixels().at(2, 2), (raster::Color{2, 3, 4, 255}));
}

TEST_F(LayerCommandsTest, AlphaLockPreservesCoverageOfStroke) {
    auto& layer = *doc.layers()[0];
    layer.setAlphaLocked(true);
    layer.pixels().set(5, 5, raster::Color{20, 30, 40, 100});
    raster::RasterBuffer painted(2, 1);
    painted.set(0, 0, raster::Color{255, 0, 0, 255});
    painted.set(1, 0, raster::Color{255, 0, 0, 255});
    history.execute(std::make_unique<StrokeCommand>(doc, 0, painted, 5, 5));
    EXPECT_EQ(layer.pixels().at(5, 5).a, 100);
    EXPECT_EQ(layer.pixels().at(6, 5).a, 0);
    history.undo();
    EXPECT_EQ(layer.pixels().at(5, 5), (raster::Color{20, 30, 40, 100}));
}

} // namespace
