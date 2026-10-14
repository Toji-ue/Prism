#include "project/ProjectIO.h"

#include "compositor/Compositor.h"
#include "layer/Layer.h"

#include <QBuffer>
#include <QByteArray>
#include <QFile>
#include <QImage>
#include <QImageReader>
#include <QImageWriter>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <algorithm>

#include <cmath>
#include <cstring>
#include <unordered_map>
#include <unordered_set>

namespace prism::io {

using prism::core::CanvasSize;
using prism::core::Document;
using prism::core::Layer;

namespace {

/// Raster RGBA buffer <-> PNG bytes, straight alpha preserved.
QByteArray encodePng(const prism::core::raster::RasterBuffer& buf) {
    const QImage image(buf.data(), buf.width(), buf.height(), buf.width() * 4,
                       QImage::Format_RGBA8888);
    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    image.copy().save(&buffer, "PNG");
    return bytes;
}

std::optional<prism::core::raster::RasterBuffer> decodePng(const QByteArray& bytes) {
    QImage image;
    if (!image.loadFromData(bytes, "PNG"))
        return std::nullopt;
    image = image.convertToFormat(QImage::Format_RGBA8888);
    prism::core::raster::RasterBuffer buf(image.width(), image.height());
    std::memcpy(buf.data(), image.constBits(),
                static_cast<std::size_t>(image.width()) * image.height() * 4);
    return buf;
}

QString blendName(prism::core::BlendMode mode) {
    switch (mode) {
    case prism::core::BlendMode::Normal:
        return "normal";
    case prism::core::BlendMode::Multiply:
        return "multiply";
    case prism::core::BlendMode::Screen:
        return "screen";
    case prism::core::BlendMode::Overlay:
        return "overlay";
    case prism::core::BlendMode::Darken:
        return "darken";
    case prism::core::BlendMode::Lighten:
        return "lighten";
    case prism::core::BlendMode::ColorDodge:
        return "color-dodge";
    case prism::core::BlendMode::ColorBurn:
        return "color-burn";
    case prism::core::BlendMode::HardLight:
        return "hard-light";
    case prism::core::BlendMode::SoftLight:
        return "soft-light";
    case prism::core::BlendMode::Difference:
        return "difference";
    case prism::core::BlendMode::Exclusion:
        return "exclusion";
    case prism::core::BlendMode::Add:
        return "add";
    case prism::core::BlendMode::Subtract:
        return "subtract";
    case prism::core::BlendMode::Divide:
        return "divide";
    case prism::core::BlendMode::Hue:
        return "hue";
    case prism::core::BlendMode::Saturation:
        return "saturation";
    case prism::core::BlendMode::Color:
        return "color";
    case prism::core::BlendMode::Luminosity:
        return "luminosity";
    }
    return "normal";
}

prism::core::BlendMode blendFromName(const QString& name) {
    static const QHash<QString, prism::core::BlendMode> table = {
        {QStringLiteral("multiply"), prism::core::BlendMode::Multiply},
        {QStringLiteral("screen"), prism::core::BlendMode::Screen},
        {QStringLiteral("overlay"), prism::core::BlendMode::Overlay},
        {QStringLiteral("darken"), prism::core::BlendMode::Darken},
        {QStringLiteral("lighten"), prism::core::BlendMode::Lighten},
        {QStringLiteral("color-dodge"), prism::core::BlendMode::ColorDodge},
        {QStringLiteral("color-burn"), prism::core::BlendMode::ColorBurn},
        {QStringLiteral("hard-light"), prism::core::BlendMode::HardLight},
        {QStringLiteral("soft-light"), prism::core::BlendMode::SoftLight},
        {QStringLiteral("difference"), prism::core::BlendMode::Difference},
        {QStringLiteral("exclusion"), prism::core::BlendMode::Exclusion},
        {QStringLiteral("add"), prism::core::BlendMode::Add},
        {QStringLiteral("subtract"), prism::core::BlendMode::Subtract},
        {QStringLiteral("divide"), prism::core::BlendMode::Divide},
        {QStringLiteral("hue"), prism::core::BlendMode::Hue},
        {QStringLiteral("saturation"), prism::core::BlendMode::Saturation},
        {QStringLiteral("color"), prism::core::BlendMode::Color},
        {QStringLiteral("luminosity"), prism::core::BlendMode::Luminosity},
    };
    return table.value(name, prism::core::BlendMode::Normal);
}

} // namespace

bool ProjectIO::save(const Document& doc, const QString& path) {
    QJsonObject root;
    root["version"] = kFormatVersion;
    const auto size = doc.canvasSize();
    root["width"] = size.width;
    root["height"] = size.height;
    root["dpi"] = size.dpi;
    root["colorProfile"] = QString::fromStdString(doc.colorProfile());
    // Where the canvas sits inside the layer buffers. Absent in files written
    // before cropping became non-destructive, and (0,0) for an uncropped
    // document, so the field needs no version bump.
    const auto origin = doc.canvasOrigin();
    if (origin.x != 0 || origin.y != 0) {
        root["originX"] = origin.x;
        root["originY"] = origin.y;
    }

    QJsonArray layers;
    for (const auto& layer : doc.layers()) {
        QJsonObject lo;
        lo["name"] = QString::fromStdString(layer->name());
        lo["id"] = QString::number(layer->id());
        lo["parentId"] = QString::number(layer->parentId());
        lo["visible"] = layer->isVisible();
        lo["locked"] = layer->isLocked();
        lo["clipped"] = layer->isClipped();
        lo["alphaLocked"] = layer->isAlphaLocked();
        lo["opacity"] = static_cast<double>(layer->opacity());
        lo["blendMode"] = blendName(layer->blendMode());
        lo["type"] = layer->type() == Layer::Type::Raster  ? "raster"
                     : layer->type() == Layer::Type::Group ? "group"
                                                           : "vector";
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

std::optional<Document> ProjectIO::load(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return std::nullopt;
    const QJsonDocument docJson = QJsonDocument::fromJson(file.readAll());
    if (!docJson.isObject())
        return std::nullopt;
    const QJsonObject root = docJson.object();
    const int version = root["version"].toInt();
    if (version != 1 && version != kFormatVersion)
        return std::nullopt;

    const int width = root["width"].toInt();
    const int height = root["height"].toInt();
    if (width < 1 || height < 1 || width > 32768 || height > 32768)
        return std::nullopt;
    Document doc(CanvasSize{width, height, root["dpi"].toInt(72)},
                 root["colorProfile"].toString("sRGB").toStdString());
    doc.setCanvasOrigin(
        prism::core::geometry::Point{root["originX"].toInt(0), root["originY"].toInt(0)});

    std::unordered_map<std::uint64_t, const Layer*> layerIds;
    for (const auto& lv : root["layers"].toArray()) {
        const QJsonObject lo = lv.toObject();
        const QString type = lo["type"].toString();
        auto layer = std::make_unique<Layer>(type == QLatin1String("raster")   ? Layer::Type::Raster
                                             : type == QLatin1String("vector") ? Layer::Type::Vector
                                                                               : Layer::Type::Group,
                                             lo["name"].toString("Layer").toStdString());
        if (version == 2) {
            bool idOk = false, parentOk = false;
            const auto id = lo["id"].toString().toULongLong(&idOk);
            const auto parentId = lo["parentId"].toString().toULongLong(&parentOk);
            if (!idOk || !parentOk || id == 0 || id == UINT64_MAX || layerIds.contains(id))
                return std::nullopt;
            layer->setId(id);
            layer->setParentId(parentId);
            layer->setClipped(lo["clipped"].toBool(false));
            layer->setAlphaLocked(lo["alphaLocked"].toBool(false));
        }
        layer->setVisible(lo["visible"].toBool(true));
        layer->setLocked(lo["locked"].toBool(false));
        layer->setOpacity(static_cast<float>(lo["opacity"].toDouble(1.0)));
        layer->setBlendMode(blendFromName(lo["blendMode"].toString()));
        if (layer->type() == Layer::Type::Raster) {
            const auto pixels =
                decodePng(QByteArray::fromBase64(lo["pixels"].toString().toLatin1()));
            if (!pixels)
                return std::nullopt;
            layer->pixels() = *pixels;
        }
        const Layer* added = layer.get();
        doc.addLayer(std::move(layer));
        layerIds.emplace(added->id(), added);
    }
    for (const auto& layer : doc.layers()) {
        std::unordered_set<std::uint64_t> ancestors;
        auto parent = layer->parentId();
        while (parent != 0) {
            if (!ancestors.insert(parent).second || parent == layer->id())
                return std::nullopt;
            const auto it = layerIds.find(parent);
            if (it == layerIds.end() || it->second->type() != Layer::Type::Group)
                return std::nullopt;
            parent = it->second->parentId();
        }
    }
    return doc;
}

std::optional<Document> ProjectIO::importImage(const QString& path) {
    QImage image(path);
    if (image.isNull())
        return std::nullopt;
    image = image.convertToFormat(QImage::Format_RGBA8888);

    Document doc(CanvasSize{image.width(), image.height(), 72});
    doc.addRasterLayer("Imported image");
    auto& layer = *doc.layers().front();
    std::memcpy(layer.pixels().data(), image.constBits(),
                static_cast<std::size_t>(image.width()) * image.height() * 4);
    return doc;
}

bool ProjectIO::exportPng(const prism::canvas::Compositor& composite, const QString& path) {
    return exportImage(composite, path, "png");
}

bool ProjectIO::exportImage(const prism::canvas::Compositor& composite, const QString& path,
                            const QByteArray& format, int quality) {
    const auto& result = composite.result();
    QImage image(result.data(), result.width(), result.height(), result.width() * 4,
                 QImage::Format_RGBA8888);
    const int dotsPerMeter = static_cast<int>(std::lround(composite.canvasDpi() / 0.0254));
    image.setDotsPerMeterX(dotsPerMeter);
    image.setDotsPerMeterY(dotsPerMeter);
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return false;

    QImageWriter writer(&file, format);
    writer.setQuality(quality);
    if (!writer.write(image.copy()))
        return false;
    return file.commit();
}

QList<QByteArray> ProjectIO::writableFormats() {
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

QList<QByteArray> ProjectIO::readableFormats() {
    // Common formats first, then EVERYTHING else this Qt build can decode.
    // The tail must stay derived from Qt: the fixed whitelist that used to live
    // here silently dropped supported extensions (.jfif, .tga, .jp2) from the
    // file dialogs, so the user's image was not even selectable — which reads
    // as "the app does not accept this format".
    static const QList<QByteArray> preferred = {
        "png", "jpg", "jpeg", "jfif", "webp", "tif", "tiff", "bmp", "gif", "tga",
    };
    const QList<QByteArray> supported = QImageReader::supportedImageFormats();
    QList<QByteArray> out;
    for (const QByteArray& f : preferred)
        if (supported.contains(f))
            out.append(f);
    for (const QByteArray& f : supported)
        if (!out.contains(f))
            out.append(f);
    return out;
}

namespace {

bool parseBrushTip(const QJsonObject& root, prism::canvas::BrushTip& tip, QString* error) {
    const QString kindStr = root["kind"].toString();
    if (kindStr == "procedural") {
        tip.kind = prism::canvas::BrushTip::Kind::Procedural;
        const QJsonObject p = root;
        tip.shape = p["shape"].toString("round") == "round"
                        ? prism::canvas::BrushTip::Shape::Round
                        : prism::canvas::BrushTip::Shape::Square;
        tip.hardness = static_cast<float>(p["hardness"].toDouble(1.0));
        tip.roundness = static_cast<float>(p["roundness"].toDouble(1.0));
        tip.angle = static_cast<float>(p["angle"].toDouble(0.0));
        if (tip.hardness < 0.0F || tip.hardness > 1.0F || tip.roundness < 0.0F ||
            tip.roundness > 1.0F) {
            if (error)
                *error = "procedural tip: hardness/roundness out of range";
            return false;
        }
    } else if (kindStr == "image") {
        tip.kind = prism::canvas::BrushTip::Kind::Image;
        const QJsonObject img = root["image"].toObject();
        if (img.isEmpty()) {
            if (error)
                *error = "image tip: missing image object";
            return false;
        }
        tip.image.mime = img["mime"].toString().toStdString();
        if (tip.image.mime != "image/png") {
            if (error)
                *error = "image tip: only PNG is supported";
            return false;
        }
        const QByteArray data = QByteArray::fromBase64(img["data"].toString().toLatin1());
        if (data.isEmpty()) {
            if (error)
                *error = "image tip: empty base64 data";
            return false;
        }

        QImage imgLoad;
        if (!imgLoad.loadFromData(data, "PNG")) {
            if (error)
                *error = "image tip: cannot decode PNG";
            return false;
        }
        if (imgLoad.width() > 1024 || imgLoad.height() > 1024) {
            if (error)
                *error = "image tip: max size 1024x1024";
            return false;
        }

        // Convert QByteArray to std::vector<uint8_t>
        tip.image.data.assign(data.begin(), data.end());
        tip.image.invert = img["invert"].toBool(false);

        // Decode the grayscale mask here so the paint loop never touches PNG.
        const QImage gray = imgLoad.convertToFormat(QImage::Format_Grayscale8);
        tip.image.maskW = gray.width();
        tip.image.maskH = gray.height();
        tip.image.mask.assign(gray.constBits(), gray.constBits() + gray.sizeInBytes());
    } else {
        if (error)
            *error = QString("unknown tip kind: %1").arg(kindStr);
        return false;
    }
    return true;
}

} // namespace

bool ProjectIO::exportBrush(const prism::canvas::BrushPreset& preset, const QString& path) {
    QJsonObject root;
    root["format"] = QStringLiteral("prism-brush");
    root["version"] = 5;
    root["name"] = QString::fromStdString(preset.name);
    root["radius"] = preset.brush.radius;
    root["hardness"] = preset.brush.hardness;
    root["flow"] = preset.brush.flow;
    root["erase"] = preset.brush.erase;
    root["pressureOpacity"] = preset.brush.pressureOpacity;
    root["spacing"] = preset.brush.spacing;
    root["pressureCurve"] = preset.brush.pressureCurve;
    root["pressureSizeMin"] = preset.brush.pressureSizeMin;
    root["stabilization"] = preset.brush.stabilization;
    root["material"] =
        preset.brush.material == prism::canvas::BrushMaterial::Graphite ? "graphite" : "solid";
    root["grain"] = preset.brush.grain;
    root["grainScale"] = preset.brush.grainScale;
    root["tiltShading"] = preset.brush.tiltShading;
    root["endTaperDiameters"] = preset.brush.endTaperDiameters;
    root["color"] = QStringLiteral("#%1%2%3")
                        .arg(preset.brush.color.r, 2, 16, QLatin1Char('0'))
                        .arg(preset.brush.color.g, 2, 16, QLatin1Char('0'))
                        .arg(preset.brush.color.b, 2, 16, QLatin1Char('0'));
    root["tip"] = ProjectIO::brushTipToJson(preset.brush.tip);

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return false;
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    return file.commit();
}

std::optional<prism::canvas::BrushPreset> ProjectIO::importBrush(const QString& path,
                                                                 QString* error) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error)
            *error = "cannot open file";
        return std::nullopt;
    }
    const QJsonDocument json = QJsonDocument::fromJson(file.readAll());
    if (!json.isObject()) {
        if (error)
            *error = "invalid JSON";
        return std::nullopt;
    }
    const QJsonObject root = json.object();
    if (root["format"].toString() != QLatin1String("prism-brush")) {
        if (error)
            *error = "not a Prism brush file";
        return std::nullopt;
    }

    const int version = root["version"].toInt(1);
    if (version < 1 || version > 5) {
        if (error)
            *error = "unsupported brush version";
        return std::nullopt;
    }

    prism::canvas::BrushPreset preset;
    preset.name = root["name"].toString(QStringLiteral("Imported brush")).toStdString();
    preset.brush.radius = static_cast<float>(root["radius"].toDouble(8.0));
    preset.brush.hardness = static_cast<float>(root["hardness"].toDouble(0.8));
    preset.brush.flow = static_cast<float>(root["flow"].toDouble(1.0));
    preset.brush.erase = root["erase"].toBool(false);
    preset.brush.pressureOpacity = root["pressureOpacity"].toBool(false);
    if (version >= 3) {
        preset.brush.spacing = static_cast<float>(root["spacing"].toDouble(0.125));
        preset.brush.pressureCurve = static_cast<float>(root["pressureCurve"].toDouble(1.0));
        preset.brush.pressureSizeMin = static_cast<float>(root["pressureSizeMin"].toDouble(0.5));
        preset.brush.stabilization = static_cast<float>(root["stabilization"].toDouble(0.0));
    }
    if (version >= 4) {
        preset.brush.material = root["material"].toString() == QLatin1String("graphite")
                                    ? prism::canvas::BrushMaterial::Graphite
                                    : prism::canvas::BrushMaterial::Solid;
        preset.brush.grain = static_cast<float>(root["grain"].toDouble(0.0));
        preset.brush.grainScale = static_cast<float>(root["grainScale"].toDouble(1.0));
        preset.brush.tiltShading = static_cast<float>(root["tiltShading"].toDouble(0.0));
    }
    if (version >= 5)
        preset.brush.endTaperDiameters =
            static_cast<float>(root["endTaperDiameters"].toDouble(0.0));
    const QColor color(root["color"].toString(QStringLiteral("#000000")));
    preset.brush.color = prism::core::raster::Color{static_cast<std::uint8_t>(color.red()),
                                                    static_cast<std::uint8_t>(color.green()),
                                                    static_cast<std::uint8_t>(color.blue()), 255};
    // Clamp what came from the file.
    preset.brush.radius = std::clamp(preset.brush.radius, 1.0F, 200.0F);
    preset.brush.hardness = std::clamp(preset.brush.hardness, 0.0F, 1.0F);
    preset.brush.flow = std::clamp(preset.brush.flow, 0.01F, 1.0F);
    preset.brush.spacing = std::clamp(preset.brush.spacing, 0.02F, 1.0F);
    preset.brush.pressureCurve = std::clamp(preset.brush.pressureCurve, 0.3F, 3.0F);
    preset.brush.pressureSizeMin = std::clamp(preset.brush.pressureSizeMin, 0.05F, 1.0F);
    preset.brush.stabilization = std::clamp(preset.brush.stabilization, 0.0F, 1.0F);
    preset.brush.grain = std::clamp(preset.brush.grain, 0.0F, 1.0F);
    preset.brush.grainScale = std::clamp(preset.brush.grainScale, 0.5F, 4.0F);
    preset.brush.tiltShading = std::clamp(preset.brush.tiltShading, 0.0F, 1.0F);
    preset.brush.endTaperDiameters = std::clamp(preset.brush.endTaperDiameters, 0.0F, 6.0F);

    // Tip (v2+). v1 files and tip-less v2 files keep the struct's default:
    // procedural, round, hardness 1, roundness 1, angle 0.
    if (version >= 2 && !root["tip"].toObject().isEmpty()) {
        if (!parseBrushTip(root["tip"].toObject(), preset.brush.tip, error))
            return std::nullopt;
    }

    return preset;
}

QJsonObject ProjectIO::brushTipToJson(const prism::canvas::BrushTip& tip) {
    QJsonObject obj;
    obj["kind"] = tip.kind == prism::canvas::BrushTip::Kind::Procedural ? "procedural" : "image";

    if (tip.kind == prism::canvas::BrushTip::Kind::Procedural) {
        obj["shape"] = tip.shape == prism::canvas::BrushTip::Shape::Round ? "round" : "square";
        obj["hardness"] = tip.hardness;
        obj["roundness"] = tip.roundness;
        obj["angle"] = tip.angle;
    } else {
        QJsonObject img;
        img["mime"] = QString::fromStdString(tip.image.mime);
        const QByteArray qdata(reinterpret_cast<const char*>(tip.image.data.data()),
                               static_cast<qsizetype>(tip.image.data.size()));
        img["data"] = QString::fromLatin1(qdata.toBase64());
        img["invert"] = tip.image.invert;
        obj["image"] = img;
    }
    return obj;
}

prism::canvas::BrushTip ProjectIO::brushTipFromJson(const QJsonObject& obj) {
    prism::canvas::BrushTip tip;
    QString err;
    if (!obj.isEmpty() && !parseBrushTip(obj, tip, &err))
        qWarning() << "brush tip preset fell back to default:" << err;
    return tip;
}

} // namespace prism::io
