#include "document/Document.h"
#include "layer/Layer.h"

#include <gtest/gtest.h>

#include <stdexcept>

namespace {

using prism::core::CanvasSize;
using prism::core::Document;
using prism::core::Layer;

TEST(Document, RejectsInvalidCanvasSize)
{
    EXPECT_THROW(Document(CanvasSize{ 0, 100 }), std::invalid_argument);
    EXPECT_THROW(Document(CanvasSize{ 100, -5 }), std::invalid_argument);
}

TEST(Document, DefaultsAndResize)
{
    Document doc(CanvasSize{ 1920, 1080, 300 });
    EXPECT_EQ(doc.canvasSize().width, 1920);
    EXPECT_EQ(doc.canvasSize().height, 1080);
    EXPECT_EQ(doc.canvasSize().dpi, 300);
    EXPECT_EQ(doc.colorProfile(), "sRGB");

    doc.resizeCanvas({ 800, 600, 72 });
    EXPECT_EQ(doc.canvasSize().width, 800);
    EXPECT_THROW(doc.resizeCanvas({ 0, 0 }), std::invalid_argument);
}

TEST(Document, AddRasterLayerAppendsInOrder)
{
    Document doc(CanvasSize{ 64, 64 });
    doc.addRasterLayer("background");
    doc.addRasterLayer("lines");

    ASSERT_EQ(doc.layers().size(), std::size_t{ 2 });
    EXPECT_EQ(doc.layers()[0]->name(), "background"); // bottom
    EXPECT_EQ(doc.layers()[1]->name(), "lines"); // top
    EXPECT_EQ(doc.layers()[0]->type(), Layer::Type::Raster);
}

TEST(Document, MoveLayerBothDirections)
{
    Document doc(CanvasSize{ 64, 64 });
    doc.addRasterLayer("a");
    doc.addRasterLayer("b");
    doc.addRasterLayer("c");

    EXPECT_TRUE(doc.moveLayer(0, 2)); // a b c -> b c a
    EXPECT_EQ(doc.layers()[0]->name(), "b");
    EXPECT_EQ(doc.layers()[2]->name(), "a");

    EXPECT_TRUE(doc.moveLayer(2, 1)); // b c a -> b a c
    EXPECT_EQ(doc.layers()[1]->name(), "a");

    EXPECT_FALSE(doc.moveLayer(0, 3));
    EXPECT_FALSE(doc.moveLayer(3, 0));
}

TEST(Document, TakeLayer)
{
    Document doc(CanvasSize{ 64, 64 });
    doc.addRasterLayer("only");

    auto layer = doc.takeLayer(0);
    ASSERT_NE(layer, nullptr);
    EXPECT_EQ(layer->name(), "only");
    EXPECT_TRUE(doc.layers().empty());
    EXPECT_EQ(doc.takeLayer(0), nullptr);
}

TEST(Layer, OpacityClampsAndFlagsDefault)
{
    Layer layer(Layer::Type::Raster, "sketch");
    layer.setOpacity(1.5F);
    EXPECT_FLOAT_EQ(layer.opacity(), 1.0F);
    layer.setOpacity(-0.25F);
    EXPECT_FLOAT_EQ(layer.opacity(), 0.0F);

    EXPECT_TRUE(layer.isVisible());
    EXPECT_FALSE(layer.isLocked());
    EXPECT_EQ(layer.blendMode(), prism::core::BlendMode::Normal);
}

} // namespace
