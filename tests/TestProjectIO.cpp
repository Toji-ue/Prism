#include "project/ProjectIO.h"

#include <QTemporaryDir>
#include <QDir>

#include <gtest/gtest.h>

namespace {

using namespace prism::core;
using prism::io::ProjectIO;

class ProjectIOTest : public ::testing::Test {
protected:
    void SetUp() override { tmp = std::make_unique<QTemporaryDir>(); }
    QString path(const char* name) const { return QDir(tmp->path()).filePath(name); }

    std::unique_ptr<QTemporaryDir> tmp;
};

TEST_F(ProjectIOTest, SaveLoadRoundTripsLayers)
{
    Document doc(CanvasSize{ 64, 48, 300 });
    doc.addRasterLayer("background");
    doc.addRasterLayer("lines");
    doc.layers()[0]->pixels().set(3, 4, raster::Color{ 10, 20, 30, 255 });
    doc.layers()[0]->setOpacity(0.75F);
    doc.layers()[1]->setBlendMode(BlendMode::Multiply);
    doc.layers()[1]->setVisible(false);

    const QString file = path("test.prism");
    ASSERT_TRUE(ProjectIO::save(doc, file));

    auto loaded = ProjectIO::load(file);
    ASSERT_TRUE(loaded.has_value());
    EXPECT_EQ(loaded->canvasSize().width, 64);
    EXPECT_EQ(loaded->canvasSize().height, 48);
    EXPECT_EQ(loaded->canvasSize().dpi, 300);
    ASSERT_EQ(loaded->layers().size(), std::size_t{ 2 });
    EXPECT_EQ(loaded->layers()[0]->name(), "background");
    EXPECT_EQ(loaded->layers()[0]->pixels().at(3, 4), (raster::Color{ 10, 20, 30, 255 }));
    EXPECT_FLOAT_EQ(loaded->layers()[0]->opacity(), 0.75F);
    EXPECT_EQ(loaded->layers()[1]->blendMode(), BlendMode::Multiply);
    EXPECT_FALSE(loaded->layers()[1]->isVisible());
}

TEST_F(ProjectIOTest, LoadRejectsWrongVersion)
{
    const QString file = path("bad.prism");
    QFile f(file);
    f.open(QIODevice::WriteOnly);
    f.write(R"({"version":999,"width":1,"height":1,"dpi":72,"colorProfile":"sRGB","layers":[]})");
    f.close();
    EXPECT_FALSE(ProjectIO::load(file).has_value());
}

TEST_F(ProjectIOTest, LoadRejectsMissingFile)
{
    EXPECT_FALSE(ProjectIO::load(path("nope.prism")).has_value());
}

} // namespace
