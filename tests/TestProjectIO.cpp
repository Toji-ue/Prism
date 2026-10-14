#include "project/ProjectIO.h"

#include <QColor>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

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

TEST_F(ProjectIOTest, SaveLoadRoundTripsLayers) {
    Document doc(CanvasSize{64, 48, 300});
    doc.addRasterLayer("background");
    doc.addRasterLayer("lines");
    doc.layers()[0]->pixels().set(3, 4, raster::Color{10, 20, 30, 255});
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
    ASSERT_EQ(loaded->layers().size(), std::size_t{2});
    EXPECT_EQ(loaded->layers()[0]->name(), "background");
    EXPECT_EQ(loaded->layers()[0]->pixels().at(3, 4), (raster::Color{10, 20, 30, 255}));
    EXPECT_FLOAT_EQ(loaded->layers()[0]->opacity(), 0.75F);
    EXPECT_EQ(loaded->layers()[1]->blendMode(), BlendMode::Multiply);
    EXPECT_FALSE(loaded->layers()[1]->isVisible());
}

TEST_F(ProjectIOTest, LoadRejectsWrongVersion) {
    const QString file = path("bad.prism");
    QFile f(file);
    f.open(QIODevice::WriteOnly);
    f.write(R"({"version":999,"width":1,"height":1,"dpi":72,"colorProfile":"sRGB","layers":[]})");
    f.close();
    EXPECT_FALSE(ProjectIO::load(file).has_value());
}

TEST_F(ProjectIOTest, LoadRejectsInvalidCanvasWithoutThrowing) {
    const QString file = path("invalid-size.prism");
    QFile f(file);
    ASSERT_TRUE(f.open(QIODevice::WriteOnly));
    f.write(R"({"version":2,"width":0,"height":100,"dpi":72,"layers":[]})");
    f.close();
    EXPECT_FALSE(ProjectIO::load(file).has_value());
}

TEST_F(ProjectIOTest, LoadRejectsMissingFile) {
    EXPECT_FALSE(ProjectIO::load(path("nope.prism")).has_value());
}

TEST_F(ProjectIOTest, V2RoundTripsNestedLayerFlagsAndV1StillOpens) {
    Document doc(CanvasSize{16, 16, 72});
    auto child = std::make_unique<Layer>(Layer::Type::Raster, "ink");
    child->pixels() = raster::RasterBuffer(16, 16);
    child->pixels().set(2, 3, raster::Color{12, 34, 56, 255});
    child->setClipped(true);
    child->setAlphaLocked(true);
    child->setLocked(true);
    auto group = std::make_unique<Layer>(Layer::Type::Group, "pages");
    child->setParentId(group->id());
    const auto groupId = group->id();
    doc.addLayer(std::move(child));
    doc.addLayer(std::move(group));
    const QString pathV2 = path("nested.prism");
    ASSERT_TRUE(ProjectIO::save(doc, pathV2));
    const auto loaded = ProjectIO::load(pathV2);
    ASSERT_TRUE(loaded.has_value());
    ASSERT_EQ(loaded->layers().size(), 2u);
    EXPECT_EQ(loaded->layers()[0]->parentId(), groupId);
    EXPECT_TRUE(loaded->layers()[0]->isClipped());
    EXPECT_TRUE(loaded->layers()[0]->isAlphaLocked());
    EXPECT_TRUE(loaded->layers()[0]->isLocked());
    EXPECT_EQ(loaded->layers()[0]->pixels().at(2, 3), (raster::Color{12, 34, 56, 255}));

    QFile saved(pathV2);
    ASSERT_TRUE(saved.open(QIODevice::ReadOnly));
    QJsonObject root = QJsonDocument::fromJson(saved.readAll()).object();
    root["version"] = 1;
    QJsonArray layers = root["layers"].toArray();
    QJsonObject old = layers[0].toObject();
    old.remove("id");
    old.remove("parentId");
    old.remove("clipped");
    old.remove("alphaLocked");
    old.remove("locked");
    layers[0] = old;
    layers.removeAt(1);
    root["layers"] = layers;
    QFile legacy(path("legacy.prism"));
    ASSERT_TRUE(legacy.open(QIODevice::WriteOnly));
    legacy.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
    legacy.close();
    const auto v1 = ProjectIO::load(legacy.fileName());
    ASSERT_TRUE(v1.has_value());
    EXPECT_EQ(v1->layers()[0]->parentId(), 0u);
    EXPECT_FALSE(v1->layers()[0]->isClipped());
    EXPECT_EQ(v1->layers()[0]->pixels().at(2, 3), (raster::Color{12, 34, 56, 255}));
}

TEST_F(ProjectIOTest, BrushV5RoundTripsTaperAndV1ToV4KeepRoundEnds) {
    prism::canvas::BrushPreset preset;
    preset.name = "Graphite";
    preset.brush.spacing = 0.28F;
    preset.brush.pressureCurve = 1.4F;
    preset.brush.pressureSizeMin = 0.3F;
    preset.brush.stabilization = 0.2F;
    preset.brush.material = prism::canvas::BrushMaterial::Graphite;
    preset.brush.grain = 0.6F;
    preset.brush.grainScale = 1.8F;
    preset.brush.tiltShading = 0.9F;
    preset.brush.endTaperDiameters = 2.5F;
    const QString v4 = path("new.prismbrush");
    ASSERT_TRUE(ProjectIO::exportBrush(preset, v4));
    const auto loaded = ProjectIO::importBrush(v4);
    ASSERT_TRUE(loaded.has_value());
    EXPECT_FLOAT_EQ(loaded->brush.spacing, preset.brush.spacing);
    EXPECT_FLOAT_EQ(loaded->brush.pressureCurve, preset.brush.pressureCurve);
    EXPECT_FLOAT_EQ(loaded->brush.pressureSizeMin, preset.brush.pressureSizeMin);
    EXPECT_FLOAT_EQ(loaded->brush.stabilization, preset.brush.stabilization);
    EXPECT_EQ(loaded->brush.material, prism::canvas::BrushMaterial::Graphite);
    EXPECT_FLOAT_EQ(loaded->brush.grain, preset.brush.grain);
    EXPECT_FLOAT_EQ(loaded->brush.grainScale, preset.brush.grainScale);
    EXPECT_FLOAT_EQ(loaded->brush.tiltShading, preset.brush.tiltShading);
    EXPECT_FLOAT_EQ(loaded->brush.endTaperDiameters, 2.5F);

    QFile old(path("old.prismbrush"));
    ASSERT_TRUE(old.open(QIODevice::WriteOnly));
    old.write(
        R"({"format":"prism-brush","version":2,"name":"Old","radius":8,"tip":{"kind":"procedural","shape":"round"}})");
    old.close();
    const auto legacy = ProjectIO::importBrush(old.fileName());
    ASSERT_TRUE(legacy.has_value());
    EXPECT_FLOAT_EQ(legacy->brush.spacing, 0.125F);
    EXPECT_FLOAT_EQ(legacy->brush.pressureSizeMin, 0.5F);
    EXPECT_FLOAT_EQ(legacy->brush.stabilization, 0.0F);
    EXPECT_EQ(legacy->brush.material, prism::canvas::BrushMaterial::Solid);
    EXPECT_FLOAT_EQ(legacy->brush.endTaperDiameters, 0.0F);

    for (int version : {1, 3, 4}) {
        QFile oldVersion(QDir(tmp->path()).filePath(QString("v%1.prismbrush").arg(version)));
        ASSERT_TRUE(oldVersion.open(QIODevice::WriteOnly));
        oldVersion.write(QString(R"({"format":"prism-brush","version":%1,"name":"Legacy"})")
                             .arg(version)
                             .toUtf8());
        oldVersion.close();
        const auto imported = ProjectIO::importBrush(oldVersion.fileName());
        ASSERT_TRUE(imported.has_value());
        EXPECT_EQ(imported->brush.material, prism::canvas::BrushMaterial::Solid);
        EXPECT_FLOAT_EQ(imported->brush.endTaperDiameters, 0.0F);
    }
    const QString graphiteV4 = path("graphite-v4.prismbrush");
    QFile v4File(graphiteV4);
    ASSERT_TRUE(v4File.open(QIODevice::WriteOnly));
    v4File.write(
        R"({"format":"prism-brush","version":4,"name":"Old HB","material":"graphite","grain":0.7,"grainScale":1.4,"tiltShading":0.8})");
    v4File.close();
    const auto oldGraphite = ProjectIO::importBrush(graphiteV4);
    ASSERT_TRUE(oldGraphite.has_value());
    EXPECT_EQ(oldGraphite->brush.material, prism::canvas::BrushMaterial::Graphite);
    EXPECT_FLOAT_EQ(oldGraphite->brush.grain, 0.7F);
    EXPECT_FLOAT_EQ(oldGraphite->brush.endTaperDiameters, 0.0F);
}

TEST_F(ProjectIOTest, FlatPngPreservesDpiWithoutResampling) {
    Document doc(CanvasSize{32, 24, 300});
    doc.addRasterLayer("Paint");
    doc.layers()[0]->pixels().set(3, 4, raster::Color{50, 90, 130, 120});
    prism::canvas::Compositor compositor(doc);
    compositor.composite({0, 0, 32, 24});
    ASSERT_TRUE(ProjectIO::exportPng(compositor, path("dpi.png")));
    QImage image(path("dpi.png"));
    ASSERT_FALSE(image.isNull());
    EXPECT_EQ(image.width(), 32);
    EXPECT_EQ(image.height(), 24);
    EXPECT_EQ(image.dotsPerMeterX(), 11811);
    EXPECT_EQ(image.dotsPerMeterY(), 11811);
    EXPECT_EQ(image.pixelColor(3, 4), QColor(50, 90, 130, 120));
}

} // namespace
