#include "application/PresetImageProvider.h"

#include "brush/BrushPresets.h"

PresetImageProvider::PresetImageProvider()
    : QQuickImageProvider(QQuickImageProvider::Image)
{
}

QImage PresetImageProvider::requestImage(const QString& id, QSize* size,
                                         const QSize& requestedSize)
{
    const int index = id.section('/', 0, 0).toInt();
    const auto& presets = prism::canvas::builtinBrushPresets();
    if (index < 0 || index >= static_cast<int>(presets.size())) {
        if (size)
            *size = QSize(1, 1);
        return {};
    }

    const int w = requestedSize.width() > 0 ? requestedSize.width() : 44;
    const int h = requestedSize.height() > 0 ? requestedSize.height() : 44;
    const auto thumb = prism::canvas::renderPresetThumbnail(
        presets[static_cast<std::size_t>(index)].brush, w, h);
    if (size)
        *size = QSize(thumb.width(), thumb.height());

    // .copy() detaches from the stack-local buffer before it dies.
    return QImage(thumb.data(), thumb.width(), thumb.height(), thumb.width() * 4,
                  QImage::Format_RGBA8888)
        .copy();
}
