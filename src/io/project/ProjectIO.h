#pragma once

#include "compositor/Compositor.h"
#include "document/Document.h"
#include "brush/BrushPresets.h"

#include <QJsonObject>
#include <QString>
#include <optional>

namespace prism::io {

/// Project file I/O for the native `.prism` format.
///
/// Format v1: a JSON envelope plus one PNG (base64) per raster layer.
/// Versioned so old files keep loading — see AGENTS.md "file format".
class ProjectIO {
public:
    static constexpr int kFormatVersion = 2;

    /// Serializes \p doc to \p path. Returns true on success.
    [[nodiscard]] static bool save(const prism::core::Document& doc, const QString& path);

    /// Loads a document from \p path. Returns nullopt on any error (wrong
    /// version, corrupt JSON, missing PNG).
    [[nodiscard]] static std::optional<prism::core::Document> load(const QString& path);

    /// Opens a raster image (PNG/JPEG/etc.) as a new single-layer document.
    [[nodiscard]] static std::optional<prism::core::Document> importImage(const QString& path);

    /// Exports the flattened document to a PNG at \p path.
    [[nodiscard]] static bool exportPng(const prism::canvas::Compositor& composite,
                                        const QString& path);

    /// Exports the flattened document in any format Qt can write (PNG, JPEG,
    /// WebP, TIFF, BMP, GIF, ...). \p format is the Qt format name; an empty
    /// one is inferred from the file suffix.
    [[nodiscard]] static bool exportImage(const prism::canvas::Compositor& composite,
                                          const QString& path, const QByteArray& format = {},
                                          int quality = 95);

    /// All formats the Save/Export dialogs offer, in Krita's order.
    [[nodiscard]] static QList<QByteArray> writableFormats();

    /// All raster formats the Open/Import dialogs offer, filtered to what this
    /// Qt build can actually decode (JPEG/GIF/ICO are plugins, not built in).
    [[nodiscard]] static QList<QByteArray> readableFormats();

    /// Brush exchange: one `.prismbrush` file holds one preset as JSON, so a
    /// brush can be handed to another artist and survives outside QSettings.
    [[nodiscard]] static bool exportBrush(const prism::canvas::BrushPreset& preset,
                                          const QString& path);
    [[nodiscard]] static std::optional<prism::canvas::BrushPreset> importBrush(const QString& path,
                                                                              QString* error = nullptr);
    /// Tip JSON codec, shared with the QSettings preset persistence so an
    /// imported image-tip brush survives an app restart.
    [[nodiscard]] static QJsonObject brushTipToJson(const prism::canvas::BrushTip& tip);
    /// Returns the tip; invalid JSON falls back to the procedural default.
    [[nodiscard]] static prism::canvas::BrushTip brushTipFromJson(const QJsonObject& obj);
};

} // namespace prism::io
