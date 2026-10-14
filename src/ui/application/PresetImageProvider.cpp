#include "application/PresetImageProvider.h"

#include "application/AppController.h"

PresetImageProvider::PresetImageProvider(AppController* controller)
    : QQuickImageProvider(QQuickImageProvider::Image), m_controller(controller) {}

QImage PresetImageProvider::requestImage(const QString& id, QSize* size,
                                         const QSize& requestedSize) {
    const int w = requestedSize.width() > 0 ? requestedSize.width() : 44;
    const int h = requestedSize.height() > 0 ? requestedSize.height() : 44;
    const QImage thumb =
        id.startsWith(QLatin1String("active"))
            ? m_controller->activeBrushPreview(w, h)
            : m_controller->presetThumbnailRaw(id.section('/', 0, 0).toInt(), w, h);
    if (size)
        *size = thumb.size();
    return thumb;
}

LayerThumbnailProvider::LayerThumbnailProvider(AppController* controller)
    : QQuickImageProvider(QQuickImageProvider::Image), m_controller(controller) {}

QImage LayerThumbnailProvider::requestImage(const QString& id, QSize* size,
                                            const QSize& requestedSize) {
    const int index = id.section('/', 0, 0).toInt();
    const int w = requestedSize.width() > 0 ? requestedSize.width() : 44;
    const int h = requestedSize.height() > 0 ? requestedSize.height() : 32;
    const QImage thumb = m_controller->layerThumbnail(index, w, h);
    if (size)
        *size = thumb.size();
    return thumb;
}

SelectionOverlayProvider::SelectionOverlayProvider(AppController* controller)
    : QQuickImageProvider(QQuickImageProvider::Image), m_controller(controller) {}

QImage SelectionOverlayProvider::requestImage(const QString&, QSize* size,
                                              const QSize& requestedSize) {
    Q_UNUSED(requestedSize);
    const QImage overlay = m_controller->selectionOverlay();
    if (size)
        *size = overlay.size();
    return overlay;
}
