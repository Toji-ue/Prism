#pragma once

#include "compositor/Compositor.h"
#include "document/Document.h"

#include <QString>
#include <optional>

namespace prism::io {

/// Project file I/O for the native `.prism` format.
///
/// Format v1: a JSON envelope plus one PNG (base64) per raster layer.
/// Versioned so old files keep loading — see AGENTS.md "file format".
class ProjectIO {
public:
    static constexpr int kFormatVersion = 1;

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
};

} // namespace prism::io
