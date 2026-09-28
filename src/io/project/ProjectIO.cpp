#include "project/ProjectIO.h"

#include "compositor/Compositor.h"
#include "layer/Layer.h"

#include <QByteArray>
#include <QBuffer>
#include <QFile>
#include <QImage>
#include <QImageReader>
#include <QImageWriter>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

#include <cstring>

namespace prism::io {

using prism::core::CanvasSize;
using prism::core::Document;
using prism::core::Layer;

namespace {

/// Raster RGBA buffer <-> PNG bytes, straight alpha preserved.
QByteArray encodePng(const prism::core::raster::RasterBuffer& buf)
{
    const QImage image(buf.data(), buf.width(), buf.height(), buf.width() * 4,
                       QImage::Format_RGBA8888);
    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    image.copy().save(&buffer, "PNG");
    return bytes;
}

std::optional<prism::core::raster::RasterBuffer> decodePng(const QByteArray& bytes)
{
    QImage image;
    if (!image.loadFromData(bytes, "PNG"))
        return std::nullopt;
    image = image.convertToFormat(QImage::Format_RGBA8888);
    prism::core::raster::RasterBuffer buf(image.width(), image.height());
    std::memcpy(buf.data(), image.constBits(),
                static_cast<std::size_t>(image.width()) * image.height() * 4);
    return buf;
}

QString blendName(prism::core::BlendMode mode)
{
    switch (mode) {
    case prism::core::BlendMode::Normal: return "normal";
    case prism::core::BlendMode::Multiply: return "multiply";
    case prism::core::BlendMode::Screen: return "screen";
    case prism::core::BlendMode::Overlay: return "overlay";
    case prism::core::BlendMode::Darken: return "darken";
    case prism::core::BlendMode::Lighten: return "lighten";
    case prism::core::BlendMode::ColorDodge: return "color-dodge";
    case prism::core::BlendMode::ColorBurn: return "color-burn";
    case prism::core::BlendMode::HardLight: return "hard-light";
    case prism::core::BlendMode::SoftLight: return "soft-light";
    case prism::core::BlendMode::Difference: return "difference";
    case prism::core::BlendMode::Exclusion: return "exclusion";
    case prism::core::BlendMode::Add: return "add";
    case prism::core::BlendMode::Subtract: return "subtract";
    case prism::core::BlendMode::Divide: return "divide";
    case prism::core::BlendMode::Hue: return "hue";
    case prism::core::BlendMode::Saturation: return "saturation";
    case prism::core::BlendMode::Color: return "color";
    case prism::core::BlendMode::Luminosity: return "luminosity";
    }
    return "normal";
}

prism::core::BlendMode blendFromName(const QString& name)
{
    static const QHash<QString, prism::core::BlendMode> table = {
        { QStringLiteral("multiply"), prism::core::BlendMode::Multiply },
        { QStringLiteral("screen"), prism::core::BlendMode::Screen },
        { QStringLiteral("overlay"), prism::core::BlendMode::Overlay },
        { QStringLiteral("darken"), prism::core::BlendMode::Darken },
        { QStringLiteral("lighten"), prism::core::BlendMode::Lighten },
        { QStringLiteral("color-dodge"), prism::core::BlendMode::ColorDodge },
        { QStringLiteral("color-burn"), prism::core::BlendMode::ColorBurn },
        { QStringLiteral("hard-light"), prism::core::BlendMode::HardLight },
        { QStringLiteral("soft-light"), prism::core::BlendMode::SoftLight },
        { QStringLiteral("difference"), prism::core::BlendMode::Difference },
        { QStringLiteral("exclusion"), prism::core::BlendMode::Exclusion },
        { QStringLiteral("add"), prism::core::BlendMode::Add },
        { QStringLiteral("subtract"), prism::core::BlendMode::Subtract },
        { QStringLiteral("divide"), prism::core::BlendMode::Divide },
        { QStringLiteral("hue"), prism::core::BlendMode::Hue },
        { QStringLiteral("saturation"), prism::core::BlendMode::Saturation },
        { QStringLiteral("color"), prism::core::BlendMode::Color },
        { QStringLiteral("luminosity"), prism::core::BlendMode::Luminosity },
    };
    return table.value(name, prism::core::BlendMode::Normal);
}

} // namespace

bool ProjectIO::save(const Document& doc, const QString& path)
{
    QJsonObject root;
    root["version"] = kFormatVersion;
    const auto size = doc.canvasSize();
    root["width"] = size.width;
    root["height"] = size.height;
    root["dpi"] = size.dpi;
    root["colorProfile"] = QString::fromStdString(doc.colorProfile());

    QJsonArray layers;
    for (const auto& layer : doc.layers()) {
        QJsonObject lo;
        lo["name"] = QString::fromStdString(layer->name());
        lo["visible"] = layer->isVisible();
        lo["opacity"] = static_cast<double>(layer->opacity());
        lo["blendMode"] = blendName(layer->blendMode());
        lo["type"] = (layer->type() == Layer::Type::Raster) ? "raster" : "group";
        if (layer->type() == Layer::Type::Raster)
            lo["pixels"] = QString::fromLatin1(encodePng(layer->pixels()).toBase64());
        layers.append(lo);
    }
    root["layers"] = layers;

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return false;
    file.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
    return file.commit();
}

std::optional<Document> ProjectIO::load(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return std::nullopt;
    const QJsonDocument docJson = QJsonDocument::fromJson(file.readAll());
    if (!docJson.isObject())
        return std::nullopt;
    const QJsonObject root = docJson.object();
    if (root["version"].toInt() != kFormatVersion)
        return std::nullopt;

    Document doc(CanvasSize{ root["width"].toInt(), root["height"].toInt(),
                             root["dpi"].toInt(72) },
                 root["colorProfile"].toString("sRGB").toStdString());

    for (const auto& lv : root["layers"].toArray()) {
        const QJsonObject lo = lv.toObject();
        auto layer = std::make_unique<Layer>(
            lo["type"].toString() == "raster" ? Layer::Type::Raster : Layer::Type::Group,
            lo["name"].toString("Layer").toStdString());
        layer->setVisible(lo["visible"].toBool(true));
        layer->setOpacity(static_cast<float>(lo["opacity"].toDouble(1.0)));
        layer->setBlendMode(blendFromName(lo["blendMode"].toString()));
        if (layer->type() == Layer::Type::Raster) {
            const auto pixels = decodePng(QByteArray::fromBase64(lo["pixels"].toString().toLatin1()));
            if (!pixels)
                return std::nullopt;
            layer->pixels() = *pixels;
        }
        doc.addLayer(std::move(layer));
    }
    return doc;
}

std::optional<Document> ProjectIO::importImage(const QString& path)
{
    QImage image(path);
    if (image.isNull())
        return std::nullopt;
    image = image.convertToFormat(QImage::Format_RGBA8888);

    Document doc(CanvasSize{ image.width(), image.height(), 72 });
    doc.addRasterLayer("Imported image");
    auto& layer = *doc.layers().front();
    std::memcpy(layer.pixels().data(), image.constBits(),
                static_cast<std::size_t>(image.width()) * image.height() * 4);
    return doc;
}

bool ProjectIO::exportPng(const prism::canvas::Compositor& composite, const QString& path)
{
    return exportImage(composite, path, "png");
}

bool ProjectIO::exportImage(const prism::canvas::Compositor& composite, const QString& path,
                            const QByteArray& format, int quality)
{
    const auto& result = composite.result();
    const QImage image(result.data(), result.width(), result.height(), result.width() * 4,
                       QImage::Format_RGBA8888);
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return false;

    QImageWriter writer(&file, format);
    writer.setQuality(quality);
    if (!writer.write(image.copy()))
        return false;
    return file.commit();
}

QList<QByteArray> ProjectIO::writableFormats()
{
    // Formats worth offering for a drawing app; anything Qt cannot write on
    // this platform is filtered out so the dialogs never offer a dead choice.
    // WebP and TIFF are missing from some Qt builds (they are plugins), and
    // offering them anyway made "Save As" fail silently.
    static const QList<QByteArray> candidates = {
        "png", "jpg", "webp", "tiff", "bmp", "gif", "ppm", "xbm", "xpm", "ico",
    };
    QList<QByteArray> out;
    for (const QByteArray& f : candidates)
        if (QImageWriter::supportedImageFormats().contains(f))
            out.append(f);
    return out;
}

QList<QByteArray> ProjectIO::readableFormats()
{
    // Ordered by usefulness for a drawing app, filtered to what this Qt build
    // can actually decode (JPEG/GIF/ICO arrive as plugins).
    static const QList<QByteArray> candidates = {
        "png", "jpg", "jpeg", "webp", "tif", "tiff", "bmp", "gif", "ppm", "pgm", "pbm", "ico",
    };
    const QList<QByteArray> supported = QImageReader::supportedImageFormats();
    QList<QByteArray> out;
    for (const QByteArray& f : candidates)
        if (supported.contains(f))
            out.append(f);
    return out;
}

} // namespace prism::io
